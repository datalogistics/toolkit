/*--------------------- UDP File Blaster server ----------------------
   usage: blasterd port [initwindow] 
   function: serve files from the current directory on specified port
   (using specified initial window) 

   (c) 2000 Micah Beck 
       2001 Modified: Erika Fuentes for IBP Data Movers
       2002 Modified:Sharmila Kancherla  
    Do not redistribute in any modified form 

-----------------------------------------------------------------------*/

#include "udplib.h"
#include "ibp_ClientLib.h"

extern int errno;
int sockUDP;                /* socket file desciptor --> for blasterd.c */
struct sockaddr_in name2;   /* this one was in blasterd.c */



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



/* ------------------------------------------- 
   handler function when packet is rejected 
   ------------------------------------------- */
time_t start1;
int tmpi = 0;

#ifndef NOALARM
void kick(int i)
{   
    int ret;

#ifdef IBP_DEBUG
    fprintf(stderr,"Kicked [%d] \n", nextack); 
#endif

    sprintf(abuf, "kick: next packet expectedAk:%d",nextack);
    if(time(NULL)-start1 > 3*60)
	   exit(3);
    if(tmpi == 1)
    {
      ret =   sendto(sockUDP, abuf, strlen(abuf)+1, 0,(struct sockaddr *)&name2,sizeof(struct sockaddr));
      if(ret < 0)
         perror("Couldn't send kick");
      tmpi = 0;
    }
    signal(SIGALRM,kick);
    ualarm(5000000,0);
}
#endif


void recvtimeout(int i)
{
  fprintf(stderr,"recv timeout occured \n");
  exit(3);
}

/* -------------------------------------------------------------------
   implements the actual data transfer from socket to file and writes 
   to a local file specified by targetfd
   ------------------------------------------------------------------- */

int ReceiveDataUDP(int sourcefd, int targetfd, unsigned long int size, int count)
{
  int                   this;                 /* last packet received */
  char                  buf[MAXBUF];          /* buffer to hold datagram */
  int                   globalSeq=0, Seq=0; 
  char                  hdr[8];               /* store temp incoming header  */
  int                   retwrite;
  int                   ooffset;
  int                   cnt, load ;
  int                   ret;
  char                  tmp[MAXBUF];
  int                   tmpack = -1;
  struct ibp_capstatus  ls_info;
  int                   flag=0;
    
  struct ibp_timer timeout;

  start1= time(NULL);

  timeout.ServerSync = 100;
  timeout.ClientTimeout = 300;

#ifdef IBP_DEBUG
  fprintf(stderr,"count-->%d  total size-->%d    UDPsock-->%d  fd-->%d\n",
	   count,size,sourcefd,targetfd);  /** debug */
#endif

  ooffset = lseek(targetfd,0,SEEK_CUR);

  this = -1;
  nextack = 0;
  while (this != (count-1) || nextack != count+1)   /* start transfering file count # of blocks */
  {
 
    if (nextack != count) 
    {
      load = (nextack != count-1) ? MAXLOAD : (size - MAXLOAD*(count-1));
      do 
      {
        signal(SIGALRM, kick);
        ualarm(500000, 0);
        tmpi=1;   
        cnt=read(sourcefd, buf, load+HDR+HDR2);
        if (cnt < 0 && errno != EINTR)
        {                 /* block & header into buf, add HDR2 to include seq # */  
          perror("Couldn't read datagram");
	  return -1;
        }
        ualarm(0, 0);
      } while (cnt < 0);

      strcpy(hdr,strtok(buf," "));
      this = atoi(hdr);                                      /* 'this'= only content of header: block#  */

      /* ------ write to the client local file ------ */
      Seq = atoi(buf+HDR);
#if 0
#ifndef NODISK
      lseek(targetfd, (this*MAXLOAD)+ooffset, SEEK_SET);
      retwrite = write(targetfd, buf+HDR+HDR2, load);        /* added to include seq #  */
      if(retwrite < 0)
        fprintf(stderr,"write error\n");
#endif
#endif

#ifdef IBP_DEBUG
      if (!(this%10)) fprintf(stderr, "#");
#endif

    }

    if (nextack == count) {sleep(1);}

    if (this == nextack)
    {
       sprintf(abuf, "AA:%d",  (this == count-1) ? count : nextack);

       if(nextack != count) 
       {
        retwrite = IBP_store(glb.IBP_cap, &timeout, buf+HDR+HDR2, load);
        if(retwrite == 0)
        {
          fprintf(stderr,"IBP_store failed errono %d \n",IBP_errno);
          exit(3);
        }
      }
    }
    else if(this > nextack){
      sprintf(abuf, "AN:%d", nextack);
   }
   ret=sendto(sourcefd, abuf, strlen(abuf)+1,0, (struct sockaddr *)&name2,sizeof(struct sockaddr));
   if(ret < 0)
   { 
        perror("Couldn't send datagram");
	return -1;
   }

    if ((this == nextack || this == (count-1)) && nextack < count+1)
    {
       if(this == (count -1) && nextack < this)
          nextack = nextack;
       else
	 nextack += 1;
    }
    globalSeq++;
  }  /* end of main while */

  return 1;
}



