/* --------------- UDP File Blaster client ------------------
   usage: blaster host port remotefile [localfile] 
   function: get blast://host:port/remotefile (to localfile) 
   (c) 2000 Micah Beck 
       2001 Modified:Erika Fuentes for IBP Data Movers
       2002 Modified:Sharmila Kancherla
   Do not redistribute in any modified form
    
   ---------------------------------------------------------- */

#include "udplib.h"
#include <pthread.h>

#include "ibp_ClientLib.h"

extern int errno;

/* -----------------------------------------
   data structures for globals and threads
   ----------------------------------------- */
typedef struct{
  unsigned long int filesize;          /* bytes to send */
  int  count;                          /* number of blocks */
  int  sourcefd;                       /* general srcfd for starting position */
  int  IBP_port;
  char IBP_depot[MAX_HOST_LEN];
  int  IBP_key;
  char IBP_cap[MAX_CAP_LEN];
  char IBP_filename[MAX_CAP_LEN];
  unsigned long *tlength;              /* length of address */
  float initwindow;                    /* initial window size */
} GLOBALS;

GLOBALS glb;


typedef struct{
  char  port[8];                       /* port of the blaster */
  char  target[MAX_HOST_LEN];          /* target for each thread */
  int   filefd;                        /* filedescriptor for each thread*/
  int   udpsock;
  int   tcpsock;
  int   index;                         /* index for arrays */
} UDTS;                                /* Thread Data Transfer Structure */

typedef UDTS* UDTSP;

struct sockaddr_in *names1, *q;        /* Address of the server  */


/* -----------------------------------------------------------
   transfers data from source local file to socket descriptor
   ----------------------------------------------------------- */

/* hopefully this will kill children who live for more than 5 min. */

time_t start1;

#ifndef NOALARM
void wake(int j) 
{ 
   fprintf(stderr,"<<@@@@@@@@@@@@ in wake>> \n");
   if (time(NULL) - start1 > 3*60) 
   {
      exit(3); 
   }
   signal(SIGALRM, wake);
   ualarm(10000000, 0);
		
}
#endif


void recvtimeout(int i)
{
  fprintf(stderr,"timeout occured \n");
  exit(3);
}

