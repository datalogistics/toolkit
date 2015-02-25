#include "config.h"
#include <stdio.h>       /* FILE, et al */
#include <errno.h>       /* errno */
#include <unistd.h>      /* close() unlink() */
#include <string.h>      /* memset() strlen() */
#include <stdlib.h>      /* malloc() */
#include "diagnostic.h"  /* FAIL() LOG() WARN() */
#include "strutil.h"     /* vstrncpy() SAFESTRCPY() */
#include "osutil.h"      /* CurrentTime() */
#include "hosts.h"
#include "host_protocol.h"
#include "nws_state.h"
#include "exp_protocol.h"
#include "nws_cache.h"
#include "fbuff.h"

#ifdef WITH_NETLOGGER
#include "logging.h"
#endif



RCache RCHash[RC_HASH_SIZE];
RCache RCFlist;
RCache RCFlist_t;
int RCEntry_count;
int RCMemorySize;
int RC_entries = RC_ENTRIES_DEFAULT;

RCache GetRCListHead()
{
	RCache r;

	if(RCFlist == NULL)
		return(NULL);

	r = RCFlist;

	if(r->fnext != NULL)
	{
		r->fnext->fprev = NULL;
		RCFlist = RCFlist->fnext;
	}

	r->fnext = NULL;

	if(RCFlist_t == r)
	{
		RCFlist_t = RCFlist = NULL;
	}


	return(r);
}

void
AddRCList(RCache r)
{
	if(RCFlist_t == NULL)
	{
		RCFlist_t = RCFlist = r;
		r->fnext = NULL;
		r->fprev = NULL;
		return;
	}

	RCFlist_t->fnext = r;
	r->fprev = RCFlist_t;
	r->fnext = NULL;
	RCFlist_t = r;

	return;
}

void
RemoveRCList(RCache r)
{
	if(RCFlist == r)
	{
		RCFlist = r->fnext;
	}

	if(RCFlist_t == r)
	{
		RCFlist_t = r->fprev;
	}

	if(r->fnext != NULL)
	{
		r->fnext->fprev = r->fprev;
	}

	if(r->fprev != NULL)
	{
		r->fprev->fnext = r->fnext;
	}

	r->fnext = NULL;
	r->fprev = NULL;

	return;
}



unsigned int RCHashIndex(const char *fname)
{
	int i;
	int ind;

	ind = 0;
	for(i=0; i < RC_FNAME_SIZE; i++)
	{
		if(fname[i] == 0)
			break;
		ind += (unsigned char)fname[i];
	}

	return(ind % RC_HASH_SIZE);
}


RCache InitRCacheEntry(const char *fname, int size)
{
	RCache r;

	r = (RCache)malloc(sizeof(struct record_cache));
	if(r == NULL)
	{
		ERROR("InitRCacheEntry: out of memory\n");
		goto fail;
	}

	r->r_timestamp = InitFBuff(size);
	if(r->r_timestamp == NULL)
	{
		ERROR("InitRCacheEntry: out of memory\n");
		goto fail;
	}

	r->r_seqno = InitFBuff(size);
	if(r->r_seqno == NULL)
	{
		ERROR("InitRCacheEntry: out of memory\n");
		goto fail;
	}

	r->r_timeout = InitFBuff(size);
	if(r->r_timeout == NULL)
	{
		ERROR("InitRCacheEntry: out of memory\n");
		goto fail;
	}

	r->v_value = InitFBuff(size);
	if(r->v_value == NULL)
	{
		ERROR("InitRCacheEntry: out of memory\n");
		goto fail;
	}

	r->v_timestamp = InitFBuff(size);
	if(r->v_timestamp == NULL)
	{
		ERROR("InitRCacheEntry: out of memory\n");
		goto fail;
	}

	r->hnext = NULL;
	r->hprev = NULL;
	r->fnext = NULL;
	r->fprev = NULL;
	strncpy(r->fname,fname,sizeof(r->fname));

	RCEntry_count++;

	return(r);

fail:
	if(r->r_timestamp != NULL)
		FreeFBuff(r->r_timestamp);
	if(r->r_seqno != NULL)
		FreeFBuff(r->r_seqno);
	if(r->r_timeout != NULL)
		FreeFBuff(r->r_timeout);
	if(r->v_timestamp != NULL)
		FreeFBuff(r->v_timestamp);
	if(r->v_value != NULL)
		FreeFBuff(r->v_value);
	free(r);
	return(NULL);
}

