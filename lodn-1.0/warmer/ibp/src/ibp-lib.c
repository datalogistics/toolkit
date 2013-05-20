/*
 *         IBP version 1.0:  Internet BackPlane System
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
 * ibp-lib.c  
 *
 * IBP utilities functions. Version name: primeur
 *
 */

# include "config-ibp.h"
# include "ibp-lib.h"
# include "ibp_thread.h"
# include "ibp_net.h"
# ifdef HAVE_PTHREAD_H
# include<pthread.h>
# endif
# include <stdlib.h>
# include <unistd.h>
# include "ibp_resources.h"

/*
 * General lib
 */

/*****************************/
/* trim                      */
/* It converts a string to standard string      */
/* return -- the new string   */
/*****************************/
char *trim( char *str){
	int i=0;
	int j=0;
	int len = 0;

	if ( str == NULL )
		return (NULL);	
	len = strlen(str)-1;
	while (len >= 0){
		if (str[len] == ' '){
			str[len] = '\0';
			len--;
		}else {
			break;
		}	
	}
	while ( (str[i] != '\0') && (str[i] == ' ')){
		i++;
	}
	if ( str[i] == '\0' )
		return(str);
	while ( str[i] != '\0' ) {
		str[j] = str[i];
		j++;
		i++;
	}
	str[j] = '\0';
	
	return(str);	
	
}

int ibpStrToInt( char *str, int *ret ){
    if ( 1 !=  sscanf(str,"%d",ret) ){
        return -1;
    }
    return 0;
}

/*****************************/
/* Write                      */
/* It writes a buffer into a file or socket connection      */
/* return -- written bytes number   */
/*****************************/
int Write (int pi_fd, char *pc_buf, size_t pi_size)
{
  char        *lc_ptr = pc_buf;
  int         li_n, li_written = 0;

  errno = 0;
  while (li_written < pi_size) {
    li_n = write (pi_fd, lc_ptr + li_written, pi_size - li_written);
    if (li_n <= 0) {
      if ((errno == EINTR) || (errno == EINPROGRESS)) 
	continue;
      else {
#ifdef IBP_DEBUG
	fprintf (stderr, "Write() failed\n\terror: %d, %s\n", errno, strerror (errno));
#endif
	return -1;
      }
    }
    else if (li_n > 0) 
      li_written += li_n;
    else
      return li_written;
  }
  return pi_size;
}

/*****************************/
/* Read                      */
/* It reads data from a file or socket connection      */
/* return -- read bytes number   */
/*****************************/
long Read (int pi_fd, char *pc_buf, size_t pi_size)
{
  char *lc_ptr = pc_buf;
  int li_n, li_read = 0;
  int li_counter = 0;
 
  errno = 0;
  while (li_read < pi_size) {
    li_n = read (pi_fd, lc_ptr + li_read, pi_size - li_read); 
    if (li_n <= 0) {
      if ((errno == EINTR) || (errno == EINPROGRESS)) 
	continue;
      else if (errno == 0) {
	li_counter++;
	if (li_counter == 10) {
#ifdef IBP_DEBUG
	  fprintf (stderr, "Read() failed\n\terror: %d, %s\n", errno,strerror(errno));
	  fprintf (stderr, "\tKeep reading 0 bytes\n");
#endif
	  return -1;
	}
      }
      else {
#ifdef IBP_DEBUG
	fprintf (stderr, "Read() failed\n\terror: %d, %s\n", errno, strerror (errno));
#endif
	return -1;
      }
    }
    else if (li_n > 0) {
      /*li_read += li_n;*/
#if 0
      for (li_inv = (li_n - 1); li_inv >= 0; li_inv--) {
	      if ((*(lc_ptr +li_read  + li_inv) == '\0') || (*(lc_ptr + li_read + li_inv) == '\n')){
	        return li_read + li_inv + 1;
        }
      }
#endif
      li_read += li_n;
    }
    else 
      return li_read;
 
  }
  return pi_size;
}

static int read_n_byte(int pi_fd, char *buf, int nbytes)
{
    char *ptr = buf;
    int n = 0,ret;

    if ( nbytes <= 0 ) {
        return 0;
    }

    while ( n < nbytes){
       ret = recv(pi_fd,ptr,nbytes-n,0);
       if ( ret <= 0 ){
        if ((errno == EINTR) || (errno == EINPROGRESS)){
            continue;
        }
        return(-1);
       }else {
          ptr += ret;
          n += ret;
       }
    }

    return (n);
}

/*****************************/
/* ReadComm                  */
/* It reads data from a file or socket connection      */
/* return -- read bytes number   */
/*****************************/
long ReadComm (int pi_fd, char *pc_buf, size_t pi_size)
{
  char *lc_ptr = pc_buf;
  char *p1 = NULL, *p2 = NULL;
  int li_n, li_read = 0;
  int left = pi_size;
  int li_counter = 0;
  int n_read;

  bzero(pc_buf,pi_size);
  errno = 0;
  while (/*******(*(lc_ptr+li_read) != '\n') &&****/ (li_read < pi_size)) 
{ 
    li_n = recv(pi_fd,lc_ptr,left,MSG_PEEK);
    if (li_n < 0) {
      if ((errno == EINTR) || (errno == EINPROGRESS)) 
        continue;
      else if (errno == 0) {
        li_counter++;
        if (li_counter == 10) {
#ifdef IBP_DEBUG
          fprintf (stderr, "Read() failed\n\terror: %d, %s\n", 
errno,strerror(errno));
          fprintf (stderr, "\tKeep reading 0 bytes\n");
#endif
          return -1;
        }
      }
      else if ( li_n == 0 ){
        
      }else {
#ifdef IBP_DEBUG
        fprintf (stderr, "ReadComm() failed\n\terror: %d, %s\n", errno, 
strerror (errno));
#endif
        return -1;
      }
    }
    else{ 
      if (  li_n == 0 ){
        return(-1); 
      }
      *(lc_ptr + li_n) = '\0';
      p1 = strchr(lc_ptr,10);
      p2 = strchr(lc_ptr,13);
      if ( p1 != NULL || p2 != NULL ){
            if ( p2 > p1 ) { 
                p1 = p2;          
            }
            n_read = p1 - lc_ptr + 1;
      }else {
            n_read = li_n;
      }
      if ( n_read != read_n_byte(pi_fd,lc_ptr,n_read)){
         return (-1);
      }
      li_read += n_read;
      if(*(pc_buf+n_read-1) == 10 || *(pc_buf+n_read-1) == 13){ 
        *(pc_buf+n_read) = '\0'; 
        break;
      }
      lc_ptr = lc_ptr + n_read;
      left -= n_read;
    }
  }

  strcat(lc_ptr,"\0");
  return li_read;
}

