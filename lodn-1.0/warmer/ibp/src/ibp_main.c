#include <signal.h>     /* signal() */
#include <assert.h>     /* assert() */
#include <fcntl.h>      /* fcntl() */
#include "ibp_server.h"
#include "ibp_connection.h"
#include "ibp_request.h"
#include "ibp_request_queue.h"
#include "ibp_thread_pool.h"

extern void *routine_check();
extern void *handle_request( IBP_REQUEST *request, int tid); 
extern int  boot(int argc, char **argv);
extern void ibp_signal_int(int i);
extern void ibp_signal_nfu(int i); 
extern int  thrallocate();
extern void *routine_check();

/* define global variable */
IBP_CONN_POOL       *glbConnPool;
IBP_REQUEST_POOL    *glbReqPool;
int                 glbDepotShutdown;
IBP_THREAD_POOL     *glbThreadPool;

static int setup_signal_handler()
{
    sigset_t    blockMask;

    signal(SIGPIPE,SIG_IGN);
    signal(SIGINT,&ibp_signal_int);
    signal(SIGTERM,&ibp_signal_int);
    signal(SIGHUP,&ibp_signal_nfu);
    signal(SIGCHLD,SIG_IGN);

    /* set signal mask: only let main thread receive int and segv signal */
    sigemptyset(&blockMask);
    sigaddset(&blockMask,SIGINT);
    sigaddset(&blockMask,SIGTERM);
    sigaddset(&blockMask,SIGSEGV);
    pthread_sigmask(SIG_UNBLOCK,&blockMask,NULL);

    return (IBP_OK);
}

static int
create_monitor_thread() {
    pthread_attr_t  attr;
    pthread_t       tid;

    /* set detach mode */
    if ( pthread_attr_init(&attr) != 0){
        perror("can't create monitor thread:");
        return (-1);
    }

    if ( pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED) != 0 ){
        perror("can't create monitor thread:");
        return (-1);
    }

    /* spawn monitor thread */
    if ( pthread_create(&tid,&attr,&routine_check,(void*)NULL) != 0 ){
        perror("can't create monitor thread:");
        return(-1);
    }

    return (IBP_OK);
};

static IBP_CONNECTION *
create_listen_conn(int fd)
{
    IBP_CONNECTION *conn = NULL;
    IBP_REQUEST    *req = NULL;
    int             flags;

    assert(fd >= 0);

    if ( (flags = fcntl(fd,F_GETFL,0)) < 0 ){
        perror("Can't get socket status \n");
        exit(-1);
    }
                                                                                
    if ( (flags = fcntl(fd,F_SETFL,flags|O_NONBLOCK)) < 0 ){
        perror("Can't set nonblock socket \n");
        exit(-1);
    };

    conn = create_connection(fd); 
    req = create_request(conn,REQ_TYPE_ACCEPT);
    set_conn_request(conn,req);
    set_conn_status(conn,IBP_CONN_RD_WAIT);
    return(conn);
}

static IBP_CONNECTION *
create_pipe_conn(int fd)
{
    IBP_CONNECTION *conn = NULL;
    IBP_REQUEST    *req = NULL;

    assert(fd >= 0);

    conn = create_connection(fd); 
    req = create_request(conn,REQ_TYPE_PIPE);
    set_conn_request(conn,req);
    set_conn_status(conn,IBP_CONN_RD_WAIT);
    return(conn);
}

int set_block_socket(int fd)
{
    int flags;

    if ( (flags = fcntl(fd,F_GETFL,0)) < 0 ){
        perror("Can't get socket status \n");
        exit(-1);
    }
                      
    if ( (flags = fcntl(fd,F_SETFL,flags&(~O_NONBLOCK))) < 0 ){
        perror("Can't set block socket \n");
        exit(-1);
    }

    return 0;
}

int set_nonblock_socket(int fd)
{
    int flags;

    if ( (flags = fcntl(fd,F_GETFL,0)) < 0 ){
        perror("Can't get socket status \n");
        exit(-1);
    }
                      
    if ( (flags = fcntl(fd,F_SETFL,flags|O_NONBLOCK)) < 0 ){
        perror("Can't set nonblock socket \n");
        exit(-1);
    }

    return 0;
}

void 
*handle_status_thread( void *arg)
{
    IBP_THREAD_HANDLER  *tHandler;
    IBP_REQUESTS_QUEUE   *queue;
    IBP_REQUEST         *request;
    IBP_THREAD_POOL     *threadPool;
    IBP_CONNECTION      *conn;
    sigset_t            block_mask;

    tHandler = (IBP_THREAD_HANDLER*)arg;
    threadPool = tHandler->pool;
    queue = threadPool->reqQueue;
    
    /*
    * set signal mask
    */
    sigemptyset(&block_mask);
    sigaddset(&block_mask,SIGINT);
    sigaddset(&block_mask,SIGTERM);
    pthread_sigmask(SIG_BLOCK,&block_mask,NULL);

#ifdef IBP_AUTHENTICATION
    ibpCreateSrvSSLCTX(glb.cafile);
#endif
    for(;;){
        if ( glbDepotShutdown == 1 ){
            free_thread_handler(threadPool,tHandler);
            pthread_exit(0);
        }
        
        /* waiting request */
        request = get_status_request(queue);
        if ( request == NULL ) { 
            continue;
        }
        assert(request != NULL);

        conn = get_request_connection(request);

        /* set block mode */
        set_block_socket(get_conn_fd(conn));

        handle_request(request,tHandler->threadID);

        if ( IBPErrorNb[tHandler->threadID] != IBP_OK){
            set_request_status(request,REQ_ST_LINK_ERROR);   
            set_request_err_code(request,IBPErrorNb[tHandler->threadID]);
        }else{
            set_request_status(request,REQ_ST_SUCCESS);
        }
        
        /* set nonblock mode back */
        set_nonblock_socket(get_conn_fd(conn));
        
        if (write(glbWrtPipe,&request,sizeof(IBP_REQUEST *)) <= 0){ 
            perror("Panic: pipe writing failed!\n");
            exit(-1);
        }
    }
}

