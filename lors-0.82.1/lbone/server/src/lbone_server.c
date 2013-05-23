/*              LBONE version 1.0:  Logistical Backbone
 *               University of Tennessee, Knoxville TN.
 *               Authors: S. Atchley, D. Jin, J. Plank
 *                   (C) 2001 All Rights Reserved
 *
 *                              NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appear in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * Neither the Institution (University of Tennessee) nor the Authors
 * make any representations about the suitability of this software for
 * any purpose. This software is provided ``as is'' without express or
 * implied warranty.
 *
 */

/*
 * lbone_server.c
 *
 * LBONE server code. Version name: alpha
 * 
 */

#include "lbone_base.h"
#include "lbone_search.h"
#include "lbone_server.h"
//#include "lbone_nws_query.h"

#include "lber.h"
#include "ldap.h"


/*
 * Function declarations
 */

extern void parse_commands();
extern void *socket_monitor();
extern void *client_thread();
extern void console();
extern void find_ldap_server();
extern void bail();
extern int  ldap_test();
extern void sigpipe_handler();
extern void parse_request();
extern int  get_local_hostname();
extern void send_list();
extern int serve_socket();
extern int call_depot();
extern JRB get_result();
extern void poll_depot();
extern void handle_client_request();
extern void handle_depot_count();
extern void handle_proximity_list();
extern void handle_unknown_request();
extern int  get_next_token();
extern void trim_leading_spaces();
extern void trim_trailing_spaces();
extern void send_failure();
extern int  find_latlon();
extern JRB  getProximity();
extern void set_depot_attr (LDAP *ldap, char *dn, char *attr, char *value);
int email_maintainer();
int notify_depot(LDAP		*ldap, LDAPMessage	*entry);


/* SAS: 05/14/02 */
void        handle_depot2depot_res(ClientInfo info, pthread_t tid);
DepotInfo *recv_depot_list(ClientInfo info, pthread_t tid, int src_cnt);
double **create_geo_resolution_array(pthread_t tid, DepotInfo *src_list, int src_cnt, 
                                     DepotInfo *target_list, int target_cnt);
int      send_resolution_array(ClientInfo info, double **res_array, 
                                int src_cnt, int target_cnt);
int send_next_token(int fd, char *buffer, int length);
DepotInfo get_depot_info(LDAP *ldap, LDAPMessage *entry);
double sphere_distance (double lat1, double lon1, double lat2, double lon2);

/* ESA: 07/23/2002 */
double **create_nws_resolution_array();
extern double **getBandwidthMatrix();



/*
 * Now the fun starts.
 * Check the command line arguments and check for a config file.
 * If it boots, set up socket monitor and client handler
 */

/*int main(int argc, char **argv, char **env) {     gcc doesn't like this*/
int main(int argc, char **argv)
{
    int                 retval = 0;
    char		*hostname = NULL;
    ServerConfig        server;
    pthread_t           tid = 0;
    struct rlimit       proc_limits;
    LDAP                *ldap;
    LDAPMessage         *entry = NULL, *result = NULL;
    ulong_t             sListed, sAvail, vListed, vAvail, tDepots, lDepots;
    time_t              tt;


   /*
    * intialize the config struct
    */
    server = (ServerConfig) malloc(sizeof(struct server_config));
    memset(server, 0 , sizeof(struct server_config));
    server->password = NULL;
    server->config_path = NULL;
    server->port = 0;
    server->ldaphost = NULL;
    server->socket = 0;
    server->poll = 1;
    retval = pthread_mutex_init(&server->lock, NULL);
    if (retval != 0) { perror("lbone_server: mutex init failed"); exit(1); }
    retval = pthread_mutex_init(&server->host_lock, NULL);
    if (retval != 0) { perror("lbone_server: mutex init failed"); exit(1); }

   /*
    * get configuration info
    */
    parse_commands(server, argc, argv);

    if (server->ldaphost == NULL)
    {
        bail(E_INVHOST, server->ldaphost);
    }

    fprintf(stdout, "lbone_server starting with the following parameters:\n");
    if (server->password != NULL) fprintf(stdout, "  password = %s\n", server->password);
    if (server->config_path != NULL) fprintf(stdout, "  config_path = %s\n", server->config_path);
    fprintf(stdout, "  poll = %d\n", server->poll);
    fprintf(stdout, "  port = %d\n", server->port);
    if (server->ldaphost != NULL) fprintf(stdout, "  ldaphost = %s\n\n", server->ldaphost);
    fflush(stdout);

   /*
    * if we made it here, we have all necessary config info and we have contacted
    * the ldap server.
    *
    * Set the limit of open file descriptors to the maximum possible
    */

    if (getrlimit(RLIMIT_NOFILE, &proc_limits) == -1)
        bail(E_GLIMIT, "Can't get resource limits");
 
    proc_limits.rlim_cur = proc_limits.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &proc_limits) == -1)
        bail(E_GLIMIT, "Can't set resource limits");


   /*
    * Time to set up the signal handler, call serve_known_socket and fork off the
    * socket monitor
    */

    /* signal(SIGPIPE, sigpipe_handler); */
    signal(SIGPIPE, SIG_IGN);

    hostname = (char *) get_local_hostname();
    if (hostname == NULL)
    {
        fprintf(stdout, "Did not find local hostname. Cannot continue\n");
        exit(1);
    }
    server->socket = serve_socket(hostname, server->port);
    if (server->socket >= 0)
    {
        fprintf(stdout, "  serving socket on %s\n", hostname);
        fflush(stdout);
    }
    else
    {
        fprintf(stdout, "lbone_server: serve_known_socket failed");
        exit(1);
    }


    /*
     * we fork off the socket_monitor to wait for clients 
     */
    retval = pthread_create(&tid, NULL, socket_monitor, (void *) server);
    if (retval != 0)
    {
        perror("lbone_server: pthread_create for socket_monitor");
        exit(1);
    }
    else fprintf(stdout, "socket_monitor started\n");

    fprintf(stdout, "lbone_server started\n");
    fflush(stdout);

    /*
     * use this thread to poll the registered depots' status
     */

    while(1)
    {

        if (server->poll == 1)
        {
            tDepots = 0; lDepots = 0; sListed = 0; sAvail = 0; vListed = 0; vAvail = 0;

            ldap = ldap_init (server->ldaphost, LDAP_PORT);
            retval = ldap_simple_bind_s (ldap, LBONE_LDAP, LBONE_PASS);
            retval = ldap_search_s (ldap, "ou=depots,o=lbone", LDAP_SCOPE_SUBTREE, "depotname=*", NULL, 0, &result);

            fprintf(stdout, "\nBegin polling depots:\n\n");
            if ((entry = ldap_first_entry (ldap, result)) != NULL) 
            {
                do 
                {
                    poll_depot (ldap, entry, &tDepots, &lDepots, 
                            &sListed, &sAvail, &vListed, &vAvail);
                }
                while ((entry = ldap_next_entry (ldap, entry)) != NULL);
            }

#if 0
            if ((entry = ldap_first_entry (ldap, result)) != NULL) {
                poll_depot (ldap, entry, &tDepots, &lDepots, &sListed, &sAvail, &vListed, &vAvail);

                while ((entry = ldap_next_entry (ldap, entry)) != NULL)
                    poll_depot (ldap, entry, &tDepots, &lDepots, &sListed, &sAvail, &vListed, &vAvail);
            }
#endif
            fprintf(stdout, "End polling\n\n");
            fflush(stdout);

            tt = time(0);

            fprintf(stdout, "*** LBONE STATUS *** %d\n", (int)tt);
            fprintf(stdout, "    %ld out of %ld depots responded\n", lDepots, tDepots);
            fprintf(stdout, "    %ld MB out of %ld MB of stable storage are available\n", 
                            sAvail, sListed);
            fprintf(stdout, "    %ld MB out of %ld MB of volatile storage are available\n", 
                            vAvail, vListed);
            fprintf(stdout, "\n");
            fflush(stdout);

            ldap_unbind (ldap);

            server->totalDepots = tDepots;
            server->liveDepots = lDepots;
            server->stableListedMb = sListed;
            server->stableAvailMb = sAvail;
            server->volListedMb = vListed;
            server->volAvailMb = vAvail;

            if ( result != NULL )
            {
                ldap_msgfree(result);
                result = NULL;
            }
        }

        /* remove sleep b/c the number of depots does not allow a repoll
         * before flagging the depot as dead. */
        sleep(60 * 45); /* poll every 45 minutes */
    }
}