/*****************************/
/* ReadLine                      */
/* It reads a line from a file or socket connection      */
/* return -- read bytes number   */
/*****************************/
int ReadLine (int pi_fd, char *pc_buf, size_t pi_size)
{
 
  int li_read = 0;
  int  n;

  while ( li_read < pi_size -1 ){
    n = Read (pi_fd, pc_buf+li_read, 1);
    if (n < 0)
      return -1;
    if ( *(pc_buf + li_read) == '\n' ){
      *(pc_buf+li_read+1) = '\0';
      return ( li_read + 1 );
    }
    li_read ++;
  }

  *(pc_buf + li_read) = '\0';

  return (li_read);

#if 0
  lc_ptr = pc_buf;
  while ((*lc_ptr++ != '\n') && (li_count++ < li_read - 1));
  *lc_ptr = '\0';

  return li_read;
#endif

}

/*****************************/
/* getfullhostname           */
/* It gets host's info by host name  */
/* return -- info of host   */
/*****************************/
char *getfullhostname (char *pc_name) {

  static char       lc_fullHostName[256];
  struct hostent    *ls_he;

  ls_he = gethostbyname(pc_name);
  if(ls_he == NULL)
    return NULL;
/*
  ls_he = gethostbyaddr(ls_he->h_addr, ls_he->h_length, ls_he->h_addrtype);
  if(ls_he == NULL)
    return NULL;
*/
  strcpy(lc_fullHostName, ls_he->h_name);
  return lc_fullHostName;
}

/*****************************/
/* getlocalhostname           */
/* It gets local host's info by host name  */
/* return -- info of host   */
/*****************************/
char *getlocalhostname () {

  struct utsname    ls_localHost;
  if (uname(&ls_localHost) == -1)
    return NULL;

  return getfullhostname(ls_localHost.nodename);
}

/*****************************/
/* Socket2File               */
/* It reads data from the socket and writes it to a file      */
/* return -- IBP_OK or error number                */
/*****************************/
int Socket2File (int sockfd, FILE *fp, ulong_t *size, time_t pl_expireTime) {
  long     n1, n2, readCount, writeCount;
/*  char     dataBuf[DATA_BUF_SIZE]; */
  char     *dataBuf;
  long     readChunk;
  char     *ptr;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_rd_set;
  int               li_return;

  dataBuf = (char *) malloc (DATA_BUF_SIZE);
  if (dataBuf == NULL)
  {
    IBP_errno = IBP_E_GENERIC;
    return(IBP_errno);
  }

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_rd_set);
  FD_SET(sockfd,&ls_rd_set);

  n1 = readCount = 0;
  while (readCount < *size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
        free(dataBuf);
        IBP_errno = IBP_E_SERVER_TIMEOUT;
        return(IBP_errno);
      }else{
        ls_tv->tv_sec = pl_expireTime - time(0);
      }
    }
    li_return = select(sockfd+1,&ls_rd_set,NULL,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        free(dataBuf);
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    readChunk = IBP_MIN(DATA_BUF_SIZE, *size - readCount);
    n1 = Read (sockfd, dataBuf, readChunk);
    if (n1 < 0){
      free(dataBuf);
      return IBP_E_SOCK_READ;
    }
    writeCount = 0; 
    ptr = dataBuf;
    while (writeCount < n1) {
      if ( (n2 = fwrite(ptr, 1, n1 - writeCount, fp)) < 0){
        free(dataBuf);
	      return IBP_E_FILE_WRITE;
      }
      ptr += n2;
      writeCount += n2;
    }
    readCount += n1;
  }
  
  *size = readCount;
  free(dataBuf);
  return IBP_OK;
}


/*****************************/
/* Socket2Queue              */
/* It reads data from the socket and writes it to a CQ file      */
/* return -- IBP_OK or error number                */
/*****************************/
int Socket2Queue (int pi_sockfd, 
                  FILE *pf_fp,
                  ulong_t pl_size,
                  ulong_t pl_fifoSize,
		  ulong_t pl_wrcount,
		  ulong_t *pl_written,
                  int     pi_HeaderLen,
                  time_t  pl_expireTime) {

  ulong_t  ll_ReadCnt, ll_WriteCnt;
  ulong_t  readChunk, writeChunk;
  int      li_read, li_wrote;
/*  char     lc_Buf[DATA_BUF_SIZE]; */
  char     *lc_Buf;
  char     *lc_ptr;
  int      li_fd;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_rd_set;
  int               li_return;

  lc_Buf = (char *) malloc (DATA_BUF_SIZE);
  if (lc_Buf == NULL)
  {
    IBP_errno = IBP_E_GENERIC;
    return(IBP_errno);
  }

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_rd_set);
  FD_SET(pi_sockfd,&ls_rd_set);

  li_fd = fileno (pf_fp);
  ll_ReadCnt = 0;
  
  while (ll_ReadCnt < pl_size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
        free(lc_Buf);
        IBP_errno = IBP_E_SERVER_TIMEOUT;
        return(IBP_errno);
      }else{
        ls_tv->tv_sec = pl_expireTime - time(0);
      }
    }
    li_return = select(pi_sockfd+1,&ls_rd_set,NULL,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        free(lc_Buf);
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      free(lc_Buf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    readChunk = min (DATA_BUF_SIZE, pl_size - ll_ReadCnt);
    li_read = Read (pi_sockfd, lc_Buf, readChunk);
    if (li_read <= 0) {
#ifdef IBP_DEBUG
      fprintf(stderr,"Socket2Queue(), sock read error\n");
#endif
      free(lc_Buf);
      return IBP_E_SOCK_READ;
    }

    ll_WriteCnt = 0;
    lc_ptr = lc_Buf;
    while (ll_WriteCnt < li_read) {

      writeChunk = min (li_read - ll_WriteCnt, pl_fifoSize-pl_wrcount);
   
      lseek(li_fd,pi_HeaderLen + pl_wrcount, SEEK_SET); 

      if ( (li_wrote = Write(li_fd, lc_ptr, writeChunk)) < 0){
#ifdef IBP_DEBUG
	printf("Socket2Queue() has write error \n");
#endif
        free(lc_Buf);
	return IBP_E_FILE_WRITE;
      }

      lc_ptr += li_wrote;
      ll_WriteCnt += li_wrote;
      pl_wrcount = (pl_wrcount + li_wrote) % pl_fifoSize;
    }
    if (ll_WriteCnt == li_read)
      ll_ReadCnt += li_read;
  }
  
  *pl_written = ll_ReadCnt;
  free(lc_Buf);
  return IBP_OK;
}


