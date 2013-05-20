#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "lodn.h"


int main(int argc, char *argv[])
{
	/* Declarations */
	lodn_session_handle_t  handle;
	char				  *host;
	char				  *path;
	int					   port;
	JRB					   options;


	/* Checks args */
	if(argc != 2)
	{
		fprintf(stderr, "Usage error: %s lodn-url\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	printf("HERE1\n");

	/* Gets the path */
	if(lodn_parse_url(argv[1], &host, &port, &path, &options) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	printf("Path is %s\n", path);

	/* Opens the session */
	printf("Opening session\n");
	if(lodn_openSession(argv[1], &handle, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Make the directory */
	printf("Calling mkdir %s\n", path);
	if(lodn_mkdir(path, handle, NULL, -1, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
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

	/* Exit successfully */
	return EXIT_SUCCESS;
}

