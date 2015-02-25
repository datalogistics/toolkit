/*              LBONE version 1.0:  Logistical Backbone
 *               University of Tennessee, Knoxville TN.
 *                Authors: A. Bassi,  M. Beck, Y. Zheng
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
 * lbone_error.h
 *
 * in case of bugs, problems, suggestions please contact: lbone@cs.utk.edu
 *
 */



#include "ibp.h"
#include "lbone_base.h"

#define LBONE_MAX_ERROR		512
/*
#define E_USAGE		1
#define E_HOMEDIR	11
#define E_FQDN		12
#define E_GLIMIT	13
#define E_SLIMIT	14
#define E_CFGFILE	15
#define E_CFGPAR	16
#define E_ACCDIR	17
#define E_ABSPATH	18
#define E_INVHOST	19
#define E_ZEROREQ	20
#define E_ACCSTORD	21
#define E_OFILE		22
#define E_RFILE		23
#define E_CFGSOK	24
#define E_LISSOK	25
#define E_RLINE		26
#define E_BOOTFAIL	27
#define E_ACCEPT	28
#define E_PORT		29
#define E_ALLOC		30

# define YES    1
# define NO     0 
*/
#define LBONE_SUCCESS		1
#define LBONE_FAILURE		2
#define SUCCESS     LBONE_SUCCESS
#define FAILURE     LBONE_FAILURE

extern void lbone_error (int nerr, char *err_message);
