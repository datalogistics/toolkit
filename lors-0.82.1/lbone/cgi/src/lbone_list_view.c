#include "lbone_cgi_base.h"

extern DepotInfo get_depot_info();
extern void html_top();
extern void html_table_header();
extern void html_end();
extern char *convert2url();
void  lbone_server_list_html();


int main() {

  int       retval = !LDAP_SUCCESS, first, add_color;
  char	   *color, time_str[32], image[128], *alt;
  ulong_t	TotalStable, TotalStableUsed, TotalStableAvail;
  ulong_t	TotalVolatile, TotalVolatileAvail;
  ulong_t	TotalAll, TotalAllAvail;

  long		now, elapsed;
  float		percent;
  DepotInfo	depot;
  JRB		depots, node;
  LDAP          *ldap;
  LDAPMessage   *entry, *result;
  struct timeval	tp;
  IS  is;
  is = new_inputstruct("lbone_server_list.txt");
  if ( is == NULL ) return 1;
  /* attempt to connect to any available ldap server from
   * lbone_server_list.txt file */
  while (get_line(is) >= 0 && retval != LDAP_SUCCESS) 
  {
      if ( is->NF >= 3 )
      {
          ldap = ldap_init (is->fields[0], atoi(is->fields[2]));
          retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
      }
  }
  
  /* 
  ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
  retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
  */
  if (retval != LDAP_SUCCESS) {

    html_top();
    printf("<p><font size=\"+1\" face=\"Arial,Helvetica\">The LDAP server did not respond.<br>");
    printf("It may be busy, the network may be congested or it may be down.</font></p>");
    printf("<p><font size=\"+1\" face=\"Arial,Helvetica\">Please try back in a few minutes.</font></p>");
    printf("<p>&nbsp;</p>");
    printf("<p><font face=\"Arial,Helvetica\">If you have noticed the server down for more");
    printf(" than 10 minutes,<br>please send an email to ");
    printf("<a href=\"mailto:lbone@cs.utk.edu\">L-Bone Support</a>.</font></p>");

    html_end();
    return 0;
  }
  retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, "depotname=*", NULL, 0, &result);

  if ((entry = ldap_first_entry (ldap, result)) != NULL) {
    depots = make_jrb();
    depot = get_depot_info(ldap, entry);
    jrb_insert_str(depots, depot->depotname, new_jval_v((void *) depot));
    while ((entry = ldap_next_entry (ldap, entry)) != NULL) {
      depot = get_depot_info(ldap, entry);
      jrb_insert_str(depots, depot->depotname, new_jval_v((void *) depot));
    }
    ldap_unbind (ldap);
  }
  else {

    ldap_unbind (ldap);
    html_top();
    printf("<p><font size=\"+2\" face=\"Arial,Helvetica\"><b>Depot List View</b></font></p>");
    printf("<p><font size=\"+1\"face=\"Arial,Helvetica\">The LDAP server did not return any depots.</p>");
    printf("<p><font size=\"+1\" face=\"Arial,Helvetica\">Please try back in a few minutes.</font></p>");    printf("<p>&nbsp;</p>");
    printf("<p><font face=\"Arial,Helvetica\">If you have noticed this message for more");
    printf(" than 10 minutes,<br>please send an email to ");
    printf("<a href=\"mailto:lbone@cs.utk.edu\">L-Bone Support</a>.</font></p>");


    printf("<p>Please press the Back button to return to the LBONE homepage.</font></p>");
    printf("<form>\n");
    printf ("<input type=\"button\" value=\"Back to List\" onClick=\"script: location=\'lbone_list_view.cgi\'\">\n");
    printf ("</form>\n");

    html_end();
    return 0;
  }

  first = 1; add_color = 0;
  memset(&tp, 0, sizeof(struct timeval));
  gettimeofday(&tp, NULL);
  now = tp.tv_sec;

  html_top();
  printf("<p><font size=\"+2\" face=\"Arial,Helvetica\"><b>LBONE Server List</b></font></p>");
  lbone_server_list_html();
  printf("<p><font size=\"+2\" face=\"Arial,Helvetica\"><b>LBONE Depot List</b></font></p>");
  printf("<p><font face=\"Arial,Helvetica\">To view a depot in detail, click on the depot name</font></p>");

  html_table_header();
  TotalStable = 0; TotalStableAvail = 0; TotalVolatile = 0; 
  TotalVolatileAvail = 0; TotalStableUsed = 0; percent = 0;
  TotalAll = 0; TotalAllAvail = 0;

  if (jrb_empty (depots) == 0) 
  {
    jrb_traverse(node, depots) 
    {
      if (add_color == 1) color = (char *) strdup("bgcolor=EEEEEE");
      else color = (char *) strdup("bgcolor=FFFFFF");
      depot = (DepotInfo) jval_v(node->val);
      printf ("<tr><td %s>", color);
/*
      printf ("<form method=POST action=\"%s\">\n", "%s/lbone_depot_view.cgi", LBONE_WEB_HOME); 
      printf("<input type=\"submit\" value=\"%s\" width=180>", depot->depotname);
      printf("<input type=\"hidden\" name=\"depotname\" value=\"%s\"></td>", depot->depotname);
*/

      printf("&nbsp;&nbsp;&nbsp;&nbsp;<a href=\"%s/lbone_depot_view.cgi?depotname=", LBONE_WEB_HOME);
      printf("%s&update=no\">%s</a></td>", convert2url(depot->depotname), depot->depotname);


      printf("  <td %s>&nbsp;%s</td>\n", color, depot->hostname);
      printf("  <td align=right %s>&nbsp;%d</td>\n", color, depot->port);
      printf("  <td align=right %s>&nbsp;%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", 
              color, (double)(depot->VolatileStorage)/1024.0);
      printf("  <td align=right %s>&nbsp;%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", 
              color, (double)(depot->AvailableVolatileStorage + 
                     depot->AvailableStableStorage -
                     depot->StableStorage)/1024.0);
      printf("  <td align=right %s>&nbsp;%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", 
              color, min(((double)(depot->AvailableStableStorage)/1024.0), 
                          (double)(depot->AvailableVolatileStorage + 
                                   depot->AvailableStableStorage -
                                   depot->StableStorage)/1024.0));
      if (depot->Duration < 0)
        printf("  <td align=center %s>Indefinite</td>\n", color);
      else
        printf("  <td align=right %s>&nbsp;%10.2f&nbsp;&nbsp;</td>\n", color, depot->Duration);

      memset(image, 0, 128);
      elapsed = now - depot->lastUpdate;
      if (elapsed < 3600) {
        sprintf(image, "%s", "../images/green.gif");
        alt = (char *) strdup("Info is less than 1 hour old");;
      }
      else if ((elapsed > 3600) && (elapsed < 86400)) {
        sprintf(image, "%s", "../images/yellow.gif");
        alt = (char *) strdup("Info is less than 1 day old");;
      }
      else {
        sprintf(image, "%s", "../images/red.gif");
        alt = (char *) strdup("Info is more than 1 day old");;
      }

      memset(time_str, 0, 32);
      if (depot->lastUpdate != 0) 
      {
        strncpy(time_str, ctime((time_t *) &depot->lastUpdate), 
                strlen(ctime((time_t *) &depot->lastUpdate)));
        time_str[strcspn(time_str, "\n")] = '\0';
      } else {
        strncpy(time_str, "Never", 6);
      }
      printf("  <td align=left %s><img src=\"%s\" width=9 height=9 alt=\"%s\" title=\"%s\">", 
              color, image, alt, alt);
      printf("&nbsp;<font size=-1>%s</font></td>\n", time_str);
      printf("  <td align=center %s><font size=-1>&nbsp;", color);
      if (depot->country != NULL) printf("%s ", depot->country);
      if (depot->state != NULL) printf("%s ", depot->state);
      if (depot->city != NULL) printf("%s ", depot->city);
      if (depot->airport != NULL) printf("%s ", depot->airport);
      printf("</font></td>\n");
      printf ("</tr>\n");

      /* to avoid including inactive depots in the storage total, this
       * summation is conditional upon an active rating. */
      if ( elapsed < 3600 )
      {
          TotalAll           += depot->VolatileStorage;
          TotalAllAvail      += depot->AvailableVolatileStorage + 
                                depot->AvailableStableStorage   - 
                                depot->StableStorage;
          TotalVolatile      += depot->VolatileStorage;
          TotalVolatileAvail += depot->AvailableVolatileStorage;
          TotalStable        += depot->StableStorage;
          TotalStableAvail   += min((depot->AvailableStableStorage), 
                                    (depot->AvailableVolatileStorage + 
                                     depot->AvailableStableStorage -
                                     depot->StableStorage));
          TotalStableUsed    += depot->StableStorage -
                                depot->AvailableStableStorage;
      }
      add_color = (add_color + 1) % 2;
      free(color);
    }
    printf("<tr>");
    printf("  <td colspan=9 bgcolor=CCCCCC>&nbsp;</td>\n");
    printf("</tr>");
    /*
    printf("<tr>");
    printf("  <td bgcolor=CCCCCC>&nbsp;</td>\n");
    printf("  <td bgcolor=CCCCCC>&nbsp;<b>Total Available</b></td>\n");
    printf("  <td align=right bgcolor=CCCCCC>&nbsp;</td>\n");
    printf("  <td align=right bgcolor=CCCCCC>&nbsp;<b>%ld</b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", TotalVolatileAvail);
    printf("  <td align=right bgcolor=CCCCCC>&nbsp;<b>%ld</b>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", TotalStableAvail);
    printf("  <td align=right bgcolor=CCCCCC>&nbsp;</td>\n");
    printf("  <td align=right bgcolor=CCCCCC>&nbsp;</td>\n");
    printf("  <td align=right bgcolor=CCCCCC>&nbsp;</td>\n");
    printf("</tr>");
    */
  }
  printf("</table>");

  printf("<table border=0 cellpadding=2 cellspacing=0>\n");
  printf("<tr>\n");
  printf("  <td>");
  printf("    <img src=\"../images/green.gif\" width=9 height=9 alt=\"Info is less than 1 hour old\" title=\"Info is less than 1 hour old\">");
  printf("&nbsp;&nbsp;Info is less than 1 hour old");
  printf("&nbsp;&nbsp;&nbsp;&nbsp;<img src=\"../images/yellow.gif\" width=9 height=9 alt=\"Info is less than 1 day old\" title=\"Info is less than 1 day old\">");
  printf("&nbsp;&nbsp;Info is less than 1 day old");
  printf("&nbsp;&nbsp;&nbsp;&nbsp;<img src=\"../images/red.gif\" width=9 height=9 alt=\"Info is more than 1 day old\" title=\"Info is more than 1 day old\">");
  printf("&nbsp;&nbsp;Info is more than 1 day old");
  printf("  </td>\n");
  printf("</tr>\n");
  printf("</table>");

  printf("<p>&nbsp;</p>");

  printf("<table border=0 cellpadding=2 cellspacing=0 width=500>\n");
  printf("<tr><td colspan=4><center>Total Storage Available to the LBone</center></td></tr>\n");
  printf("<tr>");
  printf("  <td nowrap bgcolor=87aec2 width=\"25%%\" align=center><b>Type</b></td>\n");
  printf("  <td nowrap bgcolor=87aec2 width=\"25%%\" align=center><b>Total<br>(GBs)</b></td>\n");
  printf("  <td nowrap bgcolor=87aec2 width=\"25%%\" align=center><b>Available<br>(GBs)</b></td>\n");
  printf("  <td nowrap bgcolor=87aec2 width=\"25%%\" align=center><b>Used<br>(GBs)</b></td>\n");
  printf("  <td bgcolor=87aec2 width=\"25%%\" align=center><b>Percent Available</b></td>\n");
  printf("</tr>");
  printf("<tr>");
  printf("  <td nowrap bgcolor=EEEEEE width=\"25%%\">Soft Storage</td>\n");
  printf("  <td bgcolor=EEEEEE align=right width=\"25%%\">%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", (double)(TotalAll)/1024.0);

  printf("  <td bgcolor=EEEEEE align=right width=\"25%%\">%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", (double)(TotalAllAvail)/1024.0);

  printf("  <td bgcolor=EEEEEE align=right width=\"25%%\">%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", (double)(TotalAll - TotalAllAvail)/1024.0);

  percent = ((float) TotalAllAvail / (float) TotalAll) * 100;
  printf(" <td bgcolor=EEEEEE align=right width=\"25%%\">%3.0f%%&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
          , percent);
  printf("</tr>");
  /*
  printf("<tr>");
  printf("  <td width=\"25%%\">Soft Storage</td>\n");
  printf("  <td align=right width=\"25%%\">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", 
          TotalVolatile);
  printf("  <td align=right width=\"25%%\">%ld&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", 
          TotalVolatileAvail);
  percent = ((float) TotalVolatileAvail / (float) TotalVolatile) * 100;
  printf(" <td align=right width=\"25%%\">%3.0f%%&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n"
          , percent);
  printf("</tr>");
  */
  printf("<tr>");
  printf("  <td nowrap bgcolor=EEEEEE width=\"25%%\">Hard Storage</td>\n");
  printf("  <td align=right bgcolor=EEEEEE width=\"25%%\">%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", (double)(TotalStable)/1024.0);
  printf("  <td align=right bgcolor=EEEEEE width=\"25%%\">%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", (double)(TotalStableAvail)/1024.0);
  printf("  <td align=right bgcolor=EEEEEE width=\"25%%\">%0.2f&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", (double)(TotalStableUsed)/1024.0);
  percent = ((float) TotalStableAvail / (float) TotalStable) * 100;
  printf("  <td align=right bgcolor=EEEEEE width=\"25%%\">%3.0f%%&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</td>\n", 
          percent);
  printf("</tr>");
  printf("</table>");
