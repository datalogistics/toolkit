#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/param.h>
#include "libexnode/exnode.h"
#include "LoDNAuxillaryFunctions.h"



/***---Constants---***/
#define LODN_ADD_EXNODE         0x01
#define LODN_REMOVE_EXNODE      0x02
#define LODN_DIR                "/var/tmp/lodnwarmer"
#define LODN_MSQ_FILE	        "/var/tmp/lodnwarmer/msqlog"
#define MSG_ID                  0x01
#define MAX_MSG_LEN             MAXPATHLEN + 16



/* Message Buffer */
typedef struct
{
    long mtype;
    char mtext[MAX_MSG_LEN];

} LoDNMsg;



/****-------------------------initializeExnode()---------------------------****
 * Description: This function reads the exnode and adds a filename and initial
 *              status for the exnode.
 * Input: exnodeFilename - cstring that holds the name of the exnode.
 * Output: It returns 0 on success and -1 on failure.
 ****----------------------------------------------------------------------****/
int initializeExnode(char *exnodeFilename)
{
    /* Declarations */
    struct stat statbuf;
    char *exnodeBuffer = NULL;
    int   fd           = -1;
    size_t   amtRead   = 0;
    size_t  amtWrote   = 0;
    ssize_t  count     = 0;
    Exnode  *exnode    = NULL;
    ExnodeMetadata    *exnodeMetadata = NULL;
    ExnodeValue        filenameValue;
    ExnodeType         type;
    ExnodeValue        statusValue;
    int                len;


    /* Checks the stat buf for the exnode file */
    if(stat(exnodeFilename, &statbuf) != 0 || !S_ISREG(statbuf.st_mode))
    {
        return -1;
    }

    /* Allocates a buffer to hold the exnode */
    if((exnodeBuffer = (char*)malloc(statbuf.st_size*sizeof(char))) == NULL)
    {
        fprintf(stderr, "Memory Allocation Error\n");
        exit(EXIT_FAILURE);
    }

    /* Opens the exnode file for reading */
    if((fd = open(exnodeFilename, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Error opening exnode file %s\n", exnodeFilename);
        goto ERROR_HANDLER;
    }

    /* Reads the exnode file into the buffer */
    while(amtRead < statbuf.st_size)
    {
        /* Read as much as possible at a time */
        if((count = read(fd, exnodeBuffer+amtRead, statbuf.st_size-amtRead)) < 0)
        {
            fprintf(stderr, "Error reading from exnode file %s\n", exnodeFilename);
            goto ERROR_HANDLER;
        }

        /* Update the amount read */
        amtRead += count;
    }

    /* Uses the buffer to create the exnode */
    if(exnodeDeserialize(exnodeBuffer, statbuf.st_size, &exnode) != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Error deserializing exnode file %s\n", exnodeFilename);
        goto ERROR_HANDLER;
    }

    /* Frees the resources used so far */
    free(exnodeBuffer);
    close(fd);

    /* Marks the resources as being freed */
    exnodeBuffer = NULL;
    fd           = -1;

    /* Gets the metadata values for the exnode */
    if(exnodeGetExnodeMetadata(exnode, &exnodeMetadata) != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Error reading exnode %s metadata\n", exnodeFilename);
        goto ERROR_HANDLER;
    }

    /* Gets the exnode's filename, if one is not present then it creates a
     * value from the name of the exnode */
    if(exnodeGetMetadataValue(exnodeMetadata, "filename", &filenameValue, &type)
        != EXNODE_SUCCESS || filenameValue.s == NULL || type != STRING)
    {
        /* Gets the name from the name of the exnode */
        if((filenameValue.s = strrchr(exnodeFilename, '/')+1) == NULL+1)
        {
            fprintf(stderr, "No filename for exnode %s\n", exnodeFilename);
            goto ERROR_HANDLER;
        }
    }

    /* Sets the filenname and status data */
    statusValue.s = "NEW";
    if(exnodeSetMetadataValue(exnodeMetadata, "filename", filenameValue, STRING, TRUE) != EXNODE_SUCCESS ||
       exnodeSetMetadataValue(exnodeMetadata, "status", statusValue, STRING, TRUE) != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Error setting the metadata filename of the %s exnode\n", exnodeFilename);
        goto ERROR_HANDLER;
    }

    /* Takes the exnode and fills in the buffer with a "serialized" copy */
    if(exnodeSerialize(exnode, &exnodeBuffer, &len) != 0)
    {
        fprintf(stderr, "Error serializing exnode file %s in %s on line %d\n",
                        exnodeFilename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER;
    }

    /* Opens the exnode file for writing removing any previous data */
    if((fd = open(exnodeFilename, O_WRONLY | O_TRUNC)) < 0)
    {
        fprintf(stderr, "Error opening exnode file %s in %s on line %d\n",
                     exnodeFilename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER;
    }

    /* Writes all of the serialized error into the buffer */
    while(amtWrote < len)
    {
        if((count = write(fd, exnodeBuffer+amtWrote, len-amtWrote)) < 0)
        {
            fprintf(stderr, "Error writing to exnode file %s in %s on line %d\n",
                        exnodeFilename, __FILE__, __LINE__-3);
            goto ERROR_HANDLER;
        }

        amtWrote += count;
    }

    /* Frees the resources */
    close(fd);
    free(exnodeBuffer);
    exnodeDestroyExnode(exnode);


    /* Return Successfully */
    return 0;


    /* Error Handler */
    ERROR_HANDLER:

        if(fd >= 0)
        {
            close(fd);
        }

        if(exnodeBuffer != NULL)
        {
            free(exnodeBuffer);
        }

        if(exnode != NULL)
        {
            exnodeDestroyExnode(exnode);
        }

        /* Returns unsucessfully */
        return -1;
}



/****----------------------processCommandline()----------------------------****
 * Description: This function checks the command line args.  It gets the
 *              command to use and the name of the exnode file to act on.  It
 *              checks that the exnode file exists.
 * Input: argc - int that holds the number of arguments.
 *        argv - array of cstrings that hold the command line arguments.
 * Output: command - array of chars to hold the name (pre-allocated).
 *         exnodeFilenames - array of cstrings that hold the exnode file names.
 *         It returns 0 on success other wise it causes the program to
 *         exit.
 ****----------------------------------------------------------------------****/
int processCommandline(int argc, char **argv, char *command,
                       char ***exnodeFilenames)
{
    /* Declarations */
	int i;


    /* Checks the number of command line args */
    if(argc < 3)
    {
        fprintf(stderr, "Usage Error: %s add|remove exnode-files...\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Checks the command */
    if(strcmp(argv[1], "add") != 0 && strcmp(argv[1], "remove") != 0)
    {
        fprintf(stderr, "Usage Error: %s is an invalid command\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    /* Assigns the command */
    strcpy(command, argv[1]);


    /* Creates an array of cstring pointers to hold the list of exnodes and
     * NULL. */
    if((*exnodeFilenames = (char**)malloc(sizeof(char*)*(argc-1))) == NULL)
    {
    	fprintf(stderr, "Memory allocation error\n");
    	exit(EXIT_FAILURE);
    }


    /* Builds a list of command line args */
    for(i=0; i<argc-2; i++)
    {
    	/* Creates a buffer for holding the exnode name */
    	if(((*exnodeFilenames)[i] = malloc(PATH_MAX*sizeof(char))) == NULL)
    	{
    		fprintf(stderr, "Memory allocation error\n");
    		exit(EXIT_FAILURE);
    	}

    	/* Resolves the path to the exnode file */
    	if(realpath(argv[i+2], (*exnodeFilenames)[i]) == NULL)
    	{
    		fprintf(stderr, "Usage Error: %s is not a valid exnode file\n", argv[i+2]);
    		exit(EXIT_FAILURE);
    	}
    }

    /* Null terminates the list */
    (*exnodeFilenames)[i] = NULL;

    /* Return Success */
    return 0;
}



/****----------------------getMessageQueueFilename()-----------------------****
 * Description:  This function uses the lodn warmer log file to determine the
 *               message queue file to use.  It reads through the file
 *               searching the warmer entries looking for an entry whoses path
 *               is compatible for the exnode.  It then checks if there is a
 *               warmer for the entry by determining if there is a file lock
 *               on corresponding message queue file.  It returns a heap
 *               allocated cstring copy of the message queue filename if there
 *               is a valid warmer to use or NULL if not.
 * Input: exnodePath - cstring that holds the full path of the exnode.
 * Output: It returns a heap allocated copy of the message queue file to use or
 *         NULL if one doesn't exist.
 ****----------------------------------------------------------------------****/
char *getMessageQueueFilename(char *exnodePath)
{
	/* Declarations */
	int  fd;
	int  mfd;
	int  amtRead;
	int  offset  = 0;
	char buffer[PATH_MAX*2+2];
	char msqfile[PATH_MAX];
	char warmerPath[PATH_MAX];
	char *start;
	char *sep;
	char *sep2;
	struct flock lock;
	int  warmerRunning = 0;
	int sfd;
	struct sockaddr_un server_addr;
	int sun_len;
	int i;


	/* Creates the process directory */
	mkdir(LODN_DIR, S_IRWXU|S_IRWXG|S_IRWXO);

	/* Opens the process control file for reading and writing */
	if((fd = open(LODN_MSQ_FILE, O_RDONLY|O_CREAT, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH)) < 0)
	{
		fprintf(stderr, "Couldn't open %s\n", LODN_MSQ_FILE);
		exit(EXIT_FAILURE);
	}

	lock.l_start  = 0;
	lock.l_len    = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type   = F_RDLCK;

	/* Attempts to lock the file and waits until it can have a write lock. */
	if(fcntl(fd,  F_SETLKW , &lock) == -1)
	{
		fprintf(stderr, "Couldn't lock %s", LODN_MSQ_FILE);
		exit(EXIT_FAILURE);
	}

	/* Reads the main process file into the buffer */
	while(((amtRead=read(fd, buffer+offset, sizeof(buffer)-offset)) > 0) &&
	      warmerRunning == 0)
	{
		/* Sets the about of data available */
		amtRead += offset;
		start    = buffer;

		while((sep  = memchr(start, '\n', amtRead))   != NULL &&
			  (sep2 = memchr(start, '\0', sep-start)) != NULL &&
			  warmerRunning == 0)
		{
			/* Gets the directory and message queue file */
			strncpy(warmerPath, start, sep-start);
			strncpy(msqfile, sep2+1, sep-sep2-1);
			msqfile[sep-sep2-1] = '\0';

			if(strncmp(warmerPath, exnodePath, strlen(warmerPath)) == 0)
			{
				warmerRunning  = 1;
			}

			/* Updates the amount of data available and the start of the fresh
			 * data */
			amtRead -= sep-start;
			start    = sep+1;

			/* Checks if the message queue file exists */
			if(warmerRunning > 0)
			{
				/* Checks the length of the filename for the unix doamin socket */
				if(strlen(msqfile) >= sizeof(server_addr.sun_path))
				{
					continue;
				}

				/* Creates a socket descriptor */
				if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
				{
					fprintf(stderr, "Error creating socket descriptor\n");
					exit(EXIT_FAILURE);
				}

				/* Setup the server adder for the unix domain socket */
				server_addr.sun_family = AF_UNIX;
				strncpy(server_addr.sun_path, msqfile, sizeof(server_addr.sun_path)-1);

				sun_len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family) + 1;

				#ifndef LINUX
					server_addr.sun_len = sun_len;
				#endif

				/* Attempts to connect to the warmer to see if its running */
				if(connect(sfd, (struct sockaddr*)&server_addr, sun_len) != 0)
				{
					warmerRunning = 0;
				}

				close(sfd);
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
		fprintf(stderr, "Error reading from %s\n", LODN_MSQ_FILE);
		exit(EXIT_FAILURE);
	}

	/* Closes the file descriptor */
	close(fd);


	/* If no running warmer can be found then it returns NULL */
	if(warmerRunning == 0)
	{
		return NULL;
	}

	/* Returns a heap allocated copy of the msqfile string */
	return strndup(msqfile, PATH_MAX);
}



/****----------------------sendCommandToWarmer()---------------------------****
 * Description: This function sends the command to the lodn warmer via
 *              Unix domain socket. It gets the Unix domain socket file,
 *              connects to the warmer and sends the message to the
 *              warmer.
 * Input: command - cstring that holds the command.
 *        exnodeFilename - cstring that holds the exnode file name.
 * Output: It returns 0 on success otherwise it causes the program to
 *         exit.
 ****----------------------------------------------------------------------****/
int sendCommandToWarmer(char *command, char *exnodeFilename)
{
   /* Declarations */
   char *socketFilename;
   int   msgSize;
   struct msghdr msg;
   struct iovec msgVectors[2];
   int sfd;
   struct sockaddr_un server_addr;
   int sun_len;


   /* Gets the message queue filename */
   if((socketFilename =  getMessageQueueFilename(exnodeFilename)) == NULL)
   {
	   fprintf(stderr, "Error: No running warmer\n");
	   exit(EXIT_FAILURE);
   }

   /* Checks the length of the socket file */
   if(strlen(socketFilename) >= sizeof(server_addr.sun_path))
   {
	   fprintf(stderr, "Error socketFilename exceeds maximum length of Unix Domain Socket");
	   exit(EXIT_FAILURE);
   }

   /* Opens a socket to the warmer */
   if((sfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
   {
       fprintf(stderr, "Error creating socket at %s:%d in %s\n", __FILE__, __LINE__, __FUNCTION__);
       exit(EXIT_FAILURE);
   }

   /* Sets the server addr to the warmer */
   server_addr.sun_family = AF_UNIX;
   strncpy(server_addr.sun_path, socketFilename, sizeof(server_addr.sun_path)-1);
   sun_len = strlen(server_addr.sun_path) + sizeof(server_addr.sun_family) + 1;

   #ifndef LINUX
   		server_addr.sun_len = sun_len;
   #endif

   /* Connects to the warmer */
   if(connect(sfd, (struct sockaddr *)&server_addr, sun_len) < 0)
   {
	   fprintf(stderr, "Error connection to server at %s:%d in %s\n", __FILE__, __LINE__, __FUNCTION__);
       exit(EXIT_FAILURE);
   }

   /* Initializes message struct */
   msg.msg_name 		= NULL;
   msg.msg_namelen 		= 0;
   msg.msg_control 		= NULL;
   msg.msg_controllen 	= 0;
   msg.msg_flags 		= 0;

   msg.msg_iov 			= msgVectors;
   msg.msg_iovlen 		= 2;

   /* Sets the command in the message */
   msg.msg_iov[0].iov_base = command;
   msg.msg_iov[0].iov_len  = (strlen(command)+1)*sizeof(char);

   /* Sets the name of the exnode */
   msg.msg_iov[1].iov_base  = exnodeFilename;
   msg.msg_iov[1].iov_len  = (strlen(exnodeFilename)+1)*sizeof(char);

   /* Sends the message to the server */
   if(sendmsg(sfd, &msg, 0) < 0)
   {
	   fprintf(stderr, "Error sending exnode data to warmer at %s:%d in %s\n", __FILE__, __LINE__, __FUNCTION__);
       exit(EXIT_FAILURE);
   }

   /* Closes the socket */
   close(sfd);

   /* Print Success */
   printf("Command to LoDN warmer successfully sent\n");

   /* Return Success */
   return 0;
}


/***---Main---***/
int main(int argc, char *argv[])
{
    /* Declarations */
    char **exnodeFilenames;
    char command[16];
    int  i;


    /* Process the command line and returns the program settings */
    processCommandline(argc, argv, command, &exnodeFilenames);



    /* Sends the command to the warmer via a message queue */
    for(i=0; exnodeFilenames[i] != NULL; i++)
    {
    	/* Initializes the exnode by adding the metadata for the exnode for newly
    	 * added exnodes */
    	if(strcmp(command, "remove") != 0)
    	{
    		if(initializeExnode(exnodeFilenames[i]) != 0)
    		{
    			fprintf(stderr, "Usage Error: %s is not a valid exnode file\n", argv[2]);
    		    exit(EXIT_FAILURE);
    		}
    	}

    	sendCommandToWarmer(command, exnodeFilenames[i]);
    }

    /* Return Successfully to the parent process */
    return EXIT_SUCCESS;
}
