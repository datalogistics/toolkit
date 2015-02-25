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
#define lorsDemoPrint       fprintf

int    lorsSetTrim (LorsSet * set,
                    longlong offset,
                    longlong length,
                    int nthreads,
                    int timeout,
                    int opts)
{
    JRB     node, node_tmp;
    LorsMapping *lm, **lma;
    __LorsThreadPool   *tp;
    __LorsJobPool      *jp_trim;
    __LorsJob          *job;
    int                 i, cnt,ret_val, ret;
    time_t              begin_time;
    Dllist              dlnode;

    LorsEnum            l_enum = NULL, l_iter = NULL;

    
    if ( set == NULL ) 
    {
        return LORS_INVALID_PARAMETERS;
    }
    /*
    there are 4 conditions a mapping may fall under when being trimmed.
    1. complete cover.  the offset/length boundary entirely encompasses the mapping.
    2. partial cover bottom.  only the bottom portion is lost.
    3. partial cover top.  only the top portion is lost.
    4. partial cover internal. the top and bottom remain, and the center is removed.

    one job is assigned to each affected mapping.  
    */

    if ( (ret_val = _lorsCreateJobPool(&jp_trim)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetTrim: can't create job pool!\n");
        goto bail; 
    }; 
    jp_trim->type= J_CREATE;
    jp_trim->src_offset = offset;
    jp_trim->src_length = length;

    cnt = 0;
    begin_time= time(0);

    lorsSetEnum(set, &l_enum);
#if 1
    while ( lorsEnumNext(l_enum, &l_iter, &lm) == LORS_SUCCESS )
    {
#else 
    /*jrb_rtraverse( node, set->mapping_map)*/
    /*{*/
        /*lm = (LorsMapping *)jval_v(node->val);*/
#endif
        if ( length > 0){
            if ( (length + offset) <= lm->exnode_offset)
               continue;
        };
        if ((lm->exnode_offset + lm->logical_length) <= offset )
            continue;
        {
            /* create a new job and put into job pool */
            job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
            if ( job == NULL ) {
                ret_val = LORS_NO_MEMORY;
                lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetTrim: Out of memory!\n");
                goto bail;
            };

            job->type   = J_TRIM;
            job->status = JOB_INIT;
            lm->begin_time = begin_time;
            job->src    = lm;
            job->src_offset = offset;
            job->src_length = length;
            job->timeout = timeout;

            cnt++;
            _lorsAddJob(jp_trim, job);
            /* remove node from the mapping_map; 
             * they will be added back at the end of the job. */
#if 0
            node_tmp = jrb_next(node);
            fprintf(stderr, "Removing mapping: 0x%x at offset: %lld length: %d\n",  lm, lm->exnode_offset, lm->logical_length);
            jrb_delete_node(node);
            node = node_tmp;
#endif
            fprintf(stderr, "Removing mapping: 0x%x at offset: %lld length: %d\n",  lm, lm->exnode_offset, lm->logical_length);
            lorsSetRemoveMapping(set, lm);
        }
    } 
    lorsEnumFree(l_enum);

#if 1 /* DEBUG */
    jrb_rtraverse( node, set->mapping_map)
    {
        lm = (LorsMapping *)jval_v(node->val);
        fprintf(stderr, "0x%x mapping still in mapping_map.\n", lm);
    }
#endif /* end DEBUG */
    lorsDebugPrint(D_LORS_VERBOSE, "lorsSetTrim: Creating working threads "
                                   "for IBP_manage.\n");
    ret_val = _lorsCreateThreadPool(jp_trim,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
    {
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetTrim: _lorsCreateThreadPool failed..\n");
        goto bail;
    }

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStat: Waiting Condition threads return.\n");
#if 0
    for (i=0; i < tp->nthread; i++ )
    {
        ret = _lorsJoinThread(tp, i);
        if ( ret != LORS_SUCCESS )
        {
            /* TODO: perhaps keep a 'failed_list' to do something with */
            ret_val = ret;
        }
    };
#endif
    ret_val = _lorsJoinThread(tp);

    /* add the trimmed mappings back to the Set */
    dll_traverse(dlnode,jp_trim->job_list)
    {
        job = (__LorsJob *)dlnode->val.v;
        assert(job->status == JOB_COMMIT ); 
        lma = job->dst;
        if ( lma != NULL )
        {
            for (i=0; i < 2; i++ )
            {
                lm = lma[i];
                if (lm != NULL ) 
                {

                    fprintf(stderr, "Returning mapping: 0x%x at offset: %lld length: %d\n",  lm, lm->exnode_offset, lm->logical_length);
                    lorsSetAddMapping(set, lm);
                /*jrb_insert_llong(set->mapping_map, lm->exnode_offset, new_jval_v(lm));*/
                }
            }
            /*fprintf(stderr, "calling free on lma 0x%x\n", lma);*/
            free(lma);
        }
    }

    if ( ret_val != LORS_SUCCESS )
        goto bail;

    _lorsFreeJobPool(jp_trim);
    _lorsFreeThreadPool(tp);
    return LORS_SUCCESS;
bail:
    /* TODO: free memory */
    if ( jp_trim != NULL ) _lorsFreeJobPool(jp_trim);
    if ( tp != NULL ) _lorsFreeThreadPool(tp);

    return ret_val;

}

int _lorsDoTrim( __LorsJob *job, __LorsThreadInfo *tinfo)
{
    LorsMapping     *lm, *lm2;
    LorsMapping    **lma;
    LorsSet         *set;

    int              opts, ret_val, 
                     opt_mask = LORS_TRIM_ALL |LORS_TRIM_DEAD|LORS_TRIM_DECR|
                                LORS_TRIM_KILL|LORS_TRIM_NOKILL;

    lma = (LorsMapping **)malloc(sizeof(LorsMapping *)*2);
    assert (lma != NULL );

    opts = tinfo->opts & opt_mask;
    lm = (LorsMapping *)job->src;

    set = job->extra_data;

    /* there are 4 conditions a mapping may fall under when being trimmed.  */
    if ( job->src_offset <= lm->exnode_offset &&
         job->src_offset + job->src_length >= lm->exnode_offset+lm->logical_length )
    {

        lorsDebugPrint(D_LORS_VERBOSE, "Trim Whole\n");
        /* 1. complete cover. the offset/length boundary 
         *    entirely encompasses the mapping.*/
        if ( (opts & LORS_TRIM_DEAD && lm->capstat.readRefCount == -1) )
        {
            /* already dead.  no need to do anything */
            /*fprintf(stderr, "\ttrim dead\n");*/
            job->dst = NULL;
            free(lma);
            if ( g_lors_demo )
            {
#ifdef _MINGW
                lorsDemoPrint(stderr, "DELETE Mapping %d %I64d %ld %s:%d\n",
                    lm->id, lm->exnode_offset, lm->logical_length, 
                    lm->depot.host, lm->depot.port);
#else
                lorsDemoPrint(stderr, "DELETE Mapping %d %lld %ld %s:%d\n",
                    lm->id, lm->exnode_offset, lm->logical_length, 
                    lm->depot.host, lm->depot.port);
#endif
                fflush(stderr);
            }
            _lorsFreeMapping(lm);
        }  else if  (opts & LORS_TRIM_ALL )
        {
#ifdef _MINGW
            lorsDebugPrint(1, "Trim %s:%d %I64d %d\n",
                            lm->depot.host, lm->depot.port, 
                            lm->exnode_offset, lm->logical_length);
#else
            lorsDebugPrint(1, "Trim %s:%d %lld %d\n",
                            lm->depot.host, lm->depot.port, 
                            lm->exnode_offset, lm->logical_length);
#endif
            if ( g_lors_demo )
            {
#ifdef _MINGW
                lorsDemoPrint(stderr, "DELETE Mapping %d %I64d %ld %s:%d\n",
                    lm->id, lm->exnode_offset, lm->logical_length, 
                    lm->depot.host, lm->depot.port);
#else
                lorsDemoPrint(stderr, "DELETE Mapping %d %lld %ld %s:%d\n",
                    lm->id, lm->exnode_offset, lm->logical_length, 
                    lm->depot.host, lm->depot.port);
#endif
                fflush(stderr);
            }
            if ( opts & LORS_TRIM_DECR )
            {
                /*fprintf(stderr, "\ttrim decr\n");*/
                /*fprintf(stderr, "0x%x %d\n", lm, job->timeout);*/
                ret_val = lorsMappingDecr(lm, job->timeout);
                /*fprintf(stderr, "%d\n", ret_val);*/
                /* may or may not succeed. best effor decrement */
                ret_val = LORS_SUCCESS;
                free(lma);
                job->dst = NULL;
                _lorsFreeMapping(lm);
            } else if ( opts & LORS_TRIM_KILL )
            {
                /*fprintf(stderr, "\ttrim kill\n");*/
                do 
                {
                    ret_val = lorsMappingDecr(lm, job->timeout);
                } while(ret_val == LORS_SUCCESS);
                free(lma);
                job->dst = NULL;
                _lorsFreeMapping(lm);
            } else 
            {
                free(lma);
                job->dst = NULL;
                _lorsFreeMapping(lm);
                /* ??? remove from the set? */
            }
        } else 
        {
            /* otherwise just copy the mapping. */
            /*fprintf(stderr, "\ttodo\n");*/
            job->dst = lma;
            lma[0] = lm;
            lma[1] = NULL;
        }

    } 
#if 1
    else if ( lm->exnode_offset < job->src_offset &&
                job->src_offset+job->src_length >= lm->exnode_offset +lm->logical_length)
    {
        fprintf(stderr, "Trim Bottom\n");
        /* 2. partial cover - bottom.  only the bottom portion is lost.*/
        if ( (opts & LORS_TRIM_DEAD && lm->capstat.readRefCount == -1) )
        {
            /* already dead.  no need to do anything */
            job->dst = NULL;
            /* regardless of position in the mapping, 
             * if its dead, it's dead. */
        }  else if  (opts & LORS_TRIM_ALL )
        {
            /* how to handle decr? */
            /* simply ignore decr and kill; it cannot be applied to the
             * entire mapping. rather just limit the 'length' of the mapping
             */
            /* TODO: length is assigned only in trim and store i.e. stat
             * should not change the logical length. */
            lm->logical_length = job->src_offset - lm->exnode_offset;
            assert(lm->logical_length != 0);
            job->dst = lma;
            lma[0] = lm;
            lma[1] = NULL;

            fprintf(stderr, "trim bottom\n");
            fprintf(stderr, "lm: 0x%x off: %lld len: %d\n",
                     lm, lm->exnode_offset, lm->logical_length);
            fprintf(stderr, "\trwm: %d %d %d\n", 
                    (lm->capset.readCap != NULL), 
                    (lm->capset.writeCap != NULL), 
                    (lm->capset.manageCap != NULL));
        } else 
        {
            job->dst = lma;
            lma[0] = lm;
            lma[1] = NULL;
        }


    } else if ( job->src_offset <= lm->exnode_offset &&
                job->src_offset+job->src_length < lm->exnode_offset +lm->logical_length)
    {
        /* 3. partial cover - top.  only the top portion is lost.*/
        fprintf(stderr, "Trim Top\n");
        if ( (opts & LORS_TRIM_DEAD && lm->capstat.readRefCount == -1) )
        {
            /* already dead.  no need to do anything */
            job->dst = NULL;
        }  else if  (opts & LORS_TRIM_ALL )
        {
            /* logical_length and alloc_offset are assigned only in trim and
             * store.  stat should not change these */

            lm->logical_length = (lm->exnode_offset + lm->logical_length) -
                                 (job->src_offset + job->src_length);

            lm->alloc_offset  =  (job->src_offset + job->src_length) - lm->exnode_offset;
            lm->exnode_offset =  (job->src_offset + job->src_length);

            fprintf(stderr, "logical_len: %d\n", lm->logical_length);
            job->dst = lma;
            lma[0] = lm;
            lma[1] = NULL;
            fprintf(stderr, "trim top\n");
            fprintf(stderr, "lm: 0x%x off: %lld len: %d\n",
                     lm, lm->exnode_offset, lm->logical_length);
            fprintf(stderr, "\trwm: %d %d %d\n", 
                    (lm->capset.readCap != NULL), 
                    (lm->capset.writeCap != NULL), 
                    (lm->capset.manageCap != NULL));
        } else 
        {
            job->dst = lma;
            lma[0] = lm;
            lma[1] = NULL;
        }

    } else if ( lm->exnode_offset < job->src_offset  &&
                lm->exnode_offset+lm->logical_length > job->src_offset+job->src_length)
    {
        /* 4. partial cover - internal. the top and bottom 
         *    remain, and the center is removed.*/
        fprintf(stderr, "Trim Center\n");
        if ( (opts & LORS_TRIM_DEAD && lm->capstat.readRefCount == -1) )
        {
            job->dst = NULL;
        }  else if  (opts & LORS_TRIM_ALL )
        {
            /* TODO: This needs much more work */
            lm2 = (LorsMapping *)calloc(1, sizeof(LorsMapping));
            /* duplicate metadata and functions */
            assert ( lm2 != NULL );

            /* lm2 is the bottom */
            lm2->capstat = lm->capstat;
            lm2->capset.readCap = strdup(lm->capset.readCap);
            lm2->capset.writeCap = strdup(lm->capset.writeCap);
            lm2->capset.manageCap = strdup(lm->capset.manageCap);

            lm2->logical_length = (lm->exnode_offset+lm->logical_length) - 
                                  (job->src_offset+job->src_length);
            lm2->alloc_offset = (job->src_offset+job->src_length) - lm->exnode_offset;
            lm2->exnode_offset = job->src_offset+job->src_length;

            snprintf(lm2->depot.host, 255, "%s", lm->depot.host);
            lm2->depot.port = lm->depot.port;

            /* lm is the top part */
            lm->logical_length = job->src_offset-lm->exnode_offset;

            fprintf(stderr, "trim center\n");
            fprintf(stderr, "lm: 0x%x off: %lld len: %d\n",
                     lm, lm->exnode_offset, lm->logical_length);
            fprintf(stderr, "\trwm: %d %d %d\n", 
                    (lm->capset.readCap != NULL), 
                    (lm->capset.writeCap != NULL), 
                    (lm->capset.manageCap != NULL));

            fprintf(stderr, "lm2: 0x%x off: %lld len: %d\n",
                     lm2, lm2->exnode_offset, lm2->logical_length);
            fprintf(stderr, "\trwm: %d %d %d\n", 
                    (lm2->capset.readCap != NULL), 
                    (lm2->capset.writeCap != NULL), 
                    (lm2->capset.manageCap != NULL));

            job->dst = lma;
            lma[0] = lm;
            lma[1] = lm2;
        } else 
        {
            job->dst = lma;
            lma[0] = lm;
            lma[1] = NULL;
        }
    } 
#endif
    else 
    {
        fprintf(stderr, "Trim IMPOSSIBLE.\n");
#ifdef _MINGW
        fprintf(stderr, "exnode_offset: %I64d, logical_length %ld\n", 
                            lm->exnode_offset, lm->logical_length);
        fprintf(stderr, "src_offset: %I64d, src_length%I64d\n", 
                            job->src_offset, job->src_length);
#else
        fprintf(stderr, "exnode_offset: %lld, logical_length %ld\n", 
                            lm->exnode_offset, lm->logical_length);
        fprintf(stderr, "src_offset: %lld, src_length%lld\n", 
                            job->src_offset, job->src_length);
#endif
        _lorsCommitJob(job,JOB_COMMIT,LORS_FAILURE);
    }


    _lorsCommitJob(job,JOB_COMMIT,LORS_SUCCESS);
    return LORS_SUCCESS;
}

int   lorsMappingDecr( LorsMapping *lm, time_t timeout)
{
    struct ibp_timer        timer;
    int                     ibp_ret = 0, ret_val = 0;
    int                     left_t = 0;


    if ((left_t = _lorsRemainTime(lm->begin_time,timeout)) < 0) {
        ret_val = LORS_TIMEOUT_EXPIRED;
        fprintf(stderr, "bailing.\n");
        goto bail;
    }; 

    if ( timeout > 0) {
       timer.ServerSync = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT: left_t + 1);
       timer.ClientTimeout = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT : left_t -1);
    }else{
       timer.ServerSync = 0;
       timer.ClientTimeout = 0;
    };

    /*fprintf(stderr, "entering IBP_manage()\n");*/
    ibp_ret = IBP_manage(lm->capset.manageCap, &timer, IBP_DECR, IBP_READCAP, NULL);
    if ( ibp_ret != 0 )
    {
        /*fprintf(stderr, "fuck: %d\n", ibp_ret);*/
        ret_val = IBP_errno + LORS_IBP_FAILED;
        /*fprintf(stderr, "errno: %d\n", ret_val);*/
    } else 
    {
        /*fprintf(stderr, "fuck2: %d\n", ibp_ret);*/
        ret_val = LORS_SUCCESS;
        ret_val = lorsMappingStat(lm, timeout);
        /*fprintf(stderr, "fuck3: %d\n", ibp_ret);*/
        /*fprintf(stderr, "errno3: %d\n", ret_val);*/
        /* 
         * If the decrement was successful, it may be ok for the probe to fail, 
         * but if it does, we want to set lm->capstat values to -1...
         * lorsMappingStat() does this.
         * ret_val will not be LORS_SUCCESS, however.
         */
    }

bail:

    return ret_val;
}

int    lorsSetTrimMapping (LorsSet  *se, 
                           LorsMapping *map,
                           int  timeout,
                           int  opts)
{
    /*
     * remove mapping from se->mapping_map
     * if ( opts & LORS_TRIM_DECR )
     * {
     *     IBP_manage(IBP_PROBE, DECR)
     *
     * } else if ( opts & LORS_TRIM_KILL ) {
     *     while ( read_refcount > 0 )
     *     {
     *         IBP_manage(IBP_PROBE, DECR)
     *     }
     * } else if ( opts & LORS_TRIM_NOKILL ) {
     *    It is sufficient to remove from se->mapping_map
     * } else 
     * {
     *     undefined
     * }
     */
    return LORS_FAILURE;
}

