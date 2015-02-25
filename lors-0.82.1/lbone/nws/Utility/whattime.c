/* $Id: whattime.c,v 1.2 1998/03/18 22:17:04 jhayes Exp $ */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

int
main(int argc, char *argv[])
{
  int time_val;
  int i;

  for (i = 1; i < argc; i++) {
    time_val = atoi(argv[i]);
    fprintf(stdout,"%s == %s", argv[i], ctime((time_t *)&time_val));
    fflush(stdout);
  }
  return(0);

}
