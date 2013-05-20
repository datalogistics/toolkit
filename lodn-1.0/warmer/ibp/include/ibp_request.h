#ifndef IBP_REQUEST_H
#define IBP_REQUEST_H

#include "ibp_connection.h"
#include "ibp_request_queue.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum
{
    REQ_ST_FREED = 0,
    REQ_ST_WAITING_MORE_DATA=1,
    REQ_ST_MSG_RECEIVED=2,
    REQ_ST_MSG_PARSED=3,
    REQ_ST_MSG_ERROR=4,
    REQ_ST_LINK_ERROR=5,
    REQ_ST_CONN_EST=6,
    REQ_ST_SUCCESS=7
}
IBP_REQUEST_STATUS;

typedef enum
{
    REQ_TYPE_ACCEPT = 0,
    REQ_TYPE_PIPE = 1,
    REQ_TYPE_IBP = 2
}
IBP_REQUEST_TYPE;

struct ibp_requests_queue;

typedef void (*IBP_REQ_HANDLER)(struct ibp_request * , struct ibp_conn_list *, struct ibp_requests_queue  *);

// typedef void (*IBP_REQ_HANDLER)(void *, void * , void*);

typedef struct ibp_request
{
    struct ibp_connection * conn;       /* connection handler */
    int                 version;        /* protocol version number */
    int                 msgID;          /* message id */
    int                 bufSize;        /* size of data buffer */
    char *              arguments;      /* arguments of message */
    char *              buffer;
    int                 priority;       /* message priority */
    IBP_REQUEST_STATUS  status;         /* status of request */
    int                 errorCode;      /* error number */
    struct ibp_request* next;           /* pointer to next request */
    IBP_REQ_HANDLER     handler;        /* request handler */
}
IBP_REQUEST;


typedef struct ibp_request_pool
{
    int             allocated;   /* total allocated struct so far */
    int             num;        /* number of free request structures */
    IBP_REQUEST*    reqBuffer;  /* request buffer */
}
IBP_REQUEST_POOL;

extern IBP_REQUEST_POOL*
init_request_pool();

extern IBP_REQUEST *
create_request(struct ibp_connection *conn, IBP_REQUEST_TYPE type);

extern void
free_request(IBP_REQUEST* request);

extern void
read_request_line(IBP_REQUEST* request);

extern void
parse_request_message(IBP_REQUEST * request);

extern void
init_request( IBP_REQUEST               *request, 
              struct ibp_connection     *conn,
              IBP_REQUEST_TYPE          type);

extern void
handle_new_connection( struct ibp_request        *request,
                      struct ibp_conn_list      *list,
                      struct ibp_requests_queue  *queue);
extern void
handle_finish_request( struct ibp_request        *request,
                      struct ibp_conn_list      *list,
                      struct ibp_requests_queue  *queue);
extern void
handle_ibp_request(   struct ibp_request        *request,
                      struct ibp_conn_list      *list,
                      struct ibp_requests_queue  *queue);

#define set_request_connection(request,conn)  (request)->conn = (conn)
#define get_request_connection(request)     ((request)->conn)

#define set_request_status(request,st)  (request)->status=(st)
#define get_request_status(request)         ((request)->status)

#define set_request_err_code(request,err)   ((request)->errorCode)=(err)
#define get_request_err_code(request)       ((request)->errorCode)

#define set_request_priority(request,pri)   ((request)->priority)=(pri)
#define get_request_priority(request)       ((request)->priority)

#define get_request_handler(request)        ((request)->handler)



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IBP_REQUEST_H */
