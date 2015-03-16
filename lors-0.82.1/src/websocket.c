#include <websocket.h>
#include <socket_io.h>
#include <string.h>

//#define DEBUG 

#ifdef DEBUG 
#define LOG(format, ...) fprintf(stderr, "[LOG]" format "\n", ##__VA_ARGS__)
#else
#define LOG(format, ...) 
#endif

#define ERROR(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)


int callback_socket_io(struct libwebsocket_context *context,
					   struct libwebsocket *wsi,
					   enum libwebsocket_callback_reasons reason, void *user,
					   void *in, size_t len)
{
	char              *buff;
	char              *write_buff;
	size_t             msgLen = 0;
	socket_io_msg     *msg = NULL;
	socket_io_handler *handle = libwebsocket_context_user(context);
	char              *ping_msg = "2";

    switch(reason){
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		handle->status = CONN_ERROR;
		LOG("connection error ");
		break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
		handle->status = CONN_CONNECTED;
        LOG( "Connection Established ");
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        LOG( "There is something to recieve");
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        LOG( "if u want u can write ");
		// get msg from queue
		msg = socket_io_get_msg(handle);
		if(msg != NULL){
			// convert it into protocol msg 
			socket_io_convert_to_protocol_msg(msg, &buff, &msgLen);
			if(msgLen){
				// libwebsocket require extra pre and post buffer to put header and trailar in place
				write_buff = malloc(msgLen + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING);
				memcpy((write_buff + LWS_SEND_BUFFER_PRE_PADDING), buff, msgLen);
				libwebsocket_write(wsi, (write_buff + LWS_SEND_BUFFER_PRE_PADDING), msgLen, LWS_WRITE_TEXT);
				free(write_buff);
				free(buff);
				handle->last_ping = time(NULL);
			}
		}
		if((time(NULL) - handle->last_ping) > PING_INTERVAL){
			//fprintf(stderr, "Sending Ping packet\n");
			write_buff = malloc(strlen(ping_msg) + LWS_SEND_BUFFER_PRE_PADDING + LWS_SEND_BUFFER_POST_PADDING);
			memcpy((write_buff + LWS_SEND_BUFFER_PRE_PADDING), ping_msg, strlen(ping_msg));
			libwebsocket_write(wsi, (write_buff + LWS_SEND_BUFFER_PRE_PADDING), strlen(ping_msg), LWS_WRITE_TEXT);
			free(write_buff);
			handle->last_ping = time(NULL);
		}
		
        break;

    case LWS_CALLBACK_CLOSED:
		handle->status = CONN_CLOSE;
        LOG( "connection closed");
        break;
    }
    return 0;
}

static struct libwebsocket_protocols protocols[] = {
    {
        "socket_io",        // name
        callback_socket_io, // callback
        0                   // per_session_data_size
	},
	{ NULL, NULL, 0, 0 }    // end marker
};


int websocket_init(struct libwebsocket_context **context, struct libwebsocket **wsi, char *host, char *path, int port, void *handle ){
	
	struct libwebsocket_context *c;
	struct libwebsocket *w;
	const char *interface = NULL;
    const char *cert_path = NULL;
    const char *key_path = NULL;
   	int use_ssl = 0;
    int opts = 0;
	int ietf_version = -1; 
	struct lws_context_creation_info info;

	if(handle == NULL){
		ERROR("Socket handler not found");
		return WEBSOCKET_FAIL;
	}
	
	if(host == NULL){
		ERROR("Host not found");
		return WEBSOCKET_FAIL;
	}

    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.iface = interface;
    info.protocols = protocols;
    info.extensions = libwebsocket_get_internal_extensions();
    info.ssl_cert_filepath = NULL;
    info.ssl_private_key_filepath = NULL;
    info.gid = -1;
    info.uid = -1;
    info.options = opts;
	info.user = handle;
	info.ka_time = 25000;
	info.ka_probes = 3;
	info.ka_interval = 500;

	// set log level to debug so that we dont get notice messages
	lws_set_log_level(LLL_ERR, NULL);

	// create libwebsockets context
	*context = libwebsocket_create_context(&info);
    if (context == NULL) {
        ERROR("libwebsocket init failed");
        return WEBSOCKET_FAIL;
    }
	
	// connect to handle
	*wsi = libwebsocket_client_connect(*context, host, port, use_ssl, path, host, host, NULL, ietf_version);
	if(wsi == NULL){
		ERROR("libwebsocket connect failed");
		return WEBSOCKET_FAIL;
	}
	
	return WEBSOCKET_SUCCESS;
}

int websocket_close(struct libwebsocket_context *context){
	libwebsocket_context_destroy(context);
	return WEBSOCKET_SUCCESS;
}
