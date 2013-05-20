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
 * ibp_server.h
 */

# ifndef _IBPSERVER_H
# define _IBPSERVER_H

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# include "ibp_version.h"
# include "ibp-lib.h"
# include "ibp_protocol.h"
# include "ibp_base.h"
# include "jval.h"
# include "fields.h"
# include "jrb.h"
# include "dlist.h"
# include "ibp_resources.h"
# include "ibp_request.h"
# include <stdlib.h>

# ifdef HAVE_PTHREAD_H 
# include "ibp_thread.h"
# endif

# define IBP_ALLPOL_SID 0
# define IBP_ALLPOL_EXP 1
# define SERVER_BUFFER_SIZE 524288
# define IBPSTACKSIZE  (2*1024*1024)
# define IBP_MAX_SHUTDOWN_WAIT_TIME 10

# define DONE   	0
# define RELY   	1
# define KEY    	2
# define TYPE   	3
# define LIFE   	4
# define HOST   	5
# define PORT   	6
# define WKEY   	7
# define RKEY   	8
# define OFFSET	 	9
# define SIZE    	10
# define MKEY   	11
# define DCD_CMD    	12
# define DCD_CAP    	13
# define DCD_MAXSIZE	14
# define DCD_IBPCAP     15
# define DCD_PWD        16
# define DCD_SRVSYNC    17
# define DCD_TRGSYNC    18
# define DCD_TRGTOUT    19
# define RESC           20

# define IBP_HALL_TYPE_STD  0
# define IBP_HALL_TYPE_PING 1

# define IBP_HALL_DONE      0
# define IBP_HALL_PARAM_S   1
# define IBP_HALL_PARAM_P   2
# define IBP_HALL_NEWCONT_S 101
# define IBP_HALL_NEWCONT_P 102
# define IBP_HALL_CREATE_S  201
# define IBP_HALL_CREATE_P  202
# define IBP_HALL_FILL_S    301
# define IBP_HALL_FILL_P    302
# define IBP_HALL_CLOSE_S   401
# define IBP_HALL_CLOSE_P   402
 
# define CLOSE		-1
# define HLSD_DONE      0
# define HLSD_INIT_BA 	1
# define HLSD_INIT_BU 	2
# define HLSD_INIT_FI 	3
# define HLSD_INIT_CQ 	4
# define HLSD_SEEK_BA   101
# define HLSD_SEEK_BU   102
# define HLSD_SEEK_FI   103
# define HLSD_SEEK_CQ   104
# define HL_READ_BA	211
# define HL_READ_BU	212
# define HL_READ_FI	213
# define HL_READ_CQ	214
# define HSC_SEND_BA    221
# define HSC_SEND_BU    222
# define HSC_SEND_FI    223
# define HSC_SEND_CQ    224
# define HD_GIVE_BA     231
# define HD_GIVE_BU     232
# define HD_GIVE_FI     233
# define HD_GIVE_CQ     234
# define HLSD_FINE_BA   301
# define HLSD_FINE_BU   302
# define HLSD_FINE_FI   303
# define HLSD_FINE_CQ   304
# define HL_DONE_BA     411
# define HL_DONE_BU     412
# define HL_DONE_FI     413
# define HL_DONE_CQ     414
# define HSC_AFTER_BA   421
# define HSC_AFTER_BU   422
# define HSC_AFTER_FI   423
# define HSC_AFTER_CQ   424
# define HD_DEL_BA      431
# define HD_DEL_BU      432
# define HD_DEL_FI      433
# define HD_DEL_CQ      434

