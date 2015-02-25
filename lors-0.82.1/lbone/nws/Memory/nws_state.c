/* $Id: nws_state.c,v 1.41 2001/01/15 17:21:04 rich Exp $ */

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
#ifdef WITH_NETLOGGER
#include "logging.h"
#endif

#if defined(ENABLE_CACHE)
#include "nws_cache.h"
#endif

#if defined(ENABLE_CACHE)
char Dummy_buf[MAX_RECORD_SIZE];
#else
#define MODMINUS(a,b,m) (((a) - (b) + (m)) % (m))
#define MODPLUS(a,b,m) (((a) + (b)) % (m))
#endif


#define HEADER_FORMAT "%10d %10d %10d\n"
#define HEADER_LEN 33 /* Three 10-digit floats + 2 spaces and 1 new line */
#define OVERHEAD_LEN 33 /* Three 10-digit floats + 3 spaces */
#define STATE_FORMAT "%10.0f %10.0f %10.0f %s\n"

#ifdef WITH_NETLOGGER
#define STATE_FORMAT_NL "SEQN=%.0f TIMEOUT=%.0f"
#endif

/*
** Reads and parses the header record of #fd#, which must already be opened for
** reading.  Returns the index of the next record to write, index of the oldest
** record, maximum records, and record size in #head#, #tail#, #fileSize#, and
** #recSize#, respectively.  Returns 1 if successful, else 0.
*/
static int
ReadHeaderRec(FILE *fd,
              int *head,
              int *tail,
              int *fileSize,
              size_t *recSize) {

  char firstRecord[MAX_RECORD_SIZE + 1];
  char header[HEADER_LEN + 1];

  if(fseek(fd, 0, SEEK_SET) < 0) {
    FAIL1("ReadHeaderRec: Seek to beginning of file failed (%d)\n", errno);
  }
  if(fgets(header, sizeof(header), fd) == NULL) {
    FAIL1("ReadHeaderRec: %s\n", feof(fd) ? "EOF" : "header read failed");
  }

  sscanf(header, "%d %d %d", head, tail, fileSize);
  memset(firstRecord, 0, sizeof(firstRecord));

  if(fgets(firstRecord, sizeof(firstRecord), fd) == NULL) {
    WARN("ReadHeaderRec: Unable to determine record size.\n");
    *recSize = HEADER_LEN;
  }
  else {
    *recSize = strlen(firstRecord);
    if((*recSize <= 0) || (*recSize > MAX_RECORD_SIZE)) {
      FAIL1("ReadHeaderRec: bad record size %d\n", *recSize);
    }
  }
  return(1);

}


/*
** Reads the #readIndex#'th #recordSize#-long state record from #fd#, which
** must be opened for reading.  Returns the time stamp, sequence number, and
** time-out in #timeStamp#, #seqNo#, and #timeOut#, respectively, and the
** client record in the #dataSize#-long array #data#.  Returns 1 if successful,
** else 0.
*/
static int
ReadStateRec(FILE *fd,
             size_t recSize,
             int readIndex,
             double *timeStamp,
             double *seqNo,
             double *timeOut,
             char *data,
             size_t dataSize) {

  char overhead[OVERHEAD_LEN + 1];

  if(fseek(fd, HEADER_LEN + recSize * readIndex, SEEK_SET) < 0) {
    FAIL1("ReadStateRec: fseek failed on index %d\n", readIndex);
  }

  /* grab only the overhead; looks like fgets tries to null term it. */
  memset(overhead, 0, sizeof(overhead));
  if((fgets(overhead, sizeof(overhead), fd) == NULL) ||
     (fgets(data, dataSize, fd) == NULL)) {
    FAIL1("ReadStateRec: %s\n", feof(fd) ? "EOF" : "read failed");
  }

  sscanf(overhead, "%lf %lf %lf", timeStamp, seqNo, timeOut);
  return(1);

}


/*
** Takes whatever steps are necessary to close the file #fd#.
*/
static void
Shut(FILE *fd) {
  if((fflush(fd) != 0) || (fclose(fd) != 0)) {
    (void)close(fileno(fd));
  }
}


