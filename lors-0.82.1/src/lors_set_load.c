#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <lors_api.h>
#include <lors_error.h>
#include <lors_opts.h>
#include <lors_util.h>
#include <jval.h>
#include <jrb.h>
#include <string.h>

#include "lors_job.h"
#include <lbone_client_lib.h>

extern ulong_t  g_lm_id ;
#define lorsDemoPrint fprintf
int g_load_thread_count;
pthread_mutex_t load_thread_lock;


long    lorsSetLoad (LorsSet * set,
                       char *buffer,
                       longlong offset,
                       long length,
                       ulong_t block_size,
                       LorsConditionStruct *lc,
                       int nthreads,
                       int timeout,
                       int opts) {

    return ( lorsSetRealTimeLoad(set,
                            buffer,
                            -1,
                            offset,
                            length,
                            block_size,
                            0, /* pre_buffer */
                            0, /* cache */
                            lc,
                            nthreads,
                            timeout,
                            2,
                            1,
                            -1,
                            opts) );
    
};

longlong    lorsSetRealTimeLoad (LorsSet * set,
                       char *buffer,
                       int  fd,
                       longlong offset,
                       longlong length,
                       ulong_t block_size,
                       int  pre_buf,
                       int  cache,
                       LorsConditionStruct *lc,
                       int nthreads,
                       int timeout,
                       int max_thds_per_depot,
                       int thds_per_job,
                       int progress_n,
                       int opts)
{
    int                   ret, i;
    __LorsThreadPool      *tp = NULL;
    __LorsJobPool         *jp = NULL;
    __LorsJob             *job = NULL ;
    Dllist                dl_node,job_node,lm_node;
    __LorsDLList          list = NULL,tmp_list = NULL;
    __LorsDLNode          *node;
    longlong                  dst_offset;
    int                   n_task;
    longlong              *block_array = NULL ;
    int                   array_len;
    ulong_t               data_block;    
    Dllist                new_dl_node;
    LorsMapping           *map = NULL,*lm = NULL;
    int                   has_e2e = 0;
    pthread_t             write_tid;
    int                   tmp_thread_num = 0;
    LorsDepotPool         *dpp = NULL;
    ret = LORS_SUCCESS;

    pthread_mutex_init(&load_thread_lock, NULL);
    if ( g_lors_demo )
    {
	    pthread_mutex_lock(&load_thread_lock);	
        g_load_thread_count = 0;
	    pthread_mutex_unlock(&load_thread_lock);	
        lorsDemoPrint(stderr, "MESSAGE 1 SetLoad\n");
	    fflush(stderr);
    }
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: Begin to download...\n");
    if (( set == NULL ) || ( offset < 0LL ) || ( length < 0 ) ) { 
        ret = LORS_INVALID_PARAMETERS;
#ifdef _MINGW
        fprintf(stderr, "set: 0x%x offset: %I64d length: %I64d\n",
                set, offset, length);
#else
        fprintf(stderr, "set: 0x%x offset: %lld length: %lld\n",
                set, offset, length);
#endif
        return ret;
    };

    if ( (buffer == NULL ) && ( fd < 0 ) ){
        ret = LORS_INVALID_PARAMETERS;
        fprintf(stderr, "buffer==NULL && fd< 0\n");
        return ret;
    };

    if ((ret = _lorsCreateDLList(set,offset,length,&list )) != LORS_SUCCESS ){
        goto bail;
    };
    i = 0;
    dll_traverse(dl_node,list->list){ 
         i++;
    };

    /* set up prebuffer number for stream downloading */
    if (pre_buf <  0 ) {
        pre_buf = 1;
    };
   
    if ( !jrb_empty(set->mapping_map) ){
       map = (LorsMapping *)jval_v(jrb_first(set->mapping_map)->val);
       has_e2e = _lorsHasE2E(map);  
    };
 
     if (( nthreads > 0 ) || (block_size > 0)){
        if ( nthreads <= 0 ){
             data_block = block_size;
        }else if (block_size <= 0 ){
             data_block = length/nthreads + 1;
        }else {
            data_block = block_size;
             /*data_block = (((length/nthreads + 1) > block_size) ? block_size : (length/nthreads +1));*/
        };

        if ( has_e2e ){
            block_array = _lorsNewBlockArrayE2E(set,length,offset,data_block,list,&array_len);
        } else {
            block_array = _lorsNewBlockArray(length,data_block,&array_len);
        };
        if ( block_array == NULL ){
             ret = LORS_NO_MEMORY;
             goto bail;
        }; 
        if ((ret = _lorsCreateDLListByBlockArray(set,offset,block_array,
                                array_len,&tmp_list)) != LORS_SUCCESS ){
              goto bail;
        };
        _lorsFreeDLList(list);
        list = tmp_list;
        tmp_list = NULL;
    };   

    dll_traverse(dl_node,list->list){
         node = (__LorsDLNode *)jval_v(dl_node->val);
         lorsDebugPrint(D_LORS_VERBOSE, "========================================\n");
#ifdef _MINGW
         lorsDebugPrint(D_LORS_VERBOSE, "offset = %I64d   ",node->offset);
         lorsDebugPrint(D_LORS_VERBOSE, "length = %I64d  \n",node->length);
#else
         lorsDebugPrint(D_LORS_VERBOSE, "offset = %lld   ",node->offset);
         lorsDebugPrint(D_LORS_VERBOSE, "length = %lld  \n",node->length);
#endif
         dll_traverse(new_dl_node,node->mappings){
             map = (LorsMapping *)jval_v(new_dl_node->val);
             lorsDebugPrint(D_LORS_VERBOSE, "host:port = [%s:%d]\n",
                     map->depot.host,map->depot.port); 
         };
    };
/*getchar();*/
 
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: Creating job pool...\n");
    if ((ret = _lorsCreateJobPool(&jp) != LORS_SUCCESS )){
        goto bail;
    };
    jp->type = J_DOWNLOAD;
    jp->src_offset = offset;
    jp->src_length = length;
    jp->src_cur_point = jp->src_offset;
    jp->dst_cur_point = jp->src_offset;
    jp->pre_buf = pre_buf;
    if ( cache <= 0 ) {
        jp->cache = 0;
    }else{
        jp->cache = cache;
    };
    jp->num_pending_jobs = 0;
    if ( progress_n < 0 ) {
        jp->n = 2000;
    }else{
        jp->n = progress_n;
    };
    jp->max_thds_per_depot = max_thds_per_depot;
    jp->thds_per_job = thds_per_job;
    if ( buffer == NULL ){ /* use file descriptor */
        jp->fd = fd;
    } else {                /* use data buffer */
        jp->buf = buffer;
        jp->fd = -1;
    };


    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: Adding jobs .....\n");
    dst_offset = 0;
    n_task = 0;
    dll_traverse(dl_node,list->list){
        job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
        if ( job == NULL ){
            ret = LORS_NO_MEMORY;
            goto bail;
        };
        pthread_mutex_init(&(jp->lock),NULL);
        job->n_threads = 0;
#if 0
        job->worker_threads = make_jrb(); 
#endif
        job->type = J_DOWNLOAD;
        job->status = JOB_INIT;
        job->n_done_ahead = 0;
        node = (__LorsDLNode *)jval_v(dl_node->val);
        job->src = (void*)node->mappings;
        dll_traverse(lm_node,node->mappings){
            lm = (LorsMapping *)jval_v(lm_node->val);
            lm->lors_depot = lorsGetDepotByName(lm->dp,&(lm->depot));
            if ( dpp == NULL ){
                dpp = lm->dp;
            };
            assert(lm->lors_depot != NULL);
            pthread_mutex_lock(lm->dp->lock);
            lm->lors_depot->nthread = 0;
            lm->lors_depot->nthread_done = 0;
            lm->lors_depot->nfailure = 0;
            pthread_mutex_unlock(lm->dp->lock);
        };
        job->dst = NULL;
        job->src_offset = node->offset;
        job->src_length = node->length;
        job->dst_offset = dst_offset;
        job->dst_length = node->length;
        dst_offset = dst_offset + node->length;

        job->extra_data = lc;

        _lorsAddJob(jp,job);
#ifdef _MINGW
        lorsDebugPrint(D_LORS_VERBOSE,"     lorsSetLoad: Job[%I64d:%I64d] is added.\n",node->offset, node->length);  
#else
        lorsDebugPrint(D_LORS_VERBOSE,"     lorsSetLoad: Job[%lld:%lld] is added.\n",node->offset, node->length);  
#endif        
	n_task++;
   };

    
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: Creating working threads...\n");
    if ( max_thds_per_depot > 0 ){
        tmp_thread_num = max_thds_per_depot * _lorsGetNumOfDepot(dpp);
        if ( nthreads <= 0 ){
            nthreads = tmp_thread_num;
        };
        jp->max_thds_per_depot = max_thds_per_depot;

    }else{
        if ( nthreads > 0 ){
            jp->max_thds_per_depot = nthreads / _lorsGetNumOfDepot(dpp);
        }else{
            jp->max_thds_per_depot = jp->njobs / _lorsGetNumOfDepot(dpp);
        };
        jp->max_thds_per_depot = -1;
    };
        
    /*queueSetLength(jp->q, 3); */

    ret = _lorsCreateThreadPool(jp,nthreads,&tp,timeout,opts);
    if ( ret != LORS_SUCCESS ){
       goto bail;
    };

    ret = _lorsCreateOutputThread(&write_tid,jp);
    if (ret != LORS_SUCCESS){
        goto bail;
    };
  
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: %d working threads have been created successfully.\n",tp->nthread);

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: Waitting worker thread return \n");

    pthread_join(write_tid,NULL);
#if !defined( _CYGWIN ) && !defined ( _MINGW )
    _lorsJoinThread(tp);
#endif
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetLoad: Checking all jobs...\n");
    dst_offset = offset;
    dst_offset = jp->dst_cur_point;
    ret = jp->status;

#if 0
    dll_traverse(job_node,jp->job_list)
    {
        job = (__LorsJob *)jval_v(job_node->val);
        if ( job->status != JOB_COMMIT )
        {
             ret = job->err_code;
             pthread_mutex_unlock(&(jp->lock));
             goto bail;
        };
        /*assert(dst_offset == job->dst_offset);*/
        dst_offset = dst_offset + job->dst_length; 
    };
#endif 
    
bail:

    if ( block_array != NULL ) free(block_array);
    _lorsFreeThreadPool(tp);
    _lorsFreeJobPool(jp);
    _lorsFreeDLList(list);
    _lorsFreeDLList(tmp_list);

    /*fprintf(stderr, "dst_offset: %lld offset: %lld ret: %d\n", dst_offset, offset, ret);*/
    if ( dst_offset - offset <= 0 ){
        return ret ;
    }else{
        return (dst_offset-offset);
    };
}

