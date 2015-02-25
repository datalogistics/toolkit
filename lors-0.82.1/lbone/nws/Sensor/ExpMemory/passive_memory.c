/* $Id: passive_memory.c,v 1.5 2001/02/07 18:29:09 swany Exp $ */

#include <stdio.h>
#include "diagnostic.h"
#include "memory.h"

#define MEMINFO "/proc/meminfo"

extern int debug;

int
PassiveMemoryGetFree(double *measure) {

  int bytesOfBuffer;
  int bytesOfCache;
  int bytesFree;
  int bytesShared;
  int bytesTotal;
  int bytesUsed;
  FILE *file;
  char line[511 + 1];

  file = fopen(MEMINFO, "r");
  if(file == NULL) {
    FAIL1("PassiveMemoryGetFree: Open of %s failed\n", MEMINFO);
  }

  while(fgets(line, sizeof(line), file) != NULL) {
    if(line[0] == ' ')
      continue;
    if(sscanf(line, "%*s %d %d %d %d %d %d",
              &bytesTotal, &bytesUsed, &bytesFree, &bytesShared,
              &bytesOfBuffer, &bytesOfCache) > 0) {
      /* free memory is the truly free memory, the memory used for cache (it'll
       * be freed on request) and the memory used for buffering (althought some
       * of it has to be present, so we are going to be optimistic now) */
      *measure = (bytesFree + bytesOfBuffer + bytesOfCache) / (1024 * 1024);
	  if (debug > 0) {
		DDEBUG1("PassiveMemoryGetFree: %f Mb free memory\n", *measure /
				(1024.0 * 1024.0));
	  }
      break;
    }
  }

  fclose(file);
  return(1);

}


int
PassiveMemoryMonitorAvailable(void) {
  FILE *file;
  file = fopen(MEMINFO, "r");
  if(file == NULL)
    return(0);
  fclose(file);
  return(1);
}
