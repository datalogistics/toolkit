/* $Id: start_activity.c,v 1.9 2000/10/04 19:22:53 swany Exp $ */

#include <stdio.h>          /* printf() sprintf() */
#include <stdlib.h>         /* atoi() free() REALLOC() */
#include <string.h>         /* strcat() strcpy() strchr() */
#include <unistd.h>         /* getopt() */
#include <diagnostic.h>     /* NWS diagnostic messages. */
#define NWSAPI_SHORTNAMES
#include "config.h"
#include "nws_api.h"        /* Interface functions */


/*
** This program allows the user to launch and terminate NWS activities from the
** command line.  See the USERSGUIDE for a description of the command-line
** options.
*/


/*
** Parses #option# as a "name:value" pair and appends it to the #optionsCount#-
** long string array #optionsList#, updating #optionsCount# as appropriate.
** Allows for the pair to be split between sequential calls (i.e. "name:" one
** call, "value" the next).  Returns 1 if successful, else 0.
*/
static int
AddOption(const char *option,
          char ***optionList,
          size_t *optionsCount) {

  static int isValue = 0;
  char **lastOption;
  const char *valueStart;

  if(isValue) {
    valueStart = option;
  }
  else {
    valueStart = strchr(option, ':');
    if(valueStart == NULL) {
      fprintf(stderr, "AddOption: colon missing from option %s\n", option);
      return 0;
    }
    valueStart++;
  }

  if(isValue) {
    lastOption = (*optionList) + (*optionsCount - 1);
    *lastOption =
      REALLOC(*lastOption, strlen(*lastOption) + strlen(option) + 1);
    strcat(*lastOption, option);
  }
  else {
    (*optionsCount)++;
    *optionList = (char **)
       REALLOC(*optionList, *optionsCount * sizeof(char *));
    (*optionList)[*optionsCount - 1] = strdup(option);
  }
    
  isValue = *valueStart == '\0';
  return 1;

}


/*
** Frees all elements of the #length#-long allocated element array #array#.
*/
static void
FreeAllElements(void **array,
                size_t length) {
  int i;
  for(i = 0; i < length; i++)
    free(array[i]);
}


/*
** Returns the value of the #attributeName# attribute from #object#, or an
** empty string if no such attribute exists.
*/
static const char *
GetAttributeValue(const Object object,
                  const char *attributeName) {
  Attribute attribute;
  ForEachAttribute(object, attribute) {
    if(strcmp(AttributeName(attribute), attributeName) == 0)
      return AttributeValue(attribute);
  }
  return "";
}


