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
#include <stdio.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>

#include <errno.h>

/* OpenSSL headers */
#ifdef USE_SSL
	#include "openssl/bio.h"
	#include "openssl/ssl.h"
	#include "openssl/err.h"
#endif


#include "config.h"
#include "client.pb-c.h"
#include "protocol_envelope.h"
#include "lodn_internal.h"
#include "lodn_client/lodn_options.h"
#include "lodn_client/lodn_errno.h"
#include "lodn_client/lodn_api.h"
#include "lodn_io.h"
#include "lodn.h"


static pthread_once_t _ssl_once = PTHREAD_ONCE_INIT;


#ifdef USE_SSL

static void _ssl_init(void)
{
	/* Initializing OpenSSL */
    SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
}


static int _handle_encryption(int sfd, BIO **bio, SSL_METHOD *method)
{
	/* Declarations */
	BIO *io = NULL;


    printf("%s:%d\n", __FUNCTION__, __LINE__);

	/* SSL Stuff */
	if(pthread_once(&_ssl_once, _ssl_init) != 0)
	{
		lodn_errno = LODN_ESSL;
		return -1;
	}
    
    fcntl(sfd, F_SETFL, 0);

	/* Allocates bio */
	if((io = BIO_new_socket(sfd, BIO_NOCLOSE)) == NULL)
	{
		lodn_errno = LODN_ESSL;
		return -1;
	}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
	/* Encryption */
	if(method != NULL)
	{
		SSL_CTX *ctx 	 = NULL;
		SSL     *ssl 	 = NULL;
		BIO     *ssl_bio = NULL;

    printf("%s:%d\n", __FUNCTION__, __LINE__);
		/* Flush the socket before doing the SSL negiotation */
		BIO_flush(io);

		/* Select the SSL method to use */
		//sslMethod = SSLv3_client_method();
		//method = TLSv1_client_method();

    printf("%s:%d\n", __FUNCTION__, __LINE__);
		/* Create a SSL context */
		if((ctx = SSL_CTX_new(method)) == NULL)
		{
    printf("%s:%d error is %s\n", __FUNCTION__, __LINE__, ERR_reason_error_string(ERR_get_error()));
			lodn_errno = LODN_ESSL;
			return -1;
		}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
		/* Test code */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

		/* Create the SSL structure with the given SSL context */
		if((ssl = SSL_new(ctx)) == NULL)
		{
			lodn_errno = LODN_ESSL;
			return -1;
		}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
		/* Retry negiogate option */
		SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    printf("%s:%d\n", __FUNCTION__, __LINE__);

		/* Set the underlying bio with the socket */
		SSL_set_bio(ssl, io, io);
        int retval;
    printf("%s:%d\n", __FUNCTION__, __LINE__);
		/* Make SSL connection */
		if((retval = SSL_connect(ssl)) <= 0)
		{
    printf("%s:%d error is %d\n", __FUNCTION__, __LINE__, SSL_get_error(ssl, retval));
switch(SSL_get_error(ssl, retval))
    {
    case SSL_ERROR_NONE:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_SSL:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_WANT_READ:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_WANT_WRITE:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_WANT_CONNECT:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_WANT_X509_LOOKUP:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_SYSCALL:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;

    case SSL_ERROR_ZERO_RETURN:
        printf("%s:%d\n", __FUNCTION__, __LINE__);
        break;
    }

    
			lodn_errno = LODN_ESSL;
			return -1;
		}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
		/* Create a BIO for SSL */
		if((ssl_bio = BIO_new(BIO_f_ssl())) == NULL)
		{
			lodn_errno = LODN_ESSL;
			return -1;
		}

		BIO_set_ssl(ssl_bio, ssl, BIO_CLOSE);

		//BIO_push(ssl_handle->io, ssl_handle->ssl_bio);

		/* Set the output bio to the ssl BIO */
		*bio = ssl_bio;

	/* Plain text */
	}else
	{
		/* Regular BIO */
		*bio = io;
	}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
	/* Return success */
	return 0;
}


#endif



int lodn_openSession(char *url, lodn_session_handle_t *handle, int options)
{
	/* Declarations */
	char *host    = NULL;
	char *path    = NULL;
	JRB   parmeters = NULL;
	JRB   node;
    int   port;
    int   sfd     = -1;
    unsigned int type;
    Lodn__SessionOpenMsg sessionRequestMsg = LODN__SESSION_OPEN_MSG__INIT;
    Lodn__SessionClosedMsg closeMsg =  LODN__SESSION_CLOSED_MSG__INIT;
    ProtobufCMessage* responseMsg = NULL;

#ifdef USE_SSL

    Lodn__SessionOpenMsg__EncryptionMethod *encryption_methods = NULL;
    SSL_METHOD *method = NULL;
    BIO *bio = NULL;
#endif

	/* Checks the input parameters */
	if(url == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

    /* Initialize the errno */
	lodn_errno = LODN_OK;

	/* Initialize the handler pointer */
	*handle = NULL;


	/* Scan the url and check validity */
	if(lodn_parse_url(url, &host, &port, &path, &parmeters) != 0)
	{
		/* Errno is already set in function */
		goto ERROR_HANDLER;
	}

	/* Creates a socket connection */
	if((sfd =  _lodn_create_connection(host, port)) < 0)
	{
		/* Errno is already set in function */
		goto ERROR_HANDLER;
	}

#ifdef USE_SSL

	/* Handles encryption */
	if(_handle_encryption(sfd, &bio, NULL) != 0)
	{
		goto ERROR_HANDLER;
		return -1;
	}

#endif


	/* Allocates a session handle */
	if((*handle = malloc(sizeof(struct _lodn_session_handle_t))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Assigns the values to handle */
	(*handle)->hostname    	= host;
	(*handle)->port        	= port;
	(*handle)->sfd 	    	= sfd;
	(*handle)->sessionID	= 0;
	(*handle)->currMsgID	= 0;
	(*handle)->numReaders   = 0;
	(*handle)->sessionClosed  = 1;
	(*handle)->readBuffer  = NULL;

	/* Allocates a jrb to hold the ops and the sleepers */
	if(((*handle)->ops 		=  make_jrb()) == NULL ||
	   ((*handle)->sleepers = make_jrb()) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Allocate a read buffer */
	if(((*handle)->readBuffer = malloc(DEFAULT_BUFSIZE*sizeof(unsigned char))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Sets the size of the buffer and initializes amt of data in the buffer */
	(*handle)->readBufferSize = DEFAULT_BUFSIZE *sizeof(unsigned char);
	(*handle)->amt            = 0;


	/* Initialize the mutex lock */
	if(pthread_mutex_init(&((*handle)->recvLock), NULL) != 0  ||
	   pthread_mutex_init(&((*handle)->sendLock), NULL))
	{
		lodn_errno =LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

#ifdef USE_SSL
	(*handle)->bio          = bio;
#endif

	/* Session exchange handling */

	/* Sets the message id */
	sessionRequestMsg.msgid = (*handle)->currMsgID;
	sessionRequestMsg.has_msgid = 1;
	(*handle)->currMsgID++;

	/* Sets the preferred encryption methods */
	if(options & LODN_ENCRYPT)
	{
		/* Allocates a list of encryption methods */
		if((sessionRequestMsg.encryptionmethod = malloc(
					2*sizeof(Lodn__SessionOpenMsg__EncryptionMethod))) == NULL)
		{
			lodn_errno = LODN_EMEM;
			goto ERROR_HANDLER;
		}

		/* Sets the encryption method */
		sessionRequestMsg.encryptionmethod[0] =
							LODN__SESSION_OPEN_MSG__ENCRYPTION_METHOD__TLSv1;
		sessionRequestMsg.encryptionmethod[1] =
							LODN__SESSION_OPEN_MSG__ENCRYPTION_METHOD__SSLv3;

		/* Sets the number of encryption methods */
		sessionRequestMsg.n_encryptionmethod = 2;
	}

	/* Sends the message */
	if(_lodn_sendMsg(*handle, SESSION_OPEN_MSG_TYPE,
				(ProtobufCMessage *)&sessionRequestMsg) != 0)
	{
		/* Errno already set in function */
		goto ERROR_HANDLER;
	}

	/* Receives the return message */
	if(_lodn_recvMsg(*handle, &type, &responseMsg, NULL) != 0)
	{
		/* Errno already set in function */
		goto ERROR_HANDLER;
	}


    printf("%s:%d\n", __FUNCTION__, __LINE__);
	/* Handles the message */
	switch(type)
	{
		/* Session open */
		case SESSION_OPEN_MSG_TYPE:

			/* Sets the session ID */
			(*handle)->sessionID   	= ((Lodn__SessionOpenMsg*)
													responseMsg)->sessionid;
    printf("%s:%d\n", __FUNCTION__, __LINE__);

#ifdef USE_SSL

			/* Encryption requested */
			if(options & LODN_ENCRYPT)
			{
    printf("%s:%d\n", __FUNCTION__, __LINE__);
				/* Server doesn't support any forms of encryption */
				if(((Lodn__SessionOpenMsg*)responseMsg)->n_encryptionmethod == 0)
				{
    printf("%s:%d\n", __FUNCTION__, __LINE__);
					lodn_errno = LODN_ESSL;
					goto ERROR_HANDLER;
				}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
				/* Sets the encryption method */
				switch(((Lodn__SessionOpenMsg*)responseMsg)->encryptionmethod[0])
				{
					/* Use TLSv1 */
					case LODN__SESSION_OPEN_MSG__ENCRYPTION_METHOD__TLSv1:
    printf("%s:%d TLSv1\n", __FUNCTION__, __LINE__);
						method = TLSv1_client_method();
						break;

					/* Use SSLv3 */
					case LODN__SESSION_OPEN_MSG__ENCRYPTION_METHOD__SSLv3:
    printf("%s:%d\n", __FUNCTION__, __LINE__);
						method = SSLv3_client_method();
						break;

					/* Use Plain text */
					case LODN__SESSION_OPEN_MSG__ENCRYPTION_METHOD__PLAIN:
    printf("%s:%d\n", __FUNCTION__, __LINE__);
						method = NULL;
						break;

					/* Unknown method */
					default:
    printf("%s:%d\n", __FUNCTION__, __LINE__);
						lodn_errno = LODN_ESSL;
						goto ERROR_HANDLER;
				}
			}
TEST:
    printf("%s:%d\n", __FUNCTION__, __LINE__);

			/* Frees the bio */
			BIO_free_all(bio);
			bio = NULL;

			/* Handles encryption */
			if(_handle_encryption(sfd, &bio, method) != 0)
			{
				goto ERROR_HANDLER;
				return -1;
			}

    printf("%s:%d\n", __FUNCTION__, __LINE__);
			(*handle)->bio          = bio;
#endif

			/* Session is not closed */
			(*handle)->sessionClosed = 0;

			break;

		/* Error has occurred */
		case SESSION_RESPONSE_MSG_TYPE:
    printf("%s:%d\n", __FUNCTION__, __LINE__);

			/* Close the channel */

			/* Sets the message id */
			closeMsg.msgid = (*handle)->currMsgID;
			(*handle)->currMsgID++;

			/* Sends the closed message */
			if(_lodn_sendMsg(*handle, SESSION_CLOSED_MSG_TYPE,
						(ProtobufCMessage *)&closeMsg) != 0)
			{
				/* Errno already set in function */
				goto ERROR_HANDLER;
			}

			/* Set the errno value from the message */
			lodn_errno = ((Lodn__ResponseMsg*)responseMsg)->errnum;

			goto ERROR_HANDLER;

		/* Some other message */
		default:

			/* Sets the errno value */
			lodn_errno = LODN_EUNKNOWNMSG;

			/* Close the channel */

			/* Sets the message id */
			closeMsg.msgid = (*handle)->currMsgID;
			(*handle)->currMsgID++;

			/* Sends the closed message */
			if(_lodn_sendMsg(*handle, SESSION_CLOSED_MSG_TYPE,
						(ProtobufCMessage *)&closeMsg) != 0)
			{
				/* Errno already set in function */
				goto ERROR_HANDLER;
			}

			goto ERROR_HANDLER;
	}


	/* Frees temporary resources */
	protobuf_c_message_free_unpacked(responseMsg, NULL);
	free(path);


	/* Return success */
	return 0;


	/* Handles errors */
	ERROR_HANDLER:

		/* Free resources */
		if(host != NULL)
		{
			free(host);

			if(path != NULL)
			{
				free(path);

				if(parmeters != NULL)
				{
					jrb_traverse(node, parmeters)
					{
						free(node->key.v);
						free(node->val.v);
					}

					jrb_free_tree(parmeters);
				}

				if(sfd != -1)
				{
					close(sfd);

					if(*handle != NULL)
					{
						if((*handle)->ops != NULL)
						{
							jrb_free_tree((*handle)->ops);
						}

						if((*handle)->sleepers != NULL)
						{
							jrb_free_tree((*handle)->sleepers);
						}

						if((*handle)->readBuffer != NULL)
						{
							free((*handle)->readBuffer);
						}

						free(*handle);

						/* Mark it as NULL */
						*handle = NULL;
					}

#ifdef USE_SSL

					if(bio != NULL)
					{
						BIO_free_all(bio);
					}

					if(sessionRequestMsg.encryptionmethod != NULL)
					{
						free(sessionRequestMsg.encryptionmethod);
					}
#endif

					if(responseMsg != NULL)
					{
						protobuf_c_message_free_unpacked(responseMsg, NULL);
					}
				}
			}
		}

		/* Return failure */
		return -1;
}


int lodn_closeSession(lodn_session_handle_t handle)
{
	/* Declarations */
	Lodn__SessionClosedMsg sessionCloseMsg = LODN__SESSION_CLOSED_MSG__INIT;


	/* Checks the input parameters */
	if(handle == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes the lodn_errno */
	lodn_errno = LODN_OK;


	/* Sets the message id */
	sessionCloseMsg.msgid = handle->currMsgID;
	sessionCloseMsg.has_msgid = 1;
	handle->currMsgID++;

	/* Sends the message */
	if(_lodn_sendMsg(handle, SESSION_CLOSED_MSG_TYPE,
					(ProtobufCMessage *)&sessionCloseMsg) != 0)
	{
		/* Errno already set in function */
		goto ERROR_HANDLER;
	}

	/* Marks the session as being closed */
	handle->sessionClosed = 1;

	/* Closes connection */
	close(handle->sfd);

	/* Return Success */
	return 0;


	/* Error Handler */
	ERROR_HANDLER:

		/* Free the resources */
		close(handle->sfd);

		/* Return failure */
		return -1;
}


int lodn_freeSession(lodn_session_handle_t handle)
{
	/* Checks the input parameters */
	if(handle == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes the lodn_errno */
	lodn_errno = LODN_OK;

#ifdef USE_SSL
		BIO_free_all(handle->bio);
#endif

	/* Frees the lock */
	pthread_mutex_destroy(&(handle->recvLock));
	pthread_mutex_destroy(&(handle->sendLock));

	/* Free allocated resources */
	free(handle->hostname);
	jrb_free_tree(handle->ops);
	jrb_free_tree(handle->sleepers);

	if(handle->readBuffer != NULL)
	{
		free(handle->readBuffer);
	}

	free(handle);

	/* Returns successfully */
	return 0;
}


int lodn_newRequest(lodn_request_t *request)
{
	/* Allocate the lodn_request_t struct */
	if((*request = (lodn_request_t)malloc(sizeof(struct _lodn_request_t))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		return -1;
	}

	/* Initialize the signal variable */
	if(pthread_cond_init(&((*request)->signal), NULL) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

	/* Initialize the internal variables of the request object */
	(*request)->handle 	 = NULL;
	(*request)->msgID 	 = -1;
	(*request)->done   	 = -1;
	(*request)->callback = NULL;
	(*request)->message  = NULL;
	(*request)->timeout  = -1;
	(*request)->arg      = NULL;

	/* Return successfully */
	return 0;

	/* Error handler */
	ERROR_HANDLER:

		/* Free the request struct and reset the pointer */
		if(*request != NULL)
		{
			free(*request);
			*request = NULL;
		}

		/* Return error */
		return -1;
}


int lodn_freeRequest(lodn_request_t request)
{

	/* Frees the conditional variable */
	if(pthread_cond_destroy(&(request->signal)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		return -1;
	}

	/* Resets the request struct memory */
	request->handle   = NULL;
	request->msgID 	  = -1;
	request->done     = -1;
	request->callback = NULL;
	request->message  = NULL;
	request->timeout  = -1;

	/* Frees the request */
	free(request);


	/* Return successfully */
	return 0;
}


int lodn_test(lodn_request_t request, int *done, int *result)
{
	/* Declarations */

	lodn_errno = LODN_EUNIMPLEMENTED;


	/* Lock the mutex */

	/* Check to see if I'm done */

	/* If I'm not done, then I need to do a quick read, which will require a
	 * non blocking socket call to see if I or any other messages have arrived
	 * yet. */

	/* I have arrived, Yeah success */

	/* Another message has arrived, must handle it and try again */

	/* Unlock the mutex */

	return -1;
}


static int _ptr_cmp(Jval val1, Jval val2)
{
    /* First key is less */
	if(val1.v < val2.v)
	{
		return -1;

	/* First key is greater */
	}else if(val1.v > val2.v)
	{
		return 1;
	}

	/* Keys are the same */
	return 0;
}


static int _mkdir_callback(lodn_request_t request, int type,
							ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;


	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}


static int _rmdir_callback(lodn_request_t request, int type,
						   ProtobufCMessage *message, int *result)

{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;

	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}


static int _createFile_callback(lodn_request_t request, int type,
		                        ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;


	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}


static int _getDir_callback(lodn_request_t request, int type,
		                    ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;
	Lodn__GetDirReply *getDirReply;
	size_t i;
	lodn_dirent_t **dirents;


	/* Process the reply message according to type */
	switch(type)
	{
		/* Normal reponse msg -- inidicates a error */
		case SESSION_RESPONSE_MSG_TYPE:

			/* Get a pointer for the type of message */
			responseMsg = (Lodn__ResponseMsg *)message;

			/* Sets the errno value according the return message value */
			lodn_errno = responseMsg->errnum;

			/* Sets the result value */
			*result = (responseMsg->errnum != 0) ? -1 : 0;

			break;

		/* Get directory reply */
		case GETDIR_REPLY:

			/* Gets a getDirReply pointer to the message */
			getDirReply = (Lodn__GetDirReply*)message;

			/* Allocates an array to hold all of the dirents and NULL terminates
			 * the end */
			if((dirents = (lodn_dirent_t **)calloc((getDirReply->n_dirents+1),
										sizeof(lodn_dirent_t *))) == NULL)
			{
				lodn_errno = LODN_EMEM;
				goto ERROR_HANDLER;
			}

			/* Copies the getdir msg to an array of lodn_dirent_t's structs */
			for(i=0; i<getDirReply->n_dirents; i++)
			{
				/* Allocate a lodn_dirent_t */
				if((dirents[i] = (lodn_dirent_t*)
									calloc(1, sizeof(lodn_dirent_t))) == NULL)
				{
					lodn_errno = LODN_EMEM;
					goto ERROR_HANDLER;
				}

				/* Get the length of the name */
				dirents[i]->d_namlen = strlen((getDirReply->dirents[i])->name);

				/* Checks the length of the name */
				if(dirents[i]->d_namlen > LODN_NAME_MAX)
				{
					lodn_errno = LODN_ENAMETOOLONG;
					goto ERROR_HANDLER;
				}

				/* Allocates a copy of the name for dirent struct */
				if((dirents[i]->d_name = strdup(
									(getDirReply->dirents[i])->name)) == NULL)
				{
					lodn_errno = LODN_EMEM;
					goto ERROR_HANDLER;
				}

				dirents[i]->d_type = (getDirReply->dirents[i])->type;
				dirents[i]->d_ino  = (getDirReply->dirents[i])->inode;
			}

			/* Sets the errno for no errors */
			lodn_errno = LODN_OK;

			/* Set the result for being good */
			*result = 0;

			/* Stores the dirents pointer */
			*((lodn_dirent_t***)request->arg) = dirents;

			break;

		/* Bad message type */
		default:
			lodn_errno = LODN_EBADMSG;
			*result    = -1;
	}


	/* Return success */
	return 0;


	/* Handles errors that occur in the function */
	ERROR_HANDLER:

		/* Frees allocated resources */
		if(dirents != NULL)
		{
			for(i=0; i<getDirReply->n_dirents; i++)
			{
				if(dirents[i] != NULL)
				{
					if(dirents[i]->d_name != NULL)
					{
						free(dirents[i]->d_name);
					}

					free(dirents[i]);
				}
			}

			free(dirents);
		}

		return -1;
}


int lodn_wait(lodn_request_t request, int *result)
{
	/* Declarations */
	ProtobufCMessage *message = NULL;
	unsigned int type;
	JRB node = NULL;
	lodn_request_t   nextRequest;
	int msgID;
	int isReader = 0;
	struct timespec abstime;
	struct timeval  currtime;
	struct timeval  timeout;
	int    retval;


	/* Check the arguments */
	if(request == NULL || result == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the errno */
	lodn_errno = LODN_OK;

	/* Converts the requested timeout into absolute time */
	if(request->timeout >= 0)
	{
		/* Gets the current time of day */
		if(gettimeofday(&currtime, NULL) != 0)
		{
			lodn_errno = LODN_EGENERIC;
			return -1;
		}

		timeout.tv_sec  = request->timeout + currtime.tv_sec;
		timeout.tv_usec = currtime.tv_usec;
	}

	/* Lock the handle */
	if(pthread_mutex_lock(&(request->handle->recvLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		return -1;
	}

	/* I'm not done yet so keep waiting */
	while(request->done == 0)
	{
		/* Checks if the session is reported as being closed */
		if(request->handle->sessionClosed)
		{
			lodn_errno = LODN_ESESSIONCLOSED;
			pthread_mutex_unlock(&(request->handle->recvLock));
			return -1;
		}


		/* There is already a reader so I'm going to let him to do the reading
		 * until I'm signaled otherwise */
		if(request->handle->numReaders > 0)
		{
			/* Put yourself on list of sleepers */
			if((node = jrb_insert_gen(request->handle->sleepers,
								new_jval_v(request), JNULL, _ptr_cmp)) == NULL)
			{
				lodn_errno = LODN_EMEM;
				pthread_mutex_unlock(&(request->handle->recvLock));
				return -1;
			}

			fprintf(stderr, "At %s:%d node is %x\n", __FILE__, __LINE__, node);

			/* Wait forever on message */
			if(request->timeout < 0)
			{
				/* Wait to be signaled */
				if(pthread_cond_wait(&(request->signal), &(request->handle->recvLock)) != 0)
				{
					lodn_errno = LODN_EGENERIC;
					pthread_mutex_unlock(&(request->handle->recvLock));
					return -1;
				}

			/* Wait for message until timeout has occured */
			}else
			{
				/* Sets up the wait time */
				abstime.tv_sec  = timeout.tv_sec;
				abstime.tv_nsec = timeout.tv_usec * 1000;

				/* Wait to be signaled or timeout occurs */
				retval = pthread_cond_timedwait(&(request->signal),
												&(request->handle->recvLock),
												&abstime);

				/* Handle pthread_cond_timedwait */
				switch(retval)
				{
					/* Signaled so everything is ok */
					case 0:
						break;

					/* Timeout */
					case ETIMEDOUT:

						lodn_errno = LODN_ETIMEOUT;
						pthread_mutex_unlock(&(request->handle->recvLock));
						return -1;

					/* Error */
					default:
						lodn_errno = LODN_EGENERIC;
						pthread_mutex_unlock(&(request->handle->recvLock));
						return -1;
				}
			}

		/* Do the message receive */
		}else
		{
			/* I'm a reader */
			isReader = 1;
			request->handle->numReaders++;


			if(pthread_mutex_unlock(&(request->handle->recvLock)) != 0)
			{
				lodn_errno = LODN_EGENERIC;
				return -1;
			}

			if(_lodn_recvMsg(request->handle, &type, &message,
						(request->timeout >= 0) ? &timeout : NULL) != 0)
			{
				/* Errno should already be set */

				/* Session is closed */
				request->handle->sessionClosed = 1;

				if(pthread_mutex_lock(&(request->handle->recvLock)) != 0)
				{
					protobuf_c_message_free_unpacked(message, NULL);
					return -1;
				}

				goto ERROR_HANDLER;
			}

			if(pthread_mutex_lock(&(request->handle->recvLock)) != 0)
			{
				lodn_errno = LODN_EGENERIC;
				protobuf_c_message_free_unpacked(message, NULL);
				return -1;
			}

			/* Handle close type by marking all messages as not done
			 * and releasing them */
			if(type == SESSION_CLOSED_MSG_TYPE)
			{
				lodn_errno = LODN_ESESSIONCLOSED;

				/* Session is closed */
				request->handle->sessionClosed = 1;

				goto ERROR_HANDLER;
			}

			/* Get the msg ID */
			switch(type)
			{
				case SESSION_OPEN_MSG_TYPE:
					msgID = ((Lodn__SessionOpenMsg*)message)->msgid;
					break;

				case SESSION_CLOSED_MSG_TYPE:
					msgID = ((Lodn__SessionClosedMsg*)message)->msgid;
					break;

				case SESSION_RESPONSE_MSG_TYPE:
					msgID = ((Lodn__ResponseMsg*)message)->msgid;
					break;

					/*
				case SESSION_LODNINFO_REQUEST_MSG_TYPE:
					msgID = ((Lodn__LoDNInfoRequestMsg*)message)->msgid;
					break;

				case SESSION_LODNINFO_MSG_TYPE:
					msgID = ((Lodn__LoDNInfoMsg*)message)->msgid;
					break;
					 */

				case MKDIR_MSG:
					msgID = ((Lodn__MkdirMsg*)message)->msgid;
					break;

				case GETDIR_REPLY:
					msgID = ((Lodn__GetDirReply*)message)->msgid;
					break;

				case GETMAPPINGS_REPLY:
					msgID = ((Lodn__GetDirReply*)message)->msgid;
					break;

				case GETSTAT_REQUEST:
					msgID = ((Lodn__GetStatRequest*)message)->msgid;
					break;

				case GETSTAT_REPLY:
					msgID = ((Lodn__GetStatReply*)message)->msgid;
					break;

				case GETXATTR_REPLY:
					msgID = ((Lodn__GetXattrReply*)message)->msgid;
					break;

				case LISTXATTR_REPLY:
					msgID = ((Lodn__ListXattrReply*)message)->msgid;
					break;

				default:
					lodn_errno = LODN_EUNKNOWNMSG;

					goto ERROR_HANDLER;
			}

			/* Message is mine */
			if(msgID == request->msgID)
			{
				/* Mark yourself done */
				request->done    = 1;
				request->message = message;
				request->type    = type;

				message = NULL;

				/* Send signal to a guy who is waiting to become the
				 * next reader */
				if(!jrb_empty(request->handle->sleepers))
				{
					/* Get the next request in the list of sleepers */
					nextRequest = ((lodn_request_t)
								(jrb_first(request->handle->sleepers)->key.v));

					/* Signal him */
					if(pthread_cond_signal(&(nextRequest->signal)) != 0)
					{
						lodn_errno = LODN_EGENERIC;
						protobuf_c_message_free_unpacked(request->message, NULL);
						goto ERROR_HANDLER;
					}
				}



			/* Message is not mine */
			}else
			{
				/* Find the request in ops */
				node = jrb_find_int(request->handle->ops, msgID);

				if(node != NULL)
				{
					/* Gets a pointer to the next request */
					nextRequest =(lodn_request_t)(node->val.v);

					/* Mark it done */
					nextRequest->done 	 = 1;
					nextRequest->message = message;
					request->type    	 = type;

					message = NULL;

					/* If its in the list of sleepers wake it up by sending it a
					 * signal */
					node = jrb_find_gen(request->handle->sleepers,
										new_jval_v(nextRequest), _ptr_cmp);

					if(node != NULL)
					{
						if(pthread_cond_signal(&(nextRequest->signal)) != 0)
						{
							lodn_errno = LODN_EGENERIC;
							protobuf_c_message_free_unpacked(nextRequest->message, NULL);
							goto ERROR_HANDLER;
						}

						/* Remove its node from the list of sleepers */
						jrb_delete_node(node);
					}
				}
			}

			/* No longer the reader */
			isReader = 0;
			request->handle->numReaders--;
		}
	}

	/* Take yourself out of ops */
	node = jrb_find_int(request->handle->ops, msgID);

	if(node != NULL)
	{
		jrb_delete_node(node);
	}


	/* Unlock the handle */
	if(pthread_mutex_unlock(&(request->handle->recvLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		protobuf_c_message_free_unpacked(request->message, NULL);
		return -1;
	}


	/* Handle message type with call back */
	request->callback(request, request->type, request->message, result);

	/* Free the request message */
	protobuf_c_message_free_unpacked(request->message, NULL);


	/* Return success */
	return 0;


	/* Error handler */
	ERROR_HANDLER:


		/* Only the one with the lock */
		if(isReader)
		{
			/* Decrement the number of readers */
			request->handle->numReaders--;

			if(message != NULL)
			{
				protobuf_c_message_free_unpacked(message, NULL);
			}

			/* Traverses the list waking everyone up */
			jrb_traverse(node, (request->handle->sleepers))
			{
				nextRequest = (lodn_request_t)(node->key.v);

				/* Signal him */
				if(pthread_cond_signal(&(nextRequest->signal)) != 0)
				{
					lodn_errno = LODN_EGENERIC;
					pthread_mutex_unlock(&(request->handle->recvLock));
					return -1;
				}
			}

			/* Unlock the handle */
			if(pthread_mutex_unlock(&(request->handle->recvLock)) != 0)
			{
				return -1;
			}
		}

		/* Return error */
		return -1;
}


int lodn_mkdir(char *path, lodn_session_handle_t handle,
				lodn_request_t userRequest, int timeout, int options)
{
	/* Declarations */
	Lodn__MkdirMsg  mkdirMsg 	= LODN__MKDIR_MSG__INIT;
	lodn_request_t  funcRequest = NULL;
	int             result      = 0;


	/* Checks args for validity */
	if(path == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* If a timeoute is given need to specify the user request */
	if(timeout >= 0 && userRequest == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the errno */
	lodn_errno = LODN_OK;

	/* Sets the message path */
	mkdirMsg.path = path;

	/* Sets the message id */
	mkdirMsg.msgid = handle->currMsgID;
	mkdirMsg.has_msgid = 1;
	handle->currMsgID++;

	/* Sets up the request struct  */

	/* Use internal request */
	if(userRequest == NULL)
	{
		if(lodn_newRequest(&funcRequest) != 0)
		{
			/* lodn_errno is set in the function */
			goto ERROR_HANDLER;
		}
	}else
	{
		funcRequest = userRequest;
	}

	/* Set the handle and the message id */
	funcRequest->handle   = handle;
	funcRequest->msgID    = mkdirMsg.msgid;
	funcRequest->done     = 0;
	funcRequest->callback = _mkdir_callback;
	funcRequest->message  = NULL;
	funcRequest->timeout  = timeout;

	/* Loads the job on the list of queue items */
	if(pthread_mutex_lock(&(funcRequest->handle->recvLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		return -1;
	}

	/* Loads the job on the list of queue items */
	if(jrb_insert_int(handle->ops, funcRequest->msgID,
						new_jval_v(funcRequest)) == NULL)
	{
		lodn_errno = LODN_EGENERIC;
		pthread_mutex_unlock(&(funcRequest->handle->recvLock));
		return -1;
	}

	/* Loads the job on the list of queue items */
	if(pthread_mutex_unlock(&(funcRequest->handle->recvLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		return -1;
	}

	/* Sends the message */
	if(_lodn_sendMsg(handle, MKDIR_MSG, (ProtobufCMessage *)&mkdirMsg) != 0)
	{
		/* Errno already set in function */
		return -1;
	}

	/* Wait on the job */
	if(userRequest == NULL)
	{
		/* Wait for completion */
		if(lodn_wait(funcRequest, &result) != 0)
		{
			/* Errno already set in function */
			return -1;
		}
	}

	/* Frees the internal request if necessary */
	if(userRequest == NULL && funcRequest != NULL)
	{
		lodn_freeRequest(funcRequest);
	}

	/* Return successfully */
	return result;


	/* Handle errors */
	ERROR_HANDLER:

		/* Frees the internal request if necessary */
		if(userRequest == NULL && funcRequest != NULL)
		{
			lodn_freeRequest(funcRequest);
		}

		/* Return failure */
		return -1;
}

static int _shared_component(lodn_session_handle_t handle,
							 ProtobufCMessage *msg,
							 unsigned int msgType,
							 lodn_request_t userRequest,
							 int timeout,
							 uint32_t msgid,
							 int (*callback)(struct _lodn_request_t *,
											 int,
											 ProtobufCMessage*,
											 int*),
							 void *arg)
{
	/* Declarations */
	lodn_request_t  funcRequest = NULL;
	int             result      = 0;


	/* Use an internal request if the user provides none */
	if(userRequest == NULL)
	{
		if(lodn_newRequest(&funcRequest) != 0)
		{
			/* lodn_errno is set in the function */
			goto ERROR_HANDLER;
		}

	/* Use the request struct given by the user */
	}else
	{
		funcRequest = userRequest;
	}

	funcRequest->handle 	= handle;
	funcRequest->msgID  	= msgid;
	funcRequest->done   	= 0;
	funcRequest->callback 	= callback;
	funcRequest->message    = NULL;
	funcRequest->timeout    = timeout;
	funcRequest->arg        = arg;


	/* Loads the job on the list of queue items */
	if(pthread_mutex_lock(&(funcRequest->handle->recvLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		return -1;
	}

	if(jrb_insert_int(handle->ops, funcRequest->msgID,
						new_jval_v(funcRequest)) == NULL)
	{
		lodn_errno = LODN_EGENERIC;
		pthread_mutex_unlock(&(funcRequest->handle->recvLock));

		goto ERROR_HANDLER;
	}

	/* Loads the job on the list of queue items */
	if(pthread_mutex_unlock(&(funcRequest->handle->recvLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		return -1;
	}

	/* Sends the message */
	if(_lodn_sendMsg(handle, msgType, msg) != 0)
	{
		/* Errno already set in function */
		goto ERROR_HANDLER;
	}


	/* Wait on the job */
	if(userRequest == NULL)
	{
		/* Wait for completion */
		if(lodn_wait(funcRequest, &result) != 0)
		{
			/* Errno already set in function */
			goto ERROR_HANDLER;
		}
	}

	/* Frees the internal request if necessary */
	if(userRequest == NULL && funcRequest != NULL)
	{
		lodn_freeRequest(funcRequest);
	}

	/* Return successfully */
	return result;


	/* Handles errors */
	ERROR_HANDLER:

		/* Frees the internal request if necessary */
		if(userRequest == NULL && funcRequest != NULL)
		{
			lodn_freeRequest(funcRequest);
		}

		/* Return failure */
		return -1;
}





int lodn_rmdir(char *path, lodn_session_handle_t handle,
				lodn_request_t userRequest, int timeout, int options)
{
	/* Declarations */
	Lodn__RmdirRequest rmdirRequest = LODN__RMDIR_REQUEST__INIT;
	int result;


	/* Checks args for validity */
	if(path == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* If a timeout is given need to specify the user request */
	if(timeout >= 0 && userRequest == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the errno */
	lodn_errno = LODN_OK;


	/* Sets the path */
	rmdirRequest.path 		= path;

	/* Sets the message id */
	rmdirRequest.msgid 		= handle->currMsgID;
	rmdirRequest.has_msgid 	= 1;
	handle->currMsgID++;

	/*  */
	result = _shared_component(handle, (ProtobufCMessage*)&rmdirRequest,
							   RMDIR_REQUEST, userRequest, timeout,
							   rmdirRequest.msgid, _rmdir_callback, NULL);

	/* Return the result.  lodn_errno will already be set */
	return result;
}



int lodn_createFile(char *path, lodn_session_handle_t handle,
					lodn_request_t request, int timeout, int options)
{
	/* Declarations */
	Lodn__CreateFileRequest	createFileRequest = LODN__CREATE_FILE_REQUEST__INIT;
	int result;


	/* Checks the args */
	if(path == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Must use a user request struct when a timeout is specified */
	if(timeout >= 0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Sets the path */
	createFileRequest.path = path;

	/* Sets the message id */
	createFileRequest.msgid  	= handle->currMsgID;
	createFileRequest.has_msgid = 1;
	handle->currMsgID++;

	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&createFileRequest,
							   CREATEFILE_REQUEST, request, timeout,
							   createFileRequest.msgid, _createFile_callback,
							   NULL);

	/* Return the result. lodn_errno will already be set */
	return result;
}


int lodn_chmod(const char *path, mode_t mode, lodn_session_handle_t handle,
			   lodn_request_t request, int timeout, int options)
{
	/* Declarations */

	lodn_errno = LODN_EUNIMPLEMENTED;

	return -1;
}



static int _unlink_callback(lodn_request_t request, int type,
							ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;


	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}



int lodn_unlink(char *path, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options)
{
	/* Declarations */
	Lodn__UnlinkRequest unlinkMessage = LODN__UNLINK_REQUEST__INIT;
	int result;


	/* Checks the arguments to the function */
	if(path == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* If timeout is specified then a request must be given */
	if(timeout >= 0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize lodn errno */
	lodn_errno = LODN_OK;


	/* Sets the path */
	unlinkMessage.path = path;

	/* Sets the message id */
	unlinkMessage.msgid  	 = handle->currMsgID;
	unlinkMessage.has_msgid = 1;
	handle->currMsgID++;


	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&unlinkMessage,
							   UNLINK_REQUEST, request, timeout,
							   unlinkMessage.msgid, _unlink_callback, NULL);




	/* Return result */
	return result;
}



static int _getStat_callback(lodn_request_t request, int type,
							 ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg  *responseMsg;
	Lodn__GetStatReply *getStatReply;
	struct stat      *statbuf;

	/* Procesesses the return message */
	switch(type)
	{
		/* Response message */
		case SESSION_RESPONSE_MSG_TYPE:

			/* Gets a pointer to the type of message */
			responseMsg = (Lodn__ResponseMsg *)message;

			/* Sets the errno value according the return message value */
			lodn_errno = responseMsg->errnum;

			/* Sets the result value */
			*result = (responseMsg->errnum != 0) ? -1 : 0;

			break;

		/* Normal successfully message */
		case GETSTAT_REPLY:

			/* Gets a pointer to the message */
			getStatReply = (Lodn__GetStatReply*)message;

			/* Gets a pointer to the stat buffer */
			statbuf = (struct stat *)(request->arg);

			/* Clears the statbuf */
			memset(statbuf, 0, sizeof(struct stat));

			/* Checks the stat from them message for required fields and
			 * ranges */
			if(getStatReply->stat == NULL ||
			   !(getStatReply->stat->has_inode) ||
			   !(getStatReply->stat->has_mode)  ||
			   !(getStatReply->stat->has_nlinks)  ||
			   !(getStatReply->stat->has_size) ||
			   !(getStatReply->stat->has_blocks) ||
			   !(getStatReply->stat->has_atime) ||
			   !(getStatReply->stat->has_mtime) ||
			   !(getStatReply->stat->has_ctime) ||
			   !(getStatReply->stat->has_btime) ||
			   getStatReply->stat->blocks > USHRT_MAX)
			{
				lodn_errno = LODN_ESTAT;
				*result = -1;

				return 0;
			}

			/* Copies the data to the new stat */
			statbuf->st_ino		= getStatReply->stat->inode;

			/* Translates the mode to local file type */
			switch(getStatReply->stat->mode)
			{
				case 0: statbuf->st_mode = S_IFDIR;	break;
				case 1: statbuf->st_mode = S_IFREG;	break;
				default: statbuf->st_mode = 0;      break;
			}

			statbuf->st_nlink	= getStatReply->stat->nlinks;
			statbuf->st_size	= getStatReply->stat->size;
			statbuf->st_blocks	= getStatReply->stat->blocks;

			/* Copies the times */
			statbuf->st_atime	  = getStatReply->stat->atime;
			statbuf->st_mtime 	  = getStatReply->stat->mtime;
			statbuf->st_ctime 	  = getStatReply->stat->ctime;
			//statbuf->st_birthtimespec.tv_sec  = getStatReply->stat->btime;

			statbuf->st_blksize	  = getStatReply->stat->st_blksize;


			/* Success */
			lodn_errno = LODN_OK;
			*result = 0;

			break;

		/* Bad message */
		default:
			lodn_errno = LODN_EBADMSG;
			*result    = -1;
	}

	/* Return success */
	return 0;
}


int lodn_stat(char *path, struct stat *statbuf, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options)
{
	/* Declarations */
	Lodn__GetStatRequest getStatRequest = LODN__GET_STAT_REQUEST__INIT;
	int result;


	/* Checks the args */
	if(path == NULL || statbuf == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* If a timeout is specified, the user must specify a request struct */
	if(timeout >=0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes the lodn errno */
	lodn_errno = LODN_OK;

	/* Sets the path */
	getStatRequest.path = path;

	/* Sets the message id */
	getStatRequest.msgid  	 = handle->currMsgID;
	getStatRequest.has_msgid = 1;
	handle->currMsgID++;


	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&getStatRequest,
							   GETSTAT_REQUEST, request, timeout,
							   getStatRequest.msgid, _getStat_callback, statbuf);

	/* Return the result */
	return result;
}


int lodn_getdir(char *path, lodn_dirent_t ***dirents, lodn_session_handle_t handle,
				lodn_request_t request, int timeout, int options)
{
	/* Declarations */
	Lodn__GetDirRequest	getDirRequest = LODN__GET_DIR_REQUEST__INIT;
	int result;


	/* Checks the args */
	if(path == NULL || dirents == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* If a timeout is specified, the user must specify a request struct */
	if(timeout >=0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes lodn error */
	lodn_errno = LODN_OK;

	/* Sets the path */
	getDirRequest.path = path;

	/* Sets the message id */
	getDirRequest.msgid  	= handle->currMsgID;
	getDirRequest.has_msgid = 1;
	handle->currMsgID++;

	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&getDirRequest,
							   GETDIR_REQUEST, request, timeout,
							   getDirRequest.msgid, _getDir_callback, dirents);

	/* Return the result (lodn_errno will be set) */
	return result;
}


int lodn_rename(char *origpath, char *newpath, lodn_session_handle_t handle,
		        lodn_request_t request, int timeout, int options)
{
	/* Declarations */

	lodn_errno = LODN_EUNIMPLEMENTED;

	return -1;
}


struct _getxattr_args
{
	size_t *size;
	void   *value;
};

static int _getxattr_callback(lodn_request_t request, int type,
							  ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;
	Lodn__GetXattrReply *getXattrReply;
	struct _getxattr_args *args = NULL;


	/* Gets the pointer to the struct for size and value buffer
	 * pointers */
	args = (struct _getxattr_args*)request->arg;


	/* Message type */
	switch(type)
	{
		/* Standard response indicates an error */
		case SESSION_RESPONSE_MSG_TYPE:

			/* Gets a pointer to the type of message */
			responseMsg = (Lodn__ResponseMsg *)message;

			/* Sets the errno value according the return message value */
			lodn_errno = responseMsg->errnum;

			/* Sets the result value */
			*result = (responseMsg->errnum != 0) ? -1 : 0;

			break;

		/* Getxattr reply message */
		case GETXATTR_REPLY:

			/* Gets a pointer to the type of message */
			getXattrReply = (Lodn__GetXattrReply *)message;

			/* Error no value */
			if(!getXattrReply->has_value)
			{
				lodn_errno = LODN_EBADMSG;
				*result    = -1;
				break;
			}


			/* If the input size is not zero then return the xattr and the
			 * size */
			if(*(args->size) != 0 && args->value != NULL)
			{
				/* Calculates the min size to copy */
				*(args->size) = (getXattrReply->value.len < *(args->size)) ?
									getXattrReply->value.len :  *(args->size);

				/* Copies the data */
				memcpy(args->value, getXattrReply->value.data, *(args->size));

			/* Input size was zero so just fill in the max size */
			}else
			{
				*(args->size) = getXattrReply->value.len;
			}

			/* Success */
			lodn_errno = LODN_OK;
			*result = 0;

			break;

		/* Bad message */
		default:

			lodn_errno = LODN_EBADMSG;
			*result    = -1;
	}

	/* Frees the args */
	free(args);

	/* Return Success */
	return 0;
}


int lodn_getxattr(char *path, const char *name, void *value, size_t *size,
					u_int32_t position, lodn_session_handle_t handle,
					lodn_request_t request, int timeout, int options)
{
	/* Declarations */
	Lodn__GetXAttrRequest getXattrRequest = LODN__GET_XATTR_REQUEST__INIT;
	struct _getxattr_args *args = NULL;
	int result;



	/* Checks the args */
	if(path == NULL || name == NULL || handle == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Unsupported args for right now */
	if(position != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* If a timeout is specified, the user must specify a request struct */
	if(timeout >=0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes lodn error */
	lodn_errno = LODN_OK;

	/* Initializes the request */
	getXattrRequest.path 		 = path;
	getXattrRequest.name 		 = name;
	getXattrRequest.has_position = 0;
	getXattrRequest.has_size     = 0;

	/* Sets the message id */
	getXattrRequest.msgid  	= handle->currMsgID;
	getXattrRequest.has_msgid = 1;
	handle->currMsgID++;


	/* Allocates an args for holding the return size and buffer */
	if((args = (struct _getxattr_args *)malloc(sizeof(struct _getxattr_args))) == NULL)
	{
		lodn_errno = LODN_EMEM;
	}

	/* Stores the pointers to the size and value buffer */
	args->size = size;
	args->value = value;

	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&getXattrRequest,
							   GETXATTR_REQUEST, request, timeout,
							   getXattrRequest.msgid, _getxattr_callback, args);

	/* Returns the result */
	return result;
}


static int _setxattr_callback(lodn_request_t request, int type,
							  ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;


	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}


int lodn_setxattr(const char *path, const char *name, void *value, size_t size,
					u_int32_t position, lodn_session_handle_t handle,
					lodn_request_t request, int timeout, int options)
{
	/* Declarations */
	Lodn__SetXAttrRequest setXattrRequest = LODN__SET_XATTR_REQUEST__INIT;
	int result;


	/* Checks the args */
	if(path == NULL || name == NULL || value == NULL || size < 0 ||
	   position != 0 || handle == NULL ||
	   (options | LODN_XATTR_REPLACE) != LODN_XATTR_REPLACE)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Can't have a timeout and no request */
	if(timeout >= 0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the lodn errno */
	lodn_errno = LODN_OK;

	/* Sets the variables */
	setXattrRequest.path 	 	= path;
	setXattrRequest.name 	 	= name;
	setXattrRequest.value.data  = value;
	setXattrRequest.value.len 	= size;
	setXattrRequest.has_value 	= 1;

	/* Options for replace */
	if(options & LODN_XATTR_REPLACE)
	{
		setXattrRequest.replace 	= 1;
		setXattrRequest.has_replace = 1;
	}

	/* Sets the message id */
	setXattrRequest.msgid  	= handle->currMsgID;
	setXattrRequest.has_msgid = 1;
	handle->currMsgID++;


	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&setXattrRequest,
							   SETXATTR_REQUEST, request, timeout,
							   setXattrRequest.msgid, _setxattr_callback, NULL);

	/* Returns the result */
	return result;
}


int _removexattr_callback(lodn_request_t request, int type,
		  	  	  	  	  ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;


	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}


int lodn_removexattr(const char *path, const char *name,
					 lodn_session_handle_t handle, lodn_request_t request,
					 int timeout, int options)
{
	/* Declarations */
	Lodn__RemoveXattrRequest removeXattrRequest = LODN__REMOVE_XATTR_REQUEST__INIT;
	int result;


	/* Checks the args */
	if(path == NULL || name == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Can't have a timeout and no request */
	if(timeout >= 0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the lodn errno */
	lodn_errno = LODN_OK;

	/* Sets the path and name of the attribute to delete */
	removeXattrRequest.path = path;
	removeXattrRequest.name = name;

	/* Sets the message id */
	removeXattrRequest.msgid  	= handle->currMsgID;
	removeXattrRequest.has_msgid = 1;
	handle->currMsgID++;


	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&removeXattrRequest,
							   REMOVEXATTR_REQUEST, request, timeout,
							   removeXattrRequest.msgid, _removexattr_callback,
							   NULL);

	/* Returns the result */
	return result;
}


struct _listxattr_args
{
	size_t *size;
	char   *namebuf;
};

static int _listxattr_callback(lodn_request_t request, int type,
							  ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg    *responseMsg;
	Lodn__ListXattrReply *listXattrReply;
	struct _listxattr_args *args = NULL;
	size_t size = 0;
	size_t amt  = 0;
	size_t i;


	/* Gets a pointer the args */
	args = (struct _listxattr_args*)request->arg;

	/* Message type */
	switch(type)
	{
		/* Standard response indicates an error */
		case SESSION_RESPONSE_MSG_TYPE:

			/* Gets a pointer to the type of message */
			responseMsg = (Lodn__ResponseMsg *)message;

			/* Sets the errno value according the return message value */
			lodn_errno = responseMsg->errnum;

			/* Sets the result value */
			*result = (responseMsg->errnum != 0) ? -1 : 0;

			break;

		/* Getxattr reply message */
		case LISTXATTR_REPLY:

			/* Gets a pointer to the type of message */
			listXattrReply = (Lodn__ListXattrReply *)message;

			/* If the input size is not zero then return the list of xattrs */
			if(args->namebuf != NULL)
			{
				size = *(args->size);
				*(args->size) = 0;

				/* Iterates over the list over the list of names and builds
				 * the return buffer */
				for(i=0; i<listXattrReply->n_name; i++)
				{
					amt = strlen(listXattrReply->name[i]) + 1;

					/* Name is too big for the remaining size so break out */
					if(amt > size)
					{
						break;
					}

					/* Copies the name into the buffer and updates the
					 * remaining size and the size of the buffer */
					memcpy(args->namebuf+*(args->size),
									listXattrReply->name[i], amt);
					size          -= amt;
					*(args->size) += amt;
				}

			/* No buffer so just return the size of the buffer needed */
			}else
			{
				for(i=0; i<listXattrReply->n_name; i++)
				{
					*(args->size) += strlen(listXattrReply->name[i]) + 1;
				}
			}

			break;


		/* Bad message */
		default:

			lodn_errno = LODN_EBADMSG;
			*result    = -1;
	}


	/* Frees the args */
	free(args);

	/* Return Success */
	return 0;
}


int lodn_listxattr(const char *path, char *namebuf, uint32_t *size,
			  lodn_session_handle_t handle, lodn_request_t request,
			  int timeout, int options)
{
	/* Declarations */
	Lodn__ListXattrRequest listXattrRequest = LODN__LIST_XATTR_REQUEST__INIT;
	struct _listxattr_args *args = NULL;
	int result;


	/* Checks the args */
	if(path == NULL || size == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Can't have a timeout and no request */
	if(timeout >= 0 && request == NULL)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the lodn errno */
	lodn_errno = LODN_OK;

	/* Sets the path and name of the attribute to delete */
	listXattrRequest.path = path;

	/* Sets the message id */
	listXattrRequest.msgid     = handle->currMsgID;
	listXattrRequest.has_msgid = 1;
	handle->currMsgID++;

	/* Allocates an args for holding the return size and buffer */
	if((args = (struct _listxattr_args *)malloc(sizeof(struct _listxattr_args))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		return -1;
	}

	/* Copies the pointers to the size and namebuf pointers */
	args->size    = size;
	args->namebuf = namebuf;

	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&listXattrRequest,
							   LISTXATTR_REQUEST, request, timeout,
							   listXattrRequest.msgid, _listxattr_callback,
							   args);

	/* Returns the result */
	return result;
}



static int _addMappings_callback(lodn_request_t request, int type,
							ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg *responseMsg;


	/* Checks that the type is right */
	if(type != SESSION_RESPONSE_MSG_TYPE)
	{
		lodn_errno = LODN_EBADMSG;
		*result    = -1;
		return 0;
	}

	/* Gets a pointer to the type of message */
	responseMsg = (Lodn__ResponseMsg *)message;

	/* Sets the errno value according the return message value */
	lodn_errno = responseMsg->errnum;

	/* Sets the result value */
	*result = (responseMsg->errnum != 0) ? -1 : 0;

	/* Return success */
	return 0;
}



static int _getMappings_callback(lodn_request_t request, int type,
							    ProtobufCMessage *message, int *result)
{
	/* Declarations */
	Lodn__ResponseMsg 		*responseMsg;
	Lodn__GetMappingsReply  *getMappingsMsg;
	Lodn__Mapping *mapping;
	LorsSet		  *set = NULL;
	LorsMapping *lm = NULL;
	char ibpCap[1024];
	size_t i;
	size_t j;

	LorsDepotPool *dp = NULL;
	int retval = 0;
	IBP_depot *depots;


	/* Handles the different message */
	switch(type)
	{
		/* Response message */
		case SESSION_RESPONSE_MSG_TYPE:

			/* Gets a pointer to the type of message */
			responseMsg = (Lodn__ResponseMsg *)message;

			/* Sets the errno value according the return message value */
			lodn_errno = responseMsg->errnum;

			/* Sets the result value */
			*result = (responseMsg->errnum != 0) ? -1 : 0;

			break;

		/* Normal successfully message */
		case GETMAPPINGS_REPLY:

			/* Gets a pointer to the type of message */
			getMappingsMsg = (Lodn__GetMappingsReply *)message;

			if(lorsSetCreateEmpty(&set) != LORS_SUCCESS || set == NULL)
			{
				lodn_errno = LODN_EMEM;
				goto ERROR_HANDLER;
			}

			if((depots = calloc(getMappingsMsg->n_mappings+1,
									sizeof(IBP_depot))) == NULL)
			{
				lodn_errno = LODN_EMEM;
				goto ERROR_HANDLER;
			}

			for(i=0; i<getMappingsMsg->n_mappings; i++)
			{
				mapping = getMappingsMsg->mappings[i];

				/*
				protobuf_c_boolean has_alloc_length;
				  uint64_t alloc_length;
				  protobuf_c_boolean has_alloc_offset;
				  uint64_t alloc_offset;
				  protobuf_c_boolean has_e2e_blocksize;
				  uint64_t e2e_blocksize;
				  protobuf_c_boolean has_exnode_offset;
				  uint64_t exnode_offset;
				  protobuf_c_boolean has_logical_length;
				  uint64_t logical_length;
				  char *host;
				  protobuf_c_boolean has_port;
				  uint32_t port;
				  char *rid;
				  char *readkey;
				  char *writekey;
				  char *managekey;
				  char *wrm_key;
				  size_t n_metadata;
				  Lodn__Metadata **metadata;
				  Lodn__Function *function;
				  */

				/* Validates the mapping */
				if(!mapping->has_alloc_length || !mapping->has_alloc_offset   ||
				   !mapping->has_e2e_blocksize || !mapping->has_exnode_offset ||
				   !mapping->has_logical_length || mapping->host == NULL ||
				   !mapping->has_port || mapping->rid == NULL ||
				   (mapping->readkey == NULL && mapping->writekey == NULL &&
				    mapping->managekey == NULL))
				{
					lodn_errno = LODN_EMAPPING;
					goto ERROR_HANDLER;
				}

				for(j=0; j<getMappingsMsg->n_mappings && depots[j] != NULL; j++)
				{
					if(strncmp(mapping->host, depots[j]->host, IBP_MAX_HOSTNAME_LEN-1) == 0 &&
					   mapping->port == depots[j]->port /*&&
					   mapping->rid == depots[j]->rid*/)
					{
						break;
					}
				}

				if(depots[j] == NULL)
				{
					if((depots[j] = (IBP_depot)calloc(1, sizeof(struct ibp_depot))) == NULL)
					{
						lodn_errno = LODN_EMEM;
						goto ERROR_HANDLER;
					}

					strncpy(depots[j]->host, mapping->host, IBP_MAX_HOSTNAME_LEN-1);
					depots[j]->port = mapping->port;
					/*if(sscanf(mapping->rid, "%d", &(depots[j]->rid)) != 1)
					{
						lodn_errno = LODN_EMAPPING;
						goto ERROR_HANDLER;
					}*/
				}


				/* Allocate a mapping */
				if((lm = (LorsMapping*)calloc(1,sizeof(LorsMapping))) == NULL )
				{
					lodn_errno = LODN_EMAPPING;
					goto ERROR_HANDLER;
				}

				/* Placement data */
				lm->alloc_length 	= mapping->alloc_length;
				lm->alloc_offset 	= mapping->alloc_offset;
				lm->e2e_bs 			= mapping->e2e_blocksize;
				lm->exnode_offset 	= mapping->exnode_offset;
				lm->logical_length	= mapping->logical_length;

				/* Fills in the depot struct */
				snprintf(lm->depot.host, sizeof(lm->depot), "%s", mapping->host);
				lm->depot.port = mapping->port;

				/* Assemblies the read key */
				if(mapping->readkey != NULL)
				{
					snprintf(ibpCap, sizeof(ibpCap), "ibp://%s:%d/%s#%s/READ",
							 mapping->host, mapping->port, mapping->rid,
						     mapping->readkey);

					if((lm->capset.readCap = strdup(ibpCap)) == NULL)
					{
						free(lm);

						lodn_errno = LODN_EMEM;
						goto ERROR_HANDLER;
					}

				}

				/* Assemblies the write key */
				if(mapping->writekey != NULL)
				{
					snprintf(ibpCap, sizeof(ibpCap), "ibp://%s:%d/%s#%s/WRITE",
								mapping->host, mapping->port, mapping->rid,
								mapping->writekey);

					if((lm->capset.writeCap = strdup(ibpCap)) == NULL)
					{
						if(mapping->readkey) { free(lm->capset.readCap); }
						free(lm);

						lodn_errno = LODN_EMEM;
						goto ERROR_HANDLER;
					}
				}

				/* Assemlies the manage key */
				if(mapping->managekey != NULL)
				{
					snprintf(ibpCap, sizeof(ibpCap), "ibp://%s:%d/%s#%s/MANAGE",
								mapping->host, mapping->port, mapping->rid,
							    mapping->managekey);

					if((lm->capset.manageCap = strdup(ibpCap)) == NULL)
					{
						if(mapping->writekey) { free(lm->capset.writeCap); }
						if(mapping->readkey)  { free(lm->capset.readCap);  }
						free(lm);

						lodn_errno = LODN_EMEM;
						goto ERROR_HANDLER;
					}
				}

				/* Adds the mapping to the set */
				if(lorsSetAddMapping(set, lm) != LORS_SUCCESS)
				{
					if(mapping->managekey) { free(lm->capset.manageCap); }
					if(mapping->writekey)  { free(lm->capset.writeCap);  }
					if(mapping->readkey)   { free(lm->capset.readCap);   }
					free(lm);

					lodn_errno = LODN_EGENERIC;
					goto ERROR_HANDLER;
				}
			}


			if(getMappingsMsg->n_mappings > 0)
			{
				retval = lorsGetDepotPool(&dp, NULL, 0, depots, 1, NULL, 1,
											IBP_HARD, 0, 1, 30, 0);
			}

			if(retval != LORS_SUCCESS)
			{
				fprintf(stderr, "dp not initialized, error is %d\n", retval);
				exit(EXIT_FAILURE);
			}

			LorsEnum enumList;
			LorsEnum iterator = NULL;

			if(lorsSetEnum(set, &enumList) != LORS_SUCCESS)
			{
				fprintf(stderr, "lorsSetEnum failure\n");
				exit(EXIT_FAILURE);
			}

			for(iterator=NULL, lm=NULL;
				lorsEnumNext(enumList, &iterator, &lm) != LORS_END;)
			{
				lm->dp = dp;
			}

			lorsEnumFree(enumList);


			lodn_errno = LODN_OK;
			*result = getMappingsMsg->n_mappings;

			break;

		/* Bad message */
		default:
			lodn_errno = LODN_EBADMSG;
			*result    = -1;
	}


	/* Fills in the arg (LorsSet**) or NULL*/
	*((LorsSet**)request->arg) = set;


	/* Return success */
	return 0;


	/* Error handler */
	ERROR_HANDLER:

		/* Frees the resources */
		if(set != NULL)
		{
			 lorsSetFree(set, LORS_FREE_MAPPINGS);
		}

		/* Set result to failure */
		*result = -1;

		/* But hey, at least the function returns */
		return 0;
}



static int _lodn_parseIBPCap(IBP_cap cap, char **host, int *port,
						     char **rid, char **key, char **wrm_key, char **wrm)
{
	/* Declarations */
	char tmpHost[IBP_MAX_HOSTNAME_LEN];
	int  tmpPort;
	char tmpRid[16];
	char tmpKey[256];
	char tmpWRMKey[256];
	char tmpWRM[16];


	/* Splits the IBP cap up into its elements */
	if(sscanf(cap, "ibp://%256[A-Za-z0-9.]:%5d/%15[^#]#%256[^/]/%256[^/]/%16s",
			  tmpHost, &tmpPort, tmpRid, tmpKey, tmpWRMKey, tmpWRM) != 6)
	{
		lodn_errno = LODN_EIBPCAP;
		goto ERROR_HANDLER;
	}

	/* Checks the port */
	if(tmpPort <= 0 || tmpPort > 65535)
	{
		lodn_errno = LODN_EIBPCAP;
		goto ERROR_HANDLER;
	}

	/* Checks the wrm */
	if(strncmp(tmpWRM, "READ",  sizeof(tmpWRM))  != 0 &&
	   strncmp(tmpWRM, "WRITE", sizeof(tmpWRM))  != 0 &&
	   strncmp(tmpWRM, "MANAGE", sizeof(tmpWRM)) != 0)
	{
		lodn_errno = LODN_EIBPCAP;
		goto ERROR_HANDLER;
	}

	/* Copies the values from the cap */
	if(host    != NULL)	*host    = strdup(tmpHost);
	if(port    != NULL)	*port    = tmpPort;
	if(rid	   != NULL)	*rid	 = strdup(tmpRid);
	if(key 	   != NULL)	*key	 = strdup(tmpKey);
	if(wrm_key != NULL) *wrm_key = strdup(tmpWRMKey);
	if(wrm     != NULL) *wrm     = strdup(tmpWRM);

	/* Checks the heap allocated values */
	if((host != NULL && *host == NULL) || (rid != NULL && *rid == NULL) ||
	   (key  != NULL && *key == NULL)  || (wrm_key != NULL && *wrm_key == NULL) ||
	   (wrm  != NULL && *wrm == NULL))
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Return successfully */
	return 0;


	/* Handles errors */
	ERROR_HANDLER:

		/* Release resources */
		if(host != NULL && *host != NULL)
		{
			free(*host);
			*host = NULL;
		}

		if(rid != NULL && *rid != NULL)
		{
			free(*rid);
			*rid = NULL;
		}

		if(key != NULL && *key != NULL)
		{
			free(*key);
			*key = NULL;
		}

		if(wrm_key != wrm_key && *wrm_key != NULL)
		{
			free(*wrm_key);
			*wrm_key = NULL;
		}

		if(wrm != NULL && *wrm != NULL)
		{
			free(*wrm);
			*wrm = NULL;
		}

		/* Clear the output arguments */
		if(port 	!= NULL) *port    = -1;

		/* Return Failure */
		return -1;
}


static int _serializeFunction(ExnodeFunction *f, char **buf, int *len)
{
	/* Declarations */
	xmlDocPtr 	doc   = NULL;
	xmlNodePtr 	node  = NULL;


	if((doc = xmlNewDoc(NULL)) == NULL)
	{
		return -1;
	}

	if((node = xmlNewDocRawNode(doc,NULL, "function",NULL)) == NULL)
	{
		xmlFreeDoc(doc);
		return -1;
	}

	if(xmlDocSetRootElement(doc,node) != 0)
	{
		xmlFreeNode(node);
		xmlFreeDoc(doc);
		return -1;
	}

	if(exnodeSerializeFunction(f, doc, NULL, node) != 0)
	{
		xmlFreeDoc(doc);
		return -1;
	}


	xmlDocDumpFormatMemory(doc, buf, len, 0);


	xmlFreeDoc(doc);


	return 0;
}


int lodn_insertMappings(char *path, LorsSet *lorsSet,
						lodn_session_handle_t handle,lodn_request_t request,
						int timeout, int options)
{
	/* Declarations */
	Lodn__AddMappingsRequest addMappingsRequest = LODN__ADD_MAPPINGS_REQUEST__INIT;
	LorsEnum list			  = NULL;
	LorsEnum mappingIterator  = NULL;
	LorsMapping *mapping      = NULL;
	size_t numMappings        = 0;
	Lodn__Mapping **mappings  = NULL;
	char *host = NULL;
	int   port;
	char *rid = NULL;
	char *key = NULL;
	char *wrm_key;
	size_t i;
	int length;
	int retval;
	int result;


	/* Checks the input parameters */
	if(path == NULL || lorsSet == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes lodn_errno */
	lodn_errno = LODN_OK;

	/* Get a set enum */
 	if(lorsSetEnum(lorsSet, &list) != LORS_SUCCESS)
 	{
 		lodn_errno = LODN_EGENERIC;
 		goto ERROR_HANDLER;
 	}

	/* Counts the number of mappings */
	while((retval = lorsEnumNext(list, &mappingIterator, &mapping)) != LORS_END)
	{
		switch(retval)
		{
			case LORS_SUCCESS:
				numMappings++;
				break;

			case LORS_END:
				break;

			default:
				lodn_errno = LODN_EGENERIC;
				goto ERROR_HANDLER;
		}
	}


	/* Allocates an array to hold the LoDN__Mappings */
	if((mappings = (Lodn__Mapping **)calloc(numMappings,
											sizeof(Lodn__Mapping *))) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Iterates through the list of mappings */
	for(i=0, mappingIterator=NULL, retval=lorsEnumNext(list, &mappingIterator, &mapping);
		retval != LORS_END;
	    retval = lorsEnumNext(list, &mappingIterator, &mapping), i++)
	{
		switch(retval)
		{
			/* New Mapping */
			case LORS_SUCCESS:

				/* Allocates a mapping */
				if((mappings[i] = (Lodn__Mapping*)malloc(sizeof(Lodn__Mapping))) == NULL)
				{
					lodn_errno = LODN_EMEM;
					goto ERROR_HANDLER;
				}

				/* Initializes the mapping */
				lodn__mapping__init(mappings[i]);

				/*
				      protobuf_c_boolean has_alloc_length;
					  uint64_t alloc_length;
					  protobuf_c_boolean has_alloc_offset;
					  uint64_t alloc_offset;
					  protobuf_c_boolean has_e2e_blocksize;
					  uint64_t e2e_blocksize;
					  protobuf_c_boolean has_exnode_offset;
					  uint64_t exnode_offset;
					  protobuf_c_boolean has_logical_length;
					  uint64_t logical_length;
					  char *host;
					  protobuf_c_boolean has_port;
					  uint32_t port;
					  char *rid;
					  char *readkey;
					  char *writekey;
					  char *managekey;
					  char *wrm_key;
					  size_t n_metadata;
					  Lodn__Metadata **metadata;
					  Lodn__Function *function;
				 */

				mappings[i]->has_alloc_length 	= 1;
				mappings[i]->alloc_length     	= mapping->alloc_length;
				mappings[i]->has_alloc_offset 	= 1;
				mappings[i]->alloc_offset	  	= mapping->alloc_offset;
				mappings[i]->has_e2e_blocksize	= 1;
				mappings[i]->e2e_blocksize		= mapping->e2e_bs;
				mappings[i]->has_exnode_offset  = 1;
				mappings[i]->exnode_offset		= mapping->exnode_offset;
				mappings[i]->has_logical_length = 1;
				mappings[i]->logical_length		= mapping->logical_length;

				if(mapping->capset.readCap != NULL)
				{
					if(_lodn_parseIBPCap(mapping->capset.readCap, &host, &port,
											  &rid, &key, &wrm_key, NULL) != 0)
					{
						goto ERROR_HANDLER;
					}

					mappings[i]->host 		= host;
					mappings[i]->has_port 	= 1;
					mappings[i]->port 		= port;
					mappings[i]->rid  		= rid;


					length = strlen(key)+strlen(wrm_key)+2;

					if((mappings[i]->readkey = malloc(
												length*sizeof(char))) == NULL)
					{
						goto ERROR_HANDLER;
					}

					snprintf(mappings[i]->readkey, length, "%s/%s", key, wrm_key);

					free(key);
					free(wrm_key);
				}

				if(mapping->capset.writeCap != NULL)
				{
					if(host == NULL)
					{
						if(_lodn_parseIBPCap(mapping->capset.writeCap, &host, &port,
											  &rid, &key, &wrm_key, NULL) != 0)
						{
							goto ERROR_HANDLER;
						}

						mappings[i]->host 		= host;
						mappings[i]->has_port 	= 1;
						mappings[i]->port 		= port;
						mappings[i]->rid  		= rid;

					}else
					{
						if(_lodn_parseIBPCap(mapping->capset.writeCap, NULL,
											 NULL, NULL, &key, &wrm_key, NULL) != 0)
						{
							goto ERROR_HANDLER;
						}
					}


					length = strlen(key)+strlen(wrm_key)+2;

					if((mappings[i]->writekey = malloc(
											length*sizeof(char))) == NULL)
					{
						goto ERROR_HANDLER;
					}

					snprintf(mappings[i]->writekey, length, "%s/%s", key, wrm_key);

					free(key);
					free(wrm_key);
				}

				if(mapping->capset.manageCap != NULL)
				{
					if(host == NULL)
					{
						if(_lodn_parseIBPCap(mapping->capset.manageCap, &host, &port,
											  &rid, &key, &wrm_key, NULL) != 0)
						{
							goto ERROR_HANDLER;
						}

						mappings[i]->host 		= host;
						mappings[i]->has_port 	= 1;
						mappings[i]->port 		= port;
						mappings[i]->rid  		= rid;

					}else
					{
						if(_lodn_parseIBPCap(mapping->capset.manageCap, NULL,
										NULL, NULL, &key, &wrm_key, NULL) != 0)
						{
							goto ERROR_HANDLER;
						}

						mappings[i]->managekey 	= key;
					}

					length = strlen(key)+strlen(wrm_key)+2;

					if((mappings[i]->managekey = malloc(
												length*sizeof(char))) == NULL)
					{
						goto ERROR_HANDLER;
					}

					snprintf(mappings[i]->managekey, length, "%s/%s", key, wrm_key);

					free(key);
					free(wrm_key);
				}

				char *funcBuf;
				int len;

				if(mapping->function != NULL)
				{
					if(_serializeFunction(mapping->function, &funcBuf, &len) != 0)
					{
						goto ERROR_HANDLER;
					}

					mappings[i]->function = funcBuf;
				}

				break;

			/* End of mappings */
			case LORS_END:
				break;

			/* Error */
			default:
				lodn_errno = LODN_EGENERIC;
				goto ERROR_HANDLER;
		}
	}

	/* Sets up the message for adding mappings */
	addMappingsRequest.path 	  = path;
	addMappingsRequest.n_mappings = numMappings;
	addMappingsRequest.mappings   = mappings;

	/* Sets the message id */
	addMappingsRequest.msgid  	 = handle->currMsgID;
	addMappingsRequest.has_msgid = 1;
	handle->currMsgID++;

	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&addMappingsRequest,
							   ADDMAPPINGS_REQUEST, request, timeout,
							   addMappingsRequest.msgid, _addMappings_callback,
							   NULL);

	/* Frees the resources */
	lorsEnumFree(list);

	for(i=0; i<numMappings && mappings[i] != NULL; i++)
	{
		if(mappings[i]->host		!= NULL) { free(mappings[i]->host); 	}
		if(mappings[i]->rid			!= NULL) { free(mappings[i]->rid); 		}
		if(mappings[i]->readkey		!= NULL) { free(mappings[i]->readkey); 	}
		if(mappings[i]->writekey	!= NULL) { free(mappings[i]->writekey); }
		if(mappings[i]->managekey	!= NULL) { free(mappings[i]->managekey);}
		if(mappings[i]->function    != NULL) { free(mappings[i]->function); }

		free(mappings[i]);
	}

	free(mappings);



	/* Return successfully */
	return result;

	/* Handles errors */
	ERROR_HANDLER:

		/* Frees the resources */

		if(key != NULL)
		{
			free(key);
		}

		if(wrm_key != NULL)
		{
			free(wrm_key);
		}

		if(list != NULL)
		{
			lorsEnumFree(list);

			if(mappings != NULL)
			{
				for(i=0; i<numMappings && mappings[i] != NULL; i++)
				{
					if(mappings[i]->host		!= NULL)
					{
						free(mappings[i]->host);
					}

					if(mappings[i]->rid			!= NULL)
					{
						free(mappings[i]->rid);
					}

					if(mappings[i]->readkey		!= NULL)
					{
						free(mappings[i]->readkey);
					}

					if(mappings[i]->writekey	!= NULL)
					{
						free(mappings[i]->writekey);
					}

					if(mappings[i]->managekey	!= NULL)
					{
						free(mappings[i]->managekey);
					}

					if(mappings[i]->function != NULL)
					{
						free(mappings[i]->function);
					}

					free(mappings[i]);
				}

				free(mappings);
			}
		}

		/* Return failure */
		return -1;
}




int lodn_getMappings(char *path, LorsSet **lorsSet,
					 unsigned int offset, unsigned int num_mappings,
					 lodn_session_handle_t handle,lodn_request_t request,
					 int timeout, int options)
{
	/* Declarations */
	Lodn__GetMappingsRequest getMappingsRequest = LODN__GET_MAPPINGS_REQUEST__INIT;
	int result;


	/* Checks the input parameters */
	if(path == NULL || lorsSet == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initialize the lorsSet */
	*lorsSet = NULL;

	/* Initializes lodn_errno */
	lodn_errno = LODN_OK;

	/* Sets up the message */
	getMappingsRequest.path 			 = path;
	getMappingsRequest.mappingoffset     = offset;
	getMappingsRequest.nummappings       = num_mappings;
	getMappingsRequest.has_mappingoffset = 1;
	getMappingsRequest.has_nummappings   = 1;

 	/* Sets the message id */
	getMappingsRequest.msgid  	 = handle->currMsgID;
	getMappingsRequest.has_msgid = 1;
	handle->currMsgID++;


	/* Performs the actual work of sending and receiving the messages */
	result = _shared_component(handle, (ProtobufCMessage*)&getMappingsRequest,
							   GETMAPPINGS_REQUEST, request, timeout,
							   getMappingsRequest.msgid, _getMappings_callback,
							   lorsSet);

	/* Return the result */
	return result;


	/* Handles errors */
	ERROR_HANDLER:

		/* Return failure */
		return -1;
}


int lodn_importExnodeFile(char *path, char *exnodefile,
						  lodn_session_handle_t handle, int options)
{
	/* Declarations */
	LorsExnode *exnode		  = NULL;
	LorsSet *lorsSet		  = NULL;
	LorsDepotPool *dp         = NULL;
	int result;


	/* Checks the input parameters */
	if(path == NULL || exnodefile == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Initializes lodn_errno */
	lodn_errno = LODN_OK;

	/* Reads the exnode file into exnode */
	if(lorsFileDeserialize(&exnode, exnodefile, NULL) != LORS_SUCCESS )
	{
		lodn_errno = LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

	/* Gets a lors depot pool */
	if(lorsUpdateDepotPool(exnode, &dp, NULL, 0, NULL, 1, 60, 0) != LORS_SUCCESS)
	{
		lodn_errno = LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

	/* Gets a set for the whole exnode */
	if(lorsQuery(exnode, &lorsSet, 0, exnode->logical_length, 0) != LORS_SUCCESS)
	{
		lodn_errno = LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

	/* Creates the file */
	result = lodn_createFile(path, handle, NULL, -1, 0);

	/* If the file was successfully created then it attempts to insert the
	 * mappings */
	if(result == 0)
	{
		/* Calls the routine to insert the mappings */
		result = lodn_insertMappings(path, lorsSet, handle, NULL, -1, 0);
	}else
	{
		goto ERROR_HANDLER;
	}

	/* Frees the depot pool */
 	lorsFreeDepotPool(dp);

	/* Frees the set */
	lorsSetFree(lorsSet, 0);

	/* Free the exnode */
	lorsExnodeFree(exnode);


 	/* Returns the result */
 	return result;


	/* Handle any errors */
	ERROR_HANDLER:

		/* Frees the resources */
		if(exnode != NULL)
		{
			/* Frees the depot pool */
			if(dp != NULL)
			{
				lorsFreeDepotPool(dp);
			}

			/* Frees the set */
			if(lorsSet != NULL)
			{

				lorsSetFree(lorsSet, 0);
			}

			/* Free the exnode */
			lorsExnodeFree(exnode);
		}

		/* Return Failure */
		return -1;
}



int lodn_exportExnodeFile(char *path, char *exnodefile,
						  lodn_session_handle_t handle, int options)
{
	/* Declarations */
	LorsExnode 	*exnode = NULL;
	LorsSet 	*lorsSet = NULL;



	/* Checks the input parameters */
	if(path == NULL || exnodefile == NULL || handle == NULL || options != 0)
	{
		lodn_errno = LODN_EINVALIDPARAM;
		return -1;
	}

	/* Attempts to get the mappings from lodn for the file specified */
	if(lodn_getMappings(path, &lorsSet, 0, 64, handle, NULL, -1, 0) < 0)
	{
		/* lodn_errno already set */
		goto ERROR_HANDLER;
	}


	/* Create the exnode */
	if(lorsExnodeCreate(&exnode) != LORS_SUCCESS)
	{
		/* lodn_errno already set */
		goto ERROR_HANDLER;
	}


	/* Appends the set to the exnode */
	if(lorsAppendSet(exnode, lorsSet) != LORS_SUCCESS)
	{
		/* lodn_errno already set */
		goto ERROR_HANDLER;
	}


	/* Writes the exnode to the file */
	if(lorsFileSerialize(exnode, exnodefile, 0, 0) != LORS_SUCCESS)
	{
		/* lodn_errno already set */
		goto ERROR_HANDLER;
	}


	/* Frees the set */
	lorsSetFree(lorsSet, 0);

	/* Free the exnode */
	lorsExnodeFree(exnode);


	/* Return successfully */
	return 0;

	/* Handles errors */
	ERROR_HANDLER:

		/* Frees the resources */
		if(lorsSet != NULL)
		{
			if(exnode != NULL)
			{
				/* Frees the set */
				lorsSetFree(lorsSet, 0);

				/* Free the exnode */
				lorsExnodeFree(exnode);
			}else
			{
				/* Frees the set */
				lorsSetFree(lorsSet, LORS_FREE_MAPPINGS);
			}
		}

		/* Return failure */
		return -1;
}




int lodn_getstatus(char *hostname, int port, lodn_status_t status)
{
	/* Declarations */
	MessageHeader msghdr;
	Lodn__LoDNInfoRequestMsg msg = LODN__LO_DNINFO_REQUEST_MSG__INIT;
	unsigned char *outbuf = NULL;
	unsigned char headerbuf[PROTOCOL_HEADER_SIZE];
	size_t size;
	int sfd = -1;
	size_t msgsize;
	ssize_t amtSent;
	ssize_t amtRead;
	ssize_t totalAmt = 0;
	unsigned char *inbuf = NULL;
	Lodn__LoDNInfoMsg *lodnInfoMsg = NULL;


	/* Initialize the errno */
	lodn_errno = LODN_OK;

	/* Create a connection to LoDN at host and port */
	if((sfd =  _lodn_create_connection(hostname, port)) < 0)
	{
		/* Errno already set by  _lodn_create_connection */
		return -1;
	}

	/* Construct a header for the message */
	msghdr.type = 0;
	msghdr.size = lodn__lo_dninfo_request_msg__get_packed_size(&msg);


	/* Allocate an output buffer for protocol buffer serialization */
	if((outbuf = malloc(msghdr.size+PROTOCOL_HEADER_SIZE)) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Write the the header and protocol buffer message to outbuf */
	lodn_MessageHeader_serialize(&msghdr, outbuf);
	lodn__lo_dninfo_request_msg__pack(&msg, outbuf+PROTOCOL_HEADER_SIZE);


	/* Write the message out to the socket */
	for(msgsize = msghdr.size+PROTOCOL_HEADER_SIZE; msgsize>0; msgsize-=amtSent)
	{
		if((amtSent = send(sfd, outbuf+totalAmt, msgsize, 0)) <=0)
		{
			lodn_errno = LODN_ESOCKET;
			goto ERROR_HANDLER;
		}

		totalAmt += amtSent;
	}

	/* Read the response header */
	for(msgsize=PROTOCOL_HEADER_SIZE; msgsize>0; msgsize-=amtRead)
	{
		if((amtRead = recv(sfd, headerbuf, msgsize, 0)) <= 0)
		{
			lodn_errno = LODN_ESOCKET;
			goto ERROR_HANDLER;
		}
	}

	/* Deserialize the header */
	lodn_MessageHeader_deserialize(headerbuf, &msghdr);

	/* Allocate a input buffer to the message */
	if((inbuf = malloc(msghdr.size)) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Read the message into inbuf */
	for(msgsize = msghdr.size; msgsize>0; msgsize-=amtRead)
	{
		if((amtRead = recv(sfd, inbuf, msgsize, 0)) <=0)
		{
			lodn_errno = LODN_ESOCKET;
			goto ERROR_HANDLER;
		}
	}

	/* Deserialize the message */
	if((lodnInfoMsg = lodn__lo_dninfo_msg__unpack(NULL, msghdr.size, inbuf)) == NULL)
	{
		lodn_errno = LODN_EPROTOBUF;
		goto ERROR_HANDLER;
	}


	/* Transfers the information to lodn_status from the message */
	status->major    = lodnInfoMsg->version->major;
	status->minor 	 = lodnInfoMsg->version->minor;
	status->subminor = lodnInfoMsg->version->subminor;


	/* Free the resources */
	close(sfd);
	free(outbuf);
	free(inbuf);
	lodn__lo_dninfo_msg__free_unpacked(lodnInfoMsg, NULL);

	/* Return success */
	return 0;


	/* Error hander */
	ERROR_HANDLER:

		/* Free allocated resources */
		if(sfd > 0)
		{
			close(sfd);

			if(outbuf != NULL)
			{
				free(outbuf);

				if(inbuf != NULL)
				{
					free(inbuf);

					if(lodnInfoMsg != NULL)
					{
						lodn__lo_dninfo_msg__free_unpacked(lodnInfoMsg, NULL);
					}
				}
			}
		}

		/* Return failure */
		return -1;
}






