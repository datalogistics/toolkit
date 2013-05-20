/*
 *           IBP version 1.2  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *         Authors: A. Bassi, X. Li, Y. Zheng, J. Plank, M. Beck
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
 * ibp-ram.c        
 *
 * IBP server 
 * Library of functions used by the server - file system resources
 *
 */

# include "config-ibp.h"
# include "ibp_server.h"
# ifdef HAVE_PTHREAD_H 
# include "ibp_thread.h"
# endif
# include "ibp_resources.h"
 

/*
 * =========================================================
 *          Subroutine: Memory2Socket
 * .........................................................
 * Usage : Memory2Socket
 * Input
 * Output
 * Preconditions:
 * Purpose:
 * Returns:
 * ---------------------------------------------------------
 */

int Memory2Socket(char *pc_buffer, int pi_sockfd, ulong_t pl_size, time_t pl_expireTime){
  long   n, writeCount;
  char  *ptr;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_wr_set;
  int               li_return;

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_wr_set);
  FD_SET(pi_sockfd,&ls_wr_set);

  writeCount = 0; ptr = pc_buffer;
  n = 0;
  while (writeCount < pl_size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
        IBP_errno = IBP_E_SERVER_TIMEOUT;
        return(IBP_errno);
      }else{
        ls_tv->tv_sec = pl_expireTime - time(0);
      }
    }
    li_return = select(pi_sockfd+1,NULL,&ls_wr_set,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    if ( (n = Write(pi_sockfd, ptr, pl_size - writeCount)) < 0){
      return IBP_E_SOCK_WRITE;
    }
    ptr += n;
    writeCount += n;
  }
  return IBP_OK;
}
/*
 * =========================================================
 *          Subroutine: Socket2Memory
 * .........................................................
 * Usage : Socket2Memory
 * Input
 * Output
 * Preconditions:
 * Purpose:
 * Returns:
 * ---------------------------------------------------------
 */

int Socket2Memory(int pi_sockfd, char *data, ulong_t *size, time_t pl_expireTime){

  long     n1, readCount;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_rd_set;
  int               li_return;

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_rd_set);
  FD_SET(pi_sockfd,&ls_rd_set);

  n1 = readCount = 0;
  while (readCount < *size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
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
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }
    n1 = Read (pi_sockfd, data, *size-readCount);
    if (n1 < 0)
      return IBP_E_SOCK_READ;

    data += n1;
    readCount += n1;
  }

  *size = readCount;
  return IBP_OK;
}

/*
 * =========================================================
 *          Subroutine: Socket2MemoryQueue
 * .........................................................
 * Usage : Socket2MemoryQueue
 * Input
 * Output
 * Preconditions:
 * Purpose:
 * Returns:
 * ---------------------------------------------------------
 */
