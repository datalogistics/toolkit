#include <socket_io.h>
#include <stdlib.h>
#include "url.h"
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <string.h>

//#define DEBUG 1

#ifdef DEBUG 
#define LOG(format, ...) fprintf(stderr, "DEBUG %s:%d: " format "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define LOG(formar, ...)
#endif
		
#define MAX_JOBS 1000000
#define PING_INTERVAL 25 //In sec

void static socket_io_emit_thread(socket_io_handler *handle);
char *socket_io_get_event_name_from_type(Event_type type);

int socket_io_init(socket_io_handler *handle, const char *host, const char *session_id ){
	
	socket_io_handler h;
	struct parsed_url *URL;
	handle->num_job = 0;
	handle->job_list = NULL;
	handle->status = CONN_WAITING;

	if(host == NULL){
		fprintf(stderr, "Server address is NULL \n");
		return SOCK_FAIL;
	}
	if(session_id == NULL){
		fprintf(stderr, "Session id is NULL \n");
		return SOCK_FAIL;
	}

	handle->server_add = strdup(host);
	handle->session_id = strdup(session_id);
	
	URL = parse_url(handle->server_add);
	if(URL == NULL){
		fprintf(stderr,"Failed to parse URL \n");
		return SOCK_FAIL;
	}
	
	LOG("protocol : %s Host : %s Port : %d ", URL->scheme, URL->host, atoi(URL->port));
	
	if(strstr(URL->scheme, "http") == NULL){
		fprintf(stderr, "Dont have support for %s \n", URL->scheme);
		return SOCK_FAIL;
	}else{
		URL->scheme = realloc(URL->scheme, 8);
		strcpy(URL->scheme, "http://");
	}

	if(URL->path != NULL){
		fprintf(stderr, "Socket IO do not support namespace in this version\n");
		return SOCK_FAIL;
	}
	
	if(websocket_init(&handle->context, &handle->wsi, URL->host, "/socket.io/?transport=websocket", atoi(URL->port), (void *) handle) != WEBSOCKET_SUCCESS){
		fprintf(stderr, "Failed to Init  : %s%s:%d \n", URL->scheme, URL->host, atoi(URL->port));
		return SOCK_FAIL;
	}

	while (handle->status == CONN_WAITING) {
        libwebsocket_service(handle->context, 50);
    }
	
	if(handle->status != CONN_CONNECTED){
		fprintf(stderr, "Failed to connect  : %s%s:%d \n", URL->scheme, URL->host, atoi(URL->port));
		return SOCK_FAIL;
	}

	LOG("Connected successfully to %s", handle->server_add);
	handle->last_ping = time(NULL);
	LOG("Initialized Queue");
	pthread_mutex_init(&handle->m_lock, NULL);
	handle->job_list = new_dllist();

	LOG("Starting emitter thread");
	pthread_create(&handle->emitter, NULL, socket_io_emit_thread, (void *)handle);

	return SOCK_SUCCESS;
}

void static socket_io_emit_thread(socket_io_handler *handle){
	
	while(handle->status == CONN_CONNECTED){
		libwebsocket_service(handle->context, 50);
		//fprintf(stderr, " Last ping : %lld , current time : %lld , diff : %lld \n ", handle->last_ping, time(NULL), (time(NULL) - handle->last_ping));
		if(handle->num_job > 0){ 
			libwebsocket_callback_on_writable( handle->context, handle->wsi);
		}else if((time(NULL) - handle->last_ping) > PING_INTERVAL){
			libwebsocket_callback_on_writable( handle->context, handle->wsi);
		}
	}
}
int socket_io_put_msg(socket_io_handler *handle, socket_io_msg *msg){
	if(msg == NULL){
		fprintf(stderr, "NO message to queue\n");
		return SOCK_FAIL;
	}

	if(handle->job_list == NULL){
		fprintf(stderr, "Queue is not initialized\n");
		return SOCK_FAIL;
	}

	pthread_mutex_lock(&handle->m_lock);
	if(handle->num_job >= MAX_JOBS){
		fprintf(stderr, "Queue is full, can not append message. Putting it on floor ... \n");
		free(msg);
		return SOCK_FAIL;
	}

	dll_append(handle->job_list, new_jval_v(msg));
	handle->num_job++;
	
	pthread_mutex_unlock(&handle->m_lock);
	LOG("Message appended successfully !");

	return SOCK_SUCCESS;
}

