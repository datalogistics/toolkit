#include "config-ibp.h"
#ifdef STDC_HEADERS
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_UTSNAME_H
#include <sys/utsname.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "dtmv.h"
#include "ibp_ClientLib.h"
#include "ibp_server.h"


#define DM_SUCCESS 1
#define DM_FAILURE 0

int myPort;
char *hostname;
int IBP_port;
unsigned long int the_filesize;
int exitval;
int IBPport;
char glbExecPath[256];
void sys_error(char *desc)
{ 
  perror(desc);
  exit(-1);
}


/* -------------------------------------------------------------- 
   parse the string received as argument from the IBP/simulator 
   and fill the DMAS structure
   -------------------------------------------------------------- */
Dlist array2Dlist(char **argarray, int argCount)
{
  int i=0,j=0;
  Dlist arglist;
  Anode tempAN;
 
  /**  for(i=0;i<argCount;i++){
    fprintf(stderr,"%d) <<%s>>\n",i,argarray[i]);
    }**/

  arglist = make_dl();
  for(i=0; i<argCount;i++){   
    tempAN = (Anode) malloc(sizeof(struct arg_node));
    tempAN->args = (char**)malloc(sizeof(char*) * ((numTargets*2)+1));
    if(!strcmp(argarray[i],"+s")){                /* source cap node */
      tempAN->type = S;  
      i++;
      tempAN->args[0] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
      strcpy(tempAN->args[0],argarray[i]);
      dl_insert_b(arglist,(void *)tempAN);
      continue;
    } 
    if(!strcmp(argarray[i],"+t")){                /* target caps node */
      tempAN->type = T;  
      i++;
      for(j=0; j<numTargets; j++){  
	tempAN->args[j] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
	strcpy(tempAN->args[j],argarray[i++]);
      }   
      dl_insert_b(arglist,(void *)tempAN);
      i--;
      continue;
    }
    if(!strcmp(argarray[i],"+k")){           
      tempAN->type = K;
      i++;
      tempAN->args[0] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
      strcpy(tempAN->args[0],argarray[i]);  
      dl_insert_b(arglist,(void *)tempAN);
      continue;
    }
    if(!strcmp(argarray[i],"+f")){           
      tempAN->type = F;
      i++;     
      tempAN->args[0] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
      strcpy(tempAN->args[0],argarray[i]);
      dl_insert_b(arglist,(void *)tempAN);
      continue;
    }
    if(!strcmp(argarray[i],"+z")){           
      tempAN->type = Z;
      i++;
      tempAN->args[0] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
      strcpy(tempAN->args[0],argarray[i]);
      dl_insert_b(arglist,(void *)tempAN);
      continue;
    }
    if(!strcmp(argarray[i],"+i")){     
      tempAN->type = I;
      i++;     
      for(j=0; j<(numTargets*2)+1; j++){
	tempAN->args[j] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
	strcpy(tempAN->args[j],argarray[i++]);
      }
      dl_insert_b(arglist,(void *)tempAN);
      i--; 
      continue;
    }
    if(!strcmp(argarray[i],"+n")){          
      i++;
      continue;
    }
    if(!strcmp(argarray[i],"+p")){         
      tempAN->type = P;
      i++;
      for(j=0; j<numTargets; j++){
	tempAN->args[j] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
	strcpy(tempAN->args[j],argarray[i++]);
      }  
      dl_insert_b(arglist,(void *)tempAN);
      i--;
      continue;
    }
    if(!strcmp(argarray[i],"+o")){           
      tempAN->type = O;  
      i++;
      for(j=0; j<numTargets; j++){  
	tempAN->args[j] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1)); 
	strcpy(tempAN->args[j],argarray[i++]); 
      }
      dl_insert_b(arglist,(void *)tempAN);
      i--;  
      continue;
    }
    if(!strcmp(argarray[i],"+d")){           
      tempAN->type = D;  
      i++;
      tempAN->args[0] = (char *)malloc(sizeof(char)*(strlen(argarray[i])+1));
      strcpy(tempAN->args[0],argarray[i]);  
      dl_insert_b(arglist,(void *)tempAN);  
      continue;
    }   
  }

  return arglist;
}