/*
 *
 *
 */

void poll_depot(LDAP		*ldap,
		LDAPMessage	*entry,
		ulong_t		*tDepots,
		ulong_t		*lDepots,
		ulong_t		*sListed,
		ulong_t		*sAvail,
		ulong_t		*vListed,
		ulong_t         *vAvail )

{

    int		retval;
    char	**vals;


    retval = call_depot (ldap, entry);
    if (retval == 0) *lDepots += 1;

    *tDepots += 1;

    vals = ldap_get_values(ldap, entry, "StableStorage");
    *sListed += strtoul(vals[0], NULL, 0);
    ldap_value_free(vals);

    vals = ldap_get_values(ldap, entry, "AvailableStableStorage");
    *sAvail += strtoul(vals[0], NULL, 0);
    ldap_value_free(vals);

    vals = ldap_get_values(ldap, entry, "VolatileStorage");
    *vListed += strtoul(vals[0], NULL, 0);
    ldap_value_free(vals);

    vals = ldap_get_values(ldap, entry, "AvailableVolatileStorage");
    *vAvail += strtoul(vals[0], NULL, 0);
    ldap_value_free(vals);

    /* email status poll */
    notify_depot(ldap, entry);

    return;

}

int notify_depot(LDAP		*ldap, LDAPMessage	*entry)
{
    int		retval;
    char	**vals;
    char    *dn, *email_name, *depotname;
    char    lastmail[32];
    char    period[32], unit[32];
    time_t  t_lastupdate, t_lastmail, t_notify, t_current;
    /* <EMAIL POLLING> ------------------------------------- */
    dn = ldap_get_dn(ldap, entry);


    /*fprintf(stderr, "notify_depot: %s\n", dn);*/
    /* get notify period */
    vals = ldap_get_values(ldap, entry, "notifyOwnerPeriod");
    retval = sscanf(vals[0], "%s %s", period, unit);

    /*fprintf(stderr, "vals[0]: %s period: %s unit: %s\n", vals[0], period, unit);*/

    ldap_value_free(vals);
    if ( retval == 2 )
    {
        if ( strcmp(unit, "hours") == 0 || 
             strcmp(unit, "hour") == 0 )
        {
            t_notify = atoi(period) * 3600; /* times secs/hour */
        } else 
        if ( strcmp(unit, "day") == 0 || 
             strcmp(unit, "days") == 0 )
        {
            /* if not hours assume days. */
            t_notify = atoi(period) * 3600 * 24; /* times secs/day */
        } else 
        if ( strcmp(unit, "week") == 0 || 
             strcmp(unit, "weeks") == 0 )
        {
            t_notify = atoi(period) * 3600 * 24 * 7; /* times secs/week*/

        } else 
        {
            /* if unrecognized unit, assume month */
            t_notify = atoi(period) * 3600 * 24 * 30; /* times secs/month*/
        }
        /*fprintf(stderr, "fetching lastUpdate\n");*/
        vals = ldap_get_values(ldap, entry, "lastUpdate");
        t_lastupdate = (vals[0]==NULL ? t_current : atoi(vals[0]));
        ldap_value_free(vals);

        t_current = time(0);

        /*fprintf(stderr, "fetching lastMail\n");*/
        vals = ldap_get_values(ldap, entry, "lastMail");
        if ( vals == NULL )
        {
            /* set lastmail if unset */
            sprintf(lastmail, "%d", (int)t_current);
            set_depot_attr(ldap, dn, "lastMail", lastmail);
            t_lastmail = 0;
        } else 
        {
            t_lastmail = atoi(vals[0]);
        }
        ldap_value_free(vals);


        /*fprintf(stderr, "time: %d\t%d\t%d\t%d\n", t_notify, t_lastupdate, */
                /*t_lastmail, t_current);*/

        /* we mail if 2 conditions are met:
         * 1. the lastUpdate+notify is less than current time.
         * 2. the lastMail+notify is less than current time.
         */
        if ( t_lastupdate+t_notify < t_current &&
             t_lastmail+t_notify < t_current)
        {
            int i;
            vals = ldap_get_values(ldap, entry, "email");
            email_name = strdup(vals[0]);
            ldap_value_free(vals);

            vals = ldap_get_values(ldap, entry, "depotname");
            depotname = strdup(vals[0]);
            ldap_value_free(vals);
            for(i=0; i < strlen(depotname); i++)
            {
                if ( depotname[i] == ' ' )
                {
                    depotname[i] = '+';
                }
            }

            fprintf(stdout, "Email Maintainer( %s of %s )\n", email_name, depotname);
            retval = email_maintainer(ldap, entry, email_name, depotname, 
                                      period, unit);
            if ( retval == 0 )
            {
                sprintf(lastmail, "%d", (int)t_current);
                set_depot_attr(ldap, dn, "lastMail", lastmail);
            }
        }
    } 
    /* </EMAIL POLLING> ------------------------------------- */

   return 0;
}


