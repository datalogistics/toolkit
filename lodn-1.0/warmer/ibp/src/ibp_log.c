#include <stdio.h>
#include <pthread.h>
#include <syslog.h>     /* syslog() */
#include <stdarg.h>     /* syslog() */
#include <unistd.h>     /* getpid() */
#include <sys/types.h>  /* stat() getpid() */
#include <sys/stat.h>    /* stat() */
#include <time.h>       /* localtime() */
#include <assert.h>     /* assert() */
#include <string.h>     /* strdup() */
#include "ibp_log.h"
#include "ibp_server.h"
#include "ibp_connection.h"


IBP_LOG_MSG * 
init_ibp_log_msg ( char * msg )
{
    IBP_LOG_MSG * logMsg;

    logMsg = (IBP_LOG_MSG*)calloc(1,sizeof(IBP_LOG_MSG));
    logMsg->msg = strdup(msg);
    logMsg->next = NULL;

    return logMsg;
}

void
free_ibp_log_msg ( IBP_LOG_MSG *logMsg )
{
    if ( logMsg != NULL ){
        if ( logMsg->msg != NULL ) { free(logMsg->msg); }
        free(logMsg);
    }
    return;
}

IBP_LOG_QUEUE *
init_ibp_log_queue( char * logFileName , int maxLogFileSize )
{
    IBP_LOG_QUEUE * queue = NULL;

    queue = (IBP_LOG_QUEUE *)calloc(1,sizeof(IBP_LOG_QUEUE));
    queue->lock = (pthread_mutex_t *)calloc(1,sizeof(pthread_mutex_t));
    pthread_mutex_init(queue->lock,NULL);
    queue->head = NULL;
    queue->tail = NULL;
    queue->status = 0;
    queue->errorCount = 0;
    queue->fp = NULL;
    queue->logFileName = strdup(logFileName);
    queue->maxLogFileSize = maxLogFileSize;

    /* open log file */
    if ( (queue->fp = fopen(queue->logFileName,"a")) == NULL ){
        free(queue->lock);
        free(queue->logFileName);
        free(queue);
        return(NULL);
    }

    return (queue);
}

static void
append_log_msg ( IBP_LOG_QUEUE * queue, IBP_LOG_MSG *msg )
{
    if ( queue->head == NULL ){
        queue->head = msg;
        queue->tail = msg;
        msg->next = NULL;
        return;
    }

    queue->tail->next = msg;
    msg->next = NULL;
    queue->tail = msg;

    return;
}

static int
check_log_file ( IBP_LOG_QUEUE * queue )
{
    struct stat sb;
    char * buffer = NULL;
    struct tm   *tm;
    time_t      now; 
    int         i;

    /* check if file exists */
    if ( 0 != stat(queue->logFileName,&sb) || queue->fp == NULL ){
        queue->fp = fopen(queue->logFileName,"a");
        if ( queue->fp == NULL ){
            syslog(LOG_ALERT,"IBP depot (pid:%d): can't open log file [%s]!\n",
                             getpid(),queue->logFileName);
            return(-1);
        }
    }

    /* check file size */
    if ( sb.st_size > queue->maxLogFileSize ){
        fclose(queue->fp);
        /* create new file name */
        buffer = (char*)calloc(sizeof(char),strlen(queue->logFileName)+22);
        now = time(0);
        tm = localtime(&now);
        for ( i=0;i<1024*1024*1024;i++){

            sprintf(buffer,"%s.%d-%d-%d.%d",
                        queue->logFileName,
                        tm->tm_year+1900,
                        tm->tm_mon,
                        tm->tm_mday,
                        i);
            if ( 0 == stat(buffer,&sb) && i <= (1024*1024*1024-2) ){
                /* file exists, generate a new one */
                continue;
            }
            if ( 0 != rename(queue->logFileName,buffer) ){
                syslog(LOG_ALERT,"IBP depot (pid:%d): can move %s to %s !\n",
                                getpid(),queue->logFileName,buffer);
                queue->fp = NULL;
                free(buffer);
                return;
            }else{
                break;
            }
        }

        free(buffer);
        queue->fp = fopen(queue->logFileName,"a");
        if ( queue->fp == NULL ){
            syslog(LOG_ALERT,"IBP depot (pid:%d): can't open log file [%s]!\n",
                             getpid(),queue->logFileName);
            return(-1);
        }
    }

    return 0;
}