socket_io_msg* socket_io_get_msg(socket_io_handler *handle){

	Dllist node = NULL;
	socket_io_msg *io_msg = NULL;
	
	if(handle == NULL){
		fprintf(stderr, "Socket handle is NULL\n ");
		return NULL;
	}

	pthread_mutex_lock(&handle->m_lock);
	
	if( handle->num_job > 0 ){
		node = dll_first(handle->job_list);
		if(node == NULL){
			pthread_mutex_unlock(&handle->m_lock);
			return NULL;
		}
		io_msg = (socket_io_msg*)jval_v(node->val);
		handle->num_job--;
		dll_delete_node(node);
	}

	pthread_mutex_unlock(&handle->m_lock);
	
	return io_msg;
}

void socket_io_convert_to_protocol_msg(socket_io_msg *msg, char **buff, size_t *len){

	char *event_name;
	int msgLen;

	if(msg == NULL && msg->msg == NULL){
		fprintf(stderr, "Message is NULL \n");
		buff = NULL;
		*len = 0;
		return;
	}
	
	event_name  = socket_io_get_event_name_from_type(msg->type);
	
	if(event_name == NULL){
		fprintf(stderr, "Event Name is not valid \n");
		buff = NULL;
		*len = 0;
		return;
	}

	switch(msg->m_type){
	case SOCK_JSON:
		msgLen = 8 + ((strlen(event_name) + strlen(msg->msg)) * sizeof(char));
		*buff = malloc(msgLen);
		snprintf(*buff, msgLen, "42[\"%s\",%s]", event_name, msg->msg);
		*len = msgLen - 1;  // end char is not part of buffer
		break;
		
	case SOCK_STRING:
		msgLen = 10 + ((strlen(event_name) + strlen(msg->msg)) * sizeof(char));
		*buff = malloc(msgLen);
		snprintf(*buff, msgLen, "42[\"%s\",\"%s\"]", event_name, msg->msg);
		*len = msgLen - 1; // end char is not part of buffer
		break;

	case SOCK_BINARY:
		fprintf(stderr, "No binary support yet  \n");
		buff = NULL;
		break;

	default:
		fprintf(stderr, "Message Type is not valid \n");
		buff = NULL;
		*len = 0;
	}
	free(msg->msg);
	free(msg);
	free(event_name);

}

int socket_io_close(socket_io_handler *handle){
	int count = 1;
	Dllist ptr = NULL;
	socket_io_msg *io_msg;

	while(handle->num_job != 0 && handle->status == CONN_CONNECTED){
		LOG("# Jobs %d , Waiting for job queue to complete ", handle->num_job);
		usleep(1000 * (int)pow((double)count, 2));
		count++;
	}
	
	LOG("Stopping emitter");
	handle->status = CONN_DISCONNECTED;
	pthread_join(handle->emitter, NULL);

	LOG("Closing ");
	
	if(handle->num_job != 0){
		dll_traverse(ptr, handle->job_list){
			io_msg = (socket_io_msg*)jval_v(ptr->val);
			if(io_msg != NULL && io_msg->msg != NULL){
				free(io_msg->msg);
				free(io_msg);
			}
		};
	}
	
	if(handle->job_list != NULL)
		free_dllist(handle->job_list);
	
	if(handle->server_add)
		free(handle->server_add);
	
	if(handle->session_id)
		free(handle->session_id);

	websocket_close(handle->context);
}


char *socket_io_get_event_name_from_type(Event_type type){

	char *event_name;
	char *ret;

	switch(type){
	case PERI_DOWNLOAD_REGISTER:
		event_name = "peri_download_register";
		break;
		
	case PERI_DOWNLOAD_PUSH_DATA:
		event_name = "peri_download_pushdata";
		break;
		
	case PERI_DOWNLOAD_CLEAR:
		event_name = "peri_download_clear";
		break;
		
	default:
		return NULL;
	}

	return strdup(event_name);
}

