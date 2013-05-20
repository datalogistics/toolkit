#include "config-ibp.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>   
#ifdef HAVE_MALLOC_H
#include <malloc.h>  
#endif
#include <errno.h>   
#include <unistd.h>
#include <ctype.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>

#include <sys/ioctl.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>  
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include "ibp_ClientLib.h"
#include <pthread.h>

#define MAXBUF 40960
#define HDR 10
#define MAXLOAD (MAXBUF-10)

#define MAX_CAP_LEN   2048

#define NUMTHREADS 2

char group[20];

double seconds()
{
  struct timeval ru;
  gettimeofday(&ru, (struct timezone *)0);
  return(ru.tv_sec + ((double)ru.tv_usec)/1000000);
}


typedef struct{
  unsigned long int  filesize;          /* bytes to send */
  int                sourcefd;          /* general srcfd for starting position */
  int                IBP_key;
  char               IBP_cap[MAX_CAP_LEN];
  char               IBP_filename[MAX_CAP_LEN];
} GLOBALS;
   
GLOBALS glb;


typedef struct
{
  int             id;
  pthread_mutex_t *lock;
}TARGS;


int        sfd;
socklen_t  salen;
int        remsize ;
int        wsize=0;
int        gc=0;
int        ooffset;
int        lastpkt=0;
int        recievelength=0;
int        lockstatus=0;
int        rsize;
int        this=0;


int mcast_leave(int sockfd, const struct sockaddr *sa, socklen_t salen)
{

     struct ip_mreq          mreq;

      memcpy(&mreq.imr_multiaddr,
             &((struct sockaddr_in *) sa)->sin_addr,
               sizeof(struct in_addr));
      mreq.imr_interface.s_addr = htonl(INADDR_ANY);
      return(setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP,
                                                  &mreq, sizeof(mreq)));
}



int mcast_join(int sockfd,const struct sockaddr *sa,socklen_t salen,
               const char *ifname,u_int ifindex)
{   

  struct ip_mreq    mreq;
  struct ifreq      ifreq;

  memcpy(&mreq.imr_multiaddr,&((struct sockaddr_in *)sa)->sin_addr,sizeof(struct in_addr));


  if(ifindex > 0)
  {
     if(if_indextoname(ifindex,ifreq.ifr_name) == NULL) {
      return(-1);
     }
     goto doioctl;
  }else if(ifname != NULL) 
  {
     strncpy(ifreq.ifr_name,ifname,IFNAMSIZ);
 
     doioctl:
         if(ioctl(sockfd,SIOCGIFADDR,&ifreq) < 0)
             return(-1);
     memcpy(&mreq.imr_interface,&((struct sockaddr_in *)
          &ifreq.ifr_addr)->sin_addr,sizeof(struct in_addr));
  }else
     mreq.imr_interface.s_addr = htonl(INADDR_ANY);
 
  return(setsockopt(sockfd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)));
}


int mcast_set_loop(int sockfd,int onoff)
{
  u_char flag;  
   
  flag=onoff;

  return(setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_LOOP, &flag,sizeof(flag)));
    
}

int udp_client(const char *host,const char *serv,void **saptr,socklen_t *lenp)
{
  int     sockfd,n;
  struct  addrinfo hints,*res,*ressave;
 
  bzero(&hints,sizeof(struct addrinfo));
  hints.ai_family=AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if((n=getaddrinfo(host,serv,&hints,&res)) != 0)
    fprintf(stderr,"udp_client error \n");

  ressave= res;
  
 do{
     sockfd=socket(res->ai_family,res->ai_socktype,res->ai_protocol);
     if(sockfd >= 0)
       break;
  }while( (res = res->ai_next) != NULL);
  
  if(res == NULL)
    fprintf(stderr,"udp_client error for %s, %s",host,serv);

  *saptr = malloc(res->ai_addrlen);
  if(*saptr == NULL)
     fprintf(stderr,"malloc failed\n");
 
  memcpy(*saptr,res->ai_addr,res->ai_addrlen);
  *lenp = res->ai_addrlen;
  
  freeaddrinfo(ressave);
  return sockfd;
}

void wake(int j)
{ 
   fprintf(stderr,"<<@@@@@@@@@@@@ in wake size %d>> \n",wsize);
   
   exit(-1);
}



