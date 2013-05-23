#include <sys/types.h>
#include "lbone_base.h"
#include "lbone_cgi_base.h"

/*
 * Function declarations
 */

extern void html_top();
extern void html_end();
extern void back_button();


/*
 * DepotInfo get_depot_info()
 */

DepotInfo get_depot_info(LDAP *ldap, LDAPMessage *entry) {
  char		**vals;
  DepotInfo	depot;

  depot = (DepotInfo) malloc(sizeof(struct depot_info));
  memset(depot, 0, sizeof(struct depot_info));

  vals = ldap_get_values(ldap, entry, "depotname");
  depot->depotname = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "hostname");
  depot->hostname = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "port");
  depot->port = atoi(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "StableStorage");
  depot->StableStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "AvailableStableStorage");
  depot->AvailableStableStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "VolatileStorage");
  depot->VolatileStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "AvailableVolatileStorage");
  depot->AvailableVolatileStorage = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "Duration");
  depot->Duration = (float) (atof(vals[0]) / (24*60*60));
  if (depot->Duration < 0) depot->Duration = -1;
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "status");
  depot->status = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "lastUpdate");
  depot->lastUpdate = strtoul(vals[0], NULL, 0);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "email");
  if (ldap_count_values(vals) > 0) depot->email1 = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "c");
  if (ldap_count_values(vals) > 0) depot->country = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "st");
  if (ldap_count_values(vals) > 0) depot->state = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "l");
  if (ldap_count_values(vals) > 0) depot->city = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "zip");
  if (ldap_count_values(vals) > 0) depot->zip = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "airport");
  if (ldap_count_values(vals) > 0) depot->airport = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "lat");
  if (ldap_count_values(vals) > 0) depot->lat = (float) atof(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "lon");
  if (ldap_count_values(vals) > 0) depot->lon = (float) atof(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "connection");
  if (ldap_count_values(vals) > 0) depot->connection = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "monitoring");
  if (ldap_count_values(vals) > 0) depot->monitoring = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "power");
  if (ldap_count_values(vals) > 0) depot->power = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "backup");
  if (ldap_count_values(vals) > 0) depot->backup = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "notifyOwnerPeriod");
  if (ldap_count_values(vals) > 0) depot->notifyOwnerPeriod = (char *) strdup(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "polled");
  if (ldap_count_values(vals) > 0) depot->polled = atoi(vals[0]);
  ldap_value_free(vals);

  vals = ldap_get_values(ldap, entry, "responded");
  if (ldap_count_values(vals) > 0) depot->responded = atoi(vals[0]);
  ldap_value_free(vals);

  if (depot->polled != 0) depot->uptime = ((float) depot->responded / (float) depot->polled) * 100;

  vals = ldap_get_values(ldap, entry, "firewall");
  if (ldap_count_values(vals) > 0) {
    if (strcmp(vals[0], "yes") == 0) depot->firewall = (char *) strdup("yes");
    else depot->firewall = (char *) strdup("no");
  }
  ldap_value_free(vals);

  return depot;
}



/*
 * Html Top
 */

void html_top() {
    int             fd, sret;
    struct stat     s;
    char            *buf;
    printf ("Content-Type: text/html\n\n"); 
    sret = stat("header.html", &s);
    if ( sret == -1 )
    {
        perror("Could not stat header.html file");
        printf ("<HTML>\n<HEAD>\n<TITLE>Logistical BackBONE</TITLE></HEAD>\n");
        printf ("<BODY bgcolor=\"#ffffff\"><center>\n");
        return;
    }
    fd = open("header.html", O_RDONLY);
    if ( fd == -1 )
    {
        perror("Could not open header.html file");
        return;
    }
    buf = (char *)malloc((1+s.st_size)*(sizeof(char)));
    if ( buf == NULL ) 
    {
        perror("No memory for buffer");
        return;
    }
    sret = read(fd, buf, s.st_size);
    if ( sret != s.st_size )
    {
        perror("Could not read all of file");
        return;
    }
    buf[sret] = '\0';

    printf("%s", buf);
    return;


    /*
  printf ("<HTML>\n<HEAD>\n<TITLE>Logistical BackBONE</TITLE>\n");
  printf ("<meta http-equiv=\"Refresh\" content=\"180;URL=%s/lbone_list_view.cgi\"/>\n", 
          LBONE_WEB_HOME);
  printf ("</HEAD>\n");
  printf ("<BODY bgcolor=\"#ffffff\"><center>\n");

  printf("<table border=0 width=\"100%%\" cellspacing=0 cellpadding=0>\n");
  printf("<tr>\n");
  printf("<td width=468>");
  printf("<a href=\"../index.html\" title=\"L-Bone home\">");
  printf("<img src=\"../images/lbone-logo.gif\" width=468 height=50 ");
  printf(" alt=\"Click here to go to the L-Bone homepage\" border=0></a>");
  printf("</td>");
  printf("<td width=\"70%%\" background=\"../images/background.gif\">");
  printf("<p align=right>.</p>");
  printf("</td>");
  printf("</tr>\n");
  printf("<tr>\n");
  printf("<td width=\"100%%\" background=\"../images/menubar.gif\" height=18 align=center ");
  printf(" colspan=2 valign=top>\n");
  printf("<a href=\"../index.html\" title=\"L-Bone home\"><img src=\"../images/home.gif\" width=52 height=15");
  printf(" border=0 alt=\"Return to the L-Bone homepage\">");
  printf("</a><img src=\"../images/separator.gif\" width=21 height=15 alt=\"\">");
  printf("<a href=\"lbone_list_view.cgi\" title=\"View all registered depots\">");
  printf("<img src=\"../images/view_depots.gif\" border=0 width=100 height=15 ");
  printf(" alt=\"View all registered depots\">");
  printf("</a><img src=\"../images/separator.gif\" width=21 height=15 alt=\"\">");
  printf("<a href=\"lbone_depot_modify.cgi?action=add\" title=\"Add a depot\"><img src=\"../images/add_depot.gif\" border=0 ");
  printf(" width=87 height=15 alt=\"Add a depot\">");
  printf("</a><img src=\"../images/separator.gif\" width=21 height=15 alt=\"\">");
  printf("<a href=\"../lbone_api.html\" title=\"View the client api\"><img src=\"../images/client_api.gif\" border=0 ");
  printf(" width=87 height=15 alt=\"View the client api\">");
  printf("</a><img src=\"../images/separator.gif\" width=21 height=15 alt=\"\">");
  printf("<a href=\"http://loci.cs.utk.edu/ibp\" title=\"Go to the IBP homepage\"><img src=\"../images/ibp.gif\" border=0 ");
  printf(" width=37 height=15 alt=\"Go to the IBP homepage\">");
  printf("</a>\n");
  printf("</td>\n");
  printf("</tr>\n");
  printf("</table>\n");
  */
}



