#include "config-ibp.h"
#include <assert.h>
#include "ibp_server.h"
#include "ibp_connection.h"

#ifdef IBP_AUTHENTICATION
#include <openssl/ssl.h>

#ifdef HAVE_PTHREAD_H
#include <pthread.h>

static pthread_once_t ibpSrvSSLCTXInit = PTHREAD_ONCE_INIT;
static pthread_key_t  ibpSrvSSLCTXKey;


static void ibpSrvFreeSSLCTX(void *ptr) { if ( ptr != NULL ) { SSL_CTX_free((SSL_CTX*)ptr); } };

void ibpCreateSrvSSLCTXKey() 
{
    pthread_key_create(&ibpSrvSSLCTXKey,ibpSrvFreeSSLCTX);
    SSL_library_init();

}

void ibpCreateSrvSSLCTX(char *calistFile)
{
    SSL_CTX *ctx;
    SSL_METHOD *meth;

    pthread_once(&ibpSrvSSLCTXInit,ibpCreateSrvSSLCTXKey);
    ctx = (SSL_CTX*)pthread_getspecific(ibpSrvSSLCTXKey);
    if ( ctx == NULL ){
        SSL_library_init();     
        meth = SSLv23_method();
        ctx = SSL_CTX_new(meth);
        if ( ctx == NULL ){
            fprintf(stderr,"Out of memory!\n");
            exit(-1);
        }
        SSL_CTX_set_verify(ctx,SSL_VERIFY_PEER,0);
        pthread_setspecific(ibpSrvSSLCTXKey,ctx);
    }

    if ( calistFile != NULL ){
        if ( !(SSL_CTX_load_verify_locations(ctx,calistFile,0)) ){
            fprintf(stderr,"Can't load CA list file!\n");
            exit(-1);
        }
    }

    return;
}

SSL_CTX *ibpGetSrvSSLCTX()
{
    SSL_CTX *ctx;

    pthread_once(&ibpSrvSSLCTXInit,ibpCreateSrvSSLCTXKey);
    ctx = (SSL_CTX*)pthread_getspecific(ibpSrvSSLCTXKey);
    return(ctx);
}

int ibpSrvAuthentication( int fd,IBP_CONNECTION *conn)
{
    int flags;
    char    buf[128];
    int     msgLen = 0;
    int     left,n;
    char    *ptr;
    SSL     *ssl;
    SSL_CTX *ctx;
    X509    *peer;
    char    peer_cn[257];

    assert(conn->authenticated == 0);

    /* set socket in block mode */
    if ( (flags = fcntl(fd,F_GETFL,0)) <0 ){
        fprintf(stderr,"Can't get socket status!\n");
        return(-1);
    }
    if ( fcntl(fd,F_SETFL,flags&(~O_NONBLOCK)) <  0){
        fprintf(stderr,"Can't set block socket!\n");
        return(-1);
    }

    /* send authentication request back to client */
    sprintf(buf,"%d \n",IBP_AUTHENTICATION_REQ);
    msgLen = strlen(buf);
    left = msgLen;
    ptr = buf;

    while ( left > 0 ){
       n = write(fd,ptr,left);
       if ( n <= 0 ){
         return(IBP_E_SOCK_WRITE);
       }
       ptr += n;
       left -= n;
    }

    
    /* receive client response */
    memset(buf,0,128);
    ptr = buf;
    do {
        n = read(fd,ptr,1);
        if ( n <= 0 ){
            return(IBP_E_SOCK_READ);
        }
        if ( ptr-buf > 127 ){
            return(IBP_E_BAD_FORMAT);
        }
    }while ( *(ptr++) != '\n');

    *ptr = '\0';
    if ( sscanf(buf,"%d",&n) != 1){
        return ( IBP_E_BAD_FORMAT);
    }

    if ( n != IBP_OK ){
        return (n);
    }


    /* connect to client using ssl */
    ctx = ibpGetSrvSSLCTX();
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl,fd);
    if ( SSL_connect(ssl) <= 0 ){
        SSL_free(ssl);
        return(IBP_E_AUTHENTICATION_FAILED);
    }

    /* get client certificate */
    if ( SSL_get_verify_result(ssl)!= X509_V_OK){
        SSL_free(ssl);
        return(IBP_E_AUTHENTICATION_FAILED);
    }

    peer=SSL_get_peer_certificate(ssl);
    X509_NAME_get_text_by_NID(X509_get_subject_name(peer),
                              NID_commonName,peer_cn,256);
    
    if ( conn->cn != NULL ){
        free(conn->cn);
    }
    conn->cn = strdup(peer_cn);
    if ( conn->cn == NULL ){
        fprintf(stderr,"Out of memory!\n");
        exit(-1);
    }

    conn->authenticated = 1;

    SSL_free(ssl);
    fcntl(fd,F_SETFL,flags);

    return (IBP_OK);
}

#endif

#endif


