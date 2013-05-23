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
    
# ifndef IBP_WIN32
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
# else
# include <winsock2.h>
# endif
    
# include <stdio.h>
# include <string.h>
    
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
    __declspec(thread)
     DNSCACHE *glbCache = NULL;

# else /* Unix like OSs */
    
# define SOCKET_TYPE	int
# define SYS_OK			0
# define INVALID_SOCKET	-1
# define cleanup_socket cleanup_socket_foo  
# define _close_socket	close 
    
#ifdef HAVE_PTHREAD_H
# include <pthread.h>
    pthread_once_t cacheInit = PTHREAD_ONCE_INIT;
    pthread_once_t ghbnInit = PTHREAD_ONCE_INIT;
    pthread_mutex_t *ghbnLock ; 
#endif  /* HAVE_PTHREAD_H */
    DNSCACHE * glbCache = NULL;

void cleanup_socket_foo()
{
}


# endif
    
# define MAX_READ_SIZE  (~(1<<((sizeof(int) * 8) -1 )))
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
void close_socket(int pi_fd)
{
    _close_socket(pi_fd);
    cleanup_socket();
}


# ifndef IBP_WIN32
int set_socket_noblock_unix(int pi_fd)
{
    int flags;
    if ((flags = fcntl(pi_fd, F_GETFL, 0)) < 0)
        return (-1);
    if ((flags = fcntl(pi_fd, F_SETFL, flags | O_NONBLOCK)) < 0)
        return (-1);
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
static void _initCacheData_unix(void)
{
    DNSCACHE * cache;
    int i;
    if ((cache = (DNSCACHE *) calloc(1, sizeof(DNSCACHE))) == NULL) {
        return;
    }
    
#ifdef HAVE_PTHREAD_H
        if ((cache->lock =
             (pthread_mutex_t *) calloc(1,
                                        sizeof(pthread_mutex_t))) == NULL) {
        free(cache);
        return;
    }
    pthread_mutex_init(cache->lock, NULL);
    
#endif  /*  */
        for (i = 0; i < MAX_CACHE_ADDR; i++) {
        cache->addrs[i].addr = NULL;
        cache->addrs[i].ref = 0;
        cache->addrs[i].lastUsed = 0;
        cache->addrs[i].hostName[0] = '\0';
    }
    glbCache = cache;
    return;
}
void initCache_unix()
{
    if (glbCache == NULL) {
        
#ifdef HAVE_PTHREAD_H
            pthread_once(&cacheInit, _initCacheData_unix);
        
#else   /*  */
            _initCacheData_unix();
        
#endif  /*  */
    }
}

#ifdef HAVE_PTHREAD_H
static void _initGHBNLock_unix(void){
    pthread_mutex_t *lock;
    if ( (lock = (pthread_mutex_t *)calloc(1,sizeof(pthread_mutex_t))) == NULL){
        return;
    }
    pthread_mutex_init(lock,NULL);
    ghbnLock = lock;
    return;
}
#endif

void initGHBNLock_unix()
{
#ifdef HAVE_PTHREAD_H
   if ( ghbnLock == NULL ){
       pthread_once(&ghbnInit,_initGHBNLock_unix);
   }
#endif
}


struct addrinfo *search_dns_cache_unix(char *host)
{
    int i;
    struct addrinfo *addr = NULL;
    initCache_unix();
    if (glbCache == NULL) {
        return (NULL);
    }
    
#ifdef HAVE_PTHREAD_H
        pthread_mutex_lock(glbCache->lock);
    
#endif  /*  */
        for (i = 0; i < MAX_CACHE_ADDR; i++) {
        
            /*fprintf(stderr,"dst:%s :: src:%s\n",glbCache->addrs[i].hostName,host);*/
            if (glbCache->addrs[i].addr == NULL) {
            continue;
        }
        if (strcasecmp(glbCache->addrs[i].hostName, host) == 0) {
            addr = glbCache->addrs[i].addr;
            glbCache->addrs[i].lastUsed = time(NULL);
            glbCache->addrs[i].ref++;
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

int insert_addr_to_cache_unix(char *host, struct addrinfo *addr)
{
    int idx = -1;
    int i;
    time_t  oldestTime = 0 ;
    initCache_unix();
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
                if ( oldestTime == 0 ){
                    oldestTime = glbCache->addrs[i].lastUsed;
                    idx = i;
                }else if ( oldestTime > glbCache->addrs[i].lastUsed ){
                    oldestTime = glbCache->addrs[i].lastUsed;
                    idx = i;
                }
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
void release_addr_to_cache_unix(struct addrinfo *addr)
{
    int i;
    initCache_unix();
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


# endif
    
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
void initCache_win(void)
{
    DNSCACHE * cache;
    int i;
    if ((cache = (DNSCACHE *) calloc(1, sizeof(DNSCACHE))) == NULL) {
        return;
    }
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        cache->addrs[i].addr = NULL;
        cache->addrs[i].ref = 0;
        cache->addrs[i].lastUsed = 0;
        cache->addrs[i].hostName[0] = '\0';
    }
    glbCache = cache;
    return;
}
void initGHBNLock_win(void)
{
}

struct addrinfo *search_dns_cache_win(char *host)
{
    int i;
    struct addrinfo *addr = NULL;
    initCache_win();
    if (glbCache == NULL) {
        return (NULL);
    }
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        
            /*fprintf(stderr,"dst:%s :: src:%s\n",glbCache->addrs[i].hostName,host);*/
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
    return (addr);
}
int insert_addr_to_cache_win(char *host, struct addrinfo *addr)
{
    int idx = -1;
    int i;
    time_t oldestTime = 0;
    initCache_win();
    if (glbCache == NULL) {
        return (idx);
    }
    for (i = 0; i < MAX_CACHE_ADDR; i++) {
        if (glbCache->addrs[i].addr == NULL) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        for (i = 0; i < MAX_CACHE_ADDR; i++) {
            if (glbCache->addrs[i].ref <= 0) {
                if ( oldestTime == 0 ){
                    oldestTime = glbCache->addrs[i].lastUsed;
                    idx = i;
                }else if (oldestTime > glbCache->addrs[i].lastUsed){
                    oldestTime = glbCache->addrs[i].lastUsed;
                    idx = i;
                }
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
    return (idx);
}
void release_addr_to_cache_win(struct addrinfo *addr)
{
    int i;
    initCache_win();
    if (glbCache == NULL) {
        return;
    }
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
    return;
}


#endif  /*  */
void _close_sockfd(void *fd)
{
    int i;
    i = *((int *) fd);
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
    
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_push(_close_sockfd, (void *) &(pi_fd));
    
#endif  /*  */
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
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_pop(0);
    
#endif  /*  */
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
    
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_push(_close_sockfd, (void *) &(pi_fd));
    
#endif  /*  */
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
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_pop(0);
    
#endif  /*  */
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
    
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_push(_close_sockfd, &(pi_fd));
    
#endif  /*  */
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
                if ((err == SYS_OK) || (err == EINTR) || (err == EWOULDBLOCK)
                     || (err == EINPROGRESS))
                    continue;
                
                else {
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
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_pop(0);
    
#endif  /*  */
        return (ll_nwrite);
}
extern struct hostent *gethostname_re(const char *host,
                                        struct hostent *hostbuf,
                                        char **tmphstbuf, size_t * hstbuflen);
int connect_socket(char *pc_host, int pi_port, int pi_timeout)
{
    struct hostent *ls_hoste, host_buf;
    char *tmphstbuf = NULL;
    size_t hstbuflen = 0;
    int li_sockfd = -1, li_ret = IBP_OK;
    fd_set ls_wset;
    struct timeval lt_timer = { 0, 0 }, *lp_timer;
    int err;
    time_t begin_time;
    int val, len;
    int addr_type;
    int ret;
    struct addrinfo hints, *res = NULL, *saveres;
    char serv[30];
    int fromCache = 0;
    
#ifdef HAVE_PTHREAD_H
        pthread_cleanup_push(_close_sockfd, &(li_sockfd));
    
#endif  /*  */
        
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
    
        /*
           if ( ( ret = getaddrinfo(pc_host,serv,&hints,&res)) != 0 ) {
           #ifdef IBP_DEBUG
           fprintf(stderr,"SocketConnect(): getaddrinfo() failed\n\t");
           #endif 
           IBP_errno = IBP_E_CONNECTION;
           cleanup_socket();
           li_sockfd = -1;
           goto bail;
           }
         */ 
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
        li_sockfd =
            socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (li_sockfd < 0)
            continue;
        if (set_socket_noblock(li_sockfd) != 0) {
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
        return (li_sockfd);
    }
}


