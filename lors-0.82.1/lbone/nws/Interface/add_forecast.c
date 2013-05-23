/* $Id: add_forecast.c,v 1.2 2000/06/28 19:15:54 swany Exp $ */

#include <ctype.h>        /* isspace() */
#include <stdio.h>        /* file functions */
#include <stdlib.h>       /* strtod() */
#include <string.h>       /* strlen() */
#define FORECASTAPI_SHORTNAMES
#include "config.h"
#include "forecast_api.h" /* NWS forecasting interface */


/*
** This filter takes from standard input a series of lines that contain
** time-stamp/measurement pairs and prints to standard output these lines with
** MSE forecast values and errors inserted after the measurement.  Any text
** trailing the number pair is considered the series name, and multiple series
** may be forecast in a single execution by appending the series name after the
** first number pair in each series.  (A line without a series name is
** considered to belong to the same series as the preceding line.)  Any lines
** that begin with something other than two numbers are echoed without
** interpretation.  For example:
**   nws_extract -h 0 -f time,meas,res,sou avail green.ufo.edu | add_forecast
** will give the same output as
**   nws_extract -h 0 -f time,meas,mse_f,mse_e,res,sou avail green.ufo.edu
*/


#define LINE_LENGTH (127 + 1)


int
main(int argc,
     char **argv) {

  typedef struct {
    char *name;
    ForecastState *forecastState;
  } seriesInfo;

  seriesInfo *currentSeries = NULL;
  ForecastCollection forecast;
  int i;
  char inputLine[LINE_LENGTH];
  Measurement measurement;
  char *nextWord;
  seriesInfo *series = NULL;
  unsigned seriesCount = 0;

  while(fgets(inputLine, sizeof(inputLine), stdin) != NULL) {

    measurement.timeStamp = strtod(inputLine, &nextWord);
    measurement.measurement = strtod(nextWord, &nextWord);

    if(!isspace((int)*nextWord)) {
      /* Pass along lines that begin with something other than two numbers. */
      printf("%s", inputLine);
      continue;
    }

    while(isspace((int)*nextWord) && (*nextWord != '\n'))
      nextWord++;

    if((currentSeries == NULL) || (*nextWord != '\n')) {
      for(i = 0, currentSeries = series;
          (i < seriesCount) && (strcmp(nextWord, currentSeries->name) != 0);
          i++, currentSeries++)
        ; /* Nothing more to do. */
      if(i == seriesCount) {
        seriesCount++;
        series = REALLOC(series, sizeof(seriesInfo) * seriesCount);
        if(series == NULL) {
          fprintf(stderr, "Out of memory\n");
          exit(1);
        }
        currentSeries = &series[i];
        currentSeries->name = strdup(nextWord);
        currentSeries->forecastState = NewForecastState();
      }
    }

    UpdateForecastState
      (currentSeries->forecastState, &measurement, 1, &forecast, 1);

    printf("%d %f %f %f %s", (int)forecast.measurement.timeStamp,
                             forecast.measurement.measurement,
                             forecast.forecasts[MSE_FORECAST].forecast,
                             forecast.forecasts[MSE_FORECAST].error,
                             nextWord);
    fflush(stdout);

  }

  return 0;

}