/*//int RecieveData(int *i)*/
int RecieveData(TARGS *t)
{
  int               j;
  struct            ibp_timer timeout;
  int               load;
  char              buf[MAXBUF];
  struct sockaddr   *sa;
  socklen_t         len;
  int               retread;
  char hdr[8];
  int               retwrite;
  int               nopkts;


  timeout.ServerSync = 10;
  timeout.ClientTimeout = 30;
  
  if(glb.filesize < MAXLOAD)
    nopkts = 1;
  else
    nopkts=glb.filesize/MAXLOAD ;
  
#ifdef IBP_DEBUG
  fprintf(stderr,"nopkts %d \n",nopkts);
#endif

  sa=malloc(salen);
  while(1)
  {
     pthread_mutex_lock(t->lock);
     if(lockstatus == 0 )
     {            
        if(recievelength < glb.filesize && this < nopkts)
        { 
           lockstatus = 1;
           load= (rsize > MAXLOAD ? MAXLOAD:rsize);
           /*//fprintf(stderr,"---a)%d load %d this %d remsize %d recievelength %d nopkts %d\n",t->id,load,this,remsize,recievelength,nopkts);*/
           do
           {
              signal(SIGALRM,wake);
              ualarm(10000000,0);
              retread=recvfrom(sfd,buf,load+HDR,0,sa,&len);
              recievelength += retread -10;
              rsize -= retread -10 ;
              if(retread < 0)
              {
                if(errno!= EINTR)
                {
                   fprintf(stderr,"recvfrom failed \n");
                   return -1;
                }
                else
                   fprintf(stderr,"errno %d \n",errno);
              }
              ualarm(0,0);
           }while(retread < 0);    
           lockstatus = 0; 
           strcpy(hdr,strtok(buf," "));
           this=atoi(hdr);
           pthread_mutex_unlock(t->lock);

#ifdef IBP_DEBUG
           fprintf(stderr,"---b)%d this %d gc %d load %d\n",t->id,this,gc,load);
#endif
           
            retwrite = IBP_store(glb.IBP_cap,&timeout,buf+HDR,load);
   	    /*// printf("retwrite %d \n",retwrite);*/
    	    if(retwrite == 0)
    	    {
       		fprintf(stderr,"IBP store failed \n");
       		continue;
    	    }
	  
#if 0
           lseek(glb.sourcefd, load+ooffset,SEEK_SET);
           retwrite = write(glb.sourcefd,buf,load);
           if(retwrite < 0)
             fprintf(stderr,"write error \n");
           
#endif

           if(pthread_mutex_lock(t->lock) != 0)
           {
       	      fprintf(stderr,"mutex lock failed \n");
              exit(1);
           }
           gc++;
           wsize += retwrite;
           remsize -= retwrite;
           
           /*//printf("---b)%d write %d gc %d rsize %d remsize %d\n",t->id,wsize,gc,rsize,remsize);*/
           if(pthread_mutex_unlock(t->lock) != 0)
           {
             fprintf(stderr,"mutex unlock failed \n");
             exit(1);
           }     
        }
        else
        {
           pthread_mutex_unlock(t->lock);
           /*//printf("before return t->id %d \n",t->id);*/
           return 1;
        } 
     }
     else
     {
          pthread_mutex_unlock(t->lock);
     }
  }
}

void *ReadDataFromSocket(void *k)
{
  int   j,*i;
  int   retval;
  TARGS *t;

  t=(TARGS *)k;

  retval=RecieveData(t);

  return NULL;
 
}
 
int main(int argc,char *argv[])
{
  int                  i; 
  int                  *id;
  pthread_t            *threads;
  pthread_attr_t       *attrs;
  void                 *ret;
  const int            on = 1;
  struct sockaddr      *sasend;  
  struct ibp_capstatus ls_info;
  struct ibp_timer     ls_timeout;
  TARGS                *t_args;
  pthread_mutex_t      tlock;
  int                  li_ret;

  fprintf(stderr,"\nMulticast server Datamover running\n");

#ifdef IBP_DEBUG  
  fprintf(stderr,"\n%s %s %s %s %s %s\n",
				 argv[0],argv[1],argv[2],argv[3],argv[4],argv[5]);   
#endif

  glb.sourcefd = atoi(argv[3]);
  glb.filesize = atoi(argv[4]);
  strcpy(glb.IBP_cap,argv[5]);  
  /*//printf("filesize %d \n",glb.filesize);*/
  pthread_mutex_init(&tlock,NULL);

  remsize=glb.filesize;
  rsize=glb.filesize;
  ooffset = lseek(glb.sourcefd,0,SEEK_CUR);

  sfd=udp_client(argv[1],argv[2],(void **)&sasend,&salen);

  setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

  id = (int *)malloc(sizeof(int) * NUMTHREADS);
  t_args = (TARGS *)malloc(sizeof(TARGS) * NUMTHREADS);
  threads = (pthread_t *)malloc(sizeof(pthread_t) * NUMTHREADS);
  attrs = (pthread_attr_t *)malloc(sizeof(pthread_attr_t) * NUMTHREADS);

  bind(sfd,sasend,salen);  
  
  mcast_join(sfd,sasend,salen,NULL,0);


  for(i=0;i<NUMTHREADS;i++)
  {
    t_args[i].id = i;
    t_args[i].lock = &tlock;
   /*// id[i]=i;*/
    pthread_attr_init(attrs+i);
    pthread_attr_setscope(attrs+i,PTHREAD_SCOPE_SYSTEM);
  /*//  pthread_create(threads+i,attrs+i,(void *)ReadDataFromSocket,id+i);*/
    pthread_create(threads+i,attrs+i,(void *)ReadDataFromSocket,t_args+i);
  }
  
  for(i=0;i<NUMTHREADS;i++)
  {  
     if(pthread_join(threads[i], &ret) != 0)
     {
       perror("join error \n");
       exit(1);
     }
  }

  /*//fprintf(stderr,"after successful join \n");*/
  ls_timeout.ServerSync=100;
  ls_timeout.ClientTimeout = 300;

#if 0
  li_ret=IBP_manage(glb.IBP_cap,&ls_timeout,IBP_PROBE,IBP_READCAP, &ls_info);
  if(li_ret < 0) 
    fprintf(stderr,"MANAGE failed! %d \n",errno);
  else
    fprintf(stderr,"\n...IBP manage done \n");  

   if(ls_info.currentSize != glb.filesize)
     printf("currentsize < THESIZE ; \n currentsiz :%d \n",ls_info.currentSize);
   else
     printf("currentsize = THESIZE \n");
#endif

  exit(0);
}


