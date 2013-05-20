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


static void usage(char *program)
{
    printf("Usage %s:\n\t\tadd src_addr src_mask dst_addr dst_mask host:port [host:port ...]\n", program);
    printf("\t\tdelete index_of_route\n");
    printf("\t\tshow\n");
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[])
{
    /* Declarations */
    int *num_entries = 0;
    phoebus_route_t *routes = NULL;
    void    *addr = NULL;
    size_t   length;
    int   fd;
    int   index;
    char *src_addr;
    char *src_mask;
    char *dst_addr;
    char *dst_mask;
    char *gateways[MAX_PATH_ENTRIES];
    int   i;


    /* Check the number of args */
    if(argc < 2)
    {
        usage(argv[0]);
    }



    /* Open the file */
    if((fd = open(ROUTEFILE, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) < 0)
    {
        fprintf(stderr, "Can't open routing file\n");
        exit(EXIT_FAILURE);
    }

    /* Lock the file */
    acquire_route_file_lock(fd);


    /* Add an entry */
    if(strcmp(argv[1], "add") == 0)
    {
        if(argc < 7)
        {
            fprintf(stderr, "Not enough address specified\n");
            exit(EXIT_FAILURE);
        }

        /* Sets the values */
        src_addr = argv[2];
        src_mask = argv[3];
        dst_addr = argv[4];
        dst_mask = argv[5];

        /* Stores the gateways */
        for(i=0; i<MAX_PATH_ENTRIES && (6+i) < argc; i++)
        {
            gateways[i] = argv[6+i];
        }

        if(i == MAX_PATH_ENTRIES && (6+i) == argc)
        {
            fprintf(stderr, "Too many pheobus gateways specified in route\n");
            exit(EXIT_FAILURE);
        }

        if(i < MAX_PATH_ENTRIES)
        {
            gateways[i] = NULL;
        }

        add_route(fd, src_addr, src_mask, dst_addr, dst_mask, gateways);

    /* Remove an entry */
    }else if(strcmp(argv[1], "delete") == 0)
    {
        if(argc != 3)
        {
            fprintf(stderr, "Must specify an index to delete\n");
            exit(EXIT_FAILURE);
        }

        /* Checks and converts the index */
        if(sscanf(argv[2], "%d", &index) != 1 || index < 0)
        {
            fprintf(stderr, "index must be an integer >= 0\n");
            exit(EXIT_FAILURE);
        }

        delete_route(fd, index);

    /* Display the entries */
    }else if(strcmp(argv[1], "show") == 0)
    {
        show_routes(fd);

    }else if(strcmp(argv[1], "match") == 0)
    {
        /* Map the file into memory */
        if((length = map_route_file(&addr, fd, 0)) == (size_t)-1)
        {
            fprintf(stderr, "Error memory mapping phoebus routing file\n");
            exit(EXIT_FAILURE);
        }

        /* Gets pointers to the data structure */
        num_entries = (int*)addr;
        routes      = (phoebus_route_t*)(addr+sizeof(int));

        printf("%x\n", find_route(routes[2].src, routes[2].dst, routes, *num_entries));

    /* Unknown option */
    }else
    {
        usage(argv[0]);
    }


    /* Close the file */
    release_route_file_lock(fd);
    close(fd);

    /* Returns success */
    return EXIT_SUCCESS;
}
