#ifndef __LORSAPI_H__
#define __LORSAPI_H__


/** 
 * @defgroup lors_api_exnode The LoRS API for ExNodes
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <jval.h>
#include <jrb.h>
#include <dllist.h>
#include <ibp.h>
#include <socket_io.h>
#include <lors_error.h>
#include <lors_opts.h>

#include <libexnode/exnode.h>  
#include <lbone_base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t     longlong;

#ifndef MIN
#define MIN(x, y) (x < y ? x : y )
#endif

typedef struct __LorsBoundary
{
    longlong            offset;
    longlong            length;
} LorsBoundary;

/**
 * @defgroup lors_api_set The LoRS API for Sets
 */

/*
 * @defgroup lors_api_IBP_storage_type  The LoRS API for IBP call
 */
#define IBP_HARD               IBP_STABLE
#define IBP_SOFT               IBP_VOLATILE

/** 
 * @ingroup lors_api_exnode 
 */
struct __LorsExnode
{
    /* this is a mapping from offsets to a Dllist of sets which contain
     * this offset. */
    JRB                 mapping_map;
    Exnode              *exnode;
    ExnodeMetadata     *md;
    longlong            seek_offset;
    longlong            alloc_length;
    longlong            logical_length;
};
typedef struct __LorsExnode LorsExnode;

/** 
 * @ingroup lors_api_set 
 */
struct __lorsConditionStruct 
{
    char        *function_name;
    ulong_t     blocksize;
    /* returns the outpu_length, the output data, and a new args_md
     * structure, composing all necessary information to decrypt it. */
    int        (*condition_data)(char              *input_buffer, 
                                 unsigned long int  input_length,
                                 unsigned long int  input_blocksize,
                                 char             **output_buffer, 
                                 unsigned long int *output_length, 
                                 unsigned long int *output_blocksize,
                                 ExnodeMetadata    *arguments);
    /* returns output, output_length, and accepts the previously created
     * lorsArg structure from deserialize or condition_data */
    int        (*uncondition_data)(char            *input_buffer, 
                                 unsigned long int  input_length,
                                 unsigned long int  input_blocksize,
                                 char             **output, 
                                 unsigned long int *out_length, 
                                 unsigned long int *output_blocksize,
                                 ExnodeMetadata *arguments);
    int              type;
    int              allocates;
    ExnodeMetadata   *arguments;
};
typedef struct __lorsConditionStruct LorsConditionStruct;

/** 
 * @ingroup lors_api_set
 */
struct __LorsSet
{
    JRB                 mapping_map;
    pthread_mutex_t    *lock;
    longlong            curr_length;
    longlong            seek_offset;

    longlong            exnode_offset;
    longlong            max_length;
    Dllist              condition_list;
    ulong_t             data_blocksize;
    int                 copies;
};
typedef struct __LorsSet LorsSet;
void lorsPrintSet(LorsSet *set);




typedef struct __LorsDepotList
{
    pthread_mutex_t     lock;
    Dllist          list;
    int             count;
    Dllist          bad_list;
} LorsDepotList;

/**
 * @defgroup lors_api_depotpool The LoRS API for LorsDepotPool
 */
/** 
 * @ingroup lors_api_depotpool
 */
struct __LorsDepotPool
{
    JRB                 list;   
    pthread_mutex_t    *lock;
    IBP_depot           lboneserver;
    int                 min_unique_depots;
    char               *location;
    ulong_t            size;
    int                 type;
    time_t              duration;
    int                 timeout;
    LorsDepotList       *dl;
};
typedef struct __LorsDepotPool LorsDepotPool;

struct __depot_t
{
    int                 id;
    IBP_depot           depot;
    LorsDepotPool       *dpp;
    pthread_mutex_t    *lock;
    double              score;      /* score */
    double              bandwidth;  /* bandwidth from local machine to this depot */
    double              proximity;  /* proximity to target position */
    int                 nfailure;   /* # of failures of this depot */
    int                 nthread;    /* # of threads using this depot */
    int                 nthread_done; /* # of threads finished using this depot */
    void               *tmp;
};
typedef struct __depot_t LorsDepot;

int lorsCreateDepotList(LorsDepotList **dpl);
int lorsAddDepot(LorsDepotList *dpl, LorsDepot *dp);
int lorsGetNextDepot(LorsDepotList *dpl, LorsDepot **dp);
int lorsRemoveDepot(LorsDepotList *dpl, IBP_depot dp);
int lorsCreateListFromPool(LorsDepotPool *dpp, LorsDepotList **dpl, int count);
int lorsRefillListFromPool(LorsDepotPool *dpp, LorsDepotList *dpl, int count);


/**
 * @defgroup lors_api_resolution The LoRS API for LboneResolution
 */
/** 
 * This is an opaque structure, the members of which a user should not need to 
 * access directly.
 * @ingroup lors_api_resolution
 */
typedef struct __lbone_resolution
{
    double ** resolution;
    int       src_cnt;
    int       dst_cnt;
    JRB       depotIndex;
} LboneResolution;

/** 
 * This is a structure held in the depotIndex tree, which maintains the source
 * and destination index of the resolution index.
 * @ingroup lors_api_resolution
 */
typedef struct __lbone_resoution_index
{
    int       sourceIndex;
    int       destIndex;
} LboneResolutionIndex;

