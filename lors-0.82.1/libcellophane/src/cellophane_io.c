/*
 * cellophane.io - simple socket.io client writen in c
 *
 * Copyright (C) 2014 Iker Perez de Albeniz <iker.perez.albeniz@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation:
 *  version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 */

#include <sys/select.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wchar.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <string.h>
#include <unistd.h>
#include <jansson.h>
#include <payload.h>
#include <cellophane_io.h>
#include <md5.h>


/* function prototypes to define later */
int Base64encode_len(int len);
int Base64encode(char *encoded, const char *string, int len);
int cellophane_read_ready(WsHandler * ws_handler);
void cellophane_print_log(WsHandler * ws_handler, enum cellophane_log_type logtype, enum cellophane_debug_level level,  char * format ,...);
void cellophane_reset_default_events(WsHandler * ws_handler);
void cellophane_trigger_default_events(WsHandler * ws_handler, WsEventInfo info);
void cellophane_reconect(WsHandler * ws_handler);
void cellophane_compute_md5(char *str, unsigned char digest[16]);
int cellophane_number_of_message(char * buffer, int n);
int cellophane_find_on_char_array(char ** tokens , char * key);
char** cellophane_str_split(char* a_str, const char a_delim);
char * cellophane_do_web_request(WsHandler * ws_handler);
size_t static cellophane_write_callback_func(void *buffer, size_t size, size_t nmemb, void *userp);

struct block{
	char *mem;
	size_t size;
};

int findn(int num)
{
    int n = 0;
    while(num) {
        num /= 10;
        n++;
    }
    return n;
}

void cellophane_io(WsHandler * ws_handler, char * tcp_protocol, char * address, int port ){
    cellophane_new(ws_handler, tcp_protocol, address, port, "socket.io", 1, 0, 1, 0);
}


int cellophane_io_connect(WsHandler * ws_handler){
	return cellophane_init(ws_handler, ws_handler->keep_alive_flag);
}


/** 
 * NOTE : As mentioned here https://stackoverflow.com/questions/23987640/socket-io-handshake-return-error-transport-unknown/25289378#25289378
 *  How to communicate with socket.io v1.0 server:
 *   1) GET http[s]://host:port/socket.io/?transport=polling
 *   2) Server responds a JSON config string in response body with some unknown characters as header.
 *      Warning for c-style char* useres: this string begins with '\0'.
 *   3) The string looks like: \0xxxx {"sid":"xxx", "upgrades":["websocket","polling",..], pingInterval:xxxx, pingTimeout:xxxx}.
 *        sid: seesion id for websocket connection.
 *        upgrades: available transport method. Please make sure "websocket" is one of them.
 *        pingInterval & pingTimeout: ping server every pingInterval and check for pong within pingTimeout.
 *   4) Establish websocket connection at ws[s]://host:port/socket.io/?transport=websocket&sid=sid
 *   5) Send string "52" to socket.io server upon successful connection.
 *   6) Listen on server message, waiting for string "40" to confirm websocket link between client and server.
 */

/* trans_protocol_query should look like "tranpost=polling" */

void cellophane_new(WsHandler * ws_handler, char * tcp_protocol , char * address, int port, char * path, int protocol, int read, int  checkSslPeer, enum cellophane_debug_level debug){

	char *trans_protocol_query = "transport=polling";
    int url_size = (int)(strlen(tcp_protocol) + strlen(address) + strlen(path) + findn(port) + strlen(trans_protocol_query) ) + 4;
    ws_handler->socketIOUrl = malloc(url_size);
    sprintf(ws_handler->socketIOUrl,"%s%s:%d/%s/?%s", tcp_protocol, address, port, path, trans_protocol_query);
	//fprintf(stderr, "URL : %s \n", ws_handler->socketIOUrl);
    ws_handler->serverHost = address;
    ws_handler->serverPort = port;
    int path_size = (int)(strlen(path) ) + 1;
    ws_handler->serverPath = malloc(path_size);
    sprintf(ws_handler->serverPath, "/%s", path);
    ws_handler->read = read;
    ws_handler->checkSslPeer = checkSslPeer;
    ws_handler->lastId = 0;
    ws_handler->keep_alive_flag = 0;
    ws_handler->checkSslPeer = 1;
    ws_handler->handshakeTimeout = 1000;
    if(ws_handler->debug_level == NULL){
        ws_handler->debug_level = debug;
    }
    ws_handler->user_events_len = 0;
    cellophane_reset_default_events(ws_handler);

}

