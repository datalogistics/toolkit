
#ifndef __LORS_INTERNAL_H__
#define __LORS_INTERNAL_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

int lorsGetDepot(LorsDepotPool *dp, int *id);
int lorsReleaseDepot(LorsDepotPool *dp, int id, double bw, int nfailures);

int lorsAddDepot(LorsDepot *d);
int lorsScoreDepot(LorsDepot *d);
int lorsUpdateDepot(LorsDepot *d);

#endif
