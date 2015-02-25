/* $Id: nws_api.c,v 1.86 2001/02/18 08:52:20 pgeoffra Exp $ */

#include <sys/types.h>      /* required for ntohl() on Cray */
#include <stdio.h>          /* sprintf() */
#include <stdlib.h>         /* atoi() free() malloc() REALLOC() */
#include <string.h>         /* memcpy() strdup() strcat() strchr() strcmp() */
#include <config.h>
#include <netinet/in.h>     /* ntohl() */
#include "config.h"
#include "diagnostic.h"     /* FAIL() */
#include "exp_protocol.h"   /* {Load,Store}Experiments() */
#include "host_protocol.h"  /* Host connection functions */
#include "messages.h"       /* Recv*() Send*() */
#include "strutil.h"        /* SAFESTRCPY() */
#include "dnsutil.h"        /* IPAddressValues() MyMachineName() */
#include "nws_sensor.h"     /* sensor messages */
#include "nws_forecast.h"   /* forecaster messages */
#include "nws_memory.h"     /* memory messages */
#include "register.h"       /* experiment registration */
#include "forecast_api.h"   /* Forecast struct */
#include "osutil.h"         /* GetEnvironmentValue() */
#include "nws_api.h"


/*
** Information cached for auto-fetch series that we're tracking.  A connection
** to the memory that holds the series and the (tab-deliminted) list of series
** names that this memory is sending to us.
*/
typedef struct {
  struct host_cookie memory;
  char *series;
} AutoFetchInfo;


/*
** Module globals.  #autoFetches# holds a list of memory connections that we're
** using to track auto-fetched series, and #autoFetchCount# is the length of
** this list.  The host_cookies contain connections to a default forecaster,
** memory, and name server.  All these values are overridden by a call to one
** of the Use*() functions.  We're supposed to pick the "best" forecaster and
** memory if the client doesn't specify one, and there is no longer a default
** name server, so we leave the initial values emtpy.  #forecasterMethods# is
** the list of method names used by the most recently contacted forecaster.
*/
static AutoFetchInfo *autoFetches = NULL;
static size_t autoFetchCount = 0;
static struct host_cookie defaultForecaster = {"", 0, NO_SOCKET};
static struct host_cookie defaultMemory = {"", 0, NO_SOCKET};
static struct host_cookie defaultNS = {"", 0, NO_SOCKET};
static char *forecasterMethods = NULL;


static void
AutoFetchDisconnect(Socket who);


/*
** Connects to #memory#, sends it #seriesList# in an AUTOFETCH_BEGIN message,
** and places an entry for the connection in the #autoFetches# global.  Returns
** 1 if successful, else 0.
*/
static int
AutoFetchConnect(const struct host_desc *memory,
                 const char *seriesList) {

  AutoFetchInfo *extendedAutoFetches;
  size_t ignored;
  struct host_cookie newConnection;
  char *seriesCopy;
  DataDescriptor seriesDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);

  extendedAutoFetches = (AutoFetchInfo *)
    REALLOC(autoFetches, (autoFetchCount + 1) * sizeof(AutoFetchInfo));
  if(extendedAutoFetches == NULL) {
    FAIL("AutoFetchConnect: out of memory\n");
  }
  autoFetches = extendedAutoFetches;
  seriesCopy = strdup(seriesList);
  if(seriesCopy == NULL) {
    FAIL("AutoFetchConnect: out of memory\n");
  }

  MakeHostCookie(memory->host_name, memory->port, &newConnection);
  if(!ConnectToHost(&newConnection, &newConnection.sd)) {
    free(seriesCopy);
    FAIL1("AutoFetchConnect: Unable to contact memory %s\n",
          HostDImage(memory));
  }

  autoFetches[autoFetchCount].memory = newConnection;
  autoFetches[autoFetchCount].series = seriesCopy;
  autoFetchCount++;

  seriesDescriptor.repetitions = strlen(seriesList) + 1;

  if(!SendMessageAndData(newConnection.sd,
                         AUTOFETCH_BEGIN,
                         seriesList,
                         &seriesDescriptor,
                         1,
                         PktTimeOut(newConnection.sd)) ||
     !RecvMessage(newConnection.sd,
                  AUTOFETCH_ACK,
                  &ignored,
                  PktTimeOut(newConnection.sd))) {
    AutoFetchDisconnect(newConnection.sd);
    FAIL1("AutoFetchConnect: Memory %s refused request\n", HostDImage(memory));
  }

  return 1;

}


/*
** Disconnects the auto-fetch memory connected to #who# and removes its entry
** from the #autoFetches# module global list.
*/
static void
AutoFetchDisconnect(Socket who) {

  int i;
  DataDescriptor emptyDescriptor = SIMPLE_DATA(CHAR_TYPE, 1);

  for(i = 0; (i < autoFetchCount) && (autoFetches[i].memory.sd != who); i++)
    ; /* Nothing more to do. */

  if(i == autoFetchCount)
    return; /* Non-auto-fetch memory? */

  (void)SendMessageAndData(who, AUTOFETCH_BEGIN, "", &emptyDescriptor, 1, 5);
  /* Ignore the reply.  It might get mixed with an autofetch anyway */
  DROP_SOCKET(&who);
  free(autoFetches[i].series);
  if(autoFetchCount > 1) {
    autoFetches[i] = autoFetches[--autoFetchCount];
  }
  else {
    free(autoFetches);
    autoFetches = NULL;
    autoFetchCount = 0;
  }

}