void
FreeRCacheEntry(RCache r)
{
	int ind;

	ind = RCHashIndex(r->fname);

	if(RCHash[ind] == r)
	{
		RCHash[ind] = r->hnext;
	}

	if(RCFlist == r)
	{
		RCFlist = r->fnext;
	}

	if(r->hprev != NULL)
		r->hprev->hnext = r->hnext;
	if(r->hnext != NULL)
		r->hnext->hprev = r->hprev;

	if(r->fprev != NULL)
		r->fprev->fnext = r->fnext;
	if(r->fnext != NULL)
		r->fnext->fprev = r->fprev;


	if(r->r_timestamp != NULL)
		FreeFBuff(r->r_timestamp);
	if(r->r_seqno != NULL)
		FreeFBuff(r->r_seqno);
	if(r->r_timeout != NULL)
		FreeFBuff(r->r_timeout);
	if(r->v_timestamp != NULL)
		FreeFBuff(r->v_timestamp);
	if(r->v_value != NULL)
		FreeFBuff(r->v_value);

	RCEntry_count--;
	free(r);

	return;
}


RCache FindRCacheEntry(const char *fname)
{
	RCache r;
	unsigned int ind;
	int found;

	/*
	 * if caching is disabled
	 */
	if(RC_ENTRIES == 0)
	{
		return(NULL);
	}

	ind = RCHashIndex(fname);

	found = 0;
	for(r = RCHash[ind]; r != NULL; r = r->hnext)
	{
		if(strncmp(r->fname,fname,sizeof(r->fname)) == 0)
		{
			found = 1;
			break;
		}
	}

	if(found)
	{
		RemoveRCList(r);
		AddRCList(r);
		return(r);
	}
	else
	{
		return(NULL);
	}
}


RCache GetRCacheEntry(const char *fname)
{
	RCache r;
	int ind;

	/*
	 * if caching is disabled
	 */
	if(RC_ENTRIES == 0)
	{
		return(NULL);
	}

	r = FindRCacheEntry(fname);

	if(r != NULL)
	{
		return(r);
	}

	if(RCEntry_count >= RC_ENTRIES)
	{
		r = GetRCListHead();
		FreeRCacheEntry(r);
	}

	r = InitRCacheEntry(fname,RCMemorySize);
	if(r == NULL)
	{
		ERROR("GetRCacheEntry: no space\n");
		return(NULL);
	}
	AddRCList(r);

	ind = RCHashIndex(fname);

	if(RCHash[ind] == NULL)
	{
		RCHash[ind] = r;
		r->hnext = NULL;
		r->hprev = NULL;
	}
	else
	{
		r->hprev = NULL;
		r->hnext = RCHash[ind];
		RCHash[ind]->hprev = r;
		RCHash[ind] = r;
	}

	return(r);
}

int
ReadStateRecCache(RCache r,
		  int readIndex,
		  int recordSize,
		  double *timeStamp,
		  double *seqNo,
		  double *timeOut,
		  char *data,
		  int dataSize)
{
	char buf[MAX_RECORD_SIZE+1];
	int i;

	*timeStamp = F_VAL(r->r_timestamp,readIndex);
	*seqNo = F_VAL(r->r_seqno,readIndex);
	*timeOut = F_VAL(r->r_timeout,readIndex);

	memset(buf,0,sizeof(buf));
	sprintf(buf,WEXP_FORMAT,
			F_VAL(r->v_timestamp,readIndex),
			F_VAL(r->v_value,readIndex));
	for(i=0; i < recordSize; i++)
	{
		if(buf[i] == 0)
			data[i] = ' ';
		else
			data[i] = buf[i];
		if(i >= dataSize)
			break;
	}

	data[recordSize] = '\0';

	return(1);
}



int
ReadStateCache(RCache r,
	       char *state,
	       int count,
	       int max_size,
	       double earliest_seq_no,
	       double *out_time_out,
	       double *out_seq_no,
               int *out_count,
	       int *out_recordsize)
{
	char *curr;
	double now;
	int i;
	char *endOfBuffer;
	double headSeqNo;
	int readIndex;
	double recordSeqNo;
	double recordTimeOut;
	double recordTimeStamp;
	int recordSize;
	char dummy[MAX_RECORD_SIZE];

	/*
	 * figure out how big the records must be
	 */
	sprintf(dummy,WEXP_FORMAT,1123456789.0,12345.12345);
	recordSize = strlen(dummy);
	

	memset(state,0,max_size);
	now = CurrentTime();
	curr = state;
	endOfBuffer = state + max_size;
	headSeqNo = 0.0;

	for(readIndex = F_FIRST(r->r_timestamp), i = 0;
		(readIndex != F_TAIL(r->r_timestamp)) && (i < count);
		 readIndex = MODPLUS(readIndex,1,F_SIZE(r->r_timestamp)), i++)
	{
		if((endOfBuffer - curr) < recordSize)
			/* We've exhausted the return buffer. */
			break;

		if(!ReadStateRecCache(r,
				      readIndex,
				      recordSize,
				      &recordTimeStamp,
				      &recordSeqNo,
				      &recordTimeOut,
				      curr,
				      (int)(endOfBuffer - curr)))
		{
			FAIL("ReadStateCache: serious error\n");
		}

		if((recordTimeStamp + recordTimeOut) < now)
		       /* Dead record */
			break;

		if(recordSeqNo <= earliest_seq_no)
			/* No more records of interest */
			break;

		if(i == 0)
		{
			*out_recordsize = strlen(curr);
			headSeqNo = recordSeqNo;
		}

		curr += strlen(curr);
		*curr = '\0';
	}

	*out_count = i;
	*out_seq_no = headSeqNo;
	*out_time_out = recordTimeOut;

	return(1);
}


