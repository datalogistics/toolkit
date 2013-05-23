/* $Id: nws_sensor.c,v 1.111 2000/10/04 19:05:40 swany Exp $ */

#include "config.h"
#include <unistd.h>           /* getopt() */
#include <signal.h>           /* signal() */
#include <sys/wait.h>         /* waitpid() */
#include <string.h>           /* strcmp() strlen() */
#include <stdlib.h>           /* atoi() free() malloc() */
#include "dnsutil.h"          /* MyMachineName() */
#include "clique_protocol.h"  /* clique-based control */
#include "host_protocol.h"    /* Host connection functions */
#include "diagnostic.h"       /* ABORT() FAIL() WARN() */
#include "messages.h"         /* Recv*() Send*() */
#include "osutil.h"           /* various os-related utilities */
#include "periodic.h"         /* period-based control */
#include "skills.h"           /* skill invocation */
#include "strutil.h"          /* SAFESTRCPY() vstrncpy() */
#include "nws_sensor.h"


/*
** Attempts to contact #nameServer# to draw an IP address and port number from
** the registration for #hostName#.  If successful, returns 1 and places the
** address and port into #hostDesc#; else returns 0.
*/
static int
LookupHostDesc(struct host_cookie *nameServer,
               const char *hostName,
               struct host_desc *hostDesc) {

  const char *c;
  struct host_cookie hostCookie = {"", 0, NO_SOCKET};
  Object hostObject;

  if(!Lookup(nameServer, hostName, &hostObject))
    return 0;

  /* Take the port and an address we can connect to from the registration. */
  hostCookie.port = atoi(AttributeValue(FindAttribute(hostObject, "port")));
  for(c = AttributeValue(FindAttribute(hostObject, "ipAddress"));
      GETTOK(hostCookie.name, c, ",", &c);
      ) {
    if(ConnectToHost(&hostCookie, &hostCookie.sd))
      break;
  }
  FreeObject(&hostObject);

  if(hostCookie.sd == NO_SOCKET)
    return 0;

  DisconnectHost(&hostCookie);
  SAFESTRCPY(hostDesc->host_name, hostCookie.name);
  hostDesc->port = hostCookie.port;
  return 1;

}


/*
** Attempts to contact #nameServer# to retrieve a registration for a host
** responding to messages to #address#:#port#.  If successful, stores the
** host's registration name in the #hostNameLen#-long string #hostName# and
** returns 1; else returns 0.
*/
static int
LookupHostName(struct host_cookie *nameServer,
               IPAddress address,
               unsigned short port,
               char *hostName,
               size_t hostNameLen) {
  char filter[127 + 1];
  Object hostObject = NO_OBJECT;
  ObjectSet hostSet;
  sprintf(filter,"(&(port=%d)(ipAddress=*%s*))",port,IPAddressImage(address));
  if(RetrieveObjects(nameServer, filter, &hostSet) &&
     (hostObject = NextObject(hostSet, NO_OBJECT)) != NO_OBJECT) {
    zstrncpy(hostName,
             AttributeValue(FindAttribute(hostObject, "name")),
             hostNameLen);
             
  }
  FreeObjectSet(&hostSet);
  return hostObject != NO_OBJECT;
}


/*
** A signal handler which allows the O/S to scavenge zombie children.
*/
static void
Reap(int sig) {
  pid_t pid;
  HoldSignal(SIGCHLD);
  while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
    CliqueChildDeath((int)pid);
    PeriodicChildDeath((int)pid);
  }
  ReleaseSignal(SIGCHLD);
}


/*
** Takes steps necessary to shut down the sensor.  Returns 1 if successful,
** else 0.
*/
static int
SensorExit(void) {
#ifdef HAVE_GETPGID
  return (getpgid(0) == getpid()) ?
         (StopCliqueActivity("") && StopPeriodicActivity("")) : 1;
#else
  return (getpgrp() == getpid()) ?
		(StopCliqueActivity("") && StopPeriodicActivity("")) : 1;
#endif
}


/*
** A "local" function of ProcessRequest().  Uses #control# to dispatch an
** activity start request configured by #registration#, #skill#, and #options#
** to the proper control module.  Returns 1 if successful, else 0.
*/
static int
StartActivity(const char *control,
              const char *registration,
              const char *skill,
              const char *options) {
  return (strcmp(control, CLIQUE_CONTROL_NAME) == 0) ?
         StartCliqueActivity(registration, skill, options) :
         (strcmp(control, PERIODIC_CONTROL_NAME) == 0) ?
         StartPeriodicActivity(registration, skill, options) :
         0;
}


