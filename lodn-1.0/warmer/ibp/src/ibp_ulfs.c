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
# include <assert.h>
# include "ibp_server.h"
# ifdef HAVE_PTHREAD_H 
# include "ibp_thread.h"
# endif
# include "ibp_resources.h"
# include "fs.h"

ulong_t ulfsGetBlockSize ( FAT *fat ){
    return (fat->blksize);
}

ulong_t ulfsGetBlockNo( FAT *fat, const char *filename, ulong_t offset){
    FATRecord *record;
    ulong_t blockNo = 0;

    if ( (record = ibpfs_searchFAT(fat,filename)) != NULL ){
        blockNo =offset/fat->blksize +1;
    }

    return (blockNo);
}

ulong_t ulfsGetNumofBlock( FAT *fat, ulong_t length){
    return ( length/fat->blksize + 1 );
}

int Socket2ULFS (int sockfd,IBPFILE *fp, u_int64_t offset, ulong_t *size, time_t pl_expireTime) {
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
      if ( (n2 = ibpfs_store(fp,ptr,n1-writeCount,offset)) < 0 ){
          free(dataBuf);
	      return IBP_E_FILE_WRITE;
      }
      offset += n2;
      ptr += n2;
      writeCount += n2;
    }
    readCount += n1;
  }
  
  *size = readCount;
  free(dataBuf);
  return IBP_OK;
}

void handle_lsd_end_ulfs (RESOURCE *rs , IBPFILE *pf_f, IBP_byteArray ps_container) {
  ibpfs_close(pf_f);
  ps_container->readLock--;

  return;
}

