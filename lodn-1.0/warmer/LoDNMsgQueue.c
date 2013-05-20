
#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#ifdef LINUX
#include <grp.h>
#else
#include <sys/ucred.h>
#endif

#include "jval.h"
#include "dllist.h"
#include "LoDNLogging.h"
#include "LoDNMsgQueue.h"



/***---Constants---***/
#define LODN_DIR     			"/var/tmp/lodnwarmer"
#define NEW_MSG_FILE 			"/var/tmp/lodnwarmer/new_msqlog"
#define MSG_FILE				"/var/tmp/lodnwarmer/msqlog"
#define BASE    				"/var/tmp/lodnwarmer/msq"
#define LINEEND 				"\n"
#define MAX_LISTEN_QUEUE		128
#define LODN_ADD_EXNODE         "add"
#define LODN_REMOVE_EXNODE      "remove"
#define LODN_GET_EXNODE_NOWAIT	"get_nowait"
#define LODN_GET_EXNODE_WAIT	"get_wait"
#define LODN_NONE				"none"
#define SOCKET_BUFFER_SIZE      2097152  /* 2 MiB */



/***---Structs---***/

/* Message Queue Struct for representing the internal message buffer
 * system */
typedef struct
{
	int sfd;					/* Unix Domain Socket descriptor */
	int spfd[2];				/* Socket pair for communicating between
	                             * the parent and child */
	//pid_t parentPid;			/* Pid of the parent */

	Dllist messages;			/* List of the messages */
	int numMesgs;				/* Number of messages */
	int queueSize;				/* Queue size */

	//pthread_t		listenThread; /* Thread id of the listening process */
	//pthread_mutex_t lock;		  /* Lock for the queue */
	//pthread_cond_t  signal;		  /* Signal for waiting on the queue to change */

} LoDNMsgQueue_t;

/* Message struct in the internal buffer */
typedef struct
{
	char *type;		/* Type of the message (add|remove) */
	char *exnode;	/* Path to the exnode */

} LoDNMesg_t;



/***---Globals---***/
static LoDNMsgQueue_t LoDNMsqQueue 		= { -1, {-1, -1}, NULL, 0, 0 };
static int   		  createdNewMsqFile = 0;
static char          *socketFile		= NULL;



/****------------------------------removeMsgFiles()------------------------****
 * Description: This function is run at exit to remove unnesscary resources
 *              from the message queue.
 ****----------------------------------------------------------------------****/
static void removeMsgFiles(void)
{
	/* Declarations */
	char msqfile[strlen(BASE)+32];

	/* Removes a the message queue file */
	if(createdNewMsqFile != 0)
	{
		unlink(NEW_MSG_FILE);
	}

	/* Builds the name of the possible message queue */
	sprintf(msqfile, "%s.%d", BASE, getpid());

	/* Removes the socket file */
	if(socketFile != NULL && strcmp(socketFile, msqfile)==0)
	{
		unlink(socketFile);
	}
}


/****-------------------_LoDNMsgQueueListenCleanup()-----------------------****
 * Description: This thread is responsible for free resources for the message
 *              queue.
 * Input: arg - void pointer to a LoDNMsgQueue_t struct.
 ****----------------------------------------------------------------------****/
static void _LoDNMsgQueueListenCleanup(void *arg)
{
	/* Declarations */
	LoDNMsgQueue_t *lodnMsgQueue;


	/* Casts the argument to a LoDNMsqQueue */
	lodnMsgQueue = (LoDNMsgQueue_t*)arg;

	/* Closes the socket file descriptor */
	if(lodnMsgQueue->sfd > -1)
	{
		close(lodnMsgQueue->sfd);
		lodnMsgQueue->sfd = -1;
	}

	return;
}



/****---------------------------LoDNMsgQueueListen()-----------------------****
 * Description: This function is run by a thread and listens on a Unix
 *              domain socket for commands.  It puts any valid command on
 *              the message list.
 ****----------------------------------------------------------------------****/