/*
** A "local" function of ProcessRequest().  Queries the different control
** modules to try to determine the registrant of #activity#, and, when the
** proper module is found, stops the activity.
*/
static int
StopActivity(const char *activity) {
/*   return RecognizedCliqueActivity(activity) ? */
/*          StopCliqueActivity(activity) : */
/*          RecognizedPeriodicActivity(activity) ? */
/*          StopPeriodicActivity(activity) : */
/*          0; */

  char filter_base[512] = "name:";
  char *filter;

  if(RecognizedCliqueActivity(activity)) {
	StopCliqueActivity(activity);
	return(1);
  }
  else if(RecognizedPeriodicActivity(activity)) {
	StopPeriodicActivity(activity);
	return(1);
  }
  else {
 	filter = strncat((char *)filter_base, activity, 506); 
	printf("Calling UnregisterObject(%s)\n", filter);
    UnregisterObject(activity);
	return(0);
  }
		  
}


/*
** A "local" function of main().  Handles a #messageType# message arrived on
** #sd# accompanied by #dataSize# bytes of data.
*/
static void
ProcessRequest(Socket *sd,
               MessageType messageType,
               size_t dataSize) {

  DataDescriptor actDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  char *actControl;
  char *actInfo;
  char *actOptions;
  char *actRegistration;
  char *actSkill;

  switch(messageType) {

  case ACTIVITY_START:
    actInfo = (char *)malloc(dataSize);
    if(actInfo == NULL) {
      ERROR("ProcessRequest: out of memory\n");
    }
    else {
      actDescriptor.repetitions = dataSize;
      if(!RecvData(*sd,
                   actInfo,
                   &actDescriptor,
                   1,
                   PktTimeOut(*sd))) {
        ERROR("ProcessRequest: data receive failed\n");
      }
      else {
        actRegistration = actInfo;
        actControl = actRegistration + strlen(actRegistration) + 1;
        actSkill = actControl + strlen(actControl) + 1;
        actOptions = actSkill + strlen(actSkill) + 1;
        (void)SendMessage
          (*sd,
           StartActivity(actControl, actRegistration, actSkill, actOptions) ?
           ACTIVITY_STARTED : SENSOR_FAILED,
           PktTimeOut(*sd));
      }
      free(actInfo);
    }
    break;

  case ACTIVITY_STOP:
    actInfo = (char *)malloc(dataSize);
    if(actInfo == NULL) {
      ERROR("ProcessRequest: out of memory\n");
    }
    else {
      actDescriptor.repetitions = dataSize;
      if(!RecvData(*sd,
                   actInfo,
                   &actDescriptor,
                   1,
                   PktTimeOut(*sd))) {
        ERROR("ProcessRequest: data receive failed\n");
      }
      else {
        (void)SendMessage(*sd,
                          StopActivity(actInfo) ?
                          ACTIVITY_STOPPED : SENSOR_FAILED,
                          PktTimeOut(*sd));
      }
      free(actInfo);
    }
    break;

  default:
    ERROR1("ProcessRequest: unknown message %d\n", messageType);

  }

}


#define CPU_ACTIVITY_TAG ".cpu"
#define MAX_ADDRESSES 20
#define NEVER 0.0
#define SENSOR_ARGS "a:c:De:l:M:N:Pp:"

int debug;

