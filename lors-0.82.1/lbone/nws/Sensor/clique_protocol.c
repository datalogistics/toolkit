/* $Id: clique_protocol.c,v 1.126 2001/02/24 14:27:48 swany Exp $ */

#define FORKIT

#include "config.h"
#include <sys/types.h>       /* required for ntohl() on Cray */
#include <math.h>            /* fabs() */
#include <signal.h>          /* kill() */
#include <stdio.h>           /* sprintf() */
#include <stdlib.h>          /* atoi(), free(), {m,re}alloc() */
#include <string.h>          /* string functions */
#include <netinet/in.h>      /* htonl() ntohl() */
#include "cliques.h"         /* clique messages and structures */
#include "diagnostic.h"      /* FAIL() WARN() LOG() */
#include "dnsutil.h"         /* IP/DNS conversion */
#include "exp_protocol.h"    /* experiment structure manipulation */
#include "host_protocol.h"   /* host connection protocol */
#include "messages.h"        /* RegisterListener() */
#include "osutil.h"          /* CurrentTime() */
#include "register.h"        /* FreeObject() */
#include "skills.h"          /* skill invocation */
#include "strutil.h"         /* GETTOK() SAFESTRNCPY(), vstrncpy() */
#include "clique_protocol.h" /* spec for this code */


#define SUPPORTED_SKILLS_COUNT 2
static const KnownSkills SUPPORTED_SKILLS[SUPPORTED_SKILLS_COUNT] = 
  {tcpConnectMonitor, tcpMessageMonitor};


#define DEFAULT_CLIQUE_PERIOD "120"
#define KEEP_A_LONG_TIME 315360000.0
/* inital factor to multiple period by for clique t/o */
#define TIME_OUT_MULTIPLIER 2.5
#define GAIN 0.15


/*
** TokenInfo caches information about the token for a clique.  The #clique#
** field holds the clique itself; this is what is passed between sensors.  The
** #state# field indicates whether the token has been stopped (TOKEN_STOPPED,
** e.g. via a CLIQUE_DIE method -- we retain the token so that, if a fellow
** clique member passes us a stale token, we know not to restart the clique),
** is presently in our possession (TOKEN_HELD), or has been passed onto a
** fellow clique member (TOKEN_PASSED).  If we are forking child processes to
** do experiments, then #delegatePid# holds the pid of the child process.
** #myIndex# holds the index of this host's address in the clique host table;
** #lastHostTried# the index of the clique member we most recently tried to
** contact.  #expectedCycleTime# and #expectedCycleDeviation# hold,
** respectively, the time we think it will take for the token to pass around
** all members and the amount of error we expect this value to have.
** #cycleStartTime# holds the time when, the last time we had the token, we
** tried to contact our first fellow member.  This is used to compute the cycle
** time when we again receive the token.  #nextRunTime# holds the time we wish
** to begin the next cycle.  For the member that starts the cycle (the leader),
** this is the time the previous cycle began plus the clique period; for all
** others, it's the previous cycle start plus the clique time-out (which is
** always >= than the period).  #series# is a tab-delimited list of series that
** we've registered because of this clique.
*/
enum TokenState {TOKEN_HELD, TOKEN_PASSED, TOKEN_STOPPED};

typedef struct {
  Clique clique;
  enum TokenState state;
  pid_t delegatePid;
  unsigned int myIndex;
  unsigned int lastHostTried;
  double expectedCycleTime;
  double expectedCycleDeviation;
  double cycleStartTime;
  double nextRunTime;
  char *series;
} TokenInfo;


/* Module globals */
static TokenInfo *cliqueTokens = NULL;
static int cliquesKnown = 0;
static struct host_desc memoryDesc = {"", 0};
static char *memoryReg = NULL;


/* Prototypes required so we can sort local functions for easy lookup. */
static int
FindMe(const Clique *cliqueD,
       unsigned int *indx);
static int
FindTokenInfo(const char *cliqueName,
              unsigned int *indx);
static int
RecvClique(Socket sd,
           Clique *whereTo,
           double timeOut);
static void
RegisterClique(const Clique *cliqueD,
               unsigned int myIndex);
static void
RegisterSeries(const char *cliqueName,
               Object toRegister);
static int
SendClique(Socket sd,
           MessageType message,
           const Clique *cliqueD,
           double timeOut);
static int
SendToken(int message,
          const Clique *cliqueD,
          unsigned int myIndex,
          int broadcast);
static void
UnregisterClique(TokenInfo *info,
                 unsigned int myIndex);


