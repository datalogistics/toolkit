#ifndef IBP_RESOURCES_H
#define IBP_RESOURCES_H

#ifdef HAVE_CONFIG_H
#include "config-ibp.h"
#endif
#ifdef STDC_HEADERS
#include <stdio.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "jrb.h"
#include "ibp_base.h"
#include "fields.h"

#define DIR_SEPERATOR_CHR   '/'
#define DIR_SEPERATOR_STR   "/"
#define DEFAULT_RID         "#"
#define DEFAULT_RID_CHR     '#'
#define ALL_RID             "*"
#define ALL_RID_CHR         '*'
#define RS_RESOURCEID       "RESOURCEID"
#define RS_END              "RESOURCE_END"
#define RS_TYPE             "RSTYPE"
#define RS_T_DISK           "DISK"
#define RS_T_MEMORY         "MEMORY"
#define RS_T_RMT_DEPOT      "RMT_DEPOT"
#define RS_T_ULFS           "ULFS"
#define RS_LOCATION         "LOCATION"
#define RS_ZEROCONF         "ZEROCONF"
#define RS_SUBDIRS          "DATADIRS"
#define RS_SOFTSIZE         "SOFTSIZE"
#define RS_HARDSIZE         "HARDSIZE"
#define RS_STABLESIZE       "STABLESIZE"
#define RS_VOLSIZE          "VOLSIZE"
#define RS_MINFREESIZE      "MINFREESIZE"
#define RS_MAXDURATION      "MAXDURATION"
#define RS_IPV4             "CL_IPv4"
#define RS_IPV6             "CL_IPv6"

typedef union {  /* resource location */
    char   *dir;
    struct { 
        char *host;
        int  port;
        int  RID;
    } rmtDepot;
    struct {
        char *cfgLoc;
        void *ptr;
    }ulfs;
}RSLOC; 

typedef enum {
    NONE,MEMORY,DISK,ULFS,RMT_DEPOT
}RSTYPE;

typedef struct {
    int         RID;  /* resource ID */
    RSTYPE       type;  /* resource type */
    RSLOC        loc;  /* resource location */
    ibp_ulong    StableStorage; /* size of stable space */
    ibp_ulong    StableStorageUsed; /* size of occupied stable space */
    ibp_ulong    VolStorage;    /* size of the volatile space */
    ibp_ulong    VolStorageUsed;    /* size of occupied volatile space */
    ibp_ulong    MinFreeSize;   /* minimum size of free space */
    long         MaxDuration;   /* maximum longevity of  single capability */
    JRB          ByteArrayTree; /* tree of byte arrays */
    JRB          VolatileTree;  /* tree of volatile byte arrays */
    JRB          TransientTree; /* tree of transient ( time-limited ) */
    pthread_mutex_t alloc_lock; /* mutex for the resource allocation */
    pthread_mutex_t recov_lock; /* mutex for recovery */
    pthread_mutex_t change_lock; /* mutex for change jrb tree */
    pthread_mutex_t rlock;       
    pthread_mutex_t wlock;
    pthread_mutex_t block;
    char         rcvFile[PATH_MAX];
    FILE         *rcvFd;
    char         logFile[PATH_MAX];
    FILE         *logFd;
    int          zeroconf;
    int          subdirs;
    char         rcvTimeFile[PATH_MAX];
    FILE         *rcvTimeFd;
    int          NextIndexNumber;
    time_t       rcvOptTime;
    IPv4_addr_t  *IPv4;
#ifdef AF_INET6
    IPv6_addr_t  *IPv6;
#endif
} RESOURCE;

RESOURCE *crtResource ();
void  freeResource( RESOURCE *rs);
void  printResource( RESOURCE *rs, FILE *out, char *pad);
int getRsCfg( RESOURCE *rs, IS cfgFile); 

#endif