int
main(int argc,
     char *argv[]) {

  char address[MAX_MACHINE_NAME + 1];
  IPAddress addresses[MAX_ADDRESSES];
  unsigned int addressesCount;
  char addressList[255 + 1];
  int autostartCpuMonitor;
  const char *c;
  char cpuRegistrationName[127 + 1];
  char errorFile[127 + 1];
  char logFile[127 + 1];
  IPAddress memoryAddress;
  char memoryName[127 + 1];
  struct host_desc memoryDesc;
  struct host_cookie nsCookie;
  struct host_desc nsDesc;
  double nextBeatTime;
  double nextWorkTime;
  double now;
  int opt;
  extern char *optarg;
  char password[127 + 1];
  const char *USAGE =
    "nws_sensor [-D] [-a name] [-c yes|no] [-el file] [-MN host] [-p port]";
  struct host_desc sensorDesc;
  double wakeup;

  /* Set up default values that will be overwritten by command line args. */
  addressList[0] = '\0';
  autostartCpuMonitor = 1;
  errorFile[0] = '\0';
  logFile[0] = '\0';
  SAFESTRCPY(memoryName,
             GetEnvironmentValue("SENSOR_MEMORY", "~", ".nwsrc",
                                 MyMachineName()));
  HostDValue(memoryName, DefaultHostPort(MEMORY_HOST), &memoryDesc);
  SAFESTRCPY(memoryName, NameOfHost(&memoryDesc));
  SAFESTRCPY(nsDesc.host_name,
             GetEnvironmentValue("NAME_SERVER", "~", ".nwsrc", "noname"));
  HostDValue(nsDesc.host_name, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
  HostDValue(MyMachineName(), DefaultHostPort(SENSOR_HOST), &sensorDesc);
  password[0] = '\0';
  debug = 0;

  while((opt = getopt(argc, argv, SENSOR_ARGS)) != EOF) {

    switch(opt) {

    case 'a':
      SAFESTRCPY(addressList, optarg);
      break;

    case 'c':
      autostartCpuMonitor = (*optarg == 'y') || (*optarg == 'Y');
      break;

    case 'D':
      debug = 1;
      break;

    case 'e':
      SAFESTRCPY(errorFile, optarg);
      break;

    case 'l':
      SAFESTRCPY(logFile, optarg);
      break;

    case 'M':
      SAFESTRCPY(memoryName, optarg);
      break;

    case 'N':
      HostDValue(optarg, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
      break;

    case 'P':
      printf("password? ");
      scanf("%s", password);
      break;

    case 'p':
      sensorDesc.port = atoi(optarg);
      break;

    default:
      fprintf(stderr, "nws_sensor: unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if (debug) {
    DirectDiagnostics(DIAGINFO, stdout);
    DirectDiagnostics(DIAGLOG, stdout);
    DirectDiagnostics(DIAGWARN, stderr);
    DirectDiagnostics(DIAGERROR, stderr);
    DirectDiagnostics(DIAGFATAL, stderr);
  }

  for(addressesCount = 0, c = addressList;
      GETTOK(address, c, ",", &c);
      addressesCount++) {
    if(!IPAddressValue(address, &addresses[addressesCount])) {
      ABORT1("Unable to convert '%s' into an IP address\n", address);
    }
  }
  addressesCount += IPAddressValues(sensorDesc.host_name,
                                    &addresses[addressesCount],
                                    MAX_ADDRESSES - addressesCount);

  MakeHostCookie(nsDesc.host_name, nsDesc.port, &nsCookie);
  if(!ConnectToHost(&nsCookie, &nsCookie.sd)) {
    ABORT("Unable to contact name server\n");
  }

  if(!LookupHostDesc(&nsCookie, memoryName, &memoryDesc)) {
    HostDValue(memoryName, DefaultHostPort(MEMORY_HOST), &memoryDesc);
    if(!IPAddressValue(memoryDesc.host_name, &memoryAddress) ||
       !LookupHostName(&nsCookie,
                       memoryAddress,
                       memoryDesc.port,
                       memoryName,
                       sizeof(memoryName))) {
      DisconnectHost(&nsCookie);
      ABORT("Unable to find memory registration\n");
    }
  }

  DisconnectHost(&nsCookie);

  if(!EstablishHost(NameOfHost(&sensorDesc),
                    SENSOR_HOST,
                    addresses,
                    addressesCount,
                    sensorDesc.port,
                    errorFile,
                    logFile,
                    password,
                    &nsDesc,
                    &SensorExit)) {
    exit(1);
  }
                    
  signal(SIGCHLD, Reap);
  signal(SIGPIPE, SocketFailure);

  if(!CliqueInit(NameOfHost(&memoryDesc), &memoryDesc)) {
    WARN("Sensor: clique control init failed\n");
  }
  if(!PeriodicInit(NameOfHost(&memoryDesc), &memoryDesc)) {
    WARN("Sensor: periodic control init failed\n");
  }
  if(!SkillsInit()) {
    WARN("Sensor: skills init failed\n");
  }
  if(autostartCpuMonitor) {
    vstrncpy(cpuRegistrationName, sizeof(cpuRegistrationName),
             2, EstablishedRegistration(), CPU_ACTIVITY_TAG);
    if(!StartActivity(PERIODIC_CONTROL_NAME,
                      cpuRegistrationName,
                      SkillName(cpuMonitor),
                      "")) {
      WARN("StartSensor: auto-start of CPU monitor failed\n");
    }
  }

  RegisterListener(ACTIVITY_START, "ACTIVITY_START", &ProcessRequest);
  RegisterListener(ACTIVITY_STOP, "ACTIVITY_STOP", &ProcessRequest);

  nextBeatTime = CurrentTime();

  while(1) {

    Reap(SIGCHLD);

    now = CurrentTime();
    if(now >= nextBeatTime) {
      RegisterHost(DEFAULT_HOST_BEAT * 2);
      nextBeatTime =
        now + (HostHealthy() ? DEFAULT_HOST_BEAT : SHORT_HOST_BEAT);
      now = CurrentTime();
    }

    wakeup = nextBeatTime;
    nextWorkTime = NextCliqueWork();
    if((nextWorkTime != NEVER) && (nextWorkTime < wakeup)) {
      wakeup = nextWorkTime;
    }
    nextWorkTime = NextPeriodicWork();
    if((nextWorkTime != NEVER) && (nextWorkTime < wakeup)) {
      wakeup = nextWorkTime;
    }

    ListenForMessages(wakeup - now);

    now = CurrentTime();
    nextWorkTime = NextCliqueWork();
    if((nextWorkTime != NEVER) && (now >= nextWorkTime)) {
      CliqueWork();
      now = CurrentTime();
    }
    nextWorkTime = NextPeriodicWork();
    if((nextWorkTime != NEVER) && (now >= nextWorkTime)) {
      PeriodicWork();
      now = CurrentTime();
    }

  }

  /* return 0; never reached */

}
