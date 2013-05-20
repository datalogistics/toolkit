/**
 * exnode.h
 *
 * Defines basic datatypes and function prototypes for libexnode.
 *
 * Jeremy Millar
 * (c) 2002, University of Tennessee
 */

#ifndef _EXNODE
#define _EXNODE

#ifdef __cplusplus
extern "C" {
#endif

 /**
  * Header files.
	*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/tree.h>
#include <libxml/parser.h>
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

/**
 * Error codes.
 */
#define EXNODE_SUCCESS              0
#define EXNODE_NOMEM               -1
#define EXNODE_BADPTR              -2
#define EXNODE_EXISTS              -3
#define EXNODE_NONEXISTANT         -4
#define EXNODE_BADTYPE             -5
#define EXNODE_BADPARSE            -6

/**
 * Boolean type.
 */
#ifdef FALSE
#undef FALSE
#undef TRUE
#endif
typedef enum _boolean {FALSE,TRUE} ExnodeBoolean;

/**
 * Basic types of metadata.
 */
typedef enum _type {NONE,STRING,INTEGER,DOUBLE,META} ExnodeType;

/**
 * Enumeration types.
 */
typedef enum _enumtype {NAME,FUNC,MAP} ExnodeEnumType;
/**
 * Enumeration. Holds the result of queries.
 */
typedef struct _enumeration {
	Dllist data;
	Dllist current;
	ExnodeEnumType type;
} ExnodeEnumeration;

/**
 * Metadata. Note that nesting metadata is now weird. Children should be
 * stored as a red-black tree whose root pointer is stashed in a Jval.
 * This Jval is stored as the value of the containing metadata.
 */
typedef struct _metadata {
	Jval val;
	ExnodeType type;
	char *name;
} ExnodeMetadata;


/**
 * Value. Union of possible metadata value types.
 */
typedef union _value {
	long long i;
	double d;
	char *s;
	ExnodeMetadata *m;
} ExnodeValue;

/**
 * Function. Note that arguments are really just metadata in disguise.
 * Try not to do something bizarre like having strings and integers as
 * children of an argument. Argument nesting should be reserved for
 * naturally nested structures, like vectors and matrices. Also note that
 * functions can have multiple child functions. This is useful for things
 * like vector functions, but can also lend itself to some weirdness. Be
 * careful. Note that the metadata and argument lists are anonymous -
 * call getMetadata() before trying to do anything with them.
 */
typedef struct _function {
	char *name;
	ExnodeMetadata *metadata;
	ExnodeMetadata *arguments;
	Dllist functions;
} ExnodeFunction;

/**
 * Mapping. Note that metadata is weird because values are two (or is it 
 * three?) levels of indirection deep. That is, each ExnodeMetadata
 * structure is stored as a void * in a Jval, which is stored in a 
 * red-black tree. Not only that, but the top level of metadata is 
 * anonymous.
 */
typedef struct _mapping {
	ExnodeMetadata *metadata;
	ExnodeFunction *function;
	char *read;
	char *write;
	char *manage;
} ExnodeMapping;

/**
 * Exnode. Metadata is weird, as usual. Mappings are fairly straightforward
 * since there's only one (or is it two?) levels of indirection there.
 */
typedef struct _exnode {
	ExnodeMetadata *metadata;
	Dllist mappings;
} Exnode;

/**
 * Operations on metadata
 * -----------------------
 * Create
 * Destroy
 * Copy
 * Get names of children
 * Get value of named child
 * Set value of named child (creates new object)
 * Remove named child
 */

/**
 * Create metadata.
 */
extern int exnodeCreateMetadata(ExnodeMetadata **md);

/**
 * Destroy metadata.
 * Deep.
 */
extern int exnodeDestroyMetadata(ExnodeMetadata *md);

/**
 * Copy metadata.
 * Performs a deep copy.
 */
extern int exnodeCopyMetadata(ExnodeMetadata **dest,ExnodeMetadata *src);

/**
 * Get names of children. Only valid if metadata is of type META. Returns
 * an enumeration.
 */
extern int exnodeGetMetadataNames(ExnodeMetadata *md,ExnodeEnumeration **e);

/**
 * Get the value and type of a named child. Only valid if metadata is of
 * type META. This and the set and remove functions are the reason for the
 * anonymous metadata containers.
 */
extern int exnodeGetMetadataValue(ExnodeMetadata *md,char *name,
                                  ExnodeValue *value,ExnodeType *type);

/**
 * Set the value and type of a named child. Set the replace flag to TRUE
 * to replace values in a pre-existing child. If no child with the
 * specified name exists, a new one will be created.
 */
extern int exnodeSetMetadataValue(ExnodeMetadata *md,char *name,
                                  ExnodeValue value,ExnodeType type,
																	ExnodeBoolean replace);

/**
 * Remove and destroy the named child.
 */
extern int exnodeRemoveMetadata(ExnodeMetadata *md,char *name);


/**
 * Operations on functions
 * -----------------------
 * Create
 * Destroy
 * Copy
 * Get name
 * Get arguments
 * Get subfunctions
 * Get metadata
 * Append subfunction
 * Remove subfunction
 */

/**
 * Create function.
 */
extern int exnodeCreateFunction(char *name,ExnodeFunction **f);

/**
 * Destroy function.
 * Deep.
 */
extern int exnodeDestroyFunction(ExnodeFunction *f);

/**
 * Copy function.
 * Deep.
 */
extern int exnodeCopyFunction(ExnodeFunction **dest,ExnodeFunction *src);

/**
 * Get function name.
 */
extern int exnodeGetFunctionName(ExnodeFunction *f,char **name);

/**
 * Get function arguments. Returns a pointer to an anonymous metadata
 * object. Use this object to get/set argument names and values. Don't
 * call destroy on it - you'll be sorry if you do.
 */
extern int exnodeGetFunctionArguments(ExnodeFunction *f,ExnodeMetadata **arg);

/**
 * Get subfunctions. Returns an enumeration object to be used for traversing
 * the function list. More than one subfunction indicates the parent
 * function takes vector arguments.
 */
extern int exnodeGetSubfunctions(ExnodeFunction *f,ExnodeEnumeration **e);

/**
 * Get metadata associated with a function. Returns a pointer to an 
 * anonymous metadata object. Use this object to get/set metadata. Don't
 * destroy the metadata object returned by this function.
 */
extern int exnodeGetFunctionMetadata(ExnodeFunction *f,ExnodeMetadata **md);

/**
 * Append a subfunction.
 */
extern int exnodeAppendSubfunction(ExnodeFunction *parent,
                                   ExnodeFunction *child);

/**
 * Remove a subfunction.
 */
extern int exnodeRemoveSubfunction(ExnodeFunction *parent,
                                   ExnodeFunction *child);

/**
 * Operations on mappings
 * ----------------------
 * Create
 * Destroy
 * Copy
 * Get metadata
 * Get function
 * Get capabilities
 * Set capabilities
 */

/**
 * Create a mapping.
 */
extern int exnodeCreateMapping(ExnodeMapping **map);

/**
 * Destroy a mapping.
 * Deep.
 */
extern int exnodeDestroyMapping(ExnodeMapping *map);

/**
 * Copy a mapping.
 * Deep.
 */
extern int exnodeCopyMapping(ExnodeMapping **dest,ExnodeMapping *src);

/**
 * Get mapping metadata. Does not allocate memory, so don't call destroy
 * on the returned metadata object.
 */
extern int exnodeGetMappingMetadata(ExnodeMapping *map,ExnodeMetadata **md);

/**
 * Set data function. Does a deep copy.
 */
extern int exnodeSetFunction(ExnodeMapping *map,ExnodeFunction *f,
                             ExnodeBoolean replace);

/**
 * Get data function. Does not allocate memory, so don't call destroy on the
 * returned function object.
 */
extern int exnodeGetFunction(ExnodeMapping *map,ExnodeFunction **f);

/**
 * Get the capabilities from a mapping. The user is responsible for freeing
 * the returned strings.
 */
extern int exnodeGetCapabilities(ExnodeMapping *map,char **read,char **write,
                                 char **manage);

/**
 * Set the mapping capabilities. If replace is FALSE, setting any of
 * read/write/manage to NULL will leave that value untouched.
 */
extern int exnodeSetCapabilities(ExnodeMapping *map,char *read,char *write,
                                 char *manage,ExnodeBoolean replace);

/**
 * Operations on exnodes
 * ---------------------
 * Create
 * Destroy
 * Copy
 * Get mappings
 * Get metadata
 * Serialize
 * Deserialize
 */

/**
 * Create an exnode.
 */
extern int exnodeCreateExnode(Exnode **exnode);

/**
 * Destroy an exnode. Deep.
 */
extern int exnodeDestroyExnode(Exnode *exnode);

/**
 * Copy an exnode. Deep.
 */
extern int exnodeCopyExnode(Exnode **dest,Exnode *src);

/**
 * Get exnode metadata. Does not allocate memory, so don't call destroy on
 * the returned metadata.
 */
extern int exnodeGetExnodeMetadata(Exnode *exnode,ExnodeMetadata **md);

/**
 * Get mappings from an exnode. Returns an enumeration object for stepping
 * through the mappings.
 */
extern int exnodeGetMappings(Exnode *exnode,ExnodeEnumeration **e);

/**
 * Append a mapping to an exnode.
 */
extern int exnodeAppendMapping(Exnode *exnode,ExnodeMapping *map);

/**
 * Serialize an exnode. 
 */
extern int exnodeSerialize(Exnode *exnode,char **buf,int *len);

/**
 * Deserialize an exnode.
 */
extern int exnodeDeserialize(char *buf,int len,Exnode **exnode);

/**
 * Operations on enumerations
 * --------------------------
 * Destroy
 * Get next {name|subfunction|mapping}
 */

/**
 * Destroy an enumeration.
 */
extern int exnodeDestroyEnumeration(ExnodeEnumeration *e);

/**
 * Get next name.
 */
extern int exnodeGetNextName(ExnodeEnumeration *e,char **name);

/**
 * Get next subfunction.
 */
extern int exnodeGetNextSubfunction(ExnodeEnumeration *e,ExnodeFunction **f);

/**
 * Get next mapping.
 */
extern int exnodeGetNextMapping(ExnodeEnumeration *e,ExnodeMapping **map);

#ifdef __cplusplus
}
#endif

#endif
