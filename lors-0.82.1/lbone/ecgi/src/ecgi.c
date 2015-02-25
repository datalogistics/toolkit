/*
   ecgi.c

   ECgiLib -- Easy CGI Library -- Main header file
   http://www.global-owl.de/ecgi/

   Copyright (C) 2000 by Sven Dawitz <Sven at Dawitz.de>
   based on cgic by Todor Prokopov <koprok@newmail.net>

   Distributed under the GNU General Public License, see the file COPYING for
   more details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <memfile.h>
#include <ecgi-internal.h>
#include <ecgi.h>

#define DefCheck(ret)	if(c==NULL || init_complete==false) if(cgiInit()==false) return(ret);
#define DefCheckNR	if(c==NULL || init_complete==false) if(cgiInit()==false) return;

/************************************************************
 *               EXPORTED FUNCTIONS TO USER                 *
 ************************************************************/

const char *cgiParam(const char *name)
{
	return cgiPosParam((CgiPos*)listGetByName(name));
}

int cgiInit()
{
	const char *str;

	if (init_called == true) { cgi_errno = CGIERR_REINIT; return false; }
	init_called = true;

	str = getenv("REQUEST_METHOD");
	if (str == NULL || (strcmp(str, "POST") && strcmp(str, "GET"))) 
	  	{ cgi_errno = CGIERR_UREQM; return false; }

	listInit();
	if (!strcmp(str, "POST"))
		if(initPost()==false)
			return(false);

	if (!strcmp(str, "GET"))
		if(initGet()==false)
			return(false);

	init_complete = true;
	return ECGI_SUCCESS;
}

void cgiDone()
{
	/* wierd - dont want to return, but macro needs parameter - hehe */
	DefCheckNR;
	
	listFreeAll();
}

const char *cgiStrError(int errnum)
{
	if (errnum < 0 || errnum >= NUMERRS)
		return "Unknown error";
	else
		return errmsgs[errnum];
}

void cgiSetMaxFileSize(int size)
{
	maxfilesize=size;
}

void cgiSetMaxSize(int size)
{
	maxsize=size;
}

int cgiGetKind(const char *name)
{
	return cgiPosGetKind((CgiPos*)listGetByName(name));
}

const char *cgiGetCTyp(const char *name)
{
	return cgiPosGetCTyp((CgiPos*)listGetByName(name));
}

MFILE *cgiGetMFile(const char *name)
{
	return cgiPosGetMFile((CgiPos*)listGetByName(name));
}

int cgiMFileToFile(const char *name, const char *fname, const char *fopenmode)
{
	return cgiPosMFileToFile((CgiPos*)listGetByName(name), fname, fopenmode);
}

const char *cgiNameByValue(char *value)
{
	CgiElement *r;
	DefCheck(NULL);
	if(value==NULL) return(NULL);
	
	if(c->lastvalasked!=NULL && listHasValue(c->lastvalasked, value))
		r=c->lastvalasked->next;
	else
		r=c->list;

	if(r->next==NULL) 
		return(NULL);

	while(r->next!=NULL){
		if(listHasValue(r, value)){
			c->lastvalasked=r;
			return(r->name);
		}
		r=r->next;
	}
	
	return(NULL);
}

const char *cgiGetFirstName()
{
	DefCheck(NULL);

	c->lastnameasked=NULL;

	if(c->list->next==NULL)
		return(NULL);

	
	c->lastnameasked=c->list;
	return(c->list->name);
}

const char *cgiGetNextName()
{
	DefCheck(NULL);

	if(c->lastnameasked==NULL)
		return(NULL);
	
	if(c->lastnameasked->next==NULL || c->lastnameasked->next->next==NULL)
		return(NULL);
	
	c->lastnameasked=c->lastnameasked->next;
	
	return(c->lastnameasked->name);
}