/*
** Checks to see if the #defaultNS# global has been filled in and, if not,
** fills it with fields from the user's .nwsrc file.  For convenience, returns
** the address of #defaultNS#.
*/
static struct host_cookie *
CheckNS(void) {
  struct host_desc nsDesc;
  if(defaultNS.port == 0) {
    SAFESTRCPY(nsDesc.host_name,
               NWSAPI_EnvironmentValue("NAME_SERVER", "noname"));
    HostDValue(nsDesc.host_name, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
    MakeHostCookie(nsDesc.host_name, nsDesc.port, &defaultNS);
  }
  return &defaultNS;
}


/*
** Contacts the name server to look up the series #seriesName# and return the
** memory which is storing its measurements.  Returns 1 if successful, else 0.
*/
static int
GetSeriesMemory(const char *seriesName,
                struct host_desc *seriesMemory) {
  Attribute memoryAttr;
  Object seriesObject;
  if(!Lookup(CheckNS(), seriesName, &seriesObject)) {
    FAIL1("GetSeriesMemory: unable to get registration for %s\n", seriesName);
  }
  else if((memoryAttr = FindAttribute(seriesObject, "memory")) ==
          NO_ATTRIBUTE) {
    FreeObject(&seriesObject);
    FAIL1("GetSeriesMemory: %s has no memory attribute\n", seriesName);
  }
  HostDValue(AttributeValue(memoryAttr),
             DefaultHostPort(MEMORY_HOST),
             seriesMemory);
  FreeObject(&seriesObject);
  return 1;
}


/*
** Contacts the default name server to find and contact the host with all the
** attributes of #filter# that is "closest" to the machine #host#.  Returns 1
** and connects #cookie# to the host if successful; else returns 0.
*/
#define MAX_CHOICES 50
static int
FindBestHost(const char *host,
             const char *filter,
             struct host_cookie *cookie) {

  int choice;
  size_t hostCount;
  int hostDifferences[MAX_CHOICES];
  IPAddress hostAddr;
  NWSAPI_HostSpec hostList[MAX_CHOICES];
  int i;
  Socket ignoredSocket;
  IPAddress targetAddr;

  if(!NWSAPI_GetHosts(filter, &hostList[0], MAX_CHOICES, &hostCount))
    FAIL("FindBestHost: unable to retrieve host list\n");
  else if(hostCount == 0)
    FAIL("FindBestHost: no appropriate hosts registered with name server\n");

  /*
  ** Compute the "distance" from each retrieved host to #host# by xor'ing the
  ** retrieved host address with that of #host#.  This is crude, since there's
  ** no guarantee that IP address similarity assures quick access, but it's
  ** probably better than nothing.
  */
  targetAddr = IPAddressValue(host, &hostAddr) ? ntohl(hostAddr) : 0;
    
  for(i = 0; i < hostCount; ) {
    if(IPAddressValue(hostList[i].machineName, &hostAddr))
      hostDifferences[i++] = targetAddr ^ ntohl(hostAddr);
    else
      hostList[i] = hostList[--hostCount];
  }

  /* Try to contact each host in order of decreasing "closeness". */
  while(hostCount > 0) {
    choice = 0;
    for(i = 1; i < hostCount; i++) {
      if(hostDifferences[i] < hostDifferences[choice])
        choice = i;
    }
    MakeHostCookie
      (hostList[choice].machineName, hostList[choice].machinePort, cookie);
    if(ConnectToHost(cookie, &ignoredSocket)) {
      return 1;
    }
    hostList[choice] = hostList[--hostCount];
  }

  FAIL("FindBestHost: unable to contact any appropriate host\n");

}


#define OPTION_NAME_LEN (31 + 1)


typedef struct {
  char name[OPTION_NAME_LEN];
  unsigned int minOccurrences;
  unsigned int maxOccurrences;
  char format;
} OptionInfo;


/*
** A "local" function of StartActivity().  If #attr# is listed in #options#,
** sets the fields of #intoWhere# based on the name and value of #attr# and
** returns 1; otherwise, returns 0.
*/
static int
ParseOptionAttribute(const NWSAPI_Attribute attr,
                     const char *options,
                     OptionInfo *intoWhere) {

  const char *attrName;
  const char *attrValue;
  OptionInfo newOption;

  attrName = AttributeName(attr);
  if(strstr(options, attrName) == 0) {
    return 0;
  }

  /* Option values have the format MIN_to_MAX_TYPE */
  SAFESTRCPY(newOption.name, attrName);
  for(newOption.minOccurrences = 0, attrValue = AttributeValue(attr);
      *attrValue != '_';
      newOption.minOccurrences = newOption.minOccurrences*10+*attrValue-'0',
      attrValue++)
    ; /* Nothing more to do. */
  for(newOption.maxOccurrences = 0, attrValue += strlen("_to_");
      *attrValue != '_';
      newOption.maxOccurrences = newOption.maxOccurrences*10+*attrValue-'0',
      attrValue++)
    ; /* Nothing more to do. */
  newOption.format = *(attrValue + 1);
  *intoWhere = newOption;

  return 1;

}


/*
** Receives a ForecastHeader and accompanying forecasts from the forecaster
** connected to #whoFrom# and copies the forecasts into the #atMost#-long array
** #whereTo#.  If successful, returns 1 and sets #numberReturned# to the actual
** number of forecasts receives, else returns 0.
*/
static int
RecvForecasts(struct host_cookie *whoFrom,
              FORECASTAPI_ForecastCollection *whereTo,
              size_t atMost,
              size_t *numberReturned) {

  DataDescriptor collectionsDescriptor =
    {STRUCT_TYPE, 0, 0,
     (DataDescriptor *)&forecastCollectionDescriptor,
     forecastCollectionDescriptorLength,
     PAD_BYTES(FORECASTAPI_ForecastCollection, forecasts, FORECASTAPI_Forecast,
               FORECASTAPI_FORECAST_TYPE_COUNT)};
  ForecastHeader forecastHeader;
  FORECASTAPI_ForecastCollection *forecasts;

  if(!RecvData(whoFrom->sd,
               &forecastHeader,
               forecastHeaderDescriptor,
               forecastHeaderDescriptorLength,
               MAXPKTTIMEOUT)) {
    FAIL1("RecvForecasts: recv from host %s failed\n", HostCImage(whoFrom));
  }

  forecasts = (FORECASTAPI_ForecastCollection *)
    malloc(forecastHeader.forecastCount *
           sizeof(FORECASTAPI_ForecastCollection));
  if(forecasts == NULL) {
    FAIL("RecvForecasts: out of memory\n");
  }

  collectionsDescriptor.repetitions = forecastHeader.forecastCount;
  if(!RecvData(whoFrom->sd,
               forecasts,
               &collectionsDescriptor,
               1,
               MAXPKTTIMEOUT)) {
    free(forecasts);
    FAIL1("RecvForecasts: recv from host %s failed\n", HostCImage(whoFrom));
  }

  if(forecastHeader.forecastCount <= atMost) {
    atMost = forecastHeader.forecastCount;
  }

  memcpy(whereTo, forecasts, atMost * sizeof(FORECASTAPI_ForecastCollection));

  if(numberReturned != NULL) {
    *numberReturned = atMost;
  }
  free(forecasts);
  return 1;

}


/*
** Uses the information in #seriesSpec# to set the fields in #series#.
*/
static void
SeriesSpecToSeries(const NWSAPI_SeriesSpec *seriesSpec,
                   Series *series) {

  struct host_desc host;

  HostDValue(seriesSpec->sourceMachine, DefaultHostPort(SENSOR_HOST), &host);
  SAFESTRCPY(series->host, NameOfHost(&host));
  SAFESTRCPY(series->memory, "");
  SAFESTRCPY(series->resource, seriesSpec->resourceName);

  /*
  ** TBD: This is a bit icky; we just "know" the option and label values for
  ** the default resources, and the caller has no way to specify them.
  */
  if(strcmp(seriesSpec->resourceName,
            NWSAPI_DEFAULT_AVAILABLE_CPU_RESOURCE) == 0) {
    SAFESTRCPY(series->label, "CPU Fraction");
    SAFESTRCPY(series->options, "nice:0");
  }
  else if(strcmp(seriesSpec->resourceName,
          NWSAPI_DEFAULT_BANDWIDTH_RESOURCE) == 0) {
    SAFESTRCPY(series->label, "Megabits/second");
    SAFESTRCPY(series->options, "buffer:32\tmessage:16\tsize:64");
  }
  else if(strcmp(seriesSpec->resourceName,
                 NWSAPI_DEFAULT_CURRENT_CPU_RESOURCE) == 0) {
    SAFESTRCPY(series->label, "CPU Fraction");
    SAFESTRCPY(series->options, "nice:0");
  }
  else if(strcmp(seriesSpec->resourceName,
                 NWSAPI_DEFAULT_DISK_RESOURCE) == 0) {
    SAFESTRCPY(series->label, "Megabytes");
    SAFESTRCPY(series->options, "");
  }
  else if(strcmp(seriesSpec->resourceName,
                 NWSAPI_DEFAULT_LATENCY_RESOURCE) == 0) {
    SAFESTRCPY(series->label, "Milliseconds");
    SAFESTRCPY(series->options, "");
  }
  else if(strcmp(seriesSpec->resourceName,
                 NWSAPI_DEFAULT_MEMORY_RESOURCE) == 0) {
    SAFESTRCPY(series->label, "Megabytes");
    SAFESTRCPY(series->options, "type:p");
  }
  else {
    SAFESTRCPY(series->label, "");
    SAFESTRCPY(series->options, "");
  }
  if(seriesSpec->destinationMachine[0] != '\0') {
    HostDValue
      (seriesSpec->destinationMachine, DefaultHostPort(SENSOR_HOST), &host);
    sprintf(series->options, "%s%starget:%s",
            series->options, (series->options[0] == '\0') ? "" : "\t",
            HostDImage(&host));
  }

}


/*
** Sets the #forecasterMethods# module variable to the set of methods used by
** #whoFrom#.  Returns 1 if successful, else 0.
*/
static int
UpdateForecastingMethods(struct host_cookie *whoFrom) {

  static struct host_desc whoseMethodsDoWeHave = {"", 0};
  DataDescriptor methodsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);

  if((strcmp(whoFrom->name, whoseMethodsDoWeHave.host_name) == 0) &&
     (whoFrom->port == whoseMethodsDoWeHave.port)) {
    return 1;  /* No need to contact them again. */
  }

  if(!SendMessage(whoFrom->sd,
                  METHODS_ASK,
                  PktTimeOut(whoFrom->sd))) {
    FAIL("UpdateForecastingMethods: send failed\n");
  }
  else if(!RecvMessage(whoFrom->sd,
                       METHODS_TELL,
                       &methodsDescriptor.repetitions,
                       PktTimeOut(whoFrom->sd))) {
    FAIL("UpdateForecastingMethods: receive failed\n");
  }

  if(forecasterMethods != NULL) {
    free(forecasterMethods);
    forecasterMethods = NULL;
  }

  forecasterMethods = malloc(methodsDescriptor.repetitions);

  if(forecasterMethods == NULL) {
    FAIL("UpdateForecastingMethods: out of memory\n");
  }

  if(!RecvData(whoFrom->sd,
               forecasterMethods,
               &methodsDescriptor,
               1,
               PktTimeOut(whoFrom->sd))) {
    FAIL("UpdateForecastingMethods: receive failed\n");
  }

  SAFESTRCPY(whoseMethodsDoWeHave.host_name, whoFrom->name);
  whoseMethodsDoWeHave.port = whoFrom->port;
  return 1;

}


