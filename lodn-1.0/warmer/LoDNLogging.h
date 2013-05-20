#ifndef WARMER_LOGGING_H_
#define WARMER_LOGGING_H_

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <time.h>



/***----Constants----***/
#define LOG_FILE_SIZE_LIMIT		2147483648.0	/* Maximum log file size 2G */



/***-----Structs-----***/
typedef struct
{
	char			*logname;			/* Filename of the long file */
	FILE 		    *stream;			/* File stream to print to */
	unsigned char    verboseLevel;		/* Verbosity level */
	pthread_mutex_t  lock;				/* Lock */
	time_t			 lastLogCheck;		/* Time of the last time the log file 
										   was checked */
	
} LogControl;



/***-----Globals------***/
extern LogControl *LOG_CONTROL; 			/* Global logging control */



/***----Prototypes----***/
int 	 logInit(char *logname, int verboseLevel);
void	 logPrint(int verboseLevel, char *format, ...);
void     logPrintError(int verboseLevel, char *filename, int lineNum, int exit, 
		               char *format, ...);


#endif /*WARMER_LOGGING_H_*/