CgiPos *cgiPosNext(CgiPos *lastpos)
{
	CgiElement *lp=(CgiElement*)lastpos;
	DefCheck(NULL);

	if(lastpos==NULL){
		if(c->list->next!=NULL)
			return((CgiPos*)c->list);
		return(NULL);
	}
	
	if(lp->next==NULL || lp->next->next==NULL)
		return(NULL);

	lp=lp->next;
	return((CgiPos*)lp);
}

const char *cgiPosParam(CgiPos *where)
{
	CgiElement *w=(CgiElement*)where;
	DefCheck(NULL);

	/* call with NULL to reset the get */
	if(where==NULL){
		c->lastasked=NULL;
		c->lastret=NULL;
		return(NULL);
	}
	
	/* called with same param than last time */
	if(c->lastasked!=NULL && !strcmp(c->lastasked->name, w->name)){
		if(c->lastret==NULL || c->lastret->next==NULL) return(NULL);
		c->lastret=c->lastret->next;
		return(c->lastret->value);
	}
	
	/* NORMAL call :) */
	c->lastasked=w;
	c->lastret=w->values;
	if(c->lastret->next==NULL) return(NULL);
	return(w->values->value);
}

const char *cgiPosName(CgiPos *where)
{
	DefCheck(NULL);
	if(where==NULL) return(NULL);

	return(((CgiElement*)(where))->name);
}

int cgiPosGetKind(CgiPos *where)
{
	DefCheck(false);
	if(where==NULL) return(false);

	return(((CgiElement*)(where))->type);
}

const char *cgiPosGetCTyp(CgiPos *where)
{
	DefCheck(NULL);
	if(where==NULL) return(NULL);

	return(((CgiElement*)(where))->ctyp);
}

MFILE *cgiPosGetMFile(CgiPos *where)
{
	DefCheck(NULL);
	if(where==NULL) return(NULL);

	return(((CgiElement*)(where))->mf);
}

int cgiPosMFileToFile(CgiPos *where, const char *fname, const char *fopenmode)
{
	MFILE *mf;
	FILE *f;
	int ret;
	DefCheck(false);
	if(where==NULL) return(false);
	
	mf=((CgiElement*)(where))->mf;
	f=fopen(fname, fopenmode);
	
	if(f==NULL) return(false);
	ret=mfMFileToFile(mf, f);
	fclose(f);
	
	return(ret);
}

int cgiSaveDebugData(char *fname, char *fopenmode)
{
	FILE *f;
	CgiElement *runner=c->list;
	CgiValue *vrunner;
	int count=0, subcount, i=0, envcount=0;
	DefCheck(false);
	if((f=fopen(fname, fopenmode))==NULL) return(false);
	
	/* get count */
	while(runner->next!=NULL){
		count++;
		runner=runner->next;
	}
	runner=c->list;	
	fwrite(&count, 1, 4, f);
	
	/* save internal list */
	while(runner->next!=NULL){
		fwrite(&runner->type, 1, 4, f);
		miscWriteData(f, runner->name, -1);
		miscWriteData(f, runner->ctyp, -1);
		if(runner->mf!=NULL)
			miscWriteData(f, mfGetData(runner->mf), mfGetLength(runner->mf));
		else
			miscWriteData(f, NULL, 0);

		/* get value count */
		vrunner=runner->values;
		subcount=0;
		while(vrunner->next!=NULL) {
			subcount++;
			vrunner=vrunner->next;
		}
		vrunner=runner->values;
		fwrite(&subcount, 1, 4, f);
		
		/* save values */
		while(vrunner->next!=NULL){
			miscWriteData(f, vrunner->value, -1);
			vrunner=vrunner->next;
		}

		runner=runner->next;
	}
	
	/* get environment count */
	while(environ[i++]!=NULL) envcount++;
	fwrite(&envcount, 1, 4, f);

	/* save environment */
	i=0;
	while(environ[i]!=NULL)
		miscWriteData(f, environ[i++], -1);

	fclose(f);
	return(true);
}