/*
 * just updates the data to the end of the time series
 */
int
AppendStateCache(RCache r,
		double time_out,
		double seq_no,
		const char *state,
		int stateSize)
{
	double ts;
	double value;
	double now;
	char *c;

	now = CurrentTime();

	UpdateFBuff(r->r_timestamp,now);
	UpdateFBuff(r->r_timeout,time_out);
	UpdateFBuff(r->r_seqno,seq_no);

	c = (char *)state;
	ts = strtod(c,&c);
	UpdateFBuff(r->v_timestamp,ts);

	value = strtod(c,&c);
	UpdateFBuff(r->v_value,value);

	return(1);
}
/*
 * puts data into the fbuff according to the ts value that comes
 * from the record
 */
int
WriteStateCache(RCache r,
		double time_out,
		double seq_no,
		const char *state,
		int stateSize)
{
	double ts;
	double value;
	double now;
	char *c;
	int curr_i;
	int tail;

	now = CurrentTime();
	c = (char *)state;
	ts = strtod(c,&c);
	value = strtod(c,&c);

	curr_i = F_FIRST(r->v_timestamp); 

	if(IS_EMPTY(r->v_timestamp) || (F_VAL(r->v_timestamp,curr_i) <= ts)
			|| (F_VAL(r->r_seqno,curr_i) <= seq_no))
	{
		AppendStateCache(r,
				 time_out,
				 seq_no,
				 state,
				 stateSize);
		return(1);
	}

	tail = F_TAIL(r->v_timestamp);

	while((F_VAL(r->v_timestamp,curr_i) > ts) ||
		(F_VAL(r->r_seqno,curr_i) > seq_no))
	{
		curr_i = MODPLUS(curr_i,1,F_SIZE(r->v_timestamp));
		if(curr_i == tail)
		{
			break;
		}
	}

	F_VAL(r->r_timestamp,curr_i) = now;
	F_VAL(r->r_timeout,curr_i) = time_out;
	F_VAL(r->r_seqno,curr_i) = seq_no;

	F_VAL(r->v_timestamp,curr_i) = ts;
	F_VAL(r->v_value,curr_i) = value;

	if((curr_i == tail) && 
	(MODPLUS(curr_i,1,F_SIZE(r->v_timestamp))) != F_LAST(r->v_timestamp))
	{
		F_TAIL(r->r_timestamp) = MODPLUS(F_TAIL(r->r_timestamp),
					         1,
						 F_SIZE(r->r_timestamp));
		F_TAIL(r->r_timeout) = MODPLUS(F_TAIL(r->r_timeout),
					         1,
						 F_SIZE(r->r_timeout));
		F_TAIL(r->r_seqno) = MODPLUS(F_TAIL(r->r_seqno),
					         1,
						 F_SIZE(r->r_seqno));
		F_TAIL(r->v_timestamp) = MODPLUS(F_TAIL(r->v_timestamp),
					         1,
						 F_SIZE(r->v_timestamp));
		F_TAIL(r->v_value) = MODPLUS(F_TAIL(r->v_value),
					         1,
						 F_SIZE(r->v_value));
	}

	return(1);
}


#if defined TEST

int debug;

#define TIMEOUT 12345678

