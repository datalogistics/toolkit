/*
 *           IBP Client version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *          Authors: Y. Zheng A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 1999 All Rights Reserved
 *
 *                              NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appear in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * Neither the Institution (University of Tennessee) nor the Authors 
 * make any representations about the suitability of this software for 
 * any purpose. This software is provided ``as is'' without express or
 * implied warranty.
 *
 */  
# include "config-ibp.h"
# include "ibp_os.h"
    
# ifdef HAVE_STRINGS_H
# include <strings.h>
# endif /* HAVE_STRINGS_H */
# ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
# endif /* HAVE_SYS_TIME_H */
# include <errno.h>
# include <sys/types.h>
# ifdef HAVE_UNISTD_H
# include <unistd.h>
# endif /* HAVE_UNISTD_H */
# ifdef HAVE_FCNTL_H
# include <fcntl.h>
# endif /* HAVE_FCNTL_H */
# ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# endif /* HAVE_SYS_SOCKET_H */
# ifdef HAVE_NETINET_IN_H 
# include <netinet/in.h>
# endif /* HAVE_NETINET_IN_H */
# ifdef HAVE_NETDB_H 
# include <netdb.h>
# endif /* HAVE_NETDB_H */

# ifdef IBP_WIN32 
# include <winsock2.h>
# endif
    
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
    
# include <time.h>
# include "ibp_protocol.h"
# include "ibp_net.h"
# include "ibp_errno.h"
    
# ifdef IBP_WIN32
# define SOCKET_TYPE	SOCKET
# define EINTR			WSAEINTR
# define EINPROGRESS	WSAEINPROGRESS 
# define EWOULDBLOCK	WSAEWOULDBLOCK
# define SYS_OK			ERROR_SUCCESS
# define sockaddr_in6	sockaddr_in
# define sin6_family	sin_family
# define sin6_port		sin_port
# define cleanup_socket WSACleanup
# define _close_socket	closesocket	

# else /* Unix like OSs */
    
# define SOCKET_TYPE	int
# define SYS_OK			0
# define INVALID_SOCKET	-1
# define cleanup_socket cleanup_socket_foo  
# define _close_socket	close 
void cleanup_socket_foo()
{
}

#endif

#ifdef HAVE_PTHREAD_H
# include <pthread.h>
    pthread_once_t cacheInit = PTHREAD_ONCE_INIT;
    pthread_once_t ghbnInit = PTHREAD_ONCE_INIT;
    pthread_once_t connListInit = PTHREAD_ONCE_INIT;
    pthread_once_t reusedConnInit = PTHREAD_ONCE_INIT;
    pthread_key_t reusedConnKey;
    pthread_mutex_t *ghbnLock;
    pthread_mutex_t *glbConnListLock;
# else 
    int   glbReusedConn = 0;
#endif  /* HAVE_PTHREAD_H */

CONNECTION      *glbConnList ;
DNSCACHE        *glbCache = NULL;
static int      glbMaxOpenConn = MAX_PERSISTENT_CONNECTIONS;
    
# define MAX_READ_SIZE  (~(1<<((sizeof(int) * 8) -1 )))
int ibp_ntop( struct sockaddr *addr, char *strptr, size_t len){
        char tmp[128];
        const unsigned char *p;
        const unsigned short *s;
        struct sockaddr_in *sin;
#ifndef IBP_WIN32
#ifdef AF_INET6
        struct sockaddr_in6 *sin6;
#endif
#endif

        if ( addr->sa_family == AF_INET ){
            sin = (struct sockaddr_in *)addr;
            p = (const unsigned char*)(&(sin->sin_addr));
            snprintf(tmp,127,"%d.%d.%d.%d",p[0],p[1],p[2],p[3]);
            if ( strlen(tmp) >= len ){
                return (-1);
            }
            strcpy(strptr,tmp);
            return (0);
        }
#ifndef IBP_WIN32
#ifdef AF_INET6
        if ( addr->sa_family == AF_INET6){
            sin6 = (struct sockaddr_in6 *)addr;
            s = (const unsigned short *)(&(sin6->sin6_addr));
           snprintf(tmp,127,"%x:%x:%x:%x:%x:%x:%x:%x",
                        s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7]);
           if ( strlen(tmp) >= len ){
                return(-1);
           }
           strcpy(strptr,tmp);
           return (0);
        }