int email_maintainer(LDAP		    *ldap,
		             LDAPMessage	*entry,
                     char           *name,
                     char           *depotname,
                     char           *period,
                     char           *unit)
{
    struct stat   email_stat, send_stat;
    char         *email, *email2;
    char          cmd[512];
    int           ret;
    FILE         *fd, *fcmd;

    if ( stat("time_expired.email", &email_stat) != 0 )
    {
        fprintf(stderr, "email_maintainer() failed to stat() time_expired.email\n");
        return -1;
    }
    email = (char *)calloc(1, sizeof(char)*email_stat.st_size);
    email2 = (char *)calloc(1, 2*sizeof(char)*email_stat.st_size);

    fd = fopen("time_expired.email", "r");
    if ( fd == NULL )
    {

        fprintf(stderr, "email_maintainer() failed to open time_expired.email\n");
        free(email);
        free(email2);
        return -1;
    }

    ret = fread (email, 1, email_stat.st_size, fd);
    if ( ret == -1 )
    {

        fprintf(stderr, "email_maintainer() failed to read %d bytes from "
                        "time_expired.email\n", (int)email_stat.st_size);
        fclose(fd);
        free(email);
        free(email2);
        return -1;
    }
    sprintf(email2, email, name, period, unit, period, unit, depotname);

    /*sprintf(cmd, "/bin/cat >> %s", name);*/
    /*sprintf(email2, email, name, period, unit, period, unit, depotname);*/
    
    memset(cmd, 0, 511);
    if ( stat("/usr/sbin/sendmail", &send_stat) == 0 )
    {
        sprintf(cmd, "/usr/sbin/sendmail %s < email.cache", name);
    } else if ( stat("/usr/lib/sendmail", &send_stat) == 0 )
    {
        sprintf(cmd, "/usr/lib/sendmail %s < email.cache", name);
    } else 
    {
        sprintf(cmd, "ls");
    }

#if 1 
    fprintf(stdout, "Calling popen()\n");
    fcmd = fopen("email.cache", "w");
    if ( fcmd == NULL )
    {
        fprintf(stderr, "email_maintainer() failed to popen() \"%s\"\n", "email.cache");
        fclose(fd);
        free(email);
        free(email2);
        return -1;
    }

    fprintf(stdout, "Calling fwrite()\n");
    ret = fwrite(email2, 1, strlen(email2), fcmd);
    if ( ret == -1 )
    {
        fprintf(stderr, "email_maintainer() failed to write to fopen()'d file.\n");
        fclose(fcmd);
        fclose(fd);
        free(email);
        free(email2);
        return -1;
    }

    fprintf(stderr, "Calling fclose()\n");
    fclose(fcmd);
    system(cmd);
#endif
    fclose(fd);
    free(email);
    free(email2);
    return 0;
}





/*
 * Print error message, call lbone_error and quit.
 */

void bail(int lbone_errno, char *message) {
  lbone_error(lbone_errno, message);
  exit(1);
}



/*
 * Parse the command line and do preliminary error checking.
 */

