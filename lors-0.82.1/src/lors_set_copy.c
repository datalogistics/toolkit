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

#ifdef _MINGW
#define DM_MCAST 61
#endif

extern ulong_t  g_lm_id ;
int g_copy_thread_count;

pthread_mutex_t copy_thread_lock;

#define lorsDemoPrint fprintf

#define lors_port 5673
int tmpport=lors_port;

_lorsRemoveDepots(JRB jrb_tree,IBP_depot *depots,int ndepots)
{
   JRB node;
   int i;
   LorsDepot *dp;
   int off =0;
   
   for(node= jrb_first(jrb_tree);node != jrb_nil(jrb_tree);node=jrb_next(node))
   {
       dp=(LorsDepot *)jval_v(node->val);
       for(i=0;i<ndepots;i++)
       {
           if((strcasecmp(depots[i]->host,dp->depot->host) == 0) &&
               (depots[i]->port == dp->depot->port))
           {
               off = 1;
               free(dp->depot);
               dp->depot = NULL;
               break;
           };
       };
       if(off == 1)
       {
           jrb_delete_node(node);
           node=jrb_prev(node);
       }
    };
    return;

};

static int 
_lorsCreateBlockArray( longlong **block_array,
                       longlong **block_array_e2e,
                       int *array_len,
                       longlong length,
                       ulong_t  data_blocksize, 
                       __LorsDLList dl_list ) {
      int          has_e2e;
      LorsMapping  *lm;
      __LorsDLNode *dl_node;
      Dllist       node,node1;
      longlong     cur_logical_len, cur_offset, cur_len,cur_len_e2e;
      int          n_mappings = 0 , i;
 
      assert( ( block_array != NULL ) && ( array_len != NULL ) && 
              ( length > 0 ) && ( data_blocksize > 0 ) && ( dl_list != NULL ));

      dl_node = (__LorsDLNode *)jval_v(dll_first(dl_list->list)->val); 
      lm = (LorsMapping *)jval_v(dll_first(dl_node->mappings)->val);
      if ( _lorsHasE2E(lm) ) {
          has_e2e = 1;
      } else {
          has_e2e = 0;
      }; 

    if ( !(has_e2e ) ){
         *block_array = _lorsNewBlockArray(length,data_blocksize,array_len);
         *block_array_e2e = NULL;
         if ( (*block_array) == NULL )
              return LORS_NO_MEMORY;
         else 
              return LORS_SUCCESS; 
    } ;

    /* handle end to end service mappings */
    dll_traverse(node, dl_list->list){
         n_mappings ++;
    }; 
    assert(n_mappings > 0 );

    if ( ((*block_array) = (longlong *)calloc(n_mappings,sizeof(longlong))) == NULL){
        return ( LORS_NO_MEMORY);
    };
    if ( ((*block_array_e2e) = (longlong *)calloc(n_mappings,sizeof(longlong))) == NULL){
        free(*block_array);
        return ( LORS_NO_MEMORY);
    };    
    *array_len = n_mappings;
    cur_offset = -1;
    i = 0;
    dll_traverse(node,dl_list->list){
        dl_node = (__LorsDLNode *) jval_v(node->val);
        cur_len = -1;
        cur_len_e2e = -1;
        cur_logical_len = -1;
        dll_traverse(node1,dl_node->mappings){
            lm = (LorsMapping *)jval_v(node1->val);
            if ( cur_offset == -1 ) {
                 cur_offset = lm->exnode_offset;
            } else {
                 assert(cur_offset == lm->exnode_offset );
            };
            if ( cur_logical_len == -1 ){ 
                cur_logical_len = lm->logical_length;
            }else { 
                assert( cur_logical_len == lm->logical_length);
            };    
            if ( cur_len == -1 ){
                cur_len_e2e = lm->alloc_length;
                cur_len = lm->logical_length;
            }else {
                assert(cur_len_e2e == lm->alloc_length);
                assert(cur_len == lm->logical_length);
            };    

       };
       cur_offset = cur_offset + cur_logical_len ;
       (*block_array_e2e)[i] = cur_len_e2e;
       (*block_array)[i] = cur_len;
         
       i++;
     };

     assert( i == n_mappings );

     return ( LORS_SUCCESS );

};

