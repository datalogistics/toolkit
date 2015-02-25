/* $Id: passiveCpu.c,v 1.30 2000/03/22 16:28:20 hayes Exp $ */

#include "config.h"
#include <stdio.h>      /* sprintf() */
#include <ctype.h>      /* isspace() */
#include <stdlib.h>     /* atoi() */
#include <errno.h>      /* errno */
#include <string.h>     /* memset() */
#include <sys/time.h>   /* struct timeval */
#include <sys/types.h>  /* fd_set */

#include "diagnostic.h" /* FAIL() */
#include "strutil.h"    /* GETWORD() */
#include "osutil.h"     /* CPUCount() CurrentTime() */
#include "passiveCpu.h"

/*
** NOTE: Testing seems to indicate that incorporating vmstat into our
** measurements actually lowers their accuracy.  We haven't looked into whether
** this is inherent to the way vmstat reports its values, or whether we're
** just interpreting them wrong.  At any rate, this #undef disables vmstat
** reporting for the time being.  Note also that the revised NWS configuration
** script does *not* set either the AIX or sun variables on which our vmstat
** calculations depend.  This will need to be addressed if vmstat use is ever
** re-enabled.
*/
#undef VMSTAT_PATH


#ifdef UPTIME_PATH
#define UPTIME (0)
#define UPTIMELOADFIELD 10
#define UPTIME_COUNT 1
#else
#define UPTIME_COUNT 0
#endif

#ifdef VMSTAT_PATH
#define VMSTAT (0 + UPTIME_COUNT)
#define VMSTATCPUFIELD 1 
#define VMSTAT_COUNT 1
#else
#define VMSTAT_COUNT 0
#endif

#define RESOURCE_COUNT (UPTIME_COUNT + VMSTAT_COUNT)

/*
** Module variables.  #cpus# and #period# cache the number of cpus on the
** system and the expected test frequency, respectively.  In order to query
** resources as infrequently as possible (to avoid loading the system and/or
** slowing the sensor), we cache the most recent measurements taken from each
** resource and, when called, attempt to use these in our computations.
*/
static int cpus;
static int period;

/* CachedMeasurements could be a union, but a struct is easier to work with. */
struct CachedMeasurements {
#ifdef UPTIME_PATH
  double uptimeQueue;
#endif
#ifdef VMSTAT_PATH
  int vmstatIdle;
  int vmstatProcs;
  int vmstatSys;
  int vmstatUser;
#endif
  int dummyField;
};
struct ResourceInfo {
  unsigned short niceValue;
  double timeTaken;
  struct CachedMeasurements cache;
  FILE *resourcePipe;
};
static struct ResourceInfo resources[RESOURCE_COUNT + 1];


/*
** Retrieves a line of #resource# output.  Returns the line if successful,
** otherwise NULL.  The value returned is volatile and will be overwritten by
** subsequent calls.
*/
static const char *
GetResourceLine(int resource) {

  FILE *pipeToResource;
  static char resourceOutput[255 + 1];

  switch (resource) {
#ifdef UPTIME_PATH
  case UPTIME:
    if ((pipeToResource = popen(UPTIME_PATH, "r")) == NULL)
      FAIL1("PassiveCpuGetLoad: Unable to open pipe, errno = %d\n", errno);
    break;
#endif
#ifdef VMSTAT_PATH
  case VMSTAT:
    pipeToResource = resources[VMSTAT].resourcePipe;
    break;
#endif
  default:
    FAIL1("GetResourceLine: unknown resource %d\n", resource);
  }

  /*
  ** aix needs second shot at reading... gets ECHILD on the first one but
  ** works on the second.  This is only called if the first one fails, so
  ** there's no worry about loss of data and I think fgets is predictable
  ** enough to return NULL repeatedly when there's a real problem.
  */
  if( (fgets(resourceOutput, 255, pipeToResource) == NULL) &&
      (fgets(resourceOutput, 255, pipeToResource) == NULL) ) {
    pclose(pipeToResource);
    FAIL2("GetResourceLine: %d read failed; errno=%d\n", resource, errno);
  }

  switch (resource) {
#ifdef UPTIME_PATH
  case UPTIME:
    pclose(pipeToResource);
    break;
#endif
  default:
    break; /* Nothing to do. */
  }

  return(&resourceOutput[0]);

}