static int LoDNMsgQueueListen(void)
{
	/* Declarations */
	int cfd;
	char msg[PATH_MAX*2];
	struct sockaddr_un client_addr;
	socklen_t client_len;
	socklen_t peercred_len;
	uid_t uid;
	uid_t client_uid;
	int amt;
	char *exnodefilename;
	LoDNMesg_t *lodnMesg;
	int exnodeFilenameLength;
	int mesgSize;
	int closeSocket;
	fd_set readfds;
	int sendLen;
	LoDNMesg_t *mesg;
	Dllist node;
	unsigned char waitOnMesg = 0;
	unsigned char outstandingRequest = 0;



//Linux
#ifdef LINUX
	struct ucred peercred;

//BSD and Mac
#else
	struct xucred peercred;
#endif

	/* Gets the effective user id of the warmer to compare with the
	 * client messenger */
	uid = geteuid();

	/* Runs in a loop forever */
    while(1)
	{
		/* Sets the fds */
		FD_ZERO(&readfds);
		FD_SET(LoDNMsqQueue.sfd, &readfds);
		FD_SET(LoDNMsqQueue.spfd[0], &readfds);

		/* Waits on connection */
    	if(select(((LoDNMsqQueue.sfd > LoDNMsqQueue.spfd[0]) ?
    			LoDNMsqQueue.sfd : LoDNMsqQueue.spfd[0]) + 1, &readfds,
    			NULL, NULL, NULL) < 0)
    	{
    		logPrintError(0, __FILE__, __LINE__, 0, "Bad socket connection");
    		continue;
    	}

    	/* Message socket */
    	if(FD_ISSET(LoDNMsqQueue.sfd, &readfds))
    	{
			/* Gets the size of the client addr */
			client_len = sizeof(client_addr);

			/* Waits for client connection */
			cfd = accept(LoDNMsqQueue.sfd, (struct sockaddr *)&client_addr,
							&client_len);

			/* Checks connection */
			if (cfd < 0)
			{
				logPrintError(0, __FILE__, __LINE__, 0, "Bad socket connection");
				continue;
			}

			closeSocket = 1;

			// Linux
			#ifdef	LINUX

				/* Gets the credentials of the client */
				if(getsockopt(cfd, SOL_SOCKET, SO_PEERCRED, &peercred, &peercred_len) != 0 /*||
							peercred_len != sizeof(peercred) */)
				{
					logPrintError(0, __FILE__, __LINE__, 0,
							"Error getting socket credentials for connected socket");
					if(closeSocket) { close(cfd); }
					continue;
				}

				client_uid = peercred.uid;

			//BSD and Mac
			#else
				/* Gets the credentials of the client */
				if(getsockopt(cfd, LOCAL_PEERCRED, 1, &peercred, &peercred_len) != 0/* ||
						peercred_len != sizeof(peercred)*/)
				{
					logPrintError(0, __FILE__, __LINE__, 0,
								"Error getting socket credentials for connected socket");
					if(closeSocket) { close(cfd); }
					continue;
				}

				client_uid = peercred.cr_uid;

			#endif


			/* Gets the size of the peercred struct */
			peercred_len = sizeof(peercred);

			/* Compares the effective user ids of the client */
			if(uid != client_uid)
			{
				logPrintError(0, __FILE__, __LINE__, 0,
							  "Request from an unauthorized process");
				if(closeSocket) { close(cfd); }

				continue;
			}

			/* Gets the message of of the queue */
			if((amt = recv(cfd, msg, sizeof(msg), 0)) < 0)
			{
				logPrintError(0, __FILE__, __LINE__, 0, "Error receiving message");
				if(closeSocket) { close(cfd); }
				continue;
			}

			/* Good messages */
			if(amt > 0)
			{
				/* Outstanding Request so send to warmer */
				if(outstandingRequest)
				{
					/* Sends the message to the request queue */
					if(send(LoDNMsqQueue.spfd[0], msg, amt, 0)  != amt)
					{
						logPrintError(0, __FILE__, __LINE__, 1,
										"Error sending message to child");
					}

					outstandingRequest = 0;

				/* No outstanding request so store it */
				}else
				{
					/* Add command */
					if(strncmp(msg, LODN_ADD_EXNODE, amt) == 0 &&
								amt > strlen(LODN_ADD_EXNODE) + 1)
					{
						exnodefilename  = msg + strlen(LODN_ADD_EXNODE) + 1;
						amt            -= strlen(LODN_ADD_EXNODE) + 1;

					/* Remove Command */
					}else if(strncmp(msg, LODN_REMOVE_EXNODE, amt) == 0 &&
									amt > strlen(LODN_REMOVE_EXNODE) + 1)
					{
						exnodefilename = msg + strlen(LODN_REMOVE_EXNODE) + 1;
						amt           -= strlen(LODN_REMOVE_EXNODE) + 1;
					}

					/* Gets the length of the exnode filename string */
					exnodeFilenameLength = strlen(exnodefilename)+1;

					/* Checks the length of the exnode filename */
					if(exnodeFilenameLength > amt)
					{
						logPrintError(0, __FILE__, __LINE__, 0,
								"Message exnode filename exceeds limit length");
						if(closeSocket) { close(cfd); }
						continue;
					}

					/* Separates the  message type from the exnode filename */
					*(exnodefilename-1) = '\0';

					/* Allocates a LoDNMesg_t for holding the message */
					if((lodnMesg = malloc(sizeof(LoDNMesg_t))) == NULL)
					{
						logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Failure");
					}

					/* Sets up the lodn message */
					if((lodnMesg->type = strdup(msg)) == NULL ||
					   (lodnMesg->exnode = strdup(exnodefilename)) == NULL)
					{
						logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Failure");
					}

					/* Lock Queue */
					/*if(pthread_mutex_lock(&(LoDNMsqQueue.lock)) != 0)
					{
					   logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error");
					}*/

					/* Calculates the total message size */
					mesgSize = exnodeFilenameLength + strlen(lodnMesg->type) + 1 +
								sizeof(LoDNMesg_t);

					/* Checks that there is space left in the internal message queue */
					if(mesgSize + LoDNMsqQueue.queueSize <= SOCKET_BUFFER_SIZE)
					{
						/* Puts the message on the message queue */
						dll_append(LoDNMsqQueue.messages, new_jval_v(lodnMesg));

						/* Increments the number of messages and queue size */
						LoDNMsqQueue.numMesgs++;
						LoDNMsqQueue.queueSize += mesgSize;

						/* Signals that there is message on the message queue */
						/*if(pthread_cond_signal(&(LoDNMsqQueue.signal)) != 0)
						{
							logPrintError(0, __FILE__, __LINE__, 1, "pthread condition variable error");
						}*/

					/* Not enough room */
					}else
					{
						logPrintError(0, __FILE__, __LINE__, 0, "Not enough room left in message queue");

						/* Frees allocated resources */
						free(lodnMesg->type);
						free(lodnMesg->exnode);
						free(lodnMesg);
					}

					/* Unlock Queue */
					/* if(pthread_mutex_unlock(&(LoDNMsqQueue.lock)) != 0)
					{
					   logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error");
					}*/
				}

			}

			/* Closes the connected socket */
			if(closeSocket) { close(cfd); }


		/* Request from parent */
    	}else if(FD_ISSET(LoDNMsqQueue.spfd[0], &readfds))
	    {
    		msg[0] = '\0';

			/* Gets the message of of the queue */
			if((amt = recv(LoDNMsqQueue.spfd[0], msg, sizeof(msg), 0)) < 0)
			{
				logPrintError(0, __FILE__, __LINE__, 0, "Error receiving message");
				continue;
			}

			/* Bad message */
			if(amt == 0)
			{
				continue;

			/* no wait request */
			}else if(strncmp(msg, LODN_GET_EXNODE_NOWAIT,
					         strlen(LODN_GET_EXNODE_NOWAIT)) == 0)
			{
				waitOnMesg = 0;

			/* wait request */
			}else if(strncmp(msg, LODN_GET_EXNODE_WAIT,
									strlen(LODN_GET_EXNODE_WAIT)) == 0)
			{
				waitOnMesg = 1;

			/* Unknown command from child */
			}else
			{
				continue;
			}

			logPrint(0, "Request for message: %s\n", msg);

			/* Lock Queue */
			/*if(pthread_mutex_lock(&(LoDNMsqQueue.lock)) != 0)
			{
			   logPrintError(0, __FILE__, __LINE__, 1, "pthread mutex locking error");
			}*/

			/* Removes messages from the queue */
			if(!dll_empty(LoDNMsqQueue.messages))
			{
				node = dll_first(LoDNMsqQueue.messages);
				mesg = (LoDNMesg_t*)(node->val.v);

				/* Calculates the total message size */
				mesgSize = strlen(mesg->exnode) + 1 + strlen(mesg->type) + 1 +
											sizeof(LoDNMesg_t);

				/* Decrements the number of messages and queue size */
				LoDNMsqQueue.numMesgs--;
				LoDNMsqQueue.queueSize -= mesgSize;

				/* Formats the message to the child and gets the length of the
				 * message */
				int sendLen = snprintf(msg, sizeof(msg), "%s %s", mesg->type,
										mesg->exnode) + 1;

				/* Sends the message to the request queue */
				if(send(LoDNMsqQueue.spfd[0], msg, sendLen, 0)  != sendLen)
				{
					logPrintError(0, __FILE__, __LINE__, 1,
									"Error sending message to parent");
				}

				/* Frees the unneeded items */
				free(mesg->exnode);
				free(mesg->type);
				free(mesg);
				dll_delete_node(node);

			/* Send no messages to the child instead */
			}else
			{
				/* Don't wait */
				if(!waitOnMesg)
				{
					/* Sends the message to the child */
					if(send(LoDNMsqQueue.spfd[0], LODN_NONE,
							strlen(LODN_NONE)+1, 0) != strlen(LODN_NONE)+1)
					{
						logPrintError(0, __FILE__, __LINE__, 1,
										"Error sending message to child");
					}

				/* wait */
				}else
				{
					outstandingRequest = 1;
				}
			}
	    }
	}

    /* Returns from the function which should never happen */
	return 0;
}


