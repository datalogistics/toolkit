#ifndef __LORS_UTIL_H__
#define __LORS_UTIL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <jrb.h>
#include <ibp.h>
#include <lors_resolution.h>
#include "lors_job.h"
#include <string.h>


#define D_LORS_NONE    0x0000
#define D_LORS_TERSE   0x0001
#define D_LORS_ERR_MSG 0x0002 
#define D_LORS_VERBOSE 0x0004 
#define MAX_CHAR_IN_LINE  256

extern int g_db_level;
extern int g_lors_demo;
extern int lorsDebugPrint(int db_level, char *format, ... );
int _lorsIDFromCap ( char * cap);

typedef IBP_depot (*get_IBP_depot_func) ( JRB node ) ; 

/*LorsMetaValue *new_mdv();*/
LorsBoundary *new_boundary(longlong offset, longlong length);
int ll_jvalcmp(Jval j1, Jval j2);
struct LorsTimeout
{
    time_t      timeout;
    int         set;
};

typedef struct __lorsDepotResolution {
    IBP_depot       *src_dps;
    IBP_depot       *dst_dps;
    int             n_src_dps;
    int             n_dst_dps;
    /*double          **resolution;*/
    LboneResolution *lr;
} __LorsDepotResolution;

typedef struct {
   int   n_element;
   void  *elem[5];
}__LorsData;

int lorsGetDepot(LorsDepotPool *dpp, LorsDepot **dp);
LorsDepot *lorsGetDepotByName(LorsDepotPool *dpp, IBP_depot depot);
int lorsCheckoutDepot(LorsDepot *dp);
int lorsReleaseDepot(LorsDepot *dp , double bandwidth, int nfailures);

void *lorsTimeoutSleeper(void *v);
double diffTime(double t);
double getTime(void);
LorsMapping *_lorsAllocMapping(void );
void         _lorsFreeMapping(LorsMapping *lm);
IBP_attributes  new_ibp_attributes(time_t duration_days, int reliability, int type);
JRB jrb_find_llong(JRB root, longlong lkey);
JRB jrb_insert_llong(JRB tree, longlong ll, Jval val);
longlong *_lorsNewBlockArray(longlong  length, ulong_t blocksize, int *list_len);
longlong *_lorsNewBlockArrayE2E(LorsSet *set,ulong_t length, longlong offset,ulong_t data_block,__LorsDLList dl_list, int *list_len);
LorsSet *_lorsMallocSet(void);
char *_losrTrim(char *str);
char *_lorsRTrim(char *str);
char *_lorsLTrim(char *str);
int _lorsRemainTime( time_t begin_time, int timeout);
JRB jrb_find_first_llong(JRB tree, longlong val);
int _lorsGetDepotInfo(struct ibp_depot *depot, IBP_cap cap);
int strtobytes(char *str, ulong_t *ret_bytes);
char *set_location(char *file, char *function, int line, int tid);
int  jrb_n_nodes( JRB jrb ); 
int _lorsGetNumOfDepot(LorsDepotPool *dpp);
IBP_depot _lorsGetIBPDepotFromMapping ( JRB mapping_node);
IBP_depot _lorsGetIBPDepotFromDepotPool ( JRB depot_node);
int _lorsFindDepot( IBP_depot *dp_list, IBP_depot depot);
IBP_depot new_ibp_depot(char *host, int port );
int _lorsGetPhyOffAndLen(LorsMapping *map,  
                         ulong_t l_off,
                         ulong_t l_len, 
                         ulong_t *phy_offset,
                         ulong_t *phy_length,
                         ulong_t *log_e2e_offset,
                         ulong_t    e2e_bs);

int _lorsCreateDepotList( IBP_depot *dp_list, int max_depots, JRB jrb_tree,get_IBP_depot_func get_IBP_depot );
int strtotimeout(char *str, time_t *ret_timeout);
int _lorsCalcTimeout(longlong length);
IBP_depot lors_strtodepot(char *hostname);
void get_uuid(char *buff);

#ifdef __GNU__ 
#define LOCATION() set_location(__FILE__, __FUNCTION__, __LINE__, pthread_self())
#else 
#define LOCATION() set_location(__FILE__, NULL, __LINE__, pthread_self())
#endif

#endif