int socket_io_send_register(socket_io_handler *handle, char *filename, size_t size, int conn){
	
	char buff[1000];
	
	if(filename == NULL){
		fprintf(stderr, "File name is required\n");
		return SOCK_FAIL;
	}
	
	if(handle != NULL && handle->session_id == NULL){
		fprintf(stderr, "Session id not found\n");
		return SOCK_FAIL;
	}

	json_t *json_obj = json_object();
	char *dump;

	LOG("hash_id : %s", handle->session_id );
	LOG("filename : %s", filename);
	LOG("totalsize : %lld", size);
	LOG("Num of conn : %d" ,conn );

	json_object_set(json_obj, "sessionId" ,json_string(handle->session_id));
	json_object_set(json_obj, "filename", json_string(filename));
	json_object_set(json_obj, "size", json_integer(size));
	json_object_set(json_obj, "connections", json_integer(conn));
	json_object_set(json_obj, "ts", json_integer(time(NULL)));
	dump = json_dumps(json_obj, JSON_COMPACT);
	if(dump == NULL){
		fprintf(stderr, "Register JSON dump failed \n");
		return SOCK_FAIL;
	}
	
	LOG("Event : PERI_DOWNLOAD_REGISTER , DUMP : %s", dump);
	
	socket_io_msg *io_msg = (socket_io_msg*)malloc(sizeof(socket_io_msg));
	io_msg->type = PERI_DOWNLOAD_REGISTER;
	io_msg->m_type = SOCK_JSON;
	io_msg->msg = dump;

	socket_io_put_msg(handle, io_msg);
	return SOCK_SUCCESS;
}

int socket_io_send_clear(socket_io_handler *handle){
	
	json_t *json_obj  = json_object();
	char *dump;

	if(handle != NULL && handle->session_id == NULL){
		fprintf(stderr, "Session id not found\n");
		return SOCK_FAIL;
	}
	
	//NOTE: Need for information on clear message
	json_object_set(json_obj, "sessionId" ,json_string(handle->session_id));
	json_object_set(json_obj, "ts", json_integer(time(NULL)));
	dump = json_dumps(json_obj, JSON_COMPACT);
	if(dump == NULL){
		fprintf(stderr, "clear JSON dump failed \n");
		return SOCK_FAIL;
	}
	
	LOG("Event : PERI_DOWNLOAD_CLEAR, DUMP : %s", dump);
	
	socket_io_msg *io_msg = (socket_io_msg*)malloc(sizeof(socket_io_msg));
	io_msg->m_type = SOCK_JSON;
	io_msg->type = PERI_DOWNLOAD_CLEAR;
	io_msg->msg = dump;
	
	socket_io_put_msg(handle, io_msg);
	return SOCK_SUCCESS;
}

int socket_io_send_push(socket_io_handler *handle, char *host, size_t offset, size_t len){

	if(handle != NULL && handle->session_id == NULL){
		fprintf(stderr, "Session id not found\n");
		return SOCK_FAIL;
	}
	
	json_t *json_obj = json_object();
	char *dump;

	//NOTE: Need for information on clear message
	json_object_set(json_obj, "sessionId" ,json_string(handle->session_id));
 	json_object_set(json_obj, "host" ,json_string(host));
	json_object_set(json_obj, "offset", json_integer(offset));
	json_object_set(json_obj, "length", json_integer(len));
	json_object_set(json_obj, "ts", json_integer(time(NULL)));
	dump = json_dumps(json_obj, JSON_COMPACT);
	if(dump == NULL){
		fprintf(stderr, "clear JSON dump failed \n");
		return SOCK_FAIL;
	}

	LOG("Event : PERI_DOWNLOAD_PUSH_DATA, DUMP : %s", dump);
	
	socket_io_msg *io_msg = (socket_io_msg*)malloc(sizeof(socket_io_msg));
	io_msg->m_type = SOCK_JSON;
	io_msg->type = PERI_DOWNLOAD_PUSH_DATA;
	io_msg->msg = dump;
	
	socket_io_put_msg(handle, io_msg);
	return SOCK_SUCCESS;
}


