#include <assert.h>  /* assert */
#include <stdio.h>   /* fprintf() */
#include <string.h>  /* strtok_r strerror() */
#include <stdlib.h>  /* strtol */
#include <limits.h>  /* strtol on mac */
#include <sys/types.h>  /* socket data type definitin */
#include <sys/socket.h> /* socket related functions */
#include <time.h>       /* ctime_r() time() */
#include <errno.h>      /* errno definition */
#include <strings.h>    /* bzero() */

#include "ibp_request.h"
#include "ibp_request_queue.h"
#include "ibp_connection.h"
#include "ibp_protocol.h" /* IBP definitions and error code */
#include "ibp_thread_pool.h"
#include "ibp_server.h"


extern IBP_REQUEST_POOL *glbReqPool; /* global variable for request pool */
extern int              glbDepotShutdown; /* if depot is being shut down */
extern IBP_THREAD_POOL  *glbThreadPool;

extern void ibp_crash_save(int pi_mode);

/*
 ** request queue functions  **
 */

#define IBP_MAX_MESSAGE_SIZE 512

/*
 ** request pool functions **
 */
IBP_REQUEST_POOL *
init_request_pool ( )
{
    IBP_REQUEST_POOL * pool=NULL;
    
    pool=(IBP_REQUEST_POOL*)ibp_calloc(1,sizeof(IBP_REQUEST_POOL));
    pool->allocated = 0;
    pool->num = 0;
    pool->reqBuffer = NULL;

    return(pool);
}

static IBP_REQUEST *
alloc_request ( void )
{
    IBP_REQUEST * request=NULL;

    request = (IBP_REQUEST *)ibp_calloc(1,sizeof(IBP_REQUEST));
    request->conn = NULL;
    request->version = 0;
    request->msgID = -1;
    request->bufSize = IBP_MAX_MESSAGE_SIZE;
    request->buffer = (char*)ibp_calloc(request->bufSize,sizeof(char));
    request->arguments = request->buffer;
    request->status=REQ_ST_FREED;
    request->priority = -1;
    request->errorCode = -1;
    request->handler = NULL;
    request->next = NULL;

    return (request);
}

static IBP_REQUEST *
alloc_request_from_pool(IBP_REQUEST_POOL *pool)
{
    IBP_REQUEST *req = NULL;

    assert(pool != NULL);
    req = pool->reqBuffer;
    if (req == NULL){
        /* no availabe request struct */
        req = alloc_request();
        pool->allocated ++;
    }else{
        pool->reqBuffer = req->next;
        pool->num --;
    }

    return (req);
}

static void
reclaim_request_to_pool(IBP_REQUEST_POOL *pool, IBP_REQUEST *request)
{
    assert(request != NULL && pool != NULL);
    request->next = pool->reqBuffer;
    pool->reqBuffer = request;
    request->conn = NULL;
    request->version = 0;
    request->msgID = -1;
    request->bufSize = IBP_MAX_MESSAGE_SIZE;
    request->arguments = request->buffer;
    request->status = REQ_ST_FREED;
    request->priority = -1;
    request->handler = NULL;
    pool->num++;

    return;
}


IBP_REQUEST *
create_request( IBP_CONNECTION * conn, IBP_REQUEST_TYPE type)
{
    IBP_REQUEST * request=NULL;
    
    assert( conn != NULL);

    request = alloc_request_from_pool(glbReqPool);

    assert(get_request_status(request) == REQ_ST_FREED );   

    init_request(request,conn,type); 
    return (request);
}

void
free_request(IBP_REQUEST * request)
{
    assert(request!=NULL);
    
    set_request_status(request,REQ_ST_FREED);
    reclaim_request_to_pool(glbReqPool,request);

    return;
}

static void
delete_request( IBP_REQUEST * request )
{
    if ( request != NULL ){
        free(request->buffer);
        free(request);
    }
    return;
}

void
delete_request_pool( IBP_REQUEST_POOL * pool )
{
    IBP_REQUEST *ptr, *next;

    if ( pool != NULL ){
        ptr = pool->reqBuffer;
        while ( ptr != NULL ){
            next = ptr->next;
            delete_request(ptr);
            ptr = next;
        }
        free(pool);
    }

    return;
}

void 
init_request(IBP_REQUEST        *request, 
             IBP_CONNECTION     *conn, 
             IBP_REQUEST_TYPE   type)
{
    assert(request != NULL && conn != NULL );

    request->conn = conn;
    request->arguments = request->buffer;
    request->priority = 0;
    request->status = REQ_ST_CONN_EST;
    request->msgID = -1;
    request->version = 0;
    request->next = NULL;

    switch (type){
    case REQ_TYPE_ACCEPT:
        request->handler = handle_new_connection;
        break;
    case REQ_TYPE_PIPE:
        request->handler = handle_finish_request;
        break;
    case REQ_TYPE_IBP:
        request->handler = handle_ibp_request;
        break;
    default:
        assert(0);
    }

    return;
}

