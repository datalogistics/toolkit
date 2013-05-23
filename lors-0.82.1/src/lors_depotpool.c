#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "lors_api.h"
#include "lors_util.h"
#include "lors_error.h"
#include "lbone_client_lib.h"
#include "lors_opts.h"


#define lorsDemoPrint fprintf
void printDepotPool(LorsDepotPool *dpp);
int lorsFreeDpList( IBP_depot *dpList);

int lorsCreateDepotList(LorsDepotList **dpl)
{
    LorsDepotList   *l;
    l = (LorsDepotList *)calloc(1, sizeof(LorsDepotList));
    if ( l == NULL ) return -1;
    l->list = new_dllist();
    if ( l->list == NULL ) { free(l); return -1; }
    l->bad_list = new_dllist();
    if ( l->bad_list == NULL ) { free_dllist(l->list); free(l); return -1; }
    l->count = 0;

    pthread_mutex_init(&l->lock, NULL);

    *dpl = l;
    return LORS_SUCCESS;
}

int lorsGetNextDepot(LorsDepotList *dpl, LorsDepot **dp)
{
    Dllist      node;
    LorsDepot   *d = NULL;
    pthread_mutex_lock(&dpl->lock);
    if ( dpl->count > 0 )
    {
        node = dll_first(dpl->list);
        d = node->val.v;
        dll_delete_node(node);
        dll_append(dpl->list, new_jval_v(d));
    } else 
    {
        *dp = NULL;
        pthread_mutex_unlock(&dpl->lock);
        return LORS_NOTFOUND;
    }
    pthread_mutex_unlock(&dpl->lock);
    *dp = d;
    return LORS_SUCCESS;
}

int lorsRemoveDepot(LorsDepotList *dpl, IBP_depot dp)
{
    Dllist      node;
    LorsDepot   *d;
    pthread_mutex_lock(&dpl->lock);
    dll_traverse(node, dpl->list)
    {
        d = node->val.v;
        if ( strcmp(d->depot->host, dp->host) == 0 )
        {
            dll_delete_node(node);
            dll_append(dpl->bad_list, new_jval_v(d));
            dpl->count--;
            break;
        }
    }
    pthread_mutex_unlock(&dpl->lock);
    return LORS_SUCCESS;
}

int lorsAddDepot(LorsDepotList *dpl, LorsDepot *dp)
{
    Dllist          node;
    LorsDepot       *d;
    int             bad = 0, already_there = 0;
    if (dpl==NULL) 
    if (&(dpl->lock)==NULL) 
    pthread_mutex_lock(&dpl->lock);
    dll_traverse(node, dpl->bad_list)
    {
        d = node->val.v;
        if ( strcmp(d->depot->host, dp->depot->host) == 0 )
        {
            bad++;
        }
    }
    dll_traverse(node, dpl->list)
    {
        d = node->val.v;
        if ( strcmp(d->depot->host, dp->depot->host) == 0 )
        {
            already_there++;
        }
    }
    if ( !bad && !already_there )
    {
        /*fprintf(stderr, "Adding: %s\n", dp->depot->host);*/
        dll_prepend(dpl->list, new_jval_v(dp));
        dpl->count++;
    }
    pthread_mutex_unlock(&dpl->lock);
    return (bad||already_there);
}

int lorsTagDepot(LorsDepot *dp, int tag)
{
    return -1;
}
int lorsRemoveTagged(LorsDepotList *dpl)
{
    return -1;
}
int lorsCreateListFromPool(LorsDepotPool *dpp, LorsDepotList **dpl, int count)
{
    JRB         node;
    LorsDepotList   *l;
    LorsDepot       *dp;
    int         sub_count= 0, ret = 0;
    /* create a depot list from depots in the dpp */
    if ( dpp == NULL || count == 0 )
    {
        return -1;
    }
    lorsCreateDepotList(&l);
    if ( l == NULL ) {
        return -1;
    }
    pthread_mutex_lock(dpp->lock);
    jrb_rtraverse(node, dpp->list)
    {
        if ( sub_count < count )
        {
            /* for the first 'count' unique depots */
            dp = node->val.v;

            /*fprintf(stderr, "Adding %s\n", dp->depot->host);*/
            ret = lorsAddDepot(l, dp);
            if ( ret == LORS_SUCCESS ) sub_count++;
        } else {
            break;
        }
    }
    /* TODO: problem: if sub_count < count then more depots requested than returned */
    if ( sub_count < count )
    {
        /*fprintf(stderr, "returning less than requested.\n");*/
        /* nop */
    }
    *dpl = l;
    pthread_mutex_unlock(dpp->lock);
    return LORS_SUCCESS;
}
int lorsRefillListFromPool(LorsDepotPool *dpp, LorsDepotList *dpl, int count)
{
    LorsDepot       *dp;
    JRB             node;
    int             sub_count = 0, ret = 0;
    /* compare current count to requested count */
    if ( dpl->count < count )
    {
        pthread_mutex_lock(dpp->lock);
        jrb_traverse(node, dpp->list)
        {
            if ( sub_count + dpl->count < count )
            {
                /* for the first 'count' unique depots */
                dp = node->val.v;
                /*fprintf(stderr, "Adding %s\n", dp->depot->host);*/
                ret = lorsAddDepot(dpl, dp);
                if ( ret == LORS_SUCCESS ) sub_count++;
            } else {
                break;
            }
        }
        /* TODO: problem: if (sub_count+dpl->count) < count then more depots 
         *   requested than returned , we should spawn an lbone_getDepots() call */
        if ( (sub_count + dpl->count) < count )
        {
            /*fprintf(stderr, "returning less than requested.\n");*/
            /* nop */
        }
        pthread_mutex_unlock(dpp->lock);
    }
    return LORS_SUCCESS;
}

 
static void printDepot(LorsDepot *dp, int no);
static double _lorsScoreDepot ( LorsDepot *dp)
{
    dp->score = dp->nthread + dp->nthread_done + dp->nfailure;
    return ( dp->score);
};