void parse_commands(ServerConfig server, int argc, char **argv) {
  int	 i, temp_port, retval;
  int 	 passwd_set, port_set, host_set;
  char	 temp[1024];
  DIR	*config_dir;
  char	*config_file;
  IS	 is;
  int    poll_set       = 0;

  passwd_set = 0; port_set = 0; host_set = 0;


 /*
  * check for the correct number of commands (must be odd number)
  * this catches options that do not have values
  */
  if ((argc < 3) || (argc > 10) == 1)
    bail(E_USAGE, "incorrect number of arguments on the command line");

 /*
  * Read the command line first
  */
  /*for (i = 1; i < argc; i+=2) {*/
  for (i = 1; i < argc; i++) {

    if (strcmp(argv[i], "-pw") == 0) {
      server->password = (char *) strdup(argv[i+1]);
      passwd_set = 1;
    }
    else if (strcmp(argv[i], "-cp") == 0) {
      server->config_path = (char *) strdup(argv[i+1]);
      if (server->config_path[0] != '/')
        bail(E_USAGE, "config path does not begin with '/'");
      if ((config_dir = opendir(server->config_path)) == NULL) {
        fprintf(stdout, "The config file should be in %s\n", server->config_path);
        lbone_error(E_ACCDIR, server->config_path);
        fprintf(stdout, "\nContinuing with startup. Will look for /etc/lbone.cfg\n");
        free(server->config_path);
        server->config_path = NULL;
      }
      else {     /* directory exists */
        closedir(config_dir);
        if (strlen(server->config_path) < 1022) {
          if (server->config_path[strlen(server->config_path) - 1] != '/') {
            memset(temp, 0, 1024);
            sprintf(temp, "%s%s", server->config_path, "/");
            free(server->config_path);
            server->config_path = (char *) strdup(temp);
          }
        }
      }
    }
    else if (strcmp(argv[i], "-p") == 0) {
      temp_port = atoi(argv[i+1]);
      if ((temp_port < 1024) || (temp_port > 65535)) bail(E_USAGE, "port out of bounds");
      server->port = temp_port;
      port_set = 1;
    }
    else if (strcmp(argv[i], "-lh") == 0) {
      server->ldaphost = (char *) strdup(argv[i+1]);
      retval = ldap_test(server->ldaphost);
      if (retval == 0) {
        host_set = 1;
      }
      else {
        free(server->ldaphost);
        server->ldaphost = NULL;
      }
    }
    else if (strcmp(argv[i], "-no-poll") == 0) {
      server->poll = 0;
      poll_set = 1;
    }
    else if (argv[i][0] == '-' ) {
        fprintf(stderr, "param: %s\n", argv[i]);
      bail(E_USAGE, "unknown option entered on the command line");
    }
  }
  if (passwd_set == 0) bail(E_USAGE, "failed to set password");

 /*
  * Finished with command-line arguments
  *
  * Now, look for config file to find any missing parameters
  * If no config file, use defaults
  */

  if (((port_set == 1) && (host_set == 1)) == 1) { /* none are missing */
    fprintf(stdout, "All parameters set on command-line.\n");
  }
  else {      /* some parameters are missing, search config file */
    if (server->config_path == NULL) 
      server->config_path = (char *) strdup("/etc/");
    memset(temp, 0, 1024);
    if ((strlen(server->config_path) + strlen(LBONE_CFG_FILE)) > 1023) {
      fprintf(stdout, "lbone_server: config file path too long");
      exit(1);
    }
    sprintf(temp, "%s%s", server->config_path, LBONE_CFG_FILE);
    config_file = (char *) strdup(temp);
    fprintf(stdout, "The config file should be at %s\n", config_file);

    if ((is = new_inputstruct(config_file)) == NULL) {
      lbone_error(E_OFILE, config_file);
      if (strcmp(server->config_path, "/etc/") != 0) {
        fprintf(stdout, "\nContinuing with startup. Will try /etc/lbone.cfg.\n");
        if ((is = new_inputstruct(DEFAULT_CFG)) == NULL)
          lbone_error(E_OFILE, "/etc/lbone.cfg");
      }
      if (is == NULL) {
       /*
        * Keep any command line arguments
        */
        fprintf(stdout, "\nContinuing with startup. Will use defaults for any missing options.\n");
        if (port_set == 0) server->port = DEFAULT_PORT;
        if (host_set == 0) {
          server->ldaphost = (char *) strdup(LDAP_DEFAULT);
          retval = ldap_test(server->ldaphost);
          if (retval == -1) {
            fprintf(stdout, "We can't seem to find a working lbone/ldap server\n");
            bail(E_INVHOST, server->ldaphost);
          }
        }
        if (config_file != NULL) free(config_file);
        free(server->config_path);
        server->config_path = NULL;
        return;
      }
    }
    while (get_line(is) >= 0) {
      if (is->NF != 2)   /* ignore if keyword has no value */
        continue;
      if (is->fields[0][0] == '#')  /* ignore comment lines */
        continue;

      if (strcmp(is->fields[0], "LDAPHOST") == 0) {
        if (host_set == 0) {
          server->ldaphost = (char *) strdup(is->fields[1]);
          retval = ldap_test(server->ldaphost);
          if (retval == 0) {
            host_set = 1;
          }
          else {
            free(server->ldaphost);
            server->ldaphost = NULL;
          }
        }
      }
      else if (strcmp(is->fields[0], "LBONE_PORT") == 0) {
        if (port_set == 0) {
          temp_port = atoi(is->fields[1]);
          if ((temp_port < 1024) || (temp_port > 65535)) bail(E_USAGE, "port out of bounds");
          server->port = temp_port;
        }
      }
    }
    if (is != NULL) jettison_inputstruct(is);
    free(config_file);
  }
}



/*
 * Check if a valid lbone/ldap server exists at that address
 */

int ldap_test(char *ldap_server) {
  int	 retval;
  char	*temp;
  LDAP	*ldap;

  ldap = ldap_init (ldap_server, LDAP_PORT);
  retval = ldap_simple_bind_s (ldap, LBONE_LDAP, LBONE_PASS);

//if (retval != 0) {
  if (retval != LDAP_SUCCESS) {
    temp = (char *) malloc(sizeof(char) * strlen(ldap_server) + 30);
    sprintf(temp, "lbone_server: ldap server %s", ldap_server);
    ldap_perror(ldap, temp);
    fprintf(stdout, "\n");
    free(temp);
    return -1;
  }
  fprintf(stdout, "Successfully contacted ldap server %s\n\n", ldap_server);
  ldap_unbind(ldap);
  return 0;
}


/*
 * Catch and ignore those nasty SIGPIPE signals
 */

void sigpipe_handler(int dummy)
{
  signal(SIGPIPE, sigpipe_handler);
}



/*
 * Called by main(), it accepts connections and starts client threads
 */

void *socket_monitor(void *x) {
  int			new_fd, retval;
  ServerConfig		server;
  struct sockaddr	client_addr;
  socklen_t		addr_len;
  ClientInfo		info;
  pthread_attr_t	attr_obj;

  //addr_len = sizeof(client_addr);
  addr_len = sizeof(struct sockaddr);

  server = (ServerConfig) x;

  while(1) {

    new_fd = accept(server->socket, &client_addr, &addr_len);
    if (new_fd == -1) {
      perror("lbone_server: accept failed");
      continue;
    } 

    /* create client info */
    info = (ClientInfo) malloc(sizeof(struct client_info));
    info->server = server;
    info->fd = new_fd;

    /* fork off a detached client_thread */
    retval = pthread_attr_init(&attr_obj);
    if (retval != 0) {
      perror("lbone_server: pthread_attr_init failed");
      /* do not exit - respond with errno and continue */
      exit(1);
    }

    retval = pthread_attr_setdetachstate(&attr_obj, PTHREAD_CREATE_DETACHED);
    if (retval != 0) {
      perror("lbone_server: pthread_setdetachstate failed");
      /* do not exit - respond with errno and continue */
      exit(1);
    }

    retval = pthread_create(&info->tid, &attr_obj, client_thread, (void *) info);
    if (retval != 0) {
      perror("lbone_server: pthread_create failed for client");
      /* do not exit - respond with errno and continue */
      exit(1);
    }
  }
}




/*
 *
 */

