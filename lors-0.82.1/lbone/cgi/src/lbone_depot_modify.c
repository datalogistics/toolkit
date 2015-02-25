#include <stdlib.h>
#include <string.h>
#include "lbone_cgi_base.h"
#include "lbone_server.h"
#include "lbone_poll.h"
#include "ecgi.h"

extern DepotInfo get_depot_info();
extern void html_top();
extern void html_end();
extern int call_depot();
extern DepotInfo fill_with_blanks();
extern DepotInfo fill_with_values();
extern int display_form();
extern int check_depot_lb();
extern int add_error();
extern int add_depot();
extern int modify_depot();
extern int get_latlon();
extern int replace_attr();
extern int delete_attr();
extern void back_button();
void trim_trailing_spaces();
void trim_leading_spaces();
extern char **getLatLong ();
extern int check_hostname();

char	*action, *previous;
char	errors[4096];
int	ready;
char	*nbsp = "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;";

int main() {
  DepotInfo	depot;

  depot = NULL;

  if (cgiInit() != ECGI_SUCCESS) {
    printf("ERROR: %s\n", cgiStrError(cgi_errno));
    exit(cgi_errno);
  }

  action = NULL;
  previous = NULL;
  memset(errors, 0, 4096);
  ready = 1;

  if ((action = (char *) cgiParam ("action")) == NULL) {
    /* print error, return to list view */
  }

  if (strcmp(action, "add") == 0) {

    /*
     *  strdup variables with single space
     *  display form
     */

    previous = (char *) strdup(" ");
    depot = fill_with_blanks();
    display_form(depot);

  }
  else if (strcmp(action, "modify") == 0) {

    /*
     *  strdup variables with cgi parameters
     *  display form
     */

    previous = (char *) strdup(" ");
    depot = fill_with_values();
    display_form(depot);


  }
  else if (strcmp(action, "submit") == 0) {

    if ((previous = (char *) cgiParam ("previous")) == NULL) {
      /* print error, return to list view */
    }

    /*
     *  check values
     *  if good, update ldap
     *    display results
     *    add buttons to depot view and list view
     *  if bad, redisplay values
     *    highlight errors
     */

    depot = fill_with_values();
    check_depot_lb(depot);
    if ((ready == 1) && (previous[0] == 'a')) add_depot(depot);
    else if ((ready == 1) && (previous[0] == 'm')) modify_depot(depot); 
    display_form(depot);

  }
  else {
    /* print error, 
       if modify, return to depot view
       else if add, return to list view */
  }

  return 0;
}


DepotInfo fill_with_blanks() {
  DepotInfo	depot;

  depot = (DepotInfo) malloc(sizeof(struct depot_info));
  if (depot == NULL) {
    /* malloc error, return to list view */
  }
  memset(depot,0,sizeof(struct depot_info));

  depot->depotname = (char *) strdup("");
  depot->hostname = (char *) strdup("");
  depot->port = 6714;
  depot->StableStorage = 0;
  depot->AvailableStableStorage = 0;
  depot->VolatileStorage = 0;
  depot->AvailableVolatileStorage = 0;
  depot->Duration = 0;
  depot->status = (char *) strdup("unavailable");
  depot->lastUpdate = 0;
  depot->country = (char *) strdup("");
  depot->state = (char *) strdup("");
  depot->city = (char *) strdup("");
  depot->zip = (char *) strdup("");
  depot->airport = (char *) strdup("");
  depot->lat = 0;
  depot->lon = 0;
  depot->email1 = (char *) strdup("");
  depot->email2 = (char *) strdup("");
  depot->email3 = (char *) strdup("");
  depot->connection = (char *) strdup("");
  depot->monitoring = (char *) strdup("");
  depot->power = (char *) strdup("");
  depot->backup = (char *) strdup("");
  depot->polled = 0;
  depot->responded = 0;
  depot->uptime = 0;
  depot->firewall = (char *) strdup("");
  depot->notifyOwnerPeriod = (char *) strdup("");

  return depot;
}