static void _lorsFreeDepot ( LorsDepot *dp )
{
    if ( dp != NULL ){
        if ( dp->depot != NULL ) {
            free (dp->depot);
            dp->depot = NULL;
        };
        if ( dp->lock != NULL ) {
            free (dp->lock);
            dp->lock = NULL;
        };
        
        if ( dp->tmp != NULL ) {
            free (dp->tmp);
            dp->tmp = NULL;
        };
        free ( dp );
    };    
};

static void _lorsFreeDepotJRBTree(JRB list)
{
    JRB       tmp;
    LorsDepot *dp = NULL ;
    if ( list != NULL ){
        jrb_traverse(tmp,list) {
            dp = (LorsDepot *)(jrb_val(tmp).v);
            assert ( dp != NULL );
            _lorsFreeDepot(dp); 
        };
    };
    jrb_free_tree(list);
};

int  lorsFreeDpList( IBP_depot *dpList)
{ 
    int i = 0;
    if ( dpList != NULL ){
        for ( i = 0 ; dpList[i] != NULL ; i ++ )
        {
            /*fprintf(stderr, "freeing: %s %d\n", dpList[i]->host, i);*/
            free(dpList[i]);
            dpList[i] = NULL;
        }

        /*fprintf(stderr, "freeing list\n");*/
        free(dpList);
    };
    return LORS_SUCCESS;
};

static LorsDepotPool * 
_lorsAllocDepotPool ( void ) 
{
     LorsDepotPool *dpp = NULL;

     if ( (dpp = (LorsDepotPool *)calloc(1,sizeof(LorsDepotPool))) == NULL ){
         return NULL;
     };

     if ( (dpp->lock = (pthread_mutex_t *)calloc(1,sizeof(pthread_mutex_t))) == NULL ){
         free(dpp);
         return NULL;
     };

     if ((dpp->lboneserver = ( IBP_depot)calloc(1,sizeof(struct ibp_depot))) == NULL ){
         free(dpp->lock);
         free(dpp);
         return(NULL);
     };

     pthread_mutex_init(dpp->lock,NULL);
     dpp->list = make_jrb();
     /* updated by YING 
     dpp->dl = new_dllist(); */
     /* new updated by YING */
     lorsCreateDepotList(&(dpp->dl));
     return dpp;
};

