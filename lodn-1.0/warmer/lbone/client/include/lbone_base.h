/*
 *		LBONE version 1.0:  Logistical Backbone
 *		 University of Tennessee, Knoxville TN.
 *	  Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
 *		     (C) 1999 All Rights Reserved
 *
 *				NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appear in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * Neither the Institution (University of Tennessee) nor the Authors 
 * make any representations about the suitability of this software for 
 * any purpose. This software is provided ``as is'' without express or
 * implied warranty.
 *
 */

/*
 * lbone_base.h
 * LBONE base header file
 */
 
/* Basic structures used throughout the code */

#ifndef _LBONEBASE
#define _LBONEBASE

#include "fields.h"
#include "ibp.h"

#ifndef _IBPBASE
#ifdef _LINUX	
typedef unsigned long int   ulong_t;
//typedef unsigned short int  ushort_t;
#endif 
#endif 

#ifdef _DARWIN 
#include <time.h>
/*typedef unsigned short int	ushort_t;*/
typedef unsigned int		uint_t;
typedef unsigned long int	ulong_t;
/*typedef size_t			socklen_t;*/
/*typedef int                 socklen_t; */
#endif 

#ifdef PLAT_RS6K
#include <sys/socketvar.h>
#endif

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#define  _REENTRANT
#include <pthread.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include <sys/resource.h>

#include "fields.h"
#ifdef _SERVER
#include "lber.h"
#include "ldap.h"
#include "ibp.h"
#define	LBONE_HEADER_LENGTH	20
#endif

#define LBONE_DEBUG 1

#define CLIENT_REQUEST	1
#define CONSOLE_REQUEST 2
#define DEPOT_COUNT	3
#define PROXIMITY_LIST	4
#define DEPOT2DEPOT_RESOLUTION  5

#define	LBONE_STEP	100
#define LBONE_PORT	6767
#define MAX_HOSTNAME	1024
#define	ALL_DEPOTS	-1

#define LBONE_TEMP		"o=lbone"
#define LBONE_TREE		"o=lbone"
#define LBONE_PASS		"lbone"
#define LBONE_LDAP		"cn=root,o=lbone"

#define CONTINENTDN		"continent=continent,o=lbone"
#define CONTINENT		"continent"
#define COUNTRY			"country"
#define MAINCITYDN		"maincity=maincity,o=lbone"
#define MAINCITY		"maincity"
#define COUNTRYZIP		"country_zipcode/city"
#define ZIPDEPOT		"zipcode/city_depot"

#define LOCATIONSDN		"geographical_locations=lat_lon,o=lbone"

#define WILDCARD		"*"
#define REALDEPOT		"depotname"
#define UPDATE_COUNT		5
#define STORAGEUNIT		1048576
#define SECSperDAY		86400


#define min(a,b) ((a)<(b) ? (a):(b))
#define max(a,b) ((a)>(b) ? (a):(b))

/*
 * typedef struct lbone_request {
 *   int			numDepots;
 *   unsigned long		stableSize;
 *   unsigned long		volatileSize;
 *   double		duration;
 *   char			*location;
 * } LBONE_request;
 */

typedef struct request {
  int			numDepots;
  unsigned long		stableSize;
  unsigned long		volatileSize;
  long			duration;
  char			*location;
} Request;

int	LBONE_errno;

#define LBONE_ILLEGAL_FILENAME		-1100
#define LBONE_ILLEGAL_TIMEOUT		-1101
#define LBONE_CONNECTION_FAILED		-1102
#define LBONE_SOCKET_WRITE_FAILED	-1103
#define LBONE_SOCKET_READ_FAILED	-1104
#define LBONE_SELECT_ERROR		-1105
#define LBONE_GET_TOKEN_FAILED		-1106
#define LBONE_RECEIVE_TO_NULL_BUFFER	-1107
#define LBONE_CLIENT_TIMEDOUT		-1108
#define LBONE_OPEN_FAILED		-1109
#define LBONE_CALLOC_FAILED		-1110
#define LBONE_UNKNOWN_CLIENT_REQUEST	-1111
#define LBONE_ILLEGAL_PARAMETER 	-1112
#define LBONE_LDAP_INIT_FAILED          -1113
#define LBONE_INVALID_PARAMS            -1114
#define LBONE_SEND_TOKEN_FAILED         -1115

#endif