static int  _lorsSetAllocateByBlockArray(LorsSet * se,
                         LorsDepotPool * dp,
                         longlong  offset,
                         longlong  *block_array,
                         longlong  *block_array_e2e,
                         int list_len,
                         __LorsDLList dl_list,
                         int copies,
                         int nthreads,
                         int timeout,
                         int opts)
{
    /*pthread_t       prog_tid;*/
    /*pthread_attr_t  prog_attr;*/
    /*LorsTimeout     *lt;*/
    LorsMapping    *lm = NULL,**lm_list = NULL;
    LorsSet        *set = NULL;
    LorsBoundary   *lb = NULL;
    double          tbegin, tdiff;
    longlong       *list = NULL;
    longlong        block_offset;
    int             i,j,k,tmp;
    int             blocks;
    Dllist          dl_unjoined = NULL, dl_node = NULL, dl_map = NULL;
    JRB             jrb_node = NULL;
    LorsDepot       *depot = NULL;
    __LorsDLNode    *dll_node = NULL ; 
    __LorsThreadPool   *tp = NULL;
    __LorsJobPool      *jp = NULL;
    __LorsJob          *job = NULL;
    
    int                ret_val;

    assert( (block_array != NULL) && ( se != NULL) && 
            (dp != NULL ) &&  ( copies > 0 )  );
                
    if ( (ret_val = _lorsCreateJobPool(&jp)) != LORS_SUCCESS ){
        lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCreate: can't create job pool!\n");
        goto bail; 
    }; 
    jp->type = J_CREATE;
    jp->src_offset = 0;
    jp->src_length = 0;
    
#if 0
    set = _lorsMallocSet();
    if ( set == NULL ){
        ret_val = LORS_NO_MEMORY;
        lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetCreate:Out of memory!\n");
        goto bail;
    }; 
#endif

    list = block_array;

    lm_list = (LorsMapping **)calloc(list_len*copies,sizeof(LorsMapping *));
    if ( lm_list == NULL ){
       lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetCreate: Out of memroy!\n");
       ret_val = LORS_NO_MEMORY;
       goto bail;
    };
    
    tbegin = time(0);

    /* count the number of good mappings in the DLList for each DLNode */
    /* if you use LORS_BALANCE, then you need to run lorsSetStat before
     * calling lorsSetCopy() */
    if ( HAS_OPTION(LORS_BALANCE, opts) )
    {
        dll_traverse(dl_node, dl_list->list)
        {
            dll_node = (__LorsDLNode *)jval_v(dl_node->val);
            dll_traverse(dl_map, dll_node->mappings)
            {
                lm = (LorsMapping *)dl_map->val.v;
                if ( lm->capstat.readRefCount > 0 )
                {
                    dll_node->good_copies++;
                }
            }
        }
        dll_traverse(dl_node, dl_list->list)
        {
            dll_node = (__LorsDLNode *)jval_v(dl_node->val);

            /*fprintf(stderr, "good copies: offset %lld copies: %d\n", 
                    dll_node->offset, dll_node->good_copies);*/
        }
    }


    fprintf(stderr, "min_unique_depots: %d\n", dp->min_unique_depots);
    lorsCreateListFromPool(dp, &dp->dl, dp->min_unique_depots/2);

    k = 0;
    for ( j=0; j < copies; j++)
    {
        block_offset = offset;
        dl_node = dll_first(dl_list->list);

        /*fprintf(stderr, "copies: %d\n", j);*/
        for ( i=0; i < list_len ; i++)
        {
            /*fprintf(stderr, "list_len i: %d\n", i);*/
            while ( dl_node != dl_list->list ){
                 dll_node = (__LorsDLNode *)jval_v(dl_node->val);


                 /*fprintf(stderr, "block_offset : %lld  dll_node->offset: %lld\n",
                         block_offset, dll_node->offset);*/
                 if ( ( block_offset >= dll_node->offset ) && 
                      ( block_offset < dll_node->offset + dll_node->length ) )
                 {
                      break;
                 }; 
                 dll_node = NULL;
                 dl_node = dll_next(dl_node);
            };
            assert(dll_node != NULL );


            /*fprintf(stderr, "dll_node->offset: good_copies: copies: %lld %d %d\n", 
                    dll_node->offset, dll_node->good_copies, copies);*/
            if ( !HAS_OPTION(LORS_BALANCE, opts) || 
                 ( HAS_OPTION(LORS_BALANCE, opts) && (dll_node->good_copies < copies) ) )
            {

                /*fprintf(stderr, "creating LM\n");*/

                lm = (LorsMapping *)calloc(1, sizeof(LorsMapping));
                if (lm == NULL){
                    ret_val = LORS_NO_MEMORY;
                    goto bail;
                };
                lm->begin_time = tbegin;
                lm->exnode_offset = block_offset;

                lm->logical_length = list[i];
                if ( block_array_e2e != NULL ){
                    lm->alloc_length = block_array_e2e[i];
                }else{
                    lm->alloc_length = list[i];
                };
                lm->alloc_offset = 0;
                lm->dp = dp;
                lm->timeout = timeout;
                lm->opts = opts;
                        
                lm->md = NULL;
                /*lm->sem = &sem;*/
                lm_list[k] = lm;
            
                k++;
                /*
                 * create a new job and put into job pool
                 */
                job = (__LorsJob *)calloc(1,sizeof(__LorsJob));
                if ( job == NULL ) {
                    ret_val = LORS_NO_MEMORY;
                    lorsDebugPrint(D_LORS_ERR_MSG,"lorsSetCreate:Out of memory!\n");
                    goto bail;
                };
                job->type = J_CREATE;
                job->status = JOB_INIT;
                job->src = lm;
                job->dst = (void *)dll_node;
              
                /* add to job pool */

                /*fprintf(stderr, "Add Job: of: %lld len: %d cop: %d\n",
                        lm->exnode_offset, lm->logical_length, dll_node->good_copies);*/
                _lorsAddJob(jp,job);

                if ( HAS_OPTION(LORS_BALANCE, opts) )
                {
                    dll_node->good_copies++;
                    /*fprintf(stderr, "Increase good copies: cop: %d\n",*/
                        /*dll_node->good_copies);*/
                }
            }
            block_offset = block_offset + list[i];
        }; 
        /*getchar();*/
    }



    /*
     * Create working threads
     */
    if( g_lors_demo )
    {
        lorsDemoPrint(stderr, "MESSAGE 2 Allocating..\n");
        fflush(stderr);
    }
    ret_val = _lorsCreateThreadPool(jp,nthreads,&tp,timeout,opts);
    if (ret_val != LORS_SUCCESS )
        goto bail;


   
    /* wait for all active threads, join, and free resources */
    ret_val = _lorsJoinThread(tp);
    /*for (i=0; i < tp->nthread; i ++ ){
        pthread_join(tp->threads[i].tid,NULL);
        pthread_attr_destroy(&(tp->threads[i].attr));
        if ( tp->threads[i].status != THD_SUCCESS )
            ret_val = tp->threads[i].err_code;
    }; */


    if ( ret_val != LORS_SUCCESS )
        goto bail;

    tmp = pthread_mutex_lock(&(jp->lock));
    assert(tmp == 0); 
    dll_traverse(dl_node,jp->job_list){
         job = (__LorsJob *)dl_node->val.v;
         assert(job->status == JOB_COMMIT ); 
         lm = (LorsMapping*)job->src;
        
         /* release other resources.. free(lm)? */
         /* for each mapping */
         lorsSetAddMapping(se, lm);
    }
    se->seek_offset = 0;

    _lorsFreeJobPool(jp);
    _lorsFreeThreadPool(tp);
    free(lm_list);
 
    return LORS_SUCCESS;

bail:

    _lorsFreeJobPool(jp);
    _lorsFreeThreadPool(tp);
    if ( lm_list != NULL ) {
        for ( i = 0; i < list_len*copies ; i ++ )
            if ( lm_list[i] != NULL )    free(lm_list[i]);
        free(lm_list);
    };
    if ( set != NULL ) {
        if ( set->mapping_map != NULL ) jrb_free_tree(set->mapping_map);
        /*if ( set->boundary_list != NULL ) free_dllist(set->boundary_list);*/
        free(set);
    };
    return (ret_val);  
}
static int
_lorsCreateMCopyNE( __LorsJobPool **jp,
                   longlong      *block_array,
                   int           array_len,
                   __LorsDLList  dl_list,
                   longlong      offset,
                   LorsSet       *src_set,
                   LorsSet       *dst_set,
                   __LorsDepotResolution *dp_reso ){

    __LorsJobPool *job_pool;
    int           ret = LORS_SUCCESS,i;
    __LorsDLList  new_dl_list;
    longlong      off, length;
    __LorsJob     *job;
    JRB           jrb_node,next_node , head_node;
    Dllist        dst_mappings;
    LorsMapping   *lm;

    if ((ret = _lorsCreateJobPool(&job_pool) != LORS_SUCCESS )){
        goto bail;
    };
    *jp = job_pool; 
    job_pool->type = J_MCOPY_NO_E2E;
    off = offset;
    length = 0;

    fprintf(stderr, "array_len: %d\n", array_len);
    for ( i = 0 ; i < array_len ; i ++ ) 
    {
        fprintf(stderr, "off: %lld len: %d\n", off, length);
        head_node = jrb_find_first_llong(dst_set->mapping_map,off);
        jrb_node = head_node;
        /*assert(jrb_node != NULL ); */
        if ( jrb_node != NULL )
        {
            if ( (job = (__LorsJob *)calloc(1,sizeof(__LorsJob))) == NULL ){
                ret = LORS_NO_MEMORY;
                goto bail;
            };
            job->type = J_MCOPY_NO_E2E;
            job->status = JOB_INIT;
            job->src_offset = off;
            job->src_length = block_array[i];
            job->dst_offset = off;
            job->dst_length = block_array[i];
            job->extra_data = (void *)dp_reso;  
            ret = _lorsCreateDLList(src_set,off,block_array[i],&new_dl_list);
            if ( ret != LORS_SUCCESS ) 
                goto bail;
            job->src = (void *)new_dl_list;
            _lorsAddJob(job_pool,job);

            dst_mappings = new_dllist();
            do {
                /*dll_append(dst_mappings,new_jval_v(jval_v(jrb_node->val)));*/
                dll_append(dst_mappings,jrb_node->val);
                lm = (LorsMapping *)jval_v(jrb_node->val);
                assert(lm->logical_length == block_array[i]);
                jrb_node =  jrb_next(jrb_node);
                /*fprintf(stderr, "appending dst_mappings\n");*/
                /*getchar();*/ /* SAS */
            } while ( ( jrb_node != jrb_nil(head_node)) && 
                      ( jval_ll(jrb_node->key) == off ) );
            job->dst = (void *)dst_mappings;
        }
        off = off + block_array[i]; 
        length = length + block_array[i];
     }
     job_pool->dst_offset = offset;
     job_pool->dst_length = length;
     job_pool->src_offset = offset;
     job_pool->src_length = length;

     return (LORS_SUCCESS);
bail:
    
    _lorsFreeJobPool(job_pool);
    return (ret); 

};

static int
_lorsCreateMCopyE2E( __LorsJobPool **jp,
                   longlong      *block_array,
                   int           array_len,
                   __LorsDLList  dl_list,
                   longlong      offset,
                   LorsSet       *src_set,
                   LorsSet       *dst_set,
                   __LorsDepotResolution *dp_reso ){

     __LorsJobPool *job_pool;
     int           ret = LORS_SUCCESS,i;
     Dllist        dll_node, sub_node;
     __LorsDLNode  *dl_node;
     longlong      off, length , new_off;
     __LorsJob     *job;
     JRB           jrb_node,next_node,head_node;
     Dllist        src_mappings ,dst_mappings;
     LorsMapping   *lm,*tmp_lm;

     if ((ret = _lorsCreateJobPool(&job_pool) != LORS_SUCCESS )){
         goto bail;
     };
     *jp = job_pool; 

     job_pool->type = J_MCOPY_E2E;
     off = offset;
     length = 0;
     dll_node = dll_first(dl_list->list); 
     for ( i = 0 ; i < array_len ; i ++ ) {

        head_node = jrb_find_first_llong(dst_set->mapping_map,off);
        jrb_node = head_node;
        /*assert(jrb_node != NULL ); */
        if ( jrb_node != NULL )
        {

            if ( (job = (__LorsJob *)calloc(1,sizeof(__LorsJob))) == NULL ){
                ret = LORS_NO_MEMORY;
                goto bail;
            };

            job->type = J_MCOPY_E2E;
            job->status = JOB_INIT;
            job->extra_data = (void *)dp_reso;  
            _lorsAddJob(job_pool,job);
            src_mappings = new_dllist();
            dst_mappings = new_dllist();
            job->src = (void *) src_mappings;
            job->dst = (void *) dst_mappings; 
            dl_node = (__LorsDLNode *)jval_v(dll_node->val);
            assert(dl_node != NULL);
            lm = (LorsMapping *)jval_v(dll_first(dl_node->mappings)->val);
            new_off = lm->exnode_offset;
            dll_traverse(sub_node,dl_node->mappings)
            {
                tmp_lm = (LorsMapping *)jval_v(sub_node->val);
                /*fprintf(stderr, "exnode_offset: %lld new_off: %lld\n"
                            "logical_length: %d block_array[i]: %lld\n", 
                            tmp_lm->exnode_offset, new_off,
                            tmp_lm->logical_length, block_array[i]);*/
                assert((tmp_lm->exnode_offset == new_off) &&
                       (tmp_lm->logical_length == block_array[i]));
                dll_append(src_mappings,new_jval_v(tmp_lm));
            };
        
            do {
                dll_append(dst_mappings,new_jval_v(jval_v(jrb_node->val)));
                tmp_lm = (LorsMapping *)jval_v(jrb_node->val);
                assert(tmp_lm->logical_length == block_array[i]);
                tmp_lm->exnode_offset = new_off;
                tmp_lm->logical_length = lm->logical_length; 
                jrb_node =  jrb_next(jrb_node);
            } while ( ( jrb_node != jrb_nil(head_node) ) && 
                      ( jval_ll(jrb_node->key) == off  ) );
            job->dst = (void *)dst_mappings;
            if ( off != new_off )
            {
                while ((jrb_node = jrb_find_llong(dst_set->mapping_map,off))!= NULL)
                {
                    tmp_lm = (LorsMapping *)jval_v(jrb_node->val);
                    assert(tmp_lm->exnode_offset == new_off);
                    jrb_delete_node(jrb_node);
                    jrb_insert_llong(dst_set->mapping_map,new_off,new_jval_v(tmp_lm)); 
                };
            };
            job->src_offset = lm->exnode_offset;
            job->src_length = lm->alloc_length;
            job->dst_offset = lm->exnode_offset;
            job->dst_length = lm->alloc_length;
        }
        off = off + block_array[i]; 
        length = length + block_array[i];
        dll_node = dll_next(dll_node);
     }
     job_pool->dst_offset = offset;
     job_pool->dst_length = length;
     job_pool->src_offset = offset;
     job_pool->src_length = length;

     return (LORS_SUCCESS);
bail:
    
    _lorsFreeJobPool(job_pool);
    return (ret); 

};

