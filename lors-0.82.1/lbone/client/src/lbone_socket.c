/*
 *              LBONE version 1.0:  Logistical Backbone
 *               University of Tennessee, Knoxville TN.
 *        Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 2001 All Rights Reserved
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
 * lbone_socket.c
 *
 * LBONE socket functions. Version name: alpha
 *
 * Several functions borrowed from ibp-lib.c
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
# include "lbone_socket.h"

extern int errno;


/*
 * getfullhostname borrowed from ibp-lib.c
 */

char *get_full_hostname (char *pc_name)
{

  static char       lc_fullHostName[256];
  struct hostent    *ls_he;

  ls_he = gethostbyname(pc_name);
  if(ls_he == NULL)
    return NULL;
  memset(lc_fullHostName, 0, 256);
  if (strlen(ls_he->h_name) < 256) {
    strncpy(lc_fullHostName, ls_he->h_name, strlen(ls_he->h_name));
  } else {
    return NULL;
  }
  return lc_fullHostName;
}



/*
 * getlocalhostname borrowed from ibp-lib.c
 */

char *get_local_hostname ()
{

  struct utsname    ls_localHost;
  if (uname(&ls_localHost) == -1)
    return NULL;

  return get_full_hostname(ls_localHost.nodename);
}


/*
 * serve_socket
 */

int serve_socket(char *hostName, int port)
{
  struct sockaddr_in   addr;
  socklen_t            addr_length;
  int                  sockId;
  struct hostent       *he;
  int                  sockOpt  = 1;

  he = gethostbyname(hostName);
  if (he == NULL ){
     perror("serve_known_socket() gethostbyname()");
     fprintf(stderr,"Host name: %s\n", hostName);
     return -1;
  }

  sockId = socket(AF_INET, SOCK_STREAM, 0);
  if (sockId == -1) {
     perror("serve_known_socket() socket()");
     fprintf(stderr,"Host name: %s\n", hostName);
     return -1;
  }

  /*
   * We originally thought this affected reuse of file descriptors.
   * Apparently, it allows the server to bind to the same port after
   * a restart (crash). Normally, the bind call would keep the port
   * tied up for a few minutes and might prevent the server from using
   * the correct port on re-boot.
   */
  if (setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, (char*)(&sockOpt), 
                 sizeof(int)) == -1) {
     perror("serve_known_socket() setsockopt()");
     fprintf(stderr,"Host name: %s\n", hostName);
     return -1;
  }
 
  memset(&addr, 0, sizeof(addr));  
  addr.sin_port = htons((short) port);
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sockId, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
     perror("serve_known_socket() bind()");
     fprintf(stderr,"Host name: %s\n", hostName);
     close(sockId);
     return -1;
  }
  
  addr_length = sizeof(addr);
  if ((getsockname(sockId, (struct sockaddr *)&addr, &addr_length)) == -1){
     perror("serve_known_socket() getsockname()");
     fprintf(stderr,"Host name: %s\n", hostName);
     close(sockId);
     return -1;
  }

  if (listen(sockId, LBONE_CLIENTS_WAITING) == -1) {
     perror("serve_known_socket() listen()");
     fprintf(stderr,"Host name: %s\n", hostName);
     close(sockId);
     return -1;
  }
  return sockId;
}




/*
 * request_connection modified from ibp-lib.c
 */

int request_connection (char *hostname, int port)
{

  struct hostent        *he;
  struct sockaddr_in    server_addr4;
# ifdef AF_INET6
  struct sockaddr_in6   server_addr6;
# endif
  int                   li_sockfd, li_ret;

  /*
   * 1 - get host by name and fill in the appropriate IP address record
   */

  he = gethostbyname(hostname);
  if (he == NULL) {
#ifdef LBONE_DEBUG
    fprintf(stderr, "request_connection(), gethostbyname () failed\n\terror: %d, %s\n", errno, strerror(errno));
#endif
    return -1;
  }

  switch (he->h_addrtype) {
  case AF_INET:
    server_addr4.sin_family = AF_INET;
    server_addr4.sin_port = htons( (short) port);
    server_addr4.sin_addr.s_addr = *((u_long *) he->h_addr_list [0]);
  
    break;
# ifdef AF_INET6
  case AF_INET6:
    server_addr6.sin6_family = AF_INET6;
    server_addr6.sin6_port = htons( (short) port);
    /*    server_addr6.sin6_addr.s6_addr =  */

    break;
# endif
  default:
    break;
  }

  /*
   * 2 - socket() & file control()
   */

  if ( (li_sockfd = socket(he->h_addrtype, SOCK_STREAM, 0)) == -1) {
    perror("socket failed");
# ifdef LBONE_DEBUG
    fprintf(stderr, "request_connection(), socket() failed\n\terror: %d, %s\n", errno, strerror (errno));
# endif
    return -1;
  }

  /*
   * 3 - Connect - switch needed for different IP protocols ...
   */

  switch (he->h_addrtype) {
  case AF_INET:
    li_ret = connect (li_sockfd, (struct sockaddr *) &server_addr4, sizeof (server_addr4)); 
    break;
# ifdef AF_INET6
  case AF_INET6:
    li_ret = connect (li_sockfd, (struct sockaddr *) &server_addr6, sizeof (server_addr6)); 
    break;
# endif
  default:
    break;
  }

  if (li_ret < 0) {
    perror("connect failed");
    return -1;
  }

  return li_sockfd;
}



int accept_connection(int socket_fd) {
  socklen_t l;
  struct sockaddr_in addr;
  int x;

  addr.sin_family = AF_INET;

  if (listen(socket_fd, 1) == -1) {
    perror("listen()");
    exit(1);
  }

  l = sizeof(addr);
  if ((x = accept(socket_fd, (struct sockaddr*)&addr, &l)) == -1) {
    perror("accept()");
    exit(1);
  }
  return x;
}