#endif
#endif
        return(-1);
}
void _clear_buf(void *pc_buf, unsigned long int pl_size)
{
    unsigned long int i;
    char *p;
    p = (char *) pc_buf;
    for (i = 0; i < pl_size; i++) {
        p[i] = 0;
    }
}
void set_time_val(struct timeval *ps_timeval, time_t pt_expireTime)
{
    ps_timeval->tv_sec = pt_expireTime - time(0);
    ps_timeval->tv_usec = 0;
}
static void deleteConn(int fd);

void close_socket(int pi_fd)
{

#if IBP_PERSISTENT_CONN
    deleteConn(pi_fd);
#endif
    _close_socket(pi_fd);
    cleanup_socket();
}

static void  closeIdleConnections();

# ifndef IBP_WIN32
int set_socket_noblock_unix(int pi_fd)
{
    int flags;
    if ((flags = fcntl(pi_fd, F_GETFL, 0)) < 0)
    {
        perror("fcntl-0");
        fprintf(stderr," errno = %d msg = %s\n",errno,strerror(errno));
        return (-1);
    }
    if ((flags = fcntl(pi_fd, F_SETFL, flags | O_NONBLOCK)) < 0)
    {
        perror("fcntl-1");
        return (-1);
    }
    return (0);
}
int set_socket_block_unix(int pi_fd)
{
    int flags;
    if ((flags = fcntl(pi_fd, F_GETFL, 0)) < 0)
        return (-1);
    if ((flags = fcntl(pi_fd, F_SETFL, flags & (~O_NONBLOCK))) < 0)
        return (-1);
    return (0);
}

#endif 

int IBP_setMaxOpenConn(int max)
{
#ifdef IBP_PERSISTENT_CONN
    CONNECTION *list;

    if ( max <= glbMaxOpenConn ){
        return glbMaxOpenConn;
    }

#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbConnListLock);
#endif

    list = (CONNECTION *)realloc(glbConnList,sizeof(CONNECTION)*max);
    if ( list != NULL ){
        glbConnList = list;
        glbMaxOpenConn = max;
    }

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(glbConnListLock);
#endif

    return (glbMaxOpenConn);

#else /* IBP_PERSISTENT_CONN */
    return 0;
#endif

}

static void initCacheData(void)
{
    DNSCACHE * cache;
    int i;
    if ((cache = (DNSCACHE *) calloc(1, sizeof(DNSCACHE))) == NULL) {
        return;
    }
    
#ifdef HAVE_PTHREAD_H
    if ((cache->lock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t))) == NULL) {
        free(cache);
        return;
    }
    pthread_mutex_init(cache->lock, NULL);
    
#endif  /*  */
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        cache->addrs[i].addr = NULL;
        cache->addrs[i].ref = 0;
        cache->addrs[i].hostName[0] = '\0';
        cache->addrs[i].lastUsed = 0;
    }
    glbCache = cache;
    return;
}
/*
static void _destoryConnList(void *buf)
{
    CONNECTION *list;
    int i;
    if ( buf != NULL ){
        list = (CONNECTION*)buf;
        for ( i=0; i < MAX_OPEN_CONNECTIONS ; i ++){
            if ( list[i].fd >= 0 ){
                close(list[i].fd);
                list[i].fd = -1;
            }
        }
        free(buf);
    }
}
*/

#ifdef HAVE_PTHREAD_H
static void _createConnListLock(void)
{
    if ( NULL == (glbConnListLock = (pthread_mutex_t *)calloc(sizeof(pthread_mutex_t),1))){
        fprintf(stderr,"Out of memory!\n");
        exit(-1);
    }
    if ( 0 != pthread_mutex_init(glbConnListLock,NULL)){
        fprintf(stderr,"Can't initialize pthread mutex !\n");
        exit(-1);
    }
}
#endif

static void _createConnList(void)
{
    CONNECTION *list;
    int i;

    if ( (list = (CONNECTION*)calloc(sizeof(CONNECTION),glbMaxOpenConn)) == NULL) {
        fprintf(stderr,"Out of memory! can't use persistent connections \n");
        glbMaxOpenConn = 0;
        return;
    };
    for ( i = 0 ; i < glbMaxOpenConn ; i++ ){
        list[i].fd = -1;
        list[i].lastUsed = -1;
        list[i].inUse = 0;
    }

    glbConnList = list;

}

void initConnList()
{
    CONNECTION *list;

#ifdef HAVE_PTHREAD_H
    pthread_once(&connListInit,_createConnListLock);
    pthread_mutex_lock(glbConnListLock);
#endif
    list = glbConnList;

    if ( list == NULL ){
        _createConnList();
    }

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(glbConnListLock);
#endif
   
}

