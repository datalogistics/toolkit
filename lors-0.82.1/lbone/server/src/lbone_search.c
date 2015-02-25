#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <lber.h>
#include <ldap.h>
#include <sys/time.h>
#include "jrb.h"
#include "lbone_base.h"
#include "lbone_server.h"
#include "lbone_search.h"
#include "ngc/ngc.h"

extern int find_latlon();
extern int get_latlon();
extern IS string_inputstruct();
extern void jettison_string_inputstruct(IS is);
extern int get_string();
extern void get_environment();
extern DepotInfo get_depot_info();
extern void free_depot_info(DepotInfo d);
extern int depot_matches_request();
extern char **getLatLong (char **args);
extern int check_hostname();

double sphere_distance (double lat1, double lon1, double lat2, double lon2)
{
  double dlat;
  double dlon;
  double a, c;
  
  lat1 = lat1 * .017453293;
  lat2 = lat2 * .017453293;
  lon1 = lon1 * .017453293;
  lon2 = lon2 * .017453293;
  
  dlat = lat2 - lat1;
  dlon = lon2 - lon1;
  
  a = pow (sin (dlat / 2), 2) + cos (lat1) * cos (lat2) * pow (sin (dlon / 2), 2);
  c = 2 * asin (1 <= sqrt (a) ? 1 : sqrt (a));
  
  return 3956 * c;
}



JRB get_result (Request *request, ServerConfig server)
{
  JRB           list = NULL, node = NULL, temp_node = NULL;
  char          **vals = NULL, **depots = NULL; 
  LDAP          *ldap = NULL;
  LDAPMessage   *result = NULL, *entry = NULL;
  int           calc_dist = 0;
/*  char                *hostname; */
  char          *port = NULL;
  double        distance = 0;
  int           count = 0, retval = 0;
  double        lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
  struct timeval        tv;
  pthread_t     tid;
  DepotInfo     depot = NULL;

  tid = pthread_self();
  calc_dist = 0;

  if (request->location == NULL) {
    /*pthread_mutex_lock(&server->lock);*/
    fprintf(stdout, "Client %ld: no location specified. Returning random locations.\n", tid);
    /*pthread_mutex_unlock(&server->lock);*/
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec);
  }
  else {
    calc_dist = find_latlon(server, request->location, &lat1, &lon1);
fprintf(stdout, "lat = %f  lon = %f\n", lat1, lon1);
  }

  list = make_jrb();

  ldap = ldap_init (server->ldaphost, LDAP_PORT);
  if (ldap == NULL) {
    fprintf(stdout, "**** ldap_init() failed ****\n");

    /* return error */
  }
  ldap_simple_bind_s (ldap, "cn=root,o=lbone", LBONE_PASS);
  retval = ldap_search_s(ldap, "ou=depots,o=lbone", LDAP_SCOPE_SUBTREE, 
                          "depotname=*", NULL, 0, &result);
  if ( retval != LDAP_SUCCESS )
  {
      fprintf(stderr, "ldap_search_s() failed.: %d\n", retval);
      return NULL;
  }

  if ((entry = ldap_first_entry (ldap, result)) != NULL) {
    depot = get_depot_info(ldap, entry);

    if (depot_matches_request(depot, request)) {

      depots = (char **) malloc(sizeof(char *) * 2);
      depots[0] = (char *) strdup(depot->hostname);
      port = (char *) malloc(sizeof(char) * 10);
      memset(port, 0, 10);
      sprintf(port, "%-9d", depot->port);
      depots[1] = (char *) strdup(port);

      free(port);
      port = NULL;

      if (calc_dist == 1) {
        vals = ldap_get_values(ldap, entry, "lat");
        lat2 = strtod(vals[0], NULL);
	ldap_value_free(vals);
        vals = ldap_get_values(ldap, entry, "lon");
        lon2 = strtod(vals[0], NULL);
	ldap_value_free(vals);
        if ((lat2 >= -90) && (lat2 <= 90) && (lon2 >= -180) && (lon2 <= 180)) {
          distance = sphere_distance(lat1, lon1, lat2, lon2);
        } else {
          distance = rand();
        }
      } else {
        distance = rand();
      }

      jrb_insert_dbl(list, distance, new_jval_v(depots));
    }
    free_depot_info(depot);

    while ((entry = ldap_next_entry (ldap, entry)) != NULL) 
    { 
      depot = get_depot_info(ldap, entry);
      if (depot_matches_request(depot, request)) 
      {
        depots = (char **) malloc(sizeof(char *) * 2);
        depots[0] = (char *) strdup(depot->hostname);
        port = (char *) malloc(sizeof(char) *10);
        memset(port, 0, 10);
        sprintf(port, "%-9d", depot->port);
        depots[1] = (char *) strdup(port);

        free(port);
	port = NULL;

        if (calc_dist == 1) {
          vals = ldap_get_values(ldap, entry, "lat");
          lat2 = strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "lon");
          lon2 = strtod(vals[0], NULL);
	  ldap_value_free(vals);
          if ((lat2 >= -180) && (lat2 <= 180) && (lon2 >= -180) && (lon2 <= 180)) {
            distance = sphere_distance(lat1, lon1, lat2, lon2);
          } else {
            distance = rand();
          }
        } else {
          distance = rand();
        }
        jrb_insert_dbl(list, distance, new_jval_v(depots));
      }
      free_depot_info(depot);
    }
  }
  ldap_unbind(ldap);
  if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 

  count = 0;

  jrb_traverse(node, list) {
    count++;
    if ((count > request->numDepots) && (request->numDepots != -1)) {
      temp_node = node;
      node->flink->blink = node->blink;
      node->blink->flink = node->flink;
      node = node->blink;
      depots = (char **) jval_v(temp_node->val);
      free(depots[0]);
      free(depots[1]);
      free(depots);
      /* free(temp_node);  */ /* Should we call jrb_delete_node() ? */
      jrb_delete_node(temp_node);
    }
  }
  return list;
}






