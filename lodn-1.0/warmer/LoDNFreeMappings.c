#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "jval.h"
#include "dllist.h"

#include "LoDNLogging.h"
#include "LoDNFreeMappings.h"
#include "LoDNAuxillaryFunctions.h"


/****-----------------------LoDNFreeMappingsAllocate()---------------------****
 * Description:  This function is used to allocate and initialize the fields
 *               of a LoDNFreeMappings_t struct.  It returns a pointer to
 *               the allocated struct.
 * Output: A pointer to the allocated struct.
 ****----------------------------------------------------------------------****/
LoDNFreeMappings_t *LoDNFreeMappingsAllocate(void)
{
	/* Declarations */
	LoDNFreeMappings_t *freeMappings;


	/* Allocate free mappings struct */
	if((freeMappings = (LoDNFreeMappings_t*)malloc(sizeof(LoDNFreeMappings_t)))
			== NULL)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
	}

	/* Creates a list to hold mappings to free */
	if((freeMappings->mappings = new_dllist()) == NULL)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
	}

	/* Initialize the mutex lock */
	if(pthread_mutex_init(&(freeMappings->lock), NULL) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
	}

	/* Return the allocated structure */
	return freeMappings;
}



/****-----------------------LoDNFreeMappingsAllocate()---------------------****
 * Description:  This function is used to free the LoDNFreeMappings_t struct
 *               passed in via pointer and the fields it contains.
 * Input:  freeMappings - LoDNFreeMappings_t pointer to the struct.
 ****----------------------------------------------------------------------****/
void LoDNFreeMappingsFree(LoDNFreeMappings_t *freeMappings)
{
	/* Declarations */
	Dllist node;

	/* Frees the mutex lock */
	if(pthread_mutex_destroy(&(freeMappings->lock)) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
	}

	/* Frees any mappings still in the list */
	while(!dll_empty(freeMappings->mappings))
	{
		node = dll_first(freeMappings->mappings);

		if(node->val.s != NULL)
		{
			free(node->val.s);
		}

		dll_delete_node(node);
	}

	/* Frees the associated memory */
	free_dllist(freeMappings->mappings);
	free(freeMappings);
}



/****-------------------------LoDNFreeMappingsAppend()---------------------****
 * Description: This function appends a copy of the IBP_cap that gets passed
 *              in to the list in freeMappings.
 * Input: freeMappings - pointer to the LoDNFreeMappings_t struct to add the
 *                       mapping to.
 *        cap - IBP_cap for the mapping to be freed.
 ****----------------------------------------------------------------------****/
void LoDNFreeMappingsAppend(LoDNFreeMappings_t *freeMappings, IBP_cap cap)
{
	/* Declarations */
	IBP_cap free_cap;


	/* Locks the mutex */
	if(pthread_mutex_lock(&(freeMappings->lock)) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error locking mutex");
	}

	/* Creates a copy of the cap */
	if((free_cap = strndup(cap, MAX_CAP_LEN)) == NULL)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
	}

	/* Appends the free cap */
	dll_append(freeMappings->mappings, new_jval_s(free_cap));

	/* Unlocks the mutex */
	if(pthread_mutex_unlock(&(freeMappings->lock)) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
	}
}



/****-------------------------LoDNFreeMappingsGet()------------------------****
 * Description:  This function returns the first IBP_cap in its list or
 *               NULL if the list is empty.
 * Input: freeMappings - LoDNFreeMappings_t pointer to the struct for free
 *                       mappings.
 * Output: It returns a IBP_cap or NULL if the list is empty.
 ****----------------------------------------------------------------------****/
IBP_cap LoDNFreeMappingsGet(LoDNFreeMappings_t *freeMappings)
{
	/* Declarations */
	Dllist node;
	IBP_cap cap = NULL;


	/* Locks the mutex */
	if(pthread_mutex_lock(&(freeMappings->lock)) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error locking mutex");
	}

	/* If the list is not empty, remove a cap from the list */
	if(!dll_empty(freeMappings->mappings))
	{
		node = dll_first(freeMappings->mappings);

		cap = node->val.s;

		dll_delete_node(node);
	}

	/* Unlocks the mutex */
	if(pthread_mutex_unlock(&(freeMappings->lock)) != 0)
	{
		logPrintError(0, __FILE__, __LINE__, 1, "Error unlocking mutex");
	}

	/* Returns the cap or NULL if the list is empty */
	return cap;
}