DepotInfo fill_with_values() {
  char		*temp;
  char		**vals, *filter;
  int		retval, size;
  DepotInfo	depot;
  LDAP		*ldap;
  LDAPMessage	*result, *entry;

  depot = (DepotInfo) calloc(1, sizeof(struct depot_info));
  if (depot == NULL) {
    /* malloc error, return to list view */
  }


  if ((depot->old_depotname = (char *) cgiParam ("old_depotname")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->old_depotname);

  if ((depot->hostname = (char *) cgiParam ("hostname")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->hostname);

  if ((depot->depotname = (char *) cgiParam ("depotname")) == NULL) {
      depot->depotname = strdup(depot->hostname);
  }
  trim_trailing_spaces(depot->depotname);

  if (strcmp(depot->depotname, "") == 0)
  {
      depot->depotname = (char *) strdup(depot->hostname);
  }

  if ((temp = (char *) cgiParam ("port")) == NULL) {
    /* print error, return to list view */
  }
  depot->port = atoi(temp);

  if ((temp = (char *) cgiParam ("StableStorage")) == NULL) {
    /* print error, return to list view */
  }
  depot->StableStorage = strtoul(temp, NULL, 0);

  if ((temp = (char *) cgiParam ("AvailableStableStorage")) == NULL) {
    /* print error, return to list view */
  }
  depot->AvailableStableStorage = strtoul(temp, NULL, 0);

  if ((temp = (char *) cgiParam ("VolatileStorage")) == NULL) {
    /* print error, return to list view */
  }
  depot->VolatileStorage = strtoul(temp, NULL, 0);
  if ((temp = (char *) cgiParam ("AvailableVolatileStorage")) == NULL) {
    /* print error, return to list view */
  }
  depot->AvailableVolatileStorage = strtoul(temp, NULL, 0);

  if ((temp = (char *) cgiParam ("Duration")) == NULL) {
    /* print error, return to list view */
  }
  depot->Duration = atof(temp);

  if ((depot->status = (char *) cgiParam ("status")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->status);

  if ((temp = (char *) cgiParam ("AvailableVolatileStorage")) == NULL) {
    /* print error, return to list view */
  }
  depot->lastUpdate = strtoul(temp, NULL, 0);

  if ((depot->country = (char *) cgiParam ("country")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->country);

  if ((depot->state = (char *) cgiParam ("state")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->state);


  if ((depot->city = (char *) cgiParam ("city")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->city);

  if ((depot->zip = (char *) cgiParam ("zip")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->zip);

  if ((depot->airport = (char *) cgiParam ("airport")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->airport);

  if ((temp = (char *) cgiParam ("lat")) == NULL) {
    /* print error, return to list view */
  }
  depot->lat = atof(temp);

  if ((temp = (char *) cgiParam ("lon")) == NULL) {
    /* print error, return to list view */
  }
  depot->lon = atof(temp);


  ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
  retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
  if (retval == LDAP_SUCCESS) {

    size = strlen(depot->depotname) + strlen("depotname=") + 1;
    filter = (char *) malloc(size);
    memset(filter, 0, size);
    if (depot->old_depotname == NULL)
      sprintf(filter, "%s%s", "depotname=", depot->depotname);
    else
      sprintf(filter, "%s%s", "depotname=", depot->old_depotname);

    retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
    if (retval == LDAP_SUCCESS) {
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        vals = ldap_get_values(ldap, entry, "email");
        if (ldap_count_values(vals) > 0) depot->email1 = (char *) strdup(vals[0]);
        ldap_value_free(vals);
      }
    }
  }
  ldap_unbind(ldap);


  if ((depot->email2 = (char *) cgiParam ("email2")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->email2);

  if ((depot->email3 = (char *) cgiParam ("email3")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->email3);

  if ((depot->connection = (char *) cgiParam ("connection")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->connection);

  if ((depot->monitoring = (char *) cgiParam ("monitoring")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->monitoring);

  if ((depot->power = (char *) cgiParam ("power")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->power);

  if ((depot->backup = (char *) cgiParam ("backup")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->backup);

  if ((depot->firewall = (char *) cgiParam ("firewall")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->firewall);

  if ((depot->notifyOwnerPeriod = (char *) cgiParam ("notify")) == NULL) {
    /* print error, return to list view */
  }
  trim_trailing_spaces(depot->notifyOwnerPeriod);

  return depot;
}



int display_form(DepotInfo depot) {
  char	*selected = "selected";
  char	*space = " ";
  char	*message;

  html_top();

  if ((action[0] == 's') && (ready == 0)) {
    printf("<p><font color=FF0000 face=\"Arial,Helvetica\" size=+1>");
    printf("<b>Please correct these errors:</b></font></p>\n");
    printf("<p><font color=FF0000 face=\"Arial,Helvetica\"><b>");
    printf("%s</b></font></p>\n", errors);
  }
  else if ((action[0] == 's') && (ready == 1)) {
    if (previous[0] == 'a')
      printf("<p><font face=\"Arial,Helvetica\" size=+1>Depot added successfully!</face></p>");
    else if (previous[0] == 'm')
      printf("<p><font face=\"Arial,Helvetica\" size=+1>Depot modified successfully!</face></p>");

    back_button();
    html_end();
    return 0;
  }

  printf("</center>\n");
  printf("<p>&nbsp;</p>\n");
  printf("<table border=0 width=100%% cellspacing=0 cellpadding=3>\n");
  printf("  <tr>\n");
  printf("    <td width=100%% bgcolor=87aec2><font face=\"Arial,Helvetica\">");
  printf("<b>Contact Info</b></font></td>\n");
  printf("  </tr>\n");
  printf("</table>\n");

  if (action[0] == 'a') {
    printf("<p>Please enter your email address. We will use this as your password whenever ");
    printf("you want to modify information about your depot or to remove it from the LBone ");
    printf("directory. If you want us to notify you when your depot is down, please choose ");
    printf("a notification time frame.<br><small><i>We will not send you any email other ");
    printf("than depot notifications nor will we share this address with any other parties.");
    printf("</i></small></p>\n");
  }
  else {
    printf("<p>Please enter your email address to verify this is your depot</p>");
  }

  printf("<form method=\"POST\" action=\"%s/lbone_depot_modify.cgi\">\n", LBONE_WEB_HOME);
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Email Address");
  printf("<font color=FF0000> (required)</font></td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"email2\" ");
  printf("          size=45 tabindex=1> </td>\n    </tr>\n");
  printf("    <tr>\n");
  if ((action[0] == 'a') || (previous[0] == 'a')) {
    printf("      <td width=33%%>&nbsp;Confirm your Email Address");
  printf("<font color=FF0000> (required)</font></td>\n");
    printf("      <td width=67%%><input type=\"text\" name=\"email3\" size=45 tabindex=2> </td>\n");
  }
  else {
    printf("      <td width=33%%>&nbsp;</td>\n");
    printf("      <td width=67%%>");
/*    printf("      <td width=67%%><input type=\"hidden\" name=\"email1\""); */
/*    printf(" value=\"%s\">", depot->email1); */
    printf("&nbsp;</td>\n");
  }
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Notify Time Frame");
  printf("<font color=FF0000> (required)</font></td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><select name=\"notify\" size=1>\n");
  if (action[0] == 'a') message = selected;
  else if (strcmp(depot->notifyOwnerPeriod, "never") == 0) message = selected;
  else message = space;
  printf("        <option %s value=\"never\">Do Not Notify Me</option>\n", message);
  if (strcmp(depot->notifyOwnerPeriod, "6 hours") == 0) message = selected;
  else message = space;
  printf("        <option %s value=\"6 hours\">More than 6 hours</option>\n", message);
  if (strcmp(depot->notifyOwnerPeriod, "12 hours") == 0) message = selected;
  else message = space;
  printf("        <option %s value=\"12 hours\">More than 12 hours</option>\n", message);
  if (strcmp(depot->notifyOwnerPeriod, "1 day") == 0) message = selected;
  else message = space;
  printf("        <option selected %s value=\"1 day\">More than 1 day</option>\n", message);
  if (strcmp(depot->notifyOwnerPeriod, "3 days") == 0) message = selected;
  else message = space;
  printf("        <option %s value=\"3 days\">More than 3 days</option>\n", message);
  if (strcmp(depot->notifyOwnerPeriod, "1 week") == 0) message = selected;
  else message = space;
  printf("        <option %s value=\"1 week\">More than 1 week</option>\n", message);
  printf("      </select> </td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");

  printf("  <p>&nbsp;</p>\n");
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=3>\n");
  printf("    <tr>\n");
  printf("      <td width=100%% bgcolor=87aec2><font face=\"Arial,Helvetica\">");
  printf("<b>Depot Hostname and Port</b></font></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");

  printf("  <p>Please enter your depot's hostname and port. If your hostname is an IP address");
  printf(" only, add a depotname (such as Big Orange).</p>\n");
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Hostname");
  printf("<font color=FF0000> (required)</font></td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"hostname\" size=45");
  printf(" tabindex=3 value=\"%s\"></td>\n", depot->hostname);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Port <font color=FF0000>(required)</font></td>\n");
  printf("      <td width=67%%><input type=\"text\" name=\"port\" size=45 tabindex=4");
  printf(" value=%d></td>\n", depot->port);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Depotname</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"depotname\" size=45");
  printf(" tabindex=5 value=\"%s\"></td>\n", depot->depotname);
  printf("    </tr>\n");
  printf("  </table>\n");

  printf("  <p>&nbsp;</p>\n");
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=3>\n");
  printf("    <tr>\n");
  printf("      <td width=100%% bgcolor=87aec2><font face=\"Arial,Helvetica\">");
  printf("<b>Depot Storage Characteristics</b></font></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");

  if ((action[0] == 'a') || (previous[0] == 'a')) {
    printf("  <p>If your depot is running and reachable from our server, we can fill in the ");
    printf("storage information automatically. If it is not running and reachable, please ");
    printf("enter your depot's storage information.</p>\n");
  }
  else {
    printf("  <p>Displaying cached storage information</p>\n");
  }
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Hard Storage (MBs)</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"StableStorage\" size=45 ");
  printf("tabindex=7 value=\"%ld\"></td>\n", depot->StableStorage);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Available Hard Storage (MBs)</td>\n");
  printf("      <td width=67%%><input type=\"text\" name=\"AvailableStableStorage\" size=45 ");
  printf("tabindex=8 value=\"%ld\"></td>\n", depot->AvailableStableStorage);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Soft Storage (MBs)</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"VolatileStorage\" size=45");
  printf(" tabindex=9 value=\"%ld\"></td>\n", depot->VolatileStorage);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Available Soft Storage (MBs)</td>\n");
  printf("      <td width=67%%><input type=\"text\" name=\"AvailableVolatileStorage\" size=45 ");
  printf("tabindex=10 value=\"%ld\"></td>\n", depot->AvailableVolatileStorage);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Duration (max allocation days)</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"Duration\" size=45");
  printf(" tabindex=11 value=\"%.3f\"><small>&nbsp;&nbsp;", depot->Duration);
  printf("Use -1 for indefinite</small></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");

  printf("  <p>&nbsp; </p>\n");
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=3>\n");
  printf("    <tr>\n");
  printf("      <td width=100%% bgcolor=87aec2><font face=\"Arial,Helvetica\">");
  printf("<b>Depot Location Information</b></font></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");

  printf("  <p>Many times, people using IBP depots want to position data near to a");
  printf(" certain location. Please enter as much location data below as you can.");
  printf(" The location fields are used to determine latitude and longitude. The fields");
  printf(" are used in the following order: lat/lon, US zipcode, airport code, city/state");
  printf(" then state only for US depots and city/country then country for non-US depots.</p>\n");
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Latitude (decimal format, not minutes)</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"lat\" size=45");
  printf(" tabindex=12 value=\"%f\"></td>\n", depot->lat);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Longitude (decimal format, not minutes)</td>\n");
  printf("      <td width=67%%><input type=\"text\" name=\"lon\" size=45 tabindex=13");
  printf(" value=\"%f\"></td>\n", depot->lon);
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Country (2-letter iso code)</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"country\" size=45");
  if (depot->country != NULL)
    printf(" tabindex=14 value=\"%s\"></td>\n", depot->country);
  else
    printf(" tabindex=14></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;State (US only - use 2-letter postal code)</td>\n");
  printf("      <td width=67%%><input type=\"text\" name=\"state\" size=45 tabindex=15");
  if (depot->state != NULL)
    printf(" value=\"%s\"></td>\n", depot->state);
  else
    printf("></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;City</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"city\" size=45");
  if (depot->city != NULL)
    printf(" tabindex=16 value=\"%s\"></td>\n", depot->city);
  else
    printf(" tabindex=16></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Zipcode (US only - 5 digit)</td>\n");
  printf("      <td width=67%%><input type=\"text\" name=\"zip\" size=45 tabindex=17");
  if (depot->zip != NULL)
    printf(" value=\"%s\"></td>\n", depot->zip);
  else
    printf("></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Airport Code (3-letter IATA code)</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"airport\" size=45");
  if (depot->airport != NULL)
    printf(" tabindex=18 value=\"%s\"></td>\n", depot->airport);
  else
    printf(" tabindex=18></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");
  printf("  <p>&nbsp;</p>\n");


  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=3>\n");
  printf("    <tr>\n");
  printf("      <td width=100%% bgcolor=87aec2><font face=\"Arial,Helvetica\">");
  printf("<b>Depot Environment Information</b></font></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");
  printf("  <p>Please describe the environment that your depot enjoys. Some users may want");
  printf(" to specify locations that have certain levels of service.</p>\n");
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Connection Type</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><select name=\"connection\" size=1>\n");
  if ((depot->connection == NULL) || (depot->connection[0] == '\0')) message = selected;
  else message = space;
  printf("        <option value=\"\" %s>", message);
  printf("%s%s%schoose one%s%s%s</option>\n", nbsp, nbsp, nbsp, nbsp, nbsp, nbsp);
  if ((depot->connection != NULL) && strcmp(depot->connection, "T3") == 0) message = selected;
  else message = space;
  printf("        <option value=\"T3\" %s>T3 or better</option>\n", message);
  if ((depot->connection != NULL) && strcmp(depot->connection, "T1") == 0) message = selected;
  else message = space;
  printf("        <option value=\"T1\" %s>T1</option>\n", message);
  if ((depot->connection != NULL) && strcmp(depot->connection, "Cable/DSL") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Cable/DSL\" %s>Cable/DSL</option>\n", message);
  if ((depot->connection != NULL) && strcmp(depot->connection, "ISDN/56K") == 0) message = selected;
  else message = space;
  printf("        <option value=\"ISDN/56K\" %s>ISDN/56K</option>\n", message);
  printf("      </select></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Machine Monitoring Frequency</td>\n");
  printf("      <td width=67%%><select name=\"monitoring\" size=1>\n");
  if ((depot->monitoring == NULL) || (depot->monitoring[0] == '\0')) message = selected;
  else message = space;
  printf("        <option value=\"\" %s>", message);
  printf("%s%s%schoose one%s%s%s</option>\n", nbsp, nbsp, nbsp, nbsp, nbsp, nbsp);
  if ((depot->monitoring != NULL) && strcmp(depot->monitoring, "24-7") == 0) message = selected;
  else message = space;
  printf("        <option value=\"24-7\" %s>24 hours/7 days per week</option>\n", message);
  if ((depot->monitoring != NULL) && strcmp(depot->monitoring, "9-5") == 0) message = selected;
  else message = space;
  printf("        <option value=\"9-5\" %s>Business hours (9am - 5pm)</option>\n", message);
  if ((depot->monitoring != NULL) && strcmp(depot->monitoring, "Occasional") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Occasional\" %s>It is only monitored while I use", message);
  printf(" it</option>\n");
  printf("      </select></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Power Backup Availabiity</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><select name=\"power\" size=1>\n");
  if ((depot->power == NULL) || (depot->power[0] == '\0')) message = selected;
  else message = space;
  printf("        <option value=\"\" %s>", message);
  printf("%s%s%schoose one%s%s%s</option>\n", nbsp, nbsp, nbsp, nbsp, nbsp, nbsp);
  if ((depot->power != NULL) && strcmp(depot->power, "Generator") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Generator\" %s>Generator or other indefinite", message);
  printf(" supply</option>\n");
  if ((depot->power != NULL) && strcmp(depot->power, "Hours") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Hours\" %s>Battery backup can run for hours</option>\n", message);
  if ((depot->power != NULL) && strcmp(depot->power, "Minutes") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Minutes\" %s>Battery backup can run for", message);
  printf(" minutes</option>\n");
  if ((depot->power != NULL) && strcmp(depot->power, "None") == 0) message = selected;
  else message = space;
  printf("        <option value=\"None\" %s>No backup power</option>\n", message);
  printf("      </select></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;Data Backup Frequency</td>\n");
  printf("      <td width=67%%><select name=\"backup\" size=1>\n");
  if ((depot->backup == NULL) || (depot->backup[0] == '\0')) message = selected;
  else message = space;
  printf("        <option value=\"\" %s>", message);
  printf("%s%s%schoose one%s%s%s</option>\n", nbsp, nbsp, nbsp, nbsp, nbsp, nbsp);
  if ((depot->backup != NULL) && strcmp(depot->backup, "Daily-Multiple Copies") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Daily-Multiple Copies\" %s>Daily backups w/multiple", message);
  printf(" copies kept for days</option>\n");
  if ((depot->backup != NULL) && strcmp(depot->backup, "Daily-Single Copy") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Daily-Single Copy\" %s>Daily backup using same copy", message);
  printf("</option>\n");
  if ((depot->backup != NULL) && strcmp(depot->backup, "Occasional") == 0) message = selected;
  else message = space;
  printf("        <option value=\"Occasional\" %s>Occasionally I back it up</option>\n", message);
  if ((depot->backup != NULL) && strcmp(depot->backup, "None") == 0) message = selected;
  else message = space;
  printf("        <option value=\"None\" %s>None</option>\n", message);
  printf("      </select></td>\n");
  printf("    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Behind Firewall</td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><select name=\"firewall\" size=1>\n");
  if ((depot->firewall == NULL) || (depot->firewall[0] == '\0')) message = selected;
  else message = space;
  printf("        <option value=\"\" %s>", message);
  printf("%s%s%schoose one%s%s%s</option>\n", nbsp, nbsp, nbsp, nbsp, nbsp, nbsp);
  if ((depot->firewall != NULL) && strcmp(depot->firewall, "no") == 0) message = selected;
  else message = space;
  printf("        <option value=\"no\" %s>No</option>\n", message);
  if ((depot->firewall != NULL) && strcmp(depot->firewall, "yes") == 0) message = selected;
  else message = space;
  printf("        <option value=\"yes\" %s>Yes</option>\n", message);
  printf("      </select></td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");
  printf("  <div align=\"center\"><center><p><input type=\"submit\" value=\"Submit\"");
  printf(" name=\"submit\"> <input type=\"reset\" value=\"Reset\" name=\"reset\"></p>\n");
  printf("  </center></div>\n");
  printf("  <input type=\"hidden\" name=\"status\" value=\"%s\">\n", depot->status);
  printf("  <input type=\"hidden\" name=\"action\" value=\"submit\">\n");
  if (action[0] == 'm')
    printf("  <input type=\"hidden\" name=\"old_depotname\" value=\"%s\">\n", depot->depotname);
  else if (previous[0] == 'm')
    printf("  <input type=\"hidden\" name=\"old_depotname\" value=\"%s\">\n", depot->old_depotname);
  if ((action[0] == 'a') || (previous[0] == 'a'))
    printf("  <input type=\"hidden\" name=\"previous\" value=\"add\">\n");
  else  /* action == 'm' || previous == 'm' */
    printf("  <input type=\"hidden\" name=\"previous\" value=\"modify\">\n");
  printf("</form>\n");

  html_end();
  return 0;
}






int check_depot_lb(DepotInfo depot) {

  /* check for correct email addresses */

  /* if adding depot, addresses must match and not be empty */
  if ((action[0] == 'a') || (previous[0] == 'a')) {
    if (depot->email2[0] == '\0') 
      add_error("The first email line was left blank. Please enter your email address.<br>");
    if (depot->email3[0] == '\0')
      add_error("The second email line was left blank. Please confirm your email address.<br>");
    if (strcmp(depot->email2, depot->email3) != 0)
      add_error("The two email addresses do not match.<br>");
  }

  /* if modifying depot, email2 must match email1 */
  else if ((action[0] == 's') && (previous[0] == 'm')) {
    if (depot->email1 != NULL) {
      if (depot->email1[0] == '\0') { 
        add_error("This depot does not seem to have an email address. Please email ");
        add_error("Scott Atchley to correct this.<br>");
      }
      if (depot->email2[0] == '\0') {
        add_error("The email line was left blank. Please enter your email address to");
        add_error(" confirm your access.<br>");
      }
      if (strcmp(depot->email1, depot->email2) != 0) {
        add_error("The email address that you entered does not match the address on record.");
        add_error("<br>Please enter the correct email address to complete this modification.<br>");
      }
    }
  }

  /* hostname must not be empty */
  if (depot->hostname[0] == '\0') {
    add_error("Please enter a hostname for this depot.<br>");
  }

  /* port must be between 0 and 65535 */
  if ((depot->port < 0) || (depot->port > 65535)) {
    add_error("The port number must be between 0 and 65535.<br>");
  }
 
  /* if depotname is blank, make same as hostname */
  if (depot->depotname[0] == '\0') depot->depotname = depot->hostname;

  /* make sure storage values are positive and available is less than total */
  if (depot->StableStorage < 0)
    add_error("The stable storage amount must be >= 0.<br>");
  if (depot->StableStorage < depot->AvailableStableStorage)
    add_error("The stable storage amount must be >= the available stable storage.<br>");
  if (depot->AvailableStableStorage < 0)
    add_error("The available stable storage amount must be >= 0.<br>");
  if (depot->VolatileStorage < 0)
    add_error("The volatile storage amount must be >= 0.<br>");
  if (depot->VolatileStorage < depot->AvailableVolatileStorage)
    add_error("The volatile storage amount must be >= the available volatile storage.<br>");
  if (depot->AvailableVolatileStorage < 0)
    add_error("The available volatile storage amount must be >= 0.<br>");

  if ((depot->Duration < 0) && (depot->Duration != -1)) depot->Duration = -1;

  /* validate location info */
  if ((depot->lat < -180) || (depot->lat > 180))
    add_error("The latitude must be between -180 and 180 degrees.<br>");
  if ((depot->lon < -180) || (depot->lon > 180)) 
    add_error("The longitude must be between -180 and 180 degrees.<br>");

  if ((depot->country[0] != '\0') && (strlen(depot->country) != 2))
    add_error("You entered a country code that was not 2 letters (including spaces).<br>");
  if (((strcmp(depot->country, "US") == 0) || (strcmp(depot->country, "us") == 0))) {
    if ((depot->state[0] != '\0') && (strlen(depot->state) != 2))
      add_error("You entered a state code that was not 2 letters (including spaces).<br>");
    if (depot->zip[0] != '\0') {
      if ((atoi(depot->zip) < 1001) || (atoi(depot->zip) > 99999)) 
        add_error("The zipcode must be between 01001 and 99999.<br>");
    }
  }
  else if (((strcmp(depot->country, "US") != 0) || (strcmp(depot->country, "us") != 0))) {
    if ((depot->country[0] != '\0') && (depot->state[0] != '\0')) {
      add_error("You entered a non-US country code and a state code. You must change");
      add_error(" the country code to US or delete the state code (including spaces).<br>");
    }
    if (depot->zip[0] != '\0') {
      add_error("You entered a non-US country and a US zipcode. Please change the");
      add_error(" country to US or delete the zipcode (including spaces).<br>");
    }
  }
  if ((depot->country[0] == '\0') && (depot->state[0] == '\0')) {
    if (depot->city[0] != '\0') {
      add_error("You entered a city but left country and US state empty. ");
      add_error("You need to enter either a country code or a US state code.<br>");
    }
  }
  if ((depot->airport[0] != '\0') && (strlen(depot->airport) != 3))
    add_error("You entered an airport code that was not 3 letters (including spaces).<br>");

  return 0;
}

int add_error(char *message) {
  int	length;

  ready = 0;
  length = strlen(errors);
  if (length + strlen(message) > 4096) return -1;
  if (length == 0) strcpy(errors, message);
  else strcat(errors, message);
  return 0;
}



int add_depot(DepotInfo depot) {
  char		*dn;
  char		temp[1024];
  char		**args, *host, *zip, *country, *state, *city, *airport;
  char		**hostlatlon;
  int		i, count, retval;
  double	lat, lon;
  LDAP          *ldap;
  LDAPMod	**mods;
  ServerConfig	server;

  server = (ServerConfig) malloc(sizeof(struct server_config));
  sprintf(temp, "%s:%d", LDAP_SERVER, LDAP_SRVR_PORT);
  server->ldaphost = (char *) strdup(temp);

  i = 0;

  count = 17;
  if (depot->country[0] != '\0') count++;
  if (depot->state[0] != '\0') count++;
  if (depot->city[0] != '\0') count++;
  if (depot->zip[0] != '\0') count++;
  if (depot->airport[0] != '\0') count++;
  if (depot->connection[0] != '\0') count++;
  if (depot->monitoring[0] != '\0') count++;
  if (depot->power[0] != '\0') count++;
  if (depot->backup[0] != '\0') count++;
  if (depot->firewall[0] != '\0') count++;

  memset(temp, 0, 1024);
  sprintf(temp, "depotname=%s,ou=depots,o=lbone", depot->depotname);
  dn = (char *) strdup(temp);

  mods= (LDAPMod **) malloc (sizeof (LDAPMod *) * (count + 1));

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "objectClass", "depot");
  i++;

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "depotname", depot->depotname);
  i++;

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "hostname", depot->hostname);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%d", depot->port);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "port", temp);
  i++;

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "email", depot->email2);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%ld", depot->StableStorage);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "StableStorage", temp);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%ld", depot->AvailableStableStorage);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "AvailableStableStorage", temp);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%ld", depot->VolatileStorage);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "VolatileStorage", temp);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%ld", depot->AvailableVolatileStorage);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "AvailableVolatileStorage", temp);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%f", (depot->Duration * 24*60*60));

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "Duration", temp);
  i++;

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "status", depot->status);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%ld", depot->lastUpdate);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "lastUpdate", temp);
  i++;

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "notifyOwnerPeriod", depot->notifyOwnerPeriod);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%d", depot->polled);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "polled", temp);
  i++;

  memset(temp, 0, 1024);
  sprintf(temp, "%d", depot->responded);

  mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
  replace_attr(mods[i], "responded", temp);
  i++;

  if (depot->connection[0] != '\0') {
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    replace_attr(mods[i], "connection", depot->connection);
    i++;
  }

  if (depot->monitoring[0] != '\0') {
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    replace_attr(mods[i], "monitoring", depot->monitoring);
    i++;
  }

  if (depot->power[0] != '\0') {
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    replace_attr(mods[i], "power", depot->power);
    i++;
  }

  if (depot->backup[0] != '\0') {
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    replace_attr(mods[i], "backup", depot->backup);
    i++;
  }

  if (depot->firewall[0] != '\0') {
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    replace_attr(mods[i], "firewall", depot->firewall);
    i++;
  }

  lat = 0; lon = 0;
  zip = NULL; airport = NULL; country = NULL; state = NULL; city = NULL;
  if (depot->zip[0] != '\0') {
    memset(temp, 0, 1024);
    sprintf(temp, "%s%s", "zip=", depot->zip);
    zip = (char *) strdup(temp);
  }
  if (depot->airport[0] != '\0') {
    memset(temp, 0, 1024);
    sprintf(temp, "%s%s", "airport=", depot->airport);
    airport = (char *) strdup(temp);
  }
  if (depot->country[0] != '\0') {
    memset(temp, 0, 1024);
    sprintf(temp, "%s%s", "c=", depot->country);
    country = (char *) strdup(temp);
  }
  if (depot->state[0] != '\0') {
    memset(temp, 0, 1024);
    sprintf(temp, "%s%s", "st=", depot->state);
    state = (char *) strdup(temp);
  }
  if (depot->city[0] != '\0') {
    memset(temp, 0, 1024);
    sprintf(temp, "%s%s", "l=", depot->city);
    city = (char *) strdup(temp);
  }

  retval = get_latlon(server, &lat, &lon, zip, airport, city, state, country);
  if (retval == 1) {

    memset(temp, 0, 1024);
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    if ((depot->lat == 0) && (depot->lon == 0))
      sprintf(temp, "%f", lat);
    else
      sprintf(temp, "%f", depot->lat);
    replace_attr(mods[i], "lat", temp);
    i++;

    memset(temp, 0, 1024);
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    if ((depot->lat == 0) && (depot->lon == 0))
      sprintf(temp, "%f", lon);
    else
      sprintf(temp, "%f", depot->lon);
    replace_attr(mods[i], "lon", temp);
    i++;

    if (depot->country[0] != '\0') {
      mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
      replace_attr(mods[i], "c", depot->country);
      i++;
    }

    if (depot->state[0] != '\0') {
      mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
      replace_attr(mods[i], "st", depot->state);
      i++;
    }

    if (depot->city[0] != '\0') {
      mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
      replace_attr(mods[i], "l", depot->city);
      i++;
    }

    if (depot->zip[0] != '\0') {
      mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
      replace_attr(mods[i], "zip", depot->zip);
      i++;
    }

    if (depot->airport[0] != '\0') {
      mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
      replace_attr(mods[i], "airport", depot->airport);
      i++;
    }
  }
  else {
    lat = 0;
    lon = 0;
    args = (char**) malloc(sizeof (char*) * 2);
    args[0] = (char*) strdup(depot->hostname);
    args[1] = NULL;
    retval = check_hostname(args[0]);
    if (retval == 1)
    {
       hostlatlon = getLatLong(args);
       if (hostlatlon != NULL) {
        if (hostlatlon[1] != NULL) {
          lat = atof(hostlatlon[1]);
          if (hostlatlon[2] != NULL) {
            lon = atof(hostlatlon[2]);
            if ((lat != 0) && (lon != 0)) {
            }
          }
          else lat = 0;
        }
      }
    }
    
    memset(temp, 0, 1024);
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    if ((depot->lat == 0) && (depot->lon == 0))
      sprintf(temp, "%f", lat);
    else
      sprintf(temp, "%f", depot->lat);
    replace_attr(mods[i], "lat", temp);
    i++;

    memset(temp, 0, 1024);
    mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
    if ((depot->lat == 0) && (depot->lon == 0))
      sprintf(temp, "%f", lon);
    else
      sprintf(temp, "%f", depot->lon);
    replace_attr(mods[i], "lon", temp);
    i++;
  }

  mods[i] = NULL;

  ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
  retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
  if (retval == LDAP_SUCCESS) {

    retval = ldap_add_s (ldap, dn, mods);
    if (retval != 0) ldap_perror(ldap, "add failed");
    ldap_mods_free(mods, 1);
    ldap_unbind(ldap);
  }
  return 0;
}

