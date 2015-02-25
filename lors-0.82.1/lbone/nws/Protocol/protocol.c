/* $Id: protocol.c,v 1.98 2001/02/12 14:55:12 swany Exp $ */

static int debug = 0;

#include "config.h"
#include <unistd.h>       /* close() pipe() read() write() */
#include <netinet/in.h>   /* sometimes required for #include <arpa/inet.h> */
#include <netinet/tcp.h>  /* TCP_NODELAY */
#include <arpa/inet.h>    /* inet_ntoa() */
#include <netdb.h>        /* getprotobyname() */
#include <sys/time.h>     /* struct timeval */
#include <errno.h>        /* errno */
#include <signal.h>       /* signal() */
#include <string.h>       /* memset() strerror() */
#include <sys/wait.h>     /* waitpid() */
#include <sys/socket.h>   /* getpeername() socket() */

#include "diagnostic.h"   /* DDEBUG() FAIL() LOG() WARN() */
#include "osutil.h"       /* CurrentTime() SetRealTimer() sig{hold,relse}() */
#include "protocol.h"


#define MAX_NOTIFIES 40
static SocketFunction disconnectFunctions[MAX_NOTIFIES];
static fd_set connectedEars;
static fd_set connectedPipes;
static fd_set connectedSockets;


/*
** Note: the following set of functions adaptively computes time-out values for
** connections.  It should be converted to using the forecasting facilities
** now generally available via forecaster_api.h.
*/


#define ADDR_HASH_SIZE 1000
#define MODPLUS(a,b,m) (((a) + (b)) % (m))
#define MODMINUS(a,b,m) (((a) - (b) + (m)) % (m))


#define PGAIN 0.1


struct t_o {
  double timeout;
  double est;
  double dev;
};


static struct t_o Pkt_to[ADDR_HASH_SIZE];
static struct t_o Conn_to[ADDR_HASH_SIZE];
static IPAddress Addrs[ADDR_HASH_SIZE];


static int
HashIndex(Socket sd,
          int *hashindex,
          IPAddress *addr) {

  struct sockaddr peer_name_buff;
  SOCKLEN_T peer_name_buff_size = sizeof(peer_name_buff);
  IPAddress temp_addr;
  unsigned int i;
  int end;

  if(getpeername(sd, &peer_name_buff, &peer_name_buff_size) < 0) {
    return(0);
  }

  temp_addr = ((struct sockaddr_in *)&peer_name_buff)->sin_addr.s_addr;
  i = temp_addr % ADDR_HASH_SIZE;
  end = MODMINUS(i, 1, ADDR_HASH_SIZE);

  while(i != end) {
    if((Addrs[i] == 0) || (Addrs[i] == temp_addr)) {
      *hashindex = i;
      *addr = temp_addr;
      return(1);
    }
    i = MODPLUS(i, 1, ADDR_HASH_SIZE);
  }

  *hashindex = temp_addr % ADDR_HASH_SIZE;
  *addr = temp_addr;
  /* XXX set the corresponding t_o here */
  /* <hack to work around inconsistent use of this function> */
  Pkt_to[*hashindex].timeout = PKTTIMEOUT;
  Pkt_to[*hashindex].est = PKTTIMEOUT;
  Pkt_to[*hashindex].dev = 0.0;
  Conn_to[*hashindex].timeout = CONNTIMEOUT;
  Conn_to[*hashindex].est = CONNTIMEOUT;
  Conn_to[*hashindex].dev = 0.0;
  /* </hack> */
  return(1);

}


