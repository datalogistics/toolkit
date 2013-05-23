/*
 *              LBONE version 1.0:  Logistical Backbone
 *               University of Tennessee, Knoxville TN.
 *        Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 1999 All Rights Reserved
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
 * lbone_search.h
 */

#ifndef _LBONESEARCH_H
#define _LBONESEARCH_H

#include <stdlib.h>
#include <string.h>
#include "lbone_base.h"
#include "dllist.h"
#include "jval.h"
#include "jrb.h"
#include "lbone_error.h"
#include "lbone_socket.h"

#include "lber.h"
#include "ldap.h"

#define LBONE_TEMP              "o=lbone"
#define LBONE_TREE              "o=lbone"

#define CONTINENTDN             "continent=continent,o=lbone"
#define CONTINENT               "continent"
#define COUNTRY                 "country"
#define MAINCITYDN              "maincity=maincity,o=lbone"
#define MAINCITY                "maincity"
#define COUNTRYZIP              "country_zipcode/city"
#define ZIPDEPOT                "zipcode/city_depot"

#define LOCATIONSDN             "geographical_locations=lat_lon,o=lbone"

#define WILDCARD                "*"
#define UPDATE_COUNT            5
#define STORAGEUNIT             1048576
#define SECSperDAY              86400

#define DEPOTIDLE               "idle"
#define DEPOTUPDATE             "changing"
#define DEPOTUNAVAIL            "unavailable"

#define TEMPLATELOST            -1
#define DEPOTLOST               -2
#define BOTHLOST                -3
#define DEPOTCHANGING           -4

extern int derrno;

typedef struct depot_info {
  char		*depotname;
  char		*old_depotname;
  char		*hostname;
  int		port;
  ulong_t	StableStorage;
  ulong_t	AvailableStableStorage;
  ulong_t	VolatileStorage;
  ulong_t	AvailableVolatileStorage;
  double	Duration;
  char		*status;
  char		*country;
  char		*state;
  char		*city;
  char		*zip;
  char		*airport;
  double	lat;
  double	lon;
  char		*email1;
  char		*email2;
  char		*email3;
  char		*connection;
  char		*monitoring;
  char		*power;
  char		*backup;
  int		polled;
  int		responded;
  float		uptime;
  char		*firewall;
  char		*notifyOwnerPeriod;
  ulong_t	lastUpdate;
} *DepotInfo;

/* Authenticates permissions. */
int lbone_auth (char *username, char *password, char *depot_name, int permission);

/* Searches a depot. */
LDAPMessage *search_depot (LDAP* ldap, char *type, const char *depot_name, int phase);

/* Searches a depot. */
LDAPMessage *search_attr (LDAP* ldap, char *type, const char *attr_name, int phase);

/* Checks the depot status. */
char *check_depot_status (LDAP *ldap, const char *depot_name);

/* Sets the depot status. */
void set_depot_status (LDAP *ldap, char *old_dn, char *attr, char *value);

/* Connect the depot via the Internet. */
IBP_DptInfo connect_depot (LDAP *ldap, LDAPMessage *entry, char *hostname, char *port);

/* Main functions starts here. */
void print_depot (JRB depot, int flag);

/* Depot status loading routine */
int populate_depot (LDAP *ldap, LDAPMessage *entry, JRB *depot, int flag);

/* Gets a depot status. */
int return_depot_status (const char *depot_name, char *ldap_server);

/* Main routine of getting a depot status */
void lbone_get_depot();

#endif /* _LBONESEARCH_H */