/** 
 * From a serialized form, @c lorsCreateResolutionFromFile() will allow a user
 * to bring the information back into a @c LboneResolution structure.
 *
 * @param filename      Name and path of serialized file.
 * @param lr            Pointer into which the new structure will be returned.
 * @param metric_index  The format of the file allows for different metrics
 *                          addressable by index.
 *
 * @return              There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *                          
 * @ingroup lors_api_resolution
 */
int lorsCreateResolutionFromFile(char *filename, 
                                 LboneResolution **lr, 
                                 int metric_index);
/** 
 * UNIMPLEMENTED:<BR>
 * Rather than recovering cached information from a local file, it is possible
 * to make a cache from the lbone using information.
 * @param   lboneserver    The hostname and port of the LBone server from
 *                              which to recover resolution data.
 * @param   lr             Pointer into which the new structure will be returned.
 * @param   opt         <ul>
 *                          <li> LBONE_RESOLUTION_NWS </li>
 *                          <li> LBONE_RESOLUTION_GEO </li>
 *                      </ul>
 * @return              There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 * @ingroup lors_api_resolution
 */
int lorsCreateResolutionFromLBone(IBP_depot lboneserver,
                                  LboneResolution **lr,
                                  int opt);
/** 
 * Assign a value to the LBone resolution matrix as keyed on the
 * IBP_depot names.
 * @param lr        Pointer to the LboneResolution structure.
 * @param src       The source detpot.
 * @param dst       The destination depot.
 * @param metric    The value describing this pair.
 *
 * @ingroup lors_api_resolution
 */
int lorsSetResolutionPoint(LboneResolution *lr, 
                           IBP_depot src, 
                           IBP_depot dst, 
                           double metric);
/** 
 * Retrieve a point from the LBone resolution matrix as keyed on the
 * IBP_depot names.
 * @param lr        Pointer to the LboneResolution structure.
 * @param src       The source detpot.
 * @param dst       The destination depot.
 * @param metric    Pointer to a double into which the value is returned.
 * @ingroup lors_api_resolution
 */
int lorsGetResolutionPoint(LboneResolution *lr, 
                           IBP_depot src, 
                           IBP_depot dst, 
                           double *metric);
/** 
 * UNIMPLEMENTED: <br>
 * Serialize the LboneResolution structure to disk.
 * @param filename      The filename and path to write data to.
 * @param lr            The structure to serialize.
 * @return              There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 * @ingroup lors_api_resolution
 */
int lorsSaveResolutionToFile(char *filename, LboneResolution *lr);

/**
 * To assist the @c lorsSet* functions in making choices, 
 * @c lorsUpdateDepotPoolFromResolution() updates the @c LorsDepotPool with
 * the resolution information appropriate for the Operation.  When either 
 * @c dp_src or @c dp_dst are @c NULL, it has the same meaning as "localhost",
 * and is suitable for @c lorsSetStore() or @c lorsSetLoad().
 *
 * For @c lorsSetCopy(), the operation is more complex, and therefore the 
 * @c LboneResolution structure is passed directly to that function.
 *
 * @param dp_src    The source @c LorsDepotPool
 * @param dp_dst    The destination @c LorsDepotPool
 * @param lr        The @c LboneResolution structure.
 * @param opts      ?? Undetermined. ??
 *
 * @return              There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 * @ingroup lors_api_resolution
 */
int lorsUpdateDepotPoolFromResolution(LorsDepotPool *dp_src, LorsDepotPool *dp_dst, 
                                      LboneResolution *lr, int opts);

/** 
 * Release the resources used by the LboneResolution matrix.
 * @param lr        An LboneResolution structure as returned from
 *                  a @c CreateResolution function.
 * @return              There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 * @ingroup lors_api_resolution
 */
int lorsFreeResolution(LboneResolution *lr);

typedef Dllist LorsEnum;

/** 
 * Returns package name and decimal number representing the library version.
 * the package name and library version will be returned in the approiate
 * variables.  
 *
 * Either or both may be null, in which case, nothing useful will be done.
 *
 * @param package A pointer to a char *, in which the package name will be
 *                      returned.
 * @param version A pointer to a double precision version number.
 *
 * @ingroup lors_api_exnode
 * @return void
 */
void lorsGetLibraryVersion(const char **package, const double *version);



#if 0
<<<<<<< lors_api.h
struct __lorsrsset;
=======
>>>>>>> 1.4.2.4
#endif
struct __LorsMapping
{
    /* PUBLIC READ/WRITE */
    ExnodeMetadata        *md;
    ExnodeFunction        *function;
    /* PUBLIC READ ONLY */
    longlong               exnode_offset;

    ulong_t                logical_length;
    ulong_t                logical_offset;

    ulong_t                alloc_length;   /* equivalent to 'maxSize' or physical size */
    ulong_t                alloc_offset;   /* offset within the ibp byte array */

    ulong_t                seek_offset;    /* equivalent to 'currSize' 
                                           or the amount written thus far. */
    struct   ibp_depot     depot;
    ulong_t                e2e_bs;         /* e2e block size */
    ulong_t                e2e_head_len;   /* e2e block header szie */

    /* PRIVATE */
    struct ibp_set_of_caps capset;
    struct ibp_capstatus   capstat;

