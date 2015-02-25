/* $Id: connect_exp.c,v 1.25 2000/03/01 16:37:06 hayes Exp $ */

#include "config.h"
#include "diagnostic.h"     /* FAIL() */
#include "osutil.h"         /* MicroTime() */
#include "connect_exp.h"


int
InitiateConnectExp(IPAddress machine,
                   unsigned short port,
                   unsigned int timeOut,
                   double *results) {

  Socket sd;
  long startTime;
  long stopTime;

  startTime = MicroTime();
  if(!CallAddr(machine, port, &sd, (double)timeOut)) {
    FAIL2("InitiateConnectExp: contact of %s:%d failed\n",
          IPAddressImage(machine), port);
  }
  stopTime = MicroTime();
  DROP_SOCKET(&sd);
  *results = (stopTime - startTime) / 1000.0;
  return(1);

}