int cgiLoadDebugData(char *fname)
{
	FILE *f=fopen(fname, "r");
	int count, subcount, type, mflen, envcount;
	MFILE *mf;
	char *name, *ctyp;
	CgiElement *where;
	if(init_complete==true || f==NULL) return(false);
	listInit();
	
	fread(&count, 1, 4, f);
	while(count-->0){
		fread(&type, 1, 4, f);
		name=miscReadData(f);
		ctyp=miscReadData(f);
		fread(&mflen, 1, 4, f);
		if(mflen>0){
			mf=mfopen();
			mfFileToMFileN(f, mf, mflen);
		}else
			mf=NULL;

		where=listAppendElement(type, name, ctyp, mf);
		fread(&subcount, 1, 4, f);
		
		while(subcount-->0)
			listAppendValue(where, miscReadData(f));
	}
	
	fread(&envcount, 1, 4, f);
	while(envcount-->0)
		miscReadSetEnviron(f);

	init_called = true;
	init_complete = true;
	fclose(f);
	return(true);
}

/************************************************************
 *                 LIST HANDLING FUNCTIONS                  *
 ************************************************************/

void listInit()
{
	c=malloc(sizeof(Cgi));
	c->list=malloc(sizeof(CgiElement));
	c->list->next=NULL;
	c->lastasked=NULL;
	c->lastret=NULL;
	c->lastvalasked=NULL;
	c->lastnameasked=NULL;
}

int listAddData(int type, const char *name, const char *value, const char *ctyp, MFILE *mf)
{
	CgiElement *hit=listGetByName(name);
	
	if(type==CgiKindFile && mfGetLength(mf)==0){
		mfclose(mf);
		mf=NULL;
		type=CgiKindFileEmpty;
	}

	if(hit==NULL)
		hit=listAppendElement(type, name, ctyp, mf);
	
	listAppendValue(hit, value);
	
	return(true);
}

CgiElement *listGetByName(const char *name)
{
	CgiElement *runner;
	if(name==NULL) return(NULL);
	runner=c->list;
	
	while(runner->next!=NULL){
		if(!strcmp(name, runner->name))
			return(runner);
		runner=runner->next;
	}
	
	return(NULL);
}

CgiElement *listAppendElement(int type, const char *name, const char *ctyp, MFILE *mf)
{
	CgiElement *runner=c->list;

	while(runner->next!=NULL) runner=runner->next;
	
	runner->next=malloc(sizeof(CgiElement));
	runner->next->next=NULL;
	
	runner->type=type;
	runner->name=(char*)strdup(name);
	if(ctyp!=NULL)
		runner->ctyp=(char*)strdup(ctyp);
	else
		runner->ctyp=(char*)strdup("");
	runner->mf=mf;
	
	runner->values=malloc(sizeof(CgiValue));
	runner->values->value=NULL;
	runner->values->next=NULL;
	
	return(runner);
}

CgiValue *listAppendValue(CgiElement *where, const char *value)
{
	CgiValue *runner=where->values;
	
	while(runner->next!=NULL) runner=runner->next;
	
	runner->next=malloc(sizeof(CgiValue));
	runner->next->value=NULL;
	runner->next->next=NULL;
	
	runner->value=(char*)strdup(value);
	
	return(runner);
}

int listHasValue(CgiElement *check, char *value)
{
	CgiValue *r;
	
	for(r=check->values; r->next!=NULL; r=r->next)
		if(!strcmp(r->value, value))
			return(true);

	return(false);
}

void listFreeAll()
{
	CgiElement *erunner=c->list, *etmp;
	CgiValue *vrunner, *vtmp;

	while(erunner->next!=NULL){
		vrunner=erunner->values;
		while(vrunner->next!=NULL){
			vtmp=vrunner->next;
			free(vrunner->value);
			free(vrunner);
			vrunner=vtmp;
		}
		free(vrunner);

		free(erunner->name);
		free(erunner->ctyp);
		if(erunner->type == CgiKindFile)
			mfclose(erunner->mf);

		etmp=erunner->next;
		free(erunner);
		erunner=etmp;
	}
	
	free(c);
}