static int
_lorsCreateCopyNE( __LorsJobPool **jp,
                   longlong      *block_array,
                   int           array_len,
                   __LorsDLList  dl_list,
                   longlong      offset,
                   LorsSet       *src_set,
                   LorsSet       *dst_set,
                   __LorsDepotResolution *dp_reso ){

    __LorsJobPool *job_pool;
    int           ret = LORS_SUCCESS,i,k;
    __LorsDLList  new_dl_list;
    longlong      off, length;
    __LorsJob     *job , ***job_array;
    JRB           jrb_node,next_node,head_node;
    LorsMapping   *dst_mapping;
    LorsMapping   *lm;

    if ((ret = _lorsCreateJobPool(&job_pool) != LORS_SUCCESS )){
         goto bail;
    };
    *jp = job_pool; 
    job_pool->type = J_COPY_NO_E2E;
    off = offset;
    length = 0;
    if ( (job_array = (__LorsJob ***)calloc(dst_set->copies,sizeof(__LorsJob **))) == NULL ) {
         ret = LORS_NO_MEMORY;
         goto bail;
    };

    for ( i = 0 ; i < dst_set->copies ; i ++ ){
        if ( ( job_array[i] = (__LorsJob **)calloc(array_len,sizeof(__LorsJob *))) == NULL ){
             ret = LORS_NO_MEMORY;
             goto bail;
         };
     };

     for ( i = 0 ; i < array_len ; i ++ ) 
     {
        head_node = jrb_find_first_llong(dst_set->mapping_map,off);
        jrb_node = head_node;
        /* SAS: removed assertion to allow for --balance functionality, where
         * some parts of the dst_set may be empty */

        /* assert(jrb_node != NULL );*/
        k = 0;
        if ( jrb_node != NULL )
        {
            do {
                if ( (job = (__LorsJob *)calloc(1,sizeof(__LorsJob))) == NULL ){
                    ret = LORS_NO_MEMORY;
                    goto bail;
                };
                job_array[k][i] = job;
                job->type = J_COPY_NO_E2E;
                job->status = JOB_INIT;
                job->src_offset = off;
                job->src_length = block_array[i];
                job->dst_offset = off;
                job->dst_length = block_array[i];
                job->extra_data = (void *)dp_reso;  
                dst_mapping = (LorsMapping *)jval_v(jrb_node->val);
                ret = _lorsCreateDLList(src_set,off,block_array[i],&new_dl_list);
                if ( ret != LORS_SUCCESS ) goto bail;
                job->src = (void *)new_dl_list;
                assert(dst_mapping->logical_length == block_array[i]);
                job->dst = (void *)dst_mapping;
                jrb_node =  jrb_next(jrb_node);
                k++;
            } while ( ( jrb_node != jrb_nil(head_node)) && 
                      ( jval_ll(jrb_node->key) == off) );
        }
        off = off + block_array[i]; 
        length = length + block_array[i];
    }

    for ( k  = 0 ; k < dst_set->copies ; k ++ ){
        for ( i = 0; i < array_len; i ++ ){
            if ( job_array[k][i] != NULL )
            {
                _lorsAddJob(job_pool,job_array[k][i]);
            }
            job_array[k][i] = NULL;
        };
    };

    job_pool->dst_offset = offset;
    job_pool->dst_length = length;
    job_pool->src_offset = offset;
    job_pool->src_length = length;
    
    if ( job_array != NULL ){
        for ( k = 0 ; k < dst_set->copies; k ++ ){
            if ( job_array[k] != NULL ) {
                free(job_array[k]);
                job_array[k] = NULL;
            };
        };
        free(job_array);
        job_array = NULL;
    };
    
    return (LORS_SUCCESS);
bail:
    
    if ( job_array != NULL ){
         for ( k = 0 ; k < dst_set->copies; k ++ ){
             if ( job_array[k] != NULL ) {
                for ( i = 0 ; i < array_len ; i ++ ){
                    _lorsFreeJob(job_array[k][i]);
                };
                free(job_array[k]);
                job_array[k] = NULL;
             };
         };
         free(job_array);
         job_array = NULL;
    };

    _lorsFreeJobPool(job_pool);
    return (ret); 

};

static int
_lorsCreateCopyE2E( __LorsJobPool **jp,
                   longlong      *block_array,
                   int           array_len,
                   __LorsDLList  dl_list,
                   longlong      offset,
                   LorsSet       *src_set,
                   LorsSet       *dst_set,
                   __LorsDepotResolution *dp_reso ){

     __LorsJobPool *job_pool;
     int           ret = LORS_SUCCESS,i,k;
     Dllist        dll_node = NULL, node = NULL;
     __LorsDLNode  *dl_node = NULL;
     longlong      off, length , new_off;
     __LorsJob     *job = NULL,***job_array = NULL;
     JRB           jrb_node = NULL,next_node = NULL,head_node = NULL;
     Dllist        src_mappings = NULL;
     LorsMapping   *lm = NULL,*tmp_lm = NULL,*dst_mapping = NULL;

     if ((ret = _lorsCreateJobPool(&job_pool) != LORS_SUCCESS )){
         goto bail;
     };

    if ( (job_array = (__LorsJob ***)calloc(dst_set->copies,sizeof(__LorsJob **))) == NULL ) {
         ret = LORS_NO_MEMORY;
         goto bail;
     };

     for ( i = 0 ; i < dst_set->copies ; i ++ ){
         if ( ( job_array[i] = (__LorsJob **)calloc(array_len,sizeof(__LorsJob *))) == NULL ){
             ret = LORS_NO_MEMORY;
             goto bail;
         };
     };

   
     *jp = job_pool; 
     job_pool->type = J_COPY_E2E;
     off = offset;
     length = 0;
     dll_node = dll_first(dl_list->list); 


     for ( i = 0 ; i < array_len ; i ++ ) 
     {
        head_node = jrb_find_first_llong(dst_set->mapping_map,off);
        jrb_node = head_node;
        k = 0;
        if ( jrb_node != NULL )
        {

            do 
            {
                if ( (job = (__LorsJob *)calloc(1,sizeof(__LorsJob))) == NULL ){
                    ret = LORS_NO_MEMORY;
                    goto bail;
                };
                job_array[k][i] = job; 
                job->type = J_COPY_E2E;
                job->status = JOB_INIT;
                job->extra_data = (void *)dp_reso;  
                src_mappings = new_dllist();
                job->src = (void *) src_mappings;


                dl_node = (__LorsDLNode *)jval_v(dll_node->val);

                assert(dl_node != NULL);
                lm = (LorsMapping *)jval_v(dll_first(dl_node->mappings)->val);
                new_off = lm->exnode_offset;
                dll_traverse(node,dl_node->mappings){
                    tmp_lm = (LorsMapping *)jval_v(node->val);
                    assert((tmp_lm->exnode_offset == new_off) &&
                        (tmp_lm->logical_length == block_array[i]));
                    dll_append(src_mappings,new_jval_v(tmp_lm));
                };
       
                dst_mapping = (LorsMapping*)jval_v(jrb_node->val); 
                job->dst = (void *)dst_mapping;
                assert(dst_mapping->logical_length == block_array[i]);
                dst_mapping->exnode_offset = new_off;
                dst_mapping->logical_length = lm->logical_length; 
                jrb_node =  jrb_next(jrb_node);
                k++;
                if ( g_lors_demo )
                {
#ifdef _MINGW
                   fprintf(stderr,"DRAW MappingAllocate %ld %I64d %ld %s:%4d\n",
                               dst_mapping->id,
                               dst_mapping->exnode_offset,
                               dst_mapping->logical_length,
                               dst_mapping->depot.host, dst_mapping->depot.port);
#else
                    fprintf(stderr,"DRAW MappingAllocate %ld %lld %ld %s:%4d\n",
                        dst_mapping->id,
                        dst_mapping->exnode_offset,
                        dst_mapping->logical_length,
                        dst_mapping->depot.host, dst_mapping->depot.port);
#endif
                   fflush(stderr);
                }

                job->src_offset = lm->exnode_offset;
                job->src_length = lm->alloc_length;
                job->dst_offset = lm->exnode_offset;
                job->dst_length = lm->alloc_length;
            }while ( ( jrb_node != jrb_nil(head_node)) && 
                     ( jval_ll(jrb_node->key) == off ));
        }
        if ( off != new_off ){
           while ((jrb_node = jrb_find_llong(dst_set->mapping_map,off))!= NULL){
               tmp_lm = (LorsMapping *)jval_v(jrb_node->val);
               assert(tmp_lm->exnode_offset == new_off);
               jrb_delete_node(jrb_node);
               jrb_insert_llong(dst_set->mapping_map,new_off,new_jval_v(tmp_lm)); 
           };
        };
        off = off + block_array[i]; 
        length = length + block_array[i];
        dll_node = dll_next(dll_node);
     }
     
     for ( k  = 0 ; k < dst_set->copies ; k ++ ){
         for ( i = 0; i < array_len; i ++ ){
             if ( job_array[k][i] != NULL )
             {
                _lorsAddJob(job_pool,job_array[k][i]);
             }
             job_array[k][i] = NULL;
         };
     };
     job_pool->dst_offset = offset;
     job_pool->dst_length = length;
     job_pool->src_offset = offset;
     job_pool->src_length = length;

     if ( job_array != NULL ){
         for ( k = 0 ; k < dst_set->copies; k ++ ){
             if ( job_array[k] != NULL ) {
                free(job_array[k]);
                job_array[k] = NULL;
             };
         };
         free(job_array);
         job_array = NULL;
     };

     return (LORS_SUCCESS);
bail:

     if ( job_array != NULL ){
         for ( k = 0 ; k < dst_set->copies; k ++ ){
             if ( job_array[k] != NULL ) {
                for ( i = 0 ; i < array_len ; i ++ ){
                    _lorsFreeJob(job_array[k][i]);
                };
                free(job_array[k]);
                job_array[k] = NULL;
             };
         };
         free(job_array);
         job_array = NULL;
     };

    
    _lorsFreeJobPool(job_pool);
    return (ret); 

};

