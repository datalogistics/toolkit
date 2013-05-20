#ifndef LODNTHREADS_H_
#define LODNTHREADS_H_


#include <pthread.h>
#include "dllist.h"


/* Forward Declaration */
typedef struct lodn_thread_pool LoDNThreadPool;

typedef struct
{
    pthread_t       tid;
    LoDNThreadPool *threadPool;
    pthread_mutex_t lock;
    pthread_cond_t  signal;
    void (*job)(void *);
    void *data;
    
} LoDNThread;


struct lodn_thread_pool
{
    LoDNThread *threads;
    int numThreads;
    int numInActiveThreads;
    Dllist inActiveThreads;
    pthread_mutex_t lock;
    pthread_cond_t  signal;
        
    
};



LoDNThreadPool *makeLoDNThreadPool(int numThreads);
void LoDNThreadPoolWaitOnFreeThread(LoDNThreadPool *threadPool);
void LoDNThreadPoolAssignJob(LoDNThreadPool *threadPool, void (*job)(void *), void *);
void freeLoDNThreadPool(LoDNThreadPool *threadPool);
void LoDNThreadsPoolFinished(LoDNThreadPool *threadPool);




#endif /*LODNTHREADS_H_*/
