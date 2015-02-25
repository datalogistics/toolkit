#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memfile.h>
#include "html2h.h"

void usage()
{
	printf("
html2h v0.1
usage:
	html2h input.html [output.h]
	
	if output is not set, input.h will be generated and overwritten!

	debug messages are written to stderr!

");

	exit(0);
}

int main(int argc, char **argv)
{
	char *fout, *point;
	seclist=calloc(1, sizeof(Section));

	if(argc<2 || argc>3) usage();
	if(argc==2){
		fout=(char*)strdup(argv[1]);
		point=strchr(fout, '.');
		if(point == NULL) pexit("Couldn generate output filename - no . in input file\n", argv[1]);
		strcpy(point, ".h");
	}else
		fout=argv[2];

	if(!strcmp(fout, argv[1])){
		printf("input and output files have the same name - exiting\n");
		exit(1);
	}

	fileWrite(fileRead(argv[1]), fout);
	return(0);
}

void fileWrite(MFILE *mf, char *fname)
{
	FILE *f=fopen(fname, "w");
	if(f==NULL) pexit("Cannot write to output file ", fname);
	
	fprintf(f, "/****************************************************\n");
	fprintf(f, " *       Header file generated with html2h 0.1      *\n");
	fprintf(f, " *            (c) 2000 by Sven@Dawitz.de            *\n");
	fprintf(f, " ****************************************************\n");
	listDump(f);
	fprintf(f, "\n ****************************************************/\n\n");

	mfMFileToFile(mf, f);
	fclose(f);
}

MFILE *fileRead(char *fname)
{
	MFILE *mfin=mfopen(), *mfout=mfopen(), *mfbuf=mfopen();
	FILE *f=fopen(fname, "r");
	const char *flastok, *fhit, *fend;
	if(f==NULL) pexit("Cannot read input file ", fname);
	
	mfFileToMFile(f, mfin);
	fclose(f);
	
	flastok=fhit=fend=mfGetData(mfin);
	while((fhit=strstr(flastok, "<!--#"))!=NULL){
		searchInput(flastok, fhit);
	
		fend=strstr(fhit, "-->");
		if(fend==NULL) pexit("Parse error - '-->' expected - exiting\n", fhit);
		if(commdepth==0)
			escapeWrite((void*)flastok, fhit-flastok, mfout);
		flastok=(const char*)(fend+3);
		mfSetLength(mfbuf, 0);

		mfwrite((void*)(fhit+5), 1, fend-(fhit+5), mfbuf);
		parseMeta(mfout, mfGetData(mfbuf));
	}
	searchInput(flastok, (char*)0x8FFFFFFF);
	escapeWrite((void*)flastok, strlen(flastok), mfout);
	mfprintf(mfout, "\";\n");
	
	mfclose(mfin);
	return(mfout);
}

void searchInput(const char *flastok, const char *fhit)
{
	MFILE *mfbuf=mfopen();
	const char *input, *ta, *hit, *select, *end, *test;

	hit=flastok;
	do{
		input=strstr(hit, "<input");
		ta=strstr(hit, "<textarea");
		select=strstr(hit, "<select");
		hit=(char*)0x8FFFFFFF;
		if(input!=NULL && input<hit) hit=input;
		if(ta!=NULL && ta<hit) hit=ta;
		if(select!=NULL && select < hit) hit=select;
		if(hit!=(char*)0x8FFFFFFF && hit<fhit){
			end=strchr(hit, '>');
			test=strchr(hit+1, '<');
			if(test!=NULL && test<end) end=strchr(end+1, '>');
			mfSetLength(mfbuf, 0);
			mfwrite((void*)hit+1, 1, end-hit-1, mfbuf);
			printf("Input: %s\n", mfGetData(mfbuf));
			parseInput(mfGetData(mfbuf));
			hit++;
		}
	}while(hit!=(char*)0x8FFFFFFF && hit<fhit);

	mfclose(mfbuf);
}

void parseInput(const char *input)
{
	char *name=parseInputValue(input, "name");
	char *type=NULL;
	if(secakt==NULL) pexit("Input: tried to insert, but no section found\n", input);
	
	if(!strncasecmp(input, "select", 6))
		type="select";
	if(!strncasecmp(input, "textarea", 8))
		type="textarea";
	if(!strncasecmp(input, "input", 5))
		type=parseInputValue(input, "type");
	if(type==NULL)
		type="unknown";
	
	if(commdepth==0)
		listInsertInput(name, type);
}

char *parseInputValue(const char *str, const char *name)
{
	char *hit, *nameeq, endc, *ret;
	int length=strlen(name)+1, i=0;
	
	nameeq=malloc(length+1);
	sprintf(nameeq, "%s=", name);
	hit=strstr(str, nameeq);
	if(hit==NULL) return(NULL);
	hit+=length;
	if(*hit==0) return(NULL);
	
	if(*hit=='"')   { endc='"'; hit++; }
	else		endc=' ';
	
	while(hit[i]!=0 && hit[i]!=endc) i++;
	
	ret=malloc(i+1);
	strncpy(ret, hit, i);
	ret[i]=0;
	
	return(ret);
}

void parseMeta(MFILE *mfout, const char *toparse)
{
	char *tagend=strchr(toparse, ' ');
	int taglen=tagend-toparse;
	
	fprintf(stderr, "Parsing: %s\n", toparse);

	if(!strcmp(toparse, "/*")){
		commdepth++;
		return;
	}
	
	if(!strcmp(toparse, "*/")){
		commdepth--;
		if(commdepth<0)
			pexit("comment to deep - to many '*/' found - exiting\n", "");
		return;
	}

	if(commdepth>0) return;

	if(taglen<0)
		pexit("tag wrong - ' ' expected - exiting\n", toparse);
	
	if(!strncasecmp(toparse, "section", strlen("section")))
		return parseSection(mfout, tagend+1);
	if(!strncasecmp(toparse, "param", strlen("param")))
		return (void)parseParam(mfout, secakt->list, tagend+1, false);
	if(!strncasecmp(toparse, "slink", strlen("slink")))
		return parseSLink(mfout, tagend+1);	
}

void parseSection(MFILE *mfout, const char *params)
{
	char *name=(char*)params, *comment=strchr(params, ' '), *cend;
	
	if(secakt!=NULL)
		mfprintf(mfout, "\n\";\n");

	if(comment!=NULL) {
		*comment=0;
		comment++;
		if(comment[0]!=0 && comment[0]!='\'')
			pexit("Section-Comment not started with ' - exiting\n", params);
		comment++;
		cend=strchr(comment, '\'');
		if(cend==NULL) pexit("Section-Comment not ended with ' - exiting\n", params);
		*cend=0;
	}
	
	secakt=listInsert(name, comment);
	mfprintf(mfout, "\nconst char %s[]=\"", name);
}

char *parseParam(MFILE *mfout, Content *insat, const char *params, int printpname)
{
	char *format=(char*)params, *name, *comment=NULL, *cend;
	if(secakt==NULL) pexit("Param: tried to insert, but no section found\n", params);
	
	name=strchr(format, ' ');
	if(name==NULL) pexit("Param-Format not found - exiting\n", params);
	*name=0; name++;
	if(*name==0) pexit("Param-Name not found - exiting\n", name);

	cend=strchr(name, ' ');
	if(cend!=NULL){
		*cend=0; cend++;
		while(*cend==' ') cend++;

		if(*cend=='\''){
			comment=cend;
			comment++;
			cend=strchr(comment, '\'');
			if(cend==NULL) pexit("Param-Comment not ended with ' - exiting\n", params);
			*cend=0; cend++;
			while(*cend!=0 && *cend==' ') cend++;
		}
	}

	listInsertContent(insat, format, name, comment);
	if(printpname)
		mfprintf(mfout, "%s=%%%s", name, format);
	else
		mfprintf(mfout, "%%%s", format);

	if(cend==NULL || *cend==0) return(NULL);
	return(cend);
}

void parseSLink(MFILE *mfout, const char *params)
{
	Content *c;
	char *p=(char*)params, *comment, *cend;
	if(secakt==NULL) pexit("SLink: tried to insert, but no section found\n", params);
	
	if(*params!='\'') comment=(char*)strdup("");
	else{
		comment=(char*)(params+1);
		cend=strchr(comment, '\'');
		if(cend==NULL) pexit("SLink: comment not ended with ' - exiting\n", params);
		*cend=0; cend++;
		while(*cend==' ') cend++;
		p=cend;
	}
			
	c=listInsertContent(secakt->list, "", "***SCRIPT LINK***", comment);
	listInsertContent(c->sub, "s", "script", "Script Name");

	mfprintf(mfout, "%%s?");
	while((p=parseParam(mfout, c->sub, p, true))!=NULL)
		mfputc('&', mfout);
}

Section *listInsert(const char *name, const char *comment)
{
	Section *runner=seclist;
	
	while(runner->next!=NULL) runner=runner->next;
	runner->next=calloc(1, sizeof(Section));
	runner->list=calloc(1, sizeof(Content));
	runner->ilist=calloc(1, sizeof(Input));
	
	runner->name=(char*)strdup(name);
	if(comment!=NULL)
		runner->comment=(char*)strdup(comment);
	else
		runner->comment=(char*)strdup("");
	
	return(runner);
}

Content *listInsertContent(Content *where, const char *format, 
		const char *name, const char *comment)
{
	while(where->next!=NULL) where=where->next;
	where->next=calloc(1, sizeof(Content));
	where->sub=calloc(1, sizeof(Content));
	
	where->format=(char*)strdup(format);
	where->name=(char*)strdup(name);
	if(comment!=NULL && *comment!=0)
		where->comment=(char*)strdup(comment);
	else
		where->comment=(char*)strdup("");
		
	return(where);
}

void listInsertInput(const char *name, const char *type)
{
	Input *runner;
	if(secakt==NULL) return;
	runner=secakt->ilist;
	
	while(runner->next!=NULL) runner=runner->next;
	runner->next=calloc(1, sizeof(Input));
	if(name!=NULL)	runner->name=strdup(name);
	else		runner->name="";
	if(type!=NULL)  runner->type=strdup(type);
	else		runner->type="";
}

void listDump(FILE *f)
{
	Section *runner=seclist;
	Content *crunner, *crunner2;
	Input *irunner;
	
	while(runner->next!=NULL){
		fprintf(f, "\n\n****************************************\n%-32s", runner->name);
		if(runner->comment!=NULL && *runner->comment!=0)
			fprintf(f, " \"%s\"", runner->comment);
		fprintf(f, "\n****************************************\n\n");

		// printf template

		fprintf(f, "PRINTF-TEMPLATE\n");
		fprintf(f, "----------------------------------------\n");
		if(runner->list->next!=NULL)
			fprintf(f, "printf(%s, ", runner->name);
		else
			fprintf(f, "printf(%s", runner->name);
		crunner=runner->list;
		while(crunner->next!=NULL){
			if(crunner->sub->next==NULL){
				if(crunner->next->next!=NULL)
					fprintf(f, "%s, ", crunner->name);
				else
					fprintf(f, "%s", crunner->name);
			}
			crunner2=crunner->sub;
			while(crunner2->next!=NULL){
				if(crunner2->next->next!=NULL || crunner->next->next!=NULL)
					fprintf(f, "%s, ", crunner2->name);
				else
					fprintf(f, "%s", crunner2->name);
				crunner2=crunner2->next;
			}
			crunner=crunner->next;		
		}
		fprintf(f, ");\n\n");

		// form input names

		fprintf(f, "%-31s %s\n", "FORM-INPUT-NAME", "FORM-INPUT-TYPE");
		fprintf(f, "----------------------------------------\n");
		irunner=runner->ilist;
		while(irunner->next!=NULL){
			fprintf(f, "%-31s %s\n", irunner->name, irunner->type);
			irunner=irunner->next;
		}
		
		// parameter
		
		fprintf(f, "\n%-10s %-20s %s\n", "FORMAT", "NAME", "COMMENT");
		fprintf(f,   "----------------------------------------\n");
		crunner=runner->list;
		while(crunner->next!=NULL){
			fprintf(f, "%-10s %-20s", crunner->format, crunner->name);
			if(crunner->comment!=NULL && *crunner->comment!=0)
				fprintf(f, " \"%s\"", crunner->comment);
			fprintf(f, "\n");
			crunner2=crunner->sub;
			while(crunner2->next!=NULL){
				fprintf(f, "%-10s      %-20s", crunner2->format, crunner2->name);
				if(crunner2->comment!=NULL && *crunner2->comment!=0)
					fprintf(f, " \"%s\"", crunner2->comment);
				fprintf(f, "\n");
				crunner2=crunner2->next;
			}
			crunner=crunner->next;
		}
		runner=runner->next;
	}
}

void escapeWrite(char *what, int length, MFILE *where)
{
	int i=0;
	
	for(i=0;i<length;i++)
	    switch(what[i]){
		case '"' : mfputc('\\', where); mfputc('"', where); break;
		case '%' : mfputc('%', where); mfputc('%', where); break;
		default  : mfputc(what[i], where);
	    }
}

void pexit(char *msg, const char *comment)
{
	fprintf(stderr, "%s%s\n", msg, comment);
	exit(1);
}