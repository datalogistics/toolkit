#include "mylib.h"
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



/* -------------------------------------------------------------------

   ------------------------------------------------------------------- */
int SendData2(int sourcefd, int targetfd, unsigned long int filesize)
{
  unsigned long int nread=0, nwrite, ns=0, readsize, i, offset=0;
  long int nt;
  char *buffptr, buff[MAXBUFFLEN];
  int retval;
  struct ibp_timer timeout;
 
  int fin;  char tmpbuf[3];
  
  timeout.ServerSync = 10;
  timeout.ClientTimeout = 10;
  fprintf(stderr,"IN SEND DATA 2     IBP_cap:<<%s>>\n",glb.IBP_cap);

  while(nread < filesize){ 
    readsize = ((filesize-nread)<(MAXBUFFLEN) ? (filesize-nread):(MAXBUFFLEN));
    bzero(buff,MAXBUFFLEN); 
    /**ns = sRead(sourcefd, buff, readsize);*/
    offset += ns;
    ns = IBP_load(glb.IBP_cap, &timeout, buff, readsize, offset);
    if(ns == 0){
      /**fprintf(stderr,"TCP client failed readsize= %d nread=%d\n",readsize,nread);*/
      perror("smclientTCP: error reading from FILE");
      return -1;
    }
    nwrite=0;
    buffptr = buff;
    fprintf(stderr,"#");
    while(nwrite < ns){
      nt = ns-nwrite;
      if((nt = sWrite(targetfd, buffptr, ns-nwrite)) < 0){
	perror("error writing to socket");
	return -1;                     
      }
      buffptr += nt;
      nwrite += nt;
    }    
    nread += ns;
  }
  
  fin = read(targetfd, tmpbuf, 3);
  if(fin > 0){
    if(!strcmp(tmpbuf,"FIN")){
      fprintf(stderr,"End of data transfer\n");
      retval = 1;
      goto FIN;
    }
    else{
      retval = -1;
      goto FIN;
    }
  }
  else{
    fprintf(stderr,"Cannot end transmission properly, SendData failed\n");
    retval = -1;
  }
 FIN:
  return retval;
}

char** String2Array(char *buff, int nelems, int elemsize)
{
  int i=0;
  char **retArray;

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
  int *connectfds, sourcefd, numtargets, i, nargs;
  int *retval, position, r;
  unsigned long int filesize;
  double rtmp, result;
  double s;
  char **targets, **ports;
  float rate;

  fprintf(stderr,"\nSERIALIZED TCP DATA MOVER:\n%s %s %s %s %s %s <%s>\n\n",
	  argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
 
  if(argc < 7)
    sys_error("Incorrect parameters: smclientTCP <target_hostname> <port_Number> <file_descriptor> <filesize> <numtargets> <IBP_cap>");    
 
  sourcefd = atoi(argv[3]);  
  filesize = atol(argv[4]);
  numtargets = atoi(argv[5]);
  strcpy(glb.IBP_cap,argv[6]);
  retval = (int *)malloc(sizeof(int) * numtargets);

  targets = String2Array(argv[1],numtargets,MAX_HOST_LEN);
  ports = String2Array(argv[2],numtargets,MAX_WORD_LEN);
  connectfds = (int *) malloc(sizeof(int) * numtargets);
  position = lseek(sourcefd,0,SEEK_CUR);

  for(i=0;i<numtargets;i++){
    if((connectfds[i]= tcp_connect(targets[i],ports[i]))<0) 
      sys_error("connectfd error");    
    else
      fprintf(stderr,"Target %d: Success in connectfd==%d\n",i,connectfds[i]);
  }

  for(i=0; i<numtargets; i++){
    r = lseek(sourcefd, position, SEEK_SET);
    s = seconds();  
    retval[i] = SendData2(sourcefd, connectfds[i], filesize);
    if(retval[i]<0){
      fprintf(stderr,"Target %d failed:",i);
      perror(" ");
      exit(TCP_FAIL);
    }
    else{ 
      fprintf(stderr,"\ntarget %d done\n\n",i);
      s = seconds()-s;     
      rate = (s !=0) ? filesize/(1024.0*1024*(s)) : 0;      
      fprintf(stderr,"\nTransfer time = %6.2f secs ",s);
      fprintf(stderr," size = %d   xfer rate = %.2f MB/s (%.2fMb/s)\n",
	      filesize, rate, rate*8);
    }
  } 

  for(i=0;i<numtargets;i++){
    close(connectfds[i]);  
  }

  free(retval);
  free(connectfds);
  free(targets);
  free(ports); 
  exit(TCP_OK);
}



