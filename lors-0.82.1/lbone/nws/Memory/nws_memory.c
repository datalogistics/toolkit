/* $Id: nws_memory.c,v 1.94 2001/01/15 17:21:04 rich Exp $ */

#include "config.h"
#include <errno.h>          /* errno */
#include <stdio.h>          /* sprintf() */
#include <sys/types.h>      /* size_t */
#include <sys/stat.h>       /* struct stat */
#include <unistd.h>         /* getopt() stat() */
#include <signal.h>         /* signal() */
#include <stdlib.h>         /* atoi() free() malloc() */
#include <string.h>         /* strchr() strstr() */
#include <dirent.h>         /* {close,open,read}dir() */
#include "protocol.h"       /* Socket utilities */
#include "nws_state.h"      /* State store/retrieve services */
#include "host_protocol.h"  /* Host control functions */
#include "diagnostic.h"     /* FAIL() WARN() */
#include "strutil.h"        /* SAFESTRCPY() vstrncpy() */
#include "dnsutil.h"        /* MyMachineName() */
#include "osutil.h"         /* CurrentTime() GetEnvironmentValue() */
#include "messages.h"       /* Recv*() and Send*() */
#include "nws_memory.h"     /* Memory-specific messages */

#if defined(ENABLE_CACHE)
#include "nws_cache.h"	    /* writes through an fbuff cache */
#endif


/*
** This program implements the NWS memory host.  See nws_memory.h for supported
** messages.
*/


#define JOURNAL "journal"
#define KEEP_A_LONG_TIME 315360000.0


/*
** Information about a registered auto-fetch request.  The tab-delimited list
** of state names of interest and a connection to the client who wants to be
** notified.
*/
typedef struct {
  char *stateNames;
  Socket clientSock;
} AutoFetchInfo;


/*
** Module globals.  #autoFetches# is the list of registered auto-fetch requests
** that we're still servicing; #autoFetchCount# is the length of this list.
** #fileSize# contains the maximum number of records allowed in files,
** #journalPath# is the path to a file in which the memory keeps a record of
** all writes; and #memoryDir# the directory where this memory stores files.
*/
AutoFetchInfo *autoFetches = NULL;
static size_t autoFetchCount = 0;
static size_t fileSize;
static size_t journalFileSize;
static char journalPath[255 + 1];
static char memoryDir[255 + 1];
static struct loglocation memLogLocation;


/*
** Called when an operation fails.  Cleans up any bad connections and, if we've
** run out, frees up a connection so that others may connect.
*/
static void
CheckConnections(void) {
  if(CloseDisconnections() == 0) {
    CloseConnections(0, 1, 1);
  }
}


/*
** A "local" function of ProcessRequest().  Implements the MEMORY_CLEAN service
** by deleting all files in the memory directory that have not been accessed
** within the past #idle# seconds.  Returns 1 if successful, else 0.
*/
static int
DoClean(unsigned long idle) {

  struct dirent *entry;
  DIR *directory;
  time_t expiration;
  char filePath[255 + 1];
  struct stat fileStat;
  char *namePlace;

  directory = opendir(memoryDir);
  if(directory == NULL) {
    FAIL1("DoClean: unable to open directory %s\n", memoryDir);
  }

  expiration = (time_t)CurrentTime() - (time_t)idle;
  SAFESTRCPY(filePath, memoryDir);
  namePlace = filePath + strlen(filePath);

  while((entry = readdir(directory)) != NULL) {
    strcpy(namePlace, entry->d_name);
    if(stat(filePath, &fileStat) != 0) {
      WARN1("DoClean: unable to state file %s\n", filePath);
    }
    else if(fileStat.st_mtime < expiration) {
      LOG2("DoClean: deleting %s, last modified %d\n",
           filePath, fileStat.st_mtime);
      (void)unlink(filePath);
    }
  }

  (void)closedir(directory);
  return(1);

}


/*
** Removes from the #auotFetchInfo# module variable all information for the
** client connected to #sock#.
*/
static void
EndAutoFetch(Socket sock) {
  int i;
  for(i = 0; i < autoFetchCount; i++) {
    if(autoFetches[i].clientSock == sock) {
      free(autoFetches[i].stateNames);
      autoFetches[i] = autoFetches[--autoFetchCount];
      return;
    }
  }
}


