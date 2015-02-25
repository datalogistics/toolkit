/* $Id: hybridCpu.c,v 1.24 2000/01/25 01:00:47 hayes Exp $ */

/* #define VERBOSE */

#include "config.h"
#include <math.h>        /* fabs() */
#include "diagnostic.h"  /* FAIL() LOG() */
#include "osutil.h"      /* CPUCount() */
#include "activeCpu.h"
#include "passiveCpu.h"
#include "hybridCpu.h"


#define MAX_FUSE 1000
#define MAX_NICE_VALUES 20
#define MAX_RESOURCES 10
#define TAME_VARIATION 0.01
#define WILD_VARIATION 0.15


/*
** Module variables.  These six are set by HybridCpuOpenMonitor() for reference
** by HybridCpuGetLoad().  #adaptingLength# and #adaptingPeriod# indicate
** whether or not we're using heuristics to adapt how long or how frquently we
** run the active sensor.  #activeProbeLength# and #activeProbePeriod# are the
** initial values for how long (in msecs) and how frequently (in terms of
** number of passive probes) we run active probes.  #passiveResources# caches
** the value returned by PassiveCpuResourceCount(); #totalCpus# the number of
** cpus on the system.
*/
static int adaptingLength;
static int adaptingPeriod;
static unsigned int activeProbeLength;
static int activeProbePeriod = PASSIVE_ONLY;
static int passiveResources;
static int totalCpus;

/*
** For each nice value that we're tracking, we initialize a TrackInfo struct in
** HybridCpuOpenMonitor(), then use heuristics to update it in
** HybridCpuGetLoad().  #fuse# is reduced by some fraction of #burn# every time
** HybridCpuGetLoad() runs a passive test until it reaches zero, at which point
** the function runs an active test.  #passiveError# holds the difference
** between the value reported by the prior active probe for each resource and
** the passive probe run at the same time.  #passivePrior# is the value
** reported by the previous passive probe for the #bestResource# test.
** #passiveSinceActive# is the number of passive probes which have been run
** since the most recent active probe.
*/
struct TrackInfo {
  unsigned short nice;
  int bestResource;
  int burn;
  int fuse;
  double passiveError[MAX_RESOURCES];
  double passivePrior;
  int passiveSinceActive;
};
static struct TrackInfo trackers[MAX_NICE_VALUES];
static int trackCount = 0;


int
HybridCpuCloseMonitor(void) {
  return(PassiveCpuCloseMonitor());
}


