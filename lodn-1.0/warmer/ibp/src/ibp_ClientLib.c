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
# include "config-ibp.h"
# include "ibp_os.h"
# ifdef STDC_HEADERS
# include <time.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# endif /* STDC_HEADERS */
# ifdef HAVE_MALLOC_H
/*# include <assert.h>*/
# include <malloc.h>
# endif /* HAVE_MALLOC_H */
# include "ibp_ClientLib.h"
# include "ibp_protocol.h"
# include "ibp_errno.h"
# include "ibp_uri.h"
# include "ibp_ComMod.h"
# include "ibp_net.h"

#ifdef HAVE_PTHREAD_H
# include <pthread.h>
#endif /* HAVE_PTHREAD_H */

#ifdef HAVE_PTHREAD_H
    extern struct ibp_dptinfo *_glb_dpt_info();
    #define glb_dpt_info (*_glb_dpt_info())
#else
   static struct ibp_dptinfo glb_dpt_info;
#endif


IBP_DptInfo createDptCfg();
void  freeDptInfo(IBP_DptInfo dptInfo);
#if 0
IBP_RS  createRS();
void  freeRS( IBP_RS rs );
#endif
IBP_DptInfo createDptInfo();
void freeDptInfo( IBP_DptInfo di);
int fillDptInfo(IBP_DptInfo info, char *str);

/*****************************/
/* check_timeout              */
/* It checks whether it is timeout */
/* return -- OK / not        */
/*****************************/
int check_timeout(IBP_timer  ps_timeout){
  if ( ps_timeout->ClientTimeout < 0 ){
    return(IBP_E_INVALID_PARAMETER);
  }
  return(IBP_OK);
}/* check_timeout */

#if 0
/*********************************/
/* IBP_isValidRID                */
/* Check validity of resource ID */
/*********************************/
int IBP_isValidRID ( int rid ){
    int i = 0;

    if ( rid < 0 )  { return IBP_E_INVALID_RID ; }
/*
    while ( rid[i] != '\0' && i < MAX_RID_LEN ){
      if ( ( rid[i] >= 'a' && rid[i] <= 'z' ) ||
         ( rid[i] >= 'A' && rid[0] <= 'Z' ) ||
         ( rid[i] >= '0' && rid[i] <= '9' ) ||
           rid[i] == '_' || rid[i] == '-' ) {
        i++;
      }else{
        return ( IBP_E_INVALID_RID ) ;
      }
   }
*/
   return (IBP_OK);
}
#endif

/*****************************/
/* IBP_substr                */
/* It eliminates the blank space in a string */
/* return -- the pointer of the last char    */
/*****************************/
char *IBP_substr (char *pc_buffer, char *pc_first)
{

  int       li_i = 0;
  char      *lc_save, *lc_left;

  lc_save = pc_buffer;
  lc_left = pc_buffer;

  while ((*lc_left == ' ') && (li_i < (CMD_BUF_SIZE -1))){
  lc_left ++;
  li_i ++;
  }
  while ((*lc_left != ' ') && (*lc_left != '\n') && (*lc_left != '\0') && (li_i < (CMD_BUF_SIZE - 1))) {
    lc_left++;
    li_i++;
  }
  lc_left++;

  strncpy (pc_first, lc_save, li_i);
  pc_first[li_i] = '\0';

  return lc_left;
}

char *capToString(IBP_cap cap){
   return (char *) cap;
}

/*****************************/
/* IBP_freeCapSet            */
/* It frees the cap space    */
/* return -- OK / not        */
/*****************************/
int IBP_freeCapSet(IBP_set_of_caps capSet){
/*  Free a dynamically allocated IBP_set_of_caps
 *  return 0 on success
 *  return -1 on failure
 */
   if (capSet != NULL){
      free(capSet->readCap);
      free(capSet->writeCap);
      free(capSet->manageCap);
      free(capSet);
      return 0;
   }
   IBP_errno = IBP_E_INVALID_PARAMETER;
   return -1;
}