    time_t                 begin_time;
    time_t                 timeout;
    int                    opts;
    LorsDepot             *lors_depot;
    LorsDepotPool         *dp;
    pthread_t              tid;
    pthread_attr_t         attr;
    pthread_mutex_t       *lock;
    ulong_t                   id;
    ulong_t                   i;
#if 0
<<<<<<< lors_api.h

    int                     set_index;
    int                     coding_index;
    int                     n;
    int                     ft;
    ISBlock                 *block;
    struct __lorsrsset      *rss;
    int                     ret;
=======
    int                     notvalid;
>>>>>>> 1.4.2.4
#endif
    int                     notvalid;
};
typedef struct __LorsMapping LorsMapping;

/**
 * In order to enumerate through all of the mapping within a Set, you may use
 * @c lorsSetEnum().  It returns a @c LorsEnum which can then be passed to @c
 * lorsEnumNext() to get each successive mapping.  
 *
 * @brief Form an enumeration of all mappings within the Set.
 * @param set       A @c LorsSet
 * @param list      A pointer to a @c LorsEnum
 * @ingroup lors_api_set
 *
 */
int lorsSetEnum(LorsSet *set, LorsEnum *list);
/**
 * In order to enumerate through all of the mapping within a LorsExnode, you may use
 * @c lorsExnodeEnum().  It returns a @c LorsEnum which can then be passed to @c
 * lorsEnumNext() to get each successive mapping.  
 *
 * @brief Form an enumeration of all mappings within the Exnode.
 * @param exnode    A @c LorsExnode
 * @param list      A pointer to a @c LorsEnum
 * @ingroup lors_api_exnode
 *
 */
int lorsExnodeEnum(LorsExnode *exnode, LorsEnum   *list);

/**
 * Iterate through a list of LorsMappings.
 *
 * @brief Iterate through a LorsEnum list of mappings.
 * @param list      A @c LorsEnum list returned from @c lorsSetEnum().
 * @param iterator  A pointer to an iterator. <b>Must be</b> initially NULL.
 * @param ret       A pointer into which a LorsMapping pointer is returned.
 * @ingroup lors_api_set
 * @return          On list exhaustion, @c LORS_END is returned.
 */
int    lorsEnumNext (LorsEnum list, LorsEnum *iterator, LorsMapping **ret);
/**
 * Frees resources associated with a LorsEnum.
 *
 * @brief Free a LorsEnum.
 * @param list
 *
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 */
int    lorsEnumFree (LorsEnum list);


/**
 *  lorsGetDepotPool acts as a wrapper of various LBone calls.  It acts as a means for 
 *  obtaining a collection of IBP depots from which the other LoRS procedures will 
 *  draw.  Optionally, a user may provide a filename, which will replace the
 *  lbone call to getDepots.
 *
 *  @brief Create a collection of IBP depots from the LBone or a user supplied list.
 *
 *  @param dp                   A @c LorsDepotPool is returned after having called
 *                          either @c lbone_getDepots() or reading the specified list.
 *  @param lbone_server         The hostname of a specific lboneserver. 
 *  @param lbone_server_port    The corresponding port for given lboneserver.
 *  @param *depots              If not @c NULL, @c lorsGetLorsDepotPool() will attempt to
 *                          get all IBP depots from this list.  Convenient
 *                          for testing or requiring particular depots not
 *                          listed in the lbone.
 *  @param min_unique_depots    The lbone has a limited number of depots.  For
 *                          your particular application you may need more than
 *                          are available in a particular region, and without
 *                          a maximum, the lbone request might return depots from 
 *                          other regions
 *                          to satisfy the request.  @c min_unique_depots
 *                          allows a user to set the upper bound for the
 *                          number of depots returned.  For best striping, more
 *                          depots will be better.  For best performance, fewer
 *                          depots may be better.  But not too few..
 *  @param location             The lbone accepts location hints to specify
 *                          a region. <a href="http://loci.cs.utk.edu/lbone/">Lbone API</a>
 *  @param storage_size         @c storage_size is measured in Megabytes. 
 *                          Likely, you will need a minimum per-depot
 *                          storage capacity. For example, if your file were
 *                          20GB, and you wanted to stripe it across 5
 *                          locations, @c storage_size would equal 4GB/1MB.
 *  @param storage_type         IBP allows either @c IBP_HARD or @c IBP_SOFT
 *                          allocations.  SOFT are more readily available,
 *                          while HARD have stronger guarantees about
 *                          reliability. <a href="http://loci.cs.utk.edu/ibp">IBP API</a>
 *  @param duration         Depots can also specify maximum lease time for
 *                          allocations.  This gives a minimum that you are
 *                          willing to accept for your depots.
 *  @param nthreads             The maximum number of threads to utilize while
 *                          performing this operation.  Not fully supported by
 *                          lbone_checkDepots().
 *  @param timeout              Because this is a network operation, a
 *                          timeout is provided to assure that the call will eventually 
 *                          return, even on failure.
 *  @param opts             <ul>
 *                              <li>@c LORS_CHECKDEPOTS - Call
 *                                  lbone_checkDepots() on first depot list.
 *                              <li>@c LORS_SERVER_AND_LIST - Read depots from 
 *                                  given list as 
 *                                  well as from lbone_server to meet min_unique_depots.
 *                              <li>@c SORT_BY_PROXIMITY
 *                              <li>@c SORT_BY_BANDWIDTH
 *                              <li>@c SORT_BY_OTHERS
 *                          </ul>
 *
 *  @ingroup lors_api_depotpool
 *  @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 *
 */
