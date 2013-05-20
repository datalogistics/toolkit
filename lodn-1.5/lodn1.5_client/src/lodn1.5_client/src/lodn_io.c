
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
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
#include "lodn_client/lodn_errno.h"
#include "lodn_client/lodn_types.h"
#include "lodn_io.h"



/* Holds the protobuf buffer descriptors by type */
const static ProtobufCMessageDescriptor *protobufDescriptorsByType[] =

		{
			&lodn__session_open_msg__descriptor, 		/* SESSION_OPEN_MSG_TYPE */
			&lodn__session_closed_msg__descriptor, 		/* SESSION_CLOSED_MSG_TYPE	*/
			&lodn__response_msg__descriptor,			/* SESSION_RESPONSE_MSG_TYPE	*/
			&lodn__lo_dninfo_request_msg__descriptor,	/* SESSION_LODNINFO_REQUEST_MSG_TYPE */
			&lodn__lo_dninfo_msg__descriptor, 			/* SESSION_LODNINFO_MSG_TYPE */
			&lodn__mkdir_msg__descriptor,				/* MKDIR_MSG */
			&lodn__rmdir_request__descriptor,			/* RMDIR_REQUEST */
			&lodn__create_file_request__descriptor,		/* CREATEFILE_REQUEST */
			&lodn__get_dir_request__descriptor,			/* GETDIR_REQUEST */
			&lodn__get_dir_reply__descriptor,			/* GETDIR_REPLY */
			&lodn__add_mappings_request__descriptor,	/* ADDMAPPINGS_REQUEST */
			&lodn__get_mappings_request__descriptor,	/* GETMAPPINGS_REQUEST */
			&lodn__get_mappings_reply__descriptor,	 	/* GETMAPPINGS_REPLY */
			&lodn__get_stat_request__descriptor,		/* GETSTAT_REQUEST */
			&lodn__get_stat_reply__descriptor,			/* GETSTAT_REPLY */
			&lodn__unlink_request__descriptor,			/* UNLINK_REQUEST */
			&lodn__set_xattr_request__descriptor,		/* SETXATTR REQUEST */
			&lodn__get_xattr_request__descriptor,		/* GETXATTR REQUEST */
			&lodn__get_xattr_reply__descriptor,			/* GETXATTR REPLY */
			&lodn__remove_xattr_request__descriptor,    /* REMOVEXATTR_REPLY */
			&lodn__list_xattr_request__descriptor,      /* LISTXATTR_REQUEST */
			&lodn__list_xattr_reply__descriptor,		/* LISTXATTR_REPLY */
		};




int _lodn_create_connection(char *hostname, in_port_t port)
{
	/* Declartions */
	int sfd = -1;
	static struct addrinfo hints = { 0, AF_INET, SOCK_STREAM, IPPROTO_TCP, 0,
									NULL, NULL, NULL };
	struct addrinfo *resultAddrInfo = NULL;
	struct addrinfo *addrInfo = NULL;
	char port_str[6];


	/* Initialize the lodn_errno value */
	lodn_errno = LODN_OK;

	/* Make a port string */
	sprintf(port_str, "%u", port);

	/* Gets a list of address for a host using TCP */
	if(getaddrinfo(hostname, port_str, &hints, &resultAddrInfo) != 0)
	{
		lodn_errno = LODN_EUNKNOWNHOST;
		return -1;
	}

	/* Tries each address trying to make a connection */
	for(addrInfo = resultAddrInfo; addrInfo != NULL; addrInfo = addrInfo->ai_next)
	{
		/* Creates a tcp socket */
		if((sfd = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto)) < 0)
		{
			lodn_errno = LODN_ESOCKET;
			goto ERROR_HANDLER;
		}

		if(connect(sfd, addrInfo->ai_addr, addrInfo->ai_addrlen) == 0)
		{
			break;
		}

		close(sfd);
		sfd = -1;
	}

	/* Checks that a connection was made before list exhaustion */
	if(addrInfo == NULL)
	{
		lodn_errno = LODN_ECONNECT;
		goto ERROR_HANDLER;
	}

	/* Sets the socket to nonblocking */
	if(fcntl(sfd, F_SETFL, (fcntl(sfd, F_GETFL, 0) |  O_NONBLOCK)) == -1)
	{
		lodn_errno = LODN_ESOCKET;
		goto ERROR_HANDLER;
	}

	/* Frees the result addr */
	freeaddrinfo(resultAddrInfo);

	/* Returns the connected socket */
	return sfd;


	/* Error Handler */
	ERROR_HANDLER:

		/* Free resources */
		if(resultAddrInfo != NULL)
		{
			freeaddrinfo(resultAddrInfo);

			if(sfd != -1)
			{
				close(sfd);
			}
		}

		/* Return failure */
		return -1;
}