static int
_createDepotPool ( LorsDepotPool **depot_pool,
                  IBP_depot      *depots,
                  char           *lbone_server,
                  int            lbone_server_port,
                  int            min_unique_depots,
                  char           *location,
                  ulong_t        storage_size,
                  int            storage_type,
                  time_t         duration,
                  int            timeout )
{
     int ret = LORS_SUCCESS;
     LorsDepotPool *dpp = NULL ;
    
    /*
     * check parameters
     */
    if ( (lbone_server == NULL) && ( depots == NULL ) ){
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: Both lbone_server and depots are NULL\n");
        ret =  LORS_INVALID_PARAMETERS ;
        goto bail;
    };

    if ( (depots == NULL ) && (lbone_server_port <=0 )){
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: lbone_server_port <=0 \n");
        ret =  LORS_INVALID_PARAMETERS ;
        goto bail;
    };

    if ( min_unique_depots <=0 ) {
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: min_unique_depots <=0\n");
        ret =  LORS_INVALID_PARAMETERS ;
        goto bail;
    };

    if ((storage_type != IBP_HARD) && ( storage_type != IBP_SOFT)){
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: Unknown IBP storage type( %d )\n",storage_type);
        ret =  LORS_INVALID_PARAMETERS ;
        goto bail;
    };

    if ( storage_size == 0 ) {
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: storage_size < 0\n");
        ret =  LORS_INVALID_PARAMETERS ;
        goto bail;
    };

    if ( duration< 0 ) {
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: duration_days < 0\n");
        ret =  LORS_INVALID_PARAMETERS ;
        goto bail;
    };

/*   
    if ( timeout < 0 ) {
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: timeout < 0\n");
        ret =  LORS_INVALID_PARAMETERS ;
        return( LORS_INVALID_PARAMETERS);
    };
*/
    /*
     * allocate memory
     */
     if ( (dpp = _lorsAllocDepotPool()) == NULL ){
        lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: Out of memory\n");
        ret =  LORS_NO_MEMORY ; 
        goto bail;
     };


     
     /*
      * set initial value
      */
     if ( lbone_server != NULL )
     {
        strncpy(dpp->lboneserver->host,lbone_server,IBP_MAX_HOSTNAME_LEN-1);
     }
     dpp->lboneserver->port = lbone_server_port;
     dpp->min_unique_depots = min_unique_depots;
     if ( location != NULL ){
        if ( (dpp->location = strdup(location)) == NULL ){
           lorsDebugPrint( D_LORS_ERR_MSG,"createDepotPool: Out of memory when initial lock\n");
           ret = LORS_NO_MEMORY;
           goto bail;
        };
     };
     dpp->size = storage_size;
     dpp->type = storage_type;
     dpp->duration = duration;
     dpp->timeout = timeout;
  
     *depot_pool = dpp;
     return(ret);

bail:
     if ( dpp != NULL )
        lorsFreeDepotPool(dpp);
     
     return(ret);
};



static int
_getDpListSize( IBP_depot *dp) 
{

    int i = 0;

    if ( dp == NULL ) return (0);

    while ( dp[i] != NULL ){
        i++;
    };

    return i;
};

static int  
_appendDpList( IBP_depot *dp, IBP_depot *newDp, int ndepots)
{
    int n_dp,n_newDp,i,j,k;

    if ( dp == NULL || newDp == NULL )
          return(LORS_SUCCESS);

    n_dp = _getDpListSize(dp);
    n_newDp = _getDpListSize(newDp); 


    /*fprintf(stderr, "%d %d : ndepots: %d\n", n_dp, n_newDp, ndepots);*/

    j = 0;
    for ( i = n_dp ; i < ndepots ; i ++ )
    {
       while ( newDp[j] != NULL )
       {
          if ( _lorsFindDepot(dp,newDp[j]) >=0 ){
                j++;
                continue;
          }else {
                break;
          };
       };
       
       if ( newDp[j] == NULL )
            return LORS_SUCCESS; 
       /*fprintf(stderr, "appendDpList: %d\n", i);*/
       if ( (dp[i] = (IBP_depot)calloc(1,sizeof(struct ibp_depot))) == NULL )
       {
           lorsDebugPrint(D_LORS_ERR_MSG,"appendDpList: Out of memory\n");
           return ( LORS_NO_MEMORY);
       }; 
       memcpy(dp[i], newDp[j],sizeof(struct ibp_depot));
       dp[i+1] = NULL;
       j++;
    };

    return ( LORS_SUCCESS); 
};

static int
_lorsCreateDepot(LorsDepot **depot, int dpid , IBP_depot ibpdepot, double bandwidth, double proximity)
{
   LorsDepot *dp = NULL;

   if ((dp = (LorsDepot *)calloc(1,sizeof(LorsDepot))) == NULL ){
      lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateDepot: Out of memory\n");
      return (LORS_NO_MEMORY);
   };
 
   if ( (dp->lock = (pthread_mutex_t *)calloc(1,sizeof(pthread_mutex_t))) == NULL ){
      lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateDepot: Out of memory\n");
      free(dp);
      return(LORS_NO_MEMORY);
   };
   if ( (dp->depot = (IBP_depot)calloc(1,sizeof(struct ibp_depot))) == NULL ){
      lorsDebugPrint(D_LORS_ERR_MSG,"_lorsCreateDepot: Out of memory\n");
      free(dp->lock);
      free(dp);
      return(LORS_NO_MEMORY);
   };
   pthread_mutex_init(dp->lock,NULL);
   memcpy(dp->depot ,ibpdepot,sizeof(struct ibp_depot));  
   lorsTrim(dp->depot->host);
   dp->id = dpid;
   dp->bandwidth = bandwidth;
   dp->proximity = proximity; 
   dp->nthread = 0;
   dp->nthread_done = 0;
   dp->nfailure = 0;

   *depot = dp;
   return (LORS_SUCCESS);
};