int find_latlon(ServerConfig server, char *location, double *lat1, double *lon1) {
  char  temp_location[1024];
  char  *host, *zip, *city, *state, *airport, *country, **hostlatlon;
  char  **args;
  int   i, count, retval, host_found;
  int   zip_found, airport_found, city_found, state_found, country_found;
  int   city_update, length;
  IS    is;
  double        latlon[2], lat, lon;

  i = 0; count = 0; host_found = 0;
  zip_found = 0; airport_found = 0; city_found = 0; state_found = 0;
  country_found = 0; city_update = 0; retval = 0; host_found = 0;
  host = NULL; zip = NULL; city = NULL; state = NULL; airport = NULL; country = NULL;
  latlon[0] = 0; latlon[1] = 0; lat = 0; lon = 0;

  is = string_inputstruct();

  get_string(is, location);

  for (i = 0; i < is->NF; i++) fprintf(stdout, "token[%d] = %s\n", i, is->fields[i]);

  for (i = 0; i < is->NF; i++) {

    if ((strcmp(is->fields[i], "connection=") == 0) ||
        (strcmp(is->fields[i], "monitoring=") == 0) ||
        (strcmp(is->fields[i], "power=") == 0) ||
        (strcmp(is->fields[i], "backup=") == 0) ||
        (strcmp(is->fields[i], "firewall=") == 0)) {

      city_update = 0;
      continue;
    }
    else if ((strcmp(is->fields[i], "hostname=") == 0) && (host_found == 0)) 
    {
      city_update = 0;
      if (i+1 >= is->NF || is->fields[i + 1] == NULL)
      {
          i++;
          continue;
      }
      args = (char **) malloc(sizeof(char *) * 2);
      args[0] = (char *) strdup(is->fields[i + 1]);
      args[1] = NULL;
      retval = check_hostname(args[0]);
      if (retval == 0) continue;
      pthread_mutex_lock(&server->lock);
        hostlatlon = getLatLong(args);
      pthread_mutex_unlock(&server->lock);
      if (hostlatlon != NULL) {
        if (hostlatlon[1] != NULL) {
          lat = atof(hostlatlon[1]);
          if (hostlatlon[2] != NULL) {
            lon = atof(hostlatlon[2]);
            if ((lat != 0) && (lon != 0)) {
              *lat1 = lat;
              *lon1 = lon;
              fprintf(stdout, "lat = %f  lon = %f\n", *lat1, *lon1);
              jettison_string_inputstruct(is);
	      if (args[0] != NULL) free(args[0]);
	      if (args != NULL) free(args);
              return 1;
            }
          }
        }
      }
      if (args[0] != NULL) free(args[0]);
      if (args != NULL) free(args);
    }
    else if (((strcmp(is->fields[i], "zip=") == 0) || 
              (strcmp(is->fields[i], "zipcode=") == 0)) && 
              (zip_found == 0))
    {
        city_update = 0;
        if (i+1 >= is->NF || is->fields[i + 1] == NULL)
        {
            i++;
            continue;
        }
        else
        {
            memset(temp_location, 0, 1024);
            sprintf(temp_location, "%s%s", "zip=", is->fields[i + 1]);
            zip = (char *) strdup(temp_location);
            zip_found = 1;
        }
    }
    else if ((strcmp(is->fields[i], "airport=") == 0) && 
             (airport_found == 0))
    {
        city_update = 0;
        if (i+1 >= is->NF || is->fields[i + 1] == NULL) continue;
        memset(temp_location, 0, 1024);
        sprintf(temp_location, "%s%s", "airport=", is->fields[i + 1]);
        i++;

        airport = (char *) strdup(temp_location);
        airport_found = 1;
        city_update = 0;
    }
    else if (((strcmp(is->fields[i], "state=") == 0) || 
              (strcmp(is->fields[i], "st=") == 0)) && 
              (state_found == 0))
    {
        city_update = 0;
        if (i+1 >= is->NF || is->fields[i + 1] == NULL)
        {
            i++;
            continue;
        }
        memset(temp_location, 0, 1024);
        sprintf(temp_location, "%s%s", "st=", is->fields[i + 1]);

        state = (char *) strdup(temp_location);
        state_found = 1;
    }
    else if (((strcmp(is->fields[i], "country=") == 0) ||
              (strcmp(is->fields[i], "c=") == 0)) && 
              (country_found == 0))
    {
        city_update = 0;
        if (i+1 >= is->NF || is->fields[i + 1] == NULL)
        {
            i++;
            continue;
        }
        memset(temp_location, 0, 1024);
        sprintf(temp_location, "%s%s", "c=", is->fields[i + 1]);

        country = (char *) strdup(temp_location);
        country_found = 1;
    }
    else if ((strcmp(is->fields[i], "city=") == 0) && (city_found == 0))
    { 
        if (i+1 >= is->NF || is->fields[i + 1] == NULL)
        {
            i++;
            continue;
        }
        city_update = 1;
        memset(temp_location, 0, 1024);
        if (strlen(is->fields[i + 1]) < 230) {
            sprintf(temp_location, "%s%s", "l=", is->fields[i + 1]);
            i++;

            city = (char *) strdup(temp_location);
            city_found = 1;
            length = strlen(city);
        }
    }
    else {
        if (city_update == 1) {
            if (city_found == 1) {
                memset(temp_location, 0, 1024);
                if ((length + (strlen(is->fields[i]))) < 1023) {
                    strncpy(temp_location, city, strlen(city));
                    strcat(temp_location, " ");
                    strcat(temp_location, is->fields[i]);
                    if (city != NULL) free(city);
                    city = (char *) strdup(temp_location);
                }
            }
        }
    }
  }

  fprintf(stdout, "Location information provided is:\n\n");
  if (zip != NULL) fprintf(stdout, "  %s\n", zip);
  if (airport != NULL) fprintf(stdout, "  %s\n", airport);
  if (country != NULL) fprintf(stdout,"  %s\n", country);
  if (state != NULL) fprintf(stdout, "  %s\n", state);
  if (city != NULL) fprintf(stdout, "  %s\n", city);

  jettison_string_inputstruct(is);
  return get_latlon(server, lat1, lon1, zip, airport, city, state, country);
}