void 
read_request_line(IBP_REQUEST * request )
{
    int ret;
    int size;
    char *ptr;

    assert( request !=NULL && 
            request->conn != NULL &&
            (request->status == REQ_ST_WAITING_MORE_DATA ||
             request->status == REQ_ST_CONN_EST));

    size =  (request->buffer - request->arguments) + request->bufSize;
    ptr = request->arguments; 
    ret = read_line_from_conn(request->conn,ptr,size,-1);

    if ( ret > 0 ) { /* get some data */
        ptr += ret - 1;
        if ( *ptr == IBP_NEW_LINE){
            /* receive completed request message */
            *ptr = '\0';
            request->status = REQ_ST_MSG_RECEIVED;
            parse_request_message(request);
        }else{
            /* need more data */
            size = ptr - request->buffer + 1;
            if ( size >= request->bufSize ){
                /* buffer overflow */
                request->status = REQ_ST_MSG_ERROR;
                request->errorCode = IBP_E_BAD_FORMAT;
            }
        }
    }else if ( ret < 0 ){ 
        request->status = REQ_ST_LINK_ERROR;
        request->errorCode = ret;
    };

    return;
}

void 
parse_request_message(IBP_REQUEST * request )
{
    char    *ptr, *brkt,*endptr;
    char    sep[]=" "; 
    int     tmp; 

    assert( request!=NULL && request->status == REQ_ST_MSG_RECEIVED);
   
    request->version = -1;
    request->msgID = -1;

    /* parse version */
    ptr = strtok_r(request->buffer,sep,&brkt);
    if ( ptr != NULL ){
        tmp = (int)strtol(ptr,&endptr,10);
        if ( *endptr == '\0'){
            request->version = tmp;
            /* parse message ID */
            ptr = strtok_r(NULL,sep,&brkt);
            if ( ptr != NULL ){
                tmp=(int)strtol(ptr,&endptr,10);
                if ( *endptr == '\0'){
                    request->msgID = tmp;
                }
            }
        }
    }
   
    /* check validation */
    if ( request->version != IBPv031 && request->version != IBPv040 ){
        request->status = REQ_ST_MSG_ERROR;
        request->errorCode = IBP_E_PROT_VERS ;
        return;
    }

    if ( request->msgID <= 0 || request->msgID >= IBP_MAX_NUM_CMDS ){
        request->status = REQ_ST_MSG_ERROR;
        request->errorCode = IBP_E_INVALID_CMD;
        return;
    }

    /* set message priority */
    request->priority = get_message_priority(request->msgID);
    
    /* set message arguments pointer */
    request->arguments = brkt; 
    
    /* set request status, request will be put in waiting queue */
    request->status = REQ_ST_MSG_PARSED;
    
    return;
}

void ibp_err_log(char *msg, int error)
{
    time_t  now;
    char    timeString[25];

    now = time(0);
    ctime_r(&now,timeString);
    timeString[24]='\0';

    fprintf(stderr,"%s: %s: %s\n",timeString,msg,strerror(error));
    fflush(stderr);
    return;
}


static void clean_up()
{
    int i = 0;
    assert(glbDepotShutdown == 1);
    fprintf(stderr,"IBP Depot shut down: waiting current jobs to finish ....");
    while ( get_thread_num(glbThreadPool) > 0){
        sleep(1);
        i++;
        if ( i > IBP_MAX_SHUTDOWN_WAIT_TIME ){
            break;
        }
    }
    ibp_crash_save(2);
    fprintf(stderr,"done\n");
}


void 
handle_new_connection (  struct ibp_request *request,
                        struct ibp_conn_list *list,
                        struct ibp_requests_queue *queue)
{
    IBP_CONNECTION  *conn = NULL,*newConn = NULL;
    int             newfd, acceptfd,ret;    
    IBP_REQUEST     *newReq=NULL;
    struct sockaddr cliaddr[2];
    int             cliSockLen ;
    char            errMsg[10];


    conn = get_request_connection(request); 

    /* check if depot is being shut down */
    if ( glbDepotShutdown ){
        close_connection(conn);
        delete_conn_from_list(list,conn);
        free_connection(conn);
        pthread_cond_broadcast(queue->cond);
        clean_up();
        exit(0);
    }

    /* check connection status */
    if ( conn_st_isset(conn,IBP_CONN_RD_READY) ){
        /* new request is coming */
        acceptfd = get_conn_fd(conn);
        while (1){
            cliSockLen = sizeof(struct sockaddr);
            newfd = accept(acceptfd,cliaddr,&cliSockLen);
            if ( newfd == -1 ){
                if ( errno != EWOULDBLOCK ){
                    ibp_err_log("Unable to accept new connection:",errno);
                }
                break;
            }

            /* new connection is accepted, create connection and 
             * request structure for comming connection */
            newConn = create_connection(newfd);
            newReq = create_request(newConn,REQ_TYPE_IBP);
            set_conn_request(newConn,newReq);

            /* set new connection read ready */
            clear_conn_status(newConn);
            set_conn_status(newConn,IBP_CONN_RD_READY|IBP_CONN_RD_WAIT);
            set_conn_stime(newConn,time(0));            

            /* set new connection nonblocked */
            set_nonblock_socket(newfd);

            /* put in connect list */
            ret = insert_conn_to_list(list,newConn);
            if ( ret != 0 ){
                /* too many open connections, send error back */
                sprintf(errMsg,"%d \n",IBP_E_QUEUE_FULL);
                write(newfd,errMsg,strlen(errMsg));
                close_connection(newConn);
                free_connection(newConn);
                free_request(newReq);
            }
            /* get next new connection */
        }
    }