int    lorsGetDepotPool (LorsDepotPool  **dp,
                         char        *lbone_server,
                         int          lbone_server_port,
                         IBP_depot   *depots,
                         int          min_unique_depots,
                         char        *location,
                         ulong_t     storage_size,
                         int          storage_type,
                         time_t       duration,
                         int          nthreads,
                         int          timeout,
                         int          opts);

/**
 * This function is necessary until there is a more appropriately named 
 * @c lorsCreateDepotPoolFromExnode().   As deserialized, the @c LorsExnode
 * does not include the LorsDepotPool structure.  This funciton creates one
 * and updates it with the given proximity as returned by the lbone.
 *
 * This functionality should be replaced with a combination of functions
 * lorsGetDepotPoolFromExnode and @c lorsUpdateDepotPoolFromResolution();
 *
 * @param   exnode       A recently deserialized exnode.
 * @param   ret_dpp      A pointer into which the newly created @c LorsDepotPool 
 *                          is returned.
 * @param   lbone_server    An lbone server which can return proximity
 *                          information relative to the given location.
 * @param   lbone_server_port  The lbone server port.
 * @param   location     A location hint relative to which the lbone returns
 *                          stuff.
 * @param   nthreads     The maximum number of threads to use.
 * @param   timeout      Spend no longer than this.
 * @param   opts         ??
 *  @ingroup lors_api_depotpool
 *  @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 */
int lorsUpdateDepotPool(LorsExnode *exnode,
                        LorsDepotPool **ret_dpp,
                        char       *lbone_server,
                        int         lbone_server_port,
                        char       *location,
                        int         nthreads,
                        int         timeout,
                        int         opts);

/**
 * Release the resources consumed by this structure.
 *
 * @param dpp   A previously created @c LorsDepotPool.
 *
 * @ingroup lors_api_depotpool
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 */
int lorsFreeDepotPool(LorsDepotPool *dpp);
/*-------------------------------------------------------------------------
 * Operations directly on LorsExnode 
 *-----------------------------------------------------------------------*/

/**
 * The LorsExnode acts as a container structure for all user created
 * LorsSets.  
 *
 * @brief  Allocate a container structure for LorsSets.
 * @param   exnode              A pointer to a LorsExnode pointer into 
 *                          which the allocated structure will be returned.
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsExnodeCreate (LorsExnode ** exnode);
int    lorsExnodeFree( LorsExnode *exnode);
/**
 * To append a Set to the exnode container structure,  a previously created
 * Set is given as parameters.
 *
 * @brief  Append a Set to an Exnode.
 * @param  exnode           The LorsExnode created from a successful call to
 *                          @c lorsExnodeCreate()
 * @param   set          A previously created LorsSet to be added to @c exnode.
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsAppendSet (LorsExnode * exnode,
                         LorsSet * set);

/**
 * Add an existing mapping to an exnode.  This is useful for creating a new
 * exnode from an old exnode in combination with @c lorsExnodeEnum()
 *
 * @brief Add mapping to exnode.
 * @param   exnode          The LorsExnode to add the mapping.
 * @param   map             The LorsMapping to add to the exnode.
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsAppendMapping (LorsExnode * exnode,
                         LorsMapping * map);
/**
 * Remove a mapping from an Exnode.  This is useful for modifying the content
 * of an exnode when used in combination with @c lorsExnodeEnum()
 *
 * @brief Remove mapping from exnode.
 * @param   exnode          The LorsExnode to add the mapping.
 * @param   map             The LorsMapping to add to the exnode.
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 */
int    lorsDetachMapping (LorsExnode *exnode, LorsMapping *map);


/*-------------------------------------------------------------------------
 * Wrapper Functions to the libexnode xml api
 *-----------------------------------------------------------------------*/

/**
 * After a @c lorsSerialize() call, you will have an XML  string
 * representative of the Exnode data structure.  To recover the data structure
 * from the XML serialization, you must deserialize it.
 *
 * @brief   Reconstruct the Exnode datastructure from an XML serialization.
 * @param   exnode  A pointer to a pointer, in which the new structure is returned.
 * @param   buffer  A NULL terminated  buffer with the serialized Exnode.
 * @param   length  The length of the buffer to deserialize.
 * @param   schema  An optional Schema file for DTD or Schema Validation. (unsupported)
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsDeserialize (LorsExnode ** exnode,
                        char *buffer,
                        int  length,
                        char *schema);
/**
 * This method will get JSON data in UEF format from URL and deserialize it using
 * @cu efDeserializer().
 *
 * @brief  Deserialize JSON data from URL.
 * @param   exnode  A pointer to a pointer, in which the new structure is returned.
 * @param   url     URL to prob.
 * @param   schema  An optional Schema file for DTD or Schema Validation. (unsupported)
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsUrleDeserialize (LorsExnode ** exnode,
                            char *url,
                            char *schema);

/**
 * After a @c lorsFileSerialize() call, you will have an XML  file
 * representative of the Exnode data structure.  To recover the data structure
 * from the XML serialization, you must deserialize it.
 *
 * @brief  Deserialize from File rather than from buffer.
 * @param   exnode  A pointer to a pointer, in which the new structure is returned.
 * @param   filename The name of a Serialized Exnode.
 * @param   schema  An optional Schema file for DTD or Schema Validation. (unsupported)
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */

int lorsFileDeserialize (LorsExnode ** exnode,
						 char *filename,
						 char *schema);
	
int lorsPostUnis(LorsExnode *exnode,
				 char       *url);
	
int lorsUefSerialize(LorsExnode * exnode,
					 char       *filename );
	
/**
 * To preserve the structure of the Exnode across invocations of an
 * application, the @c lorsSerialize() call translates the Exnode structure
 * to an ASCII encoding.
 *
 *
 * @brief Translate Exnode structure to XML string.
 * @param  exnode   The Exnode structure to serialize.
 * @param  buffer   The location into which a pointer to the resulting
 *                      serialization will be placed.
 * @param  readonly Whether or not to include Write and Manage Capabilities in
 *                      the XML serialization.
 * @param  length   The final length of @c buffer.
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsSerialize (LorsExnode * exnode,
                      char **buffer,
                      int readonly,
                      int *length);
/**
 * To preserve the structure of the Exnode across invocations of an
 * application, the @c lorsFileSerialize() call trans lates the Exnode structure
 * to an ASCII text File.
 *
 * @brief   Serialize Exnode and store to file.
 * @param   exnode    
 * @param   filename     Name of file to create or overwrite with new serialization.
 * @param   readonly     Whether or not to include Write and Manage Capabilities in
 *                          the XML serialization.
 * @param   opts         In what way to serialize the thing.
 * @ingroup lors_api_exnode
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsFileSerialize (LorsExnode * exnode,
                          char *filename,
                          int readonly,
                          int opts);


/** 
 * The @c lorsExnodeCreate() or @c lorsSetCreate() will allocate a
 * ExnodeMetadata.  Therefore, there is no @c lorsSetMetadataCreate() call. To access
 * the metadata for either a set or exnode use these calls. 
 * Because you are given a pointer to the same pointer that the exnode or
 * set maintain, there are no functions to set the metadata for an exnode
 * or set.
 *
 * @ingroup lors_api_metadata
 * @brief Fetch the ExnodeMetadata from a LorsExnode or LorsSet.
 * @param exnode An exnode pointer.
 * @param md     A pointer to a ExnodeMetadata variable, to be used with future
 *                  calls for Metadata manipulation.
 */
int lorsGetExnodeMetadata (LorsExnode *exnode, ExnodeMetadata **md);

/** 
 * The @c lorsExnodeCreate() or @c lorsSetCreate() will allocate a
 * ExnodeMetadata.  Therefore, there is no lorsMetadataCreate() call. To access
 * the metadata for either a set or exnode use these calls. 
 * Because you are given a pointer to the same pointer that the exnode or
 * set maintain, there are no functions to set the metadata for an exnode
 * or set.
 * @note 
 *
 * @ingroup lors_api_metadata
 * @brief Fetch the ExnodeMetadata from a LorsExnode or LorsSet.
 * @param map    A @c LorsMapping pointer.
 * @param md     A pointer to a ExnodeMetadata variable, to be used with future
 *                  calls for Metadata manipulation.
 */
int lorsGetMappingMetadata (LorsMapping *map, ExnodeMetadata **md);
int lorsMetadataMerge(ExnodeMetadata *src, ExnodeMetadata *dest);


/*-------------------------------------------------------------------------
 * Primitive lorsSet* Functions
 *-----------------------------------------------------------------------*/
/**
 * Find a set within an exnode which conforms to specified search criteria.
 * @param exnode    A LorsExnode pointer previouly created from @c
 *                      lorsFileDeserialize() or @c lorsExnodeCreate()
 * @param set       The pointer into which the matching set will be returned.
 * @param offset    Mappings from @c offset.
 * @param length    To @c offset + @c length
 * @param opt       Other options to add to the Query.
 *                          <ul>
 *                              <li>@c LORS_QUERY_REMOVE
 *                          </ul>
 *
 * @ingroup lors_api_exnode
 * @return                  There are various error conditions. See list of
 */
int    lorsQuery (LorsExnode *exnode, 
                  LorsSet **set, 
                  longlong  offset, 
                  longlong  length,
                  int       opt); 

#if 0
/** 
 * There can be a primary name associated with each set, based on either the
 * assigned name, or the name derived from a query.
 * A name will not be inherited after a @c lorsSetMerge(). It must be set
 * explicitly.  For instance, Create(set_a), Create(set_b), name (set_a, "foo"), 
 *   merge(set_a,set_b), will leave all mappings from set_b unnamed.  another 
 *   call to name(set_a,"foo") will name them all.
 * @brief Assigns a name to all mappings currently contained 
 *      within the set.
 * @param se                A LorsSet from a previous call to
 *                              @c lorsSetCreate() or a deserialize operation.
 * @param name              The name to assign to all mappings.
 * @ingroup lors_api_set
 * @return                  There are various error conditions. See list of
 *                              all possible errors @ref lors_api_errors.
 */
int    lorsNameSet (LorsSet *set,
                    char *name);
#endif

/**
 * Append a mapping to a given set.  If the mapping is already present, and
 * error will be returned.  A mapping should exist only once per set.
 * @param set
 * @param map
 * @ingroup lors_api_set
 * @return                  There are various error conditions. See list of
 *                              all possible errors @ref lors_api_errors.
 */
int    lorsSetAddMapping (LorsSet *set, 
                         LorsMapping *map);

