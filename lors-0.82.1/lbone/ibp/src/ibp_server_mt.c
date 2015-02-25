/*           IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *          Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
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
 * ibp_server_mt.c
 *
 * IBP server code. Version name: debut
 * Main of the Threaded version
 */
# include "config-ibp.h"
# ifdef HAVE_NETDB_H
# include <netdb.h>
# endif
# ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
# endif
# ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# endif
# include <assert.h>
# include "ibp_thread.h"
# include "ibp_thread_log.h"
# include "ibp_queue.h"

/*
 *
 * Level 2: handle_request
 * It has to be here because of the mutex lock
 *
 */

/*****************************/
/* handle_request            */
/* This function decodes the user's command and envokes the correspond function.  */
/* pi_connfd -- socket describtor  */
/* pi_nbt    -- thread number      */
/* return    -- none               */
/*****************************/
void handle_request(int pi_connfd, int pi_nbt)
{

    int li_destfd;
    IBP_byteArray ls_ByteArray;
    void *lv_return;
    time_t lt_now;
    char *lc_now;
    char *lc_fname;
    void *decode_cmd();
    unsigned short li_wait, li_bufflag = 0;
    time_t tt;
    char tt_tmp[50];
    struct sockaddr ls_peername;
    socklen_t ls_peernamelen;
    u_char *addr;

    bzero(exec_info[pi_nbt], 4096);
    tt = time(NULL);
#ifdef IBP_DEBUG
    sprintf(exec_info[pi_nbt], " Handle_request() beginning at %s\n",
            ctime(&tt));
#endif

    if (!glb.IBPRecover) {
        if (tt > gt_recoverstoptime) {
            printf("depot recovery finished\n");
            recover_finish();
            glb.IBPRecover = 1;
        }
    }

    lv_return = decode_cmd(pi_connfd, pi_nbt);

    IBPErrorNb[pi_nbt] = IBP_OK;

    if (ibp_cmd[pi_nbt].Cmd == IBP_NOP) {
#ifdef IBP_DEBUG
        fprintf(stderr, "Error in detecting command, NOP\n");
#endif
        return;
    }

    if (!glb.IBPRecover) {
        if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND)
            || ((ibp_cmd[pi_nbt].Cmd == IBP_MANAGE)
                && (ibp_cmd[pi_nbt].ManageCmd == IBP_PROBE))) {
        }
        else {
            handle_error(IBP_E_SERVER_RECOVERING, &pi_connfd);
        }
    }

    if ((ibp_cmd[pi_nbt].Cmd != IBP_ALLOCATE) &&
        (ibp_cmd[pi_nbt].Cmd != IBP_STATUS) &&
        (ibp_cmd[pi_nbt].Cmd != IBP_MCOPY)) {
        ls_ByteArray = (IBP_byteArray) lv_return;
        if (ls_ByteArray->attrib.type == IBP_BUFFER) {
            li_wait = 1;
            li_bufflag = 1;
            while (li_wait == 1) {
                pthread_mutex_lock(&block);
                if (ls_ByteArray->bl == 0) {
                    ls_ByteArray->bl = 1;
                    li_wait = 0;
                }
                pthread_mutex_unlock(&block);
                if (li_wait == 1)
                    sleep(1);
            }
        }
    }

    if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND)
        && (ls_ByteArray->attrib.type == IBP_BUFFER)) {
        ibp_cmd[pi_nbt].Cmd = IBP_SEND_BU;
    }

    switch (ibp_cmd[pi_nbt].Cmd) {
    case IBP_ALLOCATE:
        pthread_mutex_lock(&alloc_lock);
        handle_allocate(pi_connfd, pi_nbt, IBP_HALL_TYPE_STD);
        pthread_mutex_unlock(&alloc_lock);
        break;

    case IBP_STORE:
        li_wait = 1;
        while (li_wait == 1) {
            pthread_mutex_lock(&wlock);
            if (ls_ByteArray->wl == 0) {
                ls_ByteArray->wl = 1;
                li_wait = 0;
            }
            pthread_mutex_unlock(&wlock);
            if (li_wait == 1)
                sleep(1);
        }
        handle_store(pi_connfd, pi_connfd, pi_nbt, ls_ByteArray);

        ls_ByteArray->reference--;
        assert(ls_ByteArray->reference >= 0);
        if (ls_ByteArray->deleted == 1) {
            if ((ls_ByteArray->writeLock == 0)
                && (ls_ByteArray->readLock == 0)) {
                SaveInfo2LSS(ls_ByteArray, 2);
                jettisonByteArray(ls_ByteArray);
                break;
            }
        }
        pthread_mutex_lock(&wlock);
        ls_ByteArray->wl = 0;
        pthread_mutex_unlock(&wlock);
        break;

    case IBP_REMOTE_STORE:
        li_destfd = SocketConnect(ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);
        if (li_destfd == -1)
            handle_error(IBP_E_CONNECTION, &pi_connfd);
        else {
            li_wait = 1;
            while (li_wait == 1) {
                pthread_mutex_lock(&wlock);
                if (ls_ByteArray->wl == 0) {
                    ls_ByteArray->wl = 1;
                    li_wait = 0;
                }
                pthread_mutex_unlock(&wlock);
                if (li_wait == 1)
                    sleep(1);
            }
            handle_store(pi_connfd, li_destfd, pi_nbt, ls_ByteArray);
            pthread_mutex_lock(&wlock);
            ls_ByteArray->wl = 0;
            pthread_mutex_unlock(&wlock);
        }
        break;

    case IBP_LOAD:
    case IBP_SEND_BU:
    case IBP_DELIVER:
        if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
            li_wait = 1;
            while (li_wait == 1) {
                pthread_mutex_lock(&rlock);
                if (ls_ByteArray->rl == 0) {
                    ls_ByteArray->rl = 1;
                    li_wait = 0;
                }
                pthread_mutex_unlock(&rlock);
                if (li_wait == 1)
                    sleep(1);
            }
        }
        handle_lsd(pi_connfd, pi_nbt, ls_ByteArray);

        ls_ByteArray->reference--;
        assert(ls_ByteArray->reference >= 0);
        if (ls_ByteArray->deleted == 1) {
            if ((ls_ByteArray->writeLock == 0)
                && (ls_ByteArray->readLock == 0)) {
                SaveInfo2LSS(ls_ByteArray, 2);
                jettisonByteArray(ls_ByteArray);
                break;
            }
        }
        if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
            pthread_mutex_lock(&rlock);
            ls_ByteArray->rl = 0;
            pthread_mutex_unlock(&rlock);
        }
        break;

    case IBP_SEND:
        if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
            li_wait = 1;
            while (li_wait == 1) {
                pthread_mutex_lock(&rlock);
                if (ls_ByteArray->rl == 0) {
                    ls_ByteArray->rl = 1;
                    li_wait = 0;
                }
                pthread_mutex_unlock(&rlock);
                if (li_wait == 1)
                    sleep(1);
            }
        }
        handle_copy(pi_connfd, pi_nbt, ls_ByteArray);

        ls_ByteArray->reference--;
        assert(ls_ByteArray->reference >= 0);
        if (ls_ByteArray->deleted == 1) {
            if ((ls_ByteArray->writeLock == 0)
                && (ls_ByteArray->readLock == 0)) {
                SaveInfo2LSS(ls_ByteArray, 2);
                jettisonByteArray(ls_ByteArray);
                break;
            }
        }
        if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
            pthread_mutex_lock(&rlock);
            ls_ByteArray->rl = 0;
            pthread_mutex_unlock(&rlock);
        }
        break;

    case IBP_MANAGE:
        lc_fname = handle_manage(pi_connfd, pi_nbt, ls_ByteArray);
        break;

    case IBP_STATUS:
        handle_status(pi_connfd, pi_nbt);
        break;

    case IBP_MCOPY:
        handle_mcopy(pi_connfd, pi_nbt);
        /*
           ls_ByteArray = NULL;
           ls_ByteArray->reference--;
         */
        /* exec (datamover, arguments) */
        break;

    default:
        break;
    }

    if ((ibp_cmd[pi_nbt].Cmd != IBP_ALLOCATE) &&
        (ibp_cmd[pi_nbt].Cmd != IBP_STATUS) &&
        (ibp_cmd[pi_nbt].Cmd != IBP_MCOPY) &&
/*
        (ibp_cmd[pi_nbt].Cmd != IBP_MANAGE) &&
*/
        (li_bufflag == 1)) {
        pthread_mutex_lock(&block);
        ls_ByteArray->bl = 0;
        pthread_mutex_unlock(&block);
    }

