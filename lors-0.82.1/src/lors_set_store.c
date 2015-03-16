#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <lors_api.h>
#include <lors_error.h>
#include <lors_opts.h>
#include <lors_util.h>
#include <lors_misc.h>
#include <jval.h>
#include <jrb.h>
#include <string.h>

#include "lors_job.h"
#include <lbone_client_lib.h>
#include <lors_libe2e.h>

extern ulong_t  g_lm_id ;
int g_store_thread_count;

pthread_mutex_t store_thread_lock;

#define lorsDemoPrint fprintf

/* 
 *  JOB-> Source is the user data data
 *  JOB-> Destination is the LorsMapping
 */
int    lorsSetStore (LorsSet * set,
                     LorsDepotPool * dp,
                     char *buffer,
                     longlong offset,
                     longlong length,
                     LorsConditionStruct *lc,
                     int nthreads,
                     int timeout,
					 socket_io_handler *handle,
                     int opts)
{
    int ndata_block = 0, ndata_mod_block;
    int do_padding = 0;

    LorsMapping        *lm = NULL,**lm_list = NULL;
    LorsBoundary       *lb = NULL;
    double              tbegin, tdiff;
    longlong           *list = NULL;
    longlong            block_offset;
    char               *cond_buffer = NULL;
    int                 i,j,k,tmp;
    int                 blocks, list_len;
    int                 avoid = 0;
    Dllist              dl_unjoined, dl_node, dl_list;
    JRB                 jrb_node;
    int                 success_cnt= 0;
    LorsDepot          *depot = NULL; 
    __LorsThreadPool   *tp = NULL;
    __LorsJobPool      *jp_store = NULL;
    __LorsJobPool      *jp_condition = NULL;

    __LorsJob          *job = NULL, *job_store = NULL;
    int                 ret_val, ret;
    /* TODO: Question: how does a user pass all data at once, if it does not
     * align to 'data_blocksize'? i.e. data_blocksize = 300 and length = 1000.
     * do we return 900, and leave them to call store again for 100, or allow
     * the user to pad? or pad ourselves?. the consequences of padding ourself
     * could be bad.
     */ 
    if ( ( length <= 0) || (offset < 0) || ( set == NULL) || (dp == NULL) ) 
    {
        return LORS_INVALID_PARAMETERS;
    }
    pthread_mutex_init(&store_thread_lock, NULL);
    if ( g_lors_demo )
    {
        pthread_mutex_lock(&store_thread_lock);
        g_store_thread_count = 0;
        pthread_mutex_unlock(&store_thread_lock);
        lorsDemoPrint(stderr, "MESSAGE 1 SetStore\n");
        fflush(stderr);
    }
#if 0
    ndata_block = length / set->data_blocksize;
    ndata_mod_block = length % set->data_blocksize;
    if ( length == set->max_length )
    {
        if ( ndata_mod_block != 0 )
        {
            /* if ndata_mod_block is the last un-data_blocksize piece of data,
             * then we can handle it this pass, otherwise, we cannot. */
            do_padding = 1;
        }
    } else if ( length <= set->max_length - set->curr_length) 
    {
        if ( set->curr_length == (set->max_length - length) )
        {
            /* this is the remaining data */
            if ( ndata_mod_block != 0 )
            {
                do_padding = 1;
            }
        } else 
        {
            /* last portion of data */
            if ( ndata_block > 0 )
            {
                /* ok */
            } else 
            {
                /* error not enough data to upload*/
                lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : MISALINED LENGTH.\n");
                return LORS_MISALINED_LENGTH;
            }
        }
    } else if ( length > set->max_length - set->curr_length )
    {
        /* they've sent in mroe than we can chew */
        /* process only what we can chew */
        length = set->max_length - set->curr_length;
        ndata_block = length / set->data_blocksize;
        ndata_mod_block = length % set->data_blocksize;
        if ( ndata_mod_block != 0 ) do_padding = 1;
    }
#endif

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : creating condition job pool.\n");
    if ( (ret_val = _lorsCreateJobPool(&jp_condition)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : can't create job pool!\n");
        goto bail; 
    }; 
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : creating store job pool.\n");
    if ( (ret_val = _lorsCreateJobPool(&jp_store)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : can't create job pool!\n");
        goto bail; 
    }; 

    /* TODO: check that length is atleast as long as data_blocksize. */
    list = _lorsNewBlockArray(length, set->data_blocksize, &list_len);
    if (list == NULL ) {
       lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetStore : Out of memroy!\n");
       ret_val = LORS_NO_MEMORY;
       goto bail;
    };
    lorsDebugPrint(D_LORS_VERBOSE, "\t --* list_len: %d\n", list_len);
    for(i=0; list[i] != 0 && i < 10 ; i++)
    {
#ifdef _MINGW
        lorsDebugPrint(D_LORS_VERBOSE, "\t --* list[%d] = %I64d\n", i, list[i]);
#else
        lorsDebugPrint(D_LORS_VERBOSE, "\t --* list[%d] = %lld\n", i, list[i]);
#endif
    }

    tbegin = time(0);

    jp_condition->type = J_CREATE;
    jp_condition->src_offset = 0;
    jp_condition->src_length = length;

    block_offset = 0;
    for ( i=0; block_offset < length ; i++, block_offset+=set->data_blocksize)
    {
        /* create a new job and put into job pool */
        job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
        if ( job == NULL ) {
            ret_val = LORS_NO_MEMORY;
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetStore :Out of memory!\n");
            goto bail;
        };
        job->type = J_CONDITION;
        job->status = JOB_INIT;
        job->src = buffer+block_offset;
        job->src_length = list[i];
        job->src_offset = block_offset;
		job->handle = handle;
            /* TODO: the conditioned data will be returned in dst */
        job->dst = NULL;
        job->extra_data = lc;

        /* add to job pool */

        _lorsAddJob(jp_condition,job);
    }
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : Creating working threads "
                                  "for conditioning.\n");
    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr, "MESSAGE 2 End2End Conditioning\n");
        fflush(stderr);
    }
    ret_val = _lorsCreateThreadPool(jp_condition,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
    {
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : _lorsCreateThreadPool failed..\n");
        goto bail;
    }
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : Waiting Condition threads return.\n");
    ret_val = _lorsJoinThread(tp);
    if ( ret_val != LORS_SUCCESS )
        goto bail;

    /* PERFORM STORE OPERATIONS */
    jp_store->type = J_CREATE;
    jp_store->src_offset = 0;
    jp_store->src_length = length;

    /* create a DepotList from DepotPool */
    lorsCreateListFromPool(dp, &dp->dl, dp->min_unique_depots/2);

    /* for each complete Condition Job, setup a Store Job */
    dll_traverse(dl_node,jp_condition->job_list)
    {
        for (i=0; i < set->copies; i++ )
        {
            job = (__LorsJob *)dl_node->val.v;
            assert(job->status == JOB_COMMIT ); 

            lm = _lorsAllocMapping();
            if (lm == NULL){
                 lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetStore : Out of memroy!\n");
                 ret_val = LORS_NO_MEMORY;
                 goto bail;
            };
#ifdef _MINGW
            lorsDebugPrint(D_LORS_VERBOSE, "\t lm->exnode_offset = %I64d\n", 
                                        job->src_offset+
                                        offset+
                                        set->exnode_offset);
#else
            lorsDebugPrint(D_LORS_VERBOSE, "\t lm->exnode_offset = %lld\n", 
                                        job->src_offset+
                                        offset+
                                        set->exnode_offset);
#endif
            lm->begin_time = tbegin;
            lm->exnode_offset = job->src_offset+offset+set->exnode_offset;
            lm->logical_length = (ulong_t)job->src_length;

            lm->alloc_length =  (ulong_t)job->dst_length;
            lm->alloc_offset = 0;
            lm->dp = dp;
            lm->timeout = timeout;
            lm->opts = opts;
            /* copy function data from the Condition routines */
            lm->function = (ExnodeFunction *)job->extra_data;

            /* TODO: this is not really where e2e blocksize should be set for
             * a mapping.  it should all be integrated into an e2e encoding
             * function. the lc field 'blocksize' should be removed. it is an
             * "external" quantity, and largely meaningless. */
            /* if lc has a variable length condition function, then the
             * blocksize becomes the logical_length of the mapping */
            lm->e2e_bs = 0;
            if ( lc != NULL )
            {
                /* this is awkward to check */
                int q;
                for (q=0; lc[q].function_name != NULL; q++)
                {
                    if ( lc[q].type == LORS_E2E_VARIABLE )
                    {
                        lm->e2e_bs = lm->logical_length;
                    }
                }
                /* WARNING: this may break somehow */
                if ( lm->e2e_bs == 0 )
                {
                    lm->e2e_bs = lc[0].blocksize;
                }
            } 

            /* create a new job and put into job pool */
            job_store = (__LorsJob *)calloc(1,sizeof(__LorsJob));
            if ( job_store == NULL ) {
                ret_val = LORS_NO_MEMORY;
                lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetStore :Out of memory!\n");
                goto bail;
            };
            job_store->type = J_CREATE_STORE;
            job_store->status = JOB_INIT;
            job_store->src = job->dst;
            job_store->src_length = job->dst_length; 
            job_store->src_offset = 0;
			job_store->handle = handle;
            job_store->dst = lm;

            /* add to job pool */
            _lorsAddJob(jp_store,job_store);
        }
    }
    _lorsFreeThreadPool(tp);

    /*
     * Create working threads
     */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : Creating working threads for Store.\n");
    ret_val = _lorsCreateThreadPool(jp_store,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
    {
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : _lorsCreateThreadPool failed..\n");
        goto bail;
    }

    /* wait for all active threads, join, and free resources */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : Waitting worker thread return.\n");
    ret_val = _lorsJoinThread(tp);

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : All working threads have returned.\n");


    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStore : adding mappings to set\n");
    dll_traverse(dl_node,jp_store->job_list)
    {
        job = (__LorsJob *)dl_node->val.v;

        lm = (LorsMapping*)job->dst;
        if ( job->status == JOB_COMMIT )
        {
            if ( _lorsHasE2E(lm) )
            {
                /* this is awkward to check */
                /*int q, f = 0;*/
                /*for (q=0; lc[q].function_name != NULL; q++) { }*/
                /*if ( lc[q-1].allocates == 1 )*/
                {
                    free(job->src); /* release resources from conditioned data */
                }
            } 
            lorsSetAddMapping(set, lm);
            success_cnt ++;
        } else 
        {
            fprintf(stderr, "failed commit on mapping: 0x%x: %d\n", lm, job->status);
            /* TODO: try it again */
        }
    }
    if ( ret_val != LORS_SUCCESS && success_cnt > 0)
    {
        ret_val = LORS_PARTIAL;
    }

#ifdef _MINGW
    lorsDebugPrint(D_LORS_VERBOSE, "lorsSetStore : logical_length %I64d\n", 
                                    set->max_length);
#else
    lorsDebugPrint(D_LORS_VERBOSE, "lorsSetStore : logical_length %lld\n", 
                                    set->max_length);
#endif   
    _lorsFreeJobPool(jp_condition);
    _lorsFreeJobPool(jp_store);
    _lorsFreeThreadPool(tp);
    free(list);
 
    return ret_val;

bail:
    lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetStore : Bailing!!!!\n");

    /* TODO: determine which JobPool to free _lorsFreeJobPool(jp);*/
    _lorsFreeThreadPool(tp);
    if ( list != NULL ) free(list);
    return (ret_val);  
};

int lorsSetUpdate(LorsSet       *set, 
                  LorsDepotPool *dp, 
                  char *buffer, 
                  longlong offset, 
                  longlong length, 
                  int       nthreads,
                  int       timeout,
                  int       opts)
{
    LorsSet         *update;
    int              lret;
    lorsSetInit(&update, set->data_blocksize, 1, 0);

    lret = lorsSetStore(update, dp, buffer, offset, length, NULL, nthreads, timeout, NULL, opts);
    if ( lret != LORS_SUCCESS ) return lret;

    lret = lorsSetTrim(set, offset, length, nthreads, timeout, opts|LORS_TRIM_ALL);
    if ( lret != LORS_SUCCESS ) return lret;

    lret = lorsSetMerge(update, set);
    if ( lret != LORS_SUCCESS ) return lret;

    return LORS_SUCCESS;
}

/* 
 *  JOB-> Source is the user data data
 *  JOB-> Destination is the LorsMapping
 */

int _lorsDoCreateStore(__LorsJob *job, __LorsThreadInfo *tinfo)
{
    LorsMapping               *lm = NULL;
    struct ibp_timer          timer;
    struct ibp_attributes     *ibpattr = NULL;
    IBP_set_of_caps           socaps = NULL;
    int                       ret = 0,tmp;
    double                    difftime = 0;
    int                       opts = 0;
    ulong_t                  ibp_ret = 0;
    ulong_t                  written = 0;
    LorsDepot                *ldepot = NULL;
    time_t                   begin_time,left_t;
    int                      i;
    char *                    buf;
    int                       tried = 0;
    double                   t1, t2;
    ulong_t                   demo_len;
    /* CREATE */
    assert ( (job != NULL) && (tinfo != NULL) );
    lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: Doing job set creation\n",
              __FUNCTION__, pthread_self());
    /*
     * set up clean up routine
     */
    pthread_cleanup_push(_lorsDoCleanup,(void *)job); 
    begin_time = time(0);
    lm = (LorsMapping *)(job->dst);
    opts = tinfo->opts;

    ibpattr = new_ibp_attributes(lm->dp->duration, lm->dp->type, IBP_BYTEARRAY);

next_depot:

    if ( g_lors_demo )
    {
        fprintf(stderr,"MESSAGE 2 Allocating...\n");
        fflush(stderr);
    }

    do 
    {
        /* TODO: get depot from depotpool */
        /* ret = lorsGetDepot(lm->dp, &ldepot);*/
        if ( (left_t = _lorsRemainTime(begin_time,job->timeout)) < 0 ) {
            fprintf(stderr, "begin: %d timeout: %d left: %d\n",
                    begin_time, job->timeout, left_t);
            ret = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
        if ( job->timeout > 0  ){
            /* timer.ServerSync = left_t + 1; */
            timer.ServerSync = 0;
            timer.ClientTimeout = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT : left_t - 1);
        } else {
            timer.ServerSync = 0;
            timer.ClientTimeout = 0;
        };
        lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: Checking out a depot\n",
                __FUNCTION__, pthread_self());

        /*fprintf(stderr, "Checkign out a depot\n");*/
        if ( (ret = lorsGetNextDepot(lm->dp->dl,&ldepot)) != LORS_SUCCESS ){
            /*fprintf(stderr, "GetNextDepot Failed...\n");*/
            /* TODO: time to request more depots */
              goto bail;
        }; 

        lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: Allocating a cap %ld\n",
                __FUNCTION__, pthread_self(), lm->alloc_length);

        lorsDebugPrint(D_LORS_VERBOSE, 
                    "ALLOCATING ON %s:%d %ld : %d %d dur:%d type:%d rely:%d\n", 
                            ldepot->depot->host, ldepot->depot->port, 
                            lm->alloc_length, timer.ServerSync, 
                            timer.ClientTimeout, 
                            ibpattr->duration,
                            ibpattr->type,
                            ibpattr->reliability);

