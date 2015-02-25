/* $Id: dnsutil.c,v 1.13 2000/06/28 19:15:56 swany Exp $ */

#include <stdlib.h>     /* REALLOC() */
#include <string.h>     /* memcpy() strchr() strcasecmp() strncpy() */
#include <sys/types.h>  /* sometimes required for #include <sys/socket.h> */
#include <sys/socket.h> /* AF_INET */
#include <netinet/in.h> /* struct in_addr */
#include <arpa/inet.h>  /* inet_addr() inet_ntoa() */
#include <netdb.h>      /* {end,set}hostent() gethostby{addr,name}() */
#include "config.h"
#include "dnsutil.h"


int
gethostname();  /* Doesn't always seem to be in a system include file. */


/*
** NOTE: The man pages for {end,set}hostent() seem to imply that endhostent()
** needs/should only be called after sethostent(1).  Below, we call
** endhostent() after calling sethostent(0).  So far, this hasn't seemed to
** cause any problems, and it also appears to have squished a bug on some
** version of Unix where the O/S DNS cache was losing entries.
*/


/*
** We cache host entries locally to avoid going to the DNS too often.  This
** also gets around an old Solaris bug which leaks memory whenever dlopen is
** called (such as on the dynamic DNS lookup library).
*/
static unsigned int cacheCount = 0;
static struct hostent *cache = NULL;


/*
** Looks in the name and alias entries of #hostEntry# for a fully-qualified
** name.  Returns the fqn if found; otherwise, returns the name entry.
*/
static const char *
BestHostName(const struct hostent *hostEntry) {

  int i;

  if (!strchr(hostEntry->h_name, '.')) {
    for (i = 0; hostEntry->h_aliases[i] != NULL; i++) {
      if (strchr(hostEntry->h_aliases[i], '.'))
        return hostEntry->h_aliases[i]; /* found! */
    }
  }

  /* If we don't have a fully-qualified name, do the best we can.  */
  return hostEntry->h_name;

}


/*
** Appends a copy of #hostEntry# to the global map cache.  Returns a pointer to
** the copy, or NULL on error.
*/
static struct hostent *
CacheHostent(const struct hostent *hostEntry) {

  struct hostent *extendedCache;
  int i;
  unsigned int listLen;
  struct hostent newEntry;

  extendedCache = (struct hostent *)
    REALLOC(cache, sizeof(struct hostent) * (cacheCount + 1));
  if(extendedCache == NULL) {
    return NULL; /* Out of memory. */
  }
  cache = extendedCache;

  newEntry.h_name = strdup(hostEntry->h_name);
  if(newEntry.h_name == NULL) {
    return NULL; /* Out of memory. */
  }

  for(listLen = 0; hostEntry->h_aliases[listLen] != NULL; listLen++) {
    ; /* Nothing more to do. */
  }

  newEntry.h_aliases = (char **)malloc(sizeof(char *) * (listLen + 1));
  if(newEntry.h_aliases == NULL) {
    free(newEntry.h_name);
    return NULL; /* Out of memory. */
  }

  for(i = 0; i < listLen; i++) {
    newEntry.h_aliases[i] = strdup(hostEntry->h_aliases[i]);
    if(newEntry.h_aliases[i] == NULL) {
      for(i--; i >= 0; i--) {
        free(newEntry.h_aliases[i]);
      }
      free(newEntry.h_aliases);
      free(newEntry.h_name);
      return NULL; /* Out of memory. */
    }
  }
  newEntry.h_aliases[listLen] = NULL;

  newEntry.h_addrtype = hostEntry->h_addrtype;
  newEntry.h_length = hostEntry->h_length;

  for(listLen = 0; hostEntry->h_addr_list[listLen] != NULL; listLen++) {
    ; /* Nothing more to do. */
  }

  newEntry.h_addr_list = (char **)malloc(sizeof(char *) * (listLen + 1));
  if(newEntry.h_addr_list == NULL) {
    for(i = 0; newEntry.h_aliases[i] != NULL; i++) {
      free(newEntry.h_aliases[i]);
    }
    free(newEntry.h_aliases);
    free(newEntry.h_name);
    return NULL; /* Out of memory. */
  }

  for(i = 0; i < listLen; i++) {
    newEntry.h_addr_list[i] = (char *)malloc(newEntry.h_length);
    if(newEntry.h_addr_list[i] == NULL) {
      for(i--; i >= 0; i--) {
        free(newEntry.h_addr_list[i]);
      }
      free(newEntry.h_addr_list);
      for(i = 0; newEntry.h_aliases[i] != NULL; i++) {
        free(newEntry.h_aliases[i]);
      }
      free(newEntry.h_aliases);
      free(newEntry.h_name);
      return NULL; /* Out of memory. */
    }
    memcpy(newEntry.h_addr_list[i],
           hostEntry->h_addr_list[i],
           newEntry.h_length);
  }
  newEntry.h_addr_list[listLen] = NULL;

  cache[cacheCount] = newEntry;
  return &cache[cacheCount++];

}


