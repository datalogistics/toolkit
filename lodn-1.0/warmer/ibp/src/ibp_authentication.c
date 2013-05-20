#include "config-ibp.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "ibp_protocol.h"
#include "ibp_authentication.h"
#include "ibp_errno.h"


#ifdef IBP_AUTHENTICATION

IBP_AUTHEN_CTX  *ibpGlbAuthenCtx = NULL;

#ifdef HAVE_PTHREAD_H
    pthread_once_t  ibpSSLCTXInit = PTHREAD_ONCE_INIT;
    pthread_once_t  ibpAuthenCtxInit = PTHREAD_ONCE_INIT;
    static pthread_key_t ibpSSLCTXKey;
    void ibpFreeSSLCTX(void *ptr) { if ( ptr != NULL ) { SSL_CTX_free((SSL_CTX*)ptr); } };
    void ibpCreateSSLCTXKey(void) { pthread_key_create(&ibpSSLCTXKey,ibpFreeSSLCTX); };
#else
    SSL_CTX *ibpSSLCTX = NULL; 
#endif

static void _ibpCreateAuthenCtx()
{
   IBP_AUTHEN_CTX *ctx = NULL ;

   if ( (ctx = (IBP_AUTHEN_CTX*)calloc(sizeof(IBP_AUTHEN_CTX),1)) == NULL){
        fprintf(stderr,"Out of Memory!\n");
        exit(-1);
   }
   ctx->certificateFileName = NULL;
   ctx->privateKeyFileName = NULL;
   ctx->privateKeyPassword = NULL;
   ibpGlbAuthenCtx = ctx;

   /* initialize ssl system */
   SSL_library_init();

   return;
}

static void ibpInitSSLCTX()
{
#ifdef HAVE_PTHREAD_H
    pthread_once(&ibpSSLCTXInit,ibpCreateSSLCTXKey);
#else
    ibpSSLCTX = NULL;
#endif
    return;
}

static void ibpInitAuthenCtx()
{
#ifdef HAVE_PTHREAD_H
    pthread_once(&ibpAuthenCtxInit,_ibpCreateAuthenCtx);
#else
    if ( ibpGlbAuthenCtx != NULL ){
        _ibpCreateAuthenCxt();
    };
#endif
    ibpInitSSLCTX();
    return ; 
}

static int password_cb ( char *buf, int num, int rwflag, void *userdata);

static SSL_CTX *_ibpGetSSLCTX()
{
    SSL_CTX *ctx;

#ifdef HAVE_PTHREAD_H
    ctx = (SSL_CTX*)pthread_getspecific(ibpSSLCTXKey);
    return (ctx);
#else
    return ibpSSLCTX;
#endif
}

static SSL_CTX *ibpCreateSSLCTX();
static void ibpDestroySSLCTX( SSL_CTX *ctx);

static SSL_CTX *ibpGetSSLCTX()
{
    SSL_CTX *ctx;

    ibpInitAuthenCtx();
    ctx = _ibpGetSSLCTX();
    
    if ( ibpGlbAuthenCtx->certificateFileName != NULL && ctx == NULL ){
        ctx = ibpCreateSSLCTX();
        if ( ctx != NULL ){
            if ( !SSL_CTX_use_certificate_chain_file(ctx,ibpGlbAuthenCtx->certificateFileName)){
                goto bail;
            }
    
            SSL_CTX_set_default_passwd_cb_userdata(ctx,(void*)ibpGlbAuthenCtx->privateKeyPassword);
            SSL_CTX_set_default_passwd_cb(ctx,password_cb);

            if ( ibpGlbAuthenCtx->privateKeyFileName != NULL ){
                if ( !SSL_CTX_use_PrivateKey_file(ctx,ibpGlbAuthenCtx->privateKeyFileName,SSL_FILETYPE_PEM)) {
                    goto bail;
                }

                if ( !SSL_CTX_check_private_key(ctx) ){
                    goto bail;
                }
            }
        }
    }

    return ctx;

bail:
    ibpDestroySSLCTX(ctx);
    return NULL;
}

static SSL_CTX *ibpCreateSSLCTX()
{
    SSL_CTX *ctx;
    SSL_METHOD *meth;
   
    meth = SSLv23_method();
    ctx = SSL_CTX_new(meth);
    if ( ctx == NULL ){
       fprintf(stderr,"can't create ssl ctx struct !\n");
       return NULL;
    }

#ifdef HAVE_PTHREAD_H
    pthread_setspecific(ibpSSLCTXKey,ctx);
#else
    ibpSSLCTX = ctx;
#endif
    return (ctx);
}