/*
** NOTE: We carry host addresses in Cliques (Clique.host[n].address) in network
** format so that we can use the dnsutil functions on them.  This is a bit
** dirty, since we shouldn't be aware here of details of IPAddress as defined
** in dnsutil.h, and since it requires us to undo the host/network translation
** done by the message send/receive routines.  However, it seems the least
** ugly of an unfortunate set of choices, and we isolate the dirt by
** encapsulating the translation in {Recv,Send}Clique().
*/


/*
** Handles the recovery of the clique token #info# from a child process that
** was delegated to conduct the experiment.
*/
static void
ActivateToken(TokenInfo *info) {
  /*
  ** If delegatePid is already zero, then we already recovered this token, so
  ** no action is needed.  Otherwise, zero the pid and set up for an immediate
  ** experiment with the next host.
  */
  if(info->delegatePid != 0) {
    info->delegatePid = 0;
    if(info->state == TOKEN_HELD) {
      info->nextRunTime = CurrentTime();
    }
  }
}


/*
** Does the processing required to take control of the token for #info# in
** response to the token being passed to us or a time-out.  The processing in
** the case of time-out is somewhat different from that of a token pass, so
** #timeOut# indicates which case is being handled.
*/
static void
AdoptToken(TokenInfo *info,
           int timeOut) {

  Clique *clique = &info->clique;
  double err;
  double rtt;
  double now;

  if(timeOut) {
    /* Assume leadership and schedule work immediately. */
    clique->leader = info->myIndex;
    clique->instance += 1.0;  /* Bump so clique is newer than others' copies. */
    clique->timeOut *= 2.0;
    info->nextRunTime = CurrentTime();
  }
  else if((clique->leader == info->myIndex) && (info->cycleStartTime != 0)) {
    /* Round-trip done.  Update the token time-out, then hold it. */
    now = CurrentTime();
    rtt = now - info->cycleStartTime;
    /*
    ** TBD: the code below is crippled forecasting code.  It should be replaced
    ** by an invocation of the forecasting library code.
    */
    err = fabs(rtt - info->expectedCycleTime);
    info->expectedCycleTime =
      info->expectedCycleTime + GAIN * (rtt - info->expectedCycleTime);
    info->expectedCycleDeviation =
      fabs(info->expectedCycleDeviation + GAIN *
           (info->expectedCycleDeviation - err));
    clique->timeOut = clique->period + info->expectedCycleTime +
      2.0 * info->expectedCycleDeviation;
    info->nextRunTime = info->cycleStartTime + clique->period;
  }
  else {
    /*
    ** Token in transit; schedule work immediately unless the token has come
    ** around very quickly.  The too-quick case is most likely when some other
    ** host times out and spawns a new token.
    */
    now = CurrentTime();
    info->nextRunTime = (now < info->cycleStartTime + clique->period) ?
                        (info->cycleStartTime + clique->period) : now;
  }

  /* Try to keep the time-out within a reasonable range. */
  if( (clique->timeOut <= (clique->period + 1.0)) ||
      (clique->timeOut > (clique->period * TIME_OUT_MULTIPLIER)) ) {
    clique->timeOut = TIME_OUT_MULTIPLIER * clique->period;
  }

  info->state = TOKEN_HELD;
  info->delegatePid = 0;
  info->lastHostTried = info->myIndex;

}


/*
** A "local" function of HandleCliqueMessage().  Processes the CLIQUE_ACTIVATE
** message for clique #cliqueD#.  Returns 1 if successful, else 0.
*/
static int
DoCliqueActivate(const Clique *cliqueD) {
  unsigned int cliqueIndex;
  if(!FindTokenInfo(cliqueD->name, &cliqueIndex)) {
    FAIL1("ActivateClique: unknown clique %s\n", cliqueD->name);
  }
  ActivateToken(&cliqueTokens[cliqueIndex]);
  return 1;
}