main(int argc, char *argv[])
{

	char *fname = "test.1";
	char *fname2 = "test.2";
	char *fname3 = "test.3";
	RCache r;
	char cbuf[3*MAX_RECORD_SIZE];
	char buf[3*MAX_RECORD_SIZE];
	double out_to;
	double out_seqno;
	int out_count;
	int out_recsize;
	char string1[MAX_RECORD_SIZE];
	char string2[MAX_RECORD_SIZE];
	double seq_no;
	double old_seqno;

	RCMemorySize = 2000;
	

	r = GetRCacheEntry(fname);
	sprintf(string1,WEXP_FORMAT, 
		10.0, 
		25.7);
	seq_no = CurrentTime();
	WriteStateCache(r,TIMEOUT,seq_no,string1,strlen(string1));
	WriteState(fname,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string1,
		   strlen(string1));

	r = GetRCacheEntry(fname);
	sprintf(string2,WEXP_FORMAT, 
		11.0, 
		33.7);
	seq_no += 1;
	WriteStateCache(r,TIMEOUT,seq_no,string2,strlen(string2));
	WriteState(fname,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string2,
		   strlen(string2));

	r = GetRCacheEntry(fname);
	ReadStateCache(r,
		       cbuf,
		       2,
		       sizeof cbuf,
		       0.0,
		       &out_to,
		       &out_seqno,
		       &out_count,
		       &out_recsize);

	printf("C -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			cbuf);

	ReadState(fname,
		  buf,
		  2,
		  sizeof buf,
		  0.0,
		  &out_to,
		  &out_seqno,
		  &out_count,
		  &out_recsize);

	printf("F -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			buf);


	r = GetRCacheEntry(fname2);
	sprintf(string1,WEXP_FORMAT, 
		50.0, 
		100.7);
	seq_no = CurrentTime();
	WriteStateCache(r,TIMEOUT,seq_no,string1,strlen(string1));
	WriteState(fname2,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string1,
		   strlen(string1));

	r = GetRCacheEntry(fname2);
	sprintf(string2,WEXP_FORMAT, 
		51.0, 
		133.7);
	seq_no += 1;
	WriteStateCache(r,TIMEOUT,seq_no,string2,strlen(string2));
	WriteState(fname2,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string2,
		   strlen(string2));

	r = GetRCacheEntry(fname2);
	ReadStateCache(r,
		       cbuf,
		       2,
		       sizeof cbuf,
		       0.0,
		       &out_to,
		       &out_seqno,
		       &out_count,
		       &out_recsize);

	printf("C -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			cbuf);

	ReadState(fname2,
		  buf,
		  2,
		  sizeof buf,
		  0.0,
		  &out_to,
		  &out_seqno,
		  &out_count,
		  &out_recsize);

	printf("F -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			buf);

	r = GetRCacheEntry(fname3);
	sprintf(string1,WEXP_FORMAT, 
		100.0, 
		307.7);
	seq_no = CurrentTime();
	WriteStateCache(r,TIMEOUT,seq_no,string1,strlen(string1));
	WriteState(fname3,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string1,
		   strlen(string1));

	r = GetRCacheEntry(fname3);
	sprintf(string2,WEXP_FORMAT, 
		101.0, 
		333.7);
	seq_no += 1;
	old_seqno = seq_no;
	WriteStateCache(r,TIMEOUT,seq_no,string2,strlen(string2));
	WriteState(fname3,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string2,
		   strlen(string2));

	r = GetRCacheEntry(fname3);
	ReadStateCache(r,
		       cbuf,
		       2,
		       sizeof cbuf,
		       0.0,
		       &out_to,
		       &out_seqno,
		       &out_count,
		       &out_recsize);

	printf("C -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			cbuf);

	ReadState(fname3,
		  buf,
		  2,
		  sizeof buf,
		  0.0,
		  &out_to,
		  &out_seqno,
		  &out_count,
		  &out_recsize);

	printf("F -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			buf);

	/*
	 * three test starts here
	 */
	r = GetRCacheEntry(fname3);
	sprintf(string1,WEXP_FORMAT, 
		102.0, 
		406.765);
	seq_no += 1;
	WriteStateCache(r,TIMEOUT,seq_no,string1,strlen(string1));
	WriteState(fname3,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string1,
		   strlen(string1));
	ReadStateCache(r,
		       cbuf,
		       3,
		       sizeof cbuf,
		       0.0,
		       &out_to,
		       &out_seqno,
		       &out_count,
		       &out_recsize);

	printf("C3 -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			cbuf);
	sprintf(string1,WEXP_FORMAT, 
		101.0, 
		777.7777);
	seq_no = old_seqno;
	WriteStateCache(r,TIMEOUT,seq_no,string1,strlen(string1));
	WriteState(fname3,
		   RCMemorySize,
		   TIMEOUT,
		   seq_no,
		   string1,
		   strlen(string1));
	ReadStateCache(r,
		       cbuf,
		       3,
		       sizeof cbuf,
		       0.0,
		       &out_to,
		       &out_seqno,
		       &out_count,
		       &out_recsize);

	printf("C3 -- %10.0f %10.0f %s***\n",
			out_seqno,
			out_to,
			cbuf);

	return(0);

}
#endif
