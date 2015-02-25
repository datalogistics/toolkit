/* $Id: nws_nameserver.c,v 1.25 2000/09/26 03:29:32 swany Exp $ */

#include "config.h"
#include <sys/types.h>      /* size_t */
#include <signal.h>         /* signal() */
#include <stdio.h>          /* File access */
#include <stdlib.h>         /* atoi() atol() free() malloc() */
#include <string.h>         /* string utilities */
#include <unistd.h>         /* getopt() ftruncate() */
#include "diagnostic.h"     /* ABORT() ERROR() */
#include "dnsutil.h"        /* MyMachineName() */
#include "host_protocol.h"  /* Host control functions */
#include "messages.h"       /* Recv*() and Send*() */
#include "osutil.h"         /* CurrentTime() */
#include "strutil.h"        /* GETWORD() SAFESTRCPY() */
#include "nws_nameserver.h" /* Nameserver-specific messages */


/*
** This program implements the NWS name server host.  See nws_nameserver.h for
** supported messages.
*/


#define ETERNAL 0
#define EXPIRATION_FORMAT "%10ld"
#define EXPIRED 1


/*
** Module globals.  #registrationFile# is the file where the name server writes
** its registrations.  Each (newline-terminated) record of this file consists
** of an expiration time followed by a tab-delimited list of attribute:value
** pairs.
*/
static FILE *registrationFile;


/*
** Reads the current line of the registration file and returns it with the
** trailing newline stripped.  Returns NULL on EOF.
*/
static const char *
ReadRegistration() {

  size_t registrationLen;

  static char *longRegistration = NULL;
  static char registration[511 + 1];

  if(fgets(registration, sizeof(registration), registrationFile) == NULL) {
    return NULL;
  }
  else {
    registrationLen = strlen(registration);
    if(registration[registrationLen - 1] == '\n') {
      registration[registrationLen - 1] = '\0';
      return registration;
    }
    else {
      if(longRegistration != NULL) {
        free(longRegistration);
      }
      longRegistration = strdup(registration);
      while(fgets(registration,
                  sizeof(registration),
                  registrationFile) != NULL) {
        registrationLen += strlen(registration);
        longRegistration = REALLOC(longRegistration, registrationLen + 1);
        strcat(longRegistration, registration);
        if(longRegistration[registrationLen - 1] == '\n') {
          longRegistration[registrationLen - 1] = '\0';
          break;
        }
      }
      return longRegistration;
    }
  }
}


/*
** A "local" function of MatchFilter().  Returns 1 or 0 depending on whether or
** not #item# contains the first attribute specified by #filter#.  Updates
** #filter# to point to the next attribute.
*/
static int
MatchFilterAndUpdate(const char *item,
                     const char **filter) {

  char firstChar;
  const char *itemAttribute;
  char operator;
  int returnValue;
  char simpleName[127 + 1];
  char simplePattern[127 + 1];
  const char *value;
  const char *valueEnd;
  size_t valueLen;

  /* Allow leading and trailing spaces for legibility. */
  while(**filter == ' ')
    (*filter)++;
  firstChar = **filter;

  if(firstChar == '(') {
    /* Nested pattern. */
    (*filter)++;
    returnValue = MatchFilterAndUpdate(item, filter);
    if(**filter == ')')
      (*filter)++;
  }
  else if((firstChar == '!') || (firstChar == '|') || (firstChar == '&')) {
    /* Prefix operator: unary not, n-ary or, n-ary and */
    (*filter)++;
    if(firstChar == '!') {
      returnValue = !MatchFilterAndUpdate(item, filter);
    }
    else {
      returnValue = firstChar == '&';
      /* To advance filter correctly we must avoid short-circuit matches. */
      while((**filter != '\0') && (**filter != ')')) {
        if(MatchFilterAndUpdate(item, filter)) {
          if(firstChar == '|')
            returnValue = 1;
        }
        else if(firstChar == '&')
          returnValue = 0;
      }
    }
  }
  else {
    /* Grab the individual parts of the <attribute><op><pattern> pair. */
    if(!GETTOK(simpleName, *filter, "<=>~", filter))
      FAIL1("MatchFilterAndUpdate: parse error in %s\n", filter);
    strcat(simpleName, ":");
    operator = **filter;
    if(operator != '=') {
      (*filter)++; /* Skip < > or ~ */
      if(**filter != '=')
        FAIL1("MatchFilterAndUpdate: parse error in %s\n", filter);
    }
    (*filter)++; /* Skip '=' */
    if(!GETTOK(simplePattern, *filter, ") ", filter))
      FAIL1("MatchFilterAndUpdate: parse error in %s\n", filter);
    /* Now match against the value of each occurence of <attribute> in item. */
    for(itemAttribute = strstr(item, simpleName); itemAttribute != NULL;
        itemAttribute = strstr(itemAttribute + 1, simpleName)) {
      value = itemAttribute + strlen(simpleName);
      valueEnd = strchr(value, '\t');
      valueLen = (valueEnd == NULL) ? strlen(value) : (valueEnd - value);
      if((operator == '=') && strnmatch(value, simplePattern, valueLen))
        break;
      else if((operator == '<') &&
              (strncmp(value, simplePattern, valueLen) <= 0))
        break;
      else if((operator == '>') &&
              (strncmp(value, simplePattern, valueLen) >= 0))
        break;
      else if((operator == '~') && strnmatch(value, simplePattern, valueLen))
        /* We recognize ~= but treat it as simple = */
        break;
    }
    returnValue = itemAttribute != NULL;
  }

  while(**filter == ' ')
    (*filter)++;
  return returnValue;

}


