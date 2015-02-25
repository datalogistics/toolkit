/* $Id: spinner.h,v 1.3 1999/06/15 21:11:17 jhayes Exp $ */

/*
** Runs a computation for #msecToSpin# milliseconds, then returns the cpu time
** and wall clock time required in #cpuUsed# and #wallUsed#, respectively.
** Returns 1 if successful, 0 otherwise.
*/
int
spin(unsigned int msecToSpin,
     unsigned long *cpuUsed,
     unsigned long *wallUsed);
