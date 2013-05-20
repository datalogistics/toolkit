/**
 * mapping.c
 *
 * Functions for manipulating mappings.
 *
 * Jeremy Millar
 * (c) 2002, University of Tennessee
 */

/**
 * Header files.
 */
#include "exnode.h"

/**
 * Create mapping.
 */
int exnodeCreateMapping(ExnodeMapping **map)
{
	ExnodeMapping *temp;
	int err;
	
	temp=(ExnodeMapping *)malloc(sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
	
	err=exnodeCreateMetadata(&(temp->metadata));
	if(err!=EXNODE_SUCCESS) {
		free(temp);
		return(err);
	}
	
	temp->function=NULL;
	temp->read=NULL;
	temp->write=NULL;
	temp->manage=NULL;
	
	*map=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Destroy mapping.
 */
int exnodeDestroyMapping(ExnodeMapping *map)
{
	if(!map) {
		return(EXNODE_BADPTR);
	}


    /*fprintf(stderr, "DestroyMapping: 0x%x\n", map);*/

	free(map->read);
	free(map->write);
	free(map->manage);
	
    /*fprintf(stderr, "Meta: 0x%x\n", map->metadata);*/
	exnodeDestroyMetadata(map->metadata);
    /*fprintf(stderr, "Args: 0x%x\n", map->function);*/
	exnodeDestroyFunction(map->function);
	
	free(map);
	
	return(EXNODE_SUCCESS);
}

/**
 * Copy mapping. Deep.
 */
int exnodeCopyMapping(ExnodeMapping **dest,ExnodeMapping *src)
{
	ExnodeMapping *temp;
	int err;
	
	if(!src) {
		return(EXNODE_BADPTR);
	}
	
	temp=(ExnodeMapping *)malloc(sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}

    /*fprintf(stderr, "CopyMapping: From: 0x%x To: 0x%x\n", src, temp);*/
	
	temp->read=strdup(src->read);
	if(temp->read==NULL) {
		free(temp);
		return(EXNODE_NOMEM);
	}
	
	temp->write=strdup(src->write);
	if(temp->write==NULL) {
		free(temp->read);
		free(temp);
		return(EXNODE_NOMEM);
	}
	
	temp->manage=strdup(src->manage);
	if(temp->manage==NULL) {
		free(temp->write);
		free(temp->read);
		free(temp);
		return(EXNODE_NOMEM);
	}
	

    /*fprintf(stderr, "MappingMeta\n");*/
	err=exnodeCopyMetadata(&(temp->metadata),src->metadata);
	if(err!=EXNODE_SUCCESS) {
		free(temp->read);
		free(temp->write);
		free(temp->manage);
		free(temp);
		return(err);
	}
	
    /*fprintf(stderr, "MappingFunc\n");*/
	err=exnodeCopyFunction(&(temp->function),src->function);
	if(err!=EXNODE_SUCCESS) {
		free(temp->read);
		free(temp->write);
		free(temp->manage);
		exnodeDestroyMetadata(temp->metadata);
		free(temp);
		return(err);
	}
	
	*dest=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Set function.
 */
int exnodeSetFunction(ExnodeMapping *map,ExnodeFunction *f,
                      ExnodeBoolean replace)
{
	int err;
	
	if(!map || !f) {
		return(EXNODE_BADPTR);
	}
	
    /*fprintf(stderr, "SetFunction: 0x%x\n", f);*/
	if((map->function!=NULL) && (replace==FALSE)) {
		return(EXNODE_EXISTS);
	}
	
	if(map->function!=NULL) {
		exnodeDestroyFunction(map->function);
	}
	
	err=exnodeCopyFunction(&(map->function),f);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}
	
	return(EXNODE_SUCCESS);
}

/**
 * Get metadata.
 */
int exnodeGetMappingMetadata(ExnodeMapping *map,ExnodeMetadata **md)
{
	if(!map) {
		return(EXNODE_BADPTR);
	}
	
	*md=map->metadata;
	
	return(EXNODE_SUCCESS);
}

/**
 * Get function.
 */
int exnodeGetFunction(ExnodeMapping *map,ExnodeFunction **f)
{
	if(!map) {
		return(EXNODE_BADPTR);
	}
	
	*f=map->function;
	
	return(EXNODE_SUCCESS);
}

/**
 * Set capabilities.
 */
int exnodeSetCapabilities(ExnodeMapping *map,char *read,char *write,
                          char *manage,ExnodeBoolean replace)
{
	if(!map) {
		return(EXNODE_BADPTR);
	}
	
	if(replace==TRUE) {
		if(read!=NULL) {
			map->read=strdup(read);
		} else {
			map->read=strdup("");
		}
		if(write!=NULL) {
			map->write=strdup(write);
		} else {
			map->write=strdup("");
		}
		if(manage!=NULL) {
			map->manage=strdup(manage);
		} else {
			map->manage=strdup("");
		}
	} else {
		if(read!=NULL) {
			map->read=strdup(read);
		}
		if(write!=NULL) {
			map->write=strdup(write);
		}
		if(manage!=NULL) {
			map->manage=strdup(manage);
		}
	}
	
	return(EXNODE_SUCCESS);
}

/**
 * Get capabilities.
 */
int exnodeGetCapabilities(ExnodeMapping *map,char **read,char **write,
                          char **manage)
{
	if(!map) {
		return(EXNODE_BADPTR);
	}
	
	if(map->read!=NULL) {
		*read=strdup(map->read);
	} else {
		*read=NULL;
	}
	
	if(map->write!=NULL) {
		*write=strdup(map->write);
	} else {
		*write=NULL;
	}
	
	if(map->manage!=NULL) {
		*manage=strdup(map->manage);
	} else {
		*manage=NULL;
	}
	
	return(EXNODE_SUCCESS);
}
