/**
 * metadata.c
 *
 * Functions for manipulating metadata.
 *
 * Jeremy Millar
 * (c) 2002, University of Tennessee
 */
 
/**
 * Header files.
 */
#include "exnode.h"

/**
 * Create metadata.
 */
int exnodeCreateMetadata(ExnodeMetadata **md)
{
	ExnodeMetadata *temp;

    /* for memory leaking */
	/*temp=(ExnodeMetadata *)malloc(sizeof(*temp));*/
	temp=(ExnodeMetadata *)calloc(1,sizeof(*temp));
    memset(temp, 0x54, sizeof(ExnodeMetadata));
	

	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
    /*fprintf(stderr, "CreateMetadata: 0x%x\n", temp);*/
	
	temp->type=NONE;
	temp->name=NULL;
	temp->val=new_jval_v(NULL);
	
	*md=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Destroy metadata.
 */
int exnodeDestroyMetadata(ExnodeMetadata *md)
{
	JRB children,cur;
	
	if(!md) {
		return(EXNODE_BADPTR);
	}

    /*fprintf(stderr, "DestroyMetadata: 0x%x Type: %d Name: %s\n",*/
            /*md, md->type, (md->name!= NULL? md->name: "NullName" ));*/
    
    if(md->type == META) {
		children=(JRB)jval_v(md->val);
        /*fprintf(stderr, "\tChildren: 0x%x\n", children);*/
		jrb_traverse(cur,children) {
			exnodeDestroyMetadata((ExnodeMetadata *)jval_v(cur->val));
            cur->val.v = NULL;
		}
		jrb_free_tree(children);
        md->val.v = 0x00;
	}

/* below added by Yong Zheng for fixing memory leaking */
    if ( md->type == STRING ) 
    { 
        free (md->val.s); 
        md->val.s= NULL;
    };
    if ( md->name != NULL )
    {
	    free(md->name);
        md->name = NULL;
    }
	free(md);
	
	return(EXNODE_SUCCESS);
}

/**
 * Copy metadata.
 */
int exnodeCopyMetadata(ExnodeMetadata **dest,ExnodeMetadata *src)
{
	ExnodeMetadata *dchild,*temp;
	JRB children,current;
	
	temp=(ExnodeMetadata *)calloc(1, sizeof(*temp));
	if(temp==NULL) {
		return(EXNODE_NOMEM);
	}
    memset(temp, 0x00, sizeof(*temp));


    /*fprintf(stderr, "CopyMetadata: From: 0x%x To: 0x%x Type: %d Name: %s\n", */
            /*src, temp, src->type, (src->name!=NULL?
             * src->name:"NullName"));*/

	if(src->name!=NULL) {
		temp->name=strdup(src->name);
	} else {
		temp->name=NULL;
	}
	
	temp->type=src->type;
	
	if(temp->type==META) {
		temp->val.v=(void *)make_jrb();
		children=(JRB)jval_v(src->val);
        /*fprintf(stderr, "\tCopyMetadataChildren: 0x%x\n", children);*/
		jrb_traverse(current,children) {
			exnodeCopyMetadata(&dchild,(ExnodeMetadata *)jval_v(current->val));
			jrb_insert_str((JRB)jval_v(temp->val),dchild->name,
			               new_jval_v((void *)dchild));
		}
	} else {
        temp->val = src->val;
        if (temp->type == STRING){
		    temp->val.s= strdup(src->val.s);
        };    
	}
	
	*dest=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Get names of children.
 */
int exnodeGetMetadataNames(ExnodeMetadata *md,ExnodeEnumeration **e)
{
	ExnodeMetadata *child;
	ExnodeEnumeration *temp;
	JRB current,children;
	Jval val;
	
	if(md->type!=META) {
		return(EXNODE_BADTYPE);
	}
	
	temp=(ExnodeEnumeration *)malloc(sizeof(*temp));
	temp->type=NAME;
	temp->data=new_dllist();
	
	children=(JRB)jval_v(md->val);
	jrb_traverse(current,children) {
		child=(ExnodeMetadata *)jval_v(current->val);
		val=new_jval_s(child->name);
		dll_append(temp->data,val);
	}
	
	temp->current=dll_first(temp->data);

	*e=temp;
	return(EXNODE_SUCCESS);
}

/**
 * Get next name.
 */
int exnodeGetNextName(ExnodeEnumeration *e,char **name)
{
	if(!e) {
		return(EXNODE_BADPTR);
	}
	
	if(e->type!=NAME) {
		return(EXNODE_BADTYPE);
	}

    if ( e->current == e->data )
    {
        *name = NULL;
    } else 
    {
		*name=strdup(e->current->val.s);
    }
	e->current=e->current->flink;
	
	return(EXNODE_SUCCESS);
}

/**
 * Get value of named child.
 */
int exnodeGetMetadataValue(ExnodeMetadata *md,char *name,ExnodeValue *value,
                           ExnodeType *type)
{
	ExnodeMetadata *m;
	JRB child;
	
    if ( md->val.v != NULL )
    {
	    child=jrb_find_str((JRB)jval_v(md->val),name);
	    if(child==NULL) {
	    	return(EXNODE_NONEXISTANT);
	    }
    } else 
    {
        return EXNODE_NONEXISTANT;
    }
	
	m=(ExnodeMetadata *)jval_v(child->val);
	
	*type=m->type;
	if(m->type==INTEGER) {
		(*value).i=jval_ll(m->val);
	} else if(m->type==DOUBLE) {
		(*value).d=jval_d(m->val);
	} else if(m->type==STRING) {
		(*value).s=strdup(jval_s(m->val));
	} else if(m->type==META) {
		(*value).m=m;
	} else {
		return(EXNODE_BADTYPE);
	}
	
	return(EXNODE_SUCCESS);
}

/**
 * Set the value of a named child.
 */
int exnodeSetMetadataValue(ExnodeMetadata *md,char *name,ExnodeValue value,
                           ExnodeType type,ExnodeBoolean replace)
{
	JRB child,children;
	ExnodeMetadata *m;
	int err;

	md->type=META;
	
	children=(JRB)jval_v(md->val);
	if(children!=NULL) {
		child=jrb_find_str(children,name);
	} else {
		md->val.v=(void *)make_jrb();
		child=NULL;
	}
	
	if(child!=NULL) {
		if(replace==FALSE) {
			return(EXNODE_EXISTS);
		}
		
		m=(ExnodeMetadata *)jval_v(child->val);
	} else {
		err=exnodeCreateMetadata(&m);
		if(err!=EXNODE_SUCCESS) {
			return(err);
		}
	}

    if ( child == NULL ){
    /*if (m->name != NULL ){
        free(m->name);
    };*/
	    m->name=strdup(name);
    };
	m->type=type;
	if(type==INTEGER) {
		m->val.ll=value.i;
	} else if(type==DOUBLE) {
		m->val.d=value.d;
	} else if(type==STRING) {
        if ( m->val.s != NULL ){
            free(m->val.s);
        };
		m->val.s=strdup(value.s);
	} else if(type==META) {
		m->val=value.m->val;
	} else {
		return(EXNODE_BADTYPE);
	}
/* below added by Yong Zheng for fixing memory leaking */	
    if ( child == NULL ){	
       jrb_insert_str((JRB)jval_v(md->val),m->name,new_jval_v((void *)m));
    };
	/*jrb_insert_str((JRB)jval_v(md->val),name,new_jval_v((void *)m));*/
/**********************************************************************/    
	
	return(EXNODE_SUCCESS);
}

/**
 * Remove and destroy a named child.
 */
int exnodeRemoveMetadata(ExnodeMetadata *md,char *name)
{
	JRB child;
	
	child=jrb_find_str((JRB)jval_v(md->val),name);
	
	if(child==NULL) {
		return(EXNODE_NONEXISTANT);
	} else {
		exnodeDestroyMetadata((ExnodeMetadata *)jval_v(child->val));
	}
	
	return(EXNODE_SUCCESS);
}
