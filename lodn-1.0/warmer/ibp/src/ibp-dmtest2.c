#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h> 
#include "ibp_ClientLib.h"

#include <sys/mman.h>
#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#define THESIZE 1024*500

#define NTARGETS 2
#define DM_PORT  5666

double seconds()
{
  struct timeval ru;
  gettimeofday(&ru, (struct timezone *)0);
  return(ru.tv_sec + ((double)ru.tv_usec)/1000000);
}

int ibp_quicktest(argc,argv)
     int argc;
     char **argv;
{

  struct ibp_depot ls_depot[NTARGETS], ls_src_depot;
  
  struct ibp_attributes ls_att[NTARGETS], ls_src_att;

  IBP_set_of_caps ls_caps[NTARGETS], ls_src_caps;
 
  IBP_cap multicaps[NTARGETS];
  
  time_t ls_now;
 
  char *lc_buf;            
 
  int  li_ret;
   
  int tmpport;
  
  struct ibp_capstatus ls_info;
  IBP_DptInfo ls_dinf;
  
  int i,j,k,rtn, errors=0, ports[NTARGETS], dm_types[NTARGETS], numTargets;
  struct ibp_timer ls_srctime, ls_timeout, ls_ttimeout[NTARGETS];
  double s;
  float rate;

  char *theBuff;
  int fdtomap;
  int filesize;


  lc_buf=(char *)malloc(sizeof(char) * THESIZE);

  /* ------------- check number of arguments ------------- */

  if (argc != (((NTARGETS+1)*2)+1)) {
    fprintf (stderr, "usage: ibp-dmtest [source_depot] [source_port] [target_depot1] [target_port1] ... [target_depotn] [target_portn]\n");
    exit(0);
  }
  
  fprintf (stderr, "IBP-datamover test, initializing ...");

  strcpy (ls_src_depot.host, argv[1]);
  ls_src_depot.port = atoi(argv[2]);
  ls_src_depot.rid = 0;


  for(i=0,j=0; i<NTARGETS; i++){
    strcpy (ls_depot[i].host, argv[j+3]);
    ls_depot[i].port = atoi(argv[j+4]);
    ls_depot[i].rid = 0;
    j+=2;
  }

  /* 
   * set up the attributes
   */
  time(&ls_now);
  ls_src_att.duration =(1 * 3600);        /* set up source attributes */
  ls_src_att.duration += ls_now;
/*//  ls_src_att.reliability=IBP_STABLE;*/
  ls_src_att.reliability = IBP_VOLATILE;
  ls_src_att.type=IBP_BYTEARRAY;  

  for(i=0;i<NTARGETS;i++){                 /* set up targets attributes */
    time(&ls_now);
    ls_att[i].duration = (1 * 3600);
    ls_att[i].duration += ls_now;
    ls_att[i].reliability=IBP_STABLE;
    ls_att[i].type= IBP_BYTEARRAY;
  }
  
  
  /* -------  set memory buffer file contet --------- */
  memset(lc_buf,123456789,THESIZE);               

  /* -------- set timeout for source side ----------- */
  ls_srctime.ServerSync = 10;
  ls_srctime.ClientTimeout = 100;
 
  /* ---------- set timeout for targets ------------- */
  ls_timeout.ServerSync = 100;
  ls_timeout.ClientTimeout = 300;

  for(i=0;i<NTARGETS;i++)
  {
    ls_ttimeout[i].ServerSync = 100;
    ls_ttimeout[i].ClientTimeout =300;
  }

  fprintf (stderr, " done\n");

  /*
   * first step: allocation
   */

  fprintf (stderr, "trying to IBP_allocate ... ");    /*  allocate space in source depot */
  ls_src_caps = IBP_allocate(&ls_src_depot, &ls_timeout, THESIZE, &ls_src_att);
  if(ls_src_caps == NULL){
    fprintf(stderr,"source depot IBP_allocate failed! %d\n",IBP_errno);
    exit(1);
  }

  for(i=0;i<NTARGETS;i++){                            /* allocate space in each target depot */
    ls_caps[i] = IBP_allocate(&ls_depot[i], &ls_timeout ,THESIZE ,&ls_att[i]);
    if(ls_caps[i] == NULL){
       fprintf(stderr,"%d) Target IBP_allocate failed! %d\n", i,IBP_errno);
       exit(1);
    }
  }

  fprintf (stderr, "done\n");

  /* 
   * store the data on A1
   */


  fprintf (stderr, "trying to IBP_store ... ");

  s = seconds();

  li_ret = IBP_store(ls_src_caps->writeCap, &ls_timeout, lc_buf, THESIZE); 
  if (li_ret <= 0) {
    fprintf(stderr, "source depot IBP_store failed! %d\n",IBP_errno);
    exit(1);
  }
  fprintf(stderr, "done\n");
  s = seconds()-s;
  printf("store time %6.2f \n",s);

  for(i=0; i<NTARGETS; i++){
     li_ret = IBP_manage(ls_caps[i]->readCap, &ls_timeout, IBP_PROBE, IBP_READCAP, &ls_info);
     if (li_ret < 0)
       fprintf(stderr, "MANAGE failed! first time, %d\n",IBP_errno);
     else
      fprintf(stderr, "\n... IBP manage %d.1 done \n",i+1);
  }
  /*------------------------------------------------------------------------------------------
   * mcopy data from src to targets using 3 TCPs
   -------------------------------------------------------------------------------------------*/

  numTargets = NTARGETS;
  
  tmpport = DM_PORT;
  
  for(i=0; i<NTARGETS; i++){
    multicaps[i] = ls_caps[i]->writeCap;
    ports[i] = tmpport;
    dm_types[i] = DM_BLAST;  
/*  DM_BLAST for UDP  or  
    DM_UNI for any TCP data mover */

    tmpport++;
  }
 
  
  /***fprintf(stderr,"The readCap <<<%s>>>\nThe writeCaps <<<<%s>>>>\n<<<%s>>>\n",
      ls_caps1->readCap,ls_caps2->writeCap,ls_caps3->writeCap);*/
  s = seconds(); 

   fprintf (stderr, "trying to IBP_mcopy ...");
   li_ret = IBP_mcopy(ls_src_caps->readCap, 
		      multicaps, 
		      numTargets, 
		      &ls_srctime, 
		      ls_ttimeout, 
		      THESIZE, 
		      0,
		      dm_types, 
		      ports,
		      DM_MBLAST); 
                                     /*   for UDP: DM_BLAST or DM_MBLAST  
                                          for TCP: DM_SMULTI, DM_PMULTI, or DM_MULTI 
                                     */
 
  printf("li_ret %d \n",li_ret);
  if (li_ret <= 0) {
    fprintf(stderr, "failed! IBP_mcopy, %d\n",IBP_errno);
    exit(1);
  }
  
  fprintf(stderr," ... done\n");

  s = seconds()-s;     
  filesize = THESIZE;
  rate = (s !=0) ? filesize/(1024*1024*(s)) : 0; 
  rate = rate * numTargets;     
  fprintf(stderr,"\nTOTAL TRANSFER TIME = %6.2f secs ",s);
  fprintf(stderr," SIZE = %d  RATE = %.2f MB/s (%.2fMb/s)\n",
	  filesize, rate, rate*8);

#if 0
  fprintf (stderr, "trying to IBP_load and manage... ");
  for(i=0; i<NTARGETS; i++){
    li_ret = IBP_manage(ls_caps[i]->readCap, &ls_timeout, IBP_PROBE, IBP_READCAP, &ls_info);
    if (li_ret < 0) 
      fprintf(stderr, "MANAGE failed! first time, %d\n",IBP_errno);
    else
      fprintf(stderr, "\n... IBP manage %d.1 done \n",i+1);
    
    li_ret = IBP_load(ls_caps[i]->readCap, &ls_timeout, lc_buf, THESIZE, 0);
    if (li_ret <= 0) 
      fprintf(stderr, "failed! first time, %d\n",IBP_errno);
    else
      fprintf(stderr, "\n... IBP load %d done \n ",i+1);
  }
#endif
 
  fprintf(stderr,"done \n");

}


int
main(argc,argv)
     int argc;
     char **argv;
{

  int li_ret;

  li_ret = ibp_quicktest(argc, argv);

  return li_ret;
}




