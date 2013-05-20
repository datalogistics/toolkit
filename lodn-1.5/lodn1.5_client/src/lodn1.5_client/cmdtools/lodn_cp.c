#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/param.h>

#include "jval.h"
#include "dllist.h"


/* Marco Constants */
#define LODN_URL			"lodn://"
#define LORS_URL			"lors://"
#define RECURSIVE_COPY		0x01
#define VERBOSE				0x02
#define SPAWN_VIEWER		0x04



/* Global for lors view output */
extern int g_lors_demo;

/* Global lodn_cp output */
FILE *lodn_cp_stderr;


void print_help(void)
{
	printf("\nThis is lodn_cp version 1.5\n");
	printf("\nUsage: lodn_cp [options] sources destination\n\n");
	printf("\t-h\t\tPrint this help message\n");
	printf("\t-r\t\tRecursively copy files (source files must be regular\n\t\t\tfiles)\n");
	printf("\t-s\t\tSpawn a new LoRS Viewer\n");
	printf("\t-V [[hostname:]port]\tConnect to LoRS Viewer at the specified hostname\n\t\t\tand port\n");
	printf("\t-v\t\tBe verbose\n\n");
	exit(EXIT_SUCCESS);
}


int processCommandLine(int argc, char *argv[], int *options, Dllist *sources,
		char **destination, char **viewer_hostname, int *viewer_port)
{
	/* Declarations */
	char resolved_path[PATH_MAX];
	char *file;
	char *sep;
	int   charr;

	/* Initializes the options */
	*options = 0x0;

	/* Traverses the options from the command line */
	while((charr = getopt(argc, argv, "rsvhV::")) != -1)
	{
		switch(charr)
		{
			/* Help */
			case 'h':

				/* Prints the help statment */
				print_help();

			/* Spawn a LoRS Viewer */
			case 's':

				/* Sets the option for spawning the viewer */
				*options |= SPAWN_VIEWER;

				break;

			/* Recursive Copy */
			case 'r':

				*options |= RECURSIVE_COPY;
				break;

			/* Verbosity */
			case 'v':

				*options |= VERBOSE;
				break;

			/* Visual output to LoRS View */
			case 'V':

				/* At least hostname was given */
				if(optarg != NULL)
				{
					/* Check if hostname and port were given */
					if((sep = strchr(optarg, ':')) != NULL)
					{
						/* Allocates a copy the hostname */
						if((*viewer_hostname = strdup(optarg)) == NULL)
						{
							fprintf(lodn_cp_stderr, "Memory Allocation Error\n");
							exit(EXIT_FAILURE);
						}

						/* Ends the hostname */
						(*viewer_hostname)[sep-optarg] = '\0';

						/* Increments sep beyond the ':' */
						sep++;

						/* Gets the port value */
						if(sscanf(sep, "%d", viewer_port) != 1 || *viewer_port < 1 ||
												*viewer_port > 65535)
						{
							fprintf(lodn_cp_stderr, "Invalid port specified for LoRS Viewer\n");
							exit(EXIT_FAILURE);
						}

					}else
					{
						/* Allocates a copy the hostname */
						if((*viewer_hostname = strdup(optarg)) == NULL)
						{
							fprintf(lodn_cp_stderr, "Memory Allocation Error\n");
							exit(EXIT_FAILURE);
						}

						*viewer_port = DEFAULT_PORT;
					}
				}


				/* Set the lors output global to being on */
				g_lors_demo = 1;

				break;

			/* Unknown character */
			default:

				fprintf(lodn_cp_stderr, "%c is an unknown option.\n", charr);
				exit(EXIT_FAILURE);
		}
	}

	/* Checks that there are at least 2 more arguments for source and
	 * destination */
	if(optind > argc-2)
	{
		fprintf(lodn_cp_stderr, "Source and destination files must be given\n");
		exit(EXIT_FAILURE);
	}

	/* Allocates a list to hold the sources */
	if((*sources = new_dllist()) == NULL)
	{
		fprintf(lodn_cp_stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}

	/* Adds the sources to the list */
	for(; optind<argc-1; optind++)
	{
		if(strncmp(argv[optind], LODN_URL, strlen(LODN_URL)) == 0 ||
		   strncmp(argv[optind], LORS_URL, strlen(LORS_URL)) == 0)
		{
			strcpy(resolved_path, argv[optind]);
		}
		else if(realpath(argv[optind], resolved_path) == NULL)
		{
			fprintf(lodn_cp_stderr, "Error resolving path %s\n", argv[optind]);
			exit(EXIT_FAILURE);
		}

		if((file = strdup(argv[optind])) == NULL)
		{
			fprintf(lodn_cp_stderr, "Memory Allocation Error\n");
			exit(EXIT_FAILURE);
		}

		dll_append(*sources, new_jval_s(file));
	}

	/* Copies and stores the destination directory */
	if((*destination = strdup(argv[optind])) == NULL)
	{
		fprintf(lodn_cp_stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}

	/* Return successfully */
	return 0;
}




int main(int argc, char *argv[])
{
	/* Declarations */
	int    options 	 	  = 0;
	Dllist sources		  = NULL;
	char  *destination    = NULL;
	char *viewer_hostname = NULL;
	int   viewer_port     = -1;


	/* Process the command lines */
	processCommandLine(argc, argv, &options, &sources, &destination,
						&viewer_hostname, &viewer_port);





	/* Return successfully to the parent process */
	return EXIT_SUCCESS;
}