/* ----------------------------------------------------------------
   free the array of arguments 
   ---------------------------------------------------------------- */
void FreeArgarray(char **argarray, int n)
{
  int i=0;
  for(i=0;i<n;i++){ 
    free(argarray[i]);
    i++;
  }
  free(argarray);
}

/* -----------------------------------------------------------
   Parses the input arguments buffer
   ----------------------------------------------------------- */
Dlist ParseArguments(char *arguments, int numTargets)
{
  int count=0, tcount=0, argCount=0;
  char temp[MAX_KEY_LEN], **argarray;
  Dlist argList;

  argarray = (char **) malloc(sizeof(char *) * (numTargets*32+1)); 

  while(arguments[count] != '\0'){     
    if(isspace(arguments[count])){   
      temp[tcount] = '\0'; 
      count++;
      while(isspace(arguments[count])) count++;
      argarray[argCount] = (char *)malloc(sizeof(char)*MAX_KEY_LEN);  
      strcpy(argarray[argCount],temp); 
      tcount=0;
      argCount++;  
      memset(temp, '\0', sizeof(char)*MAX_KEY_LEN); 
    } 
    else{
      temp[tcount] = arguments[count];   
      count++;
      tcount++;
    }
  }
  argarray[argCount] = NULL;
  argList = array2Dlist(argarray, argCount);
  return argList; 
}


/* ------------------------------------------------------------------------
   given target capability, get the parameters for dm into a 2-dimensional 
   array
   ------------------------------------------------------------------------ */
char* GetTargetKey(char *capstring)
{
  char *key;
  int i=6, j=0;

  key = (char *)malloc(sizeof(char)*MAX_KEY_LEN);
  while(capstring[i] != ':') i++;
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/'){
    key[j] = capstring[i]; 
    i++; j++;
  }
  key[j] = '\0';
  return key;
}

char* GetTargetHost(char *capstring)
{
  char *host;
  int i=6,j=0;

  host = (char *)malloc(sizeof(char)*MAX_HOST_LEN);
  while(capstring[i] != ':'){
    host[j] = capstring[i];
    i++; j++;
  }
  host[j] = '\0';
  return host;
}

int GetTargetPort(char *capstring)
{
  char temp[6];
  int i=6,j=0;

  while(capstring[i] != ':') i++;
  i++;
  while(capstring[i] != '/'){
    temp[j] = capstring[i];
    i++; j++;
  }
  temp[j] = '\0';  
  return atoi(temp);
}

int GetTypeKey(char *capstring)
{
  char temp[16];
  int i=6,j=0;
  
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/') i++;
  i++;
  while(capstring[i] != '/'){
    temp[j] = capstring[i];
    i++; j++;
  }
  temp[j] = '\0';    
  return atoi(temp);
}

/* ------------------------------------------------------------
   Client: fills in the DMAS structure according to parameters
   ------------------------------------------------------------ */
