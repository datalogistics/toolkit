/* $Id: nws_search.c,v 1.30 2000/09/26 03:29:32 swany Exp $ */

#include <stdio.h>       /* printf() */
#include <stdlib.h>      /* atoi() */
#include <string.h>      /* string functions */
#include <strings.h>     /* strncasecmp() on aix */
#include <unistd.h>      /* getopt() */
#define NWSAPI_SHORTNAMES
#include "config.h"
#include "nws_api.h"     /* NWS programming interface */
#include "diagnostic.h"  /* ABORT() */


/*
** This program allows command-line query of NWS registrations.  See the
** USERSGUIDE for a description of the command-line options.
*/


#define ACTIVITY_SHORTCUT "activities"
#define CLIQUE_SHORTCUT "cliques"
#define CONTROL_SHORTCUT "controls"
#define FORECASTER_SHORTCUT "forecasters"
#define HOST_SHORTCUT "hosts"
#define MEMORY_SHORTCUT "memories"
#define MEMORY_SHORTCUT2 "memory"
#define SENSOR_SHORTCUT "sensors"
#define SERIES_SHORTCUT "series"
#define SKILL_SHORTCUT "skills"
#define SWITCHES "af:N:v"


const char *FILTERS[] =
  {ACTIVITIES, CLIQUES, CONTROLS, FORECASTERS, HOSTS, MEMORIES,
   MEMORIES, SENSORS, SERIES, SKILLS};
const char *SHORTCUTS[] =
  {ACTIVITY_SHORTCUT, CLIQUE_SHORTCUT, CONTROL_SHORTCUT, FORECASTER_SHORTCUT,
   HOST_SHORTCUT, MEMORY_SHORTCUT, MEMORY_SHORTCUT2, SENSOR_SHORTCUT,
   SERIES_SHORTCUT, SKILL_SHORTCUT};


/*
** Returns the length of the longest attribute name in #obj#.
*/
int
LongestNameLength(const Object obj) {

  Attribute attr;
  int nameLen;
  int returnValue = 0;

  ForEachAttribute(obj, attr) {
    nameLen = strlen(AttributeName(attr));
    if(nameLen > returnValue)
      returnValue = nameLen;
  }

  return returnValue;

}


/*
** Prints to stdout the name and/or value of #attr#, depending on #whatToPrint#.
** If printing names, pads the name with blanks to #namePadding# characters.
*/
typedef enum {NAMES_AND_VALUES, NAMES_ONLY, VALUES_ONLY}
  AttributeCharacteristics;
static void
PrintAttribute(Attribute attr,
               AttributeCharacteristics whatToPrint,
               int namePadding) {

  const char *c;
  int i;
  int nameLen;

  if(whatToPrint != VALUES_ONLY)
    printf("%s", AttributeName(attr));
  if(whatToPrint == NAMES_AND_VALUES) {
    nameLen = strlen(AttributeName(attr));
    for(i = nameLen; i < namePadding; i++)
      printf(" ");
    printf(":");
  }
  if(whatToPrint != NAMES_ONLY) {
    for(c = AttributeValue(attr); *c != '\0'; c++) {
      putchar((*c == ' ') ? '_' : *c);
    }
  }

}


#define DEFAULT_NAME_SERVER_PORT "8090"

int debug;

int
main(int argc,
     char **argv) {

  extern char *optarg;
  extern int optind;
  Attribute attribute;
  int attributeArgsIndex;
  const char *filter;
  size_t filterLen;
  int formatted = 1;
  int i;
  int longestName;
  HostSpec nsSpec;
  Object object;
  ObjectSet filteredObjects;
  int opt;
  const char *USAGE = "nws_search [-av] [-f yes/no] [-N host] filter [attribute ...]";
  AttributeCharacteristics whatToPrint = NAMES_AND_VALUES;

  DirectDiagnostics(DIAGINFO, stdout);
  DirectDiagnostics(DIAGLOG, stdout);
  DirectDiagnostics(DIAGWARN, stderr);
  DirectDiagnostics(DIAGERROR, stderr);
  DirectDiagnostics(DIAGFATAL, stderr);

  while((opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'a':
      whatToPrint = NAMES_ONLY;
      break;

    case 'f':
      formatted = (*optarg == 'y') || (*optarg == 'Y');
      break;

    case 'N':
      nsSpec = *MakeHostSpec
        (optarg,
         atoi(EnvironmentValue("NAME_SERVER_PORT", DEFAULT_NAME_SERVER_PORT)));
      if(!UseNameServer(&nsSpec))
        ABORT2("Unable to contact name server %s:%d\n",
               nsSpec.machineName, nsSpec.machinePort);
      break;

    case 'v':
      whatToPrint = VALUES_ONLY;
      break;

    default:
      fprintf(stderr, "nws_search: unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if (optind >= argc) {
    fprintf(stderr, "%s\n", USAGE);
    exit(1);
  }

  filter = argv[optind];
  filterLen = strlen(filter);

  for(i = 0; i < (sizeof(SHORTCUTS) / sizeof(SHORTCUTS[0])); i++) {
    if(strncasecmp(filter, SHORTCUTS[i], filterLen) == 0) {
      filter = FILTERS[i];
      break;
    }
  }

  if(!GetObjects(filter, &filteredObjects))
    exit(1);
  attributeArgsIndex = optind + 1;

  ForEachObject(filteredObjects, object) {
    longestName = formatted ? LongestNameLength(object) : 0;
    if(attributeArgsIndex >= argc) {
      ForEachAttribute(object, attribute) {
        PrintAttribute(attribute, whatToPrint, longestName);
        printf(formatted ? "\n" : " ");
      }
    }
    else {
      for(i = attributeArgsIndex; i < argc; i++) {
        ForEachAttribute(object, attribute) {
          if(strcmp(argv[i], AttributeName(attribute)) == 0) {
            PrintAttribute(attribute, whatToPrint, longestName);
            printf(formatted ? "\n" : " ");
          }
        }
      }
    }
    printf("\n");
  }

  FreeObjectSet(&filteredObjects);
  return 0;

}
