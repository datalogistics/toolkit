#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "lbone_base.h"
#include "lbone_client_lib.h"

extern int request_connection();
extern void sigalarm_handler();

/*
 * Lbone client functions
 */

extern Depot *lbone_getDepots();
extern Depot *lbone_checkDepots();
extern void *lbone_depotStatus();
extern Depot *lbone_sortByBandwidth();
extern int lbone_depotCount();
extern void *lbone_pingDepot();
/*extern int nws_ping();*/

/*
 * Text Handling Functions
 */

extern int get_next_token ();
extern void trim_leading_spaces ();
extern void trim_trailing_spaces();

/*
 * use to contact lbone server and get list of depots
 */

Depot *lbone_getDepots(Depot lboneServer, LBONE_request req, int timeout)
{
  int           retval;
  int           i;
  int           fd;
  int           type;
  int           count;
  int           fd_ready;
  int           bytes_read;
  long          duration;
  char          version[10], header[10], header_temp[10];
  char          host[IBP_MAX_HOSTNAME_LEN], host_temp[IBP_MAX_HOSTNAME_LEN], port[10];
  char          location[452], *temp, message[LBONE_MESSAGE_LENGTH];
  Depot         *depots;
  fd_set        read_fds;
  struct timeval        timer;
  struct itimerval      int_timer;

  /* error check the Depot struct */

  if ((lboneServer->port <= 1024) || (lboneServer->port > 65353))
  {
    fprintf(stderr, "lbone_client: illegal port\n");
    return NULL;
  }

  /* error check the LBONE_request struct */

  if ((req.stableSize == 0) && (req.volatileSize == 0))
  {
    fprintf(stderr, "lbone_client: no storage requested\n");
    return NULL;
  }

  if (req.duration < 0)
  {
    fprintf(stderr, "lbone_client: illegal attempt to alter the space-time continuum.\n");
    fprintf(stderr, "Please specify a request duration >= 0.\n");
    return NULL;
  }

  req.duration *= (3600 * 24);
  duration = (long) req.duration;

  /* location check */
 
  memset(location, 0, 452);
  if (req.location != NULL)
  {
    if (strlen(req.location) < 452)
    {
      strcpy(location, req.location);
    }
    else
    {
      fprintf(stderr, "lbone_client: request.location string is too long.\n");
      return NULL;
    }
  }
  else
  {
    strcpy(location, "NULL");
  }

  /* check timeout value */

  if (timeout < 0)
  {
    fprintf(stderr, "lbone_client: illegal attempt to alter the space-time continuum.\n");
    fprintf(stderr, "  Please specify a timeout value >= 0.");
    return NULL;
  }

  if (timeout > 0)
    memset(&timer, 0, sizeof(struct timeval));

  memset(&int_timer, 0, sizeof(struct itimerval));
  int_timer.it_value.tv_sec = (long) timeout;

  /* build string to send over socket */

  memset(version, 0, 10);
  strcpy(version, LBONE_CLIENT_VERSION);
  type = CLIENT_REQUEST;

  temp = (char *) malloc(LBONE_MESSAGE_LENGTH);
  memset(temp, 0, LBONE_MESSAGE_LENGTH);

  sprintf(temp, "%10s%10d%10d%10ld%10ld%10ld%s", version, type, req.numDepots, req.stableSize, req.volatileSize, duration, location);

  memset(message, 0, LBONE_MESSAGE_LENGTH);
  strcpy(message, temp);
  free(temp);

  /* start signal handler and then start timer */

  signal(SIGALRM, sigalarm_handler);

  setitimer(ITIMER_REAL, &int_timer, NULL);

  /* request_connection() and write message */

  fd = request_connection(lboneServer->host, lboneServer->port);
  if (fd < 0) {
    perror("lbone_client: request_connection failed");
    return NULL;
    /* exit(1); */
  }

  getitimer(ITIMER_REAL, &int_timer);
  timer.tv_sec = int_timer.it_value.tv_sec;
  int_timer.it_value.tv_sec = 0;
  int_timer.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &int_timer, NULL);

  retval = write(fd, message, LBONE_MESSAGE_LENGTH);
  if (retval != LBONE_MESSAGE_LENGTH) {
    fprintf(stderr, "lbone_client: write to socket failed. ");
    fprintf(stderr, "Only %d characters written.\n", retval);
    return NULL;
  }

  /* get select() ready */

  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);

  /* receive reply header */

  if (timeout > 0)
    fd_ready = select(fd+1, &read_fds, NULL, NULL, &timer);
  else
    fd_ready = select(fd+1, &read_fds, NULL, NULL, NULL);

  if (fd_ready < 0)
  {
    perror("lbone_client: select returned an error");
    return NULL;
  }

  if (fd_ready > 0)
  {
    memset(header, 0, 10);
    bytes_read = 0;
    while (bytes_read < 10)
    {
      memset(header_temp, 0, 10);
      retval = read(fd, header_temp, 10 - bytes_read);
      if (retval == 0)
      {
        perror("lbone_getDepots: read header failed");
        close(fd);
        return NULL;
      }
      if (bytes_read == 0)
      {
        strncpy(header, header_temp, retval);
      }
      else
      {
        strncat(header, header_temp, retval);
      }
      bytes_read += retval;
    }
    header[9] = '\0';

    if (atoi(header) == SUCCESS)  /* get count */
    {
      memset(header, 0, 10);
      bytes_read = 0;
      while (bytes_read < 10)
      {
        memset(header_temp, 0, 10);
        retval = read(fd, header_temp, 10 - bytes_read);
        if (retval == 0)
        {
          perror("lbone_getDepots: read header count failed");
          close(fd);
          return NULL;
        }
        if (bytes_read == 0)
        {
          strncpy(header, header_temp, retval);
        }
        else
        {
          strncat(header, header_temp, retval);
        }
        bytes_read += retval;
      }
      header[9] = '\0';
      count = atoi(header);

      /*fprintf(stderr, "lbone: count  %d\n", count);*/

    } else
    {                          /* get errno */

      memset(header, 0, 10);
      bytes_read = 0;
      while (bytes_read < 10)
      {
        memset(header_temp, 0, 10);
        retval = read(fd, header_temp, 10 - bytes_read);
        if (retval == 0)
        {
          perror("lbone_getDepots: read header error number failed");
          close(fd);
          return NULL;
        }
        if (bytes_read == 0)
        {
          strncpy(header, header_temp, retval);
        }
        else
        {
          strncat(header, header_temp, retval);
        }
        bytes_read += retval;
      }
      header[9] = '\0';
      count = atoi(header);
      fprintf(stderr, "lbone_getDepots: server returned error %d\n", count);
      return NULL;
    }

    /* receive depots and build array */

    depots = (Depot *) malloc(sizeof(Depot) * (count + 1));

    for (i = 0; i < count; i++)
    {
      depots[i] = (Depot) malloc(sizeof(struct ibp_depot));
      memset(depots[i], 0, sizeof(struct ibp_depot));

      memset(host, 0, IBP_MAX_HOSTNAME_LEN);
      bytes_read = 0;
      while (bytes_read < IBP_MAX_HOSTNAME_LEN)
      {
        memset(host_temp, 0, IBP_MAX_HOSTNAME_LEN);
        retval = read(fd, &host_temp, IBP_MAX_HOSTNAME_LEN - bytes_read);
        if (retval == 0)
        {
          perror("lbone_getDepots: read host failed");
          fprintf(stderr, "read returned after %d bytes\n\n", retval);
          close(fd);
          return NULL;
        }
        if (bytes_read == 0) {
          strncpy(host, host_temp, retval);
        }
        else
        {
          strncat(host, host_temp, retval);
        }
        bytes_read += retval;
      }

      if (host != NULL)
      {
        strncpy(depots[i]->host, host, IBP_MAX_HOSTNAME_LEN);
        depots[i]->host[strcspn(depots[i]->host, " ")] = '\0';
      }

      memset(port, 0, 10);
      bytes_read = 0;
      while (bytes_read < 10)
      {
        memset(host_temp, 0, IBP_MAX_HOSTNAME_LEN);
        retval = read(fd, &host_temp, 10 - bytes_read);
        if (retval == 0)
        {
          perror("lbone_getDepots: read port failed");
          close(fd);
          return NULL;
        }
        if (bytes_read == 0) strncpy(port, host_temp, retval);
        else strncat(port, host_temp, retval);
        bytes_read += retval;
      }
      if (port != NULL)
        depots[i]->port = atoi(port);
    } 
    depots[i] = NULL;
  }
  else
  {               /* function timed out */
    fprintf(stderr, "lbone_client: message header did not arrive before timeout.\n");
    depots = NULL;
  }

  close(fd);

  /* return depots */

  return depots;
}

