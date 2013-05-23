/***************************************************************
  mylib.h:  header file containing the libraries and prototypes 
            for the TCP connection and read/write functions for 
	    TCP based data movers
*****************************************************************/
#ifndef _MYLIB_H
#define _MYLIB_H

#include "config-ibp.h"
#ifdef STDC_HEADERS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>   
#include <sys/types.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <signal.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h> 
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

/**#include <sys/ddi.h>*/

#define MAXBUFFLEN    (4096*16)  /*4096*/
#define MAX_HOST_LEN  64
#define MAX_WORD_LEN  8
#define MAX_FILE_NAME 256
#define MAX_KEY_LEN   256
#define MAX_CAP_LEN   2048
#define BUFFSIZE      1024
#define MAXLINE       4096
#define BACKLOG       5

#define TCP_FAIL      3
#define TCP_OK        0

void sys_error(char *desc); 
double seconds();
int time_out(int sd, int sec);
int Port_resol(char *PortOrService, char *Protocol);
int tcp_connect(char *host, char *portorservice);
int tcp_listen(char *portorservice);
int tcp_accept(int listenfd);

char* GetTargetKey(char *capstring);
char* GetTargetHost(char *capstring);
int GetTargetPort(char *capstring);
int GetTypeKey(char *capstring);

/***int Write(int t_fd, char *buffer, size_t size);        
int Read(int fd, char *buffer, size_t size);           
int ReadLine(int fd, char *buff, size_t len); ***/   

int sWrite(int t_fd, char *buffer, size_t size);     /* use with IBP_store */
int sRead(int fd, char *buffer, size_t size);        /* use with IBP_load */
int sReadLine(int fd, char *buff, size_t len); 

int writen(int fd, const void *buffptr, size_t n);   /* based on UNP book */
int readn(int fd, void *buffptr, size_t n);          /* based on UNP book */
int readline(int fd, void *buffptr, size_t len);     /* based on UNP book */

int Readn(int fd, void *ptr, int nbytes);            /* based on CS-UNP */
int Writen(int fd, void *ptr, int nbytes);           /* based on CS-UNP */
int Readline(int fd, char *ptr, int maxlen);         /* based on CS-UNP */

#endif