int modify_depot(DepotInfo depot) {
  char		*filter, temp[1024], *dn;
  char		*country, *state, *city, *zip, *airport;
  int		i, retval, size, new_name;
  double	lat, lon;
  DepotInfo	old_depot;
  LDAP          *ldap;
  LDAPMessage   *entry, *result;
  LDAPMod       **mods;
  ServerConfig  server;

  server = (ServerConfig) malloc(sizeof(struct server_config));
  sprintf(temp, "%s:%d", LDAP_SERVER, LDAP_SRVR_PORT);
  server->ldaphost = (char *) strdup(temp);

  i = 0; new_name = 0;

  ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
  retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
  if (retval == LDAP_SUCCESS) {

    /*** check for depotname change ***/
    if (strcmp(depot->depotname, depot->old_depotname) != 0) {
      /* make sure new dn is available */
      size = strlen(depot->depotname) + strlen("depotname=") + 1;
      filter = (char *) malloc(size);
      memset(filter, 0, size);
      sprintf(filter, "%s%s", "depotname=", depot->depotname);

      retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
      if (retval == LDAP_SUCCESS) {
        if (ldap_count_entries(ldap, result) > 0) {
          add_error("The new depotname is already used. Please enter a different depotname.");
          ldap_unbind(ldap);
          return -1;
        }
      }
      /* dn is available */
      memset(temp, 0, 1024);
      sprintf(temp, "depotname=%s,ou=depots,o=lbone", depot->old_depotname);
      dn = (char *) strdup(temp);

      retval = ldap_delete_s(ldap, dn);
      if (retval != LDAP_SUCCESS) {
        add_error(ldap_err2string(retval));
        ldap_unbind(ldap);
        return -1;
      }
      ldap_unbind(ldap);
      return add_depot(depot);
    }


    size = strlen(depot->depotname) + strlen("depotname=") + 1;
    filter = (char *) malloc(size);
    memset(filter, 0, size);
    sprintf(filter, "%s%s", "depotname=", depot->depotname);

    retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
    if (retval == LDAP_SUCCESS) {
      if ((entry = ldap_first_entry (ldap, result)) != NULL) {
        old_depot = get_depot_info(ldap, entry);

        mods= (LDAPMod **) malloc (sizeof (LDAPMod *) * 30);

        /* if hostname changed, replace it */
        if (strcmp(depot->hostname, old_depot->hostname) != 0) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "hostname", depot->hostname);
          i++;
        }

        /* if port changed, replace it */
        if (depot->port != old_depot->port) {
          memset(temp, 0, 1024);
          sprintf(temp, "%d", depot->port);

          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "port", temp);
          i++;
        }

        /* if StableStorage changed, replace it */
        if (depot->StableStorage != old_depot->StableStorage) {
          memset(temp, 0, 1024);
          sprintf(temp, "%ld", depot->StableStorage);

          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "StableStorage", temp);
          i++;
        }

        /* if AvailableStableStorage changed, replace it */
        if (depot->AvailableStableStorage != old_depot->AvailableStableStorage) {
          memset(temp, 0, 1024);
          sprintf(temp, "%ld", depot->AvailableStableStorage);

          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "AvailableStableStorage", temp);
          i++;
        }
 
        /* if VolatileStorage changed, replace it */
        if (depot->VolatileStorage != old_depot->VolatileStorage) {
          memset(temp, 0, 1024);
          sprintf(temp, "%ld", depot->VolatileStorage);

          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "VolatileStorage", temp);
          i++;
        }

        /* if AvailableVolatileStorage changed, replace it */
        if (depot->AvailableVolatileStorage != old_depot->AvailableVolatileStorage) {
          memset(temp, 0, 1024);
          sprintf(temp, "%ld", depot->AvailableVolatileStorage);

          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "AvailableVolatileStorage", temp);
          i++;
        }

        /* if Duration changed, replace it */
        if (depot->Duration != old_depot->Duration) {
          memset(temp, 0, 1024);
          sprintf(temp, "%f", (depot->Duration * 24*60*60));

          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          replace_attr(mods[i], "Duration", temp);
          i++;
        }


        lat = 0; lon = 0;
        zip = NULL; airport = NULL; country = NULL; state = NULL; city = NULL;
        if (depot->zip[0] != '\0') {
          memset(temp, 0, 1024);
          sprintf(temp, "%s%s", "zip=", depot->zip);
          zip = (char *) strdup(temp);
        }
        if (depot->airport[0] != '\0') {
          memset(temp, 0, 1024);
          sprintf(temp, "%s%s", "airport=", depot->airport);
          airport = (char *) strdup(temp);
        }
        if (depot->country[0] != '\0') {
          memset(temp, 0, 1024);
          sprintf(temp, "%s%s", "c=", depot->country);
          country = (char *) strdup(temp);
        }
        if (depot->state[0] != '\0') {
          memset(temp, 0, 1024);
          sprintf(temp, "%s%s", "st=", depot->state);
          state = (char *) strdup(temp);
        }
        if (depot->city[0] != '\0') {
          memset(temp, 0, 1024);
          sprintf(temp, "%s%s", "l=", depot->city);
          city = (char *) strdup(temp);
        }

        retval = get_latlon(server, &lat, &lon, zip, airport, city, state, country);
        if (retval == 1) {     /* found matching lat/lon in ldap */

          /* save lat/lon coordinates */
          memset(temp, 0, 1024);
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          sprintf(temp, "%f", lat);
          replace_attr(mods[i], "lat", temp);
          i++;

          memset(temp, 0, 1024);
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          sprintf(temp, "%f", lon);
          replace_attr(mods[i], "lon", temp);
          i++;
        }

        /*** country ***/
        /* if ldap entry has no country & the user added one, add to ldap */
        if ((old_depot->country == NULL) || (old_depot->country[0] == '\0')) {
          if ((depot->country != NULL) && (depot->country[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "c", depot->country);
            i++;
          }
        }
        /* else, both records have country, save new version if different */
        else if ((depot->country != NULL) && (depot->country[0] != '\0')) {
          if (strcmp(depot->country, old_depot->country) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "c", depot->country);
            i++;
          }
        }
        /* lastly, if ldap has country, but new record does not, delete from ldap */
        else if ((depot->country == NULL) || (depot->country[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "c", old_depot->country);
          i++;
        }

        /*** state ***/
        /* if ldap entry has no state & the user added one, add to ldap */
        if ((old_depot->state == NULL) || (old_depot->state[0] == '\0')) {
          if ((depot->state != NULL) && (depot->state[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "st", depot->state);
            i++;
          }
        }
        /* else, both records have state, save new version if different */
        else if ((depot->state != NULL) && (depot->state[0] != '\0')) {
          if (strcmp(depot->state, old_depot->state) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "st", depot->state);
            i++;
          }
        }
        /* lastly, if ldap has state, but new record does not, delete from ldap */
        else if ((depot->state == NULL) || (depot->state[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "st", old_depot->state);
          i++;
        }

        /*** city ***/
        /* if ldap entry has no city & the user added one, add to ldap */
        if ((old_depot->city == NULL) || (old_depot->city[0] == '\0')) {
          if ((depot->city != NULL) && (depot->city[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "l", depot->city);
            i++;
          }
        }
        /* else, both records have city, save new version if different */
        else if ((depot->city != NULL) && (depot->city[0] != '\0')) {
          if (strcmp(depot->city, old_depot->city) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "l", depot->city);
            i++;
          }
        }
        /* lastly, if ldap has city, but new record does not, delete from ldap */
        else if ((depot->city == NULL) || (depot->city[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "l", old_depot->city);
          i++;
        }

        /*** zip ***/
        /* if ldap entry has no zip & the user added one, add to ldap */
        if ((old_depot->zip == NULL) || (old_depot->zip[0] == '\0')) {
          if ((depot->zip != NULL) && (depot->zip[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "zip", depot->zip);
            i++;
          }
        }
        /* else, both records have zip, save new version if different */
        else if ((depot->zip != NULL) && (depot->zip[0] != '\0')) {
          if (strcmp(depot->zip, old_depot->zip) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "zip", depot->zip);
            i++;
          }
        }
        /* lastly, if ldap has zip, but new record does not, delete from ldap */
        else if ((depot->zip == NULL) || (depot->zip[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "zip", old_depot->zip);
          i++;
        }

        /*** airport ***/
        /* if ldap entry has no airport & the user added one, add to ldap */
        if ((old_depot->airport == NULL) || (old_depot->airport[0] == '\0')) {
          if ((depot->airport != NULL) && (depot->airport[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "airport", depot->airport);
            i++;
          }
        }
        /* else, both records have airport, save new version if different */
        else if ((depot->airport != NULL) && (depot->airport[0] != '\0')) {
          if (strcmp(depot->airport, old_depot->airport) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "airport", depot->airport);
            i++;
          }
        }
        /* lastly, if ldap has airport, but new record does not, delete from ldap */
        else if ((depot->airport == NULL) || (depot->airport[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "airport", old_depot->airport);
          i++;
        }

        /*** connection ***/
        /* if ldap entry has no connection & the user added one, add to ldap */
        if ((old_depot->connection == NULL) || (old_depot->connection[0] == '\0')) {
          if ((depot->connection != NULL) && (depot->connection[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "connection", depot->connection);
            i++;
          }
        }
        /* else, both records have connection, save new version if different */
        else if ((depot->connection != NULL) && (depot->connection[0] != '\0')) {
          if (strcmp(depot->connection, old_depot->connection) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "connection", depot->connection);
            i++;
          }
        }
        /* lastly, if ldap has connection, but new record does not, delete from ldap */
        else if ((depot->connection == NULL) || (depot->connection[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "connection", old_depot->connection);
          i++;
        }

        /*** monitoring ***/
        /* if ldap entry has no monitoring & the user added one, add to ldap */
        if ((old_depot->monitoring == NULL) || (old_depot->monitoring[0] == '\0')) {
          if ((depot->monitoring != NULL) && (depot->monitoring[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "monitoring", depot->monitoring);
            i++;
          }
        }
        /* else, both records have monitoring, save new version if different */
        else if ((depot->monitoring != NULL) && (depot->monitoring[0] != '\0')) {
          if (strcmp(depot->monitoring, old_depot->monitoring) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "monitoring", depot->monitoring);
            i++;
          }
        }
        /* lastly, if ldap has monitoring, but new record does not, delete from ldap */
        else if ((depot->monitoring == NULL) || (depot->monitoring[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "monitoring", old_depot->monitoring);
          i++;
        }

        /*** power ***/
        /* if ldap entry has no power & the user added one, add to ldap */
        if ((old_depot->power == NULL) || (old_depot->power[0] == '\0')) {
          if ((depot->power != NULL) && (depot->power[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "power", depot->power);
            i++;
          }
        }
        /* else, both records have power, save new version if different */
        else if ((depot->power != NULL) && (depot->power[0] != '\0')) {
          if (strcmp(depot->power, old_depot->power) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "power", depot->power);
            i++;
          }
        }
        /* lastly, if ldap has power, but new record does not, delete from ldap */
        else if ((depot->power == NULL) || (depot->power[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "power", old_depot->power);
          i++;
        }

        /*** backup ***/
        /* if ldap entry has no backup & the user added one, add to ldap */
        if ((old_depot->backup == NULL) || (old_depot->backup[0] == '\0')) {
          if ((depot->backup != NULL) && (depot->backup[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "backup", depot->backup);
            i++;
          }
        }
        /* else, both records have backup, save new version if different */
        else if ((depot->backup != NULL) && (depot->backup[0] != '\0')) {
          if (strcmp(depot->backup, old_depot->backup) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "backup", depot->backup);
            i++;
          }
        }
        /* lastly, if ldap has backup, but new record does not, delete from ldap */
        else if ((depot->backup == NULL) || (depot->backup[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "backup", old_depot->backup);
          i++;
        }

        /*** firewall ***/
        /* if ldap entry has no firewall & the user added one, add to ldap */
        if ((old_depot->firewall == NULL) || (old_depot->firewall[0] == '\0')) {
          if ((depot->firewall != NULL) && (depot->firewall[0] != '\0')) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "firewall", depot->firewall);
            i++;
          }
        }
        /* else, both records have firewall, save new version if different */
        else if ((depot->firewall != NULL) && (depot->firewall[0] != '\0')) {
          if (strcmp(depot->firewall, old_depot->firewall) != 0) {
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "firewall", depot->firewall);
            i++;
          }
        }
        /* lastly, if ldap has firewall, but new record does not, delete from ldap */
        else if ((depot->firewall == NULL) || (depot->firewall[0] == '\0')) {
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "firewall", old_depot->firewall);
          i++;
        }

        /*** notifyOwnerPeriod ***/
        /* if ldap entry has no notifyOwnerPeriod & the user added one, add to ldap */
        if ((old_depot->notifyOwnerPeriod == NULL) || (old_depot->notifyOwnerPeriod[0] == '\0')) { 
          if ((depot->notifyOwnerPeriod != NULL) && (depot->notifyOwnerPeriod[0] != '\0')) { 
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "notifyOwnerPeriod", depot->notifyOwnerPeriod);
            i++;
          }
        }
        /* else, both records have notifyOwnerPeriod, save new version if different */
        else if ((depot->notifyOwnerPeriod != NULL) && (depot->notifyOwnerPeriod[0] != '\0')) { 
          if (strcmp(depot->notifyOwnerPeriod, old_depot->notifyOwnerPeriod) != 0) { 
            mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
            replace_attr(mods[i], "notifyOwnerPeriod", depot->notifyOwnerPeriod);
            i++;
          }
        }
        /* lastly, if ldap has notifyOwnerPeriod, but new record does not, delete from ldap */
        else if ((depot->notifyOwnerPeriod == NULL) || (depot->notifyOwnerPeriod[0] == '\0')) { 
          mods[i] = (LDAPMod *) malloc (sizeof (LDAPMod));
          delete_attr(mods[i], "notifyOwnerPeriod", old_depot->notifyOwnerPeriod);
          i++;
        }

        mods[i] = NULL;

        memset(temp, 0, 1024);
        sprintf(temp, "depotname=%s,ou=depots,o=lbone", depot->depotname);
        dn = (char *) strdup(temp);

        ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
        retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
        if (retval == LDAP_SUCCESS) {
          if (new_name == 0)
            retval = ldap_modify_s (ldap, dn, mods);
          else
            retval = ldap_add_s (ldap, dn, mods);
          if (retval != 0) ldap_perror(ldap, "ldap_modify failed");
          ldap_mods_free(mods, 1);
          ldap_unbind(ldap);
          return 0;
        }
      }
    }
  }
  return -1;
}