DMAS*  FillDmas(Dlist argList, int index, int service, char *path, int IBPport)
{

  DMAS *dmas;
  int fd, headerlen, retls;
  char mypath[MAX_KEY_LEN];  
  Dlist temp;
  Anode info;
  FILE *tempf;

  dmas = (DMAS *)malloc(sizeof(DMAS));
  dl_traverse(temp,argList){  
    info = temp->val;
      switch(info->type){
      case S:
/*//        printf("path %s \n",path);*/
	strcpy(mypath,path);
	strcat(mypath,info->args[0]);
/*	fprintf(stderr,"S) This is the local filename: %s service: %d\n",mypath,service);*/
	if(service == DM_CLIENT){
	  if((fd=open(mypath,O_RDONLY))==-1){ 
	    perror("DM: error opening file in client");
	    exit(DM_FILEERR);
	  }
	}
	else if(service == DM_SERVER){
	  if((fd=open(mypath,O_WRONLY))==-1){ 
	    perror("DM: error opening file in server");
	    exit(DM_FILEERR);
	  }	  
	}	
	strcpy(dmas->sourcekey,info->args[0]);
	tempf = fopen(mypath, "r+");
	fread(&headerlen,sizeof(int),1,tempf);
	retls = lseek(fd,headerlen,SEEK_SET);
	dmas->sourcefd = fd;
	fclose(tempf);
	memset(mypath, '\0', sizeof(char)*MAX_KEY_LEN); 
	break;
      case T:
	/**fprintf(stderr,"T) target capability: %s\n",info->args[index]);*/
	strcpy(dmas->targetkey,GetTargetKey(info->args[index]));
	strcpy(dmas->target,GetTargetHost(info->args[index]));
	dmas->ibp_port = GetTargetPort(info->args[index]);
	dmas->type_key = GetTypeKey(info->args[index]);
	break;
      case K:
	/**fprintf(stderr,"K) type key: %s\n",info->args[0]); */
	dmas->src_key = atoi(info->args[0]);
	break;
      case F:
	/**fprintf(stderr,"F) offset: %s\n",info->args[0]); */
	dmas->offset = atol(info->args[0]);
	break;
      case Z:
	/**fprintf(stderr,"Z) size of the file: %s\n",info->args[0]); */
	dmas->filesize = atol(info->args[0]);
	break;
      case O:
	/**fprintf(stderr,"O) dm_type: %s\n",info->args[index]); */
	dmas->dm_type = atoi(info->args[index]);
	break;	
      case P:
	/**fprintf(stderr,"P) dm_port: %s\n",info->args[index]); */
	dmas->dm_port = atoi(info->args[index]);
	break;
      case D:
	/**fprintf(stderr,"D) dm_service: %s\n",info->args[0]); */
	dmas->dm_service = atoi(info->args[0]);
	break;
      }
  } 

/*//  dmas->ibp_port = IBPport;*/
  return dmas;   
}

int GetServiceType(char *arguments)
{
  int count;
  char temp[3];
  count = strlen(arguments);

  temp[0] = arguments[count-3];
  temp[1] = arguments[count-2];
  temp[2] = '\0';
  if(!strcmp(temp,"35"))
    return DM_SERVER;                   
  else
    return DM_CLIENT;                   
}


int GetNumTargets(char *arguments)
{
  int num, count=0, i=0;
  char temp[3];

  while(arguments[count] != '\0'){
    if(arguments[count] == '+')
      if(arguments[count+1] == 'n'){ 
	count = count + 3;
	while(!isspace(arguments[count]))
	  temp[i++] = arguments[count++];
	break;
      }
    count++;
  }
  temp[i] = '\0';
  num = atoi(temp);
  return num;
}

int GetDataMoverType(char *arguments)
{
  int dmservice, count=0, i=0;
  char temp[3];

  while(arguments[count] != '\0'){
    if(arguments[count] == '+')
      if(arguments[count+1] == 'd'){ 
	count = count + 3;
	while(!isspace(arguments[count]))
	  temp[i++] = arguments[count++];
	break;
      }
    count++;
  }
  temp[i] = '\0';
  dmservice = atoi(temp);
  return dmservice;
}

/* -----------------------------------------------------------------
   build the source capability as original, for further IBP calls
   ----------------------------------------------------------------- */
char* BuildSourceCap(DMAS *dmas)
{
  char *theCap;
  int r;
  struct utsname thisHost;

  theCap = (char *)malloc(sizeof(char)*MAX_CAP_LEN);
  r = uname(&thisHost);

  sprintf(theCap,"ibp://%s:%d/%s/%d/READ",
	  thisHost.nodename,
	  IBPport,    /*dmas->ibp_port, */
	  dmas->sourcekey,
	  dmas->src_key);

  return theCap;
}


/* ------------------------------------------------------------------
   fork data mover client depending on the kind and type of service
   ------------------------------------------------------------------ */
