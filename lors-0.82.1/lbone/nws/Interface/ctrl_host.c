/* $Id: ctrl_host.c,v 1.19 2000/09/26 03:29:31 swany Exp $ */

#include <stdio.h>          /* fprintf() */
#include <stdlib.h>         /* atof() */
#include <string.h>         /* strlen() */
#include <strings.h>        /* strncasecmp() */
#include <unistd.h>         /* getopt() */
#include "config.h"
#include "diagnostic.h"     /* informational message functions */
#include "messages.h"       /* Recv*() Send*() */
#include "host_protocol.h"  /* host connections */
#include "protocol.h"       /* Socket */
#include "strutil.h"        /* SAFESTRCPY */


/*
** This program allows command-line control of running NWS hosts.  See the
** USERSGUIDE for a description of the command-line options.
*/


/*
** Implements the "error" and "log" commands by sending a HOST_DIAGNOSTICS
** message, accompanied by #whichDiagnostics#, to the host connected to #sd#.
** Returns 1 if successful within #timeOut# seconds, else 0.
*/
static int
DoDiagnosticsCommand(Socket sd,
                     int whichDiagnostics,
                     double timeOut) {
  size_t ignored;
  DataDescriptor intDescriptor = SIMPLE_DATA(INT_TYPE, 1);
  return(SendMessageAndData(sd,
                            HOST_DIAGNOSTICS,
                            &whichDiagnostics,
                            &intDescriptor,
                            1,
                            timeOut) &&
         RecvMessage(sd, HOST_DIAGNOSTICS_ACK, &ignored, timeOut));
}


/*
** Implements the "halt" command by sending a HOST_DIE message, accompanied by
** #password#, to the host connected to #sd#.  Returns 1 if successful within
** #timeOut# seconds, else 0.
*/
static int
DoHaltHost(Socket sd,
           const char *password,
           double timeOut) {

  DataDescriptor passwordDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  size_t replySize;

  passwordDescriptor.repetitions =
    (password == NULL) ? 0 : strlen(password) + 1;

  if(!SendMessageAndData(sd,
                         HOST_DIE,
                         password,
                         &passwordDescriptor,
                         1,
                         timeOut)) {
    FAIL("DoHaltHost: die message send failed\n");
  }

  if(!RecvMessage(sd, HOST_DYING, &replySize, timeOut)) {
    FAIL("DoHaltHost: die message acknowlegement failed\n");
  }

  return(1);

}


/*
** Implements the "test" command by sending a HOST_TEST message to the host
** connected to #sd#.  Returns the computed health of the host.
*/
typedef enum {DEAD, HEALTHY, SICK, UNRESPONSIVE} HostStatii;
static HostStatii
DoTestHost(Socket sd,
           HostInfo *returnedInfo,
           double timeOut) {
  size_t recvSize;
  if(!SendMessage(sd, HOST_TEST, timeOut)) {
    return(DEAD);
  }
  return( (!RecvMessage(sd, HOST_TEST_RESULT, &recvSize, timeOut) ||
           !RecvData(sd,
                     returnedInfo,
                     hostInfoDescriptor,
                     hostInfoDescriptorLength,
                     timeOut)) ?
    UNRESPONSIVE : (returnedInfo->healthy ? HEALTHY : SICK) );
}


/*
** Implements the "register" command by sending a HOST_REGISTER message,
** accompanied by #password#, to the host connected to #sd#, telling it to
** change its registration to the name server #nsName#.  Returns 1 if
** successful within #timeOut# seconds, else 0.
*/
static int
DoRegisterHost(Socket sd,
               const char *nsName,
               const char *password,
               double timeOut) {
  size_t ignored;
  DataDescriptor nameDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  nameDescriptor.repetitions = strlen(nsName) + 1;
  return(SendMessageAndData(sd,
                            HOST_REGISTER,
                            nsName,
                            &nameDescriptor,
                            1,
                            timeOut) &&
         RecvMessage(sd, HOST_REGISTERED, &ignored, timeOut));
}


/*
** Returns 1 or 0 depending on whether or not the host described by #host# can
** be verified to be name server within #timeOut# seconds.
*/
static int
IsANameServer(const struct host_desc *host,
              double timeOut) {

  struct host_cookie hostCookie;
  HostInfo hostInfo;
  char hostName[127 + 1];

  SAFESTRCPY(hostName, HostDImage(host));
  MakeHostCookie(host->host_name, host->port, &hostCookie);

  if(!ConnectToHost(&hostCookie, &hostCookie.sd)) {
    FAIL1("IsANameServer: unable to contact %s\n", hostName);
  }

  if(DoTestHost(hostCookie.sd, &hostInfo, timeOut) != HEALTHY) {
    DisconnectHost(&hostCookie);
    FAIL1("IsANameServer: unable to verify that %s is a name server\n",
          hostName);
  }

  DisconnectHost(&hostCookie);

  if(strcmp(hostName, hostInfo.nameServer) != 0) {
    FAIL1("IsANameServer: %s is not a name server\n", hostName);
  }

  return(1);

}


