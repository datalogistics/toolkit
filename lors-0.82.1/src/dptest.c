#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "lors_api.h"
#include "lors_util.h"
#include "lors_error.h"
#include "lbone_client_lib.h"
#include "lors_opts.h"


static void
printDepot(LorsDepot *dp, int no)
{
   
    fprintf(stdout,"%d\t%d\t%s\t%d\t%f\t%f\t%f\t%d\t%d\n",
                   no,dp->id,dp->depot->host,dp->depot->port,
                   dp->score,dp->bandwidth,dp->proximity,
                   dp->nfailure,dp->nthread );

};

static  void
printDepotPool(LorsDepotPool *dpp)
{
    JRB       tmp;
    int i;
    LorsDepot *dp;
    char str[81];

    for ( i = 0 ; i < 80 ; i ++ ) str[i]= '=';
    str[80] = '\0';
    fprintf(stdout,"%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n","No","ID","Depot","Port","Score","Bandwidth","Prox","NFailure","NThread");
    fprintf(stdout,"%s\n",str);
    if ( dpp == NULL )
       return;

    if (dpp->list == NULL )
       return;

    i = 0;
    jrb_traverse(tmp,dpp->list) {
        dp = (LorsDepot *)(jrb_val(tmp).v);
        assert(dp != NULL);
        printDepot(dp,i);
        i++;
    };
};
#define TEST_BUFFERSIZE 1024*1024*5 

