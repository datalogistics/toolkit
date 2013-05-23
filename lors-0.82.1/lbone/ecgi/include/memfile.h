/*
   memfile.h - fast block allocating "memory files"

   ECgiLib -- Easy CGI Library -- Main header file
   http://www.global-owl.de/ecgi/

   Copyright (C) 2000 by Sven Dawitz <Sven at Dawitz.de>
   based on cgic by Todor Prokopov <koprok@newmail.net>

   Distributed under the GNU General Public License, see the file COPYING for
   more details.
*/

#ifndef ECGI_MF_H /* Prevent multiple includes */
#define ECGI_MF_H

/* in what block should mem be increased */
#define MFILE_BLOCK 4096

#ifndef true
	#define true	(1==1)
	#define false	(0==1)
#endif

typedef struct S_MFILE{
	void *data;

	int blocks;
	int eof;
	int used;
	int pos;
}MFILE;

/**************************************************************
 * FUNCTIONS 100% COMPATIBLE WITH USUAL F* FUNCTIONS - I HOPE *
 **************************************************************/
MFILE*		mfopen();
void		mfclose(MFILE *mf);
int		mfseek(MFILE *mf, int offset, int whence);
int 		mftell(MFILE *mf);
int 		mfeof(MFILE *mf);
int 		mfread(void *data, int times, int length, MFILE *mf);
int 		mfwrite(void *data, int times, int length, MFILE *mf);
int 		mfgetc(MFILE *mf);
int 		mfputc(unsigned char c, MFILE *mf);
int		mfprintf(MFILE *mf, const char *format, ...);

/************************
 * ADDITIONAL FUNCTIONS *
 ************************/
int 		mfMFileToFile(MFILE *mf, FILE *f);
int 		mfFileToMFile(FILE *f, MFILE *mf);
int		mfFileToMFileN(FILE *f, MFILE *mf, int count);
const char*	mfGetData(MFILE *mf);
const char*	mfGetDataAt(MFILE *mf, int start);
int		mfSetLength(MFILE *mf, int length);
int 		mfGetLength(MFILE *mf);

#endif /* ECGI_MF_H */
