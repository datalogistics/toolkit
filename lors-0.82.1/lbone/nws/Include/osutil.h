/* $Id: osutil.h,v 1.9 2000/03/01 04:51:52 swany Exp $ */


#ifndef OSUTIL_H
#define OSUTIL_H


/*
** This package defines functions for working with operating system facilities.
*/


#include <sys/types.h>  /* mode_t size_t */


/*
** Returns a best-guess estimate of the number of CPUs on the system.
*/
int
CPUCount(void);


/*
** Returns the current time in number of seconds since midnight 1/1/1970, GMT.
*/
double
CurrentTime(void);


/*
** Returns the environment value for the variable #name#.  This value is
** determined in one of three ways: if an environment variable of that name is
** set, its value is used; otherwise, if a file named #rcName# in one of the
** colon-delimited set of directories #rcDirs# contains a line of the form
** #name#=value, then that value is used; otherwise #defaultValue# is used.
** For convenience, a directory of "~" in #rcDirs# is expanded to the contents
** of the HOME environment variable.
*/
const char *
GetEnvironmentValue(const char *name,
                    const char *rcDirs,
                    const char *rcName,
                    const char *defaultValue);


/*
** Attempts to determine the login id of the user running the binary.  Returns
** 1 and copies the name into the #length#-long array name if successful, else
** returns 0.
*/
int
GetUserName(char *name,
            size_t length);


/*
** Prevents the O/S from raising the signal #sig# until a subsequent call to
** ReleaseSignal() is made.
*/
void
HoldSignal(int sig);


/*
** Creates the directory specified by #path# with mode #mode#.  If
** #makeEntirePath# is non-zero, any missing subpaths in #path# will also be
** created.  Returns 1 if successful, else 0.
*/
int
MakeDirectory(const char *path,
              mode_t mode,
              int makeEntirePath);


/*
** Returns the number of microseconds that have elapsed since midnight, local
** time.
*/
long int
MicroTime(void);


/*
** Allows the O/S to raise the signal #sig#.
*/
void
ReleaseSignal(int sig);


/*
** Starts a timer that will raise a SIGALRM in #numberOfSecs# seconds.
*/
void
SetRealTimer(unsigned int numberOfSecs);
#define RESETREALTIMER SetRealTimer(0)


#endif
