/*
 *           IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *         Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
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
 * ibp_load.c
 *
 * IBP server code. Version name: primeur
 * Library of functions used by the server
 *
 */
# include "config-ibp.h"
# ifdef STDC_HEADERS 
# include <sys/types.h>
# include <assert.h>
# endif /* STDC_HEADERS */
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_thread.h"

void handle_lsd_end_disk (RESOURCE *rs , FILE *pf_f, IBP_byteArray ps_container) {
  fclose(pf_f);
  ps_container->readLock--;

  return;
}


/*****************************/
/* handle_lsd_disk           */
/* It handles the user's load and delieve requirement, envoked by handle_require() */
/* It sends the data saved a temporary file on the disk to the user  */
/* return: none              */
/*****************************/

void handle_lsd_disk ( RESOURCE *rs ,int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  int                 li_HeaderLen, li_ret = IBP_OK;
  unsigned long int   ll_TrueSize;
  char                lc_LineBuf[CMD_BUF_SIZE];
  FILE                *lf_f = NULL;
  int		              li_nextstep = 1;
  char                lc_generic[MAX_CAP_LEN];
  char                *lc_TargetKey, *lc_WriteTag, *lc_GenII;
  int                 li_fddata = 0, li_response;
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
#endif


  assert (rs != NULL);
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_lsd beginning\n");
  fprintf(stderr, "T %d, handle lsd\n",pi_nbt);
#endif

  if (ibp_cmd[pi_nbt].ServerSync != 0) {
    lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
  }
  else {
    lt_expireTime = 0;
  }
  
  /*
   *  open the file and read the header
   */
  lf_f = fopen(ps_container->fname, "r");
  
  if (lf_f == NULL) {
    handle_error (IBP_E_FILE_OPEN, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_FILE_OPEN;
    /* 
    ps_container->reference --;
    */
    return;
  }

  /* read lock added */ 
  ps_container->readLock++;
 
  if (fread(&li_HeaderLen, sizeof(int), 1, lf_f) == 0) {
    handle_error (IBP_E_FILE_ACCESS, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
    handle_lsd_end_disk(rs,lf_f, ps_container);
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
      if (fseek(lf_f, li_HeaderLen + ibp_cmd[pi_nbt].Offset, SEEK_SET) == -1) {   
	      handle_error (IBP_E_FILE_ACCESS, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
	      li_nextstep = HLSD_DONE;
	      break;
      }
    }
    else {
      if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
	      handle_error (IBP_E_FILE_ACCESS, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      ll_TrueSize = ps_container->currentSize;
    }
    break;
    
  case IBP_FIFO:
  case IBP_CIRQ:
    if (ps_container->key == NULL) {
      IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
      handle_error (IBPErrorNb[pi_nbt], &pi_fd);
      li_nextstep = HLSD_DONE;
      break;
    }
    
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
          IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
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
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
	      li_nextstep = HLSD_DONE;
	      break;
      }
    }
    else {
      if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
    	  handle_error (IBP_E_FILE_ACCESS, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      ll_TrueSize = ll_size - ll_free;
    }
    break;    

  default:
    break;
  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_disk(rs,lf_f, ps_container);
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
        IBPErrorNb[pi_nbt] = IBP_E_WRONG_CAP_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      lp_fields = malloc(sizeof(char *)*(li_nfields));
      if ( lp_fields == NULL ){
        handle_error (IBP_E_INTERNAL, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        break;
      }

      if ( GetCapFields( lp_fields,lc_generic,'/') < 0 ) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_WRONG_CAP_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      ibp_cmd[pi_nbt].host = lp_fields[2];
      lc_TargetKey = lp_fields[3];
      lc_GenII = lp_fields[4];
      lc_WriteTag = lp_fields[5];

      if ( GetCapFields( lp_fields, ibp_cmd[pi_nbt].host, ':') < 0 ) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_WRONG_CAP_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }
  
      ibp_cmd[pi_nbt].host = lp_fields[0];
      ibp_cmd[pi_nbt].port = atoi (lp_fields[1]);

      free(lp_fields);

      if ((ibp_cmd[pi_nbt].host == NULL) || (lc_TargetKey == NULL) || (lc_GenII == NULL)) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_WRONG_CAP_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      if (lc_WriteTag == NULL) {
	      handle_error (IBP_E_CAP_NOT_WRITE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_WRONG_CAP_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      if (strcmp (lc_WriteTag,"WRITE") != 0) {
	      handle_error (IBP_E_CAP_NOT_WRITE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_WRONG_CAP_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      strcpy(ibp_cmd[pi_nbt].wKey,lc_GenII);
#if 0
      if (sscanf(lc_GenII,"%d", &ibp_cmd[pi_nbt].writekey) != 1) {
	      handle_error (IBP_E_WRONG_CAP_FORMAT, &pi_fd);
	      li_nextstep = HLSD_DONE;
	      break;
      }
#endif

      li_fddata = SocketConnect (ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

      if (li_fddata == -1) {
	      handle_error (IBP_E_CONNECTION, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_CONNECTION;
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
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      if (ReadLine(li_fddata, lc_LineBuf, sizeof(lc_LineBuf)) == -1) {
	      handle_error (IBP_E_SOCK_READ, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_READ;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      if (sscanf(lc_LineBuf,"%d", &li_response) != 1 ) {
	      handle_error (IBP_E_BAD_FORMAT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_BAD_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }

      if (li_response != IBP_OK) {
	      handle_error (li_response, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_BAD_FORMAT;
	      li_nextstep = HLSD_DONE;
	      break;
      }
      break;
#if 0
  case IBP_DELIVER:
      li_fddata = SocketConnect (ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

      if (li_fddata == -1) {
	      handle_error (IBP_E_CONNECTION, &pi_fd);
	      li_nextstep = DONE;
	      break;
      }
      break;
#endif
  default:
    break;

  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_disk(rs,lf_f, ps_container);
    return;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    li_ret = File2Socket(lf_f, li_fddata, ll_TrueSize,lt_expireTime);
    SaveInfo2LSS(rs,ps_container, 2);
    break;

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
    }else{
      IBPErrorNb[pi_nbt]=IBP_E_GENERIC;
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
    }else{
      IBPErrorNb[pi_nbt]=IBP_E_GENERIC;
    }
    break;
  
  default:
    break;
  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_disk(rs,lf_f, ps_container);
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

  handle_lsd_end_disk(rs,lf_f, ps_container);
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_lsd finished\n");
#endif

  return;
}

void handle_lsd_end_ram (RESOURCE *rs, IBP_byteArray ps_container) {
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
  return;
}


/*****************************/
/* handle_lsd_ram            */
/* It handles the user's load and delieve requirement, envoked by handle_require() */
/* It sends the data saved a temporary file on the disk to the user  */
/* return: none              */
/*****************************/
void handle_lsd_ram ( RESOURCE *rs ,int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  int                 li_HeaderLen = 0;
  int                 li_ret = IBP_OK;
  unsigned long int   ll_TrueSize;
  char                lc_LineBuf[CMD_BUF_SIZE];
  int		              li_nextstep = 1;
  int                 li_fddata = 0, li_response;
  unsigned long       ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
  time_t              lt_expireTime;
#ifdef IBP_THREAD
  Thread_Info         *info;
  int                 left_time;
  int                 start_time;
  struct timespec     spec;
#endif


  assert (rs != NULL );
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_lsd beginning\n");
  fprintf(stderr, "T %d, handle lsd\n",pi_nbt);
#endif

  if (ibp_cmd[pi_nbt].ServerSync != 0) {
    lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
  }
  else {
    lt_expireTime = 0;
  }
  
  /* read lock added */ 
  ps_container->readLock++;
  
  /*
   *  check first if everything's OK
   */

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    if (ibp_cmd[pi_nbt].size > 0) {
      ll_TrueSize = ibp_cmd[pi_nbt].size;
      
#ifdef IBP_THREAD
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
	    handle_error (IBP_E_SERVER_TIMEOUT, &pi_fd);
      IBPErrorNb[pi_nbt]=IBP_E_SERVER_TIMEOUT;
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
        IBPErrorNb[pi_nbt]=IBP_E_FILE_ACCESS;
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
    }
    else {
      ll_TrueSize = ps_container->currentSize;
    }
    break;
    
  case IBP_FIFO:
  case IBP_CIRQ:
    if (ps_container->key == NULL) {
      IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
      handle_error (IBPErrorNb[pi_nbt], &pi_fd);
      li_nextstep = HLSD_DONE;
      break;
    }
    
    ll_wrcount = ps_container->wrcount;
    ll_rdcount = ps_container->rdcount;
    ll_free    = ps_container->free;
    ll_size    = ps_container->size;
    ll_readptr = ll_rdcount;

# ifdef IBP_DEBUG
    fprintf (stderr, "T %d, Lock file :%lu:%lu:%lu:%lu:\n", pi_nbt, ll_wrcount, ll_rdcount, ll_free, ll_size); 
# endif
    
    if (ibp_cmd[pi_nbt].size > (ll_size - ll_free)) {
#ifdef IBP_THREAD
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
	        handle_error (IBP_E_SERVER_TIMEOUT, &pi_fd);
          IBPErrorNb[pi_nbt]=IBP_E_SERVER_TIMEOUT;
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
      IBPErrorNb[pi_nbt]=IBP_E_GENERIC;
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
    }
    else {
      ll_TrueSize = ll_size - ll_free;
    }
    break;    

  default:
    break;
  }

  if (li_nextstep == HLSD_DONE) {
    handle_lsd_end_ram(rs,ps_container);
    return;
  }

  li_ret = loadFromRam(pi_fd,
                               pi_nbt,
                               li_HeaderLen,
                               ps_container,
                               &ll_TrueSize,
                               &ll_rdcount,
                               &ll_wrcount,
                               ll_size,
                               &ll_free,
                               &lt_expireTime,
                               ll_readptr,
                               &li_fddata);

  if (li_ret != IBP_OK) {
    handle_error (li_ret, &pi_fd);
    IBPErrorNb[pi_nbt]=li_ret;
    handle_lsd_end_ram(rs,ps_container);
    return;
  }


  if (ps_container->attrib.type == IBP_FIFO) {
# ifdef IBP_THREAD
    info = (Thread_Info *)ps_container->thread_data;
    if ((info->write > 0) && (ll_free >= info->write)) {
      pthread_cond_signal(&(info->write_cond));
    }
# endif
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

  handle_lsd_end_ram(rs,ps_container);
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_lsd finished\n");
#endif

  return;
}

