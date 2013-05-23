#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <ctype.h>
#include <lber.h>
#include <ldap.h>
#include <sys/time.h>
#include "jrb.h"
#include "lbone_base.h"
#include "lbone_server.h"
#include "lbone_search.h"

extern int find_latlon();
extern int get_latlon();
extern IS string_inputstruct();
extern int get_string();
extern char **getLatLong ();
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
  JRB		list, node, temp_node;
  char		**vals, **depots; 
  LDAP		*ldap;
  LDAPMessage	*result, *entry;
  int		calc_dist;
  char		*hostname;
  char		*port;
  long		stable;
  long		vol;
  int		duration;
  double	distance;
  int		count;
  double	lat1, lon1, lat2, lon2;
  struct timeval	tv;
  pthread_t	tid;

  tid = pthread_self();
  calc_dist = 0;

  if (request->location == NULL) {
    pthread_mutex_lock(&server->lock);
    printf("Client %ld: no location specified. Returning random locations.\n", tid);
    pthread_mutex_unlock(&server->lock);
    gettimeofday(&tv, NULL);
    srand(tv.tv_sec);
  }
  else {
    calc_dist = find_latlon(server, request->location, &lat1, &lon1);
  }

  list = make_jrb();

  ldap = ldap_init (server->ldaphost, LDAP_PORT);
if (ldap == NULL) printf("**** ldap_init() failed ****\n");
  ldap_simple_bind_s (ldap, "cn=root,o=lbone", LBONE_PASS);
  ldap_search_s(ldap, "ou=depots,o=lbone", LDAP_SCOPE_SUBTREE, "depotname=*", NULL, 0, &result);

  if ((entry = ldap_first_entry (ldap, result)) != NULL) {
    vals = ldap_get_values(ldap, entry, "hostname");
    hostname = (char *) strdup(vals[0]);
    vals = ldap_get_values(ldap, entry, "port");
    port = (char *) strdup(vals[0]);
    vals = ldap_get_values(ldap, entry, "AvailableStableStorage");
    stable = strtoul(vals[0], NULL, 0);
    vals = ldap_get_values(ldap, entry, "AvailableVolatileStorage");
    vol = strtoul(vals[0], NULL, 0);
    vals = ldap_get_values(ldap, entry, "Duration");
    duration = strtol(vals[0], NULL, 0);

    if ((stable >= request->stableSize) &&
        (vol >= request->volatileSize) &&
        ((duration >= request->duration) || (duration == -1))) {

      depots = (char **) malloc(sizeof(char *) * 2);
      depots[0] = (char *) strdup(hostname);
      depots[1] = (char *) strdup(port);

      free(hostname);
      free(port);

      if (calc_dist == 1) {
        vals = ldap_get_values(ldap, entry, "lat");
        lat2 = strtod(vals[0], NULL);
        vals = ldap_get_values(ldap, entry, "lon");
        lon2 = strtod(vals[0], NULL);
        if ((lat2 > -181) && (lat2 < 181) && (lon2 > -181) && (lon2 < 181)) {
          distance = sphere_distance(lat1, lon1, lat2, lon2);
        } else {
          distance = rand();
        }
      } else {
        distance = rand();
      }

      jrb_insert_dbl(list, distance, new_jval_v(depots));
    }

    while ((entry = ldap_next_entry (ldap, entry)) != NULL) { 
      vals = ldap_get_values(ldap, entry, "hostname");
      hostname = (char *) strdup(vals[0]);
      vals = ldap_get_values(ldap, entry, "port");
      port = (char *) strdup(vals[0]);
      vals = ldap_get_values(ldap, entry, "AvailableStableStorage");
      stable = strtoul(vals[0], NULL, 0);
      vals = ldap_get_values(ldap, entry, "AvailableVolatileStorage");
      vol = strtoul(vals[0], NULL, 0);
      vals = ldap_get_values(ldap, entry, "Duration");
      duration = strtol(vals[0], NULL, 0);

      if ((stable >= request->stableSize) &&
          (vol >= request->volatileSize) &&
          ((duration >= request->duration) || (duration == -1))) {

        depots = (char **) malloc(sizeof(char *) * 2);
        depots[0] = (char *) strdup(hostname);
        depots[1] = (char *) strdup(port);

        free(hostname);
        free(port);

        if (calc_dist == 1) {
          vals = ldap_get_values(ldap, entry, "lat");
          lat2 = strtod(vals[0], NULL);
          vals = ldap_get_values(ldap, entry, "lon");
          lon2 = strtod(vals[0], NULL);
          if ((lat2 > -181) && (lat2 < 181) && (lon2 > -181) && (lon2 < 181)) {
            distance = sphere_distance(lat1, lon1, lat2, lon2);
          } else {
            distance = rand();
          }
        } else {
          distance = rand();;
        }

        jrb_insert_dbl(list, distance, new_jval_v(depots));
      }
    }
  }
  ldap_unbind(ldap);

  count = 0;

  jrb_traverse(node, list) {
    count++;
    if (count > request->numDepots) {
      temp_node = node;
      node->flink->blink = node->blink;
      node->blink->flink = node->flink;
      node = node->blink;
      depots = (char **) jval_v(temp_node->val);
      free(depots[0]);
      free(depots[1]);
      free(temp_node);
    }
  }
  return list;
}






