#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <lors_api.h>
#include <lors_error.h>
#include <lors_opts.h>
#include <lors_util.h>
#include <jval.h>
#include <jrb.h>
#include <string.h>


void lorsGetLibraryVersion(const char **package, const double *version)
{
    const char *pack, *ver;
    pack = PACKAGE;
    ver = VERSION;
    if ( version != NULL )
    {
        sscanf(ver, "%lf", version);
    }
    if ( package != NULL ) 
    {
        *package = pack;
    }
    return;
}

int    lorsExnodeFree(LorsExnode *exnode)
{
    JRB   jrb_node;

    /*if ( exnode == NULL ) return LORS_INVALID_PARAMETERS;*/
    if ( exnode != NULL ){
        jrb_traverse(jrb_node,exnode->mapping_map){
             /*free(jrb_node->val.v);    */
            _lorsFreeMapping((LorsMapping*)jrb_node->val.v);
            jrb_node->val.v = NULL;
        };
        jrb_free_tree(exnode->mapping_map);
        exnode->mapping_map = NULL;
        if ( exnode->exnode != NULL )
        {
            exnodeDestroyExnode(exnode->exnode);
            exnode->exnode = NULL;
        }
        if ( exnode->md != NULL ) {
            exnodeDestroyMetadata(exnode->md);
            exnode->md = NULL;
        };
        free(exnode);
    };
}
int    lorsExnodeCreate (LorsExnode ** exnode)
{
    LorsExnode  *tmpx;
    tmpx = (LorsExnode *)calloc(1, sizeof (struct __LorsExnode));
    if ( tmpx == NULL ) return LORS_NO_MEMORY;
    tmpx->logical_length = 0;
    tmpx->seek_offset = 0;
    tmpx->alloc_length = 0;
    tmpx->md = NULL;
    tmpx->mapping_map = make_jrb();
    *exnode = tmpx;
    /* initialize */
    /* pthread lock */
    /* dllist */
    /* set_map */
    return LORS_SUCCESS;
}
int    lorsAppendSet (LorsExnode * exnode,
                         LorsSet * set)
{
    JRB     node = NULL, node_next = NULL;
    JRB     dst_node = NULL;
    longlong   len;
    LorsMapping *lm;

    /* 
     * merge set->mapping_map into exnode->mapping_map.
     */

    jrb_traverse(node, set->mapping_map)
    {
        /* find any node in exnode with equal offset */
        lm = (LorsMapping *)node->val.v;
        lorsAppendMapping(exnode, lm);
    }

    return LORS_SUCCESS;
}
int    lorsAppendMapping (LorsExnode * exnode,
                         LorsMapping * map)
{
    JRB     node = NULL, node_next = NULL;
    JRB     dst_node = NULL;

    dst_node = jrb_find_first_llong(exnode->mapping_map, map->exnode_offset);
    if ( dst_node == NULL )
    {
        /* add to tree */
        jrb_insert_llong(exnode->mapping_map, map->exnode_offset, new_jval_v(map));
        if ( map->exnode_offset + map->logical_length > exnode->logical_length )
        {
            exnode->logical_length = map->exnode_offset + map->logical_length;
        }
    }
    else 
    {
        /* while the offsets match and the pointers are not equal. */
        /* TODO: what happnes at the END of the JRB list?? */
        while ( map->exnode_offset == dst_node->key.ll &&
                map != dst_node->val.v )
        {
            dst_node = jrb_next(dst_node);
        }
        /* after finding all, only if the offsets do not match, add it. */
        /* if the offsets match, it means a similar pointer was found */
        if ( map->exnode_offset != dst_node->key.ll )
        {
            jrb_insert_llong(exnode->mapping_map, map->exnode_offset, new_jval_v(map));
            if ( map->exnode_offset + map->logical_length > exnode->logical_length )
            {
                exnode->logical_length = map->exnode_offset + map->logical_length;
            }
        }

    }
    return LORS_SUCCESS;
}
int    lorsDetachMapping (LorsExnode *exnode, LorsMapping *map)
{
    JRB jrb_node;
    LorsMapping *lm;
    int found = 0;
    jrb_traverse(jrb_node, exnode->mapping_map)
    {
        lm = jrb_node->val.v;
        if ( map == lm )
        {
            jrb_delete_node(jrb_node);
            found = 1;
            break;
        }
    }
    if ( found )
    {
        return LORS_SUCCESS;
    } else 
    {
        return LORS_NOTFOUND;
    }
}

/* perhaps it is not necessary right away to 'concat' fragments after a
 * 'detach' operation.. such that all boundary points created will remain. 
 */
int    lorsDetachSet (LorsExnode * exnode,
                         LorsSet * set)
{
    JRB     jrb_node;
    Dllist   dll_item,  dll_set_at_offset;
    LorsSet *se;

    /* TODO: IS THIS USEFUL?? */
#if 0
    /*foreach boundary*/
    jrb_traverse(jrb_node, exnode->mapping_map)
    {
        dll_set_at_offset = (Dllist)jrb_node->val.v;
        dll_traverse(dll_item, dll_set_at_offset)
        {
            /*check for the presence of 'set'.*/
            se = (LorsSet *)(dll_item->val.v);
            if ( se == set )
            {
                /*remove 'set' from dll_set_at_key list.*/
                dll_delete_node(dll_item);
                break;
            }
        }
    }
#endif
    return LORS_FAILURE;
}
int    lorsExnodeEnum  (LorsExnode *exnode,
                        LorsEnum   *list)
{
    Dllist xdl = NULL;
    JRB     node = NULL;
    if ( exnode == NULL || list == NULL )
    {
        return LORS_INVALID_PARAMETERS;
    }
    xdl = new_dllist();
    jrb_rtraverse(node, exnode->mapping_map)
    {
        dll_append(xdl,node->val); 
    }
    *list = xdl;
    return LORS_SUCCESS;
}
int    lorsExnodeQuery (LorsExnode * exnode,
                        LorsEnum  ** setList,
                        longlong offset,
                        int opts)
{
    JRB node;
    
#if 0
    /*find set_list_at_key.*/
    node = jrb_find_gen(exnode->mapping_map, new_jval_ll(offset), ll_jvalcmp);
    if ( node == NULL ) 
    {
        *setList = NULL;
        return LORS_FAILURE;
    }

    /* insert list into setList while applying 'opts' */
    *setList = (LorsEnum *)node->val.v;
#endif
    return LORS_FAILURE;
}
int    lorsQueryCodingBlocks (LorsExnode * exnode,
                              LorsEnum ** setList,
                              longlong offset,
                              int opts)
{
    return LORS_FAILURE;
}
int    lorsAppendCodingBlock (LorsExnode * exnode,
                              LorsSet *codingblock)
{
    return LORS_FAILURE;
}