/*
** A "local" function of HandleCliqueMessage().  Handles the #type# message
** which arrived on #sd# accompanied by #recvdClique#.  Returns 1 if
** successful, else 0.
*/
static int
DoTokenRecv(Socket sd,
            Clique *recvdClique,
            int type) {

  TokenInfo *info;
  unsigned int myIndex;
  unsigned int tokenIndex;

  if((type != CLIQUE_DIE) && 
     !SendClique(sd, CLIQUE_TOKEN_ACK, recvdClique, PktTimeOut(sd))) {
    WARN("DoTokenRecv: ack failed\n");
  }

  if(FindTokenInfo(recvdClique->name, &tokenIndex)) {

    info = &cliqueTokens[tokenIndex];

    /* Check the received clique for validity. */
    if( (info->clique.whenGenerated > recvdClique->whenGenerated) ||
        ( (info->clique.whenGenerated == recvdClique->whenGenerated) &&
          (info->clique.instance > recvdClique->instance) ) ||
        ( (info->clique.whenGenerated == recvdClique->whenGenerated) &&
          (info->clique.instance == recvdClique->instance) &&
          (info->clique.leader > recvdClique->leader) ) ) {
      FAIL5("DoTokenRecv: stale %s token; %f:%f vs. %f:%f\n",
            recvdClique->name,recvdClique->whenGenerated,recvdClique->instance,
            info->clique.whenGenerated, info->clique.instance);
    }
    else if( (info->clique.whenGenerated != recvdClique->whenGenerated) ||
             (info->clique.instance != recvdClique->instance) ||
             (info->clique.leader != recvdClique->leader) ) {
      UnregisterClique(info, info->myIndex);
      info->clique = *recvdClique;  /* Incorporate any updates. */
      if(FindMe(recvdClique, &myIndex)) {
        info->myIndex = myIndex;
        RegisterClique(recvdClique, myIndex);
      }
      else {
        type = CLIQUE_DIE;
      }
    }

  }
  else {

    if((type == CLIQUE_DIE) || !FindMe(recvdClique, &myIndex)) {
      return 1;  /* TBD Preemptive kill?  Maybe we should keep the token? */
    }

    cliquesKnown++;
    cliqueTokens = REALLOC(cliqueTokens, sizeof(TokenInfo) * cliquesKnown);
    info = &cliqueTokens[cliquesKnown - 1];
    info->clique = *recvdClique;
    info->state = TOKEN_HELD;
    info->delegatePid = 0;
    info->myIndex = myIndex;
    info->lastHostTried = myIndex;
    info->expectedCycleTime = recvdClique->period;
    info->expectedCycleDeviation = 0.0;
    info->cycleStartTime = 0;
    info->nextRunTime = 0.0;
    info->series = strdup("");
    RegisterClique(recvdClique, myIndex);

  }

  if(type == CLIQUE_DIE) {
    LOG1("DoTokenRecv: stopping clique %s\n", recvdClique->name);
    UnregisterClique(info, info->myIndex);
    info->state = TOKEN_STOPPED;
  }
  else {
    AdoptToken(info, 0);
  }

  return 1;

}


/*
** Searches the host address table of #cliqueD# for an entry corresponding to
** an address and port this sensor listens to.  If found, returns 1 and sets
** #indx# to the table index; else returns 0.
*/
static int
FindMe(const Clique *cliqueD,
       unsigned int *indx) {
  int i;
  for(i = 0; i < cliqueD->count; i++) {
    if(EstablishedInterface(cliqueD->members[i].address,
                            cliqueD->members[i].port)) {
      *indx = i;
      return 1;
    }
  }
  FAIL1("FindMe: not found in clique %s\n", cliqueD->name);
}


/*
** Searches the cliqueTokens array for a clique named #cliqueName#.  Returns 1
** and sets #indx# to the matching array index if found, else returns 0.
*/
static int
FindTokenInfo(const char *cliqueName,
              unsigned int *indx) {
  unsigned int i;
  for(i = 0; i < cliquesKnown; i++) {
    if(strcmp(cliqueName, cliqueTokens[i].clique.name) == 0) {
      *indx = i;
      return 1;
    }
  }
  return 0;
}


/*
** Serves as the listener for clique-specific messages.
*/
static void
HandleCliqueMessage(Socket *sd,
                    MessageType type,
                    size_t size) {

  char *data;
  DataDescriptor descriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  Clique recvdClique;

  switch(type) {

  case CLIQUE_ACTIVATE:
  case CLIQUE_DIE:
  case CLIQUE_TOKEN_FWD:

    if(!RecvClique(*sd,
                   &recvdClique,
                   PktTimeOut(*sd))) {
      DROP_SOCKET(sd);
      ERROR("HandleCliqueMessage: receive failed\n");
      return;
    }

    switch(type) {
    case CLIQUE_ACTIVATE:
      (void)DoCliqueActivate(&recvdClique);
      break;
    case CLIQUE_DIE:
    case CLIQUE_TOKEN_FWD:
      (void)DoTokenRecv(*sd, &recvdClique, type);
      break;
    }
    break;

  case CLIQUE_SERIES:
    data = (char *)malloc(size);
    if(data == NULL) {
      ERROR("HandleCliqueMessage: out of memory\n");
    }
    else {
      descriptor.repetitions = size;
      if(RecvData(*sd, data, &descriptor, 1, PktTimeOut(*sd))) {
        RegisterSeries(data, data + strlen(data) + 1);
      }
      else {
        ERROR("HandleCliqueMessage: receive failed\n");
      }
      free(data);
    }
    break;

  default:
    ERROR("HandleCliqueMessage: unrecognized command\n");
    break;

  }

  if(type != CLIQUE_SERIES)
    DROP_SOCKET(sd);

}


