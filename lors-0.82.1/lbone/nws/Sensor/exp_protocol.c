/* $Id: exp_protocol.c,v 1.60 2001/01/11 03:29:35 rich Exp $ */

#include "config.h"
#include <signal.h>        /* signal() */
#include <stdlib.h>        /* free() {m,re}alloc() strtod() */
#include <string.h>        /* memset() strlen() */
#include "diagnostic.h"    /* FAIL() WARN() */
#include "host_protocol.h" /* Host connection & registration routines */
#include "strutil.h"       /* GETTOK() GETWORD() SAFESTRCPY() vstrncpy() */
#include "messages.h"      /* message send/receive */
#include "osutil.h"        /* CurrentTime() */
#include "nws_memory.h"    /* Memory-specific messages */
#include "exp_protocol.h"  /* our prototypes */


#define ACTIVITY_CLASS "nwsActivity"
#define CONTROL_CLASS  "nwsControl"
#define SERIES_CLASS   "nwsSeries"
#define SKILL_CLASS    "nwsSkill"


#define ForEachAttribute(object,attr) \
  for(attr = NextAttribute(object, NO_ATTRIBUTE); \
      attr != NO_ATTRIBUTE; \
      attr = NextAttribute(object, attr))


/*
** Pulls each name:value pair out of #options# and adds it as an attribute of
** #toObject#.
*/
static void
AddOptionAttributes(Object *toObject,
                    const char *options) {

  const char *endOfName;
  const char *nextOption;
  char option[511 + 1];
  char optionName[63 + 1];

  for(nextOption = options;
      GETTOK(option, nextOption, "\t", &nextOption);
     ) {
    GETTOK(optionName, option, ":", &endOfName);
    AddAttribute(toObject, optionName, endOfName + 1);
  }

}


/*
** Returns the names from #list#, a tab-delimited list of attribute name/value
** pairs, as a comma-delimited list.
*/
static const char *
AttributeNames(const char *list) {

  static char returnValue[255 + 1];
  const char *from;
  char *to;

  for(from = list, to = returnValue; *from != '\0'; ) {
    /* Append a comma if we're not on the first name. */
    if(from != list) {
      *to++ = ',';
    }
    /* Copy chars up to the delimiter or name/value separator. */
    while((*from != '\t') && (*from != ':') && (*from != '\0'))
      *to++ = *from++;
    /* Skip the value and delimiter. */
    if(*from == ':') {
      for(from++; (*from != '\t') && (*from != '\0'); from++)
        ; /* Nothing more to do. */
    }
    if(*from == '\t')
      from++;
  }

  *to = '\0';
  return returnValue;

}


/*
** A "local" function of StoreExperiments().  Packs the #count#-long array of
** experiments #from# into #to#, returning the record size in #rec_size#.
*/
static int
PackExperiments(const Experiment *from,
                size_t count,
                char **to,
                size_t *rec_size) {

  char *curr;
  int i;
  char rec_buff[63 + 1];

  /* make a test record to see how big they will be */
  memset(rec_buff, 0, sizeof(rec_buff));
  sprintf(rec_buff, WEXP_FORMAT, from->timeStamp, from->value);
  *rec_size = strlen(rec_buff);

  *to = (char *)malloc(*rec_size * count + 1);  /* Allow for trailing '\0'. */
  if(*to == NULL) {
    FAIL("PackExperiments: malloc failed\n");
  }
  memset(*to, ' ', *rec_size * count);

  curr = *to;

  for(i = 0; i < count; i++) {
    memset(rec_buff, 0, sizeof(rec_buff));
    sprintf(curr, WEXP_FORMAT, from->timeStamp, from->value);
    from++;
    curr += *rec_size;
  }

  return(1);

}


/*
** A "local" function of LoadExperiments().  Extracts experiment information
** from #to# and stores it in the #max_count#-long array #from#.  Returns the
** number of experiments stored in #count#.
*/
static int
UnpackExperiments(const struct state *s,
                  const char *from,
                  size_t max_count,
                  Experiment *to,
                  size_t *count) {

  const char *curr;
  int i;
  char word[128];

  curr = from;

  for(i = 0; (i < s->rec_count) && (i < max_count); i++) {
    if(!GETWORD(word, curr, &curr)) {
      WARN1("UnpackExperiments: formatting ts failed: %s\n", curr);
      break;
    }
    to->timeStamp = strtod(word, NULL);
    if(!GETWORD(word, curr, &curr)) {
      WARN1("UnpackExperiments: formatting value failed: %s\n", curr);
      break;
    }
    to->value = strtod(word, NULL);
    to++;
  }

  *count = i;
  return(1);

}


