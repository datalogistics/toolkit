#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <errno.h>
#include "lors_api.h"
#include "lors_error.h"
#include "lors_job.h"
#include "lors_util.h"

/*#include "pipeline.h"*/


static void *_lorsWorkThread ( void *v);
static void _lorsCleanupThread ( void *v);
static int _lorsDoStore( __LorsJob *job, __LorsThreadInfo *tinfo){return LORS_SUCCESS;}; 
static int _lorsDoCopy( __LorsJob *job, __LorsThreadInfo *tinfo){return LORS_SUCCESS;};  
static int _lorsDoMCopy( __LorsJob *job, __LorsThreadInfo *tinfo){return LORS_SUCCESS;}; 

void _lorsGetDownloadJobCleanup ( void *para);
void _lorsCommitDLJobCleanup( void *para);

void _lorsCommitJob ( __LorsJob *job,__LorsJobStatus status, int err_code ) 
{ 
    int tmp;
    __LorsJobPool *jp;

    assert ( job != NULL );
    /*assert(job->status == JOB_CHECKOUT);  */
    jp = job->jp;
    assert(jp != NULL );

    tmp = pthread_mutex_lock(&(jp->lock));
    assert(tmp == 0 );  

    job->status = status;
    job->err_code = err_code;

    pthread_mutex_unlock(&(jp->lock)); 

    return;
    
};

static void 
_lorsKillThread( __LorsThreadPool *tp){
    int i;
    __LorsThreadInfo *tinfo;
    Dllist thd_node;
   
#if 0
    pthread_mutex_lock(&(tp->lock));
#endif
    
    for ( i =0 ; i < tp-> nthread ; i++){
#if 0
        if ( tp->threads[i].job != NULL )
        {
            fprintf(stderr, "Canceling this job: 0x%x : dst: 0x%x\n", 
                tp->threads[i].job,
                tp->threads[i].job->dst);
        }
#endif
        pthread_cancel(tp->threads[i].tid);
    };

#if 0
    dll_traverse(thd_node,tp->extra_threads){
        tinfo = (__LorsThreadInfo *)jval_v(thd_node->val);
        pthread_cancel(tinfo->tid);
    };

    pthread_mutex_unlock(&(tp->lock));
#endif
    return;
};


void * 
_lorsOutputThread(void *para){
    JRB       job_node;
    __LorsJobPool *jp;
    int       n = 0;
    char      *dst;
    longlong  cur_pos;
    __LorsJob *job = NULL;
    int       nwrite, ndone;
    __LorsThreadPool *tp = NULL;
    int       i,ret;

    jp = (__LorsJobPool *)para;
    assert(jp != NULL);

    /*fprintf(stderr, "Entering OutputThread\n");*/
    pthread_mutex_lock(&(jp->lock));
    if ( jp->num_pending_jobs <= 0 ) {
        /*fprintf(stderr, "entering cond_wait :-( \n");*/
        pthread_cond_wait(&(jp->write_cond),&(jp->lock));
        /*fprintf(stderr, "leaving cond_wait !!! \n");*/
    };

Again:
   
/*fprintf(stderr,"PERBUFFER = %d\n",jp->pre_buf);*/
    do {
        if ( jp->pre_buf > 0 ) { /* check if we have enough per-buffers */
            cur_pos = jp->dst_cur_point;
            n = 0;
            jrb_rtraverse(job_node,jp->pending_jobs)
            {
                job = (__LorsJob *)jval_v(job_node->val);
                if ( job->src_offset == cur_pos )
                {
                    if ( job->status != JOB_PENDING ){
                        assert(job->status == JOB_FAILED);
                        jp->pre_buf = 0;
                        break;
                    };
                    n++;
                    if ( n >= jp->pre_buf){
                        jp->pre_buf = 0;
                        break;
                    };
                    cur_pos = cur_pos + job->src_length;
                } else {
                    if ( jp->status != LORS_SUCCESS ) 
                    {
                        fprintf(stderr, "failure 2\n");
                        ret = jp->status;
                        pthread_mutex_unlock(&(jp->lock));
                        goto Done;
                    }
                    lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: Waitting for enough data\n");
                    /*fprintf(stderr, "entering cond_wait :-( 2\n");*/
                    pthread_cond_wait(&(jp->write_cond),&(jp->lock));
                    /*fprintf(stderr, "leaving cond_wait !!! 2\n");*/
                    goto Again; 
                };
            };
            if ( job!=NULL && (cur_pos == jp->src_offset + job->src_length) ){
                jp->pre_buf = 0;
            };
        };
        /*pthread_mutex_unlock(&(jp->lock));*/


        if ( n < jp->pre_buf ) { 
            if ( jp->status != LORS_SUCCESS ) {
                fprintf(stderr, "failure 3\n");
                ret = jp->status;
                pthread_mutex_unlock(&(jp->lock));
                goto Done;
            }
            lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: Waitting for enough data\n");
        /*fprintf(stderr, "entering cond_wait :-( 3\n");*/
            pthread_cond_wait(&(jp->write_cond),&(jp->lock));
        /*fprintf(stderr, "leaving cond_wait !!! 3\n");*/
            goto Again;
        };

        /*pthread_mutex_lock(&(jp->lock));*/
        job_node = jrb_find_llong(jp->pending_jobs,jp->dst_cur_point);
        while ( job_node != NULL ) {
            /*pthread_mutex_lock(&(jp->lock));*/
            jp->num_pending_jobs --;
            assert(jp->num_pending_jobs >= 0 );
            if ( jp->num_pending_jobs <= jp->cache ) {
                pthread_cond_broadcast(&(jp->read_cond));
            };
            pthread_mutex_unlock(&(jp->lock));

            job = (__LorsJob *)jval_v(job_node->val);
            if ( job->status != JOB_PENDING ){
                assert (job->status == JOB_FAILED);
                ret = job->err_code;
                goto Done;
            };
            assert(job->src_offset == jp->dst_cur_point);
            assert(job->status != JOB_COMMIT);
            if ( jp->buf != NULL ) { /* write into internal buffer */
                dst = jp->buf + (jp->dst_cur_point - jp->src_offset);
                memcpy(dst,job->dst,job->src_length);
                pthread_mutex_lock(&(jp->lock));
                job->status = JOB_COMMIT;
                jp->dst_cur_point = jp->dst_cur_point + job->src_length; 
                /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, job->dst);*/
                free(job->dst);
                job->dst = NULL;
                pthread_mutex_unlock(&(jp->lock));
                if ( g_lors_demo )
                {
#ifdef _MINGW
                    fprintf(stderr,"DRAW Output %I64d %ld white\n",
                            job->src_offset,job->src_length);
#else
                    fprintf(stderr,"DRAW Output %lld %ld white\n", 
                            job->src_offset,job->src_length);
#endif
                    fflush(stderr);
                }
            }else{ /* write into fd */
                assert ( jp->fd >= 0 );
                nwrite = 0;
                ndone = 0;
#ifdef _MINGW
                lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: writting data from %I64d to %I64d \n",job->src_offset, job->src_offset + job->src_length);
#else
                lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: writting data from %lld to %lld \n",job->src_offset, job->src_offset + job->src_length);
#endif
                while ( ndone < job->src_length) {
                    nwrite = write(jp->fd,(char *)job->dst + ndone,(size_t)job->src_length - ndone);
                    if ( nwrite <= 0 ){
                        if (errno == EINTR ){
                            continue;
                        };
                        fprintf(stderr, "Write Error: %d: %s\n", nwrite, strerror(errno));
                        fflush(stderr);
                        fprintf(stderr, "Cannot continue writing output.\n");
                        fflush(stderr);
                        pthread_mutex_lock(&(jp->lock));
                        jp->dst_cur_point = jp->dst_cur_point + ndone;
                        lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: failed to write to file: errno = %d\n",errno);
                        ret = LORS_SYS_FAILED + errno;
                        pthread_mutex_unlock(&(jp->lock));
                        goto Done;
                    };
                    ndone = ndone + nwrite;
                };
                pthread_mutex_lock(&(jp->lock));
                jp->dst_cur_point = jp->dst_cur_point + ndone;
                if ( job->dst != NULL ){
                    /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, job->dst);*/
                    free(job->dst);
                    job->dst = NULL;
                };
                job->status = JOB_COMMIT;
                pthread_mutex_unlock(&(jp->lock));
                if ( g_lors_demo )
                {
#ifdef _MINGW
                    fprintf(stderr,"DRAW Output %I64d %I64d white\n",
                            job->src_offset,job->src_length);
#else
                    fprintf(stderr,"DRAW Output %lld %lld white\n", 
                            job->src_offset,job->src_length);
#endif
                    fflush(stderr);
                }
            };
            pthread_mutex_lock(&(jp->lock));
            job_node = jrb_find_llong(jp->pending_jobs,jp->dst_cur_point);
        };
        /*pthread_mutex_unlock(&(jp->lock));*/

        if ( jp->status != LORS_SUCCESS ) {
                        fprintf(stderr, "failure 4\n");
                ret = jp->status;
            pthread_mutex_unlock(&(jp->lock));
            goto Done;
        }
        if ( jp->dst_cur_point >= jp->src_offset + jp->src_length ) {
            ret = LORS_SUCCESS;
            pthread_mutex_unlock(&(jp->lock));
            goto Done;
        };
        /*fprintf(stderr, "entering cond_wait :-( 4\n");*/
        pthread_cond_wait(&(jp->write_cond),&(jp->lock));
        /*fprintf(stderr, "leaving cond_wait !!! 4\n");*/
    }while (1);;
    pthread_mutex_unlock(&(jp->lock));