/* only for debugging */
void listDump()
{
	CgiElement *erunner=c->list;
	CgiValue *vrunner;

	printf("Dumping List:\n");
	while(erunner->next!=NULL){
		printf("-----------------------------------\nEntry - Name: %20s CTyp: %20s\nValues:", erunner->name, erunner->ctyp);
		vrunner=erunner->values;
		while(vrunner->next!=NULL){
			printf("%20s ", vrunner->value);
			vrunner=vrunner->next;
		}
		printf("\n");

		if(erunner->type==CgiKindFileEmpty)
			printf("Empty File!\n");
		if(erunner->type==CgiKindFile)
			printf("Dumping File (Size: %d):\n************************\n%s\n************************\n", mfGetLength(erunner->mf), mfGetData(erunner->mf));

		erunner=erunner->next;
	}
}

/************************************************************
 *                    INIT / PARSE STUFF                    *
 ************************************************************/

int initGet(void)
{
	int query_string_len;
	const char *query_string;

	query_string = getenv("QUERY_STRING");
	if (query_string == NULL) { cgi_errno = CGIERR_NULQSTR; return false; }

	query_string_len = strlen(query_string);
	if (query_string_len != 0)
		return (parseQueryString(query_string, query_string_len));

	return true;
}

int initPost(void)
{
	const char *str;
	const char *mpfd="multipart/form-data";
	unsigned int content_length;

	/* get/check content length */
	str = getenv("CONTENT_LENGTH");
	if (str == NULL) { cgi_errno = CGIERR_ICONTLEN; return false; }
	if (!miscStringToUInt(str, &content_length)) { cgi_errno = CGIERR_ICONTLEN; return false; }
	if (content_length == 0) return(true);

	/* get/check content type */
	str = getenv("CONTENT_TYPE");
	if (str == NULL) { cgi_errno = CGIERR_UCONTT; return 0; }

	/* multipart encoded ? */
	if (!strncasecmp(str, mpfd, strlen(mpfd)))
		return(initMultiPart(str));

	/* "normal" url encoded ? */
	if (!strcasecmp(str, "application/x-www-form-urlencoded"))
		return(parseQueryString(NULL, content_length));

	cgi_errno = CGIERR_UCONTT;
	return false;
}

int initMultiPart(const char *str)
{
	char *endchars, *boundary=NULL;
	int i=0;

	while(*str!=0 && strncasecmp(str++, "boundary", strlen("boundary")));
	while(*str!=0 && *str!='=') str++;
	while(*str!=0 && *str==' ') str++;

	if(*str==0) { cgi_errno = CGIERR_IBSTR; return false; }

	if(*str++=='"')	endchars="\"";
	else		endchars=" \0";
	
	boundary=(char*)strdup("--"); i=2;
	while(strchr(endchars, *str)==NULL){
		boundary=realloc(boundary, i+3);
		boundary[i++]=*str++;
	}
	boundary[i]=0;

	return(parseMultiPart(boundary));
}

int parseMultiPart(char *boundary)
{
	int bound_len=strlen(boundary), type, startat, finish=false;
	char *name=NULL, *ctyp=NULL, *fname=NULL;
	MFILE *mf=mfopen();
		
	while((startat=miscFReadLn(stdin, mf))!=-1){

		if(strncmp(boundary, mfGetDataAt(mf, startat), bound_len))
			continue;

		/* new part found - insert old part and init new one...
		 * no insert at FIRST header ...
		 * at end of multipart ??? "--boundary--" line ? */
		if(!strncmp("--", mfGetDataAt(mf, mfGetLength(mf)-4), 2))
			finish=true;

		mfSetLength(mf, startat);

		if(name!=NULL) {
			/* strip memfile, since one more <cr><lf> or only <lf> at end */
			mf->used--;
			if(*(char*)((int)mf->data+mf->used-1) == '\r') mf->used--;

			if(type==CgiKindFile){
				listAddData(type, name, fname, ctyp, mf);
				mf=mfopen();
			}else
				listAddData(type, name, mfGetData(mf), ctyp, NULL);
		}
		
		if(finish==true) return(true);
			
		type=parseMultiHead(&name, &fname, &ctyp);

		mfSetLength(mf, 0);
	}

	mfclose(mf);
	free(name); free(fname); free(ctyp);
	return true;
}