/*
 * lbone_getDepot2DepotResolution
 *      contact lbone server to discover the resolution (either NWS or
 *      distance) from each depot in srcList to each in targetList.  These
 *      results are returned in a two dimentional array.
 *
 *      values of src to target, can be indexed as follows 
 *
 *      double res = lbone_getDepot2DepotResolution(...)
 *      bw = res[0][1]       // indicates the resolution from src depot 0 
 *                           // to target depot 1
 *
 *      To obtain both to and from results of all depots, simply make srcList
 *      and targetList identical.  All entries where the src and target are
 *      equal, the resolution will be zero.
 *
 */
int lbone_getDepot2DepotResolution(double ***resArray, 
                                   Depot lboneServer, Depot *srcList, Depot *targetList,
                                   int type, unsigned int timeout)
{
    int         si, ti;
    struct timeval        timer;
    struct itimerval        int_timer;
    char                    version[10], header[10];
    char                   *temp;
    int           retval;
    int           i, j;
    int           fd;
    int           srcCount;
    int           targetCount;
    double    **resolution;

  int           fd_ready;

  char          message[LBONE_MESSAGE_LENGTH];
  Depot        *depots;
  fd_set        read_fds;

  /* error check the Depot struct */

  if ((srcList == NULL) || (targetList == NULL) || type < 0 )
  {
    fprintf(stderr, "lbone_client: NULL depot lists\n");
    return 1;
  }

  if (timeout > 0)
    memset(&timer, 0, sizeof(struct timeval));

  memset(&int_timer, 0, sizeof(struct itimerval));
  int_timer.it_value.tv_sec = (long) timeout;

  /* count size of src and target lists */
  for ( i=0; srcList[i] != NULL; i++);
  si = i;
  for ( i=0; targetList[i] != NULL; i++);
  ti = i;
  if ( si == 0 || ti == 0 )
  {
      return 1;
  }

  /* build string to send over socket */

  memset(version, 0, 10);
  strcpy(version, LBONE_CLIENT_VERSION);

    temp = (char *) malloc(LBONE_MESSAGE_LENGTH);
    memset(temp, 0, LBONE_MESSAGE_LENGTH);

    sprintf(temp, "%-10s%-10d%-10d%-10d%-10d", version, DEPOT2DEPOT_RESOLUTION, type, si, ti);

    memset(message, 0, LBONE_MESSAGE_LENGTH);
    strncpy(message, temp, LBONE_MESSAGE_LENGTH);
    free(temp);

    /* start signal handler and then start timer */

    signal(SIGALRM, sigalarm_handler);

    setitimer(ITIMER_REAL, &int_timer, NULL);

    /* request_connection() and write message */

    fd = request_connection(lboneServer->host, lboneServer->port);
    if (fd < 0) {
        perror("lbone_client: request_connection failed");
        return 1;
        /* exit(1); */
    }

    getitimer(ITIMER_REAL, &int_timer);
    timer.tv_sec = int_timer.it_value.tv_sec;
    int_timer.it_value.tv_sec = 0;
    int_timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &int_timer, NULL);

    retval = write(fd, message, strlen(message));
    if (retval != strlen(message)) {
        fprintf(stderr, "lbone_client: write to socket failed. ");
        fprintf(stderr, "Only %d characters written.\n", retval);
        return 1;
    }
    temp = (char *) calloc( LBONE_MESSAGE_LENGTH, 1);
    for ( i=0; i < si; i++)
    {
        snprintf(temp, 266, "%-256s%-10d", srcList[i]->host, srcList[i]->port);
        retval = write(fd, temp, 266);
        if (retval != 266) 
        {
            fprintf(stderr, "lbone_client: write to socket failed. ");
            fprintf(stderr, "Only %d characters written.\n", retval);
            return 1;
        }
    }
    for ( i=0; i < ti; i++)
    {
        snprintf(temp, 266, "%-256s%-10d", targetList[i]->host, targetList[i]->port);
        retval = write(fd, temp, 266);
        if (retval != 266) 
        {
            fprintf(stderr, "lbone_client: write to socket failed. ");
            fprintf(stderr, "Only %d characters written.\n", retval);
            return 1;
        }
    }
    free(temp);

    /* get select() ready */

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    /* receive reply header */

    if (timeout > 0) {
        fd_ready = select(fd+1, &read_fds, NULL, NULL, &timer);
    } else {
        fd_ready = select(fd+1, &read_fds, NULL, NULL, NULL);
    }

    if (fd_ready < 0)
    {
        perror("lbone_client: select returned an error");
        return 1;
    }

    if (fd_ready > 0)
    {
        memset(header, 0, 10);
        retval = get_next_token(fd, header, 10);
        header[9] = '\0';

        if (atoi(header) == SUCCESS)  /* get count */
        {
            memset(header, 0, 10);
            retval = get_next_token(fd, header, 10);
            header[9] = '\0';
            srcCount = atoi(header);

            memset(header, 0, 10);
            retval = get_next_token(fd, header, 10);
            header[9] = '\0';
            targetCount = atoi(header);

            /*fprintf(stderr, "srccnt: %d  targetCount: %d\n", srcCount, targetCount);*/
        }
        else
        {                          /* get errno */
            memset(header, 0, 10);
            retval = get_next_token(fd, header, 10);
            header[9] = '\0';
            srcCount = atoi(header);

            fprintf(stderr, "lbone_getDepots: server returned error %d\n", srcCount);
            return 1;
        }

        /* receive depots and build array */

        resolution = (double **) calloc(sizeof(double *) * (srcCount), 1);
        for (i=0; i < srcCount; i++)
        {
            resolution[i] = (double *) calloc(sizeof(double) * (targetCount), 1);
        }

        for (i = 0; i < srcCount; i++)
        {
            for (j=0; j < targetCount; j++)
            {
                memset(header, 0, 10);
                retval = get_next_token(fd, header, 10);
                header[9] = '\0';
                resolution[i][j] = atof(header);
            }
        } 
    } else
    {               /* function timed out */
        fprintf(stderr, "lbone_client: message header did not arrive before timeout.\n");
        //depots = NULL;  /* SA 1-22-03  why dset this to nNULL? */
        close(fd);
        return -1;
    }

    close(fd);

  /* return depots */
    *resArray = resolution;
    return 0 ;
}




