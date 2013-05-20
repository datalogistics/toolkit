/* UDP File Blaster client */
/* usage: blaster host port remotefile [localfile] */

/* function: get blast://host:port/remotefile (to localfile) */

/* (c) 2000 Micah Beck */
/* Do not redistribute in any modified form */

#include "udplib.h"
#include "ibp_ClientLib.h"

extern int errno;

unsigned long length;       /* length of address */
struct sockaddr_in name1;   /* Address of the server --> this was in blaster.c */
int UDPsock;                /* file descriptor for socket --> for blaster.c */


/* -----------------------------------------
   data structure for globals
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







/* -----------------------------------------------------------
   transfers data from source locla file to socket descriptor
   ----------------------------------------------------------- */

/* hopefully this will kill children who live for more than 5 min. */
time_t start1;
void wake(int j) 
{ 
  if (time(NULL) - start1 > 5*60) exit(3); /** -1 */
  return;
}


int SendDataUDP(int srcfd, int targfd, unsigned long int size, int count,float initwindow)
{
  int lastack;             /* last packet acked */
  int i, cnt;
  int resends = 0;
  int thisack;             /* temporarly here */
  int rflag = 0;
  int wintot = 0;
  int globalSeq=0;
  int retread;
  char data[MAXBUF], adata[MAXBUF];
  char action[5];
  float window;            /* packets in flight */
  int offset=0;
  struct ibp_timer timeout;

  int ooffset;
  start1 = time(NULL);

  lastack = -1;
  i = 0;
  globalSeq = 0;
  strcpy(action,"send\0");
  window = initwindow;
  ooffset = lseek(srcfd,0,SEEK_CUR);                      /* get position within file (header) */

  timeout.ServerSync = 0;
  timeout.ClientTimeout = 10;

  for (i=0; i < (count-1) || lastack < count; )           /* <------   main loop gets the file  */
  {   
    int load;    
    load = (i != count-1) ? MAXLOAD : (size - MAXLOAD*(count-1));

    if (lastack < (count-1) && ((i-lastack) <= window))
    {
      sprintf(data, "%08d  %08d", i,globalSeq);           /* adding header seq# to data */

#ifndef NODISK                                            /* reads from local-server file to buff to send  */
      lseek(srcfd, (i*MAXLOAD)+ooffset, SEEK_SET);        /* position in the local file  */
      retread = read(srcfd, data+HDR+HDR2, load);         /* added HDR2 to include seq #  */

      offset += retread;
      retread = IBP_load(glb.IBP_cap, &timeout, data+HDR+HDR2, load, offset);
#endif

      if (sendto(targfd, data, load+HDR+HDR2, 0, 
		 (struct sockaddr *)&name1, sizeof(name1)) < 0)
          perror("Sending datagram message");

      /* fprintf(stderr, "{s %.0f, %d, %d}\n", window, lastack, i); */

      if (i < (count-1)) i++; 
    }

    if (((i-lastack) > window) || (i == (count-1) && lastack < count))  /*  <---- i think problem here */
    {
      do {
        signal(SIGALRM, wake);
        ualarm(100000, 0);

        if (cnt=recvfrom(targfd,adata,MAXBUF,0,(struct sockaddr *)&name1,&length1)<0 
	    && errno != EINTR){
          perror("Error receving ack data from client");     
          exit(1);
        }
        ualarm(0, 0);
      } while (cnt < 0);

      wintot += window;   
      if(atoi(adata+3)!=0)                      /* added this if to see if it works */
	thisack = atoi(adata+3);                /*  <------ thisack depends on adata, if kicked is 0      */
      else
	thisack = lastack;

      if (adata[1] != 'A')
      {
        if (!rflag || adata[1] != 'N') { 
          if (adata[1] == 'N') fprintf(stderr, "(%.0f)", window);
	  else fprintf(stderr, "[%.0f]", window);

          resends += i - thisack + 1;
          i = thisack;                          /*  <-------- i modified          */
	  lastack = thisack;                    /* - window + 1; */

	  window *= WINDOW_DOWN;
	  if (window < MINWIN) window = MINWIN;
	  
	  if (adata[1] == 'N') rflag = 1; else rflag = 0;
        } 
      } 
      else {
	/*  fprintf(stderr, "(A %.0f,%d,%d,%d)\n", window, lastack, thisack, i); */
        if (!(thisack%10)) fprintf(stderr, "#");

        if (lastack <= thisack) {
          window += WINDOW_UP;
	  lastack = thisack;
	}
        rflag = 0;
      }
    } /* end if */
    globalSeq++;
  }   /* end of for transfering file */

  fprintf(stderr, "\ninit window=%.0f, avg = %0.f, final=%.0f, resends=%.0f%%\n", 
	  initwindow, wintot/(float)count, window, resends*100/(float)count);

  return 1;
}


