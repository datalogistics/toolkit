/* $Id: exp_protocol.h,v 1.18 2000/01/26 22:31:31 hayes Exp $ */

#ifndef EXP_PROTOCOL_H
#define EXP_PROTOCOL_H


#include "experiments.h" /* experiment structures */
#include "hosts.h"       /* host_cookie host_desc */
#include "register.h"    /* Object */
#include <sys/types.h>   /* size_t */


/*
** Contacts the memory connected to #mem_c# to retrieve experiment information
** for the experiment named #exp_name#.  Connects #mem_c# if necessary.  Begins
** retrieval with the first record recorded after time #seq_no#; a non-positive
** value for this parameter indicates that retrieval should begin with the
** first record.  If successful within #timeout# seconds, returns and 1 and
** stores into the array #exper# at most #count# records, along with storing
** the number of records retrieved in #out_count# and the timestamp of the
** final record in #out_seq_no#; otherwise, returns 0.  The newest experiment
** retrieved is stored in exper[0], the oldest in exper[*out_count - 1].
*/
int
LoadExperiments(struct host_cookie *mem_c,
                const char *exp_name,
                Experiment *exper,
                size_t count,
                double seq_no,
                size_t *out_count,
                double *out_seq_no,
                double timeout);


/*
** These three functions return default registration names for experiment
** structures.  Although registrations may use any name, using these functions
** will make retrieval easier.  NOTE: There is no default naming convention for
** activities.  Registerers must generate their own names for these objects.
*/
const char *
NameOfControl(const Control *control);
const char *
NameOfSeries(const Series *series);
const char *
NameOfSkill(const Skill *skill);


/*
** These eight functions are convenience functions for translating between
** experiment structures and registration Objects.  The caller is responsible
** for freeing the Objects returned by the "From" function when they are no
** longer needed.
*/
Object
ObjectFromActivity(const char *name,
                   const Activity *activity);
Object
ObjectFromControl(const char *name,
                  const Control *control);
Object
ObjectFromSeries(const char *name,
                 const Series *series);
Object
ObjectFromSkill(const char *name,
                const Skill *skill);
void
ObjectToActivity(const Object object,
                 Activity *activity);
void
ObjectToControl(const Object object,
                Control *control);
void
ObjectToSeries(const Object object,
               Series *series);
void
ObjectToSkill(const Object object,
              Skill *skill);


/*
** Stores in the memory connected to #mem_c# the experiments in the
** #count#-long array #exper#, marking the experiments to expire after
** #forHowLong# seconds.  Connects #mem_c# if necessary.  Returns 1 if
** successful, 0 otherwise.
*/
int
StoreExperiments(struct host_cookie *mem_c,
                 const char *id,
                 const Experiment *exper,
                 size_t count,
                 double forHowLong);


#endif
