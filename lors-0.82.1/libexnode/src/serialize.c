/**
 * serialize.c
 *
 * Functions for (de)serializing exnode data structures.
 *
 * Jeremy Millar, Yong Zheng, Stephen Soltesz
 * (c) 2003, University of Tennessee
 */
 
 /**
  * Header files.
	*/
#include "exnode.h"

#define EXNODE_FREE(x)  if ( (x) != NULL ) { free ((x)); } 

/**
 * Function prototypes.
 */
int exnodeSerializeMetadata(ExnodeMetadata *md,xmlDocPtr doc,xmlNsPtr ns,
                            xmlNodePtr anchor);
int exnodeSerializeArgument(ExnodeMetadata *md,xmlDocPtr doc,xmlNsPtr ns,
                            xmlNodePtr anchor);
int exnodeSerializeFunction(ExnodeFunction *f,xmlDocPtr doc,xmlNsPtr ns,
                            xmlNodePtr anchor);
int exnodeSerializeMapping(ExnodeMapping *map,xmlDocPtr doc,xmlNsPtr ns,
                           xmlNodePtr anchor);														
int exnodeDeserializeMetadata(ExnodeMetadata *md,xmlDocPtr doc,
                              xmlNsPtr ns,xmlNodePtr node);
int exnodeDeserializeArguments(ExnodeMetadata *md,xmlDocPtr doc,xmlNsPtr ns,
                               xmlNodePtr node);
int exnodeDeserializeSubfunction(ExnodeFunction *f,xmlDocPtr doc,
                                 xmlNsPtr ns,xmlNodePtr node);
int exnodeDeserializeFunction(ExnodeMapping *map,xmlDocPtr doc,
                              xmlNsPtr ns,xmlNodePtr node);
int exnodeDeserializeMapping(Exnode *exnode,xmlDocPtr doc,xmlNsPtr ns,
	                           xmlNodePtr node);															
															
int exnodeSerializeMetadata(ExnodeMetadata *md,xmlDocPtr doc,xmlNsPtr ns,
                            xmlNodePtr anchor)
{
	JRB children,current;
	xmlNodePtr node;
	char temp[1024];
	
	if(!md) {
		return(EXNODE_BADPTR);
	}
	
	if(md->type==STRING) {
		sprintf(temp,"%s",jval_s(md->val));
		node=xmlNewTextChild(anchor,ns,"metadata",temp);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","string");
	} else if(md->type==INTEGER) {
		sprintf(temp,"%lld",jval_ll(md->val));
		node=xmlNewTextChild(anchor,ns,"metadata",temp);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","integer");
	} else if(md->type==DOUBLE) {
		sprintf(temp,"%lf",jval_d(md->val));
		node=xmlNewTextChild(anchor,ns,"metadata",temp);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","double");
	} else if(md->type==META) {
		node=xmlNewChild(anchor,ns,"metadata",NULL);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","meta");
		
		children=(JRB)jval_v(md->val);
		jrb_traverse(current,children) {
			exnodeSerializeMetadata((ExnodeMetadata *)jval_v(current->val),doc,
			                        ns,node);
		}
	} else {
		return(EXNODE_BADTYPE);
	}
	
	return(EXNODE_SUCCESS);
}

