
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "dllist.h"
#include "LoDNLogging.h"
#include "LoDNThreads.h"



static void *threadRoutine(void *arg)
{
    /* Declarations */
    LoDNThread *thread;
    
    
    /* Gets a pointer to the thread struct */
    thread = (LoDNThread*)arg;
    
    /* Locks the mutex for the thread */
    if(pthread_mutex_lock(&thread->lock) != 0)
    {
       logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
    }
    
    /* Loops until the program ends */
    while(1)
    {
        /* Waits until there is a job for the program to do */
        while(thread->job == NULL)
        {
            if(pthread_cond_wait(&thread->signal, &thread->lock) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error"); 
            }
        }
    
        /* The thread executes the job */
        thread->job(thread->data);
     
        /* Clears the job */
        thread->job  = NULL;
        thread->data = NULL;
          
        /* Signals the thread pool that it is done and there is one more free thread */
        if(pthread_mutex_lock(&thread->threadPool->lock) != 0)
        {
           logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
        }
       
        thread->threadPool->numInActiveThreads++;
        
        /* Adds it to the list of inactive threads */
        dll_append(thread->threadPool->inActiveThreads, new_jval_v(thread));
        
       
        if(pthread_cond_signal(&thread->threadPool->signal) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error"); 
        }
        
        if(pthread_mutex_unlock(&thread->threadPool->lock) != 0)
        {
           logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlocking error"); 
        }
    }         
    
    
    /***--This should never be reached--***/
    
    
    /* Unlocks the thread */
    if(pthread_mutex_unlock(&thread->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlocking error"); 
    }
    
    /* Ends the thread */
    pthread_exit(NULL);   
}


LoDNThreadPool *makeLoDNThreadPool(int numThreads)
{
    /* Declarations */
    LoDNThreadPool *threadPool  = NULL;
    LoDNThread     *thread      = NULL;
    pthread_attr_t  pthreadAttr;
    int i;
    
    
    /* Allocates the thread pool */
    if((threadPool = (LoDNThreadPool*)malloc(sizeof(LoDNThreadPool))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");   
    }
    
    /* Allocates the threads */
    if((threadPool->threads = (LoDNThread*)calloc(numThreads,sizeof(LoDNThread))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }
    
    /* Initializes the internal variables */
    threadPool->numThreads         = numThreads;
    threadPool->numInActiveThreads = numThreads;
    
    /* Allocates a list to hold the in active threads */
    if((threadPool->inActiveThreads = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }
    
    /* Initialize the thread controls for the thread pool */
    if(pthread_mutex_init(&threadPool->lock, NULL)  != 0 ||
       pthread_cond_init(&threadPool->signal, NULL) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "error initializing thread controls");
    }

    /* Intializes the thread attributes */
    if(pthread_attr_init(&pthreadAttr) != 0 ||
       pthread_attr_setscope(&pthreadAttr, PTHREAD_SCOPE_SYSTEM) != 0 ||
       pthread_attr_setstacksize(&pthreadAttr, 1024*64) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "error initializing threads attributes");
    }
   
    /* Creates the actual threads */
    for(i=0; i<numThreads; i++)
    {
        /* Gets the current thread */
        thread = threadPool->threads+i;
        
        /* Initializes the variables that represent the thread */
        thread->job        = NULL;
        thread->threadPool = threadPool;
        
        /* Initiailzes the thread control primitives */
        if(pthread_mutex_init(&thread->lock, NULL)  != 0 ||
           pthread_cond_init(&thread->signal, NULL) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "error initializing thread controls");
        }
        
        /* Creates the thread and passes in the thread struct that represents it */
        if(pthread_create(&thread->tid, &pthreadAttr, threadRoutine, thread) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "error initializing threads attributes"); 
        }
        
        /* Adds it to the list of inactive threads */
        dll_append(threadPool->inActiveThreads, new_jval_v(thread));
    }
    
    
    /* Returns the threadPool */
    return threadPool;
}


void LoDNThreadPoolWaitOnFreeThread(LoDNThreadPool *threadPool)
{
               
    if(pthread_mutex_lock(&threadPool->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
    }
       
    while(threadPool->numInActiveThreads < 1)
    {  
        if(pthread_cond_wait(&threadPool->signal, &threadPool->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error"); 
        }
    }
        
    if(pthread_mutex_unlock(&threadPool->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlocking error"); 
    }
}



void LoDNThreadPoolAssignJob(LoDNThreadPool *threadPool, void (*job)(void *), void *data)
{
    /* Declarations */
    LoDNThread *thread;
    Dllist dlnode;
    
    
    if(pthread_mutex_lock(&threadPool->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
    }
       
    while(threadPool->numInActiveThreads < 1)
    {  
        if(pthread_cond_wait(&threadPool->signal, &threadPool->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error"); 
        }
    }
    
    dlnode = dll_first(threadPool->inActiveThreads);
    thread = (LoDNThread*)dlnode->val.v;
    dll_delete_node(dlnode);
        
    thread->job  = job;
    thread->data = data;
    
    
    threadPool->numInActiveThreads--;
    
    if(pthread_mutex_unlock(&threadPool->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlocking error"); 
    }
    
    /* Locks the mutex for the thread */
    if(pthread_mutex_lock(&thread->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
    }

    if(pthread_cond_signal(&thread->signal) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error"); 
    }
 
    if(pthread_mutex_unlock(&thread->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
    }
}



void LoDNThreadsPoolFinished(LoDNThreadPool *threadPool)
{
    /* Declarations */
    
        
    if(pthread_mutex_lock(&threadPool->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error"); 
    }
       
    while(threadPool->numInActiveThreads <  threadPool->numThreads)
    {  
        if(pthread_cond_wait(&threadPool->signal, &threadPool->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error"); 
        }
    }
        
    if(pthread_mutex_unlock(&threadPool->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlocking error"); 
    }
}



void freeLoDNThreadPool(LoDNThreadPool *threadPool)
{
    
    free(threadPool);
}