#ifdef _MINGW
        lorsDebugPrint(1, "Capability  %24s:%4d %8I64d %8ld\n", 
                            ldepot->depot->host, ldepot->depot->port, 
                            lm->exnode_offset,
                            lm->alloc_length);
#else
        lorsDebugPrint(1, "Capability  %24s:%4d %8lld %8ld\n", 
                            ldepot->depot->host, ldepot->depot->port, 
                            lm->exnode_offset,
                            lm->alloc_length);
#endif
        socaps = IBP_allocate(ldepot->depot, &timer, lm->alloc_length, ibpattr);
        left_t = _lorsRemainTime(begin_time,job->timeout);

        lorsDebugPrint(D_LORS_VERBOSE, "IBP_allocate RETURNED\n");
        if ( socaps == NULL )   /* allocate failed */
        {
            ret = LORS_IBP_FAILED + IBP_errno;
            fprintf(stderr, "Allocate failed on %s: %d\n", ldepot->depot->host, ret);
            /* TODO: replace depot */
            /*lorsReleaseDepot(ldepot,0,2);*/
            if ( HAS_OPTION(opts, LORS_RETRY_UNTIL_TIMEOUT))
            {
                int  count;
                count = lm->dp->dl->count;
                fprintf(stderr, "Removing Bad depot: %s\n", ldepot->depot->host);
                lorsRemoveDepot(lm->dp->dl, ldepot->depot);
                lorsRefillListFromPool(lm->dp, lm->dp->dl, count);

                if ( ( left_t > 0 ) && ( tried < 4 ) ) 
                { 
                    lorsDebugPrint(1,"%s[%d]: One depot"
                                   "[%s:%d] failed IBP_error=%d, try another %d ...\n",
                                   __FUNCTION__, pthread_self(),ldepot->depot->host,
                                   ldepot->depot->port,ret, tried);
                    ret = LORS_SUCCESS ;
                    tried ++; 
                    continue ; 
                } else {
                    lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: timeout expired\n",
                                   __FUNCTION__, pthread_self());
                    goto bail;
                };  
            } else { /* kill all worker thread immediately */
                lorsDebugPrint(1,"%s[%d]: Killing "
                        "all working threads: ret:%d\n", 
                        __FUNCTION__, pthread_self(), ret); 
                lorsDebugPrint(1, 
                        "BAD ALLOCATION %s:%d %ld : %d %d dur:%d type:%d rely:%d\n", 
                        ldepot->depot->host, ldepot->depot->port, 
                        lm->alloc_length, timer.ServerSync, 
                        timer.ClientTimeout, 
                        ibpattr->duration,
                        ibpattr->type,
                        ibpattr->reliability);
                _lorsKillWorkerThreads(tinfo);
                goto bail;
                
            }; 
        }  else 
        {
            /*lorsReleaseDepot(ldepot,0,0);*/
            lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: ASSIGNING NEW CAP\n",
                    __FUNCTION__, pthread_self());
            lm->capset = *socaps;
            lm->capstat.attrib.duration = ibpattr->duration;
            _lorsGetDepotInfo(&(lm->depot),lm->capset.readCap);
            if ( g_lors_demo )
            {
                lm->id = _lorsIDFromCap(lm->capset.readCap);
#ifdef _MINGW
                fprintf(stderr,"DRAW MappingBegin local %ld %I64d %ld %s:%4d\n",
                            lm->id,lm->exnode_offset,lm->logical_length,
                            ldepot->depot->host, ldepot->depot->port);
#else
                fprintf(stderr,"DRAW MappingBegin local %ld %lld %ld %s:%4d\n",
                            lm->id,lm->exnode_offset,lm->logical_length,
                            ldepot->depot->host, ldepot->depot->port);
#endif
                fflush(stderr);
            }
        }
    } while ( socaps == NULL );


    lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: new cap is create: %s\n",
                    __FUNCTION__, pthread_self(), socaps->readCap);

