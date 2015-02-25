/* newNGC.c
   Jin Ding
   LBONE Server Call
   NetGeo client call applied */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>


int getString(int fd, char *buf, char delimiter)
{
  char c;
  int i;
  
  i = 0;  
  buf[0] = '\0';
  if (read(fd, &c, 1) < 0) return -1;
  for (i = 0; c != delimiter; i ++) {
    strncat(buf, &c, 1);
    if (read(fd, &c, 1) < 0) return -1;
  }
  buf[i] = '\0';
  
  return i;  
}


char **getLatLong(char **args)
{
  int pipefd[2], pipefd2[2];
  int i, j, k, n, dummy;
  char s[256];
  char c;
  
  char *locations;
  char **latlongArray;
  
  if (args == NULL || args[0] == NULL) {
    fprintf(stderr, "Invalid locations argument!\n");
    return NULL;
  }

  for (i = 0, j = 0; args[i] != NULL; j += strlen( args[i] ), i ++);
  locations = (char *) malloc (sizeof( char ) * (i + 1 + j));
    
  strcpy(locations, args[0]);
  for (i = 1; args[i] != NULL; i ++) {
    strcat(locations, " ");
    strcat(locations, args[i]);
  }
  strcat(locations, "\n");
    
  if (pipe(pipefd) < 0) {
    perror("headsort: pipe");
    exit(1);
  }
  if (pipe(pipefd2) < 0) {
    perror("headsort: pipe");
    exit(1);
  }

  if (fork() == 0) {
    if (dup2(pipefd[1], 1) != 1) {
      perror("getLatLong: dup2(pipefd[1], 1)");
      exit(1);
    }
    if (dup2(pipefd2[0], 0) != 0) {
      perror("getLatLong: dup2(pipefd2[0], 0)");
      exit(1);
    }
    close(pipefd[1]);
    close(pipefd[0]);
    close(pipefd2[1]);
    close(pipefd2[0]);
    /*execlp("ngc.pl", "ngc.pl", NULL);*/
    execl("/ficus/homes/lociweb/www-home/lbone/cgi-bin/ngc.pl", "ngc.pl", NULL);
    perror("getLatLong: execlp ngc.pl failed");
    exit(1);
  } else {
    close(pipefd[1]);
    close(pipefd2[0]);
      
    write(pipefd2[1], locations, strlen( locations ));
    
    if (getString(pipefd[0], s, '\n') > 0) {
      if ((k = atoi( s )) > 0) {
        latlongArray = (char **) malloc (sizeof(char *) * (k * 3 + 1));
        for (i = 0; i < k * 3; i ++) {
	  getString(pipefd[0], s, '\n');
	  latlongArray[i] = strdup( s );
	}	  
        latlongArray[i] = NULL;
      } else {
	latlongArray = NULL;
      }
      close(pipefd[0]);
      close(pipefd2[1]);
      wait(&dummy);
    }
    
    /*if (latlongArray != NULL) {
      for (i = 0; latlongArray[i] != NULL; i ++)
        printf("%s\n", latlongArray[i]);
    }*/
    
    return latlongArray;
  }
}

/*********************************************************************
int main(int argc, char **argv)
{  
  int i = 0, j = 0;
  char **vals, **args;

  char *locations[] = {"utk.edu", "yale.edu", "derema.com", "apple.com", "npr.org", NULL};
  
  for (i = 0; i < 5; i ++)
  {
    args[0] = locations[i];
    args[1] = NULL;

    vals = getLatLong( args );
    if (vals != NULL)
    {
      j = 0;
      while (vals[j] != NULL)
      {
        printf("%s ", vals[j]);
        free(vals[j]);
        j++;
      }
    }
    printf("\n\n");
    free (vals);
  }
  return 0;
}
*********************************************************************/
