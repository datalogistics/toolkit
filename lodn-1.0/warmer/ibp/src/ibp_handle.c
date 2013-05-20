/*           IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *          Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 2001 All Rights Reserved
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
 * ibp_server_mt.c
 *
 * IBP server code. Version name: debut
 * Main of the Threaded version
 */
# include "config-ibp.h"
# ifdef HAVE_NETDB_H
# include <netdb.h>
# endif
# ifdef STDC_HEADERS
# include <assert.h>
# endif
# ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# endif
# include <stdio.h>
# include <unistd.h>
# include "ibp_thread.h"
# include "ibp_request.h"
# include "ibp_resources.h"
# include "ibp_log.h"
# include "fs.h"

extern void reload_config_file();
extern int recover_finish();
extern int ibp_ntop( struct sockaddr *addr, char *strptr, size_t len);
extern void handle_nfu(IBP_REQUEST *request, int pi_nbt);
extern int SaveInfo2LSS( RESOURCE *rs,IBP_byteArray ps_container, int pi_mode);
extern void handle_lsd_ulfs ( RESOURCE *rs ,int pi_fd, int pi_nbt, IBP_byteArray ps_container);
extern void handle_copy_disk ( RESOURCE *rs, int pi_fd, int pi_nbt, IBP_byteArray ps_container);
extern void handle_copy_ulfs ( RESOURCE *rs, int pi_fd, int pi_nbt, IBP_byteArray ps_container);
extern void handle_copy_ram(RESOURCE *rs ,int pi_fd, int pi_nbt, IBP_byteArray ps_container);
extern void handle_mcopy (RESOURCE *rs,int pi_connfd, int pi_nbt);
extern void handle_ct_copy( int sockfd, int nbt, IBP_byteArray srcCap );
/*
 *
 * Level 2: handle_request
 * It has to be here because of the mutex lock
 *
 */