int RunDMClient(DMASP *dmas, int ntargets, int dmtype,char *path)
{
  int retval,i,intstatus,myexitval=DM_CLIOK;
  char temp_s[64], temp_fd[64], temp_nt[64];
  char command[256];
  char port_s[MAX_WORD_LEN], opt_s[MAX_WORD_LEN];
  char *args[10], *temp_t,*temp_p,*temp_opt;
  char IBP_srccap[MAX_CAP_LEN*2];
  char mgroup[20]= "233.13.122.251"; 

  siginfo_t status;
  int x;

  temp_t = (char *)malloc(sizeof(char) * (MAX_HOST_LEN+1) * ntargets);
  memset( temp_t, '\0', sizeof(char) * (MAX_HOST_LEN+1) * ntargets );
  strcpy(IBP_srccap, BuildSourceCap(dmas[0]));

  temp_p=(char *)malloc(sizeof(char) * 8 * ntargets);
  temp_opt = (char *)malloc(sizeof(char) * 8 * ntargets);
  memset( temp_p, '\0', sizeof(char) * 8 * ntargets );
  memset( temp_opt, '\0', sizeof(char) * 8 * ntargets );

  /* see what datamover to use and set proper name or parameters */
  switch(dmtype){
    case DM_MULTI:
      sprintf(command,"%s%s",glbExecPath,"mclientTCP");
      args[0] = command;
      /*
      strcpy(command,"./mclientTCP");
      args[0] = "./mclientTCP";
      */
      break;
    case DM_SMULTI:  
      sprintf(command,"%s%s",glbExecPath,"smclientTCP");
      args[0] = command;
      /*
      strcpy(command,"./smclientTCP");
      args[0] = "./smclientTCP";
      */
      break; 
    case DM_PMULTI: 
      sprintf(command,"%s%s",glbExecPath,"pmclientTCP");
      args[0] = command;
      /*
      strcpy(command,"./pmclientTCP");
      args[0] = "./pmclientTCP";
      */
      break;
    case DM_BLAST:
      sprintf(command,"%s%s",glbExecPath,"blaster");
      args[0] = command;
      /*
      strcpy(command,"./blaster");
      args[0] = "./blaster"; 
      */
      break;
    case DM_MBLAST: 
      sprintf(command,"%s%s",glbExecPath,"tblaster");
      args[0] = command;
      /*
      strcpy(command,"./tblaster");
      args[0] = "./tblaster";
      */
      break;
    case DM_MULTICAST:
      strcpy(command,"./clientMCAST");
      args[0] = "./clientMCAST";
      break;
    default:   
      fprintf(stderr,"Undefined Client Data Mover Type\n");
      myexitval = DM_TYPEERR;
      exitval = DM_TYPEERR;
      goto POINT;
      break;
  }

  if(dmtype != DM_MULTICAST)
  {
    temp_opt[0] = '\0';
    temp_p[0] = '\0';
    /* ---- common argument assignment ---- */
    for(i=0; i<ntargets; i++){
      sprintf(port_s,"%d ",dmas[i]->dm_port);     /* pass a set of ports */
      strcat(temp_p,port_s);
      strcat(temp_t,dmas[i]->target);             /* pass a set of targets */
      strcat(temp_t," ");
      sprintf(opt_s,"%d ",dmas[i]->dm_type);      /* pass set of options */
      strcat(temp_opt,opt_s);
    }
    args[1] = temp_t;
    args[2] = temp_p;
    sprintf(temp_s,"%d",dmas[0]->sourcefd);       /* pass unique fd for source */
    args[3] = temp_s;
    sprintf(temp_fd,"%ld",dmas[0]->filesize);     /* pass unique file size for source */
    args[4] = temp_fd;
    sprintf(temp_nt,"%d",ntargets);
    args[5] = temp_nt;  
    args[6] = IBP_srccap;
/*//  args[7] = NULL;*/
    if(dmtype == DM_MBLAST)    
    {
      args[7] = path;
      args[8] = NULL;
    }
    else
      args[7] = NULL;
  }
  else
  {
     i=0;
     args[1] = mgroup;
     sprintf(port_s,"%d ",dmas[i]->dm_port);     /* pass a set of ports */
     strcat(temp_p,port_s);
     args[2] = temp_p;
     sprintf(temp_s,"%d",dmas[0]->sourcefd);       /* pass unique 
        fd for source */
     args[3] = temp_s;
     sprintf(temp_fd,"%ld",dmas[0]->filesize);     /* pass unique file size for source */
     args[4] = temp_fd;
     args[5] = IBP_srccap;
     args[6] = NULL;
  }

  /* ---- fork the datamover choice ---- */
  retval = fork();
  if(retval < 0){
    printf("error in forking \n");
    perror("SIM: error forking");
    exitval = DM_PROCERR;
    goto POINT;
  }
  if(retval != 0){                              /* parent */ 
    x=waitpid(retval, &intstatus, 0);
    fflush(stdout);

    if (x < 0){
     printf("critical error \n");
      perror("Critical error in wait()");
      exitval = DM_PROCERR;
      goto POINT;
    }
    else{
      /**fprintf(stderr,"SIM Parent: pid(%d) fork(%d) VAL=(%d)%d x=%d\n",
	 (int)getpid(),retval,WEXITSTATUS(intstatus),intstatus,x); */
      if(WEXITSTATUS(intstatus) != 0){
	fprintf(stderr,"DM: Something went wrong in protocol!\n");
	myexitval = DM_PROCERR;
	goto POINT;
      }
    }
  }
  else{                                         /* child */
    int j;    
    /***printf("RunDMClient Child: fork returned %d\n",retval); */
    j=execvp(command,args);
    if(1 || j<0){
      perror("RunDMClient: exec error!");
      exit(DM_PROCERR);
    } 
  }  
 POINT:
free(temp_t);  
  return myexitval;
}