/*
** Returns 1 or 0 depending on whether or not #item# has all the attributes
** specified by #filter#.
*/
static int
MatchFilter(const char *item,
            const char *filter) {
  return MatchFilterAndUpdate(item, &filter);
}


/*
** A "local" function of main().  Deletes from the registration file all
** objects that have an expiration time which has already passed.
*/
static void
CompressRegistrations(void) {

  size_t bytesCompressed = 0;
  unsigned long expiration;
  char expirationImage[15 + 1];
  const char *ignored;
  unsigned long now;
  size_t recordLen;
  const char *registration;

  now = (unsigned long)CurrentTime();

  for(rewind(registrationFile); (registration = ReadRegistration()) != NULL;) {
    (void)GETWORD(expirationImage, registration, &ignored);
    expiration = atol(expirationImage);
    recordLen = strlen(registration) + 1;
    if((expiration != ETERNAL) && (expiration <= now)) {
      bytesCompressed += recordLen;
    }
    else if(bytesCompressed > 0) {
      /* Move this record forward to the end of the still-current objects. */
      fseek(registrationFile, -(recordLen + bytesCompressed), SEEK_CUR);
      fprintf(registrationFile, "%s\n", registration);
      fseek(registrationFile, bytesCompressed, SEEK_CUR);
    }
  }

  /* Truncate any records that we've moved. */
  if(bytesCompressed > 0) {
    fseek(registrationFile, -bytesCompressed, SEEK_CUR);
    ftruncate(fileno(registrationFile), ftell(registrationFile));
  }

}


/*
** A "local" function of ProcessRequest().  Searches through the registration
** file for unexpired objects that match #filter#.  Returns a malloc'ed string
** of the matching objects.
*/
static char *
DoSearch(const char *filter) {

  unsigned long expiration;
  char expirationImage[15 + 1];
  unsigned long now;
  const char *object;
  const char *registration;
  char *returnValue;

  now = (unsigned long)CurrentTime();
  returnValue = strdup("");

  for(rewind(registrationFile); (registration = ReadRegistration()) != NULL;) {
    (void)GETWORD(expirationImage, registration, &object);
    expiration = atol(expirationImage);
    if((expiration == ETERNAL) || (expiration > now)) {
      object++; /* Skip space after expiration. */
      if(MatchFilter(object, filter)) {
        returnValue =
          REALLOC(returnValue, strlen(returnValue) + strlen(object) + 1);
        strcat(returnValue, object);
      }
    }
  }

  return returnValue;

}


