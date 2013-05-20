#include "config-ibp.h"
#include "ibp_ClientLib.h"
#include "ibp_nfu.h"
#ifdef STDC_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

void ibp_test_nfu(struct ibp_depot *depot) 
{
    struct ibp_attributes ls_att;
    time_t ls_now;
    struct ibp_timer timeout;
    IBP_set_of_caps caps1,caps2,caps3;
    char  buf[]="123456789123456789123456789123456789";
    int ret,i;
    char retBuf[60];
    PARAMETER paras[3];
    int len = strlen(buf);
    
    time(&ls_now);
    ls_att.duration = (ls_now + 5);
    ls_att.reliability = IBP_STABLE;
    ls_att.type = IBP_BYTEARRAY;

    timeout.ServerSync = 10000;
    timeout.ClientTimeout = 10000;

    if ( NULL == ( caps1 = IBP_allocate(depot,&timeout, 500,&ls_att)) ){
        fprintf(stderr," Allocate failed %d\n",IBP_errno);
        exit(-1);
    }
    if ( NULL == ( caps2 = IBP_allocate(depot,&timeout, 500,&ls_att)) ){
        fprintf(stderr,"failed %d\n",IBP_errno);
        exit(-1);
    }
    if ( NULL == ( caps3 = IBP_allocate(depot,&timeout, 500,&ls_att)) ){
        fprintf(stderr,"failed %d\n",IBP_errno);
        exit(-1);
    }
    if ( len != ( ret = IBP_store(caps1->writeCap,&timeout,buf,len)) ){
        fprintf(stderr,"failed %d\n",IBP_errno);
        exit(-1);
    }
    if ( len != ( ret = IBP_store(caps2->writeCap,&timeout,buf,len)) ){
        fprintf(stderr,"failed %d\n",IBP_errno);
        exit(-1);
    }
/*
    fprintf(stderr,"cap = %s\n",caps3->writeCap );
    fprintf(stderr,"store\n");

    if ( len != ( ret = IBP_store(caps3->writeCap,&timeout,buf,len)) ){
        fprintf(stderr,"failed %d\n",IBP_errno);
        exit(-1);
    }
*/
    //exit(-1);
    paras[0].ioType = IBP_REF_RD;
    paras[0].data = caps1->readCap;
    paras[0].offset = 0;
    paras[0].len = len;

    
    paras[1].ioType = IBP_REF_RD;
    paras[1].data = caps2->readCap;
    paras[1].offset = 0;
    paras[1].len = len;

    paras[2].ioType = IBP_REF_RDWR;
    paras[2].data = caps3->writeCap;
    paras[2].offset = 0;
    paras[2].len = len;

    if ( (ret = IBP_nfu_op(depot,1,3,paras,&timeout)) != IBP_OK ){
        fprintf(stderr," failed %d\n", IBP_errno);
        exit(-1);
    }
/*
    retBuf[32]= '\0';
    fprintf(stderr,"MD5 = %s len = %d \n",retBuf,strlen(retBuf));

    return;
*/
    if ( len != ( ret = IBP_load(caps3->readCap,&timeout,buf,len,0)) ){
        fprintf(stderr,"failed %d\n",IBP_errno);
        exit(-1);
    }
    
    for ( i = 0 ; i < len ; i ++ ){
        if ( buf[i] != 0 ){
            fprintf(stderr,"failed \n");
            exit(-1);
        }
    }

    return;
}

