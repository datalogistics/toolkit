#ifndef __LORS_RESOLUTION_H__
#define __LORS_RESOLUTION_H__

#include <lors_api.h>
#include <fields.h>


int lorsResolutionMax(LboneResolution *lr, double *max);
int lorsResolutionMin(LboneResolution *lr, double *min);
LboneResolutionIndex *newLboneResolutionIndex(int src, int dst);
JRB jrb_insert_depot(JRB tree, IBP_depot key, Jval val);
JRB jrb_find_depot(JRB root, IBP_depot key);
int depot_cmp(Jval aa, Jval bb);
/* This function needs more thought.  It does not initialize the depotIndex
 * JRB tree. */
int lorsCreateResolution(LboneResolution **lr, int src_cnt, int dst_cnt);
int lorsNormalizeResolution(LboneResolution *lr);

/* TODO: there will need to be a means of adding metrics to an existing file
 * or dataset. It is unclear to me how this would work.  Or how a process
 * could use different metrics  */

#endif

