/* $Id: nws_api.h,v 1.40 2000/05/30 21:37:24 hayes Exp $ */

#ifndef NWS_API_H
#define NWS_API_H

#ifdef	__cplusplus
extern "C" {
#endif


/*
** This file defines the API for the Network Weather Service.  The functions
** defined here allow programs to ask for forecasts, store measurements in NWS
** memories, retrieve previously-stored measurements, register objects with the
** NWS name server, and retrieve previously-registered objects.
**
** NOTE: In order to avoid name collisions, all names defined here begin with
** NWSAPI_.  To avoid having to use this prefix, #define NWSAPI_SHORTNAMES
** before including this file.
*/


#include <sys/types.h>    /* size_t */
#include "forecast_api.h" /* ForecastCollection Measurement */


/* Allow forecaster API names to be accessed using NWSAPI prefix. */
#define NWSAPI_Measurement FORECASTAPI_Measurement
#define NWSAPI_Forecast FORECASTAPI_Forecast
#define NWSAPI_FORECAST_TYPE_COUNT FORECASTAPI_FORECAST_TYPE_COUNT
#define NWSAPI_MAE_FORECAST FORECASTAPI_MAE_FORECAST
#define NWSAPI_MSE_FORECAST FORECASTAPI_MSE_FORECAST
#define NWSAPI_ForecastCollection FORECASTAPI_ForecastCollection
#define NWSAPI_ForecastStateStruct FORECASTAPI_ForecastStateStruct
#define NWSAPI_ForecastState FORECASTAPI_ForecastState
#define NWSAPI_FreeForecastState FORECASTAPI_FreeForecastState
#define NWSAPI_MethodName FORECASTAPI_MethodName
#define NWSAPI_NewForecastState FORECASTAPI_NewForecastState
#define NWSAPI_UpdateForecastState FORECASTAPI_UpdateForecastState


/*
** General NWS API declarations. **
*/


/* A description of an NWS host -- machine name and port number. */
#define NWSAPI_MACHINE_NAME_LENGTH (63 + 1)
typedef unsigned short NWSAPI_Port;
typedef struct {
  char machineName[NWSAPI_MACHINE_NAME_LENGTH];
  NWSAPI_Port machinePort;
} NWSAPI_HostSpec;


/*
** Predefined names for the resources that the NWS generally monitors.  Other
** resources may be monitored, depending the configuration of the NWS system.
** Exactly which information is available can be determined by examining series
** registered with the NWS name server (see GetObjects()).
*/
#define NWSAPI_RESOURCE_NAME_LENGTH (31 + 1)
#define NWSAPI_DEFAULT_AVAILABLE_CPU_RESOURCE "availableCpu"
#define NWSAPI_DEFAULT_BANDWIDTH_RESOURCE     "bandwidthTcp"
#define NWSAPI_DEFAULT_CURRENT_CPU_RESOURCE   "currentCpu"
#define NWSAPI_DEFAULT_DISK_RESOURCE          "freeDisk"
#define NWSAPI_DEFAULT_LATENCY_RESOURCE       "latencyTcp"
#define NWSAPI_DEFAULT_MEMORY_RESOURCE        "freeMemory"


/*
** A description of an NWS experiment series.  A source machine, a destination
** machine (ignored for single-machine resources), and a resource name.
*/
typedef struct {
  char sourceMachine[NWSAPI_MACHINE_NAME_LENGTH];
  char destinationMachine[NWSAPI_MACHINE_NAME_LENGTH];
  char resourceName[NWSAPI_RESOURCE_NAME_LENGTH];
} NWSAPI_SeriesSpec;


/* Used with GetForecasts() and GetMeasurements() to retrieve all numbers. */
#define NWSAPI_BEGINNING_OF_TIME 0.0


/*
** Checks the user's environment variables and initialization file for a
** setting for the variable #name#.  Returns its value if one is found, else
** returns #defaultValue#.  The value returned is volatile and will be
** overwritten by subsequent calls.
*/
const char *
NWSAPI_EnvironmentValue(const char *name,
                        const char *defaultValue);


/*
** Returns 1 or 0 depending on whether or not the resource named #resourceName#
** involves multiple machines (like bandwidth) or a single machine (like CPU).
*/
int
NWSAPI_IntermachineResource(const char *resourceName);


/*
** A convenience function for constructing series specifications.  The returned
** value is volatile and will be overwritten by subsequent calls.
*/
const NWSAPI_SeriesSpec *
NWSAPI_MakeSeriesSpec(const char *sourceMachine,
                      const char *destinationMachine,
                      const char *seriesName);


/*
** A convenience function for constructing host specifications.  #host# may be
** either a DNS name or an IP address image.  It may also include a trailing
** colon followed by a port number, in which case this port number is used in
** preference to #machinePort#.  The return value is volatile and will be
** overwritten by subsequent calls.
*/
const NWSAPI_HostSpec *
NWSAPI_MakeHostSpec(const char *host,
                    NWSAPI_Port machinePort);


/*
** Constructs and returns a name for #whichSeries# which can be passed to
** the memory and forecaster retrieval and storage functions.  The value
** returned is volatile and will be overwritten by subsequent calls.
** User-generated series (i.e., those stored via calls to PutMeasurements())
** may be given any name, but using this function to generate a name may make
** it easier to reconstruct the name for later retrieval.
*/
const char *
NWSAPI_SeriesName(const NWSAPI_SeriesSpec *whichSeries);


/*
** NWS forecaster API declarations. **
*/


/*
** Returns the name of the #methodIndex#th forecasting method used by the most
** recently contacted forecaster, or "" if #methodIndex# is out of range.
** This may differ from the value returned by the forecast_api MethodName
** function, since the contacted forecaster may be using a different version of
** the forecasting library, or may choose to use only a subset of the
** available methods.
*/
const char *
NWSAPI_ForecasterMethodName(unsigned int methodIndex);


/*
** Instructs the chosen forecaster (see UseForecaster) to begin generating
** forecasts for each series name specified in the #howMany#-long array
** #whichSeries#.  Calling this function before GetForecast() is not required,
** but may significantly speed the response of GetForecast() by allowing the
** forecaster to begin forecast generation ahead of time.  Returns 1 if
** successful, else 0.
*/
int
NWSAPI_GenerateForecasts(const char **whichSeries,
                         size_t howMany);


/*
** Retrieves a time series of forecast collections for measurements of the
** series named #seriesName# that have been generated since #sinceWhen# and
** returns them in the #atMost#-long array #whereTo#.  The most recent forecast
** is stored in the first element of #whereTo#, the oldest forecast in the
** last.  Stores the count of retrieved collections in #numberReturned# if it
** is non-NULL.  Returns 1 if successful, else 0.
*/
int
NWSAPI_GetForecasts(const char *seriesName,
                    double sinceWhen,
                    FORECASTAPI_ForecastCollection *whereTo,
                    size_t atMost,
                    size_t *numberReturned);
#define NWSAPI_GetForecast(which,where) \
        NWSAPI_GetForecasts(which, NWSAPI_BEGINNING_OF_TIME, where, 1, NULL)


/*
** Instructs the chosen forecaster (see UseForecaster) to generate a time
** series of forecast collections based on the #howMany#-long measurement
** series #measurements#.  Returns the generated series in the #atMost#-long
** array #whereTo#.  The oldest forecast is stored in the first element of
** #whereTo#, the most recent forecast in the last.  Stores the count of
** retrieved collections in #numberReturned# if it is non-NULL. #seriesName# is
** used to identify the set of measurements; later calls to InstantForecast()
** using the same name will incorporate both the old and new measurements in
** the forecast generation.  Returns 1 if successful, else 0.
*/
int
NWSAPI_InstantForecasts(const char *seriesName,
                        const FORECASTAPI_Measurement *measurements,
                        size_t howMany,
                        FORECASTAPI_ForecastCollection *whereTo,
                        size_t atMost,
                        size_t *numberReturned);
#define NWSAPI_InstantForecast(name,measures,many,where) \
        NWSAPI_InstantForecasts(name, measures, many, where, 1, NULL)


/*
** Directs the interface to contact #whichForecaster# to generate forecasts.
** By default the interface attempts to choose the best available forecaster
** for each forecast.  Returns 1 if successful, else 0.
*/
int
NWSAPI_UseForecaster(const NWSAPI_HostSpec *whichForecaster);


/*
** NWS memory API declarations **
*/


/*
** Contacts the memory associated with the series #seriesName# and instructs it
** to begin forwarding new measurements stored for the series.  Such
** "auto-fetched" measurements are made available via calls to the
** AutoFetchCheck() function.  Returns 1 if successful, else 0.
*/
int
NWSAPI_AutoFetchBegin(const char *seriesName);


/*
** If an auto-fetch measurement (see AutoFetchBegin()) has arrived since the
** prior call or arrives within #timeOut# seconds, returns 1 and copies the
** series name into the #nameLen#-long string #seriesName# and the measurement
** value into #seriesMeasurement#; else returns 0.
*/
int
NWSAPI_AutoFetchCheck(char *seriesName,
                      size_t nameLen,
                      NWSAPI_Measurement *seriesMeasurement,
                      unsigned long timeOut);


/*
** Contacts the memory associated with the series #seriesName# and instructs it
** to stop forwarding new measurements stored for the series.  If #seriesName#
** is an empty string, terminates forwarding of all auto-fetching.
*/
void
NWSAPI_AutoFetchEnd(const char *seriesName);


/*
** Retrieves up to #atMost# measurements for the series named #seriesName# that
** have been taken since #sinceWhen#.  If more than #atMost# measurements have
** been taken since #sinceWhen#, the most recent ones are returned.  Returns
** the measurements in the #atMost#-long array #whereTo# with the newest one in
** the first element and the oldest measurement in the last.  Stores the count
** of retrieved measurements in #numberReturned# if it is non-NULL.  Returns 1
** if successful, else 0.
*/
int
NWSAPI_GetMeasurements(const char *seriesName,
                       double sinceWhen,
                       FORECASTAPI_Measurement *whereTo,
                       size_t atMost,
                       size_t *numberReturned);
#define NWSAPI_GetMeasurement(which,where) \
        NWSAPI_GetMeasurements(which, NWSAPI_BEGINNING_OF_TIME, where, 1, NULL)


/*
** Contacts the chosen memory (see UseMemory) to store the #howMany#-long
** series of measurements #measurements# under the name #seriesName#.  Also
** registers the series with the NWS name server using #whichSeries#.  Returns
** 1 if successful, else 0.
*/
int
NWSAPI_PutMeasurements(const char *seriesName,
                       const NWSAPI_SeriesSpec *whichSeries,
                       const FORECASTAPI_Measurement *measurements,
                       size_t howMany);


/*
** Directs the interface to contact #whichMemory# to store measurements.  By
** default the interface attempts to choose the best available memory for each
** set of measurements.  Returns 1 if successful, else 0.
*/
int
NWSAPI_UseMemory(const NWSAPI_HostSpec *whichMemory);


/*
** NWS name server API declarations **
*/

/*
** Definitions of name server attributes, objects, and sets of objects.  Treat
** these as opaque types -- they'll change as LDAP integration proceeds.
*/
typedef char *NWSAPI_Attribute;
typedef char *NWSAPI_Object;
typedef char *NWSAPI_ObjectSet;


/*
** Attributes associated with NWS objects.  The items listed here should be
** considered an experimental set that may change as LDAP integration proceeds.
*/
#define NWSAPI_ACTIVITIES  "(objectclass=nwsActivity)"
#define NWSAPI_CLIQUES     "(&(objectclass=nwsActivity)(controlName=clique))"
#define NWSAPI_CONTROLS    "(objectclass=nwsControl)"
#define NWSAPI_FORECASTERS "(hostType=forecaster)"
#define NWSAPI_HOSTS       "(objectclass=nwsHost)"
#define NWSAPI_MEMORIES    "(hostType=memory)"
#define NWSAPI_SENSORS     "(hostType=sensor)"
#define NWSAPI_SERIES      "(objectclass=nwsSeries)"
#define NWSAPI_SKILLS      "(objectclass=nwsSkill)"
#define NWSAPI_CLASS_ATTR      "objectclass"
#define NWSAPI_CONTROL_ATTR    "controlName"
#define NWSAPI_HOST_ATTR       "host"
#define NWSAPI_IP_ATTR         "ipAddress"
#define NWSAPI_MEMBER_ATTR     "member"
#define NWSAPI_MEMORY_ATTR     "memory"
#define NWSAPI_NAME_ATTR       "name"
#define NWSAPI_OPTION_ATTR     "option"
#define NWSAPI_PERIOD_ATTR     "period"
#define NWSAPI_PORT_ATTR       "port"
#define NWSAPI_RESOURCE_ATTR   "resource"
#define NWSAPI_SKILL_ATTR      "skillName"
#define NWSAPI_TARGET_ATTR     "target"
#define NWSAPI_UNIT_LABEL_ATTR "unitLabel"


/*
** These two routines retrieve and return, respectively, the name and value
** associated with their parameter attribute.
*/
const char *
NWSAPI_AttributeName(NWSAPI_Attribute attribute);
const char *
NWSAPI_AttributeValue(NWSAPI_Attribute attribute);


/*
** These two functions and four macros are used to iterate through the object
** set returned by a successful call to GetObjects().  The NextObject function
** iterates through each object in the set; the NextAttribute function iterates
** through each attribute in an object.  Each function returns NULL when no
** objects/attributes remain.
*/
NWSAPI_Attribute
NWSAPI_NextAttribute(NWSAPI_Object object,
                     NWSAPI_Attribute current);
#define NWSAPI_FirstAttribute(object) \
        NWSAPI_NextAttribute(object, NULL)
#define NWSAPI_ForEachAttribute(object,attribute) \
  for(attribute = NWSAPI_FirstAttribute(object); \
      attribute != NULL; \
      attribute = NWSAPI_NextAttribute(object, attribute))
NWSAPI_Object
NWSAPI_NextObject(NWSAPI_ObjectSet objects,
                  NWSAPI_Object current);
#define NWSAPI_FirstObject(objectSet) \
        NWSAPI_NextObject(objectSet, NULL)
#define NWSAPI_ForEachObject(objectSet,object) \
  for(object = NWSAPI_FirstObject(objectSet); \
      object != NULL; \
      object = NWSAPI_NextObject(objectSet, object))


/*
** Reclaims the space occupied by #toBeFreed#.
*/
void
NWSAPI_FreeObjectSet(NWSAPI_ObjectSet *toBeFreed);


/*
** A convenience function that calls GetObjects() in order to retieve info
** about NWS hosts.  Retrieves up to #atMost# NWS host objects that match
** #filter#.  Copies the retrieved hosts into #whereTo# and stores the count of
** retrieved hosts in #numberReturned#.  Returns 1 if successful, else 0.
*/
int
NWSAPI_GetHosts(const char *filter,
                NWSAPI_HostSpec *whereTo,
                size_t atMost,
                size_t *numberReturned);


/*
** Contacts the NWS host #whoToAsk# and returns in #nameServer# the NWS name
** server with which it is registered.  Returns 1 if successful, else 0.
*/
int
NWSAPI_GetNameServer(const NWSAPI_HostSpec *whoToAsk,
                     NWSAPI_HostSpec *nameServer);


/*
** Contacts the selected NWS name server (see UseNameServer()) in order to
** retrieve information about registered objects that match the characteristics
** specified in #filter#.  Returns a representation of the matching objects in
** #whereTo#; after a successful call, this value may be passed to
** {First,Next}Object() in order to extract individual objects.  After the
** caller is done with the object set it pass it to NWSFreeOjbects() so that
** the space can be reclaimed.  Returns 1 if successful, else 0.
*/
int
NWSAPI_GetObjects(const char *filter,
                  NWSAPI_ObjectSet *whereTo);


/*
** Directs the interface to contact #whichNS# when storing and retrieving
** registration information.  Returns 1 if successful, else 0.
*/
int
NWSAPI_UseNameServer(const NWSAPI_HostSpec *whichNS);


/*
** NWS sensor API declarations **
*/


/*
** Asks #whichSensor# to terminate the activity registered as #activityName#.
** Returns 1 if successful, else 0.
*/
int
NWSAPI_HaltActivity(const NWSAPI_HostSpec *whichSensor,
                    const char *activityName);


/*
** Contacts #whichSensor# to use #controlName# to start a #skillName# activity,
** configured using the elements of the #howManyOptions#-long array #options#.
** Each element of this array is a name:value pair.  Uses #activityName# to
** register the activity with the name server.  Returns 1 if successful, else 0.
*/
int
NWSAPI_StartActivity(const NWSAPI_HostSpec *whichSensor,
                     const char *activityName,
                     const char *controlName,
                     const char *skillName,
                     const char **options,
                     size_t howManyOptions);


#ifdef NWSAPI_SHORTNAMES

#ifndef FORECASTAPI_SHORTNAMES
#define Measurement NWSAPI_Measurement
#define Forecast NWSAPI_Forecast
#define FORECAST_TYPE_COUNT NWSAPI_FORECAST_TYPE_COUNT
#define MAE_FORECAST NWSAPI_MAE_FORECAST
#define MSE_FORECAST NWSAPI_MSE_FORECAST
#define ForecastCollection NWSAPI_ForecastCollection
#define ForecastStateStruct NWSAPI_ForecastStateStruct
#define ForecastState NWSAPI_ForecastState
#define FreeForecastState NWSAPI_FreeForecastState
#define MethodName NWSAPI_MethodName
#define NewForecastState NWSAPI_NewForecastState
#define UpdateForecastState NWSAPI_UpdateForecastState
#endif

#define MACHINE_NAME_LENGTH NWSAPI_MACHINE_NAME_LENGTH
#define Port NWSAPI_Port
#define HostSpec NWSAPI_HostSpec
#define RESOURCE_NAME_LENGTH NWSAPI_NWSAPI_RESOURCE_NAME_LENGTH
#define DEFAULT_AVAILABLE_CPU_RESOURCE NWSAPI_DEFAULT_AVAILABLE_CPU_RESOURCE
#define DEFAULT_BANDWIDTH_RESOURCE NWSAPI_DEFAULT_BANDWIDTH_RESOURCE
#define DEFAULT_CURRENT_CPU_RESOURCE NWSAPI_DEFAULT_CURRENT_CPU_RESOURCE
#define DEFAULT_LATENCY_RESOURCE NWSAPI_DEFAULT_LATENCY_RESOURCE
#define DEFAULT_MEMORY_RESOURCE NWSAPI_DEFAULT_MEMORY_RESOURCE
#define SeriesSpec NWSAPI_SeriesSpec
#define BEGINNING_OF_TIME NWSAPI_BEGINNING_OF_TIME
#define EnvironmentValue NWSAPI_EnvironmentValue
#define IntermachineResource NWSAPI_IntermachineResource
#define MakeSeriesSpec NWSAPI_MakeSeriesSpec
#define MakeHostSpec NWSAPI_MakeHostSpec
#define METHOD_NAME_LENGTH NWSAPI_METHOD_NAME_LENGTH
#define ForecasterMethodName NWSAPI_ForecasterMethodName
#define GenerateForecasts NWSAPI_GenerateForecasts
#define GetForecasts NWSAPI_GetForecasts
#define GetForecast NWSAPI_GetForecast
#define InstantForecasts NWSAPI_InstantForecasts
#define InstantForecast NWSAPI_InstantForecast
#define UseForecaster NWSAPI_UseForecaster
#define AutoFetchBegin NWSAPI_AutoFetchBegin
#define AutoFetchCheck NWSAPI_AutoFetchCheck
#define AutoFetchEnd NWSAPI_AutoFetchEnd
#define GetMeasurements NWSAPI_GetMeasurements
#define GetMeasurement NWSAPI_GetMeasurement
#define PutMeasurements NWSAPI_PutMeasurements
#define SeriesName NWSAPI_SeriesName
#define UseMemory NWSAPI_UseMemory
#define Attribute NWSAPI_Attribute
#define Object NWSAPI_Object
#define ObjectSet NWSAPI_ObjectSet
#define ACTIVITIES NWSAPI_ACTIVITIES
#define CLIQUES NWSAPI_CLIQUES
#define CONTROLS NWSAPI_CONTROLS
#define FORECASTERS NWSAPI_FORECASTERS
#define HOSTS NWSAPI_HOSTS
#define MEMORIES NWSAPI_MEMORIES
#define SENSORS NWSAPI_SENSORS
#define SERIES NWSAPI_SERIES
#define SKILLS NWSAPI_SKILLS
#define CLASS_ATTR NWSAPI_CLASS_ATTR
#define CONTROL_ATTR NWSAPI_CONTROL_ATTR
#define HOST_ATTR NWSAPI_HOST_ATTR
#define IP_ATTR NWSAPI_IP_ATTR
#define MEMBER_ATTR NWSAPI_MEMBER_ATTR
#define MEMORY_ATTR NWSAPI_MEMORY_ATTR
#define NAME_ATTR NWSAPI_NAME_ATTR
#define OPTION_ATTR NWSAPI_OPTION_ATTR
#define PERIOD_ATTR NWSAPI_PERIOD_ATTR
#define PORT_ATTR NWSAPI_PORT_ATTR
#define RESOURCE_ATTR NWSAPI_RESOURCE_ATTR
#define SENSOR_ATTR NWSAPI_SENSOR_ATTR
#define SKILL_ATTR NWSAPI_SKILL_ATTR
#define SOURCE_ATTR NWSAPI_SOURCE_ATTR
#define TARGET_ATTR NWSAPI_TARGET_ATTR
#define UNIT_LABEL_ATTR NWSAPI_UNIT_LABEL_ATTR
#define AttributeName NWSAPI_AttributeName
#define AttributeValue NWSAPI_AttributeValue
#define NextAttribute NWSAPI_NextAttribute
#define FirstAttribute NWSAPI_FirstAttribute
#define ForEachAttribute NWSAPI_ForEachAttribute
#define NextObject NWSAPI_NextObject
#define FirstObject NWSAPI_FirstObject
#define ForEachObject NWSAPI_ForEachObject
#define FreeObjectSet NWSAPI_FreeObjectSet
#define GetHosts NWSAPI_GetHosts
#define GetNameServer NWSAPI_GetNameServer
#define GetObjects NWSAPI_GetObjects
#define UseNameServer NWSAPI_UseNameServer
#define HaltActivity NWSAPI_HaltActivity
#define StartActivity NWSAPI_StartActivity

#endif


#ifdef	__cplusplus
}
#endif


#endif
