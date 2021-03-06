
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "lodn.h"


int main(int argc, char *argv[])
{
	/* Declarations */
	lodn_session_handle_t handle;


	/* Checks the command line args */
	if(argc != 2)
	{
		fprintf(stderr, "usage error: %s lodn-url\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Opens the session */
	if(lodn_openSession(argv[1], &handle, LODN_ENCRYPT) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	printf("In session\n");

	/* Closes the session */
	if(lodn_closeSession(handle) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Free the session */
	lodn_freeSession(handle);

	printf("Success\n");

	/* Exit successfully */
	return EXIT_SUCCESS;
}

