/* $Id: profiler.c,v 1.7 1999/07/10 20:27:55 jhayes Exp $ */

#include "config.h"
#include <stdio.h>        /* FILE */
#include <unistd.h>       /* getpid() */
#include <sys/types.h>    /* getpid() */
#include <sys/time.h>     /* gettimeofday() */
#include <sys/resource.h> /* getrusage() */
#include "profiler.h"


#ifdef HAVE_GETRUSAGE


#ifndef timerclear
#define timerclear(tvp)     (tvp)->tv_sec = (tvp)->tv_usec = 0
#endif


static struct timeval lastUser;
static struct timeval lastSystem;
static struct timeval lastReal;
static struct timeval markLastUser;
static struct timeval markLastSystem;
static struct timeval markLastReal;
static struct rusage rusageBuffer;


int
getrusage(int who,
          struct rusage *rusage);


void
ProfWriteTime(const char *s) {
	char filename[40];
	FILE *pFile;
	struct timeval tp;
	sprintf(filename, "/tmp/complibPerf%ld", (long int)getpid());
	pFile=fopen(filename, "a");
	gettimeofday(&tp, NULL);
	fprintf(pFile, "%s:%ld.%ld\n", s, (long int)tp.tv_sec, (long int)tp.tv_usec);
	fclose(pFile);
}


int
ProfStart() {
	getrusage(RUSAGE_SELF,  &rusageBuffer);
	lastUser = rusageBuffer.ru_utime;
	lastSystem = rusageBuffer.ru_stime;
	gettimeofday(&lastReal, NULL);
	timerclear(&markLastSystem);
	timerclear(&markLastUser);
	timerclear(&markLastReal);
	/* writeTime("start"); */
	return 1;
}


void
ProfStop() {
	getrusage(RUSAGE_SELF,  &rusageBuffer);
	markLastUser.tv_sec = rusageBuffer.ru_utime.tv_sec - lastUser.tv_sec;
	markLastUser.tv_usec = rusageBuffer.ru_utime.tv_usec - lastUser.tv_usec;
	if(markLastUser.tv_usec < 0) {
		markLastUser.tv_usec += 1000000; 
		markLastUser.tv_sec-=1;
	}
	markLastSystem.tv_sec = rusageBuffer.ru_stime.tv_sec - lastSystem.tv_sec;
	markLastSystem.tv_usec = rusageBuffer.ru_stime.tv_usec - lastSystem.tv_usec;
	if(markLastSystem.tv_usec < 0) {
		markLastSystem.tv_usec += 1000000; 
		markLastSystem.tv_sec-=1;
	}
	gettimeofday(&markLastReal, NULL);
	markLastReal.tv_sec -= lastReal.tv_sec;
	markLastReal.tv_usec -= lastReal.tv_usec;
	if(markLastReal.tv_usec < 0) {
		markLastReal.tv_usec += 1000000; 
		markLastReal.tv_sec-=1;
	}
	/* writeTime("stop"); */
}


unsigned long
ProfMsec() {
	return (markLastUser.tv_sec*1000 + markLastSystem.tv_sec*1000
		 + markLastUser.tv_usec/1000 + markLastSystem.tv_usec/1000);
}


unsigned long
ProfUsec() {
	return (markLastUser.tv_sec*1000000 + markLastUser.tv_usec +
		markLastSystem.tv_sec*1000000 + markLastSystem.tv_usec);
}


unsigned long
ProfMsecR() {
	return (markLastReal.tv_sec*1000 + markLastReal.tv_usec/1000);
}


unsigned long
ProfUsecR() {
	return (markLastReal.tv_sec*1000000 + markLastReal.tv_usec);
}


/* take advantage of non-destructive stop, but hide the decision here */
unsigned long
ProfElapsedMsecR() {
	ProfStop();
	return ProfMsecR();
}


#else


int
ProfStart(void) {
  return 0;
}


void
ProfStop(void) {
}


unsigned long
ProfMsec(void) {
  return 0;
}

 
unsigned long
ProfUsec(void) {
  return 0;
}


unsigned long
ProfMsecR(void) {
  return 0;
}


unsigned long
ProfUsecR(void) {
  return 0;
}


unsigned long
ProfElapsedMsecR(void) {
  return 0;
}


void
ProfWriteTime(const char *s) {
}


#endif
