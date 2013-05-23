/* $Id: skills.h,v 1.4 2000/03/08 20:31:23 hayes Exp $ */

#ifndef SKILLS_H
#define SKILLS_H


/*
** The skills module serves as a nameserver-aware wrapper around the sensor
** experiment modules.  By going through this module, sensor control modules
** can focus on scheduling experiments, rather than on the details of how to
** invoke them and interpret the results.  Concentrating skill control in a
** single module also allows multiple controls to support overlapping skills
** without needing to declare duplicated, and therefore fragile, skill names,
** option values, and the like.
*/


#include "dnsutil.h"   /* IPAddress */
#include "hosts.h"     /* host_desc */


/* Skills supported by this sensor and the resources they measure. */
typedef enum
  {cpuMonitor, diskMonitor, memoryMonitor, tcpConnectMonitor,
   tcpMessageMonitor} KnownSkills;
typedef enum
  {availableCpu, bandwidthTcp, connectTimeTcp, currentCpu, freeDisk,
   freeMemory, latencyTcp} MeasuredResources;


/*
** The result of invoking a skill: a measured resource, the options that apply
** to the measurement, an indication of whether or not the measurement
** succeeded, and, if it did, the actual measurement value.  Note that we're
** assuming here that all resource measurements can be expressed in one double
** value.  This is presently true, but may need to be revisited.
*/
typedef struct {
  MeasuredResources resource;
  char *options;
  int succeeded;
  double measurement;
} SkillResult;


/*
** A convenience function that scans #options# for a setting for #name#.
** Returns the associated value if found, else #defaultValue#.  The return
** value is volatile and will be overwritten by subsequent calls.
*/
const char *
GetOptionValue(const char *options,
               const char *name,
               const char *defaultValue);


/*
** Returns a unit label for use when registering series of #resource#.  The
** return value is volatile and will be overwritten by subsequent calls.
*/
const char *
ResourceLabel(MeasuredResources resource);


/*
** Returns a string representation of #resource#.  The return value is volatile
** and will be overwritten by subsequent calls.
*/
const char *
ResourceName(MeasuredResources resource);


/*
** Returns 1 or 0 depending on whether or not the system supports conducting
** #skill# experiments configured by #options#.
*/
int
SkillAvailable(KnownSkills skill,
               const char *options);


/*
** Returns a string representation of #skill#.  The return value is volatile
** and will be overwritten by subsequent calls.
*/
const char *
SkillName(KnownSkills skill);


/*
** Draws from the tab-delimited list #options# the subset that applies to
** #skill#.  Returns the result in #toWhere#.
*/
void
SkillOptions(KnownSkills skill,
             const char *options,
             char *toWhere);


/*
** Returns the list of resources measured by #skill# when configured with
** #options#.  Returns the list in #resources# and its length in #length#.
*/
void
SkillResources(KnownSkills skill,
               const char *options,
               const MeasuredResources **resources,
               int *length);


/* Prepares the skill module for use.  Returns 1 if successful, else 0. */
int
SkillsInit(void);


/*
** Attempts to conduct #skill# experiments configured by #options#.  Returns
** the results of the experiments in the array #results# and the length of this
** array in #length#.  Returns failure results if the skill does not complete
** within #timeOut# seconds.
*/
void
UseSkill(KnownSkills skill,
         const char *options,
         double timeOut,
         const SkillResult **results,
         int *length);

#endif
