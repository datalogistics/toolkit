#include "config-ibp.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <ctype.h>

#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "ibp_ClientLib.h"

extern int errno;

#define HDR 10
#define MAX_CAP_LEN   2048
#define MAX_KEY_LEN   256
#define MAXBUF 40960
#define MAXLINE 40960
#define MAXLOAD (MAXBUF - HDR)



typedef struct{
  unsigned long int filesize;          /* bytes to send */
  int               sourcefd;                       /* general srcfd for starting position */
  int               IBP_key;
  char              IBP_cap[MAX_CAP_LEN];
  char              IBP_filename[MAX_CAP_LEN];
} GLOBALS;

GLOBALS glb;

/*
int dg_cli(int sockfd,const struct sockaddr *pservaddr,socklen_t servlen,unsigned long int 
filesize,int filefd);
*/


int dg_cli(int sockfd,const struct sockaddr *pservaddr,socklen_t servlen,unsigned long int filesize);

double seconds()
{
  struct timeval ru;
  gettimeofday(&ru, (struct timezone *)0);
  return(ru.tv_sec + ((double)ru.tv_usec)/1000000);
}



int main(int argc,char *argv[])
{
  
   int             sockfd,filefd;
   struct          sockaddr_in servaddr,cliaddr;
   char            filename[MAX_CAP_LEN];
   double          s1,s2,total;  
   int             retval;
   float           rate;
   unsigned char   ttl = 225;
   struct hostent  *h;

  fprintf(stderr,"\nMulticast client Datamover Running \n");


#ifdef IBP_DEBUG
  fprintf(stderr,"\n%s %s %s %s %s %s \n\n",
      argv[0],argv[1],argv[2],argv[3],argv[4],argv[5]);
#endif
  
   h = gethostbyname(argv[1]);
   if(h==NULL) {
     printf("%s: unknown host '%s'\n",argv[0],argv[1]);
     exit(1);
   }
  
   sockfd=socket(AF_INET,SOCK_DGRAM,0);
   if(sockfd < 0){
     perror("cannot open socket ");
     exit(1);
   }
   
   servaddr.sin_family = h->h_addrtype;
   memcpy((char *) &servaddr.sin_addr.s_addr,h->h_addr_list[0],h->h_length);
   servaddr.sin_port = htons((u_short)atoi(argv[2]));

#if 0    
   bzero(&servaddr,sizeof(servaddr));
   servaddr.sin_family=AF_INET;
  /* servaddr.sin_addr.s_addr=htonl(INADDR_ANY); */
   servaddr.sin_port = htons((u_short)atoi(argv[2]));
  inet_pton(AF_INET,argv[1],&servaddr.sin_addr); 
#endif

   if(!IN_MULTICAST(ntohl(servaddr.sin_addr.s_addr))) {
     printf("given address is not multicast address \n");
     exit(1);
   }

   if(setsockopt(sockfd,IPPROTO_IP,IP_MULTICAST_TTL, &ttl,sizeof(ttl)) < 0) {
     exit(1);
   }


   glb.sourcefd=atoi(argv[3]);
   glb.filesize=atoi(argv[4]);

/*//   strcpy(glb.IBP_filename,GetTargetKey(argv[5]));  */
   strcpy(glb.IBP_cap,argv[5]);

#if 0
  
   strcpy(filename,"/usr/tmp/");
   strcat(filename,glb.IBP_filename);
   
   if((filefd = open(filename,O_RDONLY)) < 0){
     fprintf(stderr, "Problem reading source capability for host <<%s>>:",filename);
     perror(" ");
     /*//return;*/
     exit(3);
   }
#endif

   s1 = seconds();
   retval=dg_cli(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr),glb.filesize);
   s2 = seconds();

  total = s2-s1;
  rate = (total !=0) ? glb.filesize/(1024*1024*(total)) : 0;
  fprintf(stderr,"\nsize = %d, time=%6.2f  xfer rate = %.2f MB/s (%.2fMb/s)\n",
          glb.filesize, total, rate, rate*8);

/*//  close(filefd);*/
  close(sockfd);
/*//  fprintf(stderr,"Transfer Done \n");*/
 
   exit(0);
}

int dg_cli(int sockfd,const struct sockaddr 
*pservaddr,socklen_t servlen,unsigned long int filesize)
{
  int              n;
  char             sendline[MAXLINE],recvline[MAXLINE+1];
  socklen_t        len;
  struct sockaddr  *preply_addr;
  int              size;
  int              load;
  int              remsize;
  int              ooffset,retread,retwrite;
  char             data[MAXBUF];
  struct ibp_timer timeout;
  int              offset;
  double           s1;
  int              i=0;

  ooffset=lseek(glb.sourcefd,0,SEEK_CUR);

  timeout.ServerSync = 10;
  timeout.ClientTimeout = 30;


  size=0;
  remsize=filesize;
  offset=0;

  s1=seconds();
  while(size < filesize)
  {

    load= (remsize > MAXLOAD ? MAXLOAD:remsize);
#if 0
    lseek(filefd,size+ooffset,SEEK_SET);
    retread = read(filefd,data,load);
    if(retread < 0)
    {
      fprintf(stderr,"read failed \n");
      continue;
    }
#endif
    
   sprintf(data,"%08d ",i);

   
    retread=IBP_load(glb.IBP_cap,&timeout,data+HDR,load,offset);
    if(retread <= 0)
    {
       fprintf(stderr,"load error \n");
       exit(3);
    }
    
    offset += retread;
   
    
   retwrite=sendto(sockfd,data,load+HDR,0,pservaddr,servlen);
   
   i++;
   if(retwrite < 0 ) 
   {
        fprintf(stderr,"Sending datagram message %d \n",errno);
        continue;
   }

    size += retread;
    remsize -= retread;
  }
  return 1;
}

