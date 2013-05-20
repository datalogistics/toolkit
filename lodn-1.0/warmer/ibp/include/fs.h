/**
 * @file fs.h
 * IBP File System
 * Functions for creating and manipulating files in the IBP file system.
 */

#ifndef IBPFS_FS_H
#define IBPFS_FS_H

#include "fat.h"

typedef struct ibpfile {
    int fd;
    FAT *fat;
    FATRecord *record;
}IBPFILE;

/**
 * Initializes IBPFS.
 * Initializes IBPFS.
 * @param device The block special file where the file system resides.
 * @return Returns a pointer to the file allocation table for the file system.
 */
extern FAT *ibpfs_init(const char *device);

/**
 * Creates a new file system.
 * Creates a new IBPFS file system on a formatted disk.
 * @param device The block special file that points to the device on which
 *                to create the file system.
 * @param numalloc The maximum number of IBP allocations that can be made on
 *                 this file system.
 * @param minsize The minimum size for each allocation. Requests for allocations *                of a smaller size will be quietly scaled up to this value.
 * @pre The device where the file system will be created has been formatted.
 * @pre \em numalloc is positive.
 * @pre \em minsize is nonnegative.
 * @post An IBPFS file system with a maximum of \em numalloc allocations has
 *       been created on \em device.
 * @return Returns 0 on success.
 */
extern int ibpfs_makefs(const char *device,unsigned int numalloc,
                        u_int64_t maxsize, unsigned int minsize);

/**
 * Allocates a new file.
 * Allocates a new file.
 * @param fat The file system where the file should be created.
 * @param filename The name of the file.
 * @param length The length of the file.
 * @pre \em fat is a pointer to the file allocation table for a valid
 *      IBPFS file system.
 * @pre \em filename is non-NULL.
 * @pre \em length is non-negative.
 * @post A new entry for \em filename has been created in \em fat.
 * @return Returns 0 on success.
 */
extern int ibpfs_allocate(FAT *fat,const char *filename,u_int64_t length);

/**
 * Deletes a file.
 * Deletes a file.
 * @param fat The file system from which the file should be deleted.
 * @param filename The name of the file.
 * @pre \em filename is a valid IBPFS file.
 * @pre \em fat points to the file allocation table of a valid IBPFS file
 *      system.
 * @post \em filename has been removed from the file system.
 * @return Returns 0 on success.
 */
extern int ibpfs_delete(FAT *fat,const char *filename);
/**
 * Open a file.
 * Open a file.
 * @param fat The file system holding \em filename
 * @param filename The file to open.
 * @pre \em fat points to a valid FAT structure for an IBPFS file system.
 * @pre \em filename has been allocated by a call to \em ibpfs_allocate().
 * @return Returns pointer of file descriptor on success, otherwise return NULL.
 */
extern IBPFILE * ibpfs_open(FAT *fat, const char *filename);
/**
 * Close a file descriptor.
 * Close a file descriptor.
 * @param fp The pointer of file descriptor.
 * @return Returns 0 on success.
 */
extern int ibpfs_close(IBPFILE *fp);
/**
 * Writes to a file.
 * Writes data to a file.
 * @param fp The pointer to file descriptor returned by ibpfs_open.
 * @param data The data to write to \em filename.
 * @param length The length of \em data.
 * @param offset The offset to write the data.
 * @pre \em fp a pointer to file descriptor.
 * @pre \em data is non-NULL.
 * @pre \em length is greater than 0.
 * @pre \em offset is equal or greater than 0.
 * @post \em length bytes of \em data have been written to \em filename.
 * @return Returns 0 on success.
 */
/*
extern int ibpfs_store(FAT *fat,const char *filename,void *data,
                       u_int64_t length);
*/
extern int ibpfs_store(IBPFILE *fp,void *data,
                       u_int64_t length,u_int64_t offset);

/**
 * Reads from a file.
 * Reads data from a file.
 * @param fp A pointer to file descriptor returned by ibpfs_open.
 * @param data A buffer to store data from \em filename.
 * @param length The amount of data to read from \em filename.
 * @param offset The offset to read the data.
 * @pre \em data is non-NULL.
 * @pre \em length is greater than 0.
 * @post \em length bytes have been read from \em filename and stored in \em
 *       data.
 * @return Returns 0 on success.
 */
extern int ibpfs_load(IBPFILE *fp,void *data,
                      u_int64_t length,u_int64_t offset);

#endif