/*****************************/
/* File2Socket               */
/* It reads data from a file and writes it to socket     */
/* return -- IBP_OK or error number                */
/*****************************/
int File2Socket(FILE *pf_fp, int pi_sockfd, ulong_t pl_size, time_t  pl_expireTime) {
  long   n1, n2, readCount, writeCount;
/*  char  dataBuf[DATA_BUF_SIZE]; */
  char   *dataBuf;
  long   readChunk;
  char  *ptr;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_wr_set;
  int               li_return;

  dataBuf = (char *) malloc (DATA_BUF_SIZE);
  if (dataBuf == NULL)
  {
    IBP_errno = IBP_E_GENERIC;
    return(IBP_errno);
  }

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_wr_set);
  FD_SET(pi_sockfd,&ls_wr_set);


  n1 = readCount = 0;
  while (readCount < pl_size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
        free(dataBuf);
        IBP_errno = IBP_E_SERVER_TIMEOUT;
        return(IBP_errno);
      }else{
        ls_tv->tv_sec = pl_expireTime - time(0);
      }
    }
/*
    li_return = select(pi_sockfd+1,NULL,&ls_wr_set,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        free(dataBuf);
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }
*/

    readChunk = IBP_MIN (DATA_BUF_SIZE, pl_size-readCount);
    n1 = fread(dataBuf, 1, readChunk, pf_fp);
    if (n1 == 0) {
      free(dataBuf);
      return IBP_E_FILE_READ;
    }

    li_return = select(pi_sockfd+1,NULL,&ls_wr_set,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        free(dataBuf);
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    writeCount = 0; ptr = dataBuf;
    while( writeCount < n1){
      if ( (n2 = Write(pi_sockfd, ptr, n1 - writeCount)) < 0){
        free(dataBuf);
	return IBP_E_SOCK_WRITE;
      }
      ptr += n2;
      writeCount += n2;
    }
    readCount += n1;
  }
  free(dataBuf);
  return IBP_OK;
}


/*****************************/
/* Fifo2Socket               */
/* It reads data from a fifo file and writes it to socket     */
/* return -- IBP_OK or error number                */
/*****************************/
int Fifo2Socket ( FILE    *pf_fp,
                  int     pi_sockfd, 
                  ulong_t pl_size,
                  ulong_t pl_fifoSize,
		              ulong_t pl_rdPtr,
		              int     pi_HeaderLen,
                  time_t  pl_expireTime) {

  ulong_t  readCount, writeCount;
  ulong_t  readChunk;
  ulong_t  emptyCount = 0;
  long     n1, n2;
  char     *dataBuf;
  char     *lc_ptr;
  int      li_fd;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_wr_set;
  int               li_return;

  dataBuf = (char *) malloc (DATA_BUF_SIZE);
  if (dataBuf == NULL)
  {
    IBP_errno = IBP_E_GENERIC;
    return(IBP_errno);
  }

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_wr_set);
  FD_SET(pi_sockfd,&ls_wr_set);

  li_fd = fileno (pf_fp);
  n1 = readCount = 0;
  
  while (readCount < pl_size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
        free(dataBuf);
        IBP_errno = IBP_E_SERVER_TIMEOUT;
        return(IBP_errno);
      }else{
        ls_tv->tv_sec = pl_expireTime - time(0);
      }
    }
/*
    li_return = select(pi_sockfd+1,NULL,&ls_wr_set,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        free(dataBuf);
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }
*/
    readChunk = IBP_MIN(DATA_BUF_SIZE, pl_size-readCount); /* buffer overflow*/
    readChunk = IBP_MIN(readChunk, pl_fifoSize-emptyCount); /* read available*/
    readChunk = IBP_MIN(readChunk, pl_fifoSize-pl_rdPtr);   /* wrap around */

#ifdef IBP_DEBUG
    fprintf (stderr, "read chunk : %lu\n", readChunk);
#endif

    lseek(li_fd,pi_HeaderLen + pl_rdPtr, SEEK_SET);
    n1 = Read (li_fd, dataBuf, readChunk);
    if (n1 < 0)
    {
      free(dataBuf);
      return IBP_E_FILE_READ;
    }

    li_return = select(pi_sockfd+1,NULL,&ls_wr_set,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        free(dataBuf);
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    pl_rdPtr = (pl_rdPtr + n1) % pl_fifoSize;
    emptyCount += n1;
    writeCount = 0; 
    lc_ptr = dataBuf;
    while( writeCount < n1){
      if ((n2 = Write(pi_sockfd, lc_ptr, n1 - writeCount)) < 0) {
        free(dataBuf);
	return IBP_E_SOCK_WRITE;
      }
      lc_ptr += n2;
      writeCount += n2;
    }
    readCount += n1;
  }

  free(dataBuf);
  return IBP_OK;
}

#ifdef HAVE_PTHREAD_H
/*
pthread_once_t  errno_once = PTHREAD_ONCE_INIT;
static pthread_key_t errno_key;

void
_errno_destructor( void *ptr)
{
	free(ptr);
}

void
_errno_once(void)
{
	pthread_key_create(&errno_key,_errno_destructor);
}
*/
/*****************************/
/* _IBP_errno                */
/* IBP_errno used in multi-thread enviroment */
/*****************************/
/*
int 
*_IBP_errno()
{
	int *output;
	
	pthread_once(&errno_once,_errno_once);
	output = pthread_getspecific(errno_key);
	if (output == NULL ){
		output = calloc(1,sizeof(int));
		pthread_setspecific(errno_key,output);
	}
	return(output);
}
*/
#endif


/*****************************/
/* socketToMemory            */
/* It reads data from socket and writes it to memory     */
/* return -- IBP_OK or error number                */
/*****************************/
int socketToMemory(int sockfd, char *data, ulong_t size){
   long   n, writeCount;
   char  *ptr;
   
   writeCount = 0; ptr = data;
   while( writeCount < size){
      n = Read(sockfd, ptr, size - writeCount);
      if (n <= 0){
         IBP_errno = IBP_E_SOCK_READ;
         return -1;
      }
      ptr += n;
      writeCount += n;
   }
   return IBP_OK;
}

/*****************************/
/* MemoryToSocket            */
/* It reads data from memory and writes it to socket  */
/* return -- IBP_OK or error number                */
/*****************************/
int memoryToSocket(char *data, int sockfd, ulong_t size){
   long   n, writeCount;
   char  *ptr;
   
   writeCount = 0; ptr = data;
   while( writeCount < size){
      if ((n = Write(sockfd, ptr, size - writeCount)) < 0){
         IBP_errno = IBP_E_SOCK_WRITE;
         return -1;
      }
      ptr += n;
      writeCount += n;
   }
   return IBP_OK;
}