void insertConnFd(char *hostname,int port, int fd)
{
   CONNECTION *list;
   int pos = -1,i;
   time_t oldestTime = -1;

#if IBP_PERSISTENT_CONN
   initConnList();
   list = glbConnList;

#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbConnListLock);
#endif

   closeIdleConnections();

   for ( i = 0 ; i < glbMaxOpenConn ; i ++ ){
      if ( list[i].fd < 0 ){
        pos = i;
        break;
      }
   }

   if ( pos < 0 ){
      for ( i = 0 ; i < glbMaxOpenConn ; i ++ ){
        if ( list[i].inUse == 1 ){
            continue;
        }
        if ( oldestTime <  0 ){
          oldestTime = list[i].lastUsed;
          pos=i;
        }else{
          if ( oldestTime > list[i].lastUsed ){
             oldestTime = list[i].lastUsed;
             pos = i;
          }
        }
      }
   }

   if ( pos >= 0 ){
        if ( list[pos].fd >= 0 ){
/*fprintf(stderr,"IBPCONN close less used connection fd = %d\n",list[pos].fd);*/
            _close_socket(list[pos].fd);
        }

        strncpy(list[pos].hostName,hostname,255);
        list[pos].port = port;
        list[pos].fd =fd;
        list[pos].lastUsed=time(0);
        list[pos].inUse = 1;
/*fprintf(stderr,"IBPCONN insert new connection fd = %d\n",list[pos].fd);*/
   }
#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(glbConnListLock);
#endif

#endif
}

void deleteConn( int fd ){
    int i,pos = -1;
    CONNECTION *list;

    initConnList();
    list = glbConnList;

#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbConnListLock);
#endif

    for ( i = 0 ; i < glbMaxOpenConn ; i ++ ){
        if ( list[i].fd == fd ){
            pos = i;
            break;
        }
    }

    if ( pos >= 0 ){
/*fprintf(stderr,"IBPCONN: delete connection fd = %d ",list[pos].fd);*/
        list[pos].hostName[0] = '\0';
        list[pos].fd = -1;
    }

    closeIdleConnections();

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(glbConnListLock);
#endif

}

static void  closeIdleConnections()
{
#ifdef IBP_PERSISTENT_CONN
    CONNECTION *list;
    int i;
    time_t now;

    list = glbConnList;
   
    now = time(0);
    for ( i=0;i < glbMaxOpenConn ; i++){
        if ( (list[i].fd >= 0) && 
             (list[i].inUse == 0) &&
             ((now - list[i].lastUsed) > IBP_MAX_CLIENT_IDLE_TIME)){
/*fprintf(stderr,"IBPCONN: close idle connection fd = %d ",list[i].fd);*/

            _close_socket(list[i].fd);
            cleanup_socket();
            list[i].fd = -1;
            list[i].hostName[0] = '\0';
                 
        }
    }

#endif
}

int findConn(char *hostname, int port ){
    int i, fd= -1;

#ifdef IBP_PERSISTENT_CONN
    CONNECTION *list;

    initConnList();
    list = glbConnList;
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbConnListLock);
#endif
    glbReusedConn = 0;
    for ( i = 0 ; i < glbMaxOpenConn ; i ++ ){
        if ( list[i].fd < 0 ){
            continue;
        };
        if (list[i].fd >= 0 &&
            strcmp(list[i].hostName,hostname) == 0 &&
            list[i].port == port &&
            list[i].inUse == 0){ 

            list[i].lastUsed = time(0);
            fd = list[i].fd;
	        glbReusedConn = 1;
            list[i].inUse = 1;
/*fprintf(stderr,"IBPCONN: find open fd = %d for host[%s] port[%d]\n",fd,list[i].hostName,list[i].port);*/
            break;
        }
    }
    closeIdleConnections();

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(glbConnListLock);
#endif
#endif

    return (fd);
}

void releaseConnection( int fd )
{
#ifdef IBP_PERSISTENT_CONN
    CONNECTION  *list;
    int  i=0,found = 0;

    initConnList();
    list = glbConnList;
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbConnListLock);
#endif
    for(i=0; i < glbMaxOpenConn; i++){
        if ( list[i].fd == fd ){
            list[i].inUse = 0;
            list[i].lastUsed = time(0);
/*fprintf(stderr,"IBPCONN: release fd = %d\n",list[i].fd);*/
            found = 1;
            break;
        }
    }

    if ( found == 0 ){
/*fprintf(stderr,"IBPCONN: list is full close fd = %d\n",fd);*/
        _close_socket(fd);
        cleanup_socket();
    }

    closeIdleConnections();

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(glbConnListLock);
#endif