/*****************************/
/* IBP_allocate              */
/* It applies a space to a IBP depot     */
/* This is an important IBP API          */
/* return -- capability info             */
/*****************************/
IBP_set_of_caps IBP_allocate (  IBP_depot          ps_depot,
                                IBP_timer          ps_timeout,
                                unsigned long int  pi_size,
                                IBP_attributes    ps_attr){

  int              li_return, li_fd;
  time_t          lt_now, lt_lifetime;
  char            lc_buffer[CMD_BUF_SIZE];
  char            lc_singleton[CMD_BUF_SIZE];
  char            *lc_left;
  IBP_CommSession  ls_comm_session;
  IBP_CommUnit_t  *lp_comm_unit;
  IBP_set_of_caps ls_caps;
  int             version = IBPv040;
  /* char            RID[MAX_RID_LEN+1]; */

  /*
   * initialize error variable
   */

  IBP_errno = IBP_OK;

  /*
   * check validation of depot parameter
   */

  if( (li_return = check_depot(ps_depot->host,&ps_depot->port)) != IBP_OK){
    IBP_errno = li_return;
    return(NULL);
  }

  /*
   * check validity of resource id
   *
  if ( (li_return = IBP_isValidRID( ps_attr->rid )) != IBP_OK){
    IBP_errno = li_return;
    return (NULL);
  }
  */

/*
  if ( rid == NULL ){
    RID[0]='#';
    RID[1]='\0';
  }else{
    strncpy(RID,rid,MAX_RID_LEN);
    RID[MAX_RID_LEN] = '\0';
  }
*/

  /*
   * check timeout
   */
  if ( (li_return = check_timeout(ps_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(NULL);
  }

  /*
   * check IBP attributes
   */
  if ((li_return = d_CheckAttrRid(ps_depot->rid)) != IBP_OK){
      IBP_errno = li_return;
      return(NULL);
  }
  if ((li_return = d_CheckAttrRely(ps_attr->reliability)) != IBP_OK){
    IBP_errno = li_return;
    return(NULL);
  }

  if ((li_return = d_CheckAttrType(ps_attr->type)) != IBP_OK){
    IBP_errno = li_return;
    return(NULL);
  }

AGAIN:
nextVersion:

  time(&lt_now);
  if ( ps_attr == NULL ){
    IBP_errno = IBP_E_INV_PAR_ATDR ;
    return(NULL);
  }
  if ( ps_attr->duration > 0){
    lt_lifetime = ps_attr->duration - lt_now;
    if ( lt_lifetime < 0){
      IBP_errno = IBP_E_INV_PAR_ATDR;
      return(NULL);
    }
  }else{
    lt_lifetime = 0;
  }

  /*
   * connect to IBP server
   */
  if ((li_fd = connect_socket(ps_depot->host,ps_depot->port,ps_timeout->ClientTimeout)) == -1 ){
    IBP_errno = IBP_E_CONNECTION;
    return(NULL);
  }

  if ( set_socket_noblock(li_fd) != 0 ){
    IBP_errno = IBP_E_CONNECTION;
    close_socket(li_fd);
    return(NULL);
  }

  /*
   * initialize the communication session structure
   */
  InitCommSession(&ls_comm_session,ps_timeout->ClientTimeout);

  /*
   * fill out the send unit
   */
  switch (version){
    case IBPv040:
      /*fprintf(stderr,"IMPDE try first here \n");*/
      sprintf(lc_buffer,"%d %d %d %d %d %d %lu %d\n",
                        version,
                        IBP_ALLOCATE,
                          ps_depot->rid,
                        ps_attr->reliability,
                        ps_attr->type,
                        (int) lt_lifetime,
                        pi_size,
                        ps_timeout->ServerSync);
      break;
    case IBPv031:
      sprintf(lc_buffer,"%d %d %d %d %d %lu %d\n",
                        version,
                        IBP_ALLOCATE,
                        ps_attr->reliability,
                        ps_attr->type,
                        (int) lt_lifetime,
                        pi_size,
                        ps_timeout->ServerSync);
      break;
    default:
      exit(0);
  }

  if (FillCommUnit(&ls_comm_session,COM_OUT,li_fd,lc_buffer,strlen(lc_buffer),DATAVAR,NULL) != IBP_OK ){
    close_socket(li_fd);
    return(NULL);
  }

  /*
   * fill out the receive unit
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,li_fd,lc_buffer,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleMsgResp) != IBP_OK ){
    close_socket(li_fd);
    return(NULL);
  }

  /*
   * do transaction
   */
  if(( lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL){
    close_socket(li_fd);
    if ((ps_depot->rid == 0) && (version == IBPv040)) {
      /*fprintf(stderr,"IMPDE I am here\n");*/
      version = IBPv031;
      IBP_errno = IBP_OK;
      goto nextVersion;
    }
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
      IBP_errno = IBP_OK;
      goto AGAIN;
    }
    return(NULL);
  }

  /*  fprintf(stderr,"errno = %d\n",IBP_errno); */
#if !IBP_PERSISTENT_CONN
    close_socket(li_fd);
#else
    releaseConnection(li_fd);
#endif

  /*fprintf(stderr," IMPDE come here\n");*/
  /*
   * process the answer
   */
  lc_left = lp_comm_unit->data_buf;
  lc_left = (char *)IBP_substr(lc_left,lc_singleton);
  if( ((li_return = atoi(lc_singleton)) != IBP_OK))  {
    /*
    if ( ( li_return == 0 ) && ( ps_depot->rid == 0) && (version == IBPv040)) {
      version = IBPv031;
      IBP_errno = IBP_OK;
      goto nextVersion;
    }
    */
    IBP_errno = li_return;
    return(NULL);
  }

  /*
   * get the three caps
   */
  ls_caps = (IBP_set_of_caps)calloc(1,sizeof(struct ibp_set_of_caps));
  if ( ls_caps == NULL){
    IBP_errno = IBP_E_ALLOC_FAILED;
    return(NULL);
  }

  /*
   * read capability
   */
  lc_left = (char*)IBP_substr(lc_left,lc_singleton);
  if (lc_singleton != NULL ){
    if ((ls_caps->readCap = (IBP_cap)strdup(lc_singleton)) == NULL){
      IBP_errno = IBP_E_ALLOC_FAILED;
      free(ls_caps);
      return(NULL);
    }
  }

  /*
   * write capability
   */
  lc_left = (char*)IBP_substr(lc_left,lc_singleton);
  if ( lc_singleton != NULL ){
    if ((ls_caps->writeCap = (IBP_cap)strdup(lc_singleton)) == NULL){
      IBP_errno = IBP_E_ALLOC_FAILED;
      free(ls_caps);
      return(NULL);
    }
  }

  /*
   * manage capability
   */
  lc_left = (char*)IBP_substr(lc_left,lc_singleton);
  if ( lc_singleton != NULL ){
    if ((ls_caps->manageCap = (IBP_cap)strdup(lc_singleton)) == NULL){
      IBP_errno = IBP_E_ALLOC_FAILED;
      free(ls_caps);
      return(NULL);
    }
  }



  /*
   * return
   */
  return(ls_caps);

} /* IBP_set_of_caps */



/*****************************/
/* IBP_store                 */
/* It stores some info to a IBP depot     */
/* This is an important IBP API          */
/* return -- the length of data             */
/*****************************/
unsigned long int IBP_store(IBP_cap     ps_cap,
          IBP_timer     ps_timeout,
          char    *pc_data,
          unsigned long int  pl_size){

  IURL             *lp_iurl;
  int             li_return;
  char             lc_line_buf[CMD_BUF_SIZE];
  unsigned long int  ll_truesize;
  IBP_CommSession  ls_comm_session;
  IBP_CommUnit_t   *lp_comm_unit;

  /*
   * initialize error variable
   */

  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  /*
   * check data buffer
   */
  if( pc_data == NULL){
    IBP_errno = IBP_E_INV_PAR_PTR;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }

AGAIN:
  /*
   * get parameters
   */
  if ( (lp_iurl = create_write_IURL(ps_cap,ps_timeout->ClientTimeout)) == NULL )
  {
   return(0);
  }
  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_timeout->ClientTimeout);

  /*
   * send IBP_STORE command to server
   */
  sprintf(lc_line_buf,"%d %d %s %s %lu %d \n",
    IBPv031,
    IBP_STORE,
    lp_iurl->key,
    lp_iurl->type_key,
    pl_size,
    ps_timeout->ServerSync);

  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleStoreResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * send user data
   */
  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,pc_data,pl_size,DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * get server response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleStoreResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }


  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
        IBP_errno = IBP_OK;
        goto AGAIN;
    }
    return(0);
  }


#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  if ( sscanf(lp_comm_unit->data_buf,"%d %lu",&li_return,&ll_truesize) != 2){
    IBP_errno = IBP_E_BAD_FORMAT;
    free_IURL(lp_iurl);
    return(0);
  }

  free_IURL(lp_iurl);
  return(ll_truesize);

} /* IBP_store */


