#include "mylib.h"
#include "ibp-lib.h"
#include "ibp_ClientLib.h"

/* -----------------------------------------
   data structures for globals and threads
   ----------------------------------------- */
typedef struct{
  int  sourcefd;
  unsigned long int filesize;
  int  IBP_port;
  char IBP_depot[MAX_HOST_LEN];
  int  IBP_key;
  char IBP_cap[MAX_CAP_LEN];
} GLOBALS;

GLOBALS glb;


int ReceiveData2(int sourcefd, int targetfd, unsigned long int *filesize)
{
  long             nread=0, nwrite, ns=0, nt, readsize;
  char             buff[MAXBUFFLEN], *buffptr;
  struct ibp_timer timeout;

/*//  fprintf(stderr,"IN RECEIVE DATA 2 <%s>\n",glb.IBP_cap);*/

  timeout.ServerSync = 10;
  timeout.ClientTimeout = 10;
  while (nread < *filesize) {  
    readsize = min(*filesize-nread,MAXBUFFLEN);
    bzero(buff,MAXBUFFLEN);
    ns = sRead(sourcefd, buff, readsize);
    if (ns < 0){
      perror("serverTCP: error reading from socket");
      return -1; 
    }
    nwrite = 0; 
    buffptr = buff;
    while (nwrite < ns) { 

#ifdef IBP_DEBUG   
      fprintf(stderr,"#");
#endif
      /**nt = sWrite(targetfd, buffptr, ns-nwrite);*/
      nt = IBP_store(glb.IBP_cap, &timeout, buffptr, ns-nwrite);
      if (nt < 0){
	fprintf(stderr,"UP TO NOW: %d\n",nwrite);
	perror("serverTCP: error writing to local storage");
	return -1;
      }
      buffptr += nt;
      nwrite += nt;
    }
    nread += ns;
  }
  
  *filesize = nread;
  write(sourcefd,"FIN",3);
  return 1;
}



/* ------------------------------------------------
       MAIN: receives port and fd from IBP DM
   ------------------------------------------------ */
int main (int argc, char **argv)
{
  int               listenfd, connectfd, maxfd=0, writefd, retval;
  unsigned long int filesize;
  char              localhost[MAX_HOST_LEN];

  fprintf(stderr,"\nDATA MOVER  serverTCP running\n");
  if (argc < 4)
    sys_error("TCP Server: incorrect number of parameters");

  writefd = atoi(argv[2]);
  filesize = atol(argv[3]);
  glb.filesize = filesize;
  strcpy(glb.IBP_cap,argv[4]);

  listenfd = tcp_listen(argv[1]);              /* server listening on port */

#ifdef IBP_DEBUG
  fprintf(stderr,"Expecting client to connect on port %s...\n",argv[1]); 
#endif

  if(gethostname(localhost,MAX_HOST_LEN)<0)
    sys_error("\ngethostname: error trying to get name\n");
 
  connectfd = tcp_accept(listenfd);            /* do the connect/accept for a client */

  if(connectfd < 0){
    perror("serverTCP: Problem trying to connect from client");
    exit(TCP_FAIL);
  }
 
#ifdef IBP_DEBUG
  fprintf(stderr,"Waiting for the client to send the data ...\n");  
#endif

  retval = ReceiveData2(connectfd, writefd, &filesize);
  if(retval < 0){
    fprintf(stderr,"serverTCP: failed receiving Data from client!\n");
    exit(TCP_FAIL);
  }

  close(connectfd);
  exit(TCP_OK);
}










