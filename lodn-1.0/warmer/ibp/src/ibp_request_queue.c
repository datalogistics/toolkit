#include <assert.h>  /* assert */
#include <stdio.h>   /* fprintf() */
#include <string.h>  /* strtok_r */
#include <stdlib.h>  /* strtol */
#include <limits.h>  /* strtol on mac */
#include <time.h>    /* struct timespec */
#include <pthread.h> /* thread functions */
#include <errno.h>   /* errno() */
#include "ibp_request_queue.h"
#include "ibp_protocol.h" /* IBP definitions and error code */
#include "ibp_thread_pool.h"



/*
 ** request queue functions  **
 */

#define IBP_MSG_PRIORITY_LEVEL 2
extern IBP_THREAD_POOL *glbThreadPool;
extern void *thread_main(void *);
extern int  glbDepotShutdown;

IBP_REQUESTS_QUEUE*
init_req_queue( int max, int levels )
{
    IBP_REQUESTS_QUEUE * queue = NULL;
    int i = 0;

    assert(levels > 0);

    queue = (IBP_REQUESTS_QUEUE *)ibp_calloc(1,sizeof(IBP_REQUESTS_QUEUE));

    queue->requestsNum = 0;
    queue->maxRequests = ( max <= 0 ? 0 : max );
    queue->levels = levels;
    queue->lock = (pthread_mutex_t*)ibp_calloc(1,sizeof(pthread_mutex_t));
    pthread_mutex_init(queue->lock,NULL);
    queue->cond = (pthread_cond_t*)ibp_calloc(1,sizeof(pthread_cond_t));
    pthread_cond_init(queue->cond,NULL);

    /* create sub request queue */
    queue->subQueues = (IBP_SUB_REQ_QUEUE**)ibp_calloc(levels,
                                                   sizeof(IBP_SUB_REQ_QUEUE*));
    for (i=0; i < levels; i++){
        queue->subQueues[i] = init_sub_req_queue(); 
    }

    return(queue);
}

void 
delete_req_queue( IBP_REQUESTS_QUEUE * queue)
{
    int i;

    if ( queue == NULL ) { return; }
    for ( i=0;i<queue->levels;i++){
        delete_sub_req_queue(queue->subQueues[i]);
    }
    free(queue->lock);
    free(queue->cond);
    free(queue);

    return;
}

int
get_requests_num(IBP_REQUESTS_QUEUE * queue)
{
    int num;
    
    assert(queue!=NULL);
    pthread_mutex_lock(queue->lock);
    num = queue->requestsNum;
    pthread_mutex_unlock(queue->lock);

    return(num);
}

IBP_REQUEST *
get_status_request(IBP_REQUESTS_QUEUE * queue)
{
    IBP_REQUEST *req=NULL;
    int ret;
    IBP_SUB_REQ_QUEUE *subQueue;

    if ( glbDepotShutdown == 1){
        return NULL;
    }


    assert(queue != NULL);
    pthread_mutex_lock(queue->lock);
AGAIN:
    req=NULL;
    subQueue=queue->subQueues[0];
    if ( get_sub_req_num(subQueue) > 0 ){
        req = get_req_from_subqueue(subQueue); 
        assert(req != NULL);
    }

    if ( req != NULL ){
        queue->requestsNum --;
        assert(queue->requestsNum >= 0);
        if ( queue->requestsNum > 0){
            pthread_cond_signal(queue->cond);
        }
    }else{
        if ( queue->requestsNum > 0 ){
            /* there are some requests in the queue 
             * other than status and manage call 
             */
            pthread_cond_broadcast(queue->cond);
        }
        /* wait for request */
        ret = pthread_cond_wait(queue->cond,queue->lock);
        if ( ret == 0 ){
            if ( glbDepotShutdown == 0){
                goto AGAIN; /* waked up by main thread */
            }else{
                req = NULL;
            }
        }else{
            assert(1==0);
        }
        /*pthread_cond_wait(queue->cond,queue->lock);*/
        /* wake up and recheck */
    }
    pthread_mutex_unlock(queue->lock);

    return (req);
}

IBP_REQUEST *
get_request(IBP_REQUESTS_QUEUE * queue)
{
    IBP_REQUEST *req=NULL;
    int i,ret;
    IBP_SUB_REQ_QUEUE *subQueue;
    struct timespec   to;

    if ( glbDepotShutdown == 1){
        return NULL;
    }

    to.tv_sec = time(0) + IBP_THD_MAX_IDLE_TIME; /* maximum waiting time for thread */
    to.tv_nsec = 0;

    assert(queue != NULL);
    pthread_mutex_lock(queue->lock);
AGAIN:
    req=NULL;
    for ( i=0;(i<queue->levels)&&(req==NULL)&&(queue->requestsNum>0);i++){
        subQueue=queue->subQueues[i];
        if ( get_sub_req_num(subQueue) > 0 ){
            req = get_req_from_subqueue(subQueue); 
            assert(req != NULL);
        }
    }

    if ( req != NULL ){
        queue->requestsNum --;
        assert(queue->requestsNum >= 0);
        if ( queue->requestsNum > 0){
            pthread_cond_signal(queue->cond);
        }
    }else{
        /* wait for request */
        ret = pthread_cond_timedwait(queue->cond,queue->lock,&to);
        if ( ret == 0 ){
            if ( glbDepotShutdown == 0){
                goto AGAIN; /* waked up by main thread */
            }else{
                req = NULL;
            }
        }else{
            assert(ret != EINVAL);
            if ( ret == EINTR && glbDepotShutdown == 0 ){
                goto AGAIN;
            }
            req = NULL;
        }
        /*pthread_cond_wait(queue->cond,queue->lock);*/
        /* wake up and recheck */
    }
    pthread_mutex_unlock(queue->lock);

    return (req);
}

