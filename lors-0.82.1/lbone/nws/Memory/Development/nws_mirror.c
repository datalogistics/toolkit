/* $Id: nws_mirror.c,v 1.7 1999/01/22 23:21:40 jhayes Exp $ */

#include "config.h"
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
/* in here twice?? ns #include <netdb.h> */
/* in here twice??? ns #include <sys/wait.h> */
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAS_SYS_SELECT_H */
#include <stdlib.h>
#include <string.h>
#ifdef linux
#include "linux.h"
#endif


#include "getopt.h"
#include "state.h"
#include "protocol.h"
#include "hosts.h"
#include "host_protocol.h"
#include "forecasts.h"
#include "experiments.h"
#include "fore_protocol.h"
#include "diagnostic.h"
#include "strutil.h"

#define DEFAULTREFRESH (30.0*60.0)
#define MAXRETRIES 60
#define DEFAULTHEARTBEAT (15.0*60.0)
#define IDLETIME 10

#define SHORT_BEAT 120

char My_in_addr[255];
char My_name[255];
struct host_desc My_fore_desc;
int My_ear;
short My_ear_port;

char My_addrs[255];
int  My_addr_count;

#define MAXMIRRORS 26

struct host_cookie Mirrors[MAXMIRRORS];
short Mirror_ports[MAXMIRRORS];
int Active_mirrors[MAXMIRRORS];
int Mirror_count;

extern int gethostname();

struct host_cookie Name_server;
struct host_cookie Me;

char My_memory_name[255];
struct host_cookie Memory_server;

int Running;

#define SENSOR_ARGS "M:m:N:n:e:l:"

extern char *optarg;

char Log_file[128];
char Error_file[128];

double Refresh_time = DEFAULTREFRESH;
double Heart_beat = DEFAULTHEARTBEAT;

char Journal_buffer[MAXRECSIZE*MEMSIZE];
double Last_seq_no;
int Journal_rec_size;

int
MakeName(char *tag, char *name, int size)
{
	char myname[255];
	int ierr;
	int end;
	struct hostent *hp;
	struct in_addr local_addr;

	if(My_in_addr[0] == 0)
	{

		
		ierr = gethostname(myname,sizeof(myname));

		if(ierr < 0)
		{
			FAIL("MakeName: gethostname failed\n");
		}

		hp = gethostbyname(myname);

		if(hp == NULL)
		{
			FAIL("MakeName: gethostbyname failed\n");
		}
		memcpy(&local_addr,hp->h_addr,sizeof(local_addr));
		SAFESTRCPY(My_in_addr,inet_ntoa(local_addr));

	}

	end = strlen(My_in_addr);

	if((end + strlen(tag)+1) > size)
	{
		FAIL2("MakeName: name %s.%s too big\n",My_in_addr,tag);
	}

	sprintf(name,"%s.%s",My_in_addr,tag);

	return(1);
}

int
IAmHealthy()
{
	struct host_desc fore_desc;
	int ierr;
	
	ierr = GetHostDesc(My_name,
			     &Name_server,
			     &fore_desc);

	if(ierr == 0)
	{
		FAIL2("IAmHealthy: failed to get descriptor from %s at %d\n",
			Name_server.name,Name_server.port);
	}

	if(strcmp(fore_desc.host_name,My_fore_desc.host_name) != 0)
	{
		FAIL2("IAmHealthy: name mismatch -- %s is not %s\n",
			fore_desc.host_name,My_fore_desc.host_name);
	}

	if(fore_desc.port != My_fore_desc.port)
	{
		FAIL2("IAmHealthy: port mismatch -- %d is not %d\n",
			fore_desc.port,My_fore_desc.port);
	}
 
	return(1);
}

int
DoSelfExam(int sd, int *error)
{
	int ierr;

	*error = 0;

	ierr = IAmHealthy();

	if(ierr == 0)
	{
		send_pkt.h.type = SENSOR_SICK;
	}
	else
	{
		send_pkt.h.type = SENSOR_HEALTHY;
	}
	send_pkt.h.data_size = 0;

	ierr = SendPkt(sd,&send_pkt,PktTimeOut(sd));

	if(ierr == 0)
	{
		*error = send_pkt.h.error;
		FAIL("DoSelfExam: send reply failed\n");
	}

	return(1);
}

