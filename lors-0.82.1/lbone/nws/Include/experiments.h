/* $Id: experiments.h,v 1.17 2001/01/11 03:09:07 rich Exp $ */

#ifndef EXPERIMENTS_H
#define EXPERIMENTS_H


#include "hosts.h" /* MAX_HOST_NAME */

/* Sizes of fields in the structs below.  Lists are tab-delimited. */
#define EXP_LABEL_SIZE (31 + 1)
#define EXP_LIST_SIZE (255 + 1)
#define EXP_NAME_SIZE (31 + 1)
#define HOST_NAME_SIZE (MAX_HOST_NAME + 1)

#define WEXP_FORMAT "%11.0f %20.5f"


/*
** An "activity" is a skill being exercised under a control using a certain set
** of option values.  #controlName# and #skillName# identify the control and
** skill involved.  #host# is the host in charge of the activity.  #options# is
** the union of the control and skill options, with specific values substituted
** for the format strings.  Note that, since the number of option values may be
** very large, the #options# field is malloced memory that must be freed by the
** client code after use.  This also implies that Activity records cannot
** (easily) be transmitted as part of a message.  #resources# holds the list of
** resources being measured by the activity.
*/
typedef struct {
  char controlName[EXP_NAME_SIZE];
  char host[HOST_NAME_SIZE];
  char *options;
  char resources[EXP_LIST_SIZE];
  char skillName[EXP_NAME_SIZE];
} Activity;


/*
** A "control" is a protocol that governs the conduct of experiments -- how
** frequently and in what order measurements are taken.  #controlName# is the
** unique designator for this control.  #host# indicates the host that supports
** the control.  #skills# holds the list of skill names that can be run via
** this control.  #options# holds a list of configuration options that this
** control supports, each followed by a colon and a string that indicates the
** format of values for this option.  This string has the format
** "<min>_to_<max>_<type>", where <min> and <max> are integers and <type> is
** one of "float", "forecaster", "int", "memory", "nameserver", "sensor", or
** "string".
*/
typedef struct {
  char controlName[EXP_NAME_SIZE];
  char host[HOST_NAME_SIZE];
  char skills[EXP_LIST_SIZE];
  char options[EXP_LIST_SIZE];
} Control;


/*
** An "experiment" is a single test of resource availability.  #timeStamp# is
** the time (taken from time()) that the experiment was taken, #value# the
** measured availability at that time.
*/
typedef struct {
  double timeStamp;
  double value;
} Experiment;


/*
** A "series" is a set of experiments.  Every activity generates one or more of
** these.  #host# indicates the sensor generating the experiments.  #label# is
** text describing the meaning of the series measurements that can be used,
** e.g., for graph display.  #memory# is the memory in which the series is
** stored.  #options# are specific parameters applied when taking measurements.
** #resource# is the type of resource being measured -- bandwidth, latency,
** available cpu, etc.
*/
typedef struct {
  char host[HOST_NAME_SIZE];
  char label[EXP_LABEL_SIZE];
  char memory[HOST_NAME_SIZE];
  char options[EXP_LIST_SIZE];
  char resource[EXP_NAME_SIZE];
} Series;


/*
** A "skill" is a service that a host can provide.  #host# indicates which host
** can provide the service.  #options# is a list of configuration options that
** this skill supports, in the same format as the options field of the Control
** struct.  #skillName# is the unique designator of the skill.
*/
typedef struct {
  char host[HOST_NAME_SIZE];
  char options[EXP_LIST_SIZE];
  char skillName[EXP_NAME_SIZE];
} Skill;


#endif