static int _lorsIsFatalIBPError(int ibp_errno){
    if ( ibp_errno == IBP_E_CAP_NOT_FOUND ||
         ibp_errno == IBP_E_CAP_NOT_WRITE ||
         ibp_errno == IBP_E_CAP_NOT_READ ||
         ibp_errno == IBP_E_CAP_NOT_MANAGE ||
         ibp_errno == IBP_E_INVALID_WRITE_CAP ||
         ibp_errno == IBP_E_INVALID_READ_CAP ||
         ibp_errno == IBP_E_INVALID_MANAGE_CAP ||
         ibp_errno == IBP_E_WRONG_CAP_FORMAT ||
         ibp_errno == IBP_E_CAP_ACCESS_DENIED ||
         ibp_errno == IBP_E_WOULD_EXCEED_LIMIT ){
        return 0;
    }else{
        return 1;
    };
};

void _lorsDoDownloadCleanup( void *para){
    void **args;
    LorsMapping *lm;
    LorsDepot   *depot;

    args = (void **)para;

    if ( args[1] != NULL ){
        /*fprintf(stderr, "Free %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, args[1]);*/

        lorsDebugPrint(D_LORS_VERBOSE, "freeing buf in DownloadCleanup: 0x%x\n", args[1]);
        free(args[1]);
    };
    lm = (LorsMapping *)args[0];
    if ( lm != NULL){
        depot = lm->lors_depot;
        pthread_mutex_lock(depot->dpp->lock);
        depot->nthread --;
        pthread_mutex_unlock(depot->dpp->lock);
    };
    free (args);
};

