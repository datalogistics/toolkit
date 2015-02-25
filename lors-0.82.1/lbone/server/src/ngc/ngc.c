#include <EXTERN.h>
#include <perl.h>
#include "perlxsi.h"


char **getLatLong (char **args)
{ 
  dSP;
  
  static PerlInterpreter *my_perl; 
  char *perl_argv[] = {"", "tNGC.pl"};
  char *perl_subroutines = "test_getLatLongArray";
   
  int retval;
  char **latlongArray;
  

  my_perl = perl_alloc ();
  perl_construct (my_perl);
  perl_parse (my_perl, xs_init, 2, perl_argv, NULL);
  
  retval = call_argv (perl_subroutines, G_ARRAY, args);

  SPAGAIN;
  
  latlongArray = (char **) malloc (sizeof(char *) * (retval + 1));  
  
  latlongArray[retval] = NULL;
  for (retval --; retval > -1; retval --) 
    latlongArray[retval] = strdup( POPp );
    
  perl_destruct (my_perl);
  perl_free (my_perl);
  
  return latlongArray;
}
