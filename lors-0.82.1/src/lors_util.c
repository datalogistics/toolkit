#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <lors_api.h>
#include <jval.h>
#include <jrb.h>

#include <ibp.h>
#include <lors_error.h>
#include <lors_job.h>
#include <lors_util.h>
#include <limits.h>
#include <sys/time.h>



#ifdef _MINGW
#include "timeval_h.h"
#endif

extern ulong_t g_lm_id;
int g_db_level = 0;
int g_lors_demo = 0;

LorsMapping *_lorsAllocMapping ( void )
{
    LorsMapping *lm = NULL;

    if ( (lm = (LorsMapping*)calloc(1,sizeof(LorsMapping))) == NULL ){
        return NULL;
    };
    memset(lm, 0x00, sizeof(LorsMapping));

    return (lm);
   
};

int  _lorsGetNumOfDepot( LorsDepotPool *dpp){
    int num = 0;
    JRB jrb_node;
    
    if ( dpp != NULL ){
        pthread_mutex_lock(dpp->lock);
        jrb_traverse(jrb_node,dpp->list ){
            num++;
        };
        pthread_mutex_unlock(dpp->lock);
    };

    return (num);
};

void _lorsFreeMapping( LorsMapping *lm){
      if ( lm != NULL ){
            if ( lm->capset.readCap != NULL){
                free(lm->capset.readCap);
                lm->capset.readCap= NULL;
            };
            if ( lm->capset.writeCap != NULL){
                free(lm->capset.writeCap);
                lm->capset.writeCap= NULL;
            };
            if ( lm->capset.manageCap != NULL){
                free(lm->capset.manageCap);
                lm->capset.manageCap= NULL;
            };
            if ( lm ->md != NULL ){
                exnodeDestroyMetadata(lm->md);
                lm->md = NULL;
            };
            if ( lm->function != NULL ) 
            {
                exnodeDestroyFunction(lm->function);
                lm->function = NULL;
            }
            free(lm);
      };
};

int _lorsIDFromCap ( char * cap)
{
    char buf[6];
    int i;
    if ( cap != NULL && strlen(cap) > 75 )
    {
        for (i=0; i < 5; i++)
        {
            buf[i] = cap[i+70];
        }
        buf[i] = '\0';

        i = atoi(buf);
    } else 
    {
        if ( g_lors_demo )
        {
            sprintf(buf, "%d", g_lm_id);
            g_lm_id ++;
        }
    }
    return atoi(buf);
}

int _lorsFindDepot ( IBP_depot *dp_list, IBP_depot depot)
{
    int i;
    int found = -1;

    for ( i=0; dp_list[i] != NULL ; i ++ ){
        if ( (strcmp(dp_list[i]->host,depot->host) == 0 ) && (dp_list[i]->port == depot->port)){
             found = i;
             break;
        };
    };

    return (found);
};

IBP_depot _lorsGetIBPDepotFromMapping ( JRB mapping_node ){
    LorsMapping *lm;

    assert(mapping_node != NULL);
    lm = (LorsMapping *)jval_v(mapping_node->val);
    return (&(lm->depot));
};

IBP_depot _lorsGetIBPDepotFromDepotPool( JRB depot_node ) {
    LorsDepot *dp;

    assert(depot_node != NULL );
    dp = (LorsDepot *)jval_v(depot_node->val);
    return (dp->depot);
};

int _lorsCreateDepotList( IBP_depot *dp_list, 
                     int       max_depots, 
                     JRB       jrb_tree, 
                     get_IBP_depot_func get_IBP_depot){
     JRB node;
     int i = 0,pos;
     IBP_depot dp;

     assert( max_depots > 0 );
     jrb_traverse(node, jrb_tree ) {
          dp = get_IBP_depot(node);
          pos = _lorsFindDepot(dp_list,dp);
          if ( pos < 0 ) {
              dp_list[i] = dp;
              dp_list[i+1] = NULL;
              i++;
              if ( i >= max_depots )  return i ;
          };
     };

     return i; 

};

char *lorsRTrim( char *str)
{
    int i , len;
    if ( str != NULL ){
        len = strlen(str);
        for ( i = len -1 ; i >=0 ; i -- )
           if ( str[i] != ' ' )
               break;
           else 
               str[i] = '\0';
    };
    
    return (str); 
};