/*****************************/
/* IBP_write                 */
/* It stores some info to a IBP depot     */
/* This is an important IBP API          */
/* return -- the length of data             */
/*****************************/
unsigned long int IBP_write(IBP_cap     ps_cap,
                            IBP_timer     ps_timeout,
                            char    *pc_data,
                            unsigned long int  pl_size,
                            unsigned long int  pl_offset){

  IURL             *lp_iurl;
  int             li_return;
  char             lc_line_buf[CMD_BUF_SIZE];
  unsigned long int  ll_truesize;
  IBP_CommSession  ls_comm_session;
  IBP_CommUnit_t   *lp_comm_unit;

  /*
   * initialize error variable
   */

  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  /*
   * check data buffer
   */
  if( pc_data == NULL){
    IBP_errno = IBP_E_INV_PAR_PTR;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }

AGAIN:
  /*
   * get parameters
   */
  if ( (lp_iurl = create_write_IURL(ps_cap,ps_timeout->ClientTimeout)) == NULL )
  {
   return(0);
  }
  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_timeout->ClientTimeout);

  /*
   * send IBP_STORE command to server
   */
  sprintf(lc_line_buf,"%d %d %s %s %lu %lu %d \n",
    IBPv031,
    IBP_WRITE,
    lp_iurl->key,
    lp_iurl->type_key,
    pl_offset,
    pl_size,
    ps_timeout->ServerSync);

  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleStoreResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * send user data
   */
  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,pc_data,pl_size,DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * get server response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleStoreResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }


  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
        IBP_errno = IBP_OK;
  goto AGAIN;
    }
    return(0);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  if ( sscanf(lp_comm_unit->data_buf,"%d %lu",&li_return,&ll_truesize) != 2){
    IBP_errno = IBP_E_BAD_FORMAT;
    free_IURL(lp_iurl);
    return(0);
  }

  free_IURL(lp_iurl);
  return(ll_truesize);

} /* IBP_write */



/*****************************/
/* IBP_copy              */
/* It copys some info from a depot to another IBP depot     */
/* This is an important IBP API          */
/* return -- data length             */
/*****************************/
unsigned long int IBP_copy(IBP_cap    ps_source,
      IBP_cap      ps_target,
      IBP_timer    ps_src_timeout,
      IBP_timer    ps_tgt_timeout,
      unsigned long int  pl_size,
      unsigned long int  pl_offset){

  int               li_return;
  IURL               *lp_iurl;
  char               lc_line_buf[CMD_BUF_SIZE];
  IBP_CommSession    ls_comm_session;
  IBP_CommUnit_t     *lp_comm_unit;

  /*
   * initialize error variable
   */
  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_src_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  if ( ps_target == NULL ){
    IBP_errno = IBP_E_WRONG_CAP_FORMAT;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }

AGAIN:

  /*
   * get parameters
   */
  if ( (lp_iurl = create_read_IURL(ps_source,ps_src_timeout->ClientTimeout)) == NULL )
    return(0);

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_src_timeout->ClientTimeout);

  /*
   * send IBP_REMOTE_STORE command to server
   */
  sprintf(lc_line_buf,"%d %d %s %s %s %lu %lu %d %d %d \n",
    IBPv031,
    IBP_SEND,
    lp_iurl->key,
    capToString(ps_target),
    lp_iurl->type_key,
    pl_offset,
    pl_size,
    ps_src_timeout->ServerSync,
    ps_tgt_timeout->ServerSync,
    ps_tgt_timeout->ClientTimeout);

  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,(func_pt*)HandleCopyResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
  IBP_errno = IBP_OK;
  goto AGAIN;
    }
    return(0);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  free_IURL(lp_iurl);

  return(lp_comm_unit->data_size);

} /* IBP_copy */



/*****************************/
/* IBP_phoebus_copy              */
/* It copys some info from a depot to another IBP depot using phoebus     */
/* This is an experimental IBP API          */
/* return -- data length             */
/*****************************/
unsigned long int IBP_phoebus_copy(IBP_cap    ps_source,
      IBP_cap      ps_target,
      IBP_timer    ps_src_timeout,
      IBP_timer    ps_tgt_timeout,
      unsigned long int  pl_size,
      unsigned long int  pl_offset,
      char *gateways[]){

  int               li_return;
  IURL               *lp_iurl;
  char               lc_line_buf[CMD_BUF_SIZE];
  IBP_CommSession    ls_comm_session;
  IBP_CommUnit_t     *lp_comm_unit;
  int i;
  char gateway_path_str[CMD_BUF_SIZE - 128];
  char *cptr;


  /*
   * initialize error variable
   */
  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_src_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  if ( ps_target == NULL ){
    IBP_errno = IBP_E_WRONG_CAP_FORMAT;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }



  /* Builds the gateway path */
  if(gateways == NULL)
  {
	  strncpy(gateway_path_str, "auto", sizeof(gateway_path_str));
  }else
  {
	  gateway_path_str[0] = '\0';

	  /* Builds the string of gateways */
	  i = 0;
	  cptr = gateway_path_str;
	  while((sizeof(gateway_path_str)-(cptr-gateway_path_str)) > (strlen(gateways[i])+1))
	  {

		  /* Adds the current gateway to the cstring */
		  strcat(cptr, gateways[i]);

		  cptr += strlen(gateways[i]);

		  /* Appends a , separator */
		  if(gateways[++i] == NULL)
		  {
			  break;
		  }

		  strcat(cptr, ",");
		  cptr ++;
	  }

	  /* List was not exhausted not so an error must have occurred */
	  if(gateways[i] != NULL)
	  {
		  IBP_errno = IBP_E_INVALID_PARAMETER;
		  return 0;
	  }
  }

AGAIN:

  /*
   * get parameters
   */
  if ( (lp_iurl = create_read_IURL(ps_source,ps_src_timeout->ClientTimeout)) == NULL )
    return(0);

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_src_timeout->ClientTimeout);

  /*
   * send IBP_REMOTE_STORE command to server
   */
  sprintf(lc_line_buf,"%d %d %s %s %s %s %lu %lu %d %d %d \n",
	IBPv040,
    IBP_PHOEBUS_SEND,
    gateway_path_str,
    lp_iurl->key,
    capToString(ps_target),
    lp_iurl->type_key,
    pl_offset,
    pl_size,
    ps_src_timeout->ServerSync,
    ps_tgt_timeout->ServerSync,
    ps_tgt_timeout->ClientTimeout);


  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,(func_pt*)HandleCopyResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
  IBP_errno = IBP_OK;
  goto AGAIN;
    }
    return(0);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  free_IURL(lp_iurl);

  return(lp_comm_unit->data_size);

} /* IBP_phoebus_copy */