int replace_attr(LDAPMod *mods, char *attr, char *value) {

  mods->mod_op = LDAP_MOD_REPLACE;
  mods->mod_type = strdup(attr);
  mods->mod_values = (char **) malloc(sizeof(char *) * 2);
  mods->mod_values[0] = strdup(value);
  mods->mod_values[1] = NULL;

  return 0;
}



int delete_attr(LDAPMod *mods, char *attr, char *value) {

  mods->mod_op = LDAP_MOD_DELETE;
  mods->mod_type = strdup(attr);
  mods->mod_values = (char **) malloc(sizeof(char *) * 2);
  mods->mod_values[0] = strdup(value);
  mods->mod_values[1] = NULL;

  return 0;
}


void trim_trailing_spaces(char *str)
{
  int	i;
  
  if (str == NULL) return;
  i = strlen(str);
  if (i == 0) return;
  while (str[i - 1] == ' ')
  {
    str[i - 1] = '\0';
    if (str[0] == '\0') continue;
    i = strlen(str);
  }
  trim_leading_spaces(str);
  return;
}



void trim_leading_spaces(char *str)
{
  int   i, j;

  if (str[0] != ' ') return;

  while (str[0] == ' ')
  {
    i = strlen(str);
    for (j = 0; j <= i; j++)
    {
      str[j] = str[j + 1];
    }
    str[i] = '\0';
  }
  return;
}






