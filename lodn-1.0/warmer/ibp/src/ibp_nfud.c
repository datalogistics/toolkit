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

/*
 * ibp_nfud.c
 *
 * IBP NFU server side code. Version name: primeur
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
# include <unistd.h>
# include <stdlib.h>
# ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
# endif
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_thread.h"
# include "ibp_nfud.h"
# include "ibp_request.h"
# include "ibp_connection.h"
# include "ibp_nfu_para.h"

static int _ParseNfuParameters(int sockfd, S_PARA ** paras, int nparas);
static void _FreeParas(S_PARA * paras, int nparas);
static int _CheckNfuParameters(S_PARA * paras, int nparas);
static int  _CheckCapPara(S_PARA *para);
static int  _CheckDupCaps(S_PARA *para, int npara);
static int  _LockWriteCaps(S_PARA *para, int npara);
static int  _OpenCaps(S_PARA *para, int npara);
/*static int _CheckCapSpace(S_PARA *paras,NFU_PARA *nfu_paras, int nparas);*/
static void _ReleaseCaps(S_PARA *para, int npara, int locked);
/*static void _UpdateBuffer( S_PARA *para, NFU_PARA *nfu_para , int npara);*/
static void _DecCapRef( S_PARA *para, int npara);
static void _SendParametersToClient( int sockfd,S_PARA *para, int npara);

int load_nfu(JRB libs, JRB ops, char *filename)
{
    IS cfg;
    int lineno = 0, opcode;
    void *lib_handle;
    JRB lib_node, op_node;
    OP op;
    char *tmp;
    Jval jval;
    NFU_OP *operation;

    /*fprintf(stderr,"IMPDE nfu file name  = %s\n",filename);*/

    if ( filename == NULL ||
         strlen(filename) == 0 ){
        return ( IBP_OK);
    }

    if ((cfg = (IS) new_inputstruct(filename)) == NULL) {
        fprintf(stderr, " Can't open NFU configuration file %s \n", filename);
        return -1;
    }

    while (get_line(cfg) >= 0) {
        lineno++;
        if (cfg->fields[0][0] == '#') {
            continue;
        }
        if ( cfg->NF == 0 ){
          continue;
        }
        if (cfg->NF < 4) {
            fprintf(stderr, " Unrecognized format at line %d in %s, \n",
                    lineno, filename);
            jettison_inputstruct(cfg);
            return (E_GETCONFIG);
        }

        sscanf(cfg->fields[0],"%x",&opcode);
        /*opcode =  atoi(cfg->fields[0]);*/

        /*fprintf(stderr,"IMPDE opcode = %d\n",opcode);*/
        /*
         * get library handle
         */
        lib_node = jrb_find_str(libs, cfg->fields[2]);
        if (lib_node == NULL) {
            lib_handle = dlopen(cfg->fields[2], RTLD_LAZY);
            if ((tmp = strdup(cfg->fields[2])) == NULL) {
                fprintf(stderr, " Out of memory \n");
                jettison_inputstruct(cfg);
                return (E_GETCONFIG);
            }
            if (lib_handle == NULL) {
                fprintf(stderr, "Can't load library %s: %s\n", tmp,
                        dlerror());
                jettison_inputstruct(cfg);
                return (E_GETCONFIG);
            }
            jval.v = (void *) lib_handle;
            jrb_insert_str(libs, tmp, jval);
        }
        else {
            lib_handle = (void *) (jrb_val(lib_node)).v;
        }

        /*
         * get function handle from library
         */
        op = (OP) dlsym(lib_handle, cfg->fields[1]);
        if (dlerror() != NULL) {
            fprintf(stderr, " Can't find function %s in %s !\n",
                    cfg->fields[1], cfg->fields[2]);
            jettison_inputstruct(cfg);
            return (E_GETCONFIG);
        }

        /*
         * insert into ops jrb
         */
        op_node = jrb_find_int(ops, opcode);
        if (op_node != NULL) {
            continue;
            /*
            fprintf(stderr, " Duplicated NFU code in line %d !\n", lineno);
            jettison_inputstruct(cfg);
            return (E_GETCONFIG);
            */
        }
        if ( (operation = calloc(sizeof(NFU_OP),1)) == NULL ){
          fprintf(stderr,"Out of memory\n");
          jettison_inputstruct(cfg);
          return(E_GETCONFIG);
        }
        operation->op = op ;
        operation->is_thread = atoi(cfg->fields[3]);
        jval.s = (void *) operation;
        jrb_insert_int(ops, opcode, jval);
    }

    glb.nfu_conf_modified = 0;
    jettison_inputstruct(cfg);
    return (IBP_OK);
}