/**
 * Remove a mapping from a given set.
 * @param set
 * @param map 
 * @ingroup lors_api_set
 * @return                  There are various error conditions. See list of
 *                              all possible errors @ref lors_api_errors.
 */
int    lorsSetRemoveMapping (LorsSet *set, 
                         LorsMapping *map);

/* 
 * Allocate the necessary IBP buffers and create the corresponding mappings
 * to fill the reqeusted number of copies and blocksize across 'length'.
 */
/**
 * @c lorsSetInit() initializes a LorsSet structure with the specified
 * data_blocksize and number of copies.  Before calling either @c lorsSetStore()
 * or @c lorsSetCopy() you should call @c lorsSetInit() to create a target
 * LorsSet into which new mappings will be added.
 *
 * @brief Initialize LorsSet internal data structures.
 * @param set            A pointer to a pointer of a LorsSet into
 *                          which the new structure will be returned.
 * @param data_blocksize The blocksize of allocated fragments.
 * @param copies         The number of replicas to create.
 * @param opts           This is always zero for now.  In the future 
 *                          there may be more options.
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */

int    lorsSetInit(LorsSet **set, 
                      ulong_t   data_blocksize,
                      int       copies,
                      int       opts);

/**
 * @c lorsSetStore() sends data to IBP allocations.
 * The number of threads used to execute the operation will depend heavily on
 * the initial blocksize given to @c lorsSetCreate() and the size of @c
 * buffer.  Only one thread can be active per Mapping (i.e. per replica).
 * <BR>In the event of depot failure during Store or Allocate, a new buffer
 * will be allocated from the LorsDepotPool.
 *
 * @brief Allocate storage and Send data to the IBP allocations.
 * @param set            A LorsSet from a previous call to
 *                          @c lorsSetCreate() or a deserialize operation.
 * @param dp            A @c LorsDepotPool from which new allocations will be
 *                          drawn.
 * @param buffer        The data a user wishes to upload.
 * @param offset        The absolute offset relative to the exnode to store the 
 *                          buffer data.
 *                          This allows multiple calls to SetStore
 *                          simultaneously, if necessary.
 * @param length        The length of @c buffer.
 * @param lc            An array of LorsConditionStructs which specify the
 *                          order to apply end2end operations.  This may be
 *                          @c NULL if no conditioning is desired.
 * @param nthreads      The maximum number of threads to use.  By default, only
 *                          (length/blocksize + 1 * copies) can be used,
 * @param timeout    Return after @c timeout seconds, even if operation is
 *                      incomplete.
 * @param handle     socket_io handle to report progress to report host
 * @param opts              <ul>
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                          </ul>
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 */
int    lorsSetStore (LorsSet        *set,
                     LorsDepotPool  *dp,
                     char           *buffer,
                     longlong        offset,
                     longlong        length,
                     LorsConditionStruct *lc,
                     int             nthreads,
                     int             timeout,
					 socket_io_handler *handle,
                     int             opts);

longlong lorsSetFindMaxlength(LorsSet *set);
/* 
 * this function is not valid for E2E encoded mappings.  Behavior is
 * unpredictable and will certainly fail if e2e mappings are given to it.
 *
 * set->data_blocksize must be a valid value.
 */
int     lorsSetUpdate(LorsSet       *set, 
                  LorsDepotPool *dp, 
                  char *buffer, 
                  longlong offset, 
                  longlong length, 
                  int       nthreads,
                  int       timeout,
                  int       opts);
/**
 *
 * @c lorsSetLoad() retrieves data from IBP and stores it into @c buffer.
 * @c buffer must contain at least @c length  bytes, or risk overwriting
 * memory.
 * <br> The number of threads used depends strictly on the amount passed
 * to @c nthreads and the @c opts Policy.
 * <br> @c lorsSetLoad() operates on 'length/nthreads' Blocksizes.  However,
 * in the case where 'Blocksize' is larger than the length to next boundary
 * point (i.e. when mappings size are not an even multiple of 'Blocksize') the
 * less of the two will be chosen to assign the next thread.
 * <br> The return value is the number of bytes read.
 *
 * @brief Retrieve data from IBP allocations.
 * @param set            A LorsSet from a previous call to
 *                          @c lorsSetCreate() or a deserialize operation.
 * @param buffer        A pointer to data area into which @c lorsSetLoad()
 *                          stores.
 * @param offset        The offset relative to the Set from which this
 *                          operation is to begin.
 * @param length        The desired size of set to retrieve, and length of
 *                          @c buffer
 * @param block_size    The maximum datasize for one IBP_load call. 
 * @param lc            An array of all possible LorsConditionStructs used to
 *                          decode any conditioning applied to the mappings in
 *                          @c set.
 * @param nthreads      The maximum number of threads to use.  Because IBP
 *                          allocations are memory mapped, any number of
 *                          simultaneous threads may be used.
 * @param timeout    Return after @c timeout seconds, even if operation is
 *                      incomplete.
 * @param opts              <ul>
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                          </ul>
 * @ingroup lors_api_set
 * @return                  On error, a negative number is returned. Otherwise
 *                      the number of bytes successfully written to the buffer 
 *                      is returned.
 *
 */
long    lorsSetLoad (LorsSet     *set,
                    char        *buffer,
                    longlong     offset,
                    long         length,
                    ulong_t         block_size,
                    LorsConditionStruct *lc,
                    int          nthreads,
                    int          timeout,
                    int          opts);

