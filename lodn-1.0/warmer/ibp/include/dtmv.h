#ifndef  DTMV_H
#define  DTMV_H

#ifdef HAVE_CONFIG_H
#include "config-ibp.h"
#endif
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "ibp-lib.h"

#include "fields.h"
#include "dlist.h" 
 
#define SPORT 3456    /* port will be used for the DM server*/
#define IBPPORT 5678  /* port will be used by IBP to serve DM */

#define MAX_ARG_LEN      1024
#define MAX_HOST_LEN     64
#define MAX_COMM_LEN     64
#define MAX_WAIT_TIME    1
#define MAX_NUM_ATTEMPTS 5
#define MAX_NUM_ARGS     64
#define MAX_WORD_LEN     8
 
/* --- in the future, these will be the kinds of parameters --- */ 
#define I 20     /* timeout data */
#define D 21     /* service type */
#define P 22     /* ports */
#define H 23     /* host */
#define O 24     /* options */
#define T 25     /* targets */
#define S 26     /* source */
#define K 27     /* key type */
#define F 28     /* offset in the source file */
#define Z 29     /* source file size */
#define N 30     /* the name of the DMhat program */
 
#define DM_CLIENT 34
#define DM_SERVER 35 

#define DM_SUCCESS 1
#define DM_FAILURE 0

int numTargets;                     /* how many targets uni/multicast */

/* --- this structure defines the arguments for data mover --- */
typedef struct{
  int     dm_type;                  /* the type of dm that will be used - we need an index or so */
  int     dm_port;                  /* the port in which the dm program will run */
  int     dm_service;               /* tells what kind of datamover */
  int     sourcefd;                 /* the file descriptor in the source */
  char    sourcekey[MAX_KEY_LEN];   /* the file name assigned in the source */
  char    targetkey[MAX_KEY_LEN];   /* the file name assigned in the target */
  int     type_key;
  int     src_key;
  ulong_t offset;                   /* file offset to start */
  ulong_t filesize;                 /* amount of byte to be transfered */
  char    source[64];               /* which is the source host */
  char    target[64];               /* which is the target (only one for now) */
  int     ibp_port;                 /* port in which IBP server is running in the target */
} DMAS;

typedef DMAS* DMASP;

/* --- this structure defines an argument type node --- */
typedef struct arg_node{
  int type;                     /* indicates which type of argument */
  char **args;                  /* array of arguments (multiple targets) */
} *Anode;


/* --- this specifies the exit and return values --- */
#define DM_FILEERR -1
#define DM_PROCERR -2
#define DM_TYPEERR -3
#define DM_NETWERR -4
#define DM_IBPERR  -5
#define DM_TIMEOUT -6
#define DM_IBPERR  -5

#define DM_CLIOK    1
#define DM_SERVOK   2

#endif
