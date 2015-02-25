/* $Id: active_memory.c,v 1.8 2001/02/18 08:52:21 pgeoffra Exp $ */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "diagnostic.h"
#include "memory.h"
#include "active_memory_custom.h"

#define READ_END 0
#define WRITE_END 1

int ActiveCpuGetLoad(unsigned short niceValue,
		     unsigned int maxWait,
		     unsigned int testLength,
		     double *available);
int PassiveCpuCloseMonitor(void);
int PassiveCpuGetLoad(int resource,
		      unsigned short niceValue,
		      double *newFraction,
		      double *existingFraction);
int PassiveCpuMonitorAvailable(void);
int PassiveCpuOpenMonitor(int sleeptime);

typedef struct bulk {
  char * ptr;
  struct bulk * next;
  int length;
} Bulk;


static int BaseMem = 0;
static int cpu_monitor_on = 0;
static unsigned long CountFromLastCalibration = 0;
static unsigned long CountBetweenCalibration = INIT_COUNT_BETWEEN_CALIBRATION;
extern int errno;
extern int debug;

/* compute the total ammount of physical memory used
   it's the sum of the resident size of each processes
   this function parse the output of "PS PS_ARGS" */
unsigned int 
ActiveMemoryGetUsed(void) {

  FILE * file;
  char command[128];
  char string[4096];
  char * token;
  unsigned int field_status, field_rss, memory_used;
  unsigned int index_token, status_valid, rss_value;
  
  strcpy(command, PS);
  strcat(command, " ");
  strcat(command, PS_ARGS);
  if ((file = popen(command, "r")) != NULL) {
    field_status = 0;
    field_rss = 0;
    memory_used = 0;
    while ((field_status == 0) || (field_rss == 0)) {
      if (fgets(string, sizeof(string), file) != NULL) {
	token = strtok(string, PS_DELIM);
	index_token = 1;
	while (token != NULL) {
	  if ((field_status ==0) && (strcmp(token, STATUS_FIELD) == 0))
	    field_status = index_token;
	  if ((field_rss ==0) && (strcmp(token, RSS_FIELD) == 0))
	    field_rss = index_token;
	  index_token++;
	  token = strtok(NULL, PS_DELIM);
	}
      } else
         break;
    }
    
    if ((field_status == 0) || (field_rss == 0))
      FAIL3("ActiveMemoryGetUsed: Error in reading (%s): "
	    "unable to find the fields %s and %s\n",
	    command, STATUS_FIELD, RSS_FIELD);
    
    memory_used = 0;
    
    while (fgets(string, sizeof(string), file) != NULL) {
      token = strtok(string, PS_DELIM);
      index_token = 1;
      status_valid = 0;
      while (token != NULL) {
	if (index_token == field_status) 
	  if (strpbrk(token, STATUS_VALID) != NULL)
	    status_valid = 1;
	if ((status_valid) && (index_token == field_rss)) {
	  if (sscanf(token, "%d", &rss_value) != 1) {
	    FAIL3("ActiveMemoryGetUsed: Error in reading (%s) from (%s):"
		  "no valid RSS value in (%s)\n",
		  command, string, token);
	  } else
	    memory_used += rss_value*MEMSOR_UNIT;
	}
	index_token++;
	token = strtok(NULL, PS_DELIM);
      }
    }
      
    pclose(file);
  } else
    FAIL2("ActiveMemoryGetUsed: execution of %s failed (error %d)\n",
          command, errno);

  return memory_used;
}


/* Try to get a good rough estimate of the free memory
   to speed up the calibration process */
