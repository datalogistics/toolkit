#ifndef MEMORY_CUSTOM_H
#define MEMORY_CUSTOM_H

#include "config.h"

/* the increment in the allocation loop */
#define CALIBRATION_BULK_SIZE 1024*1024  /* 1 MB */
/* the precision of the base memory measurement */
#define ITERATIONS_STALLED 16
/* the initial number of free memory measurements before a new calibration */
/* this delay increases as a geometric serie of factor 2 */
#define INIT_COUNT_BETWEEN_CALIBRATION 128 /* ~20 minutes for 10s period */
/* the maximum delay between calibrations */
#define MAX_COUNT_BETWEEN_CALIBRATION 65536  /* about one week for 10s period */
/* the minimum availability of the cpu to process safely a calibration */
#define MIN_CPU_FOR_CALIBRATION 0.9  /* 90% */


#if LINUX

#define PS_ARGS "av"         /* arguments for "ps" */
#define PS_DELIM " \n"       /* the delimiters to parse the ps output */
#define STATUS_FIELD "STAT"  /* the title of the status field */
#define RSS_FIELD "RSS"      /* the title of the resident size field */
#define STATUS_VALID "RDTSZ"  /* the list of valid status to parse */
#define VMSTAT VMSTAT_PATH   /* the path to vmstat */
#define VMSTAT_DELIM " \n"   /* the delimiters to parse the vmstat output */
#define VMSTAT_FREE "free"   /* the title of the FREE field */
#define CALIBRATION_SPEEDUP_RATIO (0.8) /* the ratio of the rough estimate from vmstat to speed up the calibration */
#define MEMSOR_UNIT 1        /* the factor to get the result in KB */
#define MEMORY_SUPPORT 1     /* is there a memory sensor for this architecture ? */

#elif SOLARIS

#define PS_ARGS "-e -o s,rss"
#define PS_DELIM " \n"
#define STATUS_FIELD "S"
#define RSS_FIELD "RSS"
#define STATUS_VALID "SROZT"
#define VMSTAT VMSTAT_PATH
#define VMSTAT_DELIM " \n"
#define VMSTAT_FREE "free"
#define CALIBRATION_SPEEDUP_RATIO (0.8) 
#define MEMSOR_UNIT 1
#define MEMORY_SUPPORT 1

#elif AIX

#define PS_ARGS "gv"
#define PS_DELIM " \n"
#define STATUS_FIELD "STAT"
#define RSS_FIELD "RSS"
#define STATUS_VALID "AWIT"
#define VMSTAT VMSTAT_PATH
#define VMSTAT_DELIM " \n"
#define VMSTAT_FREE "free"
#define CALIBRATION_SPEEDUP_RATIO (0.8)
#define MEMSOR_UNIT 1
#define MEMORY_SUPPORT 1

#elif IRIX

#define PS_ARGS "-el"
#define PS_DELIM " :\n"
#define STATUS_FIELD "S"
#define RSS_FIELD "RSS"
#define STATUS_VALID "OZTIXCSR"
#define VMSTAT VMSTAT_PATH
#define VMSTAT_DELIM " \n"
#define VMSTAT_FREE "free"
#define CALIBRATION_SPEEDUP_RATIO (0.8)
#define MEMSOR_UNIT (getpagesize()/1024)
#define MEMORY_SUPPORT 1

#elif 
#warning Target OS is not supported for the active memory sensor
#define PS_ARGS ""
#define PS_DELIM ""
#define STATUS_FIELD ""
#define RSS_FIELD ""
#define STATUS_VALID ""
#define VMSTAT "" 
#define VMSTAT_DELIM ""
#define VMSTAT_FREE ""
#define CALIBRATION_SPEEDUP_RATIO 0.0
#define MEMSOR_UNIT 1
#define MEMORY_SUPPORT 0

#endif

#endif