/*
 * Check status of depot(s)
 * takes depot array and a timeout value in seconds
 */

Depot *lbone_checkDepots(Depot *depots, LBONE_request request, int timeout) 
{
  int           i, j, count;
  int           num_valid;
  int           *valid;
  int           retval;
  Depot         *new_list;
  Info          **info;
  Global        g;
  pthread_t     tid;
  pthread_attr_t        attr;
  struct timeval        sleeper;


  g.working = 0;
  pthread_mutex_init(&g.lock, NULL);

  /* fork off a detached */
  retval = pthread_attr_init(&attr);
  if (retval != 0) {
    perror("lbone_client_lib: pthread_attr_init failed");
    exit(1);
  }

  retval = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (retval != 0) {
    perror("lbone_client_lib: pthread_setdetachstate failed");
    exit(1);
  }

  count = 0;
  while (depots[count] != NULL) count++;

  valid = (int *) malloc(sizeof(int) * (count));
  memset(valid, 0, sizeof(int) * count);
  info = (Info **) malloc(sizeof(Info) * (count));
  
  for (i=0; i < count; i++) {
    pthread_mutex_lock(&g.lock);
    g.working++;
    pthread_mutex_unlock(&g.lock);

    info[i] = (Info *) malloc(sizeof(Info));
    info[i]->id = i;
    info[i]->result = &valid[i];
    info[i]->timeout = timeout;
    info[i]->ibp = depots[i];
    info[i]->request = request;
    info[i]->g = &g;

    retval = pthread_create(&tid, &attr, lbone_depotStatus, (void *) info[i]);
    if (retval != 0) perror("lbone_checkDepots: pthread_create failed");
  }

  memset(&sleeper, 0, sizeof(struct timeval));
  sleeper.tv_usec = 100000;
  while (g.working > 0) select(0, NULL, NULL, NULL, &sleeper);
/*  while (g.working > 0) sleep(1); */

  num_valid = 0;
  for (i=0; i < count; i++) {
    if (valid[i] == 1) num_valid++;
    free(info[i]);
  }
  free(info);

  if (num_valid == 0) return NULL;

  new_list = (Depot *) malloc(sizeof(Depot) * (num_valid + 1));
  memset(new_list, 0, sizeof(Depot) * (num_valid + 1));

  j = 0;
  for (i=0; i < count; i++) {
    if (valid[i] == 1) {
      new_list[j] = (Depot) malloc(sizeof(struct ibp_depot));
      memset(new_list[j], 0, sizeof(struct ibp_depot));
      memcpy(new_list[j], depots[i], sizeof(struct ibp_depot));
      j++;
    }
  }
  new_list[j] = NULL;

  free(valid);
  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&g.lock);

  return new_list;
}


