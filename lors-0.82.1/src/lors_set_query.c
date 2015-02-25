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

int lorsSetEnum(LorsSet *set, LorsEnum *list)
{
    Dllist xdl = NULL;
    JRB     node = NULL;
    if ( set == NULL || list == NULL )
    {
        return LORS_INVALID_PARAMETERS;
    }
    xdl = new_dllist();
    jrb_rtraverse(node, set->mapping_map)
    {
        dll_append(xdl,node->val); 
    }
    *list = xdl;
    return LORS_SUCCESS;
}


/* if opt == LORS_QUERY_REMOVE, then the mappings which match, are removed
 * from the exnode, and placed into the set.  this is useful for operations
 * such as SetTrim where the resulting mappings would be stale if left within
 * the exnode.
 */
int    lorsQuery ( LorsExnode *exnode,
                   LorsSet    **set,
                   longlong   offset,
                   longlong   length ,
                   int          opts) 
{
        int   ret = LORS_SUCCESS;
        LorsMapping *lm;
        JRB   jrb_node = NULL, node_prev = NULL;
        LorsSet *new_set = NULL;

        if ((exnode == NULL ) ||
            ( set == NULL )   ||
            (offset < 0 )     ||
            (length < 0 ) ){
            ret = LORS_INVALID_PARAMETERS;
            goto bail;
        };

        if (( new_set = _lorsMallocSet()) == NULL ){
            ret = LORS_NO_MEMORY;
            goto bail;
        };

        jrb_rtraverse(jrb_node,exnode->mapping_map)
        {
             lm = (LorsMapping *)jval_v(jrb_node->val);
             if ( length > 0){
                 if ( (length + offset) <= lm->exnode_offset)
                    continue;
             };
             if ((lm->exnode_offset + lm->logical_length) <= offset )
                    continue;
             jrb_insert_llong(new_set->mapping_map,lm->exnode_offset,new_jval_v(lm));
             if ( lm->exnode_offset+lm->logical_length > new_set->max_length )
             {
                 new_set->max_length = lm->exnode_offset+lm->logical_length;
             }
             if ( opts & LORS_QUERY_REMOVE )
             {
                 /* TODO: verify that this is correct */
                 node_prev = jrb_next(jrb_node);
                 jrb_delete_node(jrb_node);
                 jrb_node = node_prev;
             }
        };

        *set = new_set;

bail:
        return(ret);
};

