/* $Id: protocol.h,v 1.61 2000/11/04 17:10:53 rich Exp $ */


#ifndef PROTOCOL_H
#define PROTOCOL_H


/*
** This module defines functions useful in establishing and maintaining
** connections to other processes on local or remote hosts.  The name is an
** anachronism.
*/


#include <sys/types.h>    /* pid_t */
#include "dnsutil.h"      /* IPAddress */


/* The Socket type refers to Unix sockets, typedef'ed for clarity. */
typedef int Socket;
#define NO_SOCKET ((Socket)-1)

/* A definition for socket call-back functions (see NotifyOnDisconnection()). */
typedef void (*SocketFunction)(Socket);


/*
** Attempts to establish a connection to the server listening to #addr#:#port#.
** If successful within #timeOut# seconds, returns 1 and sets #sock# to the
** socket bound to the connection, else returns 0.
*/
int
CallAddr(IPAddress addr, 
         short port, 
         Socket *sock,
         double timeOut);


/*
** Closes connections opened via calls to this module's functions.  Each
** parameter is a boolean value indicated whether that type of connection
** should be included in those closed.
*/
void
CloseConnections(int closeEars,
                 int closePipes,
                 int closeSockets);
#define CloseAllConnections() CloseConnections(1, 1, 1)


/*
** Tests all connections opened via calls to this module's functions and
** closes any that are no longer connected.  Returns the number of connections
** closed.
*/
int
CloseDisconnections(void);


/*
** Tears down #sock#.  If #waitForPeer# is non-zero, the function waits this
** many seconds for the host on the other end to close the connection and fails
** if no such close is detected.  If this parameter is zero, the function
** closes #sock# immediately.  Returns 1 and sets #sock# to NO_SOCKET if
** successful, else returns 0.
*/
int
CloseSocket(Socket *sock,
            int waitForPeer);
#define DROP_SOCKET(sock) CloseSocket(sock, 0)


/*
** Spawns a new process, a duplicate of the running one.  Returns 1 if
** successful, 0 otherwise.  Returns in #pid# a 0 to the child process and the
** process id of the child to the parent.  Both processes are given a pair of
** connections in the Socket parameters that can be used to communicate between
** the parent and child.  The parent process should send information to the
** child via #parentToChild# and receive information via #childToParent#; the
** child reads from the former and writes to the latter.  The parameters may be
** NULL indicating that communication is unidirectional (one parameter NULL),
** or that no communication will take place (both NULL).
*/
int
CreateLocalChild(pid_t *pid,
                 Socket *parentToChild,
                 Socket *childToParent);


/*
** Attempts to bind to any port between #startingPort# and #endingPort#,
** inclusive.  If successful, returns 1 and sets #ear# to the bound socket and
** #earPort# to the port to which it is bound, else returns 0.
*/
int
EstablishAnEar(short startingPort,
               short endingPort,
               Socket *ear,
               short *earPort);


/*
** Checks all connections established via calls to this module for incoming
** messages.  If one is detected within #timeOut# seconds, returns 1 and sets
** #sd# to the socket containing the message, else returns 0.
*/
int 
IncomingRequest(double timeOut,
                Socket *sd);


/*
** Returns 1 or 0 depending on whether or not #sd# is connected to another
** process.
*/
int
IsOkay(Socket sd);


/*
** Registers a function that should be called whenever a socket is disconnected,
** either directly via a call to CloseSocket(), or indirectly because the peer
** termintes the connection.  The function is passed the closed socket after it
** has been closed.
*/
void
NotifyOnDisconnection(SocketFunction notifyFn);


/*
** Pass socket #sock# along to child #child# -- call after a successful call
** to CreateLocalChild().  The parent process will no longer listen for
** activity on #sock#.  Closing the socket will be put off until after the
** child dies, just in case the parent and child share descriptors.
*/
int
PassSocket(Socket *sock,
           pid_t child);


/*
** Returns the DNS name of the host connected to #sd#, or descriptive text if
** #sd# is not an inter-host connection.  The value returned is volatile; it
** will be overwritten by subsequent calls.
*/
const char *
PeerName(Socket sd);


/*
** Receives #byteSize# bytes from connection #sd# and stores them in the
** memory referenced by #bytes#.  The caller must assure that #bytes# is at
** least #byteSize# bytes long.  Returns 1 if successful within #timeOut#
** seconds, else 0.
*/
int
RecvBytes(Socket sd,
          void *byte,
          size_t byteSize,
          double timeOut);


/*
** Sends #bytesSize# bytes from the memory pointed to by #bytes# on connection
** #sd#.  Returns 1 if successful within #timeOut# seconds, else 0.
*/
int
SendBytes(Socket sd,
          const void *bytes,
          size_t byteSize,
          double timeOut);


/*
** A signal handler for dealing with signals (specifically SIGPIPE) that
** indicate a bad socket.
*/
void
SocketFailure(int sig);


#define MINPKTTIMEOUT 15.0
#define MAXPKTTIMEOUT 60.0
#define PKTTIMEOUT 15.0

#define MINCONNTIMEOUT 5.0
#define MAXCONNTIMEOUT 60.0
#define CONNTIMEOUT MAXCONNTIMEOUT


double
ConnTimeOut(IPAddress addr);


double
PktTimeOut(Socket sd);

void SetPktTimeOut(Socket sd, double time_out, int timedOut);

#endif
