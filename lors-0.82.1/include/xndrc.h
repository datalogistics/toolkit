#ifndef __XNDRC_H__
#define __XNDRC_H__
#include <lors_api.h>
#include <fields.h>

typedef struct xndrc_info
{
    IBP_depot  *lboneList;
    IBP_depot  *depotList;
    IBP_depot  *depotList1;
    IBP_depot  *depotList2;
    char       *location;
    double      duration_days;
    time_t      duration;
    int         storage_type;
    int         verbose;
    int         demo;
    ulong_t     max_buffersize;
    ulong_t     data_blocksize;
    ulong_t     e2e_blocksize;
    char        *e2e_order_str;
    int         e2e_order[32];
    int         fragments_per_file;
    int         copies;
    int         threads;
    int         maxdepots;
    time_t      timeout;

    char	    samename;
    char	   *resolution_file;
    int		    mbuffer_size;
    longlong    offset;
    ulong_t	    length;

    time_t	    abs_time;
    char	    non_destruct;
    char	    decrement;
    char	    decr_all;
    int		    all;
    int		    nonexist;
    int		  **segments;
    int         upld_mode;
} XndRc;


int set_xndrc_defaults(XndRc *xndrc);
int read_xndrc_file(XndRc *xndrc, char *filename);
void free_xndrc(XndRc *xndrc);
int parse_xndrc_files(XndRc *xndrc);


#define XND_DEFAULT_SCHEMA_FILE		      ""
#define XND_DEFAULT_DURATION_DAYS	       1
#define XND_DEFAULT_RESOLUTION_FILE	       "resolution.txt"
#define XND_DEFAULT_STORAGE_TYPE	       IBP_HARD
#define XND_DEFAULT_VERBOSE		           0
#define XND_DEFAULT_INTERNAL_BUFFERSIZE	   (48*1024*1024)
#define XND_DEFAULT_DATA_BLOCKSIZE         (5*1024*1024)
#define XND_DEFAULT_FRAGMENTS_PER_FILE     -1
#define XND_DEFAULT_THREADS                8    /* more TCP friendly than -1 */
#define XND_DEFAULT_MAXDEPOTS              4
#define XND_DEFAULT_COPIES		           3
#define XND_DEFAULT_E2E_BLOCKSIZE           (1024*1024)
#define XND_DEFAULT_TIMEOUT		           600


#endif
