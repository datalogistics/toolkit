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

int    lorsSetFree(LorsSet *set, int opts)
{
    JRB node;
    LorsMapping *lm;
    if ( opts & LORS_FREE_MAPPINGS )
    {
        jrb_traverse(node, set->mapping_map)
        {
            lm = (LorsMapping *)node->val.v;
            if ( lm->capset.readCap != NULL){
                free(lm->capset.readCap);
                lm->capset.readCap = NULL;
            };
            if ( lm->capset.writeCap != NULL){
                free(lm->capset.writeCap);
                lm->capset.writeCap = NULL;
            };
            if ( lm->capset.manageCap != NULL){
                free(lm->capset.manageCap);
                lm->capset.writeCap = NULL;
            };
            free(lm);
        }
    }
    /*free_dllist(set->boundary_list);*/
    if ( set->mapping_map != NULL){
        jrb_free_tree(set->mapping_map);
        set->mapping_map = NULL;
    };
    free(set);
    return LORS_SUCCESS;
}
/***********************************************************************
 *                       Primitive lors functions.
 ***********************************************************************/
int    lorsSetCreateEmpty(LorsSet **set)
{
    LorsSet *s;
    s = _lorsMallocSet();
    s->condition_list = NULL;
    s->max_length = 0;
    s->exnode_offset = 0;
    s->data_blocksize = 0;
    s->copies = 0;
    *set = s;

    return LORS_SUCCESS;
}

/* is 'offset' needed? */
int    lorsSetInit(LorsSet **set, 
                   ulong_t   data_blocksize,
                   int       copies,
                   int       opts)
{
    LorsSet             *l_set;
    int                  i;

    l_set = _lorsMallocSet();
    if ( l_set == NULL ) return LORS_NO_MEMORY;

    l_set->max_length = 0;
    l_set->exnode_offset = 0;
    l_set->data_blocksize = data_blocksize;
    l_set->copies = copies;

    *set = l_set;

    return LORS_SUCCESS;
}

longlong lorsSetFindMaxlength(LorsSet *set)
{
    JRB     node;
    LorsMapping *lm;
    longlong        length = 0;
    jrb_traverse(node, set->mapping_map)
    {
        lm = node->val.v;
        if ( lm->exnode_offset + lm->logical_length > length )
        {
            length = lm->exnode_offset + lm->logical_length;
        }
    }
    set->max_length = length;
    return set->max_length;
}

int    lorsSetAddMapping (LorsSet     *set, 
                          LorsMapping *map)
{
    JRB         node, dst_node;
    int         ret = LORS_SUCCESS;

    /* find any node in dest with equal offset */
    dst_node = jrb_find_first_llong(set->mapping_map, map->exnode_offset);
    if ( dst_node == NULL )
    {
        /* add to tree */
        jrb_insert_llong(set->mapping_map, map->exnode_offset, new_jval_v(map));
        if ( map->exnode_offset+map->logical_length > set->max_length )
        {
            set->max_length = map->exnode_offset+map->logical_length;
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
            jrb_insert_llong(set->mapping_map, map->exnode_offset, new_jval_v(map));
            if ( map->exnode_offset+map->logical_length > set->max_length )
            {
                set->max_length = map->exnode_offset+map->logical_length;
            }
        } else 
        {
            ret = LORS_EXISTS;
        }
    }
    return ret;
}
int lorsSetRemoveMapping (LorsSet *set, LorsMapping *map)
{
    JRB         node, dst_node;
    int         ret = LORS_SUCCESS;

    /* find any node in dest with equal offset */
    dst_node = jrb_find_first_llong(set->mapping_map, map->exnode_offset);
    if ( dst_node == NULL )
    {
        ret = LORS_NOTFOUND;
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
            ret = LORS_NOTFOUND;
        } else 
        {
            jrb_delete_node(dst_node);
        }
    }
    return ret;
};


int     lorsSetTrimCaps(LorsSet *set, int which)
{
    LorsMapping        *lm        = NULL;
    LorsEnum        l_enum        = NULL, l_iter = NULL;

    if ( set == NULL )
    {
        return LORS_INVALID_PARAMETERS;
    }

    lorsSetEnum(set, &l_enum);

    while ( lorsEnumNext(l_enum, &l_iter, &lm) == LORS_SUCCESS )
    {
        if (which & LORS_READ_CAP)
        {
            if (lm->capset.readCap != NULL)
            {
                free(lm->capset.readCap);
                lm->capset.readCap = NULL;
            }
        }
        if (which & LORS_WRITE_CAP)
        {
            if (lm->capset.writeCap != NULL)
            {
                free(lm->capset.writeCap);
                lm->capset.writeCap = NULL;
            }
        }
        if (which & LORS_MANAGE_CAP)
        {
            if (lm->capset.manageCap != NULL)
            {
                free(lm->capset.manageCap);
                lm->capset.manageCap = NULL;
            }
        }
    }
    lorsEnumFree(l_enum);

    return 0;
}