/*
** Writes #head#, #tail#, #fileSize#, and #recSize# into the header of #fd#,
** which must already be opened for writing.  Returns 1 if successful, else 0.
*/
static int
WriteHeaderRec(FILE *fd,
               int head,
               int tail,
               int fileSize,
               size_t recSize) {
  /*
  ** TBD: #recSize# is presently ignored, although we really should include it
  ** in the header information, rather than force a read of the first record in
  ** ReadHeaderRec() just to discover the record size.
  */
  if(fseek(fd, 0, SEEK_SET) < 0) {
    FAIL1("WriteHeaderRec: Seek to beginning of file failed (%d)\n", errno);
  }
  fprintf(fd, HEADER_FORMAT, head, tail, fileSize);
  fflush(fd);
  return(1);
}


/*
** Writes #timeStamp#, #seqNo#, #timeOut#, and #data# as the #writeIndex#'th
** #recSize#-long record of #fd#, which must already be opened for writing.
** Returns 1 if successful, else 0.
*/
static int
WriteStateRec(FILE *fd,
              size_t recSize,
              int writeIndex,
              double timeStamp,
              double seqNo,
              double timeOut,
              const char *data) {

  size_t clientSize;
  int i;
  char padded[MAX_RECORD_SIZE + 1];

  if(fseek(fd, HEADER_LEN + recSize * writeIndex, SEEK_SET) < 0) {
    FAIL1("WriteStateRec: fseek failed on index %d\n", writeIndex);
  }

  clientSize = recSize - OVERHEAD_LEN - 1;
  for(i = 0; i < clientSize; i++) {
    if(data[i] == '\0') {
      memset(padded, ' ', clientSize - i);
      break;
    }
    padded[i] = data[i];
  }
  padded[clientSize] = '\0';
  fprintf(fd, STATE_FORMAT, timeStamp, seqNo, timeOut, padded);
  fflush(fd);
  return(1);

}


int
ReadState(const char *fname,
          char *state,
          int count,
          size_t max_size,
          double earliest_seq_no,
          double *out_time_out,
          double *out_seq_no,
          int *out_count,
          int *out_recordsize) {

  char *curr;
  char *endOfBuffer;
  int eofIndex;
  FILE *fd;
  int fileHead;
  int fileSize;
  int fileTail;
  double headSeqNo;
  int i;
  double now;
  int readIndex;
  double recordSeqNo;
  size_t recordSize;
  double recordTimeOut;
  double recordTimeStamp;
#if defined(ENABLE_CACHE)
  RCache r;
#endif

#if defined(ENABLE_CACHE)
  r = FindRCacheEntry(fname);
  if(r != NULL)
  {
	  (void)ReadStateCache(r,
			       state,
			       count,
			       max_size,
			       earliest_seq_no,
			       out_time_out,
			       out_seq_no,
			       out_count,
			       out_recordsize);
	  return(1);
  }
  else
  {
	  r = GetRCacheEntry(fname);
  }
#endif

  fd = fopen(fname, "r");
  if(fd == NULL) {
    FAIL2("ReadState cannot open file %s (%d)\n", fname, errno);
  }

  if(!ReadHeaderRec(fd, &fileHead, &fileTail, &fileSize, &recordSize)) {
    Shut(fd);
    FAIL1("ReadState error reading head for file %s\n", fname);
  }

  memset(state, 0, max_size);
  now = CurrentTime();
  curr = state;
  endOfBuffer = state + max_size;
  headSeqNo = 0.0;
  eofIndex = MODMINUS(fileTail, 1, fileSize);

  for(readIndex = MODMINUS(fileHead, 1, fileSize), i = 0;
      (readIndex != eofIndex) && (i < count);
      readIndex = MODMINUS(readIndex, 1, fileSize), i++) {

    if((endOfBuffer - curr) < recordSize)
      /* We've exhausted the return buffer. */
      break;

    if(!ReadStateRec(fd,
                     recordSize,
                     readIndex,
                     &recordTimeStamp,
                     &recordSeqNo,
                     &recordTimeOut,
                     curr,
                     endOfBuffer - curr)) {
      Shut(fd);
      FAIL2("ReadState: read failed on file %s, index %d\n", fname, readIndex);
    }
#if defined(ENABLE_CACHE)
    /*
     * might be NULL of user chooses no caching
     */
    if(r != NULL)
    {
    	WriteStateCache(r,
		    recordTimeOut,
		    recordSeqNo,
		    curr,
		    endOfBuffer - curr);
    }
#endif

    if((recordTimeStamp + recordTimeOut) < now)
      /* Dead record. */
      break;

    if(recordSeqNo <= earliest_seq_no)
      /* No more records of interest to the caller. */
      break;

    if(i == 0) {
      headSeqNo = recordSeqNo;
    }

    /* Drop the trailing newline added by WriteStateRec() */
    curr += strlen(curr) - 1;
    *curr = '\0';

  }

  *out_count = i;
  *out_recordsize = recordSize - OVERHEAD_LEN - 1;
  *out_seq_no = headSeqNo;
  *out_time_out = recordTimeOut;

#if defined(ENABLE_CACHE)


  /*
   * might be NULL is user chooses no caching
   */
  if(r != NULL)
  {
	  for( ; (readIndex != eofIndex); 
			  readIndex = MODMINUS(readIndex, 1, fileSize)) 
	  {
	    if(!ReadStateRec(fd,
			     recordSize,
			     readIndex,
			     &recordTimeStamp,
			     &recordSeqNo,
			     &recordTimeOut,
			     Dummy_buf,
			     sizeof(Dummy_buf))) {
	      Shut(fd);
	      FAIL2("ReadState: extra read failed on file %s, index %d\n", 
			      fname, readIndex);
	     }
	      
	    /*
	     * This will put things in timestamp and/or sequence number order
	     */
	    WriteStateCache(r,
			    recordTimeOut,
			    recordSeqNo,
			    Dummy_buf,
			    sizeof(Dummy_buf));
	  }
  }

#endif
  Shut(fd);
  return(1);

}