#endif
}

#ifdef HAVE_PTHREAD_H
static void _reusedConnDestructor(void *ptr) { free(ptr); }
void _createReusedConnKey(void) 
{
	pthread_key_create(&reusedConnKey,_reusedConnDestructor);
}
int *_reusedConn()
{
	int *output;

	pthread_once(&reusedConnInit,_createReusedConnKey);
	output = pthread_getspecific(reusedConnKey);
	if ( output == NULL ){
		output = (int*)calloc(1,sizeof(int));
		*output = 0;
		pthread_setspecific(reusedConnKey,output);
	}
	return (output);
} 
#endif


void initCache()
{
    if (glbCache == NULL) {
        
#ifdef HAVE_PTHREAD_H
            pthread_once(&cacheInit, initCacheData);
#else   
            initCacheData();
#endif  
    }
}

#ifdef HAVE_PTHREAD_H
static void _initGHBNLock(void){
    pthread_mutex_t *lock;
    if ( (lock = (pthread_mutex_t *)calloc(1,sizeof(pthread_mutex_t))) == NULL){
        fprintf(stderr,"out of memory!\n");
        exit(-1);
    } 
    if ( 0 != pthread_mutex_init(lock,NULL)){
        fprintf(stderr,"Can't initialize pthread lock!\n");
        exit(-1);
    };
    ghbnLock = lock;
    return;
}  
#endif

void initGHBNLock() 
{            
#ifdef HAVE_PTHREAD_H                   
    if ( ghbnLock == NULL ){
        pthread_once(&ghbnInit,_initGHBNLock);
    }
#endif
}


struct addrinfo *search_dns_cache(char *host)
{
    int i;
    struct addrinfo *addr = NULL;
    initCache();
    if (glbCache == NULL) {
        return (NULL);
    }
    
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbCache->lock);
#endif  /*  */
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        if (glbCache->addrs[i].addr == NULL) {
        continue;
        }
        if (strcasecmp(glbCache->addrs[i].hostName, host) == 0) {
            addr = glbCache->addrs[i].addr;
            glbCache->addrs[i].ref++;
            glbCache->addrs[i].lastUsed = time(NULL);
            break;
        }
    }
    
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(glbCache->lock);
#endif  /*  */
    return (addr);
}

#if !HAVE_FUNC_FREEADDRINFO 
void freeaddrinfo(struct addrinfo *ai){
        struct addrinfo *tmp;
        while( ai != NULL ){
            tmp = ai->ai_next;
            free(ai->ai_addr);
            free(ai);
            ai = tmp;
       }
       return;
}
#endif

#if !HAVE_FUNC_GETADDRINFO /* implement getaddrinfo */
struct addrinfo * _ibpCreateAddrInfo(){
        struct addrinfo *ai;

        if ( (ai = (struct addrinfo*)calloc(1,sizeof(struct addrinfo))) == NULL ){
            return (NULL);
        }
        if ((ai->ai_addr = (struct sockaddr *)calloc(1,sizeof(struct sockaddr_in)))== NULL ){
            free(ai);
            return(NULL);
        }

        ai->ai_family = AF_INET;
        ai->ai_socktype = SOCK_STREAM;
        ai->ai_protocol = 0;
        ai->ai_next = NULL;

        return(ai);
}

int getaddrinfo ( const char *nodename, const char *servname,
                  const struct addrinfo *hists, struct addrinfo **res)
{
    struct addrinfo *ai,*prev;
    struct hostent  *hptr;
    struct sockaddr_in *sin;
    struct in_addr  **pptr;
    int     head = 1;

    initGHBNLock();
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(ghbnLock);
#endif
    if ( (hptr=gethostbyname(nodename)) == NULL ){
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(ghbnLock);
#endif
        return(-1);
    }

    if ( hptr->h_addr_list[0] == NULL ){
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(ghbnLock);
#endif
        free(ai->ai_addr);
        free(ai);
        return(-1);
    }

    *res = NULL;
    pptr = (struct in_addr **)(hptr->h_addr_list);
   
    while( *pptr != NULL) {
        prev = ai;
        if ( (ai = _ibpCreateAddrInfo()) == NULL ){
            freeaddrinfo(*res);
#ifdef HAVE_PTHREAD_H
            pthread_mutex_unlock(ghbnLock);
#endif
            return(-1);
        }
        if ( head ){
            *res = ai;
            head = 0;
        }else{
            prev->ai_next = ai;   
        }
        ai->ai_addrlen = sizeof(struct sockaddr_in);
        sin = (struct sockaddr_in *)(ai->ai_addr);
#ifdef HAVE_STRUCT_MEMBER_SOCKADDR_IN_SIN_LEN
        sin->sin_len = sizeof(struct sockaddr_in);
#endif
        sin->sin_family = AF_INET;
        sin->sin_port = atoi(servname);
        memcpy(&(sin->sin_addr),*pptr,sizeof(struct in_addr));
        pptr++;
    }

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(ghbnLock);
#endif
    return (0);

}