void cellophane_reset_default_events(WsHandler * ws_handler){

    char * event_names[] = {"anything","connect","connecting","disconnect","connect_failed","error","message","reconnect_failed","reconnect","reconnecting"};

    int i;
    for(i=0; i < 10; i++){
        ws_handler->default_events[i].event_name = event_names[i];
        ws_handler->default_events[i].callback_func = NULL;
    }
}


void cellophane_trigger_default_events(WsHandler * ws_handler,  WsEventInfo info){

    int i;
    info.ws_handler= (void *) ws_handler;
    for(i=1; i < 10; i++){
		if( (strncmp(ws_handler->default_events[i].event_name, info.event_name, strlen(info.event_name)) == 0)
			&& (strlen(info.event_name) == strlen(ws_handler->default_events[i].event_name))
			&& ws_handler->default_events[i].callback_func != NULL){
			cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Triggered \"%s\" default event", info.event_name);
			ws_handler->default_events[i].callback_func(info);
        }
    }

    if( ws_handler->default_events[0].callback_func != NULL){
        cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Triggered \"anything\" default event");
        ws_handler->default_events[0].callback_func(info);
    }
}

void cellophane_set_debug(WsHandler * ws_handler, enum cellophane_debug_level debug){
    ws_handler->debug_level = debug;
}

void cellophane_print_log(WsHandler * ws_handler, enum cellophane_log_type logtype, enum cellophane_debug_level level,  char * format ,...){

    va_list args;
    va_start(args, format);
    char * colored_format = (char *) malloc(52+strlen(format));
    bzero(colored_format,16+strlen(format));

    time_t timer;
    char buffer[20];
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);
    strftime(buffer, 25, "%Y-%m-%d %H:%M:%S", tm_info);

    if(ws_handler->debug_level >= level){

        switch(logtype){
		case LOG_ERROR:{
			sprintf(colored_format,"%s%s  [ERR]  %s%s\n",ERRCOL,buffer,format,CLRCOL);
			vfprintf(stderr,colored_format, args);
			break;
		}
		case LOG_WARNING:{
			sprintf(colored_format,"%s%s  [WAR]  %s%s\n",WARCOL,buffer,format,CLRCOL);
			vfprintf(stderr,colored_format, args);
			break;
		}
		case LOG_FREE:{
			sprintf(colored_format,"%s%s%s",FRECOL,format,CLRCOL);
			vfprintf(stderr,colored_format, args);
			//printf(WARCOL format, ctime(&now) ,__VA_ARGS__);
			break;
		}
		default:{
			sprintf(colored_format,"%s%s  [INF]  %s%s\n",INFCOL,buffer,format,CLRCOL);
			vfprintf(stderr,colored_format, args);
			break;
		}
        }
    }

    va_end(args);

}


int  cellophane_init(WsHandler * ws_handler, int keepalive){

	int ret;
	
    ret = cellophane_handshake(ws_handler);
	if(ret <= 0 ){
		cellophane_print_log(ws_handler, LOG_ERROR, DEBUG_NONE, "Init failed no response from curl");
		return 0;
	}

    return cellophane_connect(ws_handler);

    /*if (keepalive) {
	  cellophane_keepAlive(ws_handler);
	  }*/
	
}


int cellophane_handshake(WsHandler * ws_handler) {

	json_error_t  json_err;
	json_t *json_ret;
	json_t *value;
	json_t *transport;
	void *iter;
	char * res;
	char *key;
	size_t index;

	//fprintf(stderr, "sending web request \n");
	res = cellophane_do_web_request(ws_handler);
	if (res == NULL || strcmp(res,"") == 0){
        return 0;
    }

	cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC, "Curl Response : %s", res);
	json_ret = json_loads(res, 0, &json_err);
	if(json_ret == NULL){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE, "Could not decode trasnport JSON: %d: %s\n", json_err.line, json_err.text);
		return 0;
	}

	free(res);

	cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Json parsed succesfully");
	iter = json_object_iter(json_ret);
	while(iter){
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		
		if(strcmp(key, "sid") == 0){
			ws_handler->session.sid = strdup(json_string_value(value));
			cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DETAILED,"Session id : %s", ws_handler->session.sid);
		}else if(strcmp(key, "upgrades") == 0){
			index = json_array_size(value);
			if(index > 0){
				ws_handler->session.supported_transports = (char **) malloc(index * sizeof(char*));
				json_array_foreach(value, index, transport){
					ws_handler->session.supported_transports[index] = strdup(json_string_value(transport));
				}
			}
		}else if(strcmp(key,"pingInterval") == 0){
			ws_handler->session.heartbeat_timeout = json_integer_value(value);
		   	cellophane_print_log(ws_handler, LOG_INFO, DEBUG_DETAILED, "Heartbit timeout : %d", ws_handler->session.heartbeat_timeout);
		}else if(strcmp(key, "pingTimeout") == 0){
			ws_handler->session.connection_timeout = json_integer_value(value);
			cellophane_print_log(ws_handler, LOG_INFO, DEBUG_DIAGNOSTIC,"Connection timeout : %d ", ws_handler->session.connection_timeout);
		}

		iter = json_object_iter_next(json_ret, iter);
	}

    if(!cellophane_find_on_char_array(ws_handler->session.supported_transports, "websocket")){
        return 0;
	}

	cellophane_print_log(ws_handler, LOG_INFO, DEBUG_DIAGNOSTIC,"websocket transport is supported  ");
    return 1;
}