/*****************************/
/* IBP_load              */
/* It copys some info from a depot     */
/* It is an important IBP API         */
/* return -- data length             */
/*****************************/
unsigned long int  IBP_load(IBP_cap    ps_cap,
      IBP_timer    ps_timeout,
      char      *pc_data,
      unsigned long int  pl_size,
      unsigned long int  pl_offset){

  int                li_return;
  IURL                *lp_iurl;
  char                lc_line_buf[CMD_BUF_SIZE];
  IBP_CommSession     ls_comm_session;
  IBP_CommUnit_t      *lp_comm_unit;

  /*
   * initialize error variable
   */
  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  /*
   * check data buffer
   */
  if ( pc_data == NULL ){
    IBP_errno = IBP_E_INV_PAR_PTR;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }

AGAIN:
  /*
   * get parameters
   */
  if ( (lp_iurl = create_read_IURL(ps_cap,ps_timeout->ClientTimeout)) == NULL )
    return(0);

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_timeout->ClientTimeout);

  /*
   * send IBP_REMOTE_STORE command to server
   */
  sprintf(lc_line_buf,"%d %d %s %s %lu %lu %d \n",
    IBPv031,
    IBP_LOAD,
    lp_iurl->key,
    lp_iurl->type_key,
    pl_offset,
    pl_size,
    ps_timeout->ServerSync);

  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleLoadResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the data
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,pc_data,pl_size,DATAFIXED,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
        IBP_errno = IBP_OK;
        goto AGAIN;
    }
    return(0);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  free_IURL(lp_iurl);

  return(lp_comm_unit->data_size);

} /* IBP_load */


/*****************************/
/* IBP_manage                */
/* It manages a file in a depot     */
/* It is an important IBP API         */
/* return -- OK / not             */
/*****************************/
int IBP_manage (IBP_cap    ps_cap,
    IBP_timer  ps_timeout,
    int    pi_cmd,
    int    pi_cap_type,
    IBP_CapStatus  ps_info){

  int              li_return;
  IURL              *lp_iurl;
  char              lc_line_buf[CMD_BUF_SIZE];
  time_t            lt_life;
  time_t            lt_now;
  IBP_CommSession   ls_comm_session;
  IBP_CommUnit_t    *lp_comm_unit;

  /*
   * initialize error variable
   */
  IBP_errno = IBP_OK;
  time(&lt_now);

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(-1);
  }

AGAIN:
  /*
   * get parameters
   */
  if ( ( lp_iurl = create_manage_IURL(ps_cap,ps_timeout->ClientTimeout)) == NULL )
    return(-1);

  if((strcmp(lp_iurl->type,"MANAGE") != 0) && (pi_cmd != IBP_PROBE)){
    IBP_errno = IBP_E_CAP_NOT_MANAGE;
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(-1);
  }

  /*
   * prepare the IBP manage message
   */
  switch(pi_cmd){
  case IBP_INCR:
  case IBP_DECR:
    if((pi_cap_type != IBP_READCAP) && (pi_cap_type != IBP_WRITECAP)){
      IBP_errno = IBP_E_INVALID_PARAMETER;
      close_socket(lp_iurl->fd);
      free_IURL(lp_iurl);
      return(-1);
    }
    sprintf(lc_line_buf,"%d %d %s %s %d %d %d\n",
      IBPv031,
      IBP_MANAGE,
      lp_iurl->key,
      lp_iurl->type_key,
      pi_cmd,
      pi_cap_type,
      ps_timeout->ServerSync);
    break;
  case IBP_CHNG:
  case IBP_PROBE:
    if(ps_info == NULL){
      IBP_errno = IBP_E_INVALID_PARAMETER;
      close_socket(lp_iurl->fd);
      free_IURL(lp_iurl);
      return(-1);
    }
    lt_life = (ps_info->attrib.duration > 0 ? ps_info->attrib.duration-lt_now : 0);
    sprintf(lc_line_buf,"%d %d %s %s %d %d %lu %ld %d %d\n",
      IBPv031,
      IBP_MANAGE,
      lp_iurl->key,
      lp_iurl->type_key,
      pi_cmd,
      pi_cap_type,
      ps_info->maxSize,
      lt_life,
      ps_info->attrib.reliability,
      ps_timeout->ServerSync);
    break;
  default:
    IBP_errno = IBP_E_INVALID_CMD;
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(-1);
  }

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_timeout->ClientTimeout);

  /*
   * fill out the units
   */
  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(-1);
  }

  /*
   * receive the server's response
   */
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleStoreResp) != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(-1);
  }

  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
  IBP_errno = IBP_OK;
  goto AGAIN;
    }
    return(-1);
  }


  if(pi_cmd == IBP_PROBE){
    if (sscanf(lp_comm_unit->data_buf,"%d %d %d %d %lu %ld %d %d",
         &li_return,
         &ps_info->readRefCount,
         &ps_info->writeRefCount,
         &ps_info->currentSize,
         &ps_info->maxSize,
         &ps_info->attrib.duration,
         &ps_info->attrib.reliability,
         &ps_info->attrib.type) != 8 ){
      IBP_errno = IBP_E_GENERIC;
      close_socket(lp_iurl->fd);
      free_IURL(lp_iurl);
      return(-1);
    }
    ps_info->attrib.duration += lt_now;
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  free_IURL(lp_iurl);
  return (0);


} /* IBP_manager */


/*****************************/
/* IBP_status                */
/* It manages the parameters in a depot.  It can be only used by administrator     */
/* It is an important IBP API         */
/* return -- data info             */
/*****************************/
IBP_DptInfo IBP_status( IBP_depot          ps_depot,
                 int                pi_StCmd,
                 IBP_timer          ps_timeout,
                 char                *pc_password,
                 unsigned long int    pl_StableStor,
                 unsigned long int    pl_VolStor,
                 long                pl_Duration){

  int                     li_fd;
  int                     li_return;
  char                    lc_line_buf[4096];
  char                    lc_new_buf[4096];
  /*
  char                    lc_line_buf[CMD_BUF_SIZE];
  char                    lc_new_buf[CMD_BUF_SIZE];
  */
  IBP_DptInfo             ls_dpt_info;
  IBP_CommSession          ls_comm_session;
  IBP_CommUnit_t          *lp_comm_unit;
  char                    tmpRID[MAX_RID_LEN+1];
  char                    dftPW[]="ibp";
  int                     version;

  version = IBPv040;

  /*
   * initialize errno variable
   */
  IBP_errno = IBP_OK;
  ls_dpt_info = &glb_dpt_info;

  /*
   * check parameters
   */
  if((li_return = check_depot(ps_depot->host,&(ps_depot->port))) != IBP_OK){
    IBP_errno = li_return;
    return(NULL);
  }

  if ((li_return = d_CheckAttrRid(ps_depot->rid)) != IBP_OK ){
      IBP_errno = li_return;
      return(NULL);
  }

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(NULL);
  }

  /*
   * check password
   */
  if ( pc_password == NULL ) {
    pc_password = dftPW;
  }

  if((pi_StCmd != IBP_ST_INQ) && (pi_StCmd !=  IBP_ST_CHANGE)){
    IBP_errno = IBP_E_GENERIC;
    return(NULL);
  }

