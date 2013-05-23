/* $Id: profiler.h,v 1.3 1999/07/10 20:27:55 jhayes Exp $ */

#ifndef PROFILER_H
#define PROFILER_H


int
ProfStart(void);


void
ProfStop(void);


unsigned long
ProfMsec(void); 


unsigned long
ProfUsec(void);


unsigned long
ProfMsecR(void);


unsigned long
ProfUsecR(void);


unsigned long
ProfElapsedMsecR(void);


void
ProfWriteTime(const char *s);


#endif