int SendDataUDPT(int srcfd, int targfd, unsigned long int size, int count,float initwindow, int num)
{
  int                     lastack;             /* last packet acknowledged */
  int                     i;
  ssize_t                 cnt;    
  int                     resends = 0;
  int                     thisack;  
  int                     rflag = 0;
  int                     wintot = 0;
  int                     globalSeq=0;
  int                     retread=0;
  char                    data[MAXBUF], adata[MAXBUF];
  char                    action[5];
  float                   window;            /* packets in flight */
  int                     offset;
  struct ibp_timer        timeout;  
  int                     ooffset;
  int                     load;
  int                     sflag;
  long                    tlength;
  char                    *kickbuf;
  int                     kickack;
  int                     kflag = 0;

  start1 = time(NULL);


  lastack = -1;
  i = 0;
  globalSeq = 0;
  strcpy(action,"send\0");
  window = initwindow;
  offset=0;
  sflag=0;
  ooffset = lseek(srcfd,0,SEEK_CUR);                      /* get position within file (header)  */


  timeout.ServerSync = 10;
  timeout.ClientTimeout = 30;

  for (i=0; i < (count-1) || lastack < count; )           /* <------   main loop gets the file  */
  {   
    load = (i != count-1) ? MAXLOAD : (size - MAXLOAD*(count-1));
    if (lastack <= (count-1) && ((i-lastack) <= window))
    {
      sprintf(data, "%08d  %08d", i,globalSeq);           /* adding header seq# to data  */
      if(sflag ==0 )
      {
#ifndef NODISK                                            /* reads from local-server file to buff to send */ 
      lseek(srcfd, (i*MAXLOAD)+ooffset, SEEK_SET);        /* position in the local file */
      retread = read(srcfd, data+HDR+HDR2, load);         /* added HDR2 to include seq # */
      if(retread < 0)
        fprintf(stderr,"read failed \n");      

#endif
      
      retread = IBP_load(glb.IBP_cap, &timeout, data+HDR+HDR2, load, offset);
      if(retread <= 0)
        fprintf(stderr,"load error \n");
      offset += retread;
      if(i == count-1)
        sflag=1;    

      if (sendto(targfd, data, load+HDR+HDR2, 0, 
		 (struct sockaddr *)&(names1[num]), sizeof(names1[num])) < 0)
          perror("Sending datagram message error");

     }
       if (i < (count-1)) i++; 

    }
   if (((i-lastack) > window) || (i == (count-1) && lastack < count))
   {
      do {
        signal(SIGALRM, wake);
        ualarm(10000000, 0);
        tlength = sizeof(names1[num]);
        cnt = recvfrom(targfd,adata,MAXBUF,0,(struct sockaddr *)&(names1[num]),(socklen_t *)(&tlength));
        if (cnt < 0 && errno != EINTR)
        { 
           perror("Error receving ack data from client");     
           exit(3);
        }
        ualarm(0, 0);
      } while (cnt < 0);

      wintot += window;

      if(adata[2] == ':')
      {
          thisack = atoi(adata+3);
      }
      else if(adata[2] != ':')
      {
        kickbuf = (char *)malloc(sizeof(char) * 10);
        kickbuf = strrchr(adata,':');
        kickack = atoi(kickbuf+1);
        kflag = 1;
        sflag = 0;
        if(lastack == -1)
          thisack = 0;
        else
          thisack = lastack;
      }

      if (adata[1] != 'A')
      {
          if (!rflag || adata[1] != 'N') { 
       /*   if (adata[1] == 'N') fprintf(stderr, "(%.0f)", window);
	  else fprintf(stderr, "[%.0f]", window);
       */
          resends += i - thisack + 1;
          if(kflag == 1)
          {
            i = kickack;
            kflag = 0;
          }
          else
            i = thisack;                          /* <-------- i modified      */

	  lastack = thisack;                    /* - window + 1; */
          offset=i * MAXLOAD;

	  window *= WINDOW_DOWN;
	  if (window < MINWIN) window = MINWIN;

	  if (adata[1] == 'N') rflag = 1; else rflag = 0;
        } 
      } 
      else {
        if (lastack <= thisack) 
        {
          window += WINDOW_UP;
	  lastack = thisack;
	}
        rflag = 0;
      }
    }  
    globalSeq++;
  }   /* end of for transfering file */

#ifdef IBP_DEBUG
  fprintf(stderr,"\ninit window=%.0f, avg = %0.f, final=%.0f, resends=%.0f%%\n", 
	  initwindow, wintot/(float)count, window, resends*100/(float)count);
#endif

  return 1;
}



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


/* -----------------------------------------------------------
   initialize and fill the data structure for thread
   ----------------------------------------------------------- */
UDTSP NewUDTS(char *target, char *port, int udpsd, int tcpsd, int initwindow, int num,char *path)
{
  UDTSP  u;
  int    filefd;
  char   filename[MAX_CAP_LEN];

  u = (UDTSP)malloc(sizeof(UDTS));
  if(u == NULL){
    fprintf(stderr,"Problem allocating data for host %d:",num);
    perror(" ");
    return NULL;
  }
  strcpy(u->target, target);
  strcpy(u->port, port);
/*  strcpy(filename,"/tmp/"); */

  strcpy(filename,path);
  strcat(filename,glb.IBP_filename);
  if((filefd = open(filename,O_RDONLY)) < 0){
    fprintf(stderr, "Problem reading source capability for host %d <<%s>>:",num,filename);
    perror(" ");
    return NULL;
  }
  u->filefd = filefd;
  u->udpsock = udpsd;
  u->tcpsock = tcpsd;
  u->index = num;
 
  return u;
}



