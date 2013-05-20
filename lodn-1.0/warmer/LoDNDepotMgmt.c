/******************************************************************************
 * Name: LoDNDepotMgmt.c
 * Description: This file implements the LoDNDepotMgmt calls.  These calls
 *              are responsible for maintaining the statistics about the
 *              depots and the links between them.  As well as interacting
 *              directly with the depots themselves.
 *
 * Developer: Christopher Sellers
 ******************************************************************************/



#define _ISOC99_SOURCE

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#define  __USE_MISC     // Neccessary on some linux systems for all mmap features
#include <sys/mman.h>
#undef   __USE_MISC
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "ibp.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

#include "LoDNDepotMgmt.h"
#include "LoDNExnode.h"
#include "LoDNLogging.h"
#include "LoDNAuxillaryFunctions.h"

#include "phoebus.h"


/***----Globals-----***/

/* Used for pthread once to initialize the module */
static pthread_once_t _LoDNDepotMgmt_is_initialized = PTHREAD_ONCE_INIT;

/* Used for all shared resource between depot calls */
static LoDNDepotMgmt *_LoDNDepotMgmt                = NULL;

/* Allows for a maximum duration period of allocations for refreshing and
 * allocating.  -1 means use the maximum allowed by the depot  */
static time_t max_duration = -1;


/***----Functions---***/

/****------------------------_LoDNDepotMgmtInitializer()-------------------****
 * Description: This static function is used to initialize DepotMgmt API.  It
 *              allocates the structure for the global DepotMgmt structure,
 *              intializes the mutex locks and the alphas for the predictor.
 ****----------------------------------------------------------------------****/