/*
  lv_return = (void *) close (pi_connfd);
*/

    /*
     * Update log file
     */
/*
  getpeername(pi_connfd, &ls_peername, &ls_peernamelen);
  addr = (u_char *)ls_peername.sa_data;

  if ((ibp_cmd[pi_nbt].Cmd != IBP_ALLOCATE) &&
      (ibp_cmd[pi_nbt].Cmd != IBP_STATUS) &&
       (ibp_cmd[pi_nbt].Cmd != IBP_MCOPY)) {
    if (gf_logfd == NULL) 
      gf_logfd = fopen (gc_logfile, "a");
    if (gf_logfd == NULL)
      handle_error (E_OFILE, (void *) gc_logfile);
    else {
      time (&lt_now);
      lc_now = ctime (&lt_now);
//      lc_now[24] = '\0'; 
      if (ibp_cmd[pi_nbt].Cmd != IBP_MANAGE)
        lc_fname = ls_ByteArray->fname;
      if ((lc_fname != NULL) && (lc_now != NULL)) {
        fprintf(gf_logfd, "%s # %d # \n",lc_now, ibp_cmd[pi_nbt].Cmd);
        fflush (gf_logfd); 
      }
    }
  }
  
  if (ibp_cmd[pi_nbt].Cmd == IBP_STATUS) {
    if (gf_logfd == NULL)
      gf_logfd = fopen (gc_logfile, "a");
    if (gf_logfd == NULL)
      handle_error (E_OFILE, (void *) gc_logfile);
    else {
      time (&lt_now);
      lc_now = ctime (&lt_now);
//      lc_now[24] = '\0'; 
      if (lc_now != NULL)
        fprintf(gf_logfd, "%s # %d # \n",lc_now, ibp_cmd[pi_nbt].Cmd);
      fflush (gf_logfd);

    }
  }

*/