#endif

int insert_addr_to_cache(char *host, struct addrinfo *addr)
{
    int idx = -1;
    int i;
    time_t oldestTime = 0;
    initCache();
    if (glbCache == NULL) {
        return (idx);
    }
    
#ifdef HAVE_PTHREAD_H
        pthread_mutex_lock(glbCache->lock);
    
#endif  /*  */
        for (i = 0; i < MAX_CACHE_ADDR; i++) {
        if (glbCache->addrs[i].addr == NULL) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        for (i = 0; i < MAX_CACHE_ADDR; i++) {
            if (glbCache->addrs[i].ref <= 0) {
                if ( oldestTime == 0){
                    oldestTime = glbCache->addrs[i].lastUsed;
                    idx = i;
                }else if ( oldestTime > glbCache->addrs[i].lastUsed){
                    oldestTime = glbCache->addrs[i].lastUsed;
                    idx = i;
                };
                break;
            }
        }
    }
    if (idx != -1) {
        if (glbCache->addrs[idx].addr != NULL) {
            freeaddrinfo(glbCache->addrs[idx].addr);
            glbCache->addrs[idx].addr = NULL;
        }
        strncpy(glbCache->addrs[idx].hostName, host, 255);
        glbCache->addrs[idx].hostName[255] = '\0';
        glbCache->addrs[idx].addr = addr;
        glbCache->addrs[idx].ref = 1;
        glbCache->addrs[idx].lastUsed = time(NULL);
    }
    
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(glbCache->lock);
    
#endif  /*  */
        return (idx);
}
void printDNSCache()
{
    int i;
    fprintf(stderr, "\nBEGIN\n");
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        if (glbCache->addrs[i].addr != NULL) {
            fprintf(stderr, " %s :: %d \n", glbCache->addrs[i].hostName,
                     glbCache->addrs[i].ref);
        }
    }
    fprintf(stderr, "\nEND\n");
}
void release_addr_to_cache(struct addrinfo *addr)
{
    int i;
    initCache;
    if (glbCache == NULL) {
        return;
    }
    
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(glbCache->lock);
#endif  /*  */
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        if (glbCache->addrs[i].addr != NULL
             && glbCache->addrs[i].addr == addr) {
            glbCache->addrs[i].ref--;
            if (glbCache->addrs[i].ref < 0) {
                fprintf(stderr, "Internal error !\n");
                exit(-1);
            }
            break;
        }
    }
    
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(glbCache->lock);
    
#endif  /*  */
        return;
}

# ifdef IBP_WIN32
int set_socket_noblock_win(int pi_fd)
{
    u_long val = 1;
    int retval;
    retval = ioctlsocket(pi_fd, FIONBIO, &val);
    if (retval != 0)
        return (-1);
    return (0);
}
int set_socket_block_win(int pi_fd)
{
    u_long val = 0;
    if (WSAEventSelect(pi_fd, 0, 0) != 0)
        return (-1);
    if (ioctlsocket(pi_fd, FIONBIO, &val) != 0)
        return (-1);
    return (0);
}

#endif