void *client_thread(void *x) {
  int		retval, type;
  pthread_t	tid;
  char		temp[40]; 
  char		*version = NULL;
  ClientInfo	info;
  Request	req;
  ServerConfig	server;
  time_t	mytime;
  struct timeval	tv;
  /* test for getpeername() */
  struct sockaddr       name;
  int                   namelen = 0;
  //struct hostent        *host   = NULL;
  char                  *addr   = NULL;
  /*
  char                  *client = "none";
  */
  struct sockaddr_in    client_addr;
  in_addr_t              naddr;
  struct in_addr        sin;

  info = (ClientInfo) x;
  server = (ServerConfig) info->server;
  tid = pthread_self();

#if 1
  namelen = sizeof(struct sockaddr_in);
  pthread_mutex_lock(&server->host_lock);
  retval = getpeername( info->fd, (struct sockaddr *)&client_addr, (socklen_t*) &namelen);
  if (retval == 0)
  {
      addr = strdup(inet_ntoa(client_addr.sin_addr));
#if 0
      host = gethostbyaddr((char*)&client_addr.sin_addr, sizeof(int), AF_INET);
      if (host != NULL)
      {
          client = strdup(host->h_name);
      }
      else
      {
          perror("gethostbyaddr() failed: ");
      }
#endif
  }
  else
  {
	  addr = strdup("none");
      perror("getpeername() failed:");
  }
  pthread_mutex_unlock(&server->host_lock);
#endif

  gettimeofday(&tv, NULL);
  mytime = (time_t) tv.tv_sec;
  memset(temp, 0, 40);
  sprintf(temp, "%d", (int)mytime);/*ctime(&mytime));*/
  temp[strlen(temp) - 1] = '\0';

  fprintf(stdout, "New client thread %ld started on fd %d from %s at %s\n", tid, info->fd, addr, temp);
  fflush(stdout);

  memset(&req, 0, sizeof(struct request));

  /*
   * Determine if it is a client request or a console
   */

  memset(temp, 0, 30);
  retval = read(info->fd, temp, 10);
  if (retval != 10) {
    perror("lbone_server: client_thread: read version failed");
    close(info->fd);
    pthread_exit(NULL);
  }
  version = (char *) strdup(temp);

  memset(temp, 0, 30);
  retval = read(info->fd, temp, 10);
  if (retval != 10) {
    perror("lbone_server: client_thread: read type failed");
    close(info->fd);
    pthread_exit(NULL);
  }
  type = atoi(temp);
  
  memset(temp, 0 , 30);
  fprintf(stdout, "client %ld: version %s and type ", tid, version);
  if (type == CLIENT_REQUEST) sprintf(temp, "CLIENT");
  else if (type == DEPOT_COUNT) sprintf(temp, "DEPOT_COUNT");
  else if (type == CONSOLE_REQUEST) sprintf(temp, "CONSOLE");
  else if (type == PROXIMITY_LIST) sprintf(temp, "PROXIMITY_LIST");
  else if (type == DEPOT2DEPOT_RESOLUTION) sprintf(temp, "DEPOT2DEPOT_RESOLUTION");
  else (sprintf(temp, "UNKNOWN"));
  fprintf(stdout, "%s\n", temp);


  if (type == CLIENT_REQUEST) handle_client_request(info, &req, tid);
  else if (type == DEPOT_COUNT) handle_depot_count(info, tid);
  else if (type == PROXIMITY_LIST) handle_proximity_list(info, tid);
  else if (type == DEPOT2DEPOT_RESOLUTION) handle_depot2depot_res(info, tid);
  else handle_unknown_request(info, tid);

  fprintf(stderr, "client thread %ld finished\n", tid);
  fflush(stdout);
  if (version != NULL) free(version);
  if (addr != NULL) free(addr);
  close(info->fd);
  if (info != NULL) free(info);
  pthread_exit(NULL);
  return NULL;
  /*//exit(0);  prevents gcc warning */
}



/*
 * handle_client_request()
 *
 */

void handle_client_request(ClientInfo info, Request *req, pthread_t tid)
{
    JRB			return_list	= NULL;
    JRB			rnode		= NULL;
    ServerConfig	server		= NULL;
    char		**vals		= NULL;

    server = (ServerConfig) info->server;

  /*
   * Parse client request, call get_result
   */

  parse_request(req, info->fd);

  if ( pthread_mutex_lock(&server->lock) != 0 )
  {
      fprintf(stderr, "pthread_mutex_lock() ERROR.\n");
  }

  fprintf(stdout, "  thread %ld: numDepots = %d\n", tid, req->numDepots);
  fprintf(stdout, "  thread %ld: stableSize = %ld\n", tid, req->stableSize);
  fprintf(stdout, "  thread %ld: volatileSize = %ld\n", tid, req->volatileSize);
  fprintf(stdout, "  thread %ld: duration = %ld\n", tid, req->duration);
  if (req->location != NULL)
    fprintf(stdout, "  thread %ld: location = %s\n", tid, req->location);

  if ( pthread_mutex_unlock(&server->lock) != 0 )
  {
      fprintf(stderr, "pthread_mutex_unlock() ERROR.\n");
  }

  return_list = (JRB) get_result (req, server);
  if ( return_list == NULL )
  {
      fprintf(stderr, "return_list == NULL  FAILURE\n");
      return;
  }

  /*
   * convert results to lbone_reply string and send over socket
   */

  send_list(return_list, info->fd);


  /*
   * print status messages - remove eventually
   */

  if (return_list != NULL) {
    fprintf(stdout, "thread %ld: list returned\n", tid);
  } else {
    fprintf(stdout, "thread %ld: no list\n", tid);
  }

  pthread_mutex_lock(&server->lock);

  /*
  fprintf (stdout, "Thread %ld: Returned depots:\n", tid);
  jrb_traverse (rnode, return_list) {
    vals = (char **) rnode->val.v;
    fprintf (stdout, "Thread %ld: hostname=%s\tport=%s\n", tid, vals[0], vals[1]);
  }
  */
  
  pthread_mutex_unlock(&server->lock);
  /*
  if (return_list != NULL)
  {
      jrb_traverse(rnode, return_list)
      {
        vals = (char **) rnode->val.v;
	if (vals != NULL)
	{
            if (vals[1] != NULL) free(vals[1]);
            if (vals[0] != NULL) free(vals[0]);
            free(vals);
	}
      }
      jrb_free_tree(return_list);
  }
  */
  if (req->location != NULL) free(req->location);

  return;
}