#define SWITCHES "p:t:z"

int debug;

int
main(int argc,
     char *argv[]) {

  typedef enum {ERROR, HALT, LOG, REGISTER, TEST} Commands;
  const char *COMMANDS[] = {"error", "halt", "log", "register", "test"};
  const char *USAGE = "ctrl_host [-P password] [-t n] [-z] <command> host ...";

  int c;
  Commands command;
  struct host_cookie cookie;
  struct host_desc host;
  HostInfo info;
  struct host_desc nsDesc;
  unsigned short nsPort;
  const char *parameter = NULL;
  const char *password;
  const char *STATII[] = {"dead", "healthy", "sick", "unresponsive"};
  unsigned short sensorPort;
  HostStatii status;
  const char *TYPES[] = {"forecaster", "memory", "name_server", "sensor"};
  double timeOut;
  extern char *optarg;
  extern int optind;

  password = NULL;
  timeOut = PKTTIMEOUT;

  DirectDiagnostics(DIAGINFO, stdout);
  DirectDiagnostics(DIAGLOG, stdout);
  DirectDiagnostics(DIAGWARN, stderr);
  DirectDiagnostics(DIAGERROR, stderr);
  DirectDiagnostics(DIAGFATAL, stderr);

  nsPort = DefaultHostPort(NAME_SERVER_HOST);
  sensorPort = DefaultHostPort(SENSOR_HOST);

  while((c = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(c) {

    case 'P':
      password = optarg;
      break;

    case 't':
      timeOut = atof(optarg);
      break;

    case 'z':
      DirectDiagnostics(DIAGINFO, DIAGSUPPRESS);
      DirectDiagnostics(DIAGLOG, DIAGSUPPRESS);
      DirectDiagnostics(DIAGWARN, DIAGSUPPRESS);
      DirectDiagnostics(DIAGERROR, DIAGSUPPRESS);
      DirectDiagnostics(DIAGFATAL, DIAGSUPPRESS);
      break;

    default:
      fprintf(stderr, "unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if(optind >= argc) {
    fprintf(stderr, "%s\n", USAGE);
    exit(1);
  }

  for(command = ERROR; command <= TEST; command++) {
    if(strncasecmp(COMMANDS[command], argv[optind], strlen(argv[optind])) == 0)
      break;
  }

  if(command > TEST) {
    fprintf(stderr, "unknown command %s\n", argv[optind]);
    exit(1);
  }

  optind++;

  switch(command) {

  case REGISTER:
    if(optind >= argc) {
      fprintf(stderr, "missing parameter for %c command\n", command);
      exit(1);
    }
    parameter = argv[optind++];
    HostDValue(parameter, nsPort, &nsDesc);
    if(!IsANameServer(&nsDesc, timeOut)) {
      fprintf(stderr, "%s is not a name server\n", parameter);
    }
    break;

  default:
    break;

  }

  for(; optind < argc; optind++) {

    HostDValue(argv[optind], sensorPort, &host);
    MakeHostCookie(host.host_name, host.port, &cookie);

    if(!ConnectToHost(&cookie, &cookie.sd)) {
      if(command == TEST) {
        printf("%s dead\n", HostDImage(&host));
      }
      else {
        WARN1("connect %s failed\n", HostDImage(&host));
      }
      continue;
    }

    switch(command) {

    case ERROR:
      if(!DoDiagnosticsCommand(cookie.sd, ALL_ERRORS, timeOut)) {
        WARN1("error toggle failed for host %s\n", HostDImage(&host));
      }
      break;

    case HALT:
      if(!DoHaltHost(cookie.sd, password, timeOut)) {
        WARN1("halt failed for host %s\n", HostDImage(&host));
      }
      break;

    case LOG:
      if(!DoDiagnosticsCommand(cookie.sd, ALL_LOGS, timeOut)) {
        WARN1("log toggle failed for host %s\n", HostDImage(&host));
      }
      break;

    case REGISTER:
      if(!DoRegisterHost(cookie.sd, parameter, password, timeOut)) {
        WARN1("registration change failed for host %s\n", HostDImage(&host));
      }
      break;

    case TEST:
      status = DoTestHost(cookie.sd, &info, timeOut);
      if((status == HEALTHY) || (status == SICK)) {
        printf("%s %s ", TYPES[info.hostType], HostDImage(&host));
        if((strcmp(info.registrationName, HostDImage(&host)) != 0) &&
           (strcmp(info.registrationName, "") != 0)) {
          printf("(%s) ", info.registrationName);
        }
        printf("registered with %s ", info.nameServer);
      }
      else {
        printf("%s ", HostDImage(&host));
      }
      printf("%s\n", STATII[status]);
      break;

    }

    DisconnectHost(&cookie);

  }

  return(0);

}