int 
lorsFreeDepotPool ( LorsDepotPool *dp ) 
{
    if ( dp != NULL ){
         if ( dp->list != NULL ){
              _lorsFreeDepotJRBTree(dp->list);
              dp->list = NULL;
         };
         if ( dp->lock != NULL ){
             free(dp->lock);
             dp->lock = NULL;
         };
         if ( dp->lboneserver != NULL) {
             free(dp->lboneserver); 
             dp->lboneserver = NULL;
         };
         if ( dp->location != NULL ) {
             free(dp->location);
             dp->location = NULL ;
         };
         free(dp);
    };
 
    return (LORS_SUCCESS);
};

static void
_lorsUpdateDepotPool ( LorsDepot *dp)
{
    JRB tmp;
    LorsDepot *tmp_dp = NULL ;
    LorsDepotPool *dpp = NULL ;
   
    assert(dp != NULL);
    assert(dp->dpp != NULL);
    dpp = dp->dpp;

    jrb_traverse(tmp,dpp->list){
       tmp_dp = (LorsDepot*)(jrb_val(tmp)).v;
       if (tmp_dp->id == dp->id ){
           assert((void *)tmp_dp == (void *)dp);
           jrb_delete_node(tmp);
           _lorsScoreDepot(dp);
           jrb_insert_dbl(dpp->list,dp->score,new_jval_v(dp)); 
           return;           
       };
    };

    /* should not reach here */
    assert(0);

};


int  lorsGetDepot ( LorsDepotPool *dpp , LorsDepot **depot)
{
    LorsDepot *dp = NULL ;
    int ret;
    int tmp;

    if ( (dpp == NULL) || ( depot == NULL ) ){
         lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepot: parameter is  NULL ! \n");
         return (LORS_INVALID_PARAMETERS);
    }; 
    
    tmp =  pthread_mutex_lock(dpp->lock);
    assert(tmp == 0);

    dp = (LorsDepot *)(jrb_val(jrb_first(dpp->list)).v);
    assert(dp != NULL);

    /*printDepot(dp, pthread_self());*/

    tmp =  pthread_mutex_lock(dp->lock);
    assert(tmp == 0);

    dp->nthread ++ ;

    _lorsUpdateDepotPool(dp);

    pthread_mutex_unlock(dp->lock);
    pthread_mutex_unlock(dpp->lock);

    *depot = dp;

    return(LORS_SUCCESS);
};

int 
lorsCheckoutDepot ( LorsDepot *dp)
{
    int tmp;
 
    if ( dp == NULL )
         return ( LORS_INVALID_PARAMETERS);
    if ( dp->dpp == NULL )
         return ( LORS_SUCCESS);
    
    tmp = pthread_mutex_lock(dp->dpp->lock);
    assert (tmp == 0);
    tmp = pthread_mutex_lock(dp->lock);
    assert (tmp == 0);

    dp->nthread ++;
    
    _lorsUpdateDepotPool(dp);

    pthread_mutex_unlock(dp->lock);
    pthread_mutex_unlock(dp->dpp->lock);

    return ( LORS_SUCCESS );
    
};

void __releaseDepotCleanup(void *v)
{
    LorsDepot   *dp;
    dp = (LorsDepot *)v;

    pthread_mutex_unlock(dp->lock);
    pthread_mutex_unlock(dp->dpp->lock);

    return;
}
int  
lorsReleaseDepot ( LorsDepot *dp , double bandwidth, int nfailures )
{
    LorsDepotPool *dpp = NULL;
    int           tmp;


    if ( dp == NULL ){
       lorsDebugPrint(D_LORS_ERR_MSG,"lorsReleaseDepot: Invalid parameter!\n");
       return(LORS_INVALID_PARAMETERS);
    };

    if ( dp->dpp != NULL ){
         tmp = pthread_mutex_lock(dp->dpp->lock);
         tmp = pthread_mutex_lock(dp->lock);

         /*pthread_cleanup_push(__releaseDepotCleanup, (void *)&dp);*/

         dp->nfailure += nfailures;
         dp->nthread--;
         dp->nthread_done++;
         _lorsUpdateDepotPool(dp);

         /*pthread_cleanup_pop(0);*/

         pthread_mutex_unlock(dp->lock);
         pthread_mutex_unlock(dp->dpp->lock);
    };

    return(LORS_SUCCESS);
};

LorsDepot *
lorsGetDepotByName ( LorsDepotPool    *dpp,
               IBP_depot        depot ){
      LorsDepot  *lors_depot,*tmp;

      JRB         node;
      lors_depot = NULL;

      pthread_mutex_lock(dpp->lock);

      jrb_traverse(node,dpp->list)
      {
           tmp = (LorsDepot *)jval_v(node->val);
           if ( (strcasecmp(tmp->depot->host,depot->host) == 0) &&
                (tmp->depot->port == depot->port) )
           {
                lors_depot = tmp;
                break;
           };
      };

      pthread_mutex_unlock(dpp->lock);
      
      return ( lors_depot);
      
};