/*
 * void handle_depot_count()
 *
 */

void handle_depot_count (ClientInfo info, pthread_t tid)
{
    int			retval	= 0;
    char		header[46];
    ServerConfig	server	= NULL;

    server = (ServerConfig) info->server;
    memset(header, 0, 46);
    sprintf(header, "%-10d%-16ld%-16ld", SUCCESS, server->totalDepots, server->liveDepots);

    retval = write(info->fd, header, 46);

    fprintf(stdout, "Thread %ld: sending %ld total depots %ld live depots\n", (long) tid, server->totalDepots, server->liveDepots);

    if (retval != 46)
        fprintf(stderr, "lbone_server: client %ld: send depot count failed\n", tid);

    return;
}


void handle_depot2depot_res(ClientInfo info, pthread_t tid)
{
    int		    i		= 0;
    int             fd          = 0;
    int             retval      = 0;
    int             err_number  = 0;
    char            *temp       = NULL;
    double         **res_array  = NULL;
    ServerConfig    server      = NULL;

    DepotInfo         *src_list = NULL, 
                   *target_list = NULL;

    int             type, src_cnt = 0, target_cnt = 0;

    server = (ServerConfig) info->server;
    fd = info->fd;

    temp = (char*) calloc(1, 492);
    if (temp == NULL)
    {
        err_number = LBONE_CALLOC_FAILED;
        send_failure(fd, err_number);
        return;
    }

    retval = get_next_token(fd, temp, 10);
    if (retval < 0)
    {
        err_number = LBONE_SOCKET_READ_FAILED;
        send_failure(fd, err_number);
        free(temp);
        return;
    }
    type = atoi(temp);

    fprintf(stdout, "Thread %ld: Type %d\n", tid, type);

    retval = get_next_token(fd, temp, 10);
    if (retval < 0)
    {
        err_number = LBONE_SOCKET_READ_FAILED;
        send_failure(fd, err_number);
        free(temp);
        return;
    }
    src_cnt = atoi(temp);
    fprintf(stdout, "Thread %ld: src_cnt %d\n", tid, src_cnt);

    retval = get_next_token(fd, temp, 10);
    if (retval < 0)
    {
        err_number = LBONE_SOCKET_READ_FAILED;
        send_failure(fd, err_number);
        free(temp);
        return;
    }
    target_cnt = atoi(temp);

    fprintf(stdout, "Thread %ld: target_cnt %d\n", tid, target_cnt);

    src_list = recv_depot_list(info, tid, src_cnt);
    target_list = recv_depot_list(info, tid, target_cnt);

    switch (type)
    {
        case LBONE_RES_GEO:
            res_array = create_geo_resolution_array(tid, src_list, src_cnt, 
                                                    target_list, target_cnt);
            /* we call this twice which fails because the client has already
             * closed the socket */
    //        send_resolution_array(info, res_array, src_cnt, target_cnt);
            break;
        case LBONE_RES_NWS:
            /* SA: 1/22/03 */
            /* the nws code has changed - the old code is broken
             * the new code will require modifications */
            fprintf(stdout, "Thread %ld: Ignoring NWS resolution request\n", tid);
            fflush(stdout);
    		for (i=0; i < src_cnt; i++) free_depot_info(src_list[i]);
  		if (src_list != NULL) free(src_list);
 		for (i=0; i < target_cnt; i++) free_depot_info(target_list[i]);
		if (target_list != NULL) free(target_list);
            return;
            res_array = create_nws_resolution_array(tid, src_list, src_cnt, 
                                                    target_list, target_cnt);
            break;
        default:
	    if (temp != NULL) free(temp);
            return;
    }
    send_resolution_array(info, res_array, src_cnt, target_cnt);

    for (i=0; i < src_cnt; i++) free_depot_info(src_list[i]);
    if (src_list != NULL) free(src_list);
    for (i=0; i < target_cnt; i++) free_depot_info(target_list[i]);
    if (target_list != NULL) free(target_list);

    for (i=0; i < src_cnt; i++) {
	 if (res_array[i] != NULL) free(res_array[i]);
    }
    if ( res_array != NULL ) free(res_array);
    if (temp != NULL) free(temp);
    return;
}

int      send_resolution_array(ClientInfo info, double **res_array, 
                                int src_cnt, int target_cnt)
{
    int         fd;
    int         ret, i, j, len;
    char        buf[64];

    fd  = info->fd;

    snprintf(buf, 64, "%-10d%-10d%-10d", LBONE_SUCCESS, src_cnt, target_cnt);
    send_next_token(fd, buf, strlen(buf));
    for(i=0; i < src_cnt; i++)
    {
        for (j=0; j < target_cnt; j++)
        {
            memset(buf, 0, 64);
            snprintf(buf, 64, "%-10f", res_array[i][j]);
            len = strlen(buf);

	    /*
            fprintf(stdout, "Thread %d: sending from %d to %d  w/len %d %s\n",
                    (int)info->tid, i, j, len, buf);
	    */
            buf[10]='\0';
            ret = send_next_token(fd, buf, 10);
            if ( ret != 10 )
            {
                send_failure(fd, ret);
                fprintf(stderr, "send failure %d\n", ret);
                return -1; 
            }
        }
    }
    return 0;
}
/* 
 * a src_list w/ lat and lon for each.
 * a target_list w/ lat and long for each,
 *
 * then call sphere_distance () on each combination of src to target.
 */
double **create_geo_resolution_array(pthread_t tid, DepotInfo *src_list, int src_cnt, 
                                     DepotInfo *target_list, int target_cnt)
{
    double          **res_array = NULL;
    int               i, j;

    res_array = (double **) calloc(sizeof (double*), src_cnt);
    if ( res_array == NULL ) return NULL;
    for (i=0; i < src_cnt; i++) {
        res_array[i] = (double *)calloc(sizeof(double), target_cnt);
    }


    for (i=0; src_list[i] != NULL; i++ )
    {
        for(j=0; target_list[j] != NULL; j++)
        {
            if ( strcmp(src_list[i]->hostname, target_list[j]->hostname) != 0 )
            {

                res_array[i][j] = sphere_distance(src_list[i]->lat, src_list[i]->lon,
                                           target_list[j]->lat, target_list[j]->lon);
                /*fprintf(stderr, "Thread %ld: from %d to %d  == %f\n", 
                        tid, i, j, res_array[i][j]); */
            } else 
            {
                res_array[i][j] = 0;
            }
            /*fprintf(stderr, "res_array[%d][%d] == %f\n", i, j, res_array[i][j]);*/
        }
    }

    return res_array;
}