char *lorsLTrim( char *str)
{
   int i , len , pos;

   if ( str != NULL ){
       pos = -1;
       len = strlen(str);
       for ( i =0 ; str[i] != '\0' ; i ++ ){
            if ( str[i] != ' ' )
                break;
            else
                pos = i;
       };
       if ( pos >= 0 ) {
           i = pos + 1;
           while ( str[i] != '\0' ){
               str[i-pos-1] = str[i];
               i++;
           };
   
           str[i-pos -1] = '\0'; 
       };
   };

   return (str);
   
};

char *lorsTrim ( char * str )
{
    return (lorsLTrim(lorsRTrim(str)));
};

int mapping_jvalcmp(Jval j1, Jval j2)
{
    LorsMapping *lm1, *lm2;
    lm1 = j1.v;
    lm2 = j2.v;
    if ( lm1->logical_offset == lm2->logical_offset ) return 0;
    return strcmp(lm1->capset.readCap, lm2->capset.readCap);
}

int ll_jvalcmp(Jval j1, Jval j2)
{
    if ( j1.ll == j2.ll ) return  0;
    if ( j1.ll > j2.ll ) return -1;
    else return 1;
} 

LorsBoundary *new_boundary(longlong offset, longlong length)
{
    LorsBoundary *lb;
    lb = (LorsBoundary *)calloc(1, sizeof(LorsBoundary));
    if ( lb == NULL ) return NULL;
    lb->offset = offset;
    lb->length = length;
    return lb;
}
double getTime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec*0.000001;
}
double diffTime(double t)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec + tv.tv_usec*0.000001) - t;
}
IBP_attributes  new_ibp_attributes(time_t duration_days, int reliability, int type)
{
    IBP_attributes  attr;
    attr = (IBP_attributes)calloc(1, sizeof(struct ibp_attributes));
    attr->duration = time(0) + duration_days;
    attr->reliability = reliability;
    attr->type = type;
    return attr;
}


IBP_depot new_ibp_depot(char *host, int port )
{
    IBP_depot lbserver = (IBP_depot)calloc(1, sizeof (struct ibp_depot));
    if ( lbserver != NULL ) {
        snprintf(lbserver->host, 255, "%s", host);
        lbserver->port = port;
    }
    return lbserver;
}

longlong *_lorsNewBlockArrayE2E(LorsSet *set, ulong_t length,longlong offset,ulong_t data_block,__LorsDLList dl_list, int *array_len)
{
    Dllist dlist,node;
    ulong_t  block_size;
    LorsMapping *lm;
    ulong_t  off_in_map,len;
    longlong *blk_array; 
    __LorsDLNode *dl_node;
    int i;

    assert( (set != NULL) && (length > 0 ) && (offset >= 0) &&
            ( data_block > 0 ) && (array_len != NULL) && (dl_list != NULL));

    dlist = new_dllist();
    block_size = data_block;
    dll_traverse(node,dl_list->list){
       dl_node = (__LorsDLNode *)jval_v(node->val);
       assert( !dll_empty(dl_node->mappings) );
       lm = (LorsMapping *)jval_v(dll_first(dl_node->mappings)->val);
       off_in_map = (ulong_t)(dl_node->offset - lm->exnode_offset);
       do {
            if ( (off_in_map + block_size) >= lm->logical_length ){
                 len = lm->logical_length - off_in_map;
                 dll_append(dlist,new_jval_l((long)len));
            }else {
                /* added -1 to be able todownload the proper e2e size for
                 * sub-encoded blocks */
                 len = ((off_in_map + block_size-1)/(lm->e2e_bs) + 1) * lm->e2e_bs - off_in_map;
                 dll_append(dlist,new_jval_l((long)len));
            };
            off_in_map = off_in_map + len;
       } while ( off_in_map < lm->logical_length); 
    }; 
   
    i = 0; 
    dll_traverse(node,dlist){
         i++;
    };

    *array_len = i;

    if ( (blk_array = (longlong*)calloc(i+1,sizeof(longlong))) == NULL ){
       free_dllist(dlist);
       return(NULL); 
    };
    
    i = 0;
    dll_traverse(node,dlist){
        blk_array[i] = (ulong_t)jval_l(node->val);
        i++; 
    };
    
    free_dllist(dlist);
    return(blk_array);

};