/*****************************/
/* handle_request            */
/* This function decodes the user's command and envokes the correspond function.  */
/* pi_nbt    -- thread number      */
/* return    -- none               */
/*****************************/
void handle_request(IBP_REQUEST *request, int pi_nbt) {

  IBP_byteArray       ls_ByteArray=NULL,dstCap;
  void                *lv_return;
  int                 retVal;
  int                 pi_connfd;
  char		            lc_now[40];
  char                *lc_fname=NULL;
  void		            *decode_cmd();
  unsigned short      li_wait, li_bufflag = 0;
  time_t	            tt,end_time,_dur;
  struct sockaddr     ls_peername;
  socklen_t           ls_peernamelen;
  RESOURCE            *rs,*dstRs;
  char                cmd[1024],addr_str[128];
  int                 _ret;
  char                tt_tmp[50];              
  IBP_CONNECTION        *conn;


  bzero(exec_info[pi_nbt],4096);
  tt = time(NULL);
#ifdef IBP_DEBUG
  sprintf(exec_info[pi_nbt]," Handle_request() beginning at %s\n",ctime(&tt));
#endif

  if (!glb.IBPRecover) {
    if (tt > gt_recoverstoptime) {
      pthread_mutex_lock(&(glb.glbLock));
#ifdef IBP_DEBUG
      printf("depot recovery finished\n");
#endif 
      recover_finish();
      glb.IBPRecover = 1;
      pthread_mutex_unlock(&(glb.glbLock));
    }
  }

  lv_return = decode_cmd (request, pi_nbt);

  rs = ibp_cmd[pi_nbt].rs;

  if (ibp_cmd[pi_nbt].Cmd == IBP_NOP) {
#ifdef IBP_DEBUG
    fprintf(stderr, "Error in detecting command, NOP\n");
#endif
    IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
    return;
  }

  /*
   * do client authentication 
   */
    conn = get_request_connection(request);
    assert(conn != NULL);
    pi_connfd = get_conn_fd(conn);

#ifdef IBP_AUTHENTICATION
    retVal = IBP_OK;
    if ( glb.enableAuthen && (ibp_cmd[pi_nbt].Cmd != IBP_NFU) ){
        if ( conn->authenticated != 1 ){
            retVal = ibpSrvAuthentication(pi_connfd,conn); 
        }
        if ( retVal != IBP_OK ){
            IBPErrorNb[pi_nbt] = retVal;
            goto LOG;
        }
    }
#endif


    /*
     * Update log file
     */
#ifdef IBP_AUTHENTICATION
  if (glb.enableAuthen){
#else
  if ( glb.enableLog ){
#endif
   if( 0 != getpeername(pi_connfd, &ls_peername, &ls_peernamelen) ){
       strcpy(addr_str,"0.0.0.0");
   }else if ( 0 != ibp_ntop(&ls_peername,addr_str,127) ){
       strcpy(addr_str,"0.0.0.0");
   };

  }

  IBPErrorNb[pi_nbt] = IBP_OK;

  if (!glb.IBPRecover) {
    if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND) || ((ibp_cmd[pi_nbt].Cmd == IBP_MANAGE) && (ibp_cmd[pi_nbt].ManageCmd == IBP_PROBE))) {
    }
    else {
      handle_error(IBP_E_SERVER_RECOVERING, &pi_connfd);
      return;
    }
  }
  

  if ((ibp_cmd[pi_nbt].Cmd != IBP_ALLOCATE) &&
      (ibp_cmd[pi_nbt].Cmd != IBP_STATUS) &&
      (ibp_cmd[pi_nbt].Cmd != IBP_NFU) && 
      (ibp_cmd[pi_nbt].Cmd != IBP_MCOPY)) {
    ls_ByteArray = (IBP_byteArray) lv_return;
    if (ls_ByteArray->attrib.type == IBP_BUFFER) {
      assert(rs!= NULL);
      li_wait = 1;
      li_bufflag = 1;
      while (li_wait == 1) {
        pthread_mutex_lock (&(rs->block));
        if (ls_ByteArray->bl == 0) {
          ls_ByteArray->bl = 1;
          li_wait = 0;
        }
        pthread_mutex_unlock (&(rs->block));
        if (li_wait == 1)
          sleep (1);
      }
    }
  }

  if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND)
      && (ls_ByteArray->attrib.type == IBP_BUFFER)
      && (ibp_cmd[pi_nbt].url.rs == NULL )) {
    ibp_cmd[pi_nbt].Cmd = IBP_SEND_BU;
  }

  /*
   * cut through copy
   */ 
  if ( (ibp_cmd[pi_nbt].Cmd == IBP_SEND ) && ( ibp_cmd[pi_nbt].url.rs != NULL )){
    ibp_cmd[pi_nbt].Cmd = IBP_CT_COPY;
  }


  

  switch (ibp_cmd[pi_nbt].Cmd) {
  case IBP_ALLOCATE:
    if ( glb.enableAlloc != 1 ){
      handle_error(IBP_E_WOULD_EXCEED_LIMIT , &pi_connfd);
      return;
    }
    /*pthread_mutex_lock (&(ibp_cmd[pi_nbt].rs->alloc_lock));*/
    /*fprintf(stderr," IMPDE before handle_allocate\n");*/
    handle_allocate (ibp_cmd[pi_nbt].rs, pi_connfd, pi_nbt, IBP_HALL_TYPE_STD);
    /*fprintf(stderr," IMPDE after handle_allocate\n");*/
    /*fprintf(stderr,"errcode = %d\n",IBPErrorNb[pi_nbt]);*/
    /*pthread_mutex_unlock (&(ibp_cmd[pi_nbt].rs->alloc_lock));*/
    break;

  case IBP_NFU:
    handle_nfu(request, pi_nbt) ; /*&(ibp_cmd[pi_nbt]));*/
    break;
  case IBP_WRITE:
    if ( ls_ByteArray->attrib.type != IBP_BYTEARRAY ){
      IBPErrorNb[pi_nbt] = IBP_E_TYPE_NOT_SUPPORTED;
      handle_error(IBP_E_TYPE_NOT_SUPPORTED,&pi_connfd);
      break;
    };
  case IBP_STORE:
    assert ( rs != NULL ) ;
    li_wait = 1;
    while (li_wait == 1) {
      pthread_mutex_lock (&(rs->wlock));
      if (ls_ByteArray->wl == 0) {
        ls_ByteArray->wl = 1;
        li_wait = 0;
      }
      pthread_mutex_unlock (&(rs->wlock));
      if (li_wait == 1)
        sleep (1);
    }
    handle_store (rs, pi_connfd, pi_connfd, pi_nbt, ls_ByteArray);

    ls_ByteArray->reference--;
    if ( ls_ByteArray->deleted == 1 ){
      if ((ls_ByteArray->writeLock == 0 ) && (ls_ByteArray->readLock == 0 )){
        SaveInfo2LSS(rs,ls_ByteArray,2);
        jettisonByteArray(rs,ls_ByteArray);
        break;
      }
    }
    pthread_mutex_lock (&(rs->wlock));
    ls_ByteArray->wl = 0;
    pthread_mutex_unlock (&(rs->wlock));
    break;
#if 0
  case IBP_REMOTE_STORE:
    li_destfd = SocketConnect (ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);
    if (li_destfd == -1)
      handle_error (IBP_E_CONNECTION, &pi_connfd);
    else {
      li_wait = 1;
      while (li_wait == 1) {
        pthread_mutex_lock (&wlock);
        if (ls_ByteArray->wl == 0) {
          ls_ByteArray->wl = 1;
          li_wait = 0;
        }
        pthread_mutex_unlock (&wlock);
        if (li_wait == 1)
          sleep (1);
      }
      handle_store (pi_connfd, li_destfd, pi_nbt, ls_ByteArray);
      pthread_mutex_lock (&wlock);
      ls_ByteArray->wl = 0;
      pthread_mutex_unlock (&wlock);
    }
    break;
#endif

  case IBP_LOAD:
  case IBP_SEND_BU:
  case IBP_DELIVER:
    if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
      li_wait = 1;
      while (li_wait == 1) {
        pthread_mutex_lock (&(rs->rlock));
        if (ls_ByteArray->rl == 0) {
          ls_ByteArray->rl = 1;
          li_wait = 0;
        }
        pthread_mutex_unlock (&(rs->rlock));
        if (li_wait == 1)
          sleep (1);
      }
    }
    switch ( rs->type ){
      case DISK:
        handle_lsd_disk(rs,pi_connfd,pi_nbt,ls_ByteArray);
        break;
      case MEMORY:
        handle_lsd_ram(rs,pi_connfd,pi_nbt,ls_ByteArray);
        break;
      case ULFS:
        handle_lsd_ulfs(rs,pi_connfd,pi_nbt,ls_ByteArray);
      default:
        break;
    }
    /*handle_lsd (rs,pi_connfd, pi_nbt, ls_ByteArray);*/
    ls_ByteArray->reference--;
    if ( ls_ByteArray->deleted == 1 ){
      if ((ls_ByteArray->writeLock == 0 ) && (ls_ByteArray->readLock == 0 )){
        SaveInfo2LSS(rs,ls_ByteArray,2);
        break;
      }
    }
    if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
      pthread_mutex_lock (&(rs->rlock));
      ls_ByteArray->rl = 0;
      pthread_mutex_unlock (&(rs->rlock));
    }
    break;

  case IBP_SEND:
    if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
      li_wait = 1;
      while (li_wait == 1) {
        pthread_mutex_lock (&(rs->rlock));
        if (ls_ByteArray->rl == 0) {
          ls_ByteArray->rl = 1;
          li_wait = 0;
        }
        pthread_mutex_unlock (&(rs->rlock));
        if (li_wait == 1)
          sleep (1);
      }
    }
    switch ( rs->type ){
      case DISK:
        handle_copy_disk(rs,pi_connfd,pi_nbt,ls_ByteArray);
        break;
      case ULFS:
        handle_copy_ulfs(rs,pi_connfd,pi_nbt,ls_ByteArray);
        break;
      case MEMORY:
        handle_copy_ram(rs,pi_connfd,pi_nbt,ls_ByteArray);
        break;
      default:
        break;
    }
    /*handle_copy (pi_connfd, pi_nbt, ls_ByteArray);*/
    ls_ByteArray->reference--;
    if ( ls_ByteArray->deleted == 1){
      if ((ls_ByteArray->writeLock == 0 ) && (ls_ByteArray->readLock == 0 )){
        SaveInfo2LSS(rs,ls_ByteArray,2);
        jettisonByteArray(rs,ls_ByteArray);
        break;
      }
    }
    if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
      pthread_mutex_lock (&(rs->rlock));
      ls_ByteArray->rl = 0;
      pthread_mutex_unlock (&(rs->rlock));
    }
    break;
  case IBP_CT_COPY:
    dstRs = ibp_cmd[pi_nbt].url.rs;
    dstCap = ibp_cmd[pi_nbt].url.ba;
    assert(rs!= NULL && dstRs!= NULL);
    li_wait = 1;
    while ( li_wait == 1){
      pthread_mutex_lock(&(dstRs->wlock));
      if ( dstCap->wl == 0){
        dstCap->wl = 1;
        li_wait = 0;
      }
      pthread_mutex_unlock(&(dstRs->wlock));
      if ( li_wait == 1){
        sleep(1);
      }
    }
    if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
      li_wait = 1;
      while (li_wait == 1) {
        pthread_mutex_lock (&(rs->rlock));
        if (ls_ByteArray->rl == 0) {
          ls_ByteArray->rl = 1;
          li_wait = 0;
        }
        pthread_mutex_unlock (&(rs->rlock));
        if (li_wait == 1)
          sleep (1);
      }
    }
    
    handle_ct_copy(pi_connfd,pi_nbt,ls_ByteArray);
    ls_ByteArray->reference--;
    dstCap->reference--;
    if ( (dstCap->deleted == 1) &&  
         (dstCap->writeLock == 0) &&
         ( dstCap->readLock == 0 )){
      SaveInfo2LSS(dstRs,dstCap,2);
      jettisonByteArray(dstRs,dstCap);
    }else{
      pthread_mutex_lock(&(dstRs->wlock));
      dstCap->wl = 0;
      pthread_mutex_unlock(&(dstRs->wlock));
    }
    if ( ls_ByteArray->deleted == 1){
      if ((ls_ByteArray->writeLock == 0 ) && (ls_ByteArray->readLock == 0 )){
        SaveInfo2LSS(rs,ls_ByteArray,2);
        jettisonByteArray(rs,ls_ByteArray);
        break;
      }
    }
    if (ls_ByteArray->attrib.type != IBP_BYTEARRAY) {
      pthread_mutex_lock (&(rs->rlock));
      ls_ByteArray->rl = 0;
      pthread_mutex_unlock (&(rs->rlock));
    }
    break;
  case IBP_MANAGE:
    if ( (ibp_cmd[pi_nbt].ManageCmd == IBP_INCR ||
         ibp_cmd[pi_nbt].ManageCmd == IBP_CHNG ) &&
         glb.enableAlloc != 1 ){
      handle_error(IBP_E_WOULD_EXCEED_LIMIT,&pi_connfd);
      return;
    }
    switch ( rs->type ){
      case DISK:
      case ULFS:
        lc_fname = handle_manage_disk(rs,pi_connfd, pi_nbt, ls_ByteArray);
        break;
      case MEMORY:
        lc_fname = handle_manage_ram(rs,pi_connfd, pi_nbt, ls_ByteArray);
        break;
      default:
        break;
    }
    break;

  case IBP_STATUS:
    handle_status(rs,pi_connfd, pi_nbt);
    /*fprintf(stderr," IMPDE : %s\n" , exec_info[pi_nbt]);*/
    break;

  case IBP_MCOPY:
    handle_mcopy (rs,pi_connfd, pi_nbt);
    /* exec (datamover, arguments) */
    IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
    break;

  default:
    break;
  }

  if ((ibp_cmd[pi_nbt].Cmd != IBP_ALLOCATE) && 
      (ibp_cmd[pi_nbt].Cmd != IBP_STATUS) && 
       (ibp_cmd[pi_nbt].Cmd != IBP_MCOPY) &&
/*
        (ibp_cmd[pi_nbt].Cmd != IBP_MANAGE) &&
*/
         (li_bufflag == 1)) {
    pthread_mutex_lock (&(rs->block));
    ls_ByteArray->bl = 0;
    pthread_mutex_unlock(&(rs->block));
  }