int
_lorsDoMCopyNE ( __LorsJob *job,
                 __LorsThreadInfo *tinfo )
{
     JRB             lm_tree = NULL,jrb_node=NULL;
     LorsMapping     *lm = NULL , *dst_lm = NULL,*src_lm = NULL;
     LorsDepot       *depot=NULL;
     IBP_cap         *dst_caps=NULL,src_cap = NULL;
     int             *dm_type=NULL,*ports=NULL,*index=NULL;
     Dllist          dst_mappings=NULL;
     __LorsDepotResolution *dp_reso=NULL;
     __LorsDLList    dl_list=NULL;
     __LorsDLNode    *dl_node=NULL; 
     struct ibp_timer src_timer,*dst_timers=NULL;
     ulong_t        p_off_in_mapping, p_len,nwrite;
     int             n_dst = 0, j,k;
     int             ret,i,n_dst_mappings = 0;
     time_t          begin_time,left_t;
     time_t          begin_download;
     double          bandwidth,max_metric,metric, metric_s;
     __LorsJobStatus job_status;
     Dllist          dll_node, sub_node,tmp_node,tmplist;
     LboneResolution    *lr;
     Dllist         tlist,tlist1;
     int            found = 0;

         
     assert((job != NULL && tinfo != NULL));
     lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoMCopyNE[%d]: begin mcopy no e2e...\n",pthread_self());
   
     pthread_cleanup_push(_lorsDoCleanup,(void*)job);
     begin_time = time(0);

     dst_mappings = (Dllist)job->dst;
     dp_reso = (__LorsDepotResolution *)job->extra_data;
     lr = dp_reso->lr;
     dl_list = (__LorsDLList)job->src;
     n_dst_mappings = 0;
     dll_traverse(dll_node,dst_mappings){
         n_dst_mappings ++;
     };
     /*fprintf(stderr,"n_dst_mappings: %d \n", n_dst_mappings); */

     dst_caps = (IBP_cap *)calloc(n_dst_mappings,sizeof(IBP_cap));
     dm_type = (int *)calloc(n_dst_mappings,sizeof(int));
     ports = (int *)calloc(n_dst_mappings,sizeof(int));
     index = (int *)calloc(n_dst_mappings,sizeof(int));
     dst_timers = (struct ibp_timer *)calloc(n_dst_mappings,sizeof(struct ibp_timer));
     if ( dst_caps == NULL || 
         dm_type == NULL ||
         ports == NULL ||
         index == NULL ||
         dst_timers == NULL ) {
         ret = LORS_NO_MEMORY; 
         goto bail;
     };

     tlist = new_dllist();
     n_dst = 0;
     dll_traverse(dll_node,dst_mappings){
         lm = (LorsMapping *)jval_v(dll_node->val);
         dst_caps[n_dst] = lm->capset.writeCap;
         dm_type[n_dst] = DM_UNI;
         found = 0;
         if(n_dst != 0)
         {
           dll_rtraverse(tlist1,tlist)
           {
             if(strcmp(lm->depot.host,(char *)tlist1->val.v) == 0)
             {
                found = 1;
                break;
             }
           }
         }
         if(found == 1)
         {
           if(dm_type[n_dst] != DM_MCAST)
              tmpport++;
         }

         ports[n_dst] = tmpport;
         dll_append(tlist,new_jval_v(lm->depot.host));
         k = _lorsFindDepot(dp_reso->dst_dps,&(lm->depot));
         assert(k >= 0);
         index[n_dst] = k;  
         n_dst++;
       //  tmpport++; 
     };

     dll_traverse(dll_node,dl_list->list)
     {
        lm_tree = make_jrb();
        left_t = _lorsRemainTime(begin_time,job->timeout);
        if ( left_t < 0 ){
            ret = LORS_TIMEOUT_EXPIRED;
            goto bail;
        }; 
        dl_node = (__LorsDLNode *)jval_v(dll_node->val);

       /* fprintf(stderr, "dl_node: 0x%x\n", dl_node); */
        if ( dl_node->mappings == NULL ) 
        {
            fprintf(stderr, "dl_node->mappings is NULL...\n");
            break; /* SAS */
        }
        src_cap = NULL;
        src_lm = NULL;
        dll_traverse(tmp_node,dl_node->mappings){
            lm = (LorsMapping *)jval_v(tmp_node->val);
            if ( lm->notvalid == 1 ) continue;
            k = _lorsFindDepot(dp_reso->src_dps,&(lm->depot));
            assert(k >= 0 );
            metric_s = 0;
            metric = 0;
            /* TODO: there was a bus error once. Needs testing */
            dll_traverse(sub_node,dst_mappings){
                dst_lm = (LorsMapping *)jval_v(sub_node->val);
                if ( lr != NULL ) {
                    lorsGetResolutionPoint(lr, &lm->depot, &dst_lm->depot, &metric);
                } else {
                    metric = 0;
                }
                metric_s = metric_s + metric;
            };
            jrb_insert_dbl(lm_tree,metric_s,new_jval_v(lm));
        };

        jrb_node = jrb_last(lm_tree);
next_mapping:
        src_lm = (LorsMapping*)jval_v(jrb_node->val);
        src_cap = src_lm->capset.readCap;
        if ( g_lors_demo )
        {
            dll_traverse(tmplist,dst_mappings){
          dst_lm = (LorsMapping *)jval_v(tmplist->val);
          dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
#ifdef _MINGW
          fprintf(stderr,"DRAW MappingBegin %s:%4d %ld %I64d %I64d %s:%4d\n", 
             src_lm->depot.host,src_lm->depot.port,dst_lm->id,
             dl_node->offset,dl_node->length,
             dst_lm->depot.host,dst_lm->depot.port);
          fflush(stderr);
#else
          fprintf(stderr,"DRAW MappingBegin %s:%4d %ld %lld %lld %s:%4d\n", 
             src_lm->depot.host,src_lm->depot.port,dst_lm->id,
             dl_node->offset,dl_node->length,
             dst_lm->depot.host,dst_lm->depot.port);
#endif
          fprintf(stderr,"DRAW Arrow3 from %s:%4d %d to %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port, dst_lm->id,
               dst_lm->depot.host,dst_lm->depot.port);
            };
        }
 
        p_off_in_mapping = dl_node->offset - src_lm->exnode_offset + src_lm->alloc_offset;
        p_len = dl_node->length;

        if ( job->timeout > 0 ){
            src_timer.ServerSync = left_t ;
            src_timer.ClientTimeout = left_t -1;
            for ( i =0 ; i< n_dst;i++){
              dst_timers[i].ClientTimeout = left_t;
              dst_timers[i].ServerSync = left_t + 1;
            };
        } else {
            src_timer.ServerSync = 100;
            src_timer.ClientTimeout = 200;
            for ( i =0 ; i< n_dst;i++){
              dst_timers[i].ClientTimeout = 100;
              dst_timers[i].ServerSync = 200;
            };
        };
  
     /* fprintf(stderr,"n_dst = %d p_len = %d p_off = %d\n",n_dst,p_len,p_off_in_mapping); */

        nwrite = IBP_mcopy(src_cap,dst_caps,n_dst,&src_timer,dst_timers,
                          p_len,p_off_in_mapping,dm_type,ports,DM_PMULTI);
        if ( nwrite <= 0){
            ret = LORS_IBP_FAILED + IBP_errno;

            lorsDebugPrint(D_LORS_VERBOSE, "IBP_mcopy() failed: %d : %d\n", ret, nwrite);

            fprintf(stderr, "src: %s dst: %s\n", src_cap, dst_caps[0]);
            jrb_node = jrb_prev(jrb_node);
            if ( jrb_node == jrb_nil(lm_tree)){
                goto bail;
            }else {
                if ( g_lors_demo )
                {
               dll_traverse(tmplist,dst_mappings){
                     dst_lm = (LorsMapping *)jval_v(tmplist->val);
                     dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
                     fprintf(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
                                    src_lm->depot.host,src_lm->depot.port, dst_lm->id,
                                    dst_lm->depot.host,dst_lm->depot.port);
                     fflush(stderr);
               };
                }
               ret = LORS_SUCCESS; 
               goto next_mapping;   
            };
        };
        jrb_free_tree(lm_tree);
        lm_tree = NULL;
        if ( g_lors_demo )
        {
        dll_traverse(tmplist,dst_mappings){
             dst_lm = (LorsMapping *)jval_v(tmplist->val);
             dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
             fprintf(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
                             src_lm->depot.host,src_lm->depot.port,dst_lm->id,
                             dst_lm->depot.host,dst_lm->depot.port);
#ifdef _MINGW
             fprintf(stderr,"DRAW MappingEnd %ld %I64d %I64d %s:%4d\n",
                             dst_lm->id,dl_node->offset,dl_node->length,
                             dst_lm->depot.host,dst_lm->depot.port);
             fflush(stderr);
#else
             fprintf(stderr,"DRAW MappingEnd %ld %lld %lld %s:%4d\n",
                             dst_lm->id,dl_node->offset,dl_node->length,
                             dst_lm->depot.host,dst_lm->depot.port);
#endif
        };
        }
     };


     job_status = JOB_COMMIT;
     ret = LORS_SUCCESS;

bail:
     if ( dst_caps != NULL) free (dst_caps);
     if ( dm_type != NULL ) free (dm_type);
     if ( ports != NULL )   free (ports);
 
     if ( ret != LORS_SUCCESS )
          job_status = JOB_FAILED; 
     if ( lm_tree != NULL)
          jrb_free_tree(lm_tree);
     _lorsCommitJob(job,job_status,ret);
     pthread_cleanup_pop(0);
     return(ret);     
};
                    