/**
 * To satisfy the requirements of real time streaming applications, 
 * @c lorsSetRealTimeLoad() writes to a file descriptor rather than a buffer.
 * Most of the semantics remain the same as for @c lorsSetLoad().
 *
 * @brief   Provide real-time streaming of data over a file descriptor.
 * @param   set            A LorsSet from a previous call to
 *                      @c lorsSetCreate() or a deserialize operation.
 * @param   buffer         A pointer to data area into which @c lorsSetLoad()
 *                      stores.
 * @param   fd          A valid file descriptor to which 
 *                      @c lorsSetRealTimeLoad() will write in order the 
 *                      successfully downloaded @c length requested.
 * @param offset        The offset relative to the Set from which this
 *                          operation is to begin.
 * @param length        The desired size of set to retrieve, and length of
 *                          @c buffer
 * @param block_size    The maximum datasize for one IBP_load call. 
 * @param   pre_buf     This is an integer count of the number of 
 *                      @c block_size sections to fully download before
 *                      writing to the file descriptor.
 * @param cache         The maximum amount of buffering the call will
 *                          provide in addition to the memory allocated per-thread.
 * @param lc            An array of all possible LorsConditionStructs used to
 *                          decode any conditioning applied to the mappings in
 *                          @c set.
 * @param nthreads      The maximum number of threads to use.  Because IBP
 *                          allocations are memory mapped, any number of
 *                          simultaneous threads may be used.
 * @param timeout       Return after @c timeout seconds, even if operation is
 *                           incomplete.
 * @param   max_thds_per_depot      In order to minimize the impact of our
 *                              algorithm on depots, this parameter will
 *                              provide a cap for individual depot
 *                              connections.
 * @param   thds_per_job            This is a measure of redundancy.
 * @param   progress_n              When @c progress_n is greater than 0, and
 *                              @c thds_per_job is greater than 1
 * @param   handle            socket io handle to report status to report host
 * @param   opts              <ul>
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                          </ul>
 *
 * @ingroup lors_api_set
 * @return                  On error, a negative number is returned. Otherwise
 *                      the number of bytes successfully written to the buffer 
 *                      is returned.
 */
longlong    lorsSetRealTimeLoad (LorsSet * set,
                       char *buffer,
                       int  fd,
                       longlong offset,
                       longlong length,
                       ulong_t block_size,
                       int  pre_buf,
                       int  cache,
                       LorsConditionStruct *lc,
                       int nthreads,
                       int timeout,
                       int max_thds_per_depot,
                       int thds_per_job,
                       int progress_n,
					   socket_io_handler *handle,
                       int opts);



/**
 * @c lorsSetCopy() copies data from the @offset and @c length of @c srcSet 
 * to the @c seek_offset of @c dstSet.  If the length of of @c srcSet is too
 * large to fit within @c dstSet, it may or may not generate an error condition
 * depending on the options passed in @c opts.
 *
 * @brief Create replicas of data in srcSet allocation to another.
 * @param srcSet      A set from which a copy begins.
 * @param dstSet      A pointer to a set to which the copy is placed.
 * @param dp          A @c LorsDepotPool from which new allocations will be
 *                      drawn.
 * @param lr          An LboneResolution matrix as returned by 
 *                      @c lorsCreateResolutionFromFile() or others.  From
 *                      this information, decision regarding routing are made.
 * @param offset      The exnode offset of the mappings in @c srcSet
 * @param length      Length relative to @c srcSet
 * @param nthreads    By default, this will depend on the number of mappings
 *                      in @c set , i.e. one thread per fragment.
 * @param timeout     Return after @c timeout seconds, even if operation is
 *                      incomplete.
 * @param opts              <ul>
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                              <li>@c LORS_MCOPY
 *                              <li>@c LORS_COPY
 *                          </ul>
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsSetCopy (LorsSet      *srcSet,
                    LorsSet      *dstSet,
                    LorsDepotPool *dp,
                    LboneResolution    *lr,
                    longlong      offset,
                    longlong      length,
                    int           nthreads,
                    int           timeout,
                    int           opts);

/**
 * @c lorsSetMerge() combines two Sets into a single @c dest,
 * It does not perform a deep copy on internal datastructures, and mappings.  
 * The two are not independent after lorsSetMerge().  
 * 
 * @brief Combine the mappings from two Sets into one Set.
 *
 * @param src                   The pointer to the first and source Set.
 * @param dest                  The pointer to second and destination Set.
 * @ingroup lors_api_set
 * @return                      There are various error conditions. See list of
 *                                all possible errors @ref lors_api_errors.
 */
int    lorsSetMerge (LorsSet * src,
                     LorsSet * dest);

