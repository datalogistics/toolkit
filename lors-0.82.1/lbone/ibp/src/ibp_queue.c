/*=============================================================================
 *   FILE NAME:
 *   queue.c
 *
 *   GENERAL DESCRIPTION: 
     This file implements the basic queue operations for IBP requests.

 *   REVISION HISTORY:
 *               	
 *   Version  Date        Author         Brief Description of Changes
 *-------  ----------  -------------  -----------------------------------------
 *   1.0.0    2003-01-20  Huadong Liu    Initial Creation.
 *
 *===========================================================================*/

/*=============================================================================
                                 INCLUDE FILES
=============================================================================*/
#include "ibp_queue.h"

/* ============================================================================
 *                               FUNCTIONS
 * ==========================================================================*/
RequestQueue *create_queue()
{
    RequestQueue *queue;

    queue = (RequestQueue *) malloc(sizeof(*queue));
    queue->queue = make_dl();
    queue->lock = (pthread_mutex_t *) malloc(sizeof(*(queue->lock)));
    pthread_mutex_init(queue->lock, NULL);
    queue->cv = (pthread_cond_t *) malloc(sizeof(*(queue->cv)));
    queue->resume = (pthread_cond_t *) malloc(sizeof(*(queue->resume)));
    pthread_cond_init(queue->cv, NULL);
    pthread_cond_init(queue->resume, NULL);
    queue->req_num = 0;
    queue->idleThds = 0;
    queue->sleep = 0;

    return (queue);
}

void destroy_queue(RequestQueue * queue)
{
    if (queue) {
        dl_delete_list(queue->queue);
        pthread_mutex_destroy(queue->lock);
        pthread_cond_destroy(queue->cv);
    }
    free(queue);
}

void insert_request(RequestQueue * queue, int sock)
{
    dl_insert_b(queue->queue, (void *) sock);
    queue->req_num++;
}

int remove_request(RequestQueue * queue)
{
    int val;
    Dlist first;

    first = queue->queue->flink;
    val = (int) (dl_val(first));
    dl_delete_node(first);
    queue->req_num--;

    return val;
}