int exnodeSerializeArgument(ExnodeMetadata *md,xmlDocPtr doc,xmlNsPtr ns,
                            xmlNodePtr anchor)
{
	JRB children,current;
	xmlNodePtr node;
	char temp[1024];
	
	if(!md) {
		return(EXNODE_BADPTR);
	}
	
	if(md->type==STRING) {
		sprintf(temp,"%s",jval_s(md->val));
		node=xmlNewTextChild(anchor,ns,"argument",temp);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","string");
	} else if(md->type==INTEGER) {
		sprintf(temp,"%lld",jval_ll(md->val));
		node=xmlNewTextChild(anchor,ns,"argument",temp);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","integer");
	} else if(md->type==DOUBLE) {
		sprintf(temp,"%lf",jval_d(md->val));
		node=xmlNewTextChild(anchor,ns,"argument",temp);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","double");
	} else if(md->type==META) {
		node=xmlNewChild(anchor,ns,"argument",NULL);
		xmlNewProp(node,"name",md->name);
		xmlNewProp(node,"type","meta");
		
		children=(JRB)jval_v(md->val);
		jrb_traverse(current,children) {
			exnodeSerializeArgument((ExnodeMetadata *)jval_v(current->val),doc,
			                        ns,node);
		}
	} else {
		return(EXNODE_BADTYPE);
	}
	
	return(EXNODE_SUCCESS);
}

int exnodeSerializeFunction(ExnodeFunction *f,xmlDocPtr doc,xmlNsPtr ns,
                            xmlNodePtr anchor)
{
	Dllist dllptr;
	JRB children,jrbptr;
	xmlNodePtr node;
	
	if(!f) {
		return(EXNODE_BADPTR);
	}
	
	node=xmlNewChild(anchor,ns,"function",NULL);
	xmlNewProp(node,"name",f->name);
	
	children=(JRB)jval_v(f->metadata->val);

	if(children!=NULL) {
		jrb_traverse(jrbptr,children) {
			exnodeSerializeMetadata((ExnodeMetadata *)jval_v(jrbptr->val),doc,
															ns,node);
		}
	}
	
	children=(JRB)jval_v(f->arguments->val);
	if(children!=NULL) {
		jrb_traverse(jrbptr,children) {
			exnodeSerializeArgument((ExnodeMetadata *)jval_v(jrbptr->val),doc,
															ns,node);
		}
	}
	
	dll_traverse(dllptr,f->functions) {
		exnodeSerializeFunction((ExnodeFunction *)jval_v(dllptr->val),doc,
		                        ns,node);
	}
	
	return(EXNODE_SUCCESS);
}

int exnodeSerializeMapping(ExnodeMapping *map,xmlDocPtr doc,xmlNsPtr ns,
                           xmlNodePtr anchor)
{
	JRB md,current;
	xmlNodePtr node;
	
	node=xmlNewChild(anchor,ns,"mapping",NULL);
	
	md=(JRB)jval_v(map->metadata->val);
	if(md!=NULL) {
		jrb_traverse(current,md) {
			exnodeSerializeMetadata((ExnodeMetadata *)jval_v(current->val),doc,
															ns,node);
		}
	}

	exnodeSerializeFunction(map->function,doc,ns,node);
	
	xmlNewTextChild(node,ns,"read",map->read);
	xmlNewTextChild(node,ns,"write",map->write);
	xmlNewTextChild(node,ns,"manage",map->manage);
	
	return(EXNODE_SUCCESS);
}

int exnodeSerialize(Exnode *exnode,char **buf,int *len)
{
	JRB md,jrbptr;
	Dllist dllptr;
	xmlDocPtr doc;
	xmlNsPtr ns;
	xmlNodePtr node;
	
	doc=xmlNewDoc("1.0");
	node=xmlNewDocRawNode(doc,NULL,"exnode",NULL);
	ns=xmlNewNs(node,"http://loci.cs.utk.edu/exnode","exnode");
	xmlDocSetRootElement(doc,node);
	
	md=(JRB)jval_v(exnode->metadata->val);
	if(md!=NULL) {
		jrb_traverse(jrbptr,md) {
			exnodeSerializeMetadata((ExnodeMetadata *)jval_v(jrbptr->val),
															doc,ns,node);
		}
	}
	
	dll_traverse(dllptr,exnode->mappings) {
		exnodeSerializeMapping((ExnodeMapping *)jval_v(dllptr->val),
		                       doc,ns,node);
	}
	
	xmlDocDumpFormatMemory(doc,buf,len,1);
	xmlFreeDoc(doc);

	return(EXNODE_SUCCESS);
}

