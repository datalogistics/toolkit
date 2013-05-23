#include "config-ibp.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef STDC_HEADERS
#include <sys/types.h>
#include <stdarg.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include "ibp_thread.h"

static pthread_key_t gl_FileKey;
static pthread_once_t gl_LogFileKeyOnce = PTHREAD_ONCE_INIT;

static void closeThreadLogFile(void *buf)
{
    fclose((FILE *) buf);
}

static void allocThreadLogFileKey()
{
    if (glb.IbpThreadLogEnable)
        pthread_key_create(&gl_FileKey, closeThreadLogFile);
}

void openThreadLogFile(int threadNo)
{
    FILE *fp;
    char fname[50];

    if (glb.IbpThreadLogEnable) {
        pthread_once(&gl_LogFileKeyOnce, allocThreadLogFileKey);
        sprintf(fname, "log.%02d", threadNo);
        if ((fp = fopen(fname, "a+")) == NULL) {
            fprintf(stderr, "Error in open log file!\n");
            exit(-1);
        }
        pthread_setspecific(gl_FileKey, (void *) fp);
    }
}

FILE *_getThreadLogFD(void)
{
    if (glb.IbpThreadLogEnable)
        return ((FILE *) pthread_getspecific(gl_FileKey));
}

void ibpWriteThreadLog(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);
    if (glb.IbpThreadLogEnable) {
        vfprintf(_getThreadLogFD(), format, ap);
        fflush(_getThreadLogFD());
    }
    va_end(ap);
}