void 
write_ibp_log_msg( IBP_LOG_QUEUE * queue, IBP_LOG_MSG *logMsg)
{
    struct stat sb;
    static int errorCount = 0;
    IBP_LOG_MSG * list = NULL;
    IBP_LOG_MSG * node = NULL;
    IBP_LOG_MSG * nextNode = NULL;

    pthread_mutex_lock(queue->lock);
    if ( queue->status == -1 ){
        /* can't read to log file */
        /* free log message queue */
        if ( queue->head != NULL ){
            node = queue->head;
            while (node != NULL ){
                nextNode = node->next;
                free_ibp_log_msg(node);
                node = nextNode;
            }
            queue->head = NULL;
            queue->tail = NULL;
        }
        /* increse error count */
        (queue->errorCount)++;
        if ( queue->errorCount < 500 ){
            pthread_mutex_unlock(queue->lock);
            free_ibp_log_msg(logMsg);
            return;
        }else{ /* try again */
            queue->errorCount = 0;
            queue->status = 0;
        }
    }

    append_log_msg(queue,logMsg);
    if ( queue->status == 1 ){
        /* other thread is writing logs to file */
        pthread_mutex_unlock(queue->lock);
        return;
    };


    /* write to log file */
    
    queue->status = 1;
    list = queue->head;
    queue->head = NULL;
    queue->tail = NULL;
    pthread_mutex_unlock(queue->lock); 

    /* check log file */
    if ( check_log_file(queue) != 0 ){
        /* error occurs */
        pthread_mutex_lock(queue->lock);
        queue->status = -1;
        pthread_mutex_unlock(queue->lock);
        return;
    }

    node = list;
    /* write to log file */
    while ( node != NULL ){
        nextNode = node->next;
        fwrite(node->msg,sizeof(char),strlen(node->msg),queue->fp);
        free_ibp_log_msg(node);
        node = nextNode;
    }
    fflush(queue->fp);

    pthread_mutex_lock(queue->lock);
    queue->status = 0;
    pthread_mutex_unlock(queue->lock);
     
    return; 
}

