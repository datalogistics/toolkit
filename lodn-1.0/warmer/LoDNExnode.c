/******************************************************************************
 * Name: LoDNExnode.c
 * Description: This file implements the LoDNExnode function calls.
 *
 * Developer: Christopher Sellers
 ******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <limits.h>
#include "dllist.h"
#include "LoDNLogging.h"
#include "LoDNThreads.h"
#include "LoDNExnode.h"
#include "LoDNFreeMappings.h"
#include "LoDNAuxillaryFunctions.h"
#include "libexnode/exnode.h"
#include "ibp.h"



/****---------------------------------jrb_ll_cmp()-------------------------****
 * Description: This function is used in the JRB tree for sorting exnode offset
 *              and length.  Same as int compare but with long longs.
 * Input: val1, val2 - Jval that holds the long long values.
 * Output: It returns a negative value if val1 < val2, 0 for val1 == val2, or
 *         1 for val1 > val2;
 ****----------------------------------------------------------------------****/
static int jrb_ll_cmp(Jval val1, Jval val2)
{
    if(val1.ll < val2.ll)
    {
       return -1;
    }else if( val2.ll < val1.ll)
    {
       return 1;
    }

    return 0;

	/* return (int)(val1.ll - val2.ll); <-- Doesn't work on systems where an
	 * int < long long and values are > 4 GBs */
}



/****-------------------_insert_mapping_into_segment()---------------------****
 * Description: This static function is used within this file to insert a
 *              mapping into its segment (block).  If no segment exists for
 *              the mapping then it creates a segment for the mapping.
 * Input: exnode - pointer to the LoDNExnode structure.
 *        mapping - pointer to the LoDNExnodeMapping structure that is to be
 *                  inserted into the segment list for the exnode.
 * Output: It returns 0.
 ****----------------------------------------------------------------------****/
