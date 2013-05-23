/* $Id: diagnostic.c,v 1.20 1999/08/24 22:33:54 hayes Exp $ */

#include <stdarg.h>    /* va_*, vfprintf() */
#include <stdio.h>     /* sprintf() */
#include <string.h>    /* strncpy() */
#include <sys/types.h> /* time_t */
#include <time.h>      /* time() */
#include <unistd.h>    /* getpid() */
#include "diagnostic.h"

#define MAXRECORDS 2500

static FILE *diagDestinations[] =
  {DIAGSUPPRESS, DIAGSUPPRESS, DIAGSUPPRESS,
   DIAGSUPPRESS, DIAGSUPPRESS, DIAGSUPPRESS};
static long recordCounts[] =
  {0, 0, 0, 0, 0, 0};


static void
PrintDiagnostic(DiagLevels level,
                const char *message,
                va_list arguments) {

  static const char *level_tags[] =
    {"", "", "Warning: ", "Error: ", "Fatal: ", "Debug: " };

  if(diagDestinations[level] == DIAGSUPPRESS)
    return;

  if( (recordCounts[level]++ >= MAXRECORDS) &&
      (diagDestinations[level] != stdout) &&
      (diagDestinations[level] != stderr) ) {
    /*
    ** We want to avoid filling up the disk space when the system is running
    ** for weeks at a time.  It might be nice to save the old file under
    ** another name (maybe in /tmp), then reopen.  That requires changing the
    ** DirectDiagnostics() interface to take a file name instead of a FILE *.
    */
    rewind(diagDestinations[level]);
    recordCounts[level] = 0;
  }
  fprintf(diagDestinations[level], "%ld %d ", (long)time(NULL), (int)getpid());
  fprintf(diagDestinations[level], level_tags[level]);
  vfprintf(diagDestinations[level], message, arguments);
  fflush(diagDestinations[level]);

}


void
DirectDiagnostics(DiagLevels level,
                  FILE *whereTo) {
  diagDestinations[level] = whereTo;
}


FILE *
DiagnosticsDirection(DiagLevels level) {
  return diagDestinations[level];
}


void
PositionedDiagnostic(DiagLevels level,
                     const char *fileName, 
                     int line,
                     const char *message,
                     ...) {

  va_list arguments;
  char extendedMessage[512];
  char lineImage[10];

  sprintf(lineImage, ":%d ", line);
  va_start(arguments, message);
  if((strlen(fileName) + strlen(lineImage) + strlen(message)) <
     sizeof(extendedMessage)) {
    sprintf(extendedMessage, "%s%s%s", fileName, lineImage, message);
    PrintDiagnostic(level, extendedMessage, arguments);
  }
  else {
    PrintDiagnostic(level, message, arguments);
  }
  va_end(arguments);

}


void
Diagnostic(DiagLevels level,
           const char *message,
           ...) {
  va_list arguments;
  va_start(arguments, message);
  PrintDiagnostic(level, message, arguments);
  va_end(arguments);
}
