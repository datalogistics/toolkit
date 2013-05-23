#include <stdio.h>
#include "ngc.h"
#include <EXTERN.h>
#include <perl.h>

#ifndef DO_CLEAN
#define DO_CLEAN 0
#endif

static PerlInterpreter *perl = NULL;

int main (int argc, char **argv, char **env)
{

  char *hosts[] = {"caida.org", "apple.com", "tamus.edu", "derema.com", "utk.edu", NULL};
  char **latlongArray;
  int i;



  char *embedding[] = { "", "persistent.pl" };
  char *args[] = { "", DO_CLEAN, NULL };
  char *filename = "tNGC.pl";
  int exitstatus = 0;
  STRLEN n_a;

  if ((perl = perl_alloc()) == NULL) {
     fprintf(stderr, "no memory!");
     exit(1);
  }
  perl_construct(perl);

  exitstatus = perl_parse(perl, NULL, 2, embedding, NULL);

  if(!exitstatus) {
    exitstatus = perl_run(perl);

    args[0] = filename;
    call_argv("Embed::Persistent::eval_file", G_DISCARD | G_EVAL, args);
    if(SvTRUE(ERRSV)) fprintf(stderr, "eval error: %s\n", SvPV(ERRSV,n_a));
  }



  
  latlongArray = getLatLong ( args );
    
  for (i = 0; latlongArray[i] != NULL; i ++)
    printf ("%s\n", latlongArray[i]); 
  
  latlongArray = getLatLong ( args );
    
  for (i = 0; latlongArray[i] != NULL; i ++)
    printf ("%s\n", latlongArray[i]); 
  
  return 0;
}
