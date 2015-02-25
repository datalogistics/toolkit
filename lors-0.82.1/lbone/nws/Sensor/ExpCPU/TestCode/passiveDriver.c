/* $Id: passiveDriver.c,v 1.7 2000/03/21 18:57:26 hayes Exp $ */

#include <unistd.h> /* sleep() */
#include <stdio.h> /* fprintf() */
#include <time.h> /* time(), time_t */
#include "passiveCpu.h"

int main() {

  double a,c;

  PassiveCpuOpenMonitor(5);

  while(1) {
    sleep(2);
    if(PassiveCpuGetLoad(0, 0, &a, &c)) {
      printf("%d %f current, %f avail\n", (int)time((time_t)0), c, a);
      fflush(stdout);
    }
  }

  return 0;

}
