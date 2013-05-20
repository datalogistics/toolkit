/*
 *   Copyright (C) 2009 Christopher Brumgard
 *
 *   This file is part of lodn1.5_client.
 *
 *   lodn_client is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   lodn1.5_client is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with lodn1.5_client.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LODN_ERRNO_H_
#define LODN_ERRNO_H_


#define LODN_OK 	 	     0
#define LODN_EGENERIC	    -1
#define LODN_EUNIMPLEMENTED	-2
#define LODN_EMEM		    -3
#define LODN_EINVALIDURL    -4
#define LODN_EINVALIDHOST   -5
#define LODN_EINVALIDPORT   -6
#define LODN_EINVALIDPATH   -7
#define LODN_EINVALIDPARAM  -8
#define LODN_EUNINITIALIZED -9
#define LODN_ESOCKET		-10
#define LODN_ECONNECT		-11
#define LODN_EUNKNOWNHOST	-12
#define LODN_EPROTOBUF		-13
#define LODN_ESSL			-14
#define LODN_EUNKNOWNMSG	-15
#define LODN_EBADMSG 		-16
#define LODN_ESESSIONCLOSED -17
#define LODN_ETIMEOUT		-18
#define LODN_ENAMETOOLONG	-19
#define LODN_EIBPCAP		-20
#define LODN_EMAPPING		-21
#define LODN_ESTAT			-22


#define LODN_FS_DATABASE_ERROR  -1000
#define LODN_FS_EINVALID 		-1001
#define LODN_FS_ENAMETOOLONG 	-1002
#define LODN_FS_ENOENT 			-1003
#define LODN_FS_ENOTDIR 		-1004
#define LODN_FS_EEXIST 			-1005
#define LODN_FS_EISDIR  		-1006
#define LODN_FS_ENOTEMPTY 		-1007
#define LODN_FS_ERESOURCE 		-1008
#define LODN_FS_EMAPPING        -1009
#define LODN_FS_ENOATTR         -1010





/* Macro for accessing the lodn errno */
#define lodn_errno	*get_lodn_errno()



/***---prototypes---***/
int *get_lodn_errno(void);
char *lodn_strerror(int errnum);

#endif /* LODN_ERRNO_H_ */