int get_latlon(ServerConfig server, double *lat, double *lon, char *zip, char *airport, char * city, char *state, char *country) {

  LDAP          *ldap;
  LDAPMessage   *result = NULL, *entry;
  char          *filter = NULL, **vals;
  char          temp[256];
  int           i, zipcode, retval;

  ldap = ldap_init (server->ldaphost, LDAP_PORT);
  retval = ldap_simple_bind_s (ldap, LBONE_LDAP, LBONE_PASS);

  if (zip != NULL) {
    if (strlen(zip) == 9) {                    /* make sure the zip is 5 digits */
      memset(temp, 0, 256);
      memcpy(temp, zip + 4, 5);
      zipcode = atoi(temp);
      if ((zipcode > 1000) && (zipcode <= 99999)) {
        while (zipcode > 1000) 
        {
            retval = ldap_search_s(ldap, "ou=zipcodes,o=lbone", LDAP_SCOPE_SUBTREE, zip, 
                                 NULL, 0, &result);
          if ( retval != LDAP_SUCCESS )
          {
              fprintf(stderr, "ldap_search_s failed\n");
              return  0;
          }
          if ((entry = ldap_first_entry (ldap, result)) != NULL) {
            vals = ldap_get_values(ldap, entry, "lat");
            *lat = (double) strtod(vals[0], NULL);
	    ldap_value_free(vals);
            vals = ldap_get_values(ldap, entry, "lon");
            *lon = (double) strtod(vals[0], NULL);
	    ldap_value_free(vals);
            ldap_unbind(ldap);
            fprintf(stdout,"  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", zip, *lat, *lon);
            if ( result != NULL ) { ldap_msgfree(result); result = NULL; }
	    if (zip != NULL) free(zip);
	    if (airport != NULL) free(airport);
	    if (city != NULL) free(city);
	    if (state != NULL) free(state);
	    if (country != NULL) free(country);
            return 1;
          }
          fprintf(stdout, "  Location '%s' not found\n", zip);
          zipcode--;
          if (zip != NULL) free(zip);
          if (zipcode >= 10000) {
            sprintf(temp, "zip=%d", zipcode);
          } else {
            sprintf(temp, "zip=0%d", zipcode);
          }
          zip = (char *) strdup(temp);

          /* free result from ldap_search_s() */
          if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
        }
      }
    }
  }
  if (airport != NULL) {
    if (strlen(airport) == 11) {               /* make sure it is the correct length */
      ldap_search_s(ldap, "ou=airports,o=lbone", LDAP_SCOPE_SUBTREE, airport, NULL, 0, &result);
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "lat");
        *lat = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        ldap_unbind(ldap);
        fprintf(stdout, "  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", airport, *lat, *lon);
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	if (zip != NULL) free(zip);
	if (airport != NULL) free(airport);
	if (city != NULL) free(city);
	if (state != NULL) free(state);
	if (country != NULL) free(country);
        return 1;
      }
      fprintf(stdout, "  Location '%s' not found\n", airport);
    }
  }
  if ((city != NULL) && (state != NULL)) {
    if (strlen(state) == 5) {                /* make sure it is the correct length */
      memset(temp, 0, 256);
      sprintf(temp, "%s%s%s%s%s", "(&(", state, ")(", city, "))");
      for (i = 0; i < 3; i++) 
      {
	if (filter != NULL) free(filter);
        filter = (char *) strdup(temp);
        ldap_search_s(ldap, "ou=zipcodes,o=lbone", LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
        if ((entry = ldap_first_entry (ldap, result)) != NULL) {
          vals = ldap_get_values(ldap, entry, "lat");
          *lat = (double) strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "lon");
          *lon = (double) strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "l");
          ldap_unbind(ldap);
          fprintf(stdout, "  Location '%s' found.\n", filter);
          fprintf(stdout, "    %s: Lat: %9.4f  Lon: %9.4f\n", vals[0], *lat, *lon);
          if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	  if (filter != NULL) { free(filter); filter = NULL; }
	  ldap_value_free(vals);
	  if (zip != NULL) free(zip);
  	  if (airport != NULL) free(airport);
	  if (city != NULL) free(city);
	  if (state != NULL) free(state);
	  if (country != NULL) free(country);
          return 1;
        }
        fprintf(stdout, "  Location '%s' not found\n", filter);
        if (i == 0) {
          temp[strlen(filter) - 2] = '\0';
          strcat(temp, "*))");
        } else if (i == 1) {
          temp[12] = '\0';
          strcat(temp, "*");
          strcat(temp, city + 2);
          strcat(temp, "*))");
        }
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
      }
      if (filter != NULL) { free(filter); filter = NULL; }
    }
  }
  if (state != NULL) {
    if (strlen(state) == 5) 
    {               /* make sure it is the correct length */
      ldap_search_s(ldap, "ou=zipcodes,o=lbone", LDAP_SCOPE_SUBTREE, state, NULL, 0, &result);
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "lat");
        *lat = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        ldap_unbind(ldap);
        fprintf(stdout, "  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", state, *lat, *lon);
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	if (zip != NULL) free(zip);
  	if (airport != NULL) free(airport);
	if (city != NULL) free(city);
	if (state != NULL) free(state);
	if (country != NULL) free(country);
        return 1;
      }
      fprintf(stdout, "  Location '%s' not found\n", state);
      if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
    }
  }
  if ((city != NULL) && (country != NULL)) {
    if (strlen(country) == 4) {                /* make sure it is the correct length */
      memset(temp, 0, 256);
      sprintf(temp, "%s%s%s%s%s", "(&(", country, ")(", city, "))");
      for (i = 0; i < 3; i++) 
      {
	if (filter != NULL) free(filter);
        filter = (char *) strdup(temp);
        ldap_search_s(ldap, "ou=airports,o=lbone", LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
        if ((entry = ldap_first_entry (ldap, result)) != NULL) {
          vals = ldap_get_values(ldap, entry, "lat");
          *lat = (double) strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "lon");
          *lon = (double) strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "l");
          ldap_unbind(ldap);
          fprintf(stdout, "  Location '%s' found.\n", filter);
          fprintf(stdout, "    %s: Lat: %9.4f  Lon: %9.4f\n", vals[0], *lat, *lon);
          if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	  if (filter != NULL) free(filter);
	  ldap_value_free(vals);
	  if (zip != NULL) free(zip);
  	  if (airport != NULL) free(airport);
	  if (city != NULL) free(city);
	  if (state != NULL) free(state);
	  if (country != NULL) free(country);
          return 1;
        }
        fprintf(stdout, "  Location '%s' not found in ou=airports\n", filter);
        if (i == 0) {
          temp[strlen(filter) - 2] = '\0';
          strcat(temp, "*))");
        } else if (i == 1) {
          temp[11] = '\0';
          strcat(temp, "*");
          strcat(temp, city + 2);
          strcat(temp, "*))");
        }
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
      }
      memset(temp, 0, 256);
      sprintf(temp, "%s%s%s%s%s", "(&(", country, ")(", city, "))");
      for (i = 0; i < 3; i++) 
      {
	if (filter != NULL) free(filter);
        filter = (char *) strdup(temp);
        ldap_search_s(ldap, "ou=locations,o=lbone", LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
        if ((entry = ldap_first_entry (ldap, result)) != NULL) {
          vals = ldap_get_values(ldap, entry, "lat");
          *lat = (double) strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "lon");
          *lon = (double) strtod(vals[0], NULL);
	  ldap_value_free(vals);
          vals = ldap_get_values(ldap, entry, "l");
          ldap_unbind(ldap);
          fprintf(stdout, "  Location '%s' found.\n", filter);
          fprintf(stdout, "    %s: Lat: %9.4f  Lon: %9.4f\n", vals[0], *lat, *lon);
          if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	  if (filter != NULL) free(filter);
	  ldap_value_free(vals);
	  if (zip != NULL) free(zip);
  	  if (airport != NULL) free(airport);
	  if (city != NULL) free(city);
	  if (state != NULL) free(state);
	  if (country != NULL) free(country);
          return 1;
        }
        fprintf(stdout, "  Location '%s' not found in ou=locations\n", filter);
        if (i == 0) {
          temp[strlen(filter) - 2] = '\0';
          strcat(temp, "*))");
        } else if (i == 1) {
          temp[11] = '\0';
          strcat(temp, "*");
          strcat(temp, city + 2);
          strcat(temp, "*))");
        }
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
      }
      if (filter != NULL) { free(filter); filter = NULL; }
    }
  }
  if (country != NULL) {
    if (strlen(country) == 4) 
    {               /* make sure it is the correct length */
      ldap_search_s(ldap, "ou=airports,o=lbone", LDAP_SCOPE_SUBTREE, country, NULL, 0, &result);
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "lat");
        *lat = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        ldap_unbind(ldap);
        fprintf(stdout,"  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", country, *lat, *lon);
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	if (zip != NULL) free(zip);
  	if (airport != NULL) free(airport);
	if (city != NULL) free(city);
	if (state != NULL) free(state);
	if (country != NULL) free(country);
        return 1;
      }
      fprintf(stdout, "  Location '%s' not found in ou=airports\n", country);
      if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
      ldap_search_s(ldap, "ou=locations,o=lbone", LDAP_SCOPE_SUBTREE, country, NULL, 0, &result);
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "lat");
        *lat = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
	ldap_value_free(vals);
        ldap_unbind(ldap);
        fprintf(stdout,"  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", country, *lat, *lon);
        if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
	if (zip != NULL) free(zip);
  	if (airport != NULL) free(airport);
	if (city != NULL) free(city);
	if (state != NULL) free(state);
	if (country != NULL) free(country);
        return 1;
      }
      fprintf(stdout, "  Location '%s' not found in ou=locations\n", country);
      if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 
    }
  }

  ldap_unbind(ldap);
  return 0;
}




