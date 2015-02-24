/**
 * serialize.c
 *
 * Functions for (de)serializing exnode data structures.
 *
 * Akshay Dorwat
 * (c) 2015, CREST
 */
 
/**
 * Header files.
 */

#include "exnode.h"

#define DEBUG 1

#ifdef DEBUG
#define PRINT(args...) fprintf(args)
#else
#define PRINT(args...)
#endif

#define  NO_OF_MAPPINGS 6

const char *ibp_schema = "http://unis.incntre.iu.edu/schema/exnode/ext/ibp#";
const char *uef_exnode_mapping[][2] = {
	{"name", "filename"},
	{"lorsversion", "lorsversion"},
	{"offset", "exnode_offset"},
	{"alloc_offset", "alloc_offset"},
	{"alloc_length", "alloc_length"},
	{"size", "logical_length"}
};


char *getMapping(const char *name){
	int i;

	for(i=0; i<NO_OF_MAPPINGS; i++){
		if(strcmp(name, uef_exnode_mapping[i][0]) == 0){
			return (char *)uef_exnode_mapping[i][1];
		}
	}

	return NULL;
}

int uefDeserializeMetadata(ExnodeMetadata *md, const char *key, const json_t *value){
	int type;
	ExnodeValue ex_value;
	char *name;
	int err; 

	type = json_typeof(value);
	
	if(type == JSON_STRING){
		name = getMapping(key);
		if(name == NULL){
			fprintf(stdout, "UEF => Exnode mapping for %s not found", key);
			return (EXNODE_BADPARSE);
		}
		ex_value.s=strdup(json_string_value(value));
		err=exnodeSetMetadataValue(md, name, ex_value, STRING, TRUE);
		
	}else if(type == JSON_INTEGER){
		name = getMapping(key);
		if(name == NULL){
			fprintf(stdout, "UEF => Exnode mapping for %s not found", key);
			return (EXNODE_BADPARSE);
		}
		ex_value.i = (long long)json_integer_value(value);
		err=exnodeSetMetadataValue(md, name, ex_value, INTEGER, TRUE);
	}else if(type == JSON_REAL){
		name = getMapping(key);
		if(name == NULL){
			fprintf(stdout, "UEF => Exnode mapping for %s not found", key);
			return (EXNODE_BADPARSE);
		}
		ex_value.d = (double)json_real_value(value);
		err=exnodeSetMetadataValue(md, name, ex_value, DOUBLE, TRUE);
	}
	
	return(EXNODE_SUCCESS);
}

int uefDeserializeMapping(Exnode *exnode, json_t *extent)
{

	ExnodeMapping *map;
	char *read,*write,*manage;
	int err;
	const char *key;
	json_t *value, *ret;
	const char *temp;

	read=NULL;
	write=NULL;
	manage=NULL;
	
	err=exnodeCreateMapping(&map);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}
	
	json_object_foreach(extent, key, value){
		
		if(strcmp(key,"$schema")==0){
			temp = json_string_value(value);
			// we are only suppoprting IBP storage 
			if(temp == NULL || strcmp(temp, ibp_schema) != 0){
				exnodeDestroyMapping(map);
				return(EXNODE_SUCCESS);
			}
		}else if(strcmp(key,"offset") == 0){
			err=uefDeserializeMetadata(map->metadata, key, value);
			if(err != EXNODE_SUCCESS){
				exnodeDestroyMapping(map);
				return(EXNODE_BADPARSE);
			}
			// Need this field to maintain end to end integrity 
			err=uefDeserializeMetadata(map->metadata, "alloc_offset", json_integer(0));
			if(err != EXNODE_SUCCESS){
				exnodeDestroyMapping(map);
				return(EXNODE_BADPARSE);
			}
		}else if(strcmp(key,"size") == 0){
			err=uefDeserializeMetadata(map->metadata, key, value);
			if(err != EXNODE_SUCCESS){
				exnodeDestroyMapping(map);
				return(EXNODE_BADPARSE);
			}
			// Need this field to maintain end to end integrity 
			err=uefDeserializeMetadata(map->metadata, "alloc_length", value);
			if(err != EXNODE_SUCCESS){
				exnodeDestroyMapping(map);
				return(EXNODE_BADPARSE);
			}
		}else if(strcmp(key,"mapping") == 0){
			if(!json_is_object(value)){
				exnodeDestroyMapping(map);
				return(EXNODE_BADPARSE);
			}
			
			if((ret = json_object_get(value, "read")) != NULL){
				read = json_string_value(ret);
			}
			if((ret = json_object_get(value, "write")) != NULL){
				write = json_string_value(ret);
			}
			if((ret = json_object_get(value, "manage")) != NULL){
				manage = json_string_value(ret);
			}
		}
	}


	
	err=exnodeSetCapabilities(map,read,write,manage,TRUE);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}
	
	err=exnodeAppendMapping(exnode,map);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}

	return(EXNODE_SUCCESS);
}


int uefDeserialize(char *buf, int len, Exnode **exnode)
{
	Exnode        *temp;
	json_t        *json_ret, *json_extent;
	json_error_t  json_err;
	int           err;
	int           num_obj;
	const char    *key;
	json_t        *value;
	const char    *mode;
	void          *iter;
	size_t        index;

	err=exnodeCreateExnode(&temp);
	if(err!=EXNODE_SUCCESS) {
		return(err);
	}

	json_ret = json_loads(buf, 0, &json_err);
	if(json_ret == NULL){
		fprintf(stderr, "Could not decode JSON: %d: %s\n", json_err.line, json_err.text);
		return (EXNODE_BADPARSE);
	}

	//PRINT(stdout," %s \n",json_dumps(json_ret, JSON_INDENT(4)));

	if(json_is_array(json_ret)){
		num_obj = json_array_size(json_ret);
		if(num_obj <= 0 || num_obj > 1){
			fprintf(stdout, "JSON contain for than 1 exnode or no exnode !");
			return (EXNODE_BADPARSE);
		}
		json_ret = json_array_get(json_ret, 0);
	}
	
	if(!json_is_object(json_ret)){
		fprintf(stdout, "Unknow JSON type found, expecting JSON object");
		return (EXNODE_BADPARSE);
	}

	iter = json_object_iter(json_ret);

	while(iter){
		key = json_object_iter_key(iter);
		value = json_object_iter_value(iter);
		
		if(strcmp(key,"name") == 0) {
			err=uefDeserializeMetadata(temp->metadata, key, value);
			if(err!=EXNODE_SUCCESS) {
				exnodeDestroyExnode(temp);
				return(EXNODE_BADPARSE);
			}
		}else if(strcmp(key, "mode") == 0){
			mode = json_string_value(value);
			if(mode != NULL){
				if(strcmp(mode, "file") != 0){
					fprintf(stderr, "Can not parse Folder");
					return (EXNODE_BADPARSE);
				}
			}else{
				fprintf(stderr, "WARNING: Found mode NULL");
			}
		}else if(strcmp(key, "extents") == 0){
			if(!json_is_array(value)){
				fprintf(stderr, "Expecting extents araay");
				return (EXNODE_BADPARSE);
			}
			json_array_foreach(value, index, json_extent){
				err=uefDeserializeMapping(temp, json_extent);
				if(err!=EXNODE_SUCCESS) {
					exnodeDestroyExnode(temp);
					return(EXNODE_BADPARSE);
				}
			}
		}
		iter = json_object_iter_next(json_ret, iter);
	}

	// Need to this to avoid warnings in the code
	err=uefDeserializeMetadata(temp->metadata, "lorsversion", json_real(0.826000));
	*exnode=temp;

	return(EXNODE_SUCCESS);
}