void *lbone_depotStatus(void *x) {
  int           id;
  Info          *info;
  IBP_DptInfo   status;
  IBP_timer     timer;
  char          *passwd;
  unsigned long int   total = 0, hard_avail = 0;

  info = (Info *) x;
  id = info->id;

  timer = (IBP_timer) malloc (sizeof (struct ibp_timer));
  memset(timer, 0, sizeof(struct ibp_timer));
  timer->ClientTimeout = info->timeout;

  passwd = (char *) strdup("ibp");
  
  status = IBP_status(info->ibp, IBP_ST_INQ, timer, passwd, 0, 0, 0);

#if 1
  if ( status != NULL )
  {
    if ( (status->Duration >= (info->request.duration * 3600 * 24)) ||
         (status->Duration == -1)) 
    {
        /* is there enough soft storage? */
        total = status->VolStor-status->VolStorUsed ;
        if ( total >= info->request.volatileSize )
        {
            /* there is enough soft storage */
            *info->result = 1;
        }
        /* is there enough hard storage? */
        hard_avail = status->StableStor - status->StableStorUsed;
        if ( total      >= info->request.stableSize &&
             hard_avail >= info->request.stableSize )
        {
            /* there is enough hard storage */
            *info->result = 1;
        } else 
        {
            /* regardless of whether there was enough 'soft', they've requested 
                more 'hard' than is available, so the depot doesn't have enough. */
            *info->result = 0;
        }
    }
  }
#endif 

#if 0
  if (status != NULL) {
    if (((status->StableStor - status->StableStorUsed) >= info->request.stableSize)
      && ((status->VolStor - status->VolStorUsed) >= info->request.volatileSize)
      && ((status->Duration) >= (info->request.duration * 3600 * 24)
      || (status->Duration == -1))) {

      *info->result = 1;
    }
  }
#endif

  pthread_mutex_lock(&info->g->lock);
  info->g->working--;
  pthread_mutex_unlock(&info->g->lock);

  free(timer);
  free(passwd);

  pthread_exit(NULL);
  return 0;  /* prevents compiler warning */
}



void sigalarm_handler(int dummy)
{
printf(" *** caught it ***\n");
  signal(SIGALRM, sigalarm_handler);
}




/*
 * nws_ping each depot and sort by highest bandwidth
 */
#if 0