/*****************************/
/* GetCapFieldsNum           */
/* It returns the length of a Cap Field  */
/* return -- IBP_OK or error number                */
/*****************************/
int GetCapFieldsNum (char *cap, char sep) {
	int li_i;
	int li_nfields;

	li_nfields = 0;
	li_i = -1;
	if (cap == NULL || cap[0] == '\0')
		return -1;
	do {
		li_i ++;	
		if ( cap[li_i] == sep || cap[li_i] == '\0' )
			li_nfields ++;
	} while ( cap[li_i] != '\0' );

	return(li_nfields);
}

/*****************************/
/* GetCapFields              */
/* It writes a Cap Field into a string  */
/* return -- IBP_OK or error number                */
/*****************************/
int GetCapFields( char **fields,char *cap, char sep) {

	int	li_i,li_j;
	if ( cap == NULL || cap[0] == '\0' )
		return -1;

	li_i = 0;
	li_j = 0;
	fields[li_j] = cap;
	do{
		if ( cap[li_i] == sep ){
			li_j ++;
			fields[li_j] = cap + li_i + 1;
			cap[li_i] = '\0';
		}
		li_i ++;
	}while( cap[li_i] != '\0' );

	return (0);
}


/*****************************/
/* ltrimString               */
/* It eliminates blank space in s string  */
/* return -- the new string           */
/*****************************/
char *ltrimString(char *str){
	int  li_i;
	int  li_j;

	li_i = 0;
	li_j = 0;

	if ( str == NULL )
		return (NULL);

	while ( str[li_i] == ' ' )
		li_i++;
	
	if ( li_i == 0 )
		return(str);

	do{	
		str[li_j] = str[li_i];
		li_j ++;
		li_i ++;
	}while(str[li_i-1] != '\0' );

	return(str);
}

/*****************************/
/* validCapHead              */
/* It checks the validity of the cap  */
/* return -- yes / no        */
/*****************************/
int validCapHead( char *capString){
	if ( capString[0] != 'i' && capString[0] != 'I' )
		return(0);
	if ( capString[1] != 'b' && capString[1] != 'B' )
		return(0);
	if ( capString[2] != 'p' && capString[2] != 'P' )
		return(0);
	if ( capString[3] != ':' || capString[4] != '/' || capString[5] != '/')
		return(0);
	return(1);

}

/*****************************/
/* createIURL                */
/* It builds up the connection to the IBP depot  */
/* return -- the pointer of url    */
/*****************************/
/*
IURL *createIURL ( char *capString, int mode,int timeout  ){
	IURL	*iurl;
	char	*lc_tmp;
	char	*lp_fields[6];
	int	li_return;

	if ( capString == NULL ||  strlen(capString) <= 0 ){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}


	if ( (lc_tmp = strdup(capString)) == NULL ){
		IBP_errno = IBP_E_ALLOC_FAILED ;
		return(NULL);
	}

	ltrimString(lc_tmp);
	if ( !validCapHead(lc_tmp) ){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);
		return(NULL);
	}

	if ( GetCapFieldsNum( lc_tmp,'/') != 6 ) {
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);	
		return(NULL);
	}

	if ( GetCapFields(lp_fields,lc_tmp,'/') < 0 ){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);	
		return(NULL);
	}

	if ( lp_fields[3] == NULL || ( lp_fields[4] == NULL)|| ( lp_fields[5] == NULL )){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);
		return(NULL);
	}

	switch( mode ){
		case READCAP:
			if ( strcmp("READ",lp_fields[5]) != 0 ){
				IBP_errno = IBP_E_CAP_NOT_READ;
				free(lc_tmp);
				return(NULL);
			}
			break;
		case WRITECAP:
			if ( strcmp("WRITE",lp_fields[5]) != 0 ){
				IBP_errno = IBP_E_CAP_NOT_WRITE;
				free(lc_tmp);
				return(NULL);
			}
			break;
		case MANAGECAP:
			if ( strcmp("MANAGE",lp_fields[5]) != 0 && strcmp("READ",lp_fields[5]) != 0 && strcmp("WRITE",lp_fields[5]) != 0 ){
				IBP_errno = IBP_E_CAP_NOT_MANAGE;
				free(lc_tmp);
				return(NULL);
			}
			break;
	}

	if ( (iurl = (IURL*)malloc(sizeof(IURL))) == NULL ){
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED;
		return(NULL);
	}

	if ( ( iurl->key = strdup(lp_fields[3])) == NULL){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED ;
		return(NULL);	
	}
	if ( ( iurl->type = strdup(lp_fields[5])) == NULL ){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED;
		return(NULL);
	}
	if ( sscanf(lp_fields[4],"%d",&(iurl->type_key)) != 1 ){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}

	if ( GetCapFields(lp_fields,lp_fields[2],':') < 0 ){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}
	if ( lp_fields[0] == NULL || lp_fields[1] == NULL){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}
	if ( (iurl->hostname = strdup(lp_fields[0])) == NULL){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED;
		return(NULL);
	}
	if ( sscanf(lp_fields[1],"%d",&(iurl->port)) != 1 ){
		freeIURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}

	if((li_return = CheckDepot(iurl->hostname,&(iurl->port))) != IBP_OK){
                IBP_errno = li_return;
                freeIURL(iurl);
               	free(lc_tmp); 
		return(NULL);
        }

	if ( (iurl->fd = SocketConnect_t(iurl->hostname,iurl->port,timeout)) == -1 ){
		IBP_errno = IBP_E_CONNECTION;
		freeIURL(iurl);
		free(lc_tmp);
		return(NULL);
	}
 

	free(lc_tmp);
	return(iurl);
}
*/

/*****************************/
/* freeIURL                  */
/* It closes the connection to the IBP depot  */
/* return -- none            */
/*****************************/
void freeIURL( IURL *iurl){
	if ( iurl != NULL){
		if ( iurl->hostname != NULL )
			free(iurl->hostname);
		if ( iurl->key != NULL )
			free(iurl->key);
		if ( iurl->type != NULL)
			free(iurl->type);
		if ( iurl->fd >= 0 ){
			close(iurl->fd);
			iurl->fd = -1;
		}
		free(iurl);
	}
}

/*****************************/
/* createReadIURL            */
/* It builds up the connection for READCAP  */
/* return -- none            */
/*****************************/
/*
IURL *createReadIURL( IBP_cap ps_cap , int timeout){
	return(createIURL((char*)ps_cap,READCAP,timeout));
}
IURL *createWriteIURL( IBP_cap ps_cap, int timeout ){
	return(createIURL((char*)ps_cap,WRITECAP,timeout));
}
IURL *createManageIURL( IBP_cap ps_cap, int timeout){
	return(createIURL((char*)ps_cap,MANAGECAP,timeout));
}
*/
#if 0
int ibp_ntop( struct addrinfo *sai, char *buf, int len ){
    struct sockaddr_in *sin;
#ifdef AF_INET6
    struct sockaddr_in6 *sin6;
#endif
   
    switch ( sai->ai_family ){
    case AF_INET: {
       sin = (struct sockaddr_in *)(sai->ai_addr);
       if ( inet_ntop(AF_INET,&(sin->sin_addr),buf,len) == NULL ){
          return (-1);
       }
    }
    break;
#ifdef AF_INET6
    case AF_INET6:{
      sin6 = (struct sockaddr_in6 *)(sai->ai_addr);
      if ( inet_ntop(AF_INET6,&(sin6->sin6_addr),buf,len) == NULL){
        return(-1);
    }
    break;
#endif
    }

    }

    return 0;

}
#endif