/**
 * After an allocation has expired it may be of no use to leave it in the
 * Set.  @c lorsSetTrim() provides the mechanism by which expired or
 * unneeded mappings may be removed.  Alternately, the operation is more
 * subtle, allowing for readdressing of existing mappings, such that, pieces
 * of a mapping may be ignored, while still present, in IBP.
 * <BR>
 * The number of threads needed to perform this operation are determined automatically.
 *
 * @brief Remove particular extents or Mappings from Set
 * @param set            A LorsSet from a previous call to
 *                          @c lorsSetCreate() or a deserialize operation.
 * @param offset        The offset relative to the Set.
 * @param length        The desired size of set to trim.
 * @param nthreads   By default, this will depend on the number of mappings
 *                      in @c set , i.e. one thread per fragment.
 * @param timeout    Return after @c timeout seconds, even if operation is
 *                      incomplete.
 * @param opts              <ul>
 *                              <li>@c LORS_TRIM_ALL -
 *                              <li>@c LORS_TRIM_DEAD - 
 *                              <li>@c LORS_TRIM_KILL - 
 *                              <li>@c LORS_TRIM_NOKILL - 
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                          </ul>
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsSetTrim (LorsSet * set,
                       longlong offset,
                       longlong length,
                       int nthreads,
                       int timeout,
                       int opts);
#if 0
/**
 * Back by popular demand, to complement the view of mappings exposed by
 * @c lorsSetStat(), @c lorsSetTrimMapping allows a specific mapping to
 * be removed from the given set.
 *
 * @brief Remove a mapping from a set.
 * @param se            A particular set, from which @c map originated.
 * @param map           The mapping pointer as returned from @c lorsSetStat()
 * @param timeout       The maximum amount of time to allow for the entire
 *                          operation, regardless of success or failure.
 * @param opts              <ul>
 *                              <li>@c LORS_TRIM_DEAD - 
 *                              <li>@c LORS_TRIM_KILL - 
 *                              <li>@c LORS_TRIM_NOKILL - 
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR -
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT -
 *                          </ul>
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 */
int    lorsSetTrimMapping (LorsSet  *se, 
                              LorsMapping *map,
                              int  timeout,
                              int opts);
#endif

/*
 * extend or set the expiration times of those mappings contained within
 * offset-length.
 */
/**
 * Because the underlying storage of the LoRS tools is IBP, the allocations
 * will eventually expire.  However, before they expire, it may be possible to
 * renew the lease on these allocations.  
 * <BR>The number of threads are determined automatically if 'nthreads' is @c
 * -1.
 *
 * @brief Refresh the expiration time on allocated storage.
 * @param set            A LorsSet from a previous call to
 *                          @c lorsSetCreate() or a deserialize operation.
 * @param offset        The offset relative to the Set.
 * @param length        The desired size of set to refresh.
 * @param duration          The new time in seconds.
 * @param nthreads   By default, this will depend on the number of mappings
 *                      in @c set , i.e. one thread per fragment.
 * @param timeout           Return after @c timeout seconds, even if operation is
 *                              incomplete.
 * @param opts              <ul><li>@c LORS_REFRESH_MAX - Attempt to refresh
 *                              to the largest allowable expiration time.
 *                              <li>@c LORS_REFRESH_EXTEND_BY - Attempt to add
 *                              @c duration seconds to the time out.
 *                              <li>@c LORS_REFRESH_EXTEND_TO -  Attempt to
 *                              normalize all @c duration seconds from now.
 *                              <li>@c LORS_REFRESH_ABSOLUTE - Attempt to set
 *                              expiration to @c duration seconds.
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                          </ul>
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsSetRefresh (LorsSet * set,
                       longlong offset,
                       longlong length,
                       time_t duration,
                       int nthreads,
                       int timeout,
                       int opts);

/**
 * In order to have some view of the Mapping level, @c lorsSetStat()
 * is provided to probe the data of individaul allocations.
 * This allows for more complex 
 * operations such as 'Balance' which could guarantee a minimum number 
 * of replicas per-offset, or for determining other reliability statistics.
 *
 * @brief   Perform a status operation on the Mappings of a Set.
 * @param set            A LorsSet from a previous call to
 *                          @c lorsSetCreate() or a deserialize operation.
 * @param nthreads      The maximum number of threads to use for this
 *                          operation. a value of -1 will use 1 thread per
 *                          mapping.
 * @param timeout       The maximum amount of time to allow for the entire
 *                          operation, regardless of success or failure.
 * @param opts              <ul>
 *                              <li>@c LORS_STAT_LIVE
 *                              <li>@c LORS_STAT_CACHE
 *                              <li>@c LORS_RETURN_ON_ANY_ERROR
 *                              <li>@c LORS_RETRY_UNTIL_TIMEOUT
 *                          </ul>
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                          all possible errors @ref lors_api_errors.
 *
 */
int    lorsSetStat(LorsSet *set, 
                   int            nthreads,
                   int            timeout,
                   int            opts);

/** 
 * Release memory back to local system.
 *
 * @brief Perform 'free' on internal data structures.
 * @param set           A @c LorsSet
 * @param opts          <ul>
 *                          <li> @c LORS_FREE_MAPPINGS
 *                      </ul>
 *
 * @ingroup lors_api_set
 * @return 0 
 */
int   lorsSetFree(LorsSet *set, int opts);

/** 
 * Trim IBP caps from a set of mappings.
 *
 * @brief Trim IBP caps from a set of mappings.
 * @param set           A @c LorsSet
 * @param @which        <ul>
 *                          <li> @c LORS_READ_CAP
 *                          <li> @c LORS_WRITE_CAP
 *                          <li> @c LORS_MANAGE_CAP
 *                      </ul>
 *
 * @ingroup lors_api_set
 * @return                     There are various error conditions. See list of
 *                             all possible errors @ref lors_api_errors.
 */
int   lorsSetTrimCaps(LorsSet *set, int opts);

#include <lors_util.h>
#ifdef __cplusplus
}
#endif
#endif