int cellophane_connect(WsHandler * ws_handler) {

    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buff[256];

    WsEventInfo info;
    info.event_name = "connecting";
    info.message = "Connecting";
    cellophane_trigger_default_events(ws_handler, info);

    portno = ws_handler->serverPort;
	cellophane_print_log(ws_handler,LOG_INFO,DEBUG_NONE,"Creating socket");
    ws_handler->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ws_handler->fd < 0)	{
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR opening socket");
		ws_handler->fd_alive = 0;
		return 0;
	}

    server = gethostbyname(ws_handler->serverHost);

    if (server == NULL) {
        cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR, no such host");
        ws_handler->fd_alive = 0;
        return 0;
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(ws_handler->fd,(const struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR connecting");
		ws_handler->fd_alive = 0;
		return 0;
	}

    ws_handler->fd_alive = 1;
	
    char * key = cellophane_generateKey(16);
    int out_len = (int)(strlen(ws_handler->serverPath) + strlen(ws_handler->session.sid) + strlen(ws_handler->serverHost) + findn(ws_handler->serverPort) + strlen(key) ) + 152;
    char * out = malloc(out_len);
    bzero(out, out_len);
	
    sprintf(out, "GET %s/?transport=websocket&sid=%s HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nOrigin: *\r\n\r\n",
			ws_handler->serverPath,
			ws_handler->session.sid,
			ws_handler->serverHost,
			ws_handler->serverPort,
			key);
    n = send(ws_handler->fd,out,out_len, 0);
	cellophane_print_log(ws_handler,LOG_INFO,DEBUG_NONE,"Sending HTTP req(%d/%d) : \n%s", n, strlen(out), out);
    if (n < 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR writing to socket");
		ws_handler->fd_alive = 0;
		goto bail_out;
	}

    bzero(buff,256);
    n = recv(ws_handler->fd,buff,255, 0);
    if (n < 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR reading from socket");
		ws_handler->fd_alive = 0;
		goto bail_out;
	}

    ws_handler->buffer = realloc(NULL, strlen(buff));
    strcpy(ws_handler->buffer, buff);
	cellophane_print_log(ws_handler,LOG_INFO, DEBUG_DETAILED, "Websocket response : %s", ws_handler->buffer);
    if(strncmp (ws_handler->buffer,"HTTP/1.1 101",12) != 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"Unexpected Response. Expected HTTP/1.1 101..");
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"Aborting...");
		ws_handler->fd_alive = 0;
		goto bail_out;
	}
	
	// send UPGRADE EVENT
	cellophane_send(ws_handler, EIO_UPGRADE, TYPE_EVENT, "", "", "", 0);
	
	char * m_payload;
    char ** m_payload_a;

    if(strstr(ws_handler->buffer, "40") == NULL){

        while(!cellophane_read_ready(ws_handler)){
            usleep(100*1000);
        }
        int msg_number = 0;
        m_payload_a = cellophane_read(ws_handler,&msg_number);
        m_payload = *(m_payload_a);
		if(m_payload == NULL){
			goto bail_out;
		}
 
	}else{
        m_payload = strstr(ws_handler->buffer,"40");
    }

    if(strncmp (m_payload,"40", 3) != 0){
        cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"Socket.io did not send connect response.");
        cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"Aborting...");
        ws_handler->fd_alive = 0;
        goto bail_out;
    }else{
        cellophane_print_log(ws_handler,LOG_INFO,DEBUG_MINIMAL,"Conection stablished...");
        ws_handler->fd_alive = 1;
    }

    ws_handler->heartbeatStamp = time(NULL);

	WsEventInfo info_f;
	info_f.event_name = "connect";
	info_f.message = "connect";
	cellophane_trigger_default_events(ws_handler, info_f);

	return 1;

 bail_out:
	if(key != NULL)
		free(key);
	close(ws_handler->fd);
	//free(m_payload_a);
	//free(m_payload);
	return 0;
	  
}

