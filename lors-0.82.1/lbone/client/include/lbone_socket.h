/*              LBONE version 1.1:  Logistical Backbone
 *               University of Tennessee, Knoxville TN.
 *                Authors: A. Bassi, M. Beck, Y. Zheng
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
 * lbone_socket.h
 *
 * in case of bugs, problems, suggestions please contact: lbone@cs.utk.edu
 *
 */

#ifndef _LBONE_SOCKET_H
#define _LBONE_SOCKET_H

#include "lbone_base.h"

# define YES	1
# define NO	0 
#define LBONE_CLIENTS_WAITING   50  /* listen() queue size - the OS may reduce this */

extern char *getlocalhostname ();
extern char *getfullhostname (char *name);
extern int serve_known_socket(char *hostName, int port);
extern int SocketConnect (char *host, int port);
extern int accept_connection(int s);

#endif
