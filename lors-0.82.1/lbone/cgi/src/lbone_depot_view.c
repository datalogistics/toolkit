#include "lbone_cgi_base.h"
#include "lbone_poll.h"
#include "ecgi.h"

extern DepotInfo get_depot_info();
extern void html_top();
extern void html_table_header();
extern void html_end();
extern int call_depot();
extern void back_button();
extern char *convert2url();


int main() {
  const char	*depotname;
  char		*filter, time[32];
  char		*get_update;
  int           retval, size, count, update;
  DepotInfo	depot;
  LDAP          *ldap;
  LDAPMessage   *entry, *result;

  if (cgiInit() != ECGI_SUCCESS) {
    printf("ERROR: %s\n", cgiStrError(cgi_errno));
    exit(cgi_errno);
  }

  count = 0; update = 0;

  if ((get_update = (char *) cgiParam ("get_update")) != NULL) {
    if (strcmp(get_update, "yes") == 0)
      update = 1;
  }

  if ((depotname = cgiParam ("depotname")) != NULL) {

    size = strlen(depotname) + strlen("depotname=") + 1;
    filter = (char *) malloc(size);
    memset(filter, 0, size);
    sprintf(filter, "depotname=%s", depotname);

    ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
    retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
    if (retval != LDAP_SUCCESS) {

      html_top();
      printf("<p><font size=+1 face=\"Arial,Helvetica\">The LDAP server did not respond.<br>");
      printf("It may be busy, the network may be congested or it may be down.</font></p>");
      printf("<p><font size=+1 face=\"Arial,Helvetica\">Please try back in a few minutes.</font></p>");
      printf("<p>&nbsp;</p>");
      printf("<p><font face=\"Arial,Helvetica\">If you have noticed the server down for more");
      printf(" than 10 minutes,<br>please send an email to ");
      printf("<a href=\"mailto:lbone@cs.utk.edu\">L-Bone Support</a>.</font></p>");
      html_end();

      return 0;
    }
    retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
    if (retval != LDAP_SUCCESS) {

      html_top();
      printf("<p><font size=+2 face=\"Arial,Helvetica\"><b>LBONE Depot Detail View</b></font></p>");
      printf("<p><font face=\"Arial,Helvetica\">Depot %s not found.<br>", depotname);
      printf("Please press the Back button to return to the depot list.</font></p>");

      back_button();

      html_end();

      return 0;
    }

    if ((entry = ldap_first_entry (ldap, result)) != NULL) {
      depot = get_depot_info(ldap, entry);
      if (update == 1) {
        retval = call_depot (ldap, entry);
        if (retval == 0) {
          retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
          if (retval == LDAP_SUCCESS) {
            if ((entry = ldap_first_entry (ldap, result)) != NULL) {
              depot = get_depot_info(ldap, entry);
            }
            else update = -1;
          }
          else update = -1;
        }
        else update = -1;
      }
    }
    else {

      html_top();
      printf("<p><font size=+2 face=\"Arial,Helvetica\"><b>LBONE Depot Detail View</b></font></p>");
      printf("<p><font face=\"Arial,Helvetica\">Depot <b>\"%s\"</b> not found.<br>", depotname);
      printf("Please press the Back button to return to the depot list.</font></p>");

      back_button();

      html_end();
      return 0;
    }

    html_top();

    printf("<p>&nbsp;</p>");
    printf("<table border=0 cellpadding=6 cellspacing=0 width=\"100%%\">\n");
    printf("<tr>\n");
    printf("<td width=\"50%%\" align=center>\n");

    printf("<table border=0 cellpadding=6 cellspacing=0 width=400>\n");
    printf("<tr>\n  <td bgcolor=87aec2 align=center>");
    printf("<font size=+2 face=\"Arial,Helvetica\"><b>Depot Detail View</b></font>");
    printf("</tr>\n");
    printf("</table>\n");
    printf("<table border=0 cellpadding=2 cellspacing=0 width=400>\n");
    printf("<tr>\n");
    if (update == 0) {
      printf("  <td bgcolor=FFFF80 align=center>");
      printf("<font size=-1 face=\"Arial,Helvetica\">Displaying Cached Information</td>\n");
    }
    else if (update == 1) {
      printf("  <td bgcolor=C1FFC1 align=center>");
      printf("<font size=-1 face=\"Arial,Helvetica\">Displaying Current Information</td>\n");
    }
    else {
      printf("  <td bgcolor=FF9090 align=center>");
      printf("<font size=-1 face=\"Arial,Helvetica\">Depot Did Not Respond<br>");
      printf("Displaying Cached Information</td>\n");
    }
    printf("</tr>\n");
    printf("</table>");

    printf("<table border=0 cellpadding=6 cellspacing=0 width=400>\n");
    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36><b>Depotname</b></td>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36>%s</td>\n", depot->depotname);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" height=36><b>Hostname</b></td>\n");
    printf("  <td width=\"50%%\" height=36>%s</td>\n", depot->hostname);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36><b>Port</b></td>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36>%d</td>\n", depot->port);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" height=36><b>Hard Storage</b></td>\n");
    printf("  <td width=\"50%%\" height=36>%ld MBs</td>\n", depot->StableStorage);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36><b>Available Hard Storage</b></td>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36>%ld MBs</td>\n", depot->AvailableStableStorage);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" height=36><b>Soft Storage</b></td>\n");
    printf("  <td width=\"50%%\" height=36>%ld MBs</td>\n", depot->VolatileStorage);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36><b>Available Soft Storage</b></td>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36>%ld MBs</td>\n", depot->AvailableVolatileStorage);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" height=36><b>Days Available</b></td>\n");
    if (depot->Duration < 0) printf("  <td width=\"50%%\" height=36>Indefinite</td>\n");
    else printf("  <td width=\"50%%\" height=36>%8.2f</td>\n", depot->Duration);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36><b>Status</b></td>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD height=36>%s</td>\n", depot->status);
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\"><b>Information Gathered On</b></td>\n");

    memset(time, 0, 32);
    if (depot->lastUpdate != 0) {
      strncpy(time, ctime((time_t *) &depot->lastUpdate), strlen(ctime((time_t *)&depot->lastUpdate)));
      time[strcspn(time, "\n")] = '\0';
    } else {
      strncpy(time, "Never", 6);
    }
    printf("  <td width=\"50%%\">%s</td>\n", time);

    printf("</tr>\n");
    printf("</table>");

    printf("<table border=0 cellpadding=2 cellspacing=0 width=400>\n");
    printf("<tr>\n");
    printf("  <td bgcolor=DDDDDD><font size=-2>&nbsp;</font></td>\n");
    printf("</tr>\n");
    printf("</table>");

    /* create a separate form for both the 'back to list' and 'check depot' buttons */

    if (update == 0) {
        printf("<p>Click to check the depot and display current information.<br>");
        printf("<font size=-1>It may take 3-15 seconds.</font></p>");
        printf("<table><tr><td nowrap>\n");
        printf ("<form method=POST action=\"lbone_depot_view.cgi?depotname=%s"
            "&get_update=yes\">\n", convert2url(depot->depotname));
        printf ("<input type=\"submit\" value=\"Check Depot\" name=\"submit\">\n");
        printf ("<input type=hidden name=depotname value=\"%s\">\n", 
                depot->depotname);
        printf ("<input type=hidden name=get_update value=yes>\n");
        printf("&nbsp;&nbsp;");
        printf("</form>\n");
        printf("</td><td nowrap>\n");
    }
    printf ("<form method=POST action=\"lbone_view.cgi?location=all&"
            "op=list&sortBy=depot\">\n");
    printf ("<input type=submit value=\"Back to List\" name=\"submit\">\n");
    printf ("<input type=hidden name=location value=all>\n");
    printf ("<input type=hidden name=op value=list>\n");
    printf ("<input type=hidden name=sortBy value=depot>\n");
    printf ("</form>\n");
    if ( update == 0 ) printf("</td></tr></table>\n");


    printf("  <p>If this is your depot, you may modify it.");
    printf("<br><form method=\"POST\" action=\"lbone_depot_modify.cgi\">\n");
    printf("<input type=\"submit\" name=\"action\" value=\"modify\">");
    printf("<input type=\"hidden\" name=\"depotname\" value=\"%s\">", depot->depotname);
    printf("<input type=\"hidden\" name=\"hostname\" value=\"%s\">", depot->hostname);
    printf("<input type=\"hidden\" name=\"port\" value=\"%d\">", depot->port);
    printf("<input type=\"hidden\" name=\"StableStorage\" value=\"%ld\">", depot->StableStorage);
    printf("<input type=\"hidden\" name=\"AvailableStableStorage\"");
    printf(" value=\"%ld\">", depot->AvailableStableStorage);
    printf("<input type=\"hidden\" name=\"VolatileStorage\" value=\"%ld\">", depot->VolatileStorage);
    printf("<input type=\"hidden\" name=\"AvailableVolatileStorage\"");
    printf(" value=\"%ld\">", depot->AvailableVolatileStorage);
    printf("<input type=\"hidden\" name=\"Duration\" value=\"%f\">", depot->Duration);
    printf("<input type=\"hidden\" name=\"status\" value=\"%s\">", depot->status);
    printf("<input type=\"hidden\" name=\"lastUpdate\" value=\"%ld\">", depot->lastUpdate);
    if (depot->country != NULL)
      printf("<input type=\"hidden\" name=\"country\" value=\"%s\">", depot->country);
    if (depot->state != NULL)
      printf("<input type=\"hidden\" name=\"state\" value=\"%s\">", depot->state);
    if (depot->city != NULL)
      printf("<input type=\"hidden\" name=\"city\" value=\"%s\">", depot->city);
    if (depot->zip != NULL)
      printf("<input type=\"hidden\" name=\"zip\" value=\"%s\">", depot->zip);
    if (depot->airport != NULL)
      printf("<input type=\"hidden\" name=\"airport\" value=\"%s\">", depot->airport);
    printf("<input type=\"hidden\" name=\"lat\" value=\"%f\">", depot->lat);
    printf("<input type=\"hidden\" name=\"lon\" value=\"%f\">", depot->lon);
    if (depot->connection != NULL)
      printf("<input type=\"hidden\" name=\"connection\" value=\"%s\">", depot->connection);
    if (depot->monitoring != NULL)
      printf("<input type=\"hidden\" name=\"monitoring\" value=\"%s\">", depot->monitoring);
    if (depot->power != NULL)
      printf("<input type=\"hidden\" name=\"power\" value=\"%s\">", depot->power);
    if (depot->backup != NULL)
      printf("<input type=\"hidden\" name=\"backup\" value=\"%s\">", depot->backup);
    printf("<input type=\"hidden\" name=\"polled\" value=\"%d\">", depot->polled);
    printf("<input type=\"hidden\" name=\"responded\" value=\"%d\">", depot->responded);
    printf("<input type=\"hidden\" name=\"uptime\" value=\"%f\">", depot->uptime);
    if (depot->firewall != NULL)
      printf("<input type=\"hidden\" name=\"firewall\" value=\"%s\">", depot->firewall);
    if (depot->notifyOwnerPeriod != NULL)
      printf("<input type=\"hidden\" name=\"notify\" value=\"%s\">", depot->notifyOwnerPeriod);
    printf("</form></p>\n");

    printf("<p>If this is your depot, you may delete it.\n");
    printf("<form method=\"POST\" action=\"lbone_depot_delete.cgi\">\n");
    printf("<input type=\"submit\" name=\"action\" value=\"delete\">");
    printf("<input type=\"hidden\" name=\"depotname\" value=\"%s\">", depot->depotname);
    printf("</form>\n");
    printf("</p>\n");

    printf("  </td>\n");
    printf("  <td width=\"50%%\" valign=top>\n");
    printf("    <table border=0 cellpadding=6 cellspacing=0 width=\"100%%\">\n");

    printf("      <tr>\n");
    printf("        <td colspan=2 bgcolor=87aec2 align=center><font size=+1 face=\"Arial,Helvetica\">");
    printf("<b>Location Information</b></font></td>\n");
    printf("      </tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1><b>Country</b></font></td>\n");
    if (depot->country != NULL) {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>%s</font></td>\n", depot->country);
    } else {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>not used</font></td>\n");
    }
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\"><font size=-1><b>State</b></font></td>\n");
    if (depot->state != NULL) {
      printf("  <td width=\"50%%\"><font size=-1>%s</font></td>\n", depot->state);
    } else {
      printf("  <td width=\"50%%\"><font size=-1>not used</font></td>\n");
    }
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1><b>City</b></font></td>\n");
    if (depot->city != NULL) {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>%s</font></td>\n", depot->city);
    } else {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>not used</font></td>\n");
    }
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\"><font size=-1><b>Zipcode</b></font></td>\n");
    if (depot->zip != NULL) {
      printf("  <td width=\"50%%\"><font size=-1>%s</font></td>\n", depot->zip);
    } else {
      printf("  <td width=\"50%%\"><font size=-1>not used</font></td>\n");
    }
    printf("</tr>\n");

    printf("<tr>\n");
    printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1><b>Airport</b></font></td>\n");
    if (depot->airport != NULL) {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>%s</font></td>\n", depot->airport);
    } else {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>not used</font></td>\n");
    }
    printf("</tr>\n");
    printf("    </table>\n");

    printf("    <p>&nbsp;</p>\n");

    printf("<table border=0 cellpadding=6 cellspacing=0 width=\"100%%\">\n");
    printf("  <tr>\n");
    printf("    <td colspan=2 bgcolor=87aec2 align=center><font size=+1 face=\"Arial,Helvetica\">");
    printf("<b>Environment Information</b></font></td>\n");
    printf("  </tr>\n");
    printf("  <tr>\n");
    printf("    <td width=\"50%%\" bgcolor=DDDDDD><font size=-1><b>Connection</b></font></td>\n");
    if (depot->connection != NULL) {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>%s</font></td>\n", depot->connection);
    } else {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>not used</font></td>\n");
    }
    printf("  </tr>\n");

    printf("  <tr>\n");
    printf("    <td width=\"50%%\"><font size=-1><b>Monitoring</b></font></td>\n");
    if (depot->monitoring != NULL) {
      printf("  <td width=\"50%%\"><font size=-1>%s</font></td>\n", depot->monitoring);
    } else {
      printf("  <td width=\"50%%\"><font size=-1>not used</font></td>\n");
    }
    printf("  </tr>\n");

    printf("  <tr>\n");
    printf("    <td width=\"50%%\" bgcolor=DDDDDD><font size=-1><b>Power Backup</b></font></td>\n");
    if (depot->power != NULL) {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>%s</font></td>\n", depot->power);
    } else {
      printf("  <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>not used</font></td>\n");
    }
    printf("  </tr>\n");

    printf("  <tr>\n");
    printf("    <td width=\"50%%\"><font size=-1><b>Data Backup</b></font></td>\n");
    if (depot->backup != NULL) {
      printf("  <td width=\"50%%\"><font size=-1>%s</font></td>\n", depot->backup);
    } else {
      printf("  <td width=\"50%%\"><font size=-1>not used</font></td>\n");
    }
    printf("  </tr>\n");

    printf("  <tr>\n");
    printf("    <td width=\"50%%\" bgcolor=DDDDDD><font size=-1><b>Percent Uptime</b></font></td>\n");
    printf("    <td width=\"50%%\" bgcolor=DDDDDD><font size=-1>%3.0f%%</font></td>\n", depot->uptime);
    printf("  </tr>\n");

    printf("    </table>\n");

    printf("    </td>\n");
    printf("  </tr>\n");


    printf("</table>\n");

    html_end();

  } else {

    html_top();
    printf("<p><font size=+2 face=\"Arial,Helvetica\"><b>LBONE Depot Detail View</b></font></p>");
    printf("<p><font face=\"Arial,Helvetica\">No depotname was specified.<br>");
    printf("Please press the Back button to return to the depot list.</font></p>");

    back_button();
    html_end();
  }
/*  ldap_unbind(ldap); */
  return 0;
}






