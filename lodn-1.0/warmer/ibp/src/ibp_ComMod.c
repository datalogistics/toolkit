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
# include <stdio.h>
# endif /* STDC_HEADERS */
# include <assert.h>
# include <stdlib.h>
# include "ibp_ComMod.h"
# include "ibp_net.h"
# include "ibp_protocol.h"
# include "ibp_authentication.h"

#define IBP_COM_CMD_BUF_SIZE 1024

/*****************************/
/* GetFreeUnit               */
/* It returns the first free unit in the communication units      */
/* return -- the first free unit number          */
/*****************************/
IBP_CommUnit_t *GetFreeUnit( IBP_CommSession *ps_comm_se ){

  IBP_CommUnit_t	*ls_comm_unit;
  
  if ( ps_comm_se->nunit == MAX_COMM_UNIT )
    return(NULL);
  
  ls_comm_unit = &(ps_comm_se->comm[ps_comm_se->tail]);
  ps_comm_se->tail = (ps_comm_se->tail + 1 )% MAX_COMM_UNIT;
  ps_comm_se->nunit++;
  
  return(ls_comm_unit);
} /* GetFreeUnit */


IBP_CommUnit_t *GetCurrUnit( IBP_CommSession *ps_comm_se){
  return(&(ps_comm_se->comm[ps_comm_se->head]));
} /* GetCurrUnit */


IBP_CommUnit_t *GetNextUnit( IBP_CommSession *ps_comm_se){

  int	li_next;
  
  li_next = (ps_comm_se->head + 1)%MAX_COMM_UNIT;
  return(&(ps_comm_se->comm[li_next]));
} /* GetNextUnit */

/*****************************/
/* DelFirstUnit               */
/* It frees the first used unit in the communication units      */
/* return -- none            */
/*****************************/
void DelFirstUnit(IBP_CommSession *ps_comm_se){
  if ( ps_comm_se->nunit > 0){
    ps_comm_se->head = (ps_comm_se->head + 1)%MAX_COMM_UNIT;
    ps_comm_se->nunit--;
  }
} /* DelFirstUnit */



/*****************************/
/* WriteData                 */
/* It sends formatted data to the target      */
/* return -- OK / not          */
/*****************************/
int WriteData(IBP_CommUnit_t *ps_comm_unit, time_t pl_expireTime){
	unsigned long int retVal;

	retVal = write_data(ps_comm_unit->fd,ps_comm_unit->data_size,ps_comm_unit->data_buf,pl_expireTime);
	if ( retVal == 0 && IBP_errno != IBP_OK )
		return(IBP_errno);
	
	ps_comm_unit->data_size = retVal;
	return(IBP_OK);
  
} /* WriteData */


/*****************************/
/* ReadVarData                 */
/* It reads unformatted data from the socket   */
/* return -- OK / not          */
/*****************************/
int ReadVarData( IBP_CommUnit_t *ps_comm_unit, time_t pl_expireTime){
  
	unsigned long int retVal;

	retVal = read_var_data(ps_comm_unit->fd,ps_comm_unit->data_size,ps_comm_unit->data_buf ,pl_expireTime);
	if ( retVal == 0 && IBP_errno != IBP_OK )
		return(IBP_errno);

	ps_comm_unit->data_size = retVal;

	return(IBP_OK);
	
} /* ReadVarData */


/*****************************/
/* ReadFixData                 */
/* It reads fixed formatted data from the socket   */
/* return -- OK / not          */
/*****************************/
int ReadFixData( IBP_CommUnit_t *ps_comm_unit, time_t pl_expireTime){

	unsigned long int retVal;

	retVal = read_fix_data(ps_comm_unit->fd,ps_comm_unit->data_size,ps_comm_unit->data_buf,pl_expireTime);
	if ( retVal == 0 && IBP_errno != IBP_OK )
		return(IBP_errno);

	ps_comm_unit->data_size = retVal;

	return(IBP_OK);


} /* ReadFixData */