#if 0
  if ( RID == NULL ){
    tmpRID[0] = '#';
    tmpRID[1] ='\0';
  }else{
    strncpy(tmpRID,RID,MAX_RID_LEN);
    tmpRID[MAX_RID_LEN] = '\0';
  }

#endif
  if ( (pi_StCmd == IBP_ST_CHANGE ) && (strcmp(tmpRID,"*") == 0) ){
    IBP_errno = IBP_E_GENERIC;
    return (NULL);
  }

/*
  ls_dpt_info = createDptInfo();
  if ( ls_dpt_info == NULL ){
    IBP_errno = IBP_E_ALLOC_FAILED;
    return (NULL);
  }
*/
AGAIN:
nextVersion:
  /*
   * connect to the server
   */

  if((li_fd = connect_socket(ps_depot->host,ps_depot->port,ps_timeout->ClientTimeout)) == -1 ){
    IBP_errno = IBP_E_CONNECTION;
    /*freeDptInfo(ls_dpt_info);*/
    return(NULL);
  }

  if ( set_socket_noblock(li_fd) != 0 ){
    IBP_errno = IBP_E_CONNECTION;
    close_socket(li_fd);
    /*freeDptInfo(ls_dpt_info);*/
    return(NULL);
  }

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_timeout->ClientTimeout);

  /*
   * send IBP_MANAGE command to server
   */

  switch( version ){
    case  IBPv031:
      sprintf(lc_line_buf,"%d %d %d %s %d\n",
                          version,
                          IBP_STATUS,
                          pi_StCmd,
                          pc_password,
                          ps_timeout->ServerSync);
      break;
    case IBPv040:
      sprintf(lc_line_buf,"%d %d %d %d %s %d\n",
                          version,
                          IBP_STATUS,
                            ps_depot->rid,
                          pi_StCmd,
                          pc_password,
                          ps_timeout->ServerSync);
      break;
  }



  if (FillCommUnit(&ls_comm_session,COM_OUT,li_fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
    close_socket(li_fd);
    /*freeDptInfo(ls_dpt_info);*/
    return(NULL);
  }

  if ( pi_StCmd == IBP_ST_CHANGE ){
    if (FillCommUnit(&ls_comm_session,COM_IN,li_fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,(func_pt*)HandleStoreResp) != IBP_OK ){
      close_socket(li_fd);
      /*freeDptInfo(ls_dpt_info);*/
      return(NULL);
    }

    sprintf(lc_new_buf,"%lu %lu %ld \n",
                        pl_StableStor,
                        pl_VolStor,
                        pl_Duration);
    if (FillCommUnit(&ls_comm_session,COM_OUT,li_fd,lc_new_buf,strlen(lc_new_buf),DATAVAR,NULL) != IBP_OK ){
      close_socket(li_fd);
      /*freeDptInfo(ls_dpt_info);*/
      return(NULL);
    }
  }

  switch ( version ){
    case IBPv040:
      if ( FillCommUnit(&ls_comm_session,COM_IN,li_fd,lc_line_buf,CMD_BUF_SIZE,DATAVAR,(func_pt*)HandleStatusResp) != IBP_OK){
        close_socket(li_fd);
        /*freeDptInfo(ls_dpt_info);*/
        return(NULL);
      }
      break;
  }

  if (FillCommUnit(&ls_comm_session,COM_IN,li_fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL) != IBP_OK ){
     close_socket(li_fd);
     /*freeDptInfo(ls_dpt_info);*/
     return(NULL);
  }



  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL){
    close_socket(li_fd);
    if ( IBP_errno == IBP_E_BAD_FORMAT || version == IBPv040 ){
      if ( ls_comm_session.buf != NULL ) { free ( ls_comm_session.buf); }
      IBP_errno = IBP_OK;
      version = IBPv031;
      goto nextVersion;
    }
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
        IBP_errno = IBP_OK;
        goto AGAIN;
    }
    /*freeDptInfo(ls_dpt_info);*/
    if ( ls_comm_session.buf != NULL ) { free ( ls_comm_session.buf); }
    return(NULL);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(li_fd);
#else
    releaseConnection(li_fd);
#endif


  switch ( version ){
    case IBPv031:
#if 0
      if ( (ls_dpt_info = createDptInfo()) == NULL ){
        freeDptCfg(dptCfg);
        if (ls_comm_session.buf != NULL ) { free(ls_comm_session.buf); }
        IBP_errno = IBP_E_ALLOC_FAILED;
        return (NULL);
      }
      dptCfg->dptInfo = ls_dpt_info;
#endif
      if(sscanf(lp_comm_unit->data_buf,"%lu %lu %lu %lu %ld",
                      &(ls_dpt_info->StableStor),
                      &(ls_dpt_info->StableStorUsed),
                      &(ls_dpt_info->VolStor),
                      &(ls_dpt_info->VolStorUsed),
                      &(ls_dpt_info->Duration)) != 5){
        IBP_errno = IBP_E_BAD_FORMAT;
        /*freeDptInfo(ls_dpt_info);*/
        if ( ls_comm_session.buf != NULL ) { free ( ls_comm_session.buf); }
        return(NULL);
      }
      break;
    case IBPv040:
      if ( (IBP_errno = fillDptInfo(ls_dpt_info,lp_comm_unit->data_buf)) != IBP_OK ){
        /*freeDptInfo(ls_dpt_info);*/
        if ( ls_comm_session.buf != NULL ) { free (ls_comm_session.buf); }
        return (NULL);
      }
      ls_dpt_info->StableStor = (unsigned long int)(ls_dpt_info->HardServed >> 20);
      ls_dpt_info->StableStorUsed = (unsigned long int)(ls_dpt_info->HardUsed >> 20);
      ls_dpt_info->VolStor = (unsigned long int)(ls_dpt_info->TotalServed >>20);
      ls_dpt_info->VolStorUsed = (unsigned long int)(ls_dpt_info->TotalUsed >> 20);

      break;
  }

  if ( ls_comm_session.buf != NULL ) { free(ls_comm_session.buf); }

  return(ls_dpt_info);

} /* IBP_DptInfo */

int _countToken(char *str, char *token){
  char *tmp,*rest;
  int  n = 0;
  int  len;

  len = strlen(token);

  /*assert ( (str!= NULL) && ( token != NULL));*/
  rest = str;
  do {
    tmp = strstr(rest,token);
    if ( tmp != NULL ){
      n++;
      rest = tmp + len;
    }
  }while ( tmp != NULL );

  return (n);
}