/*
** A "local" function of KeepState().  Enters #name# marked with #seqNo# into
** the state file (global) #journalPath#.  Used to keep a log of all writes
** performed by the memory.  Returns 1 if successful, else 0.
*/
static int
EnterInJournal(const char *name,
               double seqNo) {

  char journalRec[MAX_RECORD_SIZE];
  double now;

  memset(journalRec, 0, sizeof(journalRec));
  now = CurrentTime();

  if(sprintf(journalRec,"%10.0f %64s", seqNo, name) < 2) {
    FAIL1("EnterInJournal: write failed, errno %d\n", errno);
  }

  if(!WriteState(journalPath,
                 journalFileSize,
                 KEEP_A_LONG_TIME,
                 now,
                 journalRec,
                 strlen(journalRec))) {
    FAIL("EnterInJournal: write state failed\n");
  }

  return(1);

}


/*
** Constructs and returns a file path suitable for storing records for a state
** named #stateName# in the directory #dir#.
*/
const char *
FileOfState(const char *dir,
            const char *stateName) {
  char *c;
  size_t dirLen = strlen(dir);
  static char returnValue[255 + 1];
  vstrncpy(returnValue, sizeof(returnValue), 3,
           dir, (dir[dirLen - 1] == '/') ? "" : "/", stateName);
  /* Translate characters in the name that are special to the file system. */
  for(c = returnValue + dirLen; *c != '\0'; c++) {
    if(*c == '/')
      *c = '_';
  }
  return returnValue;
}


/*
** A "local" function of ProcessRequest() .  Stores #data# in #directory# with
** the attributes indicated by #s#.  Fails if the record size and count in #s#
** yield more than #max_size# total bytes.  Returns 1 if successful, else 0.
*/
static int
KeepState(const struct loglocation* logLoc,
          const struct state *s,
          const char *data,
          size_t max_size) {

  const char *curr;
  int i;
  size_t recordSize;

  if(s->rec_count > fileSize) {
    FAIL("KeepState: rec count too big\n");
  }

  if(s->rec_size > MAX_RECORD_SIZE) {
    WARN("KeepState: state record too big.\n");
    recordSize = MAX_RECORD_SIZE;
  }
  else {
    recordSize = (size_t)s->rec_size;
  }

  if(s->rec_count * recordSize > max_size) {
    FAIL1("KeepState: too much data %d\n", s->rec_count * recordSize);
  }

#ifdef WITH_NETLOGGER
  if (logLoc->loc_type==MEMORY_LOG_NETLOGGER)
  {
          for(curr = data, i = 0; i < s->rec_count; curr += recordSize, i++) {
                  if(!WriteStateNL(logLoc->path,
                          s->id,
                          fileSize,
                          s->time_out,
                          s->seq_no,
                          curr,
                          recordSize) ||
                          !EnterInJournal(s->id, s->seq_no)) {
                          if(errno == EMFILE) {
                                  CheckConnections();
                          }
                          FAIL("KeepState: write failed\n");
                  }
          }
          return (1);
  }
#endif /* WITH_NETLOGGER */

  for(curr = data, i = 0; i < s->rec_count; curr += recordSize, i++) {
    if(!WriteState(FileOfState(logLoc->path, s->id),
                   fileSize,
                   s->time_out,
                   s->seq_no,
                   curr,
                   recordSize) ||
       !EnterInJournal(s->id, s->seq_no)) {
      if(errno == EMFILE) {
        CheckConnections();
      }
      FAIL("KeepState: write failed\n");
    }
  }

  return(1);

}


