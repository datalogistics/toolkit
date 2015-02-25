#ifndef _IBP_THREAD_LOG
#define _IBP_THREAD_LOG
#include "config-ibp.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef STDC_HEADERS
#include <sys/types.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "ibp_thread.h"

static pthread_key_t gl_FileKey;
static pthread_once_t gl_LogFileKeyOnce = PTHREAD_ONCE_INIT;

static void closeThreadLogFile(void *buf);
static void allocThreadLogFileKey();
void openThreadLogFile(int threadNo);
void ibpWriteThreadLog(const char *format, ...);

#endif