int
LoadExperiments(struct host_cookie *mem_c,
                const char *exp_name,
                Experiment *exper,
                size_t count,
                double seq_no,
                size_t *out_count,
                double *out_seq_no,
                double timeout) {

  DataDescriptor contentsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  char *expContents;
  struct state expState;
  Socket memorySock;
  size_t recvSize;
  void(*was)(int);

  was = signal(SIGPIPE, SocketFailure);
  if((was != SIG_DFL) && (was != SIG_IGN)) {
    signal(SIGPIPE, was);
  }

  *out_count = 0;
  *out_seq_no = 0.0;

  if(!ConnectToHost(mem_c, &memorySock)) {
    FAIL2("LoadExperiments: couldn't contact server %s at port %d\n",
          mem_c->name, mem_c->port);
  }

  memset(&expState, 0, sizeof(struct state));
  SAFESTRCPY(expState.id, exp_name);
  expState.rec_count = count;
  /* a negative seq_no means to read count number of records regardless */
  expState.seq_no = (seq_no >= 0.0) ? seq_no : 0.0;

  if(!SendMessageAndData(memorySock,
                         FETCH_STATE,
                         &expState,
                         stateDescriptor,
                         stateDescriptorLength,
                         timeout)) {
    DisconnectHost(mem_c);
    FAIL("LoadExperiments: error making request\n");
  }
  else if(!RecvMessage(memorySock, STATE_FETCHED, &recvSize, timeout)) {
    DisconnectHost(mem_c);
    FAIL("LoadExperiments: message receive failed\n");
  }
  else if(recvSize == 0) {
    *out_count = 0;
  }
  else if(!RecvData(memorySock,
                    &expState,
                    stateDescriptor,
                    stateDescriptorLength,
                    timeout)) {
    DisconnectHost(mem_c);
    FAIL("LoadExperiments: state receive failed\n");
  }
  else if(expState.rec_count == 0) {
    *out_count = 0;
  }
  else {
    contentsDescriptor.repetitions = expState.rec_size * expState.rec_count;
    expContents = (char *)malloc(contentsDescriptor.repetitions + 1);
    if(expContents == NULL) {
      DisconnectHost(mem_c);
      FAIL("LoadExperiments: out of memory\n");
    }
    if(!RecvData(memorySock,
                 expContents,
                 &contentsDescriptor,
                 1,
                 timeout)) {
      free(expContents);
      DisconnectHost(mem_c);
      FAIL("LoadExperiments: data receive failed\n");
    }
    expContents[contentsDescriptor.repetitions] = '\0';
    if(!UnpackExperiments(&expState, expContents, count, exper, out_count)) {
      free(expContents);
      FAIL("LoadExperiments: unable to unpack experiment data.\n");
    }
    free(expContents);
  }

  *out_seq_no = expState.seq_no;
  return(1);

}


const char *
NameOfControl(const Control *control) {
  static char returnValue[255];
  /* We use host.control as our default name */
  vstrncpy(returnValue, sizeof(returnValue), 3,
           control->host, ".", control->controlName);
  return &returnValue[0];
}


const char *
NameOfSeries(const Series *series) {

  typedef struct {
    char name[15 + 1];
    char value[63 + 1];
  } optionInfo;

  const char *currentOption;
  const char *endOfName;
  int i;
  optionInfo newOption;
  optionInfo *options;
  unsigned int optionsCount;
  char optionText[127 + 1];
  static char returnValue[255 + 1];

  /* We use host.resource[.optionValue...] as our default name. */
  vstrncpy(returnValue, sizeof(returnValue), 3,
           series->host, ".", series->resource);

  /*
  ** We sort the option values, using insertion sort, by option name so that
  ** two series with the same options listed in different order will generate
  ** the same name.  This is none too efficient, but it would be extremely
  ** strange to be sorting more than a few items here.
  */
  options = (optionInfo *)malloc(sizeof(optionInfo));
  optionsCount = 0;
  for(currentOption = series->options;
      GETTOK(optionText, currentOption, "\t", &currentOption);
      ) {
    if(!GETTOK(newOption.name, optionText, ":", &endOfName)) {
      continue;
    }
    SAFESTRCPY(newOption.value, endOfName + 1);
    optionsCount++;
    options = (optionInfo *)REALLOC(options, optionsCount * sizeof(optionInfo));
    for(i = optionsCount - 2; i >= 0; i--) {
      if(strcmp(newOption.name, options[i].name) < 0) {
        options[i + 1] = options[i];
      }
      else {
        break;
      }
    }
    options[i + 1] = newOption;
  }

  for(i = 0; i < optionsCount; i++) {
    vstrncpy(returnValue, sizeof(returnValue), 3,
             returnValue, ".", options[i].value);
  }

  free(options);
  return &returnValue[0];

}


const char *
NameOfSkill(const Skill *skill) {
  static char returnValue[255];
  /* We use host.skill as our default name */
  vstrncpy(returnValue, sizeof(returnValue), 3,
           skill->host, ".", skill->skillName);
  return &returnValue[0];
}