/*
** A "local" function of main().  Handles a #messageType# message arrived on
** #sd# accompanied by #dataSize# bytes of data.
*/
static void
ProcessRequest(Socket *sd,
               MessageType messageType,
               size_t dataSize) {

  char *contents;
  DataDescriptor contentsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  AutoFetchInfo *expandedAutoFetches;
  unsigned long expiration;
  DataDescriptor expirationDescriptor = SIMPLE_DATA(UNSIGNED_LONG_TYPE, 1);
  int i;
  struct state stateDesc;
  char *stateNames;
  DataDescriptor stateNamesDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);

  switch(messageType) {

  case FETCH_STATE:
    if(!RecvData(*sd,
                 &stateDesc,
                 stateDescriptor,
                 stateDescriptorLength,
                 PktTimeOut(*sd))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: state receive failed\n");
      return;
    }
    contents = (char *)malloc(stateDesc.rec_count * MAX_RECORD_SIZE);
    if(contents == NULL) {
      (void)SendMessage(*sd, MEMORY_FAILED, PktTimeOut(*sd));
      ERROR("ProcessRequest: out of memory\n");
    }
    else {
      if(ReadState(FileOfState(memoryDir, stateDesc.id),
                   contents,
                   stateDesc.rec_count,
                   stateDesc.rec_count * MAX_RECORD_SIZE,
                   stateDesc.seq_no,
                   &stateDesc.time_out,
                   &stateDesc.seq_no,
                   &stateDesc.rec_count,
                   &stateDesc.rec_size)) {
        if(stateDesc.rec_count > 0) {
          contentsDescriptor.repetitions =
            stateDesc.rec_size * stateDesc.rec_count;
          (void)SendMessageAndDatas(*sd,
                                    STATE_FETCHED,
                                    &stateDesc,
                                    stateDescriptor,
                                    stateDescriptorLength,
                                    contents,
                                    &contentsDescriptor,
                                    1,
                                    PktTimeOut(*sd));
        }
        else {
          (void)SendMessageAndData(*sd,
                                   STATE_FETCHED,
                                   &stateDesc,
                                   stateDescriptor,
                                   stateDescriptorLength,
                                   PktTimeOut(*sd));
        }
      }
      else {
        (void)SendMessage(*sd, MEMORY_FAILED, PktTimeOut(*sd));
        if(errno == EMFILE) {
          CheckConnections();
        }
      }
      free(contents);
    }
    break;

  case STORE_STATE:
    if(!RecvData(*sd,
                 &stateDesc,
                 stateDescriptor,
                 stateDescriptorLength,
                 PktTimeOut(*sd))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: state receive failed\n");
      return;
    }
    contentsDescriptor.repetitions = stateDesc.rec_size * stateDesc.rec_count;
    contents = (char *)malloc(contentsDescriptor.repetitions + 1);
    if(contents == NULL) {
      (void)SendMessage(*sd, MEMORY_FAILED, PktTimeOut(*sd));
      ERROR("ProcessRequest: out of memory\n");
    }
    else {
      contents[contentsDescriptor.repetitions] = '\0';
      if(!RecvData(*sd,
                   contents,
                   &contentsDescriptor,
                   1,
                   PktTimeOut(*sd))) {
        DROP_SOCKET(sd);
        ERROR("ProcessRequest: data receive failed\n");
      }
      else {
        (void)SendMessage(*sd,
                          KeepState(&memLogLocation,
                                    &stateDesc,
                                    contents,
                                    contentsDescriptor.repetitions) ?
                          STATE_STORED : MEMORY_FAILED,
                          PktTimeOut(*sd));
        for(i = 0; i < autoFetchCount; i++) {
          if(strstr(autoFetches[i].stateNames, stateDesc.id) != NULL) {
            if(!SendMessageAndDatas(autoFetches[i].clientSock,
                                    STATE_FETCHED,
                                    &stateDesc,
                                    stateDescriptor,
                                    stateDescriptorLength,
                                    contents,
                                    &contentsDescriptor,
                                    1,
                                    PktTimeOut(autoFetches[i].clientSock))) {
              DROP_SOCKET(&autoFetches[i].clientSock);
              free(autoFetches[i].stateNames);
              autoFetches[i] = autoFetches[--autoFetchCount];
            }
          }
        }
      }
      free(contents);
    }
    break;

  case AUTOFETCH_BEGIN:
    stateNamesDescriptor.repetitions = dataSize;
    stateNames = (char *)malloc(dataSize);
    if(stateNames == NULL) {
      (void)SendMessage(*sd, MEMORY_FAILED, PktTimeOut(*sd));
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: out of memory\n");
    }
    else if(!RecvData(*sd,
                      stateNames,
                      &stateNamesDescriptor,
                      1,
                      PktTimeOut(*sd))) {
      (void)SendMessage(*sd, MEMORY_FAILED, PktTimeOut(*sd));
      DROP_SOCKET(sd);
      free(stateNames);
      ERROR("ProcessRequest: data receive failed\n");
    }
    else if(*stateNames == '\0') {
      free(stateNames);
      EndAutoFetch(*sd);
      (void)SendMessage(*sd, AUTOFETCH_ACK, PktTimeOut(*sd));
    }
    else {
      for(i=0; (i < autoFetchCount) && (autoFetches[i].clientSock != *sd); i++)
        ; /* Nothing more to do. */
      if(i == autoFetchCount) {
        expandedAutoFetches =
          REALLOC(autoFetches, (autoFetchCount + 1) * sizeof(AutoFetchInfo));
        if(expandedAutoFetches == NULL) {
          (void)SendMessage(*sd, MEMORY_FAILED, PktTimeOut(*sd));
          DROP_SOCKET(sd);
          ERROR("ProcessRequest: out of memory\n");
          break;
        }
        autoFetches = expandedAutoFetches;
        autoFetches[i].clientSock = *sd;
        autoFetchCount++;
      }
      else {
        free(autoFetches[i].stateNames);
      }
      autoFetches[i].stateNames = stateNames;
      (void)SendMessage(*sd, AUTOFETCH_ACK, PktTimeOut(*sd));
    }
    break;

  case MEMORY_CLEAN:
    if(!RecvData(*sd, &expiration, &expirationDescriptor, 1, PktTimeOut(*sd))) {
      DROP_SOCKET(sd);
      ERROR("ProcessRequest: data receive failed\n");
    }
    else {
      (void)SendMessage(*sd, MEMORY_CLEANED, PktTimeOut(*sd));
      (void)DoClean(expiration);
    }
    break;

#ifdef WITH_NETLOGGER	  
  case MEMORY_LOGDEST: /* config message contains log location */
	  if(!RecvData(*sd,
		  &memLogLocation,
		  loglocationDescriptor,
		  loglocationDescriptorLength,
		  PktTimeOut(*sd))) {
		  DROP_SOCKET(sd);
		  ERROR("ProcessRequest: loglocation receive failed\n");
		  return;
	  }else
	  {
		  (void)SendMessage(*sd, MEMORY_LOGDEST_ACK, PktTimeOut(*sd));
	  }
	  LOG2("ProcessRequest: loglocation %d .%s.\n", memLogLocation.loc_type, memLogLocation.path);
 
	  break;
#endif /* WITH_NETLOGGER */	  	

  default:
    DROP_SOCKET(sd);
    ERROR1("ProcessRequest: unknown message %d\n", messageType);

  }

}