longlong *_lorsNewBlockArray(longlong  length, ulong_t blocksize, int *list_len)
{
    longlong *list;
    int      blocks,i;
    int      tblock;

    if ( length > blocksize ) 
    {
        blocks = (length % blocksize == 0 ? length / blocksize : length / blocksize + 1) ;
    } else{
        blocks = 1;
    }
    list = (longlong *)malloc(sizeof(longlong)*(blocks + 1));
    if ( list == NULL ) return NULL;
    list[blocks] = 0;
    for (i=0; i  < blocks-1; i++)
    {
        list[i] = blocksize;
        length -= blocksize;
    }
    list[i] = length;
    *list_len = blocks;
    return list;
}

JRB jrb_insert_llong(JRB tree, longlong key, Jval val)
{
    return jrb_insert_gen(tree, new_jval_ll(key), val, ll_jvalcmp);
}
JRB jrb_find_llong(JRB root, longlong lkey)
{
    return jrb_find_gen(root, new_jval_ll(lkey), ll_jvalcmp);
}

LorsSet *_lorsMallocSet(void)
{
    LorsSet *se;
    se = (LorsSet *)calloc(1, sizeof(LorsSet));
    if ( se == NULL ) return (NULL);
    se->mapping_map = make_jrb();
    /*se->boundary_list = new_dllist();*/
    return se;
}


int lorsDebugPrint( int db_level, char *format, ... )
{
    va_list           marker;
    int               i = 0;
 
    va_start(marker,format);
    if ( g_db_level & db_level ) {
         i = vfprintf(stderr, format,marker);
         fflush(stderr);
    };
   
    va_end( marker);
    return ( i ); 
};

int _lorsCalcTimeout(longlong length)
{
    /* translate length to KB, then divide by 100KB/sec + Padding */
    return (length/1024/10) + 10;
}

int 
_lorsRemainTime( time_t begin_time, int timeout)
{
      int lefttime = 0;
      
      if ( timeout <= 0 )   return 0;
      lefttime =  timeout - (time(0) - begin_time);
      if ( lefttime <= 0 ) 
          return (-1);
      else 
          return (lefttime);
};

int jrb_n_nodes( JRB jrb ) {
   int i = 0 ;
   JRB node;

   if ( jrb == NULL ) return 0 ;
   jrb_traverse(node,jrb){
     i++;
   };

   return (i);
};

char *set_location(char *file, char *function, int line, int tid)
{
    static char buf[128];
    snprintf(buf, 128, "%s/%s:%d [%d]", file, function, line, tid);
    return buf;

}

int _lorsHasE2E(LorsMapping *map) 
{
    ExnodeFunction *f;
    ExnodeMetadata *args;
    ExnodeValue     val;
    ExnodeType      type;

    if ( map->e2e_bs > 0 )
    {
        return 1;
    } else {
        return 0;
    }
};