static int _insert_mapping_into_segment(LoDNExnode *exnode,
                                        LoDNExnodeMapping *mapping)
{
    /* Declarations */
    LoDNExnodeSegment *segment;
    long long          mappingStart;
    long long          mappingEnd;
    JRB                jnode2;
    int                segmentFound = 0;


    /* Gets the start and end of the mapping */
    mappingStart = mapping->exnode_offset;
    mappingEnd   = mapping->exnode_offset+mapping->logical_length-1;

    /* Traverses the list of exnode segments looking for the segment that the
     * current mapping belongs to */
    jrb_traverse(jnode2, exnode->segments)
    {
        segment = (LoDNExnodeSegment*)jnode2->val.v;

        /* Detect intersection */
        if(mappingStart == segment->start && mappingEnd == segment->end)
        {
            /* Insert the current mapping into the segments list of mappings */
            dll_append(segment->mappings, new_jval_v(mapping));

            segmentFound = 1;
            break;
        }
    }

    /* True if no corresponding segment was not found so it creates a segment
     * for the mapping and puts the new segment into the exnodes list of
     * segments */
    if(segmentFound == 0)
    {
        /* Allocates a segment structure */
        if((segment = (LoDNExnodeSegment*)malloc(sizeof(LoDNExnodeSegment))) == NULL ||
           (segment->mappings = new_dllist()) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Sets the exnode for the segment to belongs to */
        segment->exnode = exnode;

        /* Stores the segments start and end position */
        segment->start = mappingStart;
        segment->end   = mappingEnd;

        /* Appends the mapping to the segment list of mappings */
        dll_append(segment->mappings, new_jval_v(mapping));

        /* Inserts the segment into the exnodes list of segments */
        if(jrb_insert_gen(exnode->segments, new_jval_ll(mappingStart),
                          new_jval_v(segment), jrb_ll_cmp) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }

    /* Returns success */
    return 0;
}



/****-------------------LoDNExnodeReadMapping()----------------------------****
 * Description: This function reads the values from a ExnodeMapping from
 *              libExnode and creates a LoDNExnodeMapping with these values.
 * Input: stdMapping - pointer to the ExnodeMapping structure.
 *        mapping    - pointer to the pointer of what will be the created
 *                     LoDNExnodeMapping structure.
 * Output: mapping - the LoDNexnodeMapping pointer gets changed so it points
 *                   to the created LoDNExnodeMapping.
 *         It returns 0 on success and -1 on failure.
 * Warning: It creates a heap allocated structure that needs to be freed.
 ****----------------------------------------------------------------------****/
static int LoDNExnodeReadMapping(ExnodeMapping *stdMapping, LoDNExnodeMapping **mapping)
{
    /* Declarations */
    ExnodeMetadata    *metadata   = NULL;
    ExnodeValue        value;
    ExnodeType         type;
    int                hostnamelen;


    /* Intializes the mapping to NULL */
    *mapping = NULL;

    /* Allocates a new mapping */
    if((*mapping = (LoDNExnodeMapping*)malloc(sizeof(LoDNExnodeMapping))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Gets the metadata for the mapping */
    if(exnodeGetMappingMetadata(stdMapping, &metadata) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Can't get exnode mapping metadata");
        goto ERROR_HANDLER;
    }

    /* Gets the allocation length from the Exnode mapping */
    if(exnodeGetMetadataValue(metadata, "alloc_length", &value, &type) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Can't get exnode mapping alloc_length");
        goto ERROR_HANDLER;
    }

    /* Sets the allocation length */
    (*mapping)->alloc_length = value.i;

    /* Get the allocation offset from the Exnode mapping */
    if(exnodeGetMetadataValue(metadata, "alloc_offset", &value, &type) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Can't get exnode mapping alloc_offset");
        goto ERROR_HANDLER;
    }

    /* Sets the allocation offset */
    (*mapping)->alloc_offset = value.i;

    /* Gets the exnode offset from the Exnode mapping */
    if(exnodeGetMetadataValue(metadata, "exnode_offset", &value, &type) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Can't get exnode mapping exnode offset");
        goto ERROR_HANDLER;
    }

    /* Sets the exnode offset */
    (*mapping)->exnode_offset = value.i;

    /* Gets the logical length from the exnode */
    if(exnodeGetMetadataValue(metadata, "logical_length", &value, &type) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Can't get exnode mapping logical_length");
        goto ERROR_HANDLER;
    }

    /* Sets the logical length */
    (*mapping)->logical_length = value.i;


    /* Sets the status of the mapping to good */
    (*mapping)->status      = 1;

    /* Gets the status of the mapping if its present */
    if(exnodeGetMetadataValue(metadata, "status", &value, &type) == EXNODE_SUCCESS)
    {
    	if(strcmp(value.s, "DEAD") == 0)
    	{
    		(*mapping)->status = 0;
    	}
    }

    /* Gets the broken time of the mapping if its present */
    if(exnodeGetMetadataValue(metadata, "broken_since", &value, &type) == EXNODE_SUCCESS)
    {
    	(*mapping)->brokenSince = (int)value.i;
    }else
    {
    	(*mapping)->brokenSince = -1;
    }

    /* Gets the capablities for mapping */
    if(exnodeGetCapabilities(stdMapping, &(*mapping)->readCap, &(*mapping)->writeCap,
                             &(*mapping)->manageCap) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0,  "Can't get exnode capabilities");
        goto ERROR_HANDLER;
    }

    /* Checks that the readCap is there */
    if((*mapping)->readCap == NULL)
    {
       logPrintError(0, __FILE__, __LINE__, 0,  "No exnode read capability for a mapping");
       goto ERROR_HANDLER;
    }

    /* Calculates the length of the hostname from the read cap */
    hostnamelen = strchr((*mapping)->readCap+IBP_URI_LEN, ':')-((*mapping)->readCap+IBP_URI_LEN);

    /* Checks that the length of the hostname is valid */
    if(hostnamelen < 0 || hostnamelen > MAXHOSTNAMELEN)
    {
        goto ERROR_HANDLER;
    }

    /* Copies the hostname into the mapping hostname */
    strncpy((*mapping)->hostname, (*mapping)->readCap+IBP_URI_LEN, hostnamelen);
    (*mapping)->hostname[hostnamelen] = '\0';

    /* Gets the port number of the depot from the read cap and puts it into the
     * mapping */
    if((sscanf(strchr((*mapping)->readCap+IBP_URI_LEN, ':')+1, "%d", &(*mapping)->port)) != 1)
    {
        goto ERROR_HANDLER;
    }


    /* Sets the in warmer status to No */
    (*mapping)->inWarmer = 0;


    /* Return Successfully */
    return 0;

    /* Error Handler */
    ERROR_HANDLER:

        if(*mapping != NULL)
        {
            free(*mapping);
        }

        /* An error has occurred so return failure */
        return -1;
}



/****------------------------makeLoDNExnode()------------------------------****
 * Description: This function is used to make an exnode structure that
 *              represents the exnode.  It allocates the exnode, stores the
 *              name and initializes the structurs for the exnode. It then
 *              returns a pointer to this exnode.
 * Input: name - cstring that holds the name of the exnode.
 * Output: It returns a pointer to the allocated exnode.
 *
 * Warning: The returned pointer is to a heap allocated object that must be
 *          later freed.
 ****----------------------------------------------------------------------****/
LoDNExnode* makeLoDNExnode(char *name)
{
    /* Declarations */
    LoDNExnode *exnode;


    /* Creates the the exnode */
    if((exnode = (LoDNExnode*)malloc(sizeof(LoDNExnode))) == NULL)
    {
       logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Allocates and copies the name */
    if((exnode->name = strdup(name)) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Initializes the mutex lock for the exnode */
    if(pthread_mutex_init(&exnode->lock, NULL) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing mutex lock");
    }

    /* Initializes the exnode variables */
    exnode->status             = NULL;      /* Status of the exnode */
    exnode->brokenSince        = -1;        /* When the exnode became bad */
    exnode->numTasks           = 0;         /* Number of thread tasks */
    exnode->mappingsByOffset   = NULL;      /* The mappings by their position */
    exnode->segments           = NULL;      /* The segments in an exnode */

    exnode->remove             = 0;         /* When to remove the exnode */

    exnode->numBadSegments     = 0;         /* Num of bad segments (blocks)*/
    exnode->numCompleteCopies  = 0;         /* Num of complete copies */
    exnode->numFewCopySegments = 0;         /* Num segments with less 3 copies*/
    exnode->numSegments        = 0;         /* Num of segments */

    exnode->numWarmerCopies    = 0;			/* Number of warmer copies */

    exnode->filename=NULL;                  /* File name of the exnode */

    /* Returns a pointer to the exnode that's been created */
    return exnode;
}



/****------------------------freeLoDNExnode()------------------------------****
 * Description: This function frees the exnode and its underlying resources.
 * Input: exnode - LoDNExnode pointer to the allocated exnode.
 ****----------------------------------------------------------------------****/
void freeLoDNExnode(LoDNExnode *exnode)
{
    /* Declarations */
    LoDNExnodeSegment *segment;
    LoDNExnodeMapping *mapping;
    JRB                jnode1;


    /* If the mappingsByOffset structure was allocated then it must free this */
    if(exnode->mappingsByOffset != NULL)
    {
        /* Traverses the mappings in the structure */
        jrb_traverse(jnode1, exnode->mappingsByOffset)
        {
            mapping = (LoDNExnodeMapping*)jnode1->val.v;

            /* Frees the mappings and the IBP Caps if they exist */
            if(mapping != NULL)
            {
                /* Frees the read IBP Cap */
                if(mapping->readCap != NULL)
                {
                    free(mapping->readCap);
                }

                /* Frees the write IBP Cap */
                if(mapping->writeCap != NULL)
                {
                    free(mapping->writeCap);
                }

                /* Frees the manage IBP Cap */
                if(mapping->manageCap != NULL)
                {
                    free(mapping->manageCap);
                }

                /* Frees the mappings */
                free(mapping);
            }
        }

        /* Frees the tree structure */
        jrb_free_tree(exnode->mappingsByOffset);
    }

    /* If the segments structure is allocated then it frees its resources */
    if(exnode->segments != NULL)
    {
        /* Traverses the tree for the structure and frees its segments */
        jrb_traverse(jnode1, exnode->segments)
        {
            segment = (LoDNExnodeSegment*)jnode1->val.v;

            /* If the segment has been allocated then free it */
            if(segment != NULL)
            {
                /* Frees the segments mappings (same mappings in the above
                 * structure so the mapping themselves are already freed */
                if(segment->mappings != NULL)
                {
                    /* Frees the dllist that holds the mappings for each
                     * segment */
                    free_dllist(segment->mappings);
                }

                /* Frees the segment */
                free(segment);
            }
        }

        /* Frees the tree that represents the segment structure */
        jrb_free_tree(exnode->segments);
    }

    /* Frees the status cstring */
    if(exnode->status != NULL)
    {
        free(exnode->status);
    }

    /* Frees the name cstring */
    if(exnode->name != NULL)
    {
        free(exnode->name);
    }

    /* Frees the exnode */
    free(exnode);
}



/****------------------------LoDNExnodeCollection()------------------------****
 * Description: This function creates a collection of exnodes from a list of
 *              paths that it scans for exnodes recursively.  It allocates
 *              an exnode structure that represents a physical exnode on the
 *              system under one of the given paths. It returns a pointer to
 *              this collection.
 * Input: paths - Dllist of the paths where each path is a cstring that is a
 *                directory path.
 * Output: It returns a pointer to the allocated collection.
 *
 * Warning: This function returns a pointer to heap allocated structure that
 *          it and its underlying parts must later be freed.
 ****----------------------------------------------------------------------****/
LoDNExnodeCollection *buildExnodeCollection(Dllist paths)
{
    /* Declarations */
    LoDNExnodeCollection *collection = NULL;
    Dllist directories               = NULL;
    Dllist dlnode1;
    DIR             *dir;
    struct dirent   entry;
    struct dirent  *result;
    int    status;
    char   fullname[PATH_MAX];
    struct stat statbuf;
    LoDNExnode *exnode;


    /* Allocates the collection */
    if((collection = (LoDNExnodeCollection*)malloc(sizeof(LoDNExnodeCollection))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Allocates a list for holding the exnodes and a list for holding the
     * directories to traverse */
    if((collection->exnodes = new_dllist()) == NULL || (directories = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Initializes the internal collection variables */
    collection->numExnodes  = 0;
    collection->iteratorPtr = collection->exnodes;

    /* Traverses the path list and puts exnodes into the xnode list and
     * directories into the directory list to be traversed */
    dll_traverse(dlnode1, paths)
    {
        /* Gets a statbuf for the current path */
        if(lstat(dlnode1->val.s, &statbuf) != 0)
        {
            continue;
        }

        /* Detects if the current entry is an exnode and if so adds it to the collection */
        if(S_ISREG(statbuf.st_mode) && strlen(dlnode1->val.s) > 4 &&
                   strcmp(dlnode1->val.s+strlen(dlnode1->val.s)-4, ".xnd") == 0)
        {
            /* Allocates the exnode and adds it to the list */
            exnode = makeLoDNExnode(dlnode1->val.s);
            dll_append(collection->exnodes, new_jval_v(exnode));

            /* Increments the number of Exnodes */
            collection->numExnodes++;

        /* Detects if it is a directory and adds it to the list of directories to
         * be traversed */
        }else if(S_ISDIR(statbuf.st_mode))
        {
            dll_append(directories, new_jval_s(strdup(dlnode1->val.s)));
        }
    }

    /* Traverses the list of directories */
    dll_traverse(dlnode1, directories)
    {
        /* Opens the directory */
        dir = opendir(dlnode1->val.s);

        /* Reads in the entries in the directory */
        while((status = readdir_r(dir, &entry, &result)) == 0 && result != NULL)
        {
            /* Skips entries that start with . */
            if(entry.d_name[0] == '.')
            {
                continue;
            }

            /* Appends the directory to the list to be searched */
            if(entry.d_type == DT_DIR)
            {
                sprintf(fullname, "%s/%s", dlnode1->val.s, entry.d_name);

                dll_append(directories, new_jval_s(strdup(fullname)));

            /* Adds the exnode to the list of exnodes */
            }else if(entry.d_type == DT_REG && strlen(entry.d_name) > 4 &&
                     strcmp(entry.d_name+strlen(entry.d_name)-4, ".xnd") == 0)
            {
                sprintf(fullname, "%s/%s", dlnode1->val.s, entry.d_name);

                exnode = makeLoDNExnode(fullname);
                dll_append(collection->exnodes, new_jval_v(exnode));

                /* Increments the number of Exnodes */
                collection->numExnodes++;
            }
        }

        free(dlnode1->val.s);
        closedir(dir);
    }

    /* Frees the list of directories */
    free_dllist(directories);

    /* Returns a pointer to the collection */
    return collection;
}



/****-----------------------freeLoDNExnodeCollection()---------------------****
 * Description:  This function frees the exnode collection its its underlying
 *               parts.
 * Input: collection - pointer to the LoDNExnodeCollection.
 ****----------------------------------------------------------------------****/
void freeLoDNExnodeCollection(LoDNExnodeCollection *collection)
{
    /* Declarations */


    /* Frees the exnodes of the collection */
    free_dllist(collection->exnodes);

    /* Frees the collection structure itself */
    free(collection);
}



/****--------------LoDNExnodeCollectionGetNextExnode()---------------------****
 * Description: This function returns the next exnode in the list of exnodes
 *              for a LoDN Exnode Collection. Once the end of the list has
 *              been reached it returns NULL and resets the list so that the
 *              next call will start at the beginning of the list again.
 * Input: collection - pointer to the LoDNExnodeCollection structure.
 * Output: It returns a LoDNExnode pointer to the next exnode.
 ****----------------------------------------------------------------------****/
LoDNExnode *LoDNExnodeCollectionGetNextExnode(LoDNExnodeCollection *collection)
{
    /* Declarations */
    Dllist      node;


    /* Gets the next node after the current position */
    node = dll_next(collection->iteratorPtr);

    /* If the next node is the top of the list then return NULL b/c the end of the
     * list has been reached. */
    if(node == collection->exnodes)
    {
        return NULL;
    }

    /* Updates the iterator pointer*/
    collection->iteratorPtr = node;

    /* Returns the exnode */
    return (LoDNExnode*)node->val.v;
}



/****---------------------LoDNExnodeCreateMapping()------------------------****
 * Description: This function creates a LoDNExnodeMapping using the caps and
 *              attributes that are passed to the function.  It then returns
 *              a pointer to the mapping for use.
 * Input: caps - IBP_set_of_caps pointer to the structure containing the
 *               read, write, manage IBP Caps.
 *        alloc_length - long long that has the length of the mapping
 *                       allocation.
 *        alloc_offset - long long that has the offset within the mapping of the
 *                       start of the data.
 *        exnode_offset - long long that has the offset of the data within the
 *                        complete file.
 *        logical_length - long long that has the length of the data.
 * Output: It returns a pointer to the LoDNExnodeMapping that gets allocated or
 *         NULL on error.
 *
 * Warning: This function returns a pointer to a heap allocated structure that
 *          must be freed.
 ****---------------------------------------------------------------------****/
LoDNExnodeMapping *LoDNExnodeCreateMapping(IBP_set_of_caps caps,
                                           long long alloc_length,
                                           long long alloc_offset,
                                           long long exnode_offset,
                                           long long logical_length)
{
    /* Declarations */
    LoDNExnodeMapping *mapping = NULL;
    int                hostnamelen;


    /* Allocates a new mapping */
    if((mapping = (LoDNExnodeMapping*)calloc(1, sizeof(LoDNExnodeMapping))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }


    /* Copies the data */
    mapping->alloc_length   = alloc_length;
    mapping->alloc_offset   = alloc_offset;
    mapping->exnode_offset  = exnode_offset;
    mapping->logical_length = logical_length;

    /* Copies the mappings */
    mapping->readCap   = strndup(caps->readCap, MAX_CAP_LEN);
    mapping->writeCap  = strndup(caps->writeCap, MAX_CAP_LEN);
    mapping->manageCap = strndup(caps->manageCap, MAX_CAP_LEN);


    /* Checks that the allocations are valid */
    if(mapping->readCap == NULL || mapping->writeCap == NULL ||
       mapping->manageCap == NULL)
    {
       logPrintError(0, __FILE__, __LINE__, 1,  "Memory Allocation Error");
    }

    /* Gets the length of the hostname */
    hostnamelen = strchr(mapping->readCap+IBP_URI_LEN, ':')-(mapping->readCap+IBP_URI_LEN);

    if(hostnamelen < 0 || hostnamelen > MAXHOSTNAMELEN)
    {
        goto ERROR_HANDLER;
    }

    /* Copies the hostname into the mapping hostname */
    strncpy(mapping->hostname, mapping->readCap+IBP_URI_LEN, hostnamelen);
    mapping->hostname[hostnamelen] = '\0';

    /* Copies the port number into the mapping */
    if((sscanf(strchr(mapping->readCap+IBP_URI_LEN, ':')+1, "%d", &(mapping->port))) != 1)
    {
        goto ERROR_HANDLER;
    }

    /* Sets the status to good */
    mapping->status = 1;


    /* Return the mapping */
    return mapping;


    /* Error handler */
    ERROR_HANDLER:

        if(mapping == NULL)
        {
            if(mapping->readCap != NULL)
            {
                free(mapping->readCap);
            }

            if(mapping->writeCap != NULL)
            {
                free(mapping->writeCap);
            }

            if(mapping->manageCap != NULL)
            {
                free(mapping->manageCap);
            }

            free(mapping);
        }

        /* Error occurred so return NULL for failure */
        return NULL;
}



/****------------------------LoDNExnodeReadExnode()------------------------****
 * Description: This function takes an exnode and reads in the corresponding
 *              exnode file.  It stores the attributes and mappings of the
 *              exnode into the exnode.
 * Input: exnode - pointer to the LoDNExnode structure.
 * Output: It returns 0 on success and -1 on failure.
 ****----------------------------------------------------------------------****/
int LoDNExnodeReadExnode(LoDNExnode *exnode)
{
    /* Declarations */
    int      fd      = -1;
    char    *buf     = NULL;
    size_t   amtRead = 0;
    ssize_t  count   = 0;
    struct  stat       fileStatus;
    char    *sep       = NULL;
    Exnode  *stdExnode = NULL;
    ExnodeMetadata    *exnodeMetadata = NULL;
    ExnodeEnumeration *exnodeEnum  = NULL;
    ExnodeMapping     *stdMapping      = NULL;
    LoDNExnodeMapping *mapping;
    ExnodeValue        value;
    ExnodeType         type;
    int                retval;


    logPrint(3, "Reading in data for exnode %s.", exnode->name);

    /* Gets the file size of the exnode */
    if(stat(exnode->name, &fileStatus) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error obtaining attributes for exnode %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Allocates a buffer to hold the exnode */
    if((buf = (char*)malloc(fileStatus.st_size*sizeof(char))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Opens the exnode file for reading */
    if((fd = open(exnode->name, O_RDONLY)) < 0)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error opening exnode file %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Reads the exnode file into the buffer */
    while(amtRead < fileStatus.st_size)
    {
        /* Read as much as possible at a time */
        if((count = read(fd, buf+amtRead, fileStatus.st_size-amtRead)) < 0)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "Error reading from exnode file %s", exnode->name);
            goto ERROR_HANDLER;
        }

        /* Update the amount read */
        amtRead += count;
    }

    /* Uses the buffer to create the exnode */
    if(exnodeDeserialize(buf, fileStatus.st_size, &stdExnode) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error deserializing exnode file %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Gets the metadata */
    if(exnodeGetExnodeMetadata(stdExnode, &exnodeMetadata) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error getting metadata for exnode file %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Gets the filename of the exnode */
    if(exnodeGetMetadataValue(exnodeMetadata, "filename", &value, &type) != EXNODE_SUCCESS ||
       value.s == NULL || type != STRING)
    {
        if((sep = strrchr(exnode->name, '/')+1) == NULL+1)
        {
            sep = exnode->name;
        }

        if((exnode->filename = strdup(sep)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        if((sep = strrchr(exnode->filename, '.')) != NULL)
        {
            *sep = '\0';
        }

    }else
    {
        if((exnode->filename = strdup(value.s)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }


    /* Gets the lors version of the exnode */
    if(exnodeGetMetadataValue(exnodeMetadata, "lorsversion", &value, &type) != EXNODE_SUCCESS ||
       type != DOUBLE)
    {
        exnode->lorsversion = -1;
    }else
    {
        exnode->lorsversion = value.d;
    }

    /* Gets the status data */
    if(exnodeGetMetadataValue(exnodeMetadata, "status", &value, &type) != EXNODE_SUCCESS ||
       value.s == NULL || type != STRING)
    {
        if((exnode->status = strdup("UNKNOWN")) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }else
    {
        if((exnode->status = strdup(value.s)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }

    /* Detects if the exnode is broken and if it is then it gets how long it
     * has been broken for */
    if(strcmp(exnode->status, "BROKEN") == 0)
    {
        if(exnodeGetMetadataValue(exnodeMetadata, "broken_since", &value, &type)
            != EXNODE_SUCCESS || type != INTEGER)
        {
            exnode->brokenSince = -1;
        }else
        {
            exnode->brokenSince = value.i;
        }
    }

    /* Gets a list of the mappings in the exnode */
    if(exnodeGetMappings(stdExnode, &exnodeEnum) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Can't get exnode mappings for %s",  exnode->name);
        goto ERROR_HANDLER;
    }


    /* Allocates the tree to hold the mappings by their offset */
    if((exnode->mappingsByOffset = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Traverses each mapping in the exnode file */
    while((retval = exnodeGetNextMapping(exnodeEnum, &stdMapping))==EXNODE_SUCCESS &&
            stdMapping != NULL)
    {
        if(LoDNExnodeReadMapping(stdMapping, &mapping) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "Error reading in exnode mapping for exnode %s", exnode->name);
            continue;
        }

        if(jrb_insert_gen(exnode->mappingsByOffset, new_jval_ll(mapping->exnode_offset),
                          new_jval_v(mapping), jrb_ll_cmp) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        mapping->exnode = exnode;
    }

    /* If the iteration fails then it aborts. */
    if(retval != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error traversing exnode mapping for %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Fress the resources */
    free(buf);
    close(fd);
    exnodeDestroyExnode(stdExnode);
    exnodeDestroyEnumeration(exnodeEnum);


    /* Return successfully */
    return 0;


    /* Error handler */
    ERROR_HANDLER:

        /* Frees all of the resources */
        if(buf != NULL)
        {
            free(buf);

            if(fd != -1 )
            {
                close(fd);

                if(exnode != NULL)
                {
                    exnodeDestroyExnode(stdExnode);

                    if(exnodeEnum != NULL)
                    {
                        exnodeDestroyEnumeration(exnodeEnum);
                    }
                }
            }
        }

        /* Return unsuccessfully */
        return -1;

}



/****-------------------------LoDNExnodeWriteExnode()----------------------****
 * Description: This function writes the provided LoDN exnode to a file
 *              specified by the outputFilename using the libExnode api.  It
 *              writes all of the good mappings and the attribute values
 *              including the current status of the exnode and if the exnode
 *              is "broken" how long its been broken.
 * Input: exnode - pointer to the LoDNExnode structure.
 *        outputFilename - cstring to the name of the output file.
 * Output: It returns 0 on success and -1 on failure.
 ****----------------------------------------------------------------------****/
int LoDNExnodeWriteExnode(LoDNExnode *exnode, char *outputFilename,
							LoDNFreeMappings_t *freeMappings)
{
    /* Declarations */
    LoDNExnodeMapping *mapping;
    LoDNExnodeMapping *lastmapping = NULL;
    LoDNExnodeMapping *nextmapping = NULL;
    Exnode *outputExnode = NULL;
    ExnodeMetadata *warmedExnodeMetadata = NULL;
    ExnodeMetadata *mappingMetadata;
    ExnodeValue        value;
    ExnodeMapping *stdMapping;
    char   *buf       = NULL;
    int     buflen    = 0;
    int     fd        = -1;
    size_t  amtWrote  = 0;
    ssize_t count     = 0;
    ExnodeValue    statusValue;
    ExnodeValue    copyValue;
    ExnodeValue    filenameValue;
    ExnodeValue    lorsversionValue;
    JRB jnode;
    JRB jnode2;
    time_t          currTime;
    ExnodeValue     brokenSinceValue;
    ExnodeValue     filesize;
    long long       currPos = 0;
    long long       span   = 0;
    ExnodeValue numSegmentsValue;
    ExnodeValue numFewCopySegmentsValue;
    ExnodeValue numBadSegmentsValue;
    ExnodeValue lastWarmedValue;
    char        strtime[26];
    IBP_cap free_cap;


    /* Creates a new libexnode exnode */
    if(exnodeCreateExnode(&outputExnode) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error creating exnode");
        goto ERROR_HANDLER;
    }

    /* Gets the metadata struct for the new exnode */
    if(exnodeGetExnodeMetadata(outputExnode, &warmedExnodeMetadata) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error getting metadata from exnode");
        goto ERROR_HANDLER;
    }

    brokenSinceValue.i = -1;

    /* Gets the current time */
    if((currTime = time(NULL)) == (time_t)-1)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error getting current time");
    }


    /* Sets the status value */
    if(exnode->numCompleteCopies >= 3)
    {
        statusValue.s = "GOOD";

    }else if(exnode->numCompleteCopies > 0)
    {
        statusValue.s = "DANGER";

    }else if(exnode->numCompleteCopies == 0)
    {
        /* If its been broken for too long then mark it as just being dead */
        if(exnode->brokenSince != -1 &&
           currTime-exnode->brokenSince >= BROKEN2DEADTIME)
        {
            statusValue.s = "DEAD";

        /* Not Dead, just broken */
        }else
        {
            statusValue.s = "BROKEN";

            /* Get for how long its been broken */
            if(exnode->brokenSince == -1)
            {
                brokenSinceValue.i = (int)currTime;
            }else
            {
                brokenSinceValue.i = exnode->brokenSince;
            }
        }
    }

    logPrint(2, "Setting status of exnode %s to %s", exnode->filename, statusValue.s);

    /* Converts time to string */
    ctime_r(&currTime, strtime);

    /* Gets the number of complete copies, filename */
    copyValue.i               = exnode->numCompleteCopies;
    filenameValue.s           = exnode->filename;
    numSegmentsValue.i        =  exnode->numSegments;
    numFewCopySegmentsValue.i = exnode->numFewCopySegments;
    numBadSegmentsValue.i     =  exnode->numBadSegments;
    lastWarmedValue.s         = strtime;

    /* Sets the status and number of copies metadata for the exnode */
    if(exnodeSetMetadataValue(warmedExnodeMetadata, "status", statusValue, STRING, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(warmedExnodeMetadata, "number_of_copies", copyValue, INTEGER, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(warmedExnodeMetadata, "filename", filenameValue, STRING, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(warmedExnodeMetadata, "warmer_number_segments", numSegmentsValue, INTEGER, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(warmedExnodeMetadata, "warmer_number_few_segments", numFewCopySegmentsValue, INTEGER, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(warmedExnodeMetadata, "warmer_number_bad_segments", numBadSegmentsValue, INTEGER, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(warmedExnodeMetadata, "warmer_last_warmed", lastWarmedValue, STRING, TRUE) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "error writing metadata to %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Sets how long the exnode has been broken for */
    if(brokenSinceValue.i != -1)
    {
        if(exnodeSetMetadataValue(warmedExnodeMetadata, "broken_since", brokenSinceValue,
            INTEGER, TRUE) != EXNODE_SUCCESS)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "error writing metadata to %s", exnode->name);
            goto ERROR_HANDLER;
        }
    }

    /* Sets the lorsversion used to create the exnode */
    if(exnode->lorsversion >= 0)
    {
        lorsversionValue.d = exnode->lorsversion;

        if(exnodeSetMetadataValue(warmedExnodeMetadata, "lorsversion", lorsversionValue, DOUBLE, TRUE) != EXNODE_SUCCESS)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "error writing metadata to %s", exnode->name);
            goto ERROR_HANDLER;
        }
    }

    /* Initialize the filesize of the file represented by the exnode. */
    filesize.i = 0;

    /* Traverses the mappings in the exnode by their offsets */
    jrb_traverse(jnode, exnode->mappingsByOffset)
    {
        mapping = (LoDNExnodeMapping*)jnode->val.v;

		/* Doesn't write mappings out that aren't in the warmer file if
		 * the warmer policy has been meet. */
        /*if(mapping->inWarmer == 0 && exnode->numWarmerCopies >= 3)
        {


        	if((free_cap = strndup(mapping->readCap, MAX_CAP_LEN)) == NULL)
        	{
        		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        	}

        	LoDNFreeMappingsAppend(freeMappings, free_cap);

        	dll_append(freeMappings, new_jval_s(free_cap));
        	continue;
        }*/

        /* Calculates the size of the file represented by the exnode. */
        if((mapping->exnode_offset + mapping->logical_length) > currPos)
        {
        	span        = mapping->exnode_offset + mapping->logical_length - currPos;
        	filesize.i += (mapping->logical_length < span) ?
        									mapping->logical_length : span;
        	currPos     = mapping->exnode_offset + mapping->logical_length;
        }

        /* Skips bad mappings ( <= -1 is not currently used so all mappings
         * pass this test  */
       if(mapping->status >= -1)
       {
//        	/* Detects if the current mapping is dead and if so figures out to
//        	 * skip it or write it to the exnode (at least 1 mapping for
//        	 * each segment must be present even if its dead) */
//        	if(mapping->status < 1 && mapping->brokenSince != -1 && currTime-mapping->brokenSince >= BROKEN2DEADTIME)
//        	{
//        	   /* The last mapping which was written represents the same segment
//        	    * so we can skip this dead mapping */
//        	   if(lastmapping != NULL && mapping->exnode_offset == lastmapping->exnode_offset
//        	      && mapping->logical_length == lastmapping->logical_length)
//        	   {
//        		  continue;
//
//        	   /* The last mapping wasn't so we need to check if there are
//        	    * any good or broken mappings that can be written so we
//        	    * can skip this dead one (and all of the other dead ones) */
//        	   }else
//        	   {
//        		   /* Starting iterating over the mappings past the current one */
//        		   while((jnode2 = jrb_next(jnode)) != exnode->mappingsByOffset)
//        		   {
//        			   nextmapping = (LoDNExnodeMapping*)jnode2->val.v;
//
//        			   /* The next mapping is for the same segment so test
//        			    * if is not dead */
//        			   if(mapping->exnode_offset == nextmapping->exnode_offset &&
//        				  mapping->logical_length == nextmapping->logical_length)
//        			   {
//        				   /* Mapping is not dead so well use it instead and skip all
//        				    * of the dead ones that have been encountered thus far */
//        				   if(mapping->status > 0 || nextmapping->brokenSince == -1 ||
//        						   currTime-mapping->brokenSince < BROKEN2DEADTIME)
//        				   {
//        					   jnode   = jnode2;
//        					   mapping = nextmapping;
//
//        					   break;
//        				   }
//
//        			   /* Out of mappings for this segment so must write it
//        			    * even though its dead*/
//        			   }else
//        			   {
//        				   break;
//        			   }
//        		   }
//        	   }
//        	}

            /* Create a mapping  and put into good mappings */
            if(exnodeCreateMapping(&stdMapping) != EXNODE_SUCCESS)
            {
                logPrintError(0, __FILE__, __LINE__, 0, "Error creating new mapping.");
                goto ERROR_HANDLER;
            }

            /* Creates a new metadata struct for the mapping */
            if(exnodeGetMappingMetadata(stdMapping, &mappingMetadata) != EXNODE_SUCCESS)
            {
            	logPrintError(0, __FILE__, __LINE__, 0, "Error getting mapping metadata.");
                goto ERROR_HANDLER;
            }

            /* Fills in the metadata struct with the block data */
            value.i = mapping->alloc_length;
            exnodeSetMetadataValue(mappingMetadata, "alloc_length", value, INTEGER, TRUE);

            value.i = mapping->alloc_offset;
            exnodeSetMetadataValue(mappingMetadata, "alloc_offset", value, INTEGER, TRUE);

            value.i = mapping->exnode_offset;
            exnodeSetMetadataValue(mappingMetadata, "exnode_offset", value, INTEGER, TRUE);

            value.i = mapping->logical_length;
            exnodeSetMetadataValue(mappingMetadata, "logical_length", value, INTEGER, TRUE);

            /* Initializes the broken value */
            brokenSinceValue.i = -1;


            /* Good status */
            if(mapping->status > 0)
            {
            	statusValue.s = "GOOD";

            /* Dead status */
            }else if(mapping->brokenSince != -1 &&
                     currTime-mapping->brokenSince >= BROKEN2DEADTIME)
            {
            	statusValue.s = "DEAD";

            /* Broken status */
            }else
            {
            	statusValue.s = "BROKEN";

            	/* First time broken */
                if(mapping->brokenSince == -1)
                {
                	brokenSinceValue.i = (int)currTime;

                /* Broken before */
                }else
                {
                	brokenSinceValue.i = mapping->brokenSince;
                }
            }

            /* Sets the status of the mapping */
            if(exnodeSetMetadataValue(mappingMetadata, "status", statusValue,
            		                  STRING, TRUE) != EXNODE_SUCCESS)
            {
            	logPrintError(0, __FILE__, __LINE__, 0, "error writing metadata to %s", exnode->name);
                goto ERROR_HANDLER;
            }

            /* Sets how long the mapping has been broken for */
            if(brokenSinceValue.i != -1)
            {
                if(exnodeSetMetadataValue(mappingMetadata, "broken_since",
                		     brokenSinceValue, INTEGER, TRUE) != EXNODE_SUCCESS)
                {
                    logPrintError(0, __FILE__, __LINE__, 0, "error writing metadata to %s", exnode->name);
                    goto ERROR_HANDLER;
                }
            }

            /* Stores the capabilities into the mapping */
            if(exnodeSetCapabilities(stdMapping, mapping->readCap, mapping->writeCap,
                                        mapping->manageCap, TRUE) != EXNODE_SUCCESS)
            {
            	logPrintError(0, __FILE__, __LINE__, 0, "Error setting the capabilities for a mapping.");
                goto ERROR_HANDLER;
            }

            /* Appends current mapping to the new warmed exnode */
            if(exnodeAppendMapping(outputExnode, stdMapping) != EXNODE_SUCCESS)
            {
            	logPrintError(0, __FILE__, __LINE__, 0, "Error creating exnode.");
                goto ERROR_HANDLER;
            }

            lastmapping = mapping;
        }
    }

    /* Writes the filesize of the exnode */
    if(exnodeSetMetadataValue(warmedExnodeMetadata, "original_filesize", filesize, INTEGER, TRUE) != EXNODE_SUCCESS)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "error writing metadata to %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Takes the exnode and fills in the buffer with a "serialized" copy */
    if(exnodeSerialize(outputExnode, &buf, &buflen) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 0,
        		      "Error serializing exnode file %s.", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Opens the exnode file for writing removing any previous data */
    if((fd = open(exnode->name, O_WRONLY | O_TRUNC)) < 0)
    {
    	logPrintError(0, __FILE__, __LINE__, 0,
    			      "Error opening exnode file %s", exnode->name);
        goto ERROR_HANDLER;
    }

    /* Writes all of the serialized exnode into the output file */
    while(amtWrote < buflen)
    {
        if((count = write(fd, buf+amtWrote, buflen-amtWrote)) < 0)
        {
            logPrintError(0, __FILE__, __LINE__, 0,
            		      "Error writing to exnode file %s", exnode->name);
            goto ERROR_HANDLER;
        }

        amtWrote += count;
    }

    /* Free the resources */
    exnodeDestroyExnode(outputExnode);
    free(buf);
    close(fd);


    /* Return Succesfully */
    return 0;


    /* Error Handler */
    ERROR_HANDLER:

        if(outputExnode != NULL)
        {
            exnodeDestroyExnode(outputExnode);
        }

        return -1;
}



/****-----------------------LoDNExnodeInsertMapping()----------------------****
 * Description: This function inserts a mapping into a LoDN Exnode. It puts
 *              it both into the mappingsbyOffset list and the segment list
 *              if they exist.
 * Input: exnode - pointer to the LoDNExnode to insert the mapping into.
 *        mapping - pointer to the LoDNExnodeMapping to insert.
 * Output: It returns 0.
 ****----------------------------------------------------------------------****/
int LoDNExnodeInsertMapping(LoDNExnode *exnode, LoDNExnodeMapping *mapping)
{
    /* Declarations */


    /* Assigns the exnode for the mapping */
    mapping->exnode = exnode;

    /* Inserts the mapping into the mappings by offset if it has been created */
    if(exnode->mappingsByOffset != NULL)
    {
        if(jrb_insert_gen(exnode->mappingsByOffset, new_jval_ll(mapping->exnode_offset),
                          new_jval_v(mapping), jrb_ll_cmp) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }

    /* Inserts the mapping into the segment tree if it has been created */
    if(exnode->segments != NULL)
    {
        _insert_mapping_into_segment(exnode, mapping);
    }


    /* Return Successfully */
    return 0;
}



/****---------------------LoDNExnodeRemoveBadMappings()--------------------****
 * Description: This function scans through the list of mappings for an exnode
 *              and removes any bad mapping for the exnode and frees the
 *              resources tied to those mappings.
 * Input: exnode - pointer to the LoDNExnode structure.
 * Output: It returns 0 for success.
 ****----------------------------------------------------------------------****/
int LoDNExnodeRemoveBadMappings(LoDNExnode *exnode)
{
    /* Declarations */
    LoDNExnodeMapping *mapping;
    JRB jnode1;
    JRB jnode2;


    /* Traverses the mappings and removes the bad mappings */
    for(jnode2=jrb_first(exnode->mappingsByOffset);
        jnode2 != exnode->mappingsByOffset;)
    {
        jnode1 = jnode2;
        jnode2 = jrb_next(jnode2);

        mapping = (LoDNExnodeMapping*)jnode1->val.v;

        /* Mapping is bad so remove it from the exnode and frees the memory
         * allocated to the mapping */
        if(mapping->status < 0)
        {
            jrb_delete_node(jnode1);

            free(mapping->manageCap);
            free(mapping->readCap);
            free(mapping->writeCap);
            free(mapping);
        }
    }

    /* Return Successfully */
    return 0;
}



/****---------------------LoDNExnodeGetSegments()--------------------------****
 * Description: This function creates a segment list within the exnode to
 *              hold all of the mappings by their segment portions. It then
 *              traverses the list of mapping and put them into the segment
 *              list.
 * Input: exnode - pointer to the LoDNExnode structure.
 * Output: It returns 0.
 ****----------------------------------------------------------------------****/
int LoDNExnodeGetSegments(LoDNExnode *exnode)
{
    /* Declarations */
    LoDNExnodeMapping *mapping;
    JRB jnode1;



    /* If the segments list is not empty then it returns 0 */
    if(exnode->segments != NULL && jrb_empty(exnode->segments) != 1)
    {
        return 0;
    }

    /* If no segment list has been allocated then it allocates a empty list */
    if(exnode->segments == NULL)
    {
        /* Allocates a segment tree */
        if((exnode->segments = make_jrb()) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }

    /* Traverses the mapping list and puts the mappings into the segment lists*/
    jrb_traverse(jnode1, exnode->mappingsByOffset)
    {
        mapping = (LoDNExnodeMapping*)jnode1->val.v;

        /* Inserts the mapping */
        _insert_mapping_into_segment(exnode, mapping);
    }

    /* Return Success */
    return 0;
}



/****-----------------------LoDNExnodeCalculateStatus()--------------------****
 * Description: This function takes an exnode and uses its segment list to
 *              calculate the current status of the exnode in terms of the
 *              number of complete copies. number of segments, number of bad
 *              segments and the number of segments that have less than 3
 *              mappings.
 * Input: exnode - pointer to the LoDNExnode structure.
 * Output: It returns 0 on success.
 ****----------------------------------------------------------------------****/
int LoDNExnodeCalculateStatus(LoDNExnode *exnode)
{
    /* Declarations */
    LoDNExnodeSegment *segment;
    LoDNExnodeMapping *mapping;
    JRB                jnode1;
    Dllist             dlnode1;
    int                numGoodMappings;
    int                numCompleteCopies = INT_MAX;
    int                numWarmerMappings = 0;
    int                numWarmerCopies = INT_MAX;


    /* Initializes the status variables for the exnode */
    exnode->numCompleteCopies  = 0;
    exnode->numSegments        = 0;
    exnode->numBadSegments     = 0;
    exnode->numFewCopySegments = 0;
    exnode->numWarmerCopies    = 0;

    /* If there are is no segment list then it creates the segment list */
    if(exnode->segments == NULL)
    {
        if(LoDNExnodeGetSegments(exnode) != 0)
        {
            return -1;
        }
    }

    /* Traverses the segment list */
    jrb_traverse(jnode1, exnode->segments)
    {
        segment = (LoDNExnodeSegment*)jnode1->val.v;

        /* Initializes the the number of good mappings for the current
         * segment */
        numGoodMappings = 0;
        numWarmerMappings = 0;

        /* Traverses the mappings for the segment and counts the number of
         * good mappings */
        dll_traverse(dlnode1, segment->mappings)
        {
            mapping = (LoDNExnodeMapping*)dlnode1->val.v;

            /* Good mapping so increment count */
            if(mapping->status >= 0)
            {
                numGoodMappings++;

                if(mapping->inWarmer)
                {
                	numWarmerMappings++;
                }
            }
        }

        /* Increments the number of segments in the exnode */
        exnode->numSegments++;

        /* If there are no good mappings then increment the number of bad
         * segments */
        if(numGoodMappings == 0)
        {
            exnode->numBadSegments++;

        /* If there are less than 3 good mappings then increment the number of
         * few copy segments */
        }else if(numGoodMappings < 3)
        {
            exnode->numFewCopySegments++;
        }

        /* Sets the number of complete copies for the whole exnode thus far */
        numCompleteCopies = (numGoodMappings < numCompleteCopies) ?
                                            numGoodMappings : numCompleteCopies;

        /* Sets the number of complete warmer specified copies for the exnode
         * thus far. */
        numWarmerCopies = (numWarmerMappings < numWarmerCopies) ?
											numWarmerMappings : numWarmerCopies;
    }

    /* Stores the number of complete copies */
    exnode->numCompleteCopies = numCompleteCopies;
    exnode->numWarmerCopies   = numWarmerCopies;

    /* Print exnode status */
    logPrint(2, "%s exnode has %d segments", exnode->name, exnode->numSegments);
    logPrint(2, "%s exnode has %d bad segments", exnode->name, exnode->numBadSegments);
    logPrint(2, "%s exnode has %d segments with less than 3 copies", exnode->name,
    		 exnode->numFewCopySegments);
    logPrint(2, "%s exnode has %d complete copies", exnode->name,
    		 exnode->numCompleteCopies);

    /* Return Successfully */
    return 0;
}


/****------------------------buildExnodeIterator()-------------------------****
 * Description: This function creates a LoDNExnodeIterator for the series
 *              of paths that were provided.  The iterator that is returned
 *              can be used to retrieve exnodes from the path one at a time.
 * Input: paths - Dllist that contains the cstrings of the starting paths.
 * Ouput: It returns a heap allocated LoDNExnodeIterator if success and NULL
 *        on error.
 ****----------------------------------------------------------------------****/
LoDNExnodeIterator *buildExnodeIterator(Dllist paths)
{
    /* Declarations */
    LoDNExnodeIterator *iterator = NULL;
    char               *tmpDir   = NULL;
    Dllist dlnode1;


    /* Allocates the collection */
    if((iterator = (LoDNExnodeIterator*)malloc(sizeof(LoDNExnodeIterator))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Allocates a list for holding the exnodes and a list for holding the
     * directories to traverse */
    if((iterator->paths = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Intializes the directory entry */
    iterator->currDir = NULL;
    iterator->cwd     = NULL;

    /* Adds the directories to the iterator's list and makes full copies */
    dll_traverse(dlnode1, paths)
    {
    	if(dlnode1->val.s != NULL)
    	{
    		/* Allocate a new copy */
    		if((tmpDir = strdup(dlnode1->val.s)) == NULL)
    		{
    			logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    		}

    		/* Appends the directory name to the list */
    		dll_append(iterator->paths, new_jval_s(tmpDir));
    	}
    }

   /* Returns an iteraor for the exnodes */
   return iterator;
}



/****-------------------------exnodeIteratorNext()-------------------------****
 * Description: This functions uses the LoDNExnodeIterator that is passed in
 *              to recursively search and return the next LoDNExnode.  It
 *              recursively searches for exnodes starting at the provided
 *              paths and moving through the subdirectories.  It returns
 *              NULL once there are no more exnodes.
 * Input: iterator - pointer to the LoDNExnodeIterator to use.
 * Ouput: It returns a pointer to the heap allocated LoDNExnode that
 *        represents the next exnode. It returns NULL once the supply of
 *        exnodes has been exhausted.
 ****----------------------------------------------------------------------****/
LoDNExnode *LoDNExnodeIteratorNext(LoDNExnodeIterator *iterator)
{
	/* Declarations */
	Dllist dlnode1;
	char   *path           = NULL;
	struct dirent  *result = NULL;
	char   fullname[PATH_MAX];
	struct stat statbuf;
	LoDNExnode *exnode     = NULL;
    char *tmpptr           = NULL;


	/* Loops until the exnode has been found or the directories are used up */
	while(exnode == NULL && (!dll_empty(iterator->paths) || iterator->currDir != NULL))
	{
		path = NULL;

		/* Loops until the path variable is assigned. */
		do
		{
			/* No current directory so trying one of the paths */
			if(iterator->currDir == NULL)
			{
				/* Pulls the next path of the list */
				dlnode1 = dll_first(iterator->paths);
				path = dlnode1->val.s;
				dll_delete_node(dlnode1);

			/* Using an entry from the current directory */
			}else
			{
				/* Gets an entry from the directory */
				if(readdir_r(iterator->currDir, &(iterator->entry), &result) != 0)
				{
					logPrintError(0, __FILE__, __LINE__, 0, "Couldn't read directory entry");
				}

				/* Exhausted the contents of the directory so close the directory
				 * mark it */
				if(result == NULL)
				{
					closedir(iterator->currDir);
					free(iterator->cwd);
					iterator->currDir = NULL;
					iterator->cwd     = NULL;

				/* Use the current entry */
				}else
				{
					/* Creates the full name */
					sprintf(fullname, "%s/%s", iterator->cwd, result->d_name);

					/* Allocates a copy of the name */
					if((path = strdup(fullname)) == NULL)
					{
						logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
					}
				}
			}

		}while(path == NULL && (!dll_empty(iterator->paths) || iterator->currDir != NULL));

		if(path == NULL)
		{
			break;
		}

		tmpptr = strrchr(path, '/');

		/* Ignores all files and directories that start with . */
		if((tmpptr != NULL && tmpptr[1] == '.') || path[0] ==  '.')
		{
			free(path);

		/* Gets a statbuf for the current path */
		}else if(lstat(path, &statbuf) != 0)
		{
			logPrintError(0, __FILE__, __LINE__, 0, "Couldn't stat %s", path);

			/* Frees the path name */
			free(path);

		/* Detects if the current entry is an exnode and if so adds it to
	     * the collection */
		}else if(S_ISREG(statbuf.st_mode) && strlen(path) > 4 &&
			    strcmp(path+strlen(path)-4, ".xnd") == 0)
		{
			 /* Allocates the exnode and adds it to the list */
			 exnode = makeLoDNExnode(path);

			 /* Frees the path name */
			 free(path);

		/* If its a directory then it opens a directory struct for it */
		}else if(S_ISDIR(statbuf.st_mode))
		{
			/* Opens the directory if there isn't currently a directory */
			if(iterator->currDir == NULL)
			{
				/* Open directory for reading */
				if((iterator->currDir = opendir(path)) == NULL)
				{
					logPrintError(0, __FILE__, __LINE__, 0, "Couldn't open %s directory", path);
				}

				/* Assigns the current working directory */
				if(iterator->cwd != NULL)
				{
					free(iterator->cwd);
				}

				iterator->cwd = path;

			/* Appends the directory to the list of directories */
			}else
			{
				dll_append(iterator->paths, new_jval_s(path));
			}
		}else
		{
			free(path);
		}
	}

	/* Returns the exnode */
	return exnode;
}



/****------------------------freeLoDNExnodeIterator()----------------------****
 * Description: This function frees the memory and resources associated with
 *              the supplied LoDNExnodeIterator.
 * Input: iterator - pointer to the LoDNExnodeIterator.
 ****----------------------------------------------------------------------****/
void freeLoDNExnodeIterator(LoDNExnodeIterator *iterator)
{
	/* Declarations */


	/* Checks if the argument is valid */
	if(iterator != NULL)
	{
		/* Frees the paths */
		if(iterator->paths != NULL)
		{
			free_dllist(iterator->paths);
		}

		/* Closes the current working directory */
		if(iterator->currDir != NULL)
		{
			closedir(iterator->currDir);
		}

		/* Frees the name of the current working directory */
		if(iterator->cwd != NULL)
		{
			free(iterator->cwd);
		}

		/* Frees the iterator */
		free(iterator);
	}
}

