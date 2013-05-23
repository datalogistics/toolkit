/*
 *              LBONE version 1.0:  Logistical Backbone
 *               University of Tennessee, Knoxville TN.
 *        Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 2001 All Rights Reserved
 *
 *                              NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appear in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * Neither the Institution (University of Tennessee) nor the Authors 
 * make any representations about the suitability of this software for 
 * any purpose. This software is provided ``as is'' without express or
 * implied warranty.
 *
 */

/*
 * lbone_error.c
 *
 * LBONE error functions. Version name: alpha
 *
 */


# include "lbone_error.h"
# include "lbone_socket.h"

static char  *LBONE_strings[] = {
  "",
  "A generic error has occured while processing client request. Sorry for that.",
  "An error has occured while reading from an LBONE socket.",
  "An error has occured while writing to an LBONE socket.",
  "unused.",
  "unused.",
  "unused.",
  "unused.",
  "unused.",
  "unused.",
  "unused.",
  "unused.",
  "unused.",
  "An error occurred while connecting to an LBONE server.",
  "An error occurred while opening a back storage file on server.", 
  "An error occurred while reading from a back storage file on server.",
  "An error occurred while writing to a back storage file on server.",
  "An error occurred while accessing a back storage file on server.",
  "unused.",
  "unused.",
  "unused.",
  "An LBONE message has the wrong format expected by the server and/or the client.",
  "unused.",
  "A resource required by LBONE server is currently unavailable.",
  "An internal error occurred in LBONE server.",
  "The LBONE server has received a command it does not recognize.",
  "The operation attempted would block on LBONE server.",
  "The LBONE protocol version selected is not supported by this server."
  "unused.",
  "The password you entered for this LBONE server is incorrect."
  "Invalid parameter in call to LBONE.",
  "Invalid parameter (hostname).",
  "Invalid parameter (port number).",
  "Invalid parameter (attribute, duration).",
  "Invalid parameter (attribute, reliability).",
  "Invalid parameter (attribute, type).",
  "Invalid parameter (size).",
  "An internal error (memory allocation) occurred in the LBONE server."
  };


char *LBONE_strerror (int pi_err) {

   if ((pi_err > -1) || (pi_err  < -LBONE_MAX_ERROR))
      return LBONE_strings[0];
   else
      return LBONE_strings[-pi_err];
}


char *LBONE_substr (char *pc_buffer, char *pc_first)
{

  int       li_i = 0;
  char      *lc_save, *lc_left;

  lc_save = pc_buffer;
  lc_left = pc_buffer;
 
  while ((*lc_left == ' ') && (li_i < (CMD_BUF_SIZE -1))){
	lc_left ++;
	li_i ++;
  };
  while ((*lc_left != ' ') && (*lc_left != '\n') && (*lc_left != '\0') && (li_i < (CMD_BUF_SIZE - 1))) {
    lc_left++;
    li_i++;
  }
  lc_left++;

  strncpy (pc_first, lc_save, li_i);
  pc_first[li_i] = '\0';

  return lc_left;
}

void lbone_error (int nerr, char *err_message)
{
  int		li_communicate = NO;

  if (nerr > (-LBONE_MAX_ERROR) )
    li_communicate = YES;

  switch (nerr) {
  case E_USAGE:
    if (err_message != NULL) fprintf(stderr, "\nlbone_server: %s\n", err_message);
    fprintf(stderr, "\nusage: lbone_server -pw <password> [options]\n");
    fprintf(stderr, "\nLBONE server - options:\n");
    fprintf(stderr, " [-cp <path>]      : the absolute path to find the config file\n");
    fprintf(stderr, " [-lh <ldap host>] : the ldap server to use\n");
    fprintf(stderr, " [-p  <port>]      : the port to use [1024 < port < 65353]\n");
    fprintf(stderr, " [-no-poll]        : do not poll the depots and update ldap\n\n");
    fprintf(stderr, "If the path contains spaces, enclose it in double quotes. ");
    fprintf(stderr, "If any of these options\nare not specified, the default values ");
    fprintf(stderr, "found in the LBONE configuration file under\n/etc will be used. ");
    fprintf(stderr, "If there is not a LBONE configuration file or if some values\nare ");
    fprintf(stderr, "missing from the configuration file, LBONE default values will be ");
    fprintf(stderr, "used.\n\nPlease refer to the LBONE documentation ");
    fprintf(stderr, "for a complete explanation.\n\n");
    break;

  case E_HOMEDIR:
    fprintf(stderr, "Unable to determine home directory\n");
    break;
  case E_FQDN:
    fprintf(stderr, "Unable to determine FQDN of local host\n");
    break;
  case E_GLIMIT:
    fprintf(stderr, "Error in getrlimit(), %s\n",strerror(errno));
    break;
  case E_SLIMIT:
    fprintf(stderr, "Error in setrlimit(). %s\n",strerror(errno));
    break;
  case E_CFGFILE:
    fprintf(stderr, "Error in config file at :%s\n", err_message);
    break;
  case E_CFGPAR:
    fprintf(stderr, "Invalid config parameter at : %s\n", err_message);
    break;
  case E_ACCDIR:
    fprintf(stderr,"Error accessing directory %s\n", err_message);
    break;
  case E_ABSPATH:
    fprintf(stderr,"Error: expecting absolute path at %s\n", err_message);
    break;
  case E_INVHOST:
    fprintf(stderr,"Error: Hostname %s appears to be invalid\n", err_message);
    break;
  case E_ZEROREQ:
    fprintf(stderr,"Error: Zero storage requested\n");
    break;
  case E_ACCSTORD:   /* from RECOVER */
    fprintf(stderr,"Unable to access storage directory %s: %s\n",
	    err_message, strerror(errno));
    exit (1);
  case E_OFILE:
    fprintf(stderr,"Unable to open file %s: %s\n",
	    err_message, strerror(errno));
    break;
  case E_RFILE:
    fprintf(stderr,"Unable to read file %s\n", err_message);
    break;
  case E_CFGSOK:
    fprintf(stderr,"Unable to configure LBONE socket\n");
    break;
  case E_LISSOK:
    fprintf(stderr,"Error in listening socket\n");
    break;
  case E_RLINE:
    fprintf(stderr,"Error in reading line from Client \n");
    break;
  case E_BOOTFAIL:
    fprintf(stderr,"Boot failed, function code %s\n", err_message);
    break;
  case E_ACCEPT:
    fprintf(stderr, "accept () failed, %s, Thread %s\n", strerror(errno), err_message);
    break;
  case E_PORT:
    fprintf(stderr, "the port asked, %s, is out of range. The default LBONE DATA port will be used\n", err_message);
    break;
  case E_ALLOC:
    fprintf (stderr, "A memory allocation was unsuccessful\n");
    break;
  default:
    break;
	    
  }
  fflush(stderr);
  return;
}