/*****************************/
/* copy_ULFS2Socket          */
/* It copys ByteArray file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_ULFS2Socket(IBPFILE *pf_fp,IBP_cap ps_cap, IBP_timer ps_timer, ulong_t pl_size, ulong_t offset, time_t pl_expireTime) 
{
  long   n1, readCount;
  char   *dataBuf;
  long   readChunk;
  int    li_ret;
  
  dataBuf = (char *) malloc (DATA_BUF_SIZE);
  if (dataBuf == NULL)
  {
    IBP_errno = IBP_E_GENERIC;
    return(IBP_errno);
  }

  n1 = readCount = 0;
  while (readCount < pl_size) {
    if (pl_expireTime < time(NULL)) {
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    readChunk = IBP_MIN (DATA_BUF_SIZE, pl_size-readCount);
    n1 = ibpfs_load(pf_fp,dataBuf,readChunk,offset);
    if (n1 < 0) {
      free(dataBuf);
      return IBP_E_FILE_READ;
    }
    if ((li_ret = IBP_store(ps_cap, ps_timer, dataBuf, n1)) <= 0) {
      if (readCount == 0) {
        free(dataBuf);
        return(IBP_errno);
      }
      else {
        free(dataBuf);
        return(readCount);
      }
    }
    readCount += n1;
    offset += n1;
  }
  free(dataBuf);
  return(readCount);
}
void handle_copy_ulfs ( RESOURCE *rs, int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  int                 li_HeaderLen, li_ret = IBP_OK;
  unsigned long int   ll_TrueSize;
  char                lc_LineBuf[CMD_BUF_SIZE];
  IBPFILE                *lf_f = NULL;
  FAT                 *fat;
  int                 li_nextstep = 0;
  int                 li_response;
  int                 li_lsd = 0;
  unsigned long       ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
  IBP_cap             ls_cap;
  struct ibp_timer    ls_timeout;
  time_t              lt_expireTime;
  ulong_t           offset;

#ifdef HAVE_PTHREAD_H
  Thread_Info         *info;
  int                 left_time;
  int                 start_time;
  struct timespec     spec;
#endif

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_copy beginning\n");
#endif

  if (ibp_cmd[pi_nbt].Cmd == IBP_SEND) {
    li_lsd = 20;
  }

  ps_container->readLock++;
  fat = (FAT*)(rs->loc.ulfs.ptr);

#ifdef IBP_DEBUG
  fprintf(stderr, "T %d, handle copy\n",pi_nbt);
#endif

  if (ibp_cmd[pi_nbt].ServerSync != 0) {
    lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
  }
  else {
    ibp_cmd[pi_nbt].ServerSync = 3600*24;
    lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
  }

  li_nextstep += ps_container->attrib.type;

  while (li_nextstep > HLSD_DONE) {
    switch (li_nextstep) {

    case HLSD_INIT_BA:
    case HLSD_INIT_BU:
    case HLSD_INIT_FI:
    case HLSD_INIT_CQ:

      /*
       *  open the file and read the header
       */

      lf_f = ibpfs_open(fat,ps_container->fname);

      if (lf_f == NULL) {
        handle_error (IBP_E_FILE_OPEN, &pi_fd);
        li_nextstep = HLSD_DONE;
        continue;
      }

      if ( ibpfs_load(lf_f,&li_HeaderLen,sizeof(int),0) < 0){
        ibpfs_close(lf_f);
        handle_error (IBP_E_FILE_ACCESS, &pi_fd);
        li_nextstep = HLSD_DONE;
        continue;
      }
      li_HeaderLen = (li_HeaderLen/fat->blksize + 1)*fat->blksize;

      li_nextstep += IBP_STEP;
      break;


    case HLSD_SEEK_BA:
    case HLSD_SEEK_BU:
      if (ibp_cmd[pi_nbt].size >= 0) {
        ll_TrueSize = ibp_cmd[pi_nbt].size;

#ifdef HAVE_PTHREAD_H
        if (ps_container->currentSize < ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
          left_time = ibp_cmd[pi_nbt].ServerSync;
          info = (Thread_Info *)ps_container->thread_data;
          info->read_wait++;
          if (info->read != -1) {
            start_time = time(NULL);
            spec.tv_sec = time(NULL) + left_time;
            spec.tv_nsec = 0;
            pthread_mutex_lock(&filock);
            if (pthread_cond_timedwait(&(info->read_protect), &filock, &spec) != 0){
              handle_error (IBP_E_BAD_FORMAT, &pi_fd);
              pthread_mutex_unlock(&filock);
              info->read_wait--;
              li_nextstep = HLSD_DONE;
              break;
            }
            else {
              left_time -= (time(NULL) - start_time);
              pthread_mutex_unlock(&filock);
            }
          }
          if (ps_container->currentSize < ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
            info->read = ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size;
            spec.tv_sec = time(NULL) + left_time;
            spec.tv_nsec = 0;
            pthread_mutex_lock(&filock);
            if (pthread_cond_timedwait(&(info->read_cond), &filock, &spec) != 0){
              info->read = -1;
              info->read_wait--;
              if (ps_container->currentSize <= ibp_cmd[pi_nbt].Offset) {
                pthread_mutex_unlock(&filock);
                handle_error (IBP_E_FILE_ACCESS, &pi_fd);
                if (info->read_wait > 0) {
                  pthread_cond_signal(&(info->read_protect));
                }
                li_nextstep = HLSD_DONE;
                break;
              }
              else {
                info->read = -1;
                info->read_wait--;
                pthread_mutex_unlock(&filock);
                if (info->read_wait > 0) {
                  pthread_cond_signal(&(info->read_protect));
                }
              }
            }
            else {
              info->read = -1;
              info->read_wait--;
              pthread_mutex_unlock(&filock);
              if (info->read_wait > 0) {
                pthread_cond_signal(&(info->read_protect));
              }
            }
          }
          else {
            info->read_wait--;
            if (info->read_wait > 0) {
              pthread_cond_signal(&(info->read_protect));
            }
          }
        }

#endif
        ll_TrueSize =
            (ps_container->currentSize <= ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size ?
             (ps_container->currentSize > ibp_cmd[pi_nbt].Offset ?
              ps_container->currentSize - ibp_cmd[pi_nbt].Offset : 0) : ibp_cmd[pi_nbt].size);
        offset = li_HeaderLen + ibp_cmd[pi_nbt].Offset;
      }
      else {
        offset = li_HeaderLen;
        ll_TrueSize = ps_container->currentSize;
      }
      li_nextstep += IBP_STEP;
      li_nextstep += li_lsd;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_copy size determined\n");
#endif

      break;

    case HLSD_SEEK_FI:
    case HLSD_SEEK_CQ:
      handle_error(IBP_E_TYPE_NOT_SUPPORTED,&pi_fd);
      li_nextstep = HLSD_DONE;
      break;
      ll_wrcount = ps_container->wrcount;
      ll_rdcount = ps_container->rdcount;
      ll_free    = ps_container->free;
      ll_size    = ps_container->size;

      ll_readptr = ll_rdcount;
