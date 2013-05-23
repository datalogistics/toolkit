#include <string.h>
#include "lbone_poll.h"
#include "ibp.h"

extern int test_and_set_depot_lock();


IBP_DptInfo connect_depot (LDAP *ldap, LDAPMessage *entry, char *hostname, char *port)
{
  IBP_depot	ibp;
  IBP_timer	timer;
  IBP_DptInfo	info;
  char		**vals, *passwd;
  
  ibp  = (IBP_depot) malloc (sizeof (struct ibp_depot));
  memset(ibp, 0, sizeof (struct ibp_depot));
  
  if (hostname == NULL || port == NULL) {
    vals = ldap_get_values (ldap, entry, "hostname");
    strncpy (ibp->host, vals[0], strlen(vals[0]));
    vals = ldap_get_values (ldap, entry, "port");
    ibp->port = atoi (vals[0]);
    if (vals[0] != NULL) free(vals[0]);
  } else {
    strncpy (ibp->host, hostname, strlen(hostname));
    ibp->port = atoi (port);
  }

  timer = (IBP_timer) malloc (sizeof (struct ibp_timer));
  memset(timer, 0, sizeof (struct ibp_timer));
  timer->ClientTimeout = 5;

  passwd = (char *) strdup("ibp");

  info = IBP_status(ibp, IBP_ST_INQ, timer, passwd, 0, 0, 0);
  free(passwd);
  return info;
}

/* Checks the depot status. */
int check_depot_status (LDAP *ldap, char *depotname)
{
  LDAPMessage *entry, *result;
  char	*filter;
  char	**status;
  int	length;

  length = strlen(depotname) + strlen("depotname=") + 1;
  filter = (char *) malloc (length);
  memset(filter, 0, length);
  sprintf(filter, "depotname=%s", depotname);
  
  ldap_search_s (ldap, "ou=depots,o=lbone", LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
  entry = ldap_first_entry (ldap, result);
  status = ldap_get_values (ldap, entry, "status");
 
  if (strcmp(status[0], DEPOT_IDLE) == 0) return IDLE;
  if (strcmp(status[0], DEPOT_LOCKED) == 0) return LOCKED;
  return UNAVAIL;
}

/* Sets the depot status. */
void set_depot_attr (LDAP *ldap, char *dn, char *attr, char *value)
{
  LDAPMod **mods;

  mods = (LDAPMod **) malloc (2 * sizeof (LDAPMod *));
  mods[0] = (LDAPMod *) malloc (sizeof (LDAPMod));
  mods[1] = NULL;

  mods[0]->mod_op = LDAP_MOD_REPLACE;
  mods[0]->mod_type = strdup (attr);
  mods[0]->mod_values = (char **) malloc (2 * sizeof (char *));
  mods[0]->mod_values[0] = strdup (value);
  mods[0]->mod_values[1] = NULL;

  ldap_modify_s (ldap, dn, mods);
  ldap_mods_free(mods, 1);
}

int call_depot (LDAP *ldap, LDAPMessage *entry)
{
  IBP_DptInfo info;
  char	sstorage[16];
  char	asstorage[16];
  char	vstorage[16];
  char	avstorage[16];
  char	duration[16];
  char	lastUpdate[16];
  char	poll[16];
  char	respond[16];
  char	**vals;
  char	*depotname;
  char	*hostname;
  char	*port;
  char	*dn;
  char	time[30];
  int	retval, polled, responded;
  time_t	t; 
  struct timeval tv;

  dn = ldap_get_dn (ldap, entry);

  vals = ldap_get_values (ldap, entry, "depotname");
  depotname = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values (ldap, entry, "hostname");
  hostname = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values (ldap, entry, "port");
  port = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values (ldap, entry, "polled");
  polled = atoi(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values (ldap, entry, "responded");
  responded = atoi(vals[0]);
  ldap_value_free(vals);


  info = connect_depot (ldap, entry, hostname, port);

/*  retval = test_and_set_depot_lock(lock, ldap, dn, vals[0]); */

  retval = 0;
  if (retval == 0) { 

    if (info != NULL) { 
 
      memset(sstorage, 0, 16);
      memset(asstorage, 0, 16);
      memset(vstorage, 0, 16);
      memset(avstorage, 0, 16);
      memset(duration, 0, 16);
      memset(poll, 0, 16);
      memset(respond, 0, 16);

      sprintf (sstorage, "%ld", info->StableStor);
      set_depot_attr (ldap, dn, "StableStorage", sstorage);
      sprintf (asstorage, "%ld", info->StableStor-info->StableStorUsed);
      set_depot_attr (ldap, dn, "AvailableStableStorage", asstorage);
      sprintf (vstorage, "%ld", info->VolStor);
      set_depot_attr (ldap, dn, "VolatileStorage", vstorage);
      sprintf (avstorage, "%ld", info->VolStor-info->VolStorUsed); 
      set_depot_attr (ldap, dn, "AvailableVolatileStorage", avstorage);
      sprintf (duration, "%ld", info->Duration);
      set_depot_attr (ldap, dn, "Duration", duration);

      memset(&tv, 0, sizeof(struct timeval));
      memset(time, 0, 30);
      gettimeofday(&tv, NULL);
      t = (time_t) tv.tv_sec;
      strncpy(time, ctime(&t), strlen(ctime(&t)));
      time[strcspn(time, "\n")] = '\0';
/*      set_depot_attr (ldap, dn, "lastUpdate", time); */

      memset(lastUpdate, 0, 16);
      sprintf(lastUpdate, "%ld", t);
      set_depot_attr (ldap, dn, "lastUpdate", lastUpdate);

      polled++;
      responded++;
      sprintf (poll, "%d", polled); 
      set_depot_attr (ldap, dn, "polled", poll);
      sprintf (respond, "%d", responded);
      set_depot_attr (ldap, dn, "responded", respond);
      

      set_depot_attr (ldap, dn, "status", DEPOT_IDLE);
      
      fprintf(stdout, "  %s responded at %s\n", depotname, time);
      return 0;
    }
    polled++;
    sprintf (poll, "%d", polled); 
    set_depot_attr (ldap, dn, "polled", poll);
    fprintf(stdout, "  *** %s did not respond ***\n", depotname);
    set_depot_attr (ldap, dn, "status", DEPOT_UNAVAIL);
    return DEPOTLOST;
  }
  fprintf(stdout, "  *** %s is locked and can't be updated ***\n", vals[0]);
  return DEPOTCHANGING;
}



int test_and_set_depot_lock(pthread_mutex_t lock, LDAP *ldap, char *dn, char *depotname) {
  int	status;

  if (pthread_mutex_trylock(&lock) != EBUSY) {
    status = check_depot_status (ldap, depotname);
    if (status != LOCKED) {
      set_depot_attr(ldap, dn, "status", DEPOT_LOCKED);
      pthread_mutex_unlock(&lock);
      return 0;
    }
    return -1;
  }
  return -1;
}