Done:

    /*fprintf(stderr, "leaving output thread\n");*/
    jp->status = ret;
    _lorsKillThread(jp->thread_pool);
    pthread_cond_broadcast(&(jp->cond));
    pthread_cond_broadcast(&(jp->read_cond));
    pthread_exit(0);
    
};

/* new */
#if 0
void *_lorsOutputThread_newunused(void *para)
{
    JRB             job_node = NULL;
    JRB             unwritten_jobs = NULL;
    int             unwritten_jobs_count = 0;
    __LorsJobPool  *jp;
    int             n = 0;
    char           *dst;
    longlong        cur_position = 0;
    __LorsJob      *job = NULL;
    int             nwrite, ndone;
    __LorsThreadPool *tp = NULL;
    int             i, ret;

    jp = (__LorsJobPool *)para;
    assert(jp != NULL);

    unwritten_jobs = make_jrb();
    assert (unwritten_jobs != NULL);

    while (1)
    {
        job = NULL;
        ret = queuePull(jp->q, (void**)&job, 1);
        if ( ret != 0 ) { fprintf(stderr, "QueuePull error.\n"); }
        assert(job != NULL);

        jrb_insert_llong(unwritten_jobs, job->src_offset, new_jval_v(job));
        unwritten_jobs_count++;

#if 0
        if ( jp->pre_buf > 0 )
        {
            if ( unwritten_jobs_count <= jp->pre_buf ) continue;
            /* verify that those buffers in unwritten_jobs are enough */
            cur_position = jp->dst_cur_point;
            n=0;
            jrb_rtraverse(job_node, unwritten_jobs)
            {
                job = (__LorsJob *)jval_v(job_node->val);
                if ( job->src_offset == cur_position)
                {
                    if ( job->status != JOB_PENDING )
                    {
                        assert(job->status == JOB_FAILED);
                        jp->pre_buf = 0;
                        goto output_failure;
                        break;  /* this is a utter failure, so we must end work */
                    }
                    n++;
                    if ( n >= jp->pre_buf )
                    {
                        /* we've got enough for the prebuffer */
                        jp->pre_buf = 0;
                        break;
                    }
                    cur_position = cur_position + job->src_length;
                } else {
                    /* if the processing has failed entirely */
                    if ( jp->status != LORS_SUCCESS )
                    {
                        fprintf(stderr, "failure 2\n");
                        ret = jp->status;
                        pthread_mutex_unlock(&(jp->lock));
                        goto Done;
                    }
                    break;

                } /* if ( job->src_offset == cur_position ) */
            } /* jrb_rtraverse(unwritten_jobs) */
            if ( job!= NULL && ( cur_position == jp->src_offset + job->src_length) ){
                jp->pre_buf = 0;
            }
        } /* if ( pre_buf > 0 ) */
        if ( n < jp->pre_buf )
        {
            if ( jp->status != LORS_SUCCESS )
            {
                fprintf(stderr, "failure 3\n");
                ret = jp->status;
                pthread_mutex_unlock(&(jp->lock));
                goto Done;
            }
            continue;
        }
#endif

        job_node = jrb_find_llong(unwritten_jobs, jp->dst_cur_point);
        jrb_rtraverse(job_node, unwritten_jobs)
        {
            pthread_mutex_lock(&(jp->lock));
            job = (__LorsJob *)jval_v(job_node->val);
            if ( job->status == JOB_FAILED ){
                fprintf(stderr, "job->status: %d 0x%x\n", job->status, job->status);
                ret = job->err_code;
                goto Done;
            };
            pthread_mutex_unlock(&(jp->lock));
            if ( job->src_offset ==  jp->dst_cur_point )
            {
                if ( jp->buf != NULL ) /* write to buffer */
                {
                    dst = jp->buf + (jp->dst_cur_point - jp->src_offset);
                    memcpy(dst,job->dst,job->src_length);


                } else {  /* write to fd */
                    assert ( jp->fd >= 0 );
                    nwrite = 0;
                    ndone = 0;
                    lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: writting data "
                            "from %lld to %lld \n",job->src_offset, 
                            job->src_offset + job->src_length);
                    while ( ndone < job->src_length) {
                        nwrite = write(jp->fd, job->dst + ndone,
                                       (size_t)job->src_length - ndone);
                        if ( nwrite <= 0 ){
                            if (errno == EINTR ){
                                continue;
                            };
                            fprintf(stderr, "Write Error: %d: %s\n", 
                                    nwrite, strerror(errno));
                            fprintf(stderr, "Cannot continue writing output.\n");
                            pthread_mutex_lock(&(jp->lock));
                            jp->dst_cur_point = jp->dst_cur_point + ndone;
                            lorsDebugPrint(D_LORS_VERBOSE,"_lorsOutputData: failed "
                                    "to write to file: errno = %d\n",errno);
                            ret = LORS_SYS_FAILED + errno;
                            pthread_mutex_unlock(&(jp->lock));
                            goto output_failure;
                        };
                        ndone = ndone + nwrite;
                    };
                } /* write to fd */

                pthread_mutex_lock(&(jp->lock)); /* lock */
                jp->dst_cur_point = jp->dst_cur_point + job->src_length; 
                job->status = JOB_COMMIT;
                if ( job->dst != NULL ){
                    free(job->dst);
                    job->dst = NULL;
                };
                jp->num_pending_jobs --;
                if ( jp->num_pending_jobs <= jp->cache ) {
                    pthread_cond_broadcast(&(jp->read_cond));
                };
                pthread_mutex_unlock(&(jp->lock));
                if ( g_lors_demo )
                {
                    fprintf(stderr,"DRAW Output %lld %lld white\n", 
                            job->src_offset,job->src_length);
                }
            } /* job->src_offset == jp->dst_cur_point */
        } /* jrb traverse */

        /*
        fprintf(stderr, "offset + length: %lld\n",
                jp->src_offset+jp->src_length);
        fprintf(stderr, "current: %lld\n", jp->dst_cur_point);
        */
        if ( jp->dst_cur_point >= jp->src_offset + jp->src_length ) {
            ret = LORS_SUCCESS;
            goto Done;
        };

    } /* while(1) */