static int _waitForData(int sfd, struct timeval *timeout)
{
	/* Declarations */
	fd_set  fdset;
	struct timeval currtime;
	struct timeval *timeleft;
	int    retval;


	/* Timeout */
	if(timeout != NULL)
	{
		/* Gets the current time of day */
		if(gettimeofday(&currtime, NULL) != 0)
		{
			lodn_errno = LODN_EGENERIC;
			return -1;
		}

		/* Calculates the time left before timeout */
		currtime.tv_sec  = timeout->tv_sec - currtime.tv_sec;
		currtime.tv_usec = timeout->tv_usec - currtime.tv_usec;

		/* Set the timeleft to waitperiod */
		timeleft = &currtime;

	/* No timeout */
	}else
	{
		timeleft = NULL;
	}

	/* Prepares the fd mask */
	FD_ZERO(&fdset);
	FD_SET(sfd, &fdset);


	/* Calls select on the IO with a timeout */
	retval = select(sfd+1, &fdset, NULL, NULL, timeleft);

	/* Evaluates the result of timeout */
	switch(retval)
	{
		/* Error */
		case -1:
			lodn_errno = LODN_ESOCKET;
			return -1;

		/* Timeout */
		case 0:
			lodn_errno = LODN_ETIMEOUT;
			return -1;

		/* Data is ready to be read */
		default:
			break;
	}

	/* Return that data is available */
	return 0;
}



int _lodn_recvMsg(lodn_session_handle_t handle, unsigned int *type,
						ProtobufCMessage** message, struct timeval *timeout)
{
	/* Declarations */
	MessageHeader msghdr;
//	unsigned char *buffer = NULL;
//	unsigned char bufferhdr[PROTOCOL_HEADER_SIZE];
	ssize_t totalAmtToRecv;
	ssize_t amtRecv;
//	ssize_t offset;


	/* Session buffer isn't big enough */
	if(handle->amt+PROTOCOL_HEADER_SIZE-handle->amt > handle->readBufferSize)
	{
		/* Calculates the new size of the buffer */
		handle->readBufferSize = (PROTOCOL_HEADER_SIZE >
									(handle->readBufferSize*2)) ?
											PROTOCOL_HEADER_SIZE :
												handle->readBufferSize*2;

		/* Increases the size of the buffer */
		if((handle->readBuffer = realloc(handle->readBuffer,
				                         handle->readBufferSize)) == NULL)
		{
			lodn_errno = LODN_EMEM;
			goto ERROR_HANDLER;
		}
	}

	/* Reads in the message header */
	for(totalAmtToRecv=PROTOCOL_HEADER_SIZE-handle->amt;
	    totalAmtToRecv > 0;
	    totalAmtToRecv -= amtRecv, handle->amt += amtRecv)
	{

#ifdef USE_SSL
		amtRecv = BIO_read(handle->bio, handle->readBuffer+handle->amt,
						   totalAmtToRecv);
#else
		amtRecv = recv(handle->sfd, handle->readBuffer+handle->amt,
					   totalAmtToRecv, 0);
#endif

		/* Checks for error or timeout */
		if(amtRecv <= 0)
		{
			/* Timeout */
#ifdef USE_SSL
			if(BIO_should_retry(handle->bio))
#else
			if(errno == EAGAIN)
#endif
			{
				/* Wait for data until timeout or error occurs */
				if(_waitForData(handle->sfd, timeout) != 0)
				{
					/* lodn errno is already set in function */
					goto ERROR_HANDLER;
				}

			/* Error has occurred */
			}else
			{
				lodn_errno = LODN_ESOCKET;
				goto ERROR_HANDLER;
			}

			/* Sets the amount to read to None */
			amtRecv = 0;
		}
	}


	/* Deserializes the message header */
	lodn_MessageHeader_deserialize(handle->readBuffer, &msghdr);

	/* Checks the type */
	if(msghdr.type >= (sizeof(protobufDescriptorsByType)/
								sizeof(ProtobufCMessageDescriptor *)))
	{
		lodn_errno = LODN_EPROTOBUF;
		goto ERROR_HANDLER;
	}

	/* Session buffer isn't big enough */
	if((handle->amt+msghdr.size) > handle->readBufferSize)
	{
		/* Calculates the new size of the buffer */
		handle->readBufferSize = (handle->amt+msghdr.size >
									(handle->readBufferSize*2)) ?
											handle->amt+msghdr.size :
												handle->readBufferSize*2;

		/* Increases the size of the buffer */
		if((handle->readBuffer = realloc(handle->readBuffer,
										 handle->readBufferSize)) == NULL)
		{
			lodn_errno = LODN_EMEM;
			goto ERROR_HANDLER;
		}
	}

	/* Reads in the message */
	for(totalAmtToRecv=msghdr.size;
		totalAmtToRecv > 0;
		totalAmtToRecv -= amtRecv, handle->amt += amtRecv)
	{

#ifdef USE_SSL
		amtRecv = BIO_read(handle->bio, handle->readBuffer+handle->amt,
						   totalAmtToRecv);
#else
		amtRecv = recv(handle->sfd, handle->readBuffer+handle->amt,
					   totalAmtToRecv, 0);
#endif

		/* Checks for no data or error */
		if(amtRecv <= 0)
		{
			/* no data */
#ifdef USE_SSL
			if(BIO_should_retry(handle->bio))
#else
			if(errno == EAGAIN)
#endif
			{
				/* Waits for more data until timeout expires */
				if(_waitForData(handle->sfd, timeout) != 0)
				{
					/* lodn_errno is set in function */
					goto ERROR_HANDLER;
				}

			/* Error has occurred */
			}else
			{
				lodn_errno = LODN_ESOCKET;
				goto ERROR_HANDLER;
			}

			/* Sets the amount written to zero */
			amtRecv = 0;
		}
	}


	/* Unpacks the message */
	*message = protobuf_c_message_unpack(protobufDescriptorsByType[msghdr.type],
											NULL, msghdr.size,
											handle->readBuffer+
													(handle->amt-msghdr.size));

	/* Checks the result */
	if(*message == NULL)
	{
		lodn_errno = LODN_EPROTOBUF;
		goto ERROR_HANDLER;
	}


	/* Assigns the type */
	*type = msghdr.type;

	/* Clear the buffer */
	handle->amt = 0;

	/* Return success */
	return 0;


	/* Error handler */
	ERROR_HANDLER:

		/* Return failure */
		return -1;
}