/* this is REALLY a fuck ... why didnt they put the fname in a own line :( */
int parseMultiHead(char **name, char **fname, char **ctyp)
{
	char *endchars;
	const char *contt="Content-Type: ", *line;
	const char *contd="Content-Disposition: form-data; name=";
	int i, ret=0;
	MFILE *mf=mfopen(), *mfname=mfopen();
	
	free(*ctyp);
	(*ctyp)=strdup("");

	/* read till empty line appears - end of header ... */
	while((miscFReadLn(stdin, mf)>=0) && (line=mfGetData(mf)) &&
		!(line[0]=='\n' || (line[0]!=0 && line[0]=='\r' && line[1]=='\n'))){

		/* make sure, next lines starts at beginn of file again... */
		mfSetLength(mf, 0);

		/* "Content-Type: what/ever" line */
		if(!strncasecmp(line, contt, strlen(contt))){
			free(*ctyp);
			(*ctyp)=miscStringDelCrLf((char*)strdup((char*)(line+strlen(contt))));
		}

		/* "Content-Disposition: form-data; name="whatever"; filename="C:\fuck.txt"" - line */
		if(!strncasecmp(line, contd, strlen(contd))){
			i=strlen(contd);
			if(line[i]=='"') 	{endchars="\"\r\n\0"; i++;}
			else			endchars=";\r\n\0";

			/* parse name */
			while(strchr(endchars, line[i])==NULL)
				mfputc(line[i++], mfname);
			*name=realloc(*name, mfGetLength(mfname)+1);
			strcpy(*name, mfGetData(mfname));
			mfSetLength(mfname, 0);
			
			if(line[i]=='\"') i++;
			if(line[i]!=';') 	{ret=CgiKindValue; continue;}
			else			ret=CgiKindFile;
			
			/* we have a filename= part here - parse filename */
			while(line[i]!=0 && line[i]!='=') i++;
			i++;
			if(line[i]=='\"') 	{endchars="\"\r\n\0"; i++;}
			else			endchars=";\r\n\0";
			

			while(strchr(endchars, line[i])==NULL)
				mfputc(line[i++], mfname);
			if(mfGetLength(mfname)>0){
				*fname=realloc(*fname, mfGetLength(mfname)+1);
				strcpy(*fname, mfGetData(mfname));
				mfSetLength(mfname, 0);
			}else{
				*fname=realloc(*fname, 16);
				(*fname)[0]=0;
			}
		}
	}
	
	mfclose(mf); mfclose(mfname);
	return(ret);
}

/* little help func only for parseQueryString() - wont put it twice here .... */
/* not prototyped */
int locgetc(const char *str, unsigned int i){
	int c;

	if(str!=NULL) c=str[i];
	else{ 
		c=fgetc(stdin);
		if(c==EOF){ cgi_errno = CGIERR_INPREAD; }
	}
		
	return(c);
}