int isValidDptInfo( IBP_DptInfo info ){
  if ( info == NULL ) { return 0; }

  if ( info->majorVersion  < 0 ) { return 0;}
  if ( ( info->nDM > 0 ) && ( info->dmTypes == NULL ) ) { return 0; }
  if ( ( info->nNFU > 0 ) && ( info->NFU == NULL )) { return 0; }

  return 1;

}

#ifdef IBP_WIN32
#define strtok_r( _s, _sep, _lasts ) \
        ( *(_lasts) = strtok( (_s), (_sep) ) )
#endif

int fillDptInfo( IBP_DptInfo info , char *str){
  int   nRS;
  char  *last1,*last2;
  char  dli1[]=" ";
  char  dli2[]=":";
  char  *token1, *token2;
  int   i,nFd,nrs=0;
  int   ret = IBP_OK;
  int   insideRS=0;

  /*assert((str!= NULL)&&(dc != NULL));*/

  if ( (nRS = _countToken(str," RE")) <= 0 ) {
    /*fprintf(stderr,"IMPDE after countToken error \n");*/
    return (IBP_E_BAD_FORMAT);
  }

  token1 = strtok_r(str,dli1,&last1);
  while ( token1 != NULL ){
    nFd = _countToken(token1,dli2);
    token2 = strtok_r(token1,dli2,&last2);
    /*fprintf(stderr," IMPDE token2 = %s \n",token2);*/
    if ( strcmp(token2,ST_VERSION) == 0 ){
      token2 = strtok_r(NULL,dli2,&last2);
      if ( token2 != NULL ){
        if ( 1 != sscanf(token2,"%f",&(info->majorVersion) ) ){
          goto bail;
        }
      }else {
        goto next;
      }
      token2 = strtok_r(NULL,dli2,&last2);
      if ( token2 != NULL ){
        if ( 1 != sscanf(token2,"%f",&(info->minorVersion)) ){
          goto bail;
        }
      }
    }else if ( strcmp(token2,ST_NFU_OP) == 0 ){
      if ( nFd <= 0 ) {
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      if (info->NFU != NULL ) { free(info->NFU);}
      if ( (info->NFU = (int*)calloc(nFd,sizeof(int))) == NULL ){
        ret = IBP_E_ALLOC_FAILED;
        goto bail;
      }
      i = 0;
      while ( (token2 = strtok_r(NULL,dli2,&last2)) != NULL ){
        /*fprintf(stderr,"IMPDE i = %d nFd = %d datamover = %s\n",i,nFd,token2);*/
        if ( i >= nFd ){
          ret = IBP_E_BAD_FORMAT;
          goto bail;
        }
        info->NFU[i] = atoi(token2);
        i++;
      }
      if ( i != nFd ){
        ret = IBP_E_BAD_FORMAT ;
        goto bail;
      }
      info->nNFU = nFd;
    }else if ( strcmp(token2,ST_DATAMOVERTYPE) == 0 ){
      if ( nFd <= 0 ) {
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      if ( info->dmTypes != NULL ) { free(info->dmTypes);}
      if ( (info->dmTypes = (int*)calloc(nFd,sizeof(int))) == NULL ){
        ret = IBP_E_ALLOC_FAILED;
        goto bail;
      }
      i = 0;
      while ( (token2 = strtok_r(NULL,dli2,&last2)) != NULL ){
        /*fprintf(stderr,"IMPDE i = %d nFd = %d datamover = %s\n",i,nFd,token2);*/
        if ( i >= nFd ){
          ret = IBP_E_BAD_FORMAT;
          goto bail;
        }
        info->dmTypes[i] = atoi(token2);
        i++;
      }
      if ( i != nFd ){
        ret = IBP_E_BAD_FORMAT ;
        goto bail;
      }
      info->nDM = nFd;
    }else if ( strcmp(token2,ST_RESOURCEID) == 0 ){
      if ( insideRS == 1 ){
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }else{
        insideRS = 1;
      }
      if ( nrs  > nRS ){
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      if ( (token2 = strtok_r(NULL,dli2,&last2)) == NULL ){
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      if ( 1 != sscanf(token2,"%d", &(info->rid) )){
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
    }else if ( strcmp(token2,ST_RESOURCETYPE) == 0 ){
      if (( insideRS != 1 ) || ( (token2 = strtok_r(NULL,dli2,&last2)) == NULL )) {
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      info->type = atoi(token2);
    }else if (strcmp(token2,ST_CONFIG_TOTAL_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->TotalConfigured));
    }else if (strcmp(token2,ST_SERVED_TOTAL_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->TotalServed));
    }else if (strcmp(token2,ST_USED_TOTAL_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->TotalUsed));
    }else if (strcmp(token2,ST_ALLOC_TOTAL_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->SoftAllocable));
    }else if (strcmp(token2,ST_CONFIG_HARD_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->HardConfigured));
    }else if (strcmp(token2,ST_SERVED_HARD_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->HardServed));
    }else if (strcmp(token2,ST_USED_HARD_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->HardUsed));
    }else if (strcmp(token2,ST_ALLOC_HARD_SZ) == 0 ){
        if ((insideRS != 1)||(token2 = strtok_r(NULL,dli2,&last2))==NULL){
            ret = IBP_E_BAD_FORMAT;
            goto bail;
        }
        sscanf(token2,"%lld",&(info->HardAllocable));
    }else if ( strcmp(token2,ST_DURATION) == 0 ){
      if (( insideRS != 1 ) || ( (token2 = strtok_r(NULL,dli2,&last2)) == NULL )) {
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      sscanf(token2,"%ld",&(info->Duration));
    }else if ( strcmp(token2,ST_RS_END) == 0 ){
      if ( insideRS != 1 ) {
        ret = IBP_E_BAD_FORMAT;
        goto bail;
      }
      nrs ++;
      insideRS = 0;
    }
next:
    token1 = strtok_r(NULL,dli1,&last1);
  }


  if ( insideRS == 1 ){
    return ( IBP_E_BAD_FORMAT);
  }
  if ( ! isValidDptInfo(info) ){
    return ( IBP_E_BAD_FORMAT);
  }

  return (IBP_OK);

bail:

  return ( ret);

}


IBP_DptInfo createDptInfo(){
  IBP_DptInfo info;

  if ( ( info = (IBP_DptInfo)calloc(1,sizeof( struct ibp_dptinfo))) == NULL ){
    return (NULL);
  }

  info->nDM = 0;
  info->nNFU = 0;
  info->majorVersion = -1;
  info->minorVersion = -1;
  info->dmTypes = NULL;
  info->NFU = NULL;
  info->rid = -1;
  return (info);

}

void freeDptInfo( IBP_DptInfo info ){
  if ( info == NULL ) {
    return ;
  }

  if ( info->dmTypes != NULL ) { free(info->dmTypes); }
  if ( info->NFU != NULL ) { free(info->NFU); }

  return ;
}