char * cellophane_generateKey(int length) {

	char * key;
	int c = 0;
	char * tmp = malloc(16);
	unsigned char * digest = malloc(16);
	char random_number[16];
	strcpy(tmp, "");

	while( (c * 16) < length) {
		tmp = realloc(tmp,(c+1)*16);
		srand(time(NULL));
		sprintf(random_number,"%d",rand());
		cellophane_compute_md5((char *)random_number,digest);
		strcat(tmp, (const char *)digest);
		c++;
	}


	key = malloc(Base64encode_len(c*16));
	Base64encode(key, tmp, c*16);
	free(tmp);
	free(digest);
	return key;

}

char ** cellophane_read(WsHandler * ws_handler, int * message_num) {

	int n;
	int totalread = 0;
	char * m_payload;
	char buff[256];

	bzero(buff,256);
	n = recv(ws_handler->fd, buff, 1,  0);
	if (n < 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR reading from socket");
		return NULL;
	}

	bzero(buff,256);
	n = recv(ws_handler->fd,buff,255, 0);
	if (n < 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR reading from socket");
		return NULL;
	}

	ws_handler->buffer = realloc(NULL, strlen(buff));
	strcpy(ws_handler->buffer, buff);


	int num_messages = cellophane_number_of_message(buff,n);

	while(num_messages < 0){
		//printf("\n\nNot enougth data..\n\n");
		n = n + recv(ws_handler->fd,buff,255, 0);
		if (n < 0)
            {
				cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR reading from socket");
				break;
            }
		ws_handler->buffer = (char*)realloc(ws_handler->buffer,(strlen(ws_handler->buffer) + strlen(buff)));
		strcat(ws_handler->buffer, buff);
		num_messages = cellophane_number_of_message(ws_handler->buffer,n);
	}


	char ** stream_buffer = malloc(sizeof(char*) * num_messages);

	int msg_number = 0;

	if (stream_buffer) {

		int buffer_pointer = 0;
		size_t idx  = 0;

		while(buffer_pointer < n){

			int payload_len = (int)(ws_handler->buffer[buffer_pointer]);
			buffer_pointer++;
			switch (payload_len) {
			case 126:
				{
					char aux_len [2];
					strncpy(aux_len, ws_handler->buffer+buffer_pointer,2);
					payload_len = (((int)aux_len[0] & 0x00FF) << 8) | (aux_len[1]  & 0x00FF);
					buffer_pointer = buffer_pointer + 2;
					break;
				}
			case 127:
				//perror("Next 8 bytes are 64bit uint payload length, not yet implemented, since PHP can't handle 64bit longs!");
				break;
			}

			if((n-buffer_pointer)< payload_len){

				cellophane_print_log(ws_handler,LOG_WARNING,DEBUG_DIAGNOSTIC,"Not enougth data..");
			}

			//printf("Payload len: %d - %d\n",payload_len,n);

			char* message = malloc(payload_len+1);
			bzero(message ,payload_len);
			strncpy(message , ws_handler->buffer+buffer_pointer,payload_len);
			message[payload_len] = '\0';
			//sprintf(stream_buffer,"%s",ws_handler->buffer+1);
			buffer_pointer = buffer_pointer + payload_len +1;

			//printf("Stream buffer: %s\n",message);
			assert(idx < num_messages);
			*(stream_buffer + idx++) = strdup(message);

			msg_number++;
		}
		assert(idx == (num_messages));
		*(stream_buffer + idx) = 0;
	}


	// There is also masking bit, as MSB, but it's 0 in current Socket.io

	message_num = msg_number;

	return stream_buffer;
}


