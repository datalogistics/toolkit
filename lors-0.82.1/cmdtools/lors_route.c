#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lors_api.h>
#include <xndrc.h>
#include <popt.h>
#include <lors_file_opts.h>
#include <time.h>

typedef struct __LorsRoutePipe 
{
    pthread_t       tid;
    pthread_mutex_t *lockRead;
    pthread_cond_t  *condRead;
    pthread_mutex_t *lockWrite;
    pthread_cond_t  *condWrite;

    longlong         maxlength;
    LorsSet         *inputSet;
    LorsSet         *outputSet;
    ulong_t          blocksize;
    LorsDepotPool   *dp;
}LorsRoutePipe;

/* return in 'lrp'.
 * the DepotPool will be built on 'depot'
 * inputSet is set to 'set'.
 * outputSet is allocated for you.
 */
int createRoutePipe(LorsRoutePipe **lrp, IBP_depot depot, ulong_t blocksize, 
                    LorsSet *set, pthread_cond_t *cond, pthread_mutex_t *lock)
{
    LorsRoutePipe   *rp = NULL;
    IBP_depot       d[2];

    d[0] = depot;
    d[1] = NULL;

    rp = calloc(1, sizeof(LorsRoutePipe));

    rp->lockWrite = calloc(1, sizeof(pthread_mutex_t));
    rp->condWrite = calloc(1, sizeof(pthread_cond_t));
    pthread_mutex_init(rp->lockWrite, NULL);
    pthread_cond_init(rp->condWrite, NULL);

    rp->lockRead = lock;
    rp->condRead = cond;

    if ( lorsSetInit(&rp->outputSet, blocksize, 1, 0) != LORS_SUCCESS )
    {
        *lrp = NULL;
        return LORS_FAILURE;
    }

    if ( lorsGetDepotPool(&rp->dp, "dsj.sinrg.cs.utk.edu", 6714, d, 1, NULL, 
                        1024*1024, IBP_SOFT, 1000,
                        1, 20, 0) != LORS_SUCCESS )
    {
        fprintf(stderr, "GetDepotPool Failed.\n");
        return LORS_FAILURE;
    }

    rp->maxlength = 0;
    rp->blocksize = blocksize;
    rp->inputSet = set;

    *lrp = rp;

    return LORS_SUCCESS;
}
void *lorsRouteCopy(void *v)
{
    int lret;
    LorsSet     *in, *out;
    LorsRoutePipe   *lrp;
    longlong            offset = 0;
    longlong            len;
    struct timespec     ts;
    int                 cnt=0;


    lrp = (LorsRoutePipe *)v;
    in = lrp->inputSet;
    out = lrp->outputSet;

#if 1
    while ( offset < lrp->maxlength )
    {
        /* lock read from A */
        len = ( (offset + lrp->blocksize) > lrp->maxlength ? 
                lrp->maxlength - offset :
                lrp->blocksize+1);
        len = ( lrp->maxlength == in->max_length ? len-1 : len);
        if ( lrp->condRead != NULL && 
             in->max_length < out->max_length+len)
        {
            cnt=0;
            do {
                ts.tv_sec = time(0)+5;
                ts.tv_nsec = 0;
                if ( cnt > 0 )
                {
                    pthread_mutex_unlock(lrp->lockRead);
                }
                pthread_mutex_lock(lrp->lockRead);
                pthread_cond_timedwait(lrp->condRead, lrp->lockRead, &ts);
                cnt++;
                /*fprintf(stderr, "in->max_len: %lld out->max_len+len: %lld\n",*/
                        /*in->max_length, out->max_length+len);*/
                fprintf(stderr, "leaving timedwait.\n");
            } while ( in->max_length < out->max_length+len );
            /*fprintf(stderr, "in->maxlength >= out->max_len+len\n");*/
        }
        /* lock write to B */
        /*pthread_mutex_lock(lrp->lockWrite);*/

        do 
        {
            len = ( (offset + lrp->blocksize) > lrp->maxlength ? 
                    lrp->maxlength - offset :
                    lrp->blocksize);

#ifdef _MINGW
            fprintf(stderr, "blocksize: %d, maxlength: %I64d offset: %I64d\n", 
                    lrp->blocksize, lrp->maxlength, offset);
            fprintf(stderr, "length: %I64d\n", len);
#else
            fprintf(stderr, "blocksize: %d, maxlength: %lld offset: %lld\n", 
                    lrp->blocksize, lrp->maxlength, offset);
            fprintf(stderr, "length: %lld\n", len);
#endif

            lret = lorsSetCopy(in, out, lrp->dp, NULL, 
                               offset, len,
                               2, 150, LORS_COPY);
            if ( lret != LORS_SUCCESS ) {
                fprintf(stdout, "Copy Error.: %d\n", lret);
            }
            if ( lret == LORS_HAS_HOLE )
            {
                return NULL;
            }
        } while (lret == -10004 );
        if ( lrp->lockRead != NULL )
        {
            pthread_mutex_unlock(lrp->lockRead);
        }
        pthread_cond_signal(lrp->condWrite);
        /*pthread_mutex_lock(lrp->lockWrite);*/
        /*pthread_mutex_unlock(lrp->lockWrite);*/

        if ( lret != LORS_SUCCESS )
        {
            fprintf(stderr, "Copy Error.: %d\n", lret);
            exit(EXIT_FAILURE);
        } else 
        {
            offset += len;
        }
    }
#endif
    return NULL;
}
int main(int argc, char **argv)
{
    LorsRoutePipe   *lpr[8];
    pthread_mutex_t *lock = NULL;
    pthread_cond_t  *cond = NULL;
    int             i;
    ulong_t         data_blocksize = 0;
    char            *s_data_blocksize;
    const char            *filename;
    char            *rcfile = NULL;
    XndRc           xndrc;
    XndRc           drc;
    poptContext optCon;   
    LorsExnode          *exnode = NULL;
    LorsSet             *set = NULL;
    longlong                maxlength = 0;
    int                 v = 0, lret;
    double              t1, t2;
    struct poptOption optionsTable[] = {
  { "samename",   'f', POPT_ARG_NONE,   0,                  SAME_OUTPUTNAME, 
        "Specify the exNode filename by appending .xnd to the original filename.", NULL },
  { "bs", 'b', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH, &s_data_blocksize,  DATA_BLOCKSIZE, 
        "Specify the logical data blocksize of input file.", "'5M'"},
  { "xndrc", 'x', POPT_ARG_STRING, &rcfile, RCFILE, 
        "You may specify an alternate source for the route Depot List.", "<filename>"},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };
    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<filename>");
    if ( argc < 2 )
    {
        poptPrintUsage(optCon, stderr, 0);
        poptPrintHelp(optCon, stderr, 0);
                                 exit(EXIT_FAILURE);
    }
    memset(&drc, 0, sizeof(xndrc));
    memset(&xndrc, 0, sizeof(xndrc));
    parse_xndrc_files(&xndrc);
    while ( (v=poptGetNextOpt(optCon)) >= 0)
    {
        switch(v)
        {
            case DATA_BLOCKSIZE:
                if ( strtobytes(s_data_blocksize, &xndrc.data_blocksize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for "
                                    "internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case RCFILE:
                break;
            default:
                fprintf(stderr, "unknown: %d\n", v);
                break;
        }
    }
    if ( rcfile != NULL )
    {
        if ( read_xndrc_file(&drc, rcfile) != 0 )
        {
            fprintf(stderr, "Unable to open specified rcfile\n");
            exit(EXIT_FAILURE);
        }
        /* lors_route now uses ROUTE_DEPOT directive to choose destinations. */
        xndrc.depotList = drc.depotList1;
    }

    filename = poptGetArg(optCon);
    if ( filename == NULL )
    {
        fprintf(stderr, "error: provide filename\n");
        exit(EXIT_FAILURE);
    }
    lret = lorsFileDeserialize(&exnode, filename, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "deserialize failed! : %s: %d\n", filename, lret);
        exit(EXIT_FAILURE);
    }              
    lret = lorsQuery(exnode, &set, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "Query Failed!: %d\n", lret);
        exit(EXIT_FAILURE);
    }              

    t1 = getTime();

    for (i=0; i < 8 && xndrc.depotList[i] != NULL; i++)
    {
        fprintf(stderr, "Creating Pipe for Depot : %s:%d\n", 
                        xndrc.depotList[i]->host,
                        xndrc.depotList[i]->port);
        createRoutePipe(&lpr[i], xndrc.depotList[i], xndrc.data_blocksize, 
                        set, cond, lock);
        if ( i == 0 ) 
        {
#ifdef _MINGW
            fprintf(stderr, "SetMaxLength: %I64d\n", set->max_length);
            maxlength = set->max_length;
#else
            fprintf(stderr, "SetMaxLength: %lld\n", set->max_length);
            maxlength = set->max_length;
#endif
        }
        lpr[i]->maxlength = maxlength;
        set = lpr[i]->outputSet;
        lock = lpr[i]->lockWrite;
        cond = lpr[i]->condWrite;
        pthread_create(&lpr[i]->tid, NULL, lorsRouteCopy, lpr[i]);

    }
    /* wait for the last working thread to return. */
    pthread_join(lpr[i-1]->tid, NULL);

    t2 = diffTime(t1);
    fprintf(stderr, "MESSAGE 2.6 Gross %.4f Mbps\n", 
            exnode->logical_length*8*i/(1024*1024)/t2);
    fprintf(stderr, "MESSAGE 3 Avg   %.4f Mbps\n", 
            exnode->logical_length*8/(1024*1024)/t2);
    return 0;
}
