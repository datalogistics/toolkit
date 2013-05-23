/* $Id: osutil.c,v 1.18 2000/07/14 16:54:32 swany Exp $ */


#include "config.h"
#include <ctype.h>      /* isspace() */
#include <errno.h>      /* errno values */
#include <pwd.h>        /* endpwent() getpwuid() */
#include <signal.h>     /* sig{add,empty}set() sigprocmask() */
#include <stdio.h>      /* file functions */
#include <stdlib.h>     /* getenv() */
#include <string.h>     /* memset() strchr() strlen() */
#include <unistd.h>     /* alarm() getuid() sysconf() (where available) */
#include <sys/stat.h>   /* stat() */
#include <sys/time.h>   /* gettimeofday() */
#include <sys/types.h>  /* size_t time_t */
#include <time.h>       /* time() */
#include "osutil.h"


int
CPUCount( ) {
#ifdef HAVE_SYSCONF
#ifdef _SC_CRAY_NCPU
#define SYSCONF_PROCESSOR_COUNT_PARAMETER _SC_CRAY_NCPU
#elif defined(_SC_NPROC_CONF)
#define SYSCONF_PROCESSOR_COUNT_PARAMETER _SC_NPROC_CONF
#elif defined(_SC_NPROCESSORS_CONF)
#define SYSCONF_PROCESSOR_COUNT_PARAMETER _SC_NPROCESSORS_CONF
#endif
#ifdef SYSCONF_PROCESSOR_COUNT_PARAMETER 
  /* Try to gracefully handle mis-configuration. */
  int sysconfCount = sysconf(SYSCONF_PROCESSOR_COUNT_PARAMETER);
  return (sysconfCount == 0) ? 1 : sysconfCount;
#else
  return 1;
#endif
#else
  return 1;
#endif
}


double
CurrentTime(void) {
  return((double)time(NULL));
}


const char *
GetEnvironmentValue(const char *name,
                    const char *rcDirs,
                    const char *rcName,
                    const char *defaultValue) {

  const char *dirEnd;
  const char *dirStart;
  char *from;
  const char *homeDir;
  size_t nameLen;
  FILE *rcFile;
  static char rcLine[255 + 1];
  char rcPath[255 + 1];
  const char *returnValue;
  char *to;

  returnValue = getenv(name);
  if(returnValue != NULL) {
    return returnValue;
  }

  if(*rcName != '\0') {

    nameLen = strlen(name);

    for(dirStart = rcDirs, dirEnd = dirStart;
        dirEnd != NULL;
        dirStart = dirEnd + 1) {

      /* Construct a file path from the next dir in rcDirs and rcName. */
      dirEnd = strchr(dirStart, ':');
      memset(rcPath, '\0', sizeof(rcPath));
      strncpy(rcPath, dirStart,
              (dirEnd == NULL) ? strlen(dirStart) : (dirEnd - dirStart));
      if((strcmp(rcPath, "~") == 0) || (strcmp(rcPath, "~/") == 0)) {
        homeDir = getenv("HOME");
        if(homeDir != NULL) {
          strcpy(rcPath, homeDir);
        }
      }
      strcat(rcPath, "/");
      strcat(rcPath, rcName);

      rcFile = fopen(rcPath, "r");

      if(rcFile != NULL) {

        while(fgets(rcLine, sizeof(rcLine), rcFile) != NULL) {

          /* Test against pattern " *#name# *=". */
          for(from = rcLine; (*from != '\0') && isspace((int)*from); from++)
            ; /* Nothing more to do. */
          if(strncmp(from, name, nameLen) != 0) {
            continue;
          }
          for(from += nameLen; (*from != '\0') && isspace((int)*from); from++)
            ; /* Nothing more to do. */
          if(*from != '=') {
            continue;
          }

          /* We found a line that sets the variable. */
          (void)fclose(rcFile);
          for(from++; (*from != '\0') && isspace((int)*from); from++)
            ; /* Nothing more to do. */

          /* Return a single word to allow for future free-format input. */
          if(*from == '"') {
            returnValue = from + 1;
            for(from++, to = from;
                (*from != '\0') && (*from != '"');
                from++, to++) {
              if(*from == '\\') {
                from++;
                switch(*from) {
                case 'b':
                  *to = '\b';
                  break;
                case 'f':
                  *to = '\f';
                  break;
                case 'n':
                  *to = '\n';
                  break;
                case 'r':
                  *to = '\r';
                  break;
                case 't':
                  *to = '\t';
                  break;
                case 'v':
                  *to = '\v';
                  break;
                default:
                  *to = *from;
                  break;
                }
              }
              else {
                *to = *from;
              }
            }
          }
          else {
            returnValue = from;
            for(to = from; (*to != '\0') && !isspace((int)*to); to++)
              ; /* Nothing more to do. */
          }
          *to = '\0';

          return(returnValue);

        }

        (void)fclose(rcFile);

      }
      
    }

  }

  return(defaultValue);

}


int
GetUserName(char *name,
            size_t length) {
  struct passwd *myPasswd;
  myPasswd = getpwuid(getuid());
  if(myPasswd != NULL) {
    strncpy(name, myPasswd->pw_name, length);
    name[length - 1] = '\0';
  }
  endpwent();
  return(myPasswd != NULL);
}


void
HoldSignal(int sig) {
#ifdef HAVE_SIGHOLD
  sighold(sig);
#else
  sigset_t set, oset;
  sigemptyset(&set);
  sigaddset(&set, sig);\
  sigprocmask(SIG_BLOCK, &set, &oset);
#endif
}


int
MakeDirectory(const char *path,
              mode_t mode,
              int makeEntirePath) {

  const char *endSubPath;
  struct stat pathStat;
  char subPath[255 + 1];

  if(makeEntirePath) {
    endSubPath = path;  /* This will ignore leading '/' */
    while((endSubPath = strchr(endSubPath + 1, '/')) != NULL) {
      memset(subPath, '\0', sizeof(subPath));
      strncpy(subPath, path, endSubPath - path);
      if((stat(subPath, &pathStat) == -1) && (errno == ENOENT)) {
        if(mkdir(subPath, mode) == -1) {
          return 0;
        }
      }
      else if(!S_ISDIR(pathStat.st_mode)) {
        return 0;
      }
    }
  }

  if((stat(path, &pathStat) == -1) && (errno == ENOENT)) {
    return(mkdir(path, mode) != -1);
  }
  else {
    return(S_ISDIR(pathStat.st_mode));
  }
}



long
MicroTime(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return(tv.tv_sec * 1000000 + tv.tv_usec);
}


void
ReleaseSignal(int sig) {
#ifdef HAVE_SIGRELSE
  sigrelse(sig);
#else
  sigset_t set, oset;
  sigemptyset(&set);
  sigaddset(&set, sig);
  sigprocmask(SIG_UNBLOCK, &set, &oset);
#endif
}


void
SetRealTimer(unsigned int numberOfSecs) {
#ifdef HAVE_ALARM
  alarm(numberOfSecs);
#else
  ?
  /*
  ** The question mark above has been placed deliberately to cause a syntax
  ** error when compiling on a system that doesn't support alarm(2).  It is
  ** possible, though unlikely, that such a system supports setitimer(2).
  ** Because some sort of timer signal support is critcal to the NWS, an
  ** adapter must resolve this problem before the build can complete.
  */
#endif
}