/** BEGIN LEGEND **/
  printf("<table border=0 cellpadding=2 cellspacing=0 width=500>\n");
  /*printf("<tr><td colspan=4><center>Total Storage Available 
    to the LBone</center></td></tr>\n");*/
  printf("<br>\n");
  printf("<br>\n");
  printf("<br>\n");
  printf("<tr colspan=4>");
  printf("  <th bgcolor=87aec2 align=center colspan=4><b>LBone List View Legend</b></th>\n");
  printf("</tr>");
  printf("<tr>");
  printf("  <th align=top width=\"25%%\">Depot Name</th>\n");
  printf("  <td align=left width=\"100%%\">When a user enters a new IBP depot into\n"
         "the LBone, she is given the option of assigning a unique name.</td>\n");
  printf("</tr>");
  printf("<tr bgcolor=EEEEEE >");
  printf("  <th align=top width=\"25%%\">Hostname</th>\n");
  printf("  <td align=left width=\"100%%\">Each IBP depot is accessable through the\n"
         "internet, and thus, has a unique IP address.  The name bound via DNS to this\n"
         "IP address is the 'hostname.'  It is a simpler way to refer to Depots than\n"
         "by IP address.</td>\n");
  printf("</tr>");
  printf("<tr>");
  printf("  <th align=top width=\"25%%\">Port</th>\n");
  printf("  <td align=left width=\"100%%\">If 'hostname' maps to an address, then 'ports' map to entrances.  The standard IBP port is 6714, however the server may be configured to serve on any Port.</td>\n");
  printf("</tr>");
  printf("<tr bgcolor=EEEEEE >");
  printf("  <th align=top width=\"25%%\">Total Depot Storage</th>\n");
  printf("  <td align=left width=\"100%%\">This is the total storage exposed by a depot.  By default, this amount is known as Soft.  Soft storage is not guaranteed by the depot.  The OS could reclaim the space at any time.  The policy for how Soft storage is reclaimed is dictated by individual site management.  </td>\n");
  printf("</tr>");
  printf("<tr>");
  printf("  <th align=top width=\"25%%\">Available Soft Storage</th>\n");
  printf("  <td align=left width=\"100%%\">Of the Total Depot Storage, some will have been allocated by various users.  This column is the maximum possible space remaining for Soft allocations.  <br>SoftAvailable = TotalSize - SoftAllocated - HardAllocated.</td>\n");
  printf("</tr>");
  printf("<tr bgcolor=EEEEEE>");
  printf("  <th align=top width=\"25%%\">Available Hard Storage</th>\n");
  printf("  <td align=left width=\"100%%\">A certain amount of the Total Depot Storage can be Reserved by the site administrator with stronger guarantees than that provided by Soft storage.  Hard storage is guaranteed until the read reference count is zero or the expiration has passed.  The amount of Hard storage exposed by a depot is less than or equal to the Total Depot Storage.<BR>\n"
          "HardAvailable = MIN(SoftAvailable, TotalReservable-HardAllocated)</td>\n");
  printf("</tr>");
  printf("<tr >");
  printf("  <th align=top width=\"25%%\">Days Available</th>\n");
  printf("  <td align=left width=\"100%%\">The maximum time allowed for any one allocation.</td>\n");
  printf("</tr>");
  printf("<tr bgcolor=EEEEEE>");
  printf("  <th align=top width=\"25%%\">Gathered On</th>\n");
  printf("  <td align=left width=\"100%%\">The lbone periodically polls the IBP depots of which it has a record.  This is how the 'Available' statistics are gathered.  In the even that everything is fine, a greed dot will appear as well as the date recorded.  The yellow and red dot represent increasingly longer periods without a response from the Depot.</td>\n");
  printf("</tr>");
  printf("<tr>");
  printf("  <th align=top width=\"25%%\">Location</th>\n");
  printf("  <td align=left width=\"100%%\">A geographical location as specified by the maintainer of this IBP depot.</td>\n");
  printf("</tr>");
  printf("</table>");
