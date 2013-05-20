
#include "config-ibp.h"
#include "mylib.h"
#include "ibp_ClientLib.h"
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif


/* -----------------------------------------
   data structures for globals and threads
   ----------------------------------------- */
typedef struct{
  int  sourcefd;
  unsigned long int filesize;
  char IBP_cap[MAX_CAP_LEN];
  char mainbuffer[MAXBUFFLEN];
  int position;
  int exitval;
} GLOBALS;

GLOBALS glb;

typedef struct{
  char port[8];
  char target[MAX_HOST_LEN];
  int  filefd;                        /* original filedescriptor */
  int  id;
  int  *tbuff;
  int  ns;
  int  mysocket;
} TDTS;                               /* Thread Data Transfer Structure */

typedef TDTS* TDTSP;




/* -----------------------------------------------------------
   creates an array of nelems strings based on the arguments
   ----------------------------------------------------------- */
char** String2Array(char *buff, int nelems, int elemsize)
{
  int   i=0;
  char  **retArray;

  retArray = (char **)malloc(sizeof(char*) * nelems);

  retArray[i] = (char *)malloc(sizeof(char) * elemsize);  /* need to validate this size? */
  strcpy(retArray[i],strtok(buff," "));

  for(i=1; i<nelems; i++){
    retArray[i] = (char *)malloc(sizeof(char) * elemsize);
    strcpy(retArray[i],strtok(NULL," "));
  } 
  return retArray;
}



/* -----------------------------------------------------
   inititalize arguments for thread data structure
   ----------------------------------------------------- */
TDTS* NewTDTS(char *filename, int num, int ns, int mysocket)
{
  TDTS    *t;
  int     filefd;
  char    theCap[MAX_CAP_LEN];

  t = (TDTS *)malloc(sizeof(TDTS));
  if(t == NULL){
    fprintf(stderr,"Problem allocating data for host %d:",num);
    return NULL;
  }

  strcat(theCap,filename);
  t->ns = ns;
  t->id = num;
  t->mysocket = mysocket;
/*
  if((filefd = open(theCap,O_RDONLY)) < 0){
    fprintf(stderr,"Problem reading source capability for host %d:",num);
    perror(" ");  
    return NULL;
  }
  t->filefd = filefd;
*/
  return t;
}



/* ----------------------------------------------------
   thread function that sends data to each target
   ---------------------------------------------------- */
void WriteDataToSocket(TDTS *args)
{
  int   nwrite, ns, nt, sockfd;
  char  *buffptr;

  nwrite=0;
  ns = args->ns;
  buffptr = glb.mainbuffer;
  sockfd = args->mysocket;


  while(nwrite < ns){
     nt = sWrite(sockfd, buffptr, ns-nwrite);
     if(nt < 0) 
     {
       fprintf(stderr,"host %d ",args->id);
       perror("error writing to socket");
       glb.exitval = TCP_FAIL;
       pthread_exit(NULL);   
     }
     buffptr += nt;
     nwrite += nt;
  }    

}


/* -----------------------------------------------------------------
   MAIN: This is started by the IBP server in the source depot, it 
   receives as parameters name of target server, port, and readfd, 
   stablishes a connection with the server and sends the data
   ----------------------------------------------------------------- */
