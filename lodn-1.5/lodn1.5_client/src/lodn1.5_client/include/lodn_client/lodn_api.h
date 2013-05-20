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

#ifndef LODN_API_H_
#define LODN_API_H_

#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>



#define ulong_t	unsigned long
#include "lors/lors_api.h"


#include "jrb.h"
#include "google/protobuf-c/protobuf-c.h"
//#include "client.pb-c.h"
//#include "datatypes.pb-c.h"

#include "lodn_types.h"







/***---Prototypes---***/

int lodn_openSession(char *url, lodn_session_handle_t *handle, int options);

int lodn_closeSession(lodn_session_handle_t handle);
int lodn_freeSession(lodn_session_handle_t handle);

int lodn_newRequest(lodn_request_t *request);
int lodn_freeRequest(lodn_request_t request);

int lodn_test(lodn_request_t request, int *done, int *result);
int lodn_wait(lodn_request_t request, int *result);


int lodn_mkdir(char *path, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options);
int lodn_rmdir(char *path, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options);
int lodn_createFile(char *path, lodn_session_handle_t handle,
					lodn_request_t request, int timeout, int options);
int lodn_chmod(const char *path, mode_t mode, lodn_session_handle_t handle,
			   lodn_request_t request, int timeout, int options);
int lodn_unlink(char *path, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options);
int lodn_stat(char *path, struct stat *statbuf, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options);
int lodn_getdir(char *path, lodn_dirent_t ***dirents, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options);
int lodn_rename(char *origpath, char *newpath, lodn_session_handle_t handle,
		        lodn_request_t request, int timeout, int options);
int lodn_getxattr(char *path, const char *name, void *value, size_t *size,
					u_int32_t position, lodn_session_handle_t handle,
					lodn_request_t request, int timeout, int options);
int lodn_removexattr(const char *path, const char *name,
					 lodn_session_handle_t handle, lodn_request_t request,
					 int timeout, int options);
int lodn_listxattr(const char *path, char *namebuf, uint32_t *size,
			       lodn_session_handle_t handle, lodn_request_t request,
			       int timeout, int options);
int lodn_setxattr(const char *path, const char *name, void *value, size_t size,
					u_int32_t position, lodn_session_handle_t handle,
					lodn_request_t request, int timeout, int options);
int lodn_insertMappings(char *path, LorsSet *lorsSet,
						lodn_session_handle_t handle,lodn_request_t request,
						int timeout, int options);
int lodn_getMappings(char *path, LorsSet **lorsSet,
					 unsigned int offset, unsigned int num_mappings,
					 lodn_session_handle_t handle,lodn_request_t request,
					 int timeout, int options);
int lodn_importExnodeFile(char *path, char *exnodefile,
						  lodn_session_handle_t handle, int options);
int lodn_exportExnodeFile(char *path, char *exnodefile,
							lodn_session_handle_t handle, int options);


int lodn_getstatus(char *hostname, int port, lodn_status_t status);


#endif /* LODN_API_H_ */
