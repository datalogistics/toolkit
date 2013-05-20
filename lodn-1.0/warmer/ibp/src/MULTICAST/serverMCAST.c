/* before making changes to the program on 8th Jan */
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

#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif
#include <sys/ioctl.h> 
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>  
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include "ibp_ClientLib.h"


#define MAXBUF 8192
#define HDR 10
#define MAXLOAD (MAXBUF -HDR)

#define MAX_CAP_LEN   2048

char group[20];

double seconds()
{
  struct timeval ru;
  gettimeofday(&ru, (struct timezone *)0);
  return(ru.tv_sec + ((double)ru.tv_usec)/1000000);
}



typedef struct{
  unsigned long int filesize;          /* bytes to send */
  int  sourcefd;                       /* general srcfd for starting position */
  int  IBP_key;
  char IBP_cap[MAX_CAP_LEN];
  char IBP_filename[MAX_CAP_LEN];
} GLOBALS;
   
GLOBALS glb;

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

  struct ip_mreq mreq;
  struct ifreq ifreq;

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
  int sockfd,n;
  struct addrinfo hints,*res,*ressave;
 
  bzero(&hints,sizeof(struct addrinfo));
  hints.ai_family=AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  if((n=getaddrinfo(host,serv,&hints,&res)) != 0)
    printf("udp_client error \n");

  ressave= res;
  
  if((sockfd=socket(res->ai_family,res->ai_socktype,res->ai_protocol)) < 0) {
    printf("socket creation failed \n");
    return(-1);
  }
  
  if(res == NULL)
    printf("udp_client error for %s, %s",host,serv);

  *saptr = malloc(res->ai_addrlen);
  if(*saptr == NULL)
     printf("malloc failed\n");
 
  memcpy(*saptr,res->ai_addr,res->ai_addrlen);
  *lenp = res->ai_addrlen;
  
  freeaddrinfo(ressave);
  return sockfd;
}

int wsize= 0;

time_t start1;
void wake(int j)
{
   fprintf(stderr,"<<@@@@@@@@@@@@ in wake size %d>> \n",wsize);
/*//   if (time(NULL) - start1 > 1*60)*/
   {
      exit(-1);
   }
 /*//  signal(SIGALRM, wake);*/
 /*//  ualarm(10000000, 0);*/

}


 
int ReceiveData(int recvfd,socklen_t salen)
{
  ssize_t n;
  char buf[MAXBUF]; 
  int retwrite;
  int ooffset;
  int size;
  int filesize;
  int load,remsize;
  int retread;
  struct sockaddr *sa;
  socklen_t len;
  struct ibp_timer timeout;
  double temptim;
  int i;
  char hdr[8];
  int this;

  start1 = time(NULL);
  
  timeout.ServerSync = 100;
  timeout.ClientTimeout = 300;

  ooffset = lseek(glb.sourcefd,0,SEEK_CUR);
   
  size=0;
  filesize=glb.filesize;
  remsize=glb.filesize;
  sa=malloc(salen);

  i=0;  
  temptim = seconds();
  printf("--in here 2 \n");
  while(size < filesize)
  {
     load = (remsize > MAXLOAD ? MAXLOAD:remsize);
     do
     { 
        signal(SIGALRM,wake);
        ualarm(10000000,0);
        retread=recvfrom(recvfd,buf,load+HDR,0,sa,&len);
         printf("time %6.2f ",(seconds()-temptim));
        /*//printf("retread %d \n",retread);*/
        if(retread < 0)
        {
          if(errno != EINTR)
          {
            fprintf(stderr,"recvfrom failed \n");
            return -1;  
          }
          else 
            fprintf(stderr,"errno %d \n",errno);
        }
        ualarm(0,0);
     }while(retread < 0);

    strcpy(hdr,strtok(buf," "));
    this=atoi(hdr);
    printf("-->this %d i %d ",this,i);

/*
    lseek(glb.sourcefd, load+ooffset,SEEK_SET);
    retwrite = write(glb.sourcefd,buf,load);
     if(retwrite < 0)
      fprintf(stderr,"write error \n");
 */

       retwrite = IBP_store(glb.IBP_cap,&timeout,buf+HDR,load);
/*//       printf("retwrite %d \n",retwrite);*/
       if(retwrite == 0)
       {
         printf("IBP store failed \n");
         continue;
       }

     printf("time %6.2f secs \n",(seconds()-temptim));
     size += load;
     wsize += load;
     remsize -= load;
     i++;
   /*//  printf("group %s size %d remsize %d retwrite %d \n",group,size,remsize,retwrite);*/
/*//    printf("time %6.2f secs remsize %d i %d\n",(seconds()-temptim),remsize,i);    */
  } 
 return 1;  
   
} 

int main(int argc,char *argv[])
{
 
  int sendfd,recvfd;
  const int on=1;
  socklen_t salen;
  struct sockaddr *sasend,*sarecv;
  int retval;

   
  fprintf(stderr,"\nMULTICAST DATA MOVER : \n%s %s %s %s %s %s\n",
				 argv[0],argv[1],argv[2],argv[3],argv[4],argv[5]);   

  glb.sourcefd = atoi(argv[3]);
  glb.filesize = atoi(argv[4]);
  strcpy(glb.IBP_cap,argv[5]);  
   
  strcpy(group,argv[1]);

  sendfd=udp_client(argv[1],argv[2],(void **)&sasend,&salen);

  setsockopt(sendfd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

  sarecv=malloc(salen);
  memcpy(sarecv,sasend,salen);
  bind(sendfd,sasend,salen);

  mcast_join(sendfd,sasend,salen,NULL,0);
  printf("-->in here 1 \n");  
  retval=ReceiveData(sendfd,salen);
  if(retval < 0)
  {
    printf("abrubtly ended due to error \n");
    mcast_leave(sendfd,sasend,salen);
    exit(3);  
  }
  mcast_leave(sendfd,sasend,salen);
  printf("recieved all data \n");
  exit(0);
}