/** END LEGENT **/
  
  html_end();
  /*
  printf ("<p>&nbsp;</p>\n");
  printf ("<p><a href=\"http://www.utk.edu\" title=\"UTK home\"><img src=\"../images/utk.gif\" width=\"115\" height=\"63\" alt=\"UTK home\" title=\"UTK home\" border=0></a>&nbsp;&nbsp;&nbsp;&nbsp;");
  printf ("<a href=\"%s\" title=\"LoCI Lab home\">", LBONE_WEB_ROOT);
  printf ("<img src=\"../images/locifooter.jpg\" width=200 height=63 alt=\"LoCI Lab\" ");
  printf (" title=\"LoCI Lab\" border=0></a>&nbsp;&nbsp;&nbsp;&nbsp;");
  printf ("<a href=\"http://www.energy.gov\" title=\"DOE\"><img src=\"../images/doe.gif\" ");
  printf ("width=140 height=75 alt=\"DOE\" title=\"DOE\" border=0></a><a ");
  printf ("href=\"http://www.nsf.gov\" title=\"NSF\"><img src=\"../images/nsf.gif\" ");
  printf ("width=140 height=75 alt=\"NSF\" title=\"NSF\" border=0></a></p>");
  printf ("</center></body></html>\n");
  */

  return 0;
}

