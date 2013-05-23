/* $Id: memory.c,v 1.14 2001/02/07 18:29:09 swany Exp $ */

#include "memory.h"

int
MemoryGetFree(double *newFraction) {
  return(PassiveMemoryGetFree(newFraction));
}

int
MemoryMonitorAvailable(void) {
  return(PassiveMemoryMonitorAvailable());
}