/* --------------------------------------------------------------------
   this function does all the work for each thread and calls SendData
   -------------------------------------------------------------------- */
ThreadTransfer(UDTS *args)
{
  int               udpsock, tcpsock, i, retval;
  double            s1,s2,total;  
  char              buf[MAXBUF];            /* buffer to hold datagram */
  float             rate;
  int               count;                  /* buffers to send */
  unsigned long int size;        /* bytes to send */
  char              tmpbuff[MAXBUF];
  int               ret; 

  sprintf(tmpbuff,"%d",glb.sourcefd);
  i = args->index;

  /* ----------------- get the server port ------------------ */
  signal(SIGALRM,recvtimeout);
  alarm(10);
  retval=recv(args->tcpsock, &(names1[i].sin_port), sizeof(names1[i].sin_port), 0);
  alarm(0);

  ret = sendto(args->udpsock, tmpbuff, strlen(tmpbuff)+1, 0, 
	     (struct sockaddr *)&(names1[i]), sizeof(names1[i]));
  if(ret < 0) 
  {
       perror("Couldn't send filename");
       exit(3);
  }

  /***glb.initwindow = (argc == 6) ?  atoi(argv[5]) : MINWIN;            decide initwindow */

  /* --- get file size needs to know how many blocks to transfer --- */
  signal(SIGALRM,recvtimeout);
  alarm(10);
  retval=recv(args->tcpsock,buf,MAXBUF,0);
  if(retval < 0)
  {
    perror("Couldn't read file size");
    exit(3);
  }
#if 0
  if ((read(args->tcpsock, buf, MAXBUF*sizeof(char)) < 0))
  {
      perror("Couldn't read file size");
      exit(3);
  }
#endif
  alarm(0);

#ifdef IBP_DEBUG
  fprintf(stderr,"count-->%d  total size-->%ld    UDPsock-->%d  fd-->%d initwindow=%f\n",
	   glb.count,glb.filesize,args->udpsock,args->filefd,glb.initwindow);    
#endif

  s1 = seconds();
  retval = SendDataUDPT(args->filefd,args->udpsock,glb.filesize,glb.count,glb.initwindow, i);      
  s2 = seconds();

  total = s2-s1;
  rate = (total !=0) ? glb.filesize/(1024.0*1024*(total)) : 0;
  fprintf(stderr,"\nsize = %d, time=%6.2f  xfer rate = %.2f MB/s (%.2fMb/s)\n",
	  glb.filesize, total, rate, rate*8);

  close(args->filefd);
}


/* ----------------------------------------------------------------------------
      MAIN: sets up tcp/udp sockets, thread initialization, calls send data
   ---------------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
  int                     *TCPsock, *UDPsock;     /* file descriptors for sockets */
  struct sockaddr_in      *sads, *p, *pp;         /* Address of the server */
  struct hostent          **hp;
  struct protoent         *ptrp;                  /* pointer to a protocol table entry */ 
  int                     port;                   /* IBP server port */
  int                     i,j, nthreads;
  char                    *slash;
  float                   initwindow;              /* max packets in flight */
  char                    **targets, **ports; 
  pthread_t               *threads;
  pthread_attr_t          *attrs;
  void                    *retval;
  UDTSP                   *t_args;

  fprintf(stderr,"\n Threaded UDP Datamover client Running:\n");