static void ibpDestroySSLCTX( SSL_CTX *ctx)
{
    if ( ctx != NULL ){
        SSL_CTX_free(ctx);   
    };

#ifdef HAVE_PTHREAD_H
    pthread_setspecific(ibpSSLCTXKey,NULL);
#else
    ibpSSLCTX = NULL;
#endif

};

static int password_cb ( char *buf, int num, int rwflag, void *userdata)
{
    char *pemPasswd;
    static char empty_pw[]="\0";
 
    pemPasswd = (char*)userdata;
    if ( pemPasswd == NULL ){
	pemPasswd = empty_pw;	
    }
    if ( num < strlen(pemPasswd) + 1 )
        return 0;

    strcpy(buf,pemPasswd);
    return(strlen(pemPasswd));
}

int IBP_setAuthenAttribute(char *certFile, char* privateKeyFile, char *passwd)
{
    SSL_CTX *ctx;
    int     ret = IBP_OK;

    ibpInitAuthenCtx();
    ctx = _ibpGetSSLCTX();
    if ( ctx == NULL ){
        ctx = ibpCreateSSLCTX();
    }
    if ( ctx == NULL ){
        return ( IBP_E_CRT_AUTH_FAIL);
    }

    SSL_CTX_set_default_passwd_cb_userdata(ctx,(void*)passwd);
    SSL_CTX_set_default_passwd_cb(ctx,password_cb);

    if ( privateKeyFile != NULL ){
        if ( 1 != SSL_CTX_use_PrivateKey_file(ctx,privateKeyFile,SSL_FILETYPE_PEM)) {
            ret = IBP_E_INVALID_PRIVATE_KEY_PASSWD;
            goto bail;
        }

    }
    
    if ( certFile != NULL ){
        if ( 1 != SSL_CTX_use_certificate_chain_file(ctx,certFile)){
            fprintf(stderr,"error in cert\n ");
            ret = IBP_E_INVALID_CERT_FILE;
            goto bail;
        }
    }

    if ( certFile != NULL && privateKeyFile != NULL ){
        if ( 1 != SSL_CTX_check_private_key(ctx) ){
            fprintf(stderr,"error in check\n ");
            ret = IBP_E_INVALID_PRIVATE_KEY_FILE;
            goto bail;
        }
    }

    if ( ibpGlbAuthenCtx->certificateFileName == NULL ){
        ibpGlbAuthenCtx->certificateFileName = strdup(certFile);
        ibpGlbAuthenCtx->privateKeyFileName = strdup(privateKeyFile);
        ibpGlbAuthenCtx->privateKeyPassword = strdup(passwd);
    }

    return ret;

bail:
    return (ret);
}

#endif

#ifndef IBP_AUTHENTICATION
int IBP_setAuthenAttribute(char *certFile, char* privateKeyFile, char *passwd)
{
    return (IBP_E_AUTHEN_NOT_SUPPORT);
}
#endif

int ibpCliAuthentication(int fd)
{
    int ret = IBP_OK;
    char buf[128];
    int  resp;
    int flags; 
     
#ifdef IBP_AUTHENTICATION
    SSL_CTX *ctx;
    SSL     *ssl;
#endif

#ifndef IBP_AUTHENTICATION
    ret = IBP_E_AUTHEN_NOT_SUPPORT;
#else
    ctx = ibpGetSSLCTX();
    if ( ctx != NULL ){
        ret = IBP_OK;
    }else{
        ret = IBP_E_AUTHEN_NOT_SUPPORT;
    }
#endif
    sprintf(buf,"%d \n",ret);
    resp = write_data(fd,strlen(buf),buf,0);
    if ( resp == 0  && IBP_errno != IBP_OK ){
        return (resp);
    }
    
    if ( ret != IBP_OK) { 
        IBP_errno = ret;
        return (ret) ; 
    };

    /* listen on ssl */
#ifdef IBP_AUTHENTICATION 
    ssl = SSL_new(ctx);
    SSL_set_fd(ssl,fd);
    flags = fcntl(fd,F_GETFL,0);
    fcntl(fd,F_SETFL,flags&~(O_NONBLOCK));
    
    if ( (ret = SSL_accept(ssl)) <= 0 ){
        SSL_load_error_strings();
        ERR_print_errors_fp(stderr);
        ret = SSL_get_error(ssl,ret);
        IBP_errno = IBP_E_AUTHENTICATION_FAILED;
        fcntl(fd,F_SETFL,flags);
        return (IBP_errno);
    }
    
    fcntl(fd,F_SETFL,flags);
    IBP_errno = IBP_OK;
    SSL_free(ssl);
    return (IBP_OK);
#endif

}

