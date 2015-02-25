/* $Id: passiveCpu.h,v 1.7 2000/01/25 01:00:48 hayes Exp $ */


/*
** Defines a passive cpu monitor which works by parsing Unix utility output.
*/


/*
** Shuts down the passive cpu monitor.
*/
int
PassiveCpuCloseMonitor(void);


/*
** Attempts to determine, via a query of #resource#, the fraction of a cpu
** available to a new or existing process with priority #niceValue#.
** #resource# must be a value in the range 0 .. cpuResourceCount() - 1.  If
** successful, sets #newFraction# and #existingFraction#, respectively, to
** best-guess estimates for the new and existing values and returns 1; else
** returns 0.
*/
int
PassiveCpuGetLoad(int resource,
                  unsigned short niceValue,
                  double *newFraction,
                  double *existingFraction);


/*
** Returns 1 or 0 depending on whether or not the O/S provides facilities for
** passive CPU monitoring.
*/
int
PassiveCpuMonitorAvailable(void);


/*
** Spawns a passive cpu monitor; must be called before any calls to
** cpuGetLoad().  #sleeptime# specifies the amount of time the monitor should
** sleep between load checks.  Returns 1 if successful; 0 otherwise.
*/
int
PassiveCpuOpenMonitor(int sleeptime);


/*
** An internal procedure of PassiveCpuGetLoad() that can be used externally if
** utility output is available.  Calls #GetNextLine()#, passing #resource#, as
** many times as needed to determine CPU availability for new and existing
** nice-#niceValue# processes.  Returns 1 and sets #newFraction# and
** #existingFraction# if successful, else returns 0.
*/
int
PassiveCpuParse(int resource,
                unsigned short niceValue,
                const char *GetNextLine(int resource),
                double *newFraction,
                double *existingFraction);


/*
** Returns the number of passive CPU load-testing resources available.
*/
int
PassiveCpuResourceCount(void);


/*
** Returns a string that represents CPU load-testing resource #resource#, which
** must be a value in the range 0 .. cpuResourceCount() - 1.  The value
** returned is volatile and will be overwritten by subsequent calls.
*/
const char *
PassiveCpuResourceName(int resource);
