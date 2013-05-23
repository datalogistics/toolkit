#include "config-ibp.h"
#include "ibp_ClientLib.h"
#ifdef STDC_HEADERS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif


int ibp_quicktest(argc, argv)
     int argc;
     char **argv;
{

    struct ibp_depot ls_depot1, ls_depot2;
    struct ibp_attributes ls_att1, ls_att2, ls_att3, ls_att4;
    IBP_set_of_caps ls_caps1, ls_caps2, ls_caps3, ls_caps4, ls_caps5,
        ls_caps6;
    time_t ls_now;
    char lc_buf[1024 * 100];
    char lc_bufa[1024 * 25], lc_bufb[1024 * 25], lc_bufc[1024 * 25],
        lc_bufd[1024 * 25], lc_bufe[1024 * 25];
    int li_ret;
    struct ibp_capstatus ls_info;
    IBP_DptInfo ls_dinf;

    struct ibp_timer ls_timeout;

    /*
     * check if smoketest called with two depots
     */

    if (argc != 5) {
        fprintf(stderr,
                "usage: ibp-smoketest [host1] [port1] [host2] [port2]\n");
        exit(0);
    }

    fprintf(stderr, "IBP-smoketest, initializing ...");

    strcpy(ls_depot1.host, argv[1]);
    ls_depot1.port = atoi(argv[2]);

    strcpy(ls_depot2.host, argv[3]);
    ls_depot2.port = atoi(argv[4]);

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

    fprintf(stderr, " done\n");

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

    ls_caps6 =
        IBP_allocate(&ls_depot2, &ls_timeout, 1 * 1024 * 1024 * 1024,
                     &ls_att4);
    if ((ls_caps6 == NULL)) {
        fprintf(stderr, "\n 1G allocation failed! Big cap test ignored! %d\n",
                IBP_errno);
    }
    fprintf(stderr, "done\n");


    strcpy(lc_buf, "123456789");
    li_ret = IBP_store(ls_caps4->writeCap, &ls_timeout, lc_buf, 10);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    memset(lc_buf, 0, 200);
    li_ret = IBP_load(ls_caps4->readCap, &ls_timeout, lc_buf, 10, 0);
    if (li_ret <= 0) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }
    fprintf(stderr, "Output: %s\n", lc_buf);

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

    ls_dinf = IBP_status(&ls_depot2, IBP_ST_INQ, &ls_timeout, NULL, 0, 0, 0);

    if (ls_dinf == NULL) {
        fprintf(stderr, "failed! %d\n", IBP_errno);
        exit(1);
    }

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

    if (ls_caps6 != NULL) {
        fprintf(stderr, "trying to IBP_manage-probe big cap size (1G),...");
        li_ret =
            IBP_manage((IBP_cap) ls_caps6->manageCap, &ls_timeout, IBP_PROBE,
                       IBP_READCAP, &ls_info);
        if (li_ret < 0) {
            fprintf(stderr, "faild in getting capability information, %d\n",
                    IBP_errno);
            exit(1);
        }
        if (ls_info.maxSize != 1 * 1024 * 1024 * 1024) {
            fprintf(stderr, "can't get correct capability size!\n");
            exit(1);
        }
        fprintf(stderr, "Size = %lu  done\n", ls_info.maxSize);
    }

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


    if (ls_caps6 != NULL) {
        li_ret =
            IBP_manage(ls_caps6->manageCap, &ls_timeout, IBP_DECR,
                       IBP_READCAP, NULL);

        if (li_ret < 0) {
            fprintf(stderr, "failed! third time, %d\n", IBP_errno);
            exit(1);
        }
    }



    IBP_freeCapSet(ls_caps1);
    IBP_freeCapSet(ls_caps2);
    IBP_freeCapSet(ls_caps3);
    IBP_freeCapSet(ls_caps4);
    IBP_freeCapSet(ls_caps5);
    if (ls_caps6 != NULL) {
        IBP_freeCapSet(ls_caps6);
    }
    fprintf(stderr, "done\n");
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
