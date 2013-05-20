/*
 *           IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *         Authors: Y. Zheng, A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 2003 All Rights Reserved
 *
 *                              NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appear in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * Neither the Institution (University of Tennessee) nor the Authors 
 * make any representations about the suitability of this software for 
 * any purpose. This software is provided ``as is'' without express or
 * implied warranty.
 *
 */

# include "config-ibp.h"
# ifdef STDC_HEADERS
# include <sys/types.h>
# include <assert.h>
# endif /* STDC_HEADERS */
# ifdef HAVE_DLFCN_H
# include <dlfcn.h>
# endif
# ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
# endif
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_mdns.h"
# include <string.h>
# include <stdlib.h>
# include <unistd.h>
# include <errno.h>
# include <signal.h>
# include <fcntl.h>

#if IBP_ENABLE_BONJOUR

extern mDNS mDNSStorage;
extern mDNS_PlatformSupport PlatformStorage;
extern PosixService *gServiceList;
extern int gServiceID;
static const char  ibpNamePrefix[]="IBPDepot_";
static const char  ibpProtType[]="_ibprotocol._tcp.";
static const char  ibpDefaultDom[]="local.";
static volatile mDNSBool gStopNow;

static void ibpSigIntHandler( int sig)
{
    gStopNow = mDNStrue;
}

static void ibpSigQuitHandler( int sig)
{
    mDNS_Close(&mDNSStorage);
    exit(0);
}

static void ibpHandleSignal()
{
    signal(SIGINT,ibpSigIntHandler);
    signal(SIGQUIT,ibpSigQuitHandler);
    signal(SIGTERM,ibpSigQuitHandler);
}

static int ibpRegisterService(JRB resources)
{
    mStatus status;
    RESOURCE *rs;
    char name[128];
    char type[256];
    const char *dom = ibpDefaultDom;
    JRB  node;
    int  i=0;

    i = 0;
    jrb_traverse(node,resources){
        rs = (RESOURCE *)((jrb_val(node)).v);
        if ( rs->zeroconf == 0 ) { continue; };
        i=1;
        sprintf(name,"%s%d@%d",ibpNamePrefix,rs->RID,glb.IbpPort);
        sprintf(type,"%s",ibpProtType);
        status = RegisterOneService(name,type,dom,NULL,0,glb.IbpPort);
        if ( status != mStatus_NoError){
            fprintf(stderr,"mDNS: Failed to register server, name = %s, type = %s, port = %d\n",name,type,glb.IbpPort);
            status = mStatus_NoError;
        }
    }

    if ( i == 0 ){ exit(0); }
    return 0;
}

int ibpStartMDNS(JRB resources)
{
    pid_t pid;
    mStatus status;

    /*
     * create mdns child process 
     */
    pid = fork();
    if ( pid < 0 ) { 
        /* error */
        return ( -1);
    }else if ( pid > 0 ){
        /* parent process */
        return (pid);
    };

    /* child process */
    status = mDNS_Init(&mDNSStorage,&PlatformStorage,
                        mDNS_Init_NoCache,mDNS_Init_ZeroCacheSize,
                        mDNS_Init_AdvertiseLocalAddresses,
                        mDNS_Init_NoInitCallback,
                        mDNS_Init_NoInitCallbackContext);
    if ( status != mStatus_NoError ){
        exit -1;
    }

    if ( ibpRegisterService(resources) != 0 ){
        exit -1;
    }

    ibpHandleSignal();

    while ( !gStopNow ){
        int nfds = 0;
        fd_set readfds;
        struct timeval timeout;
        int result;

        FD_ZERO(&readfds);

        timeout.tv_sec = 0x3FFFFFFF;
        timeout.tv_usec = 0;

        mDNSPosixGetFDSet(&mDNSStorage,&nfds,&readfds,&timeout);

        result = select(nfds,&readfds,NULL,NULL,&timeout);
        if ( result < 0){
            if ( errno != EINTR){
                gStopNow = mDNStrue;
            }
        }else{
            mDNSPosixProcessFDSet(&mDNSStorage,&readfds);
        }
        if ( getppid() == 1 ){
            gStopNow = mDNStrue;
        }
    }

    DeregisterOurServices();
    mDNS_Close(&mDNSStorage);

    exit(0);
}

#endif /* IBP_ENABLE_BONJOUR */
