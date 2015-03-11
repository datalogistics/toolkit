/*
 * Example of a socket.io client.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <cellophane_io.h>
#include <json-c/json.h>

void on_notofication_callback(WsEventInfo info){
    WsHandler * parent_client = (WsHandler *) info.ws_handler;
    printf("Notification: %s\n", info.message);
}

int main()
{
    WsHandler io_client;
	json_object *json_obj;
    cellophane_io(&io_client,"http://", "10.0.0.113", 8000);
    cellophane_set_debug(&io_client, DEBUG_DETAILED);
    cellophane_on(&io_client, "anything", on_notofication_callback);
	printf("[Done] Client handle init \n");
    cellophane_io_connect(&io_client);

	//json_obj = json_object_new_object();
	//json_object_object_add(json_obj, "hashId" , json_object_new_int(1234));
	cellophane_emit(&io_client,"peri_download_register", "{\"hashId\":\"1234567\",\"filename\":\"test.c\",\"connections\":5,\"totalSize\":102458945}","");
	//cellophane_emit(&io_client,"peri_download_register", json_dumps(json_obj, JSON_COMPACT),"");
	//cellophane_emit_str(&io_client,"peri_download_register","{filename:\'LC80220352015054LGN00.tar.gz\',totalSize:877554922,hashId:\'\',connections:6}","");
	io_client.keep_alive_flag = 1;
    cellophane_keepAlive(&io_client);
    cellophane_close(&io_client);
    return 0;
}
