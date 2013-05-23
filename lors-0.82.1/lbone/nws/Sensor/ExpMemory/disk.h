/* $Id: disk.h,v 1.2 2000/03/08 20:34:04 hayes Exp $ */

#ifndef DISK_H
#define DISK_H


/*
** Measures the number of free bytes on the filesystem containing #path#.
** Returns 1 and sets #available# to the number of free bytes if successful,
** else returns 0.
*/
int
DiskGetFree(const char *path,
            double *available);


#endif
