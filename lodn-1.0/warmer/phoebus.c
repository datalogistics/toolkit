#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>

#include "phoebus.h"


inline static int match_route(in_addr_t target, in_addr_t base, in_addr_t mask)
{
    return !((target & mask) ^ (base  & mask));
}

phoebus_route_t *find_route(in_addr_t src, in_addr_t dst,
                            phoebus_route_t *routes, int num_routes)
{
    /* Declarations */
    int i;


    /* Iterates over the routes looking for a matching route */
    for(i=0; i<num_routes; i++)
    {
        if(match_route(src, routes[i].src, routes[i].src_mask) &&
           match_route(dst, routes[i].dst, routes[i].dst_mask))
        {
            return routes+i;
        }
    }

    /* No route found */
    return NULL;
}


int acquire_route_file_lock(int routetable_fd)
{
    /* Gets a lock on the file */
    return flock(routetable_fd, LOCK_EX);
}



int release_route_file_lock(int routetable_fd)
{
    return flock(routetable_fd, LOCK_UN);
}

off_t map_route_file(void **addr, int routetable_fd, off_t extend_length)
{
	/* Declarations */
	static off_t filesize = 0;
	struct stat statbuf;


    /* Stats the route stuff */
	if(fstat(routetable_fd, &statbuf) != 0)
	{
		goto ERROR_HANDLER;
	}

    /* Check if the file needs to be remapped */
    if(statbuf.st_size != filesize || *addr == NULL)
    {
        /* Add extra space for the int */
        if(statbuf.st_size == 0)
        {
            extend_length += sizeof(int);
        }

        /* Unmaps the old addr */
        if(*addr != NULL)
        {
            munmap(addr, filesize);
        }


        if(extend_length > 0)
        {
            ftruncate(routetable_fd, statbuf.st_size+extend_length);
        }

        /* Memory map the route table */
        if((*addr = mmap(*addr, statbuf.st_size+extend_length,
                         PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED,
                         routetable_fd, 0)) == (void*)-1)
        {
            goto ERROR_HANDLER;
        }

        /* Initializes the new memory region */
        memset(*addr + statbuf.st_size, 0, extend_length);
    }

    /* Calculate the file size */
    filesize = statbuf.st_size + extend_length;

    /* Returns the filesize */
    return filesize;

	/* Error handler */
	ERROR_HANDLER:

		if(*addr != NULL)
		{
            munmap(*addr, filesize);
		}

        return -1;
}


void add_route(int fd, char *src_addr, char *src_mask, char *dst_addr,
               char *dst_mask, char *gateways[MAX_PATH_ENTRIES])
{
    /* Declarations */
    int *num_entries = 0;
    phoebus_route_t *routes = NULL;
    void    *addr = NULL;
    size_t   length;
    int      i;
    char    *host;
    char    *port;


    /* Map the file into memory */
    if((length = map_route_file(&addr, fd, sizeof(phoebus_route_t))) == (size_t)-1)
    {
        fprintf(stderr, "Error memory mapping phoebus routing file\n");
        exit(EXIT_FAILURE);
    }

    /* Gets pointers to the data structure */
    num_entries = (int*)addr;
    routes      = (phoebus_route_t*)(addr+sizeof(int));

    if(inet_pton(AF_INET, src_addr, &(routes[*num_entries].src)) != 1 ||
       inet_pton(AF_INET, src_mask, &(routes[*num_entries].src_mask)) != 1 ||
       inet_pton(AF_INET, dst_addr, &(routes[*num_entries].dst)) != 1 ||
       inet_pton(AF_INET, dst_mask, &(routes[*num_entries].dst_mask)) != 1)
    {
        fprintf(stderr, "Error converting network addresses\n");
        goto ERROR_HANDLER;
    }

    for(i=0; i<MAX_PATH_ENTRIES && gateways[i] != NULL; i++)
    {
        /* Sets the start of the host and port in the gateway entry */
        host = gateways[i];
        port = strchr(gateways[i], ':');

        /* No port */
        if(port == NULL)
        {
            fprintf(stderr, "No port given in gateway %s\n", gateways[i]);
            goto ERROR_HANDLER;
        }

        /* Separates the host and port */
        *port = '\0';
        port++;

        /* Converts the host of the gateway */
        if(inet_pton(AF_INET, host, &(routes[*num_entries].gateway[i].host)) != 1)
        {
            fprintf(stderr, "Invalid gateway host %s\n", host);
            goto ERROR_HANDLER;
        }

        /* Converts the port of the gateway */
        if(sscanf(port, "%hu", &routes[*num_entries].gateway[i].port) != 1)
        {
            fprintf(stderr, "Invalid port given %s\n", port);
            goto ERROR_HANDLER;
        }
    }

    /* Terminates the list of routes */
    for(; i<MAX_PATH_ENTRIES; i++)
    {
        routes[*num_entries].gateway[i].host = 0;
        routes[*num_entries].gateway[i].port = 0;
    }

    (*num_entries)++;

    /* Write results back to the file system */
    if(msync(addr, length, MS_SYNC) != 0)
    {
        fprintf(stderr, "Error updating routing file\n");
        (*num_entries)--;
        goto ERROR_HANDLER;
    }

    return;

    ERROR_HANDLER:

        ftruncate(fd, length-sizeof(phoebus_route_t));
        exit(EXIT_FAILURE);
}



