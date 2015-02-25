/*
   ecgi.h

   ECgiLib -- Easy CGI Library -- Main header file
   http://www.global-owl.de/ecgi/

   Copyright (C) 2000 by Sven Dawitz <Sven at Dawitz.de>
   based on cgic by Todor Prokopov <koprok@newmail.net>

   Distributed under the GNU General Public License, see the file COPYING for
   more details.
*/

#ifndef ECGI_H /* Prevent multiple includes */
#define ECGI_H
#include <memfile.h>	/* MUST BE HERE - WE ALLWAYS NEED IT ... */

#define CgiKindValue	 1
#define CgiKindFile	 2
#define CgiKindFileEmpty 3

typedef	void CgiPos;

/****************************************
 * OLD FUNCTION NAMES FOR COMPATIBILITY *
 ****************************************/
#define	cgi_init()	cgiInit()
#define cgi_param(name) cgiParam(name)
#define cgi_done()	cgiDone()
#define cgi_strerror(no) cgiStrError(no)

/*****************************************
 * TRADITIONAL FUNCTIONS OF OLD CGIC 0.5 *
 *****************************************/
int 		cgiInit();
const char*	cgiParam(const char *name);
void		cgiDone();
const char*	cgiStrError(int errnum);

/*******************************
 * NEW FILE DEPENDED FUNCTIONS *
 *******************************/
void		cgiSetMaxFileSize(int size);
void 		cgiSetMaxSize(int size);
int		cgiGetKind(const char *name);
const char*	cgiGetCTyp(const char *name);
MFILE*		cgiGetMFile(const char *name);
int		cgiMFileToFile(const char *name, const char *fname, const char *fopenmode);

/****************************
 * MISC OTHER NEW FUNCTIONS *
 ****************************/
const char*	cgiNameByValue(char *value);

/**********************************************************
 * FUNCTION FOR GETTING ALL DATA WITHOUT KNOWING THE NAME *
 **********************************************************/
const char*	cgiGetFirstName();
const char*	cgiGetNextName();
CgiPos*		cgiPosNext(CgiPos *lastpos);
const char*	cgiPosParam(CgiPos *where);
const char*	cgiPosName(CgiPos *where);
int		cgiPosGetKind(CgiPos *where);
const char*	cgiPosGetCTyp(CgiPos *where);
MFILE*		cgiPosGetMFile(CgiPos *where);
int		cgiPosMFileToFile(CgiPos *where, const char *fname, const char *fopenmode);

/*****************************************************************
 * FUNCTIONS FOR SAVING/LOADING ENVIRONMENT - GOOD FOR DEBUGGING *
 *****************************************************************/
int		cgiSaveDebugData(char *fname, char *fopenmode);
int		cgiLoadDebugData(char *fname);

/* the error number */
extern int cgi_errno;

#define ECGI_SUCCESS	0

#endif /* ECGI_H */
