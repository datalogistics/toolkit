
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>

#include "lodn.h"



int main(int argc, char *argv[])
{
	/* Declarations */
	lodn_session_handle_t	handle;
	char				  	*host;
	char				  	*path;
	int					   	port;
	JRB					   	options;
	struct stat 			statbuf;
	char 					*typeStr;


	/* Checks the command line */
	if(argc != 2)
	{
		fprintf(stderr, "Usage error: %s lodn_url\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Gets the path */
	if(lodn_parse_url(argv[1], &host, &port, &path, &options) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Opens the session */
	if(lodn_openSession(argv[1], &handle, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Attempts to get the stat for the file at the specififed path */
	if(lodn_stat(path, &statbuf, handle, NULL, -1, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Closes the session */
	if(lodn_closeSession(handle) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Free the session */
	lodn_freeSession(handle);


	switch(statbuf.st_mode)
	{
		case S_IFDIR:
			typeStr = "Directory";
			break;

		case S_IFREG:
			typeStr = "Regular file";
			break;

		default:
			typeStr = "";
	}

	/* Prints the results */
	printf("File: \"%s\"\n", 		  	 argv[1]);
	printf("FileType: %s\n", 		  	 typeStr);
	printf("Size:  %-7ld Blocks %lld\n", statbuf.st_size, statbuf.st_blocks);
	printf("Inode: %-7ld Links: %d\n",   statbuf.st_ino, statbuf.st_nlink);
	printf("Access: %s", 				 ctime(&(statbuf.st_atime)));
	printf("Modify: %s",				 ctime(&(statbuf.st_mtime)));
	printf("Change: %s\n", 				 ctime(&(statbuf.st_ctime)));
	printf("Optimal Blocksize %d", 		 statbuf.st_blksize);
	//printf("Birth:  %s", 				ctime(&(statbuf.st_birthtimespec.tv_sec)));

	/* Return successfully */
	return EXIT_SUCCESS;
}
