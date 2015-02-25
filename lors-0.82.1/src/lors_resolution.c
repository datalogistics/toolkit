#include <lors_api.h>
#include <fields.h>
#include <lors_resolution.h>
#include <math.h>

LboneResolutionIndex *newLboneResolutionIndex(int src, int dst)
{
    LboneResolutionIndex *li;
    li = (LboneResolutionIndex *)calloc(1, sizeof(LboneResolutionIndex));
    if ( li == NULL ) return li;

    li->sourceIndex = src;
    li->destIndex = dst;
    return li;
}

JRB jrb_insert_depot(JRB tree, IBP_depot key, Jval val)
{
    return jrb_insert_gen(tree, new_jval_v(key), val, depot_cmp);
}
JRB jrb_find_depot(JRB root, IBP_depot key)
{
    return jrb_find_gen(root, new_jval_v(key), depot_cmp);
}
int depot_cmp(Jval aa, Jval bb)
{
    int i;
    IBP_depot a, b;
    a = aa.v;
    b = bb.v;

    i = strcmp(a->host, b->host);
    if ( i == 0 )
    {
        if ( a->port > b->port )
        {
            i = 1;
        } else if ( a->port == b->port )
        {
            i=0;
        } else 
        {
            i=-1;
        }
    }
    return i;
}

/* TODO: it may be more desireable to have this function take the src_cnt and
 * dst_cnt and allocate resolution, makeing it necessar for the user to know
 * this in advance... */
/*int lorsCreateResolution(LboneResolution **lr)*/
int lorsCreateResolution(LboneResolution **lr, int src_cnt, int dst_cnt)
{
    LboneResolution *l_lr;
    int     i;

    l_lr = (LboneResolution *)calloc(1, sizeof(LboneResolution));

#if 0
    l_lr->resolution = NULL;
    l_lr->src_cnt = -1;
    l_lr->dst_cnt = -1;
    l_lr->depotIndex = make_jrb();
#endif
    l_lr->resolution = calloc(src_cnt, sizeof(double *));
    for (i=0; i < src_cnt; i++)
    {
        l_lr->resolution[i] = calloc(dst_cnt, sizeof(double));
    }
    l_lr->src_cnt = src_cnt;
    l_lr->dst_cnt = dst_cnt;
    l_lr->depotIndex = NULL;

    *lr = l_lr;
    return LORS_SUCCESS;
}


/* this may or may not make an lbone call depending on opts, in which case
 * nthreads and timeout will be ignored.. */