int _lorsGetPhyOffAndLen(LorsMapping   *map,  
                         ulong_t  l_off,
                         ulong_t  l_len, 
                         ulong_t *phy_offset,
                         ulong_t *phy_length,
                         ulong_t *log_e2e_offset,
                         ulong_t  e2e_bs)
{
    ExnodeMetadata *args, md;
    ExnodeValue     val;
    ExnodeType      type;
    ExnodeFunction  *f;
    ulong_t          header_length = 0;
    ulong_t          blocksize = 0;
    ulong_t          physical_begin = 0, physical_end = 0;
    ulong_t          alloc_length = 0;

    f = map->function;
    exnodeGetFunctionArguments(f, &args);
    if ( exnodeGetMetadataValue(args, "blocksize", &val, &type) != EXNODE_SUCCESS)
    {
        *phy_offset = l_off;
        *phy_length = l_len;
        lorsDebugPrint(D_LORS_VERBOSE,"no 'blocksize' found: %d %d\n", *phy_offset, *phy_length);
        return LORS_SUCCESS;
    }
    blocksize = (ulong_t)val.i;

#if 1
    if ( exnodeGetMetadataValue(map->md, "alloc_length", &val, &type) != EXNODE_SUCCESS)
    {
        return LORS_FAILURE;
    }
    alloc_length = (ulong_t)val.i;

    lorsDebugPrint(D_LORS_VERBOSE, "alloc_length: %d\n", alloc_length);
#endif
    lorsDebugPrint(D_LORS_VERBOSE, "blocksize: %d\n", blocksize);
    lorsDebugPrint(D_LORS_VERBOSE, "l_off: %d, l_len: %d, e2e_bs: %d\n", 
                                    l_off,     l_len,     e2e_bs);

    if ( blocksize == alloc_length )
    {
        /* this is a fully encoded mapping. it may be compressed or otherwise,
         * but it is a special case which complicates other calculations */
        *log_e2e_offset = 0;
        *phy_offset = 0;
        *phy_length = alloc_length; 
        return LORS_SUCCESS;
    } else 
    {
        /* blocksize is likely smaller than alloc_length */
        ulong_t p_blocks, l_blocks, p_last_length, l_last_length;
        block_length_count(alloc_length, blocksize, &p_blocks, &p_last_length);
        /*block_length_count(l_len, e2e_bs, &l_blocks, &l_last_length);*/

        *log_e2e_offset = (l_off/e2e_bs)*e2e_bs;
        *phy_offset = (l_off/e2e_bs)*blocksize;
        if ( ((l_len+l_off) % e2e_bs) == 0 )
        {
            physical_end = ((l_off+l_len)/e2e_bs)*blocksize;
        } else {
            physical_end = (((l_off+l_len)/e2e_bs)+1)*blocksize;
        }
        if ( physical_end > alloc_length ) 
        {
            /*fprintf(stderr, "WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");*/
            /*fprintf(stderr, "physical_end: %ld > alloc_length: %ld : phy_offset: %ld\n",*/
                /*physical_end, alloc_length, *phy_offset);*/
            physical_end = alloc_length;
        }

        *phy_length = physical_end - *phy_offset; 
        return LORS_SUCCESS;
    }

#if 0
    /* phy_length and phy_offset must fall on blocksize boundaries */
    physical_begin = (l_off/e2e_bs)*blocksize;
    /* calculate the logical e2e_offset for adjustment in unconditioning later */
    *log_e2e_offset= (l_off/e2e_bs)*e2e_bs;

    lorsDebugPrint(D_LORS_VERBOSE, "mod: %d\n", ((l_len+l_off) % e2e_bs)); 
    lorsDebugPrint(D_LORS_VERBOSE, "sum: %d\n", (l_len+l_off)); 
    lorsDebugPrint(D_LORS_VERBOSE, "mod2: %d\n", (481%481)); 
    if ( ((l_len+l_off) % e2e_bs) == 0 )
    {
        physical_end = ((l_off+l_len)/e2e_bs)*blocksize;
        lorsDebugPrint(D_LORS_VERBOSE, "physical_end1: %d\n", physical_end);
    } else 
    {
        /* perhaps -1  is not correct? */
        physical_end = (((l_off+l_len)/e2e_bs)+1)*blocksize;
        lorsDebugPrint(D_LORS_VERBOSE, "physical_end2: %d\n", physical_end);
    }
#endif
#if 1
    /* this corrects the mis-alignment introduced by compression */
    if ( physical_end > alloc_length ) 
    {
        fprintf(stderr, "physical_end: %ld > alloc_length: %ld\n",
                physical_end, alloc_length);
        physical_end = map->exnode_offset+alloc_length;
    }
#endif
    *phy_offset = physical_begin;
    *phy_length = physical_end - physical_begin;
    lorsDebugPrint(D_LORS_VERBOSE, "Success translating offsets: %d %d\n", 
            *phy_offset, *phy_length);
    return (LORS_SUCCESS);
};