Object
ObjectFromActivity(const char *name,
                   const Activity *activity) {
  Object returnValue = NewObject();
  AddAttribute(&returnValue, "name", name);
  AddAttribute(&returnValue, "objectclass", ACTIVITY_CLASS);
  AddAttribute(&returnValue, "controlName", activity->controlName);
  AddAttribute(&returnValue, "host", activity->host);
  AddAttribute(&returnValue, "option", AttributeNames(activity->options));
  AddAttribute(&returnValue, "resource", AttributeNames(activity->resources));
  AddAttribute(&returnValue, "skillName", activity->skillName);
  AddOptionAttributes(&returnValue, activity->options);
  return(returnValue);
}


Object
ObjectFromControl(const char *name,
                  const Control *control) {
  Object returnValue = NewObject();
  AddAttribute(&returnValue, "name", name);
  AddAttribute(&returnValue, "objectclass", CONTROL_CLASS);
  AddAttribute(&returnValue, "controlName", control->controlName);
  AddAttribute(&returnValue, "host", control->host);
  AddAttribute(&returnValue, "option", AttributeNames(control->options));
  AddAttribute(&returnValue, "skillName", AttributeNames(control->skills));
  AddOptionAttributes(&returnValue, control->options);
  return(returnValue);
}


Object
ObjectFromSeries(const char *name,
                 const Series *series) {
  Object returnValue = NewObject();
  AddAttribute(&returnValue, "name", name);
  AddAttribute(&returnValue, "objectclass", SERIES_CLASS);
  AddAttribute(&returnValue, "host", series->host);
  AddAttribute(&returnValue, "label", series->label);
  AddAttribute(&returnValue, "memory", series->memory);
  AddAttribute(&returnValue, "option", AttributeNames(series->options));
  AddAttribute(&returnValue, "resource", series->resource);
  AddOptionAttributes(&returnValue, series->options);
  return(returnValue);
}


Object
ObjectFromSkill(const char *name,
                const Skill *skill) {
  Object returnValue = NewObject();
  AddAttribute(&returnValue, "name", name);
  AddAttribute(&returnValue, "objectclass", SKILL_CLASS);
  AddAttribute(&returnValue, "host", skill->host);
  AddAttribute(&returnValue, "option", AttributeNames(skill->options));
  AddAttribute(&returnValue, "skillName", skill->skillName);
  AddOptionAttributes(&returnValue, skill->options);
  return(returnValue);
}


void
ObjectToActivity(const Object object,
                 Activity *activity) {

  const char *attrName;
  const char *attrValue;
  Attribute activityAttr;
  char *c;
  size_t optionsLen;

  /* Set up null values for optional Activity fields. */
  activity->options = strdup("");

  /* Pull the actual field values out of the Object attributes. */
  ForEachAttribute(object, activityAttr) {
    attrName = AttributeName(activityAttr);
    attrValue = AttributeValue(activityAttr);
    if(strcmp(attrName, "controlName") == 0) {
      SAFESTRCPY(activity->controlName, attrValue);
    }
    else if(strcmp(attrName, "host") == 0) {
      SAFESTRCPY(activity->host, attrValue);
    }
    else if(strcmp(attrName, "resource") == 0) {
      SAFESTRCPY(activity->resources, attrValue);
      for(c = activity->resources; *c != '\0'; c++) {
        if(*c == ',')
          *c = '\t';
      }
    }
    else if(strcmp(attrName, "skillName") == 0) {
      SAFESTRCPY(activity->skillName, attrValue);
    }
    else if((strcmp(attrName, "name") == 0) ||
            (strcmp(attrName, "objectclass") == 0) ||
            (strcmp(attrName, "option") == 0)) {
      ; /* Registration-generated attributes. */
    }
    else {
      optionsLen =
        strlen(activity->options) + strlen(attrName) + strlen(attrValue) + 3;
      activity->options = REALLOC(activity->options, optionsLen);
      vstrncpy(activity->options, optionsLen, 5,
               activity->options, attrName, ":", attrValue, "\t");
    }
  }

}