/*
** Returns 0 if CPU fractions for a #desiredNice# process cannot be calculated
** using #resource# measurements taken from a #measuredNice# process.
** Otherwise, returns 1 and uses #measurements# to calculate an upper bound on
** the amount of CPU available to new and existing processes in #desiredNew#
** and #desiredExisting#, respectively.
*/
static int
Calculate(int resource,
          unsigned short measuredNice,
          struct CachedMeasurements measurements,
          unsigned short desiredNice,
          double *desiredNew,
          double *desiredExisting) {

#ifdef VMSTAT_PATH
  double factor;
#endif

  switch (resource) {

#ifdef UPTIME_PATH
  case UPTIME:
    if(measuredNice > desiredNice)
      return(0);
    else if (desiredNice == measuredNice) {
      *desiredNew = (float)cpus / (measurements.uptimeQueue + 1.0);
      *desiredExisting = (measurements.uptimeQueue < 1.0) ? (float)cpus :
                         ((float)cpus / measurements.uptimeQueue);
    }
    else {
      /* Pessimistically assume that everyone else has higher priority.  This
      ** means that a new process only gets CPU time that no other process
      ** wants.  For an existing process, optimistically assume that the
      ** process is presently running.  If the queue is less than one, then
      ** we'll assume that it contains only the niced process; therefore, the
      ** process gets everything.  Otherwise, 
      */
      *desiredNew = ((float)cpus > measurements.uptimeQueue) ?
                    ((float)cpus - measurements.uptimeQueue) : 0.0;
      *desiredExisting = *desiredNew;
    }
    break;
#endif

#ifdef VMSTAT_PATH
  case VMSTAT:
    if(measuredNice > desiredNice)
      return(0);
    /*
    ** The amount of user time weights the degree to which system time can be
    ** shared.  The more user time, the more the accompanying system time can
    ** be shared.
    */
    factor = (float)measurements.vmstatUser / 100.0;
    *desiredNew =
      (measurements.vmstatIdle / 100.0) +
      (((float)measurements.vmstatUser / 100.0) /
       ((float)measurements.vmstatProcs + 1.0)) +
      (((float)measurements.vmstatSys * factor / 100.0) /
       ((float)measurements.vmstatProcs + 1.0));
    if(measurements.vmstatProcs == 0) {
      *desiredExisting = *desiredNew;
    } else {
      *desiredExisting =
        (measurements.vmstatIdle / 100.0) +
        (((float)measurements.vmstatUser / 100.0) /
         ((float)measurements.vmstatProcs)) +
        (((float)measurements.vmstatSys * factor / 100.0) /
         ((float)measurements.vmstatProcs));
    }
    break;
#endif

  default:
    FAIL1("Unknown resource %d\n", resource);

  }

  return(1);

}


int
PassiveCpuCloseMonitor(void) {
#ifdef VMSTAT_PATH
  pclose(resources[VMSTAT].resourcePipe);
  resources[VMSTAT].resourcePipe = NULL;
#endif
  return(1);
}


int
PassiveCpuGetLoad(int resource,
                  unsigned short niceValue,
                  double *newFraction,
                  double *existingFraction) {

  double now;
  int parseResult;
#ifdef VMSTAT_PATH
  struct timeval noWait = {0, 0};
  fd_set pipeFDs;
#endif

  /* First, try to get values cheaply using numbers we already have. */
  now = CurrentTime();
  if(((now - resources[resource].timeTaken) < (double)period) &&
     Calculate(resource,
               resources[resource].niceValue,
               resources[resource].cache,
               niceValue,
               newFraction,
               existingFraction)) {
    return(1);
  }

#ifdef VMSTAT_PATH
  if((resource == VMSTAT)) {
    FD_ZERO(&pipeFDs);
    FD_SET(fileno(resources[VMSTAT].resourcePipe), &pipeFDs);
    if(select(FD_SETSIZE, &pipeFDs, NULL, NULL, &noWait) <= 0) {
      FAIL("PassiveCpuGetLoad: no vmstat output available\n");
    }
  }
#endif

  parseResult = PassiveCpuParse
    (resource, niceValue, GetResourceLine, newFraction, existingFraction);

#ifdef VMSTAT_PATH
  if(resource == VMSTAT) {
    /*
    ** Make sure that we pull the lastest information from the pipe, which
    ** might contain multiple lines if it's been a while since we were last
    ** called.
    */
    while(1) {
      FD_ZERO(&pipeFDs);
      FD_SET(fileno(resources[VMSTAT].resourcePipe), &pipeFDs);
      if(select(FD_SETSIZE, &pipeFDs, NULL, NULL, &noWait) <= 0)
        break;
      parseResult = PassiveCpuParse
        (resource, niceValue, GetResourceLine, newFraction, existingFraction);
    }
  }
#endif
           
  return(parseResult);

}