int lorsCreateResolutionFromLists(LboneResolution **lr, 
                                  IBP_depot lbone_server,
                                  IBP_depot *src_list, 
                                  IBP_depot *dst_list, 
                                  int       nthreads,
                                  int       timeout,
                                  int       opts)
{
    IBP_depot           d;
    LboneResolution     *l_lr;
    int                 src_cnt = 1, dst_cnt = 1;
    int                 i, j;
    JRB                 tree, node;
    LboneResolutionIndex   *li;
    int                 ret;
    double              **res;


    tree = make_jrb();
    for (i=0; src_list[i] != NULL; i++)
    {
        node = jrb_find_depot(tree, src_list[i]);
        if ( node == NULL )
        {
            d = new_ibp_depot(src_list[i]->host, src_list[i]->port);
            if ( strcmp(d->host, "localhost") == 0 )
            {
                li = newLboneResolutionIndex(0, -1);
            }else {
                li = newLboneResolutionIndex(src_cnt, -1); 
            }
            jrb_insert_depot(tree, d, new_jval_v(li));
            if ( strcmp(d->host, "localhost") != 0 )
            {
                src_cnt++;
            }
        } else 
        {
            /* already exists as a source depot. */
            li = node->val.v;
            if ( li->sourceIndex < 0 )
            {
                if ( strcmp(((IBP_depot)(node->key.v))->host, "localhost") == 0 )
                {
                    li->sourceIndex = 0;
                }else {
                    li->sourceIndex = src_cnt;
                    src_cnt++;
                }
            }
            continue;
        }
    }
    for (i=0; dst_list[i] != NULL; i++)
    {
        node = jrb_find_depot(tree, dst_list[i]);
        if ( node == NULL )
        {
            d = new_ibp_depot(dst_list[i]->host, dst_list[i]->port);
            if ( strcmp(d->host, "localhost") == 0 )
            {
                li = newLboneResolutionIndex(-1, 0);
            }else {
                li = newLboneResolutionIndex(-1, dst_cnt); 
            }
            jrb_insert_depot(tree, d, new_jval_v(li));

            if ( strcmp(d->host, "localhost") != 0 )
            {
                dst_cnt++;
            }
        } else
        {
            li = node->val.v;
            if ( li->destIndex < 0 )
            {
                if ( strcmp(((IBP_depot)(node->key.v))->host, "localhost") == 0 )
                {
                    li->destIndex = 0;
                } else {
                    li->destIndex = dst_cnt;
                    dst_cnt++;
                }
            }
        }
    }
    d = new_ibp_depot("localhost", 6714);
    if ( jrb_find_depot(tree, d) == NULL )
    {
        li = newLboneResolutionIndex(0, 0);
        jrb_insert_depot(tree, d, new_jval_v(li));
    }

    ret = lorsCreateResolution(&l_lr, src_cnt, dst_cnt);
    if ( ret != LORS_SUCCESS ) { return ret; }
    l_lr->depotIndex = tree;

    ret = lbone_getDepot2DepotResolution(&res, lbone_server, src_list, 
                                            dst_list, opts, timeout);
    if ( ret != 0 ) return ret + LORS_LBONE_FAILED;

    /* Copy res => LboneResolution structure. */
    for (i=0; src_list[i]; i++)
    {
        for(j=0; dst_list[j]; j++)
        {
            double met;
            lorsSetResolutionPoint(l_lr, src_list[i], dst_list[j], res[i][j]);
            met = -2;
            lorsGetResolutionPoint(l_lr, src_list[i], dst_list[j], &met);
        }
    }

    free(res);

    *lr = l_lr;
    return LORS_SUCCESS;

}


/* Metric is 0 for source, 1 for destination. */
int lorsCreateDepotListFromFile(char *filename, IBP_depot **dpt_list, int metric)
{
    IS                 is;
    int                depot_cnt = 0;
    int                i, j;
    IBP_depot          depot, d, *ret_list;
    JRB                j_node, j_list;

    if ( metric > 1 || metric < 0 )
    {
        return LORS_INVALID_PARAMETERS;
    }
    *dpt_list = NULL;

    j_list = make_jrb();
    is = new_inputstruct(filename);
    if ( is == NULL ) return LORS_FILE_PERMISSIONS;
    while ( get_line(is) >= 0 )
    {
        if ( is->NF < 3 ) continue;
        if ( is->text1[0] == '#' ) continue;

        if ( strcmp(is->fields[metric], "localhost") != 0 )
        {
            d = lors_strtodepot(is->fields[metric]);
            assert(d!=NULL);

            j_node = jrb_find_depot(j_list, d);
            if ( j_node == NULL )
            {
                jrb_insert_depot(j_list, d, new_jval_v(d));
                depot_cnt++;
            } else 
            {
                free(d);
            }
        }
    }

    ret_list = (IBP_depot *)calloc(depot_cnt+1, sizeof(IBP_depot));
    assert(ret_list != NULL);
    i=0;
    jrb_traverse(j_node, j_list)
    {
        ret_list[i] = j_node->val.v;
        i++;
    }
    ret_list[i] = NULL;

    jettison_inputstruct(is);
    *dpt_list = ret_list;

    return LORS_SUCCESS;
}
/* 
 * it seems moderately appropriate that we may wish to use xml to store and
 * retrieve this resolution cache.  in this way we would know in advance how
 * large the resolution matrix would be and make deserialization much easier. 
 *
 * perhaps libexnode could be used for this?
 */