int main(int argc, char *argv[])
{
  int count;                   /* buffers to send */
  unsigned long int size;      /* bytes to send */
  int fd, TCPsock;
  struct sockaddr client_addr; /* the address of the client */ 
  struct sockaddr_in sad;      /* Address of the server */
  struct hostent *hp;
  struct protoent *ptrp;       /* pointer to a protocol table entry */ 
  char buf[MAXBUF];            /* buffer to hold datagram */
  int port;                    /* server port */
  int i;
  double s1,s2,total;
  char *slash;
  float rate;
  float initwindow;            /* max packets in flight */
  char action[4];              /* denotes the current send or recv */ 
  int retval;
  char *targets;
  int temp_addr;
  char *ports;
  int connectfd;

  fprintf(stderr,"\nSINGLE UDP DATA MOVER:\n%s %s %s %s %s %s <<%s>>\n\n",
	  argv[0],argv[1],argv[2],argv[3],argv[4],argv[5],argv[6]);
 

  if (argc != 7 && argc != 8){
    printf("Usage: %s <target_host> <port remotefile> <srcfile_descriptor> <srcfile_size> <IBP_srccap>\n", 
	   argv[0]);
    exit(3);
  }

  strcpy(glb.IBP_cap,argv[6]);
  targets=argv[1];
  ports=argv[2]; 





  /* Map TCP transport protocol name to protocol number */
  if (((int)(ptrp = getprotobyname("tcp"))) == 0) {
    fprintf(stderr, "cannot map \"tcp\" to protocol number");
    exit(1);
  }
  


/*  TCPsock=socket(AF_INET,SOCK_DGRAM,0); */

  /* ----------- Create a TCP socket --------------- */
  TCPsock = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
  if (TCPsock < 0) {
    fprintf(stderr, "socket creation failed\n");
    exit(1);
  }


  /* Host and port come from input arguments and from server */
  /* Fill in the server's TCP address */
   hp = gethostbyname(argv[1]);
   if(hp == NULL) {
    fprintf(stderr, "%s: unknown host",argv[1]);
    printf("Error h_errno %d\n", h_errno);
    exit(2);
  }



  fprintf(stderr,"--->after gethost\n");
  bzero((char *)&sad, sizeof(sad));
  sad.sin_family = AF_INET;
  bcopy((char *)hp->h_addr, (char *)&sad.sin_addr, hp->h_length);
  sad.sin_port = htons((u_short)atoi(argv[2]));

  /* Connect the socket to the specified server */
  if (connect(TCPsock, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
    perror("connect failed");
    exit(1);
  }


 
   
  fprintf(stderr,"after connect \n");
  /* -------- create a  UDP  socket --------------- */
  UDPsock = socket(AF_INET, SOCK_DGRAM, 0);
  if(UDPsock < 0) {
    perror("Opening datagram socket");
    exit(1);
  }

  bzero((char *)&name1, sizeof(name1));
  name1.sin_family = AF_INET;
  name1.sin_addr.s_addr = sad.sin_addr.s_addr;
  name1.sin_port =/* 0*/htons((u_short)atoi(argv[2]));;

  length1 = sizeof(name1);

  /* ----------------- get the server port ------------------ */
  recv(TCPsock, &name1.sin_port, sizeof(name1.sin_port), 0);

  if (sendto(UDPsock, argv[3], strlen(argv[3])+1, 0, (struct sockaddr *)&name1, sizeof(name1)) < 0) 
       perror("Couldn't send filename");

  initwindow = (argc == 6) ?  atoi(argv[5]) : MINWIN;           /* decide initwindow */

  fd = atoi(argv[3]);
  size = atol(argv[4]);
  count = ((size-1) / MAXLOAD) + 1;                             /* compute # blocks of file */ 

  /* --- get file size needs to know how many blocks to transfer --- */
  if ((read(TCPsock, buf, MAXBUF*sizeof(char)) < 0))
      perror("Couldn't read file size");


  /**fprintf(stderr,"count-->%d  total size-->%ld    UDPsock-->%d  fd-->%d initwindow=%f\n",
	   count,size,UDPsock,fd,initwindow);   debug */

  s1 = seconds();
  retval = SendDataUDP(fd,UDPsock,size,count,initwindow);       /* Send Data through socket */
  s2 = seconds();

  total = s2-s1;
  rate = (total !=0) ? size/(1024.0*1024*(total)) : 0;
  fprintf(stderr,"\nsize = %d, time=%6.2f  xfer rate = %.2f MB/s (%.2fMb/s)\n",
	  size, total, rate, rate*8);
  close(fd);
  exit(0);
}

