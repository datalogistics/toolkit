#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lbone_client_lib.h"

int main(int argc, char **argv)
{
    int		retval	= 0;
    ulong_t	total	= 0;
    ulong_t	live	= 0;
    Depot	lb	= NULL;

    lb = (Depot) malloc(sizeof(struct ibp_depot));
    memset(lb, 0, sizeof(struct ibp_depot));
    strcpy(lb->host, argv[1]);
    lb->port = atoi(argv[2]);

    retval = lbone_depotCount(&total, &live, lb, 5);

    if (retval == 0)
    {
        fprintf(stderr, "lbone_Server returned an error\n");
        return 1;
    }

    fprintf(stdout, "The lbone_server lists %ld depots.\n", total);
    fprintf(stdout, "Of those, %ld depots responded in the last hour.\n", live);
    fflush(stdout);

    return 0;
}
