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
# include <string.h>
# include <stdio.h>
# include <stdlib.h>
# endif
# include "ibp_protocol.h"
# include "ibp_errno.h"
# include "ibp_net.h"
# include "ibp_uri.h"

/*****************************/
/* ltrim_string               */
/* It eliminates blank space in s string  */
/* return -- the new string           */
/*****************************/
char *ltrim_string(char *str){
	int  li_i;
	int  li_j;

	li_i = 0;
	li_j = 0;

	if ( str == NULL )
		return (NULL);

	while ( str[li_i] == ' ' )
		li_i++;
	
	if ( li_i == 0 )
		return(str);

	do{	
		str[li_j] = str[li_i];
		li_j ++;
		li_i ++;
	}while(str[li_i-1] != '\0' );

	return(str);
}

/*****************************/
/* valid_cap_cead              */
/* It checks the validity of the cap  */
/* return -- yes / no        */
/*****************************/
int valid_cap_head( char *capString){
	if ( capString[0] != 'i' && capString[0] != 'I' )
		return(0);
	if ( capString[1] != 'b' && capString[1] != 'B' )
		return(0);
	if ( capString[2] != 'p' && capString[2] != 'P' )
		return(0);
	if ( capString[3] != ':' || capString[4] != '/' || capString[5] != '/')
		return(0);
	return(1);

}

/*****************************/
/* get_cap_fields_num           */
/* It returns the length of a Cap Field  */
/* return -- IBP_OK or error number                */
/*****************************/
int get_cap_fields_num (char *cap, char sep) {
	int li_i;
	int li_nfields;

	li_nfields = 0;
	li_i = -1;
	if (cap == NULL || cap[0] == '\0')
		return -1;
	do {
		li_i ++;	
		if ( cap[li_i] == sep || cap[li_i] == '\0' )
			li_nfields ++;
	} while ( cap[li_i] != '\0' );

	return(li_nfields);
}

/*****************************
 * get_host_port
 * get hostname and port number from capability string
 */
int get_host_port( char **fields, char *cap, char sep){
	char *lst_deli = NULL, *pnt;

	if ( cap == NULL || cap[0] == '\0' ){
		return -1;
	}

	fields[0] = cap;
	pnt = cap;
	while ( *pnt != '\0' ){
		if ( *pnt == sep ){
			lst_deli = pnt;
		}
		pnt++;
	}
	if ( lst_deli != NULL ){
		fields[1] = ( lst_deli + 1);
		*lst_deli = '\0';
	}

	return 0;	
}

/*****************************/
/* get_cap_fields              */
/* It writes a Cap Field into a string  */
/* return -- IBP_OK or error number                */
/*****************************/
int get_cap_fields( char **fields,char *cap, char sep) {

	int	li_i,li_j;
	if ( cap == NULL || cap[0] == '\0' )
		return -1;

	li_i = 0;
	li_j = 0;
	fields[li_j] = cap;
	do{
		if ( cap[li_i] == sep ){
			li_j ++;
			fields[li_j] = cap + li_i + 1;
			cap[li_i] = '\0';
		}
		li_i ++;
	}while( cap[li_i] != '\0' );

	return (0);
}


/*****************************/
/* check_depot                */
/* It checks whether the depot is valid */
/* return -- OK / not        */
/*****************************/
int check_depot (char *pc_host, int *pi_port)
{
  
  int li_return;

  li_return = d_CheckHost(pc_host);
  if (li_return != IBP_OK)
    return (li_return);

  li_return = d_CheckPort(*pi_port); 
  if (li_return < 0)
    return (li_return);
  else
    *pi_port = li_return;

  return IBP_OK;

} /* check_depot */



/*****************************/
/* free_IURL                  */
/* It closes the connection to the IBP depot  */
/* return -- none            */
/*****************************/
void free_IURL( IURL *iurl){
	if ( iurl != NULL){
		if ( iurl->hostname != NULL )
			free(iurl->hostname);
		if ( iurl->key != NULL )
			free(iurl->key);
		if ( iurl->type != NULL)
			free(iurl->type);
		if ( iurl->type_key != NULL)
			free(iurl->type_key);
		/* if ( iurl->fd >= 0 ){
			close_socket(iurl->fd);
			iurl->fd = -1;
		}
		*/
		free(iurl);
	}
}