/*
 * Socket lib
 */

/*****************************/
/* serve_known_socket        */
/* It builds up the socket connect to IBP depot  */
/* return -- socket ID            */
/*****************************/
int serve_known_socket(char *hostName, int port)
{

#if 0
   struct sockaddr_in   addr;
   int                  addr_length;
   int                  sockId;
   struct hostent       *he;
   int                  sockOpt  = 1;
#endif

   int                  sockId;
   int                  sockOpt  = 1;
   struct addrinfo      hints, *res,*saveres;
   int                  ret;
   char                 servs[30];

   bzero( &hints, sizeof(struct addrinfo));
   hints.ai_flags = AI_PASSIVE;
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   sprintf(servs,"%d",port);

   if ( (ret = getaddrinfo(NULL,servs,&hints,&res))!= 0 ){
      perror("serve_known_socket(): getaddrinfo()");
      return(-1);
   }

   saveres = res;
   /*
   he = gethostbyname(hostName);
   if (he == NULL ){
      perror("serve_known_socket() gethostbyname()");
      fprintf(stderr,"Host name: %s\n", hostName);
      return -1;
   }
   */

   fprintf(stderr,"  Listening on %d \t\t\t..................... ",port);
   do {

      /*
      ret = ibp_ntop(res,address,128);
      if ( ret == 0 ){
        fprintf(stderr,"\ntrying to bind %s .......",address);
      }
      */

      sockId = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
      
      if ( sockId < 0 ){
        /* fprintf(stderr,"Failed"); */
        continue;
      }  

      setsockopt(sockId,SOL_SOCKET,SO_REUSEADDR,(char*)(&sockOpt),sizeof(int));

      if ( bind(sockId,res->ai_addr,res->ai_addrlen) == 0 ){
          /* fprintf(stderr,"Done\n"); */
          break;
      }

      close(sockId);
   }while ((res = res->ai_next) != NULL);
  
   
   freeaddrinfo(saveres);

   if ( res == NULL ){
      fprintf(stderr,"failed\n");
      perror("server_know_socket(): create socket");
      return -1;
   }

   fprintf(stderr,"Done\n");

   /*
   ret = ibp_ntop(res,address,128);
   if ( ret == 0 ){
      fprintf(stderr,"bind on %s\n",address);
   }
   */

   
   return sockId;
 
#if 0

   sockId = socket(AF_INET,SOCK_STREAM,0);
   if (sockId == -1){
      perror("serve_known_socket() socket()");
      fprintf(stderr,"Host name: %s\n", hostName);
      return -1;
   }

   if (setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, (char*)(&sockOpt), 
                 sizeof(int)) == -1) {
      perror("serve_known_socket() setsockopt()");
      fprintf(stderr,"Host name: %s\n", hostName);
      return -1;
   }
   
   bzero((char*) &addr, sizeof(addr));
   addr.sin_port = htons((short) port);
   addr.sin_family = AF_INET;
   /*
    * below added by zheng yong 10/21/2001
    * to make ibp server listen on all ip address of the host
    *
   addr.sin_addr.s_addr = INADDR_ANY;
   */

   bcopy((void *) he->h_addr, (void *) &addr.sin_addr, he->h_length); 

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
#endif

   return sockId;
}

/*****************************/
/* listen                    */
/* It listens to a network for service requests */
/* return -- none            */
/*****************************/
void Listen (int pi_sockfd) {
  if (listen(pi_sockfd, IBP_MAXCONN) == -1) {
    fprintf (stderr, "Listen() failed\n\terror %d, %s\nExiting ...\n", errno, strerror(errno));
     exit(1);
  }
  return;
}

/*****************************/
/* SocketConnect_t           */
/* It builds up a unblocked socket */
/* return -- socket ID       */
/*****************************/
int SocketConnect_t (char *host, int port, int timeout)
{

  struct hostent        *ls_hoste;
  struct sockaddr_in    ls_addr4;
# ifdef AF_INET6
  struct sockaddr_in6   ls_addr6;
# endif
  struct sockaddr       ls_AddrPeerName;
  int                   li_sockfd, li_flags, li_ret = IBP_OK, li_NameLen;
  fd_set                ls_rset, ls_wset;
  struct timeval        lt_timer={0,0},*lp_timer;

  /* set timeout value */
  if ( timeout == 0)
	lp_timer = NULL;
  else{
	lp_timer = &lt_timer;
	lp_timer->tv_sec = timeout;
  }

  /*
   * clear errno 
   */
  errno = 0;

  /*
   * 1 - get host by name and fill in the appropriate IP address record
   */

  ls_hoste = gethostbyname(host);
  if (ls_hoste == NULL) {
#ifdef IBP_DEBUG
    fprintf(stderr, "SocketConnect(), gethostbyname () failed\n\terror: %d, %s\n", errno, strerror(errno));
#endif
    return -1;
  }

  switch (ls_hoste->h_addrtype) {
  case AF_INET:
    ls_addr4.sin_family = AF_INET;
    ls_addr4.sin_port = htons(port);
    ls_addr4.sin_addr.s_addr = *((u_long *) ls_hoste->h_addr_list [0]);
  
    break;
# ifdef AF_INET6
  case AF_INET6:
    ls_addr6.sin6_family = AF_INET6;
    ls_addr6.sin6_port = htons(port);
    /*    ls_addr6.sin6_addr.s6_addr =  */

    break;
# endif
  default:
    break;
  }

  /*
   * 2 - socket() & file control()
   */

  if ( (li_sockfd = socket(ls_hoste->h_addrtype, SOCK_STREAM, 0)) == -1) {
# ifdef IBP_DEBUG
    fprintf(stderr, "SocketConnect(), socket() failed\n\terror: %d, %s\n", errno, strerror (errno));
# endif
    return -1;
  }

  if ( (li_flags = fcntl (li_sockfd, F_GETFL, 0)) < 0) {
# ifdef IBP_DEBUG
    fprintf(stderr, "SocketConnect(), first GETFL fcntl() failed\n\terror: %d, %s\n", errno, strerror(errno));
# endif
    return -1;
  }

  if (fcntl (li_sockfd, F_SETFL, li_flags | O_NONBLOCK) < 0) {
# ifdef IBP_DEBUG
    fprintf(stderr, "SocketConnect(), SETFL fcntl() failed\n\terror: %d, %s\n", errno, strerror(errno));
# endif
    return -1;
  }


  /*
   * 3 - Connect - switch needed for different IP protocols ...
   */

  switch (ls_hoste->h_addrtype) {
  case AF_INET:
    li_ret = connect (li_sockfd, (struct sockaddr *) &ls_addr4, sizeof (ls_addr4)); 
    break;
# ifdef AF_INET6
  case AF_INET6:
    li_ret = connect (li_sockfd, (struct sockaddr *) &ls_addr6, sizeof (ls_addr6)); 
    break;
# endif
  default:
    break;
  }

  if (li_ret < 0) {
    if (errno != EINPROGRESS && errno != 0) {
#ifdef IBP_DEBUG
      fprintf(stderr, "SocketConnect (), connect() failed\n\terror: %d, %s\n", errno, strerror(errno));
      fprintf(stderr,"hostname:%s port: %d\n",host,port);
#endif
      close (li_sockfd);
      return -1;
    }

    /* Error is EINPROGRESS */
    FD_ZERO (&ls_rset);
    FD_SET (li_sockfd, &ls_rset);
    ls_wset = ls_rset;

    if ( (li_ret = select (li_sockfd+1, &ls_rset, &ls_wset, NULL, lp_timer)) == 0) {
      close (li_sockfd);
      errno = ETIMEDOUT;
      return -1;
    }

    if ( li_ret < 0){
	close(li_sockfd);
	return(-1);
    }

    li_NameLen = sizeof (ls_AddrPeerName);

    if (getpeername (li_sockfd, &ls_AddrPeerName, &li_NameLen) != 0) {
#ifdef IBP_DEBUG
      fprintf (stderr, "SocketConnect (), getpeername () failed\n\terror %d, %s\n", errno, strerror (errno));
#endif
      close (li_sockfd);
      return -1;
    }
  }

  /* 
   * 4- file control again, you never know ...
   */

  fcntl (li_sockfd, F_SETFL, li_flags);
  if ( (li_flags = fcntl (li_sockfd, F_GETFL, 0)) < 0) {
#ifdef IBP_DEBUG
    fprintf(stderr, "SocketConnect (), second GETFL fcntl() failed\n\terror: %d, %s\n", errno, strerror(errno));
#endif
    close (li_sockfd);
    return -1;
  }

  return li_sockfd;
}

