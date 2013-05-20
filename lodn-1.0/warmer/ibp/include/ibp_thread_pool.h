#ifndef IBP_THREAD_POOL_H
#define IBP_THREAD_POOL_H

#include "ibp_request_queue.h"

#ifdef __cplusplus 
extern "C" {
#endif /* __cplusplus */

#define IBP_TPOOL_MIN_IDLE_THREAD   4
#define IBP_THD_MAX_IDLE_TIME       600  

typedef struct ibp_thread_handler
{
    int                     threadID;           /* thread ID */
    struct ibp_thread_pool  *pool;           /* thread pool */
    int                     status;
}
IBP_THREAD_HANDLER;

typedef void *(*IBP_WORKER_THREAD)(void *);

/* create a thread handler */
extern IBP_THREAD_HANDLER *
init_thread_handler(int threadID);

typedef struct ibp_thread_pool
{
    int                 maxThdNum;  /* maximum number of threads in the pool */
    int                 thdNum;     /* number of running threads */ 
    int                 idle;       /* number of idle threads */
    IBP_REQUESTS_QUEUE  *reqQueue;  /* request queue */
    IBP_THREAD_HANDLER  *handles;    /* thread handles array */
    pthread_mutex_t*    lock;       /* lock to exclusive access pool */ 
}
IBP_THREAD_POOL;

extern IBP_THREAD_POOL*
create_thread_pool ( int                maxThdNum ,
                     IBP_REQUESTS_QUEUE *queue);

extern void 
expand_thread_pool(IBP_THREAD_POOL *pool, IBP_WORKER_THREAD worker, int num);

extern void 
free_thread_handler(IBP_THREAD_POOL *pool, IBP_THREAD_HANDLER *handler );

extern int
get_idle_thread_num(IBP_THREAD_POOL *pool);

extern void
inc_idle_thread(IBP_THREAD_POOL *pool);

extern void
dec_idle_thread(IBP_THREAD_POOL *pool);

extern int
get_thread_num(IBP_THREAD_POOL *pool);

#ifdef __cplusplus
}
#endif


#endif /* IBP_THREAD_POOL_H */