void 
ibp_write_log(  ibp_command     *ibp_cmd, 
                char            *address,
                IBP_CONNECTION  *conn,
                time_t          begin_time, 
                time_t          end_time, 
                int             error)
{
    time_t  tt = begin_time;
    int     ret = error;
    time_t  dur;
    char    *cmd;
    char    *msg;
    RESOURCE *rs;
    char    strNow[25];
    IBP_LOG_MSG * logMsg;

    cmd = (char*)calloc(1024,sizeof(char));
    msg = (char*)calloc(2048,sizeof(char));

    end_time = time(NULL);
    dur = end_time -tt;
    ret = error; 
    rs = ibp_cmd->rs;
  
    switch( ibp_cmd->Cmd ){
    case  IBP_ALLOCATE:
        sprintf(cmd,"ALLOCATE %d %ld",error,ibp_cmd->size);
        break;
    case IBP_STORE:
        sprintf(cmd,"STORE %d",ret);
        if ( ret == IBP_OK ){ 
            sprintf(cmd,"%s %ld %d %d#%s",cmd,ibp_cmd->size,dur,rs->RID,ibp_cmd->key);
        }
        break;
    case IBP_WRITE:
        sprintf(cmd,"WRITE %d",ret);
        if ( ret == IBP_OK ){
            sprintf(cmd,"%s %ld %ld %d %d#%s",cmd,ibp_cmd->Offset,ibp_cmd->size, dur,rs->RID,ibp_cmd->key);
        }
        break;
    case IBP_LOAD:
        sprintf(cmd,"LOAD %d",ret);
        if ( ret == IBP_OK ){
            sprintf(cmd,"%s %ld %ld %d %d#%s",cmd, ibp_cmd->Offset,ibp_cmd->size,dur,rs->RID,ibp_cmd->key);
        }
        break;
    case IBP_SEND:
        if ( ret > 0 ) { ret = IBP_OK ;}
        sprintf(cmd,"IBP_COPY %d",ret);
        if ( ret > 0 ){
            sprintf(cmd,"%s %ld %ld %d %d#%s %s ",cmd, ibp_cmd->Offset,ibp_cmd->size,dur,rs->RID,ibp_cmd->key,ibp_cmd->Cap);
        };
        break;
    case IBP_CT_COPY:
        sprintf(cmd,"IBP_COPY %d",ret);
        if ( ret == IBP_OK ){
            sprintf(cmd,"%s %ld %ld %d %d#%s %s ",cmd, ibp_cmd->Offset,ibp_cmd->size,dur,rs->RID,ibp_cmd->key,ibp_cmd->Cap);
        };
        break;
    case IBP_MCOPY:
        sprintf(cmd,"MCOPY %d",ret);
        break;
    case IBP_MANAGE:
        switch(ibp_cmd->ManageCmd){
        case IBP_PROBE:
            sprintf(cmd,"MANAGE %d 0 %s %d#%s",ret,"PROBE",rs->RID,ibp_cmd->key);
            break;
        case IBP_INCR:
            sprintf(cmd,"MANAGE %d 0 %s %d#%s",ret, "INCR",rs->RID,ibp_cmd->key);
            break;
        case IBP_DECR:
            sprintf(cmd,"MANAGE %d 0 %s %d#%s", ret,"DECR" ,rs->RID,ibp_cmd->key);
            break;
        case IBP_CHNG:
            sprintf(cmd,"MANAGE %d 0 %s %d#%s",ret,"CHANGE",rs->RID,ibp_cmd->key);
            break;
        default:
            sprintf(cmd,"MANAGE %d 0 %s %d#%s",ret,"UNKNOWN",rs->RID,ibp_cmd->key);
        }
        break;
    case IBP_STATUS:
        switch(ibp_cmd->ManageCmd ){
        case IBP_ST_INQ:
            sprintf(cmd,"STATUS %d 0 %s %d",ret,"PROBE",rs->RID);
            break;
        case IBP_ST_CHANGE:
            sprintf(cmd,"STATUS %d 0 %s %d",ret,"CHANGE",rs->RID);
            break;
        default:
            sprintf(cmd,"STATUS");
        }
        break;
    case IBP_NFU:
        sprintf(cmd,"NFU %d",ret);
        break;
    case IBP_NOP:
        sprintf(cmd,"Message parsing error %d",ret);
        break;
    default:
        sprintf(cmd,"Unknown_Command:%d",ibp_cmd->Cmd);
    }

    ctime_r(&begin_time,strNow);
    strNow[24] = '\0';
    sprintf(msg,"%s [CN=%s/OU=%s] [%s] %s \n",
                    address,
                    (conn->cn == NULL ? "-" : conn->cn),
                    (conn->ou == NULL ? "-" : conn->ou),
                    strNow,
                    cmd); 


    logMsg = init_ibp_log_msg(msg);
    write_ibp_log_msg(gs_logQueue,logMsg);

    free(cmd);
    free(msg);

    return;
    
}

#ifdef IBP_LOG_UNIT_TEST


void * 
worker ( void *paras)
{
    IBP_LOG_QUEUE *queue;
    IBP_LOG_MSG   *msg;
    int i;
    char    buffer[200];

    queue = (IBP_LOG_QUEUE *)paras;
    for ( i=0; i < 1000; i++ ){
        sprintf(buffer,"Thread[%d]: log messsage number %d\n",pthread_self(),i);
        msg = init_ibp_log_msg(buffer); 
        write_log_to_queue(queue,msg);
    }

    return(0);
    
}


int main()
{
    IBP_LOG_QUEUE *queue;
    pthread_attr_t attr;
    pthread_t       tids[50];
    int i ;

    queue = init_ibp_log_queue("/tmp/ibp.log",10*1024*1024);
    if ( queue == NULL ){
        fprintf(stderr,"Can't open log file !\n");
        exit(-1);
    }
    
    pthread_attr_init(&attr);

    for ( i=0; i < 50; i++ ){
        pthread_create(&(tids[i]),&attr,worker,queue);
    }

    for(i=0;i<50;i++){
        pthread_join(tids[i],NULL);
    }

    return (0);

}

#endif