int lorsCreateResolutionFromFile(char *filename, 
                                 LboneResolution **lr, 
                                 int metric_index)
{
    /* how do I keep track of the src/dest metric data? */
    /* one possiblity is to loop through the file twice. */
    /*     this simplifies memory management. by reducing the structures to
     *     those which will be present in the end. */
    IS                 is;
    int                src_cnt = 1;
    int                dst_cnt = 1;
    int                i, j;
    IBP_depot          src_depot, dst_depot, d;
    JRB                 node, tree;
    LboneResolutionIndex *index;
    double              metric;

    /* do i want the depots in string format or in the IBP_depot mode? */
    /* if ibp_depot i need to write custom jrb compare and gen() routines. */

    tree = make_jrb();
    is = new_inputstruct(filename);
    if ( is == NULL ) 
    {
        jrb_free_tree(tree);
        return LORS_FILE_PERMISSIONS;
    }

    while ( get_line(is) >= 0 )
    {
        if ( is->NF < 3 ) continue;
        if ( is->text1[0] == '#' ) continue;
        /* TODO: if depot == "localhost" special case */
        src_depot = lors_strtodepot(is->fields[0]);
        dst_depot = lors_strtodepot(is->fields[1]);


        node = jrb_find_depot(tree, src_depot);
        if ( node == NULL )
        {
            if ( strcmp(src_depot->host, "localhost") == 0 )
            {
                index = newLboneResolutionIndex(0, -1);
            } else 
            {
                index = newLboneResolutionIndex(src_cnt, -1);
            }
            jrb_insert_depot(tree, src_depot, new_jval_v(index));
            if ( strcmp(src_depot->host, "localhost") != 0 )
            {
                src_cnt++;
            }
        } else 
        {
            /* already registered as a source depot */
            index = node->val.v;
            if ( index->sourceIndex < 0 )
            {
                if ( strcmp(src_depot->host, "localhost") == 0 )
                {
                    index->sourceIndex = 0;
                }else {
                    index->sourceIndex = src_cnt;
                    src_cnt++;
                }
            }
            free(src_depot);
        }

        node = jrb_find_depot(tree, dst_depot);
        if ( node == NULL )
        {
            if ( strcmp(dst_depot->host, "localhost") == 0 )
            {
                index = newLboneResolutionIndex(-1, 0);
            } else {
                index = newLboneResolutionIndex(-1, dst_cnt);
            }
            jrb_insert_depot(tree, dst_depot, new_jval_v(index));
            if ( strcmp(dst_depot->host, "localhost") != 0 )
            {
                dst_cnt++;
            }
        } else 
        {
            index = node->val.v;
            if ( index->destIndex < 0 )
            {
                if ( strcmp(dst_depot->host, "localhost") == 0 )
                {
                    index->destIndex = 0;
                } else 
                {
                    index->destIndex = dst_cnt;
                    dst_cnt++;
                }
            }
            free(dst_depot);
        }
    }
    jettison_inputstruct(is);
    d = new_ibp_depot("localhost", 6714);
    if ( jrb_find_depot(tree, d) == NULL )
    {
        index= newLboneResolutionIndex(0, 0);
        jrb_insert_depot(tree, d, new_jval_v(index));
    } else {
        free(d);
    }


    lorsCreateResolution(lr, src_cnt, dst_cnt);
    (*lr)->depotIndex = tree;

    is = new_inputstruct(filename);
    if ( is == NULL ) {
        jrb_free_tree(tree);
        return LORS_FILE_PERMISSIONS;
    }

    while ( get_line(is) >= 0 )
    {
        if ( is->NF < 3 ) continue;
        if ( is->text1[0] == '#' ) continue;
        src_depot = lors_strtodepot(is->fields[0]);
        dst_depot = lors_strtodepot(is->fields[1]);
        
        if ( is->NF > 2 + metric_index )
        {
            metric = atof(is->fields[2+metric_index]);
        } else 
        {
            metric = 0;
        }

#if 0
        node = jrb_find_depot((*lr)->depotIndex, src_depot);
        if ( node == NULL ) { goto while_end; }
        index = node->val.v;
        i = index->sourceIndex;

        node = jrb_find_depot((*lr)->depotIndex, dst_depot);
        if ( node == NULL ) { goto while_end; }
        index = node->val.v;
        j = index->destIndex;
#endif

        lorsSetResolutionPoint(*lr, src_depot, dst_depot, metric);

        /*(*lr)->resolution[i][j] = metric;*/

while_end:
        free(src_depot);
        free(dst_depot);
    }
    (*lr)->src_cnt = src_cnt;
    (*lr)->dst_cnt = dst_cnt;

    jettison_inputstruct(is);
    return LORS_SUCCESS;
}

