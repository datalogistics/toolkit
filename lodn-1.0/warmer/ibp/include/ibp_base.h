/*
 *         IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *        Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 1999 All Rights Reserved
 *
 *                              NOTICE
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
 * ibp_base.h
 * IBP base header file
 */
 
/* Basic structures used throughout the code */

# ifndef _IBPBASE
# define _IBPBASE

# ifdef HAVE_CONFIG_H
#include "config-ibp.h"
# endif

#ifdef STDC_HEADERS
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_PTHRAD_H
#include <pthread.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_SOCKETVAR_H 
#include <sys/socketvar.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/*
# ifdef _LINUX   
typedef unsigned long int   ulong_t;
typedef unsigned short int  ushort_t;
# endif 

# ifdef _DARWIN
# include <time.h>
typedef unsigned long int   ulong_t;
typedef unsigned short int  ushort_t;
typedef size_t              socklen_t;
# endif 
*/

#define  IBP_MAXUL 18446744073709551615
#define  IBP_MINL  (-9223372036854775807 - 1)
#define  IBP_MAXL  9223372036854775807
#define  IBP_MINAL 9223372036854775808

#ifdef HAVE_INT64_T
typedef int64_t  ibp_long;
typedef u_int64_t ibp_ulong;
#else
typedef long int  ibp_long;
typedef unsigned long int ibp_ulong;
#endif


typedef struct IPv4_addr{
	uint32_t	addr;
	uint32_t	mask;
	struct IPv4_addr *next ;
}IPv4_addr_t; 

#ifdef AF_INET6
typedef struct IPv6_addr{
	uint8_t		addr[16];
	uint8_t		mask[16];
	struct IPv6_addr *next ;
}IPv6_addr_t;
#endif

/*
# define IBP_DEBUG 1
*/


# define IBP_STEP	     100

# define IBP_CONTROL_PORT    6715

# define IBP_READCAP    1
# define IBP_WRITECAP   2

# define MAX_FILE_NAME  256 
# define MAX_KEY_LEN    256 

# define HOST_NAME_LEN  1024
# define MAX_CAP_LEN    2048

# define DATA_BUF_SIZE  524288 
# define CMD_BUF_SIZE   1024

# define MAX_NUM_TOKENS   16
# define MAX_TOKEN_SIZE   512

# define MAX_DIM_LEN  10
# define MAX_KEY_NAME_LEN     ( MAX_DIM_LEN * 25 ) 
# define MAX_RWM_KEY_LEN      ( MAX_DIM_LEN * 1 )
# define MAX_KEY_NAME_LEN_NEW ( MAX_DIM_LEN * 25 )
# define MAX_RWM_KEY_LEN_NEW  ( MAX_DIM_LEN * 1 ) 

# define min(a,b) ((a)<(b) ? (a):(b))
# define max(a,b) ((a)>(b) ? (a):(b))

# define IBP_MIN(a,b) min(a,b)

#if 0
typedef struct ibp_attributes {
   time_t            duration;         /* duration=0 => Permanent data */
                                       /* duration>0 => Expire at time x */
   int               reliability;      /* reliability = IBP_VOLATILE */
                                       /* or          = IBP_STABLE */
   int               type;             /* IBP_BYTEARRAY, IBP_FIFO or  */
                                       /* IBP_BUFFER   */ 
} *IBP_attributes;

typedef struct ibp_depot {
  char 		host[MAX_FILE_NAME];
  int		port;
} *IBP_depot;

typedef struct ibp_dptinfo {
  ulong_t		StableStor;
  ulong_t		StableStorUsed;
  ulong_t		VolStor;
  ulong_t		VolStorUsed;
  long			Duration;
} *IBP_DptInfo;

typedef struct ibp_timer {
  int 			ClientTimeout;
  int			ServerSync;
} *IBP_timer;
 
typedef struct ibp_capstatus{
   int                     readRefCount;
   int                     writeRefCount;
   int                     currentSize;
   ulong_t                 maxSize;
   struct ibp_attributes   attrib;
   char                    *passwd;
} *IBP_CapStatus;

typedef struct ibp_set_of_caps {
   IBP_cap   readCap;
   IBP_cap   writeCap;
   IBP_cap   manageCap;
} *IBP_set_of_caps;

typedef char* IBP_cap;
#endif

typedef int dtmv_op;
typedef void dtmv_params;


#if 0
#ifdef HAVE_PTHREAD_H 
extern int *_IBP_errno();
#define IBP_errno (*_IBP_errno())
#else
int         IBP_errno;
#endif
#endif

#define MAXLEN 1001
#define MAXFIELDS 1000
#define READCAP		1
#define WRITECAP 	2
#define MANAGECAP 	3

#ifndef _FIELDS_
typedef struct inputstruct {
  char *name;               /* File name */
  FILE *f;                  /* File descriptor */
  int line;                 /* Line number */
  char text1[MAXLEN];       /* The line */
  char text2[MAXLEN];       /* Working -- contains fields */
  int NF;                   /* Number of fields */
  char *fields[MAXFIELDS];  /* Pointers to fields */
  int file;                 /* 1 for file, 0 for popen */
} *IS;
#endif /* _FIELDS_ */

typedef struct iurl {
	char    *hostname;
	int     port;
	char    *key;
	int     type_key;
	char    *type;
	int	fd;
}IURL;

# define d_CheckHost(host)  ((host == NULL) ? IBP_E_INV_PAR_HOST : IBP_OK )
# define d_CheckPort(port)  ((port == 0) ? IBP_DATA_PORT : (d_PortInRange(port)) )
# define d_PortInRange(port)  (((port < 1024) || (port > 65535)) ? IBP_E_INV_PAR_PORT : port )
# define d_CheckAttrRid(rid) ((rid >= 0) ? IBP_OK : IBP_E_INVALID_RID)
# define d_CheckAttrRely(rely) (((rely == IBP_STABLE) || (rely == IBP_VOLATILE)) ? IBP_OK : IBP_E_INV_PAR_ATRL)
# define d_CheckAttrType(type) (((type == IBP_BYTEARRAY) || (type == IBP_BUFFER) || (type == IBP_FIFO) || (type == IBP_CIRQ)) ? IBP_OK : IBP_E_INV_PAR_ATTP)  

#if 0
/* ----  for Data Movers ---- */
# define DM_CLIENT  34
# define DM_SERVER  35
# define DM_SERVREP 36
# define DM_CLIREP  37

# define DM_UNI     38
# define DM_MULTI   39
# define DM_BLAST   40
# define DM_MBLAST  41
# define DM_SMULTI  42
# define DM_PMULTI  43
# define DM_MULTICAST 60
# define DM_MCAST     61


# define PORTS      3
# define HOSTS      4
# define OPTIONS    5
# define KEYS       6
# define SERVSYNCS  7
# define TIMEOUTS   8
/* -------------------------- */
#endif
 
# endif









