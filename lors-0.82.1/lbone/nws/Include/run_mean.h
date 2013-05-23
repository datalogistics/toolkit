/* $Id: run_mean.h,v 1.2 1999/09/22 16:54:46 hayes Exp $ */

#if !defined(RUN_MEAN_H)
#define RUN_MEAN_H

#include "fbuff.h"

char *InitRunMean(fbuff series, fbuff time_stamps, char *params);
void FreeRunMean(char *state);
void UpdateRunMean(char *state, double ts, double value); 
int ForcRunMean(char *state, double *v);
#endif

