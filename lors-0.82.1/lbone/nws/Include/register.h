/* $Id: register.h,v 1.17 2000/02/29 20:04:56 hayes Exp $ */

#ifndef REGISTER_H
#define REGISTER_H

/*
** This package defines the interface for updating NWS name server records.
*/


#include "hosts.h" /* host_cookie */


/* Definitions of name server attributes, objects, and sets of objects. */
typedef char *Attribute;
#define NO_ATTRIBUTE NULL
typedef char *Object;
#define NO_OBJECT NULL
typedef char *ObjectSet;
#define NO_OBJECT_SET NULL


/*
** Adds an attribute consisting of the name/value pair #addName#/#addValue# to
** the object pointed to by #toObject#.
*/
void
AddAttribute(Object *toObject,
             const char *addName,
             const char *addValue);


/*
** Inserts a copy of #addObject# into #toSet#.
*/
void
AddObject(ObjectSet *toSet,
          const Object addObject);


/*
** Returns the name of #ofAttribute#.  The value returned is volatile and will
** be overwritten by subsequent calls.
*/
const char *
AttributeName(const Attribute ofAttribute);


/*
** Returns the value of #ofAttribute#.  The value returned is volatile and will
** be overwritten by subsequent calls.
*/
const char *
AttributeValue(const Attribute ofAttribute);


/*
** Returns the first attribute in #ofObject# that with name #name#, or
** NO_ATTRIBUTE if no attribute in #ofObject# has that name.
*/
const Attribute
FindAttribute(const Object ofObject,
              const char *name);


/*
** Frees the object pointed to by #toBeFreed#, along with all the attributes
** it contains, and sets #toBeFreed# to NO_OBJECT.
*/
void
FreeObject(Object *toBeFreed);


/*
** Frees the object set pointed to by #toBeFreed#, along with all the objects
** it contains, and sets #toBeFreed# to NO_OBJECT_SET.
*/
void
FreeObjectSet(ObjectSet *toBeFreed);


/*
** Contacts #withWho# to retrieve registration information for #name#.  If
** successful, returns 1 and stores the registered attributes in #object#
** (which should eventually be passed to FreeObject()); else returns 0.
*/
int
Lookup(struct host_cookie *whoFrom,
       const char *name,
       Object *object);


/*
** Returns a new object containing no attributes, which should eventually be
** passed to FreeObject().
*/
Object
NewObject(void);


/*
** Returns a new object set containing no objects, which should eventually be
** passed to FreeObjectSet().
*/
ObjectSet
NewObjectSet(void);


/*
** Returns the attribute in #ofObject# that follows #preceding#, or
** NO_ATTRIBUTE if #preceding# is the last attribute in #ofObject#.
** #preceding# may be NO_ATTRIBUTE, in which case the first attribute in
** #ofObject# is returned.
*/
const Attribute
NextAttribute(const Object ofObject,
              const Attribute preceding);


/*
** Returns the object in #ofSet# that follows #preceding#, or NO_OBJECT if
** #preceding# is the last object in #ofSet#.  #preceding# may be NO_OBJECT, in
** which case the first object in #ofSet# is returned.
*/
const Object
NextObject(const ObjectSet ofSet,
           const Object preceding);


/*
** Registers #object# with the name server connected to #withWho#.  The
** registration will last for #forHowLong# seconds, or forever if #forHowLong#
** is 0.  Returns 1 if successful, else 0.
*/
#define NEVER_EXPIRE 0.0
int
Register(struct host_cookie *withWho,
         const Object object,
         double forHowLong);


/*
** Retrieves from #whoFrom# all objects that have attributes that match
** #filter#.  #filter# has an LDAP-like format of attribute name/value pairs;
** the value may contain wildcard characters.  If successful, returns 1 and
** sets #retrieved# to a set containing the retrieved objects (which should
** eventually be passed to FreeObjectSet()); else returns 0.
*/
int
RetrieveObjects(struct host_cookie *whoFrom,
                const char *filter,
                ObjectSet *retrieved);


/*
** Directs the name server connected to #withWho# to purge all registrations
** matching #filter#, a LDAP-like filter like that passed to RetrieveObjets().
** Returns 1 if successful, else 0.
*/
int
Unregister(struct host_cookie *withWho,
           const char *filter);


#endif