Depot *lbone_sortByBandwidth(Depot *depots, int timeout) {
  int           i, count;
  double        *bandwidth;
  int           retval;
  Depot         *new_list;
  Info          **info;
  Global        g;
  pthread_t     tid;
  JRB           sorted, node;
  pthread_attr_t        attr;
  struct timeval        sleeper;


  g.working = 0;
  pthread_mutex_init(&g.lock, NULL);

  /* fork off a detached thread */
  retval = pthread_attr_init(&attr);
  if (retval != 0) {
    perror("lbone_client_lib: pthread_attr_init failed");
    exit(1);
  }

  retval = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (retval != 0) {
    perror("lbone_client_lib: pthread_setdetachstate failed");
    exit(1);
  }

  count = 0;
  while (depots[count] != NULL) count++;

  bandwidth = (double *) malloc(sizeof(double) * (count));
  memset(bandwidth, 0, sizeof(double) * count);
  info = (Info **) malloc(sizeof(Info) * (count));

  for (i=0; i < count; i++) {
    pthread_mutex_lock(&g.lock);
    g.working++;
    pthread_mutex_unlock(&g.lock);

    info[i] = (Info *) malloc(sizeof(Info));
    info[i]->id = i;
    info[i]->bandwidth = &bandwidth[i];
    info[i]->timeout = timeout;
    info[i]->ibp = depots[i];
    info[i]->g = &g;

    retval = pthread_create(&tid, &attr, lbone_pingDepot, (void *) info[i]);
    if (retval != 0) perror("lbone_checkDepots: pthread_create failed");
  }

  memset(&sleeper, 0, sizeof(struct timeval));
  sleeper.tv_usec = 100000;
  while (g.working > 0) select(0, NULL, NULL, NULL, &sleeper);

  sorted = make_jrb();

  for (i=0; i < count; i++) {
    jrb_insert_dbl(sorted, bandwidth[i], new_jval_v((void*) depots[i]));
    free(info[i]);
  }
  free(bandwidth);
  free(info);

  new_list = (Depot *) malloc(sizeof(Depot) * (count + 1));
  memset(new_list, 0, sizeof(Depot) * (count + 1));

  i = 0;
  jrb_rtraverse(node, sorted) {
    new_list[i] =  (Depot) malloc(sizeof(struct ibp_depot));
    memset(new_list[i], 0, sizeof(struct ibp_depot));
    memcpy(new_list[i], jval_v(node->val), sizeof(struct ibp_depot));
    i++;
  }
  new_list[i] = NULL;

  jrb_free_tree(sorted);

  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&g.lock);

  return new_list;
}
#endif

/*
 * nws_ping each depot and sort by highest bandwidth
 */

#if 0
int lbone_getBandwidth(Depot *depots, double **bwlist, int timeout)
{
  int           i, count;
  double        *bandwidth;
  int           retval;
  Info          **info;
  Global        g;
  pthread_t     tid;
  pthread_attr_t        attr;
  struct timeval        sleeper;


  g.working = 0;
  pthread_mutex_init(&g.lock, NULL);

  /* fork off a detached thread */
  retval = pthread_attr_init(&attr);
  if (retval != 0) {
    perror("lbone_client_lib: pthread_attr_init failed");
    exit(1);
  }

  retval = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  if (retval != 0) {
    perror("lbone_client_lib: pthread_setdetachstate failed");
    exit(1);
  }

  count = 0;
  while (depots[count] != NULL) count++;

  bandwidth = (double *) malloc(sizeof(double) * (count));
  memset(bandwidth, 0, sizeof(double) * count);
  info = (Info **) malloc(sizeof(Info) * (count));

  for (i=0; i < count; i++) {
    pthread_mutex_lock(&g.lock);
    g.working++;
    pthread_mutex_unlock(&g.lock);

    info[i] = (Info *) malloc(sizeof(Info));
    info[i]->id = i;
    info[i]->bandwidth = &bandwidth[i];
    info[i]->timeout = timeout;
    info[i]->ibp = depots[i];
    info[i]->g = &g;

    retval = pthread_create(&tid, &attr, lbone_pingDepot, (void *) info[i]);
    if (retval != 0) perror("lbone_checkDepots: pthread_create failed");
  }

  memset(&sleeper, 0, sizeof(struct timeval));
  sleeper.tv_usec = 100000;
  while (g.working > 0) select(0, NULL, NULL, NULL, &sleeper);
  *bwlist = bandwidth;

  free(info);

  pthread_attr_destroy(&attr);
  pthread_mutex_destroy(&g.lock);

  return 0;
}


/*
 *
 */

void *lbone_pingDepot(void *x) {
  int           id, nws_port;
  Info          *info;
  double        *bandwidth, latency;

  info = (Info *) x;
  id = info->id;
  bandwidth = info->bandwidth;

  nws_port = 9060;

  pthread_mutex_lock(&info->g->lock);
  nws_ping(info->ibp->host, nws_port, bandwidth, &latency);
  pthread_mutex_unlock(&info->g->lock);

  pthread_mutex_lock(&info->g->lock);
  info->g->working--;
  pthread_mutex_unlock(&info->g->lock);

  pthread_exit(NULL);
  return NULL;  /* prevents compiler warning */
}
#endif




