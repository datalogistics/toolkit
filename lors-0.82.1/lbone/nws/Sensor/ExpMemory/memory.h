/* $Id: memory.h,v 1.5 2000/10/04 19:05:40 swany Exp $ */

#ifndef MEMORY_H
#define MEMORY_H

int
ActiveMemoryGetFree(double *newFraction);


int
ActiveMemoryMonitorAvailable(void);


int
ActiveMemoryMonitorCalibration(void);


int
PassiveMemoryGetFree(double *newFraction);


int
PassiveMemoryMonitorAvailable(void);


int
MemoryGetFree(double *newFraction);


int
MemoryMonitorAvailable(void);

#endif
