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

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "lodn_client/lodn_errno.h"


/***---Local globals---***/
static pthread_once_t _errno_once = PTHREAD_ONCE_INIT;
static pthread_key_t  _errno_key;

/* Error strings */
static char *_error_strs[] =
				{
					"LoDN Success",						// LODN_OK			0
					"Undefined error has occurred",		// LODN_EGENERIC	-1
					"Unimplemented call",				// LODN_EIMPLEMENTED -2
					"Memory error within LoDN call",	// LODN_EMEM		-3
					"Invalid LoDN URL", 				// LODN_EINVALIDURL	-4
					"Invalid LoDN hostname",			// LODN_EINVALIDHOST -5
					"Invalid LoDN port",				// LODN_EINVALIDPORT -6
					"Invalid LoDN path",				// LODN_EINVALIDPATH -7
					"Invalid LoDN parameter",			// LODN_EINVALIDPARAM -8
					"Uninitialized element", 			// LODN_EUNINITIALIZED -9
					"Error on socket",					// LODN_ESOCKET	-10
					"Error connecting to host",			// LODN_ECONNECT	-11
					"Unknown host",						// LODN_EUNKNOWNHOST -12
					"Internal protobuffer error",		// LODN_EPROTOBUF	-13
					"SSL error",						// LODN_ESSL		-14
					"Unknown message type", 			// LODN_EUNKNOWNMSG	-15
					"Badly formed message", 			// LODN_EBADMSG -16
					"Session is closed", 				// LODN_ESESSIONCLOSED -17
					"Timeout occurred",					// LODN_ETIMEOUT		-18
					"Filename component is too long",	// LODN_ENAMETOOLONG	-19
					"Bad IBP capability",				// LODN_EIBPCAP		-20
					"Bad Mapping",						// LODN_EMAPPING	-21
					"Bad Stat", 						// LODN_ESTAT		-22
				};

static char *_fs_error_strs[] =
			{
					"General filesystem error", 	// LODN_FS_DATABASE_ERROR -1000
					"Path is invalid.",				// LODN_FS_EINVALID -1001
					"Path exceeds directory limit or path component exceeds length limit", // LODN_FS_ENAMETOOLONG, -1002
					"A component of the path does not exist.", 			 // LODN_FS_ENOENT -1003
					"A component of the path prefix is not a directory", //LODN_FS_ENOTDIR -1004
					"File already exists.", 		// LODN_FS_EEXIST -1005
					"File is a directory", 			// LODN_FS_EISDIR  -1006
					"Directory has children", 		// LODN_FS_ENOTEMPTY -1007
					"Invalid resource or depot",	// LODN_FS_ERESOURCE -1008
					"Invalid mapping",				// LODN_FS_EMAPPING -1009
					"No such attribute",			// LODN_FS_ENOATTR  -1010
			};


/****-------------------------_lodn_errno_init()---------------------------****
 * Description: This function is used to initialize the lodn errno stuff.
 ****----------------------------------------------------------------------****/
static void _lodn_errno_init(void)
{
	/* Create a pthread safe key for lodn errno */
	pthread_key_create(&_errno_key, NULL);
}


/****---------------------------get_lodn_errno(void)-----------------------****
 * Description: This function returns a pointer to the global errno for the
 *              call thread.  It creates an errno for the therad if it doesn't
 *              already exist.
 * Output: It returns an int pointer to the errno.
 ****----------------------------------------------------------------------****/
int *get_lodn_errno(void)
{
	/* Declarations */
	int *errno_ptr;


	/* Initializes the errno if it needs it */
	pthread_once(&_errno_once, _lodn_errno_init);

	/* Gets the errno ptr */
	errno_ptr = (int*)pthread_getspecific(_errno_key);

	/* Errno is not initialized for this thread so allocate it and
	 * associate it with the errno key thread */
	if(errno_ptr == NULL)
	{
		errno_ptr = (int*)calloc(1, sizeof(int));

		pthread_setspecific(_errno_key, errno_ptr);
	}

	/* Returns a pointer to the errno for this thread */
	return errno_ptr;
}


/****---------------------------lodn_strerror()----------------------------****
 * Description: This function returns a cstring that describes the error
 *              provided.
 * Input: errnum - int that holds the error value.
 * Output: It returns a cstring.
 ****----------------------------------------------------------------------****/
char *lodn_strerror(int errnum)
{
	/* Negate the error number */
	errnum = -errnum;

	/* Checks the errnum is within range */

	/* General error */
	if(errnum >= 0 && errnum < sizeof(_error_strs)/sizeof(char*))
	{
		return _error_strs[errnum];
	}

	errnum -= 1000;

	/* LoDN FS related error */
	if(errnum >= 0 && errnum < sizeof(_fs_error_strs)/sizeof(char*))
	{
		return _fs_error_strs[errnum];
	}

	/* Returns unknown error string */
	return "Unknown error";
}


