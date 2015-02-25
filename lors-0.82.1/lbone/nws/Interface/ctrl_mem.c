#include <stdio.h>          /* fprintf() */
#include <stdlib.h>         /* atof() */
#include <string.h>         /* strlen() */
#include <strings.h>        /* strncasecmp() */
#include <unistd.h>         /* getopt() */
#include "diagnostic.h"     /* informational message functions */
#include "messages.h"       /* Recv*() Send*() */
#include "host_protocol.h"  /* host connections */
#include "protocol.h"       /* Socket */
#include "strutil.h"        /* SAFESTRCPY */
#include "nws_memory.h"		/* memory messages */

/*
** This program allows command-line control of running NWS memory.  See the
** USERSGUIDE for a description of the command-line options.
*/

int DoStopLogging(Socket sd, const char* phost, double timeOut)
{ 
  size_t ignored;
  struct loglocation nsMemLog;
  nsMemLog.loc_type = MEMORY_LOG_NONE;
  return(SendMessageAndData(sd,
                            MEMORY_LOGDEST,
                            &nsMemLog,
                            loglocationDescriptor,
                            loglocationDescriptorLength,
                            timeOut) &&
         RecvMessage(sd, MEMORY_LOGDEST_ACK, &ignored, timeOut));

  
}

int DoLogLocal(Socket sd, const char* phost, double timeOut)
{ 
  size_t ignored;
  struct loglocation nsMemLog;
  nsMemLog.loc_type = MEMORY_LOG_LOCAL;
  SAFESTRCPY(nsMemLog.path,phost);
  return(SendMessageAndData(sd,
                            MEMORY_LOGDEST,
                            &nsMemLog,
                            loglocationDescriptor,
                            loglocationDescriptorLength,
                            timeOut) &&
         RecvMessage(sd, MEMORY_LOGDEST_ACK, &ignored, timeOut));

  
}

int DoLogNetLogger(Socket sd, const char* phost, double timeOut)
{ 
  size_t ignored;
  struct loglocation nsMemLog;
  nsMemLog.loc_type = MEMORY_LOG_NETLOGGER;
  SAFESTRCPY(nsMemLog.path,phost);
  return(SendMessageAndData(sd,
                            MEMORY_LOGDEST,
                            &nsMemLog,
                            loglocationDescriptor,
                            loglocationDescriptorLength,
                            timeOut) &&
         RecvMessage(sd, MEMORY_LOGDEST_ACK, &ignored, timeOut));

  
}



/* switches for nws_memory control: */
/*  l: log specifier */
/*  f: file to log to, could be host:port or file or host:file */
/*  command: stop, start logging */
/*  if none is specified nothing is done */
#define SWITCHES "l:f:z"

int debug;

int
main(int argc,
     char *argv[]) {

  typedef enum {STOP,START} Commands;
  
  const char *LOGDEST[]={"none","local","netlogger","remote"};
  const char *USAGE = "ctrl_mem [-l howtolog -f wheretolog] host";

  MemLogDest logdest = MEMORY_LOG_NONE;
  const char* plogdest = NULL;
  int c;
  /* Commands command; */
  struct host_cookie cookie;
  struct host_desc host;
  unsigned short nsPort;
  const char *parameter = NULL;
  extern char *optarg;
  extern int optind;
  
  unsigned short sensorPort;
  double timeOut;

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

    case 'l':
		for(logdest = MEMORY_LOG_NONE; logdest <= MEMORY_LOG_REMOTE; logdest++) {
			if(strncasecmp(LOGDEST[logdest], optarg, strlen(optarg)) == 0)
				break;
		}
		break;

    case 'f':
      plogdest = optarg;
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

  parameter = argv[optind];
  optind++;

  HostDValue(parameter, nsPort, &host);
  
  MakeHostCookie(host.host_name, host.port, &cookie);

  
  if(!ConnectToHost(&cookie, &cookie.sd)) {
      
       WARN1("connect %s failed\n", HostDImage(&host));
	   return 1;
  }
  
    switch(logdest) {
	case MEMORY_LOG_NONE: /* stop logging */
		
		DoStopLogging(cookie.sd, plogdest, timeOut);
		printf("4\n");
		break;
	case MEMORY_LOG_LOCAL: /* set local file */
		DoLogLocal(cookie.sd, plogdest, timeOut);
		break;
	case MEMORY_LOG_NETLOGGER: /* set netlogger  */
		DoLogNetLogger(cookie.sd, plogdest, timeOut);
		break;
	case MEMORY_LOG_REMOTE: /* set remote file */
		/* DoLogRemote(cookie.sd, plogdest, timeOut); */
		break;
	}

  DisconnectHost(&cookie);
  return(0);

}