int _lodn_sendMsg(lodn_session_handle_t handle, unsigned int type,
					const ProtobufCMessage* message)
{
	/* Declarations */
	MessageHeader msghdr;
	unsigned char *buffer = NULL;
	ssize_t totalAmtToSend;
	ssize_t amtSent;
	ssize_t offset;
	fd_set writefds;


	/* Gets the size of the message */
	msghdr.size = protobuf_c_message_get_packed_size(message);

	/* Assigns the type */
	msghdr.type = type;


	/* Locks the send mutex */
	if(pthread_mutex_lock(&(handle->sendLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

	/* Allocates a buffer for the exchange */
	if((buffer = malloc(PROTOCOL_HEADER_SIZE+msghdr.size)) == NULL)
	{
		lodn_errno = LODN_EMEM;
		goto ERROR_HANDLER;
	}

	/* Serializes the header and message into the buffer */
	lodn_MessageHeader_serialize(&msghdr, buffer);
	protobuf_c_message_pack(message, buffer+PROTOCOL_HEADER_SIZE);

	/* Sends the message */
	for(totalAmtToSend=PROTOCOL_HEADER_SIZE+msghdr.size, offset=0;
		totalAmtToSend > 0;
		totalAmtToSend -= amtSent, offset += amtSent)
	{

#ifdef USE_SSL
		amtSent = BIO_write(handle->bio, buffer+offset, totalAmtToSend);
#else
		amtSent = send(handle->sfd, buffer+offset, totalAmtToSend, 0);
#endif

		/* Detects error or unable to send due to nonblocking */
		if(amtSent <= 0)
		{
			/* Nonblocking */
#ifdef USE_SSL
			if(BIO_should_retry(handle->bio))
#else
			if(errno == EAGAIN || errno == EWOULDBLOCK)
#endif
			{
				/* Fills in the fdset */
				FD_ZERO(&writefds);
				FD_SET(handle->sfd, &writefds);

				/* Waits until it is ok to write to the socket */
				if(select(handle->sfd+1, NULL, &writefds, NULL, NULL) == -1)
				{
					lodn_errno = LODN_ESOCKET;
					goto ERROR_HANDLER;
				}

			/* Error */
			}else
			{
				lodn_errno = LODN_ESOCKET;
				goto ERROR_HANDLER;
			}

			/* Resets the amt sent */
			amtSent = 0;
		}
	}

	/* unlocks the send mutex */
	if(pthread_mutex_unlock(&(handle->sendLock)) != 0)
	{
		lodn_errno = LODN_EGENERIC;
		goto ERROR_HANDLER;
	}

	/* Free resources */
	free(buffer);


	/* Return successfully */
	return 0;


	/* Error Handler */
	ERROR_HANDLER:

		/* unlocks the send mutex */
		if(pthread_mutex_unlock(&(handle->sendLock)) != 0)
		{
			lodn_errno = LODN_EGENERIC;
			goto ERROR_HANDLER;
		}

		/* Free resources */
		if(buffer != NULL)
		{
			free(buffer);
		}

		return -1;
}