/* ret_dpp was added to enable the proper memory release after it has been
 * used by download. */
int lorsUpdateDepotPool(LorsExnode *exnode,
                        LorsDepotPool   **ret_dpp,
                        char       *lbone_server,
                        int         lbone_server_port,
                        char       *location,
                        int         nthreads,
                        int         timeout,
                        int         opts)
{
    JRB node = NULL;
    LorsMapping *lm = NULL;
    int         cnt = 0;
    Dllist      depot_list = NULL, dlnode = NULL;
    IBP_depot   *dp_list = NULL;
    int             i = 0, ret = 0;
    LorsDepotPool   *dpp = NULL;

    depot_list = new_dllist();
    *ret_dpp = NULL;

    cnt = 0;
    jrb_traverse(node, exnode->mapping_map)
    {
        lm = (LorsMapping *)node->val.v;
        dll_append(depot_list, new_jval_v(&lm->depot));
        cnt++;
    }
    dp_list = (IBP_depot *) calloc((cnt+1),sizeof(IBP_depot));
    if ( dp_list == NULL ) return LORS_NO_MEMORY;

    i=0;
    dp_list[i] = NULL;
    dll_traverse(dlnode, depot_list)
    {
        if ( _lorsFindDepot(dp_list,(IBP_depot)dlnode->val.v) >= 0 ){
              continue;
        };
        dp_list[i] = (IBP_depot)dlnode->val.v;
        i++;
        dp_list[i] = NULL;
    }

    ret = lorsGetDepotPool(&dpp, lbone_server, lbone_server_port,
                    dp_list, cnt, location, IBP_SOFT, 1, 1, nthreads,
                    timeout, 0); /*opts|SORT_BY_PROXIMITY);*/
    if ( ret != LORS_SUCCESS)
    {
        dll_empty(depot_list);
        free(dp_list);
        return ret;
    }

    jrb_traverse(node, exnode->mapping_map)
    {
        lm = (LorsMapping *)node->val.v;
        lm->dp = dpp;
    }

    free_dllist(depot_list);
    free(dp_list);
    *ret_dpp = dpp;
    return LORS_SUCCESS;
}
static void
printDepot(LorsDepot *dp, int no)
{

    fprintf(stdout,"%d\t%d\t%s\t%d\t%f\t%f\t%f\t%d\t%d\t%d\n",
                   no,dp->id,dp->depot->host,dp->depot->port,
                   dp->score,dp->bandwidth,dp->proximity,
                   dp->nfailure,dp->nthread,dp->nthread_done );

};