int
_lorsDoMCopyE2E ( __LorsJob *job,
                  __LorsThreadInfo *tinfo )
{
     JRB             lm_tree = NULL,jrb_node;
     LorsMapping     *lm = NULL,*src_lm = NULL,*dst_lm = NULL;
     LorsDepot       *depot = NULL;
     IBP_cap         *dst_caps = NULL,src_cap = NULL;
     int             *dm_type = NULL,*ports = NULL,*index = NULL;
     Dllist          dst_mappings = NULL,src_mappings = NULL;
     __LorsDepotResolution *dp_reso = NULL;
     __LorsDLList    dl_list;
     __LorsDLNode    *dl_node = NULL; 
     struct ibp_timer src_timer,*dst_timers = NULL;
     longlong        nwrite;
     ulong_t         p_off_in_mapping,p_len;
     int             n_dst = 0, j,k;
     int             ret,i,n_dst_mappings = 0;
     time_t          begin_time,left_t;
     time_t          begin_download;
     double          bandwidth,max_metric,metric, metric_s;
     __LorsJobStatus job_status;
     Dllist          dll_node = NULL,tmp_node = NULL,tmplist = NULL;
     LboneResolution *lr = NULL;
     Dllist         tlist,tlist1;
     int            found = 0;


     assert(job != NULL && tinfo != NULL );
     lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoMCopyE2E[%d]: begin mcopy e2e...\n",pthread_self());

     pthread_cleanup_push(_lorsDoCleanup,(void*)job);
     begin_time = time(0);
 
     dst_mappings = (Dllist)job->dst;
     src_mappings = (Dllist)job->src;
     dp_reso = (__LorsDepotResolution *)job->extra_data;
     n_dst_mappings = 0;
     dll_traverse(dll_node,dst_mappings){
         n_dst_mappings ++;
     };
     dst_caps = (IBP_cap *)calloc(n_dst_mappings,sizeof(IBP_cap));
     dm_type = (int *)calloc(n_dst_mappings,sizeof(int));
     ports = (int *)calloc(n_dst_mappings,sizeof(int));
     index = (int *)calloc(n_dst_mappings,sizeof(int));
         dst_timers = (struct ibp_timer *)calloc(n_dst_mappings,sizeof(struct ibp_timer));
         
     if ( dst_caps == NULL ||
          dm_type == NULL ||
          ports == NULL ||
          index == NULL ) {
                          ret = LORS_NO_MEMORY; 
         goto bail;
     };

     lm_tree = make_jrb();
     tlist = new_dllist();

     n_dst = 0;
     dll_traverse(dll_node,dst_mappings){
         dst_lm = (LorsMapping *)jval_v(dll_node->val);
         dst_caps[n_dst] = dst_lm->capset.writeCap;
         dm_type[n_dst] = DM_UNI;
         found  = 0;
         if(n_dst != 0)
         {
           dll_rtraverse(tlist1,tlist)
           {
             if(strcmp(dst_lm->depot.host,(char *)tlist1->val.v) == 0)
             {
                found = 1;
                break;
             }
           }   
         }
         if(found == 1)
         {
            if(dm_type[n_dst] != DM_MCAST)
              tmpport++;
         }
 
         ports[n_dst] = tmpport;
         dll_append(tlist,new_jval_v(dst_lm->depot.host));

         k = _lorsFindDepot(dp_reso->dst_dps,&(dst_lm->depot));
         assert( k >= 0);
         index[n_dst] = k;  
         n_dst++; 
        // tmpport++;
     };

     left_t = _lorsRemainTime(begin_time,job->timeout);
     if ( left_t < 0 ){
         ret = LORS_TIMEOUT_EXPIRED;
         goto bail;
     }; 
     src_cap = NULL;
     src_lm = NULL;
     dll_traverse(tmp_node,src_mappings){
         lm = (LorsMapping *)jval_v(tmp_node->val);
            if ( lm->notvalid == 1 ) continue;
         k = _lorsFindDepot(dp_reso->src_dps,&(lm->depot));
         assert(k >= 0 );
         metric = 0;
         metric_s = 0;
         dll_traverse(dll_node,dst_mappings){
             dst_lm = (LorsMapping *)jval_v(dll_node->val);
             if ( lr != NULL ) {
                lorsGetResolutionPoint(lr, &lm->depot, &dst_lm->depot, &metric);
             } else {
                 metric = 0;
             }
             metric_s = metric_s + metric;
         };
         jrb_insert_dbl(lm_tree,metric_s,new_jval_v(lm));
     };

     jrb_node = jrb_last(lm_tree);
next_mapping:
     src_lm = (LorsMapping *)jval_v(jrb_node->val);
     src_cap = src_lm->capset.readCap;
     if ( g_lors_demo )
     {
     dll_traverse(tmplist,dst_mappings){
          dst_lm = (LorsMapping*)jval_v(tmplist->val);
          dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
#ifdef _MINGW
          fprintf(stderr,"DRAW MappingBegin %s:%4d %ld %I64d %ld %s:%4d\n",
             src_lm->depot.host,src_lm->depot.port,dst_lm->id,
             src_lm->exnode_offset,src_lm->logical_length,
             dst_lm->depot.host,dst_lm->depot.port);
          fflush(stderr);
#else
          fprintf(stderr,"DRAW MappingBegin %s:%4d %ld %lld %ld %s:%4d\n",
             src_lm->depot.host,src_lm->depot.port,dst_lm->id,
             src_lm->exnode_offset,src_lm->logical_length,
             dst_lm->depot.host,dst_lm->depot.port);
#endif
          fprintf(stderr,"DRAW Arrow3 from %s:%4d %d to %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port, dst_lm->id,
               dst_lm->depot.host,dst_lm->depot.port);

     };
     }
     assert(src_lm->exnode_offset == dst_lm->exnode_offset);
     assert(src_lm->alloc_length == dst_lm->alloc_length);
               
     p_off_in_mapping = src_lm->alloc_offset;
     p_len = lm->alloc_length;
     src_timer.ServerSync = left_t ;
     src_timer.ClientTimeout = left_t -1;
     for(i=0;i<n_dst;i++){
       dst_timers[i].ServerSync = left_t;
       dst_timers[i].ClientTimeout = left_t +1;
     };

  
/*     fprintf(stderr,"n_dst = %d p_len = %d p_off = %d\n",n_dst,p_len,p_off_in_mapping); */

     nwrite = IBP_mcopy(src_cap,dst_caps,n_dst,&src_timer,dst_timers,
                     p_len,p_off_in_mapping,dm_type,ports,DM_PMULTI);
     if ( nwrite <= 0){
        ret = LORS_IBP_FAILED + IBP_errno;
        jrb_node = jrb_prev(jrb_node);
        if ( jrb_node == jrb_nil(lm_tree)){
              goto bail;
         }else {
             if ( g_lors_demo )
             {
              dll_traverse(tmplist,dst_mappings){
                   dst_lm = (LorsMapping *)jval_v(tmplist->val);
                   dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
                   fprintf(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
                                  src_lm->depot.host,src_lm->depot.port, dst_lm->id,
                                  dst_lm->depot.host,dst_lm->depot.port);
                   fflush(stderr);
              };
             }
              ret = LORS_SUCCESS;
              goto next_mapping;   
         };

     };
     if ( g_lors_demo )
     {
      dll_traverse(tmplist,dst_mappings){
             dst_lm = (LorsMapping *)jval_v(tmplist->val);
             dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
             fprintf(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
                             src_lm->depot.host,src_lm->depot.port, dst_lm->id,
                             dst_lm->depot.host,dst_lm->depot.port);
#ifdef _MINGW
             fprintf(stderr,"DRAW MappingEnd %d %I64d %ld %s:%4d\n",
                             dst_lm->id,src_lm->exnode_offset,src_lm->logical_length,
                             dst_lm->depot.host,dst_lm->depot.port);
#else
             fprintf(stderr,"DRAW MappingEnd %d %lld %ld %s:%4d\n",
                             dst_lm->id,src_lm->exnode_offset,src_lm->logical_length,
                             dst_lm->depot.host,dst_lm->depot.port);
#endif
             fflush(stderr);
             exnodeCopyFunction(&(dst_lm->function), src_lm->function);
             dst_lm->e2e_bs = src_lm->e2e_bs;
      };
     }

     job_status = JOB_COMMIT;
    ret = LORS_SUCCESS;

bail:

     if ( dst_caps != NULL) free (dst_caps);
     if ( dm_type != NULL ) free (dm_type);
     if ( ports != NULL )   free (ports);
 
     if ( ret != LORS_SUCCESS )
          job_status = JOB_FAILED; 
     if ( lm_tree != NULL)
          jrb_free_tree(lm_tree);
     _lorsCommitJob(job,job_status,ret);
     pthread_cleanup_pop(0);
     return(ret);     

};

