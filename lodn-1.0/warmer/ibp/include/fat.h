/**
 * @file fat.h
 * IBPFS File Allocation Table.
 * Structures and functions for describing and manipulating the IBPFS
 * file allocation table.
 */

#ifndef IBPFS_FAT_H
#define IBPFS_FAT_H

#include <limits.h>
#include <sys/types.h>
#include <pthread.h>
#ifdef HAVE_CONFIG_H
# include "config-ibp.h"
#endif
#define RECORDLEN (PATH_MAX+16)

// change by Yong Zheng.To let the user data allign to the multiple of
// blocksize 
#define BASE(fat) ((((8+fat->size*RECORDLEN)/fat->blksize+1))*fat->blksize)

/**
 * Status flag for FAT records.
 * FAT records are either clean (unmodified) or dirty (modified).
 * Records that are dirty eventually need to be written to disk.
 */
typedef enum {CLEAN, DIRTY} FATStatus;

/**
 * FAT record.
 * Each record represents an entry in the file allocation table,
 * and contains metadata for a particular file.
 */
typedef struct fat_record {
	char filename[PATH_MAX];    /**< . Filename. */
	u_int64_t offset;           /**< . File offset on disk, in bytes. */
	u_int64_t length;           /**< . Length of file, in bytes. */
	FATStatus status;           /**< . Status of record (clean|dirty). */
    int       deleted;          /**< . flag of if the file should be deleted*/
    int       reference;        /**< . reference of the file */
} FATRecord;

/**
 * File Allocation Table (FAT).
 * The FAT is an array of FATRecord objects. The size of the FAT is specified
 * when the file system is created, and is immutable. 
 */
typedef struct fat {
	int size;                   /**< . Size of the file allocation table. */
	int blksize;                /**< . Size of an allocation block. */
    u_int64_t maxsize;          /**< . Maxmium size of file */
	char *device;               /**< . Device where the file system resides. */
	FATRecord *records;         /**< . Array of file records. */
	int fd;                     /**< . File descriptor for accessing device.*/
    pthread_mutex_t             *lock;
} FAT;

/**
 * Reads the file allocation table from disk.
 * Reads the file allocation table from disk into memory.
 * @param device The device where the IBP file system is located.
 * @return fat A pointer to an array of FATRecords containing the entire
 *             file allocation table.
 */
extern FAT *ibpfs_readFAT(const char *device);

/**
 * Writes the file allocation table to disk.
 * Writes the file allocation table from memory to disk. Only dirty records
 * are written.
 * @param device The device where the IBP file system is located.
 * @param fat The file allocation table as returned from \em ibpfs_readFAT().
 */
extern int ibpfs_writeFAT(FAT *fat,const char *device);

/**
 * Sorts the file allocation table.
 * Sorts the file allocation table in ascending order based on the file name.
 * @param fat The file allocation table as returned from \em ibpfs_readFAT().
 */
extern int ibpfs_sortFAT(FAT *fat);

/**
 * Retrieves a record from the file allocation table.
 * Retrieves a record from the file allocation table based on a file name
 * search.
 * @param fat The file allocation table as returned from \em ibpfs_readFAT().
 * @param key The search key (file to find).
 * @return A pointer to the FATRecord for the file, or NULL if not found.
 */
extern FATRecord *ibpfs_searchFAT(FAT *fat,const char *key);

#endif