IS string_inputstruct() {
  IS is;
  char *file = NULL;

  is = (IS) calloc(1, sizeof(struct inputstruct));

  is->text1[MAXLEN-1] = '\0';
  is->NF = 0;
  is->line = 0;
  if (file == NULL) {
    is->name = "stdin";
    is->f = stdin;
  }
  return is;
}

void jettison_string_inputstruct(IS is)
{
	if (is != NULL) free(is);
	return;
}


extern int get_string(IS is, char *str) {
  int i;
  char lastchar;
  char *line;

  is->NF = 0;

  if (strlen(str) < 1001)
    sprintf(is->text1, "%s", str);

  is->line++;
  sprintf(is->text2, "%s", is->text1);

  line = is->text2;
  lastchar = ' ';
  for (i = 0; line[i] != '\0' && i < MAXLEN-1; i++) {
    if (isspace((int) line[i])) {
      lastchar = line[i];
      line[i] = '\0';
    } else {
      if (isspace((int) lastchar)) {
        is->fields[is->NF] = line+i;
        is->NF++;
      }
      lastchar = line[i];
    }
  }
  return is->NF;
}



void get_environment(char *location,
                     int *connection,
                     int *monitoring,
                     int *power,
                     int *backup,
                     int *firewall) {

  int   i;
  char  temp[100];
  IS    is;

  is = string_inputstruct();
  get_string(is, location);

  for (i = 0; i < is->NF; i++) {

    if (strcmp(is->fields[i], "connection=") == 0) {
      if (strcmp(is->fields[i + 1], "Cable/DSL") == 0) *connection = CABLE_DSL;
      else if (strcmp(is->fields[i + 1], "T1") == 0) *connection = T1;
      else if (strcmp(is->fields[i + 1], "T3") == 0) *connection = T3;
    }

    if (strcmp(is->fields[i], "monitoring=") == 0) {
      if (strcmp(is->fields[i + 1], "9-5") == 0) *monitoring = M9_5;
      else if (strcmp(is->fields[i + 1], "24-7") == 0) *monitoring = M24_7;
    }

    if (strcmp(is->fields[i], "power=") == 0) {
      if (strcmp(is->fields[i + 1], "Minutes") == 0) *power = MINUTES;
      else if (strcmp(is->fields[i + 1], "Hours") == 0) *power = HOURS;
      else if (strcmp(is->fields[i + 1], "Generator") == 0) *power = GENERATOR;
    }

    if (strcmp(is->fields[i], "backup=") == 0) {
      memset(temp, 0, 100);
      sprintf(temp, "%s %s", is->fields[i + 1], is->fields[i + 2]);
      if (strcmp(temp, "Daily-Single Copy") == 0) *backup = DAILY_SINGLE;
      else if (strcmp(temp, "Daily-Multiple Copies") == 0) *backup = DAILY_MULTIPLE;
    }

    if (strcmp(is->fields[i], "firewall=") == 0) {
      if (strcmp(is->fields[i + 1], "yes") == 0) *firewall = YES;
    }

  }
  jettison_string_inputstruct(is);
}


