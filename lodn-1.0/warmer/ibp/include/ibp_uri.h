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

# ifndef _IBP_URI_H
# define _IBP_URI_H

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# include "ibp_os.h"
# include "ibp_ClientLib.h"

# ifdef __cpluscplus 
extern "C" {
# endif /* __cpluscplus */

# define IBP_CAP_FIELD_NUM		6
# define IBP_CAP_TYPE_READ		1
# define IBP_CAP_TYPE_WRITE		2
# define IBP_CAP_TYPE_MANAGE	3

# define d_CheckHost(host)  ((host == NULL) ? IBP_E_INV_PAR_HOST : IBP_OK )
# define d_CheckPort(port)  ((port == 0) ? IBP_DATA_PORT : (d_PortInRange(port)) )
# define d_CheckAttrRid(rid) ((rid >= 0) ? IBP_OK : IBP_E_INVALID_RID)
# define d_PortInRange(port)  (((port < 1024) || (port > 65535)) ? IBP_E_INV_PAR_PORT : port )
# define d_CheckAttrRely(rely) (((rely == IBP_STABLE) || (rely == IBP_VOLATILE)) ? IBP_OK : IBP_E_INV_PAR_ATRL)
# define d_CheckAttrType(type) (((type == IBP_BYTEARRAY) || (type == IBP_BUFFER) || (type == IBP_FIFO) || (type == IBP_CIRQ)) ? IBP_OK : IBP_E_INV_PAR_ATTP)  
 
	
typedef struct iurl {
	char    *hostname;
	int     port;
	char    *key;
	char    *type_key;
	char    *type;
	int	fd;
}IURL;

void free_IURL( IURL *iurl);
IURL *create_read_IURL( IBP_cap ps_cap, int timeout);
IURL *create_write_IURL( IBP_cap ps_cap, int timeout);
IURL *create_manage_IURL( IBP_cap ps_cap, int timeout);

int get_cap_fields_num (char *cap, char sep);
int get_cap_fields( char **fields,char *cap, char sep);
int check_depot (char *pc_host, int *pi_port);

extern char *capToString(IBP_cap cap);


# ifdef __cpluscplus
extern "C" {
# endif /* __cplusplus */

# endif /* _IBP_URI_H */