output_failure:
Done:
    /* before writing out anything verify that:
     *    - prebuffer is met 
     *    - the buffer is in the right order.
     *    - it is time to write out this buffer. */
    /* as long as the jp-> status is not failure, continue */
    /* once we've finished the length of it, we're done. */

    jp->status = ret;
    pthread_cond_broadcast(&(jp->cond));
    pthread_cond_broadcast(&(jp->read_cond));
    _lorsKillThread(jp->thread_pool);
    jrb_free_tree(unwritten_jobs);
    pthread_exit(NULL);
    return NULL;

}
#endif

int 
_lorsCreateOutputThread ( pthread_t *tid, __LorsJobPool *jp ){
    pthread_attr_t  attr;
    int             ret;

    ret = pthread_attr_init(&attr);
    assert(ret == 0);
#ifndef _CYGWIN
    ret = pthread_attr_setscope(&(attr),PTHREAD_SCOPE_SYSTEM);
    assert(ret == 0);
#endif
    ret = pthread_attr_setdetachstate(&(attr),PTHREAD_CREATE_JOINABLE);
    assert(ret == 0);

    ret = pthread_create(tid,&attr,_lorsOutputThread,(void*)jp);
    pthread_attr_destroy(&attr);
    if ( ret != 0 ){
        return (LORS_PTHREAD_CREATE_FAILED);
    }else{
        return (LORS_SUCCESS);
    };
};



static void
_lorsCalcProgressNumber(__LorsJobPool *jp, __LorsJob *job)
{
    Dllist job_node;
    __LorsJob *job_tmp;
    
    assert(jp != NULL && job != NULL);
    dll_traverse(job_node,jp->job_list){
        job_tmp = (__LorsJob *)jval_v(job_node->val);
        if (job_tmp->src_offset >= job->src_offset ){
            break;
        };
        if ( job_tmp->status != JOB_CHECKOUT ){
            continue;
        };
        job_tmp->n_done_ahead ++;
    };

    return;
};

void _lorsCommitDLJobCleanup( void *para){
   __LorsJob *job;

   job = (__LorsJob *)para;

   pthread_mutex_unlock(&(job->jp->lock));
   return;
};

