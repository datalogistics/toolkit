/* $Id: connect_exp.h,v 1.11 2000/01/25 16:56:08 hayes Exp $ */

#ifndef CONNECT_EXP_H
#define CONNECT_EXP_H

#include "dnsutil.h"  /* IPAddress */
#include "messages.h" /* MessageType */
#include "protocol.h" /* Socket */


/*
** Conducts a TCP connection experiment to #machine#:#port#.  If successful
** within #timeOut# seconds, returns 1 and sets #results# to the number of
** milliseconds required to make the connection; else returns 0.
*/
int
InitiateConnectExp(IPAddress machine,
                   unsigned short port,
                   unsigned int timeOut,
                   double *results);


#endif
