/**********************************************************************
 UDP File Blaster headers, function prototypes for TCP connections 
 and data transfer
 ********************************************************************** */
#ifndef _LIBUDP_H
#define _LIBUDP_H

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

#define MAXBUF (4096) 
#define HDR 10
#define HDR2 10                         /* added to include sequence # */
#define MAXLOAD (MAXBUF - HDR - HDR2)   /* HDR2 added to include sequence # */

#define WINDOW_UP .01	/* add to window on each good ack */
#define WINDOW_DOWN .9  /* multiply window by on each kick */
#define MINWIN 8       /* vary this parameter */
#define RANGE 100

#define MAXBUFFLEN    4096
#define MAX_HOST_LEN  64
#define MAX_WORD_LEN  8
#define MAX_KEY_LEN   256
#define MAX_CAP_LEN   2048
#define BUFFSIZE      1024
#define MAXLINE       4096

#define BACKLOG 6

/* -----------------------------------------
   blaster.c  variables
   ----------------------------------------- */
char abuf[MAXBUF];          /* buffer to hold ack  */

int nextack;                /* next packet expected */
/*int loopcount=0; */

unsigned long length;       /* length of address ---> for blasterd.c */
socklen_t length1;      /* length of address ---> new for blaster.c */

/* ------------------------------------
   my generic TCP subs
   ------------------------------------ */
int time_out(int sd, int sec);
int Port_resol(char *PortOrService, char *Protocol);
int tcp_connect(char *host, char *portorservice);
int tcp_listen(char *portorservice);
int tcp_accept(int listenfd);
void sys_error(char *desc);
double seconds(void);

/* -------------------------------------
   UDP blaster subs
   ------------------------------------- */
void kick(int i);
int ReceiveDataUDP(int sourcefd, 
		   int targetfd, 
		   unsigned long int size, 
		   int count);
void wake(int j);
int SendDataUDP(int srcfd, 
		int targfd, 
		unsigned long int size, 
		int count, 
		float initwindow);

int SendDataUDPT(int srcfd, 
		int targfd, 
		unsigned long int size, 
		int count, 
		float initwindow,
		int num);
char* GetTargetKey(char *capstring);
char* GetTargetHost(char *capstring);
int GetTargetPort(char *capstring);
int GetTypeKey(char *capstring);

#endif
