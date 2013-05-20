
# include "config-ibp.h"
# ifdef STDC_HEADERS
# include <sys/types.h>
# include <assert.h>
# endif
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_thread.h"

static int copy_File2Socket(FILE *pf_fp,
                            IBP_cap ps_cap, 
                            IBP_timer ps_timer, 
                            ulong_t pl_size, 
                            time_t pl_expireTime) ;

static int copy_Fifo2Socket (FILE *pf_fp,
                 IBP_cap ps_cap,
                 IBP_timer ps_timer,
                 ulong_t pl_size,
                 ulong_t pl_fifoSize,
                 ulong_t pl_rdPtr, 
                 int     pi_HeaderLen,
                 time_t pl_expireTime) ;


static int copy_File2Socket_ram(char *pc_buffer, 
                                int pl_offset, 
                                IBP_cap ps_cap, 
                                IBP_timer ps_timer, 
                                ulong_t pl_size, 
                                time_t pl_expireTime) ;

int copy_Fifo2Socket_ram (char *pc_buffer,
                 IBP_cap ps_cap,
                 IBP_timer ps_timer,
                 ulong_t pl_size,
                 ulong_t pl_fifoSize,
                 ulong_t pl_rdPtr, 
                 int     pi_HeaderLen,
                 time_t pl_expireTime) ;



