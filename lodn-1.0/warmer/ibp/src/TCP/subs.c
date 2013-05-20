#include "config-ibp.h"
#ifdef STDC_HEADERS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>

#include "mylib.h"

int thePort;

/*------------------------------------------------ 
  Critical error subroutine 
  -----------------------------------------------*/
void sys_error(char *desc)
{ 
  perror(desc);
  exit(TCP_FAIL);
}

double seconds()
{
  struct timeval ru;
  gettimeofday(&ru, (struct timezone *)0);
  return(ru.tv_sec + ((double)ru.tv_usec)/1000000);
}

void closeaccept(int i)
{
   fprintf(stderr,"accept timeout occurred \n");
   exit(TCP_FAIL);
}


/*-----------------------------------------------------------------
  time_out:  receives a socket descriptor and the timeout
  value in seconds, for handling server timeout
  ----------------------------------------------------------------*/
int time_out(int sd,int sec)
{
  fd_set rset;
  struct timeval tv;
  int result;

  FD_ZERO(&rset);
  FD_SET(sd, &rset);

  tv.tv_sec = sec;
  tv.tv_usec = 0;
  
  result = select(sd+1, &rset, NULL, NULL, &tv);
  return (result);
}



/* ---------------------------------------------------------------------
   Returns a port number, given the name of the service or port number
   It checks whether the port or service given is valid, and returns the 
   port, later the callingfunction will try to connect to it or send an
   error message.
 ----------------------------------------------------------------------- */
int Port_resol(char *PortOrService,char *Protocol)
{
  int myPort;
  struct servent *port_host;
  
  myPort = atoi(PortOrService);
  if((port_host = getservbyname(PortOrService,Protocol)) == NULL)
    return(myPort);
  else
    return (port_host->s_port);
}


/* -----------------------------------------------------
   Client TCP connection returns a sd
   ----------------------------------------------------- */
int tcp_connect(char *host,char *portorservice)
{
  int connectfd;
  struct hostent *hptr;          /* Structure for temporal address & name of server */
  int port;
  int temp_addr; 
  struct sockaddr_in servaddrTCP;

  port = Port_resol(portorservice,"tcp"); 
   
  temp_addr=inet_addr(host);
  if (temp_addr==-1)
    hptr=gethostbyname(host);
  else 
    hptr=gethostbyaddr( (char *) &temp_addr,sizeof(temp_addr),AF_INET);
 
  /*-- TCP client socket --*/
  if( (connectfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
    perror("tcp_connect: client can't open stream socket");
    return -1;
  }  

  bzero(&servaddrTCP,sizeof(servaddrTCP));
  servaddrTCP.sin_family=AF_INET;
  servaddrTCP.sin_port=htons(port);
  bcopy(hptr->h_addr,(char *)& servaddrTCP.sin_addr,hptr->h_length);
 
  if(connect(connectfd,(struct sockaddr *) &servaddrTCP,sizeof(servaddrTCP))<0){
    perror("\ntcp_connect: Sorry, error trying to connect TCP socket");
    return -1;
  }

  return(connectfd);
}



/* -------------------------------------------------------------------
   Server Listening function, makes all the TCP connection
   ------------------------------------------------------------------- */
int tcp_listen(char *portorservice)
{
  int portnumber,listenfd,nl,lenaddrTCP;
  struct sockaddr_in servaddrTCP;
  int sockOpt=1;
  
  if( (listenfd = socket(AF_INET,SOCK_STREAM,0)) < 0)
    sys_error("client: can't open stream socket");
        
  if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (char *)&sockOpt, sizeof(sockOpt)) == -1)
    perror("WHAT HAPPENED?\n");

  portnumber = Port_resol(portorservice,"tcp");
  
  bzero(&servaddrTCP, sizeof(servaddrTCP));
  servaddrTCP.sin_family = AF_INET;
  servaddrTCP.sin_port = htons(portnumber);
  servaddrTCP.sin_addr.s_addr=htonl(INADDR_ANY);
 
  if(bind(listenfd,(struct sockaddr *) &servaddrTCP, sizeof(servaddrTCP))<0)   /* bind TCP */
    sys_error("\nLocal TCP server: can't bind local address");

  lenaddrTCP= sizeof(servaddrTCP);   /* get port for TCP */
  nl = getsockname(listenfd, (struct sockaddr *) &servaddrTCP, &lenaddrTCP);
 
  if(listen(listenfd,BACKLOG)<0)    /* listen TCP on port assigned */ 
    sys_error("\nListen error!");

