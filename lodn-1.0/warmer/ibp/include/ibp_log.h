#ifndef IBP_LOG_H
#define IBP_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ibp_log_msg
{
    char *                  msg;
    struct ibp_log_msg *    next;
}
IBP_LOG_MSG;

typedef struct ibp_log_queue 
{
    IBP_LOG_MSG *       head;
    IBP_LOG_MSG *       tail;
    int                 errorCount;
    int                 status;
    pthread_mutex_t*    lock; 
    int                 maxLogFileSize;
    FILE *              fp;
    char *              logFileName;
}
IBP_LOG_QUEUE;

IBP_LOG_QUEUE * gs_logQueue;

extern IBP_LOG_MSG *
init_ibp_log_msg ( char * LogMsg );

extern void 
free_ibp_log_msg( IBP_LOG_MSG * logMsg);

extern IBP_LOG_QUEUE *
init_ibp_log_queue( char * logFileName, int maxLogFileSize);

extern void
write_ibp_log_msg( IBP_LOG_QUEUE * queue, IBP_LOG_MSG * logMsg);


#ifdef __cplusplus
}
#endif

#endif /* IBP_LOG_H */
