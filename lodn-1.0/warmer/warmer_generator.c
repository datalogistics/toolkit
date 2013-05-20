/*****************************************************************************
 * Name: Christopher Sellers
 * Date: 9/7/07
 * Program: warmerGenerator
 * 
 * Description: This program is used to query a lbone server for active 
 *              depots. It sorts the active depots into logical sites
 *              according to their hostname and prints the warmer file.
 * 				Use the --help option for options.
 *****************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <getopt.h>
#include <strings.h>
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

#define ulong_t         unsigned long int
#include "lbone_client_lib.h"



/***---Constants--***/
#define	DEFAULT_LBONE	"vertex.cs.utk.edu"
#define DEFAULT_PORT	6767
#define DEFAULT_MAX_DEPOTS		3000
#define DEFAULT_MIN_DURATION 1 //24*60*60
#define DEFAULT_MIN_STORAGE	1024 // 1 GiB
#define DEFAULT_NUM_GROUPS	3



/***--Prototypes--***/
void parseOptions(int argc, char **argv, struct ibp_depot *lbone_server, 
		int *ngroups, FILE **output, int *verbose);
JRB groupDepots(Depot *depots, int ngroups);



/***-----Main-----***/
int main(int argc, char *argv[])
{
	/* Declarations */
	JRB              groups;
	struct ibp_depot lbone_server;
	LBONE_request    request;
	Dllist           dnode;
	JRB				 jnode;
	Depot           *depots;
	Depot            depot;
	int			     ngroups;
	int              numDepots;
	int              verbose;
	FILE			*output;
	int              i;
	int              j;
	
	
	/* Parses the command line args */
	parseOptions(argc, argv, &lbone_server, &ngroups,&output, &verbose);
	
	/* Sets fields for the lbone request */
	request.numDepots    = ALL_DEPOTS;
	request.duration     = 1;
	request.stableSize   = 0;
	request.volatileSize = 1024;
	request.location     = NULL;
	
	
	/* Verbose message */
	if(verbose) { printf("\tQuerying %s:%d for a list of depots.\n", 
			lbone_server.host, lbone_server.port); }
	
	/* Queries the lbone for its depots */
	if((depots = lbone_getDepots(&lbone_server, request, 30)) == NULL)
	{
		fprintf(stderr, "Unable to get depots from lbone\n");
		exit(EXIT_FAILURE);
	}
	
	/* Counts the number of depots */
	for(numDepots=0; depots[numDepots]; numDepots++);
	
	/* Removes duplicates */
	for(i=0; i<numDepots; i++)
	{
		for(j=i+1; j<numDepots; j++)
		{
			if(strcmp(depots[i]->host, depots[j]->host) == 0 && 
					depots[i]->port == depots[j]->port)
			{
				free(depots[j]);
				depots[j]  = depots[numDepots-1];
				depots[numDepots-1] = NULL;
				numDepots--;
				j--;
			}
		}
	}
	
	/* Verbose message */
	if(verbose) { printf("\tChecking depots for status (may take some time).\n"); }
		
	/* Gets the free lbone list */
	depots = lbone_checkDepots(depots, request, 30);

	/* Prints the depots */
	if(verbose)
	{	
		printf("\tDepots:\n");
		for(i=0; depots[i] != NULL; i++)
		{
			printf("\t\t%s:%d\n", depots[i]->host, depots[i]->port);
		}
	}
	
	/* Verbose message */
	if(verbose) { printf("\tGrouping depots into logical sites.\n"); }
	
	/* Groups the depots into sites */
	groups = groupDepots(depots, ngroups);
	
	/* Verbose message */
	if(verbose) { printf("\tPrinting sites.\n"); }
	
	/* Printing sites */
	fprintf(output, "#Generated Sites from %s.\n\n", argv[0]);
	
	/* Prints the groups sites */
	jrb_traverse(jnode, groups)
	{
		fprintf(output, "%s:\n", (strlen(jnode->key.s) > 0) ? jnode->key.s : "site1");
		
		dll_traverse(dnode, ((Dllist)(jnode->val.v)))
		{
			depot = (Depot)dnode->val.v;
			fprintf(output, "\t%s:%d\n", depot->host, depot->port);
		}
	}
	
	/* Return to the parent process successfully */
	return EXIT_SUCCESS;
}



/****------------------------------parseOptions()--------------------------****
 * Description: This function parses the command line for the options. It
 *              returns the values filled in with either default values
 *              or user specified values.
 * Input: argc - int that holds the number of args.
 *        argv - array of cstrings that hold the command line args.
 * Output: lbone_server - ibp_depot pointer to the lbone server.
 *         ngroups      - int that holds the number of groups to try to make.
 *         output       - pointer to the file stream for the warmer file
 *         verbose      - int that holds the verbose option.
 ****----------------------------------------------------------------------****/
