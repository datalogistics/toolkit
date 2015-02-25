#include "mylib.h"
#include "ibp_ClientLib.h"

/* -----------------------------------------
   data structures for globals
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


int SendData2(int sourcefd, int targetfd, unsigned long int filesize)
{
  unsigned long int nread=0, nwrite, ns=0, nt, readsize, offset=0;
  char *buffptr, buff[MAXBUFFLEN];
  struct ibp_timer timeout;

  while(nread < filesize){ 
    readsize = ((filesize-nread)<(MAXBUFFLEN) ? (filesize-nread):(MAXBUFFLEN));
    bzero(buff,MAXBUFFLEN); 
    /*ns = sRead(sourcefd, buff, readsize);*/
    offset += ns;
    ns = IBP_load(glb.IBP_cap, &timeout, buff, readsize, offset);
    if(ns == 0){
      perror("error reading from FILE");
      return -1;
    }
    nwrite=0;
    buffptr = buff;
    fprintf(stderr,"#");
    while(nwrite < ns){
      if((nt = sWrite(targetfd, buffptr, ns-nwrite)) < 0){
	perror("error writing to socket");
	return -1;
      }
      buffptr += nt;
      nwrite += nt;
    }    
    nread += ns;
  }
  return 1;
}

/* -----------------------------------------------------------------
   MAIN: This is started by the IBP server in the source depot, it 
   receives as parameters name of target server, port, and readfd, 
   stablishes a connection with the server and sends the data
   ----------------------------------------------------------------- */
main (int argc, char **argv)
{
  int connectfd, sourcefd, numtargets;
  int retval;
  unsigned long int filesize;
  double rtmp, result;

  printf("\nTCP DATA MOVER: %s %s %s %s %s %s <%s>\n\n",argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
  if(argc != 5)
    sys_error("Incorrect parameters: clientTCP <target_hostname> <port_Number> <file_descriptor> <filesize>");    
 
  sourcefd = atoi(argv[3]);  
  filesize = atol(argv[4]);
  numtargets = atoi(argv[5]);
  strcpy(glb.IBP_cap,argv[6]);

  if((connectfd= tcp_connect(argv[1],argv[2]))<0) 
    sys_error("connectfd error");
 
  retval = SendData2(sourcefd, connectfd, filesize);
  if(retval < 0){
    fprintf(stderr,"DM clientTCP: problem sending data\n");
    close(connectfd);
    exit(TCP_FAIL);
  }
  close(connectfd);
  exit(TCP_OK);
}

