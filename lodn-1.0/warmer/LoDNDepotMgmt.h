/******************************************************************************
 * Name: LoDNDepotMgmt.h
 * Description: This file defines the LoDNDepotMgmt constants, structs and
 *              calls.  These calls are responsible for maintaining the
 *              statistics about the depots and the links between them.
 *              As well as interacting directly with the depots themselves.
 *
 * Developer: Christopher Sellers
 ******************************************************************************/
#ifndef LODNDEPOTMGMT_H_
#define LODNDEPOTMGMT_H_

#include <time.h>
#include <pthread.h>

#include "jval.h"
#include "jrb.h"
#include "LoDNExnode.h"
#include "ibp.h"


/***---Constants----***/
#define LINK_HISTORY_SIZE           10



/***----Enums-------***/

/* Values for depot status */
typedef enum { Up = 0, Down = 1} DepotStatus;


/***----Structs-----***/

/* Defines the global struct that holds all of resources for the inter
 * function calls */
typedef struct
{
    JRB linkTree;       /* First level is the ID of source depot (hostname:port)
                         * and second level is ID of destination depot
                         * (hostname:port) with value being link data */

    JRB statTree;       /* Holds the depot stat structures for all of the depots
                         * Key is (hostname:port) with value being the
                         * depotStat structure */

    float alphaHistory[LINK_HISTORY_SIZE];  /* Holds the alpha's */

    pthread_mutex_t lock;  /* Main lock for the module */
    pthread_mutex_t depotInfo_lock;

} LoDNDepotMgmt;


/* Struct that represents a link value */
typedef struct
{
    char *srcDepotID;       /* Name of the src depot, hostname:port */
    char *destDepotID;      /* Name of the dest dpeot, hostname:port */
    DepotStatus srcStatus;  /* Status of source */
    DepotStatus destStatus; /* Status of source */

    float linkHistory[LINK_HISTORY_SIZE];     /* link history */
    unsigned int   historyIndex;


    pthread_mutex_t lock;   /* lock for protecting data */

} LinkStat;


/* Struct that represents a depots status */
typedef struct
{
    long long  availHardStorage;
    long long  availSoftStorage;
    long   duration;
    time_t lastQuery;
    float  version;

    DepotStatus  status;
    int          deadAttempts;

    pthread_mutex_t lock;

} DepotStat;


/***----Prototypes---***/
void LoDNDepotMgmtInit(int duration);
int LoDNDepotQuery(char *depotID, unsigned long int allocLength, int *storageType,
                    time_t *duration, DepotStatus *status);
int LoDNDepotSetStatus(char *depotID, DepotStatus status);
int LoDNDepotMgmtRefreshMapping(LoDNExnodeMapping *mapping);
Dllist LoDNDepotMgmtCopyData(Dllist srcMappings, Dllist destDepots, long long allocLength,
                             int numCopiesWanted, int numAttempts);
int LoDNDepotMgmtFreeReadCap(IBP_cap readCap);


#endif /*LODNDEPOTMGMT_H_*/