static int
HashIndexAddr(IPAddress addr,
              int *hashindex) {

  unsigned int i;
  int end;

  i = addr % ADDR_HASH_SIZE;
  end = MODMINUS(i, 1, ADDR_HASH_SIZE);

  while(i != end) {
    if((Addrs[i] == 0) || (Addrs[i] == addr)) {
      *hashindex = i;
      return(1);
    }
    i = MODPLUS(i,1,ADDR_HASH_SIZE);
  }

  /* the table is full, but not with the address we're looking for. */
  /* overwrite in this location, assume that the old value is stale. */
  *hashindex = addr % ADDR_HASH_SIZE;
  /* <hack to work around inconsistent use of this function> */
  Pkt_to[*hashindex].timeout = PKTTIMEOUT;
  Pkt_to[*hashindex].est = 0.0;
  Pkt_to[*hashindex].dev = 0.0;
  Conn_to[*hashindex].timeout = CONNTIMEOUT;
  Conn_to[*hashindex].est = 0.0;
  Conn_to[*hashindex].dev = 0.0;
  /* </hack> */

  return(1);

}


static int
InsertHash(Socket sd) {
  int i;
  IPAddress temp_addr;
  if(!HashIndex(sd, &i, &temp_addr)) {
    FAIL("InsertHash: can't insert addr (failed getpeername)\n");
  }
  Addrs[i] = temp_addr;
  Pkt_to[i].timeout = PKTTIMEOUT;
  return(1);
}


static int
InsertHashAddr(IPAddress addr) {
  int i;
  if(!HashIndexAddr(addr, &i)) {
    FAIL("InsertHash: can't insert addr\n");
  }
  Addrs[i] = addr;
  return(1);
}


static void
SetConnTimeOut(IPAddress addr,
               double duration,
               int timedOut) {

  double dev;
  double err;
  double est;
  int hashindex;
  double timeout;

  if(HashIndexAddr(addr, &hashindex) == 0) {
    return;
  }

  est = Conn_to[hashindex].est;
  dev = Conn_to[hashindex].dev;
  err = est - duration;

  if(err < 0.0)
    err = err * -1.0;

  est = est + PGAIN * (duration - est);

  if(timedOut)
    est++;

  dev = dev + PGAIN * (err - dev);

  if(dev < 0.0)
    dev = -1.0 * dev;

  timeout = est + 2 * dev;

  Conn_to[hashindex].est = est;
  Conn_to[hashindex].dev = dev;

  if(timeout < MINCONNTIMEOUT)
    timeout = MINCONNTIMEOUT;

  if(timeout > MAXCONNTIMEOUT)
    timeout = MAXCONNTIMEOUT;

  Conn_to[hashindex].timeout = timeout;

}


void
SetPktTimeOut(Socket sd,
              double duration,
              int timedOut) {

  IPAddress addr;
  double dev;
  double err;
  double est;
  int hashindex;
  double timeout;

  if(HashIndex(sd,&hashindex, &addr) == 0) {
    WARN("SetPktTimeOut: HashIndex failed (getpeername)\n");
    return;
  }

  est = Pkt_to[hashindex].est;
  dev = Pkt_to[hashindex].dev;
  err = est - duration;

  if(err < 0.0)
    err = err * -1.0;

  est = est + PGAIN * (duration - est);

  if(timedOut)
    est++;

  dev = dev + PGAIN * (err - dev);

  if(dev < 0.0)
    dev = -1.0 * dev;

  timeout = est + 2 * dev;

  Pkt_to[hashindex].est = est;
  Pkt_to[hashindex].dev = dev;

  if(timeout < MINPKTTIMEOUT)
    timeout = MINPKTTIMEOUT;

  if(timeout > MAXPKTTIMEOUT)
    timeout = MAXPKTTIMEOUT;

  Pkt_to[hashindex].timeout = timeout;

}


double
ConnTimeOut(IPAddress addr) {
  int hashindex;
  return(HashIndexAddr(addr, &hashindex) ?
         Conn_to[hashindex].timeout : CONNTIMEOUT);
}


double
PktTimeOut(Socket sd) {
  int hashindex;
  IPAddress temp_addr;
  double time_out;

  /* XXX check to see if this peer is indexed already */
  time_out = HashIndex(sd, &hashindex, &temp_addr) ?
         Pkt_to[hashindex].timeout : PKTTIMEOUT;

  if(time_out == 0)
	time_out = PKTTIMEOUT;

  return(time_out);
}


