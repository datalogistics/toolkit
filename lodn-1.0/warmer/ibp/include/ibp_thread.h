/*
 *  threads header
 */

#ifndef _IBP_THREAD
#define _IBP_THREAD

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# ifdef HAVE_PTHREAD_H
# include <pthread.h>
# endif
# include "ibp_server.h"

#define NB_COMM_THR_MIN 8  /* default value for minimum threads */
#define NB_COMM_THR_MAX 64 /* default value for maximum threads */
#define REQ_THRESHOULD  8  /* default value for request queue threshould
                              to add more new threads */
#define MAX_THREADS    256 /* default value for maximum threads in system */
#define IBP_MAX_THD_IDLE_TIME 60 /* max idle time of worker thread before 
                                    exit (in seconds) */
#define IBP_MIN_IDLE_THDS    5 /* minimum number of idle worker threads running */

typedef struct {
    pthread_t 	thread_tid;		/* thread ID */
    long	thread_count;		/* #connection handled */
} Thread;

typedef struct {
  int read;
  int write;
  int read_wait;
  pthread_cond_t read_protect;
  pthread_cond_t read_cond;
  pthread_cond_t write_cond;
} Thread_Info;

typedef struct {
  int StorageNeeded;
  int Waiting;
  pthread_cond_t Protect;
  pthread_cond_t Cond;
} Thread_GLB;

Thread_GLB thread_glb;
  
Thread *tptr;

pthread_mutex_t /*mlock,*/ sdlock, /*wlock, block, rlock,*/ filock; 

pthread_mutex_t change_lock;
pthread_mutex_t recover_lock;

pthread_mutex_t alloc_lock;

int listenfd, nthreads;
unsigned char thr_bitmap[MAX_THREADS];

# endif