int exnodeDeserializeMetadata(ExnodeMetadata *md,xmlDocPtr doc,
                              xmlNsPtr ns,xmlNodePtr node)
{
	ExnodeMetadata *child;
	ExnodeValue value;
	xmlNodePtr cur;
	char *name = NULL ,*val = NULL ,*type = NULL;
	int err;

	memset(&value, 0, sizeof(ExnodeValue));

	type=xmlGetProp(node,"type");
	name=xmlGetProp(node,"name");
	
	if(strcmp(type,"meta")!=0) {
		val=xmlNodeListGetString(doc,node->xmlChildrenNode,1);
		
		if(strcmp(type,"integer")==0) {
			sscanf(val,"%lld",&value.i);
			err=exnodeSetMetadataValue(md,name,value,INTEGER,TRUE);
		} else if(strcmp(type,"double")==0) {
			sscanf(val,"%lf",&value.d);
			err=exnodeSetMetadataValue(md,name,value,DOUBLE,TRUE);
		} else if(strcmp(type,"string")==0) {
			value.s=strdup(val);
			err=exnodeSetMetadataValue(md,name,value,STRING,TRUE);
			free(value.s);
		} else {
			EXNODE_FREE(val);
			EXNODE_FREE(type);
			EXNODE_FREE(name);

			return(EXNODE_BADPARSE);
		}

		EXNODE_FREE(val);
		EXNODE_FREE(type);
		EXNODE_FREE(name);
		
		if(err!=EXNODE_SUCCESS) {
			return(err);
		}
	} else {
		err=exnodeCreateMetadata(&child);
		if(err!=EXNODE_SUCCESS) {
			EXNODE_FREE(type);
			EXNODE_FREE(name);
			return(err);
		}
		
		cur=node->xmlChildrenNode;
		while(cur!=NULL) {
			if((strcmp(cur->name,"metadata")==0) && (cur->ns==ns)) {
				err=exnodeDeserializeMetadata(child,doc,ns,cur);
				if(err!=EXNODE_SUCCESS) {
					EXNODE_FREE(name);
					EXNODE_FREE(type);
					return(err);
				}
			}
			cur=cur->next;
		}
	
		value.m=child;
		err=exnodeSetMetadataValue(md,name,value,META,TRUE);
		if(err!=EXNODE_SUCCESS) {
			EXNODE_FREE(type);
			EXNODE_FREE(name);
			return(err);
		}

		EXNODE_FREE(type);
		EXNODE_FREE(name);
	}

	return(EXNODE_SUCCESS);
}
	
int exnodeDeserializeArguments(ExnodeMetadata *md,xmlDocPtr doc,xmlNsPtr ns,
                                 xmlNodePtr node)
{
	ExnodeMetadata *child;
	ExnodeValue value;
	xmlNodePtr cur;
	char *name = NULL,*val= NULL,*type = NULL;
	int err;

	type=xmlGetProp(node,"type");
	name=xmlGetProp(node,"name");

	if(strcmp(type,"meta")!=0) {
		val=xmlNodeListGetString(doc,node->xmlChildrenNode,1);
		
		if(strcmp(type,"integer")==0) {
			sscanf(val,"%lld",&value.i);
			err=exnodeSetMetadataValue(md,name,value,INTEGER,TRUE);
		} else if(strcmp(type,"double")==0) {
			sscanf(val,"%lf",&value.d);
			err=exnodeSetMetadataValue(md,name,value,DOUBLE,TRUE);
		} else if(strcmp(type,"string")==0) {
			value.s=strdup(val);
			err=exnodeSetMetadataValue(md,name,value,STRING,TRUE);
		} else {
			EXNODE_FREE(type);
			EXNODE_FREE(name);
			EXNODE_FREE(val);

			return(EXNODE_BADPARSE);
		}

		EXNODE_FREE(type);
		EXNODE_FREE(name);
		EXNODE_FREE(val);
		
		if(err!=EXNODE_SUCCESS) {
			return(err);
		}
	} else {
		err=exnodeCreateMetadata(&child);
		if(err!=EXNODE_SUCCESS) {
			EXNODE_FREE(name);
			EXNODE_FREE(type);
			return(err);
		}
		
		cur=node->xmlChildrenNode;
		while(cur!=NULL) {
			if((strcmp(cur->name,"argument")==0) && (cur->ns==ns)) {
				err=exnodeDeserializeMetadata(child,doc,ns,cur);
				if(err!=EXNODE_SUCCESS) {
					EXNODE_FREE(name);
					EXNODE_FREE(type);
					return(err);
				}
			}
			cur=cur->next;
		}
	
		value.m=child;
		err=exnodeSetMetadataValue(md,name,value,META,TRUE);
		if(err!=EXNODE_SUCCESS) {
			EXNODE_FREE(name);
			EXNODE_FREE(type);
			return(err);
		}

		EXNODE_FREE(name);
		EXNODE_FREE(type);
	}


	return(EXNODE_SUCCESS);
}

