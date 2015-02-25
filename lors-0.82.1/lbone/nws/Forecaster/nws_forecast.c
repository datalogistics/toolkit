/* $Id: nws_forecast.c,v 1.69 2000/09/26 03:29:31 swany Exp $ */

#define FORKIT

#include "config.h"
#include <unistd.h>         /* getopt() */
#include <signal.h>         /* signal() */
#include <sys/wait.h>       /* waitpid() */
#include <stdlib.h>         /* atoi() free() malloc() REALLOC() */
#include "exp_protocol.h"   /* LoadExperiments() */
#include "host_protocol.h"  /* Host connection functions */
#include "diagnostic.h"     /* ERROR() FAIL() LOG() */
#include "strutil.h"        /* SAFESTRCPY() */
#include "dnsutil.h"        /* MyMachineName() */
#include "messages.h"       /* Recv*() Send*() */
#include "osutil.h"         /* CurrentTime() GetEnvironmentValue() */
#define FORECASTAPI_SHORTNAMES
#include "forecast_api.h"   /* Forecasting functions */
#include "nws_forecast.h"   /* forecaster messages */


/*
** This program implements the NWS forecaster host.  See nws_forecast.h for
** supported messages.
*/


/*
** ForecastInfo is a description of a forecast being tracked by this forecaster
** for reporting to a client process.  #name# is the client-specified name that
** comes in as part of the ForecastDataHeader or ForecastHeader.  #state#
** tracks the current forecast state.  For series forcasts, #memory# contains
** a connection to the memory host that stores the data.  These are pooled, so
** that multiple ForecastInfo records will contain copies of the same cookie if
** the same memory holds their series.  #lastUpdate# contains the timestamp
** from the latest measurement incorporated into a forecast.
** TBD: If multiple clients ask for forecasts of the same series, they will get
** different ForecastInfo records.  Maybe we should merge them?  Also, if a
** client closes its connection without telling us (via a zero moreToCome value
** in a ForecastDataHeader or ForecastSeries) that it's no longer interested
** in a particular forecast, we keep the record around anyway, resulting in a
** slow memory leak.  This needs to be fixed.
*/
typedef struct {
  char name[FORECAST_NAME_SIZE];
  ForecastState *state;
  struct host_cookie memory;
  double lastUpdate;
} ForecastInfo;
ForecastInfo *retainedForecasts = NULL;
unsigned int retainedForecastsCount = 0;
unsigned int retainedForecastsLength = 0;


/*
** Searches #retainedForecasts# for an element that contains a connection to
** the host described in #toCookie#.  If one is found, returns 1 and copies the
** socket to the sd field of #toCookie#, else returns 0.
*/
static int
CopyMemory(struct host_cookie *toCookie) {

  int i;
  struct host_desc newMemory;
  struct host_desc oldMemory;

  SAFESTRCPY(newMemory.host_name, toCookie->name);
  newMemory.port = toCookie->port;

  for(i = 0; i < retainedForecastsCount; i++) {
    SAFESTRCPY(oldMemory.host_name, retainedForecasts[i].memory.name);
    oldMemory.port = retainedForecasts[i].memory.port;
    if(SameHost(&newMemory, &oldMemory)) {
      toCookie->sd = retainedForecasts[i].memory.sd;
      return 1;
    }
  }

  return 0;

}


/*
** Frees the fields of #toDelete# and removes it from #retainedForecasts#.
*/
static void
FreeForecastInfo(ForecastInfo **toDelete) {
  struct host_cookie memory;
  if(*toDelete != NULL) {
    FreeForecastState(&(*toDelete)->state);
    memory = (*toDelete)->memory;
    *(*toDelete) = retainedForecasts[--retainedForecastsCount];
    if(!CopyMemory(&memory))
      DisconnectHost(&memory);
  }
  *toDelete = NULL;
}