void _close_sockfd(void *fd)
{
    int i;
    i = (int) fd;
    if (i >= 0) {
        close(i);
    }
    return;;
}
unsigned long int read_var_data(int pi_fd, unsigned long int pl_size,
                                  char *pc_buf, time_t pt_expireTime)
{
    struct timeval ls_timeval;
    struct timeval *ls_tv;
    fd_set ls_rd_set;
    int li_return;
    unsigned long int ll_nread;
    char lc_char;
    int err;
    
        IBP_errno = IBP_OK;
    _clear_buf(pc_buf, pl_size);
    if (pt_expireTime == 0) {
        ls_tv = NULL;
    }
    else {
        ls_tv = &ls_timeval;
        set_time_val(ls_tv, pt_expireTime);
    }
    FD_ZERO(&ls_rd_set);
    FD_SET((SOCKET_TYPE) pi_fd, &ls_rd_set);
    ll_nread = 0;
    while (ll_nread < pl_size) {
        if (ls_tv != NULL) {
            if (time(0) > pt_expireTime) {
                IBP_errno = IBP_E_CLIENT_TIMEOUT;
                ll_nread = 0;
                goto bail;
            }
            else {
                ls_tv->tv_sec = pt_expireTime - time(0);
            }
        }
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            li_return = select(pi_fd + 1, &ls_rd_set, NULL, NULL, ls_tv);
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            if (li_return < 0) {
            err = _get_errno();
            if (err == EINTR || err == SYS_OK) {
                continue;
            }
            else {
                IBP_errno = IBP_E_SOCK_READ;
                ll_nread = 0;
                goto bail;
            }
        }
        else if (li_return == 0) {
            IBP_errno = IBP_E_CLIENT_TIMEOUT;
            ll_nread = 0;
            goto bail;
        }
        else {
          READ_AGAIN:
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                li_return = recv(pi_fd, pc_buf + ll_nread, 1, 0);
            
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                if (li_return < 0) {
                err = _get_errno();
                if ((err == SYS_OK) || (err == EINTR) || (err == EWOULDBLOCK)
                     || (err == EINPROGRESS)) {
                    continue;
                }
                else {
                    IBP_errno = IBP_E_SOCK_READ;
                    ll_nread = 0;
                    goto bail;
                }
            }
            else if (li_return == 0) {
                if ( ll_nread == 0 ){
			IBP_errno = IBP_E_SOCK_READ;
			goto bail;
		}
                break;
            }
            else {
                lc_char = (char) (*((char *) (pc_buf) + ll_nread));
                ll_nread += li_return;
                if (lc_char == '\n' || lc_char == '\0') {
                    break;
                }
                goto READ_AGAIN;
            }
        }
    }
    IBP_errno = IBP_OK;
  bail:
        return (ll_nread);
}
unsigned long int read_fix_data(int pi_fd, unsigned long int pl_size,
                                   char *pc_buf, time_t pt_expireTime)
{
    struct timeval ls_timeval;
    struct timeval *ls_tv;
    fd_set ls_rd_set;
    int li_return;
    unsigned long int ll_nleft;
    unsigned long int ll_nread;
    int err;
    
        IBP_errno = IBP_OK;
    _clear_buf(pc_buf, pl_size);
    if (pt_expireTime == 0)
        ls_tv = NULL;
    
    else {
        ls_tv = &ls_timeval;
        set_time_val(ls_tv, pt_expireTime);
    }
    FD_ZERO(&ls_rd_set);
    FD_SET((SOCKET_TYPE) pi_fd, &ls_rd_set);
    ll_nleft = pl_size;
    ll_nread = 0;
    while (ll_nleft > 0) {
        if (ls_tv != NULL) {
            if (time(0) > pt_expireTime) {
                IBP_errno = IBP_E_CLIENT_TIMEOUT;
                ll_nread = 0;
                goto bail;
            }
            else {
                ls_tv->tv_sec = pt_expireTime - time(0);
            }
        }
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            li_return = select(pi_fd + 1, &ls_rd_set, NULL, NULL, ls_tv);
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            if (li_return < 0) {
            err = _get_errno();
            if (err == EINTR || err == SYS_OK) {
                continue;
            }
            else {
                IBP_errno = IBP_E_SOCK_READ;
                ll_nread = 0;
                goto bail;
            }
        }
        else if (li_return == 0) {
            IBP_errno = IBP_E_CLIENT_TIMEOUT;
            ll_nread = 0;
            goto bail;
        }
        else {
            if (ll_nleft > MAX_READ_SIZE)
                ll_nleft = MAX_READ_SIZE;
            
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                li_return = recv(pi_fd, pc_buf + ll_nread, ll_nleft, 0);
            
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                if (li_return < 0) {
                err = _get_errno();
                if ((err == SYS_OK) || (err == EINTR) || (err == EWOULDBLOCK)
                     || (err == EINPROGRESS))
                    continue;
                
                else {
                    IBP_errno = IBP_E_SOCK_READ;
                    ll_nread = 0;
                    goto bail;
                }
            }
            else if (li_return == 0) {
                break;
            }
            else {
                ll_nread += li_return;
                ll_nleft = pl_size - ll_nread;
            }
        }
    }
    IBP_errno = IBP_OK;
  bail:
        return (ll_nread);
}
unsigned long int write_data(int pi_fd, unsigned long int pl_size,
                               char *pc_buf, time_t pt_expireTime)
{
    struct timeval ls_timeval;
    struct timeval *ls_tv;
    fd_set ls_wr_set;
    int li_return;
    unsigned long int ll_nleft;
    unsigned long int ll_nwrite;
    int err;
    
        if (pt_expireTime == 0)
        ls_tv = NULL;
    
    else {
        ls_tv = &ls_timeval;
        set_time_val(ls_tv, pt_expireTime);
    }
    FD_ZERO(&ls_wr_set);
    FD_SET((SOCKET_TYPE) pi_fd, &ls_wr_set);
    ll_nleft = pl_size;
    ll_nwrite = 0;
    while (ll_nleft > 0) {
        if (ls_tv != NULL) {
            if (time(0) > pt_expireTime) {
                IBP_errno = IBP_E_CLIENT_TIMEOUT;
                ll_nwrite = 0;
                goto bail;
            }
            else {
                ls_tv->tv_sec = pt_expireTime - time(0);
            }
        }
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            li_return = select(pi_fd + 1, NULL, &ls_wr_set, NULL, ls_tv);
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            if (li_return < 0) {
            err = _get_errno();
            if ((err == EINTR) || (err == SYS_OK)) {
                continue;
            }
            else {
fprintf(stderr,"I am here\n");
                IBP_errno = IBP_E_SOCK_WRITE;
                ll_nwrite = 0;
                goto bail;
            }
        }
        else if (li_return == 0) {
            IBP_errno = IBP_E_CLIENT_TIMEOUT;
            ll_nwrite = 0;
            goto bail;
            
                /*
                   return(0);
                 */ 
        }
        else {
            if (ll_nleft > MAX_READ_SIZE)
                ll_nleft = MAX_READ_SIZE;
            
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                li_return = send(pi_fd, pc_buf + ll_nwrite, ll_nleft, 0);
            
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                if (li_return < 0) {
                err = _get_errno();
                perror("sending");
                if ((err == SYS_OK) || (err == EINTR) || (err == EWOULDBLOCK)
                     || (err == EINPROGRESS))
                    continue;
                
                else {
fprintf(stderr,"I am here too\n");
                    IBP_errno = IBP_E_SOCK_WRITE;
                    ll_nwrite = 0;
                    goto bail;
                }
            }
            else if (li_return == 0) {
                break;
            }
            else {
                ll_nwrite += li_return;
                ll_nleft = pl_size - ll_nwrite;
            }
        }
    }
    IBP_errno = IBP_OK;
  bail:
        return (ll_nwrite);
}
extern struct hostent *gethostname_re(const char *host,
                                        struct hostent *hostbuf,
                                        char **tmphstbuf, size_t * hstbuflen);