/* -------------------------------------------------------------
   in the target side, run a DM server 
   ------------------------------------------------------------- */
int RunDMServer(DMAS *dmas, int dmtype, char *IBP_cap)
{
  int retval,status,exitval;
  char command[MAX_COMM_LEN], tempp[5],temps[5],tempf[64];
  char *args[6];
  char mgroup[20]="233.13.122.251";

  the_filesize = dmas->filesize;
  switch(dmas->dm_type){                         /* select datamover and set proper parameters */
    case DM_MULTI:
    case DM_SMULTI:
    case DM_UNI:   
      sprintf(command,"%s%s",glbExecPath,"serverTCP");
      /*strcpy(command,"./serverTCP");*/
      args[0] = command;
      sprintf(tempp,"%d",dmas->dm_port);
      args[1] = tempp;
      sprintf(temps,"%d",dmas->sourcefd);
      args[2] = temps;
      sprintf(tempf,"%ld",dmas->filesize);
      args[3] = strdup(tempf);
      args[4] = IBP_cap;
      args[5] = NULL;
      break;
    case DM_BLAST:
    case DM_MBLAST:
      sprintf(command,"%s%s",glbExecPath,"blasterd");
      /*strcpy(command,"./blasterd");*/
      args[0] = command;
      sprintf(tempp,"%d",dmas->dm_port);
      args[1] = tempp;
      sprintf(temps,"%d",dmas->sourcefd);
      args[2] = temps;
      sprintf(tempf,"%ld",dmas->filesize);
      args[3] = strdup(tempf);
      args[4] = IBP_cap;
      args[5] = NULL;      
      break;
    case DM_MCAST:
      strcpy(command,"./tservMCAST");   
      args[0] = command;
      args[1] = mgroup;
      sprintf(tempp,"%d",dmas->dm_port);
      args[2] = tempp;
      sprintf(temps,"%d",dmas->sourcefd);
      args[3] = temps;  
      sprintf(tempf,"%ld",dmas->filesize);
      args[4] = strdup(tempf);
      args[5] = IBP_cap;
      args[6] = NULL;
      break;
    default:
      fprintf(stderr,"Undefined Client Data Mover Type\n");
      retval = DM_TYPEERR;
      exitval = DM_TYPEERR;
      goto POINT;      
      break;
  }
  
  retval = fork();                              /* fork the server of any data mover */
  if(retval < 0){
    perror("RunDMServer: error forking");
    exit (DM_PROCERR);
  }
  if(retval != 0){                              /* parent */
    /**printf("SIM Parent: pid(%d) has the arguments fork(%d)\n",(int)getpid(),retval);*/
    waitpid(retval, &status, 0);
  }
  else{                                         /* child */
    int j;
    /**printf("SIM Child: fork returned %d\n",retval); */
    j=execvp(command,args);
    if(j < 0){
      perror("RunDMServer: execute error!");
      exit (DM_PROCERR);
    }     
  }
 POINT:
  return retval;
}