int cellophane_send(WsHandler * ws_handler, enum eio_type e_type, enum socket_io_type s_type, char * id, char * endpoint, char * message,  int easy) {

	Payload m_payload;
	char *raw_message;
	int len, old_len;
	

	payload_init(&m_payload);
	payload_setOpcode(&m_payload, OPCODE_TEXT);

	if(message == NULL){
		raw_message = (char*)malloc(2);
		sprintf(raw_message,"%d%d",e_type,s_type);
	}else{
		old_len = 2+strlen(message)*sizeof(char);
		raw_message = (char*)malloc(3+strlen(message)*sizeof(char));
		len = sprintf(raw_message,"%d%d%s", e_type, s_type, message);
	}
	
	payload_setPayload(&m_payload,(unsigned char *)raw_message);
	payload_setMask(&m_payload, 0x0);
	int bytes_towrite;
	char * enc_payload = payload_encodePayload(&m_payload);
	
	int n = send(ws_handler->fd,enc_payload, m_payload.enc_payload_size, 0);
	cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DETAILED,"Sending data line: sent %d should be sent %d",n, m_payload.enc_payload_size);
	cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Sending    > %s (%d/%d/%d)",raw_message, strlen(raw_message), len, old_len);
	//fprintf(stderr, "Encoded(%d) : %s \n",m_payload.enc_payload_size, enc_payload);
	if (n < 0){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE,"ERROR writing to socket");
		return n;
	}
	
	if(raw_message != NULL){
		free(raw_message);
	}
	
	if(enc_payload != NULL){
		free(enc_payload);
	}
	payload_free(&m_payload);
	usleep(100*1000);
	return n;
} 

void  cellophane_emit(WsHandler * ws_handler, char * event, char * args, char * endpoint ){

	int malloc_len = (6 + (int)(strlen(event) + strlen(args))) * sizeof(char);
	char *emit_message = (char*) malloc(malloc_len);
	bzero(emit_message,malloc_len);
	snprintf(emit_message, malloc_len, "[\"%s\",%s]", event, args);
	cellophane_send(ws_handler, EIO_MESSAGE, TYPE_EVENT, "", endpoint, emit_message, 1);
	if(emit_message != NULL){
		free(emit_message);
	}
}


void cellophane_on(WsHandler * ws_handler, char * event_name, void (*on_event_callback)(WsEventInfo))
{
    int i;

    for(i=0; i < 10; i++){
        if(strcmp(ws_handler->default_events[i].event_name,event_name) == 0){
			ws_handler->default_events[i].callback_func = on_event_callback;
			return;
        }
    }

    for(i=0; i < ws_handler->user_events_len; i++){
        if(strcmp(ws_handler->events[i].event_name,event_name) == 0){
			ws_handler->events[i].callback_func = on_event_callback;
			return;
        }
    }
    if (ws_handler->user_events_len == 100){
        cellophane_print_log(ws_handler,LOG_WARNING,DEBUG_NORMAL,"User event limit (100) exceded, imposible to add");
        return;
    }
    ws_handler->events[ws_handler->user_events_len].event_name = (char *) malloc(strlen(event_name));
    memcpy(ws_handler->events[ws_handler->user_events_len].event_name,event_name,strlen(event_name));
    ws_handler->events[ws_handler->user_events_len].callback_func = on_event_callback;
    ws_handler->user_events_len++;
}


void cellophane_io_message_parse(char * raw_string, WsMessage * message){

	if(strncmp(raw_string,"0",1) == 0){
		message->e_type = EIO_OPEN;
		message->raw_message = NULL;

	}else if(strncmp(raw_string,"1",1) == 0){
		message->e_type = EIO_CLOSE;
		message->raw_message = NULL;

	}else if(strncmp(raw_string,"2",1) == 0){
		message->e_type = EIO_PING;
		message->raw_message = NULL;

	}else if(strncmp(raw_string,"3",1) == 0){
		message->e_type = EIO_PONG;
		message->raw_message = NULL;

	}else if(strncmp(raw_string,"4",1) == 0){
		message->e_type = EIO_MESSAGE;

		if(strncmp(raw_string,"40",2) == 0){
			message->s_type = TYPE_CONNECT;
			message->raw_message = NULL;
		}else if(strncmp(raw_string,"41",2) == 0){
			message->s_type = TYPE_DISCONNECT;
			message->raw_message = NULL;
		}if(strncmp(raw_string,"42",2) == 0){
			message->s_type = TYPE_EVENT;
			message->raw_message = raw_string + 2;
		}if(strncmp(raw_string,"43",2) == 0){
			message->s_type = TYPE_ACK;
			message->raw_message = raw_string + 2;
		}if(strncmp(raw_string,"44",2) == 0){
			message->s_type = TYPE_ERROR;
			message->raw_message = raw_string + 2;
		}if(strncmp(raw_string,"45",2) == 0){
			message->s_type = TYPE_BINARY_EVENT;
			message->raw_message = raw_string + 2;
		}if(strncmp(raw_string,"46",2) == 0){
			message->s_type = TYPE_BINARY_ACK;
			message->raw_message = raw_string + 2;
		} 

	}else if(strncmp(raw_string,"5",1) == 0){
		message->e_type = EIO_UPGRADE;
		message->raw_message = NULL;

	}else if(strncmp(raw_string,"6",1) == 0){
		message->e_type = EIO_NOOP;
		message->raw_message = NULL;
	}
}


