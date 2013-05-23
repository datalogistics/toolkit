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

extern ulong_t  g_lm_id ;

int    lorsSetAllocate (LorsSet ** se,
                         LorsDepotPool * dp,
                         longlong offset,
                         longlong length,
                         int copies,
                         ulong_t blocksize,
                         int nthreads,
                         int timeout,
                         int opts)
{
    /*pthread_t       prog_tid;*/
    /*pthread_attr_t  prog_attr;*/
    /*LorsTimeout     *lt;*/
    LorsMapping    *lm = NULL,**lm_list;
    LorsSet        *set=NULL;
    LorsBoundary   *lb=NULL;
    double          tbegin, tdiff;
    longlong       *list=NULL;
    longlong        block_offset;
    int             i,j,k,tmp;
    int             blocks, list_len;
    Dllist          dl_unjoined, dl_node, dl_list;
    JRB             jrb_node;
    LorsDepot       *depot = NULL; 
    __LorsThreadPool   *tp = NULL;
    __LorsJobPool      *jp = NULL;
    __LorsJob          *job = NULL;
    int                ret_val;
    
    lorsDebugPrint(D_LORS_TERSE,"lorsSetAllocate: Begin to create new set..\n");
    if((*se == NULL) || (dp == NULL) || (offset < 0) ||
       (length <=0 ) || (copies <= 0) || (blocksize == 0)){
          return ( LORS_INVALID_PARAMETERS);
    };

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: creating job pool.\n");
    if ( (ret_val = _lorsCreateJobPool(&jp)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: can't create job pool!\n");
        goto bail; 
    }; 
    jp->type = J_CREATE;
    jp->src_offset = 0;
    jp->src_length = length;
    
    set = _lorsMallocSet();
    if ( set == NULL ){
        ret_val = LORS_NO_MEMORY;
        lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetAllocate:Out of memory!\n");
        goto bail;
    }; 
    set->exnode_offset = offset;
    set->data_blocksize = blocksize;
    set->copies = copies;

    list = _lorsNewBlockArray(length, blocksize, &list_len);
    if (list == NULL ) {
       lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetAllocate: Out of memroy!\n");
       ret_val = LORS_NO_MEMORY;
       goto bail;
    };

    lm_list = (LorsMapping **)calloc(list_len*copies,sizeof(LorsMapping *));
    if ( lm_list == NULL ){
       lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetAllocate: Out of memroy!\n");
       ret_val = LORS_NO_MEMORY;
       goto bail;
    };

    k = 0;
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: Creating all jobs!\n");
    for ( j=0; j < copies; j++)
    {
        block_offset = set->exnode_offset;
        for ( i=0; list[i] != 0 ; i++)
        {
            lm = _lorsAllocMapping(); 
            if (lm == NULL){
                 lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetAllocate: Out of memroy!\n");
                 ret_val = LORS_NO_MEMORY;
                 goto bail;
            };
            lm->begin_time = tbegin;
            lm->exnode_offset = block_offset;
            lm->logical_length = (ulong_t)list[i];
            lm->alloc_length = (ulong_t)list[i];
            lm->alloc_offset = 0;
            lm->dp = dp;
            lm->timeout = timeout;
            lm->opts = opts;
            block_offset += list[i];
            lm_list[k] = lm;
            k++;
            /*
             * create a new job and put into job pool
             */
            job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
            if ( job == NULL ) {
                ret_val = LORS_NO_MEMORY;
                lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetAllocate:Out of memory!\n");
                goto bail;
            };
            job->type = J_CREATE;
            job->status = JOB_INIT;
            job->src = lm;
              
            /* add to job pool */
            _lorsAddJob(jp,job);
       }; 
    }



    /*
     * Create working threads
     */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: Creating working threads.\n");
    ret_val = _lorsCreateThreadPool(jp,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
        goto bail;

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: %d working threads have been created successfully.\n",tp->nthread);

   
    /* wait for all active threads, join, and free resources */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: Waitting worker thread return.\n");
    ret_val = _lorsJoinThread(tp);
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: All working threads have returned.\n");

    if ( ret_val != LORS_SUCCESS )
        goto bail;

    tmp = pthread_mutex_lock(&(jp->lock));
    assert(tmp == 0); 
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetAllocate: adding mapping to set\n");
    dll_traverse(dl_node,jp->job_list){
         job = (__LorsJob *)dl_node->val.v;
         assert(job->status == JOB_COMMIT ); 
         lm = (LorsMapping*)job->src;
         lorsSetAddMapping(set,lm);
    }
    *se = set;
    set->seek_offset = offset;
   
    _lorsFreeJobPool(jp);
    _lorsFreeThreadPool(tp);
    free(list);
    free(lm_list);
 
    return LORS_SUCCESS;

bail:

    _lorsFreeJobPool(jp);
    _lorsFreeThreadPool(tp);
    if ( list != NULL ) free(list);
    if ( lm_list != NULL ) {
        for ( i = 0; i < list_len*copies ; i ++ )
            if ( lm_list[i] != NULL )    free(lm_list[i]);
        free(lm_list);
    };
    if ( set != NULL ) {
        if ( set->mapping_map != NULL ) jrb_free_tree(set->mapping_map);
        free(set);
    };
    return (ret_val);  
}
int _lorsDoCreate(__LorsJob *job, __LorsThreadInfo *tinfo)
{
    /* offset, length, depotpool, begin_time, timeout, */
    LorsMapping               *lm = NULL,*tmp_lm = NULL;
    struct ibp_timer          timer;
    struct ibp_attributes     *ibpattr = NULL;
    IBP_set_of_caps           socaps = NULL;
    int                       ret = 0,tmp = 0,i;
    double                    difftime = 0;
    __LorsDLNode              *dl_node;
    int                       opts = 0, found = 0;
    LorsDepot                *ldepot = NULL,*new_ldepot = NULL;
    time_t                   begin_time,left_t;
    LorsDepotPool            *tmpdpp; 
    int                      tried = 0, valid_count=0, loop=0;
    Dllist                    offset_node = NULL;

    assert ( (job != NULL) && (tinfo != NULL) );
    lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCreate[%d]: Doing job set creation\n",pthread_self());
    /*
     * set up clean up routine
     */
    pthread_cleanup_push(_lorsDoCleanup,(void *)job); 
    begin_time = time(0);
    lm = (LorsMapping *)(job->src);
    dl_node =  (__LorsDLNode *)(job->dst);
    opts = tinfo->opts;
    
    ibpattr = new_ibp_attributes(lm->dp->duration, lm->dp->type, IBP_BYTEARRAY);

    /*printDepotPool(lm->dp);*/
    pthread_mutex_lock(&dl_node->lock);

    do 
    {
        /* TODO: get depot from depotpool */
        /* ret = lorsGetDepot(lm->dp, &ldepot);*/
        if ( (left_t = _lorsRemainTime(begin_time,job->timeout)) < 0 ) {
            ret = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
        if ( job->timeout > 0){
            timer.ClientTimeout = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT : left_t );
            timer.ServerSync = 0; 
        }else {
            timer.ServerSync = 0;
            timer.ClientTimeout = 0;
        };
        lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCreate[%d]: Checking out a depot\n",pthread_self());
        
        if ( tmp = lorsGetNextDepot(lm->dp->dl, &ldepot) != LORS_SUCCESS )
        {
           goto bail;
        }
#if 0
        if ( (ret = lorsGetDepot(lm->dp,&ldepot)) != LORS_SUCCESS ){
           goto bail;
        };
#endif

        valid_count = 0;
        dll_traverse(offset_node, dl_node->mappings)
        {
            valid_count++;
        }

        /*fprintf(stderr, "valid_count: %d\n", valid_count);*/
        loop = 0;
        /* this loop tries to guarantee strict striping over the copies in an
         * exnode.  this may have the effect of limiting the striping over the
         * extent. */
        found = 0;

        /*fprintf(stderr, "Beginning check:\n");*/
        while(found == 0)
        {
tryagain:
            /*fprintf(stderr, "checking depots\n");*/
            dll_traverse(offset_node, dl_node->mappings)
            {
                tmp_lm = (LorsMapping *)jval_v(offset_node->val);
                /* we don't want to bother with different ports either */

                /*fprintf(stderr, "comparing: %s and %s\n", 
                        tmp_lm->depot.host,ldepot->depot->host);*/
                if ( strcasecmp(tmp_lm->depot.host,ldepot->depot->host) == 0 )
                {
                    /*fprintf(stderr, "EQUAL\n");*/
                    if ( (tmp = lorsGetNextDepot(lm->dp->dl,&new_ldepot)) != LORS_SUCCESS )
                    {
                        /*lorsReleaseDepot(new_ldepot,0,0);*/
                        fprintf(stderr, "GetNextDepot != LORS_SUCCESS\n");
                        goto bail;
                    };
                    loop++;     /* count the number of times we try */
                    if ( loop > valid_count*3 ) {
                        /* we can just go with this one. */

                        /*fprintf(stderr, "Giving up looking\n");*/
                        break;
                    }
                    /*lorsReleaseDepot(ldepot,0,0);*/
                    ldepot=new_ldepot;
                    goto tryagain;
                } 
            }
            found++;
        }
        /*
            dll_traverse(offset_node, dl_node->mappings)
            {
                tmp_lm = (LorsMapping *)jval_v(offset_node->val);
                if ( strcasecmp(tmp_lm->depot.host,ldepot->depot->host) == 0 )
                {
                    fprintf(stderr, "EQUAL\n");
                    fprintf(stderr, "comparing: %s and %s\n", 
                        tmp_lm->depot.host,ldepot->depot->host);
                    exit(1);
                }
            }
            */

        lorsDebugPrint(D_LORS_VERBOSE,"_losrDoCreate[%d]: Allocating a cap\n",pthread_self());
        lorsDebugPrint(D_LORS_VERBOSE,"Allocating a cap  len: %d\n", lm->alloc_length);
        
        lorsDebugPrint(D_LORS_VERBOSE,"ALLOCATING ON %s:%d %ld : %d %d dur:%d type:%d rely:%d \n",
					ldepot->depot->host,ldepot->depot->port,
					lm->alloc_length,timer.ServerSync,
					timer.ClientTimeout,
					ibpattr->duration,
					ibpattr->type,
					ibpattr->reliability);
	lorsDebugPrint(1,"Capability %24s:%4d %8lld %8ld\n",
				ldepot->depot->host,ldepot->depot->port,
				lm->exnode_offset,
				lm->alloc_length);
        socaps = IBP_allocate(ldepot->depot, &timer, lm->alloc_length, ibpattr);
        left_t = _lorsRemainTime(begin_time,job->timeout);
        if ( socaps == NULL )   /* allocate failed */
        {
            ret = LORS_IBP_FAILED + IBP_errno;
            fprintf(stderr, "Allocate failed on %s: %d\n", ldepot->depot->host, ret);
            /*lorsReleaseDepot(ldepot,0,1);*/

            if ( HAS_OPTION(opts, LORS_RETRY_UNTIL_TIMEOUT)){ 
                int  count;
                count = lm->dp->dl->count;
                /*fprintf(stderr, "Removing Bad depot: %s\n", ldepot->depot->host);*/
                lorsRemoveDepot(lm->dp->dl, ldepot->depot);
                lorsRefillListFromPool(lm->dp, lm->dp->dl, lm->dp->min_unique_depots/2);

                if ( ( left_t > 0 ) && ( tried < 4 )) { 
                    lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCreate[%d]: One depot[%s:%d]"
                            " failed IBP_error=%d, try another ...\n",
                            pthread_self(),ldepot->depot->host,
                            ldepot->depot->port,IBP_errno); 
                    tried++;
                    continue ; 
                } else {
                    goto bail;
                };  
            } else { /* kill all worker thread immediately */
                lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCreate[%d]: Killing all working threads\n",pthread_self()); 
                _lorsKillWorkerThreads(tinfo);
                goto bail;
            }; 
        }  else 
        {
            /*lorsReleaseDepot(ldepot,0,0);*/
            lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]:ASSIGNING NEW CAP \n",
                                        __FUNCTION__,pthread_self());
            lm->capset = *socaps;
            ret = LORS_SUCCESS;
            memcpy(&(lm->depot),ldepot->depot,sizeof(struct ibp_depot));
            if ( g_lors_demo )
            {
                lm->id = _lorsIDFromCap(lm->capset.readCap);
                /*fprintf(stderr,"DRAW MappingBegin local %ld %lld %ld %s:%4d \n",
                        lm->id,lm->exnode_offset,lm->logical_length,ldepot->depot->host,
                        ldepot->depot->port);*/
            }
        }
    } while ( socaps == NULL );


    lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCreate[%d]: new cap is create: %s\n",pthread_self(),socaps->readCap);

bail:

    lm->notvalid = 1;   /* set to be unused by future copies */
    dll_append(dl_node->mappings,new_jval_v(lm));

    pthread_mutex_unlock(&dl_node->lock);
    if ( ret != LORS_SUCCESS ){
         job->status = JOB_FAILED;
         job->err_code = ret;
    };
    _lorsCommitJob(job,JOB_COMMIT,ret);

    if ( socaps != NULL ) free(socaps);
    if ( ibpattr != NULL ) free (ibpattr);
    pthread_cleanup_pop(0);
    return (ret);

}