int lorsGetResolutionPoint(LboneResolution *lr, 
                           IBP_depot src, 
                           IBP_depot dst, 
                           double *metric)
{
    int         src_index=0, dst_index=0;
    JRB         node = NULL;
    LboneResolutionIndex    *li;
    struct ibp_depot         s_depot = {"localhost", 6714};

    if ( src == NULL ) src = &s_depot;
    if ( dst == NULL ) dst = &s_depot;

    assert (metric != NULL);

    node = jrb_find_depot(lr->depotIndex, src);
    if ( node == NULL ) {
        return LORS_NO_SOURCE;
    }
    li = node->val.v;
    src_index = li->sourceIndex;

    node = jrb_find_depot(lr->depotIndex, dst);
    if ( node == NULL ) {
        return LORS_NO_DESTINATION;
    }
    li = node->val.v;
    dst_index = li->destIndex;

    /* return the value at this intersection */

    *metric = lr->resolution[src_index][dst_index];
    return LORS_SUCCESS;
}

/* this will only work if there is a space for the metric in the resolution
 * array. It will generate an error otherwise. */
int lorsSetResolutionPoint(LboneResolution *lr, 
                           IBP_depot src, 
                           IBP_depot dst, 
                           double metric)
{
    int         src_index=0, dst_index=0;
    JRB         node = NULL;
    LboneResolutionIndex    *li;
    struct ibp_depot         s_depot = {"localhost", 6714};

    if ( src == NULL ) src = &s_depot;
    if ( dst == NULL ) dst = &s_depot;

    if ( src == NULL )
    {
        src_index = 0;
    } else 
    {
        node = jrb_find_depot(lr->depotIndex, src);
        if ( node == NULL ) {
            fprintf(stderr, "no source\n");
            return LORS_NO_SOURCE;
        }
        li = node->val.v;
        src_index = li->sourceIndex;
    }

    if ( dst==NULL )
    {
        dst_index = 0;
    } else 
    {
        node = jrb_find_depot(lr->depotIndex, dst);
        if ( node == NULL ) {
            fprintf(stderr, "no destination\n");
            return LORS_NO_DESTINATION;
        }
        li = node->val.v;
        dst_index = li->destIndex;
    }

    /* set the value at this intersection */
    lr->resolution[src_index][dst_index] = metric;
    return LORS_SUCCESS;
}

/* release memeory retained from this ResolutionMatrix. */
int lorsFreeResolution(LboneResolution *lr)
{
    JRB node;
    int     i, j;
    IBP_depot               d;
    LboneResolutionIndex  *li;

    assert(lr!= NULL);

    for (i=0; i < lr->src_cnt; i++)
    {
        free(lr->resolution[i]);
    }
    free(lr->resolution);
    lr->resolution = NULL;

    jrb_traverse(node, lr->depotIndex)
    {
        d = node->key.v;
        li = node->val.v;

        /*fprintf(stderr, "calling free on d: 0x%x\n", d);*/
        free(d);
        free(li);
    }
    jrb_free_tree(lr->depotIndex);
    lr->depotIndex = NULL;
    free(lr);
    
    return LORS_SUCCESS;
}


/*
 * there are several calls to update the depotpool from a resolution metrix.
 *
 * for upload       src, null, res
 * for augment      src, dst, res
 * for download     null, dst, res
 */

/* TODO: 
 * lors_depotpool.c should export a way of iterating through the depots 
 */