unsigned int
ActiveMemoryGetRoughlyFree(void) {

  FILE * file;
  char command[128];
  char string[4096];
  char * token;
  unsigned int field_free, memory_free;
  unsigned int index_token, free_value;

  strcpy(command, VMSTAT);
  if (strstr(command, "vmstat") == NULL)
    return 0;
  strcat(command, " 1 2");
  field_free = 0;
  if ((file = popen(command, "r")) != NULL) {
    while (field_free == 0) {
      if (fgets(string, sizeof(string), file) != NULL) {
        token = strtok(string, VMSTAT_DELIM);
        index_token = 1;
        while ((token != NULL) && (field_free == 0))  {
          if ((field_free ==0) && (strcmp(token, VMSTAT_FREE) == 0))
            field_free = index_token;
          index_token++;
          token = strtok(NULL, VMSTAT_DELIM);
        }
      } else
         break;
    }

    if (field_free == 0)
      FAIL2("ActiveMemoryGetRoughlyFree: Error in reading (%s): "
            "unable to find the field %s\n",
            command, VMSTAT_FREE);
 
    memory_free = 0;
    free_value = 0;
    sleep(1);
 
    while (fgets(string, sizeof(string), file) != NULL) {
      token = strtok(string, VMSTAT_DELIM);
      index_token = 1;
      while (token != NULL) {
        if (index_token == field_free) {
          if (sscanf(token, "%d", &free_value) != 1) {
            FAIL3("ActiveMemoryGetRoughlyFree: Error in reading (%s) from (%s):"
                  "no valid FREE value in (%s)\n",
                  command, string, token);
          } else
            memory_free = free_value*MEMSOR_UNIT;
        }
        index_token++;
        token = strtok(NULL, VMSTAT_DELIM);
      }
    }

    pclose(file);
  } else
    FAIL2("ActiveMemoryGetRoughlyFree: execution of %s failed (error %d)\n",
          command, errno);

  return memory_free;
}


/* this code return the ammount of physical memory available 
   it's the base memory minus the memory used at this time */
int
ActiveMemoryGetFree(double *measure) {
 
#if MEMORY_SUPPORT
  unsigned int usedmem;
  int freemem;
  double current, available;

  if (debug > 0)
    DDEBUG3("MemoryGetFree: basemem = %d, count = %d, max = %d\n",
	    BaseMem,CountFromLastCalibration,CountBetweenCalibration);
  
  if (BaseMem == 0)
    ActiveMemoryMonitorCalibration();
  
  if (CountFromLastCalibration > CountBetweenCalibration) {
    if (cpu_monitor_on) {
      if (PassiveCpuGetLoad(0, 0, &available, &current)) {
	if (debug > 0)
	  DDEBUG1("MemoryGetFree: Passive cpu load=%f\n", current);
	if (current >= MIN_CPU_FOR_CALIBRATION) {
	  if (ActiveCpuGetLoad(19, 20, 500, &available)) {
	    if (debug > 0)
	      DDEBUG1("MemoryGetFree: Active cpu load=%f\n", available);
	    if (available >= MIN_CPU_FOR_CALIBRATION) {
	      if (PassiveCpuCloseMonitor())
		cpu_monitor_on = 0;
	      ActiveMemoryMonitorCalibration();
	      CountFromLastCalibration = 0;
	      if (CountBetweenCalibration < MAX_COUNT_BETWEEN_CALIBRATION)
		CountBetweenCalibration *= 2;
	    }
	  } else
	    LOG("MemoryGetFree: error getting an active cpu measurement\n");
	}
      } else
	LOG("MemoryGetFree: error getting a passive cpu measurement\n");  
    } else
      if (!PassiveCpuMonitorAvailable)
	CountFromLastCalibration = 0;
      else 
	if (PassiveCpuOpenMonitor(20))
	  cpu_monitor_on = 1;
	else
	  LOG("MemoryGetFree: problem starting passive CPU sensor\n");
  } else {
    if ((CountFromLastCalibration > CountBetweenCalibration - 16)
	&& (!cpu_monitor_on)
	&& (PassiveCpuMonitorAvailable))
      if (PassiveCpuOpenMonitor(20)) {
	cpu_monitor_on = 1;
      } else
	LOG("MemoryGetFree: problem starting passive CPU sensor\n");
  }
  usedmem = ActiveMemoryGetUsed();
  freemem = BaseMem - usedmem;
  if (freemem < 0)
    FAIL2("MemoryGetFree: Base memory size to small :"
	  "base=%d and memory in used=%d\n", BaseMem, usedmem);
  
  if (debug > 0) {
    DDEBUG2("MemoryGetFree: %d kB physical memory available (%d kB used)\n",
		  freemem, usedmem);
  }
  *measure = (freemem/1024.);
  CountFromLastCalibration++;
  
  return(1);
#else
  return 0;
#endif

}


