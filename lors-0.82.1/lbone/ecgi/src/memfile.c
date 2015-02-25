/*
   memfile.c

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
#include <memfile.h>
#include <stdarg.h>

MFILE *mfopen()
{
	MFILE *ret=(MFILE*)malloc(sizeof(MFILE));
	
	ret->data=(void*)malloc(MFILE_BLOCK);
	ret->blocks=1;
	ret->eof=false;
	ret->used=0;
	ret->pos=0;
	
	return(ret);
}

void mfclose(MFILE *mf)
{
	free(mf->data);
	free(mf);
}

int mfseek(MFILE *mf, int offset, int whence)
{
	mf->eof=false;

	if(whence==SEEK_END){
		mf->pos=mf->used;
		return(true);
	}

	if(whence==SEEK_CUR)
		offset=mf->pos+offset;
	
	if((whence==SEEK_SET)||(whence==SEEK_CUR)){
		if(offset<0){
			mf->pos=0;
			return(false);
		}
		mf->pos=offset;
		if(mf->pos>mf->used) mf->used=mf->pos;
		return(true);
	}

	return(false);
}

int mftell(MFILE *mf)
{
	return(mf->pos);
}

int mfeof(MFILE *mf)
{
	return(mf->eof);
}

int mfread(void *data, int times, int length, MFILE *mf)
{
	int bytesread=times*length;

	if(mf->pos+bytesread>mf->used)
		bytesread=mf->used-mf->pos;
		
	memcpy(data, (void*)((int)mf->data+mf->pos), bytesread);
	
	mf->pos+=bytesread;
	if(mf->pos==mf->used)
		mf->eof=true;
		
	return(bytesread);
}

int mfwrite(void *data, int times, int length, MFILE *mf)
{
	int newend=mf->pos+(times*length);
	
	while(newend+1>(mf->blocks*MFILE_BLOCK)){
		mf->data=(void*)realloc(mf->data, ((newend/MFILE_BLOCK)+1)*MFILE_BLOCK);
		mf->blocks=(newend/MFILE_BLOCK)+1;
	}
	if(mf->data==NULL)
		return(-2);

	memcpy((void*)((int)mf->data+mf->pos), data, times*length);
	
	if(newend>mf->used)	
		mf->used=newend;
	mf->pos=newend;
	mf->eof=true;
	
	return(times*length);
}

int mfgetc(MFILE *mf)
{
	int ret=*(char*)((int)mf->data+mf->pos);
	
	mf->pos++;
	if(mf->pos>=mf->used){
		mf->pos=mf->used;
		mf->eof=1;
		ret=EOF;
	}
	
	return(ret);
}

int mfputc(unsigned char c, MFILE *mf)
{
	int newend=mf->pos+1;
	
	if(newend+1>(mf->blocks*MFILE_BLOCK)){
		mf->data=(void*)realloc(mf->data, ((newend/MFILE_BLOCK)+1)*MFILE_BLOCK);
		mf->blocks=(newend/MFILE_BLOCK)+1;
	}
	if(mf->data==NULL)
		return(-2);

	*(char*)((int)mf->data+newend-1)=c;
	
	if(newend>mf->used)	
		mf->used=newend;
	mf->pos=newend;
	mf->eof=true;
	
	return(1);
}

int mfprintf(MFILE *mf, const char *format, ...)
{
	va_list ap;
	int max, used=-1, oldpos=mf->pos;
	
	while(used<0){
		max=mf->blocks*MFILE_BLOCK-mf->pos;
		va_start(ap, (void*)format);
		used=vsnprintf((char*)((int)mf->data+mf->pos), max, format, ap);
		va_end(ap);
		if(used<0){
			mf->data=(void*)realloc(mf->data, (mf->blocks+1)*MFILE_BLOCK);
			mf->blocks++;	
		}
	}
	
	mf->pos=oldpos+used;
	if(mf->pos>mf->used)
		mf->used=oldpos+used;
	return(used);
}

int mfMFileToFile(MFILE *mf, FILE *f)
{
	if(f==NULL || mf==NULL) return(false);

	if(fwrite(mf->data, 1, mf->used, f)!=mf->used)
		return(false);

	return(true);
}

int mfFileToMFile(FILE *f, MFILE *mf)
{
	int length, oldpos, ret;
	void *buf;
	if(f==NULL || mf==NULL) return(false);
	oldpos=ftell(f);

	fseek(f, 0, SEEK_END);
	length=ftell(f);
	fseek(f, oldpos, SEEK_SET);
	
	if((buf=malloc(length-oldpos))==NULL) return(false);
	
	fread(buf, 1, length-oldpos, f);
	fseek(f, oldpos, SEEK_SET);
	ret=mfwrite(buf, 1, length, mf);
	free(buf);
	
	return(ret);
}

int mfFileToMFileN(FILE *f, MFILE *mf, int count)
{
	int ret;
	void *buf;
	if(f==NULL || mf==NULL || count==0) return(false);
	
	if((buf=malloc(count))==NULL) return(false);
	
	fread(buf, 1, count, f);
	ret=mfwrite(buf, 1, count, mf);
	free(buf);
	
	return(ret);
}

const char *mfGetData(MFILE *mf)
{
	*(char*)((int)mf->data+mf->used)=0;
	return((char*)(mf->data));
}

const char *mfGetDataAt(MFILE *mf, int at)
{
	*(char*)((int)mf->data+mf->used)=0;
	return((char*)((int)mf->data+at));
}

int mfSetLength(MFILE *mf, int length)
{
	if(mf==NULL) return(false);
	
	mf->used=length;
	if(mf->pos>mf->used)
		mf->pos=mf->used;
	
	return(true);
}
int mfGetLength(MFILE *mf)
{
	return(mf->used);
}

#ifdef MFILE_MAIN
int main()
{
	MFILE *mf;
	FILE *f;
	char buf[1200];
	int length;
	
	mf=mfopen();
	f=fopen("/etc/squid.conf.rpmsave", "r");
	
	printf("Character wise reading ...\n");
	while(!feof(f))
		mfputc(fgetc(f), mf);
	mfseek(mf, 0, SEEK_SET);
	
	while(!mfeof(mf))
		printf("%c", mfgetc(mf));

	mfseek(mf, 1, SEEK_SET); fseek(f, 1, SEEK_SET);
	printf("seek - file: %d-%c - mfile: %d-%c\n", ftell(f), fgetc(f), mftell(mf), mfgetc(mf));
	mfseek(mf, 125, SEEK_SET); fseek(f, 125, SEEK_SET);
	printf("seek - file: %c - mfile: %d-%c\n", fgetc(f), mftell(mf), mfgetc(mf));
	mfseek(mf, 10021, SEEK_SET); fseek(f, 10021, SEEK_SET);
	printf("seek - file: %c - mfile: %c\n", fgetc(f), mfgetc(mf));
	
	mfclose(mf);fseek(f, 0, SEEK_SET);mf=mfopen();
	printf("Blockwise reading ...\n");
	while(!feof(f)){
		length=fread(buf, 1, 1200, f);
		mfwrite(buf, 1, length, mf);
	}

	mfseek(mf, 0, SEEK_SET);
	while(!mfeof(mf)){
		length=mfread(buf, 1, 1200, mf);
		if(length!=1200)
			buf[length]=0;
		printf("%s", buf);
	}
	
	mfseek(mf, 1, SEEK_SET); fseek(f, 1, SEEK_SET);
	printf("seek - file: %d-%c - mfile: %d-%c\n", ftell(f), fgetc(f), mftell(mf), mfgetc(mf));
	mfseek(mf, 125, SEEK_SET); fseek(f, 125, SEEK_SET);
	printf("seek - file: %c - mfile: %d-%c\n", fgetc(f), mftell(mf), mfgetc(mf));
	mfseek(mf, 1021, SEEK_CUR); fseek(f, 1021, SEEK_CUR);
	printf("seek - file: %d-%c - mfile: %d-%c\n", ftell(f), fgetc(f), mftell(mf), mfgetc(mf));
	mfseek(mf, -21, SEEK_CUR); fseek(f, -21, SEEK_CUR);
	printf("seek - file: %d-%c - mfile: %d-%c\n", ftell(f), fgetc(f), mftell(mf), mfgetc(mf));
	
	mfcopy(mf, "./memfile.tmp", "w");

	mfclose(mf);	
	fclose(f);
	exit(0);
}
#endif
