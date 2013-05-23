/* $Id: last_value.h,v 1.2 1999/09/22 16:54:46 hayes Exp $ */

#if !defined(LAST_VAL_H)
#define LAST_VAL_H

#include "fbuff.h"

char *InitLastValue(fbuff series, fbuff time_stamps, char *params);
void FreeLastValue(char *state);
void UpdateLastValue(char *state, double ts, double value); 
int ForcLastValue(char *state, double *v);
#endif