int run_nfu_process ( int sockfd, OP op, S_PARA *paras, int nParas,NFU_PARA *nfu_para)
{
  int ret;
  int pipes[2];
  pid_t pid;
  int size, left, n;
  char *ptr;
 
  if ( 0 != pipe(pipes) ){
    return (IBP_E_GENERIC);
  }

  
  pid = fork();
  if ( pid == -1 ){
    close(pipes[0]);
    return ( IBP_E_GENERIC);
  }
  if ( pid == 0 ) { /* child process */
     signal(SIGPIPE,SIG_DFL);
     signal(SIGSEGV,SIG_DFL);
     signal(SIGINT,SIG_DFL);
     signal(SIGHUP,SIG_DFL);
     close(pipes[0]);
     ret = run_nfu_thread(sockfd,op,paras,nParas,nfu_para);
     write(pipes[1],&ret,sizeof(int));
     if ( ret == IBP_OK ){
        write(pipes[1],(char*)(nfu_para),sizeof(NFU_PARA)*nParas);
     }
     exit(0) ; 
  }else{ /* parents */

#if 0
    FD_ZERO(&rset);
    FD_SET(rset,sockfd);
    FD_SET(rset,pipes[0]);

    if ( sockfd > pipes[0] ){
      maxfd = sockfd;
    }else{
      maxfd = pipes[0];
    }
   
    n = select(maxfd+1,&rset,NULL,NULL,NULL);
    if ( n < 0 ){
      if ( errno == EINTR ){
        continue;
      }else{
        perror("panic! select error:");
        kill(pid,SIGKILL);
        exit (-1); 
      }
    }

    if ( FD_ISSET(conn->fd , &rset) ){
      /* if ( 0 == read(fd,&msg,1) ){ */
        kill(pid,SIGKILL);
        ret = IBP_E_GENERIC;   
        goto bail;
      /* } */  
    }
    if ( FD_ISSET(pipes[0],&rset)){
#endif
    close(pipes[1]);
    n = read(pipes[0],&ret,sizeof(int)); 
    if ( n != sizeof(int) ){
      ret = IBP_E_GENERIC;
      kill(pid,SIGKILL);
      goto bail;
    }

    if ( ret == IBP_OK ){
      size = sizeof(NFU_PARA)*nParas;
      left = size;
      ptr = (char*)nfu_para;
      while ( left > 0 ){
        n = read(pipes[0],ptr,left);
        if ( n <= 0 ) {
          break;
        }else{
          left = left - n;
          ptr = ptr + n;
        }
      }
      if ( left > 0 ) {
        ret = IBP_E_GENERIC;
        kill(pid,SIGKILL);
        goto bail;
      }
    }
  } 

bail:
  close(pipes[0]);
  return (ret); 
}

int run_nfu_thread( int sockfd, OP op, S_PARA *paras, int nParas, NFU_PARA *nfu_paras )
{
    int ret, j ;
    char        msg[20];
    /*
     * open capabilities
     */
    if ((ret = _OpenCaps(paras, nParas)) != IBP_OK ){
        return ret;
    }

    /*
     * create array of parameters
    if ( ( nfu_paras = (NFU_PARA*)calloc(sizeof(NFU_PARA),nParas)) == NULL ){
        ret = IBP_E_ALLOC_FAILED;
        return ret;
    }
    */

 
    /*
     * call nfu function
     */
    for ( j = 0 ; j < nParas ; j++){
        switch ( paras[j].io_type ){
        case IBP_VAL_IN:
        case IBP_VAL_OUT:
        case IBP_VAL_INOUT:
            nfu_paras[j].data = paras[j].data;
            nfu_paras[j].len = paras[j].len;
            break;
        case IBP_REF_RD:
        case IBP_REF_WR:
        case IBP_REF_RDWR:
            nfu_paras[j].data = paras[j].buf;
            nfu_paras[j].len = paras[j].len;
            break;
        }
    }

    if ( (ret = op(nParas,nfu_paras)) != 0 ){
        free(nfu_paras);    
        return ret;
    }

    ret = IBP_OK;
   
    sprintf(msg,"%d \n",ret);
    Write(sockfd,msg,strlen(msg));
    _SendParametersToClient(sockfd,paras,nParas);
    
    return (IBP_OK);
} 

void handle_nfu(IBP_REQUEST *request, int pi_nbt) /* ibp_command * cmd) */
{
    S_PARA      *paras = NULL;
    NFU_PARA    *nfu_paras = NULL;
    OP          op;
    NFU_OP      *operation;
    JRB         op_node;
    int         ret = IBP_OK;
    int         i, nParas;
    int         _locked = 0;
    char        msg[20];
    int         sockfd; 
    ibp_command *cmd;
    IBP_CONNECTION *conn;

    IBPErrorNb[pi_nbt] = IBP_OK;

    conn = get_request_connection(request);
    sockfd = get_conn_fd(conn);
    cmd = &(ibp_cmd[pi_nbt]);
    nParas = cmd->size;
    /*
     * get function handle
     */
    if ( glb.nfu_conf_modified == 1 ){
        load_nfu(glb.nfu_libs,glb.nfu_ops,glb.nfu_ConfigFile);
    }
    if ((op_node = jrb_find_int(glb.nfu_ops, cmd->type)) == NULL) {
        ret = IBP_E_NFU_UNKNOWN;
        goto bail;
    }
    operation = (NFU_OP*) ((jrb_val(op_node)).v);
    op = operation->op;
    

    /*
     * get parameters
     */
    if ((ret = _ParseNfuParameters(sockfd, &paras, nParas)) != IBP_OK) {
        goto bail;
    }

    /* 
     * client authentication
     */
#ifdef IBP_AUTHENTICATION
    if (glb.enableAuthen){
        if ( conn->authenticated != 1 ){
            ret = ibpSrvAuthentication(sockfd,conn); 
        }
        if ( ret != IBP_OK){
            goto bail;
        }
    }
#endif

    /*
     * check parameters
     */
    if ((ret = _CheckNfuParameters(paras, nParas)) != IBP_OK) {
        goto bail;
    }

    /*
     * lock up all write capabilities
     */
    if ((ret = _LockWriteCaps( paras, nParas)) != IBP_OK ){
        goto bail;
    }

    _locked = 1;
    
    /*
     * create array of parameters
     */
    if ( ( nfu_paras = (NFU_PARA*)calloc(sizeof(NFU_PARA),nParas)) == NULL ){
        ret = IBP_E_ALLOC_FAILED;
        goto bail;
    }
   
    if ( operation->is_thread == 1 ){
      ret = run_nfu_thread(sockfd,op,paras,nParas,nfu_paras);
    }else{
      ret = run_nfu_process(sockfd,op,paras,nParas,nfu_paras);
    }
   
    if ( ret != IBP_OK ) {
      goto bail;
    }

#if 0    
    /*
     * open capabilities
     */
    if ((ret = _OpenCaps(paras, nParas)) != IBP_OK ){
        goto bail;
    }

    /*
     * create array of parameters
     */
    if ( ( nfu_paras = (NFU_PARA*)calloc(sizeof(NFU_PARA),nParas)) == NULL ){
        ret = IBP_E_ALLOC_FAILED;
        goto bail;
    }

 
    /*
     * call nfu function
     */
    for ( j = 0 ; j < nParas ; j++){
        switch ( paras[j].io_type ){
        case IBP_VAL_IN:
        case IBP_VAL_OUT:
        case IBP_VAL_INOUT:
            nfu_paras[j].data = paras[j].data;
            nfu_paras[j].len = paras[j].len;
            break;
        case IBP_REF_RD:
        case IBP_REF_WR:
        case IBP_REF_RDWR:
            nfu_paras[j].data = paras[j].buf;
            nfu_paras[j].len = paras[j].len;
            break;
        }
    }
    if ( (ret = op(nParas,nfu_paras)) != 0 ){
        goto bail;
    }
    
    ret = IBP_OK;

#endif

    /*
     * update capabilities' attributes
     */
    for ( i = 0 ; i < nParas; i ++ ){
        if ( paras[i].io_type == IBP_REF_WR ||
             paras[i].io_type == IBP_REF_RDWR ){
            if ( paras[i].begin_offset + nfu_paras[i].len > paras[i].ba->currentSize ){
                paras[i].ba->currentSize = paras[i].begin_offset + nfu_paras[i].len; 
            }
            if ( paras[i].ba->currentSize > paras[i].ba->maxSize ){
                paras[i].ba->currentSize = paras[i].ba->maxSize;
            }
        }
    }

  bail:
    /*
     * send IBP_VAL_OUT type parameters back to client 
     */
    if ( ret != IBP_OK ){
      sprintf(msg,"%d \n",ret);
      Write(sockfd,msg,strlen(msg));
    }
    IBPErrorNb[pi_nbt] = ret;

    /*
     * release locks on capabilities
     */
    if ( paras != NULL ){
        _ReleaseCaps(paras,nParas,_locked);
    }

    if ( paras != NULL ){
        _FreeParas(paras,nParas);
    }
   
    if ( nfu_paras != NULL ){
      free(nfu_paras);
    } 

   return;

}

static void _SendParametersToClient( int sockfd,S_PARA *para, int npara){
    int i;
    int np = 0;
    char msg[256];

    for ( i = 0 ; i < npara; i ++ ){
        if ( para[i].io_type == IBP_VAL_OUT ||
             para[i].io_type == IBP_VAL_INOUT ){
            np++;
        }
    }
    sprintf(msg,"%d \n",np);
    Write(sockfd,msg,strlen(msg));
    for ( i=0; i < npara; i++){
        if ( para[i].io_type != IBP_VAL_OUT &&
             para[i].io_type != IBP_VAL_INOUT ){
            continue;
        }
        sprintf(msg,"%d %d \n",i,para[i].len);
        Write(sockfd,msg,strlen(msg));
        if ( para[i].len > 0 ){
            Write(sockfd,para[i].data,para[i].len);
        }
    }

    return;
}

static void _ReleaseCaps(S_PARA *para, int npara, int locked){
    int  i;

    for ( i = 0 ;  i < npara ; i ++ ){
        if ( para[i].io_type == IBP_VAL_IN ||
             para[i].io_type == IBP_VAL_OUT ||
             para[i].io_type == IBP_VAL_INOUT ){
            continue;
        }
        switch( para[i].io_type ){
        case IBP_REF_RD:
            if ( para[i].ba != NULL ){
                if ( locked == 1 ){
                    para[i].ba->readLock --;
                }
                para[i].ba->reference--;
                if ( para[i].ba->deleted == 1 &&
                     para[i].ba->writeLock == 0 &&
                     para[i].ba->readLock == 0 ){
                    SaveInfo2LSS(para[i].rs,para[i].ba,2);
                    jettisonByteArray(para[i].rs,para[i].ba);
                }
            }
            break;
        case IBP_REF_WR:
        case IBP_REF_RDWR:
            if ( para[i].ba != NULL ){
                if ( locked == 1 ){
                    para[i].ba->writeLock--;
                }
                para[i].ba->reference --;
                if ( para[i].ba->deleted == 1 &&
                     para[i].ba->writeLock == 0 &&
                     para[i].ba->readLock == 0 ){
                    SaveInfo2LSS(para[i].rs,para[i].ba,2);
                    jettisonByteArray(para[i].rs,para[i].ba);
                }
            }
            break;
        default:
            exit(-1);
            break;
        }
    }

    return;
}

/*
static void _UpdateBuffer( S_PARA *para, NFU_PARA *nfu_para , int npara){
    int  i;

    for ( i = 0 ; i < npara ; i ++ ){
        if ( para[i].data_type != DATA_CAP ){
            continue;
        }
        switch ( para[i].io_type ){
        case IO_IN:
        case IO_INOUT:
            para[i].units_left_in_buf --;
            para[i].cur_offset += para[i].len;
            para[i].buf += para[i].len;
            break;
        case IO_OUT:
            if ( nfu_para[i].len > para[i].len ){
                nfu_para[i].len = para[i].len;
            }else if ( nfu_para[i].len < 0 ){
                nfu_para[i].len = 0;
            }
            para[i].cur_offset += nfu_para[i].len;
            para[i].buf += nfu_para[i].len;
            para[i].off_in_buf += nfu_para[i].len;
            break;
        }
    }
}
*/

/*
static int _CheckCapSpace (S_PARA *para,NFU_PARA *nfu_para, int npara){
    int     i,left,len;
    off_t   off;
    int     ret = IBP_OK;


    for ( i=0; i < npara; i++){
        switch( para[i].data_type ){
        case DATA_CAP:
            if ( para[i].rs->type != DISK ){
                continue;
            }
            switch ( para[i].io_type){
            case IO_IN:
            case IO_INOUT:
                if ( para[i].units_left_in_buf <= 0 ){
                    munmap(para[i].mmap_ptr,para[i].mmap_len);
                    left = (para[i].len * para[i].cnt) - 
                           (para[i].cur_offset - para[i].begin_offset);
                    if ( left <= (para[i].units_per_buf * para[i].len)){
                        len = left;
                    }else{
                        len = para[i].units_per_buf * para[i].len;
                    }
                    para[i].units_left_in_buf = len / para[i].len;
                    off = ( para[i].cur_offset + para[i].header_len)/glb.sys_page_size * glb.sys_page_size;
                    len = (para[i].cur_offset + para[i].header_len - off )  + len ; 
                    if ( para[i].io_type == IO_IN){
                        (char*)para[i].mmap_ptr = mmap(0,len,PROT_READ,MAP_PRIVATE,para[i].fd,off);
                    }else{
                        (char*)para[i].mmap_ptr = mmap(0,len,PROT_WRITE|PROT_READ,MAP_SHARED,para[i].fd,off);
                    }
                    if ( (char*)para[i].mmap_ptr == MAP_FAILED){
                        ret = IBP_E_FILE_OPEN;
                        para[i].mmap_ptr = NULL;
                        goto bail;
                    }
                    para[i].mmap_len = len;
                    para[i].buf = para[i].mmap_ptr + para[i].cur_offset + para[i].header_len - off;
                }
                break;
            case IO_OUT:
                if ( (para[i].mmap_len - para[i].off_in_buf) < para[i].len ){
                    munmap(para[i].mmap_ptr,para[i].mmap_len);
                    left = ( para[i].len * para[i].cnt) -
                           ( para[i].cur_offset - para[i].begin_offset );
                    if ( left <= (para[i].units_per_buf * para[i].len)){
                        len = left;
                    }else{
                        len = para[i].units_per_buf * para[i].len ; 
                    }
                    para[i].units_left_in_buf = len / para[i].len ;
                    off = (para[i].cur_offset + para[i].header_len)/glb.sys_page_size * glb.sys_page_size;
                    len = ( para[i].cur_offset + para[i].header_len - off ) + len ;
                    (char*)para[i].mmap_ptr = mmap(0,len,PROT_WRITE|PROT_READ,MAP_SHARED,para[i].fd,off);
                    if ( (char*)para[i].mmap_ptr == MAP_FAILED){
                        ret = IBP_E_FILE_OPEN;
                        para[i].mmap_ptr = NULL;
                        goto bail;
                    }
                    para[i].mmap_len = len;
                    para[i].buf = para[i].mmap_ptr + para[i].cur_offset + para[i].header_len - off;
                    para[i].off_in_buf = para[i].cur_offset + para[i].header_len - off;
                }
                break;
            }
            break;
        }
    }

bail: 
    return ( ret);

}
*/

/*
static void _ReleaseLock(S_PARA *para, int npara, int *mark){
    int  i;

    for ( i = 0 ; i < npara; i ++ ){
        if ( mark[i] == 1 ){
            assert(para[i].rs != NULL && 
                   para[i].ba != NULL &&
                   para[i].ba->wl == 1 );
            pthread_mutex_lock(&(para[i].rs->wlock));
            para[i].ba->wl = 0;
            pthread_mutex_unlock(&(para[i].rs->wlock));
            mark[i] = 0;
        }
    }
}
*/

static int  _LockWriteCaps(S_PARA *para, int npara){
    int  i;

    /*
    if ( (mark = (int *)calloc(sizeof(int),npara)) == NULL ){
        return ( IBP_E_ALLOC_FAILED);
    }
    for ( i = 0; i < npara ; i ++ ){
        mark[i] = 0;
    }
    ntries = 0;
again:
    pthread_mutex_lock(glb.nfu_lock);
    for ( i = 0 ; i < npara; i ++ ){
        if ( (para[i].data_type == DATA_CAP) &&
             (para[i].io_type == IO_OUT ||
              para[i].io_type == IO_INOUT )){
            assert(para[i].rs != NULL && para[i].ba != NULL );
            pthread_mutex_lock(&(para[i].rs->wlock));
            if ( para[i].ba->wl == 0 ){
                para[i].ba->wl = 1;
                mark[i] = 1;
            }else{
                pthread_mutex_unlock(&(para[i].rs->wlock));
                pthread_mutex_unlock(glb.nfu_lock);
                _ReleaseLock(para,npara,mark);
                ntries ++;
                if ( ntries > 10 ){
                    free(mark);
                    return (IBP_E_ALLOC_FAILED);
                }
                sleep(1);
                goto again;
            }
            pthread_mutex_unlock(&(para[i].rs->wlock));
        }
    }
    pthread_mutex_unlock(glb.nfu_lock);
    */

    for ( i = 0 ; i < npara ; i ++ ){
        if ( para[i].io_type == IBP_VAL_IN ||
             para[i].io_type == IBP_VAL_OUT ||
             para[i].io_type == IBP_VAL_INOUT ){
            continue;
        }
        switch ( para[i].io_type ){
        case IBP_REF_RD:
            para[i].ba->readLock ++;
            break;
        case IBP_REF_WR:
        case IBP_REF_RDWR:
            para[i].ba->writeLock ++;
            break;
        default:
            exit(-1);
        }
    }

    return (IBP_OK);
}

static int  _OpenCaps(S_PARA *paras, int npara){
    int         i;
    int         ret;
    int       off,tmp_newSize;
    size_t      size;
    int         header_len;

    /*buf_size = glb.sys_page_size * IBP_NFU_MAX_PAGES;*/

    assert( paras != NULL && npara > 0 );
    for ( i = 0 ; i < npara ; i ++ ){
        if ( paras[i].io_type == IBP_VAL_IN ||
             paras[i].io_type == IBP_VAL_OUT ||
             paras[i].io_type == IBP_VAL_INOUT ){
            continue;
        }

        switch ( paras[i].rs->type){
        case MEMORY:
            paras[i].mmap_ptr = NULL;
            paras[i].buf = paras[i].ba->startArea + paras[i].begin_offset;
            paras[i].fd = -1;
            break;
        case DISK:
            switch ( paras[i].io_type){
            case IBP_REF_RD:
                if ( ( paras[i].fd = open(paras[i].ba->fname,O_RDONLY)) < 0 ){
                    ret = IBP_E_FILE_OPEN;
                    goto bail;
                }
                if ( read(paras[i].fd,&header_len,sizeof(int)) != sizeof(int)){
                    ret = IBP_E_FILE_ACCESS;
                    goto bail;
                }
                paras[i].header_len = header_len;
                off = (paras[i].begin_offset+header_len)/glb.sys_page_size * glb.sys_page_size;
                size = (paras[i].begin_offset + header_len - off ) +  paras[i].len;
                if ((paras[i].mmap_ptr = mmap(0,size,PROT_READ,MAP_PRIVATE,paras[i].fd,off)) == MAP_FAILED){
                    ret = IBP_E_FILE_OPEN;
                    paras[i].mmap_ptr = NULL;
                    goto bail;
                }
                paras[i].mmap_len = size;
                paras[i].buf = paras[i].mmap_ptr + paras[i].begin_offset + header_len - off ;
                break;
            case IBP_REF_WR:
            case IBP_REF_RDWR:
                if ( ( paras[i].fd = open(paras[i].ba->fname,O_RDWR)) < 0 ){
                    ret = IBP_E_FILE_OPEN;
                    goto bail;
                }
                if ( read(paras[i].fd,&header_len,sizeof(int)) != sizeof(int)){
                    ret = IBP_E_FILE_ACCESS;
                    goto bail;
                }
                paras[i].header_len = header_len;
                if ( paras[i].begin_offset + paras[i].len > paras[i].ba->currentSize ){
                    tmp_newSize = header_len + paras[i].begin_offset + paras[i].len;
                    if ( lseek(paras[i].fd,tmp_newSize-1,SEEK_SET) == -1 ){
                        ret = IBP_E_FILE_ACCESS;
                        goto bail;
                    }
                    if ( write(paras[i].fd,"\0",1) != 1 ){
                        ret = IBP_E_FILE_ACCESS;
                        goto bail;
                    }
                }
                off = (paras[i].begin_offset+header_len)/glb.sys_page_size * glb.sys_page_size;
                size = paras[i].begin_offset + header_len - off + paras[i].len;
                if ((paras[i].mmap_ptr = mmap(0,size,PROT_WRITE|PROT_READ,MAP_SHARED,paras[i].fd,off)) == MAP_FAILED){
                    ret = IBP_E_FILE_OPEN;
                    paras[i].mmap_ptr = NULL;
                    goto bail;
                }
                paras[i].mmap_len = size;
                paras[i].buf = paras[i].mmap_ptr + paras[i].begin_offset + header_len - off;
                break;
            default:
                exit(-1);
            }
            break;
        default:
            ret = IBP_E_UNKNOWN_RS;
            goto bail;
        }
    }

    return ( IBP_OK);

bail:
    for ( i = 0 ; i < npara ; i ++ ){
        if ( paras[i].io_type == IBP_VAL_IN ||
             paras[i].io_type == IBP_VAL_OUT ||
             paras[i].io_type == IBP_VAL_INOUT ){
            continue;
        }
        if ( paras[i].mmap_ptr != NULL ){
           munmap(paras[i].mmap_ptr,paras[i].mmap_len);
           paras[i].mmap_ptr = NULL;
           if ( paras[i].fd >=0 ){
               close( paras[i].fd);
               paras[i].fd = -1;
           }
        }
    }

    return (ret);

}

static int _CheckNfuParameters(S_PARA * paras, int nparas)
{
    int i;
    int ret = IBP_OK;

    assert(paras != NULL && nparas > 0);

    for (i = 0; i < nparas; i++) {

        /*
         * check parameter type , len ,cnt ...
         */
        switch (paras[i].io_type) {
        case IBP_REF_RD:
        case IBP_REF_WR:
        case IBP_REF_RDWR:
            if (paras[i].len < 0 ||
                paras[i].begin_offset < 0) {
               ret = IBP_E_NFU_INVALID_PARA;
                   goto bail;
            }
            break;
        case IBP_VAL_IN:
        case IBP_VAL_INOUT:
            if (paras[i].len < 0) {
                ret = IBP_E_NFU_INVALID_PARA;
                goto bail;
            }
            break;
        case IBP_VAL_OUT:
            if ( paras[i].len < 0){
                ret = IBP_E_NFU_INVALID_PARA;
                goto bail;
            }
            if ( (paras[i].data = (char*)calloc(sizeof(char),paras[i].len)) == NULL){
                ret = IBP_E_ALLOC_FAILED;
                goto bail;
            }
            break;
        default:
            ret = IBP_E_NFU_UNKNOWN_PARA_IO_TYPE;
            goto bail;
        }
    }

    /*
     * get capability handler
     */
    for ( i = 0 ; i < nparas ; i ++ ){
       if ( paras[i].io_type == IBP_REF_RD ||
             paras[i].io_type == IBP_REF_WR ||
             paras[i].io_type == IBP_REF_RDWR ) {
             if ( (ret = _CheckCapPara(&(paras[i]))) != IBP_OK ){
                 _DecCapRef(paras, nparas);
                 goto bail;
            }
        }
    }

    /*
     * check duplicated capabilities
     */
    if ((ret = _CheckDupCaps(paras,nparas)) != IBP_OK ){
        _DecCapRef(paras,nparas);
        goto bail;
    }

    return (IBP_OK);

bail:
    return ( ret);

}

static int  _CheckDupCaps( S_PARA *para, int nparas){
    int             i;
    int             j;
    IBP_byteArray   ba;

    assert ( para != NULL && nparas > 0 );

    for ( i = 0 ; i < nparas ; i ++){
        if ( ( para[i].io_type == IBP_REF_RD ||
               para[i].io_type == IBP_REF_WR ||
               para[i].io_type == IBP_REF_RDWR)&&
             para[i].rs != NULL && para[i].ba != NULL ){
            ba = para[i].ba;
            for ( j = i +1 ; j < nparas ; j ++ ){
                if ( (para[j].io_type == IBP_REF_RD ||
                      para[j].io_type == IBP_REF_WR ||
                      para[j].io_type == IBP_REF_RDWR)&&
                     para[j].rs != NULL && para[j].ba != NULL ){
                    if ( (para[j].ba == ba) && (para[j].io_type == para[i].io_type)){
                        return(IBP_E_NFU_DUP_PARA);
                    }
                }
            }
        }
    }

    return (IBP_OK);
}

static void _DecCapRef( S_PARA *para, int npara){
    int     i;
    
    assert(para != NULL && npara > 0 );

    for ( i=0;i < npara; i++){
        if ( (para[i].io_type == IBP_REF_RD ||
              para[i].io_type == IBP_REF_WR ||
              para[i].io_type == IBP_REF_RDWR )&&
             para[i].rs != NULL && para[i].ba != NULL ){
            pthread_mutex_lock(&(para[i].rs->change_lock));
            para[i].ba->reference --;
            assert(para[i].ba->reference >= 0 );
            pthread_mutex_unlock(&(para[i].rs->change_lock));
            if ( para[i].ba->deleted == 1 &&
                 para[i].ba->writeLock == 0 &&
                 para[i].ba->readLock == 0 ){
                SaveInfo2LSS(para[i].rs,para[i].ba,2);
                jettisonByteArray(para[i].rs,para[i].ba);
            }
            para[i].rs = NULL;
            para[i].ba = NULL;
        }
    }
}

static int  _CheckCapPara(S_PARA *para)
{
    char          *str, *tmp;
    char          *last;
    char          dli1[] = "/";
    char          *token1;
    JRB           node;
    IBP_URL       iu;
    int           ret;

    assert( para != NULL );

    /*
     * parse ibp capability string
     */
    str = (IBP_cap)para->data;
    if ((token1 = strtok_r(str, dli1, &last)) == NULL) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }
    if (strcasecmp(token1, "ibp:") != 0) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }

    if ((token1 = strtok_r(NULL, dli1, &last)) == NULL) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }
    if ((tmp = rindex(token1, ':')) == NULL) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }
    *tmp = '\0';
    tmp = tmp + 1;
    strcpy(iu.host, token1);
    iu.port = atoi(tmp);

    if ((token1 = strtok_r(NULL, dli1, &last)) == NULL) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }
    if ((tmp = index(token1, '#')) == NULL) {
        iu.rid = 0;
    }
    else {
        *tmp = '\0';
        tmp = tmp + 1;
        iu.rid = atoi(token1);
        /*strcpy(iu.rid, token1);*/
        strcpy(iu.key, tmp);
    }

    if ((token1 = strtok_r(NULL, dli1, &last)) == NULL) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }
    strcpy(iu.attr_key, token1);


    if ((token1 = strtok_r(NULL, dli1, &last)) == NULL) {
        ret = IBP_E_WRONG_CAP_FORMAT;
        goto bail;
    }
    strcpy(iu.type, token1);

    /*
     * check cap type
     */
    switch ( para->io_type ){
    case IBP_REF_RD:
        if ( strcasecmp("READ",iu.type) != 0 ){
            ret = IBP_E_CAP_NOT_READ;
            goto bail;
        }
        break;
    case IBP_REF_WR:
    case IBP_REF_RDWR:
        if ( strcasecmp("WRITE",iu.type) != 0 ){
            ret = IBP_E_CAP_NOT_WRITE;
            goto bail;
        }
        break;
    default:
        assert(1==2);
    }


    /*
     * retrieve capability handler
     */
    if ((strcasecmp(iu.host, glb.hostname) == 0) &&
        (iu.port == glb.IbpPort)) {
        /*if (strlen(iu.rid) == 0) {*/
        if ( iu.rid == 0 ){
            para->rs = glb.dftRs;
        }
        else {
            node = jrb_find_int(glb.resources, iu.rid);
            if (node != NULL) {
                para->rs = (RESOURCE *) ((jrb_val(node)).v);
            }
            else {
                para->rs = NULL;
                para->ba = NULL;
                ret = IBP_E_CAP_NOT_FOUND;
                goto bail;
            }
        }
        if (para->rs != NULL) {
            pthread_mutex_lock(&(para->rs->change_lock));
            node = jrb_find_str(para->rs->ByteArrayTree, iu.key);
            if (node != NULL) {
                para->ba = (IBP_byteArray) ((jrb_val(node)).s);
            }
            else {
                pthread_mutex_unlock(&(para->rs->change_lock));
                para->ba = NULL;
                para->rs = NULL;
                ret = IBP_E_CAP_NOT_FOUND;
                goto bail;
            }
            if ( para->ba->deleted == 1 ){
                pthread_mutex_unlock(&(para->rs->change_lock));
                para->ba = NULL;
                para->rs = NULL;
                ret = IBP_E_CAP_NOT_FOUND;
                goto bail;
            }

            /*
             * only bytearray can be used for NFU
             */ 
            if ( para->ba->attrib.type != IBP_BYTEARRAY ){
                pthread_mutex_unlock(&(para->rs->change_lock));
                para->ba = NULL;
                para->rs = NULL;
                ret = IBP_E_CAP_ACCESS_DENIED;
                goto bail;
            }

            /*
             * check read/write authorization, length and offset
             */
            switch (para->io_type ){
            case IBP_REF_RD:
                if ( strcmp(iu.attr_key,para->ba->readKey_s) != 0 ){
                    pthread_mutex_unlock(&(para->rs->change_lock));
                    para->ba = NULL;
                    para->rs = NULL;
                    ret = IBP_E_CAP_ACCESS_DENIED;
                    goto bail;
                }
                if (para->begin_offset >= para->ba->currentSize ){
                    pthread_mutex_unlock(&(para->rs->change_lock));
                    para->ba = NULL;
                    para->rs = NULL;
                    ret = IBP_E_CAP_ACCESS_DENIED;
                    goto bail;
                }
                if ( ( (para->begin_offset + para->len ) > para->ba->currentSize) ||
                     (para->len == 0 )){
                    para->len = para->ba->currentSize - para->begin_offset;
                }

                break;
            case IBP_REF_RDWR:
            case IBP_REF_WR:
                if ( strcmp(iu.attr_key,para->ba->writeKey_s) != 0 ){
                    pthread_mutex_unlock(&(para->rs->change_lock));
                    para->ba = NULL;
                    para->rs = NULL;
                    ret = IBP_E_CAP_ACCESS_DENIED;
                    goto bail;
                }
                if ((para->begin_offset >= para->ba->maxSize) ||
                    (para->len == 0 )){
                    pthread_mutex_unlock(&(para->rs->change_lock));
                    para->ba = NULL;
                    para->rs = NULL;
                    ret = IBP_E_CAP_ACCESS_DENIED;
                    goto bail;
                }
                if ((para->begin_offset + para->len > para->ba->maxSize) ||
                    (para->len == 0 )){
                    para->len = para->ba->maxSize - para->begin_offset;
                }
                break;
            default:
                assert(1==2);
            }
            para->ba->reference++;
            pthread_mutex_unlock(&(para->rs->change_lock));
        }
    }
    else {
        para->rs = NULL;
        para->ba = NULL;
        ret = IBP_E_CAP_NOT_FOUND;
        goto bail;
    }

    return (IBP_OK);

  bail:
    return (ret);
}