/*****************************/
/* handle_copy               */
/* It handles the user's copy requirement, envoked by handle_require() */
/* It sends the data to another IBP depot  */
/* return: none              */
/*****************************/
void handle_copy_ram(RESOURCE *rs ,int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  int                 li_HeaderLen, li_ret = IBP_OK;
  unsigned long int   ll_TrueSize;
  char                lc_LineBuf[CMD_BUF_SIZE];
  FILE                *lf_f = NULL;
  int                 li_nextstep = 0;
  int                 li_response;
  int                 li_lsd = 0;
  unsigned long       ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
  IBP_cap             ls_cap = NULL;
  struct ibp_timer    ls_timeout;
  time_t              lt_expireTime;

#ifdef IBP_THREAD
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

#ifdef IBP_DEBUG
  fprintf(stderr, "T %d, handle copy\n",pi_nbt);
#endif

  if (ibp_cmd[pi_nbt].ServerSync != 0) {
    lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
  }
  else {
    lt_expireTime = 0;
  }

  li_nextstep += ps_container->attrib.type;

  while (li_nextstep > HLSD_DONE) {
    switch (li_nextstep) {

    case HLSD_INIT_BA:
    case HLSD_INIT_BU:
    case HLSD_INIT_FI:
    case HLSD_INIT_CQ:

      li_nextstep += IBP_STEP;
    break;


    case HLSD_SEEK_BA:
    case HLSD_SEEK_BU:
      if (ibp_cmd[pi_nbt].size >= 0) {
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
                handle_error (IBP_E_FILE_ACCESS, &pi_fd);
                IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
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
      li_nextstep += IBP_STEP;
      li_nextstep += li_lsd;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_copy size determined\n");
#endif

      break;

    case HLSD_SEEK_FI:
    case HLSD_SEEK_CQ:
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
          handle_error (IBP_E_SERVER_TIMEOUT, &pi_fd);
          IBPErrorNb[pi_nbt] = IBP_E_SERVER_TIMEOUT;
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
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
        li_nextstep = HLSD_DONE;
        continue;
#endif
      }
      if (ibp_cmd[pi_nbt].size > 0) {
        ll_TrueSize = (ll_size - ll_free < ibp_cmd[pi_nbt].size) ? ll_size - ll_free : ibp_cmd[pi_nbt].size;
      }
      else {
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
        IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap); 
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  before copy_File2Socket \n");
#endif

      li_ret = copy_File2Socket_ram ((char *)ps_container->startArea, ibp_cmd[pi_nbt].Offset, ls_cap, &ls_timeout, ll_TrueSize, lt_expireTime);


#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  after  copy_File2Socket\n");
#endif

      SaveInfo2LSS(rs,ps_container, 2);

      li_nextstep += IBP_STEP;
      li_nextstep += IBP_STEP;
      break;
 
    case HSC_SEND_FI:

      ls_cap = (char *) malloc (strlen(ibp_cmd[pi_nbt].Cap) + 1);
      if (ls_cap == NULL) {
        handle_error (IBP_E_INTERNAL, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;
      li_ret = copy_Fifo2Socket_ram((char *)ps_container->startArea, ls_cap, &ls_timeout, ll_TrueSize, ps_container->maxSize,ll_readptr, li_HeaderLen, lt_expireTime);

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
# ifdef IBP_THREAD
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
        IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

      li_ret = copy_Fifo2Socket_ram((char *)ps_container->startArea, ls_cap, &ls_timeout, ll_TrueSize, ps_container->maxSize,ll_readptr, li_HeaderLen, lt_expireTime);

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

  if (lf_f != NULL) {
    fclose(lf_f);
  }

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
/* copy_File2Socket          */
/* It copys ByteArray file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_File2Socket_ram(char *pc_buffer, int pl_offset, IBP_cap ps_cap, IBP_timer ps_timer, ulong_t pl_size, time_t pl_expireTime) 
{
  char   *dataBuf;
  int    li_ret;
  
  dataBuf = pc_buffer + pl_offset;

  if ((li_ret = IBP_store(ps_cap, ps_timer, dataBuf, pl_size)) <= 0) {
    return(IBP_errno);
  }
  return(pl_size);

}

/*****************************/
/* copy_Fifo2Socket          */
/* It copys fifo or CQ file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_Fifo2Socket_ram (char *pc_buffer,
                 IBP_cap ps_cap,
                 IBP_timer ps_timer,
                 ulong_t pl_size,
                 ulong_t pl_fifoSize,
                 ulong_t pl_rdPtr, 
                 int     pi_HeaderLen,
                 time_t pl_expireTime) 
{

  long     n1,n2;
  char     *dataBuf;
  int      li_ret;

  dataBuf = pc_buffer + pl_rdPtr;
  n1 = IBP_MIN(pl_size, pl_fifoSize-pl_rdPtr);
  if (li_ret = IBP_store(ps_cap, ps_timer, dataBuf, n1) <= 0) {
    return(IBP_errno);
  }
  if (n1 < pl_size) {
    if (pl_expireTime < time(NULL)) {
      return(n1);
    }

    dataBuf = pc_buffer;
    n2 = pl_size - n1;
    if (li_ret = IBP_store(ps_cap, ps_timer, dataBuf, n2) <= 0) {
      return(n1);
    }
  }
    return(pl_size);

}
/*****************************/
/* handle_copy               */
/* It handles the user's copy requirement, envoked by handle_require() */
/* It sends the data to another IBP depot  */
/* return: none              */
/*****************************/
void handle_copy_disk ( RESOURCE *rs, int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  int                 li_HeaderLen, li_ret = IBP_OK;
  unsigned long int   ll_TrueSize;
  char                lc_LineBuf[CMD_BUF_SIZE];
  FILE                *lf_f = NULL;
  int                 li_nextstep = 0;
  int                 li_response;
  int                 li_lsd = 0;
  unsigned long       ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
  IBP_cap             ls_cap = NULL;
  struct ibp_timer    ls_timeout;
  time_t              lt_expireTime;

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

      lf_f = fopen(ps_container->fname, "r");

      if (lf_f == NULL) {
        handle_error (IBP_E_FILE_OPEN, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_FILE_OPEN;
        li_nextstep = HLSD_DONE;
        continue;
      }

      if (fread(&li_HeaderLen, sizeof(int), 1, lf_f) == 0) {
        handle_error (IBP_E_FILE_ACCESS, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
        li_nextstep = HLSD_DONE;;
        continue;
      }

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
                handle_error (IBP_E_FILE_ACCESS, &pi_fd);
                IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
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
          continue;
        }
      }
      else {
        if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
          handle_error (IBP_E_FILE_ACCESS, &pi_fd);
          IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
          li_nextstep = HLSD_DONE;
          continue;
        }
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
          IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
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
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
        li_nextstep = HLSD_DONE;
        continue;
#endif
      }
      if (ibp_cmd[pi_nbt].size > 0) {
        ll_TrueSize = (ll_size - ll_free < ibp_cmd[pi_nbt].size) ? ll_size - ll_free : ibp_cmd[pi_nbt].size;
        if (fseek(lf_f, li_HeaderLen + ll_rdcount, SEEK_SET) == -1) {
          handle_error (IBP_E_FILE_ACCESS, &pi_fd);
          IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
          li_nextstep = HLSD_DONE;
          continue;
        }
      }
      else {
        if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
          handle_error (IBP_E_FILE_ACCESS, &pi_fd);
          IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
          li_nextstep = HLSD_DONE;
          continue;
        }
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
        IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap); 
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  before copy_File2Socket \n");
#endif

      li_ret = copy_File2Socket (lf_f, ls_cap, &ls_timeout, ll_TrueSize, lt_expireTime);
      IBPErrorNb[pi_nbt] = li_ret;

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
      li_ret = copy_Fifo2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize, ps_container->maxSize,ll_readptr, li_HeaderLen, lt_expireTime);

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
        IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
        li_nextstep = HLSD_DONE;
        continue;
      }
      strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
      ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
      ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

      li_ret = copy_Fifo2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize, ps_container->maxSize,ll_readptr, li_HeaderLen, lt_expireTime);

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
      IBPErrorNb[pi_nbt] = IBP_OK;
      li_response = IBP_OK;
      sprintf (lc_LineBuf, "%d %d\n", li_response, li_ret);
      Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
      li_nextstep = HLSD_DONE;
      break;

    }
  }

  fclose(lf_f);

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
/* copy_File2Socket          */
/* It copys ByteArray file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_File2Socket(FILE *pf_fp,IBP_cap ps_cap, IBP_timer ps_timer, ulong_t pl_size, time_t pl_expireTime) 
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
    n1 = fread(dataBuf, 1, readChunk, pf_fp);
    if (n1 == 0) {
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
  }
  free(dataBuf);
  return(readCount);
}

/*****************************/
/* copy_Fifo2Socket          */
/* It copys fifo or CQ file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_Fifo2Socket (FILE *pf_fp,
                 IBP_cap ps_cap,
                 IBP_timer ps_timer,
                 ulong_t pl_size,
                 ulong_t pl_fifoSize,
                 ulong_t pl_rdPtr, 
                 int     pi_HeaderLen,
                 time_t pl_expireTime) 
{

  ulong_t  readCount;
  ulong_t  readChunk;
  ulong_t  emptyCount = 0;
  long     n1;
  char     *dataBuf;
  int      li_fd;
  int      li_ret;

  dataBuf = (char *) malloc (DATA_BUF_SIZE);
  if (dataBuf == NULL)
  {
    IBP_errno = IBP_E_GENERIC;
    return(IBP_errno);
  }

  li_fd = fileno (pf_fp);
  n1 = readCount = 0;
  while (readCount < pl_size) {
    if (pl_expireTime < time(NULL)) {
      free(dataBuf);
      IBP_errno = IBP_E_SERVER_TIMEOUT;
      return(IBP_errno);
    }

    readChunk = IBP_MIN(DATA_BUF_SIZE, pl_size-readCount); /* buffer overflow*/
    readChunk = IBP_MIN(readChunk, pl_fifoSize-emptyCount); /* read available*/
    readChunk = IBP_MIN(readChunk, pl_fifoSize-pl_rdPtr);   /* wrap around*/

#ifdef IBP_DEBUG
    fprintf (stderr, "read chunk : %lu %lu\n", readChunk,pl_rdPtr);
#endif

    fseek(pf_fp, pi_HeaderLen + pl_rdPtr, SEEK_SET);
    n1 = fread(dataBuf, 1, readChunk, pf_fp);
    if (n1 < 0)
    {
      free(dataBuf);
      return IBP_E_FILE_READ;
    }
    pl_rdPtr = (pl_rdPtr + n1) % pl_fifoSize;
    emptyCount += n1;

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
  }
  free(dataBuf);
  return(readCount);
}