void parseOptions(int argc, char **argv, struct ibp_depot *lbone_server, 
		int *ngroups, FILE **output, int *verbose)
{
	/* Declarations */
	int   option;
	char *port;
	char *optstring="hl:g:o:v";
	struct option longopts[] = { { "lbone-server", required_argument, NULL, 'l'},
								 { "num-groups",   required_argument, NULL, 'g'},
								 { "verbose",      no_argument,       NULL, 'v'},
								 { "help",         no_argument,       NULL, 'h'},
								 { "output",       required_argument, NULL, 'o'},
								 { NULL,           0,                 NULL, 0 }
							   };
	
	/* Fills in the lbone server  */
	strncpy(lbone_server->host, DEFAULT_LBONE, sizeof(lbone_server->host));
	lbone_server->port = DEFAULT_PORT;
		
	*ngroups = DEFAULT_NUM_GROUPS;
	*verbose = 0;
	*output  = stdout;
	
	/* Parses the flags */
	while((option = getopt_long(argc, argv, optstring, longopts, NULL)) != -1)
	{
		switch(option)
		{
			/* Help */
			case 'h':
		
				printf("Usage: %s [options]\n", argv[0]);
				printf("\t-l,--lbone-server=hostname:port\t\tAddress of lbone server.\n");
				printf("\t-o,--output=filename\t\t\tFilename of the ouput warmer file.");
				printf("\t-n,--num-groups=int\t\t\tMininum of groups or sites to make.\n");
				printf("\t-v,--verbose\t\t\t\tBe verbose.\n");
				printf("\t-h,--help\t\t\t\tPrint this help message\n");
				printf("\n\n");
				
				exit(EXIT_SUCCESS);
			
				break;
			
			/* LBone Server */
			case 'l':
				
				/* Seperates the hostname and port of the LBone */
				if((port = strchr(optarg, ':')) == NULL)
				{
					fprintf(stderr, "Lbone-server is specified as hostname:port\n");
					exit(EXIT_FAILURE);
				}
				
				*port = '\0';
				port++;
			
				/* Stores and checks the hostname and port of the server */
				if(sscanf(optarg, "%s", lbone_server->host) != 1 ||
				   sscanf(port, "%d", &lbone_server->port) != 1)
				{
					fprintf(stderr, "Lbone-server is specified as hostname:port\n");
					exit(EXIT_FAILURE);
				}
				
			
				break;
			
			/* Number of groups */
			case 'g':
				
				/* Gets the mininum number of groups */
				if(sscanf(optarg, "%d", ngroups) != 1 || *ngroups < 1)
				{
					fprintf(stderr, "Number of groups must be an int >1.\n");
					exit(EXIT_FAILURE);
				}
				
				break;
				
			/* Output file */
			case 'o':
			
				if((*output = fopen(optarg, "w")) == NULL)
				{
					fprintf(stderr, "Not able to write to %s\n", optarg);
					exit(EXIT_FAILURE);
				}
			
				break;
				
			/* Verbose */
			case 'v':
			
				(*verbose)++;
				
				break;
				
			/* Unrecognized option */
			default:
				fprintf(stderr, "Unrecognized option %s\n", argv[optind]);
				exit(EXIT_FAILURE);
		}
	}
}



/****---------------------------groupsDepots()-----------------------------****
 * Description: This function groups the depots into logical sites based on
 *              their complete hostnames.  It returns the final groups.
 * Input: depots - pointer to the array of Depots.
 *        ngroups - int that holds the number of groups.
 * Output: It returns the groups as a JRB (key: sitename, val: Dllist of 
 *         Depots );
 ****----------------------------------------------------------------------****/