#ifdef IBP_DEBUG  
  printf("\nLocal TCP server  listening on port:  %d\n",servaddrTCP.sin_port);
#endif
  thePort = ntohs(servaddrTCP.sin_port);
 
  return(listenfd);  /* return a port number */
}


/* ---------------------------------------------------------------
   Accept connection of an TCP client
   --------------------------------------------------------------- */
int tcp_accept(int listenfd)
{
  int acceptfd,clilen;
  struct sockaddr_in cliaddr;
  struct hostent *theClient;
        
  clilen = sizeof(cliaddr);
  signal(SIGALRM,closeaccept);
  alarm(10);
  acceptfd = accept(listenfd,(struct sockaddr *) &cliaddr,&clilen);
  alarm(0);
  theClient = gethostbyaddr((char *)&(cliaddr.sin_addr),sizeof(cliaddr),AF_INET);

  /*****fprintf(stderr,"Connecting: %s\n",theClient->h_name);*/
 
  if(acceptfd != -1){      /* return  a new socketdescriptor */
    /*//fprintf(stderr,"RETURN ACCEPTFD=%d\n", acceptfd);*/
    return(acceptfd);
  }
  else{
    perror("\ntcp_accept: Failed to create Accept Socket");
    return -1;
  }
}


/* --------------------------------------------------------------------
   IBP based read/write n bytes wrappers from/to a file descriptor
   and read a chunk of specified lenght
   -------------------------------------------------------------------- */
int sWrite(int t_fd, char *buffer,size_t size)
{
  char *t_buffer;
  int n_retval, nwritten=0;

  errno=0;
  t_buffer = buffer;
  while(nwritten < size){
    n_retval = write(t_fd, t_buffer+nwritten, size-nwritten);
    if(n_retval <= 0){
      if((errno==EINTR) || (errno == EINPROGRESS))
	continue;
      else{      
	fprintf(stderr,"Error writing\n");
	return -1;
      }
    }
    else if(n_retval > 0)
      nwritten += n_retval;
    else
      return nwritten;
  }
  return size;
}

int sRead(int fd, char *buffer, size_t size)
{
  char *t_buffer;
  int n_retval, nread=0, step; 
  int li_counter = 0;
  
  errno=0;
  t_buffer = buffer;
  while(nread < size){ 
    n_retval = read(fd, t_buffer+nread, size-nread);
    if(n_retval <= 0){ 
       if((errno==EINTR) || (errno == EINPROGRESS))
       {
          continue;
       }
       else if (errno == 0){
             li_counter++;
	     if(li_counter == 10) {
		 return -1;
	      }
	}
	else
	{
            perror("Error Reading: ");
            return -1;
	}
    }
    else if(n_retval > 0){
      nread += n_retval;
      for(step=(n_retval -1); step>=0; step--)
	if((*(t_buffer+step)=='\0') /**|| (*(t_buffer+step)=='\n')**/)
	  return nread;
    }
    else
      return nread;
  }
  return size;
}

int sReadLine(int fd, char *buff, size_t len)
{
  int n=0, rc=0;
  char *ptr;

  n = readn(fd, buff, len);
  if(n<0)
    return -1;
  
  ptr = buff;
  while((*ptr++ != '\n') && (rc++ < n-1));
  ptr = '\0';
  return n;
}


/* -------------------------------------------------------------
   UNP book based read/write n bytes from a file descriptor and
   read a line of specified lenght
   ------------------------------------------------------------- */
