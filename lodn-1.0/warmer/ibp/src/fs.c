/**
 * @file fs.c
 * IBP File System
 * Functions for creating and manipulating files in the IBP file system.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include "fs.h"
#include "fat.h"

#ifdef TIMING
#include <sys/time.h>
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/**
 * Initializes IBPFS.
 * @sa ibpfs_init
 */
FAT *ibpfs_init(const char *device)
{
	int fd;
	FAT *fat;

	fat=ibpfs_readFAT(device);
	if(fat==NULL) { 
		return(NULL);
	}

	fat->device=strdup(device);

	fd=open(device,O_RDWR|O_LARGEFILE);
	if(fd==-1) {
		free(fat->device);
		free(fat);
		return(NULL);
	}
	fat->fd=fd;

	return(fat);
}

/**
 * Creates a new file system.
 * @sa ibpfs_makefs
 */
int ibpfs_makefs(const char *device,unsigned int numalloc, u_int64_t maxsize,unsigned int minsize)
{
	int i,fd;
	FAT *fat;
    u_int64_t base_offset = 0;

    if ( minsize >= maxsize ){
        return (-1);
    };

	fat=(FAT *)malloc(sizeof(FAT));
	if(fat==NULL) {
		return(-1);
	}

	fat->size=numalloc;
	fat->blksize=minsize;
    fat->maxsize = maxsize;

	fat->records=(FATRecord *)malloc(numalloc*sizeof(FATRecord));
	if(fat->records==NULL) {
		free(fat);
		return(-1);
	}

    base_offset = ((8+sizeof(u_int64_t)+numalloc*RECORDLEN)/minsize +1)*minsize;
	for(i=0;i<numalloc;i++) {
		fat->records[i].filename[0]='\0';
		fat->records[i].offset=base_offset + maxsize*i;
		fat->records[i].length=0;
		fat->records[i].status=DIRTY;
	}

	fd=open(device,O_RDWR|O_LARGEFILE);
	if(fd==-1) {
		free(fat->records);
		free(fat);
		return(-1);
	}

	write(fd,&numalloc,4);
	write(fd,&minsize,4);
    write(fd,&maxsize,sizeof(maxsize));

    fat->fd = fd;
	ibpfs_writeFAT(fat,device);

    close(fd);
	return(0);
}

/**
 * Allocates a new file.
 * @sa ibpfs_allocate
 */
int ibpfs_allocate(FAT *fat,const char *filename,u_int64_t length)
{
	u_int64_t offset,temp;
	int i;

    if ( length > fat->maxsize){
        fprintf(stderr,"Error in allocate !\n");
        return -1;
    }

    pthread_mutex_lock(fat->lock);

    /*
	temp=fat->records[0].offset;
	offset=temp+fat->records[0].length;

	for(i=0;i<fat->size;i++) {
		if(fat->records[i].offset>temp) {
			temp=fat->records[i].offset;
			offset=((temp+fat->records[i].length)/fat->blksize+1)*blksize;
		}
	}
    */

	for(i=0;i<fat->size && fat->records[i].filename[0]!='\0';i++) { }
	if(i==fat->size) {
        pthread_mutex_unlock(fat->lock);
        fprintf(stderr,"Error in allocate !\n");
		return(-1);
	}

	strncpy(fat->records[i].filename,filename,PATH_MAX);
	/* fat->records[i].offset=offset; */
	fat->records[i].length=0;
    fat->records[i].reference = 0;
    fat->records[i].deleted = 0;
	fat->records[i].status=DIRTY;
    
    pthread_mutex_unlock(fat->lock);
	return(0);
}

/**
 * Deletes a file.
 * @sa ibpfs_delete
 */
int ibpfs_delete(FAT *fat,const char *filename)
{
	int i;

    pthread_mutex_lock(fat->lock);
	for(i=0;i<fat->size;i++) {
		if(strcmp(fat->records[i].filename,filename)==0) {
            fat->records[i].deleted = 1; 
            if ( fat->records[i].reference == 0 ){
			    fat->records[i].filename[0]='\0';
			    /* fat->records[i].offset=0; */
			    fat->records[i].length=0;
                fat->records[i].reference = 0;
            }
			fat->records[i].status=DIRTY;
            pthread_mutex_unlock(fat->lock);
			return(0);
		}
	}

    pthread_mutex_unlock(fat->lock);
	return(-1);
}

