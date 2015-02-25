/* $Id: hybridCpu.h,v 1.8 2000/01/25 01:00:47 hayes Exp $ */

#ifndef HYBRIDCPU_H
#define HYBRIDCPU_H


/*
** Combines passiveCpu and activeCpu modules to compose a hybrid active/passive
** cpu monitor.
*/


/*
** Shuts down the hybrid cpu monitor.
*/
int
HybridCpuCloseMonitor(void);


/*
** Attempts to determine the fraction of a cpu available to a new and an
** existing process of priority #niceValue#, which must be one of the nice
** values previously passed to hybridCpuOpenMonitor().  If successful, sets the
** other parameters to the new and existing fractions and returns 1, else
** returns 0.
*/
int
HybridCpuGetLoad(unsigned short niceValue,
                 double *newFraction,
                 double *existingFraction);


/*
** Returns 1 or 0 depending on whether or not the O/S provides facilities for
** hybrid CPU monitoring.
*/
int
HybridCpuMonitorAvailable(void);


/*
** Spawns a hybrid cpu monitor; must be called before any calls to
** hybridGetCpuValues().  The #niceCount#-long array #niceValues# specifies the
** set of process nice values of interest.  #check_frequency# specifies the
** number of seconds which should elapse between checks.  #active_period#
** indicates how often an active probe should be run, relative to passive
** probes.  A value of 5 indicates one active probe for every five passive
** probes, 10 one active probe for every 10 passive probes, etc.  The values
** ACTIVE_ONLY and PASSIVE_ONLY may be passed to indicate that only active or
** passive probes should be made.  #active_length# specifies the running time,
** in millisecs, of the active probe.  If #adapt_period# or #adapt_length# have
** the value ADAPT, the monitor will use heuristics to iteratively adjust the
** active probing period or active probe running time.  Returns 1 if
** successful; 0 otherwise.
*/
#define ACTIVE_ONLY 1
#define PASSIVE_ONLY 0

#define ADAPT 1
#define DONT_ADAPT 0

int
HybridCpuOpenMonitor(const unsigned short *niceValues,
                     int niceCount,
                     int checkFrequency,
                     int activePeriod,
                     int adaptPeriod,
                     int activeLength,
                     int adaptLength);

#endif
