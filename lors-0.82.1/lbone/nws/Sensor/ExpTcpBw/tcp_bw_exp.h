/* $Id: tcp_bw_exp.h,v 1.19 2000/03/03 01:19:31 hayes Exp $ */

#ifndef TCP_BW_EXP_H
#define TCP_BW_EXP_H

#include "dnsutil.h"   /* IPAddress */
#include "hosts.h"     /* host_desc */
#include "messages.h"  /* DataDescriptor, MessageType */
#include "protocol.h"  /* Socket */


typedef struct {
  unsigned int expSize;
  unsigned int bufferSize;
  unsigned int msgSize;
} TcpBwCtrl;
static const DataDescriptor tcpBwCtrlDescriptor[] =
  {SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(TcpBwCtrl, expSize)),
   SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(TcpBwCtrl, bufferSize)),
   SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(TcpBwCtrl, msgSize))};
#define tcpBwCtrlDescriptorLength 3

/*
** Contacts the server listening to #machine#:#port#, sends #requestMessage#,
** and conducts a TCP latency and bandwidth experiments.  If successful within
** #timeOut# seconds, returns 1 and sets results[0] and results[1] to the
** obvserved bandwidth (megabits/second) and latency (milliseconds);
** otherwise, returns 0.
*/
int
InitiateTcpExp(IPAddress machine,
               unsigned short port,
               MessageType requestMessage,
               const TcpBwCtrl *sizes,
               double timeOut,
               double *results);


/*
** Handles a request for a TCP experiment received on #sender#.  Returns 1 if
** successful, 0 otherwise.
*/
int
TerminateTcpExp(Socket sender);


#endif