/*
  lv_return = (void *) close (pi_connfd);
*/

  /*
   * Update log file
   */
LOG:
#ifdef IBP_AUTHENTICATION
 if ( glb.enableAuthen ){
#else
 if ( glb.enableLog ) {
#endif
    ibp_write_log(&(ibp_cmd[pi_nbt]),addr_str,conn,tt,time(0),IBPErrorNb[pi_nbt]);
#if 0
  end_time = time(NULL);
  _dur = end_time -tt;
  _ret = IBPErrorNb[pi_nbt];
  switch( ibp_cmd[pi_nbt].Cmd ){
  case  IBP_ALLOCATE:
      sprintf(cmd,"ALLOCATE %d %ld",IBPErrorNb[pi_nbt],ibp_cmd[pi_nbt].size);
      break;
  case IBP_STORE:
      sprintf(cmd,"STORE %d",_ret);
      if ( _ret == IBP_OK ){ 
        sprintf(cmd,"%s %ld %d %d#%s",cmd,ibp_cmd[pi_nbt].size,_dur,rs->RID,ibp_cmd[pi_nbt].key);
      }
      break;
  case IBP_WRITE:
      sprintf(cmd,"WRITE %d",_ret);
      if ( _ret == IBP_OK ){
        sprintf(cmd,"%s %ld %ld %d %d#%s",cmd,ibp_cmd[pi_nbt].Offset,ibp_cmd[pi_nbt].size, _dur,rs->RID,ibp_cmd[pi_nbt].key);
      }
      break;
  case IBP_LOAD:
      sprintf(cmd,"LOAD %d",_ret);
      if ( _ret == IBP_OK ){
        sprintf(cmd,"%s %ld %ld %d %d#%s",cmd, ibp_cmd[pi_nbt].Offset,ibp_cmd[pi_nbt].size,_dur,rs->RID,ibp_cmd[pi_nbt].key);
      }
      break;
  case IBP_SEND:
      if ( _ret > 0 ) { _ret = IBP_OK ;}
      sprintf(cmd,"IBP_COPY %d",_ret);
      if ( _ret > 0 ){
        sprintf(cmd,"%s %ld %ld %d %d#%s %s ",cmd, ibp_cmd[pi_nbt].Offset,ibp_cmd[pi_nbt].size,_dur,rs->RID,ibp_cmd[pi_nbt].key,ibp_cmd[pi_nbt].Cap);
      };
      break;
  case IBP_CT_COPY:
      sprintf(cmd,"IBP_COPY %d",_ret);
      if ( _ret == IBP_OK ){
        sprintf(cmd,"%s %ld %ld %d %d#%s %s ",cmd, ibp_cmd[pi_nbt].Offset,ibp_cmd[pi_nbt].size,_dur,rs->RID,ibp_cmd[pi_nbt].key,ibp_cmd[pi_nbt].Cap);
      };
      break;
  case IBP_MCOPY:
      sprintf(cmd,"MCOPY %d",_ret);
      break;
  case IBP_MANAGE:
      switch(ibp_cmd[pi_nbt].ManageCmd){
      case IBP_PROBE:
          sprintf(cmd,"MANAGE %d 0 %s %d#%s",_ret,"PROBE",rs->RID,ibp_cmd[pi_nbt].key);
          break;
      case IBP_INCR:
          sprintf(cmd,"MANAGE %d 0 %s %d#%s",_ret, "INCR",rs->RID,ibp_cmd[pi_nbt].key);
          break;
      case IBP_DECR:
          sprintf(cmd,"MANAGE %d 0 %s %d#%s", _ret,"DECR" ,rs->RID,ibp_cmd[pi_nbt].key);
          break;
      case IBP_CHNG:
          sprintf(cmd,"MANAGE %d 0 %s %d#%s",_ret,"CHANGE",rs->RID,ibp_cmd[pi_nbt].key);
          break;
      default:
          sprintf(cmd,"MANAGE %d 0 %s %d#%s",_ret,"UNKNOWN",rs->RID,ibp_cmd[pi_nbt].key);
      }
      break;
  case IBP_STATUS:
      switch(ibp_cmd[pi_nbt].ManageCmd ){
      case IBP_ST_INQ:
        sprintf(cmd,"STATUS %d 0 %s %d",_ret,"PROBE",rs->RID);
        break;
      case IBP_ST_CHANGE:
        sprintf(cmd,"STATUS %d 0 %s %d",_ret,"CHANGE",rs->RID);
        break;
      default:
        sprintf(cmd,"STATUS");
      }
      break;
  case IBP_NFU:
      sprintf(cmd,"NFU %d",_ret);
      break;
  default:
      sprintf(cmd,"Unknown_Command:%d",ibp_cmd[pi_nbt].Cmd);
  }

  if ( gf_logfd == NULL ){
   gf_logfd = fopen (gc_logfile, "a");
  }
  if (gf_logfd == NULL){
    handle_error (E_OFILE, (void *) gc_logfile);
  }else {
    ctime_r (&end_time,lc_now);
    lc_now[24] = '\0'; 
    /*if ((lc_fname != NULL) && (lc_now != NULL)) {*/
      fprintf(gf_logfd, "%s  [%s] %s %s \n",addr_str, lc_now, cmd,conn->cn);
      fflush(gf_logfd);
    /*}*/
  }
#endif
  }


#ifdef IBP_DEBUG
  fprintf(stderr, "Request finished by thread %d\n", pi_nbt);
  tt = time(0);
  strcpy(tt_tmp,ctime(&tt));
  tt_tmp[strlen(tt_tmp)-2] = '\0';
#endif

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt]," Handle_request() finished\n");
#endif
  if (ibp_cmd[pi_nbt].Cmd == IBP_MANAGE)
    free(lc_fname);
  return; 
}