/* -------------------------------------------------------------------
   Send request to the IBP server (destination) to start a DM server
   that receives data from the client DM,  this one should 
   behave like a client application of IBP.  Sends message through
   the IBP port with service code CLIENT
   ------------------------------------------------------------------- */
int RequestToTarget(DMAS *dmas, Dlist argList, int t_id)
{
  struct ibp_depot src_depot, dest_depot;
  struct ibp_attributes src_attr;
  struct ibp_set_of_caps src_caps, dest_caps;
  int ibp_retval, retval=1;
  Dlist templ;
  Anode tempAN;

  struct ibp_timer timeout;

  dest_caps.writeCap = (char *)malloc(sizeof(char)*MAX_CAP_LEN);
  src_caps.readCap = (char *)malloc(sizeof(char)*MAX_CAP_LEN);

#ifdef IBP_DEBUG
  fprintf(stderr,"Sending Copy Request to Target Host... %s %d\n",dmas->target,dmas->ibp_port);
#endif

  strcpy(src_depot.host, dmas->target);
  src_depot.port = dmas->ibp_port;        /* doesn't matter which port, no use here */

  strcpy(dest_depot.host, dmas->target);
  dest_depot.port = dmas->ibp_port;       /* this is the server's port */
 
  sprintf(dest_caps.writeCap,
	  "ibp://%s:%d/%s/%d/WRITE",
	  dmas->target,
	  dmas->ibp_port,
	  dmas->targetkey,
	  dmas->type_key);
 


  sprintf(src_caps.readCap,
	  "ibp://%s:%d/%s/%d/READ",
	  dmas->target,
	  dmas->ibp_port,
	  dmas->targetkey,
	  dmas->type_key);

  src_attr.duration = -1;
  src_attr.reliability = IBP_STABLE;
  src_attr.type = IBP_BYTEARRAY;

  src_attr.duration = -1;
  src_attr.reliability = IBP_VOLATILE;
  src_attr.type = IBP_BYTEARRAY;

  dl_traverse(templ,argList){               /* handle timeouts */
    tempAN = (Anode)dl_val(templ);
    if(tempAN->type == I){
      timeout.ClientTimeout = atoi(tempAN->args[t_id+numTargets+1]);  
      timeout.ServerSync = atoi(tempAN->args[t_id+1]);
    }
  }

#ifdef IBP_DEBUG
  fprintf(stderr,"Initialization done, proceed with the mcopy...\n");  
#endif

  ibp_retval = IBP_datamover(dest_caps.writeCap,	
			     src_caps.readCap,
			     &timeout,
			     dmas->filesize, dmas->offset, 
			     dmas->dm_type, dmas->dm_port, DM_SERVER);
  if(ibp_retval <= 0){
    fprintf(stderr,"TOO BAD\n");
    retval = -1;
  }
  return retval;
}



/* ------------------------------------------------------------------- 
   this is strictly for the client side DMAS structure
   ------------------------------------------------------------------- */
