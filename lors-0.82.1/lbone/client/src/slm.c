#include "lbone_client_lib.h"
#include "fields.h"




void check_for_q(char str[128]) {
  if (((str[0] == 'Q') || (str[0] == 'q')) && (str[1] == ' ')) exit(0);
  if (((str[0] == 'Q') || (str[0] == 'q')) && (str[1] == '\0')) exit(0);
  if (((str[0] == 'Q') || (str[0] == 'q')) && (str[1] == '\n')) exit(0);
}




int main(int argc, char **argv)
{
  int		i, ret;
  LBONE_request	request;
  char 		*temp;
  IS		is;
  Depot		server, *depots;
  char		location[256];

  if (argc == 1) {
    fprintf(stderr, "usage: slm <lbone_server> [<port>]\n");
    exit(1);
  }

  /* set up server depot */
  server = (Depot) malloc(sizeof(struct ibp_depot));

  if ( argc >= 2 )
  {
      snprintf(server->host, 255, "%s", argv[1]);
      if ( argc == 3 )
      {
          server->port = atoi(argv[2]);
      } else 
      {
          server->port = 6767;
      }
  }

  is = new_inputstruct(NULL);

  printf("\nWelcome to the LBONE query tool\n\n");

  /* Get the first client request */

  printf("Enter Q at any prompt and press to return to quit\n\n");

  while (1) {

    memset(&request, 0, sizeof(struct lbone_request));

    printf("How many depots are you looking for?\n");
    printf("  # of Depots: ");
    ret = get_line(is); if ( ret == EOF ) exit(0);
    check_for_q(is->fields[0]);
    request.numDepots = atoi(is->fields[0]);

    printf("\nHow much volatile storage do you need? (MB)\n");
    printf("  Soft storage: ");
    ret = get_line(is); if ( ret == EOF ) exit(0);
    check_for_q(is->fields[0]);
    request.volatileSize = atoi(is->fields[0]);

    printf("\nHow much stable storage do you need? (MB)\n");
    printf("  Hard storage: ");
    ret = get_line(is); if ( ret == EOF ) exit(0);
    check_for_q(is->fields[0]);
    request.stableSize = atoi(is->fields[0]);

    printf("\nHow many days will you need the storage?\n");
    printf("  Duration: ");
    ret = get_line(is); if ( ret == EOF ) exit(0);
    check_for_q(is->fields[0]);
    request.duration = atof(is->fields[0]);

    printf("\nWhere do want the storage?\n");
    printf("  Location: ");
    ret = get_line(is); if ( ret == EOF ) exit(0);
    if (is->NF > 0) {
      check_for_q(is->fields[0]);
      if (strcmp(is->fields[0], "NULL") == 0) {
        request.location = NULL;
      } else {
        memset(location, 0, 256);
        strcpy(location, is->fields[0]);
        for (i = 1; i < is->NF; i++) {
          strcat(location, " ");
          strcat(location, is->fields[i]);
        }
        request.location = (char *) strdup(location);
      }
    }

    printf("\nslm: request sent:\n");
    printf("  numDepots    = %d\n", request.numDepots);
    printf("  stableSize   = %lu\n", request.stableSize);
    printf("  volatileSize = %lu\n", request.volatileSize);
    printf("  duration     = %.2f\n", request.duration);
    if (request.location != NULL) {
      printf("  location     = %s\n\n", request.location);
    } else {
      printf("  location     = NULL\n\n");
    }

    depots = lbone_getDepots(server, request, 0);

    if (request.location != NULL) free(request.location);

    if (depots == NULL) {
      printf("No depots returned\n\n");
      continue;
    }

    i = 0;
    while(depots[i] != NULL) i++;
    printf("Server returned %d depots:\n", i);

    if (depots != NULL) {
      i = 0;
      while(depots[i] != NULL) {
/*        temp = (char *) malloc(strcspn(depots[i]->host, "  "));  */
        temp = (char *) strdup(depots[i]->host);
        sprintf(temp, "%s", strncpy(temp, depots[i]->host, strcspn(depots[i]->host, "  ")));
        temp[strcspn(depots[i]->host, "  ")] = '\0';
        fprintf(stdout, "  received depot: %-30s:%d\n", temp, depots[i]->port);
        i++;
      }
      printf("\n");
    }
  }
  return 0;
}