//int LoDNMsgQueueKill(int queueID)
//{
//	/* Declarations */
//	int retval;
//
//	/* Checks that queue id is valid */
//	if(queueID != LoDNMsqQueue.sfd)
//	{
//		return -1;
//	}
//
//	logPrintError(0, __FILE__, __LINE__, 1, "this thread %d, suppose to be %d", pthread_self(), LoDNMsqQueue.listenThread);
//
//	/* Cancels the thread */
//	if((retval = pthread_cancel(LoDNMsqQueue.listenThread)) != 0)
//	{
//		logPrintError(0, __FILE__, __LINE__, 1, "Error canceling thread %d %d", retval, ESRCH);
//	}
//
//	/* Success */
//	return 0;
//}


/****------------------------LoDNMsgQueueInit()----------------------------****
 * Description: This function initializes the message queue system for the
 *              warmer. It creates the neccessary msq files and updates the
 *              entries within the main msq file if neccessary.  It creates
 *              a Unix Domain socket for communicating and an internal
 *              queue buffer for holding messages.
 * Input: paths - List of the all the paths the warmer is responsible for
 *                maintaining
 * Output: It returns the queue id.
 ****----------------------------------------------------------------------****/
int LoDNMsgQueueInit(Dllist paths)
{
	/* Declarations */
	int  fd  = 0;
	int  mfd = 0;
	int   nfd;
	int   sfd;
	mode_t mode;
	struct flock lock;
	int  amtRead;
	char buffer[1024];
	char msqfile[1024];
	char directory[1024];
	char *path;
	int  offset  = 0;
	char *start;
	char *sep;
	char *sep2;
	int  pathFound = 0;
	int  warmerRunning = 0;
	Dllist pnode;
	int path1Len;
	int path2Len;
	int cmpLen;
	int  remove;
	int rcvbuf_size = SOCKET_BUFFER_SIZE;
	struct sockaddr_un server_addr;
	int sun_len;
	char *lineEnd = LINEEND;
	struct iovec wvec[] = { { directory, 0 }, { msqfile, 0 },
			                    { lineEnd, 1*sizeof(char) }      };
	int filenameLen;
	pid_t pid;
	int i;


	/* Creates the process directory */
	mode = umask(0);
	mkdir(LODN_DIR, S_IRWXU|S_IRWXG|S_IRWXO|S_ISVTX);

	/* Registers the atexit routine for removing the new message file if
	 * neccessary */
	atexit(removeMsgFiles);

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

	/* Set to true that the new msq file was created by this process */
	createdNewMsqFile = 1;


	/* Attempts to lock the file and waits until it can have a write lock. */
	lock.l_start  = 0;
	lock.l_len    = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type   = F_WRLCK;

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

			/*** Checks if the socket file exists ***/

			/* Creates the unix domain socket  and gets a file descriptor for it */
			if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
			{
				logPrintError(0, __FILE__, __LINE__, 1, "Error creating socket");
			}

			/* Setup the server adder for the unix domain socket */
			server_addr.sun_family = AF_UNIX;
			strncpy(server_addr.sun_path, msqfile, sizeof(server_addr.sun_path)-1);

			sun_len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family) + 1;

			#ifndef LINUX
				server_addr.sun_len = sun_len;
			#endif


			/* Attempts to connect to the other process to see if the socket
			 * is still in use */
			for(i=0; i<3 && remove == 1; i++)
			{
				if(connect(sfd, (struct sockaddr *)&server_addr, sun_len) != 0)
				{
					remove = 1;
				}else
				{
					remove = 0;
				}

				sleep(1);
			}

			close(sfd);



			/* Removes the warmer entry and message queue file */
			if(remove == 1)
			{
				/* Deletes the previous socket file */
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
		filenameLen = snprintf(msqfile, sizeof(msqfile), "%s.%d", BASE, getpid());

		/* Checks the length of the Unix domain socket file */
		if(filenameLen >= sizeof(server_addr.sun_path))
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Unix domain socket file is too long");
		}

		/* Creates a copy of the msqfile */
		if((socketFile = strdup(msqfile)) == NULL)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
		}

		/* Creates the unix domain socket  and gets a file descriptor for it */
		if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Error creating socket");
		}

		/* Increases the buffer size for the receive window of the socket */
		if(setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &rcvbuf_size, sizeof(rcvbuf_size)) < 0)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Error setting socket buffer size");
		}

		/* Setup the server adder for the unix domain socket */
		server_addr.sun_family = AF_UNIX;
		strncpy(server_addr.sun_path, msqfile, sizeof(server_addr.sun_path)-1);

		sun_len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family) + 1;