static int _ParseNfuParameters(int sockfd, S_PARA ** ptrParas, int nparas)
{
    S_PARA *paras=NULL;
    char  *tmp, *buffer;
    char deli[] = " \n\r";
    char *field;
    int len, ret, i;

    if ((buffer = (char *) calloc(sizeof(char), CMD_BUF_SIZE)) == NULL) {
        ret = IBP_E_ALLOC_FAILED;
        goto bail;
    }

    if ((paras = (S_PARA *) calloc(sizeof(S_PARA), nparas)) == NULL) {
        ret = IBP_E_ALLOC_FAILED;
        goto bail;
    }
    for (i=0;i < nparas;i++){
        paras[i].fd = -1;
    }

    for (i = 0; i < nparas; i++) {
        if ((ReadComm(sockfd, buffer, CMD_BUF_SIZE)) == -1) {
            ret = IBP_E_SOCK_READ;
            goto bail;
        }

        /*
         * get io type of parameter
         */
        field = strtok_r(buffer, deli, &tmp);
        if (field == NULL) {
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        paras[i].io_type = atoi(field);

        /*
         * get offset 
         */
        field = strtok_r(NULL, deli, &tmp);
        if (field == NULL) {
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        paras[i].begin_offset = atoi(field);
        paras[i].cur_offset = paras[i].begin_offset;

        /*
         * get length
         */
        field = strtok_r(NULL, deli, &tmp);
        if (field == NULL) {
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        paras[i].len = atoi(field);

        /*
         * get length of capability or immediate data
         */
        field = strtok_r(NULL, deli, &tmp);
        if (field == NULL) {
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        len = atoi(field);
        if (len <= 0 && paras[i].io_type != IBP_VAL_OUT ) {
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }

        /* 
         * read capability or immediate data
         */
        if ( len > 0){
            if ((paras[i].data = calloc(1, len + 1)) == NULL) {
                ret = IBP_E_ALLOC_FAILED;
                goto bail;
            }
            if (Read(sockfd, paras[i].data, len) != len) {
                ret = IBP_E_SOCK_READ;
                goto bail;
            }
            ((char *) (paras[i].data))[len] = '\0';
        }
    }

    free(buffer);
    *ptrParas = paras;
    return (IBP_OK);

  bail:
    if (buffer != NULL) {
        free(buffer);
    }

    _FreeParas(paras, nparas);
    return (ret);

}

void _FreeParas(S_PARA * paras, int nparas)
{
    int i;

    if (paras == NULL) {
        return;
    }

    for (i = 0; i < nparas; i++) {
        if (paras[i].data != NULL) {
            free(paras[i].data);
        }
        if ( paras[i].mmap_ptr != NULL ){
            munmap(paras[i].mmap_ptr,paras[i].mmap_len);
        }
        if ( paras[i].fd >= 0 ){
            close(paras[i].fd);
        }
    }

    free(paras);

    return;
}
