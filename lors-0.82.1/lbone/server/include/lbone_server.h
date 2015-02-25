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
 * lbone_server.h
 */

#ifndef _LBONESERVER_H
#define _LBONESERVER_H

#include <time.h>
#include "lbone_base.h"
#include "dllist.h"
#include "jval.h"
#include "jrb.h"
#include "lbone_error.h"
#include "lbone_socket.h"


#define	LBONE_CFG_FILE	"lbone.cfg"
#define	DEFAULT_CFG	"/etc/lbone.cfg"
#define	DEFAULT_PORT	6767
#define	LDAP_DEFAULT	"adder"
#define LBONE_PASS      "lbone"
#define LBONE_LDAP      "cn=root,o=lbone"
#define LBONE_TPLT      "cn=root,o=lbone"

#define	ISDN_56K	1
#define	CABLE_DSL	2
#define	T1		3
#define	T3		4

#define	OCCASIONAL	2
#define	M9_5		3
#define	M24_7		4	

#define	NONE		1
#define	MINUTES		2
#define	HOURS		3
#define	GENERATOR	4

#define	DAILY_SINGLE	3
#define	DAILY_MULTIPLE	4

#define	NO		0
#define	YES		1	

#define LBONE_RES_NWS       0x01
#define LBONE_RES_GEO       0x02

#define LBONE_MAX_PORT  65535
#define MAX_FILE_NAME  256

/*
 * This struct holds command parameters for passing between functions.
 * This avoids needing globals.
 */

typedef struct server_config {
  char          *password;
  char          *config_path;
  int            port;
  char          *ldaphost;
  int            socket;
  ulong_t	 totalDepots;
  ulong_t	 liveDepots;
  ulong_t	 stableListedMb;
  ulong_t	 stableAvailMb;
  ulong_t	 volListedMb;
  ulong_t	 volAvailMb;
  pthread_mutex_t lock;
  pthread_mutex_t host_lock;
  int            poll;
} *ServerConfig;


typedef struct client_info {
  ServerConfig  server;
  int           fd;
  pthread_t     tid;
} *ClientInfo;



# endif