int
append_request(IBP_REQUESTS_QUEUE * queue, IBP_REQUEST * request)
{
    IBP_SUB_REQ_QUEUE * subQueue=NULL; 
    int                 priority;
    int                 num;
    
    /* exclusive access */
    pthread_mutex_lock(queue->lock);

    if ( queue->maxRequests > 0 && queue->requestsNum >= queue->maxRequests ){
        /* queue is full */
        pthread_mutex_unlock(queue->lock);
        return ( IBP_E_QUEUE_FULL);
    }
   
    priority = get_request_priority(request);

    assert( priority >= 0 && priority < queue->levels);
    subQueue = queue->subQueues[priority];
    append_req_to_subqueue(subQueue,request);
    queue->requestsNum++;
    num = queue->requestsNum;
    pthread_mutex_unlock(queue->lock);
    pthread_cond_signal(queue->cond);
    
    if ( num > get_thread_num(glbThreadPool) ){
        expand_thread_pool(glbThreadPool,thread_main,5);
    }
    
    return (IBP_OK);
}


/*
 ** sub request queue functions  **
 */
IBP_SUB_REQ_QUEUE *
init_sub_req_queue()
{
    IBP_SUB_REQ_QUEUE * queue=NULL;

    queue=(IBP_SUB_REQ_QUEUE*)ibp_calloc(1,sizeof(IBP_SUB_REQ_QUEUE));
    queue->head=NULL;
    queue->tail=NULL;
    queue->requestsNum=0;

    return(queue);
}

void
delete_sub_req_queue(IBP_SUB_REQ_QUEUE * queue)
{
    if ( queue != NULL ) { 
        free(queue);
    }
    return;
}

IBP_REQUEST *
get_req_from_subqueue(IBP_SUB_REQ_QUEUE * queue )
{
    IBP_REQUEST * req=NULL;
    
    assert(queue !=NULL);
    req = queue->head;
    if ( queue->head != NULL ){
        if ( queue->head == queue->tail ) {
            /* last one */
            assert(queue->requestsNum == 1);
            queue->head = NULL;
            queue->tail = NULL;
        }else{
            queue->head = queue->head->next;
        }
        queue->requestsNum --;
        req->next=NULL;
    }

    return (req);
}

int 
append_req_to_subqueue(IBP_SUB_REQ_QUEUE * queue, IBP_REQUEST *request)
{
    assert(queue != NULL && request != NULL );

    if ( queue->tail == NULL && queue->head == NULL ){
        /* empty queue */
        assert(queue->requestsNum == 0);
        queue->head = request;
        queue->tail = request;
    }else{
        /* move tail pointer */
        queue->tail->next = request;
        queue->tail = request;
    }
    request->next= NULL;

    queue->requestsNum++;

    return (IBP_OK);
}


int
get_message_priority(int msgID )
{
    static int * list=NULL;
    if ( list== NULL){
        list=(int*)ibp_calloc(IBP_MAX_NUM_CMDS,sizeof(int));
        list[IBP_STATUS] = 0;
        list[IBP_MANAGE] = 0;
        list[IBP_ALLOCATE] = 1;
        list[IBP_STORE] = 1;
        list[IBP_DELIVER] = 1;
        list[IBP_SEND] = 1;
        list[IBP_MCOPY] = 1;
        list[IBP_REMOTE_STORE] = 1;
        list[IBP_LOAD] = 1;
        list[IBP_SEND_BU] = 1;
        list[IBP_CT_COPY] = 1;
        list[IBP_NFU] = 1;
        list[IBP_WRITE]=1;
    };

    if ( msgID <= 0 || msgID > IBP_MAX_NUM_CMDS ) {
        return -1;
    }else{
        return (list[msgID]);
    }
}

int
get_message_priority_levels ()
{
    return IBP_MSG_PRIORITY_LEVEL;
}


void *
ibp_calloc(size_t count, size_t size)
{
    void * ptr;

    ptr = calloc(count,size);
    
    if ( ptr == NULL ) { abort(); }

    return (ptr);
}

#ifdef IBP_UNIT_TEST

#include <stdio.h>

int
read_line_from_conn(IBP_CONNECTION *  conn, 
                    int               maxSize, 
                    char *            buf, 
                    int *             readSize,
                    int               timeout)
{
    return IBP_CONN_SUCC;
}

IBP_CONNECTION *
init_connection ( int fd  )
{
    IBP_CONNECTION * conn;

    conn = (IBP_CONNECTION*)ibp_calloc(1,sizeof(IBP_CONNECTION));
    conn->fd = fd;

    return (conn);
}

IBP_REQUEST_POOL *glbReqPool;

int main()
{
    int i; 
    int pri;
    IBP_REQUESTS_QUEUE * queue;
    IBP_REQUEST_POOL * pool;
    IBP_REQUEST *request;
    IBP_CONNECTION *conn;

    queue = init_req_queue(1024,400);
    glbReqPool = init_request_pool();
    for (i=0; i< 1024;i++){
        conn = init_connection(i);
        request = create_request(conn);
        set_request_priority(request,random()%400);
        append_request(queue,request);
    }

    for(i=0; i< 1024; i++){
        request=get_request(queue);
        free_request(pool,request);
    }


    fprintf(stderr,"Total allocated  = %d idle request = %d\n",
                    glbReqPool->allocated,glbReqPool->num);
    delete_req_queue(queue);
    delete_request_pool(glbRequestPool);

    return 0;
}

#endif
