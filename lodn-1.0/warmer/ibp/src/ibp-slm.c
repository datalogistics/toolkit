/*            IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *           Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
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
 * ibp-slm.c
 *
 * IBP simple line manager code. Version name: primeur
 *
 * in case of problems, please contact: ibp@cs.utk.edu
 *
 */

#include "ibp-slm.h"
#include "ibp_ClientLib.h"
#include "ibp_net.h"

int
main(argc,argv)
     int argc;
     char **argv;
{

  struct ibp_depot ls_depot;
  char lc_passwd[17];
  int li_nextstep = SLM_DEPOT;
  char lc_cm, lc_op;
  char *lc_buffer = NULL;
  int li_return, li_fd = 0;
  unsigned long ll_Stable, ll_Vol, ll_StableUsed, ll_VolUsed;
  long ll_Duration;

  printf ("IBP - Simple Line Manager\n\n");
  while (li_nextstep != SLM_EXIT) {
    switch (li_nextstep) {
    case SLM_DEPOT:
      printf ("IBP depot to connect to? ");
      li_return = scanf ("%s",ls_depot.host);
      /*    if (strcmp (ls_depot.host, 'e') == 0) {
        li_nextstep = SLM_EXIT;
        break;
	} */
      if (li_return == 0) {
	printf ("error %d, %s\n",errno, strerror (errno));
        exit(0);
      }
      ls_depot.port = 0;
      do {
        printf ("port (between 1023 and 65535)? ");
        scanf ("%d",&ls_depot.port);
      } while ((ls_depot.port < 1023) || (ls_depot.port > 65535));
      li_nextstep = SLM_COMMAND;
      break;

    case SLM_COMMAND:
      fflush (stdin);
      printf ("Command? ");
      scanf ("%c", &lc_cm);
      switch (lc_cm) {
      case 's':
        li_nextstep = SLM_DEPOT;
        break;
      case 'c':
        li_nextstep = SLM_OPER;
        break;
      case 'e':
        li_nextstep = SLM_EXIT;
        break;
      default:
        li_nextstep = SLM_PIRLA;
        break;
      }
      break;

    case SLM_PIRLA:
      printf("The only things the IBP-slm can do are:\n");
      printf("\tc\tto select a status command\n");
      printf("\ts\tto select an IBP depot\n");
      printf("\te\tto exit\n");
      li_nextstep = SLM_COMMAND;
      break;

    case SLM_CIULA:
      printf("The only operation supported are:\n");
      printf("\tq\tto query the status of an IBP depot\n");
      printf("\tm\tto modify some parameters of an IBP depot\n");
      printf("\tc\tto go back to the command menu\n");
      printf("\te\tto exit\n");
      li_nextstep = SLM_OPER;
      break;

    case SLM_OPER:
      fflush (stdin);
      printf ("Status operation? ");
      lc_op = getchar();
      switch (lc_op) {
      case 'e':
        li_nextstep = SLM_EXIT;
        break;

      case 'c':
        li_nextstep = SLM_COMMAND;
        break;

      case 'q':
        li_fd = connect_socket (ls_depot.host, ls_depot.port,0); 
        /*li_fd = SocketConnect (ls_depot.host, ls_depot.port); */
                                                         
        if (li_fd == -1) {                                      
          printf ("The connection with the depot could not be established\n");
          li_nextstep = SLM_COMMAND;
          break;
        }
        if ( set_socket_block(li_fd) < 0 ) {
            fprintf(stderr,"error in set socket block\n");
            exit(-1);
        }

        lc_buffer = (char *) malloc (CMD_BUF_SIZE);
        if (lc_buffer == NULL) {
          printf ("Some really bad internal error happened\n");
          li_nextstep = SLM_COMMAND;
          break;
        }
  
        sprintf(lc_buffer, "%d %d %d\n",
                 IBPv031,
                 IBP_STATUS,
                 IBP_ST_INQ);

        if (Write (li_fd, lc_buffer, strlen(lc_buffer)) == -1) {
          printf ("The status message could not be sent\n");
          close (li_fd);
          free (lc_buffer);
          li_nextstep = SLM_COMMAND;
          break;
        }
        break;

      case 'm':
        printf ("Depot password? ");
        scanf ("%s",lc_passwd);
        printf ("New stable storage (in Mb)? ");
        scanf ("%lu", &ll_Stable);
        printf ("New volatile storage (in Mb)? "); 
        scanf ("%lu", &ll_Vol);
        printf ("New max duration? (in days)");
        scanf ("%ld", &ll_Duration);

        li_fd = connect_socket (ls_depot.host, ls_depot.port,0); 
        /*li_fd = SocketConnect (ls_depot.host, ls_depot.port); */
                                                         
        if (li_fd == -1) {                                      
          printf ("The connection with the depot could not be established\n");
          li_nextstep = SLM_COMMAND;
          break;
        }
        if ( set_socket_block(li_fd) < 0 ) {
            fprintf(stderr,"error in set socket block\n");
            exit(-1);
        }

        lc_buffer = (char *) malloc (CMD_BUF_SIZE);
        if (lc_buffer == NULL) {
          printf ("Some really bad internal error happened\n");
          li_nextstep = SLM_COMMAND;
          break;
        }
  
        sprintf(lc_buffer, "%d %d %d %s\n",
                 IBPv031,
                 IBP_STATUS,
                 IBP_ST_CHANGE,
                 lc_passwd);

        if (Write (li_fd, lc_buffer, strlen(lc_buffer)) == -1) {
          printf ("The status message could not be sent\n");
          close (li_fd);
          free (lc_buffer);
          li_nextstep = SLM_COMMAND;
          break;
        }

        if (ReadLine(li_fd, lc_buffer, CMD_BUF_SIZE) == -1) {
          printf ("The server answer could not be received\n");
          close (li_fd);
          free (lc_buffer);
          li_nextstep = SLM_COMMAND;
          break;
        }

	if (lc_buffer != NULL) {
	  if (atoi(lc_buffer) != IBP_OK) {
	    printf ("Server answered : %s \n", lc_buffer);
	    close (li_fd);
	    free (lc_buffer);
	    li_nextstep = SLM_COMMAND;
	    break;
	  }
	}

        sprintf(lc_buffer, "%lu %lu %ld \n",
                 ll_Stable,
                 ll_Vol,
                 ll_Duration);

        if (Write (li_fd, lc_buffer, strlen(lc_buffer)) == -1) {
          printf ("The status message could not be sent\n");
          close (li_fd);
          free (lc_buffer);
          li_nextstep = SLM_COMMAND;
          break;
        }
        break;

      default:
        li_nextstep = SLM_CIULA;
        break;
      }

      if (li_nextstep == SLM_OPER) {
        if (ReadLine(li_fd, lc_buffer, CMD_BUF_SIZE) == -1) { 
          printf ("The server current status could not be received\n");
          close (li_fd);
          free (lc_buffer);
        }
        else {
          if (sscanf (lc_buffer, "%lu %lu %lu %lu %ld",
                              &ll_Stable,
                              &ll_StableUsed,
                              &ll_Vol,
                              &ll_VolUsed,
                              &ll_Duration) != 5) {
            printf ("The server answer could not be undestood\n");
            close (li_fd);
            free (lc_buffer);
          }
          else {
            printf ("Depot: \t\t\t%s\n", ls_depot.host);
            printf ("Port: \t\t\t%d\n", ls_depot.port);
            printf ("Stable storage:\t\t%lu\n",ll_Stable);
            printf ("Stable storage used:\t%lu\n",ll_StableUsed);
            printf ("Volatile storage:\t%lu\n",ll_Vol);
            printf ("Volatile storage used:\t%lu\n",ll_VolUsed);
            printf ("Max Duration:\t\t%ld\n",ll_Duration);
          }
        }
        li_nextstep = SLM_COMMAND;
      }
      break;

    default:
      li_nextstep = SLM_EXIT;
      break;
    } 
  } /* end while */

  printf ("IBP - Simple Line Manager - exiting ...\n");
  exit (0);
}