/*
** Attempts to connect to #whichHost#.  If successful, copies host name and
** port number into #whichCookie# and returns 1; else returns 0.
*/
static int
UseHost(const NWSAPI_HostSpec *whichHost,
        struct host_cookie *whichCookie) {

  struct host_cookie connectCookie;
  Socket ignored;

  if((whichCookie->port != whichHost->machinePort) ||
     (strcmp(whichCookie->name, whichHost->machineName) != 0)) {
    MakeHostCookie(whichHost->machineName,
                   whichHost->machinePort,
                   &connectCookie);
    if(!ConnectToHost(&connectCookie, &ignored)) {
      FAIL1("Unable to contact %s\n", HostCImage(&connectCookie));
    }
    *whichCookie = connectCookie;
  }

  return 1;

}


const char *
NWSAPI_AttributeName(NWSAPI_Attribute attribute) {
  return AttributeName(attribute);
}


const char *
NWSAPI_AttributeValue(NWSAPI_Attribute attribute) {
  return AttributeValue(attribute);
}


int
NWSAPI_AutoFetchBegin(const char *seriesName) {

  AutoFetchInfo *fetch;
  int i;
  size_t listLen;
  char *listToFetch;
  struct host_desc memoryDesc;
  int returnValue;

  if(!GetSeriesMemory(seriesName, &memoryDesc)) {
    FAIL1("AutoFetchBegin: unable to determine %s memory\n", seriesName);
  }

  /* See whether we're already asking this memory for other series. */
  for(i = 0, fetch = autoFetches; i < autoFetchCount; i++, fetch++) {
    if((strcmp(memoryDesc.host_name, fetch->memory.name) == 0) &&
       (memoryDesc.port == fetch->memory.port)) {
      break;
    }
  }

  if(i == autoFetchCount) {
    return AutoFetchConnect(&memoryDesc, seriesName);
  }
  else if(strstr(fetch->series, seriesName) != NULL) {
    /* We're already fetching this series. */
    return 1;
  }
  else {
    /*
    ** An existing memory.  Because we're receiving messages asynchronously,
    ** we can't reliably reuse the connection -- we might mistake the ack for
    ** a series coming in or vice versa.  Instead, get rid of this connection
    ** and open a new one with the extended list.
    */
    listLen = strlen(fetch->series) + strlen(seriesName) + 2;
    listToFetch = (char *)malloc(listLen);
    if(listToFetch == NULL) {
      FAIL("AutoFetchBegin: out of memory\n");
    }
    vstrncpy(listToFetch, listLen, 3, fetch->series, "\t", seriesName);
    AutoFetchDisconnect(fetch->memory.sd);
    returnValue = AutoFetchConnect(&memoryDesc, listToFetch);
    free(listToFetch);
    return returnValue;
  }

}