int Socket2MemoryQueue (int pi_sockfd,
                  char *ppc_buffer,
                  ulong_t pl_size,
                  ulong_t pl_fifoSize,
                  ulong_t pl_wrcount,
                  time_t  pl_expireTime) {

  ulong_t  ll_ReadCnt;
  ulong_t  readChunk;
  int      li_read;
  char     *lc_ptr;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_rd_set;
  int               li_return;

  if ( pl_expireTime == 0 )
    ls_tv = NULL;
  else{
    ls_tv = &ls_timeval;
    ls_tv->tv_sec = pl_expireTime - time(0);
    ls_tv->tv_usec = 0;
  }
  FD_ZERO(&ls_rd_set);
  FD_SET(pi_sockfd,&ls_rd_set);

  ll_ReadCnt = 0;

  while (ll_ReadCnt < pl_size) {
    if ( ls_tv != NULL ){
      if ( time(0) > pl_expireTime  ){
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
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    lc_ptr = ppc_buffer + pl_wrcount;
    readChunk = min (pl_size - ll_ReadCnt, pl_fifoSize - pl_wrcount);
    li_read = Read (pi_sockfd, lc_ptr, readChunk);
    if (li_read <= 0) {
#ifdef IBP_DEBUG
      fprintf(stderr,"Socket2Queue(), sock read error\n");
#endif
      return IBP_E_SOCK_READ;
    }
    ll_ReadCnt += li_read;
    pl_wrcount = (pl_wrcount + li_read) % pl_fifoSize;
  }

  return IBP_OK;
}

/*
 * =========================================================
 *          Subroutine: MemoryQueue2Socket
 * .........................................................
 * Usage : MemoryQueue2Socket
 * Input
 * Output
 * Preconditions:
 * Purpose:
 * Returns:
 * ---------------------------------------------------------
 */
int MemoryQueue2Socket (
                 char *ppc_buffer,
                 int pi_sockfd,
                 ulong_t pl_size,
                 ulong_t pl_fifoSize,
                 ulong_t pl_rdPtr,
                 int     pi_HeaderLen,
                 time_t  pl_expireTime) {

  ulong_t  readCount, writeCount;
  ulong_t  readChunk;
  long     n1, n2;
  char     *lc_ptr;

  struct timeval    ls_timeval;
  struct timeval    *ls_tv;
  fd_set            ls_wr_set;
  int               li_return;

  lc_ptr = ppc_buffer;

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
        IBP_errno = IBP_E_SERVER_TIMEOUT;
        return(IBP_errno);
      }else{
        ls_tv->tv_sec = pl_expireTime - time(0);
      }
    }
    li_return = select(pi_sockfd+1,NULL,&ls_wr_set,NULL,ls_tv);
    if ( li_return < 0 ){
      if (( errno == EINTR ) || (errno == 0) ) {
        continue;
      }else {
        IBP_errno = IBP_E_SOCK_READ;
        return(IBP_errno);
      }
    }else if( li_return == 0 ){
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    readChunk = IBP_MIN(pl_size-readCount, pl_fifoSize-pl_rdPtr); /* read available*/
/*    readChunk = IBP_MIN(readChunk, pl_fifoSize-pl_rdPtr);    wrap around */

    writeCount = 0;
    lc_ptr = (char *) ppc_buffer + pl_rdPtr;
    while( writeCount < readChunk){
      if ((n2 = Write(pi_sockfd, lc_ptr, readChunk - writeCount)) < 0) {
        return IBP_E_SOCK_WRITE;
      }
      lc_ptr += n2;
      writeCount += n2;
    }
    readCount += readChunk;
    pl_rdPtr = (pl_rdPtr + readChunk) % pl_fifoSize;
  }

  return IBP_OK;
}

int storeInRam (RESOURCE      *rs,
                IBP_byteArray ps_container, 
                int           pi_HeaderLen,
                ulong_t       pl_writeptr,  
                int           pi_fd, 
                int           pi_srcfd,
                time_t        pt_expireTime,
                int           pi_nbt,
                unsigned long *pl_written, 
                unsigned long pl_size,
                unsigned long pl_free,
                unsigned long pl_rdcount,
                ulong_t       pl_wrcount) {

  void                 *lpv_areaptr;
  char                 lc_LineBuf[CMD_BUF_SIZE];
  int                  li_Ret = IBP_OK;
  int                  li_nextstep = IBP_OK;

#ifdef HAVE_PTHREAD_H
  Thread_Info          *info;
#endif

  if (ps_container->startArea == NULL) {
	  return IBP_E_INTERNAL;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
    lpv_areaptr = ps_container->startArea+ ibp_cmd[pi_nbt].Offset ;
    break;

  case IBP_BUFFER:
    lpv_areaptr = ps_container->startArea;
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    lpv_areaptr = ps_container->startArea +  pl_writeptr;
    break;

  default:
    break;
  }

  if (li_nextstep == DONE ) {
    return IBP_E_INTERNAL;
  }

  sprintf(lc_LineBuf, "%d \n", IBP_OK);
  if (Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
    return IBP_E_SOCK_WRITE;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    li_Ret = Socket2Memory(pi_srcfd, (char *) lpv_areaptr, &ibp_cmd[pi_nbt].size, pt_expireTime);
    if (li_Ret != IBP_OK) {
/*
      printf ("sun chi mi, la va no :%d:\n", li_Ret);
*/
      li_nextstep = DONE;
      break;
    }
    *pl_written = ibp_cmd[pi_nbt].size;
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    li_Ret = Socket2MemoryQueue(pi_srcfd, 
		                (char *) ps_container->startArea, 
				ibp_cmd[pi_nbt].size, 
				ps_container->maxSize,
				pl_writeptr,
				pt_expireTime);
    *pl_written = ibp_cmd[pi_nbt].size;

    if (li_Ret != IBP_OK) {
      li_nextstep = DONE;
      break;
    }
    ps_container->wrcount = pl_wrcount;
    break;

  default:
    break;
  }

/*
  printf ("sun chi mi\n");
*/
  if (li_nextstep == DONE ) {
    return li_Ret;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
    if ( ps_container->currentSize < ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size ){
      ps_container->currentSize = ibp_cmd[pi_nbt].size+ibp_cmd[pi_nbt].Offset;
    }
    break;

  case IBP_BUFFER:
    ps_container->currentSize = ibp_cmd[pi_nbt].size;

    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    break;

  default:
    break;
  }
  
  switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
#ifdef IBP_THREAD
      info = (Thread_Info *) ps_container->thread_data;
      if ((info->read > -1) && (ps_container->currentSize >= info->read)) {
        pthread_cond_signal(&(info->read_cond));
      }
#endif
      break;

   case IBP_FIFO:
   case IBP_CIRQ:
     ps_container->size = pl_size;
     ps_container->wrcount = pl_wrcount;
     ps_container->rdcount = pl_rdcount;
     ps_container->free = pl_free;

#ifdef IBP_THREAD
     info = (Thread_Info *) ps_container->thread_data;
     if ((info->read > -1) && (ll_size - ll_free >= info->read)) {
       pthread_cond_signal(&(info->read_cond));
     }
#endif
     break;

   default:
     break;
  }


  return IBP_OK;

}