/*
** Receives a Clique struct from #sd# into #whereTo# and converts all the host
** addresses in it to network format (see NOTE at top of module).  Returns 1 if
** successful within #timeOut# seconds, else 0.
*/
static int
RecvClique(Socket sd,
           Clique *whereTo,
           double timeOut) {
  int i;
  if(!RecvData(sd,
               whereTo,
               cliqueDescriptor,
               cliqueDescriptorLength,
               timeOut)) {
    FAIL("RecvClique: data receive failed\n");
  }
  for(i = 0; i < whereTo->count; i++) {
    whereTo->members[i].address = htonl(whereTo->members[i].address);
  }
  return 1;
}


/*
** Registers #cliqueD# with the name server.  #myIndex# is the index of this
** host in #cliqueD#'s host list.
*/
static void
RegisterClique(const Clique *cliqueD,
               unsigned int myIndex) {

  Activity activity;
  int i;
  struct host_desc member;
  char option[255 + 1];
  int resourceCount;
  KnownSkills skill = SUPPORTED_SKILLS[0];
  const MeasuredResources *skillResources;
  Object toRegister;

  /* Only the clique leader un/registers the clique. */
  if(cliqueD->leader != myIndex)
    return;

  SAFESTRCPY(activity.controlName, CLIQUE_CONTROL_NAME);
  SAFESTRCPY(activity.host, EstablishedRegistration());
  SAFESTRCPY(activity.resources, "");
  SkillResources(skill, cliqueD->options, &skillResources, &resourceCount);
  for(i = 0; i < resourceCount; i++) {
    if(i > 0) {
      strcat(activity.resources, "\t");
    }
    strcat(activity.resources, ResourceName(skillResources[i]));
  }
  SAFESTRCPY(activity.skillName, cliqueD->skill);
  option[0] = '\0';
  if(cliqueD->options[0] == '\0') {
    sprintf(option, "period:%d\tmember:", (int)cliqueD->period);
  }
  else {
    sprintf(option, "%s\tperiod:%d\tmember:",
            cliqueD->options, (int)cliqueD->period);
  }
  activity.options = strdup(option);
  for(i = 0; i < cliqueD->count; i++) {
    HostDValue(IPAddressImage(cliqueD->members[i].address),
               cliqueD->members[i].port,
               &member);
    vstrncpy(option, sizeof(option), 2,
             (i > 0) ? "," : "", NameOfHost(&member));
    activity.options = (char *)
      REALLOC(activity.options, strlen(activity.options) + strlen(option) + 1);
    strcat(activity.options, option);
  }
  toRegister = ObjectFromActivity(cliqueD->name, &activity);
  RegisterObject(toRegister);
  FreeObject(&toRegister);
  free(activity.options);

}


/*
** Registers #toRegister#, which designates a series generated by the clique
** #cliqueName#, and adds the series to the series field of #cliqueName#'s
** info entry.
*/
static void
RegisterSeries(const char *cliqueName,
               Object toRegister) {

 unsigned int cliqueIndex;
 TokenInfo *info;
 const char *seriesName;

  if(!FindTokenInfo(cliqueName, &cliqueIndex)) {
    ERROR1("RegisterSeries: unable to locate clique %s\n", cliqueName);
    return;
  }
  info = &cliqueTokens[cliqueIndex];
  seriesName = AttributeValue(FindAttribute(toRegister, "name"));
  info->series =
    REALLOC(info->series, strlen(info->series) + strlen(seriesName) + 2);
  if(*info->series != '\0')
    strcat(info->series, "\t");
  strcat(info->series, seriesName);
  RegisterObject(toRegister);

}


