#include "config-ibp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <errno.h>


#define MAX_KEY_LEN   256

char* GetTargetKey(char *capstring)
{
  char *key;
  int i=6, j=0;
  
  key = (char *)malloc(sizeof(char)*MAX_KEY_LEN);
  while(capstring[i] != ':') i++;
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/'){
    key[j] = capstring[i];
    i++; j++;
  }
  key[j] = '\0';
  return key;  
} 

