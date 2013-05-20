/*
 *         IBP version 1.0.3:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *        Authors: X. Li, A. Bassi, J. Plank, M. Beck
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
 * ibp_unixlib.h
 */

# ifndef _IBPUNIXLIB_H
# define _IBPUNIXLIB_H

# ifdef HAVE_CONFIG_H
# include "config-ibp.h"
# endif
# include "ibp_server.h"

# define IBP_FULLINFO 1
# define IBP_PARTINFO 2


int SaveInfo2LSS(RESOURCE *rs, IBP_byteArray ps_container, int pi_mode);
int check_unmanaged_capability(RESOURCE *rs);




# endif
