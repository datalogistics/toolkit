
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include "jval.h"
#include "dllist.h"
#include "jrb.h"
#include "ibp.h"
#include "LoDNLogging.h"
#include "LoDNMsgQueue.h"
#include "LoDNExnode.h"
#include "LoDNDepotMgmt.h"
#include "LoDNThreads.h"
#include "LoDNFreeMappings.h"
#include "LoDNSerializedWarmer.h"
#include "LoDNAuxillaryFunctions.h"


#define LODN_DIR     			"/var/tmp/lodnwarmer"
#define MSG_FILE				"/var/tmp/lodnwarmer/msqlog"
#define NEW_MSG_FILE 			"/var/tmp/lodnwarmer/new_msqlog"
#define BASE    				"/var/tmp/lodnwarmer/msq"
#define LINEEND 				"\n"
#define WARMER_FILE             ".warmer"
#define MAX_MSG_LEN             MAXPATHLEN + 16
#define IBP_URI_LEN             6
#define LODN_ADD_EXNODE         0x01
#define LODN_REMOVE_EXNODE      0x02
#define LODN_WARMER_VERSION		"3.5.1"
#define	MAPPING_MAX_DURATION 	604800		/* One Week */



typedef struct lodn_options
{
    int   background;
    int   copies;
    char *email;
    char *path;
    char *depotfile;
    char *lboneserver;
    char  refresh;
    int   threads;
    char *logfile;
    int   maxduration;
    int   verbose;

} LoDNOptions;


typedef struct
{
    int   background;
    int   defaultCopies;
    char *depotfile;
    char *lboneserver;
    char  refreshonly;
    int   numThreads;
    Dllist paths;
    LoDNThreadPool *threadPool;
    int   msgID;

} LoDNControl;


typedef struct
{
    long mtype;
    char mtext[MAX_MSG_LEN];

} LoDNMsg;


typedef struct
{
    LoDNExnode *exnode;
    JRB         destDepots;
    LoDNExnodeSegment *segment;


} LoDNSegmentAugmentControl;


int ll_cmp(Jval val1, Jval val2)
{
    return val1.ll - val2.ll;
}



void printUsage(void)
{
    printf("Usage: LoDNWarmer options\n\n");

    printf("\t-b,--background <time>\tRun warmer continously in the background\n");
    printf("\t                        every time period where time is #d#h#m#s\n");
    printf("\t                        (days, hours, minutes, seconds).\n");
    printf("\t-c,--copies <number>\tAugments each mapping that number of times.\n");
    printf("\t                    \tOnly useful in conjunction with using the lbone.\n");
    printf("\t-d,--depot-list <file>\tUse the depots in this file to augment the\n");
    printf("\t                       \texnodes with if there is no .warmer files\n");
    printf("\t                       \tfor the exnodes.\n");
    printf("\t-e,--email <address>\tSend warmer notifications to that address\n");
    printf("\t                    \t(Not currently implemented in this release).\n");
    printf("\t-h,--help\t\tPrint this help message\n");
    printf("\t-l,--lbone-server <hostname:port> Use the lbone server at this\n");
    printf("\t                        hostname and port to get depots to use for\n");
    printf("\t                        augmentation. (Not currently implemented\n");
    printf("\t                        in this release).\n");
    printf("\t-m,--max-duration <time>\tUses this as the maximum duration time\n");
    printf("\t                        for depot allocations and refreshing.\n");
    printf("\t                        Normally the maximum allowable duration\n");
    printf("\t                        for each depot is used\n");
    printf("\t-o,--log-file <file>\tWrite all log data to this file (default is\n");
    printf("\t                    \tstdout).\n");
    printf("\t-p,--path <dir1[:dir2:dir3...]>\tWarm the exnodes that lie recursively\n");
    printf("\t                        under these directories.\n");
    printf("\t-r,--refresh-only\tDo not augment the mappings only refresh the\n");
    printf("\t                 \tmappings that are already present.\n");
    printf("\t-t,--threads <number>\tUse this many worker threads.  The number of\n");
    printf("\t                     \tthreads specified is also the maximum number\n");
    printf("\t                     \tof concurrent connections IBP connections\n");
    printf("\t                     \tallowed for the warmer.\n");
    printf("\t-V,--verbose [number]\tMakes the program be verbose about what its\n");
    printf("\t                     \tdoing and if a number is included sets the\n");
    printf("\t                     \tverbosity to that level.  The higher the \n");
    printf("\t                     \tlevel the more verbose.\n");
    printf("\t-v,--version         \tPrints the version\n");

}



void parseTime(char *timestr, int *time)
{
    /* Declarations */
    int  len;
    int  i  = 0;
    int  j  = 0;
    char amt[32];
    char times[4] = {0, 0, 0, 0};


    *time = 0;

    len = strlen(timestr);

    memset(amt, 0, sizeof(amt));

    for(i=0; i<len; i++)
    {
        if(isdigit(timestr[i]))
        {
            amt[j++] = timestr[i];
        }else
        {
            if(strlen(amt) == 0)
            {
                fprintf(stderr, "Usage error: %s is an invalid period amount\n", timestr);
                exit(EXIT_FAILURE);
            }


            switch(timestr[i])
            {
                case 'd':

                    if(times[0] != 0)
                    {
                        fprintf(stderr, "Usage error: %s is an invalid period amount\n", timestr);
                        exit(EXIT_FAILURE);
                    }

                    times[0] = 1;

                    *time += strtol(amt, NULL, 10)*86400;
                    break;

                case 'h':

                    if(times[1] != 0)
                    {
                        fprintf(stderr, "Usage error: %s is an invalid period amount\n", timestr);
                        exit(EXIT_FAILURE);
                    }

                    times[1] = 1;

                    *time += strtol(amt, NULL, 10)*3600;
                    break;

                case 'm':

                    if(times[2] != 0)
                    {
                        fprintf(stderr, "Usage error: %s is an invalid period amount\n", timestr);
                        exit(EXIT_FAILURE);
                    }

                    times[2] = 1;

                    *time += strtol(amt, NULL, 10)*60;
                    break;

                case 's':

                    if(times[3] != 0)
                    {
                        fprintf(stderr, "Usage error: %s is an invalid period amount\n", timestr);
                        exit(EXIT_FAILURE);
                    }

                    times[3] = 1;

                    *time += strtol(amt, NULL, 10);
                    break;
            }

            memset(amt, 0, sizeof(amt));
            j=0;
        }
    }

    if(strlen(amt) != 0)
    {
        if(times[3] != 0)
        {
            fprintf(stderr, "Usage error: %s is an invalid period amount\n", timestr);
            exit(EXIT_FAILURE);
        }

        times[0] = 1;

        *time += strtol(amt, NULL, 10);
    }
}