/*
** Searches #retainedForecasts# for an entry named #forecastName#, and returns
** it if found.  Otherwise, allocates, initializes, and returns a new entry.
** In this case, if #memoryName# is not null, places a connection to the memory
** in the entry.  Returns NULL on failure.
*/
static ForecastInfo *
ForecastLookup(const char *forecastName,
               const char *memoryName) {

  ForecastInfo *extended;
  int i;
  struct host_desc memoryDesc;
  ForecastInfo newForecast;
  Socket sock;

  for(i = 0; i < retainedForecastsCount; i++) {
    if(strcmp(retainedForecasts[i].name, forecastName) == 0)
      return &retainedForecasts[i];
  }

  if(retainedForecastsCount == retainedForecastsLength) {
    extended = REALLOC(retainedForecasts,
                       (retainedForecastsLength + 5) * sizeof(ForecastInfo));
    if(extended == NULL)
      FAIL("ForecastLookup: out of memory\n");
    retainedForecasts = extended;
    retainedForecastsLength += 5;
  }

  newForecast.state = NewForecastState();
  if(newForecast.state == NULL)
    FAIL("ForecastLookup: unable to generate forecast state\n");

  if(memoryName == NULL) {
    memset(&newForecast.memory, 0, sizeof(newForecast.memory));
  }
  else {
    HostDValue(memoryName, DefaultHostPort(MEMORY_HOST), &memoryDesc);
    MakeHostCookie(memoryDesc.host_name, memoryDesc.port, &newForecast.memory);
    if(!CopyMemory(&newForecast.memory) &&
       !ConnectToHost(&newForecast.memory, &sock)) {
      FreeForecastState(&newForecast.state);
      FAIL1("ForecastLookup: connect to %s failed\n", memoryName);
    }
  }

  SAFESTRCPY(newForecast.name, forecastName);
  newForecast.lastUpdate = 0.0;
  retainedForecasts[retainedForecastsCount] = newForecast;
  return &retainedForecasts[retainedForecastsCount++];
   
}


/*
** Allows the system to scavenge zombie children.
*/
static void
Reap(void) {
  while(waitpid(-1, NULL, WNOHANG) > 0);
}


/*
** A "local" function of ProcessRequest().  Sends on #sd# a set of #atMost#
** forecasts generated by updating #info# with the elements of the
** #measurementCount#-long array #measurements#.
*/
static void
ProcessForecastRequest(Socket sd,
                       ForecastInfo *info,
                       size_t atMost,
                       Measurement *measurements,
                       size_t measurementCount) {

  ForecastCollection *forecasts = NULL;
  int forecastsCount;
  DataDescriptor forecastsDescriptor =
    {STRUCT_TYPE, 0, 0,
     (DataDescriptor *)forecastCollectionDescriptor,
     forecastCollectionDescriptorLength,
     PAD_BYTES(ForecastCollection, forecasts, Forecast, FORECAST_TYPE_COUNT)};
  int i;
  ForecastHeader replyHeader;

  forecastsCount = (atMost < measurementCount) ? atMost : measurementCount;

  if(forecastsCount != 0) {
    forecasts =
      (ForecastCollection *)malloc(atMost * sizeof(ForecastCollection));
    if(forecasts == NULL) {
      /* Out of memory.  For what it's worth, we can still send the header. */
      forecastsCount = 0;
    }
  }

  UpdateForecastState(info->state,
                      measurements,
                      measurementCount,
                      forecasts,
                      forecastsCount);
  SAFESTRCPY(replyHeader.forecastName, info->name);
  replyHeader.forecastCount = forecastsCount;

  (void)SendData(sd,
                 &replyHeader,
                 forecastHeaderDescriptor,
                 forecastHeaderDescriptorLength,
                 PktTimeOut(sd));

  if(forecastsCount > 0) {
    forecastsDescriptor.repetitions = forecastsCount;
    (void)SendData(sd,
                   forecasts,
                   &forecastsDescriptor,
                   1,
                   PktTimeOut(sd));
  }

  for(i = 0; i < measurementCount; i++) {
    if(measurements[i].timeStamp > info->lastUpdate)
      info->lastUpdate = measurements[i].timeStamp;
  }

  if(forecasts != NULL) {
    free(forecasts);
  }

}