void cellophane_io_json_response_parse(WsHandler *ws_handler, char * json_string, WsEventInfo * info_rsponse){

	json_t *json_ret;
	json_error_t json_err;


	json_ret = json_loads(json_string, 0, &json_err);
	if(json_ret == NULL){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE, "Could not decode JSON message: %d: %s\n", json_err.line, json_err.text);
		return;
	}
	
	if(!json_is_array(json_ret)){
		cellophane_print_log(ws_handler,LOG_ERROR,DEBUG_NONE, "Expecting JSON array in JSON Message\n");
		return;
	}
	//fprintf(stderr, "Number of items in array %d \n", json_array_size(json_ret));
	info_rsponse->event_name = json_string_value(json_array_get(json_ret, 0));
	info_rsponse->message = json_dumps(json_array_get(json_ret, 1), JSON_INDENT(1));

	cellophane_print_log(ws_handler,LOG_INFO, DEBUG_NONE, "Array(0) : %s , Array(1) : %s ", info_rsponse->event_name, info_rsponse->message);
}

void cellophane_call_handler(WsHandler *ws_handler, WsEventInfo info_rsponse){
	int j;

	for(j=0; j < ws_handler->user_events_len; j++){
		if(strcmp(ws_handler->events[j].event_name, info_rsponse.event_name) == 0){
			ws_handler->events[j].callback_func(info_rsponse);
		}
	}
}
void cellophane_event_handler(WsHandler * ws_handler){

	char ** data = cellophane_read(ws_handler, NULL);
	int i = 0;
	if (data){
		for (i = 0; *(data + i); i++){
			cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Received> %s",*(data + i));
			WsMessage ws_message;
			cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Raw Response : %s", *(data + i));
			cellophane_io_message_parse(*(data + i), &ws_message);

			if(ws_message.e_type == EIO_MESSAGE  && ws_message.s_type == TYPE_EVENT){
				WsEventInfo info_rsponse;
				cellophane_io_json_response_parse(ws_handler, ws_message.raw_message, &info_rsponse);
				
				if(info_rsponse.event_name != NULL && info_rsponse.message != NULL){
					cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Received data on message> data: %s , args: %s",info_rsponse.event_name, info_rsponse.message);
					cellophane_trigger_default_events(ws_handler, info_rsponse);
					cellophane_call_handler(ws_handler,info_rsponse);
				}else{
					cellophane_print_log(ws_handler, LOG_ERROR, DEBUG_NONE, "Failed to parse illformed message : %s ", *(data + i));
				}

			}else if (ws_message.e_type == EIO_MESSAGE  && ws_message.s_type == TYPE_DISCONNECT){
				WsEventInfo info_rsponse;
				info_rsponse.event_name = "disconnect";
				info_rsponse.message = "disconnected";
				cellophane_trigger_default_events(ws_handler, info_rsponse);
			}else{
				cellophane_print_log(ws_handler, LOG_WARNING, DEBUG_DETAILED,"We dont support handling for E_IO : %d  S_ID : %d", ws_message.e_type, ws_message.s_type);
			}
		}
		cellophane_print_log(ws_handler,LOG_INFO,DEBUG_DIAGNOSTIC,"Received %d Messages",i);
	}

	if(!i){

		cellophane_print_log(ws_handler,LOG_WARNING,DEBUG_DETAILED,"No data received....");

		cellophane_reconect(ws_handler);
		if(!ws_handler->fd_alive){
			WsEventInfo info;
			info.event_name = "reconnect_failed";
			info.message = "Reconnect Failed";
			cellophane_trigger_default_events(ws_handler, info);
		}
	}
}


