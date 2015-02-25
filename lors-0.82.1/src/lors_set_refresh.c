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

int _lorsDoRefresh( __LorsJob *job, __LorsThreadInfo *tinfo)
{
    int                 opts, ret, ibp_ret, ret_val;
    int                 opt_mask = LORS_REFRESH_MAX|LORS_REFRESH_EXTEND_BY|
                                LORS_REFRESH_EXTEND_TO|LORS_REFRESH_ABSOLUTE;
    time_t              duration = 0;
    struct ibp_timer    timer;
    IBP_DptInfo         dptinfo = NULL;
    LorsMapping         *lm;
    time_t              begin_time, left_t;

    assert ( (job != NULL) && (tinfo != NULL) );
    lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: Doing Job Set Refresh\n",
              __FUNCTION__, pthread_self());
    pthread_cleanup_push(_lorsDoCleanup,(void *)job); 

    duration = *((time_t *)job->extra_data);

    lm = (LorsMapping *)(job->src);

    begin_time = lm->begin_time;
    opts = tinfo->opts;


    switch(opt_mask & opts)
    {
        case LORS_REFRESH_MAX:
            /*  call ibp_status to discover the 
             * maximum duration for this depot.*/
            if ( (left_t = _lorsRemainTime(begin_time,job->timeout)) < 0 ) {
                ret = LORS_TIMEOUT_EXPIRED;
                _lorsCommitJob(job,JOB_COMMIT,LORS_TIMEOUT_EXPIRED);
                goto bail;
            };

            if ( job->timeout > 0){
               timer.ServerSync = LORS_MIN_TIMEOUT; 
               timer.ClientTimeout = LORS_MIN_TIMEOUT; 
            }else {
               timer.ServerSync = 0;
               timer.ClientTimeout = 0;
            };
            duration = begin_time;

            dptinfo = IBP_status(&lm->depot, IBP_ST_INQ, &timer, "ibp", 0, 0, 0);
            if ( dptinfo == NULL )
            {
                /*fprintf(stderr, "dptinfo == NULL Failure\n");*/
                _lorsCommitJob(job,JOB_COMMIT,LORS_FAILURE);
                return LORS_FAILURE;
            } else {
                /*fprintf(stderr, "dur: %d\n", dptinfo->Duration);
                fprintf(stderr, "stable: %d\n", dptinfo->StableStorUsed);
                fprintf(stderr, "volatile: %d\n", dptinfo->VolStorUsed);
                */
            }

            duration += dptinfo->Duration;
            lm->capstat.attrib.duration = duration;
            break;

        case LORS_REFRESH_EXTEND_BY:
            /*add a particular amount of time to the allocation*/
            lm->capstat.attrib.duration += duration;
            break;
        case LORS_REFRESH_EXTEND_TO:
            /*add amount of time from 'now' i.e. expire a day from now.*/
            duration = time(0) + duration;
            lm->capstat.attrib.duration = duration;
            break;
        case LORS_REFRESH_ABSOLUTE:
            lm->capstat.attrib.duration = duration;
            /*set expiration to an absolute time.*/
            break;
        default:

            lorsDebugPrint(D_LORS_VERBOSE,"unknown conditoin: 0x%x, 0x%x 0x%x\n", 
                    opts, opt_mask, opt_mask & opts);
            break;
    }

    do{
        if ( (left_t = _lorsRemainTime(begin_time,job->timeout)) < 0 ) {
            ret = LORS_TIMEOUT_EXPIRED;
            _lorsCommitJob(job,JOB_COMMIT,LORS_SUCCESS);
            goto bail;
        };
        if ( job->timeout > 0){
           timer.ServerSync = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT : left_t + 1);
           timer.ClientTimeout = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT: left_t - 1);
        }else {
           timer.ServerSync = 0;
           timer.ClientTimeout = 0;
        };
        /* call IBP_manage() */

        lorsDebugPrint(D_LORS_VERBOSE, "server:%d client:%d chng:%d time:%d\n",
                        timer.ServerSync, timer.ClientTimeout, 
                        IBP_CHNG, lm->capstat.attrib.duration);

        lorsDebugPrint(D_LORS_VERBOSE, "expiretime Before: %d\n", 
                lm->capstat.attrib.duration);
        ibp_ret = IBP_manage(lm->capset.manageCap, &timer, IBP_CHNG, 0, &lm->capstat);
        ret = ( ibp_ret == 0 ? LORS_SUCCESS : LORS_IBP_FAILED + IBP_errno);
        if ( ret == LORS_SUCCESS )
        {
            lorsDebugPrint(D_LORS_VERBOSE, "expiretime After:  %d\n", 
                    lm->capstat.attrib.duration);
        } else if ( ret == (LORS_IBP_FAILED + IBP_E_LONG_DURATION) )
        {
            break;
        } else if ( ret == (LORS_IBP_FAILED + IBP_E_CAP_NOT_FOUND) )
        {
            break;
        } else 
        {
            break;
        }
        lorsDebugPrint(D_LORS_VERBOSE, "IBP_RET: %d, ret:%d \n", ibp_ret, ret);

    } while (ibp_ret != 0 ); /*&& !have_opt(fail_on_any));*/

    _lorsCommitJob(job,JOB_COMMIT,ret);
