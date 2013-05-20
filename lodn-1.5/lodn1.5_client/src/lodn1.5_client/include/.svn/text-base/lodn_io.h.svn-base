/*
 * lodn_io.h
 *
 *  Created on: Jun 24, 2009
 *      Author: Christopher Brumgard
 */

#ifndef LODN_IO_H_
#define LODN_IO_H_


int _lodn_recvMsg(lodn_session_handle_t handle, unsigned int *type,
				    ProtobufCMessage** message, struct timeval *timeout);
int _lodn_sendMsg(lodn_session_handle_t handle, unsigned int type,
					const ProtobufCMessage* message);

#endif /* LODN_IO_H_ */