# define HS_SIZE_BA	1
# define HS_SIZE_BU	2
# define HS_SIZE_FI	3
# define HS_SIZE_CQ	4
# define HS_FREE_BA     101
# define HS_FREE_BU     102
# define HS_FREE_FI     103
# define HS_FREE_CQ     104
# define HS_OPEN_BA	201
# define HS_OPEN_BU	202
# define HS_OPEN_FI	203
# define HS_OPEN_CQ	204
# define HS_SEEK_BA	301
# define HS_SEEK_BU	302
# define HS_SEEK_FI	303
# define HS_SEEK_CQ     304
# define HS_SOCK_BA	401
# define HS_SOCK_BU	402
# define HS_SOCK_FI	403
# define HS_SOCK_CQ	404
# define HS_S2FC_BA	501
# define HS_S2FC_BU	502
# define HS_S2FC_FI	503
# define HS_S2FC_CQ	504
# define HS_UPDT_BA	601
# define HS_UPDT_BU	602
# define HS_UPDT_FI	603
# define HS_UPDT_CQ	604
# define HS_CLOS_BA	701
# define HS_CLOS_BU	702
# define HS_CLOS_FI	703
# define HS_CLOS_CQ	704

# define HT_DONE        -1
# define HT_STABLE      1
# define HT_VOL         2
# define HT_DURATION    3

socklen_t gk_addrlen;

/*
#ifndef _IN_ADDR_T
#define _IN_ADDR_T
typedef uint32_t in_addr_t;
#endif
*/

#if 0
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
#endif

#define IBP_QUEUE_LEN_DEFAULT  32

typedef struct {
    char        *versionMajor;
    char        *versionMinor;
    char 		*hostname;
    JRB         resources;
    RESOURCE    *dftRs;
    Dlist       dataMovers;
#if 0
    ibp_ulong 	StableStorage;
    ibp_ulong 	StableStorageUsed;
    ibp_ulong 	VolStorage;
    ibp_ulong  	VolStorageUsed;
    ibp_ulong   MinFreeSize;
    char 		    *VolDir;
    char 		    *StableDir;
    long 		    MaxDuration;
    JRB         ByteArrayTree;
    JRB         VolatileTree;     /* tree of volatile bytearrays */
    JRB	        TransientTree;    /* Tree of transient (time-limited)*/
#endif
    int			    IbpSocket;
    int			    ControlSocket;
    int         IbpPort;
    fd_set		  MasterSet;
    fd_set		  WorkingSet;  
    int         RecoverTime;
    int         IBPRecover;        /* recover the old file or not  */
    int			    IBP_KeyCounter;     /* TO BE REVISED */
    pid_t       mdnspid;
    IPv4_addr_t	*IPv4;
    int         AllocatePolicy; 
    char        *execPath;
    char        *cafile;
    int         enableAuthen;
    char        *depotCertFile;
    char        *depotPrivateKeyFile;
    char        *depotPrivateKeyPasswd;
    int         enableLog;
    int         maxLogFileSize;
#ifdef AF_INET6    
    IPv6_addr_t		*IPv6;
#endif 
#ifdef HAVE_PTHREAD_H
    int         IbpThreadNum;  /* number of the threads for multi threads
                                  for multi threads ibp server */
    int         IbpThreadMin;  /* number of the minimum threads for multi 
                                  threads ibp server */
    int         IbpThreadMax;  /* number of the maximum threads for multi 
                                  threads ibp server */
    int         IbpThreadLogEnable;  /* = 0 disenable per thread log
                                       otherwise enable per thread log */
    pthread_mutex_t    keyLock ;     /* mutex used for generating ibp key */
    pthread_mutex_t    glbLock;      /* mutex used to access global variable */
#endif
    
    /*
     * following fields used by nfu
     */
    /*pthread_mutex_t     *nfu_lock;*/
    JRB                  nfu_libs;
    JRB                  nfu_ops;
    char                 *nfu_ConfigFile;
    int                  sys_page_size;
    int                  nfu_conf_modified;
    int                  queue_len;
    int                  idle_time;
    int                  enableAlloc;
} globals;

globals glb;

FILE 	*gf_logfd;
char    *gc_logfile;
int	gi_sklisten;
char    *gc_passwd;
char    *gc_cfgpath;
char    *gc_cfgname;

FILE	*gf_recoverfd;
int     gi_recoverclosed;
char	*gc_recoverfile;
FILE	*gf_recovertimefd;
char	*gc_recovertimefile;
time_t  gt_recoverstoptime;
 