void  cellophane_close(WsHandler * ws_handler)
{
	cellophane_send(ws_handler, EIO_CLOSE, TYPE_DISCONNECT, "", "","", 1);
	close(ws_handler->fd);
	ws_handler->fd_alive = 0;
	WsEventInfo info;
	info.event_name = "disconnect";
	info.message = "Disconnecting";
	cellophane_trigger_default_events(ws_handler, info);
}


void cellophane_reconect(WsHandler * ws_handler){

	cellophane_print_log(ws_handler,LOG_WARNING,DEBUG_DETAILED,"Reconecting....");

	WsEventInfo info;
	info.event_name = "reconnecting";
	info.message = "Reconnecting";
	cellophane_trigger_default_events(ws_handler, info);

	cellophane_close(ws_handler);
	if(cellophane_init(ws_handler,0)){
		WsEventInfo info_r;
		info_r.event_name = "reconnect";
		info_r.message = "Reconnect";
		cellophane_trigger_default_events(ws_handler, info_r);
	}
}

void cellophane_keepAlive(WsHandler * ws_handler) {

	int result = 0;
	fd_set writefds;
	fd_set exceptfds;

	char spinner[] = "|/-\\";
	char spinner2[] = "+x";
	int spinner_index= 0;
	int spinner2_index= 0;

	while(ws_handler->fd_alive && ws_handler->keep_alive_flag){
		if (spinner2_index==2){
			spinner2_index=0;
		}

		cellophane_print_log(ws_handler,LOG_FREE,DEBUG_DETAILED,"%c\b", spinner2[spinner2_index]);
		fflush( stdout );
		usleep(100*1000);
		spinner2_index++;
		//fprintf(stderr, "is  ready ??\n");
		
		while(!cellophane_read_ready(ws_handler)){
			if(!ws_handler->keep_alive_flag){
				cellophane_print_log(ws_handler,LOG_INFO, DEBUG_DETAILED,"Keep alive flag set to False");
				return;
			}else{
				//cellophane_print_log(ws_handler,LOG_INFO, DEBUG_DETAILED,"Keep alive flag set to True");
			}

			if (ws_handler->session.heartbeat_timeout > 0 && ws_handler->session.heartbeat_timeout+ws_handler->heartbeatStamp-5 < time(NULL)) {
				cellophane_send(ws_handler, EIO_PING, TYPE_EVENT, "", "","", 1);
				ws_handler->heartbeatStamp = time(NULL);
			}
			if (spinner_index==4){
				spinner_index=0;
			}
			cellophane_print_log(ws_handler,LOG_FREE,DEBUG_DETAILED,"%c\b", spinner[spinner_index]);
			fflush( stdout );
			usleep(100*1000);
			spinner_index++;
		}
		
		cellophane_event_handler(ws_handler);
	}
}

int cellophane_clean_header(char * header)
{


}

int cellophane_read_ready(WsHandler * ws_handler)
{
    int iSelectReturn = 0;  // Number of sockets meeting the criteria given to select()
    struct timeval timeToWait;
    int fd_max = -1;          // Max socket descriptor to limit search plus one.
    fd_set readSetOfSockets;  // Bitset representing the socket we want to read
                              // 32-bit mask representing 0-31 descriptors where each
                              // bit reflects the socket descriptor based on its bit position.

    timeToWait.tv_sec  = 0;
    timeToWait.tv_usec = 100* 100;

    FD_ZERO(&readSetOfSockets);
    FD_SET(ws_handler->fd, &readSetOfSockets);

    if(ws_handler->fd > fd_max){
		fd_max = ws_handler->fd;
	}

    iSelectReturn = select(fd_max + 1, &readSetOfSockets, (fd_set*) 0, (fd_set*) 0, &timeToWait);

    // iSelectReturn -1: ERROR, 0: no data, >0: Number of descriptors found which pass test given to select()
	// Not ready to read. No valid descriptors
    if ( iSelectReturn == 0 ) {
		return 0;
	} else if ( iSelectReturn < 0 ){
		return 1;
	}

    // Got here because iSelectReturn > 0 thus data available on at least one descriptor
    // Is our socket in the return list of readable sockets
    if ( FD_ISSET(ws_handler->fd, &readSetOfSockets) ){
		return 1;
	}else{
		return 0;
	}

    return 0;
}