int
InitMirror(struct host_cooke *mem_c)
{
	int ierr;
	double now;
	int sd;
	int lcount;


	ierr = RequestRawData(mem_c->name,
			      mem_c->port,
			      JOURNAL,
			      MEMSIZE,
			      Journal_buffer,
			      sizeof(Journal_buffer),
			      now - Period,
			      &lcount,
			      &Journal_rec_size,
			      &Last_seq_no);

	if(ierr == 0)
	{
		WARN("InitMirror: inital read failed\n");
		Initd = 0;
	}

	Initd = 1;

	return(1);
}

int
ReadJournalBuffer(struct host_cookie *mem_c,
		  char *jbuffer,
		  int jbuffer_size,
		  int *out_recsize,
		  int *out_count)
{
	int ierr;
	double lseq_no;
	int lrecsize;
	int lcount;

	ierr = RequestRawData(mem_c->name,
                              mem_c->port,
                              JOURNAL,
                              MEMSIZE,
                              jbuffer,
                              jbuffer_size,
			      Last_seq_no,
                              &lcount,
                              &lrecsize,
                              &lseq_no);

	if(ierr == 0)
	{
		FAIL("ReadJournalBuffer: couldn't get data\n");
	}

	if(lrecsize != Journal_rec_size)
	{
		FAIL2("ReadJournalBuffer: size mismatch: %d vs %d\n",
			lrecsize,Journal_rec_size);
	}

	Last_seq_no = lseq_no;
	*out_count = lcount;
	*out_recsize = lrecsize;

	return(1);
}