    /* register interested event for the connection */
    clear_conn_status(conn);
    set_conn_status(conn,IBP_CONN_RD_WAIT);

    return ;
}

void 
handle_finish_request(  struct ibp_request       *request,
                        struct ibp_conn_list     *list, 
                        struct ibp_requests_queue *queue)
{
    IBP_CONNECTION  *conn;
    IBP_REQUEST     *req;   /* finished request */
    IBP_CONNECTION  *newConn;
    int             fd;     /* pipeline fd */
    int             ret;

    conn = get_request_connection(request);
    fd = get_conn_fd(conn);

    /* check connection status */
    if ( conn_st_isset(conn,IBP_CONN_RD_READY) ){
        /* ibp request is done */
        while (1){
            ret = read(fd,&req,sizeof(IBP_REQUEST *));
            if ( ret == 0 ){
                /* closed pipeline */
                fprintf(stderr,"Panic: internal pipeline is broken, exit!\n");
                exit(-1);
            }else if ( ret < 0 ){ /* error occurs */
                if ( errno == EINTR ){
                    continue; /* interrupted ,try again */ 
                }else if ( errno == EAGAIN){
                    break; /* no more data */
                }else {
                    /* error occurs , write error message */
                    fprintf(stderr,"Panic: internal pipeline is broken, exit!\n");
                    perror("reading");
                    exit(-1);
                }
            }else{ /* get data successfully */
                assert(ret == sizeof(IBP_REQUEST*));
                assert(req != NULL );
                if ( get_request_status(req) != REQ_ST_SUCCESS){
                    /* close connection and free request structure */
                    newConn = get_request_connection(req);
                    close_connection(newConn);
                    free_connection(newConn);
                    free_request(req);
                }else{
                    /* reuse this connect, put in connection list */
                    newConn = get_request_connection(req);
                    init_request(req,newConn,REQ_TYPE_IBP);
                    clear_conn_status(newConn);
                    set_conn_request(newConn,req);
                    set_conn_stime(newConn,time(0));
                    set_conn_status(newConn,IBP_CONN_RD_WAIT);
                    insert_conn_to_list(list,newConn);
                }
            }
            /* read next finish request */
        }
    }

    /* register interesting event */
    clear_conn_status(conn);
    set_conn_status(conn,IBP_CONN_RD_WAIT);

    return ;
}

static void 
send_err_msg_back(IBP_CONNECTION *conn, int error)
{
    int     fd = -1;
    char    msg[32];

    assert(conn != NULL);
    fd = get_conn_fd(conn);
    bzero(msg,32);
    sprintf(msg,"%d \n",error);
    write(fd,msg,strlen(msg));
    return ;
}

void 
handle_ibp_request(  struct ibp_request       *request,
                     struct ibp_conn_list     *list, 
                     struct ibp_requests_queue *queue)
{
    IBP_CONNECTION  *conn=NULL;
    int             fd = -1;
    time_t          now;
    int             ret;


    now = time(0);  /* current time */

    /* get connection handler */
    conn = get_request_connection(request);
    fd = get_conn_fd(conn);

    /* check if more data arrive on the connection */
    if ( conn_st_isset(conn,IBP_CONN_RD_READY) ){
        read_request_line(request); 
    };

    switch ( get_request_status(request)){
    case REQ_ST_MSG_PARSED:
        /* received correct request message */
        delete_conn_from_list(list,conn);
        if ( (ret = append_request(queue,request)) != IBP_OK ){
            /* queue is full */
            send_err_msg_back(conn,ret); 
            close_connection(conn);
            free_connection(conn);
            free_request(request);
        };
        break;
    case REQ_ST_WAITING_MORE_DATA:
        /* wait again */
        break;
    case REQ_ST_CONN_EST:
        /* check timeout */
        if ( 15 < ( now - get_conn_stime(conn))){
            /* timeout, close idle connection */
            delete_conn_from_list(list,conn);
            close_connection(conn);
            free_request(request);
            free_connection(conn);
        }
        break;
    case REQ_ST_LINK_ERROR:
        delete_conn_from_list(list,conn);
        close_connection(conn);
        free_connection(conn);
        free_request(request);
        break;
    case REQ_ST_MSG_ERROR:
        delete_conn_from_list(list,conn);
        send_err_msg_back(conn,get_request_err_code(request));
        close_connection(conn);
        free_connection(conn);
        free_request(request);
        break;
    default:
        assert(0==1);
    }

    return;
}
