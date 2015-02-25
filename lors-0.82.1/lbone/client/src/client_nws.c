#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lbone_client_lib.h"

extern int lbone_sortByBandwidth();


int main(int argc, char **argv)
{
  int		i, count;
  LBONE_request	request;
  char 		*temp;
  Depot		lbone_server, *depots, *list;

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
  request.numDepots = 10;
  request.stableSize = 5;
  request.volatileSize = 0;
  request.duration = 0.0;
  request.location = strdup("zip= 37922");

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

  if (count > 0) {
    printf("Calling lbone_checkDepots\n");
    list = lbone_checkDepots(depots, request, 7);

/*
    for (i = 0; i < count; i++) free(depots[i]);
    free(depots);
*/

    if (list != NULL) {
      i = 0;
      while(list[i] != NULL) i++;
      printf("These %d depots responded:\n", i);

      i = 0; count = 0;
      while(list[i] != NULL) {
        temp = (char *) strdup(list[i]->host);
        fprintf(stderr, "  %-30s\n", temp);
/*        free(list[i]); */
        free(temp);
        i++; count++;
      }
/*      free(list); */
      printf("\n");
    } else {
      printf("none meet requirements or they may not have responded\n\n");
    }
  }

strcpy(depots[1]->host, "dsi.ncni.net");
  if(count > 0) {
    printf("Calling nws_ping on the depots:\n");
    lbone_sortByBandwidth(list, 10);
  }




exit(0);    /*  <---------------------- early exit ----------------------- */

  printf("***************************************************************\n\n");

  /* Send the second client request */

  memset(&request, 0, sizeof(struct lbone_request));
  request.numDepots = 5;
  request.stableSize = 0;
  request.volatileSize = 1;
  request.duration = 0;
  request.location = strdup("airport= LAX");

  printf("client: second request sent\n");
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
  }

  if (count > 0) {
    printf("Calling lbone_checkDepots\n");
    list = lbone_checkDepots(depots, request, 7);
  
    for (i = 0; i < count; i++) free(depots[i]);
    free(depots);

    if (list != NULL) {
      i = 0;
      while(list[i] != NULL) i++;
      printf("These %d depots responded:\n", i);

      i = 0;
      while(list[i] != NULL) {
        temp = (char *) strdup(list[i]->host);
        fprintf(stderr, "  %-30s\n", temp);
        free(list[i]);
        free(temp);
        i++;
      }
      free(list);
      printf("\n");
    } else {
      printf("none meet requirements or they may not have responded\n\n");
    }
  }

  printf("***************************************************************\n\n");

  /* Send the third client request */

  memset(&request, 0, sizeof(struct lbone_request));
  request.numDepots = 3;
  request.stableSize = 100;
  request.volatileSize = 0;
  request.duration = 30;
  request.location = NULL;
/*  request.location = strdup("=US 90001"); */

  printf("client: third request sent\n");
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
  }

  if (count > 0) {
    printf("Calling lbone_checkDepots\n");
    list = lbone_checkDepots(depots, request, 7);

    for (i = 0; i < count; i++) free(depots[i]);
    free(depots);

    if (list != NULL) {
      i = 0;
      while(list[i] != NULL) i++;
      printf("These %d depots responded:\n", i);

      i = 0;
      while(list[i] != NULL) {
        temp = (char *) strdup(list[i]->host);
        fprintf(stderr, "  %-30s\n", temp);
        free(list[i]);
        free(temp);
        i++;
      }
      free(list);
      printf("\n");
    } else {
      printf("none meet requirements or they may not have responded\n\n");
    }
  }

  free(lbone_server);

  return 0;
}




