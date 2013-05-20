#include <assert.h>         /* assert() */
#include <sys/types.h>      /* select() */
#include <sys/select.h>     /* select() */
#include <sys/time.h>       /* select() ,struct timeval*/
#include <sys/socket.h>     /* recv() */
#include <unistd.h>         /* select() */ 
#include <stdlib.h>         /* abort() */
#include <errno.h>          /* errno */
#include <strings.h>         /* bzero() */ 
#include <sys/resource.h>   /* getrlimit() */
#include "ibp_server.h"

#include "ibp_connection.h"

extern IBP_CONN_POOL *glbConnPool;
extern int            glbDepotShutdown;
extern void *ibp_calloc(size_t count, size_t size);

IBP_CONN_POOL *
init_conn_pool ( )
{
    IBP_CONN_POOL *pool = NULL;

    pool=(IBP_CONN_POOL*)ibp_calloc(1,sizeof(IBP_CONN_POOL));
    pool->allocated = 0;
    pool->connBuffer = NULL;

    return (pool);
}

IBP_CONN_LIST *
init_conn_list()
{
    IBP_CONN_LIST *list = NULL;

    list = (IBP_CONN_LIST *)ibp_calloc(1,sizeof(IBP_CONN_LIST));
    list->maxnum = FD_SETSIZE;
    list->maxfd = -1;
    list->list = (IBP_CONNECTION**)ibp_calloc(list->maxnum,sizeof(IBP_CONNECTION*));
    list->iterator = NULL; 
    list->readfds =(fd_set*)ibp_calloc(1,sizeof(fd_set));
    list->writefds=NULL;
    return (list);
}

static IBP_CL_ITERATOR *
init_conn_list_iterator( IBP_CONN_LIST *list)
{
    IBP_CL_ITERATOR *itr=NULL;

    assert(list != NULL);
    itr = (IBP_CL_ITERATOR*)ibp_calloc(1,sizeof(IBP_CL_ITERATOR));
    itr->list = list;
    itr->cursor = -1;

    return (itr);
}

IBP_CL_ITERATOR *
get_conn_list_iterator(IBP_CONN_LIST *list)
{
    assert(list !=NULL);

    if ( list->iterator == NULL ){
        list->iterator = init_conn_list_iterator(list);
    }
    list->iterator->cursor = -1;

    return(list->iterator);
}

IBP_CONNECTION *
get_next_connection(IBP_CL_ITERATOR *itr)
{
    int fd;
    IBP_CONNECTION *conn=NULL;
    IBP_CONN_LIST  *list=NULL;

    list = itr->list;
   
    (itr->cursor)++;
    while ( itr->cursor <= list->maxfd ){
        if ( (conn=list->list[itr->cursor]) != NULL ){
            fd = get_conn_fd(conn);
            if (list->readfds != NULL && 
                 FD_ISSET(fd ,list->readfds)){ 
                set_conn_status(conn,IBP_CONN_RD_READY);
            };
            if (list->writefds !=NULL &&
                FD_ISSET(fd,list->writefds)){
                set_conn_status(conn,IBP_CONN_WR_READY);
            };
            break;
        }
        (itr->cursor)++;
    }

    return (conn);
}

static IBP_CONNECTION *
allocate_conn_from_pool(IBP_CONN_POOL *pool)
{
    IBP_CONNECTION *conn=NULL;
   
    conn = pool->connBuffer;
    if ( conn == NULL ){
        conn = (IBP_CONNECTION *)ibp_calloc(1,sizeof(IBP_CONNECTION));
        set_conn_fd(conn,-1);
        pool->allocated++;
    }else{
        pool->connBuffer = pool->connBuffer->next; 
        glbConnPool->num --; 
    };

    return(conn);
}

IBP_CONNECTION *
create_connection(int fd)
{
    IBP_CONNECTION *conn;

    assert(fd >= 0);

    conn = allocate_conn_from_pool(glbConnPool);
    set_conn_fd(conn,fd);
    clear_conn_status(conn);
    set_conn_request(conn,NULL);
    set_conn_stime(conn,0);
    conn->authenticated = 0;
    if (conn->cn != NULL ){
        free(conn->cn);
        conn->cn = NULL;
    }
    if( conn->ou != NULL ){
        free(conn->ou);
        conn->ou = NULL;
    }
    conn->next = NULL;

    return(conn);
}

static void
reclaim_conn_to_pool(IBP_CONN_POOL *pool, IBP_CONNECTION *conn)
{
    if ( conn->cn != NULL ){
        free(conn->cn);
        conn->cn = NULL;
    }
    if (conn->ou != NULL){
        free(conn->ou);
        conn->ou = NULL;
    }
    bzero(conn,sizeof(IBP_CONNECTION));
    set_conn_fd(conn,-1);
    conn->next= pool->connBuffer ;
    pool->connBuffer = conn;
    pool->num++;

    return;
}