/* 
 * a src_list w/ hostnames and port for each depot.
 * a target_list w/ hostnames and port for each,
 *
 * then convert to hostname:port strings and call getBandwidthMatrix()
 * on each combination of src to target.
 */
double **create_nws_resolution_array(int tid,
                                     DepotInfo *src_list,
                                     int src_cnt,
                                     DepotInfo *target_list,
                                     int target_cnt)
{
    double      **res_array     = NULL;
    int         i               = 0;
    char        **sources       = NULL;
    char        **targets       = NULL;
    char        temp[1024];

    sources = (char **) calloc(src_cnt, sizeof(char*));
    if (sources == NULL) return NULL;

    for (i = 0; i < src_cnt; i++)
    {
        memset (temp, 0, 1024);
        if (strlen(src_list[i]->hostname) < 1019)
        {
            if ((src_list[i]->port > 0) && (src_list[i]->port < LBONE_MAX_PORT))
            {
                sprintf(temp, "%s%s%s", src_list[i]->hostname, ":", "9060");
                sources[i] = (char *) strdup(temp);
            }
        }
        else
        {
            sources[i] = (char *) strdup("bad depot name");
        }
    }

    targets = (char **) calloc(target_cnt, sizeof(char*));
    if (targets == NULL) return NULL;

    for (i = 0; i < target_cnt; i++)
    {
        memset (temp, 0, 1024);
        if (strlen(target_list[i]->hostname) < 1019)
        {
            if ((target_list[i]->port > 0) && (target_list[i]->port < LBONE_MAX_PORT))
            {
                sprintf(temp, "%s%s%s", target_list[i]->hostname, ":", "9060");
                targets[i] = (char *) strdup(temp);
            }
        }
        else
        {
            targets[i] = (char *) strdup("bad hostname");
        }
    }

    res_array = getBandwidthMatrix(sources, src_cnt, targets, target_cnt);
    for (i=0; i < src_cnt; i++) {
    	if (sources[i] != NULL) free(sources[i]);
    }
    for (i=0; i < target_cnt; i++) {
    	if (targets[i] != NULL) free(targets[i]);
    }
    if (sources != NULL) free(sources);
    if (targets != NULL) free(targets);
    return res_array;
}

/* loop on the incomming socket inorder to read the depot names sent from the
 * client.
 *
 * save these names in the DepotInfo data struct, and pass an array of them back
 * to the client.  An ldap_search is necessary for each found depot to
 * determine the LAT LON numbers.
 *
 * the final element of this array is NULL.
 */
DepotInfo *recv_depot_list(ClientInfo info, pthread_t tid, int src_cnt)
{
    DepotInfo   *depot_list	= NULL;
    int         i;
    DepotInfo    tmp_depot	= NULL;
    char    *temp	= NULL;
    int      fd;
    int      retval, ld_ret;
    ServerConfig    server;
    char     filter[512];
    LDAP          *ldap	= NULL;
    LDAPMessage   *result	= NULL, *entry	= NULL;

    fd = info->fd;
    server = info->server;

    ldap = ldap_init (server->ldaphost, LDAP_PORT);
    if (ldap == NULL) {
        send_failure(fd, LBONE_LDAP_INIT_FAILED);
        return NULL;
    }

    ld_ret = ldap_simple_bind_s (ldap, "cn=root,o=lbone", LBONE_PASS);
    if ( ld_ret != LDAP_SUCCESS ) {
        /* error */ 
        fprintf(stderr, "ldap_simple_bind_s failed with error %d\n", ld_ret);
        return NULL;
    }

    temp = (char *)calloc(sizeof(char), 300);
    if ( temp == NULL ) 
    {
        ldap_unbind(ldap);
        return NULL;
    }

    depot_list = (DepotInfo *)calloc(sizeof (DepotInfo), src_cnt+1);
    if ( depot_list == NULL ) 
    {    
        ldap_unbind(ldap);
        free(temp);
        return NULL;
    }
    depot_list[src_cnt] = NULL;

    for  (i=0; i < src_cnt; i++)
    {
        /*tmp_depot = (DepotInfo) calloc(sizeof (struct depot_info), 1);*/
        memset(temp, 0, 300);
        retval = get_next_token(fd, temp, 256);
        if (retval < 0)
        {
            send_failure(fd, LBONE_SOCKET_READ_FAILED);
            free(temp);
            free(depot_list);
            ldap_unbind(ldap);
            return NULL;
        }

	//if (i > 0) ldap_msgfree(result);
        /* TODO: 9/19/02 lbone crashes eventually if src_list has a depot with
         * no name! I can't fix it now. */
        //fprintf(stderr, "Thread %ld: Receiving: %s\n", tid, temp);
        memset(filter, 0, 512);
        sprintf(filter, "hostname=%s", temp);
        ld_ret = ldap_search_s(ldap, "ou=depots,o=lbone", LDAP_SCOPE_SUBTREE, 
                                 filter, NULL, 0, &result);
        if ( ld_ret != LDAP_SUCCESS ) { /* error */
		fprintf(stderr, "ldap_search_s() failed\n");
	}

        if ((entry = ldap_first_entry (ldap, result)) != NULL) 
        {
            tmp_depot = get_depot_info(ldap, entry);
        } else 
        {
            ldap_unbind(ldap);
            /* error */
        }
        memset(temp, 0, 300);
        retval = get_next_token(fd, temp, 10);
        if (retval < 0)
        {
            send_failure(fd, LBONE_SOCKET_READ_FAILED);
            free(temp);
            free(depot_list);
	    ldap_msgfree(result);
            ldap_unbind(ldap);
            return NULL;
        }
        /*tmp_depot->port = atoi(temp);*/

        /*fprintf(stderr, "Thread %ld: received depot %s:%d\n\tlat %f  lon %f\n", 
                tid, tmp_depot->hostname, tmp_depot->port, 
                tmp_depot->lat, tmp_depot->lon);*/

        depot_list[i] = tmp_depot;

    }
    free(temp);
    ldap_msgfree(result);
    /*ldap_unbind(ldap);*/

    return depot_list;
}


