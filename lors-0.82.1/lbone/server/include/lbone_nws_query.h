/* the include file for getBandwidthMatirx.c
 * By Linzhen Xuan
 * Date: Thu Jun 27 16:39:31 EDT 2002
 */

#include <ctype.h>       /* isalpha() isspace() */
#include <stdio.h>       /* printf() */
#include <unistd.h>
#include <stdlib.h>      /* atoi() free() malloc() */
#include <sys/types.h>   /* size_t */
#include <stdarg.h>      /* Variable parameter stuff. */
#include <string.h>      /* strchr(), strncasecmp() strstr() */
#include <strings.h>     /* strncasecmp() on aix */
/*#include "config.h"*/
#define NWSAPI_SHORTNAMES
#include "nws_api.h"     /* NWS programming interface */
/*#include "lbone_client_lib.h"*/

#define GETDEBUG
#define MAX_MACHINE_NAME 64
#define LBONE_MAX_PORT  65535
#define SAFE_STRCPY(to, from) \
  do {strncpy(to, from, sizeof(to)); to[sizeof(to) - 1] = '\0'; if (1) break;} while (0)

#define NWS_SERVER_HOST "adder.cs.utk.edu"/*should be delete and use some where to have this line*/
#define SIGNIFICANT_HISTORY 1000
/* Defaults for command-line values and NWS host ports. */
#define DEFAULT_NAME_SERVER_PORT "8090"

/*
 * Info about series used to compute a running forecast and map series names
 * back to host and resource names.
 */
typedef struct {
  char name[256];
  SeriesSpec series;
  ForecastState *forecast;
} SeriesInfo;


#define MAX_FILE_NAME  256
