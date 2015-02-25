#ifndef _LBONE_CLIENT_LIB_H
#define _LBONE_CLIENT_LIB_H

#include <unistd.h>
#define  _REENTRANT
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "lbone_base.h"
#include "lbone_socket.h"
#include "ibp.h"
#include "jrb.h"

#define CLIENT_REQUEST  1
#define CONSOLE_REQUEST 2
#define LBONE_MESSAGE_LENGTH    512
#define SUCCESS         1
#define FAILURE         2
#define LBONE_CLIENT_VERSION    "0.0.1"


/*
 * An IBP_depot is already a pointer that points to an ibp_depot struct
 */

#define	Depot	IBP_depot


/*
 * This is the lbone client request struct.
 */

typedef struct lbone_request {
  int                   numDepots;     /* max number of returned depots           */
  unsigned long         stableSize;    /* min acceptable MB of stable storage     */
  unsigned long         volatileSize;  /* min acceptable MB of volatile storage   */
  double                duration;      /* min number of days for storage          */
  char                  *location;     /* location hints                          */
} LBONE_request;


/*******************************************************************************
 * Function:	lbone_getDepots()
 *
 * Parameters:	Depot		lboneServer	pointer to IBP_depot struct
 * 						with host and port for the lbone
 *		LBONE_request	request		the requirements for storage
 *		int		timeout		the number of seconds to return
 *						if the server has not responded
 *
 * Return Value:	on success, an array of depots (NULL if no depots match)
 *			on failure, NULL
 *
 * Description:	Primary client call. Get list of depots from server
 *******************************************************************************/

Depot *lbone_getDepots(Depot lboneServer, LBONE_request req, int timeout);




/*******************************************************************************
 * Function:	lbone_checkDepots()
 *
 * Parameters:	Depot		*depots		pointer to an array of Depots
 *		LBONE_request   request         the requirements for storage
 *		int             timeout         the number of seconds to return
 *                                              if the server has not responded
 *
 * Return Value:	on success, an array of depots (NULL if no depots match)
 *                      on failure, NULL
 *
 * Description:	Checks to see if depots are accessible and still meet request 
 * 		criteria
 *
 *		Be sure to compile with -lpthread
 *
 *******************************************************************************/

Depot *lbone_checkDepots(Depot *depots, LBONE_request request, int timeout);


/*******************************************************************************
 * Function:	lbone_depotCount()
 *
 * Parameters:	ulong_t		*totalDepots	where function should store value
 * 		ulong_t		*liveDepots	where function should store value
 *              Depot		lboneServer	who to call
 *		int             timeout         the number of seconds to return
 *
 * Return Value:	on success, values stored in totalDepots and liveDepots
 *                      on failure, NULL
 *
 * Description:	Calls lbone server and gets total and live depots (as of last poll)
 *
 *******************************************************************************/

int lbone_depotCount(ulong_t *totalDepots, ulong_t *liveDepots, Depot lboneServer, int timeout);



/*******************************************************************************
 * Function:	nws_ping()
 * Parameters:	char		*hostname		name of depot
 * 		int		port			port of depot
 * 		LBONE_request	request			the lbone request
 * 		double		*bandwidth		a pointer to a double to
 * 							store the bandwidth value
 * 		double		*latency		a pointer to a double to
 * 							store the latency value
 *
 * Return Value:	 0	on success
 * 			-1	on failure
 *
 * Description:		Get the bandwidth to/from the depot
 *
 *			Be sure to compile with libnws.a and -lpthread
 *
 *******************************************************************************/

int nws_ping(char *hostname, int port, double *bandwidth, double *latency);


/*
 *  retrieve the Depot 2 Depot resolution as specified by 'type'.  one of
 *  either NWS or geographic proximity.
 */
#define LBONE_RESOLUTION_NWS       0x01
#define LBONE_RESOLUTION_GEO       0x02
int lbone_getDepot2DepotResolution(double ***resArray, Depot lboneServer, 
                                       Depot *src_list, Depot *target_list,
                                       int type, unsigned int timeout);


/*******************************************************************************
 * Needed by lbone_checkDepots - not user accessible
 *******************************************************************************/

typedef struct {
  int	working;
  pthread_mutex_t lock;
} Global;

typedef struct {
  int		id;
  int		*result;
  int		timeout;
  Depot		ibp;
  LBONE_request request;
  Global	*g;
  double	*bandwidth;
} Info;

#endif