int
NWSAPI_AutoFetchCheck(char *seriesName,
                      size_t nameLen,
                      NWSAPI_Measurement *seriesMeasurement,
                      unsigned long timeOut) {

  /*
  ** Although in practice auto-fetch STATE_FETCHED messages always contain a
  ** single measurement, in theory multiple measurements could be enclosed, so
  ** we cache the most recently received state (#autoFetchState#) and contents
  ** (#autoFetchContents#) in case the practice ever changes.  #nextWord# is
  ** used to iteratively pull tokens out of #autoFetchContents#.
  */
  static char *autoFetchContents = NULL;
  static struct state autoFetchState;
  static const char *nextWord = NULL;

  DataDescriptor contentsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  size_t ignored;
  char measurement[127 + 1];
  Socket sender;
  char timeStamp[127 + 1];

  while((nextWord == NULL) ||
        !GETWORD(timeStamp, nextWord, &nextWord) ||
        !GETWORD(measurement, nextWord, &nextWord)) {

    if(!IncomingRequest(timeOut, &sender)) {
      return 0;
    }

    if(!RecvMessage(sender, STATE_FETCHED, &ignored, PktTimeOut(sender)) ||
       !RecvData(sender,
                 &autoFetchState,
                 stateDescriptor,
                 stateDescriptorLength,
                 PktTimeOut(sender))) {
      AutoFetchDisconnect(sender);
      continue;
    }

    contentsDescriptor.repetitions =
      autoFetchState.rec_size * autoFetchState.rec_count;
    if(autoFetchContents != NULL) {
      free(autoFetchContents);
    }
    autoFetchContents = (char *)malloc(contentsDescriptor.repetitions + 1);
    if(autoFetchContents == NULL) {
      AutoFetchDisconnect(sender);
      nextWord = NULL;
      FAIL("AutoFetchCheck: out of memory\n");
    }

    if(!RecvData(sender,
                 autoFetchContents,
                 &contentsDescriptor,
                 1,
                 PktTimeOut(sender))) {
      AutoFetchDisconnect(sender);
      free(autoFetchContents);
      autoFetchContents = NULL;
    }

    nextWord = autoFetchContents;

  }

  zstrncpy(seriesName, autoFetchState.id, nameLen);
  seriesMeasurement->timeStamp = atof(timeStamp);
  seriesMeasurement->measurement = atof(measurement);
  return 1;

}


void
NWSAPI_AutoFetchEnd(const char *seriesName) {

  int i;
  struct host_desc memory;
  char *nameEnd = NULL;
  char *nameStart = NULL;
  char *shrunkenList;

  if(*seriesName == '\0') {
    while(autoFetchCount > 0) {
      AutoFetchDisconnect(autoFetches[0].memory.sd);
    }
    return;
  }

  for(i = 0; i < autoFetchCount; i++) {
    /*
    ** Look for this series name in the list of series we're fetching from this
    ** memory.  The for loop below takes care of the unlikely possibility that
    ** the series name is a substring of some other series name.
    */
    for(nameStart = strstr(autoFetches[i].series, seriesName);
        nameStart != NULL;
        nameStart = strstr(nameStart + 1, seriesName)) {
      nameEnd = nameStart + strlen(seriesName);
      /* Tab delimiters or an end of the string at each end of the name? */
      if(((nameStart == autoFetches[i].series) || (*(nameStart - 1) == '\t')) &&
         ((*nameEnd == '\0') || (*nameEnd == '\t'))) {
        break;
      }
    }
    if(nameStart != NULL) {
      break;
    }
  }

  if(i == autoFetchCount) {
    return; /* Not fetching this series. */
  }
  else if(strcmp(autoFetches[i].series, seriesName) == 0) {
    /* No other series being fetched from this memory. */
    AutoFetchDisconnect(autoFetches[i].memory.sd);
    return;
  }

  /* Squeeze out the name and tab delimiter of the series we're ending. */
  if(*nameEnd == '\0') {
    *(nameStart - 1) = '\0';
  }
  else {
    strcpy(nameStart, nameEnd + 1);
  }

  /* See comment in AutoFetchBegin() as to why we can't reuse the connection. */
  shrunkenList = strdup(autoFetches[i].series);
  if(shrunkenList == NULL) {
    ERROR("AutoFetchEnd: out of memory\n");
  }

  SAFESTRCPY(memory.host_name, autoFetches[i].memory.name);
  memory.port = autoFetches[i].memory.port;
  AutoFetchDisconnect(autoFetches[i].memory.sd);
  (void)AutoFetchConnect(&memory, shrunkenList);
  free(shrunkenList);

}


const char *
NWSAPI_EnvironmentValue(const char *name,
                        const char *defaultValue) {
  return GetEnvironmentValue(name, "~", ".nwsrc", defaultValue);
}


int
NWSAPI_HaltActivity(const NWSAPI_HostSpec *whichSensor,
                    const char *activityName) {

  DataDescriptor activityDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  size_t ignored;
  int returnValue;
  struct host_desc sensorDesc;
  struct host_cookie sensorLink;

  HostDValue(whichSensor->machineName, whichSensor->machinePort, &sensorDesc);
  MakeHostCookie(sensorDesc.host_name, sensorDesc.port, &sensorLink);

  if(!ConnectToHost(&sensorLink, &sensorLink.sd)) {
    FAIL1("HaltActivity: unable to contact sensor %s\n",
          HostDImage(&sensorDesc));
  }

  activityDescriptor.repetitions = strlen(activityName) + 1;
  returnValue = SendMessageAndData(sensorLink.sd,
                                   ACTIVITY_STOP,
                                   activityName,
                                   &activityDescriptor,
                                   1,
                                   PktTimeOut(sensorLink.sd)) &&
                RecvMessage(sensorLink.sd,
                            ACTIVITY_STOPPED,
                            &ignored,
                            PktTimeOut(sensorLink.sd));
  DisconnectHost(&sensorLink);
  return returnValue;

}


const char *
NWSAPI_ForecasterMethodName(unsigned int methodIndex) {

  int i;
  const char *nameEnd;
  const char *nameStart;
  static char returnValue[63 + 1];

  if(forecasterMethods == NULL) {
    return "";
  }

  nameStart = forecasterMethods;

  for(i = 0; i < methodIndex; i++) {
    nameStart = strchr(nameStart, '\t');
    if(nameStart == NULL) {
      return "";
    }
    nameStart++;
  }

  nameEnd = strchr(nameStart, '\t');
  if(nameEnd == NULL) {
    return "";
  }
  zstrncpy(returnValue, nameStart, nameEnd - nameStart + 1);
  return returnValue;

}


void
NWSAPI_FreeObjectSet(NWSAPI_ObjectSet *toBeFreed) {
  FreeObjectSet(toBeFreed);
}


int
NWSAPI_GenerateForecasts(const char **whichSeries,
                         size_t howMany) {
  /* Nothing to do for now. */
  return 1;
}


