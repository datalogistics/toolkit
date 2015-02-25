/* $Id: host_protocol.c,v 1.61 2000/09/26 03:29:32 swany Exp $ */

#include "config.h"
#include <stdio.h>      /* FILE sprintf() rewind() */
#include <signal.h>     /* signal() */
#include <string.h>     /* memcpy() strlen() */
#include <stdlib.h>     /* atoi() */
#include <time.h>       /* ctime() */
#include <unistd.h>     /* ftruncate() */

#include "dnsutil.h"     /* IPAddress{Image,Machine,Values}() */
#include "protocol.h"    /* Socket stuff */
#include "nws_state.h"   /* Memory access */
#include "diagnostic.h"  /* Diagnostic utilities */
#include "osutil.h"      /* CurrentTime() GetEnvironmentValue() GetUserName() */
#include "strutil.h"     /* SAFESTRCPY() vstrncpy() zstrcpy() */
#include "register.h"    /* Register() Unregister() */
#include "host_protocol.h"


/*
** NOTE: On name server hosts, we presently ignore registration calls and
** invariably report "healthy" in response to test messages, since we can't
** conduct transactions with ourselves.  However, it might be nice to at least
** write something into the registration file.
*/


/* Module globals, mostly cached copies of the EstablishHost() params. */
static IPAddress *myAddresses = NULL;
static unsigned int myAddressesCount = 0;
static int (*myExitFunction)(void) = NULL;
static struct host_desc myNameServer = {"", 0};
static char *myPassword = NULL;
static unsigned short myPort = 0;
static char *myRegistration = NULL;
static HostTypes myType;
static ObjectSet registrationSet = NO_OBJECT_SET;


/*
** Handler for the SIGTERM signal.
*/
static void
TermHandler(int sig) {
  if((myExitFunction == NULL) || myExitFunction()) {
    CloseAllConnections();
    exit(2);
  }
}


/*
** Turns diagnostics on or off.  Toggles all error messages, all log messages,
** or both, depending on the values of #toggleErrors# and #toggleLogs#.
*/
static void
ToggleDiagnostics(int toggleErrors,
                  int toggleLogs) {

  static FILE *oldErrors = DIAGSUPPRESS;
  static FILE *oldLogs = DIAGSUPPRESS;
  FILE *pretoggle;

  if(toggleErrors) {
    pretoggle = DiagnosticsDirection(DIAGWARN);
    if((oldErrors != stderr) &&
       (oldErrors != stdout) &&
       (oldErrors != DIAGSUPPRESS)) {
      rewind(oldErrors);
      (void)ftruncate(fileno(oldErrors), 0);
    }
    DirectDiagnostics(DIAGWARN, oldErrors);
    DirectDiagnostics(DIAGERROR, oldErrors);
    DirectDiagnostics(DIAGFATAL, oldErrors);
    oldErrors = pretoggle;
  }
  if(toggleLogs) {
    pretoggle = DiagnosticsDirection(DIAGDEBUG);
    if((oldLogs != stderr) &&
       (oldLogs != stdout) &&
       (oldLogs != DIAGSUPPRESS)) {
      rewind(oldLogs);
      (void)ftruncate(fileno(oldLogs), 0);
    }
    DirectDiagnostics(DIAGDEBUG, oldLogs);
    DirectDiagnostics(DIAGINFO, oldLogs);
    DirectDiagnostics(DIAGLOG, oldLogs);
    oldLogs = pretoggle;
  }

}