#define MAX_ADDRESSES 20
#define MAX_EXPERIMENTS 1000


/*
** A "local" function of main().  Handles a #messageType# message arrived on
** #sd# accompanied by #dataSize# bytes of data.
*/
static void
ProcessRequest(Socket *sd,
               MessageType messageType,
               size_t dataSize) {

  DataDescriptor countDescriptor = SIMPLE_DATA(UNSIGNED_INT_TYPE, 1);
  Experiment *experiments;
  ForecastDataHeader dataHeader;
  size_t experimentsCount;
  double experimentsLast;
  int i;
  ForecastInfo *info;
  Measurement *measurements;
  DataDescriptor measurementsDescriptor =
    {STRUCT_TYPE, 0, 0,
     (DataDescriptor *)measurementDescriptor, measurementDescriptorLength,
     PAD_BYTES(Measurement, measurement, double, 1)};
  const char *methodName;
  char *methodsNames;
  DataDescriptor methodsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  Socket requester = *sd;
  ForecastSeries series;
  unsigned int seriesCount;

  switch(messageType) {

  case FORE_DATA:
    if(!RecvData(requester,
                 &dataHeader,
                 forecastDataHeaderDescriptor,
                 forecastDataHeaderDescriptorLength,
                 PktTimeOut(requester))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: request header receive failed\n");
      return;
    }
    measurements =
      (Measurement *)malloc(dataHeader.measurementCount * sizeof(Measurement));
    if(measurements == NULL) {
      (void)SendMessage(requester, FORE_FAIL, PktTimeOut(requester));
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: out of memory\n");
      return;
    }
    measurementsDescriptor.repetitions = dataHeader.measurementCount;
    if(!RecvData(requester,
                 measurements,
                 &measurementsDescriptor,
                 1,
                 PktTimeOut(requester))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: measurement receive failed\n");
      free(measurements);
      return;
    }
    info = ForecastLookup(dataHeader.forecastName, NULL);
    if(info == NULL) {
      (void)SendMessage(requester, FORE_FAIL, PktTimeOut(requester));
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: out of start forecast\n");
      free(measurements);
      return;
    }
    if(!SendMessage(requester,
                    FORE_FORECAST,
                    PktTimeOut(requester))) {
      ERROR("ProcessRequest: reply failed\n");
      DROP_SOCKET(sd);
      free(measurements);
      return;
    }
    ProcessForecastRequest(requester,
                           info,
                           dataHeader.atMost,
                           measurements,
                           dataHeader.measurementCount);
    if(!dataHeader.moreToCome) {
      FreeForecastInfo(&info);
    }
    free(measurements);
    break;

  case FORE_SERIES:
    if(!RecvData(requester,
                 &seriesCount,
                 &countDescriptor,
                 1,
                 PktTimeOut(requester))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: count receive failed\n");
      return;
    }
    experiments = (Experiment *)malloc(MAX_EXPERIMENTS * sizeof(Experiment));
    if(experiments == NULL) {
      (void)SendMessage(requester, FORE_FAIL, PktTimeOut(requester));
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: out of memory\n");
    }
    if(!SendMessage(requester,
                    FORE_FORECAST,
                    PktTimeOut(requester))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: reply failed\n");
      return;
    }
    for(i = 0; i < seriesCount; i++) {
      if(!RecvData(requester,
                   &series,
                   forecastSeriesDescriptor,
                   forecastSeriesDescriptorLength,
                   PktTimeOut(requester))) {
        DROP_SOCKET(sd);
        ERROR("ProcessRequest: series receive failed\n");
        free(experiments);
        return;
      }
      info = ForecastLookup(series.forecastName, series.memoryName);
      if(info == NULL) {
        ERROR("ProcessRequest: unable to start forecast\n");
        DROP_SOCKET(sd);
        free(experiments);
        return;
      }
      if(!LoadExperiments(&info->memory,
                          series.seriesName,
                          experiments,
                          MAX_EXPERIMENTS,
                          info->lastUpdate,
                          &experimentsCount,
                          &experimentsLast,
                          PktTimeOut(info->memory.sd))) {
        experimentsCount = 0;
      }
      /*
      ** WARNING: Here we're using the fact that we know that the structures of
      ** Experiments and Measurements are identical.  A bit dangerous, but it
      ** seems silly to do an item-by-item copy.
      */
      ProcessForecastRequest(requester,
                             info,
                             series.atMost,
                             (Measurement *)experiments,
                             experimentsCount);
      if(!series.moreToCome) {
        FreeForecastInfo(&info);
      }
      free(experiments);
    }
    break;

  case METHODS_ASK:
    methodsNames = strdup("");
    for(i = 0; (methodName = MethodName(i)) != NULL; i++) {
      methodsNames =
         REALLOC(methodsNames, strlen(methodsNames) + strlen(methodName) + 2);
      strcat(methodsNames, methodName);
      strcat(methodsNames, "\t");
    }
    methodsDescriptor.repetitions = strlen(methodsNames) + 1;
    (void)SendMessageAndData(requester,
                             METHODS_TELL,
                             methodsNames,
                             &methodsDescriptor,
                             1,
                             PktTimeOut(requester));
    free(methodsNames);
    break;

  default:
    DROP_SOCKET(sd);
    ERROR1("ProcessRequest: unknown message type %d\n", messageType);

  }

}


