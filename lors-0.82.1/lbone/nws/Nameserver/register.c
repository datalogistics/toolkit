/* $Id: register.c,v 1.12 2000/06/28 19:15:55 swany Exp $ */

#include "config.h"
#include <stdlib.h>         /* free() malloc() REALLOC() */
#include <string.h>         /* memcpy() strchr() strlen() strstr() */
#include "protocol.h"       /* SocketFailure() */
#include "host_protocol.h"  /* ConnectToHost()  */
#include "diagnostic.h"     /* ERROR() FAIL() WARN() */
#include "strutil.h"        /* SAFESTRCPY() strmatch() vstrncpy() */
#include "messages.h"       /* Recv*() Send*() */
#include "osutil.h"         /* CurrentTime() */
#include "nws_nameserver.h" /* Nameserver-specific messages */
#include "register.h"


/*
** Note: We hold object sets in the format used by the name server: an object
** is a tab-delimited, tab-pair-terminated list of attribute:value pairs.  An
** object set is simply a list of these.
*/
#define ATTRIBUTE_TERMINATOR "\t"
#define NAME_TERMINATOR ":"
#define OBJECT_TERMINATOR "\t\t"


/*
** Values returned from Attribute{Name,Value}.  We switched from static, fixed-
** size arrays to allocated items because the potential length (of values, at
** least) is unbounded.  This could cause a memory leak, but, since attributes
** only exist within objects, we can be assured of freeing the memory by
** checking the variable values within Free{Object,ObjectSet}.
*/
static char *currentName = NULL;
static char *currentValue = NULL;


void
AddAttribute(Object *toObject,
             const char *addName,
             const char *addValue) {

  Object expanded;
  size_t expandedLen;
  size_t objectLen;
  const char *separator;

  objectLen = strlen(*toObject);
  separator =
    (objectLen == strlen(OBJECT_TERMINATOR)) ? "" : ATTRIBUTE_TERMINATOR;

  expandedLen = objectLen + strlen(separator) + strlen(addName) +
                strlen(NAME_TERMINATOR) + strlen(addValue) + 1;
  expanded = (Object)REALLOC(*toObject, expandedLen);

  if(expanded == NULL) {
    ERROR("AddAttribute: out of memory\n");
    return;
  }

  /* Overwrite the existing object terminator. */
  vstrncpy(expanded + objectLen - strlen(OBJECT_TERMINATOR),
           expandedLen - objectLen + strlen(OBJECT_TERMINATOR), 5,
           separator, addName, NAME_TERMINATOR, addValue, OBJECT_TERMINATOR);
  *toObject = expanded;

}


void
AddObject(ObjectSet *toSet,
          const Object addObject) {

  const char *endObject;
  ObjectSet expanded;
  size_t objectLen;
  size_t setLen;

  endObject = strstr(addObject, OBJECT_TERMINATOR);
  objectLen = endObject - addObject + strlen(OBJECT_TERMINATOR);
  setLen = strlen(*toSet);
  expanded = (ObjectSet)REALLOC(*toSet, setLen + objectLen + 1);

  if(expanded == NULL) {
    ERROR("AddObject: out of memory\n");
    return;
  }

  memcpy(expanded + setLen, addObject, objectLen);
  expanded[setLen + objectLen] = '\0';
  *toSet = expanded;

}


const char *
AttributeName(const Attribute ofAttribute) {

  const char *endOfName;
  size_t nameLen;

  if(currentName != NULL) {
    free(currentName);
    currentName = NULL;
  }

  endOfName = strstr(ofAttribute, NAME_TERMINATOR);

  if(endOfName != NULL) {
    nameLen = endOfName - ofAttribute;
    currentName = (char *)malloc(nameLen + 1);
    if(currentName != NULL) {
      memcpy(currentName, ofAttribute, nameLen);
      currentName[nameLen] = '\0';
    }
  }

  return(currentName);

}


const char *
AttributeValue(const Attribute ofAttribute) {

  const char *endOfName;
  const char *endOfValue;
  size_t valueLen;

  if(currentValue != NULL) {
    free(currentValue);
    currentValue = NULL;
  }

  endOfName = strstr(ofAttribute, NAME_TERMINATOR);
  if(endOfName == NULL) {
    return NULL;
  }

  endOfValue = strstr(endOfName + 1, ATTRIBUTE_TERMINATOR);
  if(endOfValue == NULL) {
    return NULL;
  }

  valueLen = endOfValue - endOfName - 1;
  currentValue = (char *)malloc(valueLen + 1);
  if(currentValue != NULL) {
    memcpy(currentValue, endOfName + 1, valueLen);
    currentValue[valueLen] = '\0';
  }

  return(currentValue);

}


const Attribute
FindAttribute(const Object ofObject,
              const char *name) {
  size_t nameLen = strlen(name);
  Attribute nextAttr;
  for(nextAttr = NextAttribute(ofObject, NO_ATTRIBUTE);
      nextAttr != NO_ATTRIBUTE;
      nextAttr = NextAttribute(ofObject, nextAttr)) {
    if((strncmp(nextAttr, name, nameLen) == 0) &&
       (*(nextAttr + nameLen) == ':')) {
      break;
    }
  }
  return nextAttr;
}


void
FreeObject(Object *toBeFreed) {
  if(*toBeFreed != NO_OBJECT) {
    free(*toBeFreed);
    *toBeFreed = NO_OBJECT;
  }
  if(currentName != NULL) {
    free(currentName);
    currentName = NULL;
  }
  if(currentValue != NULL) {
    free(currentName);
    currentName = NULL;
  }
}