#define DEFAULT_MEMORY_DIR "."
#define DEFAULT_MEMORY_SIZE "2000"
#define DEFAULT_JOURNAL_SIZE "2000"
#define MAX_ADDRESSES 20
#if defined(ENABLE_CACHE)
#define SWITCHES "C:a:Dd:e:l:N:Pp:s:j:"
#else
#define SWITCHES "a:Dd:e:l:N:Pp:s:j:"
#endif

int debug;

int
main(int argc,
     char *argv[]) {

  char address[MAX_MACHINE_NAME + 1];
  IPAddress addresses[MAX_ADDRESSES];
  unsigned int addressesCount;
  char addressList[255 + 1];
  const char *c;
  char errorFile[127 + 1];
  char logFile[127 + 1];
  struct host_desc memoryDesc;
  double nextBeatTime;
  struct host_desc nsDesc;
  double now;
  int opt;
  extern char *optarg;
  char password[127 + 1];
  const char *USAGE =
    "nws_memory [-D] [-a name] [-d dir] [-el file] [-N host] [-p port] [-s size]";

  /* Set up default values that will be overwritten by command line args. */
  addressList[0] = '\0';
  errorFile[0] = '\0';
  fileSize = atoi(GetEnvironmentValue("MEMORY_SIZE",
                                      "~", ".nwsrc", DEFAULT_MEMORY_SIZE));
  journalFileSize = atoi(GetEnvironmentValue("JOURNAL_SIZE",
                                      "~", ".nwsrc", DEFAULT_JOURNAL_SIZE));
  logFile[0] = '\0';
  HostDValue(MyMachineName(), DefaultHostPort(MEMORY_HOST), &memoryDesc);
  SAFESTRCPY(memoryDir,
             GetEnvironmentValue("MEMORY_DIR", "~", ".nwsrc",
                                 DEFAULT_MEMORY_DIR));;
  SAFESTRCPY(nsDesc.host_name,
             GetEnvironmentValue("NAME_SERVER", "~", ".nwsrc", "noname"));
  HostDValue(nsDesc.host_name, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
  password[0] = '\0';
  memLogLocation.loc_type = MEMORY_LOG_LOCAL;
  debug = 0;

  while((int)(opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'a':
      SAFESTRCPY(addressList, optarg);
      break;

    case 'D':
      debug = 1;
      break;

    case 'd':
      SAFESTRCPY(memoryDir, optarg);
      break;

    case 'e':
      SAFESTRCPY(errorFile, optarg);
      break;

    case 'l':
      SAFESTRCPY(logFile, optarg);
      break;

    case 'N':
      HostDValue(optarg, DefaultHostPort(NAME_SERVER_HOST), &nsDesc);
      break;

    case 'p':
      memoryDesc.port = atoi(optarg);
      break;

    case 'P':
      fprintf(stdout, "Password? ");
      fscanf(stdin, "%s", password);
      break;

    case 's':
      fileSize = atoi(optarg);
      break;

    case 'j':
      journalFileSize = atoi(optarg);
      break;
#if defined(ENABLE_CACHE)
    case 'C':
      RC_entries = atoi(optarg);
      break;
#endif
    default:
      fprintf(stderr, "nws_memory: unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }
#if defined(ENABLE_CACHE)
  /*
   * WARNING: these two had better be the same or the cache and backing
   * store could be inconsistent
   */
  RCMemorySize = fileSize;
  /*
   * make sure entries value is sane
   */
  if(RC_entries <= 0)
	  RC_entries = 0;
#endif

  if (debug) {
    DirectDiagnostics(DIAGINFO, stdout);
    DirectDiagnostics(DIAGLOG, stdout);
    DirectDiagnostics(DIAGWARN, stderr);
    DirectDiagnostics(DIAGERROR, stderr);
    DirectDiagnostics(DIAGFATAL, stderr);
  }

  for(addressesCount = 0, c = addressList;
      GETTOK(address, c, ",", &c);
      addressesCount++) {
    if(!IPAddressValue(address, &addresses[addressesCount])) {
      ABORT1("Unable to convert '%s' into an IP address\n", address);
    }
  }
  addressesCount += IPAddressValues(memoryDesc.host_name,
                                    &addresses[addressesCount],
                                    MAX_ADDRESSES - addressesCount);

  if(memoryDir[strlen(memoryDir) - 1] != '/') {
    strcat(memoryDir, "/");
  }
  if(!MakeDirectory(memoryDir, 0775, 1)) {
    ABORT1("Unable to establish %s as state directory\n", memoryDir);
  }
  SAFESTRCPY(memLogLocation.path,memoryDir);

  if(!EstablishHost(NameOfHost(&memoryDesc),
                    MEMORY_HOST,
                    addresses,
                    addressesCount,
                    memoryDesc.port,
                    errorFile,
                    logFile,
                    password,
                    &nsDesc,
                    NULL)) {
    exit(1);
  }

  vstrncpy(journalPath, sizeof(journalPath), 4,
           memoryDir, EstablishedRegistration(), ".", JOURNAL);

  fclose(stdin);
  signal(SIGPIPE, SocketFailure);
  RegisterListener(STORE_STATE, "STORE_STATE", &ProcessRequest);
  RegisterListener(FETCH_STATE, "FETCH_STATE", &ProcessRequest);
  RegisterListener(AUTOFETCH_BEGIN, "AUTOFETCH_BEGIN", &ProcessRequest);
  RegisterListener(MEMORY_CLEAN, "MEMORY_CLEAN", &ProcessRequest);
#ifdef WITH_NETLOGGER
  RegisterListener(MEMORY_LOGDEST, "MEMORY_LOGDEST", &ProcessRequest);
#endif
  NotifyOnDisconnection(&EndAutoFetch);
  nextBeatTime = CurrentTime();

  /* main service loop */
  while(1) {
    now = CurrentTime();
    if(now >= nextBeatTime) {
      RegisterHost(DEFAULT_HOST_BEAT * 2);
      nextBeatTime =
        now + (HostHealthy() ? DEFAULT_HOST_BEAT : SHORT_HOST_BEAT);
    }
    ListenForMessages(nextBeatTime - now);
  }

  /* return(0); Never reached */

}
