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

int    lorsSetStat(LorsSet       *set, 
                   int            nthreads,
                   int            timeout,
                   int            opts)
{
    JRB         node; 
    int         cnt=0, ret_val, ibp_ret;
    time_t      begin_time;
    int         i, ret;

    LorsMapping        *lm = NULL;
    __LorsThreadPool   *tp = NULL;
    __LorsJobPool      *jp_stat = NULL;
    __LorsJob          *job = NULL;


    if ( (ret_val = _lorsCreateJobPool(&jp_stat)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStat: can't create job pool!\n");
        goto bail; 
    }; 
    jp_stat->type= J_CREATE;
    jp_stat->src_offset = 0;
    jp_stat->src_length = set->max_length;
    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr, "MESSAGE 1 List\n");
	    fflush(stderr);
    }

    cnt = 0;
    begin_time = time(0);
    i=0;
    jrb_rtraverse(node, set->mapping_map)
    {
        lm = (LorsMapping *)node->val.v;
        /* create a new job and put into job pool */
        job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
        if ( job == NULL ) {
            ret_val = LORS_NO_MEMORY;
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetStat :Out of memory!\n");
            goto bail;
        };
        job->type = J_STAT;
        job->status = JOB_INIT;
        job->src = lm;
        lm->begin_time = begin_time;

        lm->capstat.readRefCount = -1;
        lm->capstat.writeRefCount = -1;
        lm->capstat.attrib.duration = -1;
        lm->i = i;
        i++;

        /* add to job pool */
        cnt++;
        _lorsAddJob(jp_stat,job);
    }
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStat: Creating working threads "
                                  "for IBP_manage.\n");
    ret_val = _lorsCreateThreadPool(jp_stat,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
    {
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStat: _lorsCreateThreadPool failed..\n");
        goto bail;
    }

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetStat: Waiting Condition threads return.\n");


    ret_val = _lorsJoinThread(tp);

#if 0
    if ( !(opts&LORS_LIST_HUMAN) )
    {
        LorsEnum  list = NULL, iter = NULL;
        int lret;
        i = 0;
        lorsSetEnum(set, &list);
        do 
        {
            char    *s;
            char buf[48];
            struct tm *tmp;

            lret = lorsEnumNext(list, &iter, &lm);
            if ( lret == LORS_END ) break;

            if ( lm->capstat.attrib.duration == -1 )
            {
                s = "DEAD";
                sprintf(buf, "%s", s);
            } else 
            {
                tmp = localtime(&lm->capstat.attrib.duration);
                strftime(buf, 32, "%b %d", tmp);
                s = strdup(buf);
                sprintf(buf, "(%d) %s", i, s);
                free(s);
                s = NULL;
            }

#ifdef _MINGW
            lorsDemoPrint(stderr,"DRAW Mapping %d %I64d %ld %s:%4d %s\n",
                      lm->id,lm->exnode_offset,
                      (opts & LORS_LIST_PHYSICAL?lm->alloc_length:lm->logical_length ),
                      lm->depot.host,lm->depot.port, buf);
	    fflush(stderr);
#else
            lorsDemoPrint(stderr,"DRAW Mapping %d %lld %ld %s:%4d %s\n",
                      lm->id,lm->exnode_offset,
                      (opts & LORS_LIST_PHYSICAL?lm->alloc_length:lm->logical_length ),
                      lm->depot.host,lm->depot.port, buf);
#endif
            i++;
        } while ( lret != LORS_END );
        lorsEnumFree(list);
        lorsDemoPrint(stderr,"MESSAGE 1 Done\n");
    }
#endif
    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr,"MESSAGE 1 Done\n");
	    fflush(stderr);
    }