void
FreeObjectSet(ObjectSet *toBeFreed) {
  if(*toBeFreed != NO_OBJECT_SET) {
    free(*toBeFreed);
    *toBeFreed = NO_OBJECT_SET;
  }
  if(currentName != NULL) {
    free(currentName);
    currentName = NULL;
  }
  if(currentValue != NULL) {
    free(currentName);
    currentName = NULL;
  }
}


int
Lookup(struct host_cookie *withWho,
       const char *name,
       Object *object) {

  char nameFilter[127 + 1];
  ObjectSet nameSet;

  vstrncpy(nameFilter, sizeof(nameFilter), 2, "name=", name);
  if(!RetrieveObjects(withWho, nameFilter, &nameSet)) {
    FAIL("Lookup: retrieval failed\n");
  }
  else if(*nameSet == '\0') {
    FreeObjectSet(&nameSet);
    return(0);
  }
  else {
    /*
    ** Note that we're assigning an ObjectSet to an Object here.  We get away
    ** with it because they're both just char *.
    */
    *object = nameSet;
    return(1);
  }

}


Object
NewObject(void) {
  return(strdup(OBJECT_TERMINATOR));
}


ObjectSet
NewObjectSet(void) {
  return(strdup(""));
}


const Attribute
NextAttribute(const Object ofObject,
              const Attribute preceding) {
  char *nextOne =
    (preceding == NO_ATTRIBUTE) ? ofObject :
                                  (strstr(preceding, ATTRIBUTE_TERMINATOR) +
                                   strlen(ATTRIBUTE_TERMINATOR));
  return((*nextOne == '\t') ? NO_ATTRIBUTE : nextOne);
}


const Object
NextObject(const ObjectSet ofSet,
           const Object preceding) {
  char *nextOne =
    (preceding == NO_OBJECT) ? ofSet :
                               (strstr(preceding, OBJECT_TERMINATOR) +
                                strlen(OBJECT_TERMINATOR));
  return((*nextOne == '\0') ? NO_OBJECT : nextOne);
}


int
Register(struct host_cookie *withWho,
         const Object object,
         double forHowLong) {

  unsigned long expiration;
  DataDescriptor objectDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  size_t replySize;
  Socket sd;
  DataDescriptor timeDescriptor = SIMPLE_DATA(UNSIGNED_LONG_TYPE, 1);

  if(!ConnectToHost(withWho, &sd)) {
    FAIL1("Register: failed to connect to %s\n", HostCImage(withWho));
  }

  objectDescriptor.repetitions =
    strstr(object, OBJECT_TERMINATOR) - object + strlen(OBJECT_TERMINATOR);
  expiration = (unsigned long)forHowLong;

  if(!SendMessageAndDatas(sd,
                          NS_REGISTER,
                          object,
                          &objectDescriptor,
                          1,
                          &expiration,
                          &timeDescriptor,
                          1,
                          PktTimeOut(sd))) {
    FAIL("Register: send message failed\n");
  }

  if(!RecvMessage(sd, NS_REGISTERED, &replySize, PktTimeOut(sd))) {
    WARN("Register: acknowledgement message failed\n");
  }

  return(1);

}


int
RetrieveObjects(struct host_cookie *whoFrom,
                const char *filter,
                ObjectSet *retrieved) {

  DataDescriptor filterDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  ObjectSet filtered;
  DataDescriptor objectsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  size_t replySize;
  Socket sd;

  if(!ConnectToHost(whoFrom, &sd)) {
    FAIL("RetrieveFiltered: unable to contact name server.\n");
  }

  filterDescriptor.repetitions = strlen(filter) + 1;
  if(!SendMessageAndData(sd,
                         NS_SEARCH,
                         filter,
                         &filterDescriptor,
                         1,
                         PktTimeOut(sd))) {
    FAIL("RetrieveObjects: send failed\n");
  }
  else if(!RecvMessage(sd,
                       NS_SEARCHED,
                       &replySize,
                       PktTimeOut(sd))) {
    FAIL("RetrieveObjects: message receive failed\n");
  }

  filtered = (ObjectSet)malloc(replySize);

  if(filtered == NULL) {
    FAIL("RetrieveObjects: out of memory\n");
  }

  objectsDescriptor.repetitions = replySize;

  if(!RecvData(sd,
               filtered,
               &objectsDescriptor,
               1,
               PktTimeOut(sd))) {
    free(filtered);
    FAIL("RetrieveObjects: data receive failed\n");
  }

  *retrieved = filtered;
  return(1);

}


int
Unregister(struct host_cookie *withWho,
           const char *filter) {

  DataDescriptor filterDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  size_t replySize;
  Socket sd;

  if(!ConnectToHost(withWho, &sd)) {
    FAIL("RetrieveFiltered: unable to contact name server.\n");
  }

  filterDescriptor.repetitions = strlen(filter) + 1;
  if(!SendMessageAndData(sd,
                         NS_UNREGISTER,
                         filter,
                         &filterDescriptor,
                         1,
                         PktTimeOut(sd))) {
    FAIL("RetrieveObjects: send failed\n");
  }
  else if(!RecvMessage(sd,
                       NS_UNREGISTERED,
                       &replySize,
                       PktTimeOut(sd))) {
    FAIL("RetrieveObjects: message receive failed\n");
  }

  return 1;

}