#ifdef IBP_DEBUG
    fprintf(stderr, "Request finished by thread %d\n", pi_nbt);
    tt = time(0);
    strcpy(tt_tmp, ctime(&tt));
    tt_tmp[strlen(tt_tmp) - 2] = '\0';
    ibpWriteThreadLog("[%s] Request finished.\n", tt_tmp);
#endif

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], " Handle_request() finished\n");
#endif
    if (ibp_cmd[pi_nbt].Cmd == IBP_MANAGE)
        free(lc_fname);
    return;
}

void *routine_check()
{
    time_t lt_t;
    sigset_t block_mask;

    /*
     * set signal mask
     */
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGSEGV);
    sigaddset(&block_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block_mask, NULL);

    sleep(glb.RecoverTime);
    for (;;) {
        sleep(IBP_PERIOD_TIME);
        check_size(0, IBP_ROUTINE_CHECK);
        if (gf_recovertimefd = fopen(gc_recovertimefile, "w")) {
            lt_t = time(NULL);
            fprintf(gf_recovertimefd, "%ld\n%s depot OK\n", lt_t,
                    ctime(&lt_t));
            fclose(gf_recovertimefd);
        }
        gf_recovertimefd = NULL;
    }
}

/*
 *
 * Thread creation part
 *
 */

