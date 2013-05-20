/**
 * @file fat.c
 * IBPFS File Allocation Table.
 * Structures and functions for describing and manipulating the IBPFS
 * file allocation table.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "fat.h"

/**
 * Reads the file allocation table from disk.
 * @sa ibpfs_readFAT()
 */
FAT *ibpfs_readFAT(const char *device)
{
	int fd,i;
	int size,blksize;
    u_int64_t maxsize;
	FAT *fat;

	fd=open(device,O_RDONLY);
	if(fd==-1) {
		return(NULL);
	}

	fat=(FAT *)malloc(sizeof(*fat));
	if(fat==NULL) { return(NULL); }

	read(fd,&size,4);
	read(fd,&blksize,4);
    read(fd,&maxsize,sizeof(maxsize));
    if ( (fat->lock = (pthread_mutex_t *)calloc(sizeof(pthread_mutex_t),1)) == NULL ){
        free(fat);
        return(NULL);
    }
    pthread_mutex_init(fat->lock,NULL);
	fat->size=size;
	fat->blksize=blksize;
    fat->maxsize = maxsize;
	fat->records=(FATRecord *)malloc(size*sizeof(FATRecord));

	if(fat->records==NULL) {
        free(fat->lock);
		free(fat);
		return(NULL);
	}

	for(i=0;i<size;i++) {
		read(fd,(fat->records[i].filename),PATH_MAX*sizeof(char));
		read(fd,&(fat->records[i].offset),8);
		read(fd,&(fat->records[i].length),8);
        fat->records[i].reference = 0;
        fat->records[i].deleted = 0;
		fat->records[i].status=CLEAN;
	}

	close(fd);

	return(fat);
}

/**
 * Writes the file allocation table to disk.
 * @sa ibpfs_writeFAT()
 */
int ibpfs_writeFAT(FAT *fat,const char *device)
{
	int i;
    int ret;

    pthread_mutex_lock(fat->lock);
	lseek(fat->fd,8+sizeof(u_int64_t),SEEK_SET);

	for(i=0;i<fat->size;i++) {
		if(fat->records[i].status==DIRTY) {
			ret = write(fat->fd,(fat->records[i].filename),PATH_MAX*sizeof(char));
			ret = write(fat->fd,&(fat->records[i].offset),8);
			ret =write(fat->fd,&(fat->records[i].length),8);
            /*fprintf(stderr,"write fat with length =
            %lld\n",fat->records[i].length);*/
			fat->records[i].status=CLEAN;
		} else {
			lseek(fat->fd,RECORDLEN,SEEK_CUR);
		}
	}
    pthread_mutex_unlock(fat->lock);

	return(0);
}

/**
 * Searches the FAT for a file.
 * @sa ibpfs_searchFAT()
 */
FATRecord *ibpfs_searchFAT(FAT *fat,const char *key)
{
	int i;

	for(i=0;i<fat->size;i++) {
		if(strcmp(fat->records[i].filename,key)==0) {
			return(&fat->records[i]);
		}
	}

	return(NULL);
}
