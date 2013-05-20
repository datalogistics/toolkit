/******************************************************************************
 * Name: LoDNExnode.h
 * Description: This file defines the constants, structs and function calls
 *              used by LoDNExnode
 *
 * Developer: Christopher Sellers
 ******************************************************************************/

#ifndef LODNEXNODE_H_
#define LODNEXNODE_H_

#include <pthread.h>
#include <dirent.h>
#include "ibp.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"
#include "libexnode/exnode.h"

#include "LoDNFreeMappings.h"


/***---Constants---***/

#define IBP_URI_LEN         6           /* Length of the IBP:// URI string */
#define BROKEN2DEADTIME     259200      /* 3 days  then exnode becomes dead */



/***-----Struct----***/

/* Represents an exnode */
typedef struct
{
    char *name;                         /* Name of the exnode */

    pthread_mutex_t lock;               /* Pthread lock for the exnode */
    int             numTasks;           /* Number of thread tasks */

    JRB     mappingsByOffset;           /* List of mappings by their offsets */
    JRB     segments;                   /* List of segments (blocks) by offset */

    char   *filename;                   /* Exnode filename attribute */
    double  lorsversion;                /* Version of LoRS */
    char   *status;                     /* Exnode status */
    int     brokenSince;                /* When the exnode became broken */

    int     remove;                     /* Remove or keep the exnode */

    int     numSegments;                /* Number of segments */
    int     numBadSegments;             /* Number of bad segments */
    int     numFewCopySegments;         /* Number of segments with < 3 copies */
    int     numCompleteCopies;          /* Number of complete exnode copies */
    int     numWarmerCopies;            /* Number of warmer specified complete
                                         * copies */

} LoDNExnode;


/* Represents a mapping */
typedef struct
{
    char      hostname[IBP_MAX_HOSTNAME_LEN];   /* Name of the depot */
    int       port;                             /* Port of the depot */

    int       status;                   /* Status of the mapping (good,bad) */
    int       brokenSince;              /* How long the mapping has been broken for */
    long long alloc_length;             /* Length of the allocation */
    long long alloc_offset;             /* Offset of the allocation */
    long long exnode_offset;            /* Offset of within the exnode */
    long long logical_length;           /* Length of the mapping in the exnode*/

    int       inWarmer;					/* Holds if the mapping is specified in
                                           the warmer */

    IBP_cap   readCap;                  /* IBP read cap */
    IBP_cap   writeCap;                 /* IBP write cap */
    IBP_cap   manageCap;                /* IBP manage cap */

    LoDNExnode *exnode;                 /* Pointer to the containing exnode */

} LoDNExnodeMapping;


/* Represents a segment (block) exnode */
typedef struct
{
    LoDNExnode *exnode;                 /* Ponter to the containing exnode */

    long long start;                    /* Starting offset */
    long long end;                      /* Ending offset */

    Dllist mappings;                    /* List of mappings for the segment */

} LoDNExnodeSegment;


/* Container for the exnodes of the warmer */
typedef struct
{
    Dllist exnodes;                     /* List of LoDN Exnodes */
    Dllist iteratorPtr;                 /* Iterator pointer through the list */
    int    numExnodes;                  /* Number of exnodes */

} LoDNExnodeCollection;



/* Struct for the iterator -- replacement for LoDNExnodeCollection*/
typedef struct
{
	Dllist paths;						/* List of paths */
	DIR   *currDir;						/* Current working directory handle */
	char  *cwd;							/* Current working directory name */
	struct dirent entry;				/* Directory entry */

} LoDNExnodeIterator;



/***---Prototypes---***/
LoDNExnode* makeLoDNExnode(char *name);
void freeLoDNExnode(LoDNExnode *exnode);
LoDNExnodeCollection *buildExnodeCollection(Dllist paths);
void freeLoDNExnodeCollection(LoDNExnodeCollection *collection);
LoDNExnode *LoDNExnodeCollectionGetNextExnode(LoDNExnodeCollection *collection);
int LoDNExnodeReadExnode(LoDNExnode *exnode);
int LoDNExnodeWriteExnode(LoDNExnode *exnode, char *outputFilename,
						  LoDNFreeMappings_t *freeMappings);
int LoDNExnodeRemoveBadMappings(LoDNExnode *exnode);
int LoDNExnodeGetSegments(LoDNExnode *exnode);
int LoDNExnodeInsertMapping(LoDNExnode *exnode, LoDNExnodeMapping *stdMapping);
LoDNExnodeMapping *LoDNExnodeCreateMapping(IBP_set_of_caps caps, long long alloc_length,
               long long alloc_offset, long long exnode_offset, long long logical_length);
int LoDNExnodeCalculateStatus(LoDNExnode *exnode);
LoDNExnodeIterator *buildExnodeIterator(Dllist paths);
LoDNExnode *LoDNExnodeIteratorNext(LoDNExnodeIterator *iterator);
void freeLoDNExnodeIterator(LoDNExnodeIterator *iterator);


#endif /*LODNEXNODE_H_*/