/*****************************/
/* ChooseFunction                 */
/* It chooses one function from WriteData(), ReadVarData() and ReadFixData(), to implement the info exchange    */
/* return -- the pointer of function          */
/*****************************/
func_pt *ChooseFunction( IBP_CommUnit_t *ps_comm_unit){
  switch(ps_comm_unit->direction){
  case COM_OUT:
    return((func_pt*)WriteData);
  case COM_IN:
    switch(ps_comm_unit->resp_type){
    case DATAFIXED:
      return((func_pt*)ReadFixData);
    case DATAVAR:
      return((func_pt*)ReadVarData);
    default:
      return(NULL);
    }
    break;
  default:
    return(NULL);
  }
} /* ChooseFunction */


/*****************************/
/* PerformCommunication      */
/* It analizes the communication units and chooses proper functions to execute them */
/* return -- the unit next to the last used one / NULL          */
/*****************************/
IBP_CommUnit_t *PerformCommunication( IBP_CommSession *ps_comm_se){

  int	         (*lf_func)();
  int	         (*lf_handlefunc)();
  IBP_CommUnit_t *ls_comm_unit = NULL;



  while( !d_EmptyCommSession(ps_comm_se)){
    ls_comm_unit = d_GetFirstUnit(ps_comm_se);
    if (ls_comm_unit != NULL) {
      if((lf_func = ChooseFunction(ls_comm_unit)) == NULL){
	IBP_errno = IBP_E_UNKNOWN_FUNCTION;
	return(NULL);
      }
      if(lf_func(ls_comm_unit,ps_comm_se->expireTime) != IBP_OK){
	return(NULL);
      }
      if((lf_handlefunc = ls_comm_unit->handle_func) != NULL){
	if ( lf_handlefunc(ps_comm_se,ls_comm_unit->data_buf,ls_comm_unit->data_size) != IBP_OK)
	  return(NULL);
      }
      DelFirstUnit(ps_comm_se);
    }

  }
  return(ls_comm_unit);
}/* PerformCommunication */


/*****************************/
/* FillCommUnit              */
/* It fills the message to communication unit.     */
/* return -- IBP_OK or error number                */
/*****************************/
int  FillCommUnit(	IBP_CommSession *ps_comm_se, 
					ComDir_t		pe_direction,
					int				pi_fd,
					char			*pp_buffer,
					int				pi_size,
					DataType_t		pe_datatype,
					func_pt			*pf_func){

  IBP_CommUnit_t	*lp_comm_unit;

  if ((lp_comm_unit = GetFreeUnit(ps_comm_se)) == NULL ){
    IBP_errno = IBP_E_TOO_MANY_UNITS;
    return(IBP_errno);
  }

  lp_comm_unit->direction = pe_direction;
  lp_comm_unit->fd = pi_fd;
  lp_comm_unit->data_buf = pp_buffer;
  lp_comm_unit->data_size = pi_size;
  lp_comm_unit->handle_func = pf_func;
  if ( lp_comm_unit->direction == COM_IN){
    lp_comm_unit->resp_type = pe_datatype;
  }
	
  return(IBP_OK);
}

/*****************************/
/* InitCommSession           */
/* It initializes the communication unit.     */
/* return -- IBP_OK or error number                */
/*****************************/
void InitCommSession(IBP_CommSession *ps_comm_session, int pi_timeout){
  ps_comm_session->nunit = 0;
  ps_comm_session->head = 0;
  ps_comm_session->tail = 0;
  ps_comm_session->buf = NULL;
  if ( pi_timeout == 0 )
	ps_comm_session->expireTime = 0;
  else
	ps_comm_session->expireTime = time(0) + pi_timeout;
}

