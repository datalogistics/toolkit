/*
   ecgitk.c

   ECgiLib -- Easy CGI Library -- Main header file
   http://www.global-owl.de/ecgi/

   Copyright (C) 2000 by Sven Dawitz <Sven at Dawitz.de>
   based on cgic by Todor Prokopov <koprok@newmail.net>

   Distributed under the GNU General Public License, see the file COPYING for
   more details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <ecgi.h>
#include <ecgitk.h>

int ctkRedirect(const char *format, ...)
{
	char buf[4096];
	int used;
	va_list ap;
	
	va_start(ap, (void*)format);
	used=vsnprintf(buf, 4095, format, ap);
	va_end(ap);
	
	if(used<0) return(false);
	
	if(!strncmp(buf, "http:", 5))
		printf("Content-Type: text/html\nLocation: %s\n\n", buf);
	else
		printf("Content-Type: text/html\nLocation: http://%s%s\n\n", getenv("SERVER_NAME"), buf);
	printf("<html><head><title>Redirect to %s</title></head><body><font face=\"Verdana, Arial, Helvetica\" size=3>Redirecting to <a href=\"%s\">%s</a></font></body></html>", 
		buf, buf, buf);
	
	return(true);
}

const char *ctkGetSessionID()
{
	char *buf, tbuf[40];
	time_t t;
	const char *addr, *port;
	int bi=0, i;
	
	t=time(NULL);
	addr=getenv("REMOTE_ADDR");
	port=getenv("REMOTE_PORT");
	if(addr==NULL || port==NULL) return(NULL);
	buf=malloc(40);

	snprintf(tbuf, 39, "%d", (int)t);
	for(i=0;tbuf[i]!=0 && bi<40;i++)
		buf[bi++]=tbuf[i]+20;
	for(i=0;addr[i]!=0 && bi<40;i++)
		if(addr[i]!='.')	buf[bi++]=addr[i]+30;
		else			buf[bi++]='A';
	for(i=0;port[i]!=0 && bi<40;i++)
		buf[bi++]=port[i]+60;
	buf[bi]=0;
	
	return(buf);
}

const char *ctkTimeToHDate(time_t time)
{
	struct tm *t=localtime(&time);
	char *ret=malloc(40);
	
	snprintf(ret, 39, "%02d.%02d.%04d", t->tm_mday, t->tm_mon+1, t->tm_year+1900);
	
	return(ret);
}

time_t ctkHDateToTime(const char *hdate)
{
	time_t ret;
	struct tm t;
	char *day, *mon, *year;
	
	if(strlen(hdate)!=10 || hdate[2]!='.' || hdate[5]!='.') 
		return(0);
	day=(char*)hdate;
	day[2]=0; day[5]=0;
	mon=(char*)(hdate+3);
	year=(char*)(hdate+6);

	memset(&t, 0, sizeof(struct tm));
	t.tm_year=atoi(year)-1900;
	t.tm_mon=atoi(mon)-1;
	t.tm_mday=atoi(day);
	
	ret=mktime(&t);
	
	return(ret);
}

/*
const char *ctkTimeToHTime(time_t time);
const char *ctkTimeToHDateTime(time_t time);
time_t ctkHTimeToTime(const char *htime);
time_t ctkHDateTimeToTime(const char *hdatetime);
*/