/*
** A "local" function of ProcessRequest().  Adds an entry for #object# in the
** registration file indicating that it expires at #expiration#.
*/
static void
DoRegister(const char *object,
           unsigned long expiration) {

  char expirationImage[15 + 1];
  const char *existing;
  const char *registration;

  /*
  ** Overwrite any existing expiration.  This minimizes the size of the
  ** registration file and simplifies searches.
  */
  for(rewind(registrationFile); (registration = ReadRegistration()) != NULL;) {
    (void)GETWORD(expirationImage, registration, &existing);
    existing++; /* Skip space after expiration. */
    if(strcmp(object, existing) == 0) {
      fseek(registrationFile, -strlen(registration) - 1, SEEK_CUR);
      fprintf(registrationFile, EXPIRATION_FORMAT, expiration);
      return;
    }
  }

  fprintf(registrationFile, EXPIRATION_FORMAT, expiration);
  fprintf(registrationFile, " %s\n", object);

}

           
/*
** A "local" function of ProcessRequest().  Searches through the registration
** file for objects that match #filter# and marks them as expired.
*/
static void
DoUnregister(const char *filter) {

  unsigned long expiration;
  char expirationImage[15 + 1];
  unsigned long now;
  const char *object;
  const char *registration;

  now = (unsigned long)CurrentTime();
  for(rewind(registrationFile); (registration = ReadRegistration()) != NULL;) {
    (void)GETWORD(expirationImage, registration, &object);
    expiration = atol(expirationImage);
    if((expiration == ETERNAL) || (expiration > now)) {
      object++; /* Skip space after expiration. */
      if(MatchFilter(object, filter)) {
        fseek(registrationFile, -strlen(registration) - 1, SEEK_CUR);
        fprintf(registrationFile, EXPIRATION_FORMAT, (long)EXPIRED);
        fseek(registrationFile, strlen(registration) + 1, SEEK_CUR);
      }
    }
  }

}


/*
** A "local" function of main().  Handles exit call-backs from the host
** protocol message-handling routine.
*/
static int
NSExit(void) {
  fclose(registrationFile);
  return 1;
}


/*
** A "local" function of main().  Handles a #messageType# message arrived on
** #sd# accompanied by #dataSize# bytes of data.
*/
static void
ProcessRequest(Socket *sd,
               MessageType messageType,
               size_t dataSize) {

  unsigned long timeOut;
  DataDescriptor timeOutDescriptor = SIMPLE_DATA(UNSIGNED_LONG_TYPE, 1);
  char *matching;
  char *object;
  DataDescriptor objectDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  char *pattern;
  DataDescriptor patternDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);

  switch(messageType) {

  case NS_REGISTER:
    objectDescriptor.repetitions =
      dataSize - HomogenousDataSize(UNSIGNED_LONG_TYPE, 1, NETWORK_FORMAT);
    object = (char *)malloc(objectDescriptor.repetitions + 1);
    if(object == NULL) {
      (void)SendMessage(*sd, NS_FAILED, PktTimeOut(*sd));
      ERROR("ProcessRequest: out of memory\n");
    }
    else {
      if(!RecvData(*sd,
                   object,
                   &objectDescriptor,
                   1,
                   PktTimeOut(*sd)) ||
         !RecvData(*sd,
                   &timeOut,
                   &timeOutDescriptor,
                   1,
                   PktTimeOut(*sd))) {
        DROP_SOCKET(sd);
        ERROR("ProcessRequest: receive failed\n");
      }
      else {
        object[objectDescriptor.repetitions] = '\0';
        (void)SendMessage(*sd, NS_REGISTERED, PktTimeOut(*sd));
        /* Change time-out period to an expiration time. */
        if(timeOut != 0) {
          timeOut += (unsigned long)CurrentTime();
        }
        DoRegister(object, timeOut);
      }
      free(object);
    }
    break;

  case NS_SEARCH:
  case NS_UNREGISTER:
    patternDescriptor.repetitions = dataSize;
    pattern = (char *)malloc(dataSize);
    if(pattern == NULL) {
      (void)SendMessage(*sd, NS_FAILED, PktTimeOut(*sd));
      ERROR("ProcessRequest: out of memory\n");
    }
    else {
      if(!RecvData(*sd,
                   pattern,
                   &patternDescriptor,
                   1,
                   PktTimeOut(*sd))) {
        DROP_SOCKET(sd);
        ERROR("ProcessRequest: receive failed\n");
      }
      else if(messageType == NS_SEARCH) {
        matching = DoSearch(pattern);
        if(matching == NULL) {
          (void)SendMessage(*sd, NS_FAILED, PktTimeOut(*sd));
          ERROR("ProcessRequest: out of memory\n");
        }
        else {
          objectDescriptor.repetitions = strlen(matching) + 1;
          (void)SendMessageAndData(*sd,
                                   NS_SEARCHED,
                                   matching,
                                   &objectDescriptor,
                                   1,
                                   PktTimeOut(*sd));
          free(matching);
        }
      }
      else {
        DoUnregister(pattern);
        (void)SendMessage(*sd, NS_UNREGISTERED, PktTimeOut(*sd));
      }
      free(pattern);
    }
    break;

  default:
    ERROR1("ProcessRequest: unknown message %d\n", messageType);

  }

}