int find_latlon(ServerConfig server, char *location, int *lat1, int *lon1) {
  char	temp_location[256];
  char	*host, *zip, *city, *state, *airport, *country, **hostlatlon;
  char  **args;
  int	i, count, retval, host_found;
  int	zip_found, airport_found, city_found, state_found, country_found;
  int	city_update, length;
  IS	is;
  double	latlon[2], lat, lon;

  i = 0; count = 0;
  zip_found = 0; airport_found = 0; city_found = 0; state_found = 0;
  country_found = 0; city_update = 0; retval = 0;
  zip = NULL; city = NULL; state = NULL; airport = NULL; country = NULL;
  latlon[0] = 0; latlon[1] = 0;

  is = string_inputstruct();

  get_string(is, location);

  for (i = 0; i < is->NF; i++) printf("token[%d] = %s\n", i, is->fields[i]);

  for (i = 0; i < is->NF; i++) {

    if ((strcmp(is->fields[i], "hostname=") == 0) && (host_found == 0)) {
      city_update = 0;
      args = (char **) malloc(sizeof(char *) * 2);
      if (is->fields[i + 1] == NULL) continue;
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
              return 1;
            }
          }
        }
      }
    }


    else if (((strcmp(is->fields[i], "zip=") == 0)
      || (strcmp(is->fields[i], "zipcode=") == 0)) && (zip_found == 0)) {
      city_update = 0;
      if (is->fields[i + 1] == NULL) continue;
      memset(temp_location, 0, 256);
      sprintf(temp_location, "%s%s", "zip=", is->fields[i + 1]);
      i++;
      
      zip = (char *) strdup(temp_location);
      zip_found = 1;
    }
    else if ((strcmp(is->fields[i], "airport=") == 0) && (airport_found == 0)) {
      city_update = 0;
      if (is->fields[i + 1] == NULL) continue;
      memset(temp_location, 0, 256);
      sprintf(temp_location, "%s%s", "airport=", is->fields[i + 1]);
      i++;

      airport = (char *) strdup(temp_location);
      airport_found = 1;
    }
    else if (((strcmp(is->fields[i], "state=") == 0) || 
              (strcmp(is->fields[i], "st=") == 0)) && (state_found == 0)) {
      city_update = 0;
      if (is->fields[i + 1] == NULL) continue;
      memset(temp_location, 0, 256);
      sprintf(temp_location, "%s%s", "st=", is->fields[i + 1]);
      i++;

      state = (char *) strdup(temp_location);
      state_found = 1;
    }
    else if (((strcmp(is->fields[i], "country=") == 0) ||
              (strcmp(is->fields[i], "c=") == 0)) && (country_found == 0)) {
      city_update = 0;
      if (is->fields[i + 1] == NULL) continue;
      memset(temp_location, 0, 256);
      sprintf(temp_location, "%s%s", "c=", is->fields[i + 1]);
      i++;

      country = (char *) strdup(temp_location);
      country_found = 1;
    }
    else if ((strcmp(is->fields[i], "city=") == 0) && (city_found == 0)) { 
      city_update = 1;
      if (is->fields[i + 1] == NULL) continue;
      memset(temp_location, 0, 256);
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
          memset(temp_location, 0, 256);
          if ((length + (strlen(is->fields[i]))) < 230) {
            strncpy(temp_location, city, strlen(city));
            strcat(temp_location, " ");
            strcat(temp_location, is->fields[i]);
            free(city);
            city = (char *) strdup(temp_location);
          }
        }
      }
    }
  }

  printf("Location information provided is:\n\n");
  if (zip != NULL) printf("  %s\n", zip);
  if (airport != NULL) printf("  %s\n", airport);
  if (country != NULL) printf("  %s\n", country);
  if (state != NULL) printf("  %s\n", state);
  if (city != NULL) printf("  %s\n", city);

  return get_latlon(server, lat1, lon1, zip, airport, city, state, country);
}