typedef struct ibp_byteArray {
   unsigned long           maxSize;       /* Maximum storage in capability */
   unsigned long           currentSize;   /* current byte array size */
   int                     readRefCount;  /* Ref. count for read cap. */
   int                     writeRefCount; /* Ref. count for write cap. */
   int                     readKey;       /* Random number in read cap */
   int                     writeKey;       /* random number in write cap */
   int                     manageKey;     /* Random number in manage cap */
   int                     deleted;       /* = 1 for logical deletion */
   struct ibp_attributes   attrib;        /* Byte Array attributes */
   char                    *readKey_s;    
   char                    *writeKey_s;
   char                    *manageKey_s;
  int                      lock;          /* A lock = 1 when byteArray in use */
  int                      readLock;      /* readLock = 1 when being read */
  int                      writeLock;     /* writeLock = 1 when being written */
  char                     *key;          /* Key for access to capability */
  char                     *fname;        /* File name */
  char                     *startArea;    /* starting memory address for memory allocation */ 
  Dlist                    pending;       /* A dlist of pending requests */
  JRB                      volTreeEntry;  /* Entry in volatile storage tree*/
  JRB                      masterTreeEntry; /* Entry in master bytearray tree*/
  JRB                      transientTreeEntry; /* Entry in transient Tree */
  unsigned short           wl;   /* write lock */
  unsigned short           rl;   /* resd lock */
  unsigned short           bl;   /* buffer lock */
  void                     *thread_data;  /* data used in thread */

  int			                BackupIndex;	  /* data used for backup  */
  unsigned long		        size;	  /* these only used for FI & CQ */
  unsigned long		        free;
  unsigned long		        wrcount;
  unsigned long		        rdcount;
  time_t		              starttime;
  int                     reference;

} *IBP_byteArray;

/*
typedef struct ibp_request {
   int               request;
   int               fd;
} *IBP_request;
*/

typedef struct ibp_url {
  char          host[HOST_NAME_LEN+1];
  int           port;
  int           rid;
  char          key[MAX_KEY_LEN+1];
  char          attr_key[MAX_KEY_LEN+1];
  char          type[20];
  RESOURCE      *rs;
  IBP_byteArray ba;
}IBP_URL;

typedef struct ibp_file_header {
  int                   headerLen;
  unsigned long         maxSize;
  unsigned long         curSize;
  int                   rCount;
  int                   wCount;
  int                   rKey;
  int                   wKey;
  int                   mKey;
  int                   delete;
  struct ibp_attributes attr;
}IBP_FILE_HEADER;


typedef struct {
  int			          Cmd;
  RESOURCE          *rs;
  int               ver;
  unsigned long int	size;
  unsigned long int	NewSize;
  int			          lifetime;
  int			          reliable;
  int			          type;
  char              wKey[MAX_KEY_LEN];
  char              rKey[MAX_KEY_LEN];
  char              mKey[MAX_KEY_LEN];
  int			          writekey;
  int			          ReadKey;
  int			          ManageKey;
  int			          ManageCmd;
  unsigned long int	Offset;
  unsigned short int    port;
  char		          *host;
  char		          *key;
  int			          CapType;
  IBP_cap           Cap;
  IBP_URL           url;
  char              *passwd;
  int               ServerSync;
  int               TRGSYNC;
  int               TRGTOUT;
} ibp_command;


ibp_command *ibp_cmd;

/* 
typedef enum { IDLE, IN_USE, DIS_CONN } IBP_CONN_STATUS;

typedef struct {
  int               fd;
  time_t            idleTime;
  IBP_CONN_STATUS   status;  
  int               authenticated;
  char              *cn;
} IBP_connect;

typedef struct{
  IBP_connect *list;
  pthread_mutex_t *lock;
  int          len;
} IBP_connectList;
*/

int             glbRdPipe;
int             glbWrtPipe;              

/* IBP_connectList *glbServerConnList;*/

#define IBP_CONN_MAX_IDLE_TIME 15   /* 15 seconds */