/*
** Beginning of connection functions.
*/


static int
TcpProtoNumber(void);


/*
** Remove #sock# from all maintained socket sets.
*/
static void
ClearSocket(Socket sock) {
  FD_CLR(sock, &connectedPipes);
  FD_CLR(sock, &connectedSockets);
  FD_CLR(sock, &connectedEars);
}


static int
ConditionSocket(Socket sd) {

  int one = 1;
  int sock_opt_len = sizeof(one);

  if(setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one, sock_opt_len) < 0) {
    WARN("ConditionSocket: keepalive option failed\n");
  }

  if(setsockopt(sd, TcpProtoNumber(),
                TCP_NODELAY, (char *)&one, sock_opt_len) < 0) {
    WARN("ConditionSocket: couldn't set NODELAY flag\n");
  }

  SetPktTimeOut(sd, PKTTIMEOUT, 0);
  return(1);

}


/*
** Time-out signal handler for CallAddr().
*/
static void
ConnectTimeOut(int sig) {
  WARN("Connect timed out\n");
}


/*
** A "local" function of CloseConnections().  Closes each member of #sockets#.
*/
static void
DisconnectAll(fd_set *sockets) {
  Socket dead, i;
  for(i = 0; i < FD_SETSIZE; i++) {
    if(FD_ISSET(i, sockets)) {
      dead = i;
      DROP_SOCKET(&dead);
    }
  }
}


/*
** Notifies all registered functions that #sock# has been closed.
*/
static void
DoDisconnectNotification(Socket sock) {
  int i;
  for(i = 0; i < MAX_NOTIFIES; i++) {
    if(disconnectFunctions[i] != NULL) {
      disconnectFunctions[i](sock);
    }
  }
}


/*
** Returns 1 or 0 depending on whether or not #sd# is a connected socket.
*/
static int
IsConnected(Socket sd) {
  struct sockaddr peer_name_buff;
  SOCKLEN_T peer_name_buff_size = sizeof(peer_name_buff);
  return(getpeername(sd, &peer_name_buff, &peer_name_buff_size) >= 0);
}


/*
** Time-out signal handler for RecvBytes().
*/
static void
RecvTimeOut(int sig) {
  WARN("Receive timed out\n");
}


/*
** Returns the tcp protocol number from the network protocol data base.
*/
static int
TcpProtoNumber(void) {
  struct protoent *fetchedEntry;
  static int returnValue = 0;
  if(returnValue == 0) {
    fetchedEntry = getprotobyname("tcp");
    if(fetchedEntry != NULL) {
      returnValue = fetchedEntry->p_proto;
    }
  }
  return returnValue;
}


int
CallAddr(IPAddress addr,
         short port,
         Socket *sock,
         double timeOut) {

  struct sockaddr_in server; /* remote host address */
  Socket sd;
  int one = 1;
  int sock_opt_len = sizeof(int);
  double start;
  double end;
  double ltimeout;
  void (*was)(int);

  was = signal(SIGALRM, ConnectTimeOut);

  memset((char *)&server, 0, sizeof(server));
  server.sin_addr.s_addr = addr;
  server.sin_family = AF_INET;
  server.sin_port = htons((u_short)port);

  sd = socket(AF_INET, SOCK_STREAM, 0);

  if(sd < 0) {
    *sock = NO_SOCKET;
    signal(SIGALRM, was);
    FAIL("CallAddr: cannot create socket to server\n");
  }

  if(setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one, sock_opt_len) < 0) {
    WARN("CallAddr: keepalive option failed\n");
  }

  if(setsockopt(sd, TcpProtoNumber(),
                TCP_NODELAY, (char *)&one, sock_opt_len) < 0) {
    WARN("CallAddr: couldn't set NODELAY flag\n");
  }

  ltimeout = ConnTimeOut(addr);

  if(ltimeout == 0.0)
    ltimeout = timeOut;

  SetRealTimer((unsigned int)ltimeout);
  start = CurrentTime();
  
  if(connect(sd, (struct sockaddr *)&server, sizeof(server)) < 0) {
    shutdown(sd, 2);
    close(sd);
    *sock = NO_SOCKET;
    RESETREALTIMER;
    if(errno == EINTR) {
      /* this was a timeout */
      end = CurrentTime();
      InsertHashAddr(addr);
      SetConnTimeOut(addr, end - start, 1);
    }
    signal(SIGALRM,was);
    FAIL("CallAddr: connect failed\n");
  }

  end = CurrentTime();
  RESETREALTIMER;

  *sock = sd;
  if (debug > 0) {
      DDEBUG2("CallAddr: connected socket %d to %s\n", sd, PeerName(sd));
  }
  FD_SET(sd, &connectedSockets);

  InsertHash(sd);
  SetConnTimeOut(addr, end - start, 0);
  SetPktTimeOut(sd, end - start, 0);

  signal(SIGALRM,was);
  return(1);

}


