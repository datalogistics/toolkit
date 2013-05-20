#include <pthread.h>        /* pthread apis */
#include <assert.h>         /* assert() */
#include <stdlib.h>         /* calloc */

#include "ibp_server.h"
#include "ibp_thread_pool.h"

extern int glbDepotShutdown;
extern void *ibp_calloc(size_t count, size_t size);

IBP_THREAD_POOL *
create_thread_pool (int                max,
                    IBP_REQUESTS_QUEUE *queue)
{
    IBP_THREAD_POOL     *pool = NULL;
    IBP_THREAD_HANDLER  *handler = NULL;
    int                 i;
    
    assert(max > 0 && queue != NULL);

    pool = (IBP_THREAD_POOL*)ibp_calloc(1,sizeof(IBP_THREAD_POOL));
    pool->maxThdNum = max;
    handler = (IBP_THREAD_HANDLER*)ibp_calloc(max,sizeof(IBP_THREAD_HANDLER));
    for ( i = 0 ; i < pool->maxThdNum ; i++){ 
        handler[i].threadID = i;
        handler[i].pool = pool;
        handler[i].status = 0;
    }
    pool->handles = handler;
    pool->thdNum = 0;
    pool->idle = 0;
    pool->reqQueue = queue;
    pool->lock = (pthread_mutex_t *)ibp_calloc(1,sizeof(pthread_mutex_t));
    pthread_mutex_init(pool->lock,NULL);

    return (pool);
}

void
expand_thread_pool(IBP_THREAD_POOL      *pool,
                   IBP_WORKER_THREAD    worker,
                   int                  num)
{
    int             i,j;
    pthread_t       tid;
    pthread_attr_t  attr;
    int             ret;

    assert(pool != NULL && num >= 0);
    if ( glbDepotShutdown ) {
        return;
    };

    j = 0;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr,IBPSTACKSIZE);
    if ( pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) != 0){
        fprintf(stderr,"Unable to set detached thread !\n");
        exit(-1);
    }

    pthread_mutex_lock(pool->lock);
    for ( i=0,j=0; i<pool->maxThdNum && j<num && pool->thdNum<pool->maxThdNum ; i++){
        if ( pool->handles[i].status == 0 ){
            /* empty slot, create a new thread*/
            pool->handles[i].status = 1;
            ret = pthread_create(&tid,&attr,worker,&(pool->handles[i]));
            if ( ret != 0 ){
                pool->handles[i].status = 0;
                fprintf(stderr,"Unable to create a new worker thread!\n");
            }else{
                pool->thdNum ++; 
                j++;
            }
        }
    }
    pthread_mutex_unlock(pool->lock);

    return ;
}

int
get_thread_num ( IBP_THREAD_POOL *pool)
{
    int num;

    pthread_mutex_lock(pool->lock);
    num = pool->thdNum;
    pthread_mutex_unlock(pool->lock);

    return(num);

}
int
get_idle_thread_num ( IBP_THREAD_POOL *pool)
{
    int ret = 0;

    pthread_mutex_lock(pool->lock);
    ret = pool->idle;
    pthread_mutex_unlock(pool->lock);

    return (ret);
}

void 
free_thread_handler( IBP_THREAD_POOL *pool, IBP_THREAD_HANDLER *handler)
{
    int tid;

    assert(pool != NULL);

    pthread_mutex_lock(pool->lock);
    tid = handler->threadID;
    assert(pool->handles[tid].status == 1);
    pool->handles[tid].status = 0;
    pool->thdNum --;
    pthread_mutex_unlock(pool->lock);

    return;
}

void 
inc_idle_thread( IBP_THREAD_POOL *pool)
{
    pthread_mutex_lock(pool->lock);
    pool->idle++;
    assert(pool->idle <= pool->thdNum);
    pthread_mutex_unlock(pool->lock);
}


void 
dec_idle_thread( IBP_THREAD_POOL *pool)
{
    pthread_mutex_lock(pool->lock);
    pool->idle--;
    assert(pool->idle >= 0);
    pthread_mutex_unlock(pool->lock);
}