int
_lorsDoDownload ( __LorsJob *job,
                  __LorsThreadInfo *tinfo,
                  LorsMapping  *lm,
                  int            timeout)
{
     Dllist          lm_list = NULL, lm_node = NULL;
     LorsMapping     *mapping = NULL ;
     char              *buf = NULL ;
     char            *tmpbuf = NULL;
     LorsDepot       *depot = NULL ;
     ulong_t           nbytes,nread;
     int                try1 = 0;
     unsigned long   p_off;
     unsigned long   p_len;
     unsigned long   l_off_in_mapping = 0;
     unsigned long   l_len_in_mapping = 0;
     unsigned long   log_e2e_offset = 0;
     struct ibp_timer timer;
     int             ret,i;
     time_t          begin_time,left_t;
     struct timeval    begin_download,end_download;
     double          bandwidth;
     __LorsJobStatus job_status;
     Dllist          dll_node;
     LorsConditionStruct *lc;
     void            **paras;
     double          t1, t2;
     ulong_t         demo_len;

     assert(job != NULL && tinfo != NULL );
     lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoDownload[%d]: begin download...\n",pthread_self());

     begin_time = time(0);

     ret = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,NULL); 

     /*fprintf(stderr, "buffer length: %d\n", job->dst_length);*/
     if ( (buf = (char*)calloc(job->dst_length,sizeof(char))) == NULL ){
         ret = LORS_NO_MEMORY;
         goto bail;
     };
     /*fprintf(stderr, "Allocate %s[%d]: buf 0x%x\n", __FUNCTION__, __LINE__, buf);*/

     /* TODO: paras[] may cease to exist upon thread cancelation.
      *       it is also possible for the thread to be canceled between the
      *         allocate, and the cleanup_push.
      */
     paras = (void **)malloc(sizeof(void*)*2);
     paras[0] = (void *)lm;
     paras[1] = (void *)buf;
     pthread_cleanup_push(_lorsDoDownloadCleanup,paras);

     ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL); 

     if ( lm == NULL){
         job_status = JOB_FAILED;
         ret = LORS_HAS_HOLE;
         goto bail;
     };

     lc = (LorsConditionStruct *)job->extra_data;
     
     mapping  = lm;
     /* changed to include alloc_offset 3/2/03 */
     l_off_in_mapping = job->src_offset - mapping->exnode_offset + mapping->alloc_offset;
     l_len_in_mapping = job->src_length;
     if ( _lorsHasE2E(mapping) )
     {
          _lorsGetPhyOffAndLen(mapping,
                         l_off_in_mapping,
                         l_len_in_mapping,
                         &p_off,
                         &p_len,
                         &log_e2e_offset,
                         mapping->e2e_bs);

          lorsDebugPrint(D_LORS_VERBOSE, "e2e_bs == %d\n", mapping->e2e_bs);
          lorsDebugPrint(D_LORS_VERBOSE, "Translate %d %d => %d %d\n", 
                 l_off_in_mapping, l_len_in_mapping,
                 p_off, p_len);
     } else {
          p_off = l_off_in_mapping;
          p_len = l_len_in_mapping;
     };

     depot = mapping->lors_depot;
     assert(depot != NULL);
     if ( _lorsHasE2E(mapping) ){
         tmpbuf = (char*)calloc(p_len,sizeof(char));
         if ( tmpbuf == NULL ){
             ret = LORS_NO_MEMORY;
             goto bail;
         }; 
     }else {
         tmpbuf = buf;
     };
     gettimeofday(&begin_download,NULL);
     nread = 0; 
     try1 = 0;
     if ( g_lors_demo )
    {
#ifdef _MINGW
        fprintf(stderr,"DRAW Arrow2 from %s:%4d %I64d %I64d\n",
                mapping->depot.host,mapping->depot.port,
                job->src_offset,job->src_length);
#else
        fprintf(stderr,"DRAW Arrow2 from %s:%4d %lld %lld\n",
                mapping->depot.host,mapping->depot.port,
                job->src_offset,job->src_length);
#endif
	    fflush(stderr);

        /* begin time */
        t1 = getTime();
	    pthread_mutex_lock(&load_thread_lock);
	    g_load_thread_count++;
	    pthread_mutex_unlock(&load_thread_lock);
    }
    while (nread < p_len){ 
         if ((left_t = _lorsRemainTime(begin_time,timeout)) < 0) {
             ret = LORS_TIMEOUT_EXPIRED;
             goto bail;
         }; 
         if ( timeout > 0 ){
             timer.ServerSync = left_t + 1;
             timer.ClientTimeout = left_t -1;
         } else {
             timer.ServerSync = 30;
             timer.ClientTimeout = 0;
         };

#ifdef _MINGW
         lorsDebugPrint(1, "Load %24s:%4d %8I64d %8d\n",
                        mapping->depot.host, mapping->depot.port, 
                        mapping->exnode_offset+p_off,
                        p_len - nread);
#else
         lorsDebugPrint(1, "Load %24s:%4d %8lld %8d\n",
                        mapping->depot.host, mapping->depot.port, 
                        mapping->exnode_offset+p_off,
                        p_len - nread);
#endif
         /*fprintf(stderr, "BEFORE READCAP: %s\n", mapping->capset.readCap);*/
         nbytes = IBP_load(mapping->capset.readCap,&timer,tmpbuf + nread,p_len - nread,p_off + nread );
         /*fprintf(stderr, "AFTER READCAP: %s\n", mapping->capset.readCap);*/
        if (nbytes == 0) {
             if ( IBP_errno == IBP_E_CAP_NOT_FOUND && try1 < 1)
             {
                 /*do nothing.*/
                 /* a depot bug will temporarily lose valid capabilites.  to
                  * avoid excessive failure, we retry a few times to see if it
                  * 'finds' the cap again. 
                  */
                 try1 ++;
             } else 
             {
                 ret = LORS_IBP_FAILED + IBP_errno;

                 if ( _lorsHasE2E(mapping) ) free(tmpbuf);
                 pthread_mutex_lock(depot->dpp->lock);
                 /*if ( _lorsIsFatalIBPError(IBP_errno) ){
                    depot->nfailure ++;
                 };*/

                 lorsDebugPrint(1, "Load missed on one attempt (non-critical): %d\n",ret);
                 depot->nthread --;
                 pthread_cond_broadcast(&(job->jp->cond));
                 pthread_mutex_unlock(depot->dpp->lock);
                 goto bail;
             }
         };
         nread = nread + nbytes; 
     };
     if ( g_lors_demo )
     {
        /* end time */
        t2 = diffTime(t1);
        demo_len = nread;
	    pthread_mutex_lock(&load_thread_lock);
	    lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", 
                g_load_thread_count*(demo_len*8.0)/1024.0/1024.0/t2);
        g_load_thread_count--;
	    pthread_mutex_unlock(&load_thread_lock);
	    fflush(stderr);
     }
     pthread_mutex_lock(depot->dpp->lock);
     depot->nthread --;
     if ( depot->nfailure > 0) { depot->nfailure -- ;};
     pthread_cond_broadcast(&(job->jp->cond));
     pthread_mutex_unlock(depot->dpp->lock);
     if ( _lorsHasE2E(mapping) )
     {
         /* HACK: i've added l_len_in_mapping and p_off to the parameters to
          * allow for proper memcpy() after e2e unconditioning, with weird
          * download blocksizes. previously it segfaulted. */
         /* I need the logical e2e offset against which to subtract inside
          * UnconditionData */

         /*fprintf(stderr, "LogicalLength: %ld job->dst_length: %lld\n",*/
                 /*l_len_in_mapping, job->dst_length);*/
         if ((ret = _lorsUnconditionData(mapping, lc, 
                         tmpbuf,buf,
                         l_off_in_mapping,
                         l_len_in_mapping,
                         log_e2e_offset,
                         p_len)) != LORS_SUCCESS )
         {
                free(tmpbuf);
                tmpbuf = NULL;
                goto bail;
         };
         free(tmpbuf); 
     };

     job_status = JOB_COMMIT;
     ret = LORS_SUCCESS;


bail:

     if ( ret != LORS_SUCCESS )
          job_status = JOB_FAILED; 
     if ( (ret  = _lorsCommitDownloadJob(job,job_status,ret,mapping,depot,buf)) != LORS_SUCCESS ){
         _lorsKillWorkerThreads(tinfo);
     };
     pthread_cleanup_pop(0);
     free(paras);
     return(ret);     
};


