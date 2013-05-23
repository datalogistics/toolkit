/* $Id: spintest.c,v 1.2 1999/06/11 21:23:20 jhayes Exp $ */

/*
** This program runs iterative tests of the spin() function with increasing
** amounts of spin time and displays the results.  It may be invoked with one
** parameter, which specifies the number of seconds to sleep between tests; if
** omitted, the sleep time is 30 seconds.
*/

#include "spinner.h"
#include <stdio.h>  /* printf() */
#include <unistd.h>  /* sleep() */
#include <stdlib.h>  /* atoi() */

#define show_available(time, cpu, wall) \
  printf(" %d : %ld/%ld = %f\n", time, cpu, wall, \
         (wall != 0) ? (float)cpu / (float)wall : -1.0); \
  fflush(stdout);

int main(int argc, char *argv[]) {

  unsigned long cpu, real;
  int i;
  int pauseTime;
  int repeat;

  pauseTime = (argc > 1) ? atoi(argv[1]) : 30;

  for(repeat = 0; repeat < 40; repeat++) {
    for(i = 1; i < 60000; i <<= 1) {
      spin(i, &cpu, &real);
      show_available(i, cpu, real);
      sleep(pauseTime);
    }
  }

  return 0;

}