int main(int argc, char *argv[])
{
  int                     listenfd, sockTCP;      /* file descriptor into TCP, UDP  */
  int                     count;                  /* buffers to transfer */
  int                     size;                   /* kilobytes to transfer  */
  int                     fd;                     /* descriptor of local file */
  int                     retval;
  struct sockaddr_in sad, cad;                    /* addresses  */
  struct protoent         *ptrp;                  /* pointer to a protocol table entry */
  unsigned long           alen;
  char                    data[MAXBUF];
  char                    localhost[MAX_HOST_LEN];
  float                   initwindow;             /* max packets in flight */
  int                     ret;
  char                    tmpbuff[6];

  if(argc != 5){
  fprintf(stderr,"Usage: %s <local_port> <targfile_descriptor><targfile_size>,<IBP_cap>",argv[0]);
    exit(3);  
  }

  fprintf(stderr,"\nUDP server DataMover Running \n");   

#ifdef IBP_DEBUG
  fprintf(stderr,"\n arg1 %s arg2 %s  arg3 %d arg4 %s\n", 
         argv[1],argv[2],atol(argv[3]),argv[4]); 
#endif

  /* -------------  create TCP socket --------------- */
  listenfd = tcp_listen(argv[1]);              /* server listening on port  */
#ifdef IBP_DEBUG
  fprintf(stderr,"Expecting client to connect on port %s...\n<<%s>>\n",argv[1],argv[4]); 
#endif

  if(gethostname(localhost,MAX_HOST_LEN)<0)
    sys_error("\ngethostname: error trying to get name\n");

  sockTCP = tcp_accept(listenfd);            /* do the connect/accept for a client */
  if(sockTCP < 0)
  {
    perror("blasterd:Problem trying to connect from client");
    exit(3);
  }

  /* ------------ create a UDP socket -------------- */
  sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
  if(sockUDP < 0) {
    perror("Opening datagram socket");
    exit(3);  
  }

  bzero((char *)&name2, sizeof(name2));
  name2.sin_family = AF_INET;
  name2.sin_addr.s_addr = htonl(INADDR_ANY);
  name2.sin_port = 0;

  if(bind (sockUDP, (struct sockaddr *)&name2,sizeof(name2)) < 0) {
   perror("Error binding datagram socket \n");
    exit(3);
  }
  /* ----------------------------------------------- */

  length = sizeof(name2);
  if (getsockname(sockUDP, (struct sockaddr *)&name2,(socklen_t *) &length) < 0) {
    perror("Error getting socketname \n");
    exit(3);
  }
  

  if (send(sockTCP, &name2.sin_port, sizeof(name2.sin_port), 0) < 0)
  {
     perror("Sending UDP port number message");
     exit(3); 
  }

  initwindow = (argc == 5) ?  atoi(argv[4]) : MINWIN;
  
  signal(SIGALRM,recvtimeout);
  alarm(10);
  if (recvfrom(sockUDP, data, MAXBUF, 0, (struct sockaddr *)&name2,(socklen_t *) &length) < 0) 
  {
    perror("Error receiving filename");
    exit(3);
  } 
  alarm(0);

  fd = atoi(argv[2]);
  size = atol(argv[3]);
  glb.filesize = size;
  strcpy(glb.IBP_cap,argv[4]);

  
  count = ((size-1) / MAXLOAD) + 1;
  sprintf(data, "%d", size);
  

  ret=send(sockTCP,data,MAXBUF,0);
  if(ret < 0)
  { 
     perror("Sending file size"); 
     exit(3);
  }

#ifdef IBP_DEBUG
  fprintf(stderr,"fd=%d  sockUDP=%d size=%ld  count=%d  initwindow=%f\n",
      fd,sockUDP,size,count,initwindow); 
#endif

  retval = ReceiveDataUDP(sockUDP,fd,size,count);
  close(sockTCP);

  exit(0);
}