int
NWSAPI_GetForecasts(const char *seriesName,
                    double sinceWhen,
                    FORECASTAPI_ForecastCollection *whereTo,
                    size_t atMost,
                    size_t *numberReturned) {

  /* TBD Multiple series request */

  DataDescriptor countDescriptor = SIMPLE_DATA(UNSIGNED_INT_TYPE, 1);
  struct host_cookie *forecasterToUse;
  int i;
  struct host_cookie localForecaster;
  size_t numberUseful;
  size_t recvSize;
  ForecastSeries request;
  unsigned int requestCount;
  int returnValue;
  struct host_desc seriesMemory;

  if(!GetSeriesMemory(seriesName, &seriesMemory)) {
    return 0; /* Error message will come from GetSeriesMemory. */
  }

  if((defaultForecaster.port != 0) &&
     ((defaultForecaster.sd != NO_SOCKET) ||
      ConnectToHost(&defaultForecaster, &defaultForecaster.sd)))
    forecasterToUse = &defaultForecaster;
  else if(FindBestHost(seriesMemory.host_name,
                       NWSAPI_FORECASTERS,
                       &localForecaster)) {
    forecasterToUse = &localForecaster;
  }
  else
    return 0; /* Error message will come from FindBestHost() */

  request.moreToCome = 0;  /* TBD How do we know? */
  request.atMost = atMost;
  SAFESTRCPY(request.memoryName, HostDImage(&seriesMemory));
  SAFESTRCPY(request.seriesName, seriesName);
  SAFESTRCPY(request.forecastName, request.seriesName);

  requestCount = 1;
  if(!SendMessageAndDatas(forecasterToUse->sd,
                          FORE_SERIES,
                          &requestCount,
                          &countDescriptor,
                          1,
                          &request,
                          forecastSeriesDescriptor,
                          forecastSeriesDescriptorLength,
                          MAXPKTTIMEOUT)) {
    DisconnectHost(forecasterToUse);
    FAIL1("GetForecasts: send to host %s failed\n",
          HostCImage(forecasterToUse));
  }

  returnValue =
    RecvMessage(forecasterToUse->sd, FORE_FORECAST, &recvSize, MAXPKTTIMEOUT) &&
    RecvForecasts(forecasterToUse, whereTo, atMost, &numberUseful);

  if(!returnValue ||
     !UpdateForecastingMethods(forecasterToUse) ||
     (forecasterToUse == &localForecaster)) {
    DisconnectHost(forecasterToUse);
  }

  /* Toss out any forecasts that are older than the user's cut-off time. */
  if(returnValue) {
    for(i = 0; i < numberUseful; i++) {
      if(whereTo[i].measurement.timeStamp <= sinceWhen) {
        numberUseful = i;
        break;
      }
    }
  }

  if(numberReturned != NULL) {
    *numberReturned = numberUseful;
  }

  return (returnValue && (numberUseful > 0));

}


int
NWSAPI_GetHosts(const char *filter,
                NWSAPI_HostSpec *whereTo,
                size_t atMost,
                size_t *numberReturned) {

  char *c;
  char *hostFilter;
  int i;
  struct host_desc host;
  NWSAPI_Attribute hostAttribute;
  NWSAPI_Object    hostObject;
  NWSAPI_ObjectSet hostObjects;

  hostFilter = (char *)malloc(strlen(filter) + strlen(NWSAPI_HOSTS) + 6);
  sprintf(hostFilter, "(&(%s)%s)", filter, NWSAPI_HOSTS);
  if(!NWSAPI_GetObjects(hostFilter, &hostObjects)) {
    free(hostFilter);
    FAIL("GetHosts: object retrieval failed\n");
  }
  free(hostFilter);

  i = 0;
  NWSAPI_ForEachObject(hostObjects, hostObject) {
    if(i >= atMost)
      break;
    host.host_name[0] = '\0';
    host.port = 0;
    NWSAPI_ForEachAttribute(hostObject, hostAttribute) {
      if(strcmp(AttributeName(hostAttribute), NWSAPI_IP_ATTR) == 0) {
        /*
        ** TBD: We really should iterate through the IP addresses here looking
        ** for one we can connect to.  Instead, we just grab the first one.
        */
        SAFESTRCPY(host.host_name, AttributeValue(hostAttribute));
        if((c = strchr(host.host_name, ',')) != NULL)
          *c = '\0';
      }
      else if(strcmp(AttributeName(hostAttribute), NWSAPI_PORT_ATTR) == 0) {
        host.port = atoi(AttributeValue(hostAttribute));
      }
    }
    HostDValue(host.host_name, host.port, &host);
    SAFESTRCPY(whereTo[i].machineName, host.host_name);
    whereTo[i].machinePort = host.port;
    i++;
  }
  NWSAPI_FreeObjectSet(&hostObjects);
  
  if(numberReturned != NULL) {
    *numberReturned = i;
  }

  return 1;

}


int
NWSAPI_GetMeasurements(const char *seriesName,
                       double sinceWhen,
                       FORECASTAPI_Measurement *whereTo,
                       size_t atMost,
                       size_t *numberReturned) {

  Experiment *experiments;
  int i;
  double lastTimeStamp;
  struct host_cookie localMemory;
  struct host_cookie *memoryToUse;
  size_t returnCount;
  struct host_desc seriesMemory;

  if((defaultMemory.port != 0) &&
     ((defaultMemory.sd != NO_SOCKET) ||
      ConnectToHost(&defaultMemory, &defaultMemory.sd))) {
    memoryToUse = &defaultMemory;
  }
  else if(GetSeriesMemory(seriesName, &seriesMemory)) {
    MakeHostCookie(seriesMemory.host_name, seriesMemory.port, &localMemory);
    memoryToUse = &localMemory;
  }
  else {
    return 0; /* Error message will come from GetSeriesMemory(). */
  }

  experiments = (Experiment *)malloc(atMost * sizeof(Experiment));
  if(experiments == NULL) {
    FAIL("GetMeasurements: out of memory\n");
  }

  if(!LoadExperiments(memoryToUse,
                      seriesName,
                      experiments,
                      atMost,
                      sinceWhen,
                      &returnCount,
                      &lastTimeStamp,
                      MAXPKTTIMEOUT)) {
    DisconnectHost(memoryToUse);
    free(experiments);
    FAIL("GetMeasurements: LoadExperiments failed\n");
  }

  if(memoryToUse != &defaultMemory) {
    DisconnectHost(memoryToUse);
  }

  for(i = 0; i < returnCount; i++) {
    whereTo[i].timeStamp = experiments[i].timeStamp;
    whereTo[i].measurement = experiments[i].value;
  }
  if(numberReturned != NULL)
    *numberReturned = returnCount;
  free(experiments);
  return 1;

}