void free_depot_info(DepotInfo d)
{
    if ( d != NULL )
    {
        if (d->depotname != NULL) free(d->depotname);
        if (d->old_depotname != NULL) free(d->old_depotname);
        if (d->hostname != NULL) free(d->hostname);
        if (d->status != NULL) free(d->status);
        if (d->email1 != NULL) free(d->email1);
        if (d->email2 != NULL) free(d->email2);
        if (d->email3 != NULL) free(d->email3);
        if (d->country != NULL) free(d->country);
        if (d->state != NULL) free(d->state);
        if (d->city != NULL) free(d->city);
        if (d->zip != NULL) free(d->zip);
        if (d->airport != NULL) free(d->airport);
        if (d->connection != NULL) free(d->connection);
        if (d->monitoring != NULL) free(d->monitoring);
        if (d->power != NULL) free(d->power);
        if (d->backup != NULL) free(d->backup);
        if (d->notifyOwnerPeriod != NULL) free(d->notifyOwnerPeriod);
        if (d->firewall != NULL) free(d->firewall);
        free(d);
    }
}

DepotInfo get_depot_info(LDAP *ldap, LDAPMessage *entry) {
  char          **vals;
  DepotInfo     depot;

  depot = (DepotInfo) malloc(sizeof(struct depot_info));
  memset(depot, 0, sizeof(struct depot_info));

  vals = ldap_get_values(ldap, entry, "depotname");
  depot->depotname = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "hostname");
  depot->hostname = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "port");
  depot->port = atoi(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "StableStorage");
  depot->StableStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "AvailableStableStorage");
  depot->AvailableStableStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "VolatileStorage");
  depot->VolatileStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "AvailableVolatileStorage");
  depot->AvailableVolatileStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "Duration");
  depot->Duration = (float) (atof(vals[0]) / (24*60*60));
  if (depot->Duration < 0) depot->Duration = -1;
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "status");
  depot->status = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "lastUpdate");