bail:
    _lorsFreeJobPool(jp_stat);
    _lorsFreeThreadPool(tp);
    return LORS_SUCCESS;
}
int _lorsDoStat( __LorsJob *job, __LorsThreadInfo *tinfo)
{
    LorsMapping *lm;
    struct ibp_timer          timer;
    struct ibp_attributes     *ibpattr = NULL;
    IBP_set_of_caps           socaps = NULL;
    int                       ret = LORS_FAILURE;
    double                    difftime = 0;
    int                       opts = 0;
    int                       ibp_ret = 0;
    ulong_t                  written = 0;
    LorsDepot                *ldepot = NULL;
    time_t                   begin_time,left_t;

            char    *s;
            char buf[48];
            struct tm *tmp;

    assert ( (job != NULL) && (tinfo != NULL) );
    lorsDebugPrint(D_LORS_VERBOSE,"%s[%d]: Doing Job Set Stat\n",
              __FUNCTION__, pthread_self());


    /*
     * set up clean up routine
     */
    pthread_cleanup_push(_lorsDoCleanup,(void *)job); 

    lm = (LorsMapping *)(job->src);
    begin_time = lm->begin_time;
    opts = tinfo->opts;

    ret = lorsMappingStat(lm, job->timeout);

    if ( g_lors_demo )
    {
      if ( !(opts&LORS_LIST_HUMAN) )
      {
        if ( lm->capstat.attrib.duration == -1 )
        {
            s = "DEAD";
            sprintf(buf, "%s", s);
        } else 
        {
            tmp = localtime(&lm->capstat.attrib.duration);
            strftime(buf, 32, "%b %d", tmp);
            s = strdup(buf);
            sprintf(buf, "(%d) %s", lm->i, s);
            free(s);
            s = NULL;
        }
#ifdef _MINGW
	    lorsDemoPrint(stderr,"DRAW Mapping %d %I64d %ld %s:%4d %s\n",
            lm->id,lm->exnode_offset,
            (opts & LORS_LIST_PHYSICAL?lm->alloc_length:lm->logical_length ),
            lm->depot.host,lm->depot.port, buf);
#else	
        lorsDemoPrint(stderr,"DRAW Mapping %d %lld %ld %s:%4d %s\n",
            lm->id,lm->exnode_offset,
            (opts & LORS_LIST_PHYSICAL?lm->alloc_length:lm->logical_length ),
            lm->depot.host,lm->depot.port, buf);
#endif
	        fflush(stderr);
      }
    }

#if 0
    if ( !(opts&LORS_LIST_HUMAN) )
    {
        char    *s;
        char buf[48];
        struct tm *tmp;
        int     i=0;

        if ( lm->capstat.attrib.duration == -1 )
        {
            s = "DEAD";
            sprintf(buf, "%s", s);
        } else 
        {
            tmp = localtime(&lm->capstat.attrib.duration);
            strftime(buf, 32, "%b %d", tmp);
            s = strdup(buf);
            sprintf(buf, "(%d) %s", i, s);
            free(s);
            s = NULL;
        }
#ifdef _MINGW
        lorsDemoPrint(stderr,"DRAW Mapping %d %I64d %ld %s:%4d %s\n",
                  lm->id,lm->exnode_offset,
                  (opts & LORS_LIST_PHYSICAL?lm->alloc_length:lm->logical_length ),
                  lm->depot.host,lm->depot.port, buf);
	fflush(stderr);
#else
        lorsDemoPrint(stderr,"DRAW Mapping %d %lld %ld %s:%4d %s\n",
                  lm->id,lm->exnode_offset,
                  (opts & LORS_LIST_PHYSICAL?lm->alloc_length:lm->logical_length ),
                  lm->depot.host,lm->depot.port, buf);
#endif
    }
#endif

    pthread_cleanup_pop(0);
    _lorsCommitJob(job,JOB_COMMIT,LORS_SUCCESS);

    /* previously: return ret;, this stopped the thread queue; oops. */
    return LORS_SUCCESS;

};

int lorsMappingStat(LorsMapping *lm, time_t timeout)
{
    int                     left_t;
    struct ibp_timer        timer;
    int                     ibp_ret, ret_val, ret;

    if ((left_t = _lorsRemainTime(lm->begin_time, timeout)) < 0) {
        ret = LORS_TIMEOUT_EXPIRED;
        /* added to prevent expiration == 1969 dec 31 on timeout */

        fprintf(stderr, "I am in timeout!!!!!!!\n");
        lm->capstat.readRefCount = -1;
        lm->capstat.writeRefCount = -1;
        lm->capstat.attrib.duration = -1;
        return ret;
    }; 

    
    if ( timeout > 0 ){
       timer.ServerSync = 0; 
       timer.ClientTimeout = (left_t > LORS_MIN_TIMEOUT ? LORS_MIN_TIMEOUT : left_t );
    }else {
       timer.ServerSync = 0;
       timer.ClientTimeout = 0;
    };

    /* PROBE THe capability to deterimin size as well as refcounts. */
    ibp_ret = IBP_manage(lm->capset.readCap, &timer, IBP_PROBE, 0, &lm->capstat);
    ret = ( ibp_ret == 0 ? LORS_SUCCESS : LORS_IBP_FAILED + IBP_errno);
    if ( ret == LORS_SUCCESS )
    {
        lm->seek_offset = lm->capstat.currentSize;
        lm->alloc_length = lm->capstat.maxSize;

        /*fprintf(stderr, "success: woop.\n");*/
    } else 
    {
        lm->capstat.readRefCount = -1;
        lm->capstat.writeRefCount = -1;
        lm->capstat.attrib.duration = -1;
        /*fprintf(stderr, "failure: %d.\n", ret);*/
    }
    return ret;
}


