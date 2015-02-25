/* $Id: disk.c,v 1.3 2000/06/26 18:06:35 swany Exp $ */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include "diagnostic.h"
#include "disk.h"


#ifdef HAVE_STATVFS
#  include <sys/statvfs.h>
   typedef struct statvfs FileSystemStats;
#  define GetFileSystemStats statvfs
#  define BlockSizeField f_frsize
#else
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
typedef struct statfs FileSystemStats;
#  define GetFileSystemStats statfs
#  define BlockSizeField f_bsize
#else
#  include <sys/statfs.h>
   typedef struct statfs FileSystemStats;
#  define GetFileSystemStats statfs
#  define BlockSizeField f_bsize
#endif
#endif


int
DiskGetFree(const char *path,
            double *available) {
  FileSystemStats fsStats;
  if(GetFileSystemStats(path, &fsStats) < 0) {
    FAIL("DiskGetFree: status retrieval failed\n");
  }
  *available = (double)fsStats.f_bavail * (double)fsStats.BlockSizeField;
  return 1;
}