void *routine_check()
{
  time_t lt_t;
  RESOURCE *rs;
  JRB      node;
  sigset_t  block_mask;

  /*
   * set signal mask
   */
  sigemptyset(&block_mask);
  sigaddset(&block_mask,SIGINT);
  sigaddset(&block_mask,SIGSEGV);
  pthread_sigmask(SIG_BLOCK,&block_mask,NULL);

  sleep(glb.RecoverTime);
  for(;;)
  {
    sleep(IBP_PERIOD_TIME);
    jrb_traverse(node,glb.resources){
      rs = (RESOURCE *)((jrb_val(node)).v);
      switch ( rs->type ){
        case DISK:
          check_size(rs,0, IBP_ROUTINE_CHECK);
          if ( (rs->rcvTimeFd = fopen(gc_recovertimefile,"w")) != NULL )
          {
            lt_t = time(NULL);
            fprintf(rs->rcvTimeFd,"%ld\n%s depot OK\n",lt_t,ctime(&lt_t));
            fclose(rs->rcvTimeFd );
          } 
          rs->rcvTimeFd = NULL ;
          break;
        case MEMORY:
        case RMT_DEPOT:
        default: 
          break;
      }
    }
  }
}

/*
 *
 * Thread creation part
 *
 */

/*****************************/
/* thrallocate               */
/* It allocate the space for the thread       */
/* return -- 0                */
/*****************************/
int thrallocate ()
{
  int          li_index;

  /*
   * allocate the space for the strings
   */
  if ((ibp_cmd = (ibp_command*)calloc(sizeof(ibp_command),glb.IbpThreadMax)) == NULL ){
	handle_error(E_ALLOC,NULL);
	exit(-1);
  }

  if ((exec_info = (char **)calloc(sizeof(char *),glb.IbpThreadMax)) == NULL) {
    handle_error(E_ALLOC,NULL);
    exit(-1);
  }

  for (li_index = 0; li_index < glb.IbpThreadMax; li_index++) {
    if( (ibp_cmd[li_index].key = malloc (256)) == NULL ){
	    handle_error(E_ALLOC,NULL);
	    exit(-1);
    }
    if( (ibp_cmd[li_index].host = malloc (256)) == NULL){
	    handle_error(E_ALLOC,NULL);
	    exit(-1);
    }
    if((ibp_cmd[li_index].Cap = malloc (256)) == NULL ){
	    handle_error(E_ALLOC,NULL);
	    exit(-1);
    }
    if((ibp_cmd[li_index].passwd = malloc (17)) == NULL){
	    handle_error(E_ALLOC,NULL);
	    exit(-1);
    }
    if ((exec_info[li_index] = malloc(40960)) == NULL) {
        handle_error(E_ALLOC,NULL);
        exit(-1);
    }
    ibp_cmd[li_index].rs = NULL;

  }

  return 0;

}