int SocketConnect (char *host, int port){
	return(SocketConnect_t(host,port,0));
}



/* 
 * end socket part
 */


/*
 * IBP specific
 */


/*****************************/
/* CheckTimeout              */
/* It checks whether it is timeout */
/* return -- OK / not        */
/*****************************/
int CheckTimeout(IBP_timer	ps_timeout){
	if ( ps_timeout->ClientTimeout < 0 ){
		return(IBP_E_INVALID_PARAMETER);
	}
	return(IBP_OK);
}/* CheckTimeout */

/*****************************/
/* CheckDepot                */
/* It checks whether the depot is valid */
/* return -- OK / not        */
/*****************************/
int CheckDepot (char *pc_host, int *pi_port)
{
  
  int li_return;

  li_return = d_CheckHost(pc_host);
  if (li_return != IBP_OK)
    return (li_return);

  li_return = d_CheckPort(*pi_port); 
  if (li_return < 0)
    return (li_return);
  else
    *pi_port = li_return;

  return IBP_OK;

} /* CheckDepot */


static char  *IBP_strings[] = {
  "",
  "A generic error has occured while processing client request. Sorry for that.",
  "An error has occured while reading from an IBP socket.",
  "An error has occured while writing to an IBP socket.",
  "No such capability exists on this IBP server.",
  "The IBP capability used is not a write capability.",
  "The IBP capability used is not a read capability",   
  "The IBP capability used is not a manage capability.",
  "The IBP write capability does not have the proper key.",
  "The IBP read capability does not have the proper key.",
  "The IBP manage capability does not have the proper key.",
  "The IBP capability used has the wrong format.",
  "The IBP capability reference count is not positive. No further access allowed.",  
  "An error occurred while connecting to an IBP server.",
  "An error occurred while opening a back storage file on server.", 
  "An error occurred while reading from a back storage file on server.",
  "An error occurred while writing to a back storage file on server.",
  "An error occurred while accessing a back storage file on server.",
  "An error occurred while performing a seek operation on a back storage file on server.",
  "Not enough room on server to accomodate required storage.",
  "A write operation would exceed the size allocated for the storage area.",
  "An IBP message has the wrong format expected by the server and/or the client.",
  "The storage area type specified is not supported by the IBP server.",
  "A resource required by IBP server is currently unavailable.",
  "An internal error occurred in IBP server.",
  "The IBP server has received a command it does not recognize.",
  "The operation attempted would block on IBP server.",
  "The IBP protocol version selected is not supported by this server."
  "The IBP server does not support allocation for the specified period of time.",
  "The password you entered for this IBP server is incorrect."
  "Invalid parameter in call to IBP.",
  "Invalid parameter (hostname).",
  "Invalid parameter (port number).",
  "Invalid parameter (attribute, duration).",
  "Invalid parameter (attribute, reliability).",
  "Invalid parameter (attribute, type).",
  "Invalid parameter (size).",
  "An internal error (memory allocation) occurred in the IBP server."
  };

/*****************************/
/* IBP_strerror              */
/* It returns the error info */
/* return -- error info      */
/*****************************/
char *IBP_strerror (int pi_err) {

   if ((pi_err > -1) || (pi_err  < -IBP_MAX_ERROR))
      return IBP_strings[0];
   else
      return IBP_strings[-pi_err];
}

/*****************************/
/* IBP_substr                */
/* It eliminates the blank space in a string */
/* return -- the pointer of the last char    */
/*****************************/
/*
char *IBP_substr (char *pc_buffer, char *pc_first)
{

  int       li_i = 0;
  char      *lc_save, *lc_left;

  lc_save = pc_buffer;
  lc_left = pc_buffer;
 
  while ((*lc_left == ' ') && (li_i < (CMD_BUF_SIZE -1))){
	lc_left ++;
	li_i ++;
  }
  while ((*lc_left != ' ') && (*lc_left != '\n') && (*lc_left != '\0') && (li_i < (CMD_BUF_SIZE - 1))) {
    lc_left++;
    li_i++;
  }
  lc_left++;

  strncpy (pc_first, lc_save, li_i);
  pc_first[li_i] = '\0';

  return lc_left;
}
*/

