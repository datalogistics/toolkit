/*            IBP version 1.1:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *                Authors: A. Bassi, M. Beck, Y. Zheng
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
 * ibp-lib.h
 *
 * in case of bugs, problems, suggestions please contact: ibp@cs.utk.edu
 *
 */

#ifndef IBP_LIB_H
#define IBP_LIB_H

#ifdef HAVE_CONFIG_H
#include "config-ibp.h"
#endif
#include "ibp_base.h"
#include "ibp_ClientLib.h"
#include "ibp_protocol.h"

# define YES	1
# define NO	0 

extern int ibpStrToInt(char *str,int *ret);
extern int Write (int pi_fd, char *pc_buf, size_t pi_size);
extern long Read (int pi_fd, char *pc_buf, size_t pi_size);
extern int ReadLine (int pi_fd, char *pc_buf, size_t pi_size);
extern void handle_error (int nerr, void *errinfo);
extern char *getlocalhostname ();
extern char *getfullhostname (char *name);
extern void Listen (int sockfd);
extern int SocketConnect (char *host, int port);
extern int SocketConnect_t ( char *host, int port , int timeout);
extern int Socket2File (int sockfd, FILE *fp, ulong_t *size,time_t pl_expireTime);
extern int File2Socket(FILE *pf_fp, int pi_sockfd, ulong_t pl_size,time_t pl_expireTime);
extern int Fifo2Socket (FILE *pf_fp, 
			int sockfd,
			ulong_t size,
			ulong_t fifoSize,
			ulong_t pl_rdPtr,
                        int    pi_HeaderLen,
                        time_t pl_expireTime);
extern int serve_known_socket(char *hostName, int port);
extern char *IBP_substr (char *pc_buffer, char *pc_first);
extern int todelete (char *action);
extern int Socket2Queue (int pi_sockfd, 
			 FILE *pf_fp,
			 ulong_t pl_size,
			 ulong_t pl_fifoSize,
			 ulong_t pl_wrcount,
			 ulong_t *pl_written,
			 int	 pi_HeaderLen,
                         time_t pl_expireTime);
extern void freeIURL( IURL *iurl);
extern IURL *createReadIURL( IBP_cap ps_cap, int timeout);
extern IURL *createWriteIURL( IBP_cap ps_cap, int timeout);
extern IURL *createManageIURL( IBP_cap ps_cap, int timeout);
extern char *trim (char *str);
extern int GetCapFieldsNum (char *cap, char sep);
extern int GetCapFields( char **fields,char *cap, char sep);
extern int CheckDepot (char *pc_host, int *pi_port);
extern int CheckTimeout(IBP_timer ps_timeout);
extern char *capToString(IBP_cap cap);


/*
 * Name	 :	ibp_ltostr
 *
 * Note	 :	Convert an ibp_long integer to a string
 *
 * Para	 :	value --  value to be converted
 *		buf   --  pointer to the END of the buffer passed
 *		base  --  base of converted string, must 2=< base =< 36		
 *
 * return:	upon success, return pointer of converted string, otherwise
 *		returns null pointer.
 *
 * error:       ENIVAL -- The value of base is not supported.	
 */
extern char *ibp_ltostr(ibp_long value, char *buf, int base);


/*
 * Name	 :	ibp_ultostr
 *
 * Note	 :	Convert an ibp_ulong integer to a string
 *
 * Para	 :	value --  value to be converted
 *		buf   --  pointer to the END of the buffer passed
 *		base  --  base of converted string, must 2=< base =< 36		
 *
 * return:	upon success, return pointer of converted string, otherwise
 *		returns null pointer.
 *
 * error:       ENIVAL -- The value of base is not supported.	
 */
extern char *ibp_ultostr(ibp_ulong value, char *buf, int base);

/*
 * Name	 :	ibp_strtol
 *
 * Note	 :	Convert a string to an ibp_long integer 
 *
 * Para	 :	str   --  pointer of the string 
 *
 * return:	upon on success , returns converted value; otherwise return 0.
 * 
 * errors:	EINVAL	The value of base is not supported.
 *		ERANGE	The value to be returned is not representable. 
 */
extern ibp_long  ibp_strtol( char *str, int base);

/*
 * Name	 :	ibp_strtoul
 *
 * Note	 :	Convert a string to an ibp_ulong integer 
 *
 * Para	 :	str   --  pointer of the string 
 *
 * return:	upon on success , returns converted value; otherwise return 0.
 * 
 * errors:	EINVAL	The value of base is not supported.
 *		ERANGE	The value to be returned is not representable. 
 */
extern ibp_ulong ibp_strtoul( char *str, int base);

/*
 * Name	 :	ibp_getStrSize
 *
 * Note	 :	Get the size of storage ( vol or stable) from a string
 *		The format of string can be as following:
 *		ddd	 --- for ddd bytes
 *		ddd[k|K] --- for ddd Kbytes
 *		ddd[m|M] --- for ddd Mbytes
 *		ddd[g|G] --- for ddd Gbytes 
 *
 * Para	 :	str   --  pointer of the string 
 *
 * return:	upon on success , returns converted value; otherwise return 0.
 * 
 * errors:	EINVAL	The value of base is not supported.
 *		ERANGE	The value to be returned is not representable. 
 */
extern ibp_ulong ibp_getStrSize( char *str) ;

#endif
