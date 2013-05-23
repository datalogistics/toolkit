/* $Id: clique_protocol.h,v 1.28 2000/02/29 20:33:24 hayes Exp $ */

#ifndef CLIQUE_PROTOCOL_H
#define CLIQUE_PROTOCOL_H


#include "hosts.h"  /* host_cookie host_desc */


/* The registration name for the clique control. */
#define CLIQUE_CONTROL_NAME "clique"


/*
** Checks to see if the expired child process #pid# was performing clique work
** and does any necessary post-death processing if so.
*/
void
CliqueChildDeath(int pid);


/*
** Performs one-time initialization of the clique experiment module, setting up
** to store experiment results in #memory#, which is registered with the name
** server as #memoryName#.  Returns 1 if successful, else 0.
*/
int
CliqueInit(const char *memoryName,
           const struct host_desc *memory);


/*
** Performs any clique work due to occur at the present time or earlier.
*/
void
CliqueWork(void);


/*
** Returns the earliest time any clique work is scheduled, or 0.0 if there is
** none.
*/
double
NextCliqueWork(void);


/*
** Returns 1 or 0 depending on whether or not #name# corresponds to the
** registration name of any known ongoing clique activity.
*/
int
RecognizedCliqueActivity(const char *name);


/*
** Initiates a #skill#-type clique activity on this machine, configuring the
** activity using #options#.  Registers the activity using #registration#.
** Returns 1 if successful, else 0.
*/
int
StartCliqueActivity(const char *registration,
                    const char *skill,
                    const char *options);


/*
** Terminates the clique activity named #registration# previously started via a
** call StartCliqueActivity().  If #registration# is blank, terminates all
** ongoing clique activity.  Returns 1 if successful, 0 otherwise.
*/
int
StopCliqueActivity(const char *registration);


#endif
