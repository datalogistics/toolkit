#include <socket_io.h>
#include <stdlib.h>
#include "url.h"
#include <unistd.h>
#include <math.h>

#ifdef DEBUG 
#define LOG(format, args...) fprintf(stderr,format, args...); fprintf("\n")
#else
#define LOG(format, args...)

		
#define MAX_JOBS 100





int socket_io_init(socket_io_handler *handle ){
	
	struct parsed_url *URL;
	
	if(handle->server_add == NULL){
		fprintf(stderr, "Server address is NULL \n");
		return SOCK_FAIL;
	}
	
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
	
	cellophane_set_debug(&handle->client, DEBUG_DETAILED);
	cellophane_io(&handle->client, URL->scheme, URL->host, atoi(URL->port));
	if(cellophane_io_connect(&handle->client) != 1){
		fprintf(stderr, "Failed to connect : %s%s:%d", URL->scheme, URL->host, atoi(URL->port));
		return SOCK_FAIL;
	}
	
	LOG("Connected successfully to %s", handle->server_add);

	pthread_mutex_init(&handle->m_lock, NULL);
	pthread_cond_init(&handle->cond_emitter, NULL);
	pthread_cond_init(&handle->cond_insert, NULL);
	handle->num_job = 0;
	handle->stop_emitter = 0;
	handle->job_list = new_dllist();
	LOG("Starting keepAlive thread");
	pthread_create(&handle->emitter, NULL, socket_io_emit_thread, (void *)handle);

	LOG("Starting emitter thread");
	pthread_create(&handle->emitter, NULL, socket_io_emit_thread, (void *)handle);
}

int insert_into_queue(socket_io_handler *handle, socket_io_msg *msg){
	if(msg == NULL){
		fprintf(stderr, "NO message to queue\n");
		return SOCK_FAIL;
	}

	pthread_mutex_lock(&handle->m_lock);
	if(handle->num_job >= MAX_JOBS){
		fprintf(stderr, "Queue is full, can not append message. Putting it on floor ... \n");
		pthread_mutex_unlock(&handle->m_lock);
		return SOCK_FAIL;
	}
	//while (handle->num_job >= MAX_JOBS)
	//pthread_cond_wait(&handle->cond_insert, &handle->m_lock);
	
	dll_append(handle->job_list, new_jval_v(msg));
	handle->num_job++;
	
	pthread_cond_signal(&handle->cond_emitter);
	pthread_mutex_unlock(&handle->m_lock);
	
	LOG("Message appended successfully !");
	return SOCK_SUCCESS;

}

void socket_io_emit_thread(socket_io_handler *handle){

	Dllist node = NULL;
	socket_io_msg *io_msg;
	char *event_name;
	
	while(!handle->stop_emitter){
		node = NULL;
		io_msg = NULL;
		event_name = NULL;

		pthread_mutex_lock(&handle->m_lock);
		while (handle->num_job <= 0)
			pthread_cond_wait(&handle->cond_emitter, &handle->m_lock);
	
		node = dll_first(handle->job_list);
		if(node == NULL){
			pthread_mutex_unlock(&handle->m_lock);
			continue;
		}
		io_msg = (socket_io_msg*)jval_v(node->val);
		handle->num_job--;
	
		//pthread_cond_signal(&handle->cond_insert);
		pthread_mutex_unlock(&handle->m_lock);
		
		event_name  = socket_io_get_event_name_from_type(io_msg->type);

		if((event_name != NULL) && (io_msg->msg != NULL)){
			cellophane_emit(&handle->client, event_name, io_msg->msg, "");
			free(event_name);
			free(io_msg->msg);
		}
		
		free(io_msg);
	}
}

int socket_io_close(socket_io_handler *handle){
	int count = 1;
	Dllist ptr = NULL;
	socket_io_msg *io_msg;
	while(handle->num_job != 0 && count < 3){
		LOG("Waiting for job queue to complete ");
		usleep(1000 * (int)pow((double)count, 2));
		count++;
	}
	
	handle->stop_emitter = 1;
	pthread_cond_signal(&handle->cond_emitter);
	handle->client.keep_alive_flag = 0;
	
	pthread_join(&handle->emitter, NULL);
	pthread_join(&handle->keepAlive, NULL);

	close(handle->client.fd);
	handle->client.fd_alive = 0;

	if(handle->num_job != 0){
		dll_traverse(ptr, handle->job_list){
			io_msg = (socket_io_msg*)jval_v(ptr->val);
			if(io_msg != NULL && io_msg->msg != NULL){
				free(io_msg->msg);
				free(io_msg);
			}
		};
	}

	free_dllist(handle->job_list);
}