bail:
    pthread_cleanup_pop(0);
    return ret;

}

int    lorsSetRefresh (LorsSet * set,
                       longlong offset,
                       longlong length,
                       time_t duration,
                       int nthreads,
                       int timeout,
                       int opts)
{
    __LorsThreadPool   *tp = NULL;
    __LorsJobPool      *jp_refresh = NULL;
    __LorsJob          *job = NULL, *job_refresh = NULL;
    int                 ret, cnt, ret_val;
    time_t              begin_time;
    int                 i;
    JRB                 jrb_node;
    LorsMapping        *lm = NULL;
    Dllist              mapping_list = NULL;

    if ( length == -1 ) 
    {
        length = set->max_length;
    }

    if ( (ret_val = _lorsCreateJobPool(&jp_refresh)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetRefresh: can't create job pool!\n");
        goto bail; 
    }; 
    jp_refresh->type= J_CREATE;
    jp_refresh->src_offset = offset;
    jp_refresh->src_length = length;

    /* mark the beginning of this operation. */
    begin_time = time(0);

    cnt = 0;
    jrb_rtraverse(jrb_node,set->mapping_map)
    {
        lm = (LorsMapping *)jval_v(jrb_node->val);
        if ( length > 0){
            if ( (length + offset) <= lm->exnode_offset)
               continue;
        };
        if ((lm->exnode_offset + lm->logical_length) <= offset )
               continue;
        /* create job */
        job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
        if ( job == NULL ) {
            ret_val = LORS_NO_MEMORY;
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetRefresh :Out of memory!\n");
            goto bail;
        };
        job->type = J_REFRESH;
        job->status = JOB_INIT;
        job->src = lm;
        job->timeout = timeout;
        job->extra_data = &duration;
        /* add job to jobpool */
        cnt++;
        _lorsAddJob(jp_refresh,job);
    };
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetRefresh: Creating working threads "
                                  "for SetRefresh.\n");
    ret_val = _lorsCreateThreadPool(jp_refresh,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
    {
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetRefresh: _lorsCreateThreadPool failed..\n");
        goto bail;
    }
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetRefresh: Waiting for threads return.\n");
#if 0
    for (i=0; i < tp->nthread; i ++ )
    {
        if ( ret != LORS_SUCCESS )
        {
            /* TODO: perhaps keep a 'failed_list' to do something with */
            ret_val = ret;
        }
    };
#endif
    ret_val = _lorsJoinThread(tp);
    /*if ( ret_val != LORS_SUCCESS )*/
        /*goto bail;*/

    {
        Dllist node;
        int    cnt = 0;
        dll_traverse(node, jp_refresh->job_list)
        {
            job= (__LorsJob *)node->val.v;
            if ( job->err_code != LORS_SUCCESS )
            {
                lm = (LorsMapping*)job->dst;
                /* TODO: find the mappind number. */
                lorsDebugPrint(D_LORS_VERBOSE, "Failed Refresh on mapping %d\n", cnt);
            }
            cnt++;
        }
    }
    _lorsFreeJobPool(jp_refresh);
    _lorsFreeThreadPool(tp);
    return LORS_SUCCESS;
bail:
    if ( jp_refresh != NULL ) _lorsFreeJobPool(jp_refresh);
    if ( tp != NULL ) _lorsFreeThreadPool(tp);
    return LORS_FAILURE;
}

