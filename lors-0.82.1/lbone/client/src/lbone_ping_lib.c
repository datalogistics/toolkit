/* $Id: lbone_ping_lib.c,v 1.2 2002/06/04 16:26:52 soltesz Exp $ */

#include "config.h"
#include <ctype.h>         /* isdigit() */
#include <stdio.h>         /* {f,}printf() */
#include <stdlib.h>        /* atof() atoi() */
#include <string.h>        /* memcpy() */
#include <unistd.h>        /* sleep() */
#include "lbone_ping_lib.h"


/*
** This is a ping-like command line utility for testing bandwidth and latency
** to a list of machines which are running NWS sensors.
*/


#define DEFAULT_TIMEOUT 10.0
#define OPTION_COUNT 3

int debug;

#if 0
int
nws_ping(char *hostname, int port, double *bandwidth, double *latency) {

  const char *OPTION_NAMES[OPTION_COUNT] = {"size", "buffer", "message"};
  const unsigned int OPTION_DEFAULTS[OPTION_COUNT] = {256, 64, 32};

  int j;
  int length;
  char options[255 + 1];
  unsigned int optionValues[OPTION_COUNT];
  SkillResult *results;
  unsigned short sensorPort;
  double timeOut;
  struct host_desc targetHost;
  char targetOptions[255 + 1];
  char host_and_port[256];

  debug = 0;
  memcpy(optionValues, OPTION_DEFAULTS, sizeof(OPTION_DEFAULTS));
  sensorPort = (unsigned short) port;
  timeOut = DEFAULT_TIMEOUT;


  options[0] = '\0';
  for(j = 0; j < OPTION_COUNT; j++)
    sprintf(options, "%s\t%s:%d", options, OPTION_NAMES[j], optionValues[j]);

  if (strlen(hostname) < 64) {
    memset(&targetHost, 0, sizeof(struct host_desc));
    strcpy(targetHost.host_name, hostname);
  } else {
    return -1;
  }

  memset(host_and_port, 0, 256);
  sprintf(host_and_port, "%s:%d", hostname, sensorPort);
  sprintf(targetOptions, "%s\ttarget:%s", options, host_and_port);
  UseSkill(tcpMessageMonitor, targetOptions, timeOut, &results, &length);

  if(results[0].succeeded) {
    *bandwidth = results[0].measurement;
    *latency = results[1].measurement;
    return 0;
  }
  else {
    *bandwidth = 0;
    *latency = 0;
  }

  return 0;
}
#endif
