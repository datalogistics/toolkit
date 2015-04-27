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


/*typedef void *ExnodeMetadata;*/
/*typedef void *ExnodeMetadataEnum;*/

int lorsMetadataMerge(ExnodeMetadata *src, ExnodeMetadata *dest)
{
    ExnodeEnumeration  *ep;
    char               *name;
    int                 ret;
    ExnodeValue         val;
    ExnodeType          type;

    memset(&val, 0, sizeof(ExnodeValue));
    ret = exnodeGetMetadataNames(src, &ep);
    if ( ret != EXNODE_SUCCESS )
    {
        return LORS_FAILURE;
    }
    do 
    {
        exnodeGetNextName(ep, &name);
        if ( name == NULL )
        {
            break;
        }
        /* TODO: he had better have recursive set */
        exnodeGetMetadataValue(src, name, &val, &type);
        exnodeSetMetadataValue(dest, name, val, type, 0);
        if ( type == STRING) 
        { 
            free(val.s);
        }

        free(name);

    } while ( name != NULL );

    exnodeDestroyEnumeration(ep);
    return LORS_SUCCESS;
}

#if 0
int lorsMetadataDestroy(ExnodeMetadata md)
{
    JRB node;
    char *s;
    if ( md == NULL ) return LORS_INVALID_PARAMETERS;
    jrb_traverse(node, md)
    {
        /* foreach node free value */
        s = node->key.s;
        lorsMetadataRemove(md, s);
    }
    jrb_free_tree(md);
    return LORS_SUCCESS;
}

int lorsMetadataAdd (ExnodeMetadata md, char *name, LorsMetaValue val)
{
    Jval    j;
    JRB     newMd, md_node = NULL;
    int     ret;
    ExnodeMetadata    md_dup;
    LorsMetaValue   *mdv;
    if ( md == NULL || name == NULL ) return LORS_INVALID_PARAMETERS;

    mdv = new_mdv();
    if ( mdv == NULL ) return LORS_NO_MEMORY;
    switch (val.type)
    {
        case LORS_MDTYPE_DOUBLE:
        case LORS_MDTYPE_INT:
            mdv->type = val.type;
            mdv->val = val.val;
            break;
        case LORS_MDTYPE_STRING:
            mdv->type = val.type;
            mdv->val.s = strdup(val.val.s);
            if ( mdv->val.s == NULL ) return LORS_NO_MEMORY;
            break;
        case LORS_MDTYPE_METADATA:
            ret = lorsMetadataCreate(&md_dup);
            if ( ret != LORS_SUCCESS ) return ret;
            jrb_traverse(md_node, md)
            {
                ret = lorsMetadataAdd(md_node, md_node->key.s, 
                                        *(LorsMetaValue *)md_node->val.v);
            }
            mdv->type = LORS_MDTYPE_METADATA;
            mdv->val.m = md_dup;
            break;
        default:
            free(mdv);
            return LORS_INVALID_PARAMETERS;
            break;

    }
    j.v = (void *)mdv;
    jrb_insert_str(md, strdup(name), j);
    return LORS_SUCCESS;
}
int lorsMetadataGet (ExnodeMetadata md, char *name, LorsMetaValue *val)
{
    JRB node;
    LorsMetaValue *mdv;
    node = jrb_find_str(md, name);
    if ( node == NULL ) return LORS_NOTFOUND;
    mdv = (LorsMetaValue *)(node->val.v);
    *val = *mdv;
    return LORS_SUCCESS;
}
int lorsMetadataRemove (ExnodeMetadata md, char *name)
{
    Jval    j;
    JRB     md_node = NULL, md_val = NULL, md_tree;
    int     ret;
    ExnodeMetadata    md_dup;
    LorsMetaValue   *mdv;
    if ( md == NULL || name == NULL ) return LORS_INVALID_PARAMETERS;

    md_val = jrb_find_str(md, name);
    if ( md_node == NULL ) return LORS_NOTFOUND;

    free(md_val->key.s);
    mdv = (LorsMetaValue *)(md_val->val.v);
    switch (mdv->type)
    {
        case LORS_MDTYPE_METADATA:
            md_tree = mdv->val.m;
            jrb_traverse(md_node, md_tree)
            {
                ret = lorsMetadataRemove(md_tree, md_node->key.s );
                        /*, (LorsMetaValue *)md_node->val.v);*/
            }
            jrb_free_tree(md_tree);
            jrb_delete_node(md_val);
            break;
        case LORS_MDTYPE_STRING:
            free (mdv->val.s);
            /* fall through */
        case LORS_MDTYPE_DOUBLE:
        case LORS_MDTYPE_INT:
            jrb_delete_node(md_val);
            break;
        default:
            return LORS_INVALID_PARAMETERS;

    }
    free(mdv);
    return LORS_SUCCESS;
}

int lorsMetadataKeys (ExnodeMetadata md, ExnodeMetadataEnum *keys)
{
    if ( md == NULL || keys == NULL ) return LORS_INVALID_PARAMETERS;
    *keys = md;
    return LORS_SUCCESS;
}
char *lorsMetadataEnumNext (ExnodeMetadataEnum keys, ExnodeMetadataEnum *iterator)
{
    if ( keys == NULL || iterator == NULL ) return NULL;
    *iterator = jrb_next((*iterator));
    if ( keys == *iterator ) return NULL;

    return (*iterator)->key.s;
}
int lorsMetadataEnumFree (ExnodeMetadataEnum iterator)
{
    /* there is nothing to free */
    return LORS_SUCCESS;
}
#endif

int lorsGetExnodeMetadata (LorsExnode *exnode, ExnodeMetadata **md)
{
    if ( exnode == NULL || md == NULL ) return LORS_INVALID_PARAMETERS;
    if ( exnode->md == NULL )
    {
        /*lorsMetadataCreate(& exnode->md);*/
        exnodeCreateMetadata(&exnode->md);
    } 
    *md = exnode->md;
    return LORS_SUCCESS;
}
int lorsGetMappingMetadata (LorsMapping *map, ExnodeMetadata **md)
{
    if ( map == NULL || md == NULL ) return LORS_INVALID_PARAMETERS;
    *md = map->md;
    return LORS_SUCCESS;
}
int    lorsEnumNext (LorsEnum list, LorsEnum *iterator, LorsMapping **ret)
{
    Dllist dlnode = NULL;
    int  ret_val = 0;
    if ( list == NULL || ret == NULL || iterator == NULL ) 
        return LORS_INVALID_PARAMETERS;
    if ( *iterator == NULL ) *iterator = list;

    dlnode = *iterator;
    dlnode = dll_next(dlnode);
    *iterator = dlnode;
    if ( list == dlnode )
    {
        *ret = NULL;
        ret_val = LORS_END;
    } else 
    {
        *ret = dlnode->val.v;
        ret_val = LORS_SUCCESS;
    }
    return ret_val;
}
int    lorsEnumFree (LorsEnum list)
{
    free_dllist(list);
    return LORS_SUCCESS;
}

