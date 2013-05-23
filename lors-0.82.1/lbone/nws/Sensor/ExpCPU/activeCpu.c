/* $Id: activeCpu.c,v 1.8 2000/02/18 21:00:57 hayes Exp $ */

#include "config.h"
#include <sys/types.h>    /* pid_t */
#include <sys/wait.h>     /* wait() */
#include <setjmp.h>       /* {long,set}jmp() */
#include <signal.h>       /* signal() */
#include <unistd.h>       /* nice() pipe() */
#include "diagnostic.h"   /* FAIL() */
#include "osutil.h"       /* SetRealTimer() */
#include "spinner.h"      /* spin() */
#include "activeCpu.h"


#define READ_END 0
#define WRITE_END 1
#define SPIN_TIMED_OUT 1


/* The process state, saved before the spinner invocation. */
static sigjmp_buf preSpin;


/* A SIGALRM handler that aborts the spinner call when invoked. */
static void
SpinTimeOut(int sig) {
  longjmp(preSpin, SPIN_TIMED_OUT);
}


int
ActiveCpuGetLoad(unsigned short niceValue,
                 unsigned int maxWait,
                 unsigned int testLength,
                 double *available) {

  int connection[2];
  pid_t childPid;
  unsigned long cpuTime;
  void (*oldHandler)(int);
  unsigned long wallTime;

  if (niceValue != 0) {

    /*
    ** Since we can't ever reduce our nice value, we have to run any niced
    ** spinner in a forked process.  We use a communication channel to report
    ** results from the forked process to the parent.
    */
    if(pipe(connection) != 0) {
      FAIL("ActiveCpuGetLoad: pipe() failed\n");
    }

    childPid = fork();

    if(childPid < 0) {
      FAIL("ActiveCpuGetLoad: fork() failed\n");
    }
    else if(childPid > 0) {
      /* Parent process. */
      close(connection[WRITE_END]);
      read(connection[READ_END], available, sizeof(*available));
      close(connection[READ_END]);
      wait(NULL);
      return(*available >= 0.0);
    }

    /* Child comes here. */
    close(connection[READ_END]);
    nice(niceValue);

  }

  /*
  ** Set an alarm and begin the spin process.  If the alarm goes off before the
  ** spin completes, setjmp() will return (via SpinTimeOut()) SPIN_TIMED_OUT
  ** and we report that no cpu is availble.  This is overly pessimistic; we
  ** should probably look at the rusage ourselves and report the actual amount
  ** of CPU we received.  Also, if we really get *no* CPU time, then we'll
  ** never be able to execute any of this code anyway.
  */
  oldHandler = signal(SIGALRM, SpinTimeOut);
  SetRealTimer(maxWait);
  *available = (setjmp(preSpin) == SPIN_TIMED_OUT) ? 0.0 :
               (spin(testLength, &cpuTime, &wallTime) && (wallTime > 0.0)) ?
               ((float)cpuTime / (float)wallTime) : -1.0;
  RESETREALTIMER;
  (void)signal(SIGALRM, oldHandler);

  if (niceValue != 0) {
    write(connection[WRITE_END], available, sizeof(*available));
    close(connection[WRITE_END]);
    exit(0);
  }

  return(*available >= 0.0);

}