int
WriteState(const char *fname,
           size_t flength,
           double time_out,
           double seq_no,
           const char *state,
           size_t stateSize) {

  FILE *fd;
  int eofIndex;
  int fileHead;
  int fileSize;
  int fileTail;
  double now;
  char record[MAX_RECORD_SIZE];
  double recordSeqNo;
  size_t recordSize;
  double recordTimeStamp;
  double recordTimeOut;
  int writeIndex;
#if defined(ENABLE_CACHE)
  RCache r;
#endif

  if(stateSize > MAX_RECORD_SIZE) {
    FAIL2("WriteState: State size %d too big for %s\n", stateSize, fname);
  }

  fd = fopen(fname, "r+");

  if(fd == NULL) {
    fd = fopen(fname, "w+");
    if(fd == NULL) {
      FAIL2("WriteState: File create failed (%d) for %s\n", errno, fname);
    }
    fileHead = fileTail = 0;
    fileSize = flength;
    recordSize = OVERHEAD_LEN + stateSize + 1;
    if(!WriteHeaderRec(fd, fileHead, fileTail, flength, recordSize)) {
      Shut(fd);
      FAIL1("WriteState: Initial header write failed for %s\n", fname);
    }
    LOG1("WriteState: Opened new file %s\n", fname);
  }
  else if(!ReadHeaderRec(fd, &fileHead, &fileTail, &fileSize, &recordSize)) {
    Shut(fd);
    FAIL1("WriteState cannot get header for file %s\n", fname);
  }

  now = CurrentTime();
  if(now < seq_no) {
    seq_no = now;
  }

#if defined(ENABLE_CACHE)
  /*
   * look for the cache entry and if found, add the current data
   * to it
   */
  r = FindRCacheEntry(fname);
  if(r != NULL)
  {
	  WriteStateCache(r,
			  time_out,
			  seq_no,
			  state,
			  stateSize);
  }

#endif

  eofIndex = MODMINUS(fileTail, 1, fileSize);

  /*
  ** To find where this record should be written, start at the head and walk
  ** back until we hit the tail, timed out data, or a seq_no less than the
  ** incoming one.
  */
  for(writeIndex = MODMINUS(fileHead, 1, fileSize);
      writeIndex != eofIndex;
      writeIndex = MODMINUS(writeIndex, 1, fileSize)) {

    if(!ReadStateRec(fd,
                     recordSize,
                     writeIndex,
                     &recordTimeStamp,
                     &recordSeqNo,
                     &recordTimeOut,
                     record,
                     sizeof(record))) {
      Shut(fd);
      WARN1("WriteState: ReadStateRec %s failed\n", fname);
      if(unlink(fname) == 0) {
        return(WriteState(fname, flength, time_out, seq_no, state, stateSize));
      }
      else {
        FAIL2("WriteState: unlink %s failed with errno %d\n", fname, errno);
      }
    }

    if(recordSeqNo <= seq_no) {
      break;
    }

    if((recordTimeStamp + recordTimeOut) < now) {
      fileTail = writeIndex;
      break;
    }

  }

  if(writeIndex == MODMINUS(fileHead, 1, fileSize)) {
    /* Appending. */
    writeIndex = fileHead;
    fileHead = MODPLUS(fileHead, 1, fileSize);
    if(fileHead == fileTail) {
      fileTail = MODPLUS(fileTail, 1, fileSize);
    }
  }
  else if(writeIndex == eofIndex) {
    Shut(fd);
    FAIL1("WriteState: old merge attempted on file %s\n",fname);
  }

  if(!WriteStateRec(fd,
                    recordSize,
                    writeIndex,
                    now,
                    seq_no,
                    time_out,
                    state)) {
    Shut(fd);
    FAIL1("WriteState: write to %s failed\n", fname);
  }

  if(!WriteHeaderRec(fd, fileHead, fileTail, fileSize, recordSize)) {
    Shut(fd);
    FAIL1("WriteState: header write to %s failed\n", fname);
  }

  Shut(fd);
  return(1);

}