DMASP* CreateDMAS(Dlist argList, int service, char *path, int IBPport)
{
  DMASP *dmasArray;
  int i;
  char mypath[MAX_KEY_LEN];

  strcpy(mypath,path);
  /**fprintf(stderr,"-----> In CreateDMAS numTargets=%d  IBPport=%d\n",numTargets,IBPport);*/
  dmasArray = (DMASP *)malloc(sizeof(DMAS *) * numTargets);

  for(i=0; i<numTargets; i++){
    dmasArray[i] = FillDmas(argList,i,service,mypath,IBPport);  /* create DMAS for each target */
  }
  return dmasArray;   
}


/* ------------------------------------------------------------------- 
   parse the internal string received as request from the IBP server
   from the DM to create a server process
   ------------------------------------------------------------------- */
char** ParseServerArguments(char *arguments)
{
  int count=0, tcount=0, argcount=0;
  char temp[MAX_KEY_LEN], **argarray;

  argarray = (char **) malloc(sizeof(char *) * (10+1));
  
  while (isspace(arguments[count] )){
	count++;
  }
		   
  while(arguments[count] != '\0'){     
    if(isspace(arguments[count])){
      /* while ( isspace(arguments[++count]) ); */
      temp[tcount] = '\0';
      argarray[argcount] = (char *)malloc(sizeof(char)*MAX_KEY_LEN);
      strcpy(argarray[argcount],temp);
      tcount=0;
      argcount++;
    }
    temp[tcount] = arguments[count];
    count++;
    tcount++;
  }

  argarray[argcount] = NULL;
  return argarray; 
}

/* -----------------------------------------------------------------
   fills in the DMAS structure according to parameters
   ----------------------------------------------------------------- */
DMAS* FillServerDmas(char **argarray,char path[MAX_KEY_LEN])
{
  DMAS *dmas;
  int argcount=0, fd, headerlen, retls,i ;
  FILE *tempfd;
  char mypath[MAX_KEY_LEN];

  dmas = (DMAS *)malloc(sizeof(DMAS));

  while(argcount < 10){
      switch(argcount){
      case 0:
	strcpy(mypath,path);
	strcat(mypath,argarray[argcount]);
/*	fprintf(stderr,"0) This is the local filename: %s\n",mypath);*/
	if((fd=open(mypath,O_WRONLY))==-1){ 
	  perror("error opening file in target");
	  exit(DM_FILEERR);
	}	
	tempfd = fopen(mypath, "r+");
	fread(&headerlen,sizeof(int),1,tempfd);
	retls = lseek(fd,headerlen,SEEK_SET);
	dmas->sourcefd = fd;
	fclose(tempfd);
	break;
      case 3:
	dmas->offset = atol(argarray[argcount]);
	break;
      case 4:
/*	fprintf(stderr,"4) size of the file: %s\n",argarray[argcount]);*/
	dmas->filesize = atol(argarray[argcount]);
	break;
      case 7:
	dmas->dm_type = atoi(argarray[argcount]);
	break;	
      case 8:
	dmas->dm_port = atoi(argarray[argcount]);
	break;
      case 9:
	dmas->dm_service = atoi(argarray[argcount]);
	break;
      }
      argcount++;
  } 
  return dmas;
}


/* --------------------------------------------------------
   free all memory allocated for client/server structures
   -------------------------------------------------------- */
void FreeArray(char** argarray)
{
  int i=0;
  
  while(argarray[i] != NULL){
    free(argarray[i]);
    i++;
  }

  free(argarray);
}

void FreeServerDmas(DMAS *serverdmas)
{
  free(serverdmas);
}

void FreeDlist(Dlist argList)
{
  Anode tempAN;
  Dlist temp;
  int i;

  dl_traverse(temp,argList){
    i=0;
    tempAN = (Anode)dl_val(temp);
    while(tempAN->args[i] != NULL){
      free(tempAN->args[i]);
      i++;
    }
    free(tempAN->args);
    free(tempAN);
    dl_delete_node(temp);
  }
  free(argList);
}