void  lbone_server_list_html()
{
    IS  is;
    is = new_inputstruct("lbone_server_list.txt");
    if ( is == NULL ) return;
    printf ("<center><table border=0 cellpadding=2 cellspacing=0 width=\"50%%\">\n");
    printf ("  <tr>\n");
    printf ("    <th width=\"50%%\" bgcolor=87aec2><b>&nbsp;Hostname</b></td>\n");
    printf ("    <th width=\"50%%\" bgcolor=87aec2 align=center><b>Port</b></td>\n");
    printf ("  </tr>\n");
    while (get_line(is) >= 0) 
    {
        if ( is->NF >= 2 )
        {
            printf ("<tr>\n");
            printf (" <td width=\"50%%\" >%s</td>\n", is->fields[0]);
            printf (" <td width=\"50%%\" align=center>%s</td>\n", is->fields[1]);
            printf ("</tr>\n");
        }
    }
    printf("</table></center>\n");
    return;
}

/*
 * Table header
 */

void html_table_header() {
  printf ("<table border=0 cellpadding=2 cellspacing=0 width=\"100%%\">\n");
  printf ("  <tr>\n");
  printf ("    <td width=\"20%%\" bgcolor=87aec2><b>&nbsp;&nbsp;&nbsp;&nbsp;Depot Name</b></td>\n");
  printf ("    <td width=\"10%%\" bgcolor=87aec2><b>&nbsp;Hostname</b></td>\n");
  printf ("    <td width=\"5%%\" bgcolor=87aec2 align=center><b>Port</b></td>\n");
  printf ("    <td align=center width=\"10%%\" bgcolor=87aec2><b>Total Depot Storage<br>(GBs)</b></td>\n");
  printf ("    <td align=center width=\"10%%\" bgcolor=87aec2><b>Available Soft Storage<br>(GBs)</b></td>\n");
  printf ("    <td align=center width=\"10%%\" bgcolor=87aec2><b>Available Hard Storage<br>(GBs)</b></td>\n");
  printf ("    <td align=center width=\"5%%\" bgcolor=87aec2><b>Days Available</b></td>\n");
  printf ("    <td align=center width=\"10%%\" bgcolor=87aec2><b>Information Gathered On</b></td>\n");
  printf ("    <td align=center width=\"10%%\" bgcolor=87aec2><b>Location</b></td>\n");
  printf ("  </tr>\n");
}