/*  depot->lastUpdate = (char *) strdup(vals[0]); */
  depot->lastUpdate = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "email");
  if (ldap_count_values(vals) > 0) depot->email1 = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "c");
  if (ldap_count_values(vals) > 0) depot->country = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "st");
  if (ldap_count_values(vals) > 0) depot->state = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "l");
  if (ldap_count_values(vals) > 0) depot->city = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "zip");
  if (ldap_count_values(vals) > 0) depot->zip = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "airport");
  if (ldap_count_values(vals) > 0) depot->airport = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "lat");
  if (ldap_count_values(vals) > 0) depot->lat = (float) atof(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "lon");
  if (ldap_count_values(vals) > 0) depot->lon = (float) atof(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "connection");
  if (ldap_count_values(vals) > 0) depot->connection = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "monitoring");
  if (ldap_count_values(vals) > 0) depot->monitoring = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "power");
  if (ldap_count_values(vals) > 0) depot->power = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "backup");
  if (ldap_count_values(vals) > 0) depot->backup = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "notifyOwnerPeriod");
  if (ldap_count_values(vals) > 0) depot->notifyOwnerPeriod = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "polled");
  if (ldap_count_values(vals) > 0) depot->polled = atoi(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "responded");
  if (ldap_count_values(vals) > 0) depot->responded = atoi(vals[0]);
  ldap_value_free(vals);

  if (depot->polled != 0) depot->uptime = ((float) depot->responded / (float) depot->polled) * 100;

  vals = ldap_get_values(ldap, entry, "firewall");
  if (ldap_count_values(vals) > 0) {
    if (strcmp(vals[0], "yes") == 0) depot->firewall = (char *) strdup("yes");
    else depot->firewall = (char *) strdup("no");
  }
  ldap_value_free(vals);

  return depot;
}