# ifdef IBP_DEBUG
      fprintf (stderr, "T %d, Lock file :%lu:%lu:%lu:%lu:\n", pi_nbt, ll_wrcount, ll_rdcount, ll_free, ll_size);
# endif

      if (ibp_cmd[pi_nbt].size > (ll_size - ll_free)) {
#ifdef HAVE_PTHREAD_H
        info = (Thread_Info *)ps_container->thread_data;
        info->read = ibp_cmd[pi_nbt].size;
        spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
        spec.tv_nsec = 0;
        pthread_mutex_lock (&filock);
        if (pthread_cond_timedwait(&(info->read_cond), &filock, &spec) != 0) {
          info->read = -1;
          handle_error (IBP_E_GENERIC, &pi_fd);
          li_nextstep = HLSD_DONE;
          pthread_mutex_unlock (&filock);
          break;
        }
        else {
          info->read = -1;
          li_nextstep = ps_container->attrib.type;
          pthread_mutex_unlock(&filock);
          break;
        }
# else
        handle_error (IBP_E_GENERIC, &pi_fd);
        li_nextstep = HLSD_DONE;
        continue;
#endif
      }
      if (ibp_cmd[pi_nbt].size > 0) {
        ll_TrueSize = (ll_size - ll_free < ibp_cmd[pi_nbt].size) ? ll_size - ll_free : ibp_cmd[pi_nbt].size;
        offset = li_HeaderLen + ll_rdcount;
      }
      else {
        offset = li_HeaderLen ;
        ll_TrueSize = ll_size - ll_free;
      }

      li_nextstep += IBP_STEP;
      li_nextstep += li_lsd;
      break;

    case HSC_SEND_BA:
    case HSC_SEND_BU:

      ls_cap = (char *) malloc (strlen(ibp_cmd[pi_nbt].Cap) + 1);
      if (ls_cap == NULL) {
        handle_error (IBP_E_INTERNAL, &pi_fd);
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap); 
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  before copy_File2Socket \n");
#endif

      li_ret = copy_ULFS2Socket (lf_f, ls_cap, &ls_timeout, ll_TrueSize,offset, lt_expireTime);


#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  after  copy_File2Socket\n");
#endif

      SaveInfo2LSS (rs,ps_container, 2);

      li_nextstep += IBP_STEP;
      li_nextstep += IBP_STEP;
      break;
 
    case HSC_SEND_FI:

      ls_cap = (char *) malloc (strlen(ibp_cmd[pi_nbt].Cap) + 1);
      if (ls_cap == NULL) {
        handle_error (IBP_E_INTERNAL, &pi_fd);
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;
      /*
      li_ret = copy_Fifo2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize, ps_container->maxSize,ll_readptr, li_HeaderLen, lt_expireTime);
      */
      if (li_ret > 0)
      {
        ll_rdcount = (ll_rdcount + li_ret) % ll_size;
        ll_free += li_ret;
        ps_container->size = ll_size;
        ps_container->wrcount = ll_wrcount;
        ps_container->rdcount = ll_rdcount;
        ps_container->free = ll_free;
        SaveInfo2LSS(rs,ps_container, 2);
      }

      li_nextstep += IBP_STEP;
      li_nextstep += IBP_STEP;
# ifdef HAVE_PTHREAD_H
      info = (Thread_Info *)ps_container->thread_data;
      if ((info->write > 0) && (ll_free >= info->write)) {
        pthread_cond_signal(&(info->write_cond));
      }
# endif
      break;


    case HSC_SEND_CQ:

      ibp_cmd[pi_nbt].Offset=0;
      ls_cap = (char *) malloc (strlen(ibp_cmd[pi_nbt].Cap) + 1);
      if (ls_cap == NULL) {
        handle_error (IBP_E_INTERNAL, &pi_fd);
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

      /*
      li_ret = copy_Fifo2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize, ps_container->maxSize,ll_readptr, li_HeaderLen, lt_expireTime);
      */
      if (li_ret > 0)
      {
        ll_rdcount = (ll_rdcount + li_ret) % ll_size;
        ll_free += li_ret;
        ps_container->size = ll_size;
        ps_container->wrcount = ll_wrcount;
        ps_container->rdcount = ll_rdcount;
        ps_container->free = ll_free;
        SaveInfo2LSS(rs,ps_container, 2);
      }

      li_nextstep += IBP_STEP;
      li_nextstep += IBP_STEP;
      break;


    case HSC_AFTER_BA:
    case HSC_AFTER_BU:
    case HSC_AFTER_FI:
    case HSC_AFTER_CQ:
      if (li_ret < 0) {
        IBPErrorNb[pi_nbt] = li_ret;
        sprintf(lc_LineBuf,"%d \n", IBPErrorNb[pi_nbt]);
        Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        li_nextstep = HLSD_DONE;
        continue;
      }
     
      li_response = IBP_OK;
      sprintf (lc_LineBuf, "%d %d\n", li_response, li_ret);
      Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
      li_nextstep = HLSD_DONE;
      break;

    }
  }

  ibpfs_close(lf_f);

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_copy finished\n");
#endif
#ifdef IBP_DEBUG
  fprintf(stderr, "T %d, handle copy done\n",pi_nbt);
