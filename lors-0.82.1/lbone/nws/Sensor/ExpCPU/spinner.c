/* $Id: spinner.c,v 1.4 1999/07/10 20:27:55 jhayes Exp $ */

#include "spinner.h"
#include "profiler.h"


int
spin(unsigned int msecToSpin,
     unsigned long *msecCpuUsed,
     unsigned long *msecWallUsed) {

  int l=1;
  int i;

  if(!ProfStart()) {
    return 0;
  }
  while(ProfElapsedMsecR() < msecToSpin) {
    for(i = 0; i < 1000000; i++) {
      l *= 2;
    }
  }

  if(msecCpuUsed)
    *msecCpuUsed = ProfMsec();
  if(msecWallUsed)
    *msecWallUsed = ProfMsecR();
  return 1;

}
