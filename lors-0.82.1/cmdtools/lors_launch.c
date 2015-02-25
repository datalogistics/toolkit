#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lbone_socket.h>
#ifdef _MINGW
#include <winsock2.h>
#include <process.h>
#include <fcntl.h>
#include <io.h>
#endif

/*static int splitAddr(char *env_ip, char **ss_host, int *i_port);*/


#ifndef _MINGW
void sigpipe_handle(int d)
{
    fprintf(stderr, "You've got sigpipe: %d\n", d);
#ifndef _MINGW
    signal(SIGPIPE, sigpipe_handle);
#endif
    /*exit(EXIT_FAILURE);*/
}
#endif

int main(int argc, char **argv)
{
#define BUF_SIZE (1024)
    int fd = -1, fd_log = 0, fd_out = STDERR_FILENO, win = 0;
    int ret, i;
    char *env_ip = NULL, *env_ip_x = NULL;
    char    *s_port = NULL;
    char    *ss_host = NULL;
    int     i_port;
    char    buf[BUF_SIZE];
    int     i_pipe[2];
    int     pid;

#ifdef _MINGW
    int     tempstderr,j,tries=0,k;
    int     procstat=STILL_ACTIVE;
    char *newargv[100];
    char newWindowTitle[100];
    char *local_os;
    HANDLE hwndFound;
#endif

    if ( argc < 2 )
    {
        fprintf(stderr, "usage: lors_launch [-w] <command>\n");
        exit(EXIT_FAILURE);
    }
    for (i=0; i < argc; i++)
    {
        if ( strcmp(argv[1], "-w") == 0 )
        {
            fd_out = STDOUT_FILENO;
            win++;
            /*argv++;*/
        }
    }
#ifndef _MINGW
    signal(SIGPIPE, sigpipe_handle);
#endif



#ifdef _MINGW

local_os=getenv("LOCAL_OS");

    // Hides the console for the Windows GUI
if (local_os!=NULL &&  (strcmp(local_os, "MSWin32")==0))
       {
        // format a "unique" newWindowTitle
        wsprintf(newWindowTitle,"%d/%d",
                        GetTickCount(),
                        GetCurrentProcessId());
        // change current window title
        SetConsoleTitle(newWindowTitle);
        // ensure window title has been updated
        Sleep(40);
        // look for newWindowTitle
        hwndFound=FindWindow(NULL, newWindowTitle);
        // If found, hide it
        if ( hwndFound != NULL)
                ShowWindow( hwndFound, SW_HIDE);
        }


	
#endif
    env_ip_x = getenv("REMOTE_ADDR");
    if ( env_ip_x == NULL ) 
    {
        fprintf(stderr, "REMOTE_ADDR env variable not found.\n");
        fprintf(stderr, "Using 'localhost:5240' to connect to 'exNode View'\n");
        fprintf(stderr, "To avoid this message, set this environment variable\n");
        env_ip_x = "localhost:5240";
    }
    env_ip = strdup(env_ip_x);

    splitAddr(env_ip, &ss_host, &i_port);


    /* the simple dup/exec combination did not work. often lines were lost to
     * the viz.  as an alternate solution, i've added a pipe, to which the
     * command writes, and from which the launch reads and sends to the viz.
     * this works quite well. */
#ifndef _MINGW
    if ( pipe(i_pipe) != 0 )
    {
        perror("pipe");
#else
    if( _pipe(i_pipe, 1024, O_BINARY | O_NOINHERIT) == -1)
    { 
 	fprintf(stderr, "Creation of pipe failed\n");
#endif
        exit(EXIT_FAILURE);
    }

#ifndef _MINGW
    pid = fork();
    if ( pid == 0 ) 
    {
        /* child */
        close(i_pipe[0]);

        ret = dup2(i_pipe[1], STDERR_FILENO);
        if ( ret != 2 ) { perror("dup2"); exit(2); }
        argv++;
        if ( win ) argv++;

        execvp(argv[0], argv);
        fprintf(stdout, "failed to execute command: %s\n", argv[0]);
        exit(EXIT_FAILURE);
    } else 
    {
        /* get connection to viz */
        close(i_pipe[1]);

#ifdef _MINGW
        fd_log = open(".lors_log", O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0666);
#else
        fd_log = open(".lors_log", O_CREAT|O_TRUNC|O_WRONLY, 0666);
#endif

        if ( fd_log == -1 ) { perror("Cannot create log file"); }
        else {
            for (i=0; i < argc; i++)
            {
                write(fd_log, argv[i], strlen(argv[i]));
                write(fd_log, " ", 1);
            }
            write(fd_log, "\n\n", 2);
        }

        fd = request_connection(ss_host, i_port);
        if ( fd == -1 )
        {
            fprintf(stderr, "Could not open connection to %s\n", env_ip);
            fd = fd_out;
        } else 
        {
            char resp_buf[1024];
            int i;

            i = read (fd, buf, 6);
            if ( i != 6 )
            {
                fd=fd_out;
            }
        }
        do 
        {
            ret = read(i_pipe[0], buf, BUF_SIZE);
            usleep (100000);
            if ( fd == STDERR_FILENO )
            {
                write(fd_out, buf, ret);
            } else 
            {
                write(fd, buf, ret);
                write(fd_out, buf, ret);
            }
            /*write(fd, buf, ret);*/
            /*if ( fd != 2 ) write(1, buf, ret);*/
            if ( fd_log != -1 ) write(fd_log, buf, ret);
        } while ( ret > 0 );
        /* parent */
    }  /* MinGW code */
#else 




    tempstderr = _dup(_fileno(stderr));

    if(_dup2(i_pipe[1], _fileno(stderr)) != 0) {
	  fprintf(stderr, "Dup2 failed!\n");
	  exit(EXIT_FAILURE);
    }
    close(i_pipe[1]);
	
    /*setup new argv*/

    for(k=1;k<argc;k++) {
	    newargv[k-1]=(char *)malloc((3+strlen(argv[k]))*sizeof(char));
	    if(newargv[k-1]==NULL) {fprintf(stderr, "Out of Memory"); exit(1);}
	    strcpy(newargv[k-1], "\"");
	    strcat(newargv[k-1],argv[k]);
	    strcat(newargv[k-1],"\"");

    }
    newargv[k-1]=NULL;
			 
    pid=spawnvp(P_NOWAIT, argv[1], (const char* const*)newargv);
    if(_dup2(tempstderr, _fileno(stderr))!=0) {
	  fprintf(stderr, "Dup2 failed!\n");
	  exit(EXIT_FAILURE);
    }

    close(tempstderr);

    if(pid)
    {
       
        fd_log = open(".lors_log", O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0666);
        if ( fd_log == -1 ) { perror("Cannot create log file"); }
        else {
            for (i=1; i < argc; i++)
            {
                write(fd_log, argv[i], strlen(argv[i]));
                write(fd_log, " ", 1);
            }
            write(fd_log, "\n\n", 2);
        }

        fd = request_connection(ss_host, i_port);
        if ( fd == -1 )
        {
            fprintf(stderr, "Could not open connection to %s\n", env_ip);
            fd = _fileno(stderr);
        } else 
        {
            char resp_buf[1024];

            i = recv (fd, resp_buf, 6, 0);
            if ( i != 6 )
            {
                fd=_fileno(stderr);
            }
        }
	j=1;
	while(procstat == STILL_ACTIVE  )
	while(j>0)
	{
		j=read(i_pipe[0], buf, 1024);
		if(j>0) 
		{
			write(_fileno(stderr), buf, j);	
			if(fd != _fileno(stderr))
			{  
				send(fd, buf, j,0);
			}
			if(fd_log >= 0)
			{
				write(fd_log, buf, j);
			}

	        }	
//		Sleep(500);

		if(!GetExitCodeProcess((HANDLE)pid, (unsigned long*)&procstat)) 
		{
		  perror("GetExitCodeProcess");
		  exit(EXIT_FAILURE);
		}
         
	}
	Sleep(500);
    }
    else { // Spawn failed horribly

	 perror("Spawn failed");
    }


#endif /* End of MinGW code */
    return 0;
}
int splitAddr(char *env_ip, char **ss_host, int *i_port)
{
    char *s;
    s = strchr(env_ip, ':');
    if ( s != NULL )
    {
        *i_port = atoi(s+1);
        *s = '\0'; 
    } else 
    {
        *i_port = 5240;
    }
    *ss_host = env_ip;
}