void FreeDMASP(DMASP* mydmas, int n)
{
  int i;
  
  for(i=0;i<n;i++){
    free(mydmas[i]);
    i++;
  }
  free(mydmas);
}

/* ----------------------------------------------------------------
                              M A I N
   Arguments CLIENT: service, file descriptor, DMtype, host, port	
   Arguments SERVER: service, file descriptor, DMtype, port
   ---------------------------------------------------------------- */
int main(int argc, char *argv[])
{
  DMASP *clientdmas;
  DMAS *serverdmas;
  char **argarray;
  int i;
  int service, retval, status, dmtype, attempts=0, nc=0; 
  Dlist argList;
  char *cp;


  /*//signal(SIGCHLD,SIG_IGN);*/

  exitval = 1; 
  fprintf(stderr,"\n -------------- RUNNING DM.c -------------\n");
#ifdef IBP_DEBUG
  fprintf(stderr,"\nDM parameters: <<%s>> <<%s>> <<%s>>\n",argv[1],argv[2],argv[3]);
#endif

  service = GetServiceType(argv[1]);                         /* see if CLIENT or SERVER */
  dmtype = GetDataMoverType(argv[1]);                        /* see type: TCP or BLAST */
  IBPport = atoi(argv[3]);

  /* 
   * get path of the executable
   */
  strcpy(glbExecPath,argv[0]);
  if ( (cp = strrchr(glbExecPath,'/')) == NULL ){
      glbExecPath[0] = '\0';
  }else{
      cp[1] = '\0';
  }


  if(service == DM_SERVER){     
    argarray = ParseServerArguments(argv[1]);
    serverdmas = FillServerDmas(argarray,argv[2]);
    retval = RunDMServer(serverdmas,dmtype,argarray[1]);
  }
  else if(service == DM_CLIENT){
    numTargets = GetNumTargets(argv[1]);
    argList = ParseArguments(argv[1],numTargets);                    /* create dllist of arguments */
    clientdmas = CreateDMAS(argList,service,argv[2],IBPport);        /* fill client DMAS with arguments */

    for(i=0;i<numTargets;i++){  
      /*//fprintf(stderr,"CLIENT service type: %d  target %s:%d \n",*/
  	/*//  clientdmas[i]->dm_service,clientdmas[i]->target,clientdmas[i]->dm_port); */
      retval = RequestToTarget(clientdmas[i],argList,i);         /* contact target depot spawn SERVER */    
       if(retval < 0){	                                         /* client couldn't run */
	fprintf(stderr,"DM: Error in IBP server %d...\n",i);
	exit(-5);
      }
      nc++;
    }
    sleep(MAX_WAIT_TIME);
    retval = RunDMClient(clientdmas,numTargets,dmtype,argv[2]);          /* create the 
client */ 

   /* fprintf(stderr,"---> retval RunDMClient=%d\n",retval);*/
    while(attempts < MAX_NUM_ATTEMPTS){   
      if(retval < 0){
	attempts++; 
	fprintf(stderr,"Waiting for the Server, retry #%d...",attempts);
	sleep(MAX_WAIT_TIME); 
	retval = RunDMClient(clientdmas,numTargets,dmtype,argv[2]);      /* create the 
client */
      }
      else break;
    }
    if((attempts==MAX_NUM_ATTEMPTS) && (retval<0))
      exitval = DM_NETWERR;
  }
  else{
    fprintf(stderr,"Wrong service type, check arguments");
    exitval = DM_TYPEERR;
  }
 
  if(exitval > 0){
    if(service == DM_SERVER){
      /*FreeArray(argarray); */
      FreeServerDmas(serverdmas);
      exitval = DM_SERVOK;
    }
    else if(service == DM_CLIENT){
      exitval = DM_CLIOK;
      FreeDMASP(clientdmas,numTargets);
    }
  }

  fprintf(stderr,"Service %d DM RETURNING: %d\n",service,exitval);
  exit(exitval);
}