/*****************************/
/* createIURL                */
/* It builds up the connection to the IBP depot  */
/* return -- the pointer of url    */
/*****************************/
IURL *createIURL ( char *capString, int mode,int timeout  ){
	IURL	*iurl;
	char	*lc_tmp;
	char	*lp_fields[6];
	int		li_return;

	if ( capString == NULL ||  strlen(capString) <= 0 ){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}


	if ( (lc_tmp = strdup(capString)) == NULL ){
		IBP_errno = IBP_E_ALLOC_FAILED ;
		return(NULL);
	}

	ltrim_string(lc_tmp);
	if ( !valid_cap_head(lc_tmp) ){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);
		return(NULL);
	}

	if ( get_cap_fields_num( lc_tmp,'/') != IBP_CAP_FIELD_NUM ) {
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);	
		return(NULL);
	}

	if ( get_cap_fields(lp_fields,lc_tmp,'/') < 0 ){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);	
		return(NULL);
	}

	if ( lp_fields[3] == NULL || ( lp_fields[4] == NULL)|| ( lp_fields[5] == NULL )){
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		free(lc_tmp);
		return(NULL);
	}

	switch( mode ){
		case IBP_CAP_TYPE_READ:
			if ( strcmp("READ",lp_fields[5]) != 0 ){
				IBP_errno = IBP_E_CAP_NOT_READ;
				free(lc_tmp);
				return(NULL);
			}
			break;
		case IBP_CAP_TYPE_WRITE:
			if ( strcmp("WRITE",lp_fields[5]) != 0 ){
				IBP_errno = IBP_E_CAP_NOT_WRITE;
				free(lc_tmp);
				return(NULL);
			}
			break;
		case IBP_CAP_TYPE_MANAGE:
			if ( strcmp("MANAGE",lp_fields[5]) != 0 && strcmp("READ",lp_fields[5]) != 0 && strcmp("WRITE",lp_fields[5]) != 0 ){
				IBP_errno = IBP_E_CAP_NOT_MANAGE;
				free(lc_tmp);
				return(NULL);
			}
			break;
	}

	if ( (iurl = (IURL*)malloc(sizeof(IURL))) == NULL ){
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED;
		return(NULL);
	}

	if ( ( iurl->key = strdup(lp_fields[3])) == NULL){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED ;
		return(NULL);	
	}
	if ( ( iurl->type = strdup(lp_fields[5])) == NULL ){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED;
		return(NULL);
	}
  if ( (iurl->type_key = strdup(lp_fields[4])) == NULL ){
    free_IURL(iurl);
    free(lc_tmp);
    IBP_errno = IBP_E_WRONG_CAP_FORMAT;
    return (NULL);
  }
#if 0
	if ( sscanf(lp_fields[4],"%d",&(iurl->type_key)) != 1 ){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}
#endif

	if ( get_host_port(lp_fields,lp_fields[2],':') < 0 ){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}
	if ( lp_fields[0] == NULL || lp_fields[1] == NULL){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}
	if ( (iurl->hostname = strdup(lp_fields[0])) == NULL){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_ALLOC_FAILED;
		return(NULL);
	}
	if ( sscanf(lp_fields[1],"%d",&(iurl->port)) != 1 ){
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_WRONG_CAP_FORMAT;
		return(NULL);
	}

	/*
	 * create socket connect 
	 */
	if((li_return = check_depot(iurl->hostname,&(iurl->port))) != IBP_OK){
                IBP_errno = li_return;
                free_IURL(iurl);
               	free(lc_tmp); 
		return(NULL);
     }

	if ( (iurl->fd = connect_socket(iurl->hostname,iurl->port,timeout)) == -1 ){
		free_IURL(iurl);
		free(lc_tmp);
		return(NULL);
	}
 
	if ( set_socket_noblock(iurl->fd) != 0 ){
		close_socket(iurl->fd);
		free_IURL(iurl);
		free(lc_tmp);
		IBP_errno = IBP_E_CONNECTION;
		return(NULL);
	}

	free(lc_tmp);
	return(iurl);
}

IURL *create_read_IURL( IBP_cap ps_cap , int timeout){
	return(createIURL((char*)ps_cap,IBP_CAP_TYPE_READ,timeout));
}
IURL *create_write_IURL( IBP_cap ps_cap, int timeout ){
	return(createIURL((char*)ps_cap,IBP_CAP_TYPE_WRITE,timeout));
}
IURL *create_manage_IURL( IBP_cap ps_cap, int timeout){
	return(createIURL((char*)ps_cap,IBP_CAP_TYPE_MANAGE,timeout));
}
