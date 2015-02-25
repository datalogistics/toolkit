/* $Id: hybridDriver.c,v 1.18 2000/03/22 16:28:20 hayes Exp $ */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "hybridCpu.h"
#include "spinner.h"


/*
** This is a testing program for the hybrid CPU sensor.  It takes as input from
** stdin a series of command lines.  Blank lines and those beginning with # are
** copied to stdout without interpretation; other lines are interpreted as a
** series of ten integer values:
**    1) the time (in seconds) that the program should run;
**    2) the time (in seconds) between restarts of the probe process;
**    3) the time (in seconds) between probes;
**    4) the number of millisecs the active probe should run when invoked;
**    5) whether or not (boolean 1/0) the length of the active probe should be
**       adjusted based on hybrid CPU sensor heuristics;
**    6) the initial active probing period (1 active probe for each n passive);
**    7) whether or not (boolean 1/0) the active probing period should be
**       adjusted based on hybrid CPU sensor heuristics;
**    8) the time (in seconds) between restarts of the test process;
**    9) the time (in seconds) between tests;
**   10) the number of millisecs the test should run.
** Examples:
**
** 3600 900 10 1000 0 10 0 600 20 2000
** Run for 1 hour; restarting the probe process every 15 minutes and the test
** process every 10.  Probe every 10 seconds; test every 20 seconds.  Run a
** 1-second active probe after every 10th passive probe.  Run each test for
** 2 seconds.
**
** 1800 1800 10 2000 1 10 1 1800 10 3000
** Run for 1/2 hour; starting the probe and test process only once.  Probe and
** test every 10 seconds.  A 2-second active probe is run after every 10th
** passive probe, adapting both the active length and period heuristically.
** Run 3-second tests.
**
** 5000 5000 10 0 0 0 0 1000 0 10000
** Run 500 tests, one every 10 seconds, only running passive probes.  The test
** is 10 seconds long and is run continuously.  The probe process is only
** started once, while the test process is restarted after every 100 tests.
**
** The estimated CPU available returned from the sensor and the actual CPU
** usage of the forked process are both written to stdout.
*/


#define READEND(X) X[0]
#define WRITEEND(X) X[1]
#define COMMANDSIZE 256
#define MESSAGESIZE 256
#define SUICIDE_NOTE "DIE!"


