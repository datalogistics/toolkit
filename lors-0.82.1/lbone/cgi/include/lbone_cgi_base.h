#ifndef __LBONE_CGI_BASE_H__
#define __LBONE_CGI_BASE_H__

#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#define	 _REENTRANT
#include <pthread.h>
#include <dirent.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

#include <sys/resource.h>


#include "jrb.h"
#include "fields.h"
#include "lber.h"
#include "ldap.h"
#include "ibp.h"
#include "lbone_base.h"
#include "lbone_search.h"

#define	LDAP_SERVER	"vertex.cs.utk.edu"
#define LDAP_SRVR_PORT	6776
#define	LDAP_WHO	"cn=root,o=lbone"
#define LDAP_PASSWD	"lbone"
#define LDAP_BASE	"ou=depots,o=lbone"

//#define LBONE_WEB_HOME  "http://www.cs.utk.edu/~soltesz/cgi-bin/lbone"
//#define LBONE_WEB_ROOT  "http://www.cs.utk.edu/~/soltesz"
#define LBONE_WEB_HOME  "http://loci.cs.utk.edu/lbone/cgi-bin/"
#define LBONE_WEB_ROOT  "http://loci.cs.utk.edu/lbone"


#endif /* __LBONE_CGI_BASE_H__ */