#ifndef LINUX
		server_addr.sun_len = sun_len;
#endif

		/* Binds to the socket file specified */
		if(bind(sfd, (struct sockaddr *)&server_addr, sun_len) != 0)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Error binding unix domain server socket");
		}

		/* Sets the socket to listen for connections */
		if(listen(sfd, MAX_LISTEN_QUEUE) != 0)
		{
			logPrintError(0, __FILE__, __LINE__, 1,
						"Error converting socket to server socket");
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
	}


	/* Renames the file */
	if(rename(NEW_MSG_FILE, MSG_FILE) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Couldn't rename %s to %s",
				      NEW_MSG_FILE, MSG_FILE);
	}

	/* Set to true that the new msq file was created by this process */
	createdNewMsqFile = 0;


	/* Closes the file descriptor */
	close(nfd);
	close(fd);

	/* Restores the mask */
	umask(mode);

	/* Stores the socket file descriptor for the unix domain socket */
	LoDNMsqQueue.sfd = sfd;

	/* Creates a socket pair for parent/child IO */
	if(socketpair(AF_UNIX, SOCK_STREAM, 0, LoDNMsqQueue.spfd) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error creating socket pair");
	}

	/* Gets the pid of the current process */
	//LoDNMsqQueue.parentPid = getpid();

	/* Initialize the number of message and queue size */
	LoDNMsqQueue.numMesgs = 0;
	LoDNMsqQueue.queueSize = 0;


	/* Creates a child process to be the message queue system */
	if((pid = fork()) < 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error creating child");
	}

	/* Child process */
	if(pid == 0)
	{
		close(LoDNMsqQueue.spfd[1]);

		/* Allocates a dllist to hold the messages */
		if((LoDNMsqQueue.messages = new_dllist()) == NULL)
		{
			logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
		}

		/* Listen for messages and requests */
		LoDNMsgQueueListen();

	/* Parent process */
	}else
	{
		close(LoDNMsqQueue.spfd[0]);

		/* Closes the unix domain socket for the parent */
		close(sfd);
	}


	/* Returns the socket descriptor to use for the msg queue id */
	return sfd;
}