/* nearly untouched function from cgic0.5 - works fine */
int parseQueryString(const char *str, int length)	/* if str is NULL, read from stdin */
{
	unsigned int i = 0;
	int c, valuei, namei;
	/* char *name=(char*)strdup(""), *value=(char*)strdup(""); */
	char *name=malloc(length);	/* Fixed by BCH 1/15/01 */
	char *value=malloc(length);	/* Fixed by BCD 1/15/01 */
	
	strcpy(name, "");
	strcpy(value, "");
	
	while (i < length){
		namei=0;
		while (i < length){
			/* if str!=NULL => read string, we got from getenv - else, read stdin */
			c=locgetc(str, i);
			if (c == EOF) return false;
			i++;

			if (c == '=' || c == '&')
				break;
			/* name=realloc(name, namei+1); */ /* Fixed by BCH 1/15/01 */
			name[namei++]=c;
		}
		name[namei] = 0;

		/* After an entry name, always expecting to find '='. */
		if (c != '=') { cgi_errno = CGIERR_IURLENC; return false; }

		valuei=0;
		while (i < length){
			c = locgetc(str, i);
			if (c == EOF) return false;
			i++;

			if (c == '&' || c == '=')
				break;
			/* value=realloc(value, valuei+1); */ /* Fixed by BCH 1/15/01 */
			value[valuei++]=c;
		}

		/* If this isn't the last entry, expecting to find '&' after it. If it's
			the last, but there is '&' - error. */
		if (i < length){
			if (c != '&') 
				{ cgi_errno = CGIERR_IURLENC; return 0; }
			}else
				if (c == '&') 
					{ cgi_errno = CGIERR_IURLENC; return 0; }

			/* Here check is not needed, because valbuf_index is always less than
				CGI_MAXVAL and valbuf is CGI_MAXVAL+1 bytes long. */
			value[valuei] = 0;

			if (!miscStringDecode(name) || !miscStringDecode(value))
				{ cgi_errno = CGIERR_IURLENC; return 0; }

			if(!listAddData(CgiKindValue, name, value, NULL, NULL))
				return(false);
	}

	free(name); free(value);
	return true;
}

/************************************************************
 *                   MISC HELP FUNCTIONS                    *
 ************************************************************/

void miscWriteData(FILE *f, const void *data, int length)
{
	int zero=0;

	if(length==0 || data==NULL){
		fwrite(&zero, 1, 4, f);
		return;
	}
	if(length<0)
		length=strlen(data);
	
	fwrite(&length, 1, 4, f);
	fwrite(data, 1, length, f);
}

void *miscReadData(FILE *f)
{
	int length;
	void *ret;
	
	fread(&length, 1, 4, f);
	if(length==0) return((void*)strdup(""));

	ret=malloc(length+1);
	fread(ret, 1, length, f);
	*((char*)((int)ret+length))=0;	
	
	return(ret);
}

void miscReadSetEnviron(FILE *f)
{
	char *str=miscReadData(f);
	/* changed this, since setenv() is not supported by solaris
	 * leave old setenv() version in comments for now!
	char *name=str, *val, *hit;
	
	hit=strchr(str, '=');
	val=(char*)(hit+1);
	*hit=0;
	
	setenv(name, val, 1);
	free(str);
	*/
	putenv(str);
}

/* nearly untouched function from cgic0.5 - works fine */
int miscStringDecode(char *s)
{
	char *p = s;

	while (*s != '\0'){
		if (*s == '%'){
			s++;
			if (!isxdigit((int)*s))
				return 0;
			*p = (isalpha((int)*s) ? (*s & 0xdf) - 'A' + 10 : *s - '0') << 4;
			s++;
			if (!isxdigit((int)*s))
				return 0;
			*p += isalpha((int)*s) ? (*s & 0xdf) - 'A' + 10 : *s - '0';
		}else 
			if (*s == '+')
				*p = ' ';
			else
				*p = *s;
		s++;
		p++;
	}

	*p = '\0';
	return 1;
}

int miscFReadLn(FILE *f, MFILE *mf)
{
	int c, ret=mfGetLength(mf);

	if(feof(f)) return(-1);

	c=getc(f);
	while(c!=EOF && c!='\n'){
		mfputc(c, mf);
		c=getc(f);
	}
	if(c!=EOF) mfputc(c, mf);

	return(ret);
}

int miscStringToUInt(const char *str, unsigned int *value)
{
	char *tail;

	errno = 0;
	*value = (unsigned int)strtoul(str, &tail, 0);
	if (*tail != '\0' || errno != 0)
		return false;
	return true;
}

char *miscStringDelCrLf(char *str)
{
	int i;
	
	for(i=0;str[i]!=0 && str[i]!='\n' && str[i]!='\r'; i++);

	str[i]=0;

	return(str);
}