void printDptCfg( IBP_DptInfo info ){

  int i ;

  if ( info == NULL ) {
    return ;
  }

  if ( info->majorVersion <= 0 ){
    fprintf(stderr,"The information is got from old version depot:\n\n ");
    fprintf(stderr,"\t Stable size = %lu M\n",info->StableStor);
    fprintf(stderr,"\t Used Stable size = %lu M\n",info->StableStorUsed);
    fprintf(stderr,"\t Volatile size = %lu M\n",info->VolStor);
    fprintf(stderr,"\t Used Vol size = %lu M\n",info->VolStorUsed);
  }else {
    fprintf(stderr,"The Depot Configuration:\n");
    fprintf(stderr,"\t Version =  %.1f.%.1f \n",info->majorVersion,info->minorVersion);
    fprintf(stderr,"\t Data Mover =  ");
    for ( i = 0 ; i < info->nDM; i ++ ){
      switch ( info->dmTypes[i] ){
        case DM_TCP:
          fprintf(stderr,"TCP ");
          break;
        case DM_RUDP:
          fprintf(stderr,"Reliable UDP ");
          break;
        case DM_UUDP:
          fprintf(stderr,"Unreliable UDP ");
          break;
      }
    }
    fprintf(stderr,"\n\t NFU ops = ");
    for ( i = 0 ; i < info->nNFU ; i ++ ){
        fprintf(stderr,"%d ",info->NFU[i]);
    };
    fprintf(stderr,"\n");
      fprintf(stderr,"\n\t Resource = %d\n",info->rid);
      fprintf(stderr,"\t Type = ");
      switch(info->type){
        case RS_DISK:
          fprintf(stderr," Disk\n");
          break;
        case RS_RAM:
          fprintf(stderr," Memory\n");
          break;
      }
      fprintf(stderr,"\t Total configured storage = %lld\n",info->TotalConfigured);
      fprintf(stderr,"\t Total served storage = %lld\n",info->TotalServed);
      fprintf(stderr,"\t Total used storage = %lld\n",info->TotalUsed);
      fprintf(stderr,"\t Total allocatable storage = %lld\n",info->SoftAllocable);
      fprintf(stderr,"\t Configured hard storage = %lld\n",info->HardConfigured);
      fprintf(stderr,"\t Served hard storage = %lld\n",info->HardServed);
      fprintf(stderr,"\t Used hard storage = %lld\n",info->HardUsed);
      fprintf(stderr,"\t Allocatable hard storage = %lld\n",info->HardAllocable);
      fprintf(stderr,"\t Duration  = %ld\n",info->Duration);

      fprintf(stderr,"\n");
 }
}




/* ----------------- IBP_MCOPY  IMPLEMENTATION ------------------- */

/*******************************************************************/
/* IBP_mcopy                                                       */
/* Copy data from a src depot to some target IBP depots            */
/* receive parameters -- source cap, array of target caps, number  */
/* of targets, src timeout, array of target timeouts, file size,   */
/* file offset, array of datamover options (types: currently only  */
/* implemented for DM_UNI and DM_BLAST), array of target ports,    */
/* and service (datamover kind: DM_MULTI, DM_SMULTI, DM_BLAST)     */
/* return -- none                                                  */
/*******************************************************************/

ulong_t IBP_mcopy(IBP_cap pc_SourceCap,
      IBP_cap pc_TargetCap[],
      unsigned int pi_CapCnt,
      IBP_timer ps_src_timeout,
      IBP_timer ps_tgt_timeout,
      ulong_t pl_size,
      ulong_t pl_offset,
      int dm_type[],
      int dm_port[],
      int dm_service){

  int       li_return;
  IURL      *lp_iurl;
  char      lc_line_buf[CMD_BUF_SIZE],*lc_line_buf2;
  ulong_t   li_SizeOfBuffer;
  IBP_CommSession  ls_comm_session;
  IBP_CommUnit_t  *lp_comm_unit;
  char      *ps_targets, *ps_ports, *ps_options, *ps_targsync, *ps_targtout;

  /**fprintf(stderr,"in IBP_mcopy call: <<<%s>>>\n",pc_SourceCap);*/
  /*
   * initialize error variable
   */
  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_src_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  if ( pc_TargetCap == NULL ){
    IBP_errno = IBP_E_WRONG_CAP_FORMAT;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }
AGAIN:
  /*
   * get parameters
   */
  if ( (lp_iurl = create_read_IURL(pc_SourceCap,ps_src_timeout->ClientTimeout)) == NULL )
    return(0);

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_src_timeout->ClientTimeout);

  ps_targets = DM_Array2String(pi_CapCnt,(void *)pc_TargetCap,HOSTS);
  ps_ports = DM_Array2String(pi_CapCnt,(void *)dm_port,PORTS);
  ps_options = DM_Array2String(pi_CapCnt,(void *)dm_type,OPTIONS);
  ps_targsync = DM_Array2String(pi_CapCnt,(void *)ps_tgt_timeout,SERVSYNCS);
  ps_targtout = DM_Array2String(pi_CapCnt,(void *)ps_tgt_timeout,TIMEOUTS);

  /*
   * send IBP_MCOPY commands to server
   */

  li_SizeOfBuffer = CMD_BUF_SIZE * pi_CapCnt;

  sprintf(lc_line_buf,"%d %d %ld\n",
    IBPv031,
    IBP_MCOPY,
    li_SizeOfBuffer);

  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL)
      != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }
  /********
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL)
      != IBP_OK ){
    free_IURL(lp_iurl);
    return(0);
  }
  */

  lc_line_buf2 = (char *)malloc(sizeof(char) * li_SizeOfBuffer);

  sprintf(lc_line_buf2,"%d %d +s %s +t%s +k %s +f %lu +z %lu +i %d %s %s +o%s +p %s +n %d +d %d\n",
    IBPv031,
    IBP_MCOPY,
    lp_iurl->key,
    ps_targets,
    lp_iurl->type_key,
    pl_offset,
    pl_size,
    ps_src_timeout->ServerSync,
    ps_targsync,
    ps_targtout,
    ps_options,
    ps_ports,
    pi_CapCnt,
    dm_service);

  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf2,strlen(lc_line_buf2),DATAVAR,NULL)
      != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */

  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL)
      != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
  IBP_errno = IBP_OK;
  goto AGAIN;
    }
    return(0);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  free_IURL(lp_iurl);
  free(ps_targets);
  free(ps_ports);
  free(ps_options);
  free(ps_targsync);
  free(ps_targtout);
  free(lc_line_buf2);

  return(lp_comm_unit->data_size);

}
/* ---------------------------------------------------------------- */


