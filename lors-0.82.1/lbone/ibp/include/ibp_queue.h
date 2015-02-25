/*=============================================================================
  FILE NAME:
  queue.h
  GENERAL DESCRIPTION: 
  This file defines a request queue structure and four queue operations. 
  REVISION HISTORY:
  
  Version  Date        Author         Brief Description of Changes
  -------  ----------  -------------  -----------------------------------------
  1.0.0    2003-01-20  Huadong Liu    Initial Creation.
  ===========================================================================*/
#ifndef _QUEUE_H
#define _QUEUE_H

#ifdef __cplusplus              /* Allow header file to be included in a C++ file */
extern "C"
{
#endif

/*=============================================================================
                                 INCLUDE FILES
=============================================================================*/
#include "config-ibp.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "dlist.h"
/*=============================================================================
                                 STRUCTURES
=============================================================================*/
/**
 * Request queue data structure. 
 */
    typedef struct _request_queue
    {

        Dlist queue;
        /* request list */
        int req_num;
        /* total # of requests in the list */
        int idleThds;           /* number of idle threads */
        int sleep;              /* flag of if the main thread is sleep */
        pthread_mutex_t *lock;

        pthread_cond_t *cv;
        pthread_cond_t *resume;

    }
    RequestQueue;

/* ============================================================================
 *                               FUNCTION PROTOTYPES                   
 * ==========================================================================*/
/**
 * Create queue. Creates and initializes a RequestQueue.
 */
    extern RequestQueue *create_queue();

/**
 * Destroy queue. Destroys a RequestQueue. Any remaining 
 * requests will be leaked.
 */
    extern void destroy_queue(RequestQueue * queue);

/**
 * Insert request. Inserts a request into the RequestQueue in 
 * FIFO order. Blocks until the RequestQueue is available for
 * writing.
 */
    extern void insert_request(RequestQueue * queue, int sock);

/**
 * Remove request. Removes a request from the RequestQueue in
 * FIFO order. Blocks until a request is available.
 */
    extern int remove_request(RequestQueue * queue);

#ifdef __cplusplus              /* Allow header file to be included in a C++ file */
}
#endif

#endif