int
_lorsDoCopyNE ( __LorsJob *job,
                  __LorsThreadInfo *tinfo )
{
     JRB             lm_tree = NULL,jrb_node;
     LorsMapping     *lm,*src_lm;
     LorsDepot       *depot;
     IBP_cap         dst_cap,src_cap = NULL;
     int             index;
     LorsMapping     *dst_mapping;
     __LorsDepotResolution *dp_reso;
     __LorsDLList    dl_list;
     __LorsDLNode    *dl_node; 
     struct ibp_timer src_timer,dst_timer;
     longlong        p_off_in_mapping, p_len;
     ulong_t         n_done = 0,nwrite;
     int             n_dst = 0, j,k;
     int             ret,i;
     time_t          begin_time,left_t,tt;
     time_t          begin_download;
     double          bandwidth,max_metric,metric;
     __LorsJobStatus job_status;
     Dllist          dll_node,tmp_node;
     LboneResolution    *lr;

     double t1, t2;
     ulong_t  demo_len;

     assert(job != NULL && tinfo != NULL );
     lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCopyNE[%d]: begin copy no e2e...\n",pthread_self());

     pthread_cleanup_push(_lorsDoCleanup,(void*)job);
     begin_time = time(0);

 
     dst_mapping = (LorsMapping*)job->dst;
     dp_reso = (__LorsDepotResolution *)job->extra_data;
     dl_list = (__LorsDLList)job->src;
     dst_cap = dst_mapping->capset.writeCap;
     lr = dp_reso->lr;
     index = _lorsFindDepot(dp_reso->dst_dps,&(dst_mapping->depot));
     assert(index >= 0);


    dll_traverse(dll_node,dl_list->list)
    {
        lm_tree = make_jrb();
        left_t = _lorsRemainTime(begin_time,job->timeout);
        if ( left_t < 0 ){
            ret = LORS_TIMEOUT_EXPIRED;
            goto bail;
        }; 
        dl_node = (__LorsDLNode *)jval_v(dll_node->val);
        src_cap = NULL;
        src_lm = NULL;
        dll_traverse(tmp_node,dl_node->mappings){
            lm = (LorsMapping *)jval_v(tmp_node->val);
            if ( lm->notvalid == 1 ) continue;
            src_cap = lm->capset.readCap;
            k = _lorsFindDepot(dp_reso->src_dps,&(lm->depot));
            assert(k >= 0 );
            metric = 0;
            if ( lr != NULL )
            {
                lorsGetResolutionPoint(lr, &lm->depot, &dst_mapping->depot, &metric);
            } else 
            {
                metric = 0.0;
            }

            /*fprintf(stderr, "From %s:%d to %s:%d == %f\n", 
                                lm->depot.host, lm->depot.port,
                                dst_mapping->depot.host, dst_mapping->depot.port,
                                metric);
                                */
            jrb_insert_dbl(lm_tree,metric,new_jval_v(lm));
        };
        jrb_node = jrb_last(lm_tree);

next_mapping:
        src_lm = (LorsMapping*)jval_v(jrb_node->val);
        src_cap = src_lm->capset.readCap;
        p_off_in_mapping = dl_node->offset - src_lm->exnode_offset + src_lm->alloc_offset;
        p_len = dl_node->length;
        n_done = 0;

        if ( g_lors_demo )
        {
        dst_mapping->id = _lorsIDFromCap(dst_mapping->capset.readCap);
        lorsDemoPrint(stderr,"DRAW Arrow3 from %s:%4d %d to %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port, dst_mapping->id,
               dst_mapping->depot.host,dst_mapping->depot.port);
#ifdef _MINGW
        lorsDemoPrint(stderr,"DRAW MappingFrom %s:%4d %I64d %I64d %d %I64d %ld %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port,
               src_lm->exnode_offset+p_off_in_mapping+n_done, p_len,
               dst_mapping->id, dst_mapping->exnode_offset, 
               dst_mapping->logical_length, 
               dst_mapping->depot.host,dst_mapping->depot.port);
#else
        lorsDemoPrint(stderr,"DRAW MappingFrom %s:%4d %lld %lld %d %lld %ld %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port,
               src_lm->exnode_offset+p_off_in_mapping+n_done, p_len,
               dst_mapping->id, dst_mapping->exnode_offset, 
               dst_mapping->logical_length, 
               dst_mapping->depot.host,dst_mapping->depot.port);
#endif
               fflush(stderr);
        }
        while ( n_done < p_len)
        {
            left_t = _lorsRemainTime(begin_time,job->timeout);
            if ( left_t < 0 ) {
                ret = LORS_TIMEOUT_EXPIRED;
                goto bail;
            };

            if ( job->timeout > 0 ){
                src_timer.ServerSync = left_t;
                src_timer.ClientTimeout = left_t ;
                dst_timer.ServerSync = left_t;
                dst_timer.ClientTimeout = left_t +1;
            }else {
                src_timer.ServerSync = 1;
                src_timer.ClientTimeout = 0;
                dst_timer.ServerSync = 1;
                dst_timer.ClientTimeout = 0;
            };
            /*fprintf(stderr, "ClientTimeout: %d %d\n", */
                    /*src_timer.ClientTimeout, dst_timer.ClientTimeout);*/
            nwrite = 0;

            if ( g_lors_demo )
            {
                /* begin time */
                t1 = getTime();
                pthread_mutex_lock(&copy_thread_lock);
                g_copy_thread_count++;
                pthread_mutex_unlock(&copy_thread_lock);
            }
            nwrite = IBP_copy(src_cap,dst_cap,&src_timer,&dst_timer,
                          (ulong_t)p_len-n_done,(ulong_t)p_off_in_mapping+n_done); 
            if ( g_lors_demo )
            {
                /* end time */
                t2 = diffTime(t1);
                demo_len = p_len-n_done;
                if ( nwrite > 0 ) {
                    pthread_mutex_lock(&copy_thread_lock);
                    lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", 
                        g_copy_thread_count*(demo_len*8.0)/1024.0/1024.0/t2);
                    pthread_mutex_unlock(&copy_thread_lock);
                    fflush(stderr);
                }
                pthread_mutex_lock(&copy_thread_lock);
                g_copy_thread_count--;
                pthread_mutex_unlock(&copy_thread_lock);
            }

#ifdef _MINGW
            lorsDebugPrint(1, "Copy %24s:%4d to %24s:%4d %I64d %I64d\n",
                                src_lm->depot.host, src_lm->depot.port,
                                dst_mapping->depot.host, dst_mapping->depot.port,
                                src_lm->exnode_offset+p_off_in_mapping+n_done, 
                                p_len-n_done);
#else
            lorsDebugPrint(1, "Copy %24s:%4d to %24s:%4d %lld %lld\n",
                                src_lm->depot.host, src_lm->depot.port,
                                dst_mapping->depot.host, dst_mapping->depot.port,
                                src_lm->exnode_offset+p_off_in_mapping+n_done, 
                                p_len-n_done);
#endif
            if ( nwrite == 0){
                ret = LORS_IBP_FAILED + IBP_errno;
                jrb_node = jrb_prev(jrb_node);
                if ( jrb_node == jrb_nil(lm_tree)){
                    goto bail;
                }else {
                    if ( g_lors_demo )
                    {
                    lorsDemoPrint(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
                        src_lm->depot.host,src_lm->depot.port, dst_mapping->id,
                        dst_mapping->depot.host,dst_mapping->depot.port);
                        fflush(stderr);
                    }
                    ret = LORS_SUCCESS;
                    goto next_mapping;   
                };
            };
            n_done = n_done + nwrite;
        };
        assert(n_done == p_len);
        jrb_free_tree(lm_tree);
        lm_tree = NULL;
        if ( g_lors_demo )
        {
        lorsDemoPrint(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
               src_lm->depot.host,src_lm->depot.port, dst_mapping->id,
               dst_mapping->depot.host,dst_mapping->depot.port);
#ifdef _MINGW
        lorsDemoPrint(stderr,"DRAW MappingFinish %I64d %I64d %ld %I64d %ld %s:%4d\n",
               src_lm->exnode_offset+p_off_in_mapping, p_len,
               dst_mapping->id,dst_mapping->exnode_offset, 
               dst_mapping->logical_length,
               dst_mapping->depot.host,dst_mapping->depot.port);
#else
        lorsDemoPrint(stderr,"DRAW MappingFinish %lld %lld %ld %lld %ld %s:%4d\n",
               src_lm->exnode_offset+p_off_in_mapping, p_len,
               dst_mapping->id,dst_mapping->exnode_offset, 
               dst_mapping->logical_length,
               dst_mapping->depot.host,dst_mapping->depot.port);
#endif
        fflush(stderr);
        }
     };

     job_status = JOB_COMMIT;
     ret = LORS_SUCCESS;

