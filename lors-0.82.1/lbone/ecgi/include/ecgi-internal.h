/*
   ecgi-internal.h - only for internal use!

   ECgiLib -- Easy CGI Library -- Main header file
   http://www.global-owl.de/ecgi/

   Copyright (C) 2000 by Sven Dawitz <Sven at Dawitz.de>
   based on cgic by Todor Prokopov <koprok@newmail.net>

   Distributed under the GNU General Public License, see the file COPYING for
   more details.
*/

#ifndef ECGI_INT_H /* Prevent multiple includes */
#define ECGI_INT_H

/***********************************************
 * THE STRUCTS/LIST WE STORE THE WHOLE SHIT IN *
 ***********************************************/

/* Building of save file:

int element count
<element>
...
<element>
int env count
<environment>
...
<environment>

element:
int type
int namelength
name
int ctypelength
ctyp
int mf-length
mfdata
int value count
<value>
...
<value>
*/

typedef struct _CgiValue{
	char *value;
	struct _CgiValue *next;
}CgiValue;

typedef struct _CgiElement {
	int type;
	char *name;
	char *ctyp;
	CgiValue *values;
	MFILE *mf;
	struct _CgiElement *next;
}CgiElement;

typedef struct _CGI{
	CgiElement *list;
	CgiElement *lastasked;
	CgiValue *lastret;
	CgiElement *lastvalasked;
	CgiElement *lastnameasked;
}Cgi;

/********************************
 * INTERNAL FUNCTION PROTOTYPES *
 ********************************/

/* List Stuff */
void listInit();
int listAddData(int type, const char *name, const char *value, const char *ctyp, MFILE *mf);
CgiElement *listGetByName(const char *name);
CgiElement *listAppendElement(int type, const char *name, const char *ctyp, MFILE *mf);
CgiValue *listAppendValue(CgiElement *where, const char *value);
int listHasValue(CgiElement *check, char *value);
void listFreeAll();
void listDump();	/* for test usage only ... dumps all vals to stdout ... */

/* Init/Parse Stuff */
int initGet();
int initPost();
int initMultiPart(const char *cont_type);
int parseMultiPart(char *boundary);
int parseMultiHead(char **name, char **fname, char **ctyp);
int parseQueryString(const char *str, int length); /* str==NULL - read from stdin */

/* Misc Help Functions */
void miscWriteData(FILE *f, const void *data, int length);
void *miscReadData(FILE *f);
void miscReadSetEnviron(FILE *f);
int miscStringDecode(char *s);
int miscFReadLn(FILE *f, MFILE *mf);
int miscStringToUInt(const char *str, unsigned int *res);
char *miscStringDelCrLf(char *str);

/**********************
 * OTHER USELESS SHIT *
 **********************/

extern char **environ;
/* external vars - here for real */
int cgi_errno=0;
/* global vars */
Cgi *c=NULL;
int maxfilesize=-1;
int maxsize=-1;
int init_called=false;
int init_complete=false;

#define NUMERRS 25
static const char *errmsgs[NUMERRS] = {
  "Success",
  "Unknown request method",
  "Repeated initialization attempt",
  "Null query string",
  "Unknown content type",
  "Invalid content length",
  "Unexpected end of input stream",
  "Input stream read error",
  "Maximum entry name length exceeded",
  "Maximum entry value length exceeded",
  "Invalid URL-encoded data",
  "Maximum number of entries exceeded",
  "Memory allocation error",
  "Maximum memory limit exceeded",
  "Invalid boundary string",
  "Null file upload directory",
  "Pathname length limit exceeded",
  "Extremely long line encountered",
  "Missing boundary string",
  "Error opening file for writing",
  "Error writing to file",
  "Error closing file",
  "Error changing file permissions",
  "Missing initial boundary string",
  "Error parsing content disposition"
};

#define CGIERR_UREQM      1  /* Unknown request method */
#define CGIERR_REINIT     2  /* Repeated initialization attempt */
#define CGIERR_NULQSTR    3  /* Null query string */
#define CGIERR_UCONTT     4  /* Unknown content type */
#define CGIERR_ICONTLEN   5  /* Invalid content length */
#define CGIERR_UEOINP     6  /* Unexpected end of input stream */
#define CGIERR_INPREAD    7  /* Input stream read error */
#define CGIERR_MAXNAMEE   8  /* Maximum entry name length exceeded */
#define CGIERR_MAXVALE    9  /* Maximum entry value length exceeded */
#define CGIERR_IURLENC   10  /* Invalid URL-encoded data */
#define CGIERR_MAXENTRSE 11  /* Maximum number of entries exceeded */
#define CGIERR_MALLOC    12  /* Memory allocation error */
#define CGIERR_MAXMEME   13  /* Maximum memory limit exceeded */
#define CGIERR_IBSTR     14  /* Invalid boundary string */
#define CGIERR_NULUPLD   15  /* Null file upload directory */
#define CGIERR_PATHMAXE  16  /* Pathname length limit exceeded */
#define CGIERR_LONGLN    17  /* Extremely long line encountered */
#define CGIERR_MBSTR     18  /* Missing boundary string */
#define CGIERR_FOPEN     19  /* Error opening file for writing */
#define CGIERR_FWRITE    20  /* Error writing to file */
#define CGIERR_FCLOSE    21  /* Error closing file */
#define CGIERR_CHMOD     22  /* Error changing file permissions */
#define CGIERR_MBBSTR    23  /* Missing initial boundary string */
#define CGIERR_DISPPARS  24  /* Error parsing content disposition */


#endif /* ECGI_INT_H */