void processCommandLine(int argc, char **argv, LoDNOptions *options)
{
    /* Declarations */
    char *optstring          =  "b:c:d:e:hl:o:m:p:rt:V::v";
    struct option longopts[] =  {
                                    { "background",   1, NULL, 'b' },
                                    { "copies",       1, NULL, 'c' },
                                    { "email",        1, NULL, 'e' },
                                    { "help",         0, NULL, 'h' },
                                    { "path",         1, NULL, 'p' },
                                    { "depot-list",   1, NULL, 'd' },
                                    { "lbone-server", 1, NULL, 'l' },
                                    { "max-duration", 1, NULL, 'm' },
                                    { "refresh-only", 0, NULL, 'r' },
                                    { "threads",      1, NULL, 't' },
                                    { "log-file",     1, NULL, 'o' },
                                    { "verbose",      2, NULL, 'V' },
                                    { "version",      0, NULL, 'v' },
                                    { NULL,           0, NULL, 0   }
                                };
    int charr;


    /* Initialize the lodn options */
    options->background  = -1;
    options->copies      = -1;
    options->depotfile   = NULL;
    options->email       = NULL;
    options->lboneserver = NULL;
    options->logfile     = NULL;
    options->path        = NULL;
    options->refresh     = 0;
    options->threads     = 1;
    options->verbose     = 0;
    options->maxduration = -1;


    /* Iterates over the command line parsing the args */
    while((charr = getopt_long(argc, argv, optstring, longopts, NULL)) > 0)
    {
        switch(charr)
        {
            /* Gets the wait time between successive runs */
            case 'b':

                parseTime(optarg, &options->background);

                if(options->background < 1)
                {
                    fprintf(stderr, "Usage Error: background duration must be greater than 0\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Gets the number of copies */
            case 'c':

                if(sscanf(optarg, "%d", &options->copies) != 1 ||
                   options->copies < 1)
                {
                    fprintf(stderr, "Usage Error: copies must be a integer > 0.\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Gets the depot file to use by default */
            case 'd':

                if((options->depotfile = strdup(optarg)) == NULL)
                {
                    fprintf(stderr, "Memory Allocation Error\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Gets the email address of the user */
            case 'e':

                if((options->email = strdup(optarg)) == NULL)
                {
                    fprintf(stderr, "Memory Allocation Error\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Prints the usage message */
            case 'h':
                printUsage();

                exit(EXIT_SUCCESS);

                break;

            /* Gets the name of the lbone server */
            case 'l':

                if((options->lboneserver = strdup(optarg)) == NULL)
                {
                    fprintf(stderr, "Memory Allocation Error\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Gets the log file */
            case 'o':

                if((options->logfile = strdup(optarg)) == NULL)
                {
                    fprintf(stderr, "Memory Allocation Error\n");
                    exit(EXIT_FAILURE);
                }

                break;


            /* Gets the maximum duration */
            case 'm':

            	parseTime(optarg, &options->maxduration);

            	if(options->maxduration < 1)
                {
                    fprintf(stderr, "Usage Error: maximum duration must be greater than 0\n");
                    exit(EXIT_FAILURE);
                }

                break;


            /* Gets the content path */
            case 'p':

                if((options->path = strdup(optarg)) == NULL)
                {
                    fprintf(stderr, "Memory Allocation Error\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Gets the refresh only option */
            case 'r':

                options->refresh = 1;

                break;

            /* Gets the number of threads to run with */
            case 't':

                if(sscanf(optarg, "%d", &options->threads) != 1 ||
                   options->threads < 1)
                {
                    fprintf(stderr, "Usage Error: number of threads must be an int > 0\n");
                    exit(EXIT_FAILURE);
                }

                break;

            /* Prints the version */
            case 'v':

            	printf("Version: LoDNWarmer %s\n", LODN_WARMER_VERSION);
            	exit(EXIT_SUCCESS);

            	break;

            /* Gets the verbosity level */
            case 'V':

            	options->verbose = 1;

            	if(optarg != NULL && sscanf(optarg, "%d", &options->verbose) != 1)
            	{
            		fprintf(stderr, "Usage Error: verbosity level must be an int >= 0\n");
            		exit(EXIT_FAILURE);
            	}

            	break;

            /* Illegal value */
            default:

                fprintf(stderr, "Usage Error: Illegal flag option\n");
                exit(EXIT_FAILURE);

                break;
        }
    }

    /* Checks that a content path was specified */
    if(options->path == NULL)
    {
        fprintf(stderr, "Usage Error: No content path specified\n");
        exit(EXIT_FAILURE);
    }
}


void segFaultHandler(int signum)
{

    /* Checks that the SIGALRM caused this to be called */
    if(signum != SIGSEGV)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Unknown signal invoked seg fault handler");
    }

    logPrintError(0, __FILE__, __LINE__, 0, "Seg Fault has occurred, freeing resources");

    /* Exit with failure */
    exit(EXIT_FAILURE);
}



void alarmHandler(int signum)
{
    /* Declarations */

    /* Check that SIGALRM caused this to be called */
    if(signum != SIGALRM)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Unknown signal invoked alarm handler");
    }

    /* Do nothing -- just put in a no-op for catching it */
}



void becomeDeamon(void)
{
    /* Declarations */
    pid_t pid;
    /*int childStatus;*/


    /* Creates a child process so that the parent can return */
    if((pid = fork()) < 0)
    {
    	logPrintError(0, __FILE__, __LINE__, 1, "Error forking for deamonizing the process");
    }else if(pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    /* Sets the current process to being a session leader */
    if(setsid() < 0)
    {
    	logPrintError(0, __FILE__, __LINE__, 1, "Error setting session leader for deamonizing process");
    }

    /* Creates a child process so that the current process (session leader) can
     * exit */
    if((pid = fork()) < 0)
    {
    	logPrintError(0, __FILE__, __LINE__, 1, "Error forking for deamonizing the process");

    }else if(pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    /* Clears the umask */
    umask(0);

    /* Process is now a deamon and has no connected terminal */

    /* Changes the to the root directory */
    if(chdir("/") < 0)
    {
    	logPrintError(0, __FILE__, __LINE__, 1, "Error setting directory for deamonizing the process");
    }


    /* Forks one more time so the current process becomes the warmer process
     * control and the child the actual warmer */
    /*while((pid = fork()) != 0)
    {*/
        /* Checks that the fork was successful */
       /* if(pid < 0)
        {
        	logPrintError(0, __FILE__, __LINE__, 1, "Error forking for deamoninzing the process");
        }*/

        /* Waits on the child process to die */
        /*if(waitpid(pid, &childStatus, 0) != pid)
        {
        	logPrintError(0, __FILE__, __LINE__, 1, "Error with waiting on child");
        }*/

        /*logPrintError(0, __FILE__, __LINE__, 0, "Warmer has died, restarting in 15 seconds"); */

        /* Sleep 15 seconds before respawning to keep from having a problem
         * with constant death from the warmer */
        /*sleep(15);*/
    /*}*/


    /* Releases the stdin, stdout and stderr */
    /*close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);*/
}


/* Remove function for newer version in LoDNMsgQueue.c */
#if 0
int getExnodeCommandsFromQueue(int queueID, Dllist addExnodesList,
                               Dllist removeExnodesList, int wait)
{
    /* Declarations */
    LoDNMsg msg;
    char   *command;
    char   *exnodeName;
    int     retval;
    int     msgflg;
    int     gotMesg;
    int     numMsg = 0;
    int     stop = 0;
    int     errorOccurred;
    Dllist  list;
    char    resolvedPath[PATH_MAX];


    /* Sets whether or not to block on message */
    msgflg = (wait != 0) ? 0 : IPC_NOWAIT;

    /* Runs while there are messages */
    do
    {
        /* Intializes the flag for message detection */
        gotMesg = 0;

        /* Gets a message from the queue if there is one*/
        retval = msgrcv(queueID, &msg, MAX_MSG_LEN, 0, msgflg);


        /* If zero then there is a mesage */
        if(retval > 0)
        {
            logPrint(1, "New request from the message queue.");

            /* Sets the flag for message detection */
            gotMesg = 1;

            /* Initializes the error detection flag */
            errorOccurred = 1;

            /* Checks the command type and which list to insert into */
            switch(msg.mtype)
            {
                case LODN_ADD_EXNODE:
                    command = "add";
                    list = addExnodesList;
                    break;

                case LODN_REMOVE_EXNODE:
                    command = "remove";
                    list = removeExnodesList;
                    break;

                default:
                    list = NULL;
                    break;
            }

            /* If it is add or remove */
            if(list != NULL)
            {
                /* Resolves the path */
                if(realpath(msg.mtext, resolvedPath) != NULL)
                {
                    /* Allocates a copy of the exnode name */
                    if((exnodeName = strdup(resolvedPath)) == NULL)
                    {
                        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                    }

                    /* Appends the entry to the list */
                    dll_append(list, new_jval_s(exnodeName));

                    /* Increments the number of recieved messages */
                    numMsg++;

                    /* Sets error flag to NO */
                    errorOccurred = 0;
                }
            }

            /* Prints message result */
            if(errorOccurred == 0)
            {
                logPrint(1, "Received message to %s %s", command, exnodeName);
            }else
            {
                logPrintError(0, __FILE__, __LINE__, 0, "Recieved a bad message from message queue");
            }

        /* Message queue error */
        }else if(errno != ENOMSG && errno != EINTR)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "Message queue receive error");
        }else
        {
            if(errno == EINTR)
            {
            	/* Do nothing */
            }

            stop = 1;
        }

        /* Sets the message queue to not wait for any more message and just
         * collect what's there */
        msgflg = IPC_NOWAIT;

    }while(gotMesg > 0 && stop == 0);


    /* Returns the number of messages */
    return numMsg;
}
#endif



void convert2AbsolutePath(char **path)
{
    /* Declarations */
    char  resolved_path[PATH_MAX];


    if(path == NULL || *path == NULL)
    {
        return;
    }

    /* Gets the full path */
    if(realpath(*path, resolved_path) == NULL)
    {
        fprintf(stderr, "Error getting absolute path for %s\n", *path);
        exit(EXIT_FAILURE);
    }

    /* Frees the old allocation */
    free(*path);

    /* Creates a new allocation with the full path of the logfile */
    if((*path = strndup(resolved_path, PATH_MAX))==NULL)
    {
        fprintf(stderr, "Memory Allocation Error at %s:%d\n", __FILE__, __LINE__);
        exit(EXIT_FAILURE);
    }
}



#if 0

/****----------------------processMessageQueue()---------------------------****
 * Description:  This function uses the lodn warmer log file to verify that
 *               no other warmer is running on the current path.  It then
 *               creates a new entry in the log file and a message queue
 *               file to be used.  It generates a message queue file and
 *               logs it for the duration of the process.  It returns an int
 *               that holds the id of the message queue.
 * Input: paths - Dllist of the paths to check.
 * Output: It returns the id of the message queue as an int.
 ****----------------------------------------------------------------------****/
int processMessageQueue(Dllist paths)
{
	/* Declarations */
	int  fd  = 0;
	int  nfd = 0;
	int  mfd = 0;
	int  amtRead;
	int  offset  = 0;
	char buffer[1024];
	char msqfile[1024];
	char directory[1024];
	char *path;
	char *start;
	char *sep;
	char *sep2;
	struct flock lock;
	int  remove;
	int  pathFound = 0;
	int  warmerRunning = 0;
	char *lineEnd = LINEEND;
	int path1Len;
	int path2Len;
	int cmpLen;
	Dllist pnode;
	int msgID = -1;
	int msgKey;
	mode_t mode;
	struct iovec wvec[] = { { directory, 0 }, { msqfile, 0 },
		                    { lineEnd, 1*sizeof(char) }      };


	/* Creates the process directory */
	mode = umask(0);
	mkdir(LODN_DIR, S_IRWXU|S_IRWXG|S_IRWXO|S_ISVTX);


	/* Opens the process control file for writing */
	do
	{
		if(nfd < 0)
		{
			usleep(10000);
		}

		if((nfd = open(NEW_MSG_FILE, O_WRONLY|O_CREAT|O_EXCL,
			       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) < 0 && errno != EEXIST)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Couldn't open %s\n", NEW_MSG_FILE);
		}

	}while(nfd < 0 && errno == EEXIST);

	lock.l_start  = 0;
	lock.l_len    = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type   = F_WRLCK;

	/* Attempts to lock the file and waits until it can have a write lock. */
	if(fcntl(nfd,  F_SETLKW , &lock) == -1)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Couldn't lock %s", NEW_MSG_FILE);
	}


	/* Opens the process control file for reading and writing */
	if((fd = open(MSG_FILE, O_RDONLY|O_CREAT,
			                S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) < 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Couldn't open %s", MSG_FILE);
	}


	/* Reads the main process file into the buffer */
	while((amtRead=read(fd, buffer+offset, sizeof(buffer)-offset)) > 0)
	{
		/* Sets the about of data available */
		amtRead += offset;
		start    = buffer;

		while((sep  = memchr(start, '\n', amtRead))   != NULL &&
			  (sep2 = memchr(start, '\0', sep-start)) != NULL)
		{
			/* Gets the directory and message queue file */
			strncpy(directory, start, sep-start);
			strncpy(msqfile, sep2+1, sep-sep2-1);
			msqfile[sep-sep2-1] = '\0';

			pathFound = 0;

			/* Traverses the paths in the list */
			dll_traverse(pnode, paths)
			{
				path = pnode->val.s;

				/* Gets the length of the shortest directory */
				path1Len = strlen(directory);
				path2Len = strlen(path);

				cmpLen = (path1Len < path2Len) ? path1Len : path2Len;

				/* Compares the two directories */
				if(strncmp(directory, path, cmpLen) == 0)
				{
					pathFound++;
				}
			}

			/* Updates the amount of data available and the start of the fresh
			 * data */
			amtRead -= sep-start;
			start    = sep+1;

			/* Holds whether to remove the entry from the process file */
			remove = 1;

			/* Checks if the message queue file exists */
			if(access(msqfile, F_OK) == 0)
			{
				/* Opens a file descriptor to the message queue file */
				if((mfd = open(msqfile, O_RDONLY)) < 0)
				{
					remove = 0;
				}else
				{
					lock.l_start  = 0;
					lock.l_len    = 0;
					lock.l_whence = SEEK_SET;
					lock.l_type   = F_RDLCK;

					/* Attempts to lock the file and it fails then the warmer
					 * is still running.  If it can lock the file, then
					 * warmer is no running so the entry needs to be removed */
					if(fcntl(mfd, F_SETLK, &lock) == -1)
					{
						remove = 0;
					}else
					{
						remove = 1;
					}

					/* Closes the file */
					close(mfd);
				}

			/* If the message queue file doesn't exist then remove the entry */
			}else
			{
				remove = 1;
			}

			/* Removes the warmer entry and message queue file */
			if(remove == 1)
			{
				/* Deletes the previous queue */
				msgKey = ftok(msqfile, 0x01);
				msgID  = msgget(msgKey, 0);
				msgctl(msgID, IPC_RMID, NULL);

				unlink(msqfile);

				pathFound = 0;

			/* Otherwise it writes the entry to the new log file */
			}else
			{
				/* Updates the write iovec */
				wvec[0].iov_len  = (strlen(directory)+1)*sizeof(char);
				wvec[1].iov_len  = (strlen(msqfile))*sizeof(char);

				/* Writes the entry to the new log file */
				if(writev(nfd, wvec, 3) < 0)
				{
					logPrintError(0, __FILE__, __LINE__, 1, "Error writing to %s\n", NEW_MSG_FILE);
				}
			}

			/* If pathFound is greater than 0 then it sets the flag to
			 * indicate a warmer is actively running on the path */
			if(pathFound > 0)
			{
				warmerRunning++;
			}
		}

		/* Sets the starting offset */
		offset = amtRead;

		/* Moves left over data to the start of the buffer */
		memcpy(buffer, buffer+sizeof(buffer)-offset, offset);
	}

	/* Checks for any reading errors */
	if(amtRead < 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error reading from %s", MSG_FILE);
	}

	/* If a warmer is already running then there it states it */
	if(warmerRunning > 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1,
				     "Warming already running on a path at or beneath this one\n");

	}else
	{
		/* Forms the name of the message queue */
		sprintf(msqfile, "%s.%d", BASE, getpid());

		/* Creates the message queue file and gets a file descriptor for it */
		if((mfd = open(msqfile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Couldn't open %s", msqfile);
		}

		/* Sets the lock for an exclusive lock */
		lock.l_start  = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_type = F_WRLCK;

		/* Locks the message queue file */
		if(fcntl(mfd, F_SETLK, &lock) == -1)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Couldn't lock %s", msqfile);
		}

		/* Traverses the paths in the list */
		dll_traverse(pnode, paths)
		{
			path = pnode->val.s;

			/* Sets the iovecs to the new values and updates the lengths */
			wvec[0].iov_base = path;
			wvec[0].iov_len  = (strlen(path)+1)*sizeof(char);
			wvec[1].iov_len  = (strlen(msqfile))*sizeof(char);

			/* Writes the new warmer entry to the new log file */
			if(writev(nfd, wvec, 3) < 0)
			{
				logPrintError(0, __FILE__, __LINE__, 1, "Error writing to %s", NEW_MSG_FILE);
			}
		}

		/* Creates the key to identify the queue */
	    if((msgKey = ftok(msqfile, 0x01)) < 0)
	    {
	        logPrintError(0, __FILE__, __LINE__, 1, "Error creating message queue");
	    }else
	    {
	        /* Deletes the previous queue */
	        msgctl(msgKey, IPC_RMID, NULL);

	        /* Creates the message queue and gets the id of queue */
	        if((msgID = msgget(msgKey,IPC_CREAT|S_IRUSR|S_IWUSR)) < 0)
	        {
	            logPrintError(0, __FILE__, __LINE__, 1, "Error creating message queue");
	        }
	    }
	}

	/* Renames the file */
	if(rename(NEW_MSG_FILE, MSG_FILE) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Couldn't rename %s to %s",
				      NEW_MSG_FILE, MSG_FILE);
	}

	/* Closes the file descriptor */
	close(nfd);
	close(fd);

	/* Restores the mask */
	umask(mode);

	/* Returns the message queue filename to use */
	return msgID;
}

#endif


void initialize(LoDNOptions *options, LoDNControl *control)
{
    /* Declarations */
    char *last;
    char *token;
    char *path;
    struct stat statbuf;
    char  resolved_path[PATH_MAX];
    FILE  *tmpfile;


    /* Creates the log file if it doesn't exist */
    if(options->logfile != NULL)
    {
	    if((tmpfile = fopen(options->logfile, "a")) == NULL)
	    {
	    	fprintf(stderr, "Error opening/creating logfile %s at %s:%d\n",
	    			options->logfile, __FILE__, __LINE__);
	    	exit(EXIT_FAILURE);
	    }else
	    {
	    	fclose(tmpfile);
	    }
    }

    /* Gets the full path of the log file and depot file*/
    convert2AbsolutePath(&(options->logfile));
    convert2AbsolutePath(&(options->depotfile));

    /* Print the options */
    if(options->verbose > 1)
    {
	    printf("Options:\n");
	    printf("  background:  %d\n", options->background);
	    printf("  copies:      %d\n", options->copies);
	    printf("  email:       %s\n", options->email);
	    printf("  path:        %s\n", options->path);
	    printf("  depotfile:   %s\n", options->depotfile);
	    printf("  maxduration  %d\n", options->maxduration);
	    printf("  lboneserver: %s\n", options->lboneserver);
	    printf("  refresh:     %d\n", options->refresh);
	    printf("  threads:     %d\n", options->threads);
	    printf("  logfile:     %s\n", options->logfile);
    }

    /* Creates a list to hold all of the paths */
    if((control->paths = new_dllist()) == NULL)
    {
        fprintf(stderr, "Memory Allocation Error\n");
        exit(EXIT_FAILURE);
    }

    /* Parses the path list separated by ':' and puts it into the list of
     * of paths */
    for(token=strtok_r(options->path, ":", &last); token != NULL;
        token=strtok_r(NULL, ":", &last))
    {
        /* Resolves the path */
        if(realpath(token, resolved_path) == NULL)
        {
            fprintf(stderr, "Error: %s is not a valid path\n", token);
            exit(EXIT_FAILURE);
        }

        /* Verifies the path is a directory or file */
        if(lstat(resolved_path, &statbuf) == -1 ||
           !(S_ISDIR(statbuf.st_mode) || S_ISREG(statbuf.st_mode)))
        {
            fprintf(stderr, "Error %s in not a valid path\n", token);
            exit(EXIT_FAILURE);
        }

        /* Allocates a copy of the path */
        if((path = strdup(resolved_path)) == NULL)
        {
            fprintf(stderr, "Memory Allocation Error\n");
            exit(EXIT_FAILURE);
        }

        /* Inserts it into the list of paths */
        dll_append(control->paths, new_jval_s(path));
    }

    /* Assigns the refresh only option */
    control->refreshonly = (char)options->refresh;

    /* Assigns the default number of copies */
    control->defaultCopies = options->copies;

    /* Assigns the depot file */
    control->depotfile = options->depotfile;

    /* Assigns the lbone server */
    control->lboneserver = options->lboneserver;

    /* Assigns background */
    control->background = options->background;

    /* Assigns the number of threads */
    control->numThreads = options->threads;

    /* Initializes the depot mgmt module */
    LoDNDepotMgmtInit(options->maxduration);

    /* Initializes the logging */
    logInit(options->logfile, options->verbose);

    /* If the background is set then it makes this program run as a daemon */
    if(control->background > 0)
    {
        becomeDeamon();

        /* Creates the file that represents the message queue */
        /* Initializes the message queue */
        if((control->msgID = LoDNMsgQueueInit(control->paths)) < 0)
        {
        	logPrintError(0, __FILE__, __LINE__, 1, "Message queue could not be setup.");
        }
    }else
    {
    	control->msgID = -1;
    }



    /* Sets the SIGALARM handler */
    if(signal(SIGALRM, alarmHandler) == SIG_ERR)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Unable to set signal handler");
    }

    /* Sets the SIGSEGV handler */
    /*if(signal(SIGSEGV, segFaultHandler) == SIG_ERR)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "unable to set signal handler");
    }*/

    /* Print statement of success for initialization */
    logPrint(1, "Warmer has been successfully initialized");
}




char *findWarmerFileByExnode(char *exnodeName, LoDNControl *control)
{
    /* Declarations */
    char warmerFileName[MAXPATHLEN];
    char *dirDelimiter = NULL;
    Dllist dlnode1;
    int    found = 0;
    int    pathLimit = 0;


    if(control->depotfile != NULL)
    {
    	logPrint(2, "Using depot file %s as warmer file.", control->depotfile);
        return strdup(control->depotfile);
    }

    sprintf(warmerFileName, "%s%s", exnodeName, ".warmer");

    /* Tests for the warmer file in the current directory */
    do
    {
        logPrint(2, "Checking for warmer %s.", warmerFileName);

        if(access(warmerFileName, F_OK | R_OK) == 0)
        {
            logPrint(2, "%s warmer has been found.", warmerFileName);
            found = 1;
            break;

        }else if(dirDelimiter != NULL)
        {
            *dirDelimiter = '\0';

            dll_traverse(dlnode1, control->paths)
            {
                fwrite(warmerFileName, 1, dirDelimiter-warmerFileName, stdout);
                if(strncmp(dlnode1->val.s, warmerFileName, dirDelimiter-warmerFileName) == 0)
                {
                    pathLimit = 1;
                    break;
                }
            }
        }

        if((dirDelimiter = strrchr(warmerFileName, '/')) == NULL)
        {
            break;
        }

        *(dirDelimiter+1) = '\0';

        strncat(warmerFileName, ".warmer", MAXPATHLEN);

    }while(found == 0 && pathLimit == 0);


    if(found == 1)
    {
        return strdup(warmerFileName);
    }

    return NULL;
}



void refresh_job(void *arg)
{
    /* Declarations */
    LoDNExnodeMapping   *mapping;


    /* Assigns the argument as a LoDNExnode mapping */
    mapping = (LoDNExnodeMapping*)arg;

    logPrint(2, "Starting refresh for mapping at %ld of %s",
    		   (long int)mapping->exnode_offset, mapping->exnode->name);

    mapping->status = LoDNDepotMgmtRefreshMapping(mapping);

    pthread_mutex_lock(&mapping->exnode->lock);
    mapping->exnode->numTasks--;
    pthread_mutex_unlock(&mapping->exnode->lock);

    logPrint(2, "Finished refresh for mapping at %ld of %s in %s",
        		   (long int)mapping->exnode_offset, mapping->exnode->name,
        		   (mapping->status==0) ? "success" : "failure");

    return;


    /* Silence the compiler */
    goto ERROR_HANDLER;

    /* Error Handler */
    ERROR_HANDLER:

        pthread_mutex_lock(&mapping->exnode->lock);
        mapping->exnode->numTasks--;
        pthread_mutex_unlock(&mapping->exnode->lock);

}




/****---------------------------free_job()---------------------------------****
 * Description: This function is used by a LoDN thread job to free a list
 *              of read capabilites.
 * Input: arg - void pointer to the dllist of IBP capabilites.
 ****----------------------------------------------------------------------****/
void free_job(void *arg)
{
	/* Declarations */
	LoDNFreeMappings_t *freeMappings;
	IBP_cap cap;


	/* Gets a pointer to the dllist of mappings caps to free */
	freeMappings = (LoDNFreeMappings_t*)arg;


	/* Frees all of the mappings in the list */
	while((cap = LoDNFreeMappingsGet(freeMappings)) != NULL)
	{
		/* Attempts to free the mappings */
		LoDNDepotMgmtFreeReadCap(cap);

		/* Frees the mapping cap and the node */
		free(cap);
	}
}



void augment_job2(void *arg)
{
    /* Declarations */
    LoDNSegmentAugmentControl  *segAugCtrl;
    LoDNExnodeSegment *segment;
    LoDNExnodeMapping *mapping;
    Dllist             dlnode1;
    JRB                jnode1;
    char              *depotName;
    long long          allocLength;
    Dllist             srcMappings = NULL;
    Dllist             destDepots  = NULL;
    Dllist newMappings = NULL;
    LoDNExnodeMapping *newMapping = NULL;
    int                numNewMappings = 0;

    /* Gets a pointer to the segment augmentation */
    segAugCtrl = (LoDNSegmentAugmentControl*)arg;

    /* Gets the segment */
    segment = segAugCtrl->segment;

    /* Print statement */
    logPrint(2, "Starting augment for %s exnode's segment from %lld to %lld.",
    		  segment->exnode->name, segment->start, segment->end);

    /* Allocates a list of new mappings */
    if((srcMappings = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Allocates a list of new mappings */
    if((destDepots = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Locks the segment mutex */
    if(pthread_mutex_lock(&(segment->exnode->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
    }

    /* Traverses the mappings in the segment and puts the good ones in the list
     * to use for replication */
    dll_traverse(dlnode1, segment->mappings)
    {
        /* Gets the current mapping */
        mapping = (LoDNExnodeMapping*)dlnode1->val.v;

        /* Status is good so add it to the list of source mappings */
        if(mapping->status >= 0)
        {
            dll_append(srcMappings, new_jval_v(mapping));
        }
    }

    /* Unlocks the mutex for the segment */
    if(pthread_mutex_unlock(&(segment->exnode->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
    }

    /* Gets the list of depots that can be copied to */
    jrb_traverse(jnode1, segAugCtrl->destDepots)
    {
        /* Gets the name of the depot */
        depotName = jnode1->key.s;

        /* Adds the depot to the list */
        dll_append(destDepots, new_jval_s(depotName));
    }

    /* Calculates the length of the segment */
    allocLength = segAugCtrl->segment->end - segAugCtrl->segment->start + 1;

    /* Attempts to the copy the mapping */
    newMappings = LoDNDepotMgmtCopyData(srcMappings, destDepots, allocLength, 1, -1);

    /* If there is an error then newMappings will be NULL */
    if(newMappings == NULL)
    {
        logPrint(1, "Augmenting for %s exnode's segment from %ld to %ld Failed.",
    	    		  segment->exnode->name, segment->start, segment->end);
        goto ERROR_HANDLER;
    }

    /* Locks the exnode mutex */
    if(pthread_mutex_lock(&(segAugCtrl->exnode->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
    }

    /* Adds the mappings to the exnode */
    dll_traverse(dlnode1, newMappings)
    {
        /* Gets the new mapping */
        newMapping = (LoDNExnodeMapping*)dlnode1->val.v;

        logPrint(2, "Adding new mapping for segment from %ld to %ld to %s exnode.",
        		  (long int)segment->start, (long int)segment->end, segment->exnode->name);



        /* Insert new mapping into exnode */
        if(newMapping != NULL)
        {
        	/* Specifies that the mapping is in the warmer */
        	newMapping->inWarmer++;

            if(LoDNExnodeInsertMapping(segAugCtrl->exnode, newMapping) != 0)
            {
                free(newMapping->readCap);
                free(newMapping->writeCap);
                free(newMapping->manageCap);
                free(newMapping);
            }else
            {
            	numNewMappings++;
            }
        }
    }

    /* Unlocks the exnode mutex */
    if(pthread_mutex_unlock(&(segAugCtrl->exnode->lock)) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
    }

    /* Frees the resources */
    free(segAugCtrl);
    free_dllist(srcMappings);
    free_dllist(destDepots);
    free_dllist(newMappings);

    /* Locks the exnode */
    if(pthread_mutex_lock(&segment->exnode->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
    }

    /* Decrements the number of running exnode taskes */
    segment->exnode->numTasks--;

    /* Unlocks the exnode */
    if(pthread_mutex_unlock(&segment->exnode->lock) != 0)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
    }

    logPrint(2, "Done augmenting for %s exnode's segment from %ld to %ld with %d new mappings.",
    		  segment->exnode->name, (long int)segment->start,
    		  (long int)segment->end, numNewMappings);

    /* Returns from the function */
    return;


    /* Error Handler */
    ERROR_HANDLER:

        /* Detects if segAutCtrl is valid */
        if(segAugCtrl != NULL)
        {
            /* Frees the list of source mappings */
            if(srcMappings != NULL)
            {
                free_dllist(srcMappings);
            }

            /* Frees the list of destination depots */
            if(destDepots != NULL)
            {
                free_dllist(destDepots);
            }

            /* Frees the new mappings */
            if(newMappings != NULL)
            {
                /* Locks the exnode */
                if(pthread_mutex_lock(&(segAugCtrl->exnode->lock)) != 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
                }

                /* Traverse any good mappings and adds them to the exnode */
                dll_traverse(dlnode1, newMappings)
                {
                    /* Good mapping */
                    newMapping = (LoDNExnodeMapping*)dlnode1->val.v;

                    /* Insert new mapping into exnode */
                    if(newMapping != NULL)
                    {
                        if(LoDNExnodeInsertMapping(segAugCtrl->exnode, newMapping) != 0)
                        {
                            free(newMapping->readCap);
                            free(newMapping->writeCap);
                            free(newMapping->manageCap);
                            free(newMapping);
                        }
                    }
                }

                /* Unlocks the exnode */
                if(pthread_mutex_unlock(&(segAugCtrl->exnode->lock)) != 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
                }

                /* Frees the list of mappings */
                free_dllist(newMappings);
            }

            /* Locks the exnode */
            if(pthread_mutex_lock(&segment->exnode->lock) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "Error locking pthread mutex");
            }

            /* Decreases the number of taskes */
            segment->exnode->numTasks--;

            /* Unlocks the exnode */
            if(pthread_mutex_unlock(&segment->exnode->lock) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking pthread mutex");
            }

            /* Frees the segment */
            free(segAugCtrl);
        }

        return;
}


void augment_job(void *arg)
{
    /* Declarations */
    LoDNSegmentAugmentControl  *segAugCtrl;
    LoDNExnodeSegment *segment;
    LoDNExnodeMapping *mapping;
    Dllist             dlnode1;
    JRB                jnode1;
    char              *depotName;
    char              *sep;
    struct ibp_depot   depot;
    IBP_set_of_caps       caps       = NULL;
    unsigned long int             allocLength;
    struct ibp_timer      timeout    = {120, 120};
    struct ibp_attributes attributes = { time(NULL) + (60 * 60 * 24 * 2),
                                          IBP_STABLE, IBP_BYTEARRAY
                                        };
    unsigned long int amtCopied  = 0;
    Dllist newMappings = NULL;
    LoDNExnodeMapping *newMapping = NULL;
    int                numCopies = 0;

    printf("Starting augment\n");

    /* Gets a pointer to the segment augmentation */
    segAugCtrl = (LoDNSegmentAugmentControl*)arg;

    segment = segAugCtrl->segment;

    printf("thread %d warming segment %lld!!!\n", (int)pthread_self(), segAugCtrl->segment->start);


    /* Allocates a list of new Mappings */
    if((newMappings = new_dllist()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }


    jrb_traverse(jnode1, segAugCtrl->destDepots)
    {
        depotName = jnode1->key.s;

        if((sep = strchr(depotName, ':')) == NULL)
        {
            continue;
        }

        strncpy(depot.host, depotName, sep-depotName);
        depot.host[sep-depotName+1] = '\0';

        if(sscanf(sep+1, "%d", &depot.port) != 1 || depot.port < 1 || depot.port > 65536)
        {
            continue;
        }

        printf("Depot host %s port %d\n", depot.host, depot.port);

        allocLength = segAugCtrl->segment->end - segAugCtrl->segment->start + 1;

        printf("Allocation Length %ld\n\n", allocLength); fflush(stdout);

        /* Attempts to allocate a buffer on the destination depot */
        if((caps = IBP_allocate(&depot, &timeout, allocLength, &attributes)) == NULL)
        {
            logPrint(2, "unable make allocation of length %d on %s", allocLength, depotName);
            continue;
        }

        printf("Dest Cap is %s\n", caps->writeCap);


        // if successful, then loop through list of mappings and try to copy to
        // destination depot.

        dlnode1 = segment->mappings;

        while(numCopies != 1)
        {
            /* Gets the next mapping */
            pthread_mutex_lock(&(segment->exnode->lock));
            dlnode1 = jrb_next(dlnode1);
            pthread_mutex_unlock(&(segment->exnode->lock));

            /* Stops when the end is reached */
            if(dlnode1 == segment->mappings)
            {
                break;
            }

            mapping = (LoDNExnodeMapping*)dlnode1->val.v;

            printf("Testing mapping %s\t", mapping->readCap);

            if(mapping->status >= 0)
            {
                printf("Good mapping\n");

                timeout.ClientTimeout = 120;
                timeout.ServerSync    = 120;

                /* Attempts to make a copy on the destination depot */
                do
                {
                    printf("Allocation Length %ld\n", allocLength);

                    amtCopied = IBP_copy(mapping->readCap, caps->writeCap, &timeout,
                                            &timeout, allocLength, 0);

                    printf("AmtCopied was %ld %d \n", amtCopied, IBP_errno);

                    if(amtCopied < 0)
                    {
                        logPrintError(0, __FILE__, __LINE__, 0,
                                      "Transfer from %s:%d to %s of size %d failed with ibp error %d",
                                      mapping->hostname, mapping->port, depotName, allocLength, IBP_errno);
                    }else if(amtCopied < allocLength)
                    {
                        timeout.ClientTimeout *= 2;
                        timeout.ServerSync    *= 2;
                    }else
                    {
                        printf("Successful mapping copy %s\n", caps->writeCap);



                        /* Creates a new Mapping */
                        if((newMapping = LoDNExnodeCreateMapping(caps, allocLength,
                            0, mapping->exnode_offset, mapping->logical_length)) == NULL)
                        {

                            //Error
                        }

                        /* Appends the new mapping to list of mappings to add */
                        dll_append(newMappings, new_jval_v(newMapping));

                        /* Increments the number of successful copies made */
                        numCopies++;
                    }

                /* Stops once the whole allocation is made or there is an error */
                }while(amtCopied != allocLength && amtCopied > 0);

            }
        }
    }


    /* Adds the mappings to the exnode */
    dll_traverse(dlnode1, newMappings)
    {
        newMapping = (LoDNExnodeMapping*)dlnode1->val.v;

        printf("Adding mapping on %s to exnode\n", newMapping->readCap);

        pthread_mutex_lock(&(segAugCtrl->exnode->lock));

        /* Insert new mapping into exnode */
        if(newMapping != NULL)
        {
            if(LoDNExnodeInsertMapping(segAugCtrl->exnode, newMapping) != 0)
            {
                free(newMapping->readCap);
                free(newMapping->writeCap);
                free(newMapping->manageCap);
                free(newMapping);
            }
        }


        pthread_mutex_unlock(&(segAugCtrl->exnode->lock));
    }

    /* Frees the resources */
    free(segAugCtrl);
    free_dllist(newMappings);

    /* Decrements the number of running exnode taskes */
    pthread_mutex_lock(&segment->exnode->lock);
    segment->exnode->numTasks--;
    pthread_mutex_unlock(&segment->exnode->lock);


    /* Returns from the function */
    return;


    /* Silence the compiler */
    goto ERROR_HANDLER;

    /* Error Handler */
    ERROR_HANDLER:

        if(segAugCtrl != NULL)
        {
            free(segAugCtrl);
        }

        if(newMappings != NULL)
        {
            pthread_mutex_lock(&(segAugCtrl->exnode->lock));

            dll_traverse(dlnode1, newMappings)
            {
                newMapping = (LoDNExnodeMapping*)dlnode1->val.v;

                /* Insert new mapping into exnode */
                if(newMapping != NULL)
                {
                    if(LoDNExnodeInsertMapping(segAugCtrl->exnode, newMapping) != 0)
                    {
                        free(newMapping->readCap);
                        free(newMapping->writeCap);
                        free(newMapping->manageCap);
                        free(newMapping);
                    }
                }
            }

            pthread_mutex_unlock(&(segAugCtrl->exnode->lock));

            free_dllist(newMappings);
        }

        pthread_mutex_lock(&segment->exnode->lock);
        segment->exnode->numTasks--;
        pthread_mutex_unlock(&segment->exnode->lock);

        return;
}


/****-----------------------refreshExnodesFromList()-----------------------****
 * Description: This function takes a list of exnodes and traverses the list of
 *              mappings in the exnode and assigns a thread task to each one
 *              to attempt to refresh it. Once the exnode is done is put onto
 *              the outgoing list.
 * Input: control - pointer to the LoDNControl struct.
 *        unrefreshedExnodeList - JRB list of of the exnodes to be refreshed.
 * Output: unwarmedExnodeList - JRB list that gets the exnodes that have been
 *                              refreshed.
 ****----------------------------------------------------------------------****/
void refreshExnodesFromList(LoDNControl *control, JRB unrefreshedExnodeList,
                            JRB unwarmedExnodeList)
{
    /* Declarations */
    LoDNExnode *exnode;
    LoDNExnodeMapping *mapping;
    JRB jnode1;
    JRB jnode2;
    JRB jnode3;

    /* Traverses the list of exnodes and refreshes the exnode */
    for(jnode2=jrb_first(unrefreshedExnodeList); jnode2 != unrefreshedExnodeList;)
    {
        jnode1 = jnode2;
        jnode2 = jrb_next(jnode2);

        /* Gets the current exnode */
        exnode = (LoDNExnode*)jnode1->val.v;

        /* Removes the exnode and the list */
        if(exnode->remove == 1)
        {
            freeLoDNExnode(exnode);
            jrb_delete_node(jnode1);
            continue;
        }

        logPrint(1, "Attempting to refresh mappings for %s exnode", exnode->name);

        /* Traverses the mappings in the exnode and assigns a refresh task to
         * each of the mappings */
        jrb_traverse(jnode3, exnode->mappingsByOffset)
        {
        	/* Gets the current mapping */
        	mapping = (LoDNExnodeMapping*)jnode3->val.v;

        	//printf("Mapping exnode name: %s\n", mapping->exnode->name);
        	//printf("Mapping status is %d\n", mapping->status);

        	/* Only refreshes non dead mappings */
        	if(mapping->status > 0)
        	{
	            if(pthread_mutex_lock(&exnode->lock) != 0)
	            {
	                logPrintError(0, __FILE__, __LINE__, 1, "mutex lock error");
	            }

            	exnode->numTasks++;

            	if(pthread_mutex_unlock(&exnode->lock) != 0)
            	{
                	logPrintError(0, __FILE__, __LINE__, 1, "mutex lock error");
            	}

            	logPrint(3, "Attempting to refresh mappings for %s", exnode->name);

            	/* Assigns a refresh task to the mapping */
            	LoDNThreadPoolAssignJob(control->threadPool, refresh_job, (void*)mapping);
        	}
        }

        /* Once all of the refreshing is assigned then the exnode is ready for
         * outgoing list  */
        jrb_insert_str(unwarmedExnodeList, exnode->name, new_jval_v(exnode));
        jrb_delete_node(jnode1);
    }
}



/****------------------------warmExnodesFromList()-------------------------****
 * Description: This function takes a list of exnodes to warm.  For each exnode
 *              it finds the warmer file that describes the augmentation pattern
 *              for the exnode.  Then it gets the individual segments of each
 *              exnode and checks the segments against the warmer.  If a site
 *              is found that has no segment present or a bad one, then it
 *              assigns a task to augment the segment to the exnode.
 * Input: control - pointer to the LoDNControl struct.
 *        warmersByName - JRB list of the serialized warmers by name.
 *        unwarmedExnodeList - JRB list of the exnode to augment.
 * Output: finishedExnodeList - JRB list of exnodes that have been warmed.
 ****----------------------------------------------------------------------****/
void warmExnodesFromList(LoDNControl *control, JRB warmersByName,
                         JRB unwarmedExnodeList, JRB finishedExnodeList)
{
    /* Declarations */
    LoDNExnode           *exnode           = NULL;
    char                 *warmerFilename   = NULL;
    LoDNSerializedWarmer *serializedWarmer = NULL;
    LoDNExnodeSegment    *segment          = NULL;
    Site                 *site             = NULL;
    LoDNExnodeMapping    *mapping          = NULL;
    char depotName[MAXHOSTNAMELEN+6];
    int  mappingOnSite;
    LoDNSegmentAugmentControl *augSegCtrl  = NULL;
    int  numGoodMappings;
    Dllist dlnode1;
    JRB jnode1;
    JRB jnode2;
    JRB jnode3;
    JRB jnode4;
    JRB queue     = NULL;
    JRB queueNode = NULL;
    JRB nextQueueNode = NULL;
    Dllist queueLevel = NULL;
    Dllist queueLevelNode = NULL;
    struct stat sb;


    /* Allocates a queue to hold the segments to be copied by their position
     * in the file */
    if((queue = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Warm this collection of exnodes */
    for(jnode2=jrb_first(unwarmedExnodeList); jnode2 != unwarmedExnodeList;)
    {
        jnode1 = jnode2;
        jnode2 = jrb_next(jnode2);

        /* Gets the pointer to the current exnode */
        exnode = (LoDNExnode*)jnode1->val.v;

        if(pthread_mutex_lock(&exnode->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex lock error");
        }

        /* If the number of tasks is greater than zero then it skips this
         * exnode */
        if(exnode->numTasks > 0)
        {
            if(pthread_mutex_unlock(&exnode->lock) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlock error");
            }

            continue;
        }

        if(pthread_mutex_unlock(&exnode->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlock error");
        }

        /* Removes the exnode and the list */
        if(exnode->remove == 1)
        {
            freeLoDNExnode(exnode);
            jrb_delete_node(jnode1);
            continue;
        }

        logPrint(1, "Attempting to augment %s", exnode->name);

        /* Find the site warmer file */
        warmerFilename = findWarmerFileByExnode(exnode->name, control);

        /* If no warmer file exists then it  clears the exnode from the list */
        if(warmerFilename == NULL)
        {
            logPrintError(0, __FILE__, __LINE__, 0, "Exnode file %s has no warmer file to use", exnode->name);
            jrb_insert_str(finishedExnodeList, exnode->name, new_jval_v(exnode));
            jrb_delete_node(jnode1);

            continue;
        }

        logPrint(2, "Warmer file for %s exnode is %s", exnode->name, warmerFilename);

        /* Gets the modification time warmer file */
        if(stat(warmerFilename, &sb) != 0)
        {
        	logPrintError(0, __FILE__, __LINE__, 0, "Error stating %s\n", warmerFilename);
        	jrb_insert_str(finishedExnodeList, exnode->name, new_jval_v(exnode));
        	jrb_delete_node(jnode1);

        	continue;
        }

        /* Gets a serialized warmer to use, it searches for one in memory
         * and if one it not found or its outdate then it creates one and puts
         * into the list */
        if((jnode3 = jrb_find_str(warmersByName, warmerFilename)) == NULL ||
           ((LoDNSerializedWarmer*)jnode3->val.v)->mtime < sb.st_mtime)
        {
        	logPrint(2, "%s is a new warmer file so it mush be serialized", warmerFilename);

            /* Makes the warmer */
            if((serializedWarmer = makeLoDNSerializedWarmer(warmerFilename)) == NULL)
            {
                logPrintError(0, __FILE__,__LINE__, 0, "Error creating serialized warmer for %s",
                              warmerFilename);
                jrb_insert_str(finishedExnodeList, exnode->name, new_jval_v(exnode));
                jrb_delete_node(jnode1);
                continue;
            }

            /* Reads the warmer */
            if(LoDNSerializedWarmerReadFile(serializedWarmer) != 0)
            {
                logPrintError(0, __FILE__,__LINE__, 0, "Error serializing %s\n", warmerFilename);
                jrb_insert_str(finishedExnodeList, exnode->name, new_jval_v(exnode));
                jrb_delete_node(jnode1);
                continue;
            }

            /* If there was a preexisting warmer by this name, then it stores the
             * older warmer for later removal.  (Can't remove yet because there
             * may be references by exnodes that are being warmered.  )  */
            if(jnode3 != NULL)
            {
            	if(jrb_insert_str(warmersByName, "", jnode3->val) == NULL)
            	{
            		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
            	}
            }

            /* Inserts into the tree of warmers by its name */
            if((jnode3 = jrb_insert_str(warmersByName, serializedWarmer->name, new_jval_v(serializedWarmer))) == NULL)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
            }
        }

        /* Gets a pointer to the warmer */
        serializedWarmer = (LoDNSerializedWarmer*)jnode3->val.v;


        /* Gets mapping segments and puts them into the exnode */
        logPrint(2, "Breaking %s's mappings into individual segments.", exnode->name);
        LoDNExnodeGetSegments(exnode);


        /* Assign warmer threads to each of the segments */
        logPrint(2, "Examining %s exnode to determine which segments need to be distributed.", exnode->name);
        jrb_traverse(jnode3, exnode->segments)
        {
            /* Gets a pointer to the current segment block */
            segment = (LoDNExnodeSegment*)jnode3->val.v;

            /* For each site check if the segment is represented */
            jrb_traverse(jnode4, serializedWarmer->sites)
            {
                /* Gets a pointer to the current site */
                site = (Site*)jnode4->val.v;

                mappingOnSite = 0;

                numGoodMappings = 0;

                /* Locks the exnode */
                if(pthread_mutex_lock(&exnode->lock) != 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex lock error");
                }

                /* Traverses the mappings for the segment and if the mapping
                 * is good then it increments the number of good mappings
                 * for the segment if matches a depot name in the site list */
                dll_traverse(dlnode1, segment->mappings)
                {
                    mapping = (LoDNExnodeMapping*)dlnode1->val.v;

                    /* Bad mapping, so skip it */
                    if(mapping->status < 0)
                    {
                        continue;
                    }

                    numGoodMappings++;

                    /* Form the depot name */
                    sprintf(depotName, "%s:%d", mapping->hostname, mapping->port);

                    /* If the name is one in the site list then increment
                     * the number of mappings for the current segment */
                    if(jrb_find_str(site->depots, depotName) != NULL)
                    {
                        mappingOnSite++;

                        mapping->inWarmer++;
                    }
                }

                /* Unlocks the exnode */
                if(pthread_mutex_unlock(&exnode->lock) != 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlock error");
                }

                /* If there is not a good mapping on the site then it
                 * attempts to make one */
                if(mappingOnSite < 1 && numGoodMappings > 0)
                {
                    logPrint(2, "%s needs lacks segment from %ld to %ld for %s\n",
                    		site->name, (long int)segment->start,
                    		(long int)segment->end, exnode->name);

                    /* Allocates an augmentation control struct */
                    if((augSegCtrl = malloc(sizeof(LoDNSegmentAugmentControl))) == NULL)
                    {
                        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                    }

                    /* Fills in the values for exnode, depots and segment */
                    augSegCtrl->exnode     = exnode;
                    augSegCtrl->destDepots = site->depots;
                    augSegCtrl->segment    = segment;

                    /* Finds the list to put the segment into or if the list
                     * doesn't exist creates and inserts the list */
                    if((queueNode = jrb_find_gen(queue,
                                    new_jval_v(segment->start), ll_cmp)) == NULL)
                    {
                        /* Creates the level for current segment position */
                        if((queueLevel = new_dllist()) == NULL)
                        {
                            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                        }

                        /* Inserts the level into the queue by its position in
                         * the file */
                        if((queueNode = jrb_insert_gen(queue, new_jval_v(segment->start),
                                       new_jval_v(queueLevel), ll_cmp)) == NULL)
                        {
                            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                        }

                    }

                    /* Gets the level's list */
                    queueLevel = (Dllist)queueNode->val.v;

                    /* Puts the segment into the queue list's for this position */
                    dll_append(queueLevel, new_jval_v(augSegCtrl));

                    logPrint(2, "Assigning augmentation task for exnode %s segment from %ld to %ld to site %s",
                              augSegCtrl->exnode->name, (long int)augSegCtrl->segment->start,
                              (long int)augSegCtrl->segment->end, site->name);
                }
            }
        }

        /* Runs through queue of segments for the file assigning augment threads
         * for each of the segments.  Does the length of the file first before
         * repeating */
        while(!jrb_empty(queue))
        {
            /* Traverses the levels */
            for(queueNode = jrb_first(queue); queueNode != queue; queueNode = nextQueueNode)
            {
                nextQueueNode = jrb_next(queueNode);

                /* Gets the first segment on the current list in the queue */
                queueLevel     = (Dllist)queueNode->val.v;
                queueLevelNode = dll_first(queueLevel);

                /* Gets the segment ctrl struct for the segment */
                augSegCtrl = (LoDNSegmentAugmentControl*)queueLevelNode->val.v;

                /* Locks the exnode */
                if(pthread_mutex_lock(&exnode->lock) != 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex lock error");
                }

                /* Updates the number of tasks on the exnode */
                exnode->numTasks++;

                /* Unlocks the exnode */
                if(pthread_mutex_unlock(&exnode->lock) != 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlock error");
                }

                /* Assigns the warmer task */
                LoDNThreadPoolAssignJob(control->threadPool, augment_job2, augSegCtrl);

                /* Removes the segment from the queue level list */
                dll_delete_node(queueLevelNode);

                /* When the queue level is empty of segments, free the level
                 * list and the node in the tree */
                if(dll_empty(queueLevel))
                {
                    free_dllist(queueLevel);
                    jrb_delete_node(queueNode);
                }
            }
        }

        /* Once all of the augmenting is assigned then the exnode is ready for
         * writing */
        jrb_insert_str(finishedExnodeList, exnode->name, new_jval_v(exnode));
        jrb_delete_node(jnode1);


        /* Frees the warmer file name */
        free(warmerFilename);
    }

    /* Frees the segment queue */
    jrb_free_tree(queue);
}



/****---------------------freeMappingsFromList()---------------------------****
 * Description:  This function takes a dllist of mappings to free.  It assigns
 *               a lodn thread job to free the mappings.
 * Input: control - pointer to the LoDNControl struct.
 *        freeMappings - pointer to the LoDNFreeMappings_t of IBP capabilities
 *        				to free.
 ****----------------------------------------------------------------------****/
void freeMappingsFromList(LoDNControl *control, LoDNFreeMappings_t *freeMappings)
{
	/* Assigns the warmer task */
	LoDNThreadPoolAssignJob(control->threadPool, free_job, freeMappings);
}



/****-----------------------writeExnodesFromList()-------------------------****
 * Description: This function takes a list of exnodes and for each exnode
 *              writes the exnode mappings and metadata to the a file.  It
 *              then frees the internal exnode representation from memory.
 * Input: control - pointer to the LoDNControl struct.
 *        finishedExnodeList - JRB list of the exnodes to be written out.
 *        free - int that holds option of whether to free the exnode or not.
 *               0 then no, > 0 then yes.
 *        freeMappings - pointer to the LoDNFreeMappings_t that holds mappings
 *        				to free.
 ****----------------------------------------------------------------------****/
void writeExnodesFromList(LoDNControl *control, JRB finishedExnodeList, int free,
							LoDNFreeMappings_t *freeMappings)
{
    /* Declarations */
    LoDNExnode *exnode;
    JRB jnode1;
    JRB jnode2;


    /* All finished exnodes get written out */
    for(jnode2=jrb_first(finishedExnodeList); jnode2 != finishedExnodeList;)
    {
        jnode1 = jnode2;
        jnode2 = jrb_next(jnode2);

        /* Gets a pointer to the current exnode */
        exnode = (LoDNExnode*)jnode1->val.v;

        if(pthread_mutex_lock(&exnode->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex lock error");
        }

        /* If there are still tasks under way on the exnode then it skips it
         * for later */
        if(exnode->numTasks > 0)
        {
            if(pthread_mutex_unlock(&exnode->lock) != 0)
            {
                logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlock error");
            }

            continue;
        }

        if(pthread_mutex_unlock(&exnode->lock) != 0)
        {
            logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex unlock error");
        }

        /* Removes the exnode and the list */
        if(exnode->remove == 1)
        {
            freeLoDNExnode(exnode);
            jrb_delete_node(jnode1);
            continue;
        }

        /* Calculates the status of the exnode */
        logPrint(1, "Calculating %s exnode status.", exnode->name);
        LoDNExnodeCalculateStatus(exnode);

        /* Writes the Exnode to its file */
        logPrint(1, "Writing %s exnode back to file.", exnode->name);
        LoDNExnodeWriteExnode(exnode, NULL, freeMappings);

        /* Frees the exnode */
        if(free)
        {
            freeLoDNExnode(exnode);
        }

        jrb_delete_node(jnode1);
    }
}


/****------------------------processExnodes()------------------------------****
 * Description: This function does the main processing and control of the
 *              warmer.  It builds a list of the exnodes to warm from either
 *              the specified paths or just the ones in the addExnodeList
 *              if warmAll is 0.  It pulls an exnode off of the exnode list,
 *              refreshes the mappings, does the required augmenting and
 *              writes the exnode to its file.  Since job threads are used for
 *              the individual taskes, several exnodes can be processed at the
 *              same time.  In each processing loop, it checks the message
 *              queue to see if any new requests have been posted.
 * Input: control - pointer to the LoDNControl struct.
 *        addExnodesList - list of the exnodes to be added and need warming
 *                         (from message queue)
 *        removeExnodeList - list of exnode to be removed (from message queue)
 *        warmAll - int that holds the option of warming all of the exnodes
 *                  int the paths or just the ones in the addExnodeList.
 ****----------------------------------------------------------------------****/
void processExnodes(LoDNControl *control, Dllist addExnodesList,
                    Dllist removeExnodesList, int warmAll)
{
    /* Delcarations */
    //LoDNExnodeCollection *exnodeCollection;
	LoDNExnodeIterator   *exnodeIterator;
    LoDNExnode           *exnode;
    JRB                   unrefreshedExnodeList;
    JRB                   unwarmedExnodeList;
    JRB                   finishedExnodeList;
    JRB                   removeExnodeList;
    JRB                   warmersByName;
    char                 *exnodeName;
    JRB jnode1;
    JRB exnodeNode;
    int numComms;
    int outOfNewExnodes;
    int exnodeReadStatus;
    LoDNFreeMappings_t *freeMappings;


    /* Creates a list to hold mappings to free */
	freeMappings = LoDNFreeMappingsAllocate();


    /* Allocates a tree to hold the serialized warmers by name */
    if((warmersByName = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* If warmall is specified then it builds a list of exnodes from the path
     * to warm otherwise it just warms the ones in the add list */
    if(warmAll > 0)
    {
    	logPrint(1, "Warmer is building the collection of exnodes.");

        /* All of the exnodes in the list */
        //exnodeCollection = buildExnodeCollection(control->paths);
    	exnodeIterator = buildExnodeIterator(control->paths);
    }else
    {
    	logPrint(1, "Warmer is building the collection of new exnodes.");

        /* Just the exnode in the exnode collection */
        //exnodeCollection = buildExnodeCollection(addExnodesList);
    	exnodeIterator = buildExnodeIterator(addExnodesList);

        /* Frees names the add exnode list */
        while(!dll_empty(addExnodesList))
        {
            free(dll_first(addExnodesList)->val.s);
            dll_delete_node(dll_first(addExnodesList));
        }

        /* Frees the names the remove exnode list */
        while(!dll_empty(removeExnodesList))
        {
            free(dll_first(removeExnodesList)->val.s);
            dll_delete_node(dll_first(removeExnodesList));
        }
    }

    /* Allocates three exnode lists for refreshing, warming and writing */
    unrefreshedExnodeList = make_jrb();
    unwarmedExnodeList    = make_jrb();
    finishedExnodeList    = make_jrb();
    removeExnodeList      = make_jrb();

    if(unrefreshedExnodeList == NULL || unwarmedExnodeList == NULL ||
       finishedExnodeList == NULL || removeExnodeList == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Traverses the list of exnodes and keeps running until all of the lists
     * are empty indicating everything is finished */
    while((exnode = LoDNExnodeIteratorNext(exnodeIterator)) != NULL ||
           !jrb_empty(unrefreshedExnodeList) || !jrb_empty(unwarmedExnodeList) ||
           !jrb_empty(finishedExnodeList))
    {
        outOfNewExnodes = 1;

        /* Puts the exnode onto the list of unfreshed exnodes */
        if(exnode != NULL)
        {
            /* If the exnode is in the list to be removed then its freed else
             * its put on the unrefreshed list */
            if((jnode1 = jrb_find_str(removeExnodeList, exnode->name)) != NULL)
            {
            	logPrint(1, "Removing Exnode %s from list to be warmed", exnode->name);
                freeLoDNExnode(exnode);
            }else
            {
                /* Read in the Exnode */
            	logPrint(1, "Reading in exnode %s", exnode->name);
                exnodeReadStatus = LoDNExnodeReadExnode(exnode);

                /* Skips dead exnodes or ill formed exnodes */
                if(exnodeReadStatus != 0 || strcmp(exnode->status, "DEAD") == 0)
                {
                	logPrint(0, "Exnode %s is either dead or ill formed", exnode->name);
                    freeLoDNExnode(exnode);

                /* Puts the exnode onto the unrefreshedExnode list  */
                }else
                {
                	logPrint(1, "Adding exnode %s to the list of exnodes to be refreshed", exnode->name);

                    if(jrb_insert_str(unrefreshedExnodeList, exnode->name,
                                        new_jval_v(exnode)) == NULL)
                    {
                        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                    }
                }
            }

            outOfNewExnodes = 0;
        }

        /* Refreshes the list of exnodes and puts it either on the
         * unwarmedExnodeList or the finishedExnodeList */
        refreshExnodesFromList(control, unrefreshedExnodeList,
               (control->refreshonly) ? finishedExnodeList : unwarmedExnodeList);

        /* Attempts to warm the list of exnodes */
        warmExnodesFromList(control, warmersByName, unwarmedExnodeList,
                            finishedExnodeList);

        /* Writes the exnode to their files and frees them */
        writeExnodesFromList(control, finishedExnodeList, 1, freeMappings);

        /* Frees mappings that are no longer needed */
        freeMappingsFromList(control, freeMappings);

        /* Check the message queue for new command */
        if(control->background > 0 && control->msgID >= 0)
        {
           logPrint(1, "Checking the message queue for new requests");

           /* Checks for any new exnode changes */
           numComms = getExnodeCommandsFromQueue(control->msgID, addExnodesList,
                                                removeExnodesList, 0);

           if(numComms > 0)
           {
                logPrint(1, "There are %d requests from the message queue", numComms);

                /* Adds the exnode to the list to be processed */
                while(!dll_empty(addExnodesList))
                {
                    /* Gets the name of the new exnode */
                    exnodeName = dll_first(addExnodesList)->val.s;

                    /* Finds the exnode if its already in one of the lists and
                     * marks it to be removed */
                    if((exnodeNode = jrb_find_str(unrefreshedExnodeList, exnodeName)) != NULL ||
                       (exnodeNode = jrb_find_str(unwarmedExnodeList, exnodeName)) != NULL ||
                       (exnodeNode = jrb_find_str(finishedExnodeList, exnodeName)) != NULL)
                    {
                        exnode = (LoDNExnode*)exnodeNode->val.v;

                        exnode->remove = 1;
                    }else
                    {
                        if(jrb_insert_str(removeExnodeList, strdup(exnodeName), JNULL) == NULL)
                        {
                            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                        }
                    }

                    logPrint(1, "Adding exnode %s to the list of exnodes", exnodeName);

                    /* Creates the exnode for the new list */
                    exnode = makeLoDNExnode(exnodeName);

                    /* Reads in the exnode */
                    LoDNExnodeReadExnode(exnode);

                    /* Inserts the exnode into the unrefreshed list */
                    jrb_insert_str(unrefreshedExnodeList, exnode->name, new_jval_v(exnode));

                    outOfNewExnodes = 0;

                    free(exnodeName);
                    dll_delete_node(dll_first(addExnodesList));
                }

                /* Removes the exnode */
                while(!dll_empty(removeExnodesList))
                {
                    /* Gets the name of the new exnode */
                    exnodeName = dll_first(addExnodesList)->val.s;

                    logPrint(1, "Removing exnode %s from the list of exnodes", exnodeName);

                    /* Finds the exnode if its already in one of the lists and
                     * marks it to be removed */
                    if((exnodeNode = jrb_find_str(unrefreshedExnodeList, exnodeName)) != NULL ||
                       (exnodeNode = jrb_find_str(unwarmedExnodeList, exnodeName)) != NULL ||
                       (exnodeNode = jrb_find_str(finishedExnodeList, exnodeName)) != NULL)
                    {
                        exnode = (LoDNExnode*)exnodeNode->val.v;

                        exnode->remove = 1;
                    }else
                    {
                        if(jrb_insert_str(removeExnodeList, strdup(exnodeName), JNULL) == NULL)
                        {
                            logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
                        }
                    }

                    free(dll_first(removeExnodesList)->val.s);
                    dll_delete_node(dll_first(removeExnodesList));
                }
           }
        }


        /* If there are no new exnodes then it waits for the current taskes to
         * end so that at the end of round of there isn't any spinning. */
        if(outOfNewExnodes == 1)
        {
        	logPrint(1, "No unwarmed exnodes, waiting on current exnodes to finish");

            LoDNThreadsPoolFinished(control->threadPool);
        }
    }

    /* Waits for the pool to be finished */
    LoDNThreadsPoolFinished(control->threadPool);

    /* Frees the exnode collection list */
    //freeLoDNExnodeCollection(exnodeCollection);
    freeLoDNExnodeIterator(exnodeIterator);

    /* Frees the exnodes on the remove list */
    while(!jrb_empty(removeExnodeList))
    {
        free(jrb_first(removeExnodeList)->key.s);
        jrb_delete_node(jrb_first(removeExnodeList));
    }

    /* Frees the trees */
    jrb_free_tree(unrefreshedExnodeList);
    jrb_free_tree(unwarmedExnodeList);
    jrb_free_tree(finishedExnodeList);
    jrb_free_tree(removeExnodeList);

	/* Frees the free mappings */
    LoDNFreeMappingsFree(freeMappings);

    /* Frees the serialized warmers */
    jrb_traverse(jnode1, warmersByName)
    {
        freeLoDNSerializedWarmer((LoDNSerializedWarmer*)jnode1->val.v);
    }

    /* Frees the jrb tree that held the warmers */
    jrb_free_tree(warmersByName);

    logPrint(1, "Warmer has completed run");
}



/****--------------------------runTimerLoop()------------------------------****
 * Description: This function runs the timer loop for the process.  For each
 *              loop it gets the current time and then runs the process exnode
 *              routine.  Then it sleeps until the next interval comes around
 *              or a message arrives on the message queue.  It then runs the
 *              processExnode routine again.  If no background option is given
 *              then it only runs once.
 * Input: control - pointer to the LoDNControl struct.
 ****----------------------------------------------------------------------****/
void runTimerLoop(LoDNControl *control)
{
    /* Declarations */
    time_t lastRunStartedAt = 0;
    time_t now = 0;
    int    time2Sleep = 0;
    int    numComms = 0;
    Dllist addExnodesList    = NULL;
    Dllist removeExnodesList = NULL;
    int    timeLeft = 0;
    int    restart=0;
    pid_t  pid;
    int    childStatus;
    Dllist node;


    /* Allocates the add and remove exnode lists for the message queue */
    if((addExnodesList    = new_dllist()) == NULL ||
       (removeExnodesList = new_dllist()) == NULL )
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }

    /* Has the warmer run when the duration has expired or only once if the
     * background option wasn't specified */
    do
    {
        if(control->background > 0)
        {
        	/* Gets the current time */
        	if(timeLeft <= 0)
        	{
        		if((lastRunStartedAt = time(NULL)) < 0)
        		{
        			logPrintError(0, __FILE__, __LINE__, 1,
        					"error acquiring current time");
        		}
        	}

        	do
        	{
	        	/* Forks one more time so the current process becomes the warmer process
	             * control and the child the actual warmer */
	            if((pid = fork()) < 0)
	            {
	            	logPrintError(0, __FILE__, __LINE__, 1,
	            			"Error forking for deamoninzing the process");
	            }

	            if(pid > 0)
	            {
	            	/* Waits on the child process to die */
	                if(waitpid(pid, &childStatus, 0) != pid)
	                {
	                	logPrintError(0, __FILE__, __LINE__, 1,
	                			"Error with waiting on child");
	                }

	                if(!WIFEXITED(childStatus) ||
	                		WEXITSTATUS(childStatus) != EXIT_SUCCESS)
	                {
	                	logPrintError(0, __FILE__, __LINE__, 0,
	                			"Warmer has died, restarting in 15 seconds");

                		/* Sleep 15 seconds before respawning to keep from
                		 * having a problem with constant death from the
                		 * warmer */
	                	sleep(15);

	                	restart = 1;

	                }else
	                {
	                	restart = 0;
	                }

	            }else
	            {
					/* Kill the message queue listening thread */
	            	/*if(LoDNMsgQueueKill(control->msgID) != 0)
	            	{
	            		logPrintError(0, __FILE__, __LINE__, 1,
	            				"error killing message queue listener");
	            	}*/


	            	/* Creates the thread pool to be used */
	            	control->threadPool = makeLoDNThreadPool(control->numThreads);

	            	/* Processes the exnodes */
	            	processExnodes(control, addExnodesList, removeExnodesList, (timeLeft>0) ? 0 : 1);

	            	/* Exits successfully */
	            	exit(EXIT_SUCCESS);
	            }

            }while(restart);


        	/* Puts the process to sleep until the next interval */
        	do
            {
                /* Gets the current time */
                if((now = time(NULL)) < 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "error acquiring current time");
                }

                /* Gets the remaining time to wait for */
                time2Sleep = control->background - (now-lastRunStartedAt);

                /* Sleeps until the time is up or a message arrives */
                if(time2Sleep > 0)
                {
                    logPrint(1, "Warmer took %d seconds to complete run and is sleeping for %d seconds\n", now-lastRunStartedAt, time2Sleep);

                    /* Waits for a message or until alarm is signaled */
                    if(control->msgID >= 0)
                    {
                    	while(!dll_empty(addExnodesList))
                    	{
                    		node = dll_first(addExnodesList);
                    		free(node->val.s);
                    		dll_delete_node(node);
                    	}

                    	while(!dll_empty(removeExnodesList))
            	        {
							node = dll_first(removeExnodesList);
							free(node->val.s);
							dll_delete_node(node);
						}

                        alarm(time2Sleep);
                        numComms = getExnodeCommandsFromQueue(control->msgID, addExnodesList,
                                               removeExnodesList, 1);
                        timeLeft=alarm(0);

                    /* Sleeps until time is up */
                    }else
                    {
                        timeLeft=sleep(time2Sleep);
                    }
                }

            /* Loops until the sleep time has expired or there is a new
             * command on the message queue */
            }while(timeLeft > 0 && numComms == 0);

            /* Print statement about waking up */
            logPrint(1, "Warmer has awoken and is starting run\n");


    	}else
    	{
    		/* Creates the thread pool to be used */
    		control->threadPool = makeLoDNThreadPool(control->numThreads);

    		/* Processes the exnodes */
    		processExnodes(control, addExnodesList, removeExnodesList, (timeLeft>0) ? 0 : 1);
    	}

    /* Loops if the warmer is being run as a depot process */
    }while(control->background > 0);
}



/****--------------------------finalize()----------------------------------****
 * Description: This function is used to release any resources like the
 *              message queue that aren't released by exit.
 * Input: options - pointer to the LoDNOptions struct for the warmer.
 *        control - pointer to the LoDNControl struct for the warmer.
 ****----------------------------------------------------------------------****/
void finalize(LoDNOptions *options, LoDNControl *control)
{

#if 0
	/* Attempts to delete the msg queue id. */
	if(control->msgID >= 0)
	{
		msgctl(control->msgID, IPC_RMID, NULL);
	}
#endif

	logPrint(0, "Warmer has finished.");
}



/***----Main----***/
int main(int argc, char *argv[])
{
    /* Declarations */
    LoDNOptions options;
    LoDNControl control;


    /* Prints a warning if a version of IBP less than 1.4.0 is used */
    #if !defined(IBPv040) || IBPv040 != 1
        fprintf(stderr, "Warning: This program may have problems if running ");
        fprintf(stderr, "with a version of IBP < 1.4.0\n");
    #endif

    /* Processes the command line arguments */
    processCommandLine(argc, argv, &options);

    /* Initializes the program */
    initialize(&options, &control);

    /* Performs the actual work of the exnode */
    runTimerLoop(&control);

    /* Finalizes the program and releases any resources still held. */
    finalize(&options, &control);

    /* Return Successfully to the parent process */
    return EXIT_SUCCESS;
}