int depot_matches_request(DepotInfo depot, Request *request) {
  char  depot_env[1024], temp[1024];
  int   r_connect, r_monitor, r_power, r_backup, r_firewall;
  int   d_connect, d_monitor, d_power, d_backup, d_firewall;

  if ((depot->AvailableStableStorage >= request->stableSize) &&
      (depot->AvailableVolatileStorage >= request->volatileSize) &&
      (((depot->Duration * 24*60*60) >= request->duration) || (depot->Duration == -1))) {

    if (request->location == NULL) return 1;

    r_connect = 0; r_monitor = 0; r_power = 0; r_backup = 0; r_firewall = 0;
    d_connect = 0; d_monitor = 0; d_power = 0; d_backup = 0; d_firewall = 0;

    get_environment(request->location, &r_connect, &r_monitor, &r_power, &r_backup, &r_firewall);

    if ( (r_connect == 0) && (r_monitor == 0) && (r_power == 0) && 
         (r_backup == 0) && (r_firewall == 0) ) return 1;

    memset(depot_env, 0, 1024);
    if (depot->connection != NULL) {
      sprintf(depot_env, "connection= %s ", depot->connection);
    }
    if (depot->monitoring != NULL) {
      memset(temp, 0, 1024);
      sprintf(temp, "monitoring= %s ", depot->monitoring);
      strcat(depot_env, temp);
    }
    if (depot->power != NULL) {
      memset(temp, 0, 1024);
      sprintf(temp, "power= %s ", depot->power);
      strcat(depot_env, temp);
    }
    if (depot->backup != NULL) {
      memset(temp, 0, 1024);
      sprintf(temp, "backup= %s ", depot->backup);
      strcat(depot_env, temp);
    }
    if (depot->firewall != NULL) {
      memset(temp, 0, 1024);
      sprintf(temp, "firewall= %s ", depot->firewall);
      strcat(depot_env, temp);
    }

    get_environment(depot_env, &d_connect, &d_monitor, &d_power, &d_backup, &d_firewall);
    if ((d_connect >= r_connect) && (d_monitor >= r_monitor) && (d_power >= r_power) &&
        (d_backup >= r_backup) && (d_firewall >= r_firewall)) {
      return 1;
    }
  }
  return 0;
}





