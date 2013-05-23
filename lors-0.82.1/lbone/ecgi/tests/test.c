#include <stdio.h>
#include <stdlib.h>
#include "ecgi.h"

const char *nl(const char *x)
{
	if(x==NULL) return("(NULL)");
	if(*x==0) return("(EMPTY)");
	return(x);
}

int main()
{
	CgiPos *p=NULL;
	const char *val;
	MFILE *mf; 
	const char *name;
	
	    setvbuf(stdout, NULL, _IOLBF, 0);
	    setvbuf(stderr, NULL, _IOLBF, 0);

	printf("Content-Type: text/html\n\n<html><body><pre>\n");
	
	if(cgi_init()!=true || cgi_errno!=0){
		printf("%s\n", cgi_strerror(cgi_errno));
		exit(0);
	}

	printf("init done - now dumping the list\n");
	listDump();

	printf("get text: %s\n", nl(cgi_param("text")));
	printf("get text: %s\n", nl(cgi_param("text")));
	printf("get text: %s\n", nl(cgi_param("text")));
	printf("get null: %s\n", nl(cgi_param(NULL)));
	printf("get text: %s\n", nl(cgi_param("text")));
	printf("get text: %s\n", nl(cgi_param("text")));
	printf("get text: %s\n", nl(cgi_param("text")));

	printf("get multiple select: %s\n", nl(cgi_param("multiple select")));
	printf("get multiple select: %s\n", nl(cgi_param("multiple select")));
	printf("get multiple select: %s\n", nl(cgi_param("multiple select")));
	printf("get null: %s\n", nl(cgi_param(NULL)));
	printf("get multiple select: %s\n", nl(cgi_param("multiple select")));
	printf("get multiple select: %s\n", nl(cgi_param("multiple select")));
	printf("get multiple select: %s\n", nl(cgi_param("multiple select")));

	printf("get textarea: %s\n", nl(cgi_param("textarea")));

	printf("get SHIT: %s\n", nl(cgi_param("SHIT")));
	
	printf("----------------SO FAR, SO GOOD--------------\n");

	if(cgiGetKind("file")==CgiKindFile){
		mf=cgiGetMFile("file");
		printf("duming non empty file %s (Length: %d CType: %s)\n", nl(cgiParam("file")), mfGetLength(mf), cgiGetCTyp("file"));
		printf("%s\n", mfGetData(mf));
	}	
	
	printf("Getting all names, where value = 'test'\n");
	while((name=cgiNameByValue("test"))!=NULL)
		printf("%s\n", name);



	// notice, no need to know any of the names of the fields from above

	printf("----------------Getting all names/values with the pos funcs -----------\n");

	while((p=cgiPosNext(p))!=NULL){
		printf("%s : ", cgiPosName(p));
		while((val=cgiPosParam(p))!=NULL)
			printf("%s ", val);
		printf("\n");
	}

	printf("----------------Getting all names with the name funcs -----------\n");
	printf("%s\n", cgiGetFirstName());
	while((val=cgiGetNextName())!=NULL)
		printf("%s\n", val);

	printf("----------------- dumping all data with the build in listDump function for debugging ---------------\n");
	listDump();

	printf("saving data to /tmp/ecgitstdump\n");
	cgiSaveDebugData("/tmp/ecgitstdump", "w");
	printf("saving done\n");

	cgi_done();

	printf("Free'd all Test complete! cgi lib working!\n");
	
	return(0);
}