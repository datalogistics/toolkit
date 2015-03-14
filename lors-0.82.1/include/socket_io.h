#ifndef SOCKET_IO_H
#define SOCKET_IO_H

#define SOCK_SUCCESS 1
#define SOCK_FAIL  0

#define PING_INTERVAL 25 //In sec

#include <websocket.h>
#include <dllist.h>
#include <jansson.h>
#include <time.h>

typedef enum _Conn_status{
	CONN_CONNECTED,
	CONN_DISCONNECTED,
	CONN_ERROR,
	CONN_CLOSE,
	CONN_WAITING
} Conn_status;

typedef enum _Event_type{
	PERI_DOWNLOAD_REGISTER,
	PERI_DOWNLOAD_CLEAR,
	PERI_DOWNLOAD_PUSH_DATA
} Event_type;

typedef struct _socket_io_handler{
	struct libwebsocket_context *context;
	struct libwebsocket *wsi;
	Conn_status status;
	char *session_id;
	char *server_add;
	pthread_t emitter;
	pthread_mutex_t m_lock;
	Dllist job_list;
	int num_job;
	time_t last_ping;
} socket_io_handler;

typedef enum _Msg_type{
	SOCK_JSON,
	SOCK_BINARY,
	SOCK_STRING
} Msg_type;

typedef struct _socket_io_msg{
	char *msg;
	Event_type type;
	Msg_type m_type;
} socket_io_msg;

extern int socket_io_init(socket_io_handler *handle, const char *host, const char *session_id);
extern socket_io_msg* socket_io_get_msg(socket_io_handler *handle);
extern int socket_io_send_register(socket_io_handler *handle, char *filename, size_t size, int conn);
extern int socket_io_send_clear(socket_io_handler *handle);
extern int socket_io_send_push(socket_io_handler *handle, char *host,  size_t offset, size_t len);
extern int socket_io_close(socket_io_handler *handle);
extern char *socket_io_get_event_name_from_type(Event_type type);
extern void socket_io_convert_to_protocol_msg(socket_io_msg *msg, char **buff, size_t *len);
#endif