void
free_connection(IBP_CONNECTION *conn)
{
    assert(conn != NULL);

    reclaim_conn_to_pool(glbConnPool,conn);
    return;
}

int 
wait_ready_conn(IBP_CONN_LIST *list, struct timeval *to)
{
    int i=0,ret;
    IBP_CONNECTION *conn=NULL;
    assert(list != NULL);

    /* clear fd set */
    if ( list->readfds != NULL ) { FD_ZERO(list->readfds); }
    if ( list->writefds != NULL ){ FD_ZERO(list->writefds);}

    /* set interested event */
    for (i=0;i<=list->maxfd;i++){
        conn = list->list[i];
        if ( conn == NULL ) {continue;};
        if ( conn_st_isset(conn,IBP_CONN_RD_WAIT) && list->readfds != NULL ){
            FD_SET(get_conn_fd(conn),list->readfds);
        }
        if ( conn_st_isset(conn,IBP_CONN_WR_WAIT) && list->readfds != NULL ){
            FD_SET(get_conn_fd(conn),list->writefds);
        }
    }

    /* call select function */
    while (1){
        ret = select(list->maxfd+1,list->readfds,list->writefds,NULL,to);
        if ( ret < 0 ){
            if ( errno == EINTR ){ 
                if ( glbDepotShutdown == 1 ){
                    return 0;
                }
                fprintf(stderr,"interrupted\n");
                continue; 
            }else{
                perror("panic! select error:");
                abort();
            }
        }else{
            break;
        }
    }

    return (ret);
}

void
close_connection(IBP_CONNECTION *conn)
{
    int fd;

    fd = get_conn_fd(conn);
    if (conn->request != NULL){
        conn->request = NULL;
    }
    close(fd);
    clear_conn_status(conn);
    set_conn_status(conn,IBP_CONN_CLOSED);
    return;
};


int
insert_conn_to_list(IBP_CONN_LIST *list, IBP_CONNECTION *conn)
{
    int fd;

    assert(list != NULL && conn != NULL );
    assert(list->list[get_conn_fd(conn)] == NULL );
    
    fd = get_conn_fd(conn);

    if ( fd >= list->maxnum ) { return -1; };

    list->list[fd] = conn;
    if ( fd > list->maxfd ){
        list->maxfd = fd;
    }

    return (0);
}

IBP_CONNECTION *
delete_conn_from_list(IBP_CONN_LIST *list, IBP_CONNECTION *conn)
{
    int fd;
    assert(list != NULL && conn != NULL);
    
    list->list[get_conn_fd(conn)] = NULL;
    fd = get_conn_fd(conn);
    if ( fd >= list->maxfd ){
        fd--;
        while ( fd >= 0 ){
            if ( list->list[fd] != NULL ){
                break;
            }
            fd--;
        }
        list->maxfd = fd;
    }

    return(conn);
}

int 
read_line_from_conn( IBP_CONNECTION     *conn,
                     char               *buf,
                     int                bufSize,
                     int                timeout)
{
    int     fd = -1;
    int     nread = 0;
    char    *ptr = NULL;
    char    *ptr10=NULL, *ptr13=NULL;
    int     size;
    int     ret;

    assert ( timeout == -1 && conn != NULL && buf != NULL );

    fd = get_conn_fd(conn);
    ptr = buf;
    nread = 0;

RECV_AGAIN:
    ptr = buf + nread;
    size = bufSize - nread; 
    ret = recv(fd,ptr,size,MSG_PEEK);
    if ( ret < 0 ){
        /* error occur */
        if ( errno == EINTR ){
            /* interrupted, recv again */
            goto RECV_AGAIN;
        }else if ( errno == EAGAIN ){
            return (nread);
        }else {
            return (IBP_E_SOCK_READ);
        }
    }else if ( ret == 0){
        return (IBP_E_SOCK_READ);
    }

    *(ptr+ret) = '\0'; 
    /* find new line character */
    ptr10 = strchr(ptr,10);
    ptr13 = strchr(ptr,13);
    if ( ptr10 != NULL || ptr13 != NULL ){
        if (ptr13 > ptr10){
            ptr10 = ptr13;
        }
        size = ptr10 - ptr + 1;
    }else{
        size = ret;
    }

READ_AGAIN:
    ret = read(fd,ptr,size);
    if ( ret < 0 ){
        if ( errno == EINTR ) { 
            goto READ_AGAIN;
        }else if ( errno == EAGAIN ){
            return (nread); 
        }else{
            return (IBP_E_SOCK_READ);
        }
    }else if ( ret == 0 ){ 
        return (IBP_E_SOCK_READ);
    }

    return (ret);
};