int
MoveData(struct host_cookie *mem_c,
         struct host_cookie *mirror_c,
	 int mirror_count,
	double seq_no)
{
	int ierr;
	int lcount;
	int lrecsize;
	double lseq_no;
	int i;
	struct state *in_s;
	struct state *out_s;

	if(Initd == 0)
	{
		ierr = InitMirror(mem_c);

		if(ierr == 0)
		{
			FAIL("MoveDat: couldn't init\n");
		}
		Initd = 1;
		return(1);
	}

	ierr = ConnectToHost(mem_c,&sd);

	if(ierr == 0)
	{
		FAIL1("MoveData: couldn't connect to host %s\n", mem_c->name);
	}

	out_s = (struct pkt *)send_pkt.data;
	memset(out_s,0,sizeof(struct state));

	SAFESTRCPY(out_s->id,name);
	out_s->seq_no = htond(seq_no);
	out_s->count = htonl(MEMSIZE);

	send_pkt.h.type = LOAD_STATE;
	send_pkt.h.data_size = sizeof(struct state);

	ierr = SendPkt(sd,&send_pkt,PKTTIMEOUT);

	if(ierr == 0)
	{
		DisconnectHost(mem_c);
		FAIL("MoveData: load state send failed\n");
	}

	ierr = RecvPkt(sd,&recv_pkt,PktTimeOut(sd));

	if(ierr == 0)
	{
		DisconnectHost(mem_c);
		FAIL1("MoveData: couldn't recv state for name %s\n", name);
	}

	if(recv_pkt.h.type != STATE_TYPE)
	{
		DisconnectHost(mem_c);
		FAIL2("MoveData: reply error for %s, %d\n",name,recv_pkt.h.type);
	}

	in_s = (struct state *)recv_pkt.data;
	in_s->seq_no = ntohd(in_s->seq_no);
	in_s->timeout = ntohd(in_s->timeout);
	in_s->rec_size = ntohl(in_s->rec_size);
	in_s->rec_count = ntohl(in_s->rec_count);

	lrecsize = in_s->rec_size;
	lcount = in_s->rec_count;

	curr = (char *)send_pkt.data + sizeof(struct state);
	curr1 = (char *)recv_pkt.data + lrecsize*lcount - lrecsize;
	i = 0;

	while(i < lcount)
	{
		memcpy(curr,curr1,lrecsize);
		curr += lrecsize;
		if(curr >= ((char *)send_pkt.data))
			break;
		curr1 -= lrecsize;
		i++;
	}

	memset(out_s,0,sizeof(struct state));
	out_s->seq_no = in_s->seq_no;
	out_s->rec_count = in_s->rec_count;
	out_s->rec_size = in_s->rec_size;
	out_s->timeout = in_s->timeout;

	i = 0;
	while(i < mirror_count)
	{
		ierr = ConnectToHost(&(mirrors[i]),&sd);
		if(ierr == 0)
		{
			WARN1("MoveData: couldn't connect to mirror %s\n", mirrors[i].name);
			i++;
			continue;
		}

		send_pkt.h.type = STORE_STATE_NJ;
		send_pkt.h.data_size = sizeof(struct state) + 
						lreccount*lrecsize;
		ierr = SendPkt(sd,&send_pkt,PktTimeOut(sd));

		if(ierr == 0)
		{
			WARN1("MoveData: couldn't send to mirror %s\n", mirrors[i].name);
			DisconnectHost(&mirrors[i]);
			i++;
			continue;
		}

		ierr = RecvPkt(sd,&recv_pkt,PktTimeOut(sd));

		if(ierr == 0)
		{
			WARN1("MoveData: couldn't receive ack from %s\n",
				mirrors[i].name);
			DisconnectHost(&mirrors[i]);
			i++;
			continue;
		}
		
		i++;
	}

	return(1);
}
	
	
void
DoMirror(struct host_cookie *mem_c,
	 struct host_cookie *mirrors,
	 int mirror_count)
{
	int ierr;
	int ljrecsize;
	int ljreccount;
	char *jrec;
	int i;
	char **jreclist;

	ierr = ReadJournalBuffer(mem_c,
				 Journal_buffer,
				 sizeof(Journal_buffer),
				 &ljrecsize,
				 &ljreccount);

	if(ierr == 0)
	{
		Diagnostic(DIAGERROR,"DoMirrors: couldn't read journal\n");
		return;
	}

	jrec = (char *)malloc(jrecsize+1);

	if(jrec == NULL)
	{
		Diagnostic(DIAGERROR,"DoMirrors: could get space for journal rec\n");
	}

	jreclist = (char **)malloc(jreccount*sizeof(char *));

	if(jreclist == NULL)
	{
		Diagnostic(DIAGERROR,"DoMirrors: could get space for journal rec\n");
	}









StartMirror()
{
	int ierr;
	int i;
	char *curr;
	int lcount;

	ierr = MakeInAddr(My_name,My_In_addr,sizeof(My_in_addr));

	if(ierr == 0)
	{
		ABORT("StartMirror: make in addr failed\n");
	}

	SAFESTRCPY(My_fore_desc.host_name,My_in_addr);

	ierr = EstablishAnEar(DEFAULTPREDPORT,
			      DEFAULTPREDPORT,
			      &My_ear,
			      &My_ear_port);

	if(ierr == 0)
	{
		ABORT2(
	"StartMirror: couldn't establish an ear between %d and %d\n",
			DEFAULTPREDPORT,DEFAULTPREDPORT);
	}

	My_fore_desc.port = My_ear_port;

	MakeHostCookie(My_fore_desc.host_name,My_fore_desc.port,&Me);

	ierr = InitMirror(&Memory_server);

	if(ierr == 0)
	{
		WARN("StartMirror: couldn't init mirror\n");
	}


	Running = 1;

	return;

}

void
MirrorKill()
{
	fd_set readfds;
	fd_set writefds;
	int i;

	shutdown(My_ear,2);

	close(My_ear);
	DisconnectHost(&Name_server);
	DisconnectHost(&Memory_server);

		

	readfds = Connected_sockets;
	writefds = Connected_sockets;

	select(FD_SETSIZE,&readfds,&writefds,0,NULL);

	for(i=0; i < FD_SETSIZE; i++)
	{
		if(FD_ISSET(i,&readfds) || FD_ISSET(i,&writefds))
		{
			shutdown(i,2);
			close(i);
		}
	}

	WARN("PredKilled\n");
	exit(2);
}


int
ProcessRequest(int sd, struct in_addr *from, int *error)
{
	int ierr;
	int comp_code;
	int pid;
	char print_name[MAXHOSTNAME];

	*error = 0;
	comp_code = 1;

	ierr = RecvPkt(sd,&recv_pkt,PktTimeOut(sd));

	if(ierr == 0)
	{
		*error = recv_pkt.h.error;
		FAIL("ProcessRequest: recv pkt failed\n");
	}
	
	ierr = SocketToName(sd,print_name,sizeof(print_name));
	if(ierr == 1)
	{
		LOG2("Processing type %d from %s\n",recv_pkt.h.type,print_name);
	}
	else
	{
		LOG1("Processing type %d\n",recv_pkt.h.type);
	}
 

	switch(recv_pkt.h.type)
	{
		case SENSOR_TEST:
			ierr = DoSelfExam(sd,error);
			if(ierr == 0)
			{
				WARN("ProcessRequest: fore test failed\n");
				comp_code = 0;
			}
			CloseDown(sd);
			break;
		case SENSOR_KILL:
			MirrorKill();
			break;
		default:
			WARN("ProcessRequest: pkt type error\n");
			comp_code = 0;
	}

	return(comp_code);
}
			
		
void
main(int argc, char *argv[])
{

	int c;
	int ierr;
	int jerr;
	int sd;
	struct in_addr from;
	int error;
	double now;
	FILE *openFD;
	double lasttime;
	double wakeup;
	fd_set potential;
	int mem_specified;
	int mirror_specified;
	int index;
	int i;


	MakeHostCookie(DEFAULTNAMESERVER,DEFAULTNAMEPORT,&Name_server);
	mem_specified = 0;
	mirror_specified = 0;
	Period = 60;

	signal(SIGUSR1,RestartPred);

	for(i=0; i< MAXMIRRORS; i++)
	{
		Active_mirrors[i] = -1;
		Mirror_ports[i] = DEFAULTMEMPORT;
	}

	while((c = getopt(argc,argv,SENSOR_ARGS)) != EOF)
        {
                switch(c)
                {
			case 'A':
				SAFESTRCPY(Mirrors[Mirror_count].name,optarg);
				Active_mirrors[0] = Mirror_count;
				mirror_specified = 1;
				Mirror_count++;
				break;
			case 'a':
				Mirror_ports[0] = (short)atoi(optarg);
				break;
			case 'N':
				SAFESTRCPY(Name_server.name,optarg);
				break;
                        case 'n':
                                Name_server.port = atoi(optarg);
                                break;
			case 'M':
				SAFESTRCPY(Memory_server.name,optarg);
				mem_specified = 1;
				break;
                        case 'm':
                                Memory_server.port = atoi(optarg);
                                break;
			case 'P':
				Period = atof(optarg);
				break;
		        case 'e':
                                SAFESTRCPY(Error_file,optarg);
                                break;
                        case 'l':
                                SAFESTRCPY(Log_file,optarg);
                                break;
                        default:
				WARN1("nws_mirror: unrecognized argument: %c\n",c);
                                break;
                }
        }

        if(Log_file[0] != 0)
        {
                openFD = fopen(Log_file,"w");
                if(openFD == NULL)
                {
			WARN1("nws_mirror: couldn't open log file %s\n",
                                Log_file);
                }
		else
		{
			DirectDiagnostics(DIAGLOG,openFD);
		}
        }

        if(Error_file[0] != 0)
        {
                openFD = fopen(Error_file,"w");
                if(openFD == NULL)
                {
			WARN1("nws_mirror: couldn't open error file %s\n",
                                Error_file);
                }
		else
		{
			DirectDiagnostics(DIAGWARN,openFD);
			DirectDiagnostics(DIAGERROR,openFD);
			DirectDiagnostics(DIAGFATAL,openFD);
		}
        }

	for(i=0; i< MAXMIRRORS; i++)
	{
		index = Active_mirrors[i];
		if(index != -1)
		{
			Mirrors[index].port = Mirror_ports[i];
		}
	}

	(void)StartMirror();

	GetCurrentTime(&now);
	lasttime = now;

	/*
	 * main service loop
	 */
	while(1)
	{
		potential = Connected_sockets;
		FD_SET(My_ear,&potential);

		wakeup = Period;

		ierr = IncomingRequest(&potential,wakeup,&sd,&from,&error); 

		if(ierr == 0)
		{
			if(error == BADSOCKET)
			{
				/*
				 * this is really horrible
				 *
				 * if the other side closes gracefully,
				 * we don't seem to get a SIGPIPE
				 * on the socket so select (buried in
				 * IncomingRequest) tries to read a bad
				 * socket
				 *
				 * there does not seem to be a reliable
				 * way to check for this condition
				 * which is a socket with a valid
				 * peername that only reads 0
				 */
				if(sd == Name_server.sd)
				{
					DisconnectHost(&Name_server);
				}
				if(sd == Memory_server.sd)
				{
					DisconnectHost(&Memory_server);
				}
				close(sd);
			}
			SocketFailure(SIGPIPE);
		}
		else
		{
			ierr = ProcessRequest(sd,&from,&error);

			if(ierr == 0)
			{
				if(error == NOT_CONN)
				{
					if(sd == Name_server.sd)
					{
						DisconnectHost(&Name_server);
					}
					if(sd == Memory_server.sd)
					{
						DisconnectHost(&Name_server);
					}
					close(sd);
				}
				SocketFailure(SIGPIPE);
			}
		}

	}



}