/*
** Retrieves control and skill names matching the partial names in #control#
** and #skill# that have been registered by #sensor#, then attempts to narrow
** the choices to one control/skill pair using the options listed in the
** #optionsCount#-long list #options#.  If successful, returns 1 and sets
** #control# and #skill# to the retrieved names; otherwise, returns 0.
*/
static int
GetControlAndSkill(const HostSpec *sensor,
                   char *control,
                   char *skill,
                   const char **options,
                   int optionsCount) {

  Object controlObject;
  ObjectSet controlObjects;
  char filter[255 + 1];
  int i;
  char optionName[31 + 1];
  char possiblePairs[255 + 1];
  char recognizedOptions[255 + 1];
  Object skillObject;
  ObjectSet skillObjects;
  char supportedSkills[255 + 1];

  /* Grab lists of matching controls and objects from the name server. */
  sprintf(filter, "(&(%s=%s:%d)%s(%s=*%s*))",
          HOST_ATTR, sensor->machineName, sensor->machinePort,
          CONTROLS, CONTROL_ATTR, control);
  if(!GetObjects(filter, &controlObjects)) {
    fprintf(stderr, "GetControlAndSkill: unable to contact name server\n");
    return 0;
  }
  sprintf(filter, "(&(%s=%s:%d)%s(%s=*%s*))",
          HOST_ATTR, sensor->machineName, sensor->machinePort,
          SKILLS, SKILL_ATTR, skill);
  if(!GetObjects(filter, &skillObjects)) {
    FreeObjectSet(&controlObjects);
    fprintf(stderr, "GetControlAndSkill: unable to contact name server\n");
    return 0;
  }

  /*
  ** For each control/supported skill pair, match the option list against the
  ** control and skill options to see if the pair recognizes all of them.
  ** Throw out pairs where this isn't true and hope that we have exactly one
  ** pair at the end.
  */
  possiblePairs[0] = '\0';
  ForEachObject(controlObjects, controlObject) {

    strcpy(supportedSkills, GetAttributeValue(controlObject, SKILL_ATTR));

    ForEachObject(skillObjects, skillObject) {

      if(strstr(supportedSkills, GetAttributeValue(skillObject, SKILL_ATTR)) ==
         NULL)
        continue;

      strcpy(recognizedOptions, ",");
      strcat(recognizedOptions, GetAttributeValue(controlObject, OPTION_ATTR));
      strcat(recognizedOptions, ",");
      strcat(recognizedOptions, GetAttributeValue(skillObject, OPTION_ATTR));
      strcat(recognizedOptions, ",");

      for(i = 0; i < optionsCount; i++) {
        strcpy(optionName, ",");
        strcat(optionName, options[i]);
        *strchr(optionName, ':') = '\0';
        strcat(optionName, ",");
        if(strstr(recognizedOptions, optionName) == NULL)
          break; /* Unknown option. */
      }
      if(i == optionsCount) {
        strcpy(control, GetAttributeValue(controlObject, CONTROL_ATTR));
        strcpy(skill, GetAttributeValue(skillObject, SKILL_ATTR));
        if(possiblePairs[0] != '\0')
          strcat(possiblePairs, "\n");
        strcat(possiblePairs, CONTROL_ATTR);
        strcat(possiblePairs, ":");
        strcat(possiblePairs, control);
        strcat(possiblePairs, " ");
        strcat(possiblePairs, SKILL_ATTR);
        strcat(possiblePairs, ":");
        strcat(possiblePairs, skill);
      }
    }

  }

  FreeObjectSet(&controlObjects);
  FreeObjectSet(&skillObjects);

  if(possiblePairs[0] == '\0') {
    fprintf(stderr, "%s\n%s\n",
            "No registered control and skill support all listed options",
            "Please check your spelling and try again");
    return 0;
  }
  else if(strchr(possiblePairs, '\n') != NULL) {
    fprintf(stderr, "Ambiguous control/skill; please specify one of:\n");
    fprintf(stderr, "%s\n", possiblePairs);
    return 0;
  }

  return 1;

}


/*
** Returns 1 or 0 depending or not an object named #name# is registered with
** the name server.
*/
static int
LookupName(const char *name) {

  char filter[127 + 1];
  ObjectSet objects;
  int returnValue;

  sprintf(filter, "(%s=%s)", NAME_ATTR, name);
  if(!GetObjects(filter, &objects)) {
    return 0;
  }
  returnValue = FirstObject(objects) != NULL;
  FreeObjectSet(&objects);
  return returnValue;

}


/*
** Reads #fileName# and appends to #optionsList# any "name:value" pairs
** contained within.  Returns 1 if successful, else 0.
*/
static int
ParseOptionsFile(const char *fileName,
                 char ***optionsList,
                 size_t *optionsCount) {

  char line[255 + 1];
  FILE *optionsFile;
  char *word;

  optionsFile = fopen(fileName, "r");
  if(optionsFile == NULL) {
    fprintf(stderr, "ParseOptionsFile: unable to open file %s\n", fileName);
    return 0;
  }

  while(fgets(line, sizeof(line), optionsFile) != NULL) {
    for(word = strtok(line, " \n\t");
        (word != NULL) && (*word != '#');
        word = strtok(NULL, " \n\t")) {
      if(!AddOption(word, optionsList, optionsCount)) {
        fclose(optionsFile);
        return 0; /* Message will come from AddOption(). */
      }
    }
  }

  (void)fclose(optionsFile);
  return 1;

}


static void
RemoveOption(const char *optionName,
             char ***options,
             size_t *optionsCount,
             char *value) {

  int i;
  int j;
  int nameLen = strlen(optionName);

  *value = '\0';
  for(i = 0; i < *optionsCount; ) {
    if((strncmp((*options)[i], optionName, nameLen) == 0) &&
       ((*options)[i][nameLen] == ':')) {
      strcpy(value, &(*options)[i][nameLen + 1]);
      free((*options)[i]);
      (*optionsCount)--;
      for(j = i; j < *optionsCount; j++)
        (*options)[j] = (*options)[j + 1];
    }
    else {
      i++;
    }
  }

}