int
PassiveCpuMonitorAvailable(void) {
  return(RESOURCE_COUNT > 0);
}


int
PassiveCpuOpenMonitor(int sleeptime) {

#ifdef VMSTAT_PATH
  char runCommand[255 + 1];
#endif

  cpus = CPUCount();
  period = sleeptime;
  memset(&resources, 0, sizeof(resources));

#ifdef VMSTAT_PATH
  sprintf(runCommand, "%s %d", VMSTAT_PATH, sleeptime);
  resources[VMSTAT].resourcePipe = popen(runCommand, "r");
#endif

  return(RESOURCE_COUNT > 0);

}


int
PassiveCpuParse(int resource,
                unsigned short niceValue,
                const char *GetNextLine(int resource),
                double *newFraction,
                double *existingFraction) {

  char cpuWord[99 + 1];
  const char *curr;
  int field;

  curr = GetNextLine(resource);
  if(curr == NULL)
    FAIL("PassiveCpuParse: unable to retrieve resource line\n");

  switch (resource) {

#ifdef UPTIME_PATH
  case UPTIME:

    for (field = 1; field < UPTIMELOADFIELD; field++) {
      GETWORD(cpuWord, curr, &curr);
    }
    if (cpuWord[0] == 'a') {
      /* Beginning of the word "average" -- occasionally we see an extra word */
      GETWORD(cpuWord, curr, &curr);
    }

    resources[UPTIME].niceValue = 0;
    resources[UPTIME].cache.uptimeQueue = atof(cpuWord);
    break;
#endif

#ifdef VMSTAT_PATH
  case VMSTAT:

    /* Skip header lines. */
    while(1) {
      GETWORD(cpuWord, curr, &curr);
      if(isdigit((int)cpuWord[0]))
        break;
      curr = GetNextLine(resource);
      if(curr == NULL)
        FAIL("PassiveCpuParse: unable to retrieve resource line\n");
    }

    resources[VMSTAT].niceValue = 0;

    /* get the number of running processes */
    for (field = 2; field < VMSTATCPUFIELD; field++) {
      GETWORD(cpuWord, curr, &curr);
    }
    resources[VMSTAT].cache.vmstatProcs = atoi(cpuWord);

    /* find the last non-space character. */
    while(*curr != '\n') curr++;
    while(isspace((int)*curr)) curr--;

#if defined(AIX)
    /* on AIX the rightmost field is the wait state field.  Skip it.  */
    curr -= 3;
    while(isspace((int)*curr)) curr--;
#endif

    /* Convert the idle cpu field to a percentage. */
    curr -= 2;
    resources[VMSTAT].cache.vmstatIdle = atoi(curr);

    if(resources[VMSTAT].cache.vmstatIdle == 100) {
      resources[VMSTAT].cache.vmstatSys  = 0;
      resources[VMSTAT].cache.vmstatUser = 0;
    } else {
      /* Convert the sys cpu field to a percentage. */
      curr--;
      while(isspace((int)*curr)) curr--;
      curr -= 2;
      resources[VMSTAT].cache.vmstatSys = atoi(curr);
      resources[VMSTAT].cache.vmstatUser =
        100 - (resources[VMSTAT].cache.vmstatIdle +
               resources[VMSTAT].cache.vmstatSys);
    }

#if defined(sun) && defined(__SVR4)
    /*
    ** Hack city: solaris fails to report one of the running processes in
    ** vmstat.  vmstat will also report 0, with an idle time of less that 100%,
    ** if there are a few very short running jobs.  Here, we arbitrarily set
    ** 80% idle at the cutoff for solaris.
    */
    if(resources[VMSTAT].cache.vmstatIdle <= 80)
      resources[VMSTAT].cache.vmstatProcs++;
#endif
    break;

#endif
  default:
    FAIL1("PassiveCpuParseStat: unknown resource %d\n", resource);

  }

  resources[resource].timeTaken = CurrentTime();
  Calculate(resource,
            resources[resource].niceValue,
            resources[resource].cache,
            niceValue,
            newFraction,
            existingFraction);
  return(1);

}


int
PassiveCpuResourceCount(void) {
  return(RESOURCE_COUNT);
}


const char *
PassiveCpuResourceName(int resource) {
  static const char *resourceNames[RESOURCE_COUNT + 1] = {
#ifdef UPTIME_PATH
     "uptime",
#endif
#ifdef VMSTAT_PATH
     "vmstat",
#endif
      ""
  };
  return resourceNames[resource];
}