int exnodeDeserializeSubfunction(ExnodeFunction *f,xmlDocPtr doc,
                                 xmlNsPtr ns,xmlNodePtr node)
{
	ExnodeFunction *sub;
	xmlNodePtr cur;
	char *name = NULL;
	int err;
	
	name=xmlGetProp(node,"name");
	
	err=exnodeCreateFunction(name,&sub);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}

	cur=node->xmlChildrenNode;
	while(cur!=NULL) {
		if((strcmp(cur->name,"metadata")==0) && (node->ns==ns)) {
			err=exnodeDeserializeMetadata(sub->metadata,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyFunction(sub);
				EXNODE_FREE(name);
				return(err);
			}
		} else if((strcmp(cur->name,"argument")==0) && (node->ns==ns)) {
			err=exnodeDeserializeMetadata(sub->arguments,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyFunction(sub);
				EXNODE_FREE(name);
				return(err);
			}
		} else if((strcmp(cur->name,"function")==0) && (node->ns==ns)) {
			err=exnodeDeserializeSubfunction(sub,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyFunction(sub);
				EXNODE_FREE(name);
				return(err);
			}
		}
		cur=cur->next;
	}

	err=exnodeAppendSubfunction(f,sub);

	EXNODE_FREE(name);

	return(EXNODE_SUCCESS);
}

int exnodeDeserializeFunction(ExnodeMapping *map,xmlDocPtr doc,
                              xmlNsPtr ns,xmlNodePtr node)
{
	ExnodeFunction *f;
	xmlNodePtr cur;
	char *name = NULL;
	int err;
	
	name=xmlGetProp(node,"name");
	
	err=exnodeCreateFunction(name,&f);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}
	
	cur=node->xmlChildrenNode;
	while(cur!=NULL) {
		if((strcmp(cur->name,"metadata")==0) && (cur->ns==ns)) {
			err=exnodeDeserializeMetadata(f->metadata,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyFunction(f);
				EXNODE_FREE(name);
				return(err);
			}
		} else if((strcmp(cur->name,"argument")==0) && (cur->ns==ns)) {
			err=exnodeDeserializeMetadata(f->arguments,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyFunction(f);
				EXNODE_FREE(name);
				return(err);
			}
		} else if((strcmp(cur->name,"function")==0) && (cur->ns==ns)) {
			err=exnodeDeserializeSubfunction(f,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyFunction(f);
				EXNODE_FREE(name);
				return(err);
			}
		}
		cur=cur->next;
	}
	
	err=exnodeSetFunction(map,f,TRUE);

	exnodeDestroyFunction(f);
	EXNODE_FREE(name);

	if(err!=EXNODE_SUCCESS) {
		return(err);
	}
	
	return(EXNODE_SUCCESS);
}

