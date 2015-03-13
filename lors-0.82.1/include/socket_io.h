#ifndef SOCKET_IO_H
#define SOCKET_IO_H

#define SOCK_SUCCESS 1
#define SOCK_FAIL  0

#include <websocket.h>
#include <dllist.h>
#include <jansson.h>

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
	pthread_t keepAlive;
	pthread_mutex_t m_lock;
	pthread_cond_t cond_emitter;
	pthread_cond_t cond_insert;
	Dllist job_list;
	int num_job;
	int stop_emitter;
} socket_io_handler;

typedef struct _socket_io_msg{
	char *msg;
	Event_type type;
} socket_io_msg;

extern int socket_io_init(socket_io_handler *handle, const char *host, const char *session_id);
//extern void socket_io_emit_thread(socket_io_handler *handle);
//extern void socket_io_keepAlive_thread(socket_io_handler *handle);
extern int socket_io_send_register(socket_io_handler *handle, char *filename, size_t size, int conn);
extern int socket_io_send_clear(socket_io_handler *handle);
extern int socket_io_send_push(socket_io_handler *handle, char *host,  size_t offset, size_t len);
extern int socket_io_close(socket_io_handler *handle);
extern char *socket_io_get_event_name_from_type(Event_type type);

#endif