/****------------------------getExnodeCommandsFromQueue()------------------****
 * Description:  This function checks the message queue for any message and
 *               removes messages from the queue and puts them on one of the
 *               two lists depending on the type of message.  If wait is
 *               positive then the function blocks until it has a message.
 * Input: queueID - The id of the queue.
 *        wait  - Whether to block or not block for at least one message.
 * Output: addExnodeList - List that gets the new exnodes added to LoDN.
 *         removeExnodesList - List that gets the removed exnodes added to
 *                             LoDN.
 * It returns the number messages or -1 on error.
 ****----------------------------------------------------------------------****/
int getExnodeCommandsFromQueue(int queueID, Dllist addExnodesList,
                               Dllist removeExnodesList, int wait)
{
	/* Declarations */
	int numMsgs = 0;
	LoDNMesg_t *mesg;
	Dllist node;
	int mesgSize;
	char msg[PATH_MAX*2];
	char *exnodefilename;
	char *command;


	/* Checks that queue id is valid */
	if(queueID != LoDNMsqQueue.sfd)
	{
		return -1;
	}

	do
	{
		/* Wait on message */
		if(wait && numMsgs == 0)
		{
			command = LODN_GET_EXNODE_WAIT;
		}else
		{
			command = LODN_GET_EXNODE_NOWAIT;
		}


		/* Sends a request message to the child */
		if(send(LoDNMsqQueue.spfd[1], command,
			strlen(command)+1, 0) != (strlen(command)+1))
		{
			logPrintError(0, __FILE__, __LINE__, 1,
								"Error sending request to parent");
		}

		/* Initialize the message */
		msg[0] = '\0';

		/* Waits on the child for a result */
		if((mesgSize = recv(LoDNMsqQueue.spfd[1], msg, sizeof(msg), 0)) < 0)
		{
			logPrintError(0, __FILE__, __LINE__, 1,
								"Error receiving response from parent");
		}

		/* Add command */
		if(strncmp(msg, LODN_ADD_EXNODE, strlen(LODN_ADD_EXNODE)) == 0 &&
					mesgSize > strlen(LODN_ADD_EXNODE) + 1)
		{
			/* Makes a copy of the exnode name */
			if((exnodefilename = strndup((msg + strlen(LODN_ADD_EXNODE) + 1),
											sizeof(msg))) == NULL)
			{
				logPrintError(0, __FILE__, __LINE__, 1,
									"Memory allocation error");
			}

			/* Puts it on the list of added exnodes */
			dll_append(addExnodesList, new_jval_s(exnodefilename));

			numMsgs++;

		/* Remove Command */
		}else if(strncmp(msg, LODN_REMOVE_EXNODE, strlen(LODN_REMOVE_EXNODE)) == 0 &&
					mesgSize > strlen(LODN_REMOVE_EXNODE) + 1)
		{
			/* Makes a copy of the exnode name */
			if((exnodefilename = strndup((msg + strlen(LODN_REMOVE_EXNODE) + 1),
											sizeof(msg))) == NULL)
			{
				logPrintError(0, __FILE__, __LINE__, 1,
									"Memory allocation error");
			}

			/* Puts it on the list of removed exnodes */
			dll_append(removeExnodesList, new_jval_s(exnodefilename));

			numMsgs++;
		}

	}while(wait && numMsgs == 0);


	/* Returns the number of messages */
	return numMsgs;
}





#if 0

/* Test code */
int main(void)
{
	/* Declarations */
	Dllist paths;
	Dllist addExnodesList;
	Dllist removeExnodesList;
	Dllist node;

	logInit(NULL, 0);

	paths = new_dllist();
	dll_append(paths, new_jval_s(strdup("/home/sellers/")));

	addExnodesList = new_dllist();
	removeExnodesList = new_dllist();

	LoDNMsgQueueInit(paths);

	while(1)
	{
		getExnodeCommandsFromQueue(0, addExnodesList, removeExnodesList, 1);

		while(!dll_empty(addExnodesList))
		{
			node = dll_first(addExnodesList);
			printf("Add %s\n", node->val.s);
			dll_delete_node(node);
		}

		while(!dll_empty(removeExnodesList))
		{
			node = dll_first(removeExnodesList);
			printf("Remove %s\n", node->val.s);
			dll_delete_node(node);
		}
	}


	return EXIT_SUCCESS;
}

#endif