#define DEFAULT_COMPRESSION_FREQUENCY 3600
#define DEFAULT_FILE "registrations"
#define MAX_ADDRESSES 20
#define SWITCHES "a:c:De:f:l:p:P"

int debug;

int
main(int argc,
     char *argv[]) {

  char address[MAX_MACHINE_NAME + 1];
  IPAddress addresses[MAX_ADDRESSES];
  unsigned int addressesCount;
  char addressList[255 + 1];
  const char *c;
  unsigned long compressionFreq;
  char errorFile[127 + 1];
  char logFile[127 + 1];
  unsigned long nextCompression;
  struct host_desc nsDesc;
  char password[127 + 1];
  int opt;
  extern char *optarg;
  char registrationFileName[255 + 1];
  const char *USAGE =
    "nws_nameserver [-D] [-a name] [-c seconds] [-efl file] [-p port]";

  /* Set up default values that will be overwritten by command line args. */
  addressList[0] = '\0';
  compressionFreq = DEFAULT_COMPRESSION_FREQUENCY;
  errorFile[0] = '\0';
  logFile[0] = '\0';
  HostDValue(MyMachineName(), DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
  SAFESTRCPY(registrationFileName, DEFAULT_FILE);
  password[0] = '\0';
  debug = 0;

  while((int)(opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'a':
      SAFESTRCPY(addressList, optarg);
      break;

    case 'c':
      compressionFreq = atol(optarg);
      break;

    case 'D':
      debug = 1;
      break;

    case 'e':
      SAFESTRCPY(errorFile, optarg);
      break;

    case 'f':
      SAFESTRCPY(registrationFileName, optarg);
      break;

    case 'l':
      SAFESTRCPY(logFile, optarg);
      break;

    case 'p':
      nsDesc.port = atoi(optarg);
      break;

    case 'P':
      fprintf(stdout, "Password? ");
      fscanf(stdin, "%s", password);
      break;

    default:
      fprintf(stderr, "nws_nameserver: unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if (debug) {
    DirectDiagnostics(DIAGINFO, stdout);
    DirectDiagnostics(DIAGLOG, stdout);
    DirectDiagnostics(DIAGWARN, stderr);
    DirectDiagnostics(DIAGERROR, stderr);
    DirectDiagnostics(DIAGFATAL, stderr);
  }

  addressesCount = IPAddressValues(nsDesc.host_name, addresses, MAX_ADDRESSES);
  for(c = addressList; GETTOK(address, c, ",", &c); ) {
    if(!IPAddressValue(address, &addresses[addressesCount++])) {
      ABORT1("Unable to convert '%s' into an IP address\n", address);
    }
  }

  registrationFile = fopen(registrationFileName, "r+");
  if(registrationFile == NULL) {
    registrationFile = fopen(registrationFileName, "w+");
  }
  
  if(registrationFile == NULL) {
    ABORT1("Unable to open %s for storing registrations\n",
           registrationFileName);
  }

  if(!EstablishHost(NameOfHost(&nsDesc),
                    NAME_SERVER_HOST,
                    addresses,
                    addressesCount,
                    nsDesc.port,
                    errorFile,
                    logFile,
                    password,
                    &nsDesc,
                    &NSExit)) {
    exit(1);
  }

  fclose(stdin);
  signal(SIGPIPE, SocketFailure);
  RegisterListener(NS_REGISTER, "NS_REGISTER", &ProcessRequest);
  RegisterListener(NS_SEARCH, "NS_SEARCH", &ProcessRequest);
  RegisterListener(NS_UNREGISTER, "NS_UNREGISTER", &ProcessRequest);
  nextCompression = CurrentTime() + compressionFreq;

  /* main service loop */
  while(1) {
    ListenForMessages(nextCompression - CurrentTime());
    if(CurrentTime() > nextCompression) {
      CompressRegistrations();
      nextCompression = CurrentTime() + compressionFreq;
    }
  }

  /* return 0; Never reached */

}
