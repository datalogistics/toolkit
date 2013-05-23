#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lbone_client_lib.h"
#include <jrb.h>

Depot strtodepot(char *hostname)
{
    char *s;
    Depot   ibpdepot;
    ibpdepot = (Depot) malloc(sizeof(struct ibp_depot));

    s = strchr(hostname, ':');
    if ( s != NULL )
    {
        ibpdepot->port = atoi(s+1);
        *s = '\0';
    } else 
    {
        ibpdepot->port = 6714;
    }
    strcpy(ibpdepot->host, hostname);

    fprintf(stderr, "%s:%d\n", ibpdepot->host, ibpdepot->port);
    return ibpdepot;
}

int main(int argc, char **argv)
{
  int        i, j, count;
  int       ret;
  LBONE_request        request;
  char       *temp;
  Depot      lbone_server, *depots;
  double    **resArray;
  Depot      srcList[2];
  JRB           sort_tree, sort_node;
  srcList[1] = NULL;

  if (argc != 4) {
    fprintf(stderr, "usage: client lbone_server lbone_port <ibp_depot with nwssensor>\n");
    exit(1);
  }

  if (atoi(argv[2]) <= 1024) {
    fprintf(stderr, "Must use a port > 1024\n");
    exit(1);
  }

  /* set up IBP depot */

  srcList[0] = strtodepot(argv[3]);

  lbone_server = (Depot) malloc(sizeof(struct ibp_depot));
  strcpy(lbone_server->host, argv[1]);
  lbone_server->port = atoi(argv[2]);

  printf("\n");

  /* Send the first client request */

  memset(&request, 0, sizeof(struct lbone_request));
  request.numDepots = -1;
  request.stableSize = 1;
  request.volatileSize = 1;
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

  fprintf(stderr, "CLEAR\n");
  ret = lbone_getDepot2DepotResolution(&resArray, lbone_server, srcList, depots,
  LBONE_RESOLUTION_GEO, 20);
  /* sort all bandwidths greater than zero.
   * find  max.
   * scale each on a scale of 1 - 10. 
   */
  if ( ret != 0 )
  {
      fprintf(stderr, "error\n");
  }else 
  {
      int cnt = 0;
      double max;
      double min;

      fprintf(stderr, "From %d: %s\n", 0, srcList[0]->host);
      sort_tree = make_jrb();
      for(j=0; j<count; j++)
      {
          if ( resArray[0][j] > 0.0 )
          {
              jrb_insert_dbl(sort_tree, resArray[0][j], new_jval_i(j));
              cnt++;
          }
      }
      sort_node = jrb_last(sort_tree);
      max = sort_node->key.d;
      sort_node = jrb_first(sort_tree);
      min = sort_node->key.d;

      fprintf(stderr, "NWS SCALE %f %f\n", min/max, min/min);
      jrb_traverse(sort_node, sort_tree)
      {
          double t, scale;
          int    j;
          int       i= 0;
          t = sort_node->key.d;
          j = sort_node->val.i;
          if ( t > 0.0 )
          {
               fprintf(stderr, "NWS Arrow %s:%d to %s:%d %f Mbps\n",
                    srcList[0]->host, srcList[0]->port, 
                    depots[j]->host, depots[j]->port, min/t);
               i++;
          }
      }
      printf("\n");
  }

  free(lbone_server);

  return 0;
}