int HandleMsgResp( IBP_CommSession  *cs,
                   char             *buf,
                   unsigned long int    size)
{
    int resp;
    int ret = IBP_OK;
    IBP_CommUnit_t *cu;

    if ( sscanf(buf,"%d",&resp) != 1 ){
        IBP_errno = IBP_E_BAD_FORMAT;
        return (IBP_errno);
    }

    if ( resp == IBP_AUTHENTICATION_REQ ){
       cu = GetCurrUnit(cs); 
       ret = ibpCliAuthentication(cu->fd); 
       if ( ret == IBP_OK){
            assert( cu->direction == COM_IN && cu->resp_type == DATAVAR);
            cu->data_size = IBP_COM_CMD_BUF_SIZE; 
            ret = ReadVarData(cu,cs->expireTime);
            return (ret);
       }else{
          IBP_errno = ret;
          return ( IBP_errno);
       }
    }else{
        return (IBP_OK);
    }
}

/*****************************/
/* HandleStoreResp           */
/* It handles the return of IBP_store()     */
/* return -- IBP_OK or error number                */
/*****************************/
int HandleStoreResp(IBP_CommSession		*ps_comm_session,
					char				*pc_buf, 
					unsigned long int	pl_size){
  int	li_resp;
  IBP_CommUnit_t *cu;
  int   ret;

  if ( sscanf(pc_buf,"%d",&li_resp) != 1){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }
  if ( li_resp == IBP_AUTHENTICATION_REQ ){
    cu = GetCurrUnit(ps_comm_session);
    ret = ibpCliAuthentication(cu->fd);
    if ( ret == IBP_OK){
        assert(cu->direction == COM_IN && cu->resp_type == DATAVAR);
        cu->data_size = IBP_COM_CMD_BUF_SIZE;
        ret = ReadVarData(cu,ps_comm_session->expireTime); 
        if ( ret == IBP_OK ){
            if ( sscanf(cu->data_buf,"%d",&li_resp) != 1 ){
                IBP_errno = IBP_E_BAD_FORMAT;
                return (IBP_errno);
            }else{
                IBP_errno = li_resp;
                return (IBP_errno);
            }
        }else{
            IBP_errno = ret;
            return (IBP_errno);
        }
    }else{
        IBP_errno = ret;
        return(IBP_errno);
    }
  }else if ( li_resp != IBP_OK  ){
    IBP_errno = li_resp;
    return(IBP_errno);
  }

  return(IBP_OK);
} /* HandleStoreResp */


/*****************************/
/* HandleCopyResp           */
/* It handles the return of IBP_copy()     */
/* return -- IBP_OK or error number                */
/*****************************/
int HandleCopyResp(	IBP_CommSession		*ps_comm_session,
					char				*pc_data,
					unsigned long int	pl_size){

  IBP_CommUnit_t 	*lp_comm_unit,*cu;
  int				li_response;
  unsigned long int	ll_truesize;
  int               ret;

  if ( sscanf(pc_data,"%d",&li_response) != 1 ){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }

  if ( li_response == IBP_AUTHENTICATION_REQ){
    cu = GetCurrUnit(ps_comm_session);
    ret = ibpCliAuthentication(cu->fd);
    if ( ret != IBP_OK ){
        IBP_errno = ret;
        return (IBP_errno);
    }
    assert(cu->direction == COM_IN && cu->resp_type == DATAVAR);
    cu->data_size = IBP_COM_CMD_BUF_SIZE;
    ret = ReadVarData(cu,ps_comm_session->expireTime);
    if (ret != IBP_OK ){
        IBP_errno = ret;
        return (IBP_errno);
    }
    pc_data = cu->data_buf;
    if ( sscanf(pc_data,"%d",&li_response) != 1 ){
        IBP_errno = IBP_E_BAD_FORMAT;
        return (IBP_errno);
    }
    if ( li_response != IBP_OK ){
        IBP_errno = li_response;
        return(IBP_errno);
    }
  }else if ( li_response != IBP_OK ){
    IBP_errno = li_response;
    return(IBP_errno);
  }

  if ( sscanf(pc_data,"%d %lu",&li_response,&ll_truesize) != 2 ){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }

  /*
   * fill out the communication unit
   */
  lp_comm_unit = GetCurrUnit(ps_comm_session);

  lp_comm_unit->data_size = ll_truesize;

  return(IBP_OK);

} /* HandleCopyResp */