/*
 * Html End
 */

void html_end() {
    int             fd, sret;
    struct stat     s;
    char            *buf;
    sret = stat("footer.html", &s);
    if ( sret == -1 )
    {
        perror("Could not stat footer.html file");
        printf("</BODY></HTML>\n");
        return;
    }
    fd = open("footer.html", O_RDONLY);
    if ( fd == -1 )
    {
        perror("Could not open footer.html file");
        return;
    }
    buf = (char *)malloc((1+s.st_size)*(sizeof(char)));
    if ( buf == NULL ) 
    {
        perror("No memory for buffer");
        return;
    }
    sret = read(fd, buf, s.st_size);
    if ( sret != s.st_size )
    {
        perror("Could not read all of file");
        return;
    }
    buf[sret] = '\0';

    printf("%s", buf);
    return;

    /*
    printf("<center>\n");
    printf ("<p><a href=\"http://www.utk.edu\" title=\"UTK home\"><img src=\"../images/utk.gif\" width=\"115\" height=\"63\" alt=\"UTK home\" title=\"UTK home\" border=0></a>&nbsp;&nbsp;&nbsp;&nbsp;");
    printf ("<a href=\"%s\" title=\"LoCI Lab home\">", "http://loci.cs.utk.edu/");
    printf ("<img src=\"../images/locifooter.jpg\" width=200 height=63 alt=\"LoCI Lab\" ");
    printf (" title=\"LoCI Lab\" border=0></a>&nbsp;&nbsp;&nbsp;&nbsp;");
    printf ("<a href=\"http://www.energy.gov\" title=\"DOE\"><img src=\"../images/doe.gif\" ");
    printf ("width=140 height=75 alt=\"DOE\" title=\"DOE\" border=0></a><a ");
    printf ("href=\"http://www.nsf.gov\" title=\"NSF\"><img src=\"../images/nsf.gif\" ");
    printf ("width=140 height=75 alt=\"NSF\" title=\"NSF\" border=0></a></p>");

    printf("</center></BODY></HTML>\n");
    */
}




/*
 * Convert to url
 */
char *convert2url(char *s) {
  char		temp[1024], new_line[1024];
  char		*final;
  int		i, j;

  i = 0; j = 0;
  memset(new_line, 0, 1024);
  memset(temp, 0, 1024);

  while (s[i] != '\0') {
    if (((s[i] >= 'A') && (s[i] <= 'Z')) ||
        ((s[i] >= 'a') && (s[i] <= 'z')) ||
        ((s[i] >= '0') && (s[i] <= '9')) ||
        (s[i] == '-') || (s[i] == '_') || (s[i] == '.')) {

      new_line[j] = s[i];
      i++;
      j++;
      continue;
    }
    else if (s[i] == ' ') {
      new_line[j] = '+';
      i++;
      j++;
      continue;
    }
    else {
      memset(temp, 0, 1024);
      strcpy(temp, new_line); 
      memset(new_line, 0, 1024);
      sprintf(new_line, "%s%s%x", temp, "%", s[i]);
      i++;
      j += 3;
      continue;
    }
  }
  new_line[j] = '\0';
  final = (char *) strdup(new_line);
  return final;
}




/*
 * HTML back button form
 */

void back_button() {
    printf ("<form method=POST action=\"lbone_view.cgi?location=all&"
            "op=list&sortBy=depot\">\n");
    printf ("<input type=submit value=\"Back to List\" name=\"submit\">\n");
    printf ("<input type=hidden name=location value=all>\n");
    printf ("<input type=hidden name=op value=list>\n");
    printf ("<input type=hidden name=sortBy value=depot>\n");
    printf ("</form>\n");
}

