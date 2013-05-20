
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "lodn.h"


int main(int argc, char *argv[])
{
	/* Declarations */
	lodn_session_handle_t   handle = NULL;
	lodn_dirent_t 		  **dirents = NULL;
	struct stat 			statbuf;
	char *host;
	int   port;
	char *path;
	JRB   options;
	int   i;


	/* Checks the command line args */
	if(argc != 2)
	{
		fprintf(stderr,
				"Usage error: %s lodn://host[:port]/path[;parameters...]\n",
				argv[0]);
		exit(EXIT_FAILURE);
	}


	/* Parses the lodn url */
	if(lodn_parse_url(argv[1], &host, &port, &path, &options) != 0)
	{
		fprintf(stderr, "Error parsing %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	/* Opens the session */
	printf("Opening session\n");
	if(lodn_openSession(argv[1], &handle, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Attempts to get a listing of the contents of a directory */
	printf("Geting the contents of the directory\n");
	if(lodn_getdir(path, &dirents, handle, NULL, -1, 0) != 0)
	{
		fprintf(stderr,  "lodn_getdir(): %s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	char *fullpath = NULL;
	int   fullpathLen = 0;
	int   pathLen     = 0;
	int   j;

	for(j=strlen(path)-1; path[j] == '/'; j--) { path[j] = '\0'; }

	pathLen = strlen(path);


	/* Prints the contents of the directory */
	for(i=0; dirents[i] != NULL; i++)
	{
		printf("Entry is %s\n", dirents[i]->d_name);

		if((pathLen + strlen(dirents[i]->d_name) + 2) > fullpathLen)
		{
			fullpathLen = pathLen + strlen(dirents[i]->d_name) + 2;

			if((fullpath = realloc(fullpath, fullpathLen)) == NULL)
			{
				fprintf(stderr, "Memory error\n");
				exit(EXIT_FAILURE);
			}
		}

		snprintf(fullpath, fullpathLen, "%s/%s", path, dirents[i]->d_name);


		/* Attempts to get the stat for the file at the specififed path */
		if(lodn_stat(fullpath, &statbuf, handle, NULL, -1, 0) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

		printf("%10lld %s %s\n", statbuf.st_size,
				ctime(&(statbuf.st_mtime)), dirents[i]->d_name);
	}

	/* Closes the session */
	printf("Closing the session\n");
	if(lodn_closeSession(handle) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Free the session */
	printf("Freeing the session\n");
	lodn_freeSession(handle);

	printf("Success\n");


	/* Return successfully */
	return EXIT_SUCCESS;
}