/*
** Searches the DNS mapping cache for #address#, adding a new entry if needed.
** Returns a pointer to the mapping entry, or NULL on error.
*/
static const struct hostent*
LookupByAddress(IPAddress address) {

  struct in_addr addrAsInAddr;
  struct hostent *addrEntry;
  struct in_addr **cachedAddr;
  int i;

  for(i = 0; i < cacheCount; i++) {
    for(cachedAddr = (struct in_addr**)cache[i].h_addr_list;
        *cachedAddr != NULL;
        cachedAddr++) {
      if((**cachedAddr).s_addr == address) {
        return &cache[i];
      }
    }
  }

  addrAsInAddr.s_addr = address;
  sethostent(0);
  addrEntry =
    gethostbyaddr((char *)&addrAsInAddr, sizeof(addrAsInAddr), AF_INET);
  if(addrEntry == NULL) {
    endhostent();
    return NULL;
  }
  else if(addrEntry->h_length != sizeof(struct in_addr)) {
    endhostent();
    return NULL; /* We don't (yet) handle non-in_addr addresses. */
  }

  addrEntry = CacheHostent(addrEntry);
  endhostent();
  return addrEntry;

}


/*
** Searches the DNS mapping cache for #name#, adding a new entry if needed.
** Returns a pointer to the mapping entry, or NULL on error.
*/
static const struct hostent*
LookupByName(const char *name) {

  char **cachedName;
  char **extendedAliases;
  struct hostent *nameEntry;
  int i;
  int listLen;

  for(i = 0; i < cacheCount; i++) {
    if(strcasecmp(name, cache[i].h_name) == 0) {
      return &cache[i];
    }
    for(cachedName = cache[i].h_aliases; *cachedName != NULL; cachedName++) {
      if(strcasecmp(*cachedName, name) == 0) {
        return &cache[i];
      }
    }
  }

  sethostent(0);
  nameEntry = gethostbyname(name);
  if(nameEntry == NULL) {
    endhostent();
    return NULL;
  }
  else if(nameEntry->h_length != sizeof(struct in_addr)) {
    endhostent();
    return NULL; /* We don't (yet) handle non-in_addr addresses. */
  }

  /* We extend cached entries' h_aliases lists to include nicknames. */
  for(i = 0; i < cacheCount; i++) {
    if(strcmp(nameEntry->h_name, cache[i].h_name) == 0) {
      for(listLen = 0; cache[i].h_aliases[listLen] != NULL; listLen++) {
        ; /* Nothing more to do. */
      }
      extendedAliases =
        (char **)REALLOC(cache[i].h_aliases, sizeof(char **) * (listLen + 2));
      if(extendedAliases != NULL) {
        extendedAliases[listLen] = strdup(name);
        extendedAliases[listLen + 1] = NULL;
        cache[i].h_aliases = extendedAliases;
      }
      endhostent();
      return &cache[i];
    }
  }

  nameEntry = CacheHostent(nameEntry);
  endhostent();
  return nameEntry;

}


const char *
IPAddressImage(IPAddress addr) {
  struct in_addr addrAsInAddr;
  addrAsInAddr.s_addr = addr;
  return inet_ntoa(addrAsInAddr);
}


const char *
IPAddressMachine(IPAddress addr) {
  const struct hostent *hostEntry;
  static char returnValue[63 + 1];
  hostEntry = LookupByAddress(addr);
  strncpy(returnValue,
          (hostEntry == NULL) ? "" : BestHostName(hostEntry),
          sizeof(returnValue));
  return returnValue;
}


int
IPAddressValues(const char *machineOrAddress,
                IPAddress *addressList,
                unsigned int atMost) {

  const struct hostent *hostEntry;
  int i;
  int itsAnAddress;

  /*
  ** inet_addr() has the weird behavior of returning an unsigned quantity but
  ** using -1 as an error value.  Furthermore, the value returned is sometimes
  ** int and sometimes long, complicating the test.  Once inet_aton() is more
  ** widely available, we should switch to using it instead.
  */
  itsAnAddress = (inet_addr(machineOrAddress) ^ -1) != 0;

  if(itsAnAddress && (atMost == 1)) {
    *addressList = inet_addr(machineOrAddress);
    return 1;
  }

  hostEntry = itsAnAddress ?
              LookupByAddress(inet_addr(machineOrAddress)) :
              LookupByName(machineOrAddress);
  if(hostEntry == NULL) {
    return 0;
  }
  else if(atMost == 0) {
    return 1;
  }

  for(i = 0; i < atMost; i++) {
    if(hostEntry->h_addr_list[i] == NULL) {
      break;
    }
    addressList[i] = ((struct in_addr **)hostEntry->h_addr_list)[i]->s_addr;
  }

  return i;

}


const char *
MyMachineName(void) {

  const struct hostent *myEntry;
  static char returnValue[100] = "";

  /* If we have a value in returnValue, then we've already done the work. */
  if(returnValue[0] == '\0') {
    /* try the simple case first */
    if(gethostname(returnValue, sizeof(returnValue)) == -1) {
      return 0;
    }
    if(!strchr(returnValue, '.')) {
      /* Okay, that didn't work; take what we can get from the DNS. */
      myEntry = LookupByName(returnValue);
      if(myEntry == NULL) {
        return NULL;
      }
      strncpy(returnValue, BestHostName(myEntry), sizeof(returnValue));
      returnValue[sizeof(returnValue) - 1] = '\0';
    }
  }

  return returnValue;

}