/* STORE */
#ifdef _MINGW
    lorsDebugPrint(D_LORS_VERBOSE, "%s[%d]: Begining Store loop %ld %I64d\n",
            __FUNCTION__, pthread_self(), written, job->dst_length);
#else
    lorsDebugPrint(D_LORS_VERBOSE, "%s[%d]: Begining Store loop %ld %lld\n",
            __FUNCTION__, pthread_self(), written, job->dst_length);
#endif
    if ( g_lors_demo )
    {
        fprintf(stderr,"MESSAGE 2 Storing...\n");
#ifdef _MINGW
         fprintf(stderr,"DRAW Arrow1 fro %d %I64d %ld %s:%4d\n",
            lm->id,lm->exnode_offset,lm->logical_length,
            ldepot->depot->host,ldepot->depot->port);
#else
        fprintf(stderr,"DRAW Arrow1 fro %d %lld %ld %s:%4d\n",
            lm->id,lm->exnode_offset,lm->logical_length,
            ldepot->depot->host,ldepot->depot->port);
#endif
        fflush(stderr);
    }

    while ( written != job->src_length)
    {
        if ( (left_t = _lorsRemainTime(begin_time,job->timeout)) < 0 ) 
        {
            fprintf(stderr, "2begin: %d timeout: %d left: %d\n",
                    begin_time, job->timeout, left_t);
            ret = LORS_TIMEOUT_EXPIRED;
            goto bail;
        }
        if ( job->timeout > 0 ){
            timer.ServerSync = left_t + 1;
            timer.ClientTimeout = left_t - 1;
        } else {
            timer.ServerSync = 30;
            timer.ClientTimeout = 0;
        };
#ifdef _MINGW
        lorsDebugPrint(D_LORS_VERBOSE,
               "%s[%d]: SRC_LENGTH %I64d WRITTEN %d\n",
                __FUNCTION__, pthread_self(),
                 job->src_length,
                 written);
        lorsDebugPrint(D_LORS_VERBOSE,
                "%s[%d]: STORING to cap %I64d - %I64d\n",
                __FUNCTION__,
                pthread_self(),
                job->src_offset+written,
                job->src_length-written
                );
        lorsDebugPrint(1, "Store       %24s:%4d %8I64d %8ld\n", 
                            ldepot->depot->host, ldepot->depot->port, 
                            lm->exnode_offset+written,
                            (long)(job->src_length-written));
#else
        lorsDebugPrint(D_LORS_VERBOSE,
               "%s[%d]: SRC_LENGTH %lld WRITTEN %d\n",
                __FUNCTION__, pthread_self(),
                 job->src_length,
                 written);
        lorsDebugPrint(D_LORS_VERBOSE,
                "%s[%d]: STORING to cap %lld - %lld\n",
                __FUNCTION__,
                pthread_self(),
                job->src_offset+written,
                job->src_length-written
                );
        lorsDebugPrint(1, "Store       %24s:%4d %8lld %8ld\n", 
                            ldepot->depot->host, ldepot->depot->port, 
                            lm->exnode_offset+written,
                            (long)(job->src_length-written));
#endif

        if ( g_lors_demo )
        {
            /* begin time */
            t1 = getTime();
            pthread_mutex_lock(&store_thread_lock);
            g_store_thread_count++;
            pthread_mutex_unlock(&store_thread_lock);
        }
        ibp_ret = IBP_store(lm->capset.writeCap, 
                  &timer, 
                  ((char*)job->src)+written, 
                  job->src_length-written);
        if ( g_lors_demo )
        {
            /* end time */
            t2 = diffTime(t1);
            if ( ibp_ret != 0 )
            {
                demo_len = job->src_length-written;
                pthread_mutex_lock(&store_thread_lock);
                lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", 
                    g_store_thread_count*(demo_len*8.0)/1024.0/1024.0/t2);
                pthread_mutex_unlock(&store_thread_lock);
            }
            fflush(stderr);
            pthread_mutex_lock(&store_thread_lock);
            g_store_thread_count--;
            pthread_mutex_unlock(&store_thread_lock);
        }

        left_t = _lorsRemainTime(begin_time,job->timeout);
        ret = LORS_IBP_FAILED + IBP_errno;

        lorsDebugPrint(D_LORS_VERBOSE, "IBP_store RETURNED\n");
        if ( ibp_ret == 0 )
        {
            if ( HAS_OPTION(opts, LORS_RETRY_UNTIL_TIMEOUT)) 
            {
                lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: One depot"
                               "[%s:%d] failed IBP_error=%d, try another ...\n",
                               __FUNCTION__, pthread_self(),ldepot->depot->host,
                               ldepot->depot->port,ret); 
                ret = LORS_SUCCESS ;
                goto next_depot ; 
            } else 
            {
                /* killall other threads */
                lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: One depot"
                               "[%s:%d] failed IBP_error=%d, giving up...\n",
                               __FUNCTION__, pthread_self(),ldepot->depot->host,
                               ldepot->depot->port,ret);
                if ( g_lors_demo )
                {
#ifdef _MINGW
                    fprintf(stderr,"DELETE Arrow1 fro %d %I64d %ld %s:%4d\n",
                        lm->id,lm->exnode_offset,lm->logical_length,
                        ldepot->depot->host,ldepot->depot->port);
#else
                    fprintf(stderr,"DELETE Arrow1 fro %d %lld %ld %s:%4d\n",
                        lm->id,lm->exnode_offset,lm->logical_length,
                        ldepot->depot->host,ldepot->depot->port);
#endif
                    fflush(stderr);
                }
                _lorsKillWorkerThreads(tinfo); 
                goto bail;
            }
        } else { 

			// report status to report host 
			if(job->handle->status == CONN_CONNECTED){
				socket_io_send_push(job->handle, ldepot->depot->host, lm->exnode_offset+written, (job->src_length-written));
			}

            written += ibp_ret;
            lm->seek_offset += ibp_ret;
			
            lorsDebugPrint(D_LORS_VERBOSE, "lm->seek_offset %d, ibp_ret. %d\n", 
                    lm->seek_offset, ibp_ret);
            ret = LORS_SUCCESS ;
            /* TODO: print speed */
            /* print speed */
        }
    }

    if ( g_lors_demo )
    {
#ifdef _MINGW
        fprintf(stderr,"DELETE Arrow1 fro %d %I64d %ld %s:%4d\n",
                    lm->id,lm->exnode_offset,lm->logical_length,
                    ldepot->depot->host,ldepot->depot->port);
        fprintf(stderr,"DRAW MappingEnd %ld %I64d %ld %s:%4d\n",
                    lm->id,lm->exnode_offset,lm->logical_length,
                    ldepot->depot->host,ldepot->depot->port);
#else
        fprintf(stderr,"DELETE Arrow1 fro %d %lld %ld %s:%4d\n",
                    lm->id,lm->exnode_offset,lm->logical_length,
                    ldepot->depot->host,ldepot->depot->port);
        fprintf(stderr,"DRAW MappingEnd %ld %lld %ld %s:%4d\n",
                    lm->id,lm->exnode_offset,lm->logical_length,
                    ldepot->depot->host,ldepot->depot->port);
        fflush(stderr);
#endif
    }

    _lorsCommitJob(job,JOB_COMMIT,LORS_SUCCESS);