/*****************************/
/* handle_error              */
/* It handles the error in IBP codes */
/* return -- none            */
/*****************************/
void handle_error (int nerr, void *errinfo)
{
  IS		ls_in;
  char	        *lc_LineBuf;
  int		li_communicate = NO;

  if (nerr > (-IBP_MAX_ERROR) )
    li_communicate = YES;

  switch (nerr) {
  case E_USAGE:
    fprintf(stderr,"\nUsage: ibp_server_mt [OPTION] ...\n");
    fprintf(stderr,"\nExample:\n");
    fprintf(stderr,"   ibp_server_mt -cf /etc -tn 8 -tm 50\n");
    fprintf(stderr,"   ibp_server_mt -c /etc/ibp.cfg -p 6714\n");
    fprintf(stderr, "\nIBP server - options:\n");
    fprintf(stderr, "   -help                 : this screen\n");
    fprintf(stderr, "   -V,--version          : output version information and exit\n");
    fprintf(stderr, "   --rid <id>            : the ID of default resource \n");
    fprintf(stderr, "   --rtype <t>           : the type of the default resource\n\t\t\t : t = d    Disk depot\n\t\t\t : t = m    Memory depot\n");
    fprintf(stderr, "   -s  <stable storage>  : the amount of stable storage to allocate for the default resource\n");
    fprintf(stderr, "   -v  <vol storage>     : the amount of volatile storage to allocate for the default resource\n");
    fprintf(stderr, "   -m  <min free size>   : the minimum free storage size  for the default resource\n");
    fprintf(stderr, "   -ds <stable dir>      : the path of the stable storage directory for the default resource\n");
    fprintf(stderr, "   -dv <vol dir>         : the path of the volatile storage directory for the default resource\n");
    fprintf(stderr, "   --loc < dir >         : the path of the default resource location \n");
    fprintf(stderr, "   -l  <lifetime>        : the max duration of the default resource\n");
    fprintf(stderr, "   -p  <port>            : the port to use 1025-65534\n");
    fprintf(stderr, "   -pw <password>        : the password (to access the server fundamental data)\n");
    fprintf(stderr, "   -cf <cfg path>        : the path to find the config file and store the log file\n");
    fprintf(stderr, "   -c  <cfg name>        : the name of the config file\n");
    fprintf(stderr, "   -hn <hostname>        : the host name of the ibp server\n");
    fprintf(stderr, "   -tn <threads>         : number of worker threads to start initially\n");
    fprintf(stderr, "   -tm <threads>         : limit of total number of worker threads running. (maximum is %d) \n",MAX_THREADS);
    fprintf(stderr, "   --ca-file <CA cert file> : CA certificate file name \n");
    fprintf(stderr, "   --enableauthen        : enable client authentication \n");
    fprintf(stderr, "   --privatekeypasswd <passwd> : passwd for depot's certificate private key \n");
    fprintf(stderr, "   --log                   enable server loging\n");
    fprintf(stderr, "   -tl                   : enable per thread logging\n");
    fprintf(stderr, "   -nr                   : skip recovery\n");
    fprintf(stderr, "   -rt                   : the time of grace period\n");
    fprintf(stderr, "\nIf any of these parameters are not specified, the default values found in the IBP configuration file under /etc will be taken.\n");
    fprintf(stderr, "In case some values will be missing from the IBP configuration file, IBP default values will be used. Please refer to the IBP documentaton for a complete explanation.\n");
    fprintf(stderr,"\nReport bugs to <ibp@cs.utk.edu>.\n");

    exit(0);
    break;

  case E_HOMEDIR:
    fprintf(stderr, "Unable to determine home directory\n");
    break;
  case E_FQDN:
    fprintf(stderr, "Unable to determine FQDN of local host\n");
    break;
  case E_GLIMIT:
    fprintf(stderr, "Error in getrlimit(), %s\n",strerror(errno));
    break;
  case E_SLIMIT:
    fprintf(stderr, "Error in setrlimit(). %s\n",strerror(errno));
    break;
  case E_CFGFILE:
    ls_in = *((IS *) errinfo);
    if (ls_in->text1 != NULL) {
      fprintf(stderr, "Error in config file at :%s\n", ls_in->text1);
    }
    /* jettison_inputstruct(ls_in); */
    break;
  case E_CFGPAR:
    ls_in = *((IS *) errinfo);
    if (ls_in->text1 != NULL) {
      fprintf(stderr, "Invalid config parameter at : %s\n", ls_in->text1);
    }
    /* jettison_inputstruct(ls_in); */
    break;
  case E_ACCDIR:
    ls_in = *((IS *) errinfo);
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Error accessing directory %s\n", ls_in->fields[1]);
    }
    /* jettison_inputstruct(ls_in); */
    break;
  case E_ABSPATH:
    ls_in = *((IS *) errinfo);
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Error: expecting absolute path at %s\n", ls_in->text1);
    }
    /* jettison_inputstruct(ls_in);  */
    break;
  case E_INVHOST:
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Error: Hostname %s appears to be invalid\n",(char *) errinfo);
    }
    break;
  case E_ZEROREQ:
    fprintf(stderr,"Error: Zero storage requested\n");
    break;
  case E_ACCSTORD:   /* from RECOVER */
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Unable to access storage directory %s: %s\n",
	    (char *) errinfo, strerror(errno));
    }
    exit (1);
  case E_OFILE:
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Unable to open file %s: %s\n",
	    (char *) errinfo, strerror(errno));
    }
    break;
  case E_RFILE:
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Unable to read file %s\n",(char *) errinfo);
    }
    break;
  case E_CFGSOK:
    fprintf(stderr,"Unable to configure IBP socket\n");
    break;
  case E_LISSOK:
    fprintf(stderr,"Error in listening socket\n");
    break;
  case E_RLINE:
    fprintf(stderr,"Error in reading line from Client \n");
    break;
  case E_BOOTFAIL:
    if (ls_in->text1 != NULL) {
      fprintf(stderr,"Boot failed, function code %d\n",*((int *) errinfo));
    }
    break;
  case E_ACCEPT:
    if (ls_in->text1 != NULL) {
      fprintf(stderr, "accept () failed, %s, Thread %d\n",strerror(errno), (int) errinfo);
    }
    break;
  case E_PORT:
    if (ls_in->text1 != NULL) {
      fprintf(stderr, "the port asked, %d, is out of range. The default IBP DATA port will be used\n", *((int *) errinfo));
    }
    break;
  case E_ALLOC:
    fprintf (stderr, "A memory allocation was unsuccessful\n");
    break;
  case E_CMDPAR:
    fprintf (stderr, "Invalid command line parameters\n");
    break;
  default:
    break;
	    
  }

  if (li_communicate == YES) {
    lc_LineBuf = malloc (CMD_BUF_SIZE);
    sprintf(lc_LineBuf, " %d \n", nerr);
    /*    WriteSock (*((int *) errinfo), lc_LineBuf, strlen(lc_LineBuf)); */
    if (errinfo != NULL) {
      Write (*((int *) errinfo), lc_LineBuf, strlen(lc_LineBuf));
    }
    free (lc_LineBuf);
  }

  return;
}