int main() {
  
    char lbserver[]="galapagos.cs.utk.edu";
    /*char lbserver[]="dsj.sinrg.cs.utk.edu"; */
    int  lbport = 6767;

    IBP_depot   dps[4];
   
    char location[]="zip= 37916";
    char dst_location[]="state= CA";
    int  max = 8;
    longlong storage_size = 1024*5;
    int  storage_type = IBP_HARD;
    double duration = 0.2;
    int  timeout = 200;
    int opts = LORS_SERVER_AND_LIST | LORS_CHECKDEPOTS | SORT_BY_PROXIMITY | SORT_BY_BANDWIDTH | LORS_DEPOTSTATUS ;
    /*int opts =   LORS_CHECKDEPOTS | SORT_BY_BANDWIDTH;*/
/*    char    buf[TEST_BUFFERSIZE];
    char    buf1[TEST_BUFFERSIZE];
*/
    char    *buf,*buf1,buf2[300];
    int         i;
    int         size;
    char        *tmpbuf;

    LorsSet    *se,*copy_set;
    LorsDepotPool *dpp;
    LorsDepotPool *dst_dpp;
    LorsDepot *dp;
    LorsEnum        mapl;
    LorsEnum        iter;
    LorsMapping     *lm;
    longlong length = 10*1024*1024;
    int copies = 2;
    ulong_t blk_size = 512*1024;
    ulong_t dst_block_size = 256*1024;
    int nthreads = 10;
    int ret;
    int opt_create = LORS_RETRY_UNTIL_TIMEOUT;

    struct ibp_depot d1={"ibanez.cs.utk.edu",6714};
    struct ibp_depot d2={"i2-dsj.ibt.tamus.edu",6714}; 
    struct ibp_depot d3={"pompone.cs.ucsb.edu",6714};


    if ((buf = calloc(TEST_BUFFERSIZE,sizeof(char))) == NULL ){
       exit(-1);
    };
    if ((buf1 = calloc(TEST_BUFFERSIZE,sizeof(char))) == NULL ){
       exit(-1);
    };
    for(i=0; i < TEST_BUFFERSIZE; i++) buf[i] = (char)(i%26+'a');
    for(i=0; i < TEST_BUFFERSIZE; i++) buf1[i] = (char)('C');
    for(i=0; i < 299; i++) buf2[i] = (char)('C');
    g_db_level = D_LORS_TERSE | D_LORS_VERBOSE;
    dps[0] = &d1;
    dps[1] = &d2;
    dps[2] = &d3;
    dps[3] = NULL;

    if ( lorsGetDepotPool(&dpp,
                          lbserver,
                          lbport,
                          NULL, 
                          max,
                          location,
                          storage_size,
                          storage_type,
                          duration,
                          nthreads,
                          timeout,
                          opts) != LORS_SUCCESS ){
         fprintf(stderr,"Error in get depotpool!\n");
         exit(-1);
     };
    if ( lorsGetDepotPool(&dst_dpp,
                          lbserver,
                          lbport,
                          NULL, 
                          12,
                          dst_location,
                          storage_size,
                          storage_type,
                          duration,
                          nthreads,
                          timeout,
                          opts) != LORS_SUCCESS ){
         fprintf(stderr,"Error in get depotpool!\n");
         exit(-1);
     };


     printDepotPool(dpp);
     printDepotPool(dst_dpp);

     if ( lorsGetDepot(dpp,&dp) != LORS_SUCCESS ){
         fprintf(stderr,"Error in get depot!\n");
         exit(-1);
     };
     if ( lorsGetDepot(dpp,&dp) != LORS_SUCCESS ){
         fprintf(stderr,"Error in get depot!\n");
         exit(-1);
     };
     if ( lorsGetDepot(dpp,&dp) != LORS_SUCCESS ){
         fprintf(stderr,"Error in get depot!\n");
         exit(-1);
     };
     if ( lorsGetDepot(dpp,&dp) != LORS_SUCCESS ){
         fprintf(stderr,"Error in get depot!\n");
         exit(-1);
     };

     printDepotPool(dpp);

     if ( lorsReleaseDepot(dp,0,2) != LORS_SUCCESS){
         fprintf(stderr,"Error in release depot!\n");
         exit(-1);
     };

     printDepotPool(dpp);


     timeout = 3*60;
     /*if ( lorsSetCreate(&se,dpp,length,copies,blk_size,nthreads,timeout,opt_create) 
      * != LORS_SUCCESS ) {*/
     if ( lorsSetCreate(&se,0,TEST_BUFFERSIZE,1024*512,copies, 0) != LORS_SUCCESS ) {
         fprintf(stderr,"Error in create set!\n");
         exit(-1);

     };
     tmpbuf = buf;
     for(size= 1024*1024; se->curr_length < TEST_BUFFERSIZE; )
     {
        
        if (lorsSetStore(se, dpp, tmpbuf, size, NULL, nthreads, timeout, 0)!= LORS_SUCCESS)
        {
            fprintf(stderr,"Error in lorsStoreSet!\n");
            exit(-1);
        }
        tmpbuf = tmpbuf+size;
     } 
     if ( lorsSetStore(se, dpp, buf, TEST_BUFFERSIZE, NULL, 4, timeout, 0) != LORS_SUCCESS)
     {
         fprintf(stderr,"Error in lorsStoreSet!\n");
         exit(-1);
     }

     fprintf(stderr,"Beginning download....\n");

     /*  if ( (i= lorsSetLoad(se,buf1,0,TEST_BUFFERSIZE,nthreads,timeout,0)) <= 0 ){ */
     if ( (i= lorsSetLoad(se,buf1,0,512*1024*5,0,NULL,15,timeout,0)) <= 0 ){
            fprintf(stderr,"Error in lorsSetLoad  = %d!\n" ,i);
            exit(-1);
         
     };
     for (i=0;i < 26 ; i++){

         fprintf(stderr, "buf1[%d] = %c\n", i, buf1[i]);
     }

     fprintf(stderr,"Beginning copy .....\n");
     if ( (i = lorsSetCopy(se,&copy_set,dst_dpp,dst_block_size,copies,0,TEST_BUFFERSIZE,1,timeout,LORS_COPY)) != LORS_SUCCESS ){
         fprintf(stderr,"Error in lorsSetCopy! error = %d\n",i);
         exit(-1);
     };

     if ( (i= lorsSetLoad(copy_set,buf2,128,256,0,NULL,nthreads,timeout,0)) <= 0 ){
            fprintf(stderr,"Error in lorsSetLoad  = %d!\n" ,i);
            exit(-1);
         
     };
     for (i=0;i < 26 ; i++){

         fprintf(stderr, "buf2[%d] = %c\n", i, buf2[i]);
     };
     getchar();


     fprintf(stderr, "Regular stat\n");
     if ( lorsSetStat(se, 10, timeout, 0) != LORS_SUCCESS)
     {
         fprintf(stderr, "Stat failed\n");
         exit(1);
     }
     lorsPrintSet(se);
     fprintf(stderr, "Refresh to Maximum \n");
     if ( lorsSetRefresh(se, 0, -1, 0, -1, 60, LORS_REFRESH_MAX) != LORS_SUCCESS)
     {
         fprintf(stderr, "Refresh failed\n");
         exit(1);
     }
     if ( lorsSetStat(se, 10, 100, 0) != LORS_SUCCESS)
     {
         fprintf(stderr, "Stat failed\n");
         exit(1);
     }
     lorsPrintSet(se);
     fprintf(stderr, "Set duration to 2 days from now.\n");
     if ( lorsSetRefresh(se, 0, -1, 60*60*24*2, -1, 60,LORS_REFRESH_EXTEND_TO) 
             != LORS_SUCCESS)
     {
         fprintf(stderr, "Refresh failed\n");
         exit(1);
     }
     if ( lorsSetStat(se, 10, 100, 0) != LORS_SUCCESS)
     {
         fprintf(stderr, "Stat failed\n");
         exit(1);
     }
     lorsPrintSet(se);
     if ( lorsSetTrim(se, 300, 300, 2, 100, LORS_TRIM_ALL) != LORS_SUCCESS )
     {
         fprintf(stderr, "Trim failed.\n");
         exit(1);
     }
     lorsPrintSet(se);

     lorsFreeDepotPool(dpp);
     lorsSetFree(se, LORS_FREE_MAPPINGS);
 
     fprintf(stderr,"SUCCESS!!!!!!!\n");

     return(0);
};