int
NWSAPI_GetNameServer(const NWSAPI_HostSpec *whoToAsk,
                     NWSAPI_HostSpec *nameServer) {

  struct host_cookie *cookieToUse;
  HostInfo info;
  struct host_cookie localCookie;
  size_t recvSize;

  if((whoToAsk->machinePort == defaultForecaster.port) &&
     (strcmp(whoToAsk->machineName, defaultForecaster.name) == 0)) {
    cookieToUse = &defaultForecaster;
  }
  else if((whoToAsk->machinePort == defaultMemory.port) &&
          (strcmp(whoToAsk->machineName, defaultMemory.name) == 0)) {
    cookieToUse = &defaultMemory;
  }
  else if((whoToAsk->machinePort == CheckNS()->port) &&
          (strcmp(whoToAsk->machineName, defaultNS.name) == 0)) {
    cookieToUse = &defaultNS;
  }
  else {
    MakeHostCookie(whoToAsk->machineName, whoToAsk->machinePort, &localCookie);
    cookieToUse = &localCookie;
  }

  if(!ConnectToHost(cookieToUse, &cookieToUse->sd)) {
    FAIL1("GetNameServer: unable to contact %s\n", HostCImage(cookieToUse));
  }

  if(!SendMessage(cookieToUse->sd, HOST_TEST, MAXPKTTIMEOUT)) {
    DisconnectHost(cookieToUse);
    FAIL("GetNameServer: send failed\n");
  }

  if(!RecvMessage(cookieToUse->sd,
                  HOST_TEST_RESULT,
                  &recvSize,
                  MAXPKTTIMEOUT) ||
     !RecvData(cookieToUse->sd,
               &info,
               hostInfoDescriptor,
               hostInfoDescriptorLength,
               MAXPKTTIMEOUT)) {
    DisconnectHost(cookieToUse);
    FAIL("GetNameServer: receive failed\n");
  }

  if(cookieToUse == &localCookie) {
    DisconnectHost(cookieToUse);
  }

  *nameServer = *NWSAPI_MakeHostSpec(info.nameServer, 0);
  return 1;

}


int
NWSAPI_GetObjects(const char *filter,
                  NWSAPI_ObjectSet *whereTo) {
  if(RetrieveObjects(CheckNS(), filter, whereTo))
    return 1;
  else
    FAIL1("GetObjects: unable to retrieve objects from %s\n",
          HostCImage(&defaultNS));
}


int
NWSAPI_InstantForecasts(const char *seriesName,
                        const FORECASTAPI_Measurement *measurements,
                        size_t howMany,
                        FORECASTAPI_ForecastCollection *whereTo,
                        size_t atMost,
                        size_t *numberReturned) {

  struct host_cookie *forecasterToUse;
  struct host_cookie localForecaster;
  DataDescriptor measurementsDescriptor =
    {STRUCT_TYPE, 0, 0, 
     (DataDescriptor *)&measurementDescriptor, measurementDescriptorLength,
     PAD_BYTES(FORECASTAPI_Measurement, measurement, double, 1)};
  size_t recvSize;
  ForecastDataHeader request;
  int returnValue;

  if((defaultForecaster.port != 0) &&
     ((defaultForecaster.sd != NO_SOCKET) ||
      ConnectToHost(&defaultForecaster, &defaultForecaster.sd)))
    forecasterToUse = &defaultForecaster;
  else if(FindBestHost(MyMachineName(), NWSAPI_FORECASTERS, &localForecaster))
    forecasterToUse = &localForecaster;
  else
    return 0; /* Error message will come from FindBestHost() */

  request.moreToCome = 0;  /* TBD How do we know? */
  request.atMost = atMost;
  SAFESTRCPY(request.forecastName, seriesName);
  request.measurementCount = howMany;

  measurementsDescriptor.repetitions = howMany;
  if(!SendMessageAndDatas(forecasterToUse->sd,
                          FORE_DATA,
                          &request,
                          forecastDataHeaderDescriptor,
                          forecastDataHeaderDescriptorLength,
                          measurements,
                          &measurementsDescriptor,
                          1,
                          MAXPKTTIMEOUT)) {
    DisconnectHost(forecasterToUse);
    FAIL1("InstantForecasts: send to host %s failed\n",
          HostCImage(forecasterToUse));
  }

  returnValue =
    RecvMessage(forecasterToUse->sd, FORE_FORECAST, &recvSize, MAXPKTTIMEOUT) &&
    RecvForecasts(forecasterToUse, whereTo, atMost, numberReturned);

  if(!returnValue ||
     !UpdateForecastingMethods(forecasterToUse) ||
     (forecasterToUse == &localForecaster)) {
    DisconnectHost(forecasterToUse);
  }

  return returnValue;

}


int
NWSAPI_IntermachineResource(const char *resourceName) {

  char filter[255 + 1];
  ObjectSet objects;
  int returnValue;

  if(strncmp(resourceName,
             NWSAPI_DEFAULT_AVAILABLE_CPU_RESOURCE,
             strlen(NWSAPI_DEFAULT_AVAILABLE_CPU_RESOURCE)) == 0 ||
     strncmp(resourceName,
             NWSAPI_DEFAULT_CURRENT_CPU_RESOURCE,
             strlen(NWSAPI_DEFAULT_CURRENT_CPU_RESOURCE)) == 0 ||
     strncmp(resourceName,
             NWSAPI_DEFAULT_DISK_RESOURCE,
             strlen(NWSAPI_DEFAULT_DISK_RESOURCE)) == 0 ||
     strncmp(resourceName,
             NWSAPI_DEFAULT_MEMORY_RESOURCE,
             strlen(NWSAPI_DEFAULT_MEMORY_RESOURCE)) == 0) {
    return 0;
  }
  else if(strncmp(resourceName,
                  NWSAPI_DEFAULT_BANDWIDTH_RESOURCE,
                  strlen(NWSAPI_DEFAULT_BANDWIDTH_RESOURCE)) == 0 ||
          strncmp(resourceName,
                  NWSAPI_DEFAULT_LATENCY_RESOURCE,
                  strlen(NWSAPI_DEFAULT_LATENCY_RESOURCE)) == 0) {
    return 1;
  }

  /*
  ** Find out whether there's any series registered with the name server that
  ** list both this resource and a target host.
  */
  vstrncpy(filter, sizeof(filter), 10,
           "(&", NWSAPI_SERIES,
                 "(", NWSAPI_RESOURCE_ATTR, "=", resourceName, ")",
                 "(", NWSAPI_TARGET_ATTR, "=*)",
           ")");

  if(!NWSAPI_GetObjects(filter, &objects)) {
    return 0;  /* No way to tell for sure. */
  }

  returnValue = NextObject(objects, NO_OBJECT) != NO_OBJECT;
  FreeObjectSet(&objects);
  return returnValue;

}