int
main(int argc, char *argv[]) {

  enum serverTypes {PROBE, TEST};

  struct {
    int count;
    int frequency;
    char image[10];
    time_t nextRestart;
    time_t nextRun;
    int pid;
    int pipeFromServer[2];
    int pipeToServer[2];
    int restart;
  } serverInfo[2];

  double available, current, serverResults;
  unsigned long cpuTime, wallTime;
  char message[MESSAGESIZE];
  char commandLine[COMMANDSIZE];
  time_t nextActivity, now, whenDone;
  int activePeriod, activeTime, adaptPeriod, adaptTime, runTime, serverTime,
      testTime;
  fd_set readSet;
  enum serverTypes server;
  struct timeval noWait = {0, 0};
  unsigned short nice0 = 0;

  serverInfo[PROBE].pid = serverInfo[TEST].pid = 0;
  strcpy(serverInfo[PROBE].image, "probe");
  strcpy(serverInfo[TEST].image, "test");

  while (1) {

    /* Get the next command line.  */
    if (!fgets(commandLine, COMMANDSIZE - 1, stdin))
      break;

    /* Copy blank and comment lines to stdout w/out interpretation. */
    if ((commandLine[0] == '\n') || (commandLine[0] == '#')) {
      printf("%s", commandLine);
      fflush(stdout);
      continue;
    }

    /* Parse the parameters of this test run. */
    sscanf(commandLine, "%d %d %d %d %d %d %d %d %d %d",
      &runTime, &serverInfo[PROBE].restart, &serverInfo[PROBE].frequency,
      &activeTime, &adaptTime, &activePeriod, &adaptPeriod,
      &serverInfo[TEST].restart, &serverInfo[TEST].frequency, &testTime);

    time(&now);
    whenDone = now + (time_t)runTime;

    for (server = PROBE; server <= TEST; server++) {
      if (serverInfo[server].frequency == 0) {
        serverInfo[server].nextRestart = whenDone;
        serverInfo[server].nextRun = whenDone;
      }
      else {
        serverInfo[server].nextRestart = now;
        serverInfo[server].nextRun = now;
        if (serverInfo[server].restart == 0)
          serverInfo[server].restart = runTime;
      }
      serverInfo[server].count = 0;
    }

    printf("# Run for %d secs; restart probe every %d, test every %d\n",
           runTime, serverInfo[PROBE].restart, serverInfo[TEST].restart);
    printf("# Probe each %d secs, %d-m/s active (%s) every %dth passive (%s)\n",
           serverInfo[PROBE].frequency,
           activeTime, adaptTime ? "adaptable" : "fixed",
           activePeriod, adaptPeriod ? "adaptable" : "fixed");
    printf("# Tests run for %d m/s every %d secs\n",
           testTime, serverInfo[TEST].frequency);
    fflush(stdout);

    while (now < whenDone) {

      nextActivity = (serverInfo[PROBE].nextRun < serverInfo[TEST].nextRun) ?
                     serverInfo[PROBE].nextRun : serverInfo[TEST].nextRun;

      if (nextActivity > now)
        sleep(nextActivity - now);

      for (server = PROBE; server <= TEST; server++) {

        if (now >= serverInfo[server].nextRestart) {

          printf("Control: forking new %s process at %ld\n",
                 serverInfo[server].image, (long)nextActivity);
          fflush(stdout);

          if (serverInfo[server].pid != 0) {
            if (write(WRITEEND(serverInfo[server].pipeToServer), SUICIDE_NOTE,
                      strlen(SUICIDE_NOTE) + 1) < 0) {
              printf("Control: %s kill note failed\n",
                     serverInfo[server].image);
              kill(serverInfo[server].pid, SIGKILL);
            }
            wait(NULL);
            close(READEND(serverInfo[server].pipeFromServer));
            close(WRITEEND(serverInfo[server].pipeToServer));
          }

          if ( (pipe(serverInfo[server].pipeFromServer) < 0) ||
               (pipe(serverInfo[server].pipeToServer) < 0) ) {
            perror("Control: pipe failed");
            exit(1);
          }

          serverInfo[server].pid = fork();

          if (serverInfo[server].pid < 0) {
            perror("Control: fork failed");
            exit(1);
          }

          if (serverInfo[server].pid == 0) {

            /* Server process. */
            close(READEND(serverInfo[server].pipeFromServer));
            close(WRITEEND(serverInfo[server].pipeToServer));

            if (server == PROBE) {
              HybridCpuOpenMonitor
                (&nice0, 1, serverInfo[PROBE].frequency, activePeriod,
                 adaptPeriod, activeTime, adaptTime);
            }

            while (1) {

              /* Synchronize with control process. */
              if (read(READEND(serverInfo[server].pipeToServer), message,
                       MESSAGESIZE - 1) < 0) {
                perror("Read failed\n");
                exit(1);
              }

              if (strcmp(message, SUICIDE_NOTE) == 0) {
                if (server == PROBE)
                  HybridCpuCloseMonitor();
                close(READEND(serverInfo[server].pipeToServer));
                close(WRITEEND(serverInfo[server].pipeFromServer));
                exit(0);
              }

              time(&now);

              if (server == PROBE) {
                /* Run probe and report time & results through the pipe. */
                HybridCpuGetLoad(0, &available, &current);
                sprintf(message, "%ld %f %f", (long)now, available, current);
              }
              else {
                /* Run test and report results through the pipe. */
                spin(testTime, &cpuTime, &wallTime);
                sprintf(message, "%ld %ld %ld", (long)now, cpuTime, wallTime);
              }

              if (write(WRITEEND(serverInfo[server].pipeFromServer),
                        message, strlen(message) + 1) < 0) {
                perror("Write failed");
                exit(1);
              }

            }

          }
          else {

            /* Control process. */
            close(READEND(serverInfo[server].pipeToServer));
            close(WRITEEND(serverInfo[server].pipeFromServer));
            serverInfo[server].nextRestart += serverInfo[server].restart;

          }

        }

        if (now >= serverInfo[server].nextRun) {
          /* Tell the server process to go collect data. */
          if (write(WRITEEND(serverInfo[server].pipeToServer), "", 1) < 0) {
            perror("Control: probe write failed");
            exit(1);
          }
          serverInfo[server].nextRun += serverInfo[server].frequency;
        }

        /* Read and display any results reported from the server. */

        while (1) {

          FD_ZERO(&readSet);
          FD_SET(READEND(serverInfo[server].pipeFromServer), &readSet);

          if (select(FD_SETSIZE, &readSet, NULL, NULL, &noWait) < 1)
            break;

          if (read(READEND(serverInfo[server].pipeFromServer),
                   message, MESSAGESIZE - 1) < 0) {
            perror("Control: read failed\n");
            exit(1);
          }

          serverInfo[server].count++;

          if (server == PROBE) {
            sscanf(message, "%d %lf %lf", &serverTime, &available, &current);
            serverResults = available;
          }
          else {
            sscanf(message, "%d %ld %ld", &serverTime, &cpuTime, &wallTime);
            serverResults = (double)cpuTime / (double)wallTime;
          }
          printf("Control: %s %d at %d reports %f\n",
                 serverInfo[server].image, serverInfo[server].count,
                 serverTime, serverResults);
          fflush(stdout);

        }

      }

      time(&now);

      for (server = PROBE; server <= TEST; server++) {
        while (now > serverInfo[server].nextRun+serverInfo[server].frequency) {
          printf("Warning:  Control has fallen behind.  Skipping %ld %s\n",
                 serverInfo[server].nextRun, serverInfo[server].image);
          fflush(stdout);
          serverInfo[server].nextRun += serverInfo[server].frequency;
        }
      }

    }

  }

  for (server = PROBE; server <= TEST; server++) {
    if (serverInfo[server].pid != 0) {
      if (write(WRITEEND(serverInfo[server].pipeToServer), SUICIDE_NOTE,
                strlen(SUICIDE_NOTE + 1)) < 0) {
        perror("Control: kill note failed");
        kill(serverInfo[server].pid, SIGKILL);
      }
      close(READEND(serverInfo[server].pipeFromServer));
      close(WRITEEND(serverInfo[server].pipeToServer));
    }
  }

  return 0;

}
