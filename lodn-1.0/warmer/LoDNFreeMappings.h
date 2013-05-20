
#ifndef LODNFREEMAPPINGS_H_
#define LODNFREEMAPPINGS_H_

#include <pthread.h>

#include "jval.h"
#include "dllist.h"
#include "ibp.h"

/***---Structs----***/

/* Struct for representing a collection of free mappings */
typedef struct
{
	Dllist mappings;				/* List of mappings */
	pthread_mutex_t lock;			/* Lock for protecting struct */

} LoDNFreeMappings_t;


/***---Prototypes---***/
LoDNFreeMappings_t *LoDNFreeMappingsAllocate(void);
void LoDNFreeMappingsFree(LoDNFreeMappings_t *freeMappings);
void LoDNFreeMappingsAppend(LoDNFreeMappings_t *freeMappings, IBP_cap cap);
IBP_cap LoDNFreeMappingsGet(LoDNFreeMappings_t *freeMappings);

#endif /* LODNFREEMAPPINGS_H_ */