void 
*thread_main(void *arg)
{
    IBP_THREAD_HANDLER  *tHandler;
    IBP_REQUESTS_QUEUE   *queue;
    IBP_REQUEST         *request;
    IBP_THREAD_POOL     *threadPool;
    IBP_CONNECTION      *conn;
    sigset_t            block_mask;

    tHandler = (IBP_THREAD_HANDLER*)arg;
    threadPool = tHandler->pool;
    queue = threadPool->reqQueue;
  
    /*
    * set signal mask
    */
    sigemptyset(&block_mask);
    sigaddset(&block_mask,SIGINT);
    sigaddset(&block_mask,SIGTERM);
    pthread_sigmask(SIG_BLOCK,&block_mask,NULL);

#ifdef IBP_AUTHENTICATION
    ibpCreateSrvSSLCTX(glb.cafile);
#endif
    for(;;){
        if ( glbDepotShutdown == 1 ){
            free_thread_handler(threadPool,tHandler);
            pthread_exit(0);
        }
        /*nums = get_requests_num(queue);*/
        
        /* waiting request */
        inc_idle_thread(threadPool);
        request = get_request(queue);
        dec_idle_thread(threadPool);
        if ( request == NULL ) { 
            /* waiting time out */
            if ( get_idle_thread_num(threadPool) > IBP_TPOOL_MIN_IDLE_THREAD){
                free_thread_handler(threadPool,tHandler);
                pthread_exit(0);
            }else{
                continue;
            }
        }
        assert(request != NULL);

        conn = get_request_connection(request);

        /* set block mode */
        set_block_socket(get_conn_fd(conn));

        handle_request(request,tHandler->threadID);

        if ( IBPErrorNb[tHandler->threadID] != IBP_OK){
            set_request_status(request,REQ_ST_LINK_ERROR);                
            set_request_err_code(request,IBPErrorNb[tHandler->threadID]);
        }else{
            set_request_status(request,REQ_ST_SUCCESS);
        }
        
        /* set nonblock mode back */
        set_nonblock_socket(get_conn_fd(conn));

        if (write(glbWrtPipe,&request,sizeof(IBP_REQUEST *)) <= 0){ 
            perror("Panic: pipe writing failed!\n");
            exit(-1);
        }
    }
}

int  main ( int argc , char **argv )
{
    IBP_CONN_LIST       *cList = NULL;
    IBP_REQUESTS_QUEUE  *rQueue = NULL;
    IBP_CL_ITERATOR     *itr = NULL;
    IBP_CONNECTION      *conn = NULL;
    IBP_REQUEST         *request = NULL;
    IBP_CONNECTION      *lsnConn = NULL;
    IBP_CONNECTION      *pipeConn = NULL;
    struct timeval      tm;

    /* parse command line options and initialize depot */
    if ( IBP_OK != boot(argc,argv) ){
        exit(-1);
    }

    /* set up signal handler */
    if ( IBP_OK != setup_signal_handler() ){
        exit(-1);
    }

    /* spawn monitor thread */
    if ( IBP_OK != create_monitor_thread() ){
        exit(-1);
    }

    thrallocate();

    /* create global data structure pool */
    glbConnPool = init_conn_pool();
    glbReqPool  = init_request_pool(); 
    assert(glbConnPool != NULL && glbReqPool != NULL );

    /* initialize connection list and request queue */
    cList = init_conn_list();
    rQueue = init_req_queue(glb.queue_len,2);
    assert(cList != NULL && rQueue != NULL);

    /* insert listen socket and pipeline to connection list */
    lsnConn = create_listen_conn(glb.IbpSocket);
    pipeConn = create_pipe_conn(glbRdPipe); 
    insert_conn_to_list(cList,lsnConn);
    insert_conn_to_list(cList,pipeConn);
    
    /* create handler_thread pool */
    glbThreadPool = create_thread_pool(glb.IbpThreadMax,rQueue);
    assert(glbThreadPool != NULL);
    expand_thread_pool(glbThreadPool,thread_main,glb.IbpThreadNum);
    expand_thread_pool(glbThreadPool,handle_status_thread,1);

    /* infinit loop to accept request and handle request */
    while (1){
        tm.tv_sec = glb.idle_time;
        tm.tv_usec = 0;
        wait_ready_conn(cList,&tm);        
        itr = get_conn_list_iterator(cList);
        while ( (conn = get_next_connection(itr)) != NULL ){
            request = get_conn_request(conn);
            assert(request != NULL);
            request->handler(request,cList,rQueue);
        }

        /* need more threads ? */
        if ( get_idle_thread_num(glbThreadPool) < IBP_TPOOL_MIN_IDLE_THREAD){
            expand_thread_pool(glbThreadPool,thread_main,5);
        }
    }
    
    return (0);
}