/*****************************/
/* ibp_crash_save            */
/* It saves the latest thread information to ibp.crash when the depot is crashed
or stopped by administrator  */
/* return: none              */
/*****************************/
void ibp_crash_save(int pi_mode)
{
  int i;
  char  lc_crashname[128];
  char  buf[300];
  FILE  *lf_crashfd;
  time_t lt_time;
  char  *lc_time;
  RESOURCE *rs;
  JRB      node;
  int    crsfd,rcvfd;

/*
  if (gi_recoverclosed == 1) {
    gi_recoverclosed = 0;
    printf("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
    fclose(gf_recoverfd);
  }
*/
  jrb_traverse(node,glb.resources){
    rs = (RESOURCE *)((jrb_val(node)).v);
    switch ( rs->type ){
      case DISK:
        if (rs->loc.dir[strlen(rs->loc.dir)-1] != DIR_SEPERATOR_CHR){
          sprintf(lc_crashname,"%s%cibp.crash",rs->loc.dir,DIR_SEPERATOR_CHR);
        }else{
          sprintf(lc_crashname,"%sibp.crash",rs->loc.dir);
        }

        if ( ( crsfd = open(lc_crashname,O_CREAT|O_TRUNC|O_RDWR,S_IWUSR|S_IRUSR)) < 0 ) {
            perror("openning");
          fprintf(stderr,"  %d : Crash file <%s>  open error\n", rs->RID,lc_crashname);
          continue;
        }
#if 0
        if ((lf_crashfd = fopen(lc_crashname, "w")) == NULL) {
          fprintf(stderr,"  %s : Crash file <%s>  open error\n", rs->RID,lc_crashname);
          continue;
        }
#endif
        lt_time = time(NULL);
        lc_time = ctime(&lt_time);
        if (pi_mode == 1) {
          sprintf(buf,"IBP Depot crashed at %s\n",lc_time);
          write(crsfd,buf,strlen(buf));
          /*fprintf(lf_crashfd,"IBP Depot crashed at %s\n",lc_time);*/
          rcvfd = open(rs->rcvTimeFile,O_CREAT|O_TRUNC|O_RDWR,S_IRUSR|S_IWUSR);
          if ( rcvfd >= 0 )
          /*if ( (rs->rcvTimeFd = fopen(rs->rcvTimeFile,"w")) != NULL )*/
          {
            sprintf(buf,"%ld\n%s depot crash\n",lt_time,lc_time);
            write(rcvfd,buf,strlen(buf));
            /*fprintf(rs->rcvTimeFd,"%ld\n%s depot
             * crash\n",lt_time,lc_time);*/
            /*fclose(rs->rcvTimeFd);*/
            close(rcvfd);
            rs->rcvTimeFd = NULL;
          }
        } else {
          sprintf(buf,"IBP Depot stopped at %s\n",lc_time);
          write(crsfd,buf,strlen(buf));
          /*fprintf(lf_crashfd,"IBP Depot stopped at %s\n",lc_time);*/
          rcvfd = open(rs->rcvTimeFile,O_CREAT|O_TRUNC|O_RDWR,S_IRUSR|S_IWUSR);
          if ( rcvfd >= 0 )
          /*if ((rs->rcvTimeFd  = fopen(rs->rcvTimeFile,"w")) != NULL )*/
          {
            sprintf(buf,"%ld\n%s depot stopped\n",lt_time,lc_time);
            write(rcvfd,buf,strlen(buf));
            /*fprintf(rs->rcvTimeFd,"%ld\n%s depot stopped\n",lt_time,lc_time);*/
            /*fclose(rs->rcvTimeFd);*/
            close(rcvfd);
            rs->rcvTimeFd = NULL ;
          }
        }

        for (i=0; i<glb.IbpThreadNum; i++) {
          sprintf(buf,"Thread %d:\n%s\n",i,exec_info[i]);
          write(crsfd,buf,strlen(buf));
          /*fprintf(lf_crashfd,"Thread %d:\n%s\n", i, exec_info[i]);*/
        }
        close(crsfd);
        /*fclose(lf_crashfd);*/
        lf_crashfd = NULL;
        break;
      case ULFS:
        ibpfs_writeFAT((FAT*)(rs->loc.ulfs.ptr),((FAT*)(rs->loc.ulfs.ptr))->device);
        break;
      case MEMORY:
      case RMT_DEPOT:
      default:
        continue;
        break; 
    }
  }
}

