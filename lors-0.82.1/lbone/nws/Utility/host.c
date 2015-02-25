/* $Id: host.c,v 1.2 1999/09/22 16:54:46 hayes Exp $ */

#include <stdio.h>
#include "dnsutil.h"


#define MAX_ADDRESSES 20


int
main(int argc,
     char **argv) {

  int addrCount;
  int i;
  int j;
  IPAddress addresses[MAX_ADDRESSES];

  if(argc < 1)
    exit(1);

  for(i = 1; i < argc; i++) {
    addrCount = IPAddressValues(argv[i], addresses, MAX_ADDRESSES);
    if(addrCount == 0) {
      printf("%s ?", argv[i]);
    }
    else {
      printf("%s", IPAddressMachine(addresses[0]));
      for(j = 0; j < addrCount; j++) {
        printf(" %s", IPAddressImage(addresses[j]));
      }
    }
    printf("\n");
  }

  return 0;

}