int lorsUpdateDepotPoolFromResolution(LorsDepotPool *dp_src, LorsDepotPool *dp_dst, 
                                      LboneResolution *lr, int opts)
{
    JRB             node = NULL;
    LorsDepot       *depot = NULL;
    int             lret = -1;
    double          metric = 0.0;
    struct ibp_depot        localhost = {"localhost", 6714};
    if ( (dp_src == NULL && dp_dst == NULL) || lr == NULL )
    {
        return LORS_INVALID_PARAMETERS;
    }

    if ( dp_src == NULL && dp_dst != NULL )
    {
        /* align for download */
        jrb_traverse(node, dp_dst->list)
        {
            depot = node->val.v;

            lret = lorsGetResolutionPoint(lr, &localhost, depot->depot, &metric);
            if ( lret == LORS_SUCCESS )
            {
                depot->proximity = metric;
            } else 
            {
                depot->proximity = 0.0;
            }
        }
    } else if ( dp_dst == NULL && dp_src != NULL )
    {
        /* align for upload */
        jrb_traverse(node, dp_src->list)
        {
            depot = node->val.v;

            lret = lorsGetResolutionPoint(lr, depot->depot, &localhost, &metric);
            if ( lret == LORS_SUCCESS )
            {
                /*fprintf(stderr, "Setting proximity: %f\n", metric);*/
                depot->proximity = metric;
            } else 
            {
                /*fprintf(stderr, "Error getting resolution point\n");*/
                depot->proximity = 0.0;
            }
        }
    } else 
    {
        /*fprintf(stderr, "failure\n");*/
        return LORS_FAILURE;
    }
    return LORS_SUCCESS;
}
int lorsResolutionMax(LboneResolution *lr, double *max)
{
    double      l_max = 0;
    int         first = 0;
    int         i, j;
    for (i=0; i < lr->src_cnt; i++)
    {
        for(j=0; j< lr->dst_cnt; j++)
        {
            if ( lr->resolution[i][j] > 0 )
            {
                if ( first == 0 )
                {
                    l_max = lr->resolution[i][j];
                    first = 1;
                }
                l_max = (lr->resolution[i][j] > l_max ? lr->resolution[i][j] : l_max );
            }
        }
    }
    *max = l_max;
    return LORS_SUCCESS;
}

int lorsResolutionMin(LboneResolution *lr, double *min)
{
    double      l_min = 0;
    int         first = 0;
    int         i, j;
    for (i=0; i < lr->src_cnt; i++)
    {
        for(j=0; j< lr->dst_cnt; j++)
        {
            if ( lr->resolution[i][j] > 0 )
            {
                if ( first == 0 )
                {
                    l_min = lr->resolution[i][j];
                    first = 1;
                }
                l_min = (lr->resolution[i][j] < l_min ? lr->resolution[i][j] : l_min );
            }
        }
    }
    *min = l_min;
    return LORS_SUCCESS;
}

int lorsNormalizeResolution(LboneResolution *lr)
{
    int i, j;
    double min, max;
    int first = 0;

    if ( lr == NULL ) 
    {
        return LORS_INVALID_PARAMETERS;
    }
    for (i=0; i < lr->src_cnt; i++)
    {
        for(j=0; j< lr->dst_cnt; j++)
        {
            if ( lr->resolution[i][j] > 0 )
            {
                if ( first == 0 )
                {
                    min = lr->resolution[i][j];
                    max = lr->resolution[i][j];
                    first++;
                }
                if ( lr->resolution[i][j] < min )
                {
                    min = lr->resolution[i][j];
                }
                if ( lr->resolution[i][j] > max )
                {
                    max = lr->resolution[i][j];
                }
            }
        }
    }

    for (i=0; i < lr->src_cnt; i++)
    {
        for(j=0; j< lr->dst_cnt; j++)
        {
            if ( lr->resolution[i][j] != 0 )
            {
                if ( lr->resolution[i][j] < 1 )
                {
                    lr->resolution[i][j] = 0.0;
                } else 
                {
                    if ( i > 0 && j > 0 ) 
                    {
                        /* because the proximity returned by
                         * lbone_getProximity1() is not real proximity */
                        lr->resolution[i][j] = log(max) - log(lr->resolution[i][j]);
                    } else 
                    {
                        lr->resolution[i][j] = log(lr->resolution[i][j]);
                    }
                }
            }
#if 0
            lr->resolution[i][j] /= max;
            lr->resolution[i][j] -= 1;
            lr->resolution[i][j] *= -1;
#endif
        }
    }

    return LORS_SUCCESS;
}