int *IBPErrorNb;

char  **exec_info;

typedef struct {
  char	key[MAX_KEY_LEN];
  char	fname[PATH_MAX];
  int	readKey;
  int	writeKey;
  int	manageKey;
#if 0
  char rKey[MAX_KEY_LEN];
  char wKey[MAX_KEY_LEN];
  char mKey[MAX_KEY_LEN];
#endif
  time_t starttime;
} IBP_StableBackupInfo;

typedef struct {
  unsigned long	 maxSize;
  unsigned long	 currentSize;
  int		 readRefCount;
  int		 writeRefCount;
  unsigned long	 size;
  unsigned long	 free;
  unsigned long	 wrcount;
  unsigned long	 rdcount;
  struct ibp_attributes	 attrib;
  int		 valid;
} IBP_VariableBackupInfo;

enum config {VOLSIZE = 1, STABLESIZE, STABLEDIR, VOLDIR, MAXDURATION, CFGPORT} config;

#define BASE_HEADER ( 2 * sizeof(unsigned long) + 6 * sizeof(int) + \
sizeof(struct ibp_attributes))

#define HEADER_LEN (2*sizeof(unsigned long) + 7*sizeof(int) +\
sizeof(struct ibp_attributes) + 1024)

#define MAX_BUFFER (BASE_HEADER + PATH_MAX + MAX_KEY_LEN + sizeof(int))

/* Function prototypes */

char *createcap ( RESOURCE *rs , char *pc_Key, char *pc_RWM, char **rwmKey,int  version);
void jettisonByteArray(RESOURCE *rs, IBP_byteArray ba);
void truncatestorage(RESOURCE *rs, int pi_type, void *pv_par);
void handle_allocate(RESOURCE *rs, int pi_fd, int pi_nbt, ushort_t pf_AllocationType);
void handle_store(RESOURCE *rs,int pi_fd, int pi_destfd, int pi_nbt, IBP_byteArray ps_ByteArray);
void handle_lsd_disk (RESOURCE *rs , int pi_fd, int pi_nbt, IBP_byteArray ps_ByteArray);
void handle_lsd_ram (RESOURCE *rs , int pi_fd, int pi_nbt, IBP_byteArray ps_ByteArray);
char *handle_manage_disk (RESOURCE *rs , int pi_fd, int pi_nbt, IBP_byteArray ps_ByteArray);
char *handle_manage_ram (RESOURCE *rs , int pi_fd, int pi_nbt, IBP_byteArray ps_ByteArray);
void handle_status (RESOURCE *rs,int pi_fd, int pi_nbt);
void *decode_cmd (IBP_REQUEST *request, int pi_nbt);

int getconfig(char *pc_path);
int recover ();
int decodepar (int argc, char **argv);
int boot(int argc, char **argv);
int setIBPURI(IBP_URL *iu, IBP_cap cap ); 
/* 
IBP_connectList *IBP_createConnList(int size); 
IBP_connect *IBP_searchConn(IBP_connectList *list, int fd);
void IBP_lockConnList( IBP_connectList *list);
void IBP_unlockConnList(IBP_connectList *list);
IBP_connect *IBP_insertConn(IBP_connectList *list, int fd);
void IBP_setConnStatus( IBP_connect *connect, IBP_CONN_STATUS status);
void IBP_closeIdleConn(IBP_connectList *connList);
*/
int  IBP_createPipe();
/*
int IBP_setFDSet(fd_set *set, IBP_connectList *connList);
*/
int SaveInfo2LSS( RESOURCE *rs,IBP_byteArray ps_container, int pi_mode);

/*JRB vol_cap_insert(int pi_size, int pi_lifetime, Jval pu_val);*/
JRB vol_cap_insert( RESOURCE *rs , int pi_size, int pi_lifetime, Jval pu_val);
int check_size(RESOURCE *rs, long pi_size, int pi_mode);

#ifdef IBP_AUTHENTICATION
int ibpSrvAuthentication(int fd, IBP_CONNECTION *conn);
#endif

# endif






