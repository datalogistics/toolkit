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
 * ibp_ComMod.h defines the data structure and subroutines used by
 * ibp communication module.
 */

#ifndef _IBP_COMMOD_H 
#define _IBP_COMMOD_H

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# include "ibp_os.h"
# include <time.h>

# ifdef __cplusplus
extern "C" {
# endif

#define MAX_COMM_UNIT	256

typedef func_pt (int (*func)());

typedef enum{
	COM_IN = 0, COM_OUT = 1
}ComDir_t;

typedef enum{
	DATAFIXED = 0, DATAVAR = 1
}DataType_t;

typedef struct {
	ComDir_t		direction;
	int				fd;
	unsigned long	data_size;
	void			*data_buf;
	DataType_t		resp_type;
	int				(*handle_func)();
}IBP_CommUnit_t;

typedef struct {
	time_t			expireTime;
	IBP_CommUnit_t	comm[MAX_COMM_UNIT];
	int				head;
	int				tail;
	int				nunit;
  char      *buf;
}IBP_CommSession;

#define d_GetFirstUnit( ls_comm_se )  &((ls_comm_se)->comm[(ls_comm_se)->head])

#define d_EmptyCommSession( ls_comm_se ) ((ls_comm_se)->nunit == 0)

IBP_CommUnit_t *GetFreeUnit( IBP_CommSession *ps_comm_se );
IBP_CommUnit_t *GetCurrUnit( IBP_CommSession *ps_comm_se );
IBP_CommUnit_t *GetNextUnit( IBP_CommSession *ps_comm_se );
IBP_CommUnit_t *PerformCommunication (IBP_CommSession *ps_comm_se);

int  FillCommUnit(	IBP_CommSession *ps_comm_se, 
					ComDir_t		pe_direction,
					int				pi_fd,
					char			*pp_buffer,
					int				pi_size,
					DataType_t		pe_datatype,
					func_pt			*pf_func);

void InitCommSession(IBP_CommSession *ps_comm_session, int pi_timeout);

int HandleMsgResp( IBP_CommSession	*ps_comm_session,
					 char				*pc_buf, 
					 unsigned long int	pl_size);
int HandleStatusResp( IBP_CommSession	*ps_comm_session,
					 char				*pc_buf, 
					 unsigned long int	pl_size);
int HandleStoreResp( IBP_CommSession	*ps_comm_session,
					 char				*pc_buf, 
					 unsigned long int	pl_size);

int HandleCopyResp( IBP_CommSession		*ps_comm_session,
					char				*pc_data,
					unsigned long int	pl_size);

int HandleLoadResp( IBP_CommSession		*ps_comm_session,
					char				*pc_data,
					unsigned long int	pl_size);






# ifdef __cplusplus 
}
# endif

#endif /* _IBP_COMMOD_H */

