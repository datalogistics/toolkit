#ifndef __LORS_JOB_H__
#define __LORS_JOB_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "lors_api.h"
/*#include "pipeline.h"*/

/*
#define JOB_INIT       0x00000001
#define JOB_CHECKOUT   0x00000002
#define JOB_EXEC       0x00000004
#define JOB_DONE       0x00000010
#define JOB_FAILED     0x00000020
#define JOB_PENDING    0x01000000
#define JOB_COMMIT     0x02000000
*/

typedef enum __lorsJobStatus {
   JOB_INIT,JOB_CHECKOUT,JOB_EXEC,JOB_DONE,JOB_COMMIT,JOB_PENDING,JOB_FAILED,JOB_CANCELED
} __LorsJobStatus;

typedef enum __lorsThreadStatus {
 THD_NULL,THD_START,THD_EXEC,THD_SUCCESS,THD_FAILED,THD_CANCELED 
} __LorsThreadStatus; 

typedef enum __lorsJobType{
 J_CREATE,J_STORE,J_DOWNLOAD,J_COPY_NO_E2E,J_COPY_E2E,J_MCOPY_NO_E2E,J_MCOPY_E2E, J_CONDITION, J_CREATE_STORE, J_CREATE_COPY, J_STAT, J_REFRESH, J_TRIM
} __LorsJobType;

/*typedef Dllist   __LorsDLList;*/
typedef struct __lorsdllist 
{
    Dllist            list;
    pthread_mutex_t   lock;
} *__LorsDLList;

typedef struct __lorsDLNode{
    int               notvalid;
    pthread_mutex_t     lock;
    Dllist            mappings;
    longlong          offset;
    longlong          length;
    int               good_copies;
}__LorsDLNode;

typedef struct __lorsJobPool
{
     pthread_mutex_t   lock;
     Dllist            job_list;
     __LorsJobType     type;
     longlong          src_offset;
     longlong          src_length;
     longlong          src_cur_point;
     longlong          dst_offset;
     longlong          dst_length;
     longlong          dst_cur_point;
     __LorsJobStatus   status;
     JRB               pending_jobs;
     int               njobs;

     /*  following fields are only used by download job*/ 
     /*LorsQueue        *q;*/
     int              fd;
     char             *buf;
     int              pre_buf;
     int              cache;
     int              max_thds_per_depot;
     int              thds_per_job;
     pthread_cond_t   cond;
     int              n;
     void             *thread_pool;
     pthread_cond_t   write_cond;
     pthread_mutex_t  write_lock;
     int              num_pending_jobs;
     pthread_cond_t   read_cond;
} __LorsJobPool;

typedef struct __lorsJob 
{
    __LorsJobPool     *jp; 
    __LorsJobType     type;
    void              *src;
    void              *dst;
    void              *extra_data;
    longlong          src_offset;
    longlong          src_length;
    longlong          src_cur_point;
    longlong          dst_offset;
    longlong          dst_length;
    longlong          dst_cur_point;
    int               timeout; 
    int               status;
    int               err_code;

    /* following fields are only used by download job */
    pthread_mutex_t  lock;
#if 0
    JRB              worker_threads;
#endif
    int              n_threads;
    int              waitting;
    int              n_done_ahead;
    void            *param[2];
}__LorsJob;

typedef struct __lorsThreadInfo 
{
    pthread_t            tid;
    pthread_attr_t       attr;
    struct __lorsThreadPool     *tp;
    __LorsJobPool        *jp;
    __LorsJob            *job;
    int                  timeout;
    int                  opts;
    __LorsThreadStatus   status;
    int                  err_code;

    /* following flieds are only used by downloading */
    pthread_mutex_t     dum_lock;
} __LorsThreadInfo;

typedef struct __lorsThreadPool 
{
     int               nthread;
     __LorsThreadInfo  *threads;
#if 0
     Dllist            extra_threads;
     pthread_mutex_t   lock;
#endif
} __LorsThreadPool;

int _lorsCreateJobPool ( __LorsJobPool **jp );
void  _lorsAddJob ( __LorsJobPool *jp,  __LorsJob *job );
int _lorsGetJob ( __LorsJobPool *jp , __LorsJob **job);
void _lorsCommitJob ( __LorsJob *job, __LorsJobStatus status, int err_code);
int  _lorsCommitDownloadJob (__LorsJob *job, __LorsJobStatus status, int err_code , LorsMapping *lm, LorsDepot *ldp, char *buf);
void _lorsDoCleanup ( void *job);
void _lorsDoCleanupDL (void *paras);
void _lorsFreeJobPool ( __LorsJobPool *jp );
void _lorsFreeThreadPool ( __LorsThreadPool *tp);
int _lorsCreateThreadPool ( __LorsJobPool *jp, int nthread,__LorsThreadPool **tp, int timeout, int opts );
int _lorsCreateOutputThread(pthread_t *tid, __LorsJobPool *jp);
int _lorsCreateDLList(LorsSet *set, longlong offset, longlong length, __LorsDLList *list); 
#endif /* __LORS_JOB_H__ */