int ibp_quicktest(argc, argv)
     int argc;
     char **argv;
{
    int i;

    struct ibp_depot ls_depot1, ls_depot2;
    struct ibp_attributes ls_att1, ls_att2, ls_att3, ls_att4;
    IBP_set_of_caps ls_caps1=0, ls_caps2=0, ls_caps3=0, ls_caps4=0, ls_caps5=0;
    time_t ls_now;
    char lc_buf[1024 * 100];
    char lc_bufa[1024 * 25], lc_bufb[1024 * 25], lc_bufc[1024 * 25],
        lc_bufd[1024 * 25], lc_bufe[1024 * 25];
    int li_ret;
    struct ibp_capstatus ls_info;
    IBP_DptInfo ls_dinf1,ls_dinf2;

    struct ibp_timer ls_timeout;

    /*
     * check if smoketest called with two depots
     */

#if 0
    if ( IBP_setAuthenAttribute("client.pem","client.pem","adder") != IBP_OK ){
        fprintf(stderr,"authen setup failed!\n");
        exit(-1);
    }
#endif

    if (argc != 5) {
        fprintf(stderr,
                "usage: ibp-smoketest [host1] [port1] [host2] [port2]\n");
        exit(0);
    }

    //fprintf(stderr, "IBP-smoketest, initializing ...");

    strcpy(ls_depot1.host, argv[1]);
    ls_depot1.port = atoi(argv[2]);
    ls_depot1.rid = 0;

    strcpy(ls_depot2.host, argv[3]);
    ls_depot2.port = atoi(argv[4]);
    ls_depot2.rid = 0;

    /* 
     * set up the attributes
     */

    time(&ls_now);
    ls_att1.duration = (ls_now + 24 * 3600);
    ls_att1.reliability = IBP_STABLE;
    ls_att1.type = IBP_BYTEARRAY;

    ls_att2.duration = (ls_now + 24 * 3600);
    ls_att2.reliability = IBP_STABLE;
    ls_att2.type = IBP_BYTEARRAY;

    ls_att3.duration = (ls_now + 24 * 3600);
    ls_att3.reliability = IBP_STABLE;
    ls_att3.type = IBP_CIRQ;

    ls_att4.duration = (ls_now + 24 * 3600);
    ls_att4.reliability = IBP_STABLE;
    ls_att4.type = IBP_BYTEARRAY;
    /* 
     * set the memory buffer
     */

    memset(lc_buf, 123456789, 1024 * 100);
    memset(lc_bufa, 111111111, 1024 * 25);
    memset(lc_bufb, 222222222, 1024 * 25);
    memset(lc_bufc, 333333333, 1024 * 25);
    memset(lc_bufd, 444444444, 1024 * 25);
    memset(lc_bufe, 555555555, 1024 * 25);
    /*
     * set the timeout, not used anyway
     */

    ls_timeout.ServerSync = 10;
    ls_timeout.ClientTimeout = 10;


    ls_dinf2 = IBP_status(&ls_depot2, IBP_ST_INQ, &ls_timeout, NULL, 0, 0, 0);

    if (ls_dinf2 == NULL) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    printDptCfg(ls_dinf2);

    //ibp_test_nfu(&ls_depot1);

    //return 0;
    //fprintf(stderr, " done\n");

    /*
     * first step: allocation
     */

    fprintf(stderr, "trying to IBP_allocate ... ");

    ls_caps1 = IBP_allocate(&ls_depot1, &ls_timeout, 1024 * 1000, &ls_att1);
    if ((ls_caps1 == NULL)) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    ls_caps2 = IBP_allocate(&ls_depot2, &ls_timeout, 1024 * 1000, &ls_att2);
    if ((ls_caps2 == NULL)) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    ls_caps3 = IBP_allocate(&ls_depot1, &ls_timeout, 1024 * 100, &ls_att3);
    if ((ls_caps3 == NULL)) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    ls_caps4 = IBP_allocate(&ls_depot1, &ls_timeout, 1024 * 1000, &ls_att1);
    if ((ls_caps4 == NULL)) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    ls_caps5 = IBP_allocate(&ls_depot2, &ls_timeout, 1024 * 1000, &ls_att2);
    if ((ls_caps5 == NULL)) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr, "done\n");

    fprintf(stderr," trying IBP_store ...");
    strcpy(lc_buf, "123456789A");
    li_ret = IBP_store(ls_caps4->writeCap, &ls_timeout, lc_buf, 10);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    strcpy(lc_buf, "123456789");
    li_ret = IBP_store(ls_caps4->writeCap, &ls_timeout, lc_buf, 10);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    memset(lc_buf, 0, 200);
    li_ret = IBP_load(ls_caps4->readCap, &ls_timeout, lc_buf, 20, 0);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr, "Output: %s\n", lc_buf);
    fprintf(stderr,"Done\n");
    fprintf(stderr,"trying IBP_write..."); 
    for ( i = 9 ; i >=1 ;i -- ){
      sprintf(lc_buf,"%d",i);
      li_ret = IBP_write(ls_caps2->writeCap,&ls_timeout,lc_buf,1,i-1);
      if ( li_ret <= 0 ){
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
      }
    }
    fprintf(stderr,"IBP_write done\n");
    
    li_ret =
        IBP_manage(ls_caps2->manageCap, &ls_timeout, IBP_PROBE, IBP_READCAP,
                   &ls_info);

    if (li_ret < 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr,"cap size = %lu\n",ls_info.currentSize);
    memset(lc_buf, 0, 200);
    li_ret = IBP_load(ls_caps2->readCap, &ls_timeout, lc_buf, 9, 0);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr, "Output: %s\n", lc_buf);
    fprintf(stderr,"done\n");

    fprintf(stderr,"ls_caps4 = %s \n ls_caps5= %s\n",ls_caps4->readCap,ls_caps5->writeCap);
    li_ret =
        IBP_copy(ls_caps4->readCap, ls_caps5->writeCap, &ls_timeout,
                 &ls_timeout, 10, 0);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    memset(lc_buf, 0, 200);

    li_ret = IBP_load(ls_caps5->readCap, &ls_timeout, lc_buf, 10, 0);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr, "Output: %s\n", lc_buf);


    /* 
     * store the data on A1
     */

    fprintf(stderr, "trying to IBP_store ... ");

    li_ret = IBP_store(ls_caps1->writeCap, &ls_timeout, lc_buf, 1024 * 100);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }

    fprintf(stderr, "done\n");

    /*
     * copy data from A1 to A2
     */

    fprintf(stderr, "trying to IBP_copy ... ");

    li_ret =
        IBP_copy(ls_caps1->readCap, ls_caps2->writeCap, &ls_timeout,
                 &ls_timeout, 1024 * 100, 0);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }

    fprintf(stderr, "done\n");

    /* 
     * manage - probe A2
     */

    fprintf(stderr, "trying to IBP_manage-probe ... ");

    li_ret =
        IBP_manage(ls_caps2->manageCap, &ls_timeout, IBP_PROBE, IBP_READCAP,
                   &ls_info);

    if (li_ret < 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }

    fprintf(stderr, "done\n");

    /* 
     * status - inquire A2
     */

    fprintf(stderr, "trying to IBP_status-inquire ... ");

    ls_dinf2 = IBP_status(&ls_depot2, IBP_ST_INQ, &ls_timeout, NULL, 0, 0, 0);

    if (ls_dinf2 == NULL) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    printDptCfg(ls_dinf2);
    
    fprintf(stderr, "trying to IBP_status-inquire ... ");

    ls_dinf2 = IBP_status(&ls_depot1, IBP_ST_INQ, &ls_timeout, NULL, 0, 0, 0);

    if (ls_dinf2 == NULL) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    printDptCfg(ls_dinf2);


    fprintf(stderr, "done\n");


    /*
     * load data from A2
     */

    fprintf(stderr, "trying to IBP_load ... ");

    li_ret = IBP_load(ls_caps2->readCap, &ls_timeout, lc_buf, 1024 * 50, 0);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! first time, %d\n", IBP_errno);
        exit(1);
    }

    li_ret =
        IBP_load(ls_caps2->readCap, &ls_timeout, lc_buf, 1024 * 50,
                 1024 * 50);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! second time, %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr, "done\n");


    /*
     * store in the cirq
     */

    fprintf(stderr, "trying to IBP_store, CIRQ this time ... ");

    li_ret = IBP_store(ls_caps3->writeCap, &ls_timeout, lc_bufa, 1024 * 25);

    if (li_ret <= 0) {
        fprintf(stderr, "failed, a! %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_store(ls_caps3->writeCap, &ls_timeout, lc_bufb, 1024 * 25);

    if (li_ret <= 0) {
        fprintf(stderr, "failed, b! %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_store(ls_caps3->writeCap, &ls_timeout, lc_bufc, 1024 * 25);

    if (li_ret <= 0) {
        fprintf(stderr, "failed, c! %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_store(ls_caps3->writeCap, &ls_timeout, lc_bufd, 1024 * 25);

    if (li_ret <= 0) {
        fprintf(stderr, "failed, d! %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_store(ls_caps3->writeCap, &ls_timeout, lc_bufe, 1024 * 25);

    if (li_ret <= 0) {
        fprintf(stderr, "failed, e! %d\n", IBP_errno);
        exit(1);
    }

    fprintf(stderr, "done\n");

    /*
     * load from the cirq
     */

    fprintf(stderr, "trying to IBP_load, cirq ... ");

    li_ret = IBP_load(ls_caps3->readCap, &ls_timeout, lc_buf, 1024 * 25, 0);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! first time, %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_load(ls_caps3->readCap, &ls_timeout, lc_buf, 1024 * 25, 0);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! second time, %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_load(ls_caps3->readCap, &ls_timeout, lc_buf, 1024 * 25, 0);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! third time, %d\n", IBP_errno);
        exit(1);
    }

    li_ret = IBP_load(ls_caps3->readCap, &ls_timeout, lc_buf, 1024 * 25, 0);

    if (li_ret <= 0) {
        fprintf(stderr, "failed! fourth time, %d\n", IBP_errno);
        exit(1);
    }

    fprintf(stderr, "done\n");
/*
*/
    /*
     * cleam it up
     */

    fprintf(stderr, "trying to IBP_manage-decr, ... ");

    li_ret =
        IBP_manage(ls_caps1->manageCap, &ls_timeout, IBP_DECR, IBP_READCAP,
                   NULL);

    if (li_ret < 0) {
        fprintf(stderr, "failed! first time, %d\n", IBP_errno);
        exit(1);
    }

    li_ret =
        IBP_manage(ls_caps2->manageCap, &ls_timeout, IBP_DECR, IBP_READCAP,
                   NULL);

    if (li_ret < 0) {
        fprintf(stderr, "failed! second time, %d\n", IBP_errno);
        exit(1);
    }

    li_ret =
        IBP_manage(ls_caps3->manageCap, &ls_timeout, IBP_DECR, IBP_READCAP,
                   NULL);

    if (li_ret < 0) {
        fprintf(stderr, "failed! third time, %d\n", IBP_errno);
        exit(1);
    }

    fprintf(stderr,"Done \n");

    /*
     * test IBP_status
     */

  /* 
   * test IBP_status 
   */
  fprintf (stderr, "trying to IBP_status ... ");
  ls_dinf1 = IBP_status (&ls_depot1, IBP_ST_INQ,&ls_timeout, NULL, 0, 0, 0);
  if ( ls_dinf1 == NULL ){
    fprintf(stderr,"error in IBP_status = %d!\n",IBP_errno);
    exit(-1);
  }
  
  ls_dinf2 = IBP_status (&ls_depot2, IBP_ST_INQ, &ls_timeout, NULL, 30, 40, 36000);
  if ( ls_dinf2 == NULL ){
    fprintf(stderr,"error in IBP_status = %d!\n",IBP_errno);
    exit(-1);
  }
  fprintf(stderr," Done\n");
  
  if ( ls_dinf1->majorVersion > 1.3 ){
      fprintf(stderr,"trying to IBP_nfu_op ...");
      ibp_test_nfu(&ls_depot1);
      fprintf(stderr," Done\n");
  };

    IBP_freeCapSet(ls_caps1);
    IBP_freeCapSet(ls_caps2);
    IBP_freeCapSet(ls_caps3);
    IBP_freeCapSet(ls_caps4);
    IBP_freeCapSet(ls_caps5);
    return IBP_OK;

}


int main(argc, argv)
     int argc;
     char **argv;
{

    int li_ret;

    li_ret = ibp_quicktest(argc, argv);

    return li_ret;
}
