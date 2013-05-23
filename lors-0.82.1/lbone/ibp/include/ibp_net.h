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
    
/*
 * ibp_net.h
 *
 * Definition of network-related APIs
 *
 */ 
    
# ifndef _IBP_NET_H
# define _IBP_NET_H
    
# include "config-ibp.h"
# include "ibp_os.h"
    
# ifndef IBP_WIN32
# include <sys/time.h>
# else
# include <time.h>
# endif
# ifdef HAVE_PTHREAD_H
# include <pthread.h>
# endif
    
# include "ibp_errno.h"
    
# ifdef __cplusplus 
extern "C"
{
# endif /* __cplusplus */

#define HAVE_STRU_ADDRINFO  1
#define HAVE_FUNC_GETADDRINFO 1
#define HAVE_FUNC_FREEADDRINFO 1
#define IS_OSF_OS 0
#if !HAVE_STRU_ADDRINFO  /* if no addrinfo is defined, define it */
struct addrinfo {
    int     ai_flags;
    int     ai_family;
    int     ai_socktype;
    int     ai_protocol;
    size_t  ai_addrlen;
    char    *ai_canonname;
    struct sockaddr *ai_addr;
    struct addrinfo *ai_next;
};
#endif

#ifndef AI_PASSIVE
#define AI_PASSIVE 0x00000001
#endif

#if !HAVE_FUNC_GETADDRINFO /* if getaddrinfo is not available, define it */
int getaddrinfo(const char *nodename, const char *servname, 
                const struct addrinfo *hints, struct addrinfo **res);
#endif

#if !HAVE_FUNC_FREEADDRINFO
void freeaddrinfo(struct addrinfo *ai);
#endif

typedef struct
{
    char hostName[256];
    struct addrinfo *addr;
    int ref;
    time_t lastUsed;
}
CACHEADDR;
         
#define MAX_CACHE_ADDR 512 
typedef struct
{
#ifndef IBP_WIN32
#ifdef HAVE_PTHREAD_H
   pthread_mutex_t * lock;
#endif                          /*  */
#else                           /*  */
#endif                          /*  */
    CACHEADDR addrs[MAX_CACHE_ADDR];
}
DNSCACHE;
        
# ifndef IBP_WIN32
int set_socket_noblock_unix(int pi_fd);
int set_socket_block_unix(int pi_fd);
void init_dns_cache_unix();
struct addrinfo *search_dns_cache_unix(char *host);
int insert_addr_to_cache_unix(char *host, struct addrinfo *addr);
void release_addr_to_cache_unix(struct addrinfo *addr);
void initCache_unix(void);
void initGHBNLock_unix(void);
        
# define set_socket_noblock set_socket_noblock_unix
# define set_socket_block	set_socket_block_unix
# define search_dns_cache search_dns_cache_unix
# define insert_addr_to_cache insert_addr_to_cache_unix
# define release_addr_to_cache release_addr_to_cache_unix
# define initCache initCache_unix
# define initGHBNLock initGHBNLock_unix 
# else
int set_socket_noblock_win(int pi_fd);
int set_socket_block_win(int pi_fd);
void initCache_win(void);
struct addrinfo *search_dns_cache_unix(char *host);
int insert_addr_to_cache_unix(char *host, struct addrinfo *addr);
void release_addr_to_cache_unix(struct addrinfo *addr);
void initGHBNLock_win(void);
            
# define set_socket_noblock	set_socket_noblock_win
# define set_socket_block	set_socket_block_win
# define search_dns_cache search_dns_cache_win
# define insert_addr_to_cache insert_addr_to_cache_win
# define release_addr_to_cache release_addr_to_cache_win
# define initCache initCache_win
# define initGHBNLock initGHBNLock_win 
# endif
int connect_socket(char *ps_host, int pi_port, int pi_timeout);
void close_socket(int pi_fd);
unsigned long int read_fix_data(int pi_fd,
                                unsigned long int pl_size,
                                char *pc_buf, time_t pt_expireTime);
unsigned long int read_var_data(int pi_fd,
                                unsigned long int pl_size,
                                char *pc, time_t pt_expireTime);
unsigned long int write_data(int pi_fd, unsigned long int pl_size,
                            char *pc_buf, time_t pt_expireTime);
void set_time_val(struct timeval *ps_timeval, time_t pi_expireTime);
void _clear_buf(void *pc_buf, unsigned int long pl_size);
      
# ifdef __cplusplus
}
      
# endif /* __cplusplus */
      
# endif /* _IBP_NET_H */
      
