/*
 * lodn_xattr.c
 *
 *  Created on: Sep 13, 2010
 *      Author: brumgard
 */


#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/stat.h>

#include "lodn.h"


struct command
{
	unsigned char set    :1;
	unsigned char get    :1;
	unsigned char list   :1;
	unsigned char remove :1;

	char 		  *name;
	unsigned char *value;
};


int parse_commandline(int argc, char *argv[], struct command *command)
{
	/* Declarations */
	int ch;


	/* Clears the command struct */
	memset(command, 0, sizeof(struct command));

	/* Parses the options */
	while((ch = getopt(argc, argv, "hglrs")) != -1)
	{
		switch(ch)
		{
			/* Help message */
			case 'h':
				printf("usage: xattr -s attr_name attr_value lodn-url\n");
				exit(EXIT_SUCCESS);
				break;

			/* Get xattr */
			case 'g':
				command->get = 1;
				break;

			/* list xattr */
			case 'l':
				command->list = 1;
				break;

			/* remove xattr */
			case 'r':
				command->remove = 1;
				break;

			/* Set xattr */
			case 's':
				command->set = 1;
				break;

			/* Unkown option */
			default:
				fprintf(stderr, "usage error: unknown option %c\n", ch);
				exit(EXIT_FAILURE);
		}
	}

	/* Updates argc and argv */
	argc -= optind;
	argv += optind;

	/* Checks that one and only one of the commands were selected */
	if(command->set + command->get + command->list + command->remove != 1)
	{
		fprintf(stderr, "usage error: must select -s, -g, -l, -r\n");
		exit(EXIT_FAILURE);
	}

	/* Handles the set command */
	if(command->set)
	{
		if(argc != 3)
		{
			fprintf(stderr, "usage error: xattr -s xattr_name attr_value lodn-url\n");
			exit(EXIT_FAILURE);
		}

		command->name 	= argv[argc-3];
		command->value 	= argv[argc-2];

	/* Handles the get command */
	}else if(command->get)
	{
		if(argc != 2)
		{
			fprintf(stderr, "usage error: xattr -g xattr_name attr_value lodn-url\n");
			exit(EXIT_FAILURE);
		}

		command->name = argv[argc-2];

	/* Handles the remove command */
	}else if(command->remove)
	{
		if(argc != 2)
		{
			fprintf(stderr, "usage error: xattr -r xattr_name attr_value lodn-url\n");
			exit(EXIT_FAILURE);
		}

		command->name = argv[argc-2];

	/* Handles the list command */
	}else if(command->list)
	{
		if(argc != 1)
		{
			fprintf(stderr, "usage error: xattr -r xattr_name attr_value lodn-url\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Return success */
	return 0;
}


int main(int argc, char *argv[])
{
	/* Declarations */
	struct command 			command;
	lodn_session_handle_t	handle;
	char				  	*host;
	char				  	*path;
	int					   	port;
	JRB					   	options;
	void*					value;
	size_t					size = 0;
	size_t 					i;
	char 					*namebuf;
	size_t 					amt;


	/* Checks the command line */
	parse_commandline(argc, argv, &command);

	/* Gets the path */
	if(lodn_parse_url(argv[argc-1], &host, &port, &path, &options) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Opens the session */
	if(lodn_openSession(argv[argc-1], &handle, 0) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}


	/* Set xattr */
	if(command.set)
	{
		if(lodn_setxattr(path, command.name, command.value,
						 strlen(command.value)+1, 0, handle, NULL, -1,
						 LODN_XATTR_REPLACE) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

	/* Get xattr */
	}else if(command.get)
	{
		/* Gets the size of the getxattr */
		if(lodn_getxattr(path, command.name, NULL, &size, 0, handle, NULL, -1, 0) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

		printf("size is %d\n", size);
		/* Allocates a buffer for the xattr */
		if((value = malloc(size)) == NULL)
		{
			fprintf(stderr, "Memory error\n");
			exit(EXIT_FAILURE);
		}

		/* Gets the size of the getxattr */
		if(lodn_getxattr(path, command.name, value, &size, 0, handle, NULL, -1, 0) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

		/* Tests if value is a cstring */
		for(i=0; i<size; i++)
		{
			if(!isprint(((char*)value)[i]))
			{
				break;
			}
		}

		/* Its a cstring */
		if(((char*)value)[i] == '\0')
		{
			printf("%s\n", (char*)value);

		/* Print it has hexidecimal */
		}else
		{
			for(i=0; i<size; i++)
			{
				printf("%x\n", ((unsigned char*)value)[i]);
			}
		}

	/* Remove */
	}else if(command.remove)
	{
		if(lodn_removexattr(path, command.name, handle, NULL, -1, 0) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

	/* List */
	}else if(command.list)
	{
		/* Gets the size required */
		if(lodn_listxattr(path, NULL, &size, handle, NULL, -1, 0) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

		/* Allocates the namebuf */
		if((namebuf = malloc(size*sizeof(char))) == NULL)
		{
			fprintf(stderr, "Memory error\n");
			exit(EXIT_FAILURE);
		}

		/* Gets the list of names */
		if(lodn_listxattr(path, namebuf, &size, handle, NULL, -1, 0) != 0)
		{
			fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
			exit(EXIT_FAILURE);
		}

		for(amt=0; amt<size;)
		{
			amt += printf("%s\n", namebuf+amt);
		}
	}

	/* Closes the session */
	if(lodn_closeSession(handle) != 0)
	{
		fprintf(stderr, "%s\n", lodn_strerror(lodn_errno));
		exit(EXIT_FAILURE);
	}

	/* Free the session */
	lodn_freeSession(handle);


	/* Return successfully */
	return EXIT_SUCCESS;
}
