#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fat.h"
#include "fs.h"

int main(int argc,char **argv)
{
	FAT *fat;
	char *device;
	int i;

	if(argc!=2) {
		printf("Usage: readfat <device>\n");
		exit(1);
	}
	
	device=strdup(argv[1]);

	fat=ibpfs_init(device);

	printf("FAT Parameters\n");
	printf("==============\n");
	printf("Device: %s\n",fat->device);
	printf("Maximum number of files: %d\n",fat->size);
	printf("Minimum allocation size: %d\n",fat->blksize);
	printf("Maximum allocation size: %lld\n",fat->maxsize);
	printf("\n");

	printf("FAT Records\n");
	printf("(Filename) (Offset) (Length)\n");
	printf("============================\n");

	for(i=0;i<fat->size;i++) {
		if(fat->records[i].filename[0]=='\0') {
			printf("null  %lld  %lld\n",fat->records[i].offset,
			                            fat->records[i].length);
		} else {
			printf("%s  %lld  %lld\n",fat->records[i].filename,
			                          fat->records[i].offset,
			                          fat->records[i].length);
		}
	}

	free(fat->records);
	free(fat);

	exit(0);
}