/**
 * Open a file.
 * @sa ibpfs_open
 */
IBPFILE *ibpfs_open(FAT *fat, const char *filename)
{
    IBPFILE *fp = NULL;
    int  fd;
    FATRecord *record;

	if ((fd=open(fat->device,O_RDWR|O_LARGEFILE)) < 0 ){
        return fp;
    };

    if ( (fp = (IBPFILE*)calloc(sizeof(struct ibpfile),1)) == NULL ){
        return fp;
    }

    pthread_mutex_lock(fat->lock);
    record = ibpfs_searchFAT(fat,filename);
    if ( record == NULL || record->deleted == 1 ){
        pthread_mutex_unlock(fat->lock);
        free(fp);
        return NULL;
    }
    record->reference++;
    fp->record = record ;
    fp->fd = fd;
    fp->fat = fat;

    pthread_mutex_unlock(fat->lock);
    return fp;
}


/**
 * Close a file
 */
int ibpfs_close(IBPFILE *fp)
{
    if ( fp != NULL ){
        close(fp->fd);
        pthread_mutex_lock(fp->fat->lock);
        fp->record->reference -- ;
        if ( fp->record->reference == 0 && fp->record->deleted == 1 ){
			    fp->record->filename[0]='\0';
			    /* fat->records[i].offset=0; */
			    fp->record->length=0;
                fp->record->reference = 0;
                fp->record->status = DIRTY; 
        }
        pthread_mutex_unlock(fp->fat->lock);
        free(fp);
    }
    return 0;
}

/**
 * Writes to a file.
 * @sa ibpfs_store
 */
int ibpfs_store(IBPFILE *fp,void *data,u_int64_t length,u_int64_t offset)
{

	u_int64_t bleft,bwritten;
	void *ptr;
	FATRecord *record;
#ifdef TIMING
	struct timeval start,end;
#endif
    if ( offset > fp->fat->maxsize ){
        return -1;
    }
    if ( offset + length > fp->fat->maxsize ){
        length = fp->fat->maxsize - offset;
    }

    record = fp->record;
	if ((off_t)(-1) == lseek(fp->fd,record->offset+offset,SEEK_SET)){
        perror("lseeking");
        exit(-1);
    };

	ptr=data;
	bleft=length;

#ifdef TIMING
	gettimeofday(&start,NULL);
#endif
	while(bleft>0) {
		bwritten=write(fp->fd,ptr,bleft);
		if(bwritten==-1) {
			if(errno==EINTR) {
				bwritten=0;
			} else {
				return(-1);
			}
		}

		bleft=bleft-bwritten;
		ptr=ptr+bwritten;
	}
#ifdef TIMING
	gettimeofday(&end,NULL);

	printf(" %ld %ld ",end.tv_sec-start.tv_sec,end.tv_usec-start.tv_usec);
#endif
    
    if ( length + offset > fp->record->length){
        fp->record->length = length + offset;
        fp->record->status = DIRTY;
    }

	return(length);
}

/**
 * Reads from a file.
 * @sa ibpfs_load
 */
int ibpfs_load(IBPFILE *fp,void *data,u_int64_t length,u_int64_t offset)
{
	u_int64_t bleft,bread;
	void *ptr;
#ifdef TIMING
        struct timeval start,end;
#endif


    if ( offset + length > fp->record->length  ){
        length = fp->record->length - offset;
    }

	lseek(fp->fd,fp->record->offset+offset,SEEK_SET);

	ptr=data;
	bleft=length;

#ifdef TIMING
        gettimeofday(&start,NULL);
#endif
	while(bleft>0) {
		bread=read(fp->fd,ptr,bleft);
		if(bread==-1) {
			if(errno==EINTR) {
				bread=0;
			} else {
				return(-1);
			}
		}

		bleft=bleft-bread;
		ptr=ptr+bread;
	}
#ifdef TIMING
        gettimeofday(&end,NULL);

        printf(" %ld %ld ",end.tv_sec-start.tv_sec,end.tv_usec-start.tv_usec);
#endif
    
	return(length);
}
