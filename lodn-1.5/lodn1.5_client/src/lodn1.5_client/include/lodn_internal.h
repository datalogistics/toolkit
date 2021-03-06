/*
 * lodn_internal.h
 *
 *  Created on: Jun 30, 2009
 *      Author: sellers
 */

#ifndef LODN_INTERNAL_H_
#define LODN_INTERNAL_H_

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "jrb.h"
#include "jval.h"

/* OpenSSL headers */
#ifdef USE_SSL
	#include "openssl/bio.h"
	#include "openssl/ssl.h"
	#include "openssl/err.h"
#endif



struct _lodn_session_handle_t
{
	char *hostname;						/* Hostname of lodn server */
	int  port;							/* Port of lodn server */

	int  sfd;							/* Socket */

#ifdef USE_SSL
	BIO *bio;
#endif

	int sessionID;						/* ID of the session */
	int sessionClosed;                  /* State of the session */
	int currMsgID;						/* Current message ID to use */

	JRB ops;							/* List of outstanding operations,
	                                     * key is msg id and value is
	                                     * lodn_request_t */

	pthread_mutex_t recvLock;
	pthread_mutex_t sendLock;
	JRB				sleepers;
	int             numReaders;

#define DEFAULT_BUFSIZE	1024

	unsigned char  *readBuffer;
	size_t			readBufferSize;
	size_t          amt;

};


struct _lodn_request_t
{
	struct _lodn_session_handle_t *handle;
	int msgID;
	int done;

	int (*callback)(struct _lodn_request_t *, int, ProtobufCMessage*, int*);
	int type;
	ProtobufCMessage *message;

	pthread_cond_t signal;

	int timeout;

	void *arg;

};



struct _lodn_status_t
{
	unsigned int major;
	unsigned int minor;
	unsigned int subminor;

};



#endif /* LODN_INTERNAL_H_ */
