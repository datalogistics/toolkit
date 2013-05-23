/* $Id: nws_state.h,v 1.19 2001/01/12 19:03:47 rich Exp $ */

#ifndef NWS_STATE_H
#define NWS_STATE_H


#include <sys/types.h> /* size_t */

#define ENABLE_CACHE


/* Largest allowable record size. */
#define MAX_RECORD_SIZE 512

#ifdef WITH_NETLOGGER
#define MAX_NL_MSG_LEN  1024

int
WriteStateNL(const char* path,
			 const char* id,
			 size_t flength,
			 double time_out,
			 double seq_no,
			 const char *state,
			 size_t stateSize);

#endif /* WITH_NETLOGGER */

/*
** Reads #count# records from #fname# into the #max-size#-long buffer #state#.
** The newest record is returned at the beginning of #state#, the oldest at
** the end.  Ignores records stamped with a sequence number that is less than
** #earliest_seq_no*.  Returns the earliest time out of any returned record
** in #out_time_out#, the greatest sequence number in #out_seq_no#, the number
** of records returned in #out_count# and the length of each record in
** #out_recordsize#.  Returns 1 if successful, else 0.
*/
int
ReadState(const char *fname,
          char *state,
          int count,
          size_t max_size,
          double earliest_seq_no,
          double *out_time_out,
          double *out_seq_no,
          int *out_count,
          int *out_recordsize);


/*
** Write the #stateSize#-long buffer #state# into file #fname#.  Marks the
** state record with time out value #timeOut# and sequence number #seqNo#.
** If the file has not previously been opened, #flength# indicates the maximum
** number of records it may hold.  Returns 1 if successful, else 0.
*/
int
WriteState(const char *fname,
           size_t flength,
           double timeOut,
           double seqNo,
           const char *state,
           size_t stateSize);


#endif