/*
** A "local" function of RunCliqueJob().  Performs the experiment specified by
** #info# to the #destIndex#th member and updates #info# to reflect the new
** state.  Returns 1 if successful, else 0.
*/
static int
RunCliqueExperiment(TokenInfo *info,
                    unsigned int destIndex) {

  Socket childToParent;
  Clique *clique;
  Experiment expToStore;
  int i;
  struct host_cookie memCookie;
  char options[63 + 1];
  DataDescriptor nameDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  DataDescriptor objectDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  const SkillResult *results;
  int resultsLength;
  Series series;
  const char *seriesName;
  KnownSkills skill = SUPPORTED_SKILLS[0];
  unsigned int timeOut;
  Object toRegister;

  clique = &info->clique;
  for(i = 0; i < SUPPORTED_SKILLS_COUNT; i++) {
    if(strcmp(clique->skill, SkillName(SUPPORTED_SKILLS[i])) == 0) {
      skill = SUPPORTED_SKILLS[i];
      break;
    }
  }

  if(i == SUPPORTED_SKILLS_COUNT) {
    FAIL2("RunCliqueExperiment: Unsupported skill %s found in %s\n",
          clique->skill, clique->name);
  }

#ifdef FORKIT
  if(!CreateLocalChild(&info->delegatePid, NULL, &childToParent)) {
    FAIL1("RunCliqueExperiment: fork for clique %s failed\n", clique->name);
  }

  if(info->delegatePid > 0) {
    /* Parent process. */
    return 1;
  }

  /* Child process. */
#endif

  timeOut = (skill == tcpConnectMonitor) ?
            ((unsigned int)(clique->period / clique->count) - 1) :
            ConnTimeOut(clique->members[destIndex].address);
  /*
  ** TBD HACK ALERT: The intermachine skills expect a "target" option that
  ** designates the experiment target.  Since clique experiment targets are
  ** inherited from the membership, the API code (see related HACK ALERT in
  ** nws_api.c) suppresses this option when lauching clique activities.  Here
  ** we construct a target option value from the current target peer.  NOTE: we
  ** could do multiple targets (i.e., handle all clique peers in a single fork)
  ** by giving the skill module a comma-delimited list here as the value of the
  ** target option.
  */
  sprintf(options, "%s\ttarget:%s:%ld",
          clique->options,
          IPAddressMachine(clique->members[destIndex].address),
          clique->members[destIndex].port);
  UseSkill(skill, options, timeOut, &results, &resultsLength);
  /* Store resource measurements. */
  MakeHostCookie(memoryDesc.host_name, memoryDesc.port, &memCookie);
  expToStore.timeStamp = CurrentTime();
  for(i = 0; i < resultsLength; i++) {
    if(results[i].succeeded || (skill == tcpConnectMonitor)) {
      expToStore.value =
        results[i].succeeded ? results[i].measurement : (timeOut * 10000.0);
      SAFESTRCPY(series.host, EstablishedRegistration());
      SAFESTRCPY(series.label, ResourceLabel(results[i].resource));
      SAFESTRCPY(series.memory, memoryReg);
      SAFESTRCPY(series.options, results[i].options);
      SAFESTRCPY(series.resource, ResourceName(results[i].resource));
      seriesName = NameOfSeries(&series);
      if(StoreExperiments(&memCookie,
                          seriesName,
                          &expToStore,
                          1,
                          KEEP_A_LONG_TIME) &&
         (strstr(info->series, seriesName) == 0)) {
        toRegister = ObjectFromSeries(seriesName, &series);
#ifdef FORKIT
        /* Send the registration request back to the parent process. */
        nameDescriptor.repetitions = strlen(clique->name) + 1;
        objectDescriptor.repetitions = strlen(toRegister) + 1;
        SendMessageAndDatas(childToParent,
                            CLIQUE_SERIES,
                            clique->name,
                            &nameDescriptor,
                            1,
                            toRegister,
                            &objectDescriptor,
                            1,
                            MINPKTTIMEOUT);
#else
        RegisterSeries(clique->name, toRegister);
#endif
        FreeObject(&toRegister);
      }
    }
    else {
      WARN2("RunCliqueExperiment: %s failed to %s\n",
            ResourceName(results[i].resource),
            IPAddressMachine(clique->members[destIndex].address));
    }
  }
  DisconnectHost(&memCookie);

#ifdef FORKIT
  /* Send reactivate message. */
  (void)SendClique(childToParent, CLIQUE_ACTIVATE, clique, MINPKTTIMEOUT);
  DROP_SOCKET(&childToParent);
  exit(results[0].succeeded);
#endif

  return results[0].succeeded;

}