void
printDepotPool(LorsDepotPool *dpp)
{
    JRB       tmp;
    int i;
    LorsDepot *dp;
    char str[81];

    for ( i = 0 ; i < 80 ; i ++ ) str[i]= '=';
    str[80] = '\0';
    fprintf(stdout,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n","No","ID","Depot","Port","Score","Bandwidth","Prox","NFailure","NThread", "NThreadDone");
    fprintf(stdout,"%s\n",str);
    if ( dpp == NULL )
       return;

    if (dpp->list == NULL )
       return;

    i = 0;
    jrb_traverse(tmp,dpp->list) {
        dp = (LorsDepot *)(jrb_val(tmp).v);
        assert(dp != NULL);
        printDepot(dp,i);
        i++;
    };
};


/* examine 'list' and remove any duplicate hostnames.  This prevents the
 * treatment of different host/port combinations holding multiple copies in an
 * exnode.*/
Depot *_unique_depots(Depot *list)
{
    Depot   *d;
    int     count = 0, len = 0;
    JRB     unique_list, node;
    int         i = 0;

    if ( list == NULL ) return NULL;
    unique_list = make_jrb();
    len = _getDpListSize(list);

    /*fprintf(stderr, "Unique: len: %d\n", len);*/
    for(i=0; i < len; i++)
    {

        /*fprintf(stderr, "list[i]->host: %s\n", list[i]->host);*/
        node = jrb_find_str(unique_list, list[i]->host);
        if (node == NULL)
        {
            jrb_insert_str(unique_list, list[i]->host, new_jval_v(list[i]));
            count++;
            /*fprintf(stderr, "adding : %d\n", count);*/
        }
    }
    d = (Depot *)calloc(count+1, sizeof(Depot));
    i=0;
    jrb_traverse(node, unique_list)
    {
        d[i] = node->val.v;
        i++;
    }
    d[i] = NULL;
    return d;
}

int 
lorsGetDepotPool ( LorsDepotPool  **depotpool,
                   char           *lbone_server,
                   int            lbone_server_port,
                   IBP_depot      *depots,
                   int            min_unique_depots,
                   char           *location,
                   ulong_t        storage_size,
                   int            storage_type,
                   time_t         duration,
                   int            nthreads,
                   int            timeout,
                   int            opts) 
{
    LBONE_request    lbone_req;
    LorsDepotPool    *depot_pool = NULL;
    int              ret_val = 0,i = 0;
    int              ndepots = 0, ndepots_file = 0,ndepots_svr = 0;
    int              min_depots = 0, count = 2, last_count = 0;
    IBP_depot        *rawdp = NULL,
                     *availdp_file = NULL,
                     *rawdp_lbn_svr = NULL,
                     *availdp_lbn_svr = NULL,
                     *tmp_dp = NULL;
    double           *proximity = NULL;
    double           *bandwidth = NULL;
    LorsDepot        *dp = NULL; 
    static int       dpid = 0;
    time_t           begin_time = 0, left_t = 0;
    time_t          lb_timeout = 0;
    int              rawdp_length = 0;

/*
    lorsDebugPrint(D_LORS_VERBOSE, "server:%s:%d\n"
                                   "max: %d\n"
                                   "location: %s\n"
                                   "size: %ld\n"
                                   "type: %d\n"
                                   "duration: %d\n"
                                   "nthreads: %d\n"
                                   "timeout: %d\n",
                    lbone_server, lbone_server_port, min_unique_depots, location, storage_size, storage_type, duration, nthreads, timeout);
*/
    /*
     * create a new depot pool
     */
    begin_time = time(0);
    if ( min_unique_depots == 0 ) 
    {
        return LORS_INVALID_PARAMETERS;
    }

    lorsDebugPrint(D_LORS_VERBOSE, "createDepotPool();\n");
    if ( (ret_val = _createDepotPool(&depot_pool,
                                    depots,
                                    lbone_server,
                                    lbone_server_port,
                                    min_unique_depots,
                                    location,
                                    storage_size,
                                    storage_type,
                                    duration,
                                    timeout)) != LORS_SUCCESS ) 
             return ( ret_val);

    rawdp_length = min_unique_depots+1;

    /*fprintf(stderr, "rawdp_length: %d\n", rawdp_length);*/
    lorsDebugPrint(D_LORS_VERBOSE, "min_unique_depots: %d\n", min_unique_depots);
    if ( (rawdp = (IBP_depot *)calloc(min_unique_depots+1,sizeof(IBP_depot))) == NULL ){
        lorsDebugPrint( D_LORS_ERR_MSG,"lorsGetDepotPool: Out of memory \n");
        ret_val = LORS_NO_MEMORY;
        goto bail; 
    };
    
    ndepots = 0;
    ndepots_file = 0;
 
    /*
     * read depots from depot list if appliable 
     */
    if ( depots != NULL ) {
        for ( ndepots_file = 0 ; 
               ( depots[ndepots_file] != NULL && ndepots_file < min_unique_depots ) ; 
               ndepots_file ++ ){
              if ( _lorsFindDepot(rawdp,depots[ndepots_file]) >= 0 ){
                     continue;
              };
              if ( ( rawdp[ndepots_file] = (IBP_depot)calloc(1,sizeof (struct ibp_depot))) == NULL ){
                   lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: Out of memory \n");
                   ret_val = LORS_NO_MEMORY;
                   goto bail;
              };
              memcpy(rawdp[ndepots_file],depots[ndepots_file],sizeof(struct ibp_depot));
              lorsTrim(rawdp[ndepots_file]->host);
        };  
    }; 
  
    /*
     * create Lbone_request 
     */
    memset(&lbone_req,0,sizeof(LBONE_request));
    lbone_req.location = location;
    lbone_req.numDepots = min_unique_depots * count;
    switch ( storage_type ) {
    case IBP_HARD:
         lbone_req.stableSize = storage_size;
         lbone_req.volatileSize = 0;
         break;
    case IBP_SOFT:
         lbone_req.stableSize = 0;
         lbone_req.volatileSize = storage_size;
         break;
    }; 
    lbone_req.duration = duration/(24.0*60.0*60.0);
    
    /*
     *  check IBP depots if applicable
     */
    
    if ( (depots != NULL ) &&  HAS_OPTION(opts,LORS_CHECKDEPOTS) ){
        lorsDebugPrint( D_LORS_VERBOSE,"lorsGetDepotPool: Call lbone_checkDepots for depots from list\n");
        left_t = _lorsRemainTime(begin_time,timeout);
        if (left_t < 0 ){
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: time out\n");
            ret_val = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
        lorsDebugPrint(D_LORS_VERBOSE, "lbone_checkDepots();\n");
        if ( g_lors_demo )
        {
            lorsDemoPrint(stderr, "MESSAGE 2 Checking Available Depots\n");
            fflush(stderr);
        }
        /* TODO: if no depot is present, the wait can be very long.
         * How to pass a reasonable timeout without waiting too long? */
        availdp_file = lbone_checkDepots(rawdp,lbone_req,(left_t > 10 ? 10: left_t));
        if ( availdp_file == NULL ){
             lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: No available depots can be found from list\n"); 
        };
        lorsDebugPrint(D_LORS_VERBOSE, "lbone_checkDepots  RETURNED\n");
        ndepots_file = _getDpListSize(availdp_file);
    };

    /*
     * get depots from lbone_server
     */
    /* help guarantee that there are at least half as many depots as requested. 
       min_depots = (min_unique_depots/2 == 0 ? 1 : min_unique_depots/2);*/
    /* guarantee there are as many depots returned as requested */
    min_depots = min_unique_depots;
    ndepots = ndepots_file;
    while ( ((lbone_server != NULL)
          && (depots != NULL) 
          && (ndepots_file < min_unique_depots) 
          && HAS_OPTION(opts,LORS_SERVER_AND_LIST)) || 
         ( (depots == NULL) && (ndepots_svr < min_depots) ) )
    {
         if ( ndepots == 0 )
              lbone_req.numDepots = min_unique_depots * count;
         else
              lbone_req.numDepots = ALL_DEPOTS;

         left_t = _lorsRemainTime(begin_time,timeout);
         if (left_t < 0 ){
              lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: time out\n");
              ret_val = LORS_TIMEOUT_EXPIRED;
              goto bail;
         };
         lorsDebugPrint(D_LORS_VERBOSE, "lbone_getDepots();\n");
         if ( g_lors_demo )
         {
            lorsDemoPrint(stderr, "MESSAGE 2 Contacting L-Bone\n");
            fflush(stderr);
         }
         lb_timeout = (left_t > 10 ? 10 : left_t );
/*
         fprintf(stderr, "getDepots:lbone_req %s %d %d %d %f\n",
                    lbone_req.location, lbone_req.numDepots,
                    lbone_req.stableSize, lbone_req.volatileSize,
                    lbone_req.duration);
*/
         if ((rawdp_lbn_svr = lbone_getDepots(depot_pool->lboneserver,lbone_req,lb_timeout)) == NULL ){
             lorsDebugPrint(D_LORS_VERBOSE,"losrGetDepotPool: lbone_getDepots Error: %d\n",LBONE_errno);
             ret_val = LORS_LBONE_FAILED;
             goto bail;             
         };

         tmp_dp = rawdp_lbn_svr;

        left_t = _lorsRemainTime(begin_time,timeout);
        if (left_t < 0 ){
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: time out\n");
            ret_val = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
         if ( HAS_OPTION(opts,LORS_CHECKDEPOTS))
         {
             lorsDebugPrint(D_LORS_VERBOSE, "lbone_checkDepots();\n");
             if ( g_lors_demo )
             {
                lorsDemoPrint(stderr, "MESSAGE 2 Checking Available Depots\n");
                fflush(stderr);
             }
             if ( g_db_level & D_LORS_VERBOSE )
             {
                 for (i=0; rawdp_lbn_svr[i] != NULL ; i++)
                 {
                     fprintf(stderr, "%s:%d\n", 
                             rawdp_lbn_svr[i]->host, 
                             rawdp_lbn_svr[i]->port);
                 }
             }
             if ( (availdp_lbn_svr = lbone_checkDepots(rawdp_lbn_svr,lbone_req,
                                        (left_t > 10 ? 10 : left_t))) == NULL ){
                  lorsDebugPrint(D_LORS_VERBOSE,"lorsGetDepotPool: lbone_checkDepots "
                          "Error: %d\n",LBONE_errno);
                  ret_val = LBONE_NO_DEPOTS;
                  min_unique_depots *= 2;
                  continue;
             }; 
             tmp_dp = availdp_lbn_svr;
         };

         tmp_dp = _unique_depots(tmp_dp);


         /* compare last count to this count.  when they are the same, the
          * user has requested more depots than the lbone has, and we should
          * stop with the ones we have. */
         last_count = ndepots_svr;
         ndepots_svr = _getDpListSize(tmp_dp); 
         if ( count > 1 && ndepots_svr == last_count ) break;
         count++;
    };

    /*
     * merge two depot list if possible
     */
    if ( ndepots_file == 0 && ndepots_svr == 0 ){
        lorsDebugPrint(D_LORS_ERR_MSG,"losrGetDepotPool: No available depots are found!\n");
        ret_val = LORS_NO_DEPOTS_FOUND;
        goto bail;
    };
    if ( ndepots_file == 0 )
        rawdp[0] = NULL;
    if ( availdp_file != NULL ) {
         assert(ndepots_file > 0 && ndepots_file <= min_unique_depots);
         for ( i = 0; availdp_file[i] != NULL ; i ++)
         {
             if ( i > rawdp_length ) 
             {
                 fprintf(stderr, "RAWDP_LENGTH IS TOO SHORT!!! %d\n", i);
             }
             memcpy(rawdp[i],availdp_file[i],sizeof(struct ibp_depot));
         }
         /* 
          * free unused depots
          */ 
         while ( rawdp[i] != NULL )
         {
             if ( i > rawdp_length ) 
             {
                 fprintf(stderr, "RAWDP_LENGTH IS TOO SHORT!!! %d\n", i);
             }
              free(rawdp[i]);
              rawdp[i] = NULL;
              i++;
         };
    };


    if ( (ret_val = _appendDpList(rawdp, tmp_dp, rawdp_length-1) ) != LORS_SUCCESS )
         goto bail;


    /*
     * get proximity and bandwidth from lbone_server
     */
    ndepots = _getDpListSize(rawdp);
    if ( HAS_OPTION(SORT_BY_PROXIMITY,opts) ){
        left_t = _lorsRemainTime(begin_time, timeout);
        if (left_t < 0  ){
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: time out\n");
            ret_val = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
         if ( (ret_val = lbone_getProximity1(depot_pool->lboneserver,lbone_req.location,rawdp,&proximity,left_t)) != 0 ){
             lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: lbone_getProximity1 error : %d\n",ret_val);
             ret_val = LORS_LBONE_FAILED;
             goto bail;
         };
         for ( i =0 ; i < ndepots ; i ++ ){
            if ( isnan(proximity[i]) != 0 ) proximity[i] = 0.0;
         };   
    }else {
        if ((proximity = (double *)calloc(ndepots,sizeof(double))) == NULL ){
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: Out of memory!\n");
            ret_val = LORS_NO_MEMORY;
            goto bail;
        };
        for ( i = 0 ; i < ndepots ;  i ++ )
            proximity[i] = 0.0;
    };

    /* WARNING: lbone_getBandwidth() is no longer part of the LBONE client api
     * lbone_getDepot2DepotResolution() instead.  */
#if 0
   if ( HAS_OPTION(SORT_BY_BANDWIDTH,opts)){
#endif
    if ( 0 ){
        left_t = _lorsRemainTime(begin_time,timeout);
        if (left_t < 0  ){
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: time out\n");
            ret_val = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
#if 0
        /* WARNING: lbone_getBandwidth is no longer part of the LBONE client api */
         if ( (ret_val = lbone_getBandwidth(rawdp,&bandwidth,left_time)) != 0 ){
             lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: lbone_getBandwidth error : %d\n",ret_val);
             ret_val = LORS_LBONE_FAILED;
             goto bail;
         };
         for ( i =0 ; i < ndepots ; i ++ ){
            if ( isnan(bandwidth[i]) != 0 ) proximity[i] = 0.0;
         };   
#endif
    }else {
        if ((bandwidth = (double *)calloc(ndepots,sizeof(double))) == NULL ){
            lorsDebugPrint(D_LORS_ERR_MSG,"lorsGetDepotPool: Out of memory!\n");
            ret_val = LORS_NO_MEMORY;
            goto bail;
        };
        for ( i = 0 ; i < ndepots ;  i ++ )
            bandwidth[i] = 0.0;
   };

   /*
    * fill depots to depot_pool
    */

   for ( i = 0  ; rawdp[i] != NULL ; i++ ){
       assert(rawdp[i] != NULL );
       if ( (ret_val = _lorsCreateDepot(&dp,dpid++,rawdp[i],bandwidth[i],proximity[i])) != LORS_SUCCESS)
             goto bail;
       _lorsScoreDepot(dp);
       dp->dpp = depot_pool;
       jrb_insert_dbl(depot_pool->list,dp->score,new_jval_v(dp));        
       /* updated by YING */
      lorsAddDepot(depot_pool->dl,dp); 
   };

   if ( g_db_level & D_LORS_VERBOSE )
   {
       printDepotPool(depot_pool);
   }

   /* 
    * free all allocated memory
    */
   if ( bandwidth != NULL ) free ( bandwidth);
   if ( proximity != NULL ) free ( proximity);
   lorsFreeDpList(rawdp);
   lorsFreeDpList(availdp_file);
   lorsFreeDpList(rawdp_lbn_svr);
   lorsFreeDpList(availdp_lbn_svr);

   *depotpool = depot_pool;
   return (LORS_SUCCESS);


bail:
   if ( bandwidth != NULL ) free ( bandwidth);
   if ( proximity != NULL ) free ( proximity);
   lorsFreeDpList(rawdp);
   lorsFreeDpList(availdp_file);
   lorsFreeDpList(rawdp_lbn_svr);
   lorsFreeDpList(availdp_lbn_svr);
   lorsFreeDepotPool ( depot_pool);

   return (ret_val);
}; 