int _lorsUnconditionData ( LorsMapping *map,
                           LorsConditionStruct *lc_array,
                           char *cond_buf,
                           char *uncond_buf,
                           ulong_t src_offset,
                           ulong_t l_len,
                           ulong_t log_e2e_offset,
                           ulong_t phy_length )
{
    LorsConditionStruct lc;
    char           *src_data;
    ulong_t         src_length;
    ulong_t         src_blocksize;
    char           *dst_data;
    ulong_t         dst_length;
    ulong_t         dst_blocksize;
    int             i, ret;
    int             prev_lc = 0; /* THIS IS A HACK */
    ExnodeFunction  *f = NULL, *fsub = NULL;
    ExnodeMetadata  *args;
    ExnodeEnumeration *e;
    ExnodeValue     val;
    ExnodeType      type;
    char            *name;
    char            *v2 = NULL;


    lorsDebugPrint(D_LORS_VERBOSE, "src_offset: %d, phy_length: %d\n", 
                                    src_offset,     phy_length);
    if ( lc_array != NULL )
    {
        f = map->function;
        src_data = cond_buf;
        src_length = phy_length;

        while ( 1 )
        {
            exnodeGetFunctionName(f, &name);
            exnodeGetFunctionArguments(f, &args);
            if ( exnodeGetMetadataValue(args, "blocksize", &val, &type) != EXNODE_SUCCESS)
            {
                lorsDebugPrint(D_LORS_VERBOSE, "No known element 'blocksize'\n");
                return LORS_FAILURE;
            }
            src_blocksize = (ulong_t)val.i;

            /* find the appropriate ConditionStruct to uncondition this data */
            lc.function_name = NULL;
            for (i=0; lc_array[i].function_name != NULL;i++)
            {
                if ( strcmp(lc_array[i].function_name, name) == 0 )
                {
                    lc = lc_array[i];
                    break;
                }
            }
            if ( lc.function_name == NULL )
            {
                lorsDebugPrint(D_LORS_VERBOSE, 
                        "Unable to find matching function in lc_array[]\n");
                break;
            }


            /*fprintf(stderr, "Input: 0x%x len: %ld bs: %ld\n",*/
                    /*src_data, src_length, src_blocksize);*/

            /*fprintf(stderr, "Uncondition: %s\n", lc.function_name);*/
            ret = lc.uncondition_data(src_data, src_length, src_blocksize,
                            &dst_data, &dst_length, &dst_blocksize,
                            args);
            /*fprintf(stderr, "Output: 0x%x len: %ld bs: %ld\n",*/
                    /*dst_data, dst_length, dst_blocksize);*/
            if ( ret != LORS_SUCCESS )
            {
                /* perhaps a corrupt checksum or other unrecoverable error */
                break;
            }
            /* queue variable for next iteration */
            src_data = dst_data;
            src_length = dst_length;

            /* get next function */
            exnodeGetSubfunctions(f, &e);
            exnodeGetNextSubfunction(e, &fsub);
            if ( v2 == NULL ) 
            {
                v2 = dst_data;
            } else
            {
                /*if ( lc_array[prev_lc].allocates == 1){*/
                    free(v2);   /* after more than 1 pass, free the old memory */
                /*}*/
                v2 = dst_data;
            }
            prev_lc = i;        /* record the previous */
            if ( fsub == NULL )
            {
                lorsDebugPrint(D_LORS_VERBOSE, "memcpy3: "
                        "src_length: %ld src_offset: %ld l_len: %ld log_e2e_offset: %ld\n",
                        src_length, src_offset, l_len, log_e2e_offset); 

                /*fprintf(stderr, "l_len: %ld\n", l_len);*/
                /*getchar();*/
                /* TODO: verify that this is correct for all cases */
                /*memcpy(uncond_buf, src_data+src_offset, l_len);*/
                memcpy(uncond_buf, src_data+abs(src_offset-log_e2e_offset), l_len);
                /*
                if ( src_offset >= abs(src_length-l_len) )
                {
                    fprintf(stderr, "AAAAAAAAAAAAAAAAAA\n");
                    memcpy(uncond_buf, src_data, l_len);
                } else {
                    fprintf(stderr, "BBBBBBBBBBBBBBBBBB\n");
                    memcpy(uncond_buf, src_data+abs(src_length-l_len), l_len);
                }
                */
                free(src_data);
                return LORS_SUCCESS;
            } else 
            {
                f = fsub;
            }
        } /* end while(1) */
    }

    return (LORS_FAILURE);
};