/*****************************/
/* thread_make               */
/* It creates a thread       */
/* i -- thread num           */
/* return -- none             */
/*****************************/
void thread_make(int i)
{
    void *thread_main(void *);
    pthread_attr_t attr;
#ifdef IBP_DEBUG
    static int num = 0;
#endif
    int ret;

    pthread_attr_init(&attr);
    if (pthread_attr_setstacksize(&attr, IBPSTACKSIZE) != 0) {
        fprintf(stderr, "Fail to set stack size !\n");
        exit(-1);
    }
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        fprintf(stderr, "Fail to set thread detached !\n");
        exit(-1);
    }

    do {
#ifdef IBP_DEBUG
        fprintf(stderr, " %d th forking new thread\n", ++num);
#endif
        ret =
            pthread_create(&tptr[i].thread_tid, &attr, &thread_main,
                           (void *) i);
        if (ret != 0) {
#ifdef IBP_DEBUG
            fprintf(stderr, "Error occur in forking new thread [%d]\n", ret);
#endif
        }
    } while (ret != 0);

    return;
}

/*****************************/
/* thread_main               */
/* This is the main function for a thread.  It builds up the connection with the user.  And envoke handle_request()       */
/* arg   -- thread parameter */
/* return -- none             */
/*****************************/
void *thread_main(void *arg)
{
    int li_connfd;
    time_t tt;
    char tt_tmp[50];
    char tmp[50];
    sigset_t block_mask;
    struct timespec spec;
    int ret;

    /*
     * set signal mask
     */
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGINT);
    sigaddset(&block_mask, SIGSEGV);
    sigaddset(&block_mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &block_mask, NULL);

    openThreadLogFile((int) arg);

#ifdef IBP_DEBUG
    fprintf(stderr, "T %d starting\n", (int) arg);
#endif

    for (;;) {
#ifdef IBP_DEBUG
        fprintf(stderr, "T %d, waiting for the lock\n", (int) arg);
        tt = time(0);
        strcpy(tt_tmp, ctime(&tt));
        tt_tmp[strlen(tt_tmp) - 2] = '\0';
        ibpWriteThreadLog("\n\n[%s] waitting for the lock\n", tt_tmp);
#endif

        /* dynamic thread exiting */
        pthread_mutex_lock(queue->lock);

# ifdef IBP_DEBUG
        fprintf(stderr, "T %d checking exit condition \n", (int) arg);
        fprintf(stderr, "%d left \n", queue->req_num);
# endif
#if 0
        if ((queue->req_num < REQ_THRESHOULD)
            && ((int) arg >= glb.IbpThreadMin)) {
            thr_bitmap[(int) arg] = 0;
            glb.IbpThreadNum--;
            pthread_mutex_unlock(queue->lock);

# ifdef IBP_DEBUG
            fprintf(stderr, "T %d exit...\n", (int) arg);
# endif
            pthread_exit(0);
        }
#endif
    /**
     * Retrieve request from the queue.
     */
        if (dl_empty(queue->queue)) {
          again:
            queue->idleThds++;
            spec.tv_sec = time(NULL) + IBP_MAX_THD_IDLE_TIME;
            spec.tv_nsec = 0;
            ret = pthread_cond_timedwait(queue->cv, queue->lock, &spec);
            queue->idleThds--;
            if (dl_empty(queue->queue)) {
                if (((int) arg >= glb.IbpThreadMin) &&
                    (queue->idleThds >= IBP_MIN_IDLE_THDS) &&
                    (ret == ETIMEDOUT)) {
                    /* dynamic allocated thread and time out */
                    thr_bitmap[(int) arg] = 0;
                    glb.IbpThreadNum--;
                    pthread_mutex_unlock(queue->lock);
                    pthread_exit(0);
                }
                else {
                    goto again;
                }
            }
        }

        if (dl_empty(queue->queue)) {
            fprintf(stderr, "Panic: Thread Sync failed!!!!\n");
            exit(-1);
        }
        li_connfd = remove_request(queue);
        if (!dl_empty(queue->queue)) {
            pthread_cond_signal(queue->cv);
        }
        if (queue->sleep == 1) {
            pthread_cond_signal(queue->resume);
        }
        pthread_mutex_unlock(queue->lock);

# ifdef IBP_DEBUG
        fprintf(stderr, "T %d, remove %d from queue\n", (int) arg, li_connfd);
# endif

# ifdef IBP_DEBUG
        tt = time(0);
        strcpy(tt_tmp, ctime(&tt));
        tt_tmp[strlen(tt_tmp) - 2] = '\0';
        ibpWriteThreadLog("[%s] release mutex. \n", tt_tmp);
# endif

        if (li_connfd != -1) {
            handle_request(li_connfd, (int) arg);
        }
        else {
            handle_error(E_ACCEPT, arg);
        }
        close(li_connfd);
    }
}