int get_latlon(ServerConfig server, double *lat, double *lon, char *zip, char *airport, char * city, char *state, char *country) {

  LDAP		*ldap;
  LDAPMessage	*result, *entry;
  char		*filter, **vals;
  char		temp[256];
  int		i, zipcode, retval;

  ldap = ldap_init (server->ldaphost, LDAP_PORT);
  retval = ldap_simple_bind_s (ldap, LBONE_LDAP, LBONE_PASS);

  if (zip != NULL) {
    if (strlen(zip) == 9) {                    /* make sure the zip is 5 digits */
      memset(temp, 0, 256);
      memcpy(temp, zip + 4, 5);
      zipcode = atoi(temp);
      if ((zipcode > 1000) && (zipcode <= 99999)) {
        while (zipcode > 1000) {
          ldap_search_s(ldap, "ou=zipcodes,o=lbone", LDAP_SCOPE_SUBTREE, zip, NULL, 0, &result);
          if ((entry = ldap_first_entry (ldap, result)) != NULL) {
            vals = ldap_get_values(ldap, entry, "lat");
            *lat = (double) strtod(vals[0], NULL);
            vals = ldap_get_values(ldap, entry, "lon");
            *lon = (double) strtod(vals[0], NULL);
            ldap_unbind(ldap);
            fprintf(stderr, "  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", zip, *lat, *lon);
            return 1;
          }
          fprintf(stderr, "  Location '%s' not found\n", zip);
          zipcode--;
          free(zip);
          if (zipcode >= 10000) {
            sprintf(temp, "zip=%d", zipcode);
          } else {
            sprintf(temp, "zip=0%d", zipcode);
          }
          zip = (char *) strdup(temp);
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
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
        ldap_unbind(ldap);
        fprintf(stderr, "  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", airport, *lat, *lon);
        return 1;
      }
      fprintf(stderr, "  Location '%s' not found\n", airport);
    }
  }
  if ((city != NULL) && (state != NULL)) {
    if (strlen(state) == 5) {                /* make sure it is the correct length */
      memset(temp, 0, 256);
      sprintf(temp, "%s%s%s%s%s", "(&(", state, ")(", city, "))");
      for (i = 0; i < 3; i++) {
        filter = (char *) strdup(temp);
        ldap_search_s(ldap, "ou=zipcodes,o=lbone", LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
        if ((entry = ldap_first_entry (ldap, result)) != NULL) {
          vals = ldap_get_values(ldap, entry, "lat");
          *lat = (double) strtod(vals[0], NULL);
          vals = ldap_get_values(ldap, entry, "lon");
          *lon = (double) strtod(vals[0], NULL);
          vals = ldap_get_values(ldap, entry, "l");
          ldap_unbind(ldap);
          fprintf(stderr, "  Location '%s' found.\n", filter);
          fprintf(stderr, "    %s: Lat: %9.4f  Lon: %9.4f\n", vals[0], *lat, *lon);
          return 1;
        }
        fprintf(stderr, "  Location '%s' not found\n", filter);
        if (i == 0) {
          temp[strlen(filter) - 2] = '\0';
          strcat(temp, "*))");
        } else if (i == 1) {
          temp[12] = '\0';
          strcat(temp, "*");
          strcat(temp, city + 2);
          strcat(temp, "*))");
        }
      }
    }
  }
  if (state != NULL) {
    if (strlen(state) == 5) {               /* make sure it is the correct length */
      ldap_search_s(ldap, "ou=zipcodes,o=lbone", LDAP_SCOPE_SUBTREE, state, NULL, 0, &result);
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "lat");
        *lat = (double) strtod(vals[0], NULL);
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
        ldap_unbind(ldap);
        fprintf(stderr, "  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", state, *lat, *lon);
        return 1;
      }
      fprintf(stderr, "  Location '%s' not found\n", state);
    }
  }
  if ((city != NULL) && (country != NULL)) {
    if (strlen(country) == 4) {                /* make sure it is the correct length */
      memset(temp, 0, 256);
      sprintf(temp, "%s%s%s%s%s", "(&(", country, ")(", city, "))");
      for (i = 0; i < 3; i++) {
        filter = (char *) strdup(temp);
        ldap_search_s(ldap, "ou=airports,o=lbone", LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
        if ((entry = ldap_first_entry (ldap, result)) != NULL) {
          vals = ldap_get_values(ldap, entry, "lat");
          *lat = (double) strtod(vals[0], NULL);
          vals = ldap_get_values(ldap, entry, "lon");
          *lon = (double) strtod(vals[0], NULL);
          vals = ldap_get_values(ldap, entry, "l");
          ldap_unbind(ldap);
          fprintf(stderr, "  Location '%s' found.\n", filter);
          fprintf(stderr, "    %s: Lat: %9.4f  Lon: %9.4f\n", vals[0], *lat, *lon);
          return 1;
        }
        fprintf(stderr, "  Location '%s' not found\n", filter);
        if (i == 0) {
          temp[strlen(filter) - 2] = '\0';
          strcat(temp, "*))");
        } else if (i == 1) {
          temp[11] = '\0';
          strcat(temp, "*");
          strcat(temp, city + 2);
          strcat(temp, "*))");
        }
      }
    }
  }
  if (country != NULL) {
    if (strlen(country) == 4) {               /* make sure it is the correct length */
      ldap_search_s(ldap, "ou=airports,o=lbone", LDAP_SCOPE_SUBTREE, country, NULL, 0, &result);
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "lat");
        *lat = (double) strtod(vals[0], NULL);
        vals = ldap_get_values(ldap, entry, "lon");
        *lon = (double) strtod(vals[0], NULL);
        ldap_unbind(ldap);
        fprintf(stderr, "  Location '%s' found. Lat: %9.4f  Lon: %9.4f\n", country, *lat, *lon);
        return 1;
      }
      fprintf(stderr, "  Location '%s' not found\n", country);
    }
  }

  ldap_unbind(ldap);
  return 0;
}




IS string_inputstruct() {
  IS is;
  char *file = NULL;

  is = (IS) malloc(sizeof(struct inputstruct));

  is->text1[MAXLEN-1] = '\0';
  is->NF = 0;
  is->line = 0;
  if (file == NULL) {
    is->name = "stdin";
    is->f = stdin;
  }
  return is;
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