static void _LoDNDepotMgmtInitializer(void)
{
    /* Delcarations */
    float pow2 = 1;
    float sum  = 0;
    int i;


    /* Allocates the main global struct */
    if((_LoDNDepotMgmt = (LoDNDepotMgmt*)malloc(sizeof(LoDNDepotMgmt))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Creates the empty link tree to be used */
    if((_LoDNDepotMgmt->linkTree = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Creates the empyt statu tree to be used */
    if((_LoDNDepotMgmt->statTree = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Initializes the pthread mutex lock */
    if(pthread_mutex_init(&(_LoDNDepotMgmt->lock), NULL) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing pthread mutex");
    }

    /* Initializes the pthread mutex lock for the depot info function */
    if(pthread_mutex_init(&(_LoDNDepotMgmt->depotInfo_lock), NULL) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing pthread mutex");
    }

    /* Builds the alpha values for the predictor */
    for(i=0; i<LINK_HISTORY_SIZE; i++)
    {
        pow2 *= 2;

        _LoDNDepotMgmt->alphaHistory[i] = 1.0 / pow2;

        sum += _LoDNDepotMgmt->alphaHistory[i];
    }

    _LoDNDepotMgmt->alphaHistory[i-1] += 1.0 - sum;
}



/****-------------------------_LoDNQueryDepot()----------------------------****
 * Description: This function overcomes the problem with shared global
 * 		        IBP_DptInfo in ibp that prevents multiple threads from
 *              simutaneously preforming queries. It forks off a sub process
 *              that does the actual query (therefore no shared global
 *              IBP_DptInfo) and uses a memory mapped segment between the
 *              parent and child process to share the results of the query
 *              with the parent. The child returns sucessfully if the
 *              query worked and unsuccessfully if not.  This allows the
 *              parent process (this process) to know if the query worked.
 *              The parent then fills in the info struct.
 * Input: depot - pointer to the IBP_depot struct that holds the depot to
 *                query.
 *        depotStat - pointer to the DepotStat struct for the depot.
 * Output: It returns 0 if successfuly query and -1 on failure.
 ****----------------------------------------------------------------------****/
#if 0
static int _LoDNQueryDepot(IBP_depot depot, DepotStat *depotStat)
{
	/* Declarations */
	pid_t cpid;
	int   status;
	IBP_DptInfo depotInfo = NULL;
	IBP_DptInfo tmpDepotInfo = NULL;
	IBP_DptInfo retDepotInfo = NULL;
	struct ibp_timer timeout   = { 30, 30 };

	time_t now;

	/* Linux and Darwin use different flags for anonyomous */
	#ifndef MAP_ANONYMOUS
    #      define MAP_ANONYMOUS 	MAP_ANON
	#endif



	/* Creates shared memory for the depotInfo struct between processes. */
	if((depotInfo = mmap(NULL, sizeof(struct ibp_dptinfo), PROT_READ|PROT_WRITE,
			             MAP_SHARED|MAP_ANONYMOUS, -1, 0)) == NULL)
	{
		logPrintError(0, __FILE__, __LINE__, 0, "Error mmap memory");
		goto ERROR_HANDLER;
	}

	/* Forks a child Process */
	cpid = fork();

	/* Handles the 3 outcomes of fork. */
	switch(cpid)
	{
		/* Error Occurred so return bad */
		case -1:

			logPrintError(0, __FILE__, __LINE__, 0, "Error forking child for query");
			goto ERROR_HANDLER;

			break;

		/* Child process */
		case 0:

			/* Queries the depot for information */
			if((tmpDepotInfo = IBP_status(depot, IBP_ST_INQ, &timeout, NULL, 0, 0, 0))== NULL)
			{
				logPrintError(2, __FILE__, __LINE__, 1, "IBP_status error: %d", IBP_errno);
			}

			/* Copies the data to the shared memory for the depotInfo */
			memcpy(depotInfo, tmpDepotInfo, sizeof(struct ibp_dptinfo));

			exit(EXIT_SUCCESS);

			break;

		/* Parent process */
		default:

			/* Waits for the child process to return */
			if(waitpid(cpid, &status, 0) < 0)
			{
				logPrintError(2, __FILE__, __LINE__, 0, "Error waiting on child process for query");
				goto ERROR_HANDLER;
			}

			/* Checks that the child process returned successfully */
			if(!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS)
			{
				logPrintError(2, __FILE__, __LINE__, 0, "Child process query didn't finish successfully");
				goto ERROR_HANDLER;
			}

			/* Fills in the stable and volatile storage information */
			if(depotInfo->majorVersion < 1.4)
			{
				depotStat->availHardStorage = (depotInfo->StableStor - depotInfo->StableStorUsed);
				depotStat->availSoftStorage = (depotInfo->VolStor - depotInfo->VolStorUsed);
			}
			else
			{
				depotStat->availHardStorage = (depotInfo->HardAllocable);
				depotStat->availSoftStorage = (depotInfo->SoftAllocable);
			}


			depotStat->version  = depotInfo->majorVersion+depotInfo->minorVersion;
			depotStat->duration = (time_t)depotInfo->Duration;
			depotStat->status = Up;

			depotStat->deadAttempts = 0;

			/* Gets the current time */
			if((now = time(NULL)) == (time_t)-1)
			{
				logPrintError(0, __FILE__, __LINE__, 1, "Error accessing current time");
			}

			/* Sets the last query data */
			depotStat->lastQuery = now;


			/* Unmaps the shared memory for depot info */
			if(munmap(depotInfo, sizeof(struct ibp_dptinfo)) != 0)
			{
				 logPrintError(0, __FILE__, __LINE__, 0, "Error unmaping memory");
				 goto ERROR_HANDLER;
			}

			break;
	}

	/* Return successfully */
	return 0;


	/* Error Handler */
	ERROR_HANDLER:

		/* Unmaps the shared memory for depot info */
		if(depotInfo != NULL)
		{
			if(munmap(depotInfo, sizeof(struct ibp_dptinfo)) != 0)
			{
				 logPrintError(0, __FILE__, __LINE__, 1, "Error unmaping memory");
			}
		}

		/* Return unsuccessfully */
		return -1;
}
#endif


/****--------------------_LoDNDepotMgmtGetLink()---------------------------****
 * Description: This function returns the LinkStat structure that represents
 *              the connection between two input depots.  If there is no
 *              prior LinkStat structure then it creates one and inserts it
 *              into the LinkTree by srcDepotID and destDepotID.
 * Input: srcDepotID - cstring that represents the source depot by
 *                     hostname:port.
 *        destDepotID - cstring that represents the destination depot by
 *                      hostname:port.
 * Output: It returns a pointer to the corresponding LinkStat on success and
 *         NULL on failure.
 ****----------------------------------------------------------------------****/
static LinkStat *_LoDNDepotMgmtGetLink(char *srcDepotID, char *destDepotID)
{
    /* Declarations */
    JRB srcNode;
    JRB destTree;
    JRB destNode;
    LinkStat *linkStat;
    int       i;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }

    /* Checks the depots */
    if(srcDepotID == NULL || destDepotID == NULL)
    {
        return NULL;
    }

    /* Lock the mutex */
    if(pthread_mutex_lock(&(_LoDNDepotMgmt->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
    }

    /* Attempts to get the source depot */
    srcNode = jrb_find_str(_LoDNDepotMgmt->linkTree, srcDepotID);

    /* No previous instance of the depot, so we'll make one */
    if(srcNode == NULL)
    {
        /* the sub link tree */
        if((destTree = make_jrb()) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

         /* Copies the ids */
        if((srcDepotID = strndup(srcDepotID, IBP_MAX_HOSTNAME_LEN+16)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Inserts the depot into the tree, so the next time it will be there */
        if((srcNode = jrb_insert_str(_LoDNDepotMgmt->linkTree, srcDepotID,
                                        new_jval_v(destTree))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }


    /* Gets the src Depot */
    destTree = (JRB)srcNode->val.v;

    /* Attempts to get the destination */
    destNode = jrb_find_str(destTree, destDepotID);

    /* If the destination isn't there then it makes it */
    if(destNode == NULL)
    {
        /* Copies the destination id */
        if((destDepotID = strndup(destDepotID,IBP_MAX_HOSTNAME_LEN+16)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Allocate the depot */
        if((linkStat = (LinkStat*)malloc(sizeof(LinkStat))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Fills the two depots */
        linkStat->srcDepotID  = srcDepotID;
        linkStat->destDepotID = destDepotID;

        /* Initializes the link */
        linkStat->srcStatus  = Up;
        linkStat->destStatus = Up;

        /* Initializes the history */
        for(i=0; i<LINK_HISTORY_SIZE; i++)
        {
            linkStat->linkHistory[i] = HUGE_VALF;
        }

        /* Sets the initial history index */
        linkStat->historyIndex = 0;

        /* Initializes the link for the lock */
        if(pthread_mutex_init(&(linkStat->lock), NULL) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error initializing mutex lock");
        }

        /* Inserts the link into the sub tree */
        if((destNode = jrb_insert_str(destTree, destDepotID,
                                            new_jval_v(linkStat))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }
    }

    /* Gets the link status */
    linkStat = (LinkStat*)destNode->val.v;

    /* Unlock the mutex */
    if(pthread_mutex_unlock(&(_LoDNDepotMgmt->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
    }

    /* Returns the link stat */
    return linkStat;
}



/***--------------------------------_ptrCmp()------------------------------****
 * Description: This function is used in the JRB tree for sorting void
 *              pointers (whicy maybe either 4 or 8 bytes).
 * Input: val1, val2 - Jval that holds the void pointer values.
 * Output: It returns a negative value if val1 < val2, 0 for val1 == val2, or
 *         1 for val1 > val2;
 ****----------------------------------------------------------------------****/
static int _ptrCmp(Jval v1, Jval v2)
{
    return v1.v - v2.v;
}


/****-----------------------_getRandomLinkValue()--------------------------****
 * Description: This function returns random value between 0 and 1 for the
 *              linkStat value
 * Input: linkStat - pointer to the linkStat structure.
 * Output: It returns the predicted value on success and -1 on failure.
 ****----------------------------------------------------------------------****/
static float _getRandomLinkValue(LinkStat *linkStat)
{
	return (float)((random()%100)/100.0);
}



/****--------------------------_getLinkValue()-----------------------------****
 * Description: This function returns the predictor value for the linkStat
 *              between two depots using a weighted alpha predictor.  It
 *              return the value calculated.
 * Input: linkStat - pointer to the linkStat structure.
 * Output: It returns the predicted value on success and -1 on failure.
 ****----------------------------------------------------------------------****/
static float _getLinkValue(LinkStat *linkStat)
{
    /* Delcarations */
    float value;          /* Holds the value of the link */
    int i;
    int index;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }

    /* Checks the link value */
    if(linkStat == NULL)
    {
        return -1;
    }

    /* Locks the link mutex */
    if(pthread_mutex_lock(&(linkStat->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
    }

    /* Calculates the link value */
    value = 2 - (linkStat->srcStatus + linkStat->destStatus);

    for(i=0; i < LINK_HISTORY_SIZE; i++)
    {
        index = linkStat->historyIndex-i;

        if(index < 0)
        {
            index = LINK_HISTORY_SIZE + index;
        }

        value += _LoDNDepotMgmt->alphaHistory[i%LINK_HISTORY_SIZE] *
                 linkStat->linkHistory[i%LINK_HISTORY_SIZE];
    }

    /* Unlocks the link mutex */
    if(pthread_mutex_unlock(&(linkStat->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
    }

    /* Returns the value */
    return value;
}



/****------------------------_findBestLink()-------------------------------****
 * Description: This static function takes a JRB list of links (LinkStats)
 *              and returns the LinkStat that has the best predicted value.
 * Input: links - JRB list of linkStats where the key is a pointer to the
 *                linkStat.
 * Output: It returns the best (highest) predicted linkStat.
 ****----------------------------------------------------------------------****/
static LinkStat * _findBestLink(JRB links)
{
    /* Declarations */
    JRB linkNode;
    LinkStat *currLink = NULL;
    LinkStat *bestLink = NULL;
    float bestValue    = -1.0;
    float value        = -1.0;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }


    /* Traverses the list of best links and attempts to find the best one */
    jrb_traverse(linkNode, links)
    {
        /* Gets the curr link under consideration */
        currLink = (LinkStat*)linkNode->key.v;

        /* Gets the value for the link */
#ifdef _RANDOM_LINK_VALUE
        value = _getRandomLinkValue(currLink);
#else
        value = _getLinkValue(currLink);
#endif


        /* If this value is better than the any of the others so far then
         * make it the best link */
        if(value > bestValue)
        {
            bestValue = value;
            bestLink = currLink;
        }
    }

    /* Returns the best link */
    return bestLink;
}


/****--------------------------depotIDtohnpt()-----------------------------****
 * Description: This static function converts a depot represented by a
 *              cstring in the form of hostname:port to a IBP_depot.
 * Input: depotID - cstring that holds the depot in the form of hostname:port
 * Output: depot - IBP_depot that gets filled in with information from depotID.
 *         It returns 0 on success and -1 on failure.
 ****----------------------------------------------------------------------****/
static int _depotIDtohnpt(char *depotID, IBP_depot depot)
{
    /* Declarations */
    char *pos;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }

    memset(depot, 0, sizeof(struct ibp_depot));

    /* Copies the depot id into the hostname for the depot */
    strncpy(depot->host, depotID, IBP_MAX_HOSTNAME_LEN);

    /* Finds the split between the hostname and port */
    if((pos = strchr(depot->host, ':')) == NULL)
    {
        return -1;
    }

    /* Ends the hostname */
    *pos = '\0';

    /* Gets the port */
    if(sscanf(pos+1, "%d", &(depot->port)) != 1)
    {
        return -1;
    }

    /* Return Successfully */
    return 0;
}



#if defined(IBPv031) && IBPv031 != 0

/****-----------------------_clearIBPCache()------------------------------****
 * Description: This function is used to handle a problem in lors-0.82
 *              version's of IBP that has a caching of port numbers problem.
 *              It keeps the cache clear so that this will not happen.  The
 *              problem doesn't occur in the recent releases of IBP.
 ****----------------------------------------------------------------------****/
void _clearIBPCache(void)
{
    /* Declarations */


    if(glbCache == NULL)
    {
        return;
    }



    /* Clears the cache if it needs clearing */
    if(glbCache != NULL)
    {
        #ifdef HAVE_PTHREAD_H
            free(glbCache->lock);
        #endif

        free(glbCache);
        glbCache = NULL;
    }
}

#endif




#ifdef PHOEBUS

static char **_getPhoebusGatewayRoute(char *src, char *dst)
{
	/* Declarations */
	int fd;
	int *num_entries = 0;
	phoebus_route_t *routes = NULL;
	static void *addr = NULL;
	size_t   length;
	phoebus_route_t *selected_route;
	char **path = NULL;
	int i;
	struct addrinfo *src_addrinfo = NULL;
	struct addrinfo *dst_addrinfo = NULL;


	if(getaddrinfo(src, NULL, NULL, &src_addrinfo) != 0 ||
	   getaddrinfo(dst, NULL, NULL, &dst_addrinfo) != 0)
	{
		logPrintError(0, __FILE__,__LINE__, 0,
					  "Unable to find IPv4 addresses for a set of depots %s\n",
					  ROUTEFILE);
		return NULL;
	}


    /* Open the file */
    if((fd = open(ROUTEFILE, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) < 0)
    {
    	logPrintError(0, __FILE__,__LINE__, 0,
						"Unable to open route file %s\n", ROUTEFILE);
        return NULL;
    }

    /* Lock the file */
    if(acquire_route_file_lock(fd) == -1)
    {
    	logPrintError(0, __FILE__,__LINE__, 0,
						"Unable to lock route file %s\n", ROUTEFILE);
    	return NULL;
    }

    /* Map the file into memory */
	if((length = map_route_file(&addr, fd, 0)) == (size_t)-1)
	{
		logPrintError(0, __FILE__,__LINE__, 0,
						  "Unable to memory map route file %s\n", ROUTEFILE);
		goto ERROR_HANDLER;
	}

	/* Gets pointers to the data structure */
	num_entries = (int*)addr;
	routes      = (phoebus_route_t*)(addr+sizeof(int));


	/* Attempts to find a route for the source and destination depots */
	selected_route = find_route(((struct sockaddr_in*)(src_addrinfo->ai_addr))->sin_addr.s_addr,
								((struct sockaddr_in*)(dst_addrinfo->ai_addr))->sin_addr.s_addr,
							    routes, *num_entries);



	/* Route was found */
	if(selected_route != NULL)
	{
		/* Allocates an array of cstring pointers for the gateways */
		if((path = malloc((MAX_PATH_ENTRIES+1)*sizeof(char*))) == NULL)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocaton Error");
		}

	    /* Iterates over the routes looking for a matching route */
	    for(i=0; i<MAX_PATH_ENTRIES && selected_route->gateway[i].host != 0; i++)
	    {
	    	/* Allocates a string to hold the gateway */
	    	if((path[i] = malloc((256+6)*sizeof(char))) == NULL)
	    	{
	    		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocaton Error");
	    	}

	    	/* Fills in the cstring with the address and port of gateway */
	    	snprintf(path[i], (256+5)*sizeof(char), "%s/%u",
	    			inet_ntoa(selected_route->gateway[i].host),
	    			selected_route->gateway[i].port);
	    }

	    /* NULL terminate the list */
	    path[i] = NULL;

	/* No route found */
	}else
	{
		path = NULL;
	}

	/* Close the file */
	if(release_route_file_lock(fd) == -1)
	{
		logPrintError(0, __FILE__,__LINE__, 0,
								"Unable to unlock route file %s\n", ROUTEFILE);
		goto ERROR_HANDLER;
	}

	/* Close the file */
	close(fd);

	/* Frees the addr structs */
	freeaddrinfo(src_addrinfo);
	freeaddrinfo(dst_addrinfo);

	/* Returns the path */
	return path;


	/* Error handler */
	ERROR_HANDLER:

		release_route_file_lock(fd);
		close(fd);

		if(src_addrinfo != NULL)
		{
			freeaddrinfo(src_addrinfo);
		}

		if(dst_addrinfo != NULL)
		{
			freeaddrinfo(dst_addrinfo);
		}

		/* Free path */
		if(path != NULL)
		{
			for(i=0; i<MAX_PATH_ENTRIES && path[i] != NULL; i++)
			{
				free(path[i]);
			}

			free(path);
		}

		return NULL;
}

#endif


/****--------------------------LoDNDepotMgmtInit()-------------------------****
 * Description: This function is used to initialize the LoDNDepotMgmt
 *              module.  It calls the internal _LoDNDepotMgmtInitialier
 *              and sets the max duration.
 * Input: durration: int that holds the max duration time for refreshing.
 *                   If -1 then it just uses the max duration time set by
 *                   the depot otherwise its the mininum of the max allowed
 *                   by the depot and the duration.
 ****----------------------------------------------------------------------****/
void LoDNDepotMgmtInit(int duration)
{
	/* Initializes the module */
	if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
	}

	/* Sets the max duration */
    max_duration = 	(time_t)duration;
}



/****--------------------------LoDNDepotQuery()----------------------------****
 * Description: This function queries a depot about the type of storage it
 *              has and what the length of its duration is.  The allocLength
 *              indicates how much storage is needed so that the depot will
 *              be to the type of storage that provides enough for that need.
 *              If the depot is already in the list of depots then it uses that
 *              otherwise it queries the depot directly and creates an
 *              entry for it.
 * Input: depotID - cstring that holds the depot to be queried in the form of
 *                  hostname:port.
 *        allocLength - unsigned long int that indicates how much storage is
 *                      needed in bytes.
 * Output: storageType - int pointer to the type of storage (IBP_STABLE,
 *                       IBP_HARD).
 *         duration - time_t that gets the maximum length in seconds for a
 *                    mappings duration on a depot.
 *         status   - pointer to the depot status that gets set.
 *         It returns 0 on success or -1 on failure.
 ****----------------------------------------------------------------------****/
int LoDNDepotQuery(char *depotID, unsigned long int allocLength, int *storageType,
                    time_t *duration, DepotStatus *status)
{
    /* Declarations */
    JRB              statNode;
    char            *newID     = NULL;
    struct ibp_depot depot;
    IBP_DptInfo      depotInfo = NULL;
    DepotStat       *depotStat = NULL;
    unsigned char    newEntry  = 0;
    struct ibp_timer timeout   = { 30, 30 };
    time_t           now;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }

    /* Locks the mutex for protecting the tree */
    if(pthread_mutex_lock(&(_LoDNDepotMgmt->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking mutex");
    }

    /* Finds the stat struct for this depot and if it doesn't exits creates
     * and inserts into into the tree */
    if((statNode = jrb_find_str(_LoDNDepotMgmt->statTree, depotID)) == NULL)
    {
        /* New ID copyied from depotID */
        if((newID = strndup(depotID, IBP_MAX_HOSTNAME_LEN+16)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Allocates a depot stat */
        if((depotStat = (DepotStat*)calloc(1, sizeof(DepotStat))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Initializes the pthread mutex lock */
        if(pthread_mutex_init(&(depotStat->lock), NULL) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error initializing mutex");
        }

        /* Inserts the depot stat into the tree by its depot ID */
        if((statNode = jrb_insert_str(_LoDNDepotMgmt->statTree, newID, new_jval_v(depotStat))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        newEntry = 1;
    }

    depotStat = (DepotStat*)statNode->val.v;

    /* Locks the depot stat lock */
    if(pthread_mutex_lock(&(depotStat->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking mutex");
    }

    /* Unlocks the mutex for protecting the tree */
    if(pthread_mutex_unlock(&(_LoDNDepotMgmt->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
    }

    /* Gets the current time */
    if((now = time(NULL)) == (time_t)-1)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error accessing current time");
    }

    /* Queries the depot for a new depot or if the depot hasn't been checked in
     * a day or if the depot is down and its been 5 minutes since the last attempt*/
    if(newEntry || (depotStat->lastQuery+(time_t)86500 < now) ||
       (depotStat->status == Down &&
       (depotStat->lastQuery+(time_t)(depotStat->deadAttempts*300)) < now))
    {
        logPrint(3, "Querying depot %s for information.", depotID);

        /* Sets the last query data */
        depotStat->lastQuery = now;

        /* Converts the depot ID to a depot  */
        if(_depotIDtohnpt(depotID, &depot) != 0)
        {
            goto ERROR_HANDLER;
        }


        /* Clears the IBP 1.3 cache due to a dns caching bug */
        #if defined(IBPv031) && IBPv031 != 0
        	 printf("Clearing the IBP cache\n");
        	 exit(EXIT_FAILURE);
             _clearIBPCache();
        #endif


        /* Attempts to get the status of the depot */
        if((depotInfo = IBP_status(&depot, IBP_ST_INQ, &timeout, NULL, 0, 0, 0)) == NULL)
        {
            logPrint(3, "Depot %s did not respond with query information.", depotID);
            logPrint(3, "Marking depot %s as down for a while.", depotID);

            /* Mark depot has down */
            depotStat->status = Down;

            if(depotStat->deadAttempts < 6)
            {
                depotStat->deadAttempts++;
            }

            goto ERROR_HANDLER;
        }

        /* Fills in the stable and volatile storage information */
        if(depotInfo->majorVersion < 1.4)
        {
            depotStat->availHardStorage = (depotInfo->StableStor - depotInfo->StableStorUsed);
            depotStat->availSoftStorage = (depotInfo->VolStor - depotInfo->VolStorUsed);
        }else
        {
            depotStat->availHardStorage = (depotInfo->HardAllocable);
            depotStat->availSoftStorage = (depotInfo->SoftAllocable);
        }

        depotStat->duration         = (time_t)depotInfo->Duration;
        depotStat->status           = Up;

        depotStat->deadAttempts = 0;


        /* Print statement about depot */
        logPrint(3, "Depot %s is version %f with %ld hard storage %ld soft storage and a maximum duration of %d.",
        		 depotID, depotStat->version,
        		 (long int)depotStat->availHardStorage, (long int)depotStat->availSoftStorage,
        		 depotStat->duration);


        /* Gets the current time */
        if((now = time(NULL)) == (time_t)-1)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error accessing current time");
        }

        /* Sets the last query data */
        depotStat->lastQuery = now;
    }

    /* Sets the duration and status */
    *duration = depotStat->duration;
    *status   = depotStat->status;

    /* Determines the type of storage based on the amount needed */
    if(depotStat->availHardStorage*1e6 > 10*allocLength)
    {
        *storageType = IBP_HARD;  /* Same as IBP_STABLE */
    }else if(depotStat->availSoftStorage*1e6 > 10*allocLength)
    {
        *storageType = IBP_SOFT;  /* Same as IBP_VOLATILE */
    }else
    {
        goto ERROR_HANDLER;
    }

    /* Unlocks the depot stat lock */
    if(pthread_mutex_unlock(&(depotStat->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
    }

    /* Return Successfully */
    return 0;

    /* Handles any errors that occur */
    ERROR_HANDLER:

        /* Unlocks the depot stat lock */
        if(pthread_mutex_unlock(&(depotStat->lock)) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
        }

        /* Mark it as down */
        *status = Down;

        return -1;
}



/****----------------------LoDNDepotSetStatus()----------------------------****
 * Description: This function takes a depot ID (hostname:port) and sets the
 *              status of the depot to the status that gets passed in.  If
 *              the depotStat does not already exist then it will create it.
 * Input: depotID - cstring that holds the hostname:port of the depot.
 *        status  - DepotStatus that has the new status variable.
 * Output: It returns 0 on success and -1 on failure.
 ****----------------------------------------------------------------------****/
int LoDNDepotSetStatus(char *depotID, DepotStatus status)
{
    /* Declarations */
    JRB              statNode;
    char            *newID     = NULL;
    DepotStat       *depotStat = NULL;
    unsigned char    newEntry  = 0;
    time_t           now;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }

    /* Locks the mutex for protecting the tree */
    if(pthread_mutex_lock(&(_LoDNDepotMgmt->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking mutex");
    }

    /* Finds the stat struct for this depot and if it doesn't exits creates
     * and inserts into into the tree */
    if((statNode = jrb_find_str(_LoDNDepotMgmt->statTree, depotID)) == NULL)
    {
    	logPrint(3, "New Depot %s stat.", depotID);

        /* New ID copyied from depotID */
        if((newID = strndup(depotID, IBP_MAX_HOSTNAME_LEN+16)) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Allocates a depot stat */
        if((depotStat = (DepotStat*)calloc(1, sizeof(DepotStat))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        /* Initializes the pthread mutex lock */
        if(pthread_mutex_init(&(depotStat->lock), NULL) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error initializing mutex");
        }

        /* Inserts the depot stat into the tree by its depot ID */
        if((statNode = jrb_insert_str(_LoDNDepotMgmt->statTree, newID, new_jval_v(depotStat))) == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
        }

        newEntry = 1;
    }

    depotStat = (DepotStat*)statNode->val.v;

    /* Locks the depot stat lock */
    if(pthread_mutex_lock(&(depotStat->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking mutex");
    }

    /* Unlocks the mutex for protecting the tree */
    if(pthread_mutex_unlock(&(_LoDNDepotMgmt->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
    }

    /* Gets the current time */
    if((now = time(NULL)) == (time_t)-1)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error accessing current time");
    }

    /* Fills in the stable and volatile storage information */
    depotStat->status = status;

    if(status == Up)
    {
    	logPrint(3, "Setting depot %s as up.", depotID);
        depotStat->deadAttempts = 0;
    }else
    {
    	logPrint(3, "Setting depot %s as down.", depotID);
        if(depotStat->deadAttempts < 6)
        {
            depotStat->deadAttempts++;
        }
    }

    /* Sets the last query data */
    depotStat->lastQuery = now;


    /* Unlocks the depot stat lock */
    if(pthread_mutex_unlock(&(depotStat->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
    }

    /* Return Successfully */
    return 0;


    /* Silence compiler */
    goto ERROR_HANDLER;

    /* Handles any errors that occur */
    ERROR_HANDLER:

        /* Unlocks the depot stat lock */
        if(pthread_mutex_unlock(&(depotStat->lock)) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
        }

        return -1;
}



/****---------------------LoDNDepotMgmtRefreshMapping()--------------------****
 * Description: This function takes a pointer to a LoDNExnodeMapping and
 *              attempts to update its duration on the depot for the mapping
 *              if required.
 * Input: mapping - pointer to the LoDNExnodeMapping structure.
 * Output: It returns 0 on success and -1 on failure.
 ****----------------------------------------------------------------------****/
int LoDNDepotMgmtRefreshMapping(LoDNExnodeMapping *mapping)
{
    /* Declarations */
    int                  bad_cap;
    time_t               refresh_time;
    struct ibp_capstatus capstatus;
    struct ibp_timer     timeout  = { 30, 30 };
    time_t               duration;
    int                  storageType;
    char                 depotID[IBP_MAX_HOSTNAME_LEN+12];
    char                *start;
    char                *end;
    int                  depotIDLen;
    DepotStatus          status;


    /* Checks the mapping pointer */
    if(mapping == NULL)
    {
        return -1;
    }

    /* Gets the name of depot from the mapping */
    if(strncmp(mapping->manageCap, "ibp://", strlen("ibp://")) != 0)
    {
        return -1;
    }

    start = mapping->manageCap+strlen("ibp://");

    if((end = strchr(mapping->manageCap+strlen("ibp://"), '/')) == NULL)
    {
        return -1;
    }

    /* Computes the len of the name */
    depotIDLen = end - start;

    /* Checks that the length is valid */
    if(depotIDLen == 0 || depotIDLen >= sizeof(depotID))
    {
        return -1;
    }

    strncpy(depotID, start, depotIDLen);

    *(depotID+depotIDLen) = '\0';

    /* Queries the depot about its storage type and duration */
    if(LoDNDepotQuery(depotID, 0, &storageType, &duration, &status) != 0)
    {
        logPrint(3, "Depot %s did not respond to query so can't refresh mapping for %s exnode.",
        		depotID, mapping->exnode->name);
        return -1;
    }

    /* Check the status of the depot that the mapping resides on */
    if(status == Down)
    {
        logPrint(3, "Depot %s has been marked as being down so skipping attempt at refresh for %s exnode.",
        		depotID, mapping->exnode->name);
        return -1;
    }

    /* Clears the IBP 1.3 cache due to a dns caching bug */
    #if defined(IBPv031) && IBPv031 != 0
        _clearIBPCache();
    #endif

    /* Determining if cap is still good or gone bad */
    logPrint(3, "Attempting to obtain data about a mapping for %s exnode.", mapping->exnode->name);
    bad_cap = (IBP_manage(mapping->manageCap, &timeout, IBP_PROBE, 0, &capstatus)== 0) ? 0 : 1;


    /*  Good cap so attempt to refresh it */
    if(!bad_cap)
    {
    	/* Calcuates the mininum duration period if a maximum duration
    	 * period is specified */
    	if(max_duration > -1)
    	{
    		duration = (duration < max_duration) ? duration : max_duration;
    	}

        /* Gets the refresh time for max that is allowed by the depot */
        refresh_time = time(NULL) + duration;

        /* If the current duration is less than the refresh time then refresh it
         * or a max duration is set */
        if(capstatus.attrib.duration < refresh_time || max_duration > -1)
        {
            logPrint(3, "Mapping is good but time is under the refresh time so I'm refreshing it.");

            /* Set the new time */
            capstatus.attrib.duration = refresh_time;

            /* Clears the IBP 1.3 cache due to a dns caching bug */
            #if defined(IBPv031) && IBPv031 != 0
                _clearIBPCache();
            #endif

            /* Update the duration */
            if(IBP_manage(mapping->manageCap, &timeout, IBP_CHNG, 0, &capstatus) != 0)
            {
                logPrintError(3, __FILE__, __LINE__, 0, "Unable to refresh mapping on depot %s for %s exnode at %ld offset.",
                		depotID, mapping->exnode->name, mapping->exnode_offset);

                /* Sets the depot status to down if a connection couldn't be made */
                if(IBP_errno == IBP_E_CONNECTION)
                {
                    LoDNDepotSetStatus(depotID, Down);
                }

                return -1;
            }else
            {
            	logPrint(3, "Refreshed mapping on depot %s for %s exnode at %lld offset.",
            	                depotID, mapping->exnode->name, mapping->exnode_offset);
            }
        }

        /* Return Good Status */
        return 1;

    /* Bad Cap so return -1 */
    }else
    {
       logPrintError(0, __FILE__, __LINE__, 0,
    		         "Mapping on depot %s for %s exnode at %lld offset may no longer be good.",
    	                 depotID, mapping->exnode->name, mapping->exnode_offset);

       /* Sets the depot status to down if a connection couldn't be made */
       if(IBP_errno == IBP_E_CONNECTION)
       {
            LoDNDepotSetStatus(depotID, Down);
       }

       return -1;
    }
}



/****-----------------------LoDNDepotMgmtCopyData()------------------------****
 * Description: This function uses IBP_copy to copy data from one set of
 *              mappings to another depot.  It attempts to predict which
 *              links are best to use try those first. It also updates the
 *              stat data for each depot and the link history for later
 *              prediction.
 * Input: srcMappings - Dllist of mappings to use as sources for the data.
 *        destDepots  - Dllist of depots to use as potential destinations.
 *        allocLength - long long that holds the size of the required
 *                      allocation.
 *        numCopiesWanted - int that holds the number of new copies wanted.
 *        numAttempts     - int that holds the number of times to try.
 * Output: It returns a Dllist of the new LoDNExnodeMappings or NULL on
 *         failure.
 *
 * Warning: The list of mappings and the mappings themselves must be freed
 *          at some point since they are all heap allocated.
 ****----------------------------------------------------------------------****/
Dllist LoDNDepotMgmtCopyData(Dllist srcMappings, Dllist destDepots,
                            long long allocLength, int numCopiesWanted,
                            int numAttempts)
{
    /* Declarations */
    Dllist srcMappingNode;
    Dllist destDepotNode;
    char  srcDepotID[IBP_MAX_HOSTNAME_LEN+12];
    char  *destDepotID;
    LinkStat *linkStat = NULL;
    JRB       links  = NULL;
    JRB       linkNode;
    Dllist    newMappings = NULL;
    LoDNExnodeMapping *mapping = NULL;
    LoDNExnodeMapping *newMapping = NULL;
    int       numCopies = 0;
    struct ibp_depot   depot;
    IBP_set_of_caps     caps = NULL;
    struct ibp_timer    timeout;
    struct ibp_attributes attributes;
    unsigned long int amtCopied  = 0;
    struct timeval start, end;
    float  bandwidth = 0.0;
    time_t duration;
    int storageType;
    DepotStatus status;
    char *copy_method;
    char **route;
    int    i;


    /* Initializes the module */
    if(pthread_once(&_LoDNDepotMgmt_is_initialized, _LoDNDepotMgmtInitializer) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error initializing LoDN Depot Mgmt Module");
    }

    /* Makes a tree that holds the links by the their rank values, so the
     * fastest ones can be picked out later */
    if((links = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocaton Error");
    }

    /* Traveres the src and dest depots getting all combinations of links */
    dll_traverse(srcMappingNode, srcMappings)
    {
        /* Source Depot */
        mapping = (LoDNExnodeMapping*)srcMappingNode->val.v;

        /* Constructs the depot ID */
        sprintf(srcDepotID, "%s:%d", mapping->hostname, mapping->port);

        dll_traverse(destDepotNode, destDepots)
        {
            /* Destination Depot */
            destDepotID = destDepotNode->val.s;

            /* Gets the link going from the src depot to the dest depot */
            if((linkStat = _LoDNDepotMgmtGetLink(srcDepotID, destDepotID)) == NULL)
            {
                continue;
            }

            /* Inserts the link into the tree by its value */
            if(jrb_insert_gen(links, new_jval_v(linkStat), new_jval_v(mapping), _ptrCmp) == NULL)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
            }
        }
    }

    /* Allocates a dllist to hold the new mappings */
    if((newMappings = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }


    /* Loops until a copy is made or we are out of links */
    while(numCopies < numCopiesWanted && !jrb_empty(links) && numAttempts != 0)
    {
        /* Find best remaining link */
        linkStat = _findBestLink(links);

        /* Tests that the link stat isn't NULL */
        if(linkStat == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "No links found returning from copy");
            goto ERROR_HANDLER;
        }


        /* Gets the mapping for the linkStat */
        linkNode = jrb_find_gen(links, new_jval_v(linkStat), _ptrCmp);
        mapping  = (LoDNExnodeMapping*)linkNode->val.v;

        /* Removes the link from the list for future consideration */
        jrb_delete_node(linkNode);

        /* Set the depot hostname and port */
        if(_depotIDtohnpt(linkStat->destDepotID, &depot) != 0)
        {
            continue;
        }

        /* Queries the depot about its storage type and duration */
        if(LoDNDepotQuery(linkStat->destDepotID, (unsigned long int)allocLength,
                            &storageType, &duration, &status) != 0 || status == Down)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "%s is a currently unreachable or down.\n",
            		      linkStat->destDepotID);
            continue;
        }

        logPrint(3, "Best predicted destination depot for augmentation is %s:%d",
                		 depot.host, depot.port);

      	/* Calcuates the mininum duration period if a maximum duration
         * period is specified */
        if(max_duration > -1)
        {
        	duration = (duration < max_duration) ? duration : max_duration;
        }


        /* Sets the timeout */
        timeout.ClientTimeout = 120;
        timeout.ServerSync    = 120;

        /* Sets the attributes */
        attributes.duration    = time(NULL) + duration;
        attributes.reliability = storageType;
        attributes.type        = IBP_BYTEARRAY;

        /* Clears the IBP 1.3 cache due to a dns caching bug */
        #if defined(IBPv031) && IBPv031 != 0
             _clearIBPCache();
        #endif

        logPrint(3, "Creating a new allocation on %s:%d of length %ld.",
        		 depot.host, depot.port, allocLength);

        /* Attempts to allocate a buffer on the destination depot */
        if((caps = IBP_allocate(&depot, &timeout, (unsigned long int)allocLength,
                                &attributes)) == NULL)
        {
            logPrintError(0, __FILE__,__LINE__, 0,
            		      "Unable to make allocation on %s:%d with IBP error %d",
                          depot.host, depot.port, IBP_errno);

            LoDNDepotSetStatus(linkStat->destDepotID, Down);

            continue;
        }


        /* Loops trying to complete until all of the data has been copied or
         * an error occurs in which case, its time to try a different depot */
        do
        {
        	logPrint(3, "Attempting to copy data from %s:%d to %s:%d of size %ld.",
        			 mapping->hostname, mapping->port, depot.host, depot.port,
        			 (long int)allocLength);

        	/* Clears the IBP 1.3 cache due to a dns caching bug */
			#if defined(IBPv031) && IBPv031 != 0
				_clearIBPCache();
			#endif


            if(gettimeofday(&start, NULL) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "Error obtaining time");
            }

            /* Only use regular IBP_copy */
            #ifndef PHOEBUS



            /* Attempts to copy the data to the destination depot */
            amtCopied = IBP_copy(mapping->readCap, caps->writeCap, &timeout,
                                  &timeout, (unsigned long int)allocLength, 0);

            copy_method = "regular ibp_copy";

            /* Try phoebus */
			#else

            /* Tests if depots support phoebus to see if phoebus should be
             * used */
            if(1)
            {

            	/* Get a route path for phoebus */
            	route = _getPhoebusGatewayRoute(mapping->hostname, depot.host);

				/* Attempts to copy the data to the destination depot using
				 * phoebus */
            	logPrint(0, "Attempting phoebus copy");
				amtCopied = IBP_phoebus_copy(mapping->readCap, caps->writeCap,
											 &timeout, &timeout,
											 (unsigned long int)allocLength, 0,
											 route);
				logPrint(0, "phoebus copied %d\n", (int)amtCopied);
				/* Frees the memory allocated to route */
				if(route != NULL)
				{
					for(i=0; route[i] != NULL; i++)
					{
						free(route[i]);
					}

					free(route);
				}

				copy_method = "phoebus ibp_copy";

				/* Fall back to regular copy */
				if(amtCopied < 0)
				{
					logPrintError(0, __FILE__, __LINE__, 0,
								  "Transfer from %s:%d to %s:%d of size %ld via %s failed, falling back to regular IBP_copy",
								  mapping->hostname, mapping->port, depot.host,
								  depot.port, (long int)allocLength,  copy_method);

					/* Attempts to copy the data to the destination depot */
					amtCopied = IBP_copy(mapping->readCap, caps->writeCap,
										 &timeout, &timeout,
										 (unsigned long int)allocLength, 0);

					copy_method = "regular ibp_copy";
				}

            }else
            {
				/* Attempts to copy the data to the destination depot */
				amtCopied = IBP_copy(mapping->readCap, caps->writeCap,
									 &timeout, &timeout,
									 (unsigned long int)allocLength, 0);

				copy_method = "regular ibp_copy";
            }


			#endif

            if(gettimeofday(&end, NULL) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "Error obtaining time");
            }

            /* Error occurred in IBP copy */
            if(amtCopied < 0)
            {
                logPrintError(0, __FILE__, __LINE__, 0,
                              "Transfer from %s:%d to %s:%d at time %lf to %lf of size %ld failed with ibp error %d",
                              mapping->hostname, mapping->port, depot.host,
                              depot.port,
                              ((double)(start.tv_sec*1e6 + start.tv_usec))/1e6,
                              ((double)(end.tv_sec*1e6   + end.tv_usec))/1e6,
                              (long int)allocLength, IBP_errno);

                free(caps->readCap);
                free(caps->writeCap);
                free(caps->manageCap);
                free(caps);
                caps = NULL;

                /* Error so set bandwidth to 0.0 */
                bandwidth = 0.0;

            /* Data did transfer but not all of it so try again increasing the
             * timeout period */
            }else if(amtCopied < allocLength)
            {
            	logPrintError(3, __FILE__, __LINE__, 0,
            			      "Transfer from %s:%d to %s:%d at time %lf to %lf of size %d via %s was partially successfully, going to attempt again with bigger timeout.",
            			      mapping->hostname, mapping->port, depot.host, depot.port,
            			      ((double)(start.tv_sec*1e6 + start.tv_usec))/1e6,
            			      ((double)(end.tv_sec*1e6   + end.tv_usec))/1e6,
            			      allocLength, copy_method);

            	/* Calculates the bandwidth */
            	bandwidth = ((double)amtCopied) /
            	             (((double)((end.tv_sec*1e6+end.tv_usec)-(start.tv_sec*1e6+start.tv_usec)))/1e6);

                timeout.ClientTimeout = allocLength / bandwidth * 2;
                timeout.ServerSync    = allocLength / bandwidth * 2;

            /* Copy did fine and was successful */
            }else
            {
                logPrint(3, "Successfully transferred %ld bytes via %s from %s:%d to %s:%d at time %lf to %lf in %f seconds.",
                		 (long int)allocLength, copy_method, mapping->hostname, mapping->port,
                		 depot.host, depot.port,
                		 ((double)(start.tv_sec*1e6 + start.tv_usec))/1e6,
                		 ((double)(end.tv_sec*1e6   + end.tv_usec))/1e6,
                		 (((float)((end.tv_sec*1e6+end.tv_usec)-(start.tv_sec*1e6+start.tv_usec)))/1e6));

                /* Creates a new Mapping */
                if((newMapping = LoDNExnodeCreateMapping(caps, allocLength,
                    0, mapping->exnode_offset, mapping->logical_length)) == NULL)
                {
                    logPrintError(0, __FILE__, __LINE__, 0, "Error creating new mapping");
                }

                free(caps->readCap);
                free(caps->writeCap);
                free(caps->manageCap);
                free(caps);
                caps = NULL;


                dll_append(newMappings, new_jval_v(newMapping));

                numCopies++;

                /* Calculates the bandwidth */
                bandwidth = ((double)allocLength/1024.0/1024.0) /
                        (((double)((end.tv_sec*1e6+end.tv_usec)-(start.tv_sec*1e6+start.tv_usec)))/1e6);
            }

        /* Test for completeness or error */
        }while(amtCopied < allocLength && amtCopied > 0);

        /* Protects the linkstat history */
        if(pthread_mutex_lock(&(linkStat->lock)) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
        }

        /* Stores the bandwidth into the history records */
        linkStat->linkHistory[linkStat->historyIndex%LINK_HISTORY_SIZE] = bandwidth;
        linkStat->historyIndex++;

        /* Unlocks the linkstat history */
        if(pthread_mutex_unlock(&(linkStat->lock)) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
        }
    }

    /* Frees the tree's resources */
    jrb_free_tree(links);

    /* Returns a list of the new mappings */
    return newMappings;


    /* Error Handler */
    ERROR_HANDLER:

        if(links == NULL)
        {
            jrb_free_tree(links);
        }

        if(caps != NULL)
        {
            if(caps->readCap != NULL)
            {
                free(caps->readCap);
            }

            if(caps->writeCap != NULL)
            {
                free(caps->writeCap);
            }

            if(caps->manageCap != NULL)
            {
                free(caps->manageCap);
            }

            free(caps);
        }

        /* Return Bad */
        return NULL;
}



/****-------------------LoDNDepotMgmtFreeReadCap()-------------------------****
 * Description: This function is used free IBP read capability.
 * Input: readCap - IBP_cap that has the capability to decrement.
 * Output: It returns the return value of IBP_Manage.
 ****----------------------------------------------------------------------****/
int LoDNDepotMgmtFreeReadCap(IBP_cap readCap)
{
	/* Declarations */
	struct ibp_timer timer = {60, 60};


	/* Attempts to free the mappings */
	return IBP_manage(readCap, &timer, IBP_DECR, IBP_READCAP, NULL);
}