/*
** A "local" function of EstablishHost().  Handles a #messageType# message
** arrived on #sd# accompanied by #dataSize# bytes of data.
*/
static void
ProcessRequest(Socket *sd,
               MessageType messageType,
               size_t dataSize) {

  char dataString[100];
  DataDescriptor diagDescriptor = SIMPLE_DATA(INT_TYPE, 1);
  HostInfo myInfo;
  DataDescriptor nsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  DataDescriptor passwordDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  int whichDiagnostics;

  switch(messageType) {

  case HOST_DIAGNOSTICS:
    if(!RecvData(*sd, &whichDiagnostics, &diagDescriptor, 1, PktTimeOut(*sd))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: data receive failed\n");
    }
    else {
      (void)SendMessage(*sd, HOST_DIAGNOSTICS_ACK, PktTimeOut(*sd));
      ToggleDiagnostics((whichDiagnostics == ALL_DIAGNOSTICS) ||
                        (whichDiagnostics == ALL_ERRORS),
                        (whichDiagnostics == ALL_DIAGNOSTICS) ||
                        (whichDiagnostics == ALL_LOGS));
    }
    break;

  case HOST_DIE:
    if(dataSize == 0) {
      SAFESTRCPY(dataString, "");
    }
    else {
      passwordDescriptor.repetitions = dataSize;
      if(!RecvData(*sd, &dataString, &passwordDescriptor, 1, PktTimeOut(*sd))) {
        DROP_SOCKET(sd);
        ERROR("ProcessRequest: data receive failed\n");
      }
    }
    if ( ( (strcmp(myPassword, "") == 0) ||
           (strcmp(myPassword, dataString) == 0) ) &&
         ( (myExitFunction == NULL) || myExitFunction() ) ) {
      (void)SendMessage(*sd, HOST_DYING, PktTimeOut(*sd));
      CloseAllConnections();
      exit(2);
    }
    else {
      (void)SendMessage(*sd, HOST_REFUSED, PktTimeOut(*sd));
    }
    break;

  case HOST_REGISTER:
    /*
    ** Note: all registration is done by this parent process, not by any forked
    ** processes.  This means that it's not a problem that we can't inform the
    ** sensor CPU probe, for example, of the new name server.
    */
    nsDescriptor.repetitions = dataSize;
    if(!RecvData(*sd, &dataString, &nsDescriptor, 1, PktTimeOut(*sd))) {
      DROP_SOCKET(sd);
      WARN("ProcessRequest: data receive failed\n");
    }
    else {
      (void)SendMessage(*sd, HOST_REGISTERED, PktTimeOut(*sd));
      HostDValue(dataString, DefaultHostPort(NAME_SERVER_HOST), &myNameServer);
      RegisterHost(DEFAULT_HOST_BEAT);
    }
    break;

  case HOST_TEST:
    SAFESTRCPY(myInfo.registrationName, myRegistration);
    myInfo.hostType = myType;
    SAFESTRCPY(myInfo.nameServer, HostDImage(&myNameServer));
    myInfo.healthy = HostHealthy();
    if(!SendMessageAndData(*sd,
                           HOST_TEST_RESULT,
                           &myInfo,
                           hostInfoDescriptor,
                           hostInfoDescriptorLength,
                           PktTimeOut(*sd))) {
      WARN("ProcessRequest: send failed on message ack\n");
    }
    break;

  default:
    DROP_SOCKET(sd);
    ERROR1("ProcessRequest: unknown message type %d\n", messageType);

  }

}


#define MAX_ADDRESSES 20


int
ConnectToHost(struct host_cookie *host_c,
              int *sd) {

  int addrCount;
  IPAddress addrs[MAX_ADDRESSES];
  int i;

  if(IsOkay(host_c->sd)) {

    *sd = host_c->sd;
    return 1;

  }
  else {

    addrCount = IPAddressValues(host_c->name, addrs, MAX_ADDRESSES);

    if(addrCount == 0) {
      FAIL("ConnectToHost: address retrieval failed\n");
    }

    for(i = 0; i < addrCount; i++) {
      if(CallAddr(addrs[i], host_c->port, sd, CONNTIMEOUT)) {
        host_c->sd = *sd;
        return 1;
      }
    }

    FAIL1("ConnectToHost: connect to %s failed\n", HostCImage(host_c));

  }

}


