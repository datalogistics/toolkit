/* $Id: activeCpu.h,v 1.3 1999/01/11 20:13:50 jhayes Exp $ */

/*
** This module implements a monitor which tests cpu availability through active
** probing.
*/

/*
** Runs a nice #niceValue# process for #testLength# milliseconds to test the
** amount of cpu available.  Sets #available# to the percentage of wall clock
** time during the life of a process that it spends in a CPU, or 0.0 if the
** process is not completed within #maxWait# seconds.  Returns 1 if successful,
** 0 otherwise.
*/
int ActiveCpuGetLoad(unsigned short niceValue,
                     unsigned int maxWait,
                     unsigned int testLength,
                     double *available);