#define DEFAULT_SENSOR_PORT "8060"
#define START_COMMAND "start"
#define SWITCHES "f:F"

int debug;

int
main(int argc,
     char *argv[]) {

  extern char *optarg;
  extern int optind;
  char activityName[127 + 1];
  char controlName[127 + 1];
  HostSpec nsSpec;
  int opt;
  char **options;
  size_t optionsCount;
  char optionsFile[127 + 1];
  HostSpec sensorSpec;
  char skillName[127 + 1];
  const char *USAGE = "start_activity [-f file] [-F] host [option ...]";

  int force = 0;

  optionsFile[0] = '\0';
  DirectDiagnostics(DIAGERROR, stderr);
  DirectDiagnostics(DIAGFATAL, stderr);

  while((opt = getopt(argc, argv, SWITCHES)) != EOF) {

    switch(opt) {

    case 'f':
      strcpy(optionsFile, optarg);
      break;

	case 'F':
	  force = 1;
	  break;

    default:
      fprintf(stderr, "Unrecognized switch\n%s\n", USAGE);
      exit(1);
      break;

    }

  }

  if(optind >= argc) {
    fprintf(stderr, "%s\n", USAGE);
    exit(1);
  }

  sensorSpec = *MakeHostSpec
    (argv[optind++],
     atoi(EnvironmentValue("SENSOR_PORT", DEFAULT_SENSOR_PORT)));

  if(!GetNameServer(&sensorSpec, &nsSpec)) {
    fprintf(stderr, "Unable to contact sensor %s:%d\n",
            sensorSpec.machineName, sensorSpec.machinePort);
    exit(1);
  }
  else if(!UseNameServer(&nsSpec)) {
    fprintf(stderr, "Unable to contact name server %s:%d\n",
            nsSpec.machineName, nsSpec.machinePort);
    exit(1);
  }

  options = NULL;
  optionsCount = 0;

  if((optionsFile[0] != '\0') &&
     !ParseOptionsFile(optionsFile, &options, &optionsCount)) {
    FreeAllElements((void **)options, optionsCount);
    free(options);
    exit(1);  /* Message will come from ParseOptionsFile() */
  }

  for(; optind < argc; optind++) {
    if(!AddOption(argv[optind], &options, &optionsCount)) {
      FreeAllElements((void **)options, optionsCount);
      free(options);
      exit(1);  /* Message will come from AddOption() */
    }
  }

  activityName[0] = '\0';
  controlName[0] = '\0';
  skillName[0] = '\0';
  RemoveOption("name", &options, &optionsCount, activityName);
  RemoveOption(CONTROL_ATTR, &options, &optionsCount, controlName);
  RemoveOption(SKILL_ATTR, &options, &optionsCount, skillName);

  if(!GetControlAndSkill(&sensorSpec,
                         controlName,
                         skillName,
                         (const char **)options,
                         optionsCount)) {
    FreeAllElements((void **)options, optionsCount);
    free(options);
    exit(1);  /* Message will come from GetControlAndSkill() */
  }

  if(activityName[0] == '\0') {
    sprintf(activityName, "%s:%d.%s.%s.activity",
            sensorSpec.machineName, sensorSpec.machinePort, controlName, skillName);
  }
  if(LookupName(activityName)) {
	if (force == 0) {
	  fprintf(stderr,
			  "Activity already registered; use 'halt_activity %s' to restart\n",
			  activityName);
	  exit(1);
	}
	else {
	  fprintf(stderr, "Activity already registered; restarting anyway.\n");
	}
  }

  if(StartActivity(&sensorSpec,
                        activityName,
                        controlName,
                        skillName,
                        (const char **)options,
                        optionsCount)) {
    printf("Started activity %s on %s:%d\n",
           activityName, sensorSpec.machineName, sensorSpec.machinePort);
  }

  FreeAllElements((void **)options, optionsCount);
  free(options);
  return 0;

}
