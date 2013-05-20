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



int SendData2(int sourcefd, int *targetfd, unsigned long int filesize,int numtargets)
{
  unsigned long    int nread=0, nwrite, ns=0, readsize, i, offset=0;
  long             int nt;
  char             *buffptr, buff[MAXBUFFLEN];
  struct ibp_timer timeout;
  int              fin,retval;  
  char             tmpbuf[3];

  while(nread < filesize){ 
    readsize = ((filesize-nread)<(MAXBUFFLEN) ? (filesize-nread):(MAXBUFFLEN));
    bzero(buff,MAXBUFFLEN); 
    /***ns = sRead(sourcefd, buff, readsize);*/
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
      for(i=0; i<numtargets; i++){
	if((nt = sWrite(targetfd[i], buffptr, ns-nwrite)) < 0){
	  fprintf(stderr,"(host %d) ",i);
	  perror("error writing to socket");
	  return -1;                    /* one fails, everything else fails*/
	}
      }
      buffptr += nt;
      nwrite += nt;
    }    
    nread += ns;
  }

  for(i=0; i<numtargets; i++){
    fin = read(targetfd[i], tmpbuf, 3);
    if(fin > 0){
      if(!strcmp(tmpbuf,"FIN")){
	fprintf(stderr,"End of data transfer [%d]\n",i);
	retval = 1;
      }
      else{
	retval = -1;
	goto FIN;
      }
    }
    else{
      fprintf(stderr,"Cannot end transmission properly, target %d failed\n",i);
      retval = -1;
      goto FIN;
    }
  }
 FIN:  
  return retval;
}

char** String2Array(char *buff, int nelems, int elemsize)
{
  int   i=0;
  char  **retArray;

  retArray = (char **)malloc(sizeof(char*) * nelems);

  retArray[i] = (char *)malloc(sizeof(char) * elemsize);  
  strcpy(retArray[i],strtok(buff," "));

  for(i=1; i<nelems; i++){
    retArray[i] = (char *)malloc(sizeof(char) * elemsize);
    strcpy(retArray[i],strtok(NULL," "));
  } 
  return retArray;
}

/* -----------------------------------------------------------------
   MAIN: This is started by the IBP server in the source depot, it 
   receives as parameters name of target server, port, and readfd, 
   stablishes a connection with the server and sends the data
   ----------------------------------------------------------------- */
main (int argc, char **argv)
{
  int              *connectfds, sourcefd, numtargets, i, nargs;
  int               retval;
  unsigned long int filesize;
  double            rtmp, result;
  double            s;
  char              **targets, **ports;
  float             rate;

  fprintf(stderr,"\nTCP DATA MOVER:\n%s %s %s %s %s %s <%s>\n\n\n",
	  argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
 
  if(argc < 7)
    sys_error("Incorrect parameters: smclientTCP <target_hostname> <port_Number> <file_descriptor> <filesize> <numtargets> <IBP_cap>"); 
 
  sourcefd = atoi(argv[3]);  
  filesize = atol(argv[4]);
  numtargets = atoi(argv[5]);
  strcpy(glb.IBP_cap,argv[6]);

  targets = String2Array(argv[1],numtargets,MAX_HOST_LEN);
  ports = String2Array(argv[2],numtargets,MAX_WORD_LEN);
  connectfds = (int *) malloc(sizeof(int) * numtargets);

  for(i=0;i<numtargets;i++){
    if((connectfds[i]= tcp_connect(targets[i],ports[i]))<0) 
      sys_error("connectfd error: host %d failed");    
  }

  s = seconds();      
  retval = SendData2(sourcefd, connectfds, filesize, numtargets);
  if(retval<0){
    fprintf(stderr,"DM mclientTCP problem sending data\n");
    exit(TCP_FAIL);
  }
  s = seconds()-s;     
  rate = (s !=0) ? filesize/(1024.0*1024*(s)) : 0;

  fprintf(stderr,"\nTransfer time = %6.2f secs ",s);
  fprintf(stderr," size = %d   xfer rate = %.2f MB/s (%.2fMb/s)\n",
	  filesize, rate, rate*8);

  for(i=0;i<numtargets;i++)
    close(connectfds[i]);  

  free(connectfds);
  free(targets);
  free(ports);
  exit(TCP_OK);
}

