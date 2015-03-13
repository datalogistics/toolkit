#include <websocket.h>
#include <socket_io.h>
#include <string.h>

#define ERROR(format, ...) fprintf(stderr, format "\n", ##__VA_ARGS__)


int callback_socket_io(struct libwebsocket_context *context,
					   struct libwebsocket *wsi,
					   enum libwebsocket_callback_reasons reason, void *user,
					   void *in, size_t len)
{
	socket_io_handler *handle = libwebsocket_context_user(context);

    switch(reason){
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		handle->status = CONN_ERROR;
		fprintf(stderr, "connection error \n");
		break;

    case LWS_CALLBACK_CLIENT_ESTABLISHED:
		handle->status = CONN_CONNECTED;
        fprintf(stderr, "Connection Established \n");

        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        fprintf(stderr, "There is something to recieve\n");
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        fprintf(stderr, "if u want u can write \n");
        break;

    case LWS_CALLBACK_CLOSED:
		handle->status = CONN_CLOSE;
        fprintf(stderr, "connection closed\n");
        break;
    }
    return 0;
}

static struct libwebsocket_protocols protocols[] = {
    /* first protocol must always be HTTP handler */
    {
        "socket_io",   // name
        callback_socket_io, // callback
        0              // per_session_data_size
		},
	{ NULL, NULL, 0, 0 } /* end */
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