bail:
    if ( ret != LORS_SUCCESS ){
         job->status = JOB_FAILED;
         job->err_code = ret;
    };
    if ( socaps != NULL ) free(socaps);
    if ( ibpattr != NULL ) free (ibpattr);
    pthread_cleanup_pop(0);
    return (ret);
};

int _lorsDoCondition( __LorsJob *job, __LorsThreadInfo *tinfo )
{
    LorsSet        *set = NULL;
    LorsConditionStruct *lc_array = NULL, lc;
    Dllist          dlnode;
    char           *src_data = NULL;
    ulong_t         src_length;
    ulong_t         src_blocksize;
    char           *dst_data = NULL ;
    ulong_t         dst_length;
    ulong_t         dst_blocksize;
    char           *v2 = NULL;
    int             i, ret;
    ExnodeFunction  *f = NULL, *fsub = NULL;
    ExnodeMetadata  *args;


    src_data = job->src;
    src_length = job->src_length;
    lc_array = job->extra_data;

    if ( lc_array != NULL )
    {
        /* TODO: change this to a blocksize smaller than length */ 
        /*src_blocksize = src_length;*/
        src_blocksize = lc_array[0].blocksize;
        for ( i=0; lc_array[i].function_name != NULL; i++)
        {
            lc = lc_array[i];
            exnodeCreateFunction(lc.function_name, &f);
            exnodeGetFunctionArguments(f, &lc.arguments);

            ret = lc.condition_data(src_data, 
                              src_length,
                              src_blocksize,
                              &dst_data, 
                              &dst_length, 
                              &dst_blocksize,
                              lc.arguments);
            /* TODO: check value of 'ret' */
            /* TODO: call free() on intermediate dst_data's: DONE */
            src_data = dst_data;
            src_length = dst_length;
            src_blocksize = dst_blocksize;
            if ( fsub == NULL )
            {
                fsub = f;
            } else 
            {
                exnodeAppendSubfunction(f, fsub);
                fsub = f;
            }
            if ( v2 == NULL )
            {
                v2 = dst_data;  /* initially */
            } else 
            {
                /* verify that the previous condition function allocated memory */
                /*if ( lc_array[i-1].allocates == 1){*/
                    free(v2);       /* after more than 1 pass, free the old memory */
                /*}*/
                v2 = dst_data;  /* reset for next pass */
            }
        }
    }
    job->dst = src_data;
    job->dst_length = src_length;

    job->extra_data = f;

    _lorsCommitJob(job,JOB_COMMIT,LORS_SUCCESS);
    return 0;
}
