/* $Id: nws_insert.c,v 1.30 2000/09/26 03:29:32 swany Exp $ */

#include <stdio.h>       /* file functions */
#include <stdlib.h>      /* atoi() */
#include <string.h>      /* strncasecmp() */
#include <strings.h>     /* strncasecmp() on aix */
#include <unistd.h>      /* getopt() */
#include "config.h"
#include "diagnostic.h"  /* ABORT() */
#define NWSAPI_SHORTNAMES
#include "nws_api.h"     /* NWS programming interface */


/*
** This program allows users to store measurements in NWS memories from the
** command line.  See the USERSGUIDE for a description of the command-line
** options.
*/


#define SAFE_STRCPY(to, from) \
  do {strncpy(to, from, sizeof(to)); to[sizeof(to) - 1] = '\0'; if (1) break;} while (0)


#define LINE_LENGTH 80
#define MAX_MEASUREMENTS 2000
#define SWITCHES "f:M:"
#define DEFAULT_MEMORY_PORT "8050"

int debug;

int
main(int argc,
     char **argv) {

  extern char *optarg;
  extern int optind;
  FILE *inputFile = stdin;
  char inputLine[LINE_LENGTH];
  Measurement measurements[MAX_MEASUREMENTS];
  size_t lineCount;
  size_t measurementCount;
  HostSpec memorySpec;
  int opt;
  int optlen;
  SeriesSpec series;
  const char *USAGE = "nws_insert [-f file] [-M host] resource host [host]";

  DirectDiagnostics(DIAGINFO, stdout);
  DirectDiagnostics(DIAGLOG, stdout);
  DirectDiagnostics(DIAGWARN, stderr);
  DirectDiagnostics(DIAGERROR, stderr);
  DirectDiagnostics(DIAGFATAL, stderr);

  while((opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'f':
      if(inputFile != stdin)
        fclose(inputFile);
      inputFile = fopen(optarg, "r");
      if(inputFile == NULL)
        ABORT1("Unable to open file %s\n", optarg);
      break;

    case 'M':
      memorySpec = *MakeHostSpec
        (optarg, atoi(EnvironmentValue("MEMORY_PORT", DEFAULT_MEMORY_PORT)));
      if(!UseMemory(&memorySpec))
        ABORT2("Unable to contact memory %s:%d\n",
               memorySpec.machineName, memorySpec.machinePort);
      break;

    default:
      fprintf(stderr, "nws_insert: unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if((optind + 1) >= argc) {
    fprintf(stderr, "%s\n", USAGE);
    exit(1);
  }

  optarg = argv[optind];
  optlen = strlen(optarg);
  SAFE_STRCPY(series.resourceName,
    (strncasecmp(optarg, DEFAULT_BANDWIDTH_RESOURCE, optlen) == 0) ?
       DEFAULT_BANDWIDTH_RESOURCE :
    (strncasecmp(optarg, DEFAULT_AVAILABLE_CPU_RESOURCE, optlen) == 0)  ?
       DEFAULT_AVAILABLE_CPU_RESOURCE :
    (strncasecmp(optarg, DEFAULT_CURRENT_CPU_RESOURCE, optlen) == 0)   ?
       DEFAULT_CURRENT_CPU_RESOURCE :
    (strncasecmp(optarg, DEFAULT_LATENCY_RESOURCE, optlen) == 0)   ?
       DEFAULT_LATENCY_RESOURCE :
    (strncasecmp(optarg, DEFAULT_MEMORY_RESOURCE, optlen) == 0)   ?
       DEFAULT_MEMORY_RESOURCE :
    optarg);
  SAFE_STRCPY(series.sourceMachine, argv[optind + 1]);

  if((optind + 2) < argc)
    SAFE_STRCPY(series.destinationMachine, argv[optind + 2]);
  else if(IntermachineResource(series.resourceName))
    ABORT("You must specify a destination machine\n");
  else
    SAFE_STRCPY(series.destinationMachine, "");

  lineCount = 0;
  measurementCount = 0;
  while(fgets(inputLine, sizeof(inputLine), inputFile) != NULL) {
    if( (strcmp(inputLine, "\n") != 0) &&
        (inputLine[0] != '#') ) {
      if(sscanf(inputLine, "%lf%lf",
                &measurements[measurementCount].timeStamp,
                &measurements[measurementCount].measurement) != 2)
        ABORT2("Bad format in line %d: %s\n", lineCount, inputLine);
      measurementCount++;
    }
    lineCount++;
  }

  if(inputFile != stdin)
    (void)fclose(inputFile);

  if(!PutMeasurements(SeriesName(&series),
                      &series,
                      measurements,
                      measurementCount))
    ABORT("Attempt to store experiments failed\n");

  exit(0);

}
