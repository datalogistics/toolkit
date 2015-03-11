/*
 * Example of a socket.io client.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "cellophane_io.h"

void on_notofication_callback(WsEventInfo info){

    WsHandler * parent_client = (WsHandler *) info.ws_handler;
    printf("Notification: %s\n", info.message);

}

void static keepBeating(WsHandler *io_client){
	printf("---------------->starting\n ");
	cellophane_emit(io_client, "login", "foo", "");
	//cellophane_emit(&io_client, "login", "tango", "");
}

int main()
{
	int ret;
	pthread_t heartbeat; 

    WsHandler io_client;
    cellophane_io(&io_client,"http://", "10.0.0.113", 8000);
    cellophane_set_debug(&io_client, DEBUG_DETAILED);
    cellophane_on(&io_client, "anything", on_notofication_callback);
    
	ret = cellophane_io_connect(&io_client);
	if(ret == 0 ){
		fprintf(stderr, "Failed to connect \n");
		return 0;
	}

	pthread_create(&heartbeat, NULL, keepBeating, &io_client);
	
    pthread_join(heartbeat, NULL);
	cellophane_keepAlive(&io_client);
    cellophane_close(&io_client);
    return 0;

}
