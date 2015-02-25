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
int    lorsSetMerge (LorsSet * src,
                     LorsSet * dest)
{
    JRB             node = NULL, 
                    node_next = NULL,
                    dst_node = NULL;

    JRB             ptr_tree = NULL, 
                    dst_tree = NULL;
    Dllist          dl = NULL, 
                    dl_next = NULL,
                    dl_node = NULL;

    longlong        max_offset = 0;
    LorsMapping     *lm;

    jrb_traverse(node, src->mapping_map)
    {
        lorsSetAddMapping(dest, node->val.v);
    }
    return LORS_SUCCESS;
#if 0
        /* find any node in dest with equal offset */
        dst_node = jrb_find_first_llong(dest->mapping_map, node->key.ll);
        if ( dst_node == NULL )
        {
            /* add to tree */
            jrb_insert_llong(dest->mapping_map, node->key.ll, node->val);
        }
        else 
        {
            /* while the offsets match and the pointers are not equal. */
            /* TODO: what happnes at the END of the JRB list?? */
            while ( node->key.ll == dst_node->key.ll &&
                    node->val.v != dst_node->val.v )
            {
                dst_node = jrb_next(dst_node);
            }
            /* after finding all, only if the offsets do not match, add it. */
            /* if the offsets match, it means a similar pointer was found */
            if ( node->key.ll != dst_node->key.ll )
            {
                jrb_insert_llong(dest->mapping_map, node->key.ll, node->val);
            }

        }
#endif
}


