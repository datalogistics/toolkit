#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "LoDNLogging.h"


/***------Globals-----***/
LogControl *LOG_CONTROL = NULL;			/* Global logging control */



int logInit(char *logname, int verboseLevel)
{
	/* Check if the global log control has already been initialized */
	if(LOG_CONTROL != NULL)
	{
		return 0;
	}

	/* Allocate the global log control */
	if((LOG_CONTROL = (LogControl*)calloc(1, sizeof(LogControl))) == NULL)
	{
		fprintf(stderr, "Memory allocation error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if(pthread_mutex_init(&(LOG_CONTROL->lock), NULL) != 0)
	{
		fprintf(stderr, "Pthread mutex init error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if(pthread_mutex_lock(&(LOG_CONTROL->lock)) != 0)
	{
		fprintf(stderr, "Pthread mutex lock error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if(logname != NULL)
	{
		LOG_CONTROL->logname = logname;

	}else
	{
		LOG_CONTROL->stream = stdout;
	}

	LOG_CONTROL->verboseLevel = verboseLevel;

	LOG_CONTROL->lastLogCheck = 0;

    if(pthread_mutex_unlock(&(LOG_CONTROL->lock)) != 0)
	{
		fprintf(stderr, "Pthread mutex unlock error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	/* Return Successfully */
	return 0;
}



void logPrint(int verboseLevel, char *format, ...)
{
	/* Declarations */
	va_list 	vargs;
	FILE	   *stream;
	time_t  	now;
	struct tm 	tm;
	char        timeStr[32];
	struct stat statbuf;
	int    		index;
	char  	   *newLogname;
	int    		newLognameLen;
	int    		oldLognameLen;


	if(verboseLevel > LOG_CONTROL->verboseLevel)
	{
		return;
	}

	va_start(vargs, format);

	time(&now);
	localtime_r(&now, &tm);
	asctime_r(&tm, timeStr);

	*strchr(timeStr, '\n') = '\0';

	if(pthread_mutex_lock(&(LOG_CONTROL->lock)) != 0)
	{
		fprintf(stderr, "Pthread mutex lock error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if(LOG_CONTROL->logname != NULL)
	{
		/* Every ten minutes it checks if the log file needs to replaced */
		if((LOG_CONTROL->lastLogCheck + 600) < time(NULL))
		{
			/* Gets the log file size */
			if(stat(LOG_CONTROL->logname, &statbuf) != 0)
			{
				fprintf(stderr, "Error stating the log file %s at %s:%d\n",
						LOG_CONTROL->logname, __FILE__, __LINE__);
				exit(EXIT_FAILURE);
			}

			/* If the log file is over 90 percent full then it is time
			 * to replace it */
			if(1.0*statbuf.st_size > (0.90*LOG_FILE_SIZE_LIMIT))
			{
				/* Computes the length of max logname length, based on the
				 * original log name, max length of a 64bit int and a space */
				oldLognameLen = strlen(LOG_CONTROL->logname) + 1;
				newLognameLen = oldLognameLen + 32;

				/* Allocates spaces for the new logname */
				if((newLogname = malloc(newLognameLen*sizeof(char))) == NULL)
				{
					fprintf(stderr, "Memory Allocation error at %s:%d\n",
							__FILE__, __LINE__);
					exit(EXIT_FAILURE);
				}

				/* Prints the basename to the new log name */
				snprintf(newLogname, oldLognameLen, "%s", LOG_CONTROL->logname);

				/* Iterates over index creating new lognames until it finds
				 * one that doesn't exist and then it uses that name */
				for(index=0; index >= 0; index++)
				{
					snprintf(newLogname+oldLognameLen-1, 32, ".%d", index);

					if(access(newLogname, F_OK) == -1)
					{
						break;
					}
				}

				/* Renames the current log file to the new name thus emptying
				 * the log file for reuse */
				if(index >= 0)
				{
					rename(LOG_CONTROL->logname, newLogname);
				}

				free(newLogname);
			}
		}

		if((stream = fopen(LOG_CONTROL->logname, "a")) == NULL)
		{
			fprintf(stderr, "Error opening log file %s at %s:%d\n",
					LOG_CONTROL->logname, __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}else
	{
		stream = LOG_CONTROL->stream;
	}

	fprintf(stream, "%s: ", timeStr);
	vfprintf(stream, format, vargs);
    fprintf(stream, "\n");

    fflush(stream);

    if(LOG_CONTROL->logname != NULL)
    {
    	fclose(stream);
    }

	if(pthread_mutex_unlock(&(LOG_CONTROL->lock)) != 0)
	{
		fprintf(stderr, "Pthread mutex unlock error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	va_end(vargs);
}


void logPrintError(int verboseLevel, char *filename, int lineNum, int exitVal,
		           char *format, ...)
{
	/* Declarations */
	va_list 	vargs;
	FILE	   *stream;
	time_t  	now;
	struct tm 	tm;
	char        timeStr[32];
	struct stat statbuf;
	int    		index;
	char  	   *newLogname;
	int    		newLognameLen;
	int    		oldLognameLen;

	if(verboseLevel > LOG_CONTROL->verboseLevel)
	{
		return;
	}


	va_start(vargs, format);

	time(&now);
	localtime_r(&now, &tm);
	asctime_r(&tm, timeStr);

	*strchr(timeStr, '\n') = '\0';



	if(pthread_mutex_lock(&(LOG_CONTROL->lock)) != 0)
	{
		fprintf(stderr, "Pthread mutex lock error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	if(LOG_CONTROL->logname != NULL)
	{
		/* Every ten minutes it checks if the log file needs to replaced */
		if((LOG_CONTROL->lastLogCheck + 600) < time(NULL))
		{
			/* Gets the log file size */
			if(stat(LOG_CONTROL->logname, &statbuf) != 0)
			{
				fprintf(stderr, "Error stating the log file %s at %s:%d\n",
						LOG_CONTROL->logname, __FILE__, __LINE__);
				exit(EXIT_FAILURE);
			}

			/* If the log file is over 90 percent full then it is time
			 * to replace it */
			if(1.0*statbuf.st_size > (.90*LOG_FILE_SIZE_LIMIT))
			{
				/* Computes the length of max logname length, based on the
				 * original log name, max length of a 64bit int and a space */
				oldLognameLen = strlen(LOG_CONTROL->logname)+1;
				newLognameLen = oldLognameLen + 32;

				/* Allocates spaces for the new logname */
				if((newLogname = malloc(newLognameLen*sizeof(char))) == NULL)
				{
					fprintf(stderr, "Memory Allocation error at %s:%d\n",
							__FILE__, __LINE__);
					exit(EXIT_FAILURE);
				}

				/* Prints the basename to the new log name */
				snprintf(newLogname, oldLognameLen, "%s", LOG_CONTROL->logname);

				/* Iterates over index creating new lognames until it finds
				 * one that doesn't exist and then it uses that name */
				for(index=0; index >= 0; index++)
				{
					snprintf(newLogname+oldLognameLen-1, 32, ".%d", index);

					if(access(newLogname, F_OK) == -1)
					{
						break;
					}
				}

				/* Renames the current log file to the new name thus emptying
				 * the log file for reuse */
				if(index >= 0)
				{
					rename(LOG_CONTROL->logname, newLogname);
				}

				free(newLogname);
			}
		}

		if((stream = fopen(LOG_CONTROL->logname, "a")) == NULL)
		{
			fprintf(stderr, "Error opening log file %s at %s:%d\n",
					LOG_CONTROL->logname, __FILE__, __LINE__);
			exit(EXIT_FAILURE);
		}
	}else
	{
		stream = LOG_CONTROL->stream;
	}

	fprintf(stream, "%s: ", timeStr);
	vfprintf(stream, format, vargs);
	fprintf(stream, " at %s:%d\n", filename, lineNum);

	fflush(stream);

	if(LOG_CONTROL->logname != NULL)
	{
		fclose(stream);
	}

	if(pthread_mutex_unlock(&(LOG_CONTROL->lock)) != 0)
	{
		fprintf(stderr, "Pthread mutex unlock error at %s:%d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	va_end(vargs);

    if(exitVal != 0)
    {
	   exit(EXIT_FAILURE);
    }
}