static int set_port_num( struct addrinfo *addr, int port)
{
   struct sockaddr_in *sa;
#ifndef IBP_WIN32
#ifdef PF_INET6
   struct sockaddr_in6 *sa6;
#endif
#endif
   int ret = 0;
   
   switch ( addr->ai_family){
   case PF_INET:
     sa = (struct sockaddr_in *)(addr->ai_addr);
     sa->sin_port = htons(port);
     break;
#ifndef IBP_WIN32
#ifdef PF_INET6
   case PF_INET6:
     sa6 = (struct sockaddr_in6 *)(addr->ai_addr);
     sa6->sin6_port = htons(port);
     break;
#endif
#endif
   default:
     ret = -1;
   }

   return ret;
} 
int connect_socket(char *pc_host, int pi_port, int pi_timeout)
{
    int li_sockfd = -1, li_ret = IBP_OK;
    fd_set ls_wset;
    struct timeval lt_timer = { 0, 0 }, *lp_timer;
    int err;
    time_t begin_time;
    int val, len;
    int ret;
    struct addrinfo hints, *res = NULL, *saveres;
    char serv[30];
    int fromCache = 0;

    
# ifdef IBP_WIN32
    WORD wVersionRequested;
    WSADATA wsaData;
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err == SOCKET_ERROR) {
        /* Tell the user that we could not find a usable */ 
        /* WinSock DLL.   */ 
        IBP_errno = IBP_E_CONNECTION;
        cleanup_socket();
        li_sockfd = -1;
        goto bail;
    }
    
# endif
	if ((li_sockfd = findConn(pc_host,pi_port)) >= 0 ){
		return (li_sockfd);
	}

    int garbage=1;        
#ifdef HAVE_PTHREAD_H
    pthread_cleanup_push(_close_sockfd, (void*)(li_sockfd));
    