int
ActiveMemoryMonitorAvailable(void) {
  return(MEMORY_SUPPORT);
}


/* a calibration is needed to compute the maximum ammount
   of memory available on this machine. It allocates step
   by step CALIBRATION_BULK_SIZE of memory and ask the sensor to give
   the ammount of memory used by the application. When this ammount of
   used memory does not grow any more, we have reached the top. */

int
ActiveMemoryMonitorCalibration(void) {

#if MEMORY_SUPPORT
  unsigned int i, page_size, used, max_used, stop_count, base_start;
  struct bulk *start, *current;
  int connection[2];
  pid_t childPid;
 
  if(pipe(connection) != 0) {
      FAIL("ActiveMemoryMonitorCalibration: pipe() failed\n");
    }

  childPid = fork();

  if(childPid < 0) {
      FAIL("ActiveMemoryMonitorCalibration: fork() failed\n");
  }
  else if(childPid > 0) {
    /* Parent process. */
    close(connection[WRITE_END]);
    read(connection[READ_END], &BaseMem, sizeof(int));
    close(connection[READ_END]);
    wait(NULL);
    return 1;
  }

  /* Child comes here. */
  BaseMem = 0;
  close(connection[READ_END]);
  nice(19);
  LOG("ActiveMemoryMonitorCalibration: start calibration\n");
  page_size = getpagesize();
  base_start = ActiveMemoryGetRoughlyFree()*CALIBRATION_SPEEDUP_RATIO; 
  if (debug > 0) {
        DDEBUG1("Calibration speedup : start from %d KB of base memory\n", base_start);
  }

  start = (struct bulk *)malloc(sizeof(struct bulk));
  if (start == NULL) {
    write(connection[WRITE_END], &BaseMem, sizeof(int));
    close(connection[WRITE_END]);
    exit(0);
  }
  start->ptr = calloc(base_start*1024,sizeof(char));
  if (start->ptr == NULL) {
    free(start);
    write(connection[WRITE_END], &BaseMem, sizeof(int));
    close(connection[WRITE_END]);
    exit(0);
  } 
  start->length = base_start*1024*sizeof(char);
  start->next = NULL;

  used = 99;
  for (i=0; i<start->length; i+=page_size)
    start->ptr[i] = (int)used;
  start->ptr[start->length-1] = (int)used;
  
  current = start;
  max_used = 0;
  stop_count = 0;
  used = 1;
  while (stop_count <ITERATIONS_STALLED) {
    current->next = (struct bulk *)malloc(sizeof(struct bulk));
    if (current->next == NULL)
      break;
    current = current->next;
    current->ptr = malloc(CALIBRATION_BULK_SIZE*sizeof(char));
    if (current->ptr == NULL) {
      stop_count++;
      continue;
    }
    current->length = CALIBRATION_BULK_SIZE*sizeof(char);
    current->next = NULL;
    
    for (i=0; i<current->length; i+=page_size)
      current->ptr[i] = (int)used;
    current->ptr[current->length-1] = (int)used;
    
    used = ActiveMemoryGetUsed();
    if (used > max_used)
      max_used = used;
    else
      stop_count++;
  }
  
  BaseMem = max_used;
  if (debug > 0) {
	DDEBUG1("Calibration done : %d KB of base memory\n", BaseMem);
  }

  current = start;
  while (current != NULL) {
     free(current->ptr);
    current = current->next;
  }
  LOG1("ActiveMemoryMonitorCalibration: end of calibration (%d KB)\n",
       BaseMem);
  write(connection[WRITE_END], &BaseMem, sizeof(int));
  close(connection[WRITE_END]);
  exit(0); 
#else
  return 0;
#endif

}
