#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat.h"
#include "fs.h"

int main(int argc,char **argv)
{
	char *device;
	unsigned int size;
	unsigned int blksize;
    u_int64_t maxsize;

	if(argc!=5) {
		printf("Usage: makefs <device> <size> <blksize> <maxsize>\n");
		exit(1);
	}

	device=strdup(argv[1]);
	size=atoi(argv[2]);
	blksize=atoi(argv[3]);
    maxsize=atoi(argv[4]);

	if(ibpfs_makefs(device,size,maxsize,blksize)!=0) {
		printf("ERROR: Unable to make file system.\n");
	} else {
		printf("IBPFS file system successfully created on %s.\n",device);
		printf("Maximum number of files: %d\n",size);
		printf("Minimum allocation size: %d\n",blksize);
	}

	exit(0);
}