/*****************************/
/* thrallocate               */
/* It allocate the space for the thread       */
/* return -- 0                */
/*****************************/
int thrallocate()
{
    int li_index;

    /*
     * allocate the space for the strings
     */
    if ((ibp_cmd =
         (ibp_command *) calloc(sizeof(ibp_command),
                                glb.IbpThreadMax)) == NULL) {
        handle_error(E_ALLOC, NULL);
        exit(-1);
    }

    if ((exec_info =
         (char **) calloc(sizeof(char *), glb.IbpThreadMax)) == NULL) {
        handle_error(E_ALLOC, NULL);
        exit(-1);
    }

    for (li_index = 0; li_index < glb.IbpThreadMax; li_index++) {
        if ((ibp_cmd[li_index].key = malloc(256)) == NULL) {
            handle_error(E_ALLOC, NULL);
            exit(-1);
        }
        if ((ibp_cmd[li_index].host = malloc(256)) == NULL) {
            handle_error(E_ALLOC, NULL);
            exit(-1);
        }
        if ((ibp_cmd[li_index].Cap = malloc(256)) == NULL) {
            handle_error(E_ALLOC, NULL);
            exit(-1);
        }
        if ((ibp_cmd[li_index].passwd = malloc(17)) == NULL) {
            handle_error(E_ALLOC, NULL);
            exit(-1);
        }
        if ((exec_info[li_index] = malloc(4096)) == NULL) {
            handle_error(E_ALLOC, NULL);
            exit(-1);
        }

    }

    return 0;

}

/*****************************/
/* ibp_crash_save            */
/* It saves the latest thread information to ibp.crash when the depot is crashed
or stopped by administrator  */
/* return: none              */
/*****************************/
static void ibp_crash_save(int pi_mode)
{
    int i;
    char lc_crashname[128];
    FILE *lf_crashfd;
    time_t lt_time;
    char *lc_time;

/*
  if (gi_recoverclosed == 1) {
    gi_recoverclosed = 0;
    printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
    fclose(gf_recoverfd);
  }
*/

    if (gc_cfgpath[strlen(gc_cfgpath) - 1] != '/')
        sprintf(lc_crashname, "%s/ibp.crash", gc_cfgpath);
    else
        sprintf(lc_crashname, "%sibp.crash", gc_cfgpath);

    if ((lf_crashfd = fopen(lc_crashname, "w")) == NULL) {
        fprintf(stderr, "Crash file open error\n");
        exit(1);
    }

#if 0
    if (glb.VolDir[strlen(glb.VolDir) - 1] != '/')
        sprintf(lc_crashname, "%s/ibp.crash", glb.VolDir);
    else
        sprintf(lc_crashname, "%sibp.crash", glb.VolDir);

    if ((lf_crashfd = fopen(lc_crashname, "w")) == NULL) {
        fprintf(stderr, "Crash file open error\n");
        exit(1);
    }
#endif


    lt_time = time(NULL);
    lc_time = ctime(&lt_time);
    if (pi_mode == 1) {
        fprintf(lf_crashfd, "IBP Depot crashed at %s\n", lc_time);
        if (gf_recovertimefd = fopen(gc_recovertimefile, "w")) {
            fprintf(gf_recovertimefd, "%ld\n%s depot crash\n", lt_time,
                    lc_time);
            fclose(gf_recovertimefd);
        }
    }
    else {
        fprintf(lf_crashfd, "IBP Depot stopped at %s\n", lc_time);
        if (gf_recovertimefd = fopen(gc_recovertimefile, "w")) {
            fprintf(gf_recovertimefd, "%ld\n%s depot stopped\n", lt_time,
                    lc_time);
            fclose(gf_recovertimefd);
        }
    }

    for (i = 0; i < glb.IbpThreadNum; i++) {
        fprintf(lf_crashfd, "Thread %d:\n%s\n", i, exec_info[i]);
    }
    fclose(lf_crashfd);

}