#endif  /*  */

    begin_time = time(0);
    
    /*
     * clear errno 
     */ 
    _set_errno(SYS_OK);
    IBP_errno = IBP_OK;
    
    /*
     * 1 - get host by name and fill in the appropriate IP address record
     */ 
    bzero(&hints, sizeof(struct addrinfo));
#if IS_OSF_OS
    hints.ai_family = AF_INET;
#else
    hints.ai_family = AF_UNSPEC;
#endif
    hints.ai_socktype = SOCK_STREAM;
    sprintf(serv, "%d", pi_port);
    
    if ((res = search_dns_cache(pc_host)) != NULL) {
        fromCache = 1;
    }
    else {
        if ((ret = getaddrinfo(pc_host, serv, &hints, &res)) != 0) {
            
#ifdef IBP_DEBUG
                fprintf(stderr, "SocketConnect(): getaddrinfo() failed\n\t");
            
#endif  /*  */
            IBP_errno = IBP_E_CONNECTION;
            cleanup_socket();
            li_sockfd = -1;
            goto bail;
        }
        if (insert_addr_to_cache(pc_host, res) >= 0) {
            fromCache = 1;
        }
        else {
            fromCache = 0;
        }
    }
    saveres = res;
    
        /*
           * 3 - Connect - switch needed for different IP protocols ...
         */ 
        
    do {
        
            /* set timeout value */ 
            if (pi_timeout <= 0)
            lp_timer = NULL;
        
        else {
            lp_timer = &lt_timer;
            lp_timer->tv_sec = pi_timeout - (time(0) - begin_time);
            lp_timer->tv_usec = 0;
            if (lp_timer->tv_sec <= 0) {
                IBP_errno = IBP_E_CLIENT_TIMEOUT;
                res = NULL;
                break;
            }
        }

        if ( set_port_num(res,pi_port) != 0 ){
          fprintf(stderr,"continue");
          continue;
        }


        li_sockfd =
            socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (li_sockfd < 0)
            continue;
        if (set_socket_noblock(li_sockfd) != 0) {
            fprintf(stderr,"noblock set\n");
            close_socket(li_sockfd);
            continue;
        }
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            li_ret = connect(li_sockfd, res->ai_addr, res->ai_addrlen);
        
#ifdef HAVE_PTHREAD_H
            pthread_testcancel();
        
#endif  /*  */
            if (li_ret < 0) {
            err = _get_errno();
            if (err != EINPROGRESS && err != EWOULDBLOCK && err != SYS_OK) {
                
#ifdef IBP_DEBUG
                    fprintf(stderr,
                            "SocketConnect (), connect() failed\n\terror: %d, %s\n",
                            errno, strerror(errno));
                fprintf(stderr, "hostname:%s port: %d\n", pc_host, pi_port);
                
#endif  /*  */
                    close_socket(li_sockfd);
                continue;
            }
            FD_ZERO(&ls_wset);
            FD_SET((SOCKET_TYPE) li_sockfd, &ls_wset);
            
#ifdef HAVE_PTHREAD_H
                pthread_testcancel();
            
#endif  /*  */
                if ((li_ret =
                     select(li_sockfd + 1, NULL, &ls_wset, NULL,
                            lp_timer)) == 0) {
                
#ifdef HAVE_PTHREAD_H
                    pthread_testcancel();
                
#endif  /*  */
                    close_socket(li_sockfd);
                IBP_errno = IBP_E_CLIENT_TIMEOUT;
                res = NULL;
                break;
            }
            if (li_ret < 0) {
                close_socket(li_sockfd);
                break;
            }
            len = sizeof(val);
            if (getsockopt(li_sockfd, SOL_SOCKET, SO_ERROR, &val, &len) != 0) {
                close_socket(li_sockfd);
                continue;
            }
            if (val != 0) {
                close_socket(li_sockfd);
                continue;
            }
        }
        
            /* connect successfully */ 
            if (set_socket_block(li_sockfd) != 0) {
            close_socket(li_sockfd);
            continue;
        }
        else {
            break;
        }
    } while ((res = res->ai_next) != NULL);
    
        /*
         * 3 - check return value 
         */ 
        if (fromCache == 1) {
        release_addr_to_cache(saveres);
    }
    else {
        freeaddrinfo(saveres);
    }
  bail:
        garbage=0;        
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_pop(0);
#endif  /*  */
        if (res == NULL) {
        if (IBP_errno == IBP_OK) {
            IBP_errno = IBP_E_CONNECTION;
        }
        return (-1);
    }
    else {
        insertConnFd(pc_host,pi_port,li_sockfd);
	glbReusedConn = 0;
        return (li_sockfd);
    }
}