void
CloseConnections(int closeEars,
                 int closePipes,
                 int closeSockets) {
  if(closeEars)
    DisconnectAll(&connectedEars);
  if(closePipes)
    DisconnectAll(&connectedPipes);
  if(closeSockets)
    DisconnectAll(&connectedSockets);
}


int
CloseDisconnections(void) {
  Socket dead, i;
  int returnValue = 0;
  for(i = 0; i < FD_SETSIZE; i++) {
    if(FD_ISSET(i, &connectedSockets) && !IsConnected(i)) {
      dead = i;
      DROP_SOCKET(&dead);
      returnValue++;
    }
  }
  return(returnValue);
}


int
CloseSocket(Socket *sock,
            int waitForPeer) {

  fd_set readFDs;
  Socket sd = *sock;
  struct timeval timeout;

  if (debug > 0) {
      DDEBUG1("CloseSocket: Closing connection %d\n", *sock);
  }

  if(*sock == NO_SOCKET) {
    return 1;  /* Already closed; nothing to do. */
  }

  if(waitForPeer > 0) {

    FD_ZERO(&readFDs);
    FD_SET(sd, &readFDs);
    timeout.tv_sec = waitForPeer;
    timeout.tv_usec = 0;

    if(select(FD_SETSIZE, &readFDs, NULL, NULL, &timeout) < 0) {
      FAIL2("CloseSocket: no response on select %d %d\n", sd, errno);
    }

  }

  if(!FD_ISSET(sd, &connectedPipes)) {
    if(shutdown(sd, 2) < 0) {
      /* The other side may have beaten us to the reset. */
      if ((errno != ENOTCONN) && (errno != ECONNRESET)) {
        WARN1("CloseSocket: shutdown error %d\n", errno);
      }
    }
  }

  if(close(sd) < 0) {
    WARN2("CloseSocket: close error %d (%s)\n", errno, strerror(errno));
  }
  ClearSocket(sd);
  DoDisconnectNotification(sd);
  *sock = NO_SOCKET;
  return(1);

}


#define READ_END 0
#define WRITE_END 1

int
CreateLocalChild(pid_t *pid,
                 Socket *parentToChild,
                 Socket *childToParent) {

  int childWrite[2];
  int parentWrite[2];
  int myEnd;

  if(parentToChild != NULL) {
    if(pipe(parentWrite) == -1) {
      FAIL1("CreateLocalChild: couldn't get pipe, errno: %d\n", errno);
    }
  }
  if(childToParent != NULL) {
    if(pipe(childWrite) == -1) {
      if(parentToChild != NULL) {
        close(parentWrite[0]);
        close(parentWrite[1]);
      }
      FAIL1("CreateLocalChild: couldn't get pipe, errno: %d\n", errno);
    }
  }

  *pid = fork();

  if(*pid == -1) {
    if(parentToChild != NULL) {
      close(parentWrite[0]);
      close(parentWrite[1]);
    }
    if(childToParent != NULL) {
      close(childWrite[0]);
      close(childWrite[1]);
    }
    FAIL2("CreateLocalChild: couldn't fork, errno: %d (%s)\n",
          errno, strerror(errno));
  }

  /* Close descriptors that this process won't be using. */
  if(parentToChild != NULL) {
    myEnd = (*pid == 0) ? READ_END : WRITE_END;
    close(parentWrite[1 - myEnd]);
    FD_SET(parentWrite[myEnd], &connectedPipes);
    *parentToChild = parentWrite[myEnd];
  }

  if(childToParent != NULL) {
    myEnd = (*pid == 0) ? WRITE_END : READ_END;
    close(childWrite[1 - myEnd]);
    FD_SET(childWrite[myEnd], &connectedPipes);
    *childToParent = childWrite[myEnd];
  }

  return(1);

}


