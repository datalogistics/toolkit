#include <stdlib.h>
#include "lbone_cgi_base.h"
#include "lbone_server.h"
#include "lbone_poll.h"
#include "ecgi.h"

extern void html_top();
extern void html_end();
extern int display_form();
extern int add_error();
extern int delete_depot();
extern int check_email_address();
extern void back_button();

char	errors[4096];
char	*action;
int	ready;

int main() {
  int           retval;
  DepotInfo	depot;

  depot = (DepotInfo) malloc(sizeof(struct depot_info));

  if (cgiInit() != ECGI_SUCCESS) {
    printf("ERROR: %s\n", cgiStrError(cgi_errno));
    exit(cgi_errno);
  }

  memset(errors, 0, 4096);
  ready = 1;

  if ((action = (char *) cgiParam ("action")) == NULL) {
    /* print error, return to list view */
    return 1;
  }

  if ((depot->depotname = (char *) cgiParam ("depotname")) == NULL) {
    /* print error, return to list view */
  }

  if (action[0] == 'd') {
    retval = display_form(depot);
  }
  else if (action[0] == 's') {
    retval = check_email_address(depot);
    if (ready == 1) retval = delete_depot(depot);
    retval = display_form(depot);
  }
  else {
    /* neither delete nor submit, print error, return */

  }
  return 0;
}



int check_email_address(DepotInfo depot) {
  char  **vals, *filter;
  int   retval, size;
  LDAP  *ldap;
  LDAPMessage   *result, *entry;

  ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
  retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
  if (retval == LDAP_SUCCESS) {

    size = strlen(depot->depotname) + strlen("depotname=") + 1;
    filter = (char *) malloc(size);
    memset(filter, 0, size);
    sprintf(filter, "%s%s", "depotname=", depot->depotname);

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

  if ((depot->email2 = (char *) cgiParam ("email")) == NULL) {
    /* print error, return to list view */
  }

  if ((depot->email2 == NULL) || (depot->email2[0] == '\0')) {
    add_error("The email line was left blank. Please enter your email address to");
    add_error(" confirm your access.<br>");
    return 1;
  }

  if (strcmp(depot->email1, depot->email2) != 0) {
    add_error("The email address that you entered does not match the address on record.");
    add_error("<br>Please enter the correct email address to complete this deletion.<br>");
  }

  return 0;
}


int delete_depot(DepotInfo depot) {
  char          *filter, temp[1024], *dn;
  int           retval, size;
  LDAP          *ldap;
  LDAPMessage   *result;

  ldap = ldap_init (LDAP_SERVER, LDAP_SRVR_PORT);
  retval = ldap_simple_bind_s (ldap, LDAP_WHO, LDAP_PASSWD);
  if (retval == LDAP_SUCCESS) {

    /* check ldap for entry */
    size = strlen(depot->depotname) + strlen("depotname=") + 1;
    filter = (char *) malloc(size);
    memset(filter, 0, size);
    sprintf(filter, "%s%s", "depotname=", depot->depotname);

    retval = ldap_search_s (ldap, LDAP_BASE, LDAP_SCOPE_SUBTREE, filter, NULL, 0, &result);
    if (retval == LDAP_SUCCESS) {
      if (ldap_count_entries(ldap, result) == 0) {
        add_error("The depot was not found.");
        ldap_unbind(ldap);
        return -1;
      }
    }
    /* depot was found */
    memset(temp, 0, 1024);
    sprintf(temp, "depotname=%s,ou=depots,o=lbone", depot->depotname);
    dn = (char *) strdup(temp);

    retval = ldap_delete_s(ldap, dn);
    if (retval != LDAP_SUCCESS) {
      add_error(ldap_err2string(retval));
      ldap_unbind(ldap);
      return -1;
    }
    ldap_unbind(ldap);
    return 0;
  }
  add_error("Could not bind to ldap server.");
  return -1;
}


int display_form(DepotInfo depot) {

  html_top();

  if (ready == 0) {
    printf("<p><font color=FF0000 face=\"Arial,Helvetica\" size=+1>");
    printf("<b>Please correct these errors:</b></font></p>\n");
    printf("<p><font color=FF0000 face=\"Arial,Helvetica\"><b>");
    printf("%s</b></font></p>\n", errors);
  }
  else if ((action[0] == 's') && (ready == 1)) {
    printf("<p><font face=\"Arial,Helvetica\" size=+1>Depot deleted successfully!</face></p>");

    back_button();
    html_end();
    return 0;
  }

  printf("</center>\n");
  printf("<p>&nbsp;</p>\n");

  printf("<p>&nbsp;</p>\n");
  printf("<table border=0 width=100%% cellspacing=0 cellpadding=3>\n");
  printf("  <tr>\n");
  printf("    <td width=100%% bgcolor=87aec2><font face=\"Arial,Helvetica\">");
  printf("<b>Confirm Ownership of Depot</b></font></td>\n");
  printf("  </tr>\n");
  printf("</table>\n");

  printf("<p>Please enter your email address to verify this is your depot</p>");

  printf("<form method=\"POST\" action=\"%s/lbone_depot_delete.cgi\">\n",
          LBONE_WEB_HOME);
  printf("  <table border=0 width=100%% cellspacing=0 cellpadding=0>\n");
  printf("    <tr>\n");
  printf("      <td width=33%% bgcolor=EEEEEE>&nbsp;Email Address");
  printf("<font color=FF0000> (required)</font></td>\n");
  printf("      <td width=67%% bgcolor=EEEEEE><input type=\"text\" name=\"email\" ");
  printf("          size=45 tabindex=1> </td>\n    </tr>\n");
  printf("    <tr>\n");
  printf("      <td width=33%%>&nbsp;</td>\n");
  printf("      <td width=67%%>");
  printf("&nbsp;</td>\n");
  printf("    </tr>\n");
  printf("  </table>\n");
  printf("  <div align=\"center\"><center><p><input type=\"submit\" value=\"Confirm\"");
  printf(" name=\"submit\"> <input type=\"reset\" value=\"Reset\" name=\"reset\"></p>\n");
  printf("  </center></div>\n");
  printf("  <input type=\"hidden\" name=\"depotname\" value=\"%s\">\n", depot->depotname);
  printf("  <input type=\"hidden\" name=\"action\" value=\"submit\">\n");
  printf("</form>\n");

  html_end();
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




