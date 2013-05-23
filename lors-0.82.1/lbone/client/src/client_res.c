#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lbone_client_lib.h"


int main(int argc, char **argv)
{
  int        i, j, count;
  int       ret;
  LBONE_request        request;
  char       *temp;
  Depot      lbone_server, *depots, *srcList, *targetList;
  double    **resArray;

  if (argc != 3) {
    fprintf(stderr, "usage: client lbone_server lbone_port\n");
    exit(1);
  }

  if (atoi(argv[2]) <= 1024) {
    fprintf(stderr, "Must use a port > 1024\n");
    exit(1);
  }

  /* set up IBP depot */

  lbone_server = (Depot) malloc(sizeof(struct ibp_depot));
  strcpy(lbone_server->host, argv[1]);
  lbone_server->port = atoi(argv[2]);

  printf("\n");

  /* Send the first client request */

  memset(&request, 0, sizeof(struct lbone_request));
  request.numDepots = -1;
  request.stableSize = 1;
  request.volatileSize = 0;
  request.duration = 1.0;
  request.location = strdup("x");

  printf("client: first request sent\n");
  printf("  numDepots    = %d\n", request.numDepots);
  printf("  stableSize   = %lu\n", request.stableSize);
  printf("  volatileSize = %lu\n", request.volatileSize);
  printf("  duration     = %f\n", request.duration);
  if (request.location != NULL)
    printf("  location     = %s\n\n", request.location);
  else
    printf("\n");

  depots = lbone_getDepots(lbone_server, request, 7);

  if (request.location != NULL) free(request.location);

  count = 0;
  if (depots != NULL) {
    i = 0;
    while(depots[i] != NULL) i++;
    printf("Server returned %d depots:\n", i);
    count = i;
  
    i = 0;
    while(depots[i] != NULL) {
      temp = (char *) strdup(depots[i]->host);
      fprintf(stderr, "  received depot: %-30s:%d\n", temp, depots[i]->port);
      free(temp);
      i++;
    }
    printf("\n");
  } else {
    printf("none returned\n\n");
  }

  ret = lbone_getDepot2DepotResolution(&resArray, lbone_server, depots, depots,
  LBONE_RESOLUTION_NWS, 20);
  if ( ret != 0 )
  {
      fprintf(stderr, "error\n");
  }else 
  {
      for(i=0; i < count; i++)
      {

          fprintf(stderr, "From %d: %s\n", i, depots[i]->host);
          for(j=0; j<count; j++)
          {
              fprintf(stderr, "\tTo %d: %s == %f\n", j, depots[j]->host, resArray[i][j]);
          }
          printf("\n");
      }
  }

  free(lbone_server);

  return 0;
}




