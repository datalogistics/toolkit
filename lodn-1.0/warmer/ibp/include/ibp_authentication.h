/*
 *           IBP Client version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *          Authors: Y. Zheng A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 1999 All Rights Reserved
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
    
/*
 * ibp_authenticate.c
 */ 
#ifndef IBP_AUTHENTICATION_H
#define IBP_AUTHENTICATION_H

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# include "ibp_os.h"

# ifdef HAVE_PTHREAD_H
# include <pthread.h>
# endif

#ifdef IBP_AUTHENTICATION
#include <openssl/ssl.h>
#endif
    
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifdef IBP_AUTHENTICATION

typedef struct {    
    char            *certificateFileName;
    char            *privateKeyFileName;
    char            *privateKeyPassword;
}IBP_AUTHEN_CTX; 

#ifdef HAVE_PTHREAD_H
    SSL_CTX *_ibp_getSSLCTX();
#define ibpSSLCTX _ibp_getSSLCTX()
#else
    SSL_CTX *ibpSSLCTX;
#endif

SSL_CTX *IBP_getSSLCTX();
#endif

int ibpCliAuthentication(int fd);

#ifdef __cplusplus
} /* extern "C" */
#endif
      
#endif /* IBP_AUTHENTICATION_H */      