int lbone_depotCount(ulong_t *totalDepots, ulong_t *liveDepots, Depot lboneServer, int timeout)
{
  int           retval;
  int           fd;
  int           type;
  int           fd_ready;
  int           bytes_read;
  char          version[10], header[10], header_temp[10];
  char          *temp, message[LBONE_MESSAGE_LENGTH];
  fd_set        read_fds;
  struct timeval        timer;
  struct itimerval      int_timer;


  /* check timeout value */

  if (timeout < 0) {
    fprintf(stderr, "lbone_client: illegal attempt to alter the space-time continuum.\n");
    fprintf(stderr, "  Please specify a timeout value >= 0.");
    return LBONE_ILLEGAL_TIMEOUT;
  }

  if (timeout > 0)
    memset(&timer, 0, sizeof(struct timeval));
    memset(&int_timer, 0, sizeof(struct itimerval));
    int_timer.it_value.tv_sec = (long) timeout;

  /* build string to send over socket */

  memset(version, 0, 10);
  strcpy(version, LBONE_CLIENT_VERSION);
  type = DEPOT_COUNT;

  temp = (char *) malloc(20);
  memset(temp, 0, 20);

  sprintf(temp, "%10s%10d", version, type);

  memset(message, 0, 20);
  strcpy(message, temp);
  free(temp);

  /* start signal handler and then start timer */

  signal(SIGALRM, sigalarm_handler);

  setitimer(ITIMER_REAL, &int_timer, NULL);

  /* request_connection() and write message */

  fd = request_connection(lboneServer->host, lboneServer->port);
  if (fd < 0) {
    perror("lbone_client: request_connection failed");
    return LBONE_CONNECTION_FAILED;
  }

  getitimer(ITIMER_REAL, &int_timer);
  timer.tv_sec = int_timer.it_value.tv_sec;
  int_timer.it_value.tv_sec = 0;
  int_timer.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &int_timer, NULL);

  retval = write(fd, message, 46);

  if (retval != 46) {
    fprintf(stderr, "lbone_client: write to socket failed. ");
    fprintf(stderr, "Only %d characters written.\n", retval);
    return LBONE_SOCKET_WRITE_FAILED;
  }

  /* get select() ready */

  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);

  /* receive reply header */

  if (timeout > 0)
    fd_ready = select(fd+1, &read_fds, NULL, NULL, &timer);
  else
    fd_ready = select(fd+1, &read_fds, NULL, NULL, NULL);

  if (fd_ready < 0) {
    perror("lbone_client: select returned an error");
    return LBONE_SELECT_ERROR;
  }

  if (fd_ready > 0) {

    memset(header, 0, 10);
    bytes_read = 0;
    while (bytes_read < 10) {
      memset(header_temp, 0, 10);
      retval = read(fd, header_temp, 10 - bytes_read);
      if (retval == 0) {
        perror("lbone_getDepots: read header failed");
        close(fd);
        return LBONE_SOCKET_READ_FAILED;
      }
      if (bytes_read == 0) {
        strncpy(header, header_temp, retval);
      } else {
        strncat(header, header_temp, retval);
      }
      bytes_read += retval;
    }
    header[9] = '\0';

    if (atoi(header) == SUCCESS) {  /* get total depots */

      memset(header, 0, 10);
      bytes_read = 0;
      while (bytes_read < 10) {
        memset(header_temp, 0, 10);
        retval = read(fd, header_temp, 10 - bytes_read);
        if (retval == 0) {
          perror("lbone_getDepots: read header count failed");
          close(fd);
          return LBONE_SOCKET_READ_FAILED;
        }
        if (bytes_read == 0) {
          strncpy(header, header_temp, retval);
        } else {
          strncat(header, header_temp, retval);
        }
        bytes_read += retval;
      }
      header[9] = '\0';
      *totalDepots = strtoul(header, NULL, 0);

      memset(header, 0, 10);      /* get live depots */
      bytes_read = 0;
      while (bytes_read < 10) {
        memset(header_temp, 0, 10);
        retval = read(fd, header_temp, 10 - bytes_read);
        if (retval == 0) {
          perror("lbone_getDepots: read header count failed");
          close(fd);
          return LBONE_SOCKET_READ_FAILED;
        }
        if (bytes_read == 0) {
          strncpy(header, header_temp, retval);
        } else {
          strncat(header, header_temp, retval);
        }
        bytes_read += retval;
      }
      header[9] = '\0';
      *liveDepots = strtoul(header, NULL, 0);

   } 
    else {                          /* get errno */

      memset(header, 0, 10);
      bytes_read = 0;
      while (bytes_read < 10) {
        memset(header_temp, 0, 10);
        retval = read(fd, header_temp, 10 - bytes_read);
        if (retval == 0) {
          perror("lbone_getDepots: read header error number failed");
          close(fd);
          return LBONE_SOCKET_READ_FAILED;
        }
        if (bytes_read == 0) {
          strncpy(header, header_temp, retval);
        } else {
          strncat(header, header_temp, retval);
        }
        bytes_read += retval;
      }
      header[9] = '\0';
      retval = atoi(header);
      fprintf(stderr, "lbone_getDepots: server returned error %d\n", retval);
      return retval;
    }
  }
  return 0;
}


