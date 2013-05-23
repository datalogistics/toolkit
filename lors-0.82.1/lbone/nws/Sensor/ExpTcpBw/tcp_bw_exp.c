/* $Id: tcp_bw_exp.c,v 1.58 2001/02/07 18:29:09 swany Exp $ */

#include "config.h"
#include <sys/types.h>     /* system type definitions */
#include <stdlib.h>        /* free malloc  */
#include <assert.h>        
#include <string.h>        /* memset */
#include <netinet/in.h>    /* sometimes required for #include <arpa/inet.h> */
#include <sys/socket.h>    /* [gs]etsockopt */
#include <sys/time.h>      /* struct timeval */
#include <unistd.h>        /* select (some systems) */
#include "protocol.h"      /* Socket functions. */
#include "diagnostic.h"    /* FAIL WARN */
#include "osutil.h"        /* MicroTime SetRealTimer */
#include "messages.h"      /* {Recv,Send}Message() */
#include "tcp_bw_exp.h"


#define REUSE_CONNECTION 0
#define TCP_HANDSHAKE 411
typedef struct {
  IPAddress ipAddr; /* Reserved for future use. */
  unsigned short portNum;
} TcpHandshake;
static const DataDescriptor tcpHandshakeDescriptor[] =
  {SIMPLE_MEMBER(UNSIGNED_INT_TYPE, 1, offsetof(TcpHandshake, ipAddr)),
   SIMPLE_MEMBER(UNSIGNED_SHORT_TYPE, 1, offsetof(TcpHandshake, portNum))};
#define tcpHandshakeDescriptorLength 2


/*
 * Interacts with the peer connected to #peer# to perform the TCP experiment.
 * #sender# indicates whether this process is the sender (non-zero) or receiver
 * (zero) of the data.  Latency and bandwidth values are returned in #results#.
 */
static int
DoTcpExp(Socket peer,
         const TcpBwCtrl *sizes,
         int sender,
         double *results) {

  int bytesThisCall;
  int bytesThisMessage;
  int bytesTotal;
  char *expData;
  fd_set peerFDs;
  int result;
  long startTime;
  long stopTime;
  struct timeval timeOut;

  /* Allocate enough data to cover the bandwidth experiment. */
  if((expData = (char *)malloc(sizes->msgSize)) == NULL) {
    FAIL1("TerminateTcpExp: malloc %d failed\n", sizes->msgSize);
  }

#ifdef LINUX
      if(siginterrupt(SIGALRM, 1) < 0) {
	              printf("siginterrupt failed\n");
		              exit(-1);
			          }
#endif

  /* Latency experiment ... */
  SetRealTimer((int)PktTimeOut(peer));
  startTime = MicroTime();
  result = sender ?
           (send(peer, expData, 1, 0) > 0 && recv(peer, expData, 1, 0) > 0) :
           (recv(peer, expData, 1, 0) > 0 && send(peer, expData, 1, 0) > 0);
  stopTime = MicroTime();
  RESETREALTIMER;
  if(!result) {
    SetPktTimeOut(peer,stopTime - startTime,1);
    free(expData);
    FAIL("DoTcpExp: latency failed\n");
  }
  SetPktTimeOut(peer,stopTime - startTime,0);
  results[1] = ((double)(stopTime - startTime)) / 1000.0;

  /* ... then bandwidth. */
  SetRealTimer((int)PktTimeOut(peer) * (sizes->expSize / sizes->msgSize + 1));
  startTime = MicroTime();

  for(bytesTotal = 0;
      bytesTotal < sizes->expSize;
      bytesTotal += bytesThisMessage) {
    for(bytesThisMessage = 0;
        bytesThisMessage < sizes->msgSize;
        bytesThisMessage += bytesThisCall) {
      bytesThisCall = sender ?
                      send(peer, expData, sizes->msgSize - bytesThisMessage, 0):
                      recv(peer, expData, sizes->msgSize - bytesThisMessage, 0);
      if(bytesThisCall <= 0) {
        RESETREALTIMER;
        free(expData);
        FAIL1("DoTcpExp: connection failed after %d bytes\n",
              bytesTotal + bytesThisMessage);
      }
    }
  }

  RESETREALTIMER;

  if(sender) {
    /*
     * Wait for the receiver to close the connection so that we're sure that
     * all the data has been sent and received, rather than locally buffered.
     */
    FD_ZERO(&peerFDs);
    FD_SET(peer, &peerFDs);
    timeOut.tv_sec = PktTimeOut(peer);
    assert(!(timeOut.tv_sec == 0));
    timeOut.tv_usec = 0;
    (void)select(peer + 1, &peerFDs, NULL, NULL, &timeOut);
  }

  stopTime = MicroTime();
  results[0] = ( ((double)sizes->expSize * 8.0) /
                 ((double)(stopTime - startTime) / 1000000.0) ) / 1000000.0;
  free(expData);
#ifdef LINUX
        if(siginterrupt(SIGALRM, 0) < 0) {
	    printf("siginterrupt failed\n");
	    exit(-1);
	}
#endif

  return 1;

}