main (int argc, char **argv)
{ 
  int                numtargets, i,fin;
  unsigned long int  nread=0, readsize, ns=0, offset=0;   
  struct ibp_timer   timeout;  
  int                *targetsockets;
  char               **targets, **ports; 
  char               IBP_Cap[MAX_CAP_LEN], filename[MAX_FILE_NAME];
  void               *retval;
  TDTSP              *t_args;
  pthread_t          *threads;
  pthread_attr_t     *attrs;
  double             ts;
  float              trate;
  char               *tmpbuf;  

  fprintf(stderr,"\nTHREADED TCP DATA MOVER:\n");

#ifdef IBP_DEBUG
  fprintf(stderr,"\n%s %s %s %s %s %s <%s>\n\n",
     argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
#endif

  if(argc != 7){
    fprintf(stderr,"Usage: %s <target_hostname> <port_Number> <fd> <filesize> <numtargets> <srcCap>",
	      argv[0]);
    exit(TCP_FAIL);
  }

  glb.sourcefd = atoi(argv[3]);  
  glb.filesize = atol(argv[4]);
  numtargets = atoi(argv[5]);

  timeout.ServerSync = 100;
  timeout.ClientTimeout = 200;

  targets = String2Array(argv[1],numtargets,MAX_HOST_LEN);
  ports = String2Array(argv[2],numtargets,MAX_WORD_LEN);
  
  t_args = (TDTSP *) malloc(sizeof(TDTSP) * numtargets);
  threads = (pthread_t *) malloc(sizeof(pthread_t)*numtargets);
  attrs = (pthread_attr_t *) malloc(sizeof(pthread_attr_t)*numtargets);

  
  /* ------- strip src capability ---------- */
  strcpy(glb.IBP_cap,argv[6]);
  strcpy(filename,GetTargetKey(argv[6]));

  /* --- initialize the buffers for the threads --- */
  glb.position = lseek(glb.sourcefd,0,SEEK_CUR);
  
  /* --- initialize the sockets for targets --- */
  targetsockets = (int *)malloc(sizeof(int)*numtargets);
  for(i=0;i<numtargets;i++){
    if((targetsockets[i] = tcp_connect(targets[i],ports[i]))<0){
      fprintf(stderr,"connectfd error: host %d failed",i);
      perror("");
      exit(TCP_FAIL);
    }
  }

  ts = seconds();  
  /* ------- set up & create threads ------- */

  glb.exitval=1;

  while(nread < glb.filesize){
    readsize = ((glb.filesize-nread)<(MAXBUFFLEN) ? (glb.filesize-nread):(MAXBUFFLEN));
    bzero(glb.mainbuffer,MAXBUFFLEN); 
    offset += ns;
    ns = IBP_load(glb.IBP_cap, &timeout, glb.mainbuffer, readsize, offset);
    if(ns == 0){
      perror("MAIN: error reading from FILE");
      exit(TCP_FAIL);
    }

    for(i=0;i<numtargets;i++){
      t_args[i] = NewTDTS(filename, i, ns, targetsockets[i]);
      if(t_args[i] == NULL)
         exit(TCP_FAIL);    
      pthread_attr_init(attrs+i);
      pthread_attr_setscope(attrs+i, PTHREAD_SCOPE_SYSTEM);
      pthread_create(threads+i, attrs+i, (void *)WriteDataToSocket, t_args[i]);
      if(glb.exitval != 1){
	exit(TCP_FAIL);
      }
    }
  
    for (i = 0; i < numtargets; i++) {
      if(pthread_join(threads[i], &retval) != 0)
      {
         perror("join error \n");
         exit(TCP_FAIL);
      }
    }
   
   for(i=0;i < numtargets;i++)
   {
     /*close(t_args[i]->filefd);*/
     free(t_args[i]);
  }
    nread += ns;
  }

  tmpbuf=(char *)malloc(sizeof(char) * 4);
  tmpbuf[3]='\0';

  for(i=0; i<numtargets; i++){
      fin = read(targetsockets[i], tmpbuf, 3);
      if(fin > 0){      
	if(!strncmp(tmpbuf,"FIN",3))
        {
#ifdef IBP_DEBUG
	  fprintf(stderr,"End of data transfer host #%d\n",i);
#endif
          glb.exitval=TCP_OK;
        }
	else{
	  glb.exitval = TCP_FAIL;
	  fprintf(stderr,"Problem closing connection with target #%d",i);
	  goto POINT;
	}
      }
      else{
	fprintf(stderr,"Cannot end transmission properly, SendData failed for target #%d\n",i);
	glb.exitval = TCP_FAIL;
	goto POINT;
      }
  }

#ifdef IBP_DEBUG
  fprintf(stderr,"All transfers done.\n");
#endif

  ts = seconds()-ts;     
  trate = (ts !=0) ? glb.filesize/(1024.0*1024*(ts)) : 0;      
  fprintf(stderr,"\nTransfer t-time = %6.7f secs ",ts);
  fprintf(stderr," size = %d   xfer trate = %.2f MB/s (%.2fMb/s)\n",
	  glb.filesize, trate, trate*8);

 POINT:
  for(i=0;i<numtargets;i++)
     close(targetsockets[i]);
  free(t_args);
  free(targets);
  free(ports);

  /*  exit (TCP_OK);*/
  exit(glb.exitval);
}











