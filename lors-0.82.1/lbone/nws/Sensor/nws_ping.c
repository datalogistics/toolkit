/* $Id: nws_ping.c,v 1.14 2000/09/26 03:29:32 swany Exp $ */

#include "config.h"
#include <ctype.h>         /* isdigit() */
#include <stdio.h>         /* {f,}printf() */
#include <stdlib.h>        /* atof() atoi() */
#include <string.h>        /* memcpy() */
#include <unistd.h>        /* sleep() */
#include "host_protocol.h" /* DefaultHostPort() HostDImage() HostDValue() */
#include "osutil.h"        /* CurrentTime() */
#include "skills.h"        /* Skill invocation */


/*
** This is a ping-like command line utility for testing bandwidth and latency
** to a list of machines which are running NWS sensors.  See the user's guide
** for a description of the command line parameters.
*/


#define DEFAULT_TIMEOUT 10.0
#define OPTION_COUNT 3

int debug;

int
main(int argc,
     char **argv) {

  const char *OPTION_NAMES[OPTION_COUNT] = {"size", "buffer", "message"};
  const unsigned int OPTION_DEFAULTS[OPTION_COUNT] = {64, 32, 16};

  const char *curr;
  unsigned int endTime;
  int hostIndex;
  int i;
  int j;
  int length;
  char options[255 + 1];
  unsigned int optionValues[OPTION_COUNT];
  unsigned int repeatPeriod;
  const SkillResult *results;
  unsigned short sensorPort;
  unsigned int *size;
  unsigned int startTime;
  double timeOut;
  struct host_desc targetHost;
  char targetOptions[255 + 1];
  const char* USAGE = 
    "nws_ping [-d] [-repeat seconds] [-size experiment,buffer,message] [-timeout seconds] host ...";

  debug = 0;
  memcpy(optionValues, OPTION_DEFAULTS, sizeof(OPTION_DEFAULTS));
  repeatPeriod = 0;
  sensorPort = DefaultHostPort(SENSOR_HOST);
  timeOut = DEFAULT_TIMEOUT;

  for(i = 1; i < argc; i++) {

    if(*argv[i] == '-') {

      if(++i == argc) {
        fprintf(stderr, "usage: %s\n", USAGE);
        exit(1);
      }

      switch(argv[i - 1][1]) {

      case 'r':
        repeatPeriod = atoi(argv[i]);
        break;

      case 's':
        /*
        ** Parse the comma-delimited size list, allowing each element to
        ** default if unspecified (e.g. "32", "10,,5" and ",,8" are all legal).
        */
        curr = argv[i];
        memcpy(optionValues, OPTION_DEFAULTS, sizeof(OPTION_DEFAULTS));
        for(size = optionValues;
            size < &optionValues[OPTION_COUNT];
            size++) {
          if(isdigit((int)*curr)) {
            *size = 0;
            for( ; isdigit((int)*curr); curr++)
              *size = *size * 10 + *curr - '0';
            if(*curr == ',')
              curr++;
          }
          else if(*curr == ',') {
            curr++;
          }
        }
        break;

      case 't':
        timeOut = atof(argv[i]);
        break;

      case 'd':
	debug = 1;
	break;

      default:
        fprintf(stderr, "usage: %s\n", USAGE);
        exit(1);
        break;

      }

    }
    else {

      break;

    }

  }

  if(i >= argc) {
    fprintf(stderr, "usage: %s\n", USAGE);
    exit(1);
  }

  options[0] = '\0';
  for(j = 0; j < OPTION_COUNT; j++)
    sprintf(options, "%s\t%s:%d", options, OPTION_NAMES[j], optionValues[j]);

  while(1) {

    startTime = (unsigned int)CurrentTime();

    for(hostIndex = i; hostIndex < argc; hostIndex++) {

      HostDValue(argv[hostIndex], sensorPort, &targetHost);

      printf("(%dk", optionValues[0]);
      for(j = 1; j < OPTION_COUNT; j++) {
        printf(",%dk", optionValues[j]);
      }
      printf(") to %s:", HostDImage(&targetHost));

      sprintf(targetOptions, "%s\ttarget:%s", options, HostDImage(&targetHost));
      UseSkill(tcpMessageMonitor, targetOptions, timeOut, &results, &length);

      if(results[0].succeeded) {
        for(j = 0; j < length; j++)
          printf(" %s: %f %s",
                 ResourceName(results[j].resource),
                 results[j].measurement,
                 ResourceLabel(results[j].resource));
      }
      else
        printf(" failed");
      printf("\n");


    }

    fflush(stdout);

    if(repeatPeriod == 0)
      break;
 
    endTime = (unsigned int)CurrentTime();
    if((endTime - startTime) < repeatPeriod)
      sleep(repeatPeriod - (endTime - startTime));

  }

  return 0;

}