JRB jrb_find_first_llong(JRB tree, longlong val)
{
    JRB node, node_prev;
    node = jrb_find_llong(tree, val);
    if ( node == NULL ) return NULL;

    do {
        node_prev = node;
        node = jrb_prev(node_prev);
    } while ( node->key.ll == val );
    return node_prev;
}
int _lorsGetDepotInfo(struct ibp_depot *depot, IBP_cap cap)
{
    char port[256];
    char *tmp;

    if (cap == NULL) {
        lorsDebugPrint(D_LORS_VERBOSE, "Invalid IBP cap\n");
        return -1;
    }

    if (depot == NULL) {
        lorsDebugPrint(D_LORS_VERBOSE, "Invalid ibp_depot struct\n");
            fflush( stderr );
        return -1;
    }

    tmp = strstr(cap, "://");
    strcpy (depot->host, tmp + 3);
    depot->host[strchr(depot->host, ':') - depot->host] = '\0';

    strcpy (port, tmp + 3 + strlen( depot->host ) + 1);
    port[strchr(port, '/') - port] = '\0';
    depot->port = atoi( port );

    return 1;
}

/* takes a string of format '1000M' or '50 K' or even '0.5G' and translates it
 * to bytes.  must fit within an ulong_t, otherwise an error is returned */
int strtobytes(char *str, ulong_t *ret_bytes)
{
    char *s;
    double bs;
    char    *accept="0123456789GMKgmk. ";
    int     len;
    sscanf(str, "%lf", &bs);
    len = strspn(str, accept);
    if ( len < strlen(str) )
    {
        lorsDebugPrint(D_LORS_VERBOSE,"Improper string format.  <Integer><Space*><Identifier*>\n");
        return -1;
    }
    while (1) /* goto without labels >:] */
    {
        s = strchr(str, 'G');
        if ( s != NULL )
        {
            bs *= 1024*1024*1024;
            break;
        } 
        s = strchr(str, 'g');
        if ( s != NULL )
        {
            bs *= 1024*1024*1024;
            break;
        } 
        s = strchr(str, 'M');
        if ( s != NULL )
        {
            bs *= 1024*1024;
            break;
        } 
        s = strchr(str, 'm');
        if ( s != NULL )
        {
            bs *= 1024*1024;
            break;
        } 
        s = strchr(str, 'K');
        if ( s != NULL )
        {
            bs *= 1024;
            break;
        }
        s = strchr(str, 'k');
        if ( s != NULL )
        {
            bs *= 1024;
            break;
        }
        break;
    }
    if ( bs > ((ulong_t)(-1)) ) return -1;
    *ret_bytes = (ulong_t)bs;
    return 0;
}
int strtotimeout(char *str, time_t *ret_timeout)
{
    char *s;
    double bs;
    sscanf(str, "%lf", &bs);
    while (1) /* goto without labels >:] */
    {
        s = strchr(str, 'd');
        if ( s != NULL )
        {
            bs *= 60*60*24;
            break;
        } 
        s = strchr(str, 'D');
        if ( s != NULL )
        {
            bs *= 60*60*24;
            break;
        } 
        s = strchr(str, 'h');
        if ( s != NULL )
        {
            bs *= 60*60;
            break;
        } 
        s = strchr(str, 'H');
        if ( s != NULL )
        {
            bs *= 60*60;
            break;
        } 
        s = strchr(str, 'm');
        if ( s != NULL )
        {
            bs *= 60;
            break;
        }
        s = strchr(str, 'M');
        if ( s != NULL )
        {
            bs *= 60;
            break;
        }
        s = strchr(str, 's');
        if ( s != NULL )
        {
            break;
        }
        s = strchr(str, 'S');
        if ( s != NULL )
        {
            break;
        }

        /*bs *= 60*60*24;*/
        break;
    }

    if ( bs > INT_MAX ) return -1;
    *ret_timeout = (time_t)bs;
    return 0;
}

IBP_depot lors_strtodepot(char *hostname)
{
    char *s;
    IBP_depot ibpdepot;
    ibpdepot = (IBP_depot) malloc(sizeof(struct ibp_depot));
    if ( ibpdepot == NULL ) return NULL;

    s = strchr(hostname, ':');
    if ( s != NULL )
    {
        ibpdepot->port = atoi(s+1);
        *s = '\0';
    } else
    {
        ibpdepot->port = 6714;
    }
    strncpy(ibpdepot->host, hostname, 255);

    return ibpdepot;
}