/*
 * various old stuff ...
 */


/*****************************/
/* IBP_freeCapSet            */
/* It frees the cap space    */
/* return -- OK / not        */
/*****************************/
/*
int IBP_freeCapSet(IBP_set_of_caps capSet){
*/
/*  Free a dynamically allocated IBP_set_of_caps 
 *  return 0 on success
 *  return -1 on failure
 */
/*
   if (capSet != NULL){
      free(capSet->readCap);
      free(capSet->writeCap);
      free(capSet->manageCap);
      free(capSet);
      return 0;
   }
   IBP_errno = IBP_E_INVALID_PARAMETER;
   return -1;
}
*/

/*
char *capToString(IBP_cap cap){
   return (char *) cap;
}
*/

IBP_cap stringToCap(char *str){
   return strdup(str);
}

void  deleteCap(IBP_cap cap){
   free(cap);
}

/*
 * Name	 :	ibp_ultostr
 *
 * Note	 :	Convert an ibp_ulong integer to a string
 *
 * Para	 :	value --  value to be converted
 *		buf   --  pointer to the END of the buffer passed
 *		base  --  base of converted string, must 2=< base =< 36		
 *
 * return:	upon success, return pointer of converted string, otherwise
 *		returns null pointer.
 *
 * error:       ENIVAL -- The value of base is not supported.	
 */

 	   	
/*****************************/
/* IBP_ultostr               */
/* It translates an unsigned long to a string    */
/* return -- the string        */
/*****************************/
char * ibp_ultostr( ibp_ulong value, char *buf, int base ){
	
	int digit;
	
	/* clear up the error code */
	errno = 0;
	
	/* check the validation of 'base' */
	if ( ( base < 2 ) || ( base > 36)){
		errno = EINVAL;
		return 0;
	}
	
	/*  terminate the return string */
	*buf = '\0';
	
	/* do convert */
	do {
		digit = value % base;
		value = value / base;
		
		*(--buf) = '0' + digit;
		if ( digit > 9 )
			*buf = 'A'+ digit - 10;
	} while ( value );
	
	return(buf);
}

/*
 * Name	 :	ibp_ltostr
 *
 * Note	 :	Convert an ibp_long integer to a string
 *
 * Para	 :	value --  value to be converted
 *		buf   --  pointer to the END of the buffer passed
 *		base  --  base of converted string, must 2=< base =< 36		
 *
 * return:	upon success, return pointer of converted string, otherwise
 *		returns null pointer.
 *
 * error:       ENIVAL -- The value of base is not supported.	
 */

char * ibp_ltostr( ibp_long value, char *buf, int base){
	ibp_ulong	uval;
	char 		*pos;
	int		nega;
	
	nega = 0;
	
	
	if ( value < 0) {
		nega = 1;
		uval = ((ibp_ulong)(-(1+value)))+ 1 ;
	}else {
		uval = value;
	}
	
	pos = ibp_ultostr(uval,buf,base);
	
	if ( pos && nega ) {
		*(--pos) = '-';
	}
	
	return pos;
}


/*
 * Name	 :	ibp_strtoul
 *
 * Note	 :	Convert a string to an ibp_ulong integer 
 *
 * Para	 :	str   --  pointer of the string 
 *
 * return:	upon on success , returns converted value; otherwise return 0.
 * 
 * errors:	EINVAL	The value of base is not supported.
 *		ERANGE	The value to be returned is not representable. 
 */

ibp_ulong ibp_strtoul( char *str, int base ) {

	char	*pos;
	ibp_ulong	value = 0,max = 1;
	ibp_ulong	digit;
	char	ch;
	
	
	/* clear up the errno */
	errno = 0;
	
	/* check the validation of base */
	if ( ( base < 2) || (base > 36)){
		errno = EINVAL;
		return 0;
	}
	
	pos = str;
	max = (~max)+1;
	
	while ( *pos == ' ')  pos++ ;
	if ( *pos == '-' ){
		errno = ERANGE;
		return 0;
	}
	if ( *pos == '+' ) pos++;
	while ( *pos != '\0' && *pos != ' ' ){
		ch = *pos;
		if ( ch >='0' && ch <= '9' ){
			digit = ch - '0';
		}else if( ch >='a' && ch <='z' ){
			digit = 10 + ch - 'a';
		}else if ( ch >='A' && ch <= 'Z'){
			digit = 10 + ch - 'A';
		}else {
			errno = ERANGE;
			return 0;
		}
		if ( digit >= base ){
			errno = ERANGE;
			return 0;
		}
		
		if ( (ibp_ulong)((max - digit)/base) <  value ){
			errno = ERANGE;
			return 0;
		}
		
		value = value * base + digit;
		
		pos ++;	
	}
	
	return value;
	
	 
}


/*
 * Name	 :	ibp_strtol
 *
 * Note	 :	Convert a string to an ibp_long integer 
 *
 * Para	 :	str   --  pointer of the string 
 *
 * return:	upon on success , returns converted value; otherwise return 0.
 * 
 * errors:	EINVAL	The value of base is not supported.
 *		ERANGE	The value to be returned is not representable. 
 */

ibp_long ibp_strtol( char *str, int base ) {
	int   nega;
	ibp_ulong uval,max,min;
	ibp_long val;
	
	nega = 0;
	
	while ( *str == ' ' ) str ++;
	if ( *str == '-' )  {
		nega = 1;
		str++;
	}	
	
	uval = ibp_strtoul(str,base);
	max = (ibp_ulong)IBP_MAXL; /* there might be warning on 32 bit machines */
	min = max+1;
	if ( errno != 0 )
		return 0;
		 
	if ( nega ){
		if ( uval > min ){
			errno = ERANGE;
			return 0;
		}else {
			val = (~uval) + 1;
		}
	}else {
		if ( uval > max ) {
			errno = ERANGE;
			return 0;
		}else{
			val = uval;
		}	
	}
	
	return (val);
}

ibp_ulong  ibp_getStrSize( char *str ){
	char		ch;
	int		shift;
	
	trim(str);
	ch = *(str + strlen(str) -1);
	switch (ch){
	case 'k':
	case 'K':
		shift = 10;
		*(str + strlen(str)-1) = '\0';
		break;
	case 'm':
	case 'M':
		shift = 20;
		*(str + strlen(str) -1) = '\0';
		break;
	case 'g':
	case 'G':
		shift = 30;
		*(str + strlen(str) -1) = '\0';
		break;
	default:
		shift = 20;	
	}
	
	return ( ibp_strtoul(str,10) << shift);
	
}