/*
** A "local" function of CliqueWork().  Performs one experiment indicated by
** #info#, or passes the clique token on if all members in the clique have
** already been contacted.  Updates #info# to reflect the new token status.
** Returns 1 if successful, else 0.
*/
static int
RunCliqueJob(TokenInfo *info) {

  Clique *clique;
  double now;

  now = CurrentTime();
  clique = &info->clique;

  if(now < info->nextRunTime) {
    FAIL("RunCliqueJob: premature call\n");
  }

  if(info->state != TOKEN_HELD) {
    /*
    ** Time out: Kill the clique and assume leadership and the token.  Since
    ** we're taking over leadership, we need to re-register the clique.
    */
    WARN2("RunCliqueJob: clique %s timed out after %d secs\n",
          clique->name, (int)clique->timeOut);
#ifdef KILL_TOKEN
    /*
    ** NOTE: Sending a CLIQUE_DIE to other clique members here would avoid
    ** having multiple active tokens for a single clique.  If the clique period
    ** is insufficient to cover the hosts, however, it might be useful to
    ** maintain multiple tokens running simultaneously.
    */
    (void)SendToken(CLIQUE_DIE, clique, info->myIndex, 1);
#endif
    UnregisterClique(info, info->myIndex);
    AdoptToken(info, 1);
    RegisterClique(clique, info->myIndex);
  }
  else if(info->delegatePid != 0) {
    /*
    ** Either the clique has timed out (despite a generous allowance of time)
    ** before the child completed the experiment, or the child has died
    ** undetected.  The latter case is no problem, since we're now recovering
    ** the token.  In the former case, we will schedule a new experiment with a
    ** new child, whose pid we will store in delegatePid.  If the first child
    ** then completes, we'll go ahead and schedule *another* experiment, even
    ** though the second is still active.  To avoid this, we try to kill off
    ** the existing delegate before proceeding.
    */
    WARN2("RunCliqueJob: wresting clique %s away from child %d\n",
          clique->name, info->delegatePid);
    (void)kill(info->delegatePid, SIGKILL);
    info->delegatePid = 0;
  }

  if(info->lastHostTried == info->myIndex) {
    /* Record the time we try our first contact for later time-out calc. */
    info->cycleStartTime = CurrentTime();
  }

  for(info->lastHostTried = (info->lastHostTried + 1) % clique->count;
      info->lastHostTried != info->myIndex;
      info->lastHostTried = (info->lastHostTried + 1) % clique->count) {
    if(RunCliqueExperiment(info,
                           info->lastHostTried)) {
      if(info->delegatePid > 0) {
        /*
        ** Give the child some time to finish the experiment.  This has the
        ** affect of serializing the experiments of a particular clique.  Note,
        ** however, that multiple experiments will still run simultaneously if
        ** multiple cliques happen to be scheduled at the same time.
        */
        now = CurrentTime();
        info->nextRunTime = now + clique->period;
      }
      break;
    }
  }

  if(info->lastHostTried == info->myIndex) {
    /* We've been through all members and need to pass the token. */
    if(SendToken(CLIQUE_TOKEN_FWD, clique, info->myIndex, 0)) {
      info->state = TOKEN_PASSED;
      info->nextRunTime = info->cycleStartTime + clique->timeOut;
    }
    else {
      /* Set up for time-out in a single period. */
      info->nextRunTime = info->cycleStartTime + clique->period;
      WARN1("RunCliqueJob: forward failed; holding token for clique %s\n",
            clique->name);
    }
  }

  return 1;

}


/*
** Sends #message# accompanied by #cliqueD# on #sd# after first converting all
** the host addresses to host format (see note at top of module).  Returns 1 if
** successful within #timeOut# seconds, else 0.
*/
static int
SendClique(Socket sd,
           MessageType message,
           const Clique *cliqueD,
           double timeOut) {

  Clique hostVersion;
  int i;

  if(htonl(1) != 1) {
    /* Translation needed. */
    hostVersion = *cliqueD;
    for(i = 0; i < cliqueD->count; i++) {
      hostVersion.members[i].address = ntohl(hostVersion.members[i].address);
    }
    cliqueD = &hostVersion;
  }

  return SendMessageAndData(sd,
                            message,
                            cliqueD,
                            cliqueDescriptor,
                            cliqueDescriptorLength,
                            timeOut);

}