/*****************************/
/* ibp_signal_segv           */
/* It works when the depot is crashed.  It saves the crash information to ibp.cra
sh  */
/* return: none              */
/*****************************/
/*
static void ibp_signal_segv() {
  ibp_crash_save(1);
  fprintf(stderr,"\n  Please email ibp.crash to ibp@cs.utk.edu\n");
  fprintf(stderr,"  Thank you for using IBP.\n");
  exit(1);
}
*/

/*****************************/
/* ibp_signal_int            */
/* It works when the depot is stopped by the administrator. */
/* return: none              */
/*****************************/
extern int glbDepotShutdown;
void ibp_signal_int()
{
  void (*int_handler)(int);
  void (*term_handler)(int);

  int_handler = signal(SIGINT,SIG_IGN);
  term_handler = signal(SIGTERM,SIG_IGN);

  if ( glbDepotShutdown == 1 ){
    return;
  }
  glbDepotShutdown = 1;

  /*fprintf(stderr,"\n In signal handling \n");*/
  /*ibp_crash_save(2);*/

  /*fprintf(stderr,"\n  Thank you for using IBP.\n");*/
  signal(SIGINT,int_handler);
  signal(SIGTERM,term_handler);
}

/*****************************/
/* ibp_signal_int            */
/* It works when the nfu configuration file is modified . */
/* return: none              */
/*****************************/
void ibp_signal_nfu(){
    glb.nfu_conf_modified = 1;
    reload_config_file();
}