char *socket_io_get_event_name_from_type(Event_type type){

	char *event_name;
	char *ret;

	switch(type){
	case PERI_DOWNLOAD_REGISTER:
		event_name = "peri_download_register";
		break;
		
	case PERI_DOWNLOAD_PUSH_DATA:
		event_name = "peri_download_pushData";
		break;
		
	case PERI_DOWNLOAD_CLEAR:
		event_name = "peri_download_clear";
		break;
		
	default:
		return NULL;
	}

	return strdup(event_name);
}

void socket_io_keepAlive_thread(socket_io_handler *handle){
	cellophane_keepAlive(&handle->client);
}


int socket_io_send_register(socket_io_handler *handle, char *filename, size_t size, int conn){
	
	if(filename == NULL){
		fprintf(stderr, "File name is required\n");
		return SOCK_FAIL;
	}
	
	if(handle != NULL && handle->session_id == NULL){
		fprintf(stderr, "Session id not found\n");
		return SOCK_FAIL;
	}

	json_t *json_obj;
	char *dump;

	json_object_set(json_obj, "hashId" ,json_string(handle->session_id));
	json_object_set(json_obj, "filename", json_string(filename));
	json_object_set(json_obj, "totalSize", json_integer(size));
	json_object_set(json_obj, "connections", json_integer(conn));

	dump = json_dumps(json_obj, JSON_INDENT(1));
	if(dump == NULL){
		fprintf(stderr, "Register JSON dump failed \n");
		return SOCK_FAIL;
	}
	
	LOG("Event : PERI_DOWNLOAD_REGISTER , DUMP : %s", dump);
	
	socket_io_msg *io_msg = (socket_io_msg*)malloc(sizeof(socket_io_msg));
	io_msg->type = PERI_DOWNLOAD_REGISTER;
	io_msg->msg = dump;
	
	insert_into_queue(handle, io_msg);
	return SOCK_SUCCESS;
}

int socket_io_send_clear(socket_io_handler *handle){
	
	json_t *json_obj;
	char *dump;

	if(handle != NULL && handle->session_id == NULL){
		fprintf(stderr, "Session id not found\n");
		return SOCK_FAIL;
	}
	
	//NOTE: Need for information on clear message
	json_object_set(json_obj, "hashId" ,json_string(handle->session_id));
	dump = json_dumps(json_obj, JSON_INDENT(1));
	if(dump == NULL){
		fprintf(stderr, "clear JSON dump failed \n");
		return SOCK_FAIL;
	}
	
	LOG("Event : PERI_DOWNLOAD_CLEAR, DUMP : %s", dump);
	
	socket_io_msg *io_msg = (socket_io_msg*)malloc(sizeof(socket_io_msg));
	io_msg->type = PERI_DOWNLOAD_CLEAR;
	io_msg->msg = dump;
	
	insert_into_queue(handle, io_msg);
	return SOCK_SUCCESS;
}

int socket_io_push(socket_io_handler *handle, char *host, size_t offset, size_t len){

	if(handle != NULL && handle->session_id == NULL){
		fprintf(stderr, "Session id not found\n");
		return SOCK_FAIL;
	}
	
	json_t *json_obj;
	char *dump;

	//NOTE: Need for information on clear message
	json_object_set(json_obj, "hashId" ,json_string(handle->session_id));
	json_object_set(json_obj, "ip" ,json_string(host));
	json_object_set(json_obj, "offset", json_integer(offset));
	json_object_set(json_obj, "progress", json_integer(len));
	dump = json_dumps(json_obj, JSON_INDENT(1));
	if(dump == NULL){
		fprintf(stderr, "clear JSON dump failed \n");
		return SOCK_FAIL;
	}
	
	LOG("Event : PERI_DOWNLOAD_PUSH_DATA, DUMP : %s", dump);
	
	socket_io_msg *io_msg = (socket_io_msg*)malloc(sizeof(socket_io_msg));
	io_msg->type = PERI_DOWNLOAD_PUSH_DATA;
	io_msg->msg = dump;
	
	insert_into_queue(handle, io_msg);
	return SOCK_SUCCESS;
}

#endif