#ifdef WITH_NETLOGGER
int
WriteStateNL(const char* path,
			 const char* id,
			 size_t flength,
			 double time_out,
			 double seq_no,
			 const char *state,
			 size_t stateSize) {
	struct host_desc host;
	unsigned short nsPort = 0;
	NLhandle * nlhandle;
	char nlmsg[MAX_NL_MSG_LEN];
	char buffer[MAX_RECORD_SIZE];
	int i,start,end,nfield;
	char c;
	//decode host:socket and open connection
	HostDValue(path, nsPort, &host);
	printf("WriteStateNL: host.host_name: .%s., host.port:%d\n", host.host_name, host.port);
	
	sprintf(nlmsg, STATE_FORMAT_NL, seq_no, time_out);
	printf("WriteStateNL: .%s.\n", nlmsg);
	nfield=0;
	i=0;
	while(i<stateSize)
	{
		//ignore blanks
		while(state[i]==' ') i++;
		
		start = i;
		//find next blank or 0x00
		while(state[i]!=' ' && state[i]!=0) i++;

		end = i;
		printf("WriteStateNL: %d .%d.\n",end,start);
		//insert filed
		if(end-start>0)
		{
			sprintf(buffer," FIELD%d=",nfield);
			strcat(nlmsg,buffer);
			memcpy(buffer,state+start,end-start);
			buffer[end-start]=0;
			strcat(nlmsg,buffer);
			printf("WriteStateNL: %d .%s.\n",end-start,nlmsg);
			nfield++;
		}
	}
	nlhandle = NetLoggerOpen(NL_HOST, (char *)"NWS_MEMORY",
							 (char *)"nws_memory.log", host.host_name, host.port);
	
	if(nlhandle==NULL)
	{
		LOG2("WriteStateNL: NetLoggerOpen failed host:%s port:%d\n", host.host_name, host.port);
		return 0;
	}

	printf("WriteStateNL: NetLoggerOpen done\n");
	//now convert all apha to upper case, and non-alpah-numeric to _
	strcpy(buffer,id);
	i=strlen(buffer)-1;
	while(i>=0)
	{
		c= buffer[i];
		if(c>='a' && c<='z')//lower case
			buffer[i]=c+'A'-'a';
		else if((c<'A' || c>'Z') && (c<'0' || c>'9'))//non alpha numeric
			buffer[i]='_';
		i--;
	}
	//now format and write
	NetLoggerWrite(nlhandle, buffer, (char *)"%s", nlmsg);
	
	//close NL connection
	NetLoggerClose(nlhandle);
	
	return (1);
}
#endif /* WITH_NETLOGGER */