bail:
     if ( ret != LORS_SUCCESS )
          job_status = JOB_FAILED; 
     _lorsCommitJob(job,job_status,ret);
     if ( lm_tree != NULL)
         jrb_free_tree(lm_tree);
     pthread_cleanup_pop(0);
     return(ret);     
};
                    

int
_lorsDoCopyE2E ( __LorsJob *job,
                  __LorsThreadInfo *tinfo )
{
     JRB             lm_tree = NULL,jrb_node;
     LorsMapping     *lm,*src_lm,*dst_lm;
     LorsDepot       *depot;
     IBP_cap         dst_cap,src_cap = NULL;
     int             index;
     Dllist          src_mappings;
     __LorsDepotResolution *dp_reso;
     __LorsDLList    dl_list;
     __LorsDLNode    *dl_node; 
     struct ibp_timer src_timer,dst_timer;
     ulong_t        nwrite ,n_done = 0;
     int             n_dst = 0, j,k;
     int             ret,i;
     time_t          begin_time,left_t;
     time_t          begin_download;
     double          bandwidth,max_metric,metric;
     __LorsJobStatus job_status;
     Dllist          dll_node,tmp_node;
     ulong_t         p_off_in_mapping,p_len;
     LboneResolution *lr;
            double t1, t2;
            ulong_t  demo_len;

     assert(job != NULL && tinfo != NULL );
     lorsDebugPrint(D_LORS_VERBOSE,"_lorsDoCopyE2E[%d]: begin copy e2e...\n",pthread_self());

     pthread_cleanup_push(_lorsDoCleanup,(void*)job);
     begin_time = time(0);
 
     dst_lm = (LorsMapping *)job->dst;
     dst_cap = dst_lm->capset.writeCap;
     src_mappings = (Dllist)job->src;
     dp_reso = (__LorsDepotResolution *)job->extra_data;
     lr = dp_reso->lr;
     index = _lorsFindDepot(dp_reso->dst_dps,&(dst_lm->depot));
     assert(index >= 0);


     left_t = _lorsRemainTime(begin_time,job->timeout);
     if ( left_t < 0 ){
         ret = LORS_TIMEOUT_EXPIRED;
         goto bail;
     }; 

     lm_tree = make_jrb();
     src_cap = NULL;
     src_lm = NULL;
     dll_traverse(tmp_node,src_mappings){
         lm = (LorsMapping *)jval_v(tmp_node->val);
            if ( lm->notvalid == 1 ) continue;
         k = _lorsFindDepot(dp_reso->src_dps,&(lm->depot));
         assert(k >= 0 );
         metric = 0;
         if ( lr != NULL )
         {
            lorsGetResolutionPoint(lr, &lm->depot, &dst_lm->depot, &metric);
         } else 
         {
             metric = 0.0;
         }
         jrb_insert_dbl(lm_tree,metric,new_jval_v(lm));
     };

     jrb_node = jrb_last(lm_tree);

next_mapping:
     src_lm = (LorsMapping*)jval_v(jrb_node->val);
     src_cap = src_lm->capset.readCap;

     assert(src_lm->exnode_offset == dst_lm->exnode_offset);
     assert(src_lm->alloc_length == dst_lm->alloc_length);
     p_off_in_mapping = src_lm->alloc_offset;
     p_len = lm->alloc_length;
     n_done = 0;
     if ( g_lors_demo )
     {
       dst_lm->id = _lorsIDFromCap(dst_lm->capset.readCap);
       lorsDemoPrint(stderr,"DRAW Arrow3 from %s:%4d %d to %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port, dst_lm->id,
               dst_lm->depot.host,dst_lm->depot.port);
#ifdef _MINGW
       lorsDemoPrint(stderr,"DRAW MappingFrom %s:%4d %I64d %ld %d %I64d %ld %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port,
               src_lm->exnode_offset+p_off_in_mapping+n_done, p_len,
               dst_lm->id, dst_lm->exnode_offset, 
               dst_lm->logical_length, 
               dst_lm->depot.host,dst_lm->depot.port);
#else
       lorsDemoPrint(stderr,"DRAW MappingFrom %s:%4d %lld %ld %d %lld %ld %s:%4d 10\n",
               src_lm->depot.host,src_lm->depot.port,
               src_lm->exnode_offset+p_off_in_mapping+n_done, p_len,
               dst_lm->id, dst_lm->exnode_offset, 
               dst_lm->logical_length, 
               dst_lm->depot.host,dst_lm->depot.port);
#endif
               fflush(stderr);
     }
     while ( n_done < p_len){
        left_t = _lorsRemainTime(begin_time, job->timeout);
        if ( left_t < 0 ){
            ret = LORS_TIMEOUT_EXPIRED;
            goto bail;
        };
        if ( job->timeout > 0){
           src_timer.ServerSync = left_t ;
           src_timer.ClientTimeout = left_t ;
           dst_timer.ServerSync = left_t;
           dst_timer.ClientTimeout = left_t +1;
        }else {
           src_timer.ServerSync = 1;
           src_timer.ClientTimeout = 0;
           dst_timer.ServerSync = 1;
           dst_timer.ClientTimeout = 0;
        };
        lorsDebugPrint(1, "Copy %24s:%4d to %24s:%4d %d %d\n",
                            src_lm->depot.host, src_lm->depot.port,
                            dst_lm->depot.host, dst_lm->depot.port,
                            (ulong_t)(src_lm->exnode_offset+p_off_in_mapping+n_done), 
                            p_len-n_done);
        if ( g_lors_demo )
        {
            /* begin time */
            t1 = getTime();
            pthread_mutex_lock(&copy_thread_lock); 
            g_copy_thread_count++;
            pthread_mutex_unlock(&copy_thread_lock);
        }
        nwrite = IBP_copy(src_cap,dst_cap,&src_timer,&dst_timer,
                     (ulong_t)p_len-n_done,(ulong_t)p_off_in_mapping+n_done);
        if ( g_lors_demo )
        {
            /* end time */
            t2 = diffTime(t1);
            demo_len = p_len-n_done;
            if ( nwrite > 0 )
            {
                pthread_mutex_lock(&copy_thread_lock);
                lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", 
                    g_copy_thread_count*(demo_len*8.0)/1024.0/1024.0/t2);
                pthread_mutex_unlock(&copy_thread_lock);
                fflush(stderr);
            }
            pthread_mutex_lock(&copy_thread_lock);
            g_copy_thread_count--;
            pthread_mutex_unlock(&copy_thread_lock);
        }
        if ( nwrite <= 0){

            ret = LORS_IBP_FAILED + IBP_errno;
            fprintf(stderr, "ERROR: Copy E2E %d\n", ret);
              jrb_node = jrb_prev(jrb_node);
              if ( jrb_node == jrb_nil(lm_tree)){
                    goto bail;
              }else {
                  if ( g_lors_demo )
                  {
                     lorsDemoPrint(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
                             src_lm->depot.host,src_lm->depot.port, dst_lm->id,
                             dst_lm->depot.host,dst_lm->depot.port);
                     fflush(stderr);
                  }
                  ret = LORS_SUCCESS;
                  goto next_mapping;   
              };
        };
        n_done = n_done + nwrite;
     };
     if ( g_lors_demo )
     {
       lorsDemoPrint(stderr,"DELETE Arrow3 from %s:%4d %d to %s:%4d \n",
               src_lm->depot.host,src_lm->depot.port, dst_lm->id,
               dst_lm->depot.host,dst_lm->depot.port);
#ifdef _MINGW
       lorsDemoPrint(stderr,"DRAW MappingFinish %I64d %ld %ld %I64d %ld %s:%4d\n",
               src_lm->exnode_offset+p_off_in_mapping, p_len,
               dst_lm->id,dst_lm->exnode_offset, 
               dst_lm->logical_length,
               dst_lm->depot.host,dst_lm->depot.port);
#else
       lorsDemoPrint(stderr,"DRAW MappingFinish %lld %ld %ld %lld %ld %s:%4d\n",
               src_lm->exnode_offset+p_off_in_mapping, p_len,
               dst_lm->id,dst_lm->exnode_offset, 
               dst_lm->logical_length,
               dst_lm->depot.host,dst_lm->depot.port);
#endif
        fflush(stderr);
     }
     /* copy the 'functions' from src_lm to dst_lm */
     exnodeCopyFunction(&(dst_lm->function), src_lm->function);
     dst_lm->e2e_bs = src_lm->e2e_bs;

     assert( n_done == p_len);

     job_status = JOB_COMMIT;
     ret = LORS_SUCCESS;