unsigned short
DefaultHostPort(HostTypes hostType) {

  const char *environmentName;
  const char *fallback;

  switch (hostType) {
  case FORECASTER_HOST:
    environmentName = "FORECASTER_PORT";
    fallback = "8070";
    break;
  case MEMORY_HOST:
    environmentName = "MEMORY_PORT";
    fallback = "8050";
    break;
  case NAME_SERVER_HOST:
    environmentName = "NAME_SERVER_PORT";
    fallback = "8090";
    break;
  case SENSOR_HOST:
    environmentName = "SENSOR_PORT";
    fallback = "8060";
    break;
  default:
    return 0;
  }

  return (unsigned short)
    atoi(GetEnvironmentValue(environmentName, "~", ".nwsrc", fallback));

}


void
DisconnectHost(struct host_cookie *host_c) {
  if(host_c->sd != NO_SOCKET) {
    DROP_SOCKET(&host_c->sd);
  }
}


int
EstablishHost(const char *registration,
              HostTypes hostType,
              IPAddress *addresses,
              unsigned int addressesCount,
              unsigned short port,
              const char *errorFile,
              const char *logFile,
              const char *password,
              struct host_desc *nameServer,
              int (*exitFunction)(void)) {

  const char *HOST_TYPES[] = {"forecaster", "memory", "nameserver", "sensor"};
  const char *MONTH_NAMES[] =
    {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

  FILE *diagFD;
  short earPort;
  Object hostObject;
  int i;
  char myAttr[255 + 1];
  Socket myEar;
  time_t startTime;
  struct tm timeInfo;

  if(!EstablishAnEar(port, port, &myEar, &earPort)) {
    /* Another host already running? */
    FAIL1("EstablishHost: attempt to attach port %d failed\n", port);
  }

  DirectDiagnostics(DIAGDEBUG, stdout);
  DirectDiagnostics(DIAGINFO, stdout);
  DirectDiagnostics(DIAGLOG, stdout);
  DirectDiagnostics(DIAGWARN, stderr);
  DirectDiagnostics(DIAGERROR, stderr);
  DirectDiagnostics(DIAGFATAL, stderr);

  if((errorFile != NULL) && (errorFile[0] != '\0')) {
    diagFD = fopen(errorFile, "w");
    if(diagFD == NULL) {
      WARN1("EstablishHost: open of %s failed\n", errorFile);
    }
    else {
      DirectDiagnostics(DIAGWARN, diagFD);
      DirectDiagnostics(DIAGERROR, diagFD);
      DirectDiagnostics(DIAGFATAL, diagFD);
      if((logFile != NULL) && (strcmp(errorFile, logFile) == 0)) {
        DirectDiagnostics(DIAGDEBUG, diagFD);
        DirectDiagnostics(DIAGINFO, diagFD);
        DirectDiagnostics(DIAGLOG, diagFD);
      }
    }
  }
  if((logFile != NULL) && (logFile[0] != '\0') &&
     ((errorFile == NULL) || (strcmp(errorFile, logFile) != 0))) {
    diagFD = fopen(logFile, "w");
    if(diagFD == NULL) {
      WARN1("EstablishHost: open of %s failed\n", logFile);
    }
    else {
      DirectDiagnostics(DIAGDEBUG, diagFD);
      DirectDiagnostics(DIAGINFO, diagFD);
      DirectDiagnostics(DIAGLOG, diagFD);
    }
  }

  myAddresses = (IPAddress *)malloc(addressesCount * sizeof(IPAddress));
  myRegistration = strdup(registration);
  myPassword = strdup((password == NULL) ? "" : password);
  if((myAddresses == NULL) || (myRegistration == NULL) || (myPassword == NULL)){
    FAIL("EstablishHost: out of memory\n");
  }

  memcpy(myAddresses, addresses, addressesCount * sizeof(IPAddress));
  myAddressesCount = addressesCount;
  myExitFunction = exitFunction;
  myNameServer = *nameServer;
  myPort = port;
  myType = hostType;

  signal(SIGTERM, TermHandler);

  RegisterListener(HOST_DIAGNOSTICS, "HOST_DIAGNOSTICS", &ProcessRequest);
  RegisterListener(HOST_DIE, "HOST_DIE", &ProcessRequest);
  RegisterListener(HOST_REGISTER, "HOST_REGISTER", &ProcessRequest);
  RegisterListener(HOST_TEST, "HOST_TEST", &ProcessRequest);

  hostObject = NewObject();
  AddAttribute(&hostObject, "name", myRegistration);
  AddAttribute(&hostObject, "objectclass", "nwsHost");
  AddAttribute(&hostObject, "hostType", HOST_TYPES[myType]);
  if(addressesCount > 0) {
    SAFESTRCPY(myAttr, IPAddressImage(addresses[0]));
    for(i = 1; i < addressesCount; i++) {
      strncat(myAttr, ",", sizeof(myAttr));
      strncat(myAttr, IPAddressImage(addresses[i]), sizeof(myAttr)); 
    }
    AddAttribute(&hostObject, "ipAddress", myAttr);
  }
  if(GetUserName(myAttr, sizeof(myAttr))) {
    AddAttribute(&hostObject, "owner", myAttr);
  }
  sprintf(myAttr, "%d", port);
  AddAttribute(&hostObject, "port", myAttr);
  startTime = (time_t)CurrentTime();
  timeInfo = *localtime(&startTime);
  sprintf(myAttr, "%s %02d, %4d %02d:%02d:%02d",
          MONTH_NAMES[timeInfo.tm_mon], timeInfo.tm_mday,
          timeInfo.tm_year + 1900, timeInfo.tm_hour, timeInfo.tm_min,
          timeInfo.tm_sec);
  AddAttribute(&hostObject, "started", myAttr);
#ifdef VERSION
  AddAttribute(&hostObject, "version", VERSION);
#endif

  RegisterObject(hostObject);
  FreeObject(&hostObject);

  return 1;

}


int
EstablishedInterface(IPAddress address,
                     unsigned short port) {
  int i;
  if(port != myPort)
    return 0;
  for(i = 0; i < myAddressesCount; i++) {
    if(myAddresses[i] == address)
      return 1;
  }
  return 0;
}


const char *
EstablishedRegistration(void) {
  return myRegistration;
}


const char *
HostCImage(const struct host_cookie *host_c) {
  static char returnValue[255];
  sprintf(returnValue, "%s:%d", host_c->name, host_c->port);
  return &returnValue[0];
}


const char *
HostDImage(const struct host_desc *host_d) {
  static char returnValue[255];
  sprintf(returnValue, "%s:%d", host_d->host_name, host_d->port);
  return &returnValue[0];
}


void
HostDValue(const char *image,
           unsigned short defaultPort,
           struct host_desc *host_d) {

  char *c;
  const char *fullName;
  IPAddress hostAddress;

  SAFESTRCPY(host_d->host_name, image);
  host_d->port = defaultPort;

  for(c = host_d->host_name; *c != '\0'; c++) {
    if(*c == ':') {
      *c = '\0';
      host_d->port = (short)atoi(c + 1);
      break;
    }
  }

  /* Normalize to an all-lowercase, fully-qualified DNS name. */
  if(IPAddressValue(host_d->host_name, &hostAddress) &&
     ((*(fullName = IPAddressMachine(hostAddress))) != '\0')) {
    SAFESTRCPY(host_d->host_name, fullName);
  }
  strcase(host_d->host_name, ALL_LOWER);

}


int
HostHealthy(void) {

  Object hostObject = NO_OBJECT;
  struct host_cookie nsCookie;
  int returnValue;

  if(myType == NAME_SERVER_HOST) {
    /* See note at top of module. */
    return 1;
  }

  MakeHostCookie(myNameServer.host_name, myNameServer.port, &nsCookie);
  returnValue = Lookup(&nsCookie, myRegistration, &hostObject);
  if(hostObject != NO_OBJECT) {
    FreeObject(&hostObject);
  }
  DisconnectHost(&nsCookie);
  return returnValue;

}


void
MakeHostCookie(const char *host_name,
               short host_port,
               struct host_cookie *host_c) {
  SAFESTRCPY(host_c->name, host_name);
  host_c->port = host_port;
  host_c->sd = NO_SOCKET;
}


const char *
NameOfHost(const struct host_desc *host_d) {
  static char hostName[MAX_HOST_NAME + 1];
  char machinePort[MAX_PORT_IMAGE + 1];
  /*
  ** NOTE: We don't normalize here, which means, for example, that a call that
  ** passes {"thing1", 8050} will generate a different return value from one
  ** that passes {"thing1.ucsd.edu", 8050}.  It's arguable whether or not this
  ** is the proper behavior.  In practice, however, the values passed to this
  ** function have invariably been generated by HostDValue, which does
  ** normalize.  The NWS code calls this function very frequently, and avoiding
  ** normalization here cuts down significantly on the number of calls out to
  ** the DNS.
  */
  sprintf(machinePort, "%d", host_d->port);
  vstrncpy(hostName, sizeof(hostName), 3,
           host_d->host_name, ":", machinePort);
  return &hostName[0];
}


void
RegisterHost(double timeOut) {

  struct host_cookie nsCookie;
  Object object;

  LOG("*** Beat ***\n");

  if(myType == NAME_SERVER_HOST) {
    /* See note at top of module. */
    return;
  }

  if(registrationSet != NO_OBJECT_SET) {
    MakeHostCookie(myNameServer.host_name, myNameServer.port, &nsCookie);
    for(object = NextObject(registrationSet, NO_OBJECT);
        object != NO_OBJECT;
        object = NextObject(registrationSet, object)) {
      (void)Register(&nsCookie, object, timeOut);
    }
    DisconnectHost(&nsCookie);
  }

}


void
RegisterObject(const Object object) {

  struct host_cookie nsCookie;

  if(registrationSet == NO_OBJECT_SET) {
    registrationSet = NewObjectSet();
  }

  AddObject(&registrationSet, object);

  if(myType == NAME_SERVER_HOST) {
    /* See note at top of module. */
    return;
  }

  MakeHostCookie(myNameServer.host_name, myNameServer.port, &nsCookie);
  (void)Register(&nsCookie, object, DEFAULT_HOST_BEAT);
  DisconnectHost(&nsCookie);

}


int
SameHost(const struct host_desc *left,
         const struct host_desc *right) {
  IPAddress leftAddr, rightAddr;
  if(left->port != right->port) {
    return 0;
  }
  if(strcmp(left->host_name, right->host_name) == 0) {
    return 1;
  }
  if(!IPAddressValue(left->host_name, &leftAddr) ||
     !IPAddressValue(right->host_name, &rightAddr)) {
    return 0;  /* No way to tell if they're the same. */
  }
  return leftAddr == rightAddr;
}


void
UnregisterObject(const char *objectName) {

  Object current;
  char filter[127 + 1];
  Attribute nameAttr;
  struct host_cookie nsCookie;
  ObjectSet thinnedSet = NewObjectSet();

  MakeHostCookie(myNameServer.host_name, myNameServer.port, &nsCookie);
  vstrncpy(filter, sizeof(filter), 3, "(name=", objectName, ")");
  (void)Unregister(&nsCookie, filter);
  DisconnectHost(&nsCookie);

  if (registrationSet == NO_OBJECT_SET) return;

  for(current = NextObject(registrationSet, NO_OBJECT);
      current != NO_OBJECT;
      current = NextObject(registrationSet, current)) {
    if(((nameAttr = FindAttribute(current, "name")) != NO_ATTRIBUTE) &&
       (strcmp(AttributeValue(nameAttr), objectName) == 0)) {
      for(current = NextObject(registrationSet, current);
          current != NO_OBJECT;
          current = NextObject(registrationSet, current)) {
        AddObject(&thinnedSet, current);
      }
      break;
    }
    AddObject(&thinnedSet, current);
  }
 
  FreeObjectSet(&registrationSet);
  registrationSet = thinnedSet;

}