int
HybridCpuGetLoad(unsigned short niceValue,
                 double *newFraction,
                 double *existingFraction) {

  double activeAvail;
  int activeResult;
  double bestError;
  int i;
  double passiveAvail[MAX_RESOURCES];
  double passiveReport;
  int passiveResult[MAX_RESOURCES];
  struct TrackInfo *track;
  static int testNumber = 0;

  for(i = 0; i < trackCount; i++) {
    if(trackers[i].nice == niceValue)
      break;
  }

  if(i >= trackCount) {
    FAIL1("hybridCpuGetLoad: untracked nice value %d\n", niceValue);
  }

  track = &trackers[i];
  track->passiveSinceActive++;
  testNumber++;

  for(i = 0; i < passiveResources; i++) {
    passiveResult[i] =
      PassiveCpuGetLoad(i, niceValue, &passiveAvail[i], existingFraction);
  }

  if(!passiveResult[track->bestResource]) {
    FAIL("hybridCpuGetLoad: passive resource failed\n");
  }

  *newFraction = passiveAvail[track->bestResource] +
                 track->passiveError[track->bestResource];
  /* Check for obviously-incorrect values. */
  if(*newFraction < 0.0)
    *newFraction = 0.0;
  else if(*newFraction > totalCpus)
    *newFraction = totalCpus;

  /*
  ** Heuristic description: if the values reported by the passive probe begin
  ** to vary wildly, then the error we stored on the most recent active probe
  ** is stale and we want to move more quickly toward a new active probe.
  ** When we run an active probe, if the passive error is fairly consistent
  ** then we can relax a bit; otherwise, we want to move toward running
  ** active probes more frequently.
  */

  if(activeProbePeriod != PASSIVE_ONLY) {

    if(!adaptingPeriod)
      track->fuse -= track->burn;
    else if(fabs(track->passivePrior - passiveAvail[track->bestResource]) >=
            WILD_VARIATION)
      track->fuse -= track->burn * 2;
    else if(fabs(track->passivePrior - passiveAvail[track->bestResource]) <=
            TAME_VARIATION)
      track->fuse -= track->burn / 2;
    else
      track->fuse -= track->burn;

    if(track->fuse <= 0) {

      /* Run an active probe and reset all counters. */
      activeResult =
        ActiveCpuGetLoad(niceValue, 60, activeProbeLength, &activeAvail);
      LOG2("hybridCpu: Active probe %d reports %f\n", testNumber, activeAvail);

      if(activeResult) {

        bestError = 101.0;
        for(i = 0; i < passiveResources; i++) {
          if( passiveResult[i] &&
              (fabs(activeAvail - passiveAvail[i]) < fabs(bestError)) ) {
            bestError = activeAvail - passiveAvail[i];
            track->bestResource = i;
          }
        }
        LOG1("hybridCpu: using %s for passive probes.\n",
             PassiveCpuResourceName(track->bestResource));

        if(!adaptingPeriod || (track->passiveSinceActive <= 1))
          track->burn = MAX_FUSE / activeProbePeriod;
        else if(fabs(bestError - track->passiveError[track->bestResource]) >=
                WILD_VARIATION)
          track->burn = MAX_FUSE / (track->passiveSinceActive / 2);
        else if(fabs(bestError - track->passiveError[track->bestResource]) <=
                TAME_VARIATION)
          track->burn = MAX_FUSE / (track->passiveSinceActive * 2);
        else
          track->burn = MAX_FUSE / (track->passiveSinceActive + 1);
        *newFraction = activeAvail;
        for(i = 0; i < passiveResources; i++) {
          track->passiveError[i] = activeAvail - passiveAvail[i];
        }

      }

      track->fuse = MAX_FUSE;
      track->passiveSinceActive = 0;

    }

  }

#ifdef VERBOSE
  for(i = 0; i < passiveResources; i++) {
    passiveReport = passiveAvail[i] + passiveError[i];
    /* Check for obviously-incorrect values. */
    if(passiveReport < 0.0)
      passiveReport = 0.0;
    else if(passiveReport > totalCpus)
      passiveReport = totalCpus;
    LOG3("hybridCpu: %s probe %d reports %f\n",
         PassiveCpuResourceName(i), testNumber, passiveAvail[i]);
    LOG3("hybridCpu: %s probe %d returns %f\n",
         PassiveCpuResourceName(i), testNumber, passiveReport);
  }
#else
  passiveReport = 0.0; /* Avoid compiler warning. */
#endif

  track->passivePrior = passiveAvail[track->bestResource];
  return(1);

}


int
HybridCpuMonitorAvailable(void) {
  return PassiveCpuMonitorAvailable();
}


int
HybridCpuOpenMonitor(const unsigned short *niceValues,
                     int niceCount,
                     int checkFrequency,
                     int activePeriod,
                     int adaptPeriod,
                     int activeLength,
                     int adaptLength) {

  int i;
  int resource;

  LOG2("HybridCPUOpenMonitor: run every %d secs, active every %dth time\n",
       checkFrequency, activePeriod);

  activeProbeLength = activeLength;
  activeProbePeriod = activePeriod;
  adaptingLength = adaptLength;
  adaptingPeriod = adaptPeriod;
  passiveResources = PassiveCpuResourceCount();
  totalCpus = CPUCount();
  trackCount = niceCount;

  LOG1("HybridCPUOpenMonitor: %d CPUs detected\n", totalCpus);

  for(i = 0; i < trackCount; i++) {
    trackers[i].nice = niceValues[i];
    trackers[i].bestResource = 0;
    trackers[i].burn = 0;
    trackers[i].fuse = 0; /* This forces an initial active probe. */
    trackers[i].passivePrior = 0.0;
    trackers[i].passiveSinceActive = 0;
    for (resource = 0; resource < passiveResources; resource++) {
      trackers[i].passiveError[resource] = 0.0;
    }
  }

  return(PassiveCpuOpenMonitor(checkFrequency));

}