int lbone_getProximity(Depot lboneServer, char *location, char *filename, int timeout)
{
    int                 retval          = 0;
    int                 i               = 0;
    int                 fd              = 0;
    int                 count           = 0;
    int                 fd_ready        = 0;
    char                version[10], header[10];
    char                message[LBONE_MESSAGE_LENGTH];
    char                *temp;
    char                **hosts;
    double              *proximity;
    fd_set              read_fds;
    struct timeval      timer;
    struct itimerval    int_timer;


    /* check parameters */

    if (location == NULL) location = (char *) strdup(" ");
    if (strlen(location) > 492) location[491] = '\0';

    if (filename == NULL)
    {
        fprintf(stderr, "lbone_client: lbone_getProximity: filename pointer = NULL\n");
        return LBONE_ILLEGAL_FILENAME;
    }

    /* check timeout value */

    if (timeout < 0) {
        fprintf(stderr, "lbone_client: lbone_getProximity: negative timeout value.\n");
        fprintf(stderr, "  Please specify a timeout value >= 0.");
        return LBONE_ILLEGAL_TIMEOUT;
    }

    if (timeout > 0)
        memset(&timer, 0, sizeof(struct timeval));

    memset(&int_timer, 0, sizeof(struct itimerval));
    int_timer.it_value.tv_sec = (long) timeout;

    /* build string to send over socket */

    memset(message, 0, LBONE_MESSAGE_LENGTH);
    sprintf(message, "%-10s%-10d%-492s", LBONE_CLIENT_VERSION, PROXIMITY_LIST, location);


    /* start signal handler and then start timer */

    signal(SIGALRM, sigalarm_handler);

    setitimer(ITIMER_REAL, &int_timer, NULL);

    /* request_connection() and write message */

    fd = request_connection(lboneServer->host, lboneServer->port);
    if ( 0) {
        perror("lbone_client: request_connection failed");
        return LBONE_CONNECTION_FAILED;
    }

    getitimer(ITIMER_REAL, &int_timer);
    timer.tv_sec = int_timer.it_value.tv_sec;
    int_timer.it_value.tv_sec = 0;
    int_timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &int_timer, NULL);

    retval = write(fd, message, LBONE_MESSAGE_LENGTH);
    if (retval != LBONE_MESSAGE_LENGTH) {
        fprintf(stderr, "lbone_client: write to socket failed. ");
        fprintf(stderr, "Only %d characters written.\n", retval);
        close(fd);
        return LBONE_SOCKET_WRITE_FAILED;
    }

    /* get select() ready */

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    /* receive reply header */

    if (timeout > 0)
        fd_ready = select(fd+1, &read_fds, NULL, NULL, &timer);
    else
        fd_ready = select(fd+1, &read_fds, NULL, NULL, NULL);

    if (fd_ready < 0)
    {
        perror("lbone_client: select returned an error");
        close(fd);
        return LBONE_SELECT_ERROR;
    }

    if (fd_ready > 0)
    {
        retval = get_next_token (fd, header, 10);
        if (retval < 0)
        {
           close(fd);
           return retval;
        }

        retval = get_next_token (fd, header, 10);
        if (retval < 0)
        {
           close(fd);
           return retval;
        }

        count = atoi(header);

        if (count < 0)
        {
            fprintf(stderr, "lbone_getDepots: server returned error %d\n", count);
            close(fd);
            return count;
        }

        /* receive hosts and proximities */

        hosts = (char **) calloc(count + 1, sizeof(char *)); /* +1 for NULL terminated */
        proximity = (double *) calloc(count, sizeof(double));

        for (i = 0; i < count; i++)
        {
            hosts[i] = (char *) calloc(1, 256);
            retval = get_next_token (fd, hosts[i], 256);
            if (retval < 0)
            {
               close(fd);
               return retval;
            }

            retval = get_next_token (fd, header, 10);
            if (retval < 0)
            {
               close(fd);
               return retval;
            }
            proximity[i] = atof(header);
        }

    }
    else    /* function timed out */
    {
        fprintf(stderr, "lbone_client: message header did not arrive before timeout.\n");
        close(fd);
        return LBONE_CLIENT_TIMEDOUT;
    }
    close(fd);

    /* write list to file */

    fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        fprintf(stderr, "lbone_client: open(\"%s\") failed.\n", filename);
        for (i=0; i < count; i++) if (hosts[i] != NULL) free(hosts[i]);
        if (hosts != NULL) free(hosts);
        if (proximity != NULL) free(proximity);
        return LBONE_OPEN_FAILED;
    }

    for (i=0; i < count; i++)
    {
        /* when +16 there was heap corruption. 64 avoids this, but this is
         * a non-ideal fix. */
        temp = (char *) calloc(1, strlen(hosts[i]) + 64);
        /*fprintf(stderr, "%s %-10f\n", hosts[i], proximity[i]);*/
        sprintf(temp, "%s %-10f\n", hosts[i], proximity[i]);
        retval = write(fd, temp, strlen(temp));
        free(temp);
        free(hosts[i]);
    }
    if (hosts != NULL) free(hosts);
    if (proximity != NULL) free(proximity);
    close(fd);

    return 0;
}

