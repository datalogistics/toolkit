/* $Id: halt_activity.c,v 1.8 2000/09/29 21:36:11 swany Exp $ */

#include <stdio.h>   /* fprintf() sprintf() */
#include <stdlib.h>  /* atoi() */
#include <string.h>  /* strcmp() */
#include <unistd.h>  /* getopt() */
#define NWSAPI_SHORTNAMES
#include "config.h"
#include "nws_api.h" /* Interface functions */


/*
** This program allows the user to terminate NWS activities from the command
** line.  See the USERSGUIDE for a description of the command-line options.
*/


/*
** Returns the value of the #attributeName# attribute from #object#, or an
** empty string if no such attribute exists.
*/
static const char *
GetAttributeValue(const Object object,
                  const char *attributeName) {
  Attribute attribute;
  ForEachAttribute(object, attribute) {
    if(strcmp(AttributeName(attribute), attributeName) == 0)
      return AttributeValue(attribute);
  }
  return "";
}


#define DEFAULT_NAME_SERVER_PORT "8090"
#define DEFAULT_SENSOR_PORT "8060"
#define SWITCHES "N:S:"

int debug;

int
main(int argc,
     char *argv[]) {

  extern char *optarg;
  extern int optind;
  ObjectSet activities;
  Object activity;
  char filter[255 + 1];
  char name[255 + 1];
  HostSpec nameServer = {"", 0};
  int opt;
  HostSpec sensor = {"", 0};
  const char *USAGE = "halt_activity [-N host] [-S host] name";

  while((opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'N':
      nameServer = *MakeHostSpec
        (optarg,
         atoi(EnvironmentValue("NAME_SERVER_PORT", DEFAULT_NAME_SERVER_PORT)));
      break;

    case 'S':
      sensor = *MakeHostSpec
        (optarg,
         atoi(EnvironmentValue("SENSOR_PORT", DEFAULT_SENSOR_PORT)));
      break;

    default:
      fprintf(stderr, "Unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if(optind >= argc) {
    fprintf(stderr, "%s\n", USAGE);
    exit(1);
  }

  if(sensor.machinePort == 0) {

    /*
    ** Contact the name server to resolve the user-specified name (embedded in
    ** wildcards) into a name server registration name.
    */

    if((nameServer.machinePort != 0) && !UseNameServer(&nameServer)) {
      fprintf(stderr, "Unable to contact name server %s:%d\n",
              nameServer.machineName, nameServer.machinePort);
      exit(1);
    }

    /* Retrieve all registered activities matching the user's specifications. */
    sprintf(filter, "(&%s(%s=*%s*))", ACTIVITIES, NAME_ATTR, argv[optind]);

    if(!GetObjects(filter, &activities)) {
      fprintf(stderr, "Unable to retrieve registrations from name server\n");
      exit(1);
    }

    activity = FirstObject(activities);

    if(activity == NULL) {
      fprintf(stderr,
              "No activity matching '%s' is registered with the name server\n",
              argv[optind]);
      FreeObjectSet(&activities);
      exit(1);
    }
    else if(NextObject(activities, activity) != NULL) {
      fprintf(stderr,
              "Multiple activities matching '%s' are registered with the name server\n",
              argv[optind]);
      ForEachObject(activities, activity)
        fprintf(stderr, "  %s\n", GetAttributeValue(activity, NAME_ATTR));
      FreeObjectSet(&activities);
      exit(1);
    }

    sensor = *MakeHostSpec
      (GetAttributeValue(activity, HOST_ATTR),
       atoi(EnvironmentValue("SENSOR_PORT", DEFAULT_SENSOR_PORT)));
    strcpy(name, GetAttributeValue(activity, NAME_ATTR));
    FreeObjectSet(&activities);

  }
  else {

    /* Use the user's name and sensor specification directly. */
    strcpy(name, argv[optind]);

  }

  if(!HaltActivity(&sensor, name)) {
    fprintf(stderr, "Unable to halt activity\n");
    /* FreeObjectSet(&activities); */
    exit(1);
  }

  return 0;

}