void
ObjectToControl(const Object object,
                Control *control) {

  const char *attrName;
  const char *attrValue;
  char *c;
  Attribute controlAttr;

  /* Set up null values for optional Control fields. */
  SAFESTRCPY(control->options, "");

  /* Pull the actual field values out of the Object attributes. */
  ForEachAttribute(object, controlAttr) {
    attrName = AttributeName(controlAttr);
    attrValue = AttributeValue(controlAttr);
    if(strcmp(attrName, "controlName") == 0) {
      SAFESTRCPY(control->controlName, attrValue);
    }
    else if(strcmp(attrName, "host") == 0) {
      SAFESTRCPY(control->host, attrValue);
    }
    else if((strcmp(attrName, "name") == 0) ||
            (strcmp(attrName, "objectclass") == 0) ||
            (strcmp(attrName, "option") == 0)) {
      ; /* Registration-generated attributes. */
    }
    else if(strcmp(attrName, "skillName") == 0) {
      SAFESTRCPY(control->skills, attrValue);
      for(c = control->skills; *c != '\0'; c++) {
        if(*c == ',')
          *c = '\t';
      }
    }
    else {
      vstrncpy(control->options, sizeof(control->options), 5,
               control->options, attrName, ":", attrValue, "\t");
    }
  }

}


void
ObjectToSeries(const Object object,
               Series *series) {

  const char *attrName;
  const char *attrValue;
  Attribute seriesAttr;

  /* Set up null values for optional Series fields. */
  SAFESTRCPY(series->options, "");

  /* Pull the actual field values out of the Object attributes. */
  ForEachAttribute(object, seriesAttr) {
    attrName = AttributeName(seriesAttr);
    attrValue = AttributeValue(seriesAttr);
    if(strcmp(attrName, "host") == 0) {
      SAFESTRCPY(series->host, attrValue);
    }
    else if(strcmp(attrName, "label") == 0) {
      SAFESTRCPY(series->label, attrValue);
    }
    else if(strcmp(attrName, "memory") == 0) {
      SAFESTRCPY(series->memory, attrValue);
    }
    else if(strcmp(attrName, "resource") == 0) {
      SAFESTRCPY(series->resource, attrValue);
    }
    else if((strcmp(attrName, "name") == 0) ||
            (strcmp(attrName, "objectclass") == 0) ||
            (strcmp(attrName, "option") == 0)) {
      ; /* Registration-generated attributes. */
    }
    else {
      vstrncpy(series->options, sizeof(series->options), 5,
               series->options, attrName, ":", attrValue, "\t");
    }
  }

}


void
ObjectToSkill(const Object object,
              Skill *skill) {

  const char *attrName;
  Attribute skillAttr;
  const char *attrValue;

  /* Set up null values for optional Skill fields. */
  SAFESTRCPY(skill->options, "");

  /* Pull the actual field values out of the Object attributes. */
  ForEachAttribute(object, skillAttr) {
    attrName = AttributeName(skillAttr);
    attrValue = AttributeValue(skillAttr);
    if(strcmp(attrName, "host") == 0) {
      SAFESTRCPY(skill->host, attrValue);
    }
    else if(strcmp(attrName, "skillName") == 0) {
      SAFESTRCPY(skill->skillName, attrValue);
    }
    else if((strcmp(attrName, "name") == 0) ||
            (strcmp(attrName, "objectclass") == 0) ||
            (strcmp(attrName, "option") == 0)) {
      ; /* Registration-generated attributes. */
    }
    else {
      vstrncpy(skill->options, sizeof(skill->options), 5,
               skill->options, attrName, ":", attrValue, "\t");
    }
  }

}


int
StoreExperiments(struct host_cookie *mem_c,
                 const char *id,
                 const Experiment *exper,
                 size_t count,
                 double forHowLong) {

  char *content;
  DataDescriptor contentsDescriptor = SIMPLE_DATA(CHAR_TYPE, 0);
  struct state expState;
  Socket memSock;
  size_t recSize;
  size_t recvSize;
  void(*was)(int);

  was = signal(SIGPIPE, SocketFailure);
  if((was != SIG_DFL) && (was != SIG_IGN)) {
    signal(SIGPIPE, was);
  }

  if(!ConnectToHost(mem_c, &memSock)) {
    FAIL2("StoreExperiments: couldn't contact server %s at port %d\n",
          mem_c->name, mem_c->port);
  }

  if(!PackExperiments(exper, count, &content, &recSize)) {
    FAIL("StoreExperiments: packing failed\n");
  }

  SAFESTRCPY(expState.id, id);
  expState.rec_count = count;
  expState.seq_no = CurrentTime();
  expState.rec_size = recSize;
  expState.time_out = forHowLong;
  contentsDescriptor.repetitions = count * recSize;

  if(!SendMessageAndDatas(memSock,
                          STORE_STATE,
                          &expState,
                          stateDescriptor,
                          stateDescriptorLength,
                          content,
                          &contentsDescriptor,
                          1,
                          PktTimeOut(memSock))) {
    free(content);
    DisconnectHost(mem_c);
    FAIL("StoreExperiments: error sending pkt\n");
  }

  free(content);

  if(!RecvMessage(memSock, STATE_STORED, &recvSize, PktTimeOut(memSock))) {
    DisconnectHost(mem_c);
    WARN("StoreExperiments: error recving ack\n");
  }

  return(1);

}