const NWSAPI_SeriesSpec *
NWSAPI_MakeSeriesSpec(const char *sourceMachine,
                      const char *destinationMachine,
                      const char *resourceName) {
  static NWSAPI_SeriesSpec returnValue;
  SAFESTRCPY(returnValue.sourceMachine, sourceMachine);
  SAFESTRCPY(returnValue.destinationMachine,
             (destinationMachine == NULL) ? "" : destinationMachine);
  SAFESTRCPY(returnValue.resourceName, resourceName);
  return &returnValue;
}


const NWSAPI_HostSpec *
NWSAPI_MakeHostSpec(const char *host,
                    NWSAPI_Port machinePort) {
  struct host_desc hostD;
  static NWSAPI_HostSpec returnValue;
  HostDValue(host, machinePort, &hostD);
  SAFESTRCPY(returnValue.machineName, hostD.host_name);
  returnValue.machinePort = hostD.port;
  return &returnValue;
}


NWSAPI_Attribute
NWSAPI_NextAttribute(NWSAPI_Object object,
                     NWSAPI_Attribute current) {
  return NextAttribute(object, (current == NULL) ? NO_ATTRIBUTE : current);
}


NWSAPI_Object
NWSAPI_NextObject(NWSAPI_ObjectSet objects,
                  NWSAPI_Object current) {
  return NextObject(objects, (current == NULL) ? NO_OBJECT : current);
}


#define KEEP_A_LONG_TIME 315360000.0


int
NWSAPI_PutMeasurements(const char *seriesName,
                       const NWSAPI_SeriesSpec *whichSeries,
                       const FORECASTAPI_Measurement *measurements,
                       size_t howMany) {

  Experiment *experiments;
  int i;
  struct host_cookie localMemory;
  struct host_desc memoryDesc;
  NWSAPI_HostSpec memoryNSSpec;
  NWSAPI_HostSpec memorySpec;
  struct host_cookie *memoryToUse;
  struct host_cookie registrarCookie;
  int returnValue;
  Series series;
  Object toRegister;

  if((defaultMemory.port != 0) &&
     ((defaultMemory.sd != NO_SOCKET) ||
      ConnectToHost(&defaultMemory, &defaultMemory.sd)))
    memoryToUse = &defaultMemory;
  else if(FindBestHost(MyMachineName(), NWSAPI_MEMORIES, &localMemory)) {
    memoryToUse = &localMemory;
  }
  else
    return 0; /* Error message will come from FindBestHost() */

  /*
  ** Try to register with the memory's name server.  If that doesn't work, fall
  ** back on whatever name server we're connected to.  This could result in
  ** the weird situation where the series is registered with a different name
  ** server than the memory, but this may be sufficient for the user.
  */
  SeriesSpecToSeries(whichSeries, &series);
  memorySpec = *NWSAPI_MakeHostSpec(memoryToUse->name, memoryToUse->port);
  if(NWSAPI_GetNameServer(&memorySpec, &memoryNSSpec))
    MakeHostCookie(memoryNSSpec.machineName,
                   memoryNSSpec.machinePort,
                   &registrarCookie);
  else
    MakeHostCookie(CheckNS()->name, CheckNS()->port, &registrarCookie);
  HostDValue(memoryToUse->name, memoryToUse->port, &memoryDesc);
  SAFESTRCPY(series.memory, NameOfHost(&memoryDesc));

  toRegister = ObjectFromSeries(seriesName, &series);
  if(!Register(&registrarCookie, toRegister, NEVER_EXPIRE)) {
    FreeObject(&toRegister);
    DisconnectHost(memoryToUse);
    DisconnectHost(&registrarCookie);
    FAIL("StoreMeasurements: series registration failed\n");
  }
  FreeObject(&toRegister);
  DisconnectHost(&registrarCookie);

  experiments = (Experiment *)malloc(howMany * sizeof(Experiment));
  if(experiments == NULL) {
    DisconnectHost(memoryToUse);
    FAIL("StoreMeasurements: malloc failed\n");
  }

  for(i = 0; i < howMany; i++) {
    experiments[i].timeStamp = measurements[i].timeStamp;
    experiments[i].value = measurements[i].measurement;
  }

  returnValue = StoreExperiments(memoryToUse,
                                 NameOfSeries(&series),
                                 experiments,
                                 howMany,
                                 KEEP_A_LONG_TIME);
  free(experiments);
  if(memoryToUse != &defaultMemory)
    DisconnectHost(memoryToUse);
  return returnValue;

}


#define MAX_OPTIONS 64