int 
_lorsCommitDownloadJob(__LorsJob *job,__LorsJobStatus status, int err_code, LorsMapping *lm,  LorsDepot *ldp, char *buf )
{

    int ret;
    JRB thd_node;
    __LorsThreadInfo *tinfo;
  
    pthread_cleanup_push(_lorsCommitDLJobCleanup,((void*)(job)));
    pthread_mutex_lock(&(job->jp->lock));
    job->n_threads --;
    pthread_cond_broadcast(&(job->jp->cond));
    assert(job->n_threads >= 0);

    /*fprintf(stderr, "\n");
    fprintf(stderr, "CommitedDownload\n");
    fprintf(stderr, "status: %d\n", status);
    fprintf(stderr, "job->status: %d\n",job->status);
    fprintf(stderr, "buf: 0x%x\n", buf);
    fprintf(stderr, "\n");*/
    if ( (job->status == JOB_COMMIT) || ( job->status == JOB_PENDING) )
    { 
        /* job has already been commited, give up */
        if (buf != NULL ) 
        {
            /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, buf);*/
            free(buf);
        };
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsCommitDownloadJob: Job has been "
                                      "commited, give up\n");
        if ( g_lors_demo )
        {
            if (lm != NULL )
            {
#ifdef _MINGW
                fprintf(stderr,"DELETE Arrow2 from %s:%4d %I64d %I64d\n",
                    lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#else
                fprintf(stderr,"DELETE Arrow2 from %s:%4d %lld %lld\n",
                    lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#endif
                fflush(stderr);
            };
        }
        /*pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);*/
        /*fprintf(stderr, "unlock 1\n");*/
        /* TODO: check if thisis right ? */
        pthread_cond_broadcast(&(job->jp->write_cond));
        pthread_mutex_unlock(&(job->jp->lock));
        ret = LORS_SUCCESS;
        goto bail;
    };
    if ((status != JOB_COMMIT))
    {
        if ( (job->n_threads > 0) || (!dll_empty((Dllist)job->src))) 
        { 
            /* multi-threads are working on this job, give up */
            assert(job->status == JOB_CHECKOUT);
            if (buf != NULL ) 
            {
                /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, buf);*/
                free(buf);
            };
            lorsDebugPrint(D_LORS_VERBOSE,"_lorsCommitDownloadJob: operation failed, "
                        "let another thread to commit,  give up\n");
            if ( g_lors_demo )
            {
                if (lm != NULL )
                {
#ifdef _MINGW
                    fprintf(stderr,"DELETE Arrow2 from %s:%4d %I64d %I64d\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#else
                    fprintf(stderr,"DELETE Arrow2 from %s:%4d %lld %lld\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#endif
                    fflush(stderr);
                };
            }
            /*pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);*/
            /*fprintf(stderr, "unlock 2\n");*/
            pthread_cond_broadcast(&(job->jp->write_cond));
            pthread_mutex_unlock(&(job->jp->lock));
            /*return(LORS_SUCCESS);*/
            ret = LORS_SUCCESS;
            goto bail;
        } else { 
            /* commit failed job */
            job->status = status;
            lorsDebugPrint(D_LORS_VERBOSE,"_lorsCommitDownloadJob: operation failed, "
                                            "commit it\n");
            if ( g_lors_demo )
            {
                if (lm != NULL ){
#ifdef _MINGW
                    fprintf(stderr,"DELETE Arrow2 from %s:%4d %I64d %I64d\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#else
                    fprintf(stderr,"DELETE Arrow2 from %s:%4d %lld %lld\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#endif
                    fflush(stderr);
                };
            }
            if ( buf != NULL ) 
            {
                /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, buf);*/
                free(buf);
            };
            job->err_code = err_code;
            job->dst = NULL;
            jrb_insert_llong(job->jp->pending_jobs,job->src_offset,new_jval_v(job));
            job->jp->num_pending_jobs ++;

#if 0            
            thd_node = jrb_find_int(job->worker_threads,pthread_self());
            assert(thd_node != NULL);
            jrb_delete_node(thd_node);
#endif
            /*pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);*/
            /*fprintf(stderr, "unlock 3\n");*/
            pthread_cond_broadcast(&(job->jp->write_cond));
            pthread_mutex_unlock(&(job->jp->lock));
            /*return(LORS_SUCCESS);*/
            ret = LORS_SUCCESS;
            goto bail;
        };
    };

    ret = LORS_SUCCESS;
    assert(status == JOB_COMMIT);
    assert(ldp != NULL);
    if ( status == JOB_COMMIT ){
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsCommitDownloadJob: committing job \n");
        assert(job->status == JOB_CHECKOUT);
        pthread_mutex_lock(ldp->dpp->lock);
        ldp->nthread_done++;
        pthread_mutex_unlock(ldp->dpp->lock);
        if ( g_lors_demo )
        {
            if (lm != NULL ){
#ifdef _MINGW
                fprintf(stderr,"DRAW DLSlice %s:%4d %I64d %I64d\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
                fprintf(stderr,"DELETE Arrow2 from %s:%4d %I64d %I64d\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#else
                fprintf(stderr,"DRAW DLSlice %s:%4d %lld %lld\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
                fprintf(stderr,"DELETE Arrow2 from %s:%4d %lld %lld\n",
                        lm->depot.host,lm->depot.port,job->src_offset,job->src_length);
#endif
            };
        }
        job->status = JOB_PENDING;
        _lorsCalcProgressNumber(job->jp,job);
        job->dst = buf;
        jrb_insert_llong(job->jp->pending_jobs,job->src_offset,new_jval_v(job));
        /*queuePush(job->jp->q, job, 1);*/
        job->jp->num_pending_jobs ++;
        /*pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);*/
        /*fprintf(stderr, "unlock 4\n");*/
        pthread_cond_broadcast(&(job->jp->write_cond));
        pthread_mutex_unlock(&(job->jp->lock));
        /*return(LORS_SUCCESS);*/
        ret = LORS_SUCCESS;
        goto bail;
    };
    assert(0);

bail:

    pthread_cleanup_pop(0);
    return (ret);
};

void 
_lorsDoCleanupDL( void *paras)
{
    void **args;

    args = (void **)paras;
    _lorsCommitDownloadJob((__LorsJob *)args[0],JOB_CANCELED,LORS_PTHREAD_CANCELED,NULL,NULL,(char*)args[1]);
};

void 
_lorsDoCleanup( void *job)
{
      _lorsCommitJob((__LorsJob *)job,JOB_CANCELED,LORS_PTHREAD_CANCELED);
};



static int _lorsCreateDLNode ( __LorsDLNode **dl_node ) 
{
    __LorsDLNode    *node;

    if ( (node = (__LorsDLNode*)calloc(1,sizeof(__LorsDLNode))) == NULL ){
        return ( LORS_NO_MEMORY);
    };
    pthread_mutex_init(&node->lock, NULL);

    node->mappings = new_dllist();
    node->good_copies = 0;

    *dl_node = node;

    return ( LORS_SUCCESS );
};

void _lorsFreeDLList( __LorsDLList list)
{
    Dllist  node;
    __LorsDLNode *dl_node;

    if ( list == NULL )
         return;
    
    dll_traverse(node,list->list){
        dl_node = (__LorsDLNode *)jval_v(node->val);
        if ( dl_node->mappings != NULL ){
             free_dllist( dl_node->mappings);
             free(dl_node);
        };
    };
    
    free_dllist(list->list);
    free(list);
};


static void _lorsSetDLNode ( __LorsDLNode *node, 
                             LorsSet      *set, 
                             longlong     offset,
                             longlong     length){
    JRB      jrb_node;
    longlong tmp1, tmp2;
    Dllist   mappings_list,list_node;
    LorsMapping *mapping;
    LorsDepot   *depot;
    longlong    next_boundary = -1;

    node->length = -1;


    assert( node != NULL &&
            set  != NULL &&
            offset >= 0 &&
            length > 0 &&
            jrb_empty(node->mappings) );
    jrb_rtraverse(jrb_node,set->mapping_map) {
        tmp1 = jval_ll(jrb_node->key);
        if ( tmp1 > offset ) 
            break;
        mapping = (LorsMapping *)jrb_node->val.v;
        if ( (mapping->logical_length + mapping->exnode_offset) <= offset )
            continue;
        assert(mapping->exnode_offset <= offset);
        dll_append(node->mappings,new_jval_v(mapping));
        tmp2 = mapping->exnode_offset + mapping->logical_length - offset;
        if ( node->length < 0)
            node->length = tmp2;
        else if (tmp2 < node->length )
            node->length = tmp2;
    };

    if ( jrb_node != jrb_nil(set->mapping_map) ){
        tmp1 = jval_ll(jrb_node->key);
        if ( node->length < 0 ) 
            node->length = tmp1 - offset;
        else {
            if ( (tmp1 - offset ) < node->length ) 
                  node->length = (tmp1 - offset);
        };
    }else {
        if ( node->length < 0 )  node->length = length;
    }; 

    if ( node->length > length ) node->length = length;

    node->offset = offset;

    return;    
};

int _lorsNewDLList(__LorsDLList *list )
{
    __LorsDLList        new_list;
    new_list = (__LorsDLList)calloc(1, sizeof(struct __lorsdllist));
    if ( new_list == NULL ) return LORS_NO_MEMORY;

    pthread_mutex_init(&new_list->lock, NULL);
    new_list->list = new_dllist();

    *list = new_list;
    return LORS_SUCCESS;
}

/* scan 'set' and at every hard boundary create a new DLNode. 
 * This function allows for division for sets with inconsistent block
 * boundaries. */
int _lorsCreateDLList(LorsSet        *set,
                      longlong       offset,
                      longlong       length,
                      __LorsDLList   *list){
    __LorsDLList       new_list;
    longlong           cur_offset = 0,seg_len = 0;
    LorsDepot          *depot;
    JRB                cur_node, tmp;
    __LorsDLNode       *node;
    int                ret;

    assert ( set != NULL &&
             offset >= 0 &&
             length > 0 ); 

    if ( _lorsNewDLList(&new_list) != LORS_SUCCESS )
    {
        return LORS_NO_MEMORY;
    }

    cur_offset = offset;
 

     while ( cur_offset < offset + length ){
        if ( ( ret = _lorsCreateDLNode( &node )) != LORS_SUCCESS)
            goto bail;
        /*fprintf(stderr, "cur_offset: %lld offset: %lld length: %lld.\n",
                    cur_offset, offset, length);*/
        _lorsSetDLNode(node, set, cur_offset, length-(cur_offset - offset));
        dll_append(new_list->list ,new_jval_v(node));
        cur_offset = node->offset + node->length ;
    }; 
    *list = new_list;

    return ( LORS_SUCCESS);

bail:
    *list = NULL;
    fprintf(stderr, "CreateDLNode failed.\n");
   _lorsFreeDLList(new_list);

   return ( ret);

};

int _lorsCreateDLListByBlockArray( LorsSet *set,
                                   longlong offset,
                                   longlong *block_array,
                                   int      array_len,
                                   __LorsDLList *dl_list ){

         __LorsDLList list,tmp_list;
         Dllist       node;
         int          i , ret = LORS_SUCCESS;

         _lorsNewDLList(&list);
         
         for ( i=0; i < array_len ; i ++){
              if ((ret = _lorsCreateDLList(set,offset,block_array[i],&tmp_list)) != LORS_SUCCESS ){
                  goto bail;
              };
              dll_traverse(node,tmp_list->list){
                  dll_append(list->list,new_jval_v(jval_v(node->val)));
              };
              offset = offset + block_array[i];
              /*_lorsFreeDLList(tmp_list);*/ /*free_dllist(tmp_list);*/
         };

         *dl_list = list;
         return (ret);

bail:
         _lorsFreeDLList(list);
         _lorsFreeDLList(tmp_list);
         return(ret);
        
};
                      
void _lorsFreeThreadPool ( __LorsThreadPool *tp ) 
{
     Dllist   thd_node;
     __LorsThreadInfo *tinfo;

     if ( tp != NULL ){
         if ( tp->threads != NULL ){
              free(tp->threads);
         };
#if 0
         if ( tp->extra_threads != NULL){
             dll_traverse(thd_node,tp->extra_threads){
                 tinfo = (__LorsThreadInfo *)jval_v(thd_node->val);
                 free(tinfo);
             };
             free_dllist(tp->extra_threads);
         };
#endif         
         tp->nthread = 0;
         free(tp);
     };
};

int _lorsCreateJobPool ( __LorsJobPool **jp )
{
   __LorsJobPool *job_pool;

   if ( (job_pool = (__LorsJobPool *)calloc(1,sizeof(__LorsJobPool)))==NULL){
       lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateJobPool: Out of Memory!\n");
       return ( LORS_NO_MEMORY);
   };

   pthread_mutex_init(&(job_pool->lock),NULL);
   pthread_cond_init(&(job_pool->cond),NULL);
   pthread_cond_init(&(job_pool->write_cond),NULL);
   pthread_cond_init(&(job_pool->read_cond),NULL);
   job_pool->job_list = new_dllist();
   job_pool->pending_jobs = make_jrb();
   job_pool->status = JOB_INIT;
   job_pool->njobs = 0;
   job_pool->fd = -1;
   job_pool->buf = NULL;
   job_pool->num_pending_jobs = 0;
   job_pool->cache = 0;
   *jp = job_pool;

   return (LORS_SUCCESS);
};
void _lorsFreeJob ( __LorsJob *job)
{
     if ( job != NULL ){
         switch(job->type){
         case J_COPY_NO_E2E:
            if ( job->src != NULL ){
               _lorsFreeDLList((__LorsDLList)job->src); 
            };
            break;
         case J_COPY_E2E:
            if ( job->src != NULL ){
               free_dllist((Dllist)job->src);
               /* _lorsFreeDLList((__LorsDLList)job->src);  */
            };
            break;
         case J_MCOPY_NO_E2E:
            if ( job->src != NULL ){
               _lorsFreeDLList((__LorsDLList)job->src); 
            };
            if ( job->dst != NULL) {
               free_dllist((Dllist)job->dst);
            };
            break;
         case J_MCOPY_E2E:
            if ( job->src != NULL ){
               free_dllist((Dllist)job->src);
               job->src = NULL;
            };
            if ( job->dst != NULL) {
               free_dllist((Dllist)job->dst);
               job->dst= NULL;
            };
            break;
         case J_DOWNLOAD:
            if (job->dst != NULL ){
                /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, job->dst);*/
                free(job->dst);
            };
         default:
            break;
         };
#if 0         
         if ( job->worker_threads != NULL ){
             jrb_free_tree(job->worker_threads);
         };
#endif
         free(job);
     };
};

void _lorsFreeJobPool ( __LorsJobPool *jp)
{
    Dllist     dl_node;
    __LorsJob  *job;

    if ( jp != NULL ){
        dll_traverse(dl_node,jp->job_list){
            job = (__LorsJob *)dl_node->val.v;
            assert(job != NULL);
            _lorsFreeJob(job);
        };
        /*queueFree(jp->q);*/
        free_dllist(jp->job_list);
        jrb_free_tree(jp->pending_jobs);
        free(jp); 
    };
};


void  _lorsAddJob ( __LorsJobPool *jp, __LorsJob *job)
{
     int ret;
     assert (( jp != NULL) && (job != NULL));

     ret = pthread_mutex_lock(&(jp->lock));
     assert (ret == 0 );
     assert(job->status == JOB_INIT );
     job->jp = jp;
     jp->njobs ++;
     dll_append(jp->job_list,new_jval_v(job));

     pthread_mutex_unlock(&(jp->lock));

};

/*#define LORS_CALC_SCORE(done,failure) ((done) - pow(1.1,(failure))  + 1)*/
#define LORS_CALC_SCORE(done,failure) (0)
static LorsMapping *
_lorsGetNextMapping(Dllist lm_list, __LorsJob *job){
    Dllist       lm_node = NULL,opt_node = NULL;
    LorsMapping  *lm = NULL, *opt_lm = NULL ;
    int          score,max_score, min_thread;
    double       max_prox;


again:
    lorsDebugPrint(D_LORS_VERBOSE,"_lorsGetNextMapping: looking next mapping.....");
    dll_traverse(lm_node,lm_list){
        lm = (LorsMapping*)jval_v(lm_node->val);
        pthread_mutex_lock(lm->lors_depot->dpp->lock);
        if ( (job->jp->max_thds_per_depot > 0)  && 
             (lm->lors_depot->nthread >= job->jp->max_thds_per_depot)){
            pthread_mutex_unlock(lm->lors_depot->dpp->lock);
            continue;
        };
        if ( opt_lm == NULL ){
            max_score = LORS_CALC_SCORE(lm->lors_depot->nthread_done,lm->lors_depot->nfailure);
            min_thread = lm->lors_depot->nthread;
            max_prox = lm->lors_depot->proximity;
            opt_lm = lm; 
            opt_node = lm_node;
        }else {
            score = LORS_CALC_SCORE(lm->lors_depot->nthread_done,lm->lors_depot->nfailure);
            if ( score > max_score) {
                max_score = score;
                min_thread = lm->lors_depot->nthread;
                max_prox = lm->lors_depot->proximity;
                opt_lm = lm;
                opt_node = lm_node;
            }else if ( score == max_score ){
                /*fprintf(stderr, "%s:%d prox: %f\n", lm->depot.host, */
                            /*lm->depot.port, lm->lors_depot->proximity);*/
                if ( lm->lors_depot->nthread < min_thread ){
                    min_thread = lm->lors_depot->nthread;
                    max_prox = lm->lors_depot->proximity;
                    opt_lm = lm;
                    opt_node = lm_node;
                }else if ( lm->lors_depot->nthread == min_thread ){

                    if (lm->lors_depot->proximity > max_prox ){
                        max_prox = lm->lors_depot->proximity;
                        opt_lm = lm;
                        opt_node = lm_node;
                    };
                };
            };
        };
        pthread_mutex_unlock(lm->lors_depot->dpp->lock);
    };
    
    if ( opt_lm != NULL) {
        pthread_mutex_lock(opt_lm->lors_depot->dpp->lock);
        opt_lm->lors_depot->nthread ++;
        dll_delete_node(opt_node);
        pthread_mutex_unlock(opt_lm->lors_depot->dpp->lock);
    };
    if ( opt_lm != NULL ){
        lorsDebugPrint(D_LORS_VERBOSE,"Done\n");
    }else {
        lorsDebugPrint(D_LORS_VERBOSE,"no found\n");
    };

    return (opt_lm);
    
};

void _lorsGetDownloadJobCleanup ( void *para){
    __LorsJobPool *jp = NULL;

    jp = (__LorsJobPool *)para;

    pthread_mutex_unlock(&(jp->lock));

    return;
};

int _lorsGetDownloadJob ( __LorsJobPool *jp, 
                          __LorsJob **jb ,
                          LorsMapping **mapping,
                          __LorsThreadInfo *tinfo){
    __LorsJob  *job = NULL, *tmp = NULL;
    __LorsJob  *delay_job = NULL;
    LorsMapping *lm;
    Dllist     node;
    int        ret = LORS_SUCCESS;
    int        k = 0;


    if ( jp == NULL || jb == NULL ) {
        return ( LORS_INVALID_PARAMETERS);
    };
    pthread_cleanup_push(_lorsGetDownloadJobCleanup,(void*)jp);

    ret = pthread_mutex_lock(&(jp->lock));
    assert(ret == 0);

    pthread_testcancel();

#if 0
    if ( jp->cache > 0 ){
        while ( jp->num_pending_jobs > jp->cache ){
            pthread_cond_wait(&(jp->read_cond),&(jp->lock));
        };
    };
#endif


    /*
     * firstly , find delay job which is the job that at least jp->n following 
     * jobs have already been done.
     */
again:

   lorsDebugPrint(D_LORS_VERBOSE,"_lorsGetDownloadJob: Looking for slow jobs\n");

    dll_traverse(node,jp->job_list){
        tmp = (__LorsJob *)(dll_val(node).v);
        if ( tmp->status != JOB_CHECKOUT ) {
            if (tmp->status == JOB_FAILED){
                pthread_exit(0);
            };
            continue;
        };
        if ( (tmp->n_threads >= jp->thds_per_job) ){
            continue;
        };
        
        if ( tmp->n_threads == 0 ){
            if ( (lm = _lorsGetNextMapping((Dllist)tmp->src,tmp)) != NULL){
                job = tmp;
                job->n_threads ++;
                break;
            }else{
                continue;
            };
        }else if(jp->n <= 0 ){
            if ( (lm = _lorsGetNextMapping((Dllist)tmp->src,tmp)) != NULL){
                job = tmp;
                job->n_threads ++;
                break;
            }else{
                continue;
            };
        }else if ( tmp->n_done_ahead >= jp->n ){
            if ( dll_empty(tmp->src) ){
                continue;
            };
            lm = _lorsGetNextMapping((Dllist)tmp->src,tmp);
            if ( lm == NULL ) {
                continue;
            };
            job = tmp;
            job->n_threads++;
            break;
        };
    };
   
    if ( (job == NULL) ){
        if ( jp->num_pending_jobs > jp->cache ){
            /* HACK: this will not stand up */
            if ( jp->status != LORS_SUCCESS ) {
                ret = jp->status;
                goto bail;
            }
            /*fprintf(stderr, "getjobpool wait 1\n");*/
            pthread_cond_wait(&(jp->read_cond),&(jp->lock)); /* WCR */
            /*fprintf(stderr, "getjobpool leave 1\n");*/
            goto again;
        };
    };

     
    /*
     * secondly, find the job that no thread working on 
     */
    if ( job == NULL ){
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsGetDownloadJob: Looking for new jobs\n");
        dll_traverse(node,jp->job_list){
            tmp = (__LorsJob *)(dll_val(node).v);
            if ( tmp->status == JOB_INIT ) {
                job = tmp;
                if ( dll_empty((Dllist)job->src)){
                    lm = NULL;
                }else{
                    lm = _lorsGetNextMapping((Dllist)job->src,job);
                    if ( lm == NULL ){
                        job = NULL;
                        if ( g_lors_demo )
                        {
#ifdef _MINGW
               		        fprintf(stderr,"DRAW DLBuffer %I64d %I64d\n",
				                tmp->src_offset,tmp->src_length);
#else
                            fprintf(stderr,"DRAW DLBuffer %lld %lld\n",
                                tmp->src_offset,tmp->src_length);
#endif
                            fflush(stderr);
                        }
                        tmp->status = JOB_CHECKOUT;
                        continue;
                    };
                };
                job->status = JOB_CHECKOUT;
                job->n_threads++;
                assert(job->n_threads == 1);
                if ( g_lors_demo )
                {
#ifdef _MINGW
                    fprintf(stderr,"DRAW DLBuffer %I64d %I64d\n",
		    	    job->src_offset,job->src_length);
#else
                    fprintf(stderr,"DRAW DLBuffer %lld %lld\n",
                            job->src_offset,job->src_length);
#endif
                    fflush(stderr);
                }
                break;
            };
        };
    };


    /*
     * otherwise, find the first non-finished job 
     */
    if ( job == NULL ){
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsGetDownloadJob: Looking for unfinished jobs\n");
        dll_traverse(node,jp->job_list){
            tmp = (__LorsJob *)(dll_val(node).v);
            if ( tmp->status != JOB_CHECKOUT ){
                continue;
            };
            if ( ((jp->thds_per_job >0) && (tmp->n_threads >= jp->thds_per_job)) ||
                 (dll_empty(tmp->src)) ){
                continue;
            };
            lm = _lorsGetNextMapping((Dllist)tmp->src,tmp);
            if (lm == NULL){
                continue;
            };
            job = tmp;
            assert(lm != NULL);
            job->n_threads++;
            break;
        };
    };

wait:

    if ( job == NULL ) {
        if ( jp->dst_cur_point < jp->src_offset + jp->src_length ){
            /*fprintf(stderr, "getjobpool wait 2\n");*/
            pthread_cond_wait(&(jp->cond),&(jp->lock));
            /*fprintf(stderr, "getjobpool leave 2\n");*/
            goto again;
        }else{
            ret = LORS_NO_JOBS_FOUND ;
        };
    }else {
        ret = LORS_SUCCESS;
    };
                
    *jb = job;
    *mapping = lm;
    if ( job != NULL ){ 
#ifdef _MINGW
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsGetDownloadJob[%d]: get job offset = %I64d\n",pthread_self(),job->src_offset);
#else
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsGetDownloadJob[%d]: get job offset = %lld\n",pthread_self(),job->src_offset);
#endif
#if 0        
        jrb_insert_int(job->worker_threads,tinfo->tid,new_jval_v(tinfo));
#endif
    };
bail:
    pthread_mutex_unlock(&(jp->lock));
    pthread_cleanup_pop(0);
    return (ret);
};

int _lorsGetJob ( __LorsJobPool *jp , __LorsJob **job ) 
{
     __LorsJob *jb = NULL,*tmp;
     Dllist    node;
     int       ret;

     /* for other job pool */
     if  ( jp == NULL || job == NULL )
           return ( LORS_INVALID_PARAMETERS);


     ret = pthread_mutex_lock(&(jp->lock));
     assert( ret == 0);  
      /*  
       * look for a uncheckout job
       */
      dll_traverse(node,jp->job_list) {
          tmp = (__LorsJob *)(dll_val(node).v);
          if ( tmp->status == JOB_INIT ){
               jb = tmp;
               break;
          }; 
      }; 

      if ( jb != NULL ){
          jb->status = JOB_CHECKOUT;
          ret = LORS_SUCCESS;
      }else {
          ret = LORS_NO_JOBS_FOUND;
      };    
     
 
      *job = jb;

       pthread_mutex_unlock(&(jp->lock)); 

       return( ret );
     
};

int 
_lorsCreateThreadPool ( __LorsJobPool *jp, int nthread, __LorsThreadPool **thread_pool, int timeout, int opts )
{
     pthread_t tid;
     pthread_attr_t attr;
     int       i,k;
     Dllist    node;
     __LorsThreadPool *tp;

     if ( (jp == NULL ) || ( thread_pool == NULL ) ){ 
            lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateThreadPool: Invalid paramters!\n"); 
            return ( LORS_INVALID_PARAMETERS);
     };
     
     if ( nthread <= 0 ) {
         nthread = jp->njobs;
     };
     /*
      * else if ( jp->njobs < nthread ){
         nthread = jp->njobs;
     };
      */  
   
     if ( (tp = (__LorsThreadPool *)calloc(1,sizeof(__LorsThreadPool)))== NULL ){
            lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateThreadPool: Out of Memory!\n");
            return (LORS_NO_MEMORY);
     };

     jp->thread_pool = (__LorsThreadPool*)tp;
     
     if ( (tp->threads = (__LorsThreadInfo *)calloc ( nthread,sizeof(__LorsThreadInfo))) == NULL){
            free(tp); 
            lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateThreadPool: Out of Memory!\n");
            return (LORS_NO_MEMORY);
     };

     k = 0;
   
 
     for ( i = 0 ; i <nthread ; i ++ ){
         pthread_attr_init(&(tp->threads[i].attr));
         pthread_attr_setscope(&(tp->threads[i].attr),PTHREAD_SCOPE_SYSTEM);
         pthread_attr_setdetachstate(&(tp->threads[i].attr),PTHREAD_CREATE_JOINABLE);
         tp->threads[i].tp = tp;

 
         tp->threads[i].jp = jp;
         tp->threads[i].job = NULL;
         tp->threads[i].timeout = timeout;
         tp->threads[i].status = THD_START;
         tp->threads[i].err_code = LORS_SUCCESS;
         tp->threads[i].opts = opts;
         if ( pthread_create(&(tid),&(tp->threads[i].attr),_lorsWorkThread,(void*)&(tp->threads[i])) != 0 ){
              break;
         };
         k++;
         tp->threads[i].tid = tid;
         lorsDebugPrint(D_LORS_VERBOSE,"_lorsCreateThreadPool: New working thread [%d] has been created.\n",tp->threads[i].tid);
     };

     if ( k == 0 ){
         free(tp->threads);
         free(tp);
         return ( LORS_PTHREAD_CREATE_FAILED );
     };

     tp->nthread = k;

     *thread_pool = tp;
     return ( LORS_SUCCESS );

};

void *_lorsKillWorkerThreads ( __LorsThreadInfo *tinfo)
{
     __LorsThreadPool   *tp;
     int                i;


     assert(tinfo != NULL);
     tp = tinfo->tp;
     assert(tp != NULL);

     for ( i = 0 ; i < tp->nthread ; i ++){
         if ( tp->threads[i].tid != tinfo->tid ) {
              pthread_cancel(tp->threads[i].tid);
         };
     };   
     
};

int _lorsJoinThread( __LorsThreadPool *tp)
{
    int i ,ret = LORS_SUCCESS;
    Dllist thd_node;
    __LorsThreadInfo *tinfo;
    
    assert(tp != NULL);
#if 0
    pthread_mutex_lock(&(tp->lock));
#endif
    for(i=0;i< tp->nthread; i++){
        pthread_join(tp->threads[i].tid,NULL);
        pthread_attr_destroy(&(tp->threads[i].attr));
        lorsDebugPrint(D_LORS_VERBOSE,"lorsJoinThread: [%d] status = %d\n",
                        tp->threads[i].tid , tp->threads[i].status);
        if ( tp->threads[i].status != THD_SUCCESS )
        {   if ( tp->threads[i].err_code != LORS_SUCCESS ){
                 ret =  tp->threads[i].err_code;
                 /*fprintf(stderr, "err_code: %d\n", tp->threads[i].err_code );*/
            };
        }
    }

#if 0

    dll_traverse(thd_node,tp->extra_threads){
        tinfo = (__LorsThreadInfo *)jval_v(thd_node->val);
        pthread_join(tinfo->tid,NULL);
        pthread_attr_destroy(&(tinfo->attr));
        lorsDebugPrint(D_LORS_VERBOSE,"lorsJoinThread: [%d] status = %d\n",
                        tinfo->tid , tinfo->status);
        if ( tinfo->status != THD_SUCCESS )
        {   if ( tinfo->err_code != LORS_SUCCESS ){
                 ret =  tinfo->err_code;
            };
        }
    };

    pthread_mutex_unlock(&(tp->lock));
#endif

    return ret;
}


void  *_lorsWorkThread (void *v)
{
     __LorsThreadInfo *tinfo;
     __LorsJobPool    *jp;
     __LorsJob        *job;
     pthread_t        tid;
     LorsMapping      *lm;
     int              lefttime;
     time_t           begin_time;
     int              ret_val;

     lorsDebugPrint(D_LORS_VERBOSE,"_lorsWorkThread[%d]: Beginning to work...\n",pthread_self());
     /*
      * set pthread cancelable 
      */
     ret_val = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED,NULL);
     assert( ret_val == 0);
     ret_val = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); 
     assert( ret_val == 0);
     tinfo = (__LorsThreadInfo *)v;
     pthread_cleanup_push(_lorsCleanupThread,(void *)tinfo);

     tinfo->tid = pthread_self();

     lorsDebugPrint(D_LORS_VERBOSE, "Starting thread: %d\n", tinfo->tid);
     assert ( ( tinfo->status == THD_START )
             && ( tinfo->job == NULL) 
             && ( tinfo->jp != NULL) );

     jp = tinfo->jp;
     begin_time = time(0);     
   
     tinfo->status = THD_EXEC; 
     while ( 1 ) 
     {
        /* 
         * get a new job from job pool
         */
        if ( jp->type == J_DOWNLOAD){
            if ((ret_val = _lorsGetDownloadJob(jp,&job,&lm,tinfo) != LORS_SUCCESS)){
                break;
            };
        }else{
            if ( (ret_val = _lorsGetJob(jp, &job)) != LORS_SUCCESS ){
                break;
            };
        };

        tinfo->job = job;
        if ((lefttime = _lorsRemainTime(begin_time,tinfo->timeout)) < 0 ){
             ret_val = LORS_TIMEOUT_EXPIRED;
             break;
        }; 

        if ( jp->type != J_DOWNLOAD) {
            job->timeout = lefttime;
        };
        switch ( job->type ){
        case J_CREATE:
            ret_val = _lorsDoCreate(job,tinfo);
            /*fprintf(stderr, "ret_val dc: %d\n", ret_val);*/
            break;
        case J_STORE:
            ret_val = _lorsDoStore(job,tinfo);
            break;
        case J_DOWNLOAD:
            ret_val = _lorsDoDownload(job,tinfo,lm,lefttime);
            break;
        case J_MCOPY_NO_E2E:
            ret_val = _lorsDoMCopyNE(job,tinfo);
            break;
        case J_MCOPY_E2E:
            ret_val = _lorsDoMCopyE2E(job,tinfo);
            break;
        case J_COPY_NO_E2E:
            ret_val = _lorsDoCopyNE(job,tinfo);
            /*fprintf(stderr, "ret_val : %d\n", ret_val);*/
            break;
        case J_COPY_E2E:
            ret_val = _lorsDoCopyE2E(job,tinfo);
            /*fprintf(stderr, "ret_val e: %d\n", ret_val);*/
            break;
        case J_CONDITION:
            ret_val = _lorsDoCondition(job,tinfo);
            break;
        case J_STAT:
            ret_val = _lorsDoStat(job,tinfo);
            break;
        case J_REFRESH:
            ret_val = _lorsDoRefresh(job, tinfo);
            break;
        case J_TRIM:
            ret_val = _lorsDoTrim(job, tinfo);
            break;
        case J_CREATE_STORE:
            ret_val = _lorsDoCreateStore(job,tinfo);
            break;
        };
   
        if ( ret_val != LORS_SUCCESS ) 
            break; 
     }; 

     

     /* finish all jobs or meet a error
      * change thread status
      */
     if ( ret_val == LORS_NO_JOBS_FOUND ){
          tinfo->status = THD_SUCCESS;
          ret_val = LORS_SUCCESS;
     }else {
          tinfo->status = THD_FAILED;
          pthread_mutex_lock(&jp->lock);
          jp->status = ret_val;
          /* FIXES deadlock on write from Timeout */
          /*fprintf(stderr, "Broadcasting after DoDownload failure: %d: %d\n", */
                  /*ret_val, pthread_self());*/
          pthread_cond_broadcast(&(jp->write_cond));
          pthread_mutex_unlock(&jp->lock);
     }; 
     tinfo->err_code = ret_val;

     lorsDebugPrint(D_LORS_VERBOSE,"_lorsWorkThread[%d]: Done\n",pthread_self());

     /*
      * pop up clean up routine
      */
     pthread_cleanup_pop(0);

     pthread_exit(0);
};

void 
_lorsCleanupThread ( void *v)
{
    __LorsThreadInfo  *tinfo;

    assert(v != NULL );
    tinfo = (__LorsThreadInfo *)v;

    tinfo->status = THD_CANCELED; 
    tinfo->err_code = LORS_PTHREAD_CANCELED;    
};
