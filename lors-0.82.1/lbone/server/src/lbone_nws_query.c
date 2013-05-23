/* Some code are from nws_extract.c,v 1.60
 * By Linzhen Xuan
 * Thu Jun 20 23:48:56 EDT 2002
 *
 * Modified by Scott Atchley to accept different source and target lists
 * Tue Jul 23 2002
 */

#include "lbone_nws_query.h"

/*
 * Appends a variable number of items of different data types
 */

static int
MultiCat(char *dest,
         size_t len,
         int count,
         ...)
{

    va_list paramList;
    int i;
    const char *source;
    char *end = dest + len - 1;
    char *next;

    next = dest + strlen(dest);
    va_start(paramList, count);
    for (i = 0; i < count; i++)
    {
        for (source = va_arg(paramList, const char*);
           (next < end) && (*source != '\0'); next++, source++)
        *next = *source;
    }
    *next = '\0';
    va_end(paramList);
    return next - dest;
}


/*
 * Look through the an array #series# to make sure
 * source-destination pair has an entry
 *
 * If entry exists, return success (0)
 * If not, return -1
 */
static int
CheckSeriesCov(const SeriesInfo *series,
               size_t howManySeries,
               const char *sources,
               const char *dests,
               const char *resourceName)
{

    const SeriesInfo *currentSeries;
    const SeriesInfo *endOfSeries=series + howManySeries;

    for(currentSeries = series; currentSeries < endOfSeries; currentSeries++)
    {
        if(strncmp(currentSeries->series.sourceMachine, sources, strlen(sources)) == 0)
        {
            if(strncmp(currentSeries->series.destinationMachine, dests, strlen(dests)) == 0)
                /* value found, return success (0) */
                return 0;
        }
    }

    /* no value found for pair, return -1 */
    /* fprintf(stderr, "Warning: no %s series available from %s to %s.\n",
            resourceName, sources, dests); */
    return -1;
}

/*
 * Fetches measurements for #seriesName# taken since #sinceWhen# and uses them
 * to compute in #stateToUse# and return up to #atMost# forecasts in #whereTo#.
 * If successful, returns 1 and sets #numberReturned# to the number of
 * forecasts copied into #where#; otherwise, returns 0.
 */
int
ExtractForecastone(const char *seriesName,
                   double sinceWhen,
                   ForecastState *stateToUse,
                   FORECASTAPI_ForecastCollection *whereTo,
                   size_t *numberReturned)
    {

    size_t historySize;
    static Measurement *measurements = NULL;

    /*
     * Retrieve enough history to generate reasonably accurate forecasts even if
     * the user wants only a few measurements,
     */
    historySize = SIGNIFICANT_HISTORY;

    if(measurements == NULL)
    {
        measurements = (Measurement *)malloc(historySize * sizeof(Measurement));
        if(measurements == NULL)
        {
            fprintf(stderr, "ExtractForecast: malloc failed\n");
            return 0;
        }
    }


    if(!GetMeasurements(seriesName,
                        sinceWhen,
                        measurements,
                        historySize,
                        &historySize))
    {
        fprintf(stderr, "ExtractForecast: unable to retrieve measurements for series %s\n",
                seriesName);
        return 0;
    }
    UpdateForecastState(stateToUse, measurements, historySize, whereTo, 1);
    *numberReturned = (historySize==0)?0:1;
    return 1;
}

/*
 * Retrieves from the name server registrations for all series matching
 * #filter# that have a source equal to #sources# and a destination equal to
 * #dests#. Returns an array of series info for the retrieved
 * registration and sets #seriesCount# to its length.  Returns NULL on error.
 */
static SeriesInfo *
RetrieveSeriesInf(const char *sources,
                  const char *dests,
                  int *seriesCount)
{

    const char *attributeName;
    SeriesInfo currentSeries;
    char fullFilter[4095 + 1] = "";
    SeriesInfo *returnValue;
    Attribute seriesAttribute;
    Object seriesObject;
    ObjectSet seriesObjectSet;

    /*
     * Combine #filter#, #sources# and, #dests# into a name server
     * retrieval filter:
     * (&(objectclass=nwsSeries)<filter>(|(source=<source>)...)(|(dest=<dest)...))
     */
    MultiCat(fullFilter, sizeof(fullFilter), 8, "(&", SERIES,
             "(", RESOURCE_ATTR, "=", "bandwidthTcp", ")", "(|");
    MultiCat(fullFilter, sizeof(fullFilter), 5,
               "(", HOST_ATTR, "=", sources,
               (strchr(sources, ':') != NULL) ? ")" :
               (strchr(sources, '.') != NULL) ? ":*)" : ".*)");

    MultiCat(fullFilter, sizeof(fullFilter), 2, ")", "(|");
    MultiCat(fullFilter, sizeof(fullFilter), 5, 
           "(", TARGET_ATTR, "=", dests, 
            (strchr(sources, ':') != NULL) ? ")" : 
            (strchr(sources, '.') != NULL) ? ":*)" : ".*)");

    if(GetObjects(fullFilter, &seriesObjectSet)!=1)
    {
        fprintf(stderr, "RetrieveSeriesInfo: series retrieval failed\n");
        return NULL;
    }

    *seriesCount = 0;
    ForEachObject(seriesObjectSet, seriesObject)
    {
        (*seriesCount)++;
    }
    returnValue = malloc(*seriesCount * sizeof(SeriesInfo));

    if(returnValue == NULL)
    {
        FreeObjectSet(&seriesObjectSet);
        fprintf(stderr, "RetrieveSeriesInf: malloc failed\n");
        return NULL;
    }

    /* Parse each retrieved series into a SeriesInfo struct. */
    *seriesCount = 0;
    ForEachObject(seriesObjectSet, seriesObject)
    {

        SAFE_STRCPY(currentSeries.series.sourceMachine, "");
        SAFE_STRCPY(currentSeries.series.destinationMachine, "");
        SAFE_STRCPY(currentSeries.series.resourceName, "");
        SAFE_STRCPY(currentSeries.name, "");
        currentSeries.forecast = NewForecastState();

        ForEachAttribute(seriesObject, seriesAttribute)
        {
            attributeName = AttributeName(seriesAttribute);
            if(strcmp(attributeName, TARGET_ATTR) == 0)
            {
                SAFE_STRCPY(currentSeries.series.destinationMachine,
                            AttributeValue(seriesAttribute));
            }
            else if(strcmp(attributeName, NAME_ATTR) == 0)
            {
                SAFE_STRCPY(currentSeries.name, AttributeValue(seriesAttribute));
            }
            else if(strcmp(attributeName, RESOURCE_ATTR) == 0)
            {
                SAFE_STRCPY(currentSeries.series.resourceName,
                            AttributeValue(seriesAttribute));
            }
            else if(strcmp(attributeName, HOST_ATTR) == 0)
            {
                SAFE_STRCPY(currentSeries.series.sourceMachine,
                            AttributeValue(seriesAttribute));
            }

        }

        returnValue[(*seriesCount)++] = currentSeries;

    }

    FreeObjectSet(&seriesObjectSet);
    return returnValue;
}

