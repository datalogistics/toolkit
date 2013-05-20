/**
 * function.c
 *
 * Functions for manipulating functions.
 *
 * Jeremy Millar
 * (c) 2002, University of Tennessee
 */

/**
 * Header files.
 */
#include "exnode.h"

/**
 * Create a function.
 */
int exnodeCreateFunction(char *name,ExnodeFunction **func)
{
	ExnodeFunction *f;
	int err;
	
    /* for debuging */
	f=(ExnodeFunction *)calloc(1, sizeof(*f));
	if(f==NULL) {
		return(EXNODE_NOMEM);
	}
    memset(f, 0x00, sizeof(ExnodeFunction));
	

    /*fprintf(stderr, "CreateFunction: 0x%x \n", f);*/

	err=exnodeCreateMetadata(&(f->metadata));
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}
    /*fprintf(stderr, "\tMeta: 0x%x\n", f->metadata);*/
	
	err=exnodeCreateMetadata(&(f->arguments));
	if(err!=EXNODE_SUCCESS) {
		exnodeDestroyMetadata(f->metadata);
		return(err);
	}
    /*fprintf(stderr, "\tArgs: 0x%x\n", f->arguments);*/
	
	f->name=strdup(name);
	if(f->name==NULL) {
		exnodeDestroyMetadata(f->metadata);
		exnodeDestroyMetadata(f->arguments);
		return(EXNODE_NOMEM);
	}
	
	f->functions=new_dllist();
	if(f->functions==NULL) {
		free(f->name);
		exnodeDestroyMetadata(f->metadata);
		exnodeDestroyMetadata(f->arguments);
	}
	
	*func=f;
	return(EXNODE_SUCCESS);
}

/**
 * Destroy a function. Deep.
 */
int exnodeDestroyFunction(ExnodeFunction *f)
{
	Dllist current,next;
	
	if(!f) {
		return(EXNODE_BADPTR);
	}
	

    /*fprintf(stderr, "DestroyFunction: 0x%x Name: %s\n", f, f->name);*/
	free(f->name);

    /*fprintf(stderr, "Meta\n");*/
	exnodeDestroyMetadata(f->metadata);
    f->metadata = NULL;
    /*fprintf(stderr, "Args\n");*/
	exnodeDestroyMetadata(f->arguments);
    f->arguments = NULL;
	
    /* Changed to fix BUG when only one nested function. */
    next = f->functions->flink;
    while ( next != f->functions )
    {
		exnodeDestroyFunction((ExnodeFunction *)jval_v(next->val));
        next->val.v = NULL;
        next = next->flink;
    } 
#if 0
	dll_traverse(current,f->functions) {
		next=current->flink;
		exnodeDestroyFunction((ExnodeFunction *)jval_v(current->val));
		current=next;
	}
#endif
	
	free_dllist(f->functions);
    f->functions = NULL;
	
	free(f);
	
	return(EXNODE_SUCCESS);
}

/**
 * Copy a function. Deep.
 */
int exnodeCopyFunction(ExnodeFunction **dest,ExnodeFunction *src)
{
	ExnodeFunction *temp,*sub;
	Dllist current;
	int err;
	
	if(!src) {
		return(EXNODE_BADPTR);
	}

	temp=(ExnodeFunction *)calloc(1, sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
    memset(temp, 0x55, sizeof(*temp));

    /*fprintf(stderr, "CopyFunction: From: 0x%x To: 0x%x Name: %s\n",*/
            /*src, temp, (src->name != NULL ? src->name : "NullName" ));*/

	temp->name=strdup(src->name);
	if(temp->name==NULL) {
		free(temp);
		return(EXNODE_NOMEM);
	}
	


    /*fprintf(stderr, "\tMeta\n");*/
	err=exnodeCopyMetadata(&(temp->metadata),src->metadata);
	if(err!=EXNODE_SUCCESS) {
		free(temp->name);
		free(temp);
		return(err);
	}
	
    /*fprintf(stderr, "\tArgs\n");*/
	err=exnodeCopyMetadata(&(temp->arguments),src->arguments);
	if(err!=EXNODE_SUCCESS) {
		free(temp->name);
		exnodeDestroyMetadata(temp->metadata);
		free(temp);
		return(err);
	}
	
	temp->functions=new_dllist();
	if(temp->functions==NULL) {
		exnodeDestroyMetadata(temp->metadata);
		exnodeDestroyMetadata(temp->arguments);
		free(temp->name);
		free(temp);
		return(EXNODE_NOMEM);
	}
	
	dll_traverse(current,src->functions) {
		exnodeCopyFunction(&sub,(ExnodeFunction *)jval_v(current->val));
		dll_append(temp->functions,new_jval_v((void *)sub));
	}
	
	*dest=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Get name.
 */
int exnodeGetFunctionName(ExnodeFunction *f,char **name)
{
	if(!f) {
		return(EXNODE_BADPTR);
	}
	
	*name=strdup(f->name);
	if(*name==NULL) {
		return(EXNODE_NOMEM);
	}
	
	return(EXNODE_SUCCESS);
}

/**
 * Get arguments.
 */
int exnodeGetFunctionArguments(ExnodeFunction *f,ExnodeMetadata **arg)
{
	if(!f) {
		return(EXNODE_BADPTR);
	}
	
	*arg=f->arguments;
	
	return(EXNODE_SUCCESS);
}

/**
 * Get metadata.
 */
int exnodeGetFunctionMetadata(ExnodeFunction *f,ExnodeMetadata **md)
{
	if(!f) {
		return(EXNODE_BADPTR);
	}
	
	*md=f->metadata;
	
	return(EXNODE_SUCCESS);
}

/** * Get subfunctions.
 */
int exnodeGetSubfunctions(ExnodeFunction *f,ExnodeEnumeration **e)
{
	ExnodeEnumeration *temp;
	
	if(!f) {
		return(EXNODE_BADPTR);
	}
	
	temp=(ExnodeEnumeration *)malloc(sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
	
	temp->data=f->functions;
	temp->current=(void *)dll_first(temp->data);
	temp->type=FUNC;
	
	*e=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Get next subfunction.
 */
int exnodeGetNextSubfunction(ExnodeEnumeration *e,ExnodeFunction **f)
{
	if(!e) {
		return(EXNODE_BADPTR);
	}
	
	if(e->type!=FUNC) {
		return(EXNODE_BADTYPE);
	}
	
    if ( e->current == e->data )
    {
        *f = NULL;
    } else 
    {
	    *f=(ExnodeFunction *)jval_v(e->current->val);
    }
	e->current=e->current->flink;
	
	return(EXNODE_SUCCESS);
}

/**
 * Append subfunction.
 */
int exnodeAppendSubfunction(ExnodeFunction *parent,ExnodeFunction *child)
{
	if(!parent || !child) {
		return(EXNODE_BADPTR);
	}
	
	dll_append(parent->functions,new_jval_v((void *)child));
	
	return(EXNODE_SUCCESS);
}

/**
 * Remove subfunction.
 */
int exnodeRemoveSubfunction(ExnodeFunction *parent,ExnodeFunction *child)
{
	Dllist current;
	
	if(!parent || !child) {
		return(EXNODE_BADPTR);
	}
	
	dll_traverse(current,parent->functions) {
		if(child==jval_v(current->val)) {
			dll_delete_node(current);
			break;
		}
	}
	
	return(EXNODE_SUCCESS);
}
