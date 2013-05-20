#ifndef LODNWARMER_H_
#define LODNWARMER_H_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>


typedef struct
{
    char *name;
    JRB depots;
  
} Site;


typedef struct
{
    char *name;
    time_t  mtime;
    JRB   sites;
    
}LoDNSerializedWarmer;

LoDNSerializedWarmer *makeLoDNSerializedWarmer(char *filename);
void freeLoDNSerializedWarmer(LoDNSerializedWarmer *warmer);
int LoDNSerializedWarmerReadFile(LoDNSerializedWarmer *warmer);


#endif /*LODNWARMER_H_*/