/* ---------------- IBP DATAMOVER INTERNAL CMD -------------------- */
ulong_t IBP_datamover( IBP_cap pc_TargetCap,
           IBP_cap pc_ReadCap,
           IBP_timer ps_tgt_timeout,
           ulong_t pl_size,
           ulong_t pl_offset,
           int dm_type,
           int dm_port,
           int dm_service){

  int  li_return;
  IURL  *lp_iurl;
  char  lc_line_buf[CMD_BUF_SIZE],*lc_line_buf2;
  ulong_t  li_SizeOfBuffer;
  IBP_CommSession  ls_comm_session;
  IBP_CommUnit_t  *lp_comm_unit;

  fprintf(stderr,"in IBP_datamover INTERNAL call \n");
  /*
   * initialize error variable
   */
  IBP_errno = IBP_OK;

  /*
   * check time out
   */
  if ( (li_return = check_timeout(ps_tgt_timeout)) != IBP_OK ){
    IBP_errno = li_return;
    return(0);
  }

  if ( pc_TargetCap == NULL ){
    IBP_errno = IBP_E_WRONG_CAP_FORMAT;
    return(0);
  }

  /*
   * check data size
   */
  if ( pl_size <= 0 ){
    IBP_errno = IBP_E_INV_PAR_SIZE;
    return(0);
  }

AGAIN:
   if ( (lp_iurl = create_read_IURL(pc_ReadCap,ps_tgt_timeout->ClientTimeout)) == NULL )
     return(0);

  /*
   * initialize communication session
   */
  InitCommSession(&ls_comm_session,ps_tgt_timeout->ClientTimeout);


  /*
   * send IBP_DATAMOVER command to server
   */

  li_SizeOfBuffer = CMD_BUF_SIZE;

  sprintf(lc_line_buf,"%d %d %d\n\x0",
    IBPv031,
    IBP_MCOPY,
    li_SizeOfBuffer,EOF);

  /***fprintf(stderr,"1) IBP_datamover sending 1: <%s>\n",lc_line_buf);*/
  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf,strlen(lc_line_buf),DATAVAR,NULL)
      != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*******
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL)
      != IBP_OK ){
    free_IURL(lp_iurl);
    return(0);
  }
  */

  lc_line_buf2 = (char *)malloc(sizeof(char) * li_SizeOfBuffer);

  sprintf(lc_line_buf2,"%s %s %s %lu %lu %d %d %d %d %d\n",
    /**IBPv031,
       IBP_MCOPY,*/
    lp_iurl->key,
    capToString(pc_TargetCap),
    lp_iurl->type_key,
    pl_offset,pl_size,
    ps_tgt_timeout->ServerSync,
    ps_tgt_timeout->ClientTimeout,
    dm_type, dm_port, DM_SERVER);

  /***fprintf(stderr,"2) IBP_datamover sending: <%s>\n",lc_line_buf2); */
  if (FillCommUnit(&ls_comm_session,COM_OUT,lp_iurl->fd,lc_line_buf2,strlen(lc_line_buf2),DATAVAR,NULL)
      != IBP_OK ){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    return(0);
  }

  /*
   * receive the server's response
   */

  /*   if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL)
       != IBP_OK ){
    free_IURL(lp_iurl);
    return(0);
  }
  */

  /*
   * do communication transaction
   */
  if(( lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL){
    close_socket(lp_iurl->fd);
    free_IURL(lp_iurl);
    if( ( IBP_errno == IBP_E_SOCK_READ || IBP_errno == IBP_E_SOCK_WRITE) &&
        glbReusedConn == 1 ){
  IBP_errno = IBP_OK;
  goto AGAIN;
    }
    return(0);
  }

#if !IBP_PERSISTENT_CONN
    close_socket(lp_iurl->fd);
#else
    releaseConnection(lp_iurl->fd);
#endif

  free_IURL(lp_iurl);

  return(lp_comm_unit->data_size);

}
/* --------------------------------------------------- */


/* ------------- DATA MOVER: Arrays/string conversion ------------- */
/* Array2String: for use by data mover client                       */
/* converts the given array into a formated string, depending on    */
/* the array "type" parameter, returns the string                   */
/* -----------------------------------------------------------------*/
char* DM_Array2String(int numelems, void  *array[], int type)
{
  char *target,temp[16],*tempcap;
  int i;
  target = (char *)malloc(sizeof(char)*numelems*MAX_CAP_LEN);
  bzero(target,sizeof(char)*numelems*MAX_CAP_LEN);

  switch(type){
  case HOSTS:
    /*strcat(target,"+t");*/
    for(i=0; i<numelems; i++){
      strcat(target," ");
      tempcap = capToString((IBP_cap) array[i]);   /* check this IBP_cap * data type */
      strcat(target,tempcap);
    }
    break;
  case KEYS:
    /*strcat(target,"+k");*/
    for(i=0; i<numelems; i++){
      sprintf(temp," %d",(int)array[i]);
      strcat(target,temp);
    }
    break;
  case PORTS:
    /*strcat(target,"+p");*/
    for(i=0; i<numelems; i++){
      sprintf(temp," %d",(int)array[i]);
      strcat(target,temp);
    }
    break;
  case OPTIONS:
    /*strcat(target,"+o");*/
    for(i=0; i<numelems; i++){
      sprintf(temp," %d",(int)array[i]);
      strcat(target,temp);
    }
    break;
  case SERVSYNCS:
    for(i=0; i<numelems; i++){
      sprintf(temp," %d",(int)(((IBP_timer)array)[i].ServerSync));
      strcat(target,temp);
    }
    break;
  case TIMEOUTS:
    for(i=0; i<numelems; i++){
      sprintf(temp," %d",(int)(((IBP_timer)array)[i].ClientTimeout));
      strcat(target,temp);
    }
    break;
  }
  return target;
}



#ifdef HAVE_PTHREAD_H

pthread_once_t dpt_info_once = PTHREAD_ONCE_INIT;
static pthread_key_t dpt_info_key;

void _dpt_info_destructor( void *ptr)
{
  free(ptr);
}

void
_dpt_info_once(void)
{
  pthread_key_create(&dpt_info_key,_dpt_info_destructor);
}

struct ibp_dptinfo *
_glb_dpt_info(void)
{
  struct ibp_dptinfo *output;

  pthread_once(&dpt_info_once,_dpt_info_once);
  output = (struct ibp_dptinfo*)pthread_getspecific(dpt_info_key);
  if ( output == NULL ) {
    output = (struct ibp_dptinfo*)calloc(1,sizeof(struct ibp_dptinfo));
    pthread_setspecific(dpt_info_key,output);
  }
  return(output);
}

#endif