/*
 * Establishes a connection to #machine#:#port# on which the buffer sizes have
 * been set to #size# bytes.  If succeessful, returns 1 and sets #connected# to
 * the connected socket; else returns 0.
 */
static int
ConnectBuffered(IPAddress machine,
                unsigned short port,
                unsigned int size,
                Socket *connected) {
  struct sockaddr_in peer;
  if((*connected = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    FAIL("ConnectBuffered: socket() failed\n");
  }
  setsockopt(*connected, SOL_SOCKET, SO_RCVBUF, (char *)&size, sizeof(size));
  setsockopt(*connected, SOL_SOCKET, SO_SNDBUF, (char *)&size, sizeof(size));
  memset(&peer, 0, sizeof(peer));
  peer.sin_addr.s_addr = machine;
  peer.sin_family = AF_INET;
  peer.sin_port = htons(port);
  SetRealTimer(MINCONNTIMEOUT);
  if(connect(*connected, (struct sockaddr *)&peer, sizeof(peer)) < 0) {
    RESETREALTIMER;
    DROP_SOCKET(connected);
    FAIL2("ConnectBuffered: connect to experiment socket %s:%d failed\n",
          IPAddressMachine(machine), port);
  }
  RESETREALTIMER;
  return 1;
}


int
InitiateTcpExp(IPAddress machine,
               unsigned short port,
               MessageType requestMessage,
               const TcpBwCtrl *sizes,
               double timeOut,
               double *results) {

  TcpHandshake handshake;
  Socket serverSock;
  int returnValue;

  LOG5("TCP(%d, %d, %d) to %s:%d\n",
       sizes->expSize, sizes->bufferSize, sizes->msgSize,
       IPAddressMachine(machine), port);

  if(!ConnectBuffered(machine, port, sizes->bufferSize, &serverSock)) {
    return 0; /* Error message will come from ConnectBuffered */
  }

  if(!SendMessageAndData(serverSock,
                         requestMessage,
                         sizes,
                         tcpBwCtrlDescriptor,
                         tcpBwCtrlDescriptorLength,
                         PktTimeOut(serverSock))) {
    DROP_SOCKET(&serverSock);
    FAIL("InitiateTcpExp: request send failed\n");
  }

  /*
   * Note: the server may have to set up a new listening port before it can
   * respond, so we use a generous time-out value here.
   */
  if(!RecvMessageAndData(serverSock,
                         TCP_HANDSHAKE,
                         &handshake,
                         tcpHandshakeDescriptor,
                         tcpHandshakeDescriptorLength,
                         MAXPKTTIMEOUT)) {
    DROP_SOCKET(&serverSock);
    FAIL("InitiateTcpExp: receive of handshake failed\n");
  }

  if(handshake.portNum != REUSE_CONNECTION) {
    /* Reestablish the connection on the port contained in the handshake. */
    DROP_SOCKET(&serverSock);
    if(!ConnectBuffered((handshake.ipAddr == 0) ? machine : handshake.ipAddr,
                        handshake.portNum,
                        sizes->bufferSize,
                        &serverSock)) {
      return 0; /* Error message will come from ConnectBuffered */
    }
  }

  returnValue = DoTcpExp(serverSock, sizes, 1, results);
  DROP_SOCKET(&serverSock);
  return 1;

}


int
TerminateTcpExp(Socket sender) {

  int bufferSize;
  SOCKLEN_T bufferSizeSize = sizeof(bufferSize);
  struct sockaddr_in client;
  SOCKLEN_T clientSize = sizeof(client);
  Socket clientSock;
  Socket earSock;
  fd_set earFD;
  TcpHandshake handshake;
  double ignored[2];
  struct sockaddr_in myEnd;
  SOCKLEN_T myEndSize = sizeof(myEnd);
  int returnValue;
  struct sockaddr_in server;
  SOCKLEN_T serverSize = sizeof(server);
  TcpBwCtrl sizes;
  struct timeval timeOut;

  if(!RecvData(sender,
               &sizes,
               tcpBwCtrlDescriptor,
               tcpBwCtrlDescriptorLength,
               PktTimeOut(sender))) {
    FAIL("TerminateTcpExp: configuration receive failed\n");
  }

  LOG3("Servicing TCP(%d, %d, %d)\n",
       sizes.expSize, sizes.bufferSize, sizes.msgSize);

  getsockname(sender, (struct sockaddr *)&myEnd, &myEndSize);
  /* The following statement causes problems if we're behind a firewall. */
  /* handshake.ipAddr = myEnd.sin_addr.s_addr; */
  handshake.ipAddr = 0;

  if(getsockopt(sender,
                SOL_SOCKET,
                SO_RCVBUF,
                (char *)&bufferSize,
                &bufferSizeSize) == 0 &&
     bufferSize == sizes.bufferSize) {

    /* We can reuse the connection for the experiment. */
    handshake.portNum = REUSE_CONNECTION;
    if(!SendMessageAndData(sender,
                           TCP_HANDSHAKE,
                           &handshake,
                           tcpHandshakeDescriptor,
                           tcpHandshakeDescriptorLength,
                           PktTimeOut(sender))) {
      FAIL1("TerminateTcpExp: send of reply failed on %d\n", sender);
    }
    returnValue = DoTcpExp(sender, &sizes, 0, ignored);

  }
  else {

    /* Establish a listening port with the experiment buffer size. */
    earSock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(earSock,
               SOL_SOCKET,
               SO_RCVBUF,
               (char *)&sizes.bufferSize,
               sizeof(sizes.bufferSize));
    memset(&server, 0, sizeof(server));
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    server.sin_port = 0;

    if((bind(earSock, (struct sockaddr *)&server, sizeof(server)) < 0) ||
       (listen(earSock, 1) < 0) ||
       (getsockname(earSock, (struct sockaddr *)&server, &serverSize) < 0)) {
      DROP_SOCKET(&earSock);
      FAIL("TerminateTcpExp: bind/listen failed.\n");
    }

    /* Tell the client what port to contact, then wait for them to respond. */
    handshake.portNum = ntohs(server.sin_port);

    if(!SendMessageAndData(sender,
                           TCP_HANDSHAKE,
                           &handshake,
                           tcpHandshakeDescriptor,
                           tcpHandshakeDescriptorLength,
                           PktTimeOut(sender))) {
      DROP_SOCKET(&earSock);
      FAIL1("TerminateTcpExp: send of reply failed on %d\n", sender);
    }

    /* Give the client 10 seconds to connect. */
    FD_ZERO(&earFD);
    FD_SET(earSock, &earFD);
    timeOut.tv_sec = 10;
    timeOut.tv_usec = 0;

    if((select(earSock + 1, &earFD, NULL, NULL, &timeOut) < 1) ||
       ((clientSock = accept(earSock,
                             (struct sockaddr *)&client,
                             &clientSize)) < 0)) {
      DROP_SOCKET(&earSock);
      FAIL("TerminateTcpExp: accept failed\n");
    }

    returnValue = DoTcpExp(clientSock, &sizes, 0, ignored);
    DROP_SOCKET(&clientSock);
    DROP_SOCKET(&earSock);

  }

  return returnValue;

}