int exnodeDeserializeMapping(Exnode *exnode,xmlDocPtr doc,xmlNsPtr ns,
	                           xmlNodePtr node)
{
	xmlNodePtr cur;
	ExnodeMapping *map;
	char *read,*write,*manage;
	int err;
	
	read=NULL;
	write=NULL;
	manage=NULL;
	
	err=exnodeCreateMapping(&map);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}

	cur=node->xmlChildrenNode;
	while(cur!=NULL) {
		if((strcmp(cur->name,"metadata")==0) && (cur->ns==ns)) {
			err=exnodeDeserializeMetadata(map->metadata,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyMapping(map);
				return(err);
			}
		} else if((strcmp(cur->name,"function")==0) && (cur->ns==ns)) {
			err=exnodeDeserializeFunction(map,doc,ns,cur);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyMapping(map);
				return(err);
			}
		} else if((strcmp(cur->name,"read")==0) && (cur->ns==ns)) {
			read=xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		} else if((strcmp(cur->name,"write")==0) && (cur->ns==ns)) {
			write=xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		} else if((strcmp(cur->name,"manage")==0) && (cur->ns==ns)) {
			manage=xmlNodeListGetString(doc,cur->xmlChildrenNode,1);
		} /*else {
			exnodeDestroyMapping(map);
			return(EXNODE_BADPARSE);
		}*/
		cur=cur->next;
	}
	
	err=exnodeSetCapabilities(map,read,write,manage,TRUE);
	if(err!=EXNODE_SUCCESS) {
        if ( read != NULL ) { free (read) ; };
        if ( write != NULL ) { free (write) ; };
        if ( manage != NULL ) { free (manage) ; };
		return(err);
	}
	
	err=exnodeAppendMapping(exnode,map);
	if(err!=EXNODE_SUCCESS) {
        if ( read != NULL ) { free (read) ; };
        if ( write != NULL ) { free (write) ; };
        if ( manage != NULL ) { free (manage) ; };
		return(err);
	}

	if (read != NULL ) { free(read); };
	if (write != NULL ) { free(write); };
	if (manage != NULL ) { free(manage); };
	
	return(EXNODE_SUCCESS);
}

int exnodeDeserialize(char *buf,int len,Exnode **exnode)
{
	Exnode *temp;
	xmlNodePtr node,root;
	xmlDocPtr doc;
	xmlNsPtr ns;
	int err;
	
	err=exnodeCreateExnode(&temp);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}

	doc=xmlParseMemory(buf,len);
	if(doc==NULL) {
		exnodeDestroyExnode(temp);
		return(EXNODE_BADPARSE);
	}

	root=xmlDocGetRootElement(doc);
	if(root==NULL) {
		exnodeDestroyExnode(temp);
		return(EXNODE_BADPARSE);
	}
	
	ns=xmlSearchNsByHref(doc,root,"http://loci.cs.utk.edu/exnode");
	if(ns==NULL) {
		exnodeDestroyExnode(temp);
		return(EXNODE_BADPARSE);
	}
	
	node=root->xmlChildrenNode;
	while(node!=NULL) {
		if((strcmp(node->name,"metadata")==0) && (node->ns==ns)) {
			err=exnodeDeserializeMetadata(temp->metadata,doc,ns,node);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyExnode(temp);
				return(EXNODE_BADPARSE);
			}
		} else if((strcmp(node->name,"mapping")==0) && (node->ns==ns)) {
			err=exnodeDeserializeMapping(temp,doc,ns,node);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyExnode(temp);
				return(EXNODE_BADPARSE);
			}
		} /*else {
			printf("Deserialize: Found unknown tag %s.\n",node->name);
			exnodeDestroyExnode(temp);
			return(EXNODE_BADPARSE);
		}*/
		node=node->next;
	}
	
	*exnode=temp;

	xmlFreeDoc(doc);

	return(EXNODE_SUCCESS);
}