JRB groupDepots(Depot *depots, int ngroups)
{
	/* Declarations */
	JRB  groups;
	JRB  newgroups;
	JRB  jnode1;
	JRB jnode2;
	Dllist depotList;
	Dllist newdepotList;
	Dllist dnode;
	char *host;
	char *ptr;
	int i = 0;
	char *suffix = "";
	char *newSuffixname;
	int  suffix_len = 0;
	int  numGroups = 0;
	int  offset = 0;
	int  newSuffix;
	Depot depot;
	char *format;
	
	
	/* Allocates the groups */
	if((groups = make_jrb()) == NULL)
	{
		fprintf(stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}
	
	/* Allocates a list for the top level depot list */
	if((depotList = new_dllist()) == NULL)
	{
		fprintf(stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}
	
	/* Puts all of the input depots into the top level depot list */
	for(i=0; depots[i] != NULL; i++)
	{
		dll_append(depotList, new_jval_v(depots[i]));
	}
	
	/* Allocates the starting suffix */
	if((suffix = strdup("")) == NULL)
	{
		fprintf(stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}
	
	/* Inserts the depot list into empty domain */
	if(jrb_insert_str(groups, suffix, new_jval_v(depotList)) == NULL)
	{
		fprintf(stderr, "Memory Allocation Error\n");
		exit(EXIT_FAILURE);
	}

	/* Sets the number of groups and suffixes */
	numGroups = 1;
	newSuffix = 1;
	
	/* Continues while the number of groups is less the requested number of
	 * groups and there are still new divisions being made. */
	while(numGroups < ngroups && newSuffix > 0)
	{
		/* Resets the number of groups and suffixes */
		numGroups = 0;
		newSuffix = 0;
		
		/* Allocates a new groups container */
		if((newgroups = make_jrb()) == NULL)
		{
			fprintf(stderr, "Memory Allocation Error\n");
			exit(EXIT_FAILURE);
		}
		
		/* Traverses the old groups */
		jrb_traverse(jnode1, groups)
		{
			/* Gets the length of the current groups suffix */
			suffix_len = strlen(jnode1->key.s);

			/* Gets the list of depots for the current group */
			depotList  = (Dllist)jnode1->val.v;
			
			/* Traverses each depot in the current group */
			dll_traverse(dnode, depotList)
			{
				/* Gets the current depot */
				depot = (Depot)dnode->val.v;
				
				/* Allocates copy of the depot hostname */
				if((host = strdup(depot->host)) == NULL)
				{
					fprintf(stderr, "Memory Allocation Error\n");
					exit(EXIT_FAILURE);
				}
				
				//printf("\n\nSuffix:   %s\n", jnode1->key.s);
				//printf("host len: %d\n", (int)strlen(host)-suffix_len);
				
				/* Calculates the need for the . adjustment offset */
				offset = (suffix_len > 0) ? 1 : 0;
				
				/* Terminates the hostname at the current suffix */
				if(((int)strlen(host)-suffix_len-offset) < 0)
				{
					host[0] = '\0';
				}else
				{
					//printf("host index: %d\n", (int)strlen(host)-suffix_len-offset);
					host[strlen(host)-suffix_len-offset] = '\0';
				}
				
				//printf("host is %s\n", host);
				
				/* Searches for the . delimitor in the hostname and if can't
				 * find it uses that part of the hostname as an addition to the
				 *  hostname */
				if((ptr = strrchr(host, '.')) == NULL)
				{ 
					/* Allocates a copy of the suffix */
					if((suffix = strdup(jnode1->key.s)) == NULL)
					{
						fprintf(stderr, "Memory Allocation Error\n");
						exit(EXIT_FAILURE);
					}
					 
					//printf("here1: %s\n", suffix);
					
					/* If the length of the hostname minus the suffix is 
					 * greater than zero then uses the full hostname and
					 * combines it with the suffix */
					if((int)strlen(host) > 0)
					{
						//printf("new: %s\n", host);
						
						/* Allocates memory for holding the new suffix */
						if((suffix = (char*)malloc(suffix_len+strlen(host)+2)) == NULL)
						{
							fprintf(stderr, "Memory Allocation Error\n");
							exit(EXIT_FAILURE);
						}
						
						/* Sets the format of the suffix */
						format = (suffix_len > 0) ? "%s.%s" : "%s%s";
						
						/* Creates the new suffix */
						sprintf(suffix, format, host, jnode1->key.s);
										
						/* Increates the number of new suffixes */
						newSuffix++;
					}
					
				/* There is still a . delimiter so use the new part to increase
				 * the suffix */
				}else
				{
					//printf("new: %s, len: %d\n", ptr+1, strlen(ptr+1));
					
					/* Allocates memory for the new suffix */
					if((suffix = (char*)malloc(suffix_len+strlen(ptr+1)+2)) == NULL)
					{
						fprintf(stderr, "Memory Allocation Error\n");
						exit(EXIT_FAILURE);
					}
					
					/* Sets the format of the suffix */
					format = (suffix_len > 0 && strlen(ptr+1) > 0) ? "%s.%s" : "%s%s";
					
					/* Creates the new suffix */
					sprintf(suffix, format, ptr+1, jnode1->key.s);
					
					/* Increases the number of new suffixes */
					newSuffix++;
				}
				
				//printf("suffix is %s\n", suffix); 
				
				/* Inserts the depot into the new group based on the suffix and
				 * craetes the group and suffix for the first depot */
				if((jnode2 = jrb_find_str(newgroups, suffix)) == NULL)
				{
					/* Allocates a new depot list and suffix name */
					if((newdepotList  = new_dllist()) == NULL ||
					   (newSuffixname = strdup(suffix)) == NULL)
					{
						fprintf(stderr, "Memory Allocation Error\n");
						exit(EXIT_FAILURE);
					}
					
					/* Inserts the depot list into the newgroups by suffix */
					if((jnode2 = jrb_insert_str(newgroups, newSuffixname, 
							new_jval_v(newdepotList))) == NULL)
					{
						fprintf(stderr, "Memory Allocation Error\n");
						exit(EXIT_FAILURE);
					}
					
					/* Increases the number of new groups */
					numGroups++;
				}
				
				/* Gets the depot list for suffix */
				newdepotList = (Dllist)jnode2->val.v;
				
				/* Inserts the depot into the depot list for the suffix */
				dll_append(newdepotList, new_jval_v(depot));
				
				free(suffix);
				free(host);
			}
		}
		
		/* Frees the old groups and their depot lists */
		while(jrb_empty(groups))
		{
			free(jrb_first(groups)->key.s);
			free_dllist((Dllist)jrb_first(groups)->val.v);
		}
		
		jrb_free_tree(groups);
		
		/* The new groups become the current groups */
		groups = newgroups;	
	}
	
	/* Returns the groups */
	return groups;
}
