#if !defined(NWS_CACHE_H)
#define NWS_CACHE_H

#include "fbuff.h"

#define RC_HASH_SIZE (100)
#define RC_ENTRIES_DEFAULT (256)
#define RC_ENTRIES (RC_entries)
#define RC_FNAME_SIZE (1024)

extern int RC_entries;

struct record_cache
{
	char fname[RC_FNAME_SIZE];
	fbuff r_timestamp;
	fbuff r_seqno;
	fbuff r_timeout;
	fbuff v_timestamp;
	fbuff v_value;
	struct record_cache *hnext;
	struct record_cache *hprev;
	struct record_cache *fnext;
	struct record_cache *fprev;
};

typedef struct record_cache *RCache;

/*
 * finds one and fails if it doesn't exist
 */
RCache FindRCacheEntry(const char *fname);

/*
 * finds and existing one and none exists, makes a new one
 */
RCache GetRCacheEntry(const char *fname);

int
ReadStateCache(RCache r,
	       char *state,
	       int count,
	       int max_size,
	       double earliest_seq_no,
	       double *out_time_out,
	       double *out_seq_no,
	       int *out_count,
	       int *out_recordsize);

int
WriteStateCache(RCache r, 
		double time_out, 
		double seq_no, 
		const char *state, 
		int stateSize);

int
AppendStateCache(RCache r, 
		double time_out, 
		double seq_no, 
		const char *state, 
		int stateSize);

extern int RCMemorySize;

#endif