/* the function to return the content for a url */
char * cellophane_do_web_request(WsHandler * ws_handler)
{
    /* keeps the handle to the curl object */
    CURL *curl_handle = NULL;
    /* to keep the response */
    struct block response;

    /* initializing curl and setting the url */
    curl_handle = curl_easy_init();

    curl_easy_setopt(curl_handle, CURLOPT_URL, ws_handler->socketIOUrl );
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1);

	if (!ws_handler->checkSslPeer){
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
    }

    if (ws_handler->handshakeTimeout != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, ws_handler->handshakeTimeout);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, ws_handler->handshakeTimeout);
    }

    /* setting a callback function to return the data */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, cellophane_write_callback_func);

    /* passing the pointer to the response as the callback parameter */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &response);

    /* perform the request */
    int res = curl_easy_perform(curl_handle);

    /* cleaning all curl stuff */
    curl_easy_cleanup(curl_handle);
	
    if (res != 0 && res != 23){
        return NULL;
    }
		
    return response.mem;
}

/* the function to invoke as the data recieved */
size_t static cellophane_write_callback_func(void *buffer,
											 size_t size,
											 size_t nmemb,
											 void *userp)
{
    struct block *response  =  (struct block *)userp;
	char *temp;
	char *buf = (char *)buffer;
	size_t buf_size = (size * nmemb) - 5, cnt = 0;
	/*Magic number 5 is required to avoid gargabe at the start of the response, with this hack json is recieved*/
    response->mem = (char *) malloc( buf_size );
	memcpy(response->mem, (buf+5), buf_size);
	*(response->mem + buf_size) = '\0';
}

char** cellophane_str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp){
		if (a_delim == *tmp){
			count++;
			last_comma = tmp;
		}
		tmp++;
	}

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)	{
		size_t idx  = 0;
		char* token = strtok(a_str, delim);

		while (token){
			assert(idx < count);
			*(result + idx++) = strdup(token);
			token = strtok(0, delim);
		}
		assert(idx == count - 1);
		*(result + idx) = 0;
	}

    return result;
}


int cellophane_number_of_message(char * buffer, int n){

	int msg_counter = 0;
	int buffer_pointer = 0;

	while(buffer_pointer < n){

		int payload_len = (int)(buffer[buffer_pointer]);
		buffer_pointer++;
		switch (payload_len) {
		case 126:
			{
				char aux_len [2];
				strncpy(aux_len, buffer+buffer_pointer,2);
				payload_len = (((int)aux_len[0] & 0x00FF) << 8) | (aux_len[1]  & 0x00FF);
				buffer_pointer = buffer_pointer + 2;
				break;
			}
		case 127:
			//perror("Next 8 bytes are 64bit uint payload length, not yet implemented, since PHP can't handle 64bit longs!");
			break;
		}

		if((n-buffer_pointer)< payload_len){
			return -1;
		}

		buffer_pointer = buffer_pointer + payload_len +1;
		msg_counter++;
	}
	return msg_counter;
}

int cellophane_find_on_char_array(char ** tokens , char * key){

    if (tokens){
		int i;
		for (i = 0; *(tokens + i); i++){
			if(strcmp(*(tokens + i), key) == 0){
				return 1;
			}
		}
	}
    return 0;
}

void cellophane_compute_md5(char *str, unsigned char digest[16]) {
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, str, strlen(str));
    MD5_Final(digest, &ctx);
}

static const char basis_64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int Base64encode_len(int len)
{
    return ((len + 2) / 3 * 4) + 1;
}

int Base64encode(char *encoded, const char *string, int len)
{
    int i;
    char *p;

    p = encoded;
    for (i = 0; i < len - 2; i += 3) {
		*p++ = basis_64[(string[i] >> 2) & 0x3F];
		*p++ = basis_64[((string[i] & 0x3) << 4) |
						((int) (string[i + 1] & 0xF0) >> 4)];
		*p++ = basis_64[((string[i + 1] & 0xF) << 2) |
						((int) (string[i + 2] & 0xC0) >> 6)];
		*p++ = basis_64[string[i + 2] & 0x3F];
    }
    if (i < len) {
		*p++ = basis_64[(string[i] >> 2) & 0x3F];
		if (i == (len - 1)) {
			*p++ = basis_64[((string[i] & 0x3) << 4)];
			*p++ = '=';
		}
		else {
			*p++ = basis_64[((string[i] & 0x3) << 4) |
							((int) (string[i + 1] & 0xF0) >> 4)];
			*p++ = basis_64[((string[i + 1] & 0xF) << 2)];
		}
		*p++ = '=';
    }

    *p++ = '\0';
    return p - encoded;
}

