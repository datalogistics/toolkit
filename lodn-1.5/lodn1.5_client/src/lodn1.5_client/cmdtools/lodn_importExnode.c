
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "lodn.h"
#include "lors.h"


int main(int argc, char *argv[])
{
	/* Declarations */
	lodn_session_handle_t  handle;
	char				  *host;
	char				  *path;
	int					   port;
	JRB					   options;


	/* Checks the command line args */
	if(argc != 3)
	{
		fprintf(stderr, "Usage error: %s exnode-file lodn-url\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Gets the path */
	if(lodn_parse_url(argv[2], &host, &port, &path, &options) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Opens the session */
	if(lodn_openSession(argv[2], &handle, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Reads the exnode and attempts to import it into LoDN at argv[2] */
	if(lodn_importExnodeFile(path, argv[1], handle, 0) != 0)
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

	printf("Success\n");

	/* Return successfully */
	return EXIT_SUCCESS;
}