/*
 * Get the bandwidth information among the depots contained in #allDepots#. An pointer to
 * an array of double data.
 * Arguments: int totalHost: the number of the hosts.
 *            *allDepots the array of pointers the depost name string.
 * Return value: a pointer to an array of bandwith data. getBandwidthMatrix[seq] represents
 *               the bandwidth from seq/totalHost to seq%totalHost.
 */
/* extern double *getBandwidthMatrix(int totalHost, char **hostArray) */
extern double **getBandwidthMatrix(char **src_list, 
                                   int src_count, 
                                   char **target_list, 
                                   int target_count)
{
    /* User-settable parameters. */
    double      sinceWhen       = BEGINNING_OF_TIME;

    /* Other local variables. */
    int         extractCount    = 0;
    SeriesInfo  *extractSeries  = NULL;
    ForecastCollection *forecasts= NULL;
    HostSpec    host;
    int         i = 0;
    int         j = 0;
    int         k = 0;
    const char  *resourceName;
    size_t      returnedCount   = 0;
    double      **bandwidth     = NULL;
    double      lasttimestamp   = 0;

    host = *MakeHostSpec (NWS_SERVER_HOST, atoi(EnvironmentValue("NAME_SERVER_PORT", 
                                                DEFAULT_NAME_SERVER_PORT)));

    if(!UseNameServer(&host))
    {
        fprintf(stderr, "Unable to contact name server %s:%d\n",
                host.machineName, host.machinePort);
        return NULL;
        /* exit(1); */
    }

    resourceName = strdup(DEFAULT_BANDWIDTH_RESOURCE);

    if ((src_count == 0) || (target_count == 0))
    {
        fprintf(stderr, "No any host?\n");
        return NULL;
    }

    /*
    if(!(BandWidthes=(double *)malloc(sizeof(double)*totalHost*totalHost)))
    {
        fprintf(stderr, "malloc failed in getBandWidth.\n");
        exit(1);
    }
    */

    bandwidth = (double **) calloc(sizeof (double *), src_count);
    if ( bandwidth == NULL ) return NULL;

    for (i=0; i < src_count; i++)
    {
        bandwidth[i] = (double *) calloc(sizeof(double), target_count);
        if ( bandwidth[i] == NULL )
        {
            for (k = i; k >= 0; k--)
            {
                if (bandwidth[k] != NULL) free(bandwidth[k]);
            }
            if (bandwidth != NULL) free(bandwidth);
            return NULL;
        }
    }

    for(j = 0; j < src_count; j++)
    {
        for(k = 0; k < target_count; k++)
        {
            bandwidth[j][k] = 0;
            /* if(j == k) continue; */
            if (strcmp(src_list[j], target_list[k]) == 0) continue;

            /* Get the information about the specified
            series from the name server. */
            extractSeries = RetrieveSeriesInf(src_list[j], target_list[k], &extractCount);
            if(extractSeries == NULL) continue;
            if((CheckSeriesCov(extractSeries,
                               extractCount,
                               src_list[j],
                               target_list[k],
                               resourceName) ) == -1) continue;

            forecasts = (ForecastCollection *)
                malloc(sizeof(ForecastCollection));
            if(forecasts == NULL)
            {
                fprintf(stderr, "malloc failed\n");
                exit(1);
            }

            /* Compute the forcasted value. */
            for(i = 0; i < extractCount; i++)
            {
                if(ExtractForecastone(extractSeries[i].name,
                                      sinceWhen,
                                      extractSeries[i].forecast,
                                      forecasts,
                                      &returnedCount))
                {
                    if(returnedCount > 0)
                    {
                        if(lasttimestamp < forecasts->measurement.timeStamp)
                            bandwidth[j][k]=forecasts->measurement.measurement;
                    } else {
                        fprintf(stderr, "No measurements from %s qualified.\n",
                            extractSeries[i].series.sourceMachine);
                    }
                }
            }
        }
    }

    free(forecasts);

    /* Tidy up. */
    for(i = 0; i < extractCount; i++)
    {
        if(extractSeries != NULL)FreeForecastState(&extractSeries[i].forecast);
    }
    if(extractSeries != NULL)free(extractSeries);

    return bandwidth;
}

