#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exnode.h"
#include <lors_resolution.h>

int main(int argc,char **argv)
{
    LboneResolution     *lr;
	char *buf;
    int i,j;

    if ( argc < 2 ) 
    {
        fprintf(stderr, "lbone_test filename\n");
        exit(EXIT_FAILURE);
    }
    lorsCreateResolutionFromFile(argv[1], &lr, 0);
    for(i=0; i< lr->src_cnt; i++)
    {
        for (j=0; j< lr->dst_cnt; j++)
        {
            fprintf(stderr, "%2.2f ", lr->resolution[i][j]);
        }
        fprintf(stderr, "\n");
    }

    return 0;
}
