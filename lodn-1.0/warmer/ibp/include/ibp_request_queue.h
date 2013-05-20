#ifndef IBP_REQUEST_QUEUE_H
#define IBP_REQUEST_QUEUE_H

#include <stdlib.h>
#include <pthread.h>
#include "ibp_request.h"
#include "ibp_connection.h"



#ifdef __cplusplus
extern "C" {
#endif

/* ibp version of calloc: 
 * Always return good pointer, will abort if run out of memory */
extern void *
ibp_calloc( size_t count, size_t size);


/* sub request queue definition */ 
typedef struct ibp_sub_req_queue
{
    struct ibp_request   *head;       /* the first request */
    struct ibp_request   *tail;       /* the last request */ 
    int             requestsNum; /* number of requests */
}
IBP_SUB_REQ_QUEUE;

/* create sub requests queue */
extern IBP_SUB_REQ_QUEUE*
init_sub_req_queue();

#define get_sub_req_num(subque) ((subque)->requestsNum) 

/* append request to sub requests queue */
extern int
append_req_to_subqueue(IBP_SUB_REQ_QUEUE* queue, struct ibp_request * request);

/* get a request */
extern struct ibp_request *
get_req_from_subqueue(IBP_SUB_REQ_QUEUE* queue);

/* free sub requests queue*/
extern void
delete_sub_req_queue(IBP_SUB_REQ_QUEUE* queue);

/* IBP request queue definition */
typedef struct ibp_requests_queue
{
    int                 requestsNum;    /* number of requests in the queue */
    int                 maxRequests;    /* maximum number of requests */
    int                 levels;         /* number of priority levels */
    IBP_SUB_REQ_QUEUE** subQueues;      /* list of sub request queues */
    pthread_mutex_t*    lock;           /* lock to access queue */
    pthread_cond_t*     cond;           /* condition variable of request queue*/
}
IBP_REQUESTS_QUEUE;

/* create a request queue */
extern IBP_REQUESTS_QUEUE*
init_req_queue( int max, int levels ); 

/* get the number of requests int the queue */
extern int
get_requests_num(IBP_REQUESTS_QUEUE * queue);


/* append a request to the queue */
extern int
append_request(IBP_REQUESTS_QUEUE* queue, struct ibp_request * request  );

/* get a request from the queue */
extern struct ibp_request *
get_request(IBP_REQUESTS_QUEUE* queue);

extern struct ibp_request *
get_status_request(IBP_REQUESTS_QUEUE *queue);

/* free the queue */
extern void 
delete_req_queue (IBP_REQUESTS_QUEUE* queue);

extern int 
get_message_priority( int msgID) ;

extern int 
get_message_priority_level();


#ifdef __cplusplus
}
#endif 

#endif /* IBP_REQUEST_QUEUE_H */