#ifdef IBP_DEBUG
  fprintf(stderr,"\n%s %s %s %s %s %s %s\n\n",
      argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
#endif
 
  if(argc !=8){
    fprintf(stderr,"Incorrect parameters: %s <target_hostnames> <port_Numbers> <file_descriptor> <filesize> <numtargets> <filename> [initwindow] < path > \n",argv[0]); 
    exit(3);
  }

  glb.sourcefd = atoi(argv[3]);
  glb.filesize = atoi(argv[4]);
  glb.count = ((glb.filesize-1) / MAXLOAD) + 1;
  nthreads = atoi(argv[5]);
  glb.initwindow = MINWIN;

  targets = String2Array(argv[1], nthreads, MAX_HOST_LEN);
  ports = String2Array(argv[2],nthreads,MAX_WORD_LEN);

  /* ------- strip src capability ---------- */
  strcpy(glb.IBP_cap,argv[6]);
  strcpy(glb.IBP_filename,GetTargetKey(argv[6]));
  strcpy(glb.IBP_depot,GetTargetHost(argv[6]));
  glb.IBP_port = GetTargetPort(argv[6]);
  glb.IBP_key = GetTypeKey(argv[6]);

  /* ----------------- allocate memory for arrays --------------------- */
  threads = (pthread_t *) malloc(sizeof(pthread_t) * nthreads);
  attrs = (pthread_attr_t *) malloc(sizeof(pthread_attr_t) * nthreads);
  UDPsock = (int *)malloc(sizeof(int) * nthreads);
  TCPsock = (int *)malloc(sizeof(int) * nthreads);
  t_args = (UDTSP *)malloc(sizeof(UDTSP) * nthreads);
  sads = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in) * nthreads);
  names1 = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in) * nthreads);
  hp = (struct hostent **)malloc(sizeof(struct hostent *) * nthreads);
  glb.tlength = (unsigned long *)malloc(sizeof(unsigned long) * nthreads);

  /* ------- Map TCP transport protocol name to protocol number ------- */
  if (((int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "cannot map \"tcp\" to protocol number");
    exit(3);
  }
  

  /* ------------ loop initializes thread global data ----------------- */
  for(i=0,p=sads, q=names1; i<nthreads; i++,p++,q++){  
    hp[i] = gethostbyname(targets[i]);
    if(hp[i] == 0) {
      fprintf(stderr, " %s: unknown host",targets[i]);
      fprintf(stderr,"Error h_errno %d\n", h_errno);
      exit(3);
    }
    
    TCPsock[i] = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);    /* TCP sockets */
    if (TCPsock[i] < 0) {
      fprintf(stderr, "Socket creation failed for host: %d\n",i);
      exit(3);
    }

    bzero((char *)p, sizeof(*p));                                /* Fill server TCP address */
    p->sin_family = AF_INET;
    bcopy((void *)(hp[i]->h_addr),(void *)(&(p->sin_addr)), sizeof(struct in_addr));
    p->sin_port=htons((u_short)atoi(ports[i]));

    /* Connect the socket to the specified server */
    if (connect(TCPsock[i], (struct sockaddr *)&(sads[i]), sizeof(sads[i])) < 0) {
      perror("connect failed");
      exit(3);
    }

  /*  fprintf(stderr,"after connect \n");*/
    UDPsock[i] = socket(AF_INET, SOCK_DGRAM, 0);         /* UDP sockets */    
    if(UDPsock[i] < 0) {
      perror("Opening datagram socket");
      exit(3);
    }
    
    bzero((char *)&(names1[i]), sizeof(names1[i]));
    names1[i].sin_family = AF_INET;
    names1[i].sin_addr.s_addr = sads[i].sin_addr.s_addr;
    names1[i].sin_port = 0;

    glb.tlength[i] = sizeof(names1[i]);    
  }

  /* ------------ loop creates threads ---------------------- */
  for(i=0;i<nthreads;i++){
    t_args[i] = NewUDTS(targets[i], ports[i], UDPsock[i], TCPsock[i], glb.initwindow,i,argv[7]);
    pthread_attr_init(attrs+i);
    pthread_attr_setscope(attrs+i, PTHREAD_SCOPE_SYSTEM);
    pthread_create(threads+i, attrs+i, (void *)ThreadTransfer, t_args[i]);
  }

  for (j = 0; j < nthreads; j++) {
    pthread_join(threads[j], &retval);
  }
/*  printf("All transfers done.\n"); */

  for(i=0;i<nthreads;i++)
    close(TCPsock[i]);
 
  free(t_args);
  free(ports);
  free(targets);

  exit(0);
}