/*****************************/
/* ibp_signal_segv           */
/* It works when the depot is crashed.  It saves the crash information to ibp.cra
sh  */
/* return: none              */
/*****************************/
static void ibp_signal_segv()
{
    ibp_crash_save(1);
    fprintf(stderr, "\nPlease email ibp.crash to ibp@cs.utk.edu\n");
    fprintf(stderr, "Thank you for using IBP.\n");
    exit(1);
}

/*****************************/
/* ibp_signal_int            */
/* It works when the depot is stopped by the administrator. */
/* return: none              */
/*****************************/
static void ibp_signal_int()
{
    ibp_crash_save(2);

    fprintf(stderr, "Thank you for using IBP.\n");
    exit(-1);
}

/*
 *
 * level 0 - main
 *
 */

/*****************************/
/* main                      */
/* the main function of ibp_server_mt  */
/* return -- 0                */
/*****************************/
int main(int argc, char **argv)
{
    int li_index, li_nthreads, li_ret;
    void thread_make(int);
    pthread_t pt;
    int li_connfd;
    socklen_t clilen;
    pthread_attr_t attr;
    struct sockaddr *cliaddr;

    gk_addrlen = sizeof(struct sockaddr);
    cliaddr = malloc(gk_addrlen * 2);
    clilen = gk_addrlen;

    if ((li_ret = boot(argc, argv)) != IBP_OK) {
        handle_error(E_BOOTFAIL, (void *) &li_ret);
        exit(-1);
    }

    li_ret = thrallocate();
    pthread_attr_init(&attr);
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        fprintf(stderr, "Fail to set thread detached !\n");
        exit(-1);
    }
    pthread_create(&pt, &attr, &routine_check, NULL);

# ifdef HAVE_PTHREAD_H
    li_nthreads = glb.IbpThreadNum;
# else
    li_nthreads = 1;
# endif


    tptr = calloc(glb.IbpThreadMax, sizeof(Thread));
    memset(thr_bitmap, 0, sizeof(unsigned char) * MAX_THREADS);

    for (li_index = 0; li_index < li_nthreads; li_index++) {
        thread_make(li_index);
        thr_bitmap[li_index] = 1;
    }

    signal(SIGPIPE, SIG_IGN);
    signal(SIGSEGV, ibp_signal_segv);
    signal(SIGINT, ibp_signal_int);

  /**
   * Accept and dispatch incoming requests.
   */
    while (1) {
        li_connfd = accept(glb.IbpSocket, cliaddr, &clilen);
        pthread_mutex_lock(queue->lock);
        insert_request(queue, li_connfd);

# ifdef IBP_DEBUG
        fprintf(stderr, "insert %d to queue \n", li_connfd);
# endif
        if ((queue->idleThds <= IBP_MIN_IDLE_THDS)
            && (glb.IbpThreadNum < glb.IbpThreadMax)) {
            for (li_index = glb.IbpThreadMin; li_index < glb.IbpThreadMax;
                 li_index++) {
                if (thr_bitmap[li_index] == 0) {
                    break;
                }
            }
            if (li_index < glb.IbpThreadMax) {
                thread_make(li_index);
# ifdef IBP_DEBUG
                fprintf(stderr, "create thread %d \n", li_index);
# endif
                thr_bitmap[li_index] = 1;
                glb.IbpThreadNum++;
            }
        }

        pthread_cond_signal(queue->cv);
        /*
         * request queue is full 
         */
        if (queue->req_num >= MAX_THREADS) {
            queue->sleep = 1;
            pthread_cond_wait(queue->resume, queue->lock);
            queue->sleep = 0;
        }
        pthread_mutex_unlock(queue->lock);
    }

    pthread_exit(0);
}
