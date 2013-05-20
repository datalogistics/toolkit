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

#ifndef PROTOCOL_HEADER_H_
#define PROTOCOL_HEADER_H_


#define PROTOCOL_HEADER_SIZE 	sizeof(MessageHeader)


#define SESSION_OPEN_MSG_TYPE 				0
#define SESSION_CLOSED_MSG_TYPE 			1
#define SESSION_RESPONSE_MSG_TYPE 			2
#define SESSION_LODNINFO_REQUEST_MSG_TYPE 	3
#define SESSION_LODNINFO_MSG_TYPE 			4
#define MKDIR_MSG							5
#define RMDIR_REQUEST						6
#define CREATEFILE_REQUEST					7
#define GETDIR_REQUEST 						8
#define GETDIR_REPLY 						9
#define ADDMAPPINGS_REQUEST					10
#define GETMAPPINGS_REQUEST					11
#define GETMAPPINGS_REPLY					12
#define GETSTAT_REQUEST						13
#define GETSTAT_REPLY 						14
#define UNLINK_REQUEST						15
#define SETXATTR_REQUEST					16
#define GETXATTR_REQUEST					17
#define GETXATTR_REPLY 						18
#define REMOVEXATTR_REQUEST 			    19
#define LISTXATTR_REQUEST					20
#define LISTXATTR_REPLY						21



/* Struct for the Message header */
typedef struct
{
	u_int32_t type;		/* Type of the message */
	u_int32_t size;		/* Size of the message */

} MessageHeader;



/***---Prototype---***/
int lodn_MessageHeader_deserialize(unsigned char *data, MessageHeader *header);
int lodn_MessageHeader_serialize(MessageHeader *header, unsigned char *data);


#endif /* PROTOCOL_HEADER_H_ */