bail:

     if ( ret != LORS_SUCCESS )
          job_status = JOB_FAILED; 
     if ( lm_tree != NULL)
          jrb_free_tree(lm_tree);
     _lorsCommitJob(job,job_status,ret);
     pthread_cleanup_pop(0);
     return(ret);     
};


int lorsSetCopy (LorsSet  *srcSE,
                 LorsSet  *dstSE,
                 LorsDepotPool  * dp,
                 LboneResolution    *lr,
                 longlong offset,
                 longlong length,
                 int      nthreads,
                 int      timeout,
                 int      opts)
{


    int         ret = LORS_SUCCESS,i;
    int         n_src_dps, n_dst_dps;
    int         success_cnt = 0;
    LorsSet     *new_set = NULL;
    IBP_depot   *src_dps = NULL, *dst_dps = NULL;
    double      **res = NULL; 
    time_t      l_time, begin_time;
    Dllist      node,job_node;
    __LorsJob   *job;
    __LorsDLList dl_list = NULL;
    __LorsDLNode *dl_node;
    longlong     *block_array = NULL , *block_array_e2e = NULL;
    int         array_len;
    __LorsDepotResolution dp_reso;
    __LorsJobPool  *jp = NULL;
    __LorsThreadPool *tp = NULL;
    int           job_type;
    int            has_e2e;
    LorsMapping  *lm;
    ulong_t  data_blocksize;
    int      copies;
    data_blocksize = dstSE->data_blocksize;
    copies  = dstSE->copies;

    pthread_mutex_init(&copy_thread_lock, NULL);
    if ( g_lors_demo )
    {
        pthread_mutex_lock(&copy_thread_lock);
        g_copy_thread_count = 0;
        pthread_mutex_unlock(&copy_thread_lock);
        lorsDemoPrint(stderr, "MESSAGE 1 SetCopy\n");
        fflush(stderr);
    }

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: Beginning copy....\n");
    begin_time = time(0);
    l_time = timeout;
    if ( ( srcSE == NULL ) || ( dp == NULL ) ||
         ( copies <=0    ) || ( offset < 0 ) ||
         ( length <= 0   )  ) {
         return ( LORS_INVALID_PARAMETERS );
    };
    
    n_src_dps = jrb_n_nodes(srcSE->mapping_map);
    n_dst_dps = jrb_n_nodes(dp->list);
           
    if ( ( n_src_dps == 0 ) || ( n_dst_dps == 0)) {
        fprintf(stderr, "n_src_dps: %d n_dst_dps: %d\n", n_src_dps, n_dst_dps);
         ret = LORS_NO_AVAILABLE_DEPOT;
         goto bail;
    }; 

    if ( ((src_dps = (IBP_depot *)calloc(n_src_dps + 1,sizeof(IBP_depot))) == NULL) ||
         ((dst_dps = (IBP_depot *)calloc(n_dst_dps + 1,sizeof(IBP_depot))) == NULL)) {
        ret = LORS_NO_MEMORY;
        goto bail; 
    };

    dp_reso.n_src_dps = _lorsCreateDepotList(src_dps,n_src_dps,srcSE->mapping_map,_lorsGetIBPDepotFromMapping);
   /***code added by kancherl **/
   /* _lorsRemoveDepots(dp->list,src_dps,dp_reso.n_src_dps); */
   /**till here **/        
    dp_reso.n_dst_dps = _lorsCreateDepotList(dst_dps,n_dst_dps,dp->list,_lorsGetIBPDepotFromDepotPool);

    dp_reso.src_dps = src_dps;
    dp_reso.dst_dps = dst_dps;

    /* get depots to depots resolution by calling lbone service */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: getting depots to depots resolution ...\n"); 

    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr, "MESSAGE 2 LBone NWS Resolution\n");
        fflush(stderr);
    }

    if ( (l_time = _lorsRemainTime(begin_time,timeout)) < 0 ) {
         ret = LORS_TIMEOUT_EXPIRED;
         goto bail;
    };
    dp_reso.lr = lr;
 
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: Creating download list ...\n"); 
    if (( ret = _lorsCreateDLList(srcSE,offset,length,&dl_list)) != LORS_SUCCESS){
        goto bail;
    };
    
    dll_traverse(node,dl_list->list){
       dl_node = (__LorsDLNode *)jval_v(node->val);
       if ( dll_empty(dl_node->mappings) ){
           /* TODO: augment should operate with holes. */
           ret = LORS_HAS_HOLE;
           goto bail;
       };
    };
    if (( ret = _lorsCreateBlockArray(&block_array,&block_array_e2e,&array_len,
                    length,data_blocksize,dl_list)) != LORS_SUCCESS ){
        goto bail;
    };

    /* create a new empty set */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: creating a new set ....\n");
    if ( (ret = _lorsSetAllocateByBlockArray(dstSE, dp,offset, block_array, 
                    block_array_e2e,array_len, dl_list, copies, nthreads,
                    l_time,opts|LORS_RETRY_UNTIL_TIMEOUT)) != LORS_SUCCESS ) {
          goto bail;
    };
    /**dstSE = new_set;*/
  
    l_time = _lorsRemainTime( begin_time,timeout);
    if ( l_time < 0 ){
           ret = LORS_TIMEOUT_EXPIRED;
           goto bail;
    };  


    /* create job pool */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: creating job pool ....\n");
    if ( HAS_OPTION(LORS_COPY,opts) )
       job_type = 0;
    else 
       job_type = 1 ;

        
    dl_node = ( __LorsDLNode *)jval_v(dll_first(dl_list->list)->val); 
    lm = (LorsMapping *)jval_v(dll_first(dl_node->mappings)->val);
    if ( _lorsHasE2E(lm) ) {
        has_e2e = 1;
    } else {
        has_e2e = 0;
    };

    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr, "MESSAGE 2 Copying..\n");
        fflush(stderr);
    }


    /*fprintf(stderr, "array_len: %d\n", array_len);*/
    if ( ( job_type == 0 ) && ( has_e2e == 1 ) ){
        ret = _lorsCreateCopyE2E(&jp,block_array,array_len,dl_list,offset,srcSE,dstSE,&dp_reso);
    } else if ( (job_type == 0) && ( has_e2e == 0) ){
        ret = _lorsCreateCopyNE(&jp,block_array,array_len,dl_list,offset,srcSE,dstSE,&dp_reso);
    } else if ((job_type == 1) && ( has_e2e == 1)) {
        ret = _lorsCreateMCopyE2E(&jp,block_array,array_len,dl_list,offset,srcSE,dstSE,&dp_reso);
    } else if ((job_type == 1) && ( has_e2e == 0)){
        ret = _lorsCreateMCopyNE(&jp,block_array,array_len,dl_list,offset,srcSE,dstSE,&dp_reso);
    } else {
        assert(0);
    };

    /* create thread pool */
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: Creating working threads...\n");
    ret = _lorsCreateThreadPool(jp,nthreads,&tp,l_time,opts);
    if ( ret != LORS_SUCCESS )
       goto bail;

     
    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: %d working threads have been created successfully.\n",tp->nthread);

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: Waitting worker thread return \n");
    ret = _lorsJoinThread(tp);

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: All Working threads have returned. \n");

    lorsDebugPrint(D_LORS_VERBOSE,"lorsSetCopy: Checking all jobs...\n");
    pthread_mutex_lock(&(jp->lock));
    dll_traverse(job_node,jp->job_list){
        job = (__LorsJob *)jval_v(job_node->val);
        if ( job->status != JOB_COMMIT ){
             switch(job->type){
             case J_COPY_NO_E2E:
             case J_COPY_E2E: 
                 lm = (LorsMapping *)job->dst;
                 lorsSetRemoveMapping(dstSE,lm);
                 _lorsFreeMapping(lm); 
                 break;
             case J_MCOPY_NO_E2E:
             case J_MCOPY_E2E:
                 dll_traverse(node,((Dllist)(job->dst))){
                      lm = (LorsMapping *)(jval_v(node->val));
                      lorsSetRemoveMapping(dstSE,lm);
                      _lorsFreeMapping(lm);
                 };
                 break;
             };
             ret = job->err_code;

             fprintf(stderr, "job->status != COMMIT: %d  err_code: %d\n", 
                     job->status, job->err_code);
        } else {
            success_cnt++;
        }
    };
    pthread_mutex_unlock(&(jp->lock));
   
bail:
    _lorsFreeThreadPool(tp);
    _lorsFreeJobPool(jp);
    _lorsFreeDLList(dl_list);
    if ( src_dps != NULL )  { free(src_dps); };
    if ( dst_dps != NULL )  { free(dst_dps); };
    if ( res !=  NULL ){
        for(i=0;i<dp_reso.n_src_dps;i++){
            free(res[i]);
        };
        free(res);
    };
    if ( block_array != NULL ) { free(block_array) ; };
    if ( block_array_e2e != NULL ) { free(block_array_e2e); };    
    if ( success_cnt > 0 && ret != LORS_SUCCESS ) return LORS_PARTIAL;

    return(ret);
}