int lbone_getProximity1(Depot lboneServer, char *location, Depot *depots, double **proximities, int timeout)
{
    int                 retval          = 0;
    int                 i               = 0;
    int                 fd              = 0;
    int                 count           = 0;
    int                 fd_ready        = 0;
    char                header[10];
    char                host[IBP_MAX_HOSTNAME_LEN];
    char                message[LBONE_MESSAGE_LENGTH];
    char                *temp;
    char                **hosts;
    double              *proximity, *plist;
    fd_set              read_fds;
    struct timeval      timer;
    struct itimerval    int_timer;
    int                 j, k;
    int                 total;

    /* check parameters */

    if (location == NULL) location = (char *) strdup(" ");
    if (strlen(location) > 492) location[491] = '\0';

#if 0 // do we need to complain?
    if (proximities != NULL) {
      fprintf(stderr, "lbone_client: lbone_getProximity: proximitylist != NULL\n");
      return LBONE_ILLEGAL_PARAMETER;
    }
#endif

    if (depots == NULL) {
      fprintf(stderr, "lbone_client: lbone_getProximity: depots = NULL\n");
      return LBONE_ILLEGAL_PARAMETER;
    }

    /* check timeout value */

    if (timeout < 0) {
        fprintf(stderr, "lbone_client: lbone_getProximity: negative timeout value.\n");
        fprintf(stderr, "  Please specify a timeout value >= 0.");
        return LBONE_ILLEGAL_TIMEOUT;
    }

    if (timeout > 0)
        memset(&timer, 0, sizeof(struct timeval));

    memset(&int_timer, 0, sizeof(struct itimerval));
    int_timer.it_value.tv_sec = (long) timeout;

    /* build string to send over socket */

    memset(message, 0, LBONE_MESSAGE_LENGTH);
    sprintf(message, "%-10s%-10d%-492s", LBONE_CLIENT_VERSION, PROXIMITY_LIST, location);


    /* start signal handler and then start timer */

    signal(SIGALRM, sigalarm_handler);

    setitimer(ITIMER_REAL, &int_timer, NULL);

    /* request_connection() and write message */

    fd = request_connection(lboneServer->host, lboneServer->port);
    if ( 0) {
        perror("lbone_client: request_connection failed");
        return LBONE_CONNECTION_FAILED;
    }

    getitimer(ITIMER_REAL, &int_timer);
    timer.tv_sec = int_timer.it_value.tv_sec;
    int_timer.it_value.tv_sec = 0;
    int_timer.it_value.tv_usec = 0;
    setitimer(ITIMER_REAL, &int_timer, NULL);

    retval = write(fd, message, LBONE_MESSAGE_LENGTH);
    if (retval != LBONE_MESSAGE_LENGTH) {
        fprintf(stderr, "lbone_client: write to socket failed. ");
        fprintf(stderr, "Only %d characters written.\n", retval);
        close(fd);
        return LBONE_SOCKET_WRITE_FAILED;
    }

    /* get select() ready */

    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    /* receive reply header */

    if (timeout > 0)
        fd_ready = select(fd+1, &read_fds, NULL, NULL, &timer);
    else
        fd_ready = select(fd+1, &read_fds, NULL, NULL, NULL);

    if (fd_ready < 0)
    {
        perror("lbone_client: select returned an error");
        close(fd);
        return LBONE_SELECT_ERROR;
    }

    if (fd_ready > 0)
    {
        retval = get_next_token (fd, header, 10);
        if (retval < 0)
        {
           close(fd);
           return retval;
        }

        retval = get_next_token (fd, header, 10);
        if (retval < 0)
        {
           close(fd);
           return retval;
        }

        count = atoi(header);

        if (count < 0)
        {
            fprintf(stderr, "lbone_getDepots: server returned error %d\n", count);
            close(fd);
            return count;
        }

        /* receive hosts and proximities */

        hosts = (char **) calloc(count + 1, sizeof(char *)); /* +1 for NULL terminated */
        proximity = (double *) calloc(count, sizeof(double));

        for (i = 0; i < count; i++)
        {
            hosts[i] = (char *) calloc(1, 256);
            retval = get_next_token (fd, hosts[i], 256);
            if (retval < 0)
            {
               close(fd);
               return retval;
            }

            retval = get_next_token (fd, header, 10);
            if (retval < 0)
            {
               close(fd);
               return retval;
            }
            proximity[i] = atof(header);
        }

    }
    else    /* function timed out */
    {
        fprintf(stderr, "lbone_client: message header did not arrive before timeout.\n");
        close(fd);
        return LBONE_CLIENT_TIMEDOUT;
    }
    close(fd);

    total = 0;
    while (depots[total] != NULL) total++;

    plist = (double *) calloc(total, sizeof(double));
    memset(plist, 0, total*sizeof(double));
    // find the corresponding proximity for depots
    for (j=0; j<total; j++){
        if (depots[j]==NULL) plist[i]=0;
        else {
            plist[j]=0;
            for (k=0; k<count; k++){
                if (strcmp(depots[j]->host, hosts[k])==0) {
                    plist[j]=proximity[k];
                  break;
                }
            } // for k
        }
    } // for j
    *proximities = plist;
    
    if (proximity!=NULL) free(proximity);
    for (i=0; i < count; i++) if (hosts[i] != NULL) free(hosts[i]);
    if (hosts!=NULL) free(hosts);

    return 0;
}


/*
 * Text Handling Functions
 */

int get_next_token (int fd, char *buffer, int length)
{
    int         retval          = 0;
    int         bytes_read      = 0;

    if (fd < 0 || length < 1) return -1;
    if (buffer == NULL) return LBONE_RECEIVE_TO_NULL_BUFFER;

    while (bytes_read < length)
    {
        retval = read (fd, buffer + bytes_read, length - bytes_read);
        if (retval == -1)
            return LBONE_GET_TOKEN_FAILED;
        bytes_read += retval;
    }
    buffer[length - 1] = '\0';
    trim_leading_spaces (buffer);
    return 0;
}


void trim_leading_spaces (char *str)
{
    int                 i, j;

    if (str[0] != ' ')
    {
        trim_trailing_spaces(str);
        return;
    }

    while (str[0] == ' ')
    {
        i = strlen (str);
        for (j = 0; j < i; j++)
        {
            str[j] = str[j + 1];
        }
        str[i] = '\0';
    }
    trim_trailing_spaces(str);
    return;
}


void trim_trailing_spaces(char *str)
{
    int         i       = 0;

    for (i=0; (i<256)&&(str[i]!='\0'); i++)
    {
        if (str[i] == ' ')
        {
            str[i] = '\0';
            return;
        }
    }
    return;
}