/*****************************/
/* HandleLoadResp           */
/* It handles the return of IBP_load()     */
/* return -- IBP_OK or error number                */
/*****************************/
int HandleLoadResp( IBP_CommSession		*ps_comm_session,
					char				*pc_data,
					unsigned long int	pl_size){

  IBP_CommUnit_t 	*lp_comm_unit,*cu;
  int				li_response;
  unsigned long int	ll_truesize;
  int               ret;

  if ( sscanf(pc_data,"%d",&li_response) != 1 ){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }

  if ( li_response == IBP_AUTHENTICATION_REQ){
    cu = GetCurrUnit(ps_comm_session);
    ret = ibpCliAuthentication(cu->fd);
    if (ret != IBP_OK){
        IBP_errno = ret;
        return(IBP_errno);
    }
    assert(cu->direction == COM_IN && cu->resp_type == DATAVAR);
    cu->data_size = IBP_COM_CMD_BUF_SIZE;
    ret = ReadVarData(cu,ps_comm_session->expireTime);
    if ( ret != IBP_OK ){
        IBP_errno = ret;
        return (IBP_errno);
    }
    pc_data = cu->data_buf;
    if ( sscanf(pc_data,"%d",&li_response) != 1 ){
        IBP_errno = IBP_E_BAD_FORMAT;
        return (IBP_errno);
    }
    if ( li_response != IBP_OK){
        IBP_errno = li_response;
        return(IBP_errno);
    }
  }else if ( li_response != IBP_OK ){
    IBP_errno = li_response;
    return(IBP_errno);
  }

  if ( sscanf(pc_data,"%d %lu",&li_response,&ll_truesize) != 2 ){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }

  /*
   * fill out the communication unit
   */
  lp_comm_unit = GetNextUnit(ps_comm_session);

  lp_comm_unit->data_size = ll_truesize;

  return(IBP_OK);

} /* HandleLoadResp */

/*****************************/
/* HandleStatusResp           */
/* It handles the return of IBP_load()     */
/* return -- IBP_OK or error number                */
/*****************************/
int HandleStatusResp( IBP_CommSession		*ps_comm_session,
					            char				      *pc_data,
					            unsigned long int	pl_size){

  IBP_CommUnit_t 	*lp_comm_unit,*cu;
  int				li_response;
  int	            len;
  char              *buf;
  int               ret;

  if ( sscanf(pc_data,"%d",&li_response) != 1 ){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }

  if ( li_response == IBP_AUTHENTICATION_REQ ){
    cu = GetCurrUnit(ps_comm_session);
    ret = ibpCliAuthentication(cu->fd);
    if ( ret != IBP_OK ){
        IBP_errno = ret;
        return (IBP_errno);
    }
    assert(cu->direction == COM_IN && cu->resp_type == DATAVAR);
    cu->data_size = IBP_COM_CMD_BUF_SIZE;
    ret = ReadVarData(cu,ps_comm_session->expireTime);
    if ( ret != IBP_OK){
        IBP_errno = ret;
        return (IBP_errno);
    }
    pc_data = cu->data_buf;
    if ( sscanf(pc_data,"%d",&li_response) != 1 ){
        IBP_errno = IBP_E_BAD_FORMAT;
        return (IBP_errno);
    }
  }else if ( li_response != IBP_OK ){
    IBP_errno = li_response;
    return(IBP_errno);
  }

  if ( sscanf(pc_data,"%d %d",&li_response,&len) != 2 ){
    IBP_errno = IBP_E_BAD_FORMAT;
    return(IBP_errno);
  }


  ps_comm_session->buf = NULL;

  /*
   * fill out the communication unit
   */
  lp_comm_unit = GetNextUnit(ps_comm_session);

  
  buf = (char *)calloc(len+1,sizeof(char));
  if ( buf == NULL ) {
    IBP_errno = IBP_E_ALLOC_FAILED;
    return (IBP_errno );
  }
  lp_comm_unit->data_buf = buf ;
  buf[len] = '\0';
  lp_comm_unit->data_size = len;

  ps_comm_session->buf = buf ;

  return(IBP_OK);

} /* HandleLoadResp */