int
NWSAPI_StartActivity(const NWSAPI_HostSpec *whichSensor,
                     const char *activityName,
                     const char *controlName,
                     const char *skillName,
                     const char **options,
                     size_t howManyOptions) {

  DataDescriptor activityDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  char *activitySpec;
  const char *c;
  NWSAPI_Attribute controlAttr;
  char controlFilter[127 + 1];
  NWSAPI_Object controlObject;
  NWSAPI_ObjectSet controlObjects;
  int i;
  int j;
  size_t ignored;
  const char *nameEnd;
  char optionNames[255 + 1];
  OptionInfo optInfo[MAX_OPTIONS];
  unsigned int optInfoCount;
  char portImage[MAX_PORT_IMAGE + 1];
  int returnValue;
  struct host_desc sensorDesc;
  char sensorName[MAX_MACHINE_NAME + sizeof(portImage)];
  struct host_cookie sensorLink;
  NWSAPI_Attribute skillAttr;
  char skillFilter[127 + 1];
  NWSAPI_Object skillObject;
  NWSAPI_ObjectSet skillObjects;
  int valueCount;

  /* Retrieve control and skill registrations from the name server. */
  sprintf(portImage, "%d", whichSensor->machinePort);
  vstrncpy(sensorName, sizeof(sensorName), 3,
           whichSensor->machineName, ":", portImage);
  vstrncpy(controlFilter, sizeof(controlFilter), 12,
           "(&", NWSAPI_CONTROLS,
                 "(", NWSAPI_HOST_ATTR, "=", sensorName, ")",
                 "(", NWSAPI_CONTROL_ATTR, "=", controlName, "))");
  vstrncpy(skillFilter, sizeof(skillFilter), 12,
           "(&", NWSAPI_SKILLS,
                 "(", NWSAPI_HOST_ATTR, "=", sensorName, ")",
                 "(", NWSAPI_SKILL_ATTR, "=", skillName, "))");

  if(!NWSAPI_GetObjects(controlFilter, &controlObjects)) {
    FAIL("StartActivity: unable to retrieve sensor control information\n");
  }
  else if((controlObject = NWSAPI_FirstObject(controlObjects)) == NULL) {
    NWSAPI_FreeObjectSet(&controlObjects);
    FAIL2("StartActivity: no control named %s is registered by %s\n",
          controlName, sensorName);
  }
  if(!NWSAPI_GetObjects(skillFilter, &skillObjects)) {
    NWSAPI_FreeObjectSet(&controlObjects);
    FAIL("StartActivity: unable to retrieve sensor skill information\n");
  }
  else if((skillObject = NWSAPI_FirstObject(skillObjects)) == NULL) {
    NWSAPI_FreeObjectSet(&controlObjects);
    NWSAPI_FreeObjectSet(&skillObjects);
    FAIL2("StartActivity: no skill named %s is registered by %s\n",
          skillName, sensorName);
  }

  /* Parse each skill option attribute value into an element of optInfo. */
  controlAttr = FindAttribute(controlObject, NWSAPI_OPTION_ATTR);
  SAFESTRCPY(optionNames,
             (controlAttr == NULL) ? "" : AttributeValue(controlAttr));
  skillAttr = FindAttribute(skillObject, NWSAPI_OPTION_ATTR);
  if((skillAttr != NULL) && (*AttributeValue(skillAttr) != '\0')) {
    if(optionNames[0] != '\0')
      strcat(optionNames, ",");
    strcat(optionNames, AttributeValue(skillAttr));
  }
  optInfoCount = 0;
  NWSAPI_ForEachAttribute(controlObject, controlAttr) {
    if(ParseOptionAttribute(controlAttr, optionNames, &optInfo[optInfoCount])) {
      optInfoCount++;
    }
  }
  NWSAPI_ForEachAttribute(skillObject, skillAttr) {
    /*
    ** TBD HACK ALERT: inter-machine skills include a required "target" option
    ** that designates the experiment target machines.  However, when the
    ** clique control exercises these skills, the target list is inherited from
    ** the clique membership, so a separate target list is both unnecessary and
    ** unwanted (see related HACK ALERT in clique_protocol.c).  The easiest way
    ** to implement this here is to ignore the "target" option entirely.
    */
    if((strcmp(controlName, "clique") == 0) &&
       (strcmp(AttributeName(skillAttr), "target") == 0))
      continue;
    if(ParseOptionAttribute(skillAttr, optionNames, &optInfo[optInfoCount])) {
      optInfoCount++;
    }
  }

  NWSAPI_FreeObjectSet(&controlObjects);
  NWSAPI_FreeObjectSet(&skillObjects);

  /* Make sure all names in #options# are valid option names... */
  for(i = 0; i < howManyOptions; i++) {
    nameEnd = strchr(options[i], ':');
    if(nameEnd == NULL) {
      FAIL1("StartActivity: bad option format %s\n", options[i]);
    }
    for(j = 0;
        (j < optInfoCount) &&
        (strncmp(optInfo[j].name, options[i], nameEnd - options[i]) != 0);
        j++)
      ; /* Nothing more to do. */
    if(j == optInfoCount) {
      FAIL1("StartActivity: unknown option %s\n", options[i]);
    }
  }

  /* ... and that #options# includes a proper number of each option. */
  for(i = 0; i < optInfoCount; i++) {
    valueCount = 0;
    for(j = 0; j < howManyOptions; j++) {
      nameEnd = strchr(options[j], ':');
      if(strncmp(optInfo[i].name, options[j], nameEnd - options[j]) == 0) {
        for(c = nameEnd + 1; c != NULL; c = strchr(c + 1, ',')) {
          valueCount++;
        }
      }
    }
    if((valueCount < optInfo[i].minOccurrences) ||
       (valueCount > optInfo[i].maxOccurrences)) {
      FAIL4("StartActivity: %d values given for %s option; %d to %d allowed\n",
            valueCount, optInfo[i].name, optInfo[i].minOccurrences,
            optInfo[i].maxOccurrences);
    }
  }

  HostDValue(sensorName, 0, &sensorDesc);
  MakeHostCookie(sensorDesc.host_name, sensorDesc.port, &sensorLink);
  if(!ConnectToHost(&sensorLink, &sensorLink.sd)) {
    FAIL1("StartActivity: unable to contact sensor %s\n", sensorName);
  }

  /*
  ** Package the registration name, control name, skill name, and options into
  ** a series of nul-terminated strings to accompany the start message.
  */
  activityDescriptor.repetitions =
    strlen(activityName) + strlen(controlName) + strlen(skillName) + 4;
  for(i = 0; i < howManyOptions; i++) {
    activityDescriptor.repetitions += strlen(options[i]) + 1;
  }
  activitySpec = (char *)malloc(activityDescriptor.repetitions);
  if(activitySpec == NULL) {
    DisconnectHost(&sensorLink);
    FAIL("StartActivity: out of memory\n");
  }

  /* Use spaces to reserve space for the nul characters. */
  vstrncpy(activitySpec, activityDescriptor.repetitions, 6,
           activityName, " ", controlName, " ", skillName, " ");
  for(i = 0; i < howManyOptions; i++) {
    strcat(activitySpec, options[i]);
    strcat(activitySpec, "\t");
  }
  activitySpec[strlen(activityName)] = '\0';
  activitySpec[strlen(activityName) + strlen(controlName) + 1] = '\0';
  activitySpec
    [strlen(activityName) + strlen(controlName) + strlen(skillName) + 2] = '\0';

  /* Finally, send the start message. */
  returnValue =
    SendMessageAndData(sensorLink.sd,
                       ACTIVITY_START,
                       activitySpec,
                       &activityDescriptor,
                       1,
                       PktTimeOut(sensorLink.sd)) &&
    RecvMessage(sensorLink.sd,
                ACTIVITY_STARTED,
                &ignored,
                PktTimeOut(sensorLink.sd));
  DisconnectHost(&sensorLink);
  free(activitySpec);
  return returnValue;

}


int
NWSAPI_UseForecaster(const NWSAPI_HostSpec *whichForecaster) {
  return UseHost(whichForecaster, &defaultForecaster);
}


const char *
NWSAPI_SeriesName(const NWSAPI_SeriesSpec *whichSeries) {
  Series series;
  SeriesSpecToSeries(whichSeries, &series);
  return NameOfSeries(&series);
}


int
NWSAPI_UseMemory(const NWSAPI_HostSpec *whichMemory) {
  return UseHost(whichMemory, &defaultMemory);
}


int
NWSAPI_UseNameServer(const NWSAPI_HostSpec *whichNS) {
  return UseHost(whichNS, CheckNS());
}
