#include <socket_io.h>

int main(int argc, char **argv){

	int i;

	if(argc !=  3){
		fprintf(stderr, "USAGE %s , <HOST> <SESSION ID>", argv[0]);
		return 0;
	}

	socket_io_handler handle;
	
	socket_io_init(&handle, argv[1], argv[2]);
	socket_io_send_register(&handle, "test.c", 102458945, 5);
	
	for(i=0; i<100; i++){
		socket_io_send_push(&handle, "depot1.loc1.tacc.reddnet.org", i*100, i*1000);
	}

	socket_io_send_clear(&handle);
	socket_io_close(&handle);
	
}
