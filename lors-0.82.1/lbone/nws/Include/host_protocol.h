/* $Id: host_protocol.h,v 1.20 2000/02/25 23:02:26 hayes Exp $ */

#ifndef HOST_PROTOCOL_H
#define HOST_PROTOCOL_H


#include "register.h" /* Object */
#include "hosts.h"    /* host_cookie host_desc */


/*
** Establishes a connection to the host described in #host_c# and places a
** copy of the cookie socket in #sd#.  Returns 1 if successful, else 0.
*/
int
ConnectToHost(struct host_cookie *host_c,
              Socket *sd);


/*
** Returns a default port number for hosts of type #hostType#.
*/
unsigned short
DefaultHostPort(HostTypes hostType);


/*
** Tears down the connection contained in #host_c#.
*/
void
DisconnectHost(struct host_cookie *host_c);


/*
** Takes several steps to establish an NWS host:
**   1) Attaches port #port# in order to listen for messages.
**   2) Directs error and log output to #errorFile# and #logFile#.  If
**      #errorFile# is NULL or blank, error messages are directed to stderr;
**      if #logFile# is NULL or blank, log messages are directed to stdout.
**   3) Sets up a listening function to handle host messages.  This handler
**      will call #exitFunction# when a HOST_DIE message accompanied by a
**      string that matches #password# is received.  If #exitFunction# is NULL
**      or returns 1 when called, the handler will terminate via exit(2).  The
**      handler uses a cached copy of #nameServer# to service HOST_NSASK and
**      HOST_NSTELL messages, and to process calls to HostRegister() and
**      HostHealthy().  Uses #registration#, #hostType#, and the #addressCount#-
**      long array #addresses# to register the host with the name server.
** NOTE: #addresses# could also be used to select interfaces of interest.  This
**       is not presently implemented; hosts will receive messages that arrive
**       on any interface.
*/
int
EstablishHost(const char *registration,
              HostTypes hostType,
              IPAddress *addresses,
              unsigned int addressesCount,
              unsigned short port,
              const char *errorFile,
              const char *logFile,
              const char *password,
              struct host_desc *nameServer,
              int (*exitFunction)(void));


/*
** Returns 1 or 0 depending on whether or not this host is receiving messages
** sent to #address#:#port#.
*/
int
EstablishedInterface(IPAddress address,
                     unsigned short port);


/*
** Returns the registration name previously passed to EstablishHost().
*/
const char *
EstablishedRegistration(void);


/*
** Functions which translate between host structures and printable strings.
** The values returned by HostCImage() and HostDImage() are volatile; subsequent
** calls to the function will overwrite any prior value.  HostDValue() uses
** #defaultPort# if no port is specified as part of #image#.
*/
const char *
HostCImage(const struct host_cookie *host_c);
const char *
HostDImage(const struct host_desc *host_d);
void
HostDValue(const char *image,
           unsigned short defaultPort,
           struct host_desc *host_d);


/*
** Returns 1 or 0 depending on whether or not the information stored with the
** name server for this host is current.
*/
int
HostHealthy(void);


/*
** A convenience routine to set all the fields of #host_c#.
*/
void
MakeHostCookie(const char *host_name,
               short host_port,
               struct host_cookie *host_c);


/*
** Returns a default registration name for host #host_d#.  Hosts may register
** using any name they wish, but using this function to generate a name will
** make retrieval easier.
*/
const char *
NameOfHost(const struct host_desc *host_d);


/*
** Re-registers all host objects, good for the next #timeOut# seconds.
*/
void
RegisterHost(double timeOut);


/*
** Immediately registers #object# with the name server and adds it to the set
** of objects to be re-registered during a beat.
*/
void
RegisterObject(const Object object);


/*
** Returns 1 or 0 depending on whether or not #left# and #right# refer to the
** same host.
*/
int
SameHost(const struct host_desc *left,
         const struct host_desc *right);


/*
** Removes the object with a name attribute #objectName# from the set of
** objects to be re-registered during a beat.
*/
void
UnregisterObject(const char *objectName);


#endif