/*
 * void handle_proximity()
 */

void handle_proximity_list (ClientInfo info, pthread_t tid)
{
    int             fd          = 0;
    int             retval      = 0;
    int             err_number  = 0;
    char            *temp       = NULL;
    ServerConfig    server      = NULL;
    JRB             list        = NULL;

    server = (ServerConfig) info->server;
    fd = info->fd;

    temp = (char*) calloc(1, 512);
    if (temp == NULL)
    {
        err_number = LBONE_CALLOC_FAILED;
        send_failure(fd, err_number);
        return;
    }

    retval = get_next_token(fd, temp, 492);
    if (retval < 0)
    {
        err_number = LBONE_SOCKET_READ_FAILED;
        send_failure(fd, err_number);
        free(temp);
        return;
    }

    list = getProximity(server, temp);
    send_list(list, fd);   
    if (temp != NULL) free(temp);
    return;
}





/*
 * parse_request()
 *
 */

void parse_request(Request *req, int fd) {
  char		temp[11], location[452];
  int		retval;

  memset(temp, 0, 11);
  retval = read(fd, temp, 10);
  if (retval != 10) {
    perror("lbone_server: client_thread: read request");
    close(fd);
    pthread_exit(NULL);
  }
  req->numDepots = atoi(temp);

  memset(temp, 0, 11);
  retval = read(fd, temp, 10);
  if (retval != 10) {
    perror("lbone_server: client_thread: read request");
    close(fd);
    pthread_exit(NULL);
  }
  req->stableSize = strtoul(temp, NULL, 0);
/*  request->stableSize *= (1024*1024); */

    /*fprintf(stderr, "parse a\n");*/
  memset(temp, 0, 10);
  retval = read(fd, temp, 10);
  if (retval != 10) {
    perror("lbone_server: client_thread: read request");
    close(fd);
    pthread_exit(NULL);
  }
  req->volatileSize = strtoul(temp, NULL, 0);
/*  request->volatileSize *= (1024*1024); */

  memset(temp, 0, 10);
  retval = read(fd, temp, 10);
  if (retval != 10) {
    perror("lbone_server: client_thread: read request");
    close(fd);
    pthread_exit(NULL);
  }
  req->duration = atol(temp);

  memset(location, 0, 452);
  retval = read(fd, &location, 452);
  if (retval != 452) {
    perror("lbone_server: client_thread: read location");
    close(fd);
    pthread_exit(NULL);
  }
  location[451] = '\0';
  if (strcmp(location, "NULL") != 0) 
    req->location = (char *) strdup(location);

  /*fprintf(stderr, "parse b\n");*/
}



/*
 *
 */

void send_list(JRB list, int fd) {
  char	message[1034];
  char	host[MAX_FILE_NAME], port[10];
  char	header[20];
  char	temp[20];
  char	**vals;
  int	count;
  int	retval;
  JRB	node;

  /* count how many hosts are being returned */

  count = 0;
  jrb_traverse(node, list) {
    count++;
  }


  /* create return header message */

  memset(temp, 0, LBONE_HEADER_LENGTH);
  sprintf(temp, "%-10d%-10d", SUCCESS, count);

  memset(header, 0, LBONE_HEADER_LENGTH);
  sprintf(header, "%s", temp);

  retval = write(fd, temp, LBONE_HEADER_LENGTH);

  if (retval != LBONE_HEADER_LENGTH) {    /* don't try to send depots */
    fprintf(stdout, "lbone_server: client %ld: send head failed\n", pthread_self());
  }
  else {                                  /* send depots */

    /* convert pointers to arrays and send over socket */

    jrb_traverse(node, list) {

      vals = jval_v(node->val);
      memset(host, 0, MAX_FILE_NAME);
      sprintf(host, "%s", vals[0]);

      memset(port, 0, 10);
      sprintf(port, "%s", vals[1]);

      memset(message, 0, 1034);
      sprintf(message, "%-256s%-10s", host, port);

      retval = write(fd, message, MAX_FILE_NAME + 10);

      if (retval != (MAX_FILE_NAME + 10)) {    /* don't try to send depots */
        fprintf(stdout, "lbone_server: client %ld: send depot failed\n", pthread_self());
        return;
      }
    }
  }
}

/* 
 * uses write() to send the buffer of 'length' size over the specified 'fd'
 */
int send_next_token(int fd, char *buffer, int length)
{
    int         retval          = 0;
    int         bytes_written   = 0;

    if (fd < 0 || length < 1) return LBONE_INVALID_PARAMS;
    if (buffer == NULL) return LBONE_RECEIVE_TO_NULL_BUFFER;

    while (bytes_written < length)
    {
        retval = write (fd, buffer + bytes_written, length - bytes_written);
        if (retval == -1) {
            perror("write:");
            return LBONE_SEND_TOKEN_FAILED;
        }
        bytes_written += retval;
    }
    return bytes_written;
    /*buffer[length - 1] = '\0';*/
    /*trim_leading_spaces (buffer);*/
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

    /* buffer[length] = '\0'; */
    /* this is a memory error - if the user passes a string of length n */
    /* then the last byte should n-1 */
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
    return;
}



void trim_trailing_spaces(char *str)
{
    int         i       = 0;
    int         length  = 0;

    length = strlen(str);

    for (i=length; i > 0; i--)
    {
        if (str[i] == ' ')
        {
            str[i] = '\0';
        }
        else
        {
            return;
        }
    }
    return;
}



/*
 * handle_unknown_request()
 */

void handle_unknown_request(ClientInfo info, pthread_t tid)
{
    int                 err_number;

    err_number = LBONE_UNKNOWN_CLIENT_REQUEST;
    send_failure(info->fd, err_number);

    fprintf(stdout, "lbone_server: client %ld: unknown client request\n", tid);
    return;
}


/*
 * send_failure()
 */

void send_failure(int fd, int lbone_err_number)
{
    char        message[20];

    sprintf(message, "%-10d%-10d", FAILURE, lbone_err_number);
    write(fd, message, 20);
    return;
}

