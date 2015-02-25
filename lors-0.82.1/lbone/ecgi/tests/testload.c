#include <stdio.h>
#include <stdlib.h>
#include "ecgi.h"

int main()
{
	int i=0;
	extern char **environ;

	setvbuf(stdout, NULL, _IOLBF, 0);
	setvbuf(stderr, NULL, _IOLBF, 0);

	printf("Content-Type: text/html\n\n<html><body><pre>\n");
	
	printf("loading data from /tmp/ecgitstdump\n");
	cgiLoadDebugData("/tmp/ecgitstdump");
	printf("loading done\n");
	listDump();
	printf("dump done!\n");

	cgi_done();

	printf("Free'd all Test complete! cgi lib working!\n");
	
	printf("Dumping environment:\n");
	while(environ[i]!=NULL)
		printf("%s\n", environ[i++]);
	
	return(0);
}