/**
 * exnode.c
 *
 * Functions for manipulating exnodes.
 *
 * Jeremy Millar
 * (c) 2002, University of Tennessee
 */
 
 /**
  * Header files.
	*/
#include "exnode.h"

/**
 * Create exnode.
 */
int exnodeCreateExnode(Exnode **exnode)
{
	Exnode *temp;
	ExnodeValue val;
	int err;
	
	temp=(Exnode *)malloc(sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
	
	err=exnodeCreateMetadata(&(temp->metadata));
	if(err!=EXNODE_SUCCESS) {
		free(temp);
		return(err);
	}
	
	temp->mappings=new_dllist();
	if(temp->mappings==NULL) {
		exnodeDestroyMetadata(temp->metadata);
		free(temp);
		return(EXNODE_NOMEM);
	}

    /* below changed by Yong Zheng for fixing memory leaks */
	val.s="3.0";
	exnodeSetMetadataValue(temp->metadata,"Version",val,STRING,TRUE);
	
	*exnode=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Destroy exnode.
 */
int exnodeDestroyExnode(Exnode *exnode)
{
    Dllist dll_node;
    ExnodeMapping *mapping;


	if(!exnode) {
		return(EXNODE_BADPTR);
	}

    /*fprintf(stderr, "DestroyExnode: 0x%x\n", exnode);*/
	
    /*fprintf(stderr, "\tMeta\n");*/
	exnodeDestroyMetadata(exnode->metadata);
/* below added by Yong Zheng for fixing memory leaking */
#if 1
    dll_traverse(dll_node,exnode->mappings){
          mapping = (ExnodeMapping *)jval_v(dll_node->val);
          exnodeDestroyMapping(mapping);
    };
#endif
/*******************************************************/ 

	free_dllist(exnode->mappings);
	free(exnode);
	
	return(EXNODE_SUCCESS);
}

/**
 * Copy exnode. Deep.
 */
int exnodeCopyExnode(Exnode **dest,Exnode *src)
{
	Exnode *temp;
	ExnodeMapping *map;
	Dllist current;
	int err;
	
	if(!src) {
		return(EXNODE_BADPTR);
	}
	
	temp=(Exnode *)malloc(sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
	
	err=exnodeCopyMetadata(&(temp->metadata),src->metadata);
	if(err!=EXNODE_SUCCESS) {
		free(temp);
		return(err);
	}
	
	dll_traverse(current,src->mappings) {
		err=exnodeCopyMapping(&map,(ExnodeMapping *)jval_v(current->val));
		if(err!=EXNODE_SUCCESS) {
			exnodeDestroyMetadata(temp->metadata);
			free(temp);
			return(err);
		}
		dll_append(temp->mappings,new_jval_v((void *)map));
	}
	
	*dest=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Get metadata.
 */
int exnodeGetExnodeMetadata(Exnode *exnode,ExnodeMetadata **md)
{
	if(!exnode) {
		return(EXNODE_BADPTR);
	}
	
	*md=exnode->metadata;
	
	return(EXNODE_SUCCESS);
}

/**
 * Get mappings.
 */
int exnodeGetMappings(Exnode *exnode,ExnodeEnumeration **e)
{
	ExnodeEnumeration *temp;
	
	if(!exnode) {
		return(EXNODE_BADPTR);
	}
	
	temp=(ExnodeEnumeration *)malloc(sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
	
	temp->data=exnode->mappings;
	temp->current=dll_first(temp->data);
	temp->type=MAP;
	
	*e=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Get next mapping.
 */
int exnodeGetNextMapping(ExnodeEnumeration *e,ExnodeMapping **map)
{
	if(!e) {
		return(EXNODE_BADPTR);
	}
	
	if(e->type!=MAP) {
		return(EXNODE_BADTYPE);
	}
	
    if ( e->data == e->current ) 
    {
        *map = NULL;
    } else 
    {
	    *map=(ExnodeMapping *)jval_v(e->current->val);
    }
	e->current=e->current->flink;
	
	return(EXNODE_SUCCESS);
}

/**
 * Append mapping.
 */
int exnodeAppendMapping(Exnode *exnode,ExnodeMapping *map)
{
	if(!exnode || !map) {
		return(EXNODE_BADPTR);
	}
	
	dll_append(exnode->mappings,new_jval_v((void *)map));
	
	return(EXNODE_SUCCESS);
}

/**
 * Destroy an enumeration.
 */
int exnodeDestroyEnumeration(ExnodeEnumeration *e)
{
	if(!e) {
		return(EXNODE_BADPTR);
	}
	
	if(e->type==NAME) {
		free_dllist(e->data);
	}
	
	free(e);
	
	return(EXNODE_SUCCESS);
}
		