int readn(int fd, void *buffptr, size_t n)
{
  size_t nleft;
  int nread;
  char *ptr;

  ptr = buffptr;
  nleft = n;
  while(nleft > 0){
    if((nread = read(fd,ptr,nleft)) < 0){
      if(errno == EINTR)
	nread = 0;
      else
	return(-1);
    }
    else if(nread == 0)
      break;
    nleft -= nread;
    ptr += nread;
  }
  return(n-nleft);
}

int writen(int fd, const void *buffptr, size_t n)
{
  size_t nleft;
  int nwritten;
  const char *ptr;

  ptr = buffptr;
  nleft = n;
  while(nleft > 0){
    if((nwritten = write(fd,ptr,nleft)) < 0){
      if(errno == EINTR)
	nwritten = 0;
      else
	return(-1);
    }
    nleft -= nwritten;
    ptr += nwritten;
  }
  return(n);
}

int readline(int fd, void *buffptr, size_t len)
{
  ssize_t n, rc;
  char c, *ptr;

  ptr = buffptr;
  for(n=1; n<len; n++){
  again:
    if( (rc = read(fd,&c,1))==1){
      *ptr++ = c;
      if(c=='\n')
	break;
    }
    else if(rc==0){
      if(n==1)
	return(0);
      else
	break;
    }
    else{
      if(errno == EINTR)
	goto again;
      return(-1);
    }
  }
  *ptr = 0;
  return(n);
}


/* ----------------------------------------------------------------
   CS-UNP based read/write wrappers from/to a file descriptor, and
   read a chunk of specified lenght
   ---------------------------------------------------------------- */
int Readn(int fd, void *ptr, int nbytes)
{
  int nleft, nread;
  char *cptr = ptr;
  
  nleft = nbytes;
  while (nleft > 0){
    if((nread = read(fd, cptr, nleft))<0){ 
      if(errno == EINTR)
	nread = 0;
      else
	return(-1);
    }
    else if (nread ==0)
      break;
    nleft -= nread;
    cptr += nread;
  }
  return(nbytes - nleft);
}

int Writen(int fd, void *ptr, int nbytes)
{
  int nleft,nwritten;
  char *cptr = ptr;                  
  nleft = nbytes;
  
  while (nleft > 0){
    if( (nwritten = write(fd, cptr, nleft)) <=0){
      if(errno == EINTR)
        nwritten = 0;
      else
        return(-1);
    }
    nleft -= nwritten;
    cptr += nwritten;
  }        
  return (nbytes-nleft);
}


int Readline(int fd, char *ptr, int maxlen)
{
  int n, rc;
  char c; 
                   
  for (n=1; n<maxlen; n++){
again:
    if ((rc=read(fd,&c,1))==1){
      *ptr++=c;
      if(c=='\n')  break;
    }
    else if (rc==0){       
      if(n==1) return(0);
      else  break;
    }
    else{
      if(errno==EINTR) goto again;
      return(-1);
    }
  }
  *ptr = 0;
  return(n);
}
 

/* ----------------------------------------------------------
   Split capability to get the parameters for data movers
   ---------------------------------------------------------- */
char* GetTargetKey(char *capstring)
{
  char *key;
  int i=6, j=0;

  key = (char *)malloc(sizeof(char)*MAX_KEY_LEN);
  while(capstring[i] != ':') i++;
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/'){
    key[j] = capstring[i]; 
    i++; j++;
  }
  key[j] = '\0';
  return key;
}

char* GetTargetHost(char *capstring)
{
  char *host;
  int i=6,j=0;

  host = (char *)malloc(sizeof(char)*MAX_HOST_LEN);
  while(capstring[i] != ':'){
    host[j] = capstring[i];
    i++; j++;
  }
  host[j] = '\0';
  return host;
}

int GetTargetPort(char *capstring)
{
  char temp[6];
  int i=6,j=0;

  while(capstring[i] != ':') i++;
  i++;
  while(capstring[i] != '/'){
    temp[j] = capstring[i];
    i++; j++;
  }
  temp[j] = '\0';  
  return atoi(temp);
}

int GetTypeKey(char *capstring)
{
  char temp[6];
  int i=6,j=0;
  
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/'){
    temp[j] = capstring[i];
    i++; j++;
  }
  temp[j] = '\0';    
  return atoi(temp);
}