#endif
  ps_container->readLock--;

#if 0
  /* check whether this capability has been deleted */
  if (ps_container->deleted == 1)
  {
    if ((ps_container->writeLock == 0) && (ps_container->readLock == 0))
    {
      SaveInfo2LSS(rs,ps_container, 2);
      jettisonByteArray(rs,ps_container);
    }
  }
#endif
  if (ls_cap != NULL)
  {
    free(ls_cap);
  }

  return;

}

/*****************************/
/* handle_lsd_ulfs           */
/* It handles the user's load and delieve requirement, envoked by handle_require() */
/* It sends the data saved a temporary file on the disk to the user  */
/* return: none              */
/*****************************/

void handle_lsd_ulfs ( RESOURCE *rs ,int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  int                 li_HeaderLen, li_ret = IBP_OK;
  unsigned long int   ll_TrueSize;
  char                lc_LineBuf[CMD_BUF_SIZE];
  IBPFILE                *lf_f = NULL;
  FAT                 *fat;
  int		              li_nextstep = 1;
  char                lc_generic[MAX_CAP_LEN];
  char                *lc_TargetKey, *lc_WriteTag, *lc_GenII;
  int                 li_fddata = 0, li_response;
  int                 li_lsd = 0;
  unsigned long       ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
  char	              **lp_fields;
  int	                li_nfields;
  int                 li_timeout;
  time_t              lt_expireTime;
#ifdef HAVE_PTHREAD_H
  Thread_Info         *info;
  int                 left_time;
  int                 start_time;
  struct timespec     spec;
  ulong_t           offset;
#endif


  assert (rs != NULL);
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_lsd beginning\n");
  fprintf(stderr, "T %d, handle lsd\n",pi_nbt);
#endif
  
  fat = (FAT*)(rs->loc.ulfs.ptr);
  if (ibp_cmd[pi_nbt].ServerSync != 0) {
    lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
  }
  else {
    lt_expireTime = 0;
  }
  
  /*
   *  open the file and read the header
   */
  lf_f = ibpfs_open(fat,ps_container->fname);
 
 
  if (lf_f == NULL) {
    handle_error (IBP_E_FILE_OPEN, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_FILE_OPEN;
    ps_container->reference --;
    return;
  }

  /* read lock added */ 
  ps_container->readLock++;

  if (ibpfs_load(lf_f,&li_HeaderLen,sizeof(int),0) < 0 ){
    handle_error (IBP_E_FILE_ACCESS, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
    handle_lsd_end_ulfs(rs,lf_f, ps_container);
    return;
  }
  

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    if (ibp_cmd[pi_nbt].size > 0) {
      ll_TrueSize = ibp_cmd[pi_nbt].size;
      
#ifdef HAVE_PTHREAD_H
      if (ps_container->currentSize < ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
	      left_time = ibp_cmd[pi_nbt].ServerSync;
	      info = (Thread_Info *)ps_container->thread_data;
	      info->read_wait++;
	      if (info->read != -1) {
	        start_time = time(NULL);
	        spec.tv_sec = time(NULL) + left_time;
	        spec.tv_nsec = 0;
	        pthread_mutex_lock(&filock);
	        if (pthread_cond_timedwait(&(info->read_protect), &filock, &spec) != 0){
	          handle_error (IBP_E_BAD_FORMAT, &pi_fd);
            IBPErrorNb[pi_nbt] = IBP_E_BAD_FORMAT;
	          pthread_mutex_unlock(&filock);
	          info->read_wait--;
	          li_nextstep = HLSD_DONE;
	          break;
	        }
	        else {
	          left_time -= (time(NULL) - start_time);
	          pthread_mutex_unlock(&filock);
	        }
	      }
	      if (ps_container->currentSize < ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
	        info->read = ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size;
	        spec.tv_sec = time(NULL) + left_time;
	        spec.tv_nsec = 0;
	        pthread_mutex_lock(&filock);
	        if (pthread_cond_timedwait(&(info->read_cond), &filock, &spec) != 0){
	          info->read = -1;
	          info->read_wait--;
	          if (ps_container->currentSize <= ibp_cmd[pi_nbt].Offset) {
	            pthread_mutex_unlock(&filock);
	            handle_error (IBP_E_SERVER_TIMEOUT, &pi_fd);
              IBPErrorNb[pi_nbt] = IBP_E_SERVER_TIMEOUT;
	            if (info->read_wait > 0) {
		            pthread_cond_signal(&(info->read_protect));
	            }
	            li_nextstep = HLSD_DONE;
	            break;
	          }
	          else {
	            info->read = -1;
	            info->read_wait--;
	            pthread_mutex_unlock(&filock);
	            if (info->read_wait > 0) {
		            pthread_cond_signal(&(info->read_protect));
	            }
	          }
	        }
	        else {
	          info->read = -1;
	          info->read_wait--;
	          pthread_mutex_unlock(&filock);
	          if (info->read_wait > 0) {
	            pthread_cond_signal(&(info->read_protect));
	          } 
	        }
	      }
	      else {
	        info->read_wait--;
	        if (info->read_wait > 0) {
	          pthread_cond_signal(&(info->read_protect));
	        }
	      }
      }
 
#endif
      ll_TrueSize =
	(ps_container->currentSize <= ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size ?
	 (ps_container->currentSize > ibp_cmd[pi_nbt].Offset ?
	  ps_container->currentSize - ibp_cmd[pi_nbt].Offset : 0) : ibp_cmd[pi_nbt].size);
      offset = (li_HeaderLen/fat->blksize +1)*fat->blksize + ibp_cmd[pi_nbt].Offset;
    }
    else {
      offset = (li_HeaderLen/fat->blksize +1)*fat->blksize;
      ll_TrueSize = ps_container->currentSize;
    }
    break;
    
  case IBP_FIFO:
  case IBP_CIRQ:
    IBPErrorNb[pi_nbt] = IBP_E_TYPE_NOT_SUPPORTED;
    handle_error (IBPErrorNb[pi_nbt], &pi_fd);
    li_nextstep = HLSD_DONE;
    break;
    /* 
    ll_wrcount = ps_container->wrcount;
    ll_rdcount = ps_container->rdcount;
    ll_free    = ps_container->free;
    ll_size    = ps_container->size;
    ll_readptr = ll_rdcount;

# ifdef IBP_DEBUG
    fprintf (stderr, "T %d, Lock file :%lu:%lu:%lu:%lu:\n", pi_nbt, ll_wrcount, ll_rdcount, ll_free, ll_size); 
# endif
    
    if (ibp_cmd[pi_nbt].size > (ll_size - ll_free)) {
#ifdef HAVE_PTHREAD_H
      info = (Thread_Info *)ps_container->thread_data;
      info->read = ibp_cmd[pi_nbt].size;
      spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
      spec.tv_nsec = 0;
      pthread_mutex_lock (&filock);
      if (pthread_cond_timedwait(&(info->read_cond), &filock, &spec) != 0) {
	      info->read = -1;
	      pthread_mutex_unlock (&filock);
        if (ps_container->size - ps_container->free == 0)
        {
	        handle_error (IBP_E_GENERIC, &pi_fd);
	        li_nextstep = HLSD_DONE;
	        break;
        }
      }
      else {
	      info->read = -1;
	      pthread_mutex_unlock(&filock);
      } 
# else
      handle_error (IBP_E_GENERIC, &pi_fd);
      li_nextstep = HLSD_DONE;
      break;
#endif
    }
    ll_wrcount = ps_container->wrcount;
    ll_rdcount = ps_container->rdcount;
    ll_free    = ps_container->free;
    ll_size    = ps_container->size;

    if (ibp_cmd[pi_nbt].size > 0) {
      if (ibp_cmd[pi_nbt].size <= (ll_size - ll_free)) {
        ll_TrueSize = ibp_cmd[pi_nbt].size;
      }
      else
      {
        ll_TrueSize = ll_size - ll_free;
      }

      if (fseek(lf_f, li_HeaderLen + ll_rdcount, SEEK_SET) == -1) {   
	      handle_error (IBP_E_FILE_ACCESS, &pi_fd);
	      li_nextstep = HLSD_DONE;
	      break;
      }
    }
    else {
      if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
    	  handle_error (IBP_E_FILE_ACCESS, &pi_fd);
	      li_nextstep = HLSD_DONE;
	      break;
      }
      ll_TrueSize = ll_size - ll_free;
    }
    break;    
  */
  default:
    break;
  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_ulfs(rs,lf_f, ps_container);
    return;
  }


  switch (ibp_cmd[pi_nbt].Cmd) {
  case IBP_LOAD:
    sprintf (lc_LineBuf,"%d %lu \n", IBP_OK, ll_TrueSize);
    
    if (Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      li_nextstep = HLSD_DONE;
      break;
    }
    if ((ibp_cmd[pi_nbt].size == -2) || (ll_TrueSize == 0)) {
      li_nextstep = HLSD_DONE;
      break;
    }
    li_fddata = pi_fd;
    break;

  case IBP_SEND_BU:
      strcpy(lc_generic, ibp_cmd[pi_nbt].Cap);
      li_nfields = GetCapFieldsNum(lc_generic, '/');
      if ( li_nfields < 6 ) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      lp_fields = malloc(sizeof(char *)*(li_nfields));
      if ( lp_fields == NULL ){
        handle_error (IBP_E_INTERNAL, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
        li_nextstep = HLSD_DONE;
        break;
      }

      if ( GetCapFields( lp_fields,lc_generic,'/') < 0 ) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      ibp_cmd[pi_nbt].host = lp_fields[2];
      lc_TargetKey = lp_fields[3];
      lc_GenII = lp_fields[4];
      lc_WriteTag = lp_fields[5];

      if ( GetCapFields( lp_fields, ibp_cmd[pi_nbt].host, ':') < 0 ) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }
  
      ibp_cmd[pi_nbt].host = lp_fields[0];
      ibp_cmd[pi_nbt].port = atoi (lp_fields[1]);

      free(lp_fields);

      if ((ibp_cmd[pi_nbt].host == NULL) || (lc_TargetKey == NULL) || (lc_GenII == NULL)) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      if (lc_WriteTag == NULL) {
	      handle_error (IBP_E_CAP_NOT_WRITE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      if (strcmp (lc_WriteTag,"WRITE") != 0) {
	      handle_error (IBP_E_CAP_NOT_WRITE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      strcpy(ibp_cmd[pi_nbt].wKey,lc_GenII);

      li_fddata = SocketConnect (ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

      if (li_fddata == -1) {
	      handle_error (IBP_E_CONNECTION, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      li_timeout = 0;

      sprintf(lc_LineBuf, "%d %d %s %s %lu %d \n", 
	      IBPv031,      
	      IBP_STORE, 
	      lc_TargetKey, 
	      ibp_cmd[pi_nbt].wKey, 
	      ll_TrueSize,
        li_timeout);

      if (Write (li_fddata, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
	      handle_error(IBP_E_SOCK_WRITE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      if (ReadLine(li_fddata, lc_LineBuf, sizeof(lc_LineBuf)) == -1) {
	      handle_error (IBP_E_SOCK_READ, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      if (sscanf(lc_LineBuf,"%d", &li_response) != 1 ) {
	      handle_error (IBP_E_BAD_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      if (li_response != IBP_OK) {
	      handle_error (li_response, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      break;
  default:
    break;

  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_ulfs(rs,lf_f, ps_container);
    return;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    li_ret = ULFS2Socket(lf_f, li_fddata, ll_TrueSize, offset,lt_expireTime);
    SaveInfo2LSS(rs,ps_container, 2);
    break;
/*
  case IBP_FIFO:
    li_ret = Fifo2Socket(lf_f, li_fddata, ll_TrueSize,
			 ps_container->maxSize,ll_readptr,li_HeaderLen,lt_expireTime);
    if (li_ret >= 0)
    {
      ll_rdcount = (ll_rdcount + ll_TrueSize) % ll_size;
      ll_free += ll_TrueSize;
      ps_container->size = ll_size;
      ps_container->wrcount = ll_wrcount;
      ps_container->rdcount = ll_rdcount;
      ps_container->free = ll_free;
      SaveInfo2LSS(rs,ps_container, 2);
    }

# ifdef HAVE_PTHREAD_H
    info = (Thread_Info *)ps_container->thread_data;
    if ((info->write > 0) && (ll_free >= info->write)) {
      pthread_cond_signal(&(info->write_cond));
    }
# endif
    break;

  case IBP_CIRQ:
    if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND) || (ibp_cmd[pi_nbt].Cmd == IBP_DELIVER))
      ibp_cmd[pi_nbt].Offset=0;
    li_ret = Fifo2Socket(lf_f, li_fddata, ll_TrueSize,
			 ps_container->maxSize,ll_readptr,li_HeaderLen,lt_expireTime);
    if (li_ret >= 0)
    {
      ll_rdcount = (ll_rdcount + ll_TrueSize) % ll_size;
      ll_free += ll_TrueSize;
      ps_container->size = ll_size;
      ps_container->wrcount = ll_wrcount;
      ps_container->rdcount = ll_rdcount;
      ps_container->free = ll_free;
      SaveInfo2LSS(rs,ps_container, 2);
    }
    break;
*/  
  default:
    break;
  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_ulfs(rs,lf_f, ps_container);
    return;
  }

  switch (ibp_cmd[pi_nbt].Cmd) {
  case IBP_LOAD:
    break;
    
  case IBP_SEND_BU:
    if (li_ret != IBP_OK) {
      IBPErrorNb[pi_nbt] = li_ret;
      sprintf(lc_LineBuf,"%d \n", IBPErrorNb[pi_nbt]);
      Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
      li_nextstep = HLSD_DONE;
      break;
    }
    
    if (ReadLine(li_fddata, lc_LineBuf, sizeof(lc_LineBuf)) == -1) {
      IBPErrorNb[pi_nbt] = IBP_E_SOCK_READ;
      handle_error (IBPErrorNb[pi_nbt], &pi_fd);
      close (li_fddata); 
      li_nextstep = HLSD_DONE;
      break;
    }
    close (li_fddata); 
    
    if (sscanf (lc_LineBuf, "%d %lu\n", &li_response, &ll_TrueSize) != 2) {
      IBPErrorNb[pi_nbt] = IBP_E_BAD_FORMAT;
      handle_error (IBPErrorNb[pi_nbt], &pi_fd);
      li_nextstep = HLSD_DONE;
      break;
    }
    
    sprintf (lc_LineBuf, "%d %lu\n", li_response, ll_TrueSize);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  case IBP_DELIVER:
    if (li_ret != IBP_OK) {
      IBPErrorNb[pi_nbt] = li_ret;
      li_response = IBPErrorNb[pi_nbt];
    }
    else
      li_response = IBP_OK;
    
    close (li_fddata);
    sprintf (lc_LineBuf, "%d %lu \n", li_response, ll_TrueSize);
    Write (pi_fd, lc_LineBuf, strlen (lc_LineBuf));
    break;
   
  default:
    break;
  }

  handle_lsd_end_ulfs(rs,lf_f, ps_container);
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_lsd finished\n");
#endif

  return;
}

int ULFS2Socket(IBPFILE *pf_fp, int pi_sockfd, ulong_t pl_size, ulong_t offset, time_t  pl_expireTime) {
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

    readChunk = IBP_MIN (DATA_BUF_SIZE, pl_size-readCount);
    n1 = ibpfs_load(pf_fp,dataBuf,readChunk,offset);
    if (n1 < 0) {
      free(dataBuf);
      return IBP_E_FILE_READ;
    }
    offset += readChunk;

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

int storeToULFS( RESOURCE       *rs,
                 IBP_byteArray  ps_container,
                 int            pi_nbt,
                 int            pi_fd,
                 int            pi_srcfd,
                 ulong_t        ll_writeptr, 
                 unsigned long  ll_size,
                 unsigned long  ll_free,
                 unsigned long  ll_wrcount,
                 unsigned long  ll_rdcount,
                 unsigned long  *ll_written,
                 time_t         lt_expireTime,
                 char           *lc_LineBuf){

  IBPFILE    *lf_f;
  FAT        *fat;
  int     li_HeaderLen;
  int     li_nextstep = IBP_OK;
  int     li_Ret;
  u_int64_t offset;

#ifdef HAVE_PTHREAD_H
  Thread_Info           *info;
#endif

  fat = (FAT*)(rs->loc.ulfs.ptr);
  lf_f = ibpfs_open(fat,ps_container->fname);
  if (lf_f == NULL) {
    return (IBP_E_FILE_ACCESS) ;
  }

  if (ibpfs_load(lf_f,&li_HeaderLen,sizeof(int),0) < 0){
    ibpfs_close(lf_f);
    return(IBP_E_FILE_ACCESS);
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
      offset = (li_HeaderLen/fat->blksize +1)*fat->blksize + ibp_cmd[pi_nbt].Offset;
      break;
  case IBP_BUFFER:
      offset = (li_HeaderLen/fat->blksize +1)*fat->blksize;
      break;

  case IBP_FIFO:
  case IBP_CIRQ:
      offset = (li_HeaderLen/fat->blksize +1)*fat->blksize + ll_writeptr;
       break;

  default:
    break;
  }


  if (li_nextstep == DONE ) {
    return(IBPErrorNb[pi_nbt]);
  }

  sprintf(lc_LineBuf, "%d \n", IBP_OK);
  if (Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
    return (IBP_E_SOCK_WRITE);
  }


  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    li_Ret = Socket2ULFS(pi_srcfd, lf_f, offset, &ibp_cmd[pi_nbt].size, lt_expireTime);
    if (li_Ret != IBP_OK) {
      IBPErrorNb[pi_nbt] = li_Ret;
      li_nextstep = DONE;
      break;
    }
    *ll_written = ibp_cmd[pi_nbt].size;
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    IBPErrorNb[pi_nbt] = IBP_E_TYPE_NOT_SUPPORTED;
    li_nextstep = DONE;
    break;
    /*
    li_Ret = Socket2Queue (pi_srcfd,
                           lf_f,
                           ibp_cmd[pi_nbt].size,
                           ps_container->maxSize,
                           ll_writeptr,
                           ll_written,
                           li_HeaderLen,
                           lt_expireTime);
    */
    if (li_Ret != IBP_OK) {
      IBPErrorNb[pi_nbt] = li_Ret;
      /*handle_error (IBPErrorNb[pi_nbt], &pi_fd);*/
      li_nextstep = DONE;
      break;
    }
    ps_container->size = ll_size;
    ps_container->wrcount = ll_wrcount;
    ps_container->rdcount = ll_rdcount;
    ps_container->free = ll_free;

    break;

  deafult:
    break;
  }

  if (li_nextstep == DONE ) {
    /*handle_store_end(pi_fd, pi_nbt, ps_container);*/
    return ( IBPErrorNb[pi_nbt]) ;
  }

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_store data transfered\n");
#endif

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
    if ( ps_container->currentSize < ibp_cmd[pi_nbt].Offset +  ibp_cmd[pi_nbt].size ){
      ps_container->currentSize = ibp_cmd[pi_nbt].Offset +  ibp_cmd[pi_nbt].size;
    }
#ifdef HAVE_PTHREAD_H
    info = (Thread_Info *) ps_container->thread_data;
    if ((info->read > -1) && (ps_container->currentSize >= info->read)) {
      pthread_cond_signal(&(info->read_cond));
    }
#endif
    break;

  case IBP_BUFFER:
    ps_container->currentSize = ibp_cmd[pi_nbt].size;
#ifdef HAVE_PTHREAD_H
    info = (Thread_Info *) ps_container->thread_data;
    if ((info->read > -1) && (ps_container->currentSize >= info->read)) {
      pthread_cond_signal(&(info->read_cond));
    }
#endif
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
#ifdef HAVE_PTHREAD_H
    info = (Thread_Info *) ps_container->thread_data;
    if ((info->read > -1) && (ll_size - ll_free >= info->read)) {
      pthread_cond_signal(&(info->read_cond));
    }
#endif
    break;

  default:
    break;
  }

  ibpfs_store(lf_f,ps_container,BASE_HEADER,sizeof(int));
  ibpfs_close(lf_f);

  return (IBP_OK);

}
