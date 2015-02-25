#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lbone_base.h"
#include "lbone_client_lib.h"


typedef struct globals {
  Depot	server;
  LBONE_request	request;
  int	id;
} ThreadInfo;

pthread_mutex_t lock;
int counter;

void *do_it(void *x) {
  int i;
  char *temp;
  ThreadInfo *info;
  Depot *depots;

  info = (ThreadInfo *) x;

  pthread_mutex_lock(&lock);
  printf("Thread %d: request sent\n", info->id);
  printf("  numDepots    = %d\n", info->request.numDepots);
  printf("  stableSize   = %lu\n", info->request.stableSize);
  printf("  volatileSize = %lu\n", info->request.volatileSize);
  printf("  duration     = %.2f\n", info->request.duration);
  printf("  location     = %s\n\n", info->request.location);
  pthread_mutex_unlock(&lock);

  depots = lbone_getDepots(info->server, info->request, 0);

  if (depots != NULL) {
    i = 0;
    while(depots[i] != NULL) i++;

    pthread_mutex_lock(&lock);
    printf("Thread %d: server returned %d depots:\n", info->id, i);

    i = 0;
    while(depots[i] != NULL) {
/*
      temp = (char *) malloc(strcspn(depots[i]->host, "  "));
      sprintf(temp, "%s", strncpy(temp, depots[i]->host, strcspn(depots[i]->host, "  ")));
      temp[strcspn(depots[i]->host, "  ")] = '\0';
*/
      temp = (char *) strdup(depots[i]->host);
      fprintf(stderr, "  received depot: %-30s:%d\n", temp, depots[i]->port);
      i++;
    }
    printf("\n");
    pthread_mutex_unlock(&lock);
  }
  pthread_mutex_lock(&lock);
    counter--;
  pthread_mutex_unlock(&lock);
  pthread_exit(NULL);
  return 0; /* prevent compiler warning */
}

int main(int argc, char **argv)
{
  int		i, retval;
  LBONE_request	request[3];
  Depot		lbone_server;
  ThreadInfo	info[3];
  pthread_attr_t	attr;
  pthread_t	ptid;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  counter = 0;

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
  pthread_mutex_init(&lock, NULL);


  memset(&request, 0, sizeof(struct lbone_request) * 3);
  request[0].numDepots = 10;
  request[0].stableSize = 50;
  request[0].volatileSize = 0;
  request[0].duration = 0;
  request[0].location = strdup("zip= 37996");

  request[1].numDepots = 5;
  request[1].stableSize = 0;
  request[1].volatileSize = 1;
  request[1].duration = 0;
  request[1].location = strdup("airport= ORD");

  request[2].numDepots = 3;
  request[2].stableSize = 100;
  request[2].volatileSize = 0;
  request[2].duration = 30;
  request[2].location = strdup("country= it city= aosta");

  for (i=0; i < 3; i++) {
    info[i].server = lbone_server;
    info[i].request = request[i];
    info[i].id = i;
    retval = pthread_create(&ptid, NULL, do_it, (void*) &info[i]);
    pthread_mutex_lock(&lock);
      counter++;
    pthread_mutex_unlock(&lock);
    if (retval != 0) perror("pthread_create failed");
  }

  while(1) {
    pthread_mutex_lock(&lock);
      if (counter == 0) {
        if (request[0].location != NULL) free(request[0].location);
        if (request[1].location != NULL) free(request[1].location);
        if (request[2].location != NULL) free(request[2].location);
        free(lbone_server);
        pthread_attr_destroy(&attr);
        pthread_mutex_unlock(&lock);
        pthread_mutex_destroy(&lock);
        return 0;
      }
    pthread_mutex_unlock(&lock);
    sleep(1);
  }
}