int loadFromRam (int pi_fd,
                        int pi_nbt, 
                        int pi_HeaderLen,
                        IBP_byteArray ps_container,
                        ulong_t *ppl_TrueSize,
                        ulong_t *ppl_rdcount,
                        ulong_t *ppl_wrcount,
                        ulong_t  pl_size,
                        ulong_t *ppl_free,
                        time_t *pt_expireTime,
                        ulong_t pl_readptr,
                        int *pi_fddata ) {

  int       li_nextstep = IBP_OK;
  char      lc_LineBuf[CMD_BUF_SIZE];
  char      *lc_generic, *lc_TargetKey, *lc_WriteTag, *lc_GenII;
  int       li_error = IBP_OK, li_nfields;
  char      **lp_fields;
  int       li_timeout,li_response;
  int       li_ret;
  void      *lpv_start;


  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    if (ibp_cmd[pi_nbt].size >= 0) {
      lpv_start = (void *) (ps_container->startArea + ibp_cmd[pi_nbt].Offset);
    }
    else {
      li_nextstep = HLSD_DONE;
      break;
    }
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    if (ibp_cmd[pi_nbt].size >= 0) {
//      *ppl_TrueSize = ibp_cmd[pi_nbt].size;
      lpv_start = (void *) (ps_container->startArea + *ppl_rdcount);
 //     *ppl_rdcount = (*ppl_rdcount + *ppl_TrueSize) % pl_size;
 //     *ppl_free += *ppl_TrueSize;
    }
    else {
      li_nextstep = HLSD_DONE;
      break;
    }
    break;

  default:
    li_nextstep = HLSD_DONE;
    break;
  }

  if (li_nextstep == HLSD_DONE) {
    return IBP_E_FILE_ACCESS;
  }

  switch (ibp_cmd[pi_nbt].Cmd) {
  case IBP_LOAD:
    sprintf (lc_LineBuf,"%d %lu \n", IBP_OK, *ppl_TrueSize);
#ifdef IBP_DEBUG
    fprintf(stderr, "T %d, linebuff in handle_load: %s\n",pi_nbt, lc_LineBuf);
#endif
    if (Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
      li_nextstep = HLSD_DONE;
      break;
    }
    if (*ppl_TrueSize == 0) {
      li_nextstep = HLSD_DONE;
      break;
    }
    *pi_fddata = pi_fd;
#ifdef IBP_DEBUG
    printf ("mannaggia, fddata :%d: fd :%d:\n", *pi_fddata, pi_fd);
#endif
    break;

  case IBP_SEND_BU:
  case IBP_SEND:
      lc_generic = malloc (MAX_CAP_LEN);
      if (lc_generic == NULL) {
        li_error = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        break;
      }

      strcpy(lc_generic, ibp_cmd[pi_nbt].Cap);
      li_nfields = GetCapFieldsNum(lc_generic, '/');
      if ( li_nfields < 6 ) {
        li_error = IBP_E_WRONG_CAP_FORMAT;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }

      lp_fields = malloc(sizeof(char *)*(li_nfields));
      if ( lp_fields == NULL ){
        li_error = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }

      if ( GetCapFields( lp_fields,lc_generic,'/') < 0 ) {
        li_error = IBP_E_WRONG_CAP_FORMAT;
        li_nextstep = HLSD_DONE;
        free(lp_fields);
        free(lc_generic);
        break;
      }

      ibp_cmd[pi_nbt].host = lp_fields[2];
      lc_TargetKey = lp_fields[3];
      lc_GenII = lp_fields[4];
      lc_WriteTag = lp_fields[5];

      if ( GetCapFields( lp_fields, ibp_cmd[pi_nbt].host, ':') < 0 ) {
        li_error = IBP_E_WRONG_CAP_FORMAT;
        li_nextstep = HLSD_DONE;
        free(lp_fields);
        free(lc_generic);
        break;
      }
 
      ibp_cmd[pi_nbt].host = lp_fields[0];
      ibp_cmd[pi_nbt].port = atoi (lp_fields[1]);

      free(lp_fields);

      if ((ibp_cmd[pi_nbt].host == NULL) || (lc_TargetKey == NULL) || (lc_GenII == NULL)) {
        li_error = IBP_E_WRONG_CAP_FORMAT;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }
      if (lc_WriteTag == NULL) {
        li_error = IBP_E_CAP_NOT_WRITE;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }
      if (strcmp (lc_WriteTag,"WRITE") != 0) {
        li_error = IBP_E_CAP_NOT_WRITE;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }
      if (sscanf(lc_GenII,"%d", &ibp_cmd[pi_nbt].writekey) != 1) {
        li_error = IBP_E_WRONG_CAP_FORMAT;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }

      *pi_fddata = SocketConnect (ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

      if (*pi_fddata == -1) {
        li_error = IBP_E_CONNECTION;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }
      /* li_timeout = -1; */

      sprintf(lc_LineBuf, "%d %d %s %d %lu %d \n",
              IBPv031,
              IBP_STORE,
              lc_TargetKey,
              ibp_cmd[pi_nbt].writekey,
              *ppl_TrueSize,
              ibp_cmd[pi_nbt].TRGSYNC);

      if (Write (*pi_fddata, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
        li_error = IBP_E_SOCK_WRITE;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }

      if (ReadLine(*pi_fddata, lc_LineBuf, sizeof(lc_LineBuf)) == -1) {
        li_error = IBP_E_SOCK_READ;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }

      if (sscanf(lc_LineBuf,"%d", &li_response) != 1 ) {
        li_error = IBP_E_BAD_FORMAT;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }

      if (li_response != IBP_OK) {
        li_error = li_response;
        li_nextstep = HLSD_DONE;
        free(lc_generic);
        break;
      }
      free(lc_generic);
      break;

  case IBP_DELIVER:
      *pi_fddata = SocketConnect (ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

      if (*pi_fddata == -1) {
        li_error = IBP_E_CONNECTION;
        li_nextstep = DONE;
        break;
      }
      break;

  default:
    break;

  }

  if (li_nextstep == HLSD_DONE) {
    return li_error;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    li_ret = Memory2Socket((char *) lpv_start, *pi_fddata, *ppl_TrueSize,*pt_expireTime);
    /* SaveInfo2LSS(ps_container, 2); */
    break;

  case IBP_FIFO:
    li_ret = MemoryQueue2Socket(ps_container->startArea, *pi_fddata, 
		                *ppl_TrueSize,
                         ps_container->maxSize,pl_readptr,pi_HeaderLen,*pt_expireTime);
    if (li_ret >= 0)
    {
      *ppl_rdcount = (*ppl_rdcount + *ppl_TrueSize) % pl_size;
      *ppl_free += *ppl_TrueSize;
      ps_container->size = pl_size;
      ps_container->wrcount = *ppl_wrcount;
      ps_container->rdcount = *ppl_rdcount;
      ps_container->free = *ppl_free;
      /* SaveInfo2LSS(ps_container, 2); */
    }
    break;

  case IBP_CIRQ:
    if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND) || (ibp_cmd[pi_nbt].Cmd == IBP_DELIVER))
      ibp_cmd[pi_nbt].Offset=0;
    li_ret = MemoryQueue2Socket(ps_container->startArea, 
		                            *pi_fddata, 
				                        *ppl_TrueSize,
                                ps_container->maxSize,
				                        pl_readptr,
				                        pi_HeaderLen,
                                *pt_expireTime);
    if (li_ret >= 0)
    {
      *ppl_rdcount = (*ppl_rdcount + *ppl_TrueSize) % pl_size;
      *ppl_free += *ppl_TrueSize;
      ps_container->size = pl_size;
      ps_container->wrcount = *ppl_wrcount;
      ps_container->rdcount = *ppl_rdcount;
      ps_container->free = *ppl_free;
      /* SaveInfo2LSS(ps_container, 2); */
    }
    break;
 
  default:
    break;
  }

  return IBP_OK;
}