/*
** A "local" function of RunCliqueJob().  Attempts to send #message#,
** accompanied by #cliqueD#, to each participating host in turn except the
** #myIndex#'th one (assumed to be this sensor).  If #broadcast# is false,
** returns 1 as soon as any contact succeeds or 0 of all fail; otherwise,
** returns 1 no matter how many contacts succeed.
*/
static int
SendToken(int message,
          const Clique *cliqueD,
          unsigned int myIndex,
          int broadcast) {

  IPAddress hostAddr;
  size_t ignored;
  unsigned int nextIndex;
  Socket sd;

  for(nextIndex = (myIndex + 1) % cliqueD->count;
      nextIndex != myIndex;
      nextIndex = (nextIndex + 1) % cliqueD->count) {

    hostAddr = cliqueD->members[nextIndex].address;

    if(!CallAddr(hostAddr,
                 cliqueD->members[nextIndex].port,
                 &sd,
                 MINCONNTIMEOUT)) {
      WARN1("SendToken: connect to host %s failed\n",
            IPAddressMachine(hostAddr));
      continue;
    }
    else if(!SendClique(sd, message, cliqueD, PktTimeOut(sd))) {
      WARN1("SendToken: send to host %s failed\n", IPAddressMachine(hostAddr));
    }
    else {
      if((message == CLIQUE_TOKEN_FWD) &&
         !RecvMessage(sd, CLIQUE_TOKEN_ACK, &ignored, PktTimeOut(sd))) {
        WARN1("SendToken: no ack received from %s\n",
              IPAddressMachine(hostAddr));
      }
      if(!broadcast) {
        DROP_SOCKET(&sd);
        return 1;
      }
    }

    DROP_SOCKET(&sd);

  }

  return !broadcast;

}


/*
** Unregisters all objects generated by #info# with the name server.
** #myIndex# is the index of this host in #info#s host list.
*/
static void
UnregisterClique(TokenInfo *info,
                 unsigned int myIndex) {
  const char *c;
  char seriesName[255 + 1];
  /* Only the clique leader un/registers the clique. */
  if(info->clique.leader == myIndex) {
    UnregisterObject(info->clique.name);
  }
  /* We don't want to unregister the series for what could be
     a temporary fault.  The nws_nameserver will time these records
     out eventually.
  */
  free(info->series);
  info->series = strdup("");
}


void
CliqueChildDeath(int pid) {
  int i;
  for(i = 1; i < cliquesKnown; i++) {
    if(cliqueTokens[i].delegatePid == pid) {
      ActivateToken(&cliqueTokens[i]);
    }
  }
}


int
CliqueInit(const char *memoryName,
           const struct host_desc *memory) {

  Control control;
  int i;
  Object toRegister;

  /* Cache memory for later use. */
  memoryDesc = *memory;
  memoryReg  = strdup(memoryName);

  /* Register listeners for all clique messages. */
  RegisterListener(CLIQUE_ACTIVATE, "CLIQUE_ACTIVATE", HandleCliqueMessage);
  RegisterListener(CLIQUE_DIE, "CLIQUE_DIE", HandleCliqueMessage);
  RegisterListener(CLIQUE_SERIES, "CLIQUE_SERIES", HandleCliqueMessage);
  RegisterListener(CLIQUE_TOKEN_FWD, "CLIQUE_TOKEN_FWD", HandleCliqueMessage);

  /* Register the clique control. */
  SAFESTRCPY(control.controlName, CLIQUE_CONTROL_NAME);
  SAFESTRCPY(control.host, EstablishedRegistration());
  SAFESTRCPY(control.skills, SkillName(SUPPORTED_SKILLS[0]));
  for(i = 1; i < SUPPORTED_SKILLS_COUNT; i++) {
    strcat(control.skills, "\t");
    strcat(control.skills, SkillName(SUPPORTED_SKILLS[i]));
  }
  sprintf
    (control.options, "member:2_to_%d_sensor\tperiod:0_to_1_int", MAX_MEMBERS);
  toRegister = ObjectFromControl(NameOfControl(&control), &control);
  RegisterObject(toRegister);
  FreeObject(&toRegister);

  return 1;

}


void
CliqueWork(void) {
  int i;
  double now;
  for(i = 0; i < cliquesKnown; i++) {
    if(cliqueTokens[i].state != TOKEN_STOPPED) {
      now = CurrentTime();
      if(now >= cliqueTokens[i].nextRunTime) {
        (void)RunCliqueJob(&cliqueTokens[i]);
      }
    }
  }
}


double
NextCliqueWork(void) {
  int i;
  double next = 0.0;
  for(i = 0; i < cliquesKnown; i++) {
    if((cliqueTokens[i].state != TOKEN_STOPPED) &&
       ((next == 0.0) || (cliqueTokens[i].nextRunTime < next))) {
      next = cliqueTokens[i].nextRunTime;
    }
  }
  return next;
}


int
RecognizedCliqueActivity(const char *name) {
  unsigned int ignored;
  return FindTokenInfo(name, &ignored);
}