#define SWITCHES "a:De:l:N:Pp:"

int debug;

int
main(int argc,
     char *argv[]) {

  char address[MAX_MACHINE_NAME + 1];
  IPAddress addresses[MAX_ADDRESSES];
  unsigned int addressesCount;
  char addressList[255 + 1];
  const char *c;
  char errorFile[127 + 1];
  struct host_desc forecasterDesc;
  char logFile[127 + 1];
  double nextBeatTime;
  double now;
  struct host_desc nsDesc;
  int opt;
  extern char *optarg;
  char password[128];
  const char *USAGE = "nws_forecast [-D] [-a name] [-el file] [-N host] [-p port]";

  addressList[0] = '\0';
  errorFile[0] = '\0';
  HostDValue(MyMachineName(),DefaultHostPort(FORECASTER_HOST),&forecasterDesc);
  logFile[0] = '\0';
  SAFESTRCPY(nsDesc.host_name,
             GetEnvironmentValue("NAME_SERVER", "~", ".nwsrc", "noname"));
  HostDValue(nsDesc.host_name, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
  password[0] = '\0';
  debug = 0;

  while((opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'a':
      SAFESTRCPY(addressList, optarg);
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

    case 'N':
      HostDValue(optarg, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
      break;

    case 'P':
      fprintf(stdout, "Password? ");
      fscanf(stdin, "%s", password);
      break;

    case 'p':
      forecasterDesc.port = atoi(optarg);
      break;

    default:
      fprintf(stderr, "nws_forecast: unrecognized switch\n%s\n", USAGE);
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
  addressesCount += IPAddressValues(forecasterDesc.host_name,
                                    &addresses[addressesCount],
                                    MAX_ADDRESSES - addressesCount);

  if(!EstablishHost(NameOfHost(&forecasterDesc),
                    FORECASTER_HOST,
                    addresses,
                    addressesCount,
                    forecasterDesc.port,
                    errorFile,
                    logFile,
                    password,
                    &nsDesc,
                    NULL)) {
    exit(1);
  }

  fclose(stdin);
  signal(SIGPIPE, SocketFailure);
  RegisterListener(FORE_DATA, "FORE_DATA", ProcessRequest);
  RegisterListener(FORE_SERIES, "FORE_SERIES", ProcessRequest);
  RegisterListener(METHODS_ASK, "METHODS_ASK", ProcessRequest);
  nextBeatTime = CurrentTime();

  while(1) {
    Reap();
    now = CurrentTime();
    if(now >= nextBeatTime) {
      RegisterHost(DEFAULT_HOST_BEAT * 2);
      nextBeatTime =
        now + (HostHealthy() ? DEFAULT_HOST_BEAT : SHORT_HOST_BEAT);
    }
    ListenForMessages(nextBeatTime - now);
  }

  /* return(0); never reached */

}
