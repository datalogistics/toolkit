/* $Id: periodic.h,v 1.3 2000/02/29 20:33:24 hayes Exp $ */

#ifndef PERIODIC_H
#define PERIODIC_H


/*
** The periodic module is a sensor control module that supports executing
** skills at fixed intervals.
*/


#include "hosts.h"  /* host_cookie host_desc */


/* The registration name for the periodic control. */
#define PERIODIC_CONTROL_NAME "periodic"


/*
** Checks to see if the expired child process #pid# was performing periodic
** work and does any necessary post-death processing if so.
*/
void
PeriodicChildDeath(int pid);


/*
** Performs one-time initialization of the periodic experiment module, setting
** up to store experiment results in #memory#, which is registered with the
** name server as #memoryName#.  Returns 1 if successful, else 0.
*/
int
PeriodicInit(const char *memoryName,
             const struct host_desc *memory);


/*
** Performs any periodic work due to occur at the present time or earlier.
*/
void
PeriodicWork(void);


/*
** Returns the earliest time any periodic work is scheduled, or 0.0 if there is
** none.
*/
double
NextPeriodicWork(void);


/*
** Returns 1 or 0 depending on whether or not #name# corresponds to the
** registration name of any known ongoing periodic activity.
*/
int
RecognizedPeriodicActivity(const char *name);


/*
** Initiates a #skill#-type periodic activity on this machine, configuring the
** activity using #options#.  Registers the activity using #registration#.
** Returns 1 if successful, else 0.
*/
int
StartPeriodicActivity(const char *registration,
                      const char *skill,
                      const char *options);


/*
** Terminates the periodic activity named #registration# previously started via
** a call StartPeriodicActivity().  If #registration# is blank, terminates all
** ongoing periodic activity.  Returns 1 if successful, 0 otherwise.
*/
int
StopPeriodicActivity(const char *registration);


#endif
