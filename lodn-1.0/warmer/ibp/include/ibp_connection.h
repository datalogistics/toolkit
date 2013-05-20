#ifndef IBP_CONNECTION_H
#define IBP_CONNECTION_H

#include <sys/types.h>
#include <sys/time.h> /* struct timeval */
#include <unistd.h>
#include "ibp_request.h"

#ifdef __cplusplus
#define "C" {
#endif

typedef enum 
{
    IBP_CONN_CLEAR    = 0x00000000,
    IBP_CONN_RD_READY = 0x00000001,
    IBP_CONN_WR_READY = 0x00000002,
    IBP_CONN_CLOSED   = 0x00000010,
    IBP_CONN_RD_WAIT  = 0x00010000,
    IBP_CONN_WR_WAIT  = 0x00020000
}
IBP_CONN_STATUS;

#define IBP_NEW_LINE            '\n'

typedef struct ibp_connection
{
    int                     fd;      /* connection handler */
    IBP_CONN_STATUS         status;  /* connection status */
    struct ibp_request      *request;/* request sent on the connection */
    time_t                  stime;   /* the time connection start to wait data */  
    int                     authenticated;
    char                    *cn;
    char                    *ou;
    struct ibp_connection   *next;   /* link to next connection structure */
}
IBP_CONNECTION;

typedef struct ibp_conn_pool
{
    int                 allocated;
    int                 num;
    IBP_CONNECTION      *connBuffer; 
}
IBP_CONN_POOL;

typedef struct ibp_cl_iterator
{
    struct ibp_conn_list  *list;
    int                   cursor;
}
IBP_CL_ITERATOR;

typedef struct ibp_conn_list
{
    int                 maxnum;    /* maximum number of fd handled by select*/
    int                 maxfd;    /* number of connection in the list */
    IBP_CONNECTION      **list;    /* list of connections */     
    IBP_CL_ITERATOR     *iterator; /* connection list iterator */ 
    fd_set              *readfds;  /* read fd set */
    fd_set              *writefds; /* write fd set */
#if 0
    pthread_mutex_t*   lock;        /* lock to access list */  
#endif
}
IBP_CONN_LIST;

/* create a connection pool */
extern IBP_CONN_POOL *
init_conn_pool( );

/* create a connection list */
extern IBP_CONN_LIST *
init_conn_list();

extern IBP_CONNECTION *
create_connection(int fd);

extern void
free_connection(IBP_CONNECTION *conn);

/* insert a connection */
extern int 
insert_conn_to_list( IBP_CONN_LIST *list, IBP_CONNECTION *conn);

/* delete connection from list */
extern IBP_CONNECTION *
delete_conn_from_list( IBP_CONN_LIST *list, IBP_CONNECTION *conn);

extern IBP_CL_ITERATOR *
get_conn_list_iterator( IBP_CONN_LIST *list);

extern IBP_CONNECTION *
get_next_connection(IBP_CL_ITERATOR *iterator);

extern int
wait_ready_conn(IBP_CONN_LIST *list, struct timeval *to);

#define get_conn_fd(conn)           ((conn)->fd)
#define set_conn_fd(conn,handler)   ((conn)->fd)=handler 

#define get_conn_request(conn)      ((conn)->request)
#define set_conn_request(conn,req) (((conn)->request) = (req))

#define get_conn_status(conn)       ((conn)->status)
#define set_conn_status(conn,st)    (((conn)->status) |= (st))
#define clear_conn_status(conn)     (((conn)->status)=IBP_CONN_CLEAR)
#define conn_st_isset(conn,st)      ((conn)->status & (st))
#define get_conn_stime(conn)        ((conn)->stime)
#define set_conn_stime(conn,st)     (((conn)->stime) = (st))

extern void 
close_connection(IBP_CONNECTION * conn);

extern int
read_line_from_conn(IBP_CONNECTION *  conn, 
                    char *            buf, 
                    int               bufSize, 
                    int               timeout);

extern int
read_data_from_conn(IBP_CONNECTION *  conn, 
                    int               maxSize, 
                    char *            buf, 
                    int *             readSize,
                    int               timeout);

extern int
write_data_to_conn(IBP_CONNECTION    *conn, 
                   int               maxSize, 
                   char *            buf, 
                   int *             writeSize,
                   int               timeout);

extern void
free_conn_list_iterator(IBP_CL_ITERATOR *iterator);


#ifdef __cplusplus
#define }
#endif


#endif /* ibp_connection.h */