void delete_route(int fd, int index)
{
    /* Declarations */
    int *num_entries = 0;
    phoebus_route_t *routes = NULL;
    void    *addr = NULL;
    size_t   length;
    int      i;


    /* Map the file into memory */
    if((length = map_route_file(&addr, fd, 0)) == (size_t)-1)
    {
        fprintf(stderr, "Error memory mapping phoebus routing file\n");
        exit(EXIT_FAILURE);
    }

    /* Gets pointers to the data structure */
    num_entries = (int*)addr;
    routes      = (phoebus_route_t*)(addr+sizeof(int));

    /* Checks the index */
    if(index < 0 || index >= *num_entries)
    {
        fprintf(stderr, "Invalid index for deletion specified\n");
        exit(EXIT_FAILURE);
    }

    /* Over writes the route table to get rid of the deleted index with
     * the remaining entries */
    memcpy(routes+index,
           routes+index+1,
           (*num_entries-index-1)*sizeof(phoebus_route_t));

    /* Decrement the number of entries */
    (*num_entries)--;

    /* Write results back to the file system */
    if(msync(addr, length-sizeof(phoebus_route_t), MS_SYNC) != 0)
    {
        fprintf(stderr, "Error updating routing file\n");
        exit(EXIT_FAILURE);
    }

    /* Shorten the file by the length of one entry */
    ftruncate(fd, length-sizeof(phoebus_route_t));
}



void show_routes(int fd)
{
    /* Declarations */
    int *num_entries = 0;
    phoebus_route_t *routes = NULL;
    void    *addr = NULL;
    size_t   length;
    int      i;
    int      j;


    /* Map the file into memory */
    if((length = map_route_file(&addr, fd, 0)) == (size_t)-1)
    {
        fprintf(stderr, "Error memory mapping phoebus routing file\n");
        exit(EXIT_FAILURE);
    }

    /* Gets pointers to the data structure */
    num_entries = (int*)addr;
    routes      = (phoebus_route_t*)(addr+sizeof(int));

    for(i=0; i<*num_entries; i++)
    {
        printf("%d: %s/", i, inet_ntoa(routes[i].src));
        printf("%s --> ", inet_ntoa(routes[i].src_mask));
        printf("%s/", inet_ntoa(routes[i].dst));
        printf("%s\n",inet_ntoa(routes[i].dst_mask));

        for(j=0; j<MAX_PATH_ENTRIES && routes[i].gateway[j].host != 0; j++)
        {
            printf("\t%s/%u\n", inet_ntoa(routes[i].gateway[j].host),
                                routes[i].gateway[j].port);
        }
    }
}