int
EstablishAnEar(short startingPort,
               short endingPort,
               Socket *ear,
               short *earPort) {

  int k32 = 32 * 1024;
  int on = 1;
  short port;
  Socket sd = NO_SOCKET;
  struct sockaddr_in server;

  for(port = startingPort; port <= endingPort; port++) {
    server.sin_port = htons((u_short)port);
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_family = AF_INET;
    if((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      FAIL("EstablishAnEar: socket allocation failed\n");
    }
    (void)setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
    /*
     * Set the socket buffer sizes to 32k, which just happens to correspond to
     * the most common option value for tcpMessageMonitor activities.  This
     * allows us to use a client connection to conduct the experiment, rather
     * than needing to configure and open a new connection.
     */
    (void)setsockopt(sd, SOL_SOCKET, SO_RCVBUF, (char *)&k32, sizeof(k32));
    (void)setsockopt(sd, SOL_SOCKET, SO_SNDBUF, (char *)&k32, sizeof(k32));
    if(bind(sd, (struct sockaddr *)&server, sizeof(server)) != -1 &&
       listen(sd, 5) != -1) {
      break;
    }
    close(sd);
  }

  if(port > endingPort) {
    FAIL2("EstablishAnEar: couldn't find a port between %d and %d\n",
          startingPort, endingPort);
  }

  if (debug > 0) {
      DDEBUG1("EstablistAnEar: connected socket %d\n", sd);
  }
  FD_SET(sd, &connectedEars);

  *ear = sd;
  *earPort = port;
  return(1);

}


int
IncomingRequest(double timeOut,
                Socket *sd) {

  Socket dead;
  Socket i;
  char lookahead;
  Socket newSock;
  double now;
  struct sockaddr_in peer_in;
  SOCKLEN_T peer_in_len = sizeof(peer_in);
  fd_set readFds;
  struct timeval tout;
  double wakeup;

  /*
  ** nextToService is used to make sure that every connection gets a chance to
  ** be serviced.  If we always started checking at 0, very active connections
  ** with small descriptor numbers could starve larger-descriptor connections.
  */
  static Socket nextToService = 0;

  *sd = NO_SOCKET;
  tout.tv_usec = 0;
  wakeup = CurrentTime() + timeOut;

  while((now = CurrentTime()) < wakeup) {

    /* Construct in readFds the union of connected ears, pipes, and sockets. */
    readFds = connectedSockets;
    for(i = 0; i < FD_SETSIZE; i++) {
      if(FD_ISSET(i, &connectedPipes)) {
        FD_SET(i, &readFds);
      }
    }
    for(i = 0; i < FD_SETSIZE; i++) {
      if(FD_ISSET(i, &connectedEars)) {
        FD_SET(i, &readFds);
      }
    }

    tout.tv_sec = (unsigned int)wakeup - (unsigned int)now;

    switch(select(FD_SETSIZE, &readFds, NULL, NULL, &tout)) {

    case -1:
      if(errno == EINTR) {
        continue;
      }
      else if(errno == EINVAL) {
        /* we blew it somehow -- this is mostly for osf... */
        /* can be because (from man page):
          [EINVAL] The time limit specified by the timeout parameter is invalid.

                   The nfds parameter is less than 0, or greater than or equal
                   to FD_SETSIZE.

                   One of the specified file descriptors refers to a STREAM or
                   multiplexer that is linked (directly or indirectly)
                   downstream from a multiplexer.
        */

        FAIL4("IncomingRequest: invalid select - nfds: %d, rfds: %d, to: %d.%d",
              FD_SETSIZE, readFds, tout.tv_sec, tout.tv_usec);
      }
      else {
        FAIL1("IncomingRequest: select error %d\n", errno);
      }
      break;

    case 0:
      /* timeout */
      break;

    default:

      i = nextToService;

      while(1) {

        if(FD_ISSET(i, &readFds)) {

          if(FD_ISSET(i, &connectedEars)) {
            /* Add the new connection to connectedSockets. */
            newSock = accept(i, (struct sockaddr *)&peer_in, &peer_in_len);
            if(newSock == -1) {
              SocketFailure(SIGPIPE);
            }
            else {
              ConditionSocket(newSock);
	      if (debug > 0) {
                  DDEBUG2("IncomingRequest: connected socket %d to %s\n",
                      newSock, PeerName(newSock));
	      }
              FD_SET(newSock, &connectedSockets);
            }
          }
          else if(FD_ISSET(i, &connectedPipes)) {
            *sd = i;
            nextToService = (i + 1) % FD_SETSIZE;
            return(1);
          }
          else {
            /* Existing socket connection. */
            if(recv(i, &lookahead, 1, MSG_PEEK) > 0) {
              *sd = i;
              nextToService = (i + 1) % FD_SETSIZE;
              return(1);
            }
            else {
              /*
              ** This is how we find out about connections closed by a peer.
              ** Drop it from our list of known connections.
              */
		if (debug > 0) {
                  DDEBUG1("IncomingRequest: Dropping closed connection %d\n", i);
		}
              dead = i;
              DROP_SOCKET(&dead);
            }
          }

        }

        i = (i + 1) % FD_SETSIZE;
        if(i == nextToService) {
          /* We've been through the entire set without finding a request. */
          break;
        }

      }
      break;

    }

  }

  return(0);

}


int
IsOkay(Socket sd) {

  fd_set readFds;
  fd_set writeFds;
  struct timeval timeout;

  if(sd == NO_SOCKET)
    return(0);

  FD_ZERO(&readFds);
  FD_ZERO(&writeFds);
  FD_SET(sd, &readFds);
  FD_SET(sd, &writeFds);
  timeout.tv_sec  = 60; /* wait one minute then punt */
  timeout.tv_usec = 0;

  return(select(FD_SETSIZE, NULL, &writeFds, NULL, &timeout) == 1);

}


void
NotifyOnDisconnection(SocketFunction notifyFn) {
  int i;
  for(i = 0; i < MAX_NOTIFIES; i++) {
    if(disconnectFunctions[i] == NULL) {
      disconnectFunctions[i] = notifyFn;
      break;
    }
  }
}


#define MAXPASSES 40
static pid_t passedPids[MAXPASSES];
static Socket passedSockets[MAXPASSES];


int
PassSocket(Socket *sock,
           pid_t child) {

  int i, childStat;

  /* Clean up any sockets previously passed to children who have exited. */
  for(i = 0; i < MAXPASSES; i++) {
    if(passedPids[i] != 0) {
      if((waitpid(passedPids[i], &childStat, WNOHANG) < 0) ||
         WIFEXITED(childStat)) {
        LOG1("PassSocket: Reclaiming connection %d\n", passedSockets[i]);
        (void)shutdown(passedSockets[i], 2);
        (void)close(passedSockets[i]);
        DoDisconnectNotification(passedSockets[i]);
        passedPids[i] = 0;
      }
    }
  }

  /* Record this socket in passedSockets and remove all other memory of it. */
  for(i = 0; i < MAXPASSES; i++) {
    if(passedPids[i] == 0) {
      LOG2("PassSocket: Passing connection %d to %d\n", *sock, child);
      passedPids[i] = child;
      passedSockets[i] = *sock;
      ClearSocket(*sock);
      *sock = NO_SOCKET;
      return(1);
    }
  }

  return(0);

}


const char *
PeerName(Socket sd) {
  struct sockaddr peer;
  SOCKLEN_T peer_size = sizeof(peer);
  static char returnValue[200];
  strcpy(returnValue, FD_ISSET(sd, &connectedPipes) ? "pipe" :
                      (getpeername(sd, &peer, &peer_size) < 0) ?
                      "(unknown)" :
                      inet_ntoa(((struct sockaddr_in *)&peer)->sin_addr));
  return returnValue;
}



int
RecvBytes(Socket sd,
          void *bytes,
          size_t byteSize,
          double timeOut) {

  double end;
  int isPipe;
  char *nextByte;
  fd_set readFds;
  int recvd;
  double start;
  int totalRecvd;
  struct timeval tout;
  void (*was)(int);

  isPipe = FD_ISSET(sd, &connectedPipes);
  FD_ZERO(&readFds);
  FD_SET(sd, &readFds);

  tout.tv_sec = (int)timeOut;
  tout.tv_usec = 0;

  start = CurrentTime();
  was = signal(SIGALRM, RecvTimeOut);
  nextByte = (char *)bytes;

  for(totalRecvd = 0; totalRecvd < byteSize; totalRecvd += recvd) {
    
    recvd = 0;
    switch (select(FD_SETSIZE, &readFds, NULL, NULL, &tout)) {

    case -1:
      if(errno == EINTR) {
        end = CurrentTime();
        if((int)(end - start) < timeOut) {
          tout.tv_sec = (int)(timeOut - (end - start));
          continue;
        }
      }
      signal(SIGALRM, was);
      FAIL2("RecvBytes: select %d failed %s\n", sd, strerror(errno));
      break;

    case 0:
      if(!isPipe) {
        end = CurrentTime();
        SetPktTimeOut(sd, end - start, 1);
      }
      signal(SIGALRM, was);
      FAIL3("RecvBytes: timed out socket %d after %d/%fs\n",
            sd, tout.tv_sec, timeOut);
      break;

    default:
      SetRealTimer((unsigned int)timeOut);

 	  recvd = read(sd, nextByte, (size_t)(byteSize - totalRecvd));

      RESETREALTIMER;

      if(recvd <= 0) {
		WARN3("RecvBytes: read() on descriptor %d returned %d (errno=%d).\n", sd, recvd, errno);
        if(!isPipe) {
          end = CurrentTime();
          SetPktTimeOut(sd, end - start, 0);
        }
        signal(SIGALRM, was);
        FAIL5("RecvBytes: sd %d (addr:%s) failed with %s after %d of %d\n",
              sd, PeerName(sd), strerror(errno), totalRecvd, byteSize);
      }
      break;

    }

    nextByte += recvd;

  }

  return(1);

}


int
SendBytes(Socket sd,
          const void *bytes,
          size_t byteSize,
          double timeOut) {

  int isPipe;
  char *nextByte;
  int sent;
  int totalSent;

  if (debug > 0) {
      DDEBUG2("SendBytes: To %d with t/o %f\n", sd, timeOut);
  }

  isPipe = FD_ISSET(sd, &connectedPipes);
  nextByte = (char *)bytes;

  for(totalSent = 0; totalSent < byteSize; totalSent += sent) {
    sent = isPipe ? write(sd, nextByte, (size_t)(byteSize - totalSent)) :
                    send(sd, nextByte, byteSize - totalSent, 0);
    if(sent <= 0) {
      FAIL1("SendBytes: %d send failed\n", sd);
    }
    nextByte += sent;
  }

  return(1);

}


void
SocketFailure(int sig) {
  HoldSignal(SIGPIPE);
  (void)CloseDisconnections();
  if(signal(SIGPIPE, SocketFailure) == SIG_ERR) {
    WARN("SocketFailure: error resetting signal\n");
  }
  ReleaseSignal(SIGPIPE);
}