int
StartCliqueActivity(const char *registration,
                    const char *skill,
                    const char *options) {

  Clique *clique;
  struct host_desc host;
  IPAddress hostAddress;
  char hostName[MAX_HOST_NAME];
  int i;
  TokenInfo *info;
  const char *member;
  KnownSkills supported = SUPPORTED_SKILLS[0];
  unsigned int tokenIndex;

  /* Figure out what skill we're being asked to exercise. */
  for(i = 0; i < SUPPORTED_SKILLS_COUNT; i++) {
    if(strcmp(skill, SkillName(SUPPORTED_SKILLS[i])) == 0) {
      supported = SUPPORTED_SKILLS[i];
      break;
    }
  }

  if(i == SUPPORTED_SKILLS_COUNT) {
    FAIL1("StartCliqueActivity: unsupported skill %s\n", skill);
  }

  /* Find out if this is a new or previously-stopped clique. */
  if(FindTokenInfo(registration, &tokenIndex)) {
    info = &cliqueTokens[tokenIndex];
    if(info->state != TOKEN_STOPPED) {
      FAIL1("StartCliqueActivity: %s is already running\n", registration);
    }
    /* Restarting, so prior cycleStartTime has no meaning. */
    info->cycleStartTime = 0;
  }
  else {
    cliquesKnown++;
    cliqueTokens = REALLOC(cliqueTokens, sizeof(TokenInfo) * cliquesKnown);
    info = &cliqueTokens[cliquesKnown - 1];
    info->expectedCycleTime = atoi(DEFAULT_CLIQUE_PERIOD);
    info->expectedCycleDeviation = 0.0;
    info->cycleStartTime = 0;
    info->series = strdup("");
  }

  clique = &info->clique;
  memset(clique, 0, sizeof(Clique));  /* Make purify happy. */
  SAFESTRCPY(clique->name, registration);
  clique->whenGenerated = CurrentTime();
  clique->instance = 0;
  SAFESTRCPY(clique->skill, skill);
  SkillOptions(supported, options, clique->options);
  clique->period =
    atoi(GetOptionValue(options, "period", DEFAULT_CLIQUE_PERIOD));
  clique->timeOut = clique->period * TIME_OUT_MULTIPLIER;
  clique->count = 0;
  clique->leader = 0;

  /* Convert the member roster from strings to address/port pairs. */
  for(member = GetOptionValue(options, "member", "");
      GETTOK(hostName, member, ",", &member);
      ) {
    HostDValue(hostName, DefaultHostPort(SENSOR_HOST), &host);
    if(IPAddressValue(host.host_name, &hostAddress) &&
       (*IPAddressMachine(hostAddress) != '\0')) {
      clique->members[clique->count].address = hostAddress;
      clique->members[clique->count].port = host.port;
      clique->count++;
    }
    else {
      WARN1("StartCliqueActivity: conversion of %s failed\n", hostName);
    }
  }

  /* If I'm in the clique, start work; otherwise, consider it stopped. */
  if(FindMe(clique, &info->myIndex)) {
    AdoptToken(info, 0);
    RegisterClique(clique, info->myIndex);
  }
  else {
    info->state = TOKEN_STOPPED;
  }

  return 1;

}


int
StopCliqueActivity(const char *registration) {

  unsigned int tokenIndex;

  if(registration[0] == '\0') {
    /*
    ** NOTE: We're being told to halt all clique activity.  It's a bit unclear
    ** what we should do in this case.  The most drastic course would be to
    ** halt all cliques of which we are a member.  A kinder, gentler approach
    ** would be to spawn new tokens for all our cliques with our membership
    ** deleted.  Instead, we take the simplest course and simply stop
    ** conducting experiments ourselves, while allowing other clique members
    ** to continue contacting us.  In practice, this may not be the right thing
    ** to do, since the only occasion (presently) when this function is called
    ** with a blank parameter is when the sensor is about to exit. In contrast,
    ** if we're given a clique name we really do shut it down (see below).
    */
    for(tokenIndex = 0; tokenIndex < cliquesKnown; tokenIndex++) {
      cliqueTokens[tokenIndex].state = TOKEN_STOPPED;
      UnregisterClique(&cliqueTokens[tokenIndex],
                       cliqueTokens[tokenIndex].myIndex);
    }
    return 1;
  }

  if(!FindTokenInfo(registration, &tokenIndex)) {
    FAIL1("StopCliqueActivity: unknown clique %s", registration);
  }

  cliqueTokens[tokenIndex].state = TOKEN_STOPPED;
  (void)SendToken(CLIQUE_DIE,
                  &cliqueTokens[tokenIndex].clique,
                  cliqueTokens[tokenIndex].myIndex,
                  1);
  UnregisterClique(&cliqueTokens[tokenIndex],
                   cliqueTokens[tokenIndex].myIndex);
  return 1;

}