int check_hostname(char *str)
{
    char        *first  = NULL;
    char        *second = NULL;
    int         offset, i, length;

    first = index(str, '.');
    if (first == NULL) return 0;  /* not proper format */

    second = rindex(str, '.');

    while (first != second)
    {
        length = strlen(str);
        offset = (int) (first - str) + 1;
        for (i = 0; i < (length - offset); i++)
        {
            str[i] = str[i + offset];
        }
        str[i] = '\0';
        first = index(str, '.');
        second = rindex(str, '.');
    }
    if ((strlen(str) - ((int) (second - str))) == 4) return 1;
    return 0;
}




JRB getProximity (ServerConfig server, char *location)
{
  JRB           list = NULL, 
                node = NULL;
  char          **depots = NULL; 
  LDAP          *ldap = NULL;
  LDAPMessage   *result = NULL, *entry = NULL;
  int           calc_dist = 0;
  double        distance = 0;
  double        lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
  pthread_t     tid;
  DepotInfo     depot = NULL;
  double        max     = 0;

  tid = pthread_self();
  calc_dist = 0;

  calc_dist = find_latlon(server, location, &lat1, &lon1);

  list = make_jrb();

  ldap = ldap_init (server->ldaphost, LDAP_PORT);
  if (ldap == NULL) {
    fprintf(stdout, "**** ldap_init() failed ****\n");
    return NULL;
    /* return error */
  }
    ldap_simple_bind_s (ldap, "cn=root,o=lbone", LBONE_PASS);
    ldap_search_s(ldap, "ou=depots,o=lbone", LDAP_SCOPE_SUBTREE, "depotname=*", NULL, 0, &result);

  if ((entry = ldap_first_entry (ldap, result)) != NULL) {
    depot = get_depot_info(ldap, entry);

    depots = (char **) malloc(sizeof(char *) * 2);
    depots[0] = (char *) strdup(depot->hostname);
    depots[1] = (char *) calloc(1, 11);

    if (calc_dist == 1) {
      lat2 = depot->lat;
      lon2 = depot->lon;
      if ((lat2 >= -90) && (lat2 <= 90) && (lon2 >= -180) && (lon2 <= 180)) {
        distance = sphere_distance(lat1, lon1, lat2, lon2);
        max = distance;
      } else {
        distance = 0;
      }
    } else {
      distance = 0;
    }
    sprintf(depots[1], "%-10f", distance);

    jrb_insert_str(list, depots[0], new_jval_v(depots));
    free_depot_info(depot);

    while ((entry = ldap_next_entry (ldap, entry)) != NULL) 
    { 
      depot = get_depot_info(ldap, entry);

      depots = (char **) malloc(sizeof(char *) * 2);
      depots[0] = (char *) strdup(depot->hostname);
      depots[1] = (char *) calloc(1, 11);   /* added more bytes to prevent sprintf*/

      if (calc_dist == 1) {
        lat2 = depot->lat;
        lon2 = depot->lon;
        if ((lat2 >= -180) && (lat2 <= 180) && (lon2 >= -180) && (lon2 <= 180)) {
          distance = sphere_distance(lat1, lon1, lat2, lon2);
          if (distance > max) max = distance;
        } else {
          distance = 0;
        }
      } else {
        distance = 0;
      }
      sprintf(depots[1], "%-10.3f", distance);

      jrb_insert_str(list, depots[0], new_jval_v(depots));
      free_depot_info(depot);
    }
  }
  ldap_unbind(ldap);
  if ( result != NULL ) { ldap_msgfree(result); result = NULL; } 

  jrb_traverse(node, list)
  {
      depots = (char **) jval_v(node->val);
      distance = atof(depots[1]);
      distance = (max / distance) / 5;
      snprintf(depots[1], 11, "%-10f", distance);
  }
  return list;
}


