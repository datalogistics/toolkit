/*
 *           IBP Client version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *          Authors: Y. Zheng A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 1999 All Rights Reserved
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
# include "config-ibp.h"
# include "ibp_os.h"
# ifdef STDC_HEADERS
# include <time.h>
# include <stdio.h>
# include <string.h>
# include <stdlib.h>
# endif /* STDC_HEADERS */
# ifdef HAVE_MALLOC_H
# include <malloc.h>
# endif /* HAVE_MALLOC_H */
# include "ibp_ClientLib.h"
# include "ibp_protocol.h"
# include "ibp_errno.h"
# include "ibp_uri.h"
# include "ibp_ComMod.h"
# include "ibp_net.h"

# ifndef IBP_WIN32
#	ifdef HAVE_PTHREAD_H
# include <pthread.h>
#	endif /* HAVE_PTHREAD_H */
# endif /* IBP_WIN32 */

# ifndef IBP_WIN32              /* unix-like OS */
#	ifdef HAVE_PTHREAD_H
extern struct ibp_dptinfo *_glb_dpt_info();
#		define glb_dpt_info (*_glb_dpt_info())
#	else
static struct ibp_dptinfo glb_dpt_info;
#	endif
# else /* Windows OS */
__declspec(thread)
     struct ibp_dptinfo glb_dpt_info;
# endif


/*****************************/
/* check_timeout              */
/* It checks whether it is timeout */
/* return -- OK / not        */
/*****************************/
int check_timeout(IBP_timer ps_timeout)
{
    if (ps_timeout->ClientTimeout < 0) {
        return (IBP_E_INVALID_PARAMETER);
    }
    return (IBP_OK);
}                              /* check_timeout */

/*****************************/
/* IBP_substr                */
/* It eliminates the blank space in a string */
/* return -- the pointer of the last char    */
/*****************************/
char *IBP_substr(char *pc_buffer, char *pc_first)
{

    int li_i = 0;
    char *lc_save, *lc_left;

    lc_save = pc_buffer;
    lc_left = pc_buffer;

    while ((*lc_left == ' ') && (li_i < (CMD_BUF_SIZE - 1))) {
        lc_left++;
        li_i++;
    }
    while ((*lc_left != ' ') && (*lc_left != '\n') && (*lc_left != '\0')
           && (li_i < (CMD_BUF_SIZE - 1))) {
        lc_left++;
        li_i++;
    }
    lc_left++;

    strncpy(pc_first, lc_save, li_i);
    pc_first[li_i] = '\0';

    return lc_left;
}

char *capToString(IBP_cap cap)
{
    return (char *) cap;
}

/*****************************/
/* IBP_freeCapSet            */
/* It frees the cap space    */
/* return -- OK / not        */
/*****************************/
int IBP_freeCapSet(IBP_set_of_caps capSet)
{
/*  Free a dynamically allocated IBP_set_of_caps 
 *  return 0 on success
 *  return -1 on failure
 */
    if (capSet != NULL) {
        free(capSet->readCap);
        free(capSet->writeCap);
        free(capSet->manageCap);
        free(capSet);
        return 0;
    }
    IBP_errno = IBP_E_INVALID_PARAMETER;
    return -1;
}

/*****************************/
/* IBP_allocate              */
/* It applies a space to a IBP depot     */
/* This is an important IBP API          */
/* return -- capability info             */
/*****************************/
IBP_set_of_caps IBP_allocate(IBP_depot ps_depot,
                             IBP_timer ps_timeout,
                             unsigned long int pi_size,
                             IBP_attributes ps_attr)
{

    int li_return, li_fd;
    time_t lt_now, lt_lifetime;
    char lc_buffer[CMD_BUF_SIZE];
    char lc_singleton[CMD_BUF_SIZE];
    char *lc_left;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;
    IBP_set_of_caps ls_caps;

    /*
     * initialize error variable 
     */

    IBP_errno = IBP_OK;

    /*
     * check validation of depot parameter
     */

    if ((li_return = check_depot(ps_depot->host, &ps_depot->port)) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    /*
     * check timeout
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    /*
     * check IBP attributes 
     */
    time(&lt_now);
    if (ps_attr == NULL) {
        IBP_errno = IBP_E_INV_PAR_ATDR;
        return (NULL);
    }
    if (ps_attr->duration > 0) {
        lt_lifetime = ps_attr->duration - lt_now;
        if (lt_lifetime < 0) {
            IBP_errno = IBP_E_INV_PAR_ATDR;
            return (NULL);
        }
    }
    else {
        lt_lifetime = 0;
    }

    if ((li_return = d_CheckAttrRely(ps_attr->reliability)) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    if ((li_return = d_CheckAttrType(ps_attr->type)) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    /*
     * connect to IBP server
     */
    if ((li_fd =
         connect_socket(ps_depot->host, ps_depot->port,
                        ps_timeout->ClientTimeout)) == -1) {
        IBP_errno = IBP_E_CONNECTION;
        return (NULL);
    }

    if (set_socket_noblock(li_fd) != 0) {
        IBP_errno = IBP_E_CONNECTION;
        close_socket(li_fd);
        return (NULL);
    }

    /*
     * initialize the communication session structure
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * fill out the send unit
     */
    sprintf(lc_buffer, "%d %d %d %d %d %lu %d\n",
            IBPv031,
            IBP_ALLOCATE,
            ps_attr->reliability,
            ps_attr->type,
            (int) lt_lifetime, pi_size, ps_timeout->ServerSync);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, li_fd, lc_buffer, strlen(lc_buffer),
         DATAVAR, NULL) != IBP_OK) {
        close_socket(li_fd);
        return (NULL);
    }

    /*
     * fill out the receive unit
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, li_fd, lc_buffer, CMD_BUF_SIZE, DATAVAR,
         NULL) != IBP_OK) {
        close_socket(li_fd);
        return (NULL);
    }

    /*
     * do transaction
     */
    if ((lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL) {
        close_socket(li_fd);
        return (NULL);
    }

    close_socket(li_fd);

    /*
     * process the answer
     */
    lc_left = lp_comm_unit->data_buf;
    lc_left = (char *) IBP_substr(lc_left, lc_singleton);
    if ((li_return = atoi(lc_singleton)) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    /* 
     * get the three caps 
     */
    ls_caps = (IBP_set_of_caps) calloc(1, sizeof(struct ibp_set_of_caps));
    if (ls_caps == NULL) {
        IBP_errno = IBP_E_ALLOC_FAILED;
        return (NULL);
    }

    /*
     * read capability
     */
    lc_left = (char *) IBP_substr(lc_left, lc_singleton);
    if (lc_singleton != NULL) {
        if ((ls_caps->readCap = (IBP_cap) strdup(lc_singleton)) == NULL) {
            IBP_errno = IBP_E_ALLOC_FAILED;
            free(ls_caps);
            return (NULL);
        }
    }

    /*
     * write capability
     */
    lc_left = (char *) IBP_substr(lc_left, lc_singleton);
    if (lc_singleton != NULL) {
        if ((ls_caps->writeCap = (IBP_cap) strdup(lc_singleton)) == NULL) {
            IBP_errno = IBP_E_ALLOC_FAILED;
            free(ls_caps);
            return (NULL);
        }
    }

    /*
     * manage capability
     */
    lc_left = (char *) IBP_substr(lc_left, lc_singleton);
    if (lc_singleton != NULL) {
        if ((ls_caps->manageCap = (IBP_cap) strdup(lc_singleton)) == NULL) {
            IBP_errno = IBP_E_ALLOC_FAILED;
            free(ls_caps);
            return (NULL);
        }
    }

    /*
     * return
     */
    return (ls_caps);

}                              /* IBP_set_of_caps */



/*****************************/
/* IBP_store                 */
/* It stores some info to a IBP depot     */
/* This is an important IBP API          */
/* return -- the length of data             */
/*****************************/
unsigned long int IBP_store(IBP_cap ps_cap,
                            IBP_timer ps_timeout,
                            char *pc_data, unsigned long int pl_size)
{

    IURL *lp_iurl;
    int li_return;
    char lc_line_buf[CMD_BUF_SIZE];
    unsigned long int ll_truesize;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */

    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    /*
     * check data buffer
     */
    if (pc_data == NULL) {
        IBP_errno = IBP_E_INV_PAR_PTR;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }
    /*
     * get parameters
     */
    if ((lp_iurl =
         create_write_IURL(ps_cap, ps_timeout->ClientTimeout)) == NULL) {
        return (0);
    }
    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * send IBP_STORE command to server
     */
    sprintf(lc_line_buf, "%d %d %s %d %lu %d \n",
            IBPv031,
            IBP_STORE,
            lp_iurl->key, lp_iurl->type_key, pl_size, ps_timeout->ServerSync);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf, CMD_BUF_SIZE,
         DATAVAR, (func_pt *) HandleStoreResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * send user data
     */
    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, pc_data, pl_size, DATAVAR,
         NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * get server response 
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf, CMD_BUF_SIZE,
         DATAVAR, (func_pt *) HandleStoreResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }


    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }
 

    if (sscanf(lp_comm_unit->data_buf, "%d %lu", &li_return, &ll_truesize) !=
        2) {
        IBP_errno = IBP_E_BAD_FORMAT;
        free_IURL(lp_iurl);
        return (0);
    }

    free_IURL(lp_iurl);
    return (ll_truesize);

}                              /* IBP_store */



/*****************************/
/* IBP_remote_store              */
/* It store data to an IBP depot     */
/* return -- capability info             */
/*****************************/
unsigned long int IBP_remote_store(IBP_cap ps_cap,
                                   IBP_depot ps_remote_depot,
                                   IBP_timer ps_timeout,
                                   unsigned long int pl_size)
{

    int li_return;
    IURL *lp_iurl;
    char lc_line_buf[CMD_BUF_SIZE];
    unsigned long int ll_truesize;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }

    /*
     * get parameters
     */
    if ((lp_iurl =
         create_write_IURL(ps_cap, ps_timeout->ClientTimeout)) == NULL)
        return (0);

    if ((li_return =
         check_depot(ps_remote_depot->host,
                     &ps_remote_depot->port)) != IBP_OK) {
        IBP_errno = li_return;
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * send IBP_REMOTE_STORE command to server
     */
    sprintf(lc_line_buf, "%d %d %s %s %d %d %lu %d \n",
            IBPv031,
            IBP_REMOTE_STORE,
            lp_iurl->key,
            ps_remote_depot->host,
            ps_remote_depot->port,
            lp_iurl->type_key, pl_size, ps_timeout->ServerSync);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf, CMD_BUF_SIZE,
         DATAVAR, (func_pt *) HandleStoreResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }

    if (sscanf(lp_comm_unit->data_buf, "%d %lu", &li_return, &ll_truesize) !=
        2) {
        IBP_errno = IBP_E_BAD_FORMAT;
        free_IURL(lp_iurl);
        return (0);
    }

    free_IURL(lp_iurl);
    return (ll_truesize);

}                              /* IBP_remote_store */



/*****************************/
/* IBP_copy              */
/* It copys some info from a depot to another IBP depot     */
/* This is an important IBP API          */
/* return -- data length             */
/*****************************/
unsigned long int IBP_copy(IBP_cap ps_source,
                           IBP_cap ps_target,
                           IBP_timer ps_src_timeout,
                           IBP_timer ps_tgt_timeout,
                           unsigned long int pl_size,
                           unsigned long int pl_offset)
{

    int li_return;
    IURL *lp_iurl;
    char lc_line_buf[CMD_BUF_SIZE];
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_src_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    if (ps_target == NULL) {
        IBP_errno = IBP_E_WRONG_CAP_FORMAT;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }

    /*
     * get parameters
     */
    if ((lp_iurl =
         create_read_IURL(ps_source, ps_src_timeout->ClientTimeout)) == NULL)
        return (0);

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_src_timeout->ClientTimeout);

    /*
     * send IBP_REMOTE_STORE command to server
     */
    sprintf(lc_line_buf, "%d %d %s %s %d %lu %lu %d %d %d \n",
            IBPv031,
            IBP_SEND,
            lp_iurl->key,
            capToString(ps_target),
            lp_iurl->type_key,
            pl_offset,
            pl_size,
            ps_src_timeout->ServerSync,
            ps_tgt_timeout->ServerSync, ps_tgt_timeout->ClientTimeout);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf,
         sizeof(lc_line_buf), DATAVAR,
         (func_pt *) HandleCopyResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }

    free_IURL(lp_iurl);

    return (lp_comm_unit->data_size);

}                              /* IBP_copy */


/*****************************/
/* IBP_load              */
/* It copys some info from a depot     */
/* It is an important IBP API         */
/* return -- data length             */
/*****************************/
unsigned long int IBP_load(IBP_cap ps_cap,
                           IBP_timer ps_timeout,
                           char *pc_data,
                           unsigned long int pl_size,
                           unsigned long int pl_offset)
{

    int li_return;
    IURL *lp_iurl;
    char lc_line_buf[CMD_BUF_SIZE];
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    /*
     * check data buffer
     */
    if (pc_data == NULL) {
        IBP_errno = IBP_E_INV_PAR_PTR;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }

    /*
     * get parameters
     */
    if ((lp_iurl =
         create_read_IURL(ps_cap, ps_timeout->ClientTimeout)) == NULL)
        return (0);

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * send IBP_REMOTE_STORE command to server
     */
    sprintf(lc_line_buf, "%d %d %s %d %lu %lu %d \n",
            IBPv031,
            IBP_LOAD,
            lp_iurl->key,
            lp_iurl->type_key, pl_offset, pl_size, ps_timeout->ServerSync);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf, CMD_BUF_SIZE,
         DATAVAR, (func_pt *) HandleLoadResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the data 
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, pc_data, pl_size, DATAFIXED,
         NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }

    free_IURL(lp_iurl);

    return (lp_comm_unit->data_size);

}                              /* IBP_load */


/*****************************/
/* IBP_deliver              */
/* It delivers some info to a depot     */
/* return -- data length             */
/*****************************/
unsigned long int IBP_deliver(IBP_cap ps_cap,
                              IBP_depot ps_target_depot,
                              IBP_timer ps_timeout,
                              unsigned long int pl_size,
                              unsigned long int pl_offset)
{

    int li_return;
    IURL *lp_iurl;
    char lc_line_buf[CMD_BUF_SIZE];
    unsigned long int ll_truesize;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }

    /*
     * get parameters
     */
    if ((lp_iurl =
         create_read_IURL(ps_cap, ps_timeout->ClientTimeout)) == NULL)
        return (0);

    if ((li_return =
         check_depot(ps_target_depot->host,
                     &(ps_target_depot->port))) != IBP_OK) {
        IBP_errno = li_return;
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * send IBP_REMOTE_STORE command to server
     */
    sprintf(lc_line_buf, "%d %d %s %s %d %d %lu %lu %d \n",
            IBPv031,
            IBP_DELIVER,
            lp_iurl->key,
            ps_target_depot->host,
            ps_target_depot->port,
            lp_iurl->type_key, pl_offset, pl_size, ps_timeout->ServerSync);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAFIXED, NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf, CMD_BUF_SIZE,
         DATAVAR, (func_pt *) HandleStoreResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }

    if (sscanf(lp_comm_unit->data_buf, "%d %lu", &li_return, &ll_truesize) !=
        2) {
        IBP_errno = IBP_E_BAD_FORMAT;
        free_IURL(lp_iurl);
        return (0);
    }

    free_IURL(lp_iurl);
    return (ll_truesize);
}                              /* IBP_deliver */



/*****************************/
/* IBP_manage                */
/* It manages a file in a depot     */
/* It is an important IBP API         */
/* return -- OK / not             */
/*****************************/
int IBP_manage(IBP_cap ps_cap,
               IBP_timer ps_timeout,
               int pi_cmd, int pi_cap_type, IBP_CapStatus ps_info)
{

    int li_return;
    IURL *lp_iurl;
    char lc_line_buf[CMD_BUF_SIZE];
    time_t lt_life;
    time_t lt_now;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;
    time(&lt_now);

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (-1);
    }

    /*
     * get parameters
     */
    if ((lp_iurl =
         create_manage_IURL(ps_cap, ps_timeout->ClientTimeout)) == NULL)
        return (-1);

    if ((strcmp(lp_iurl->type, "MANAGE") != 0) && (pi_cmd != IBP_PROBE)) {
        IBP_errno = IBP_E_CAP_NOT_MANAGE;
        free_IURL(lp_iurl);
        return (-1);
    }

    /*
     * prepare the IBP manage message 
     */
    switch (pi_cmd) {
    case IBP_INCR:
    case IBP_DECR:
        if ((pi_cap_type != IBP_READCAP) && (pi_cap_type != IBP_WRITECAP)) {
            IBP_errno = IBP_E_INVALID_PARAMETER;
            free_IURL(lp_iurl);
            return (-1);
        }
        sprintf(lc_line_buf, "%d %d %s %d %d %d %d\n",
                IBPv031,
                IBP_MANAGE,
                lp_iurl->key,
                lp_iurl->type_key,
                pi_cmd, pi_cap_type, ps_timeout->ServerSync);
        break;
    case IBP_CHNG:
    case IBP_PROBE:
        if (ps_info == NULL) {
            IBP_errno = IBP_E_INVALID_PARAMETER;
            free_IURL(lp_iurl);
            return (-1);
        }
        lt_life =
            (ps_info->attrib.duration >
             0 ? ps_info->attrib.duration - lt_now : 0);
        sprintf(lc_line_buf, "%d %d %s %d %d %d %lu %ld %d %d\n", IBPv031,
                IBP_MANAGE, lp_iurl->key, lp_iurl->type_key, pi_cmd,
                pi_cap_type, ps_info->maxSize, lt_life,
                ps_info->attrib.reliability, ps_timeout->ServerSync);
        break;
    default:
        IBP_errno = IBP_E_INVALID_CMD;
        free_IURL(lp_iurl);
        return (-1);
    }

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * fill out the units
     */
    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL) != IBP_OK) {
        free_IURL(lp_iurl);
        return (-1);
    }

    /*
     * receive the server's response
     */
    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf, CMD_BUF_SIZE,
         DATAVAR, (func_pt *) HandleStoreResp) != IBP_OK) {
        free_IURL(lp_iurl);
        return (-1);
    }

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL) {
        free_IURL(lp_iurl);
        return (-1);
    }


    if (pi_cmd == IBP_PROBE) {
        if (sscanf(lp_comm_unit->data_buf, "%d %d %d %d %lu %ld %d %d",
                   &li_return,
                   &ps_info->readRefCount,
                   &ps_info->writeRefCount,
                   &ps_info->currentSize,
                   &ps_info->maxSize,
                   &ps_info->attrib.duration,
                   &ps_info->attrib.reliability,
                   &ps_info->attrib.type) != 8) {
            IBP_errno = IBP_E_GENERIC;
            free_IURL(lp_iurl);
            return (-1);
        }
        ps_info->attrib.duration += lt_now;
    }

    free_IURL(lp_iurl);
    return (0);


}                              /* IBP_manager */


/*****************************/
/* IBP_status                */
/* It manages the parameters in a depot.  It can be only used by administrator     */
/* It is an important IBP API         */
/* return -- data info             */
/*****************************/
IBP_DptInfo IBP_status(IBP_depot ps_depot,
                       int pi_StCmd,
                       IBP_timer ps_timeout,
                       char *pc_password,
                       unsigned long int pl_StableStor,
                       unsigned long int pl_VolStor, long pl_Duration)
{

    int li_fd, li_passflag = 0;
    int li_return;
    char lc_line_buf[CMD_BUF_SIZE];
    char lc_new_buf[CMD_BUF_SIZE];
    IBP_DptInfo ls_dpt_info;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;
    int i;

    /*
     * initialize errno variable
     */
    IBP_errno = IBP_OK;
    ls_dpt_info = &glb_dpt_info;

    /*
     * check parameters
     */
    if ((li_return =
         check_depot(ps_depot->host, &(ps_depot->port))) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (NULL);
    }

    /*
     * check password
     */
    if (pc_password == NULL) {
        pc_password = malloc(16);
        if (pc_password != NULL) {
            sprintf(pc_password, "ibp\n");
            li_passflag = 1;
        }
    }

    if ((pi_StCmd != IBP_ST_INQ) && (pi_StCmd != IBP_ST_CHANGE)) {
        IBP_errno = IBP_E_GENERIC;
        return (NULL);
    }

    /*
     * connect to the server 
     */

    if ((li_fd =
         connect_socket(ps_depot->host, ps_depot->port,
                        ps_timeout->ClientTimeout)) == -1) {
        IBP_errno = IBP_E_CONNECTION;
        return (NULL);
    }

    if (set_socket_noblock(li_fd) != 0) {
        IBP_errno = IBP_E_CONNECTION;
        close_socket(li_fd);
        return (NULL);
    }

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_timeout->ClientTimeout);

    /*
     * send IBP_MANAGE command to server
     */


    sprintf(lc_line_buf, "%d %d %d %s %d\n",
            IBPv031,
            IBP_STATUS, pi_StCmd, pc_password, ps_timeout->ServerSync);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, li_fd, lc_line_buf, strlen(lc_line_buf),
         DATAVAR, NULL) != IBP_OK) {
        close_socket(li_fd);
        return (NULL);
    }

    if (pi_StCmd == IBP_ST_CHANGE) {
        if (FillCommUnit
            (&ls_comm_session, COM_IN, li_fd, lc_line_buf,
             sizeof(lc_line_buf), DATAVAR,
             (func_pt *) HandleStoreResp) != IBP_OK) {
            close_socket(li_fd);
            return (NULL);
        }

        sprintf(lc_new_buf, "%lu %lu %ld \n",
                pl_StableStor, pl_VolStor, pl_Duration);
        if (FillCommUnit
            (&ls_comm_session, COM_OUT, li_fd, lc_new_buf, strlen(lc_new_buf),
             DATAVAR, NULL) != IBP_OK) {
            close_socket(li_fd);
            return (NULL);
        }
    }

    if (FillCommUnit
        (&ls_comm_session, COM_IN, li_fd, lc_line_buf, sizeof(lc_line_buf),
         DATAVAR, NULL) != IBP_OK) {
        close_socket(li_fd);
        return (NULL);
    }



    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&(ls_comm_session))) == NULL) {
        close_socket(li_fd);
        return (NULL);
    }

    close_socket(li_fd);

    i = sscanf(lp_comm_unit->data_buf, "%lu %lu %lu %lu %ld %s %s ",
               &(ls_dpt_info->StableStor),
               &(ls_dpt_info->StableStorUsed),
               &(ls_dpt_info->VolStor),
               &(ls_dpt_info->VolStorUsed),
               &(ls_dpt_info->Duration),
               ls_dpt_info->VersionMajor, ls_dpt_info->VersionMinor);

    if (i < 5) {
        IBP_errno = IBP_E_BAD_FORMAT;
        return (NULL);
    }
    if (i == 5) {
        ls_dpt_info->VersionMajor[0] = '\0';
        ls_dpt_info->VersionMinor[0] = '\0';
    }
    else if (i == 6) {
        ls_dpt_info->VersionMinor[0] = '\0';
    }



    if (li_passflag == 1)
        pc_password = NULL;

    return (ls_dpt_info);

}                              /* IBP_DptInfo */




/* ----------------- IBP_MCOPY  IMPLEMENTATION ------------------- */

/*******************************************************************/
/* IBP_mcopy                                                       */
/* Copy data from a src depot to some target IBP depots            */
/* receive parameters -- source cap, array of target caps, number  */
/* of targets, src timeout, array of target timeouts, file size,   */
/* file offset, array of datamover options (types: currently only  */
/* implemented for DM_UNI and DM_BLAST), array of target ports,    */
/* and service (datamover kind: DM_MULTI, DM_SMULTI, DM_BLAST)     */
/* return -- none                                                  */
/*******************************************************************/

ulong_t IBP_mcopy(IBP_cap pc_SourceCap,
                  IBP_cap pc_TargetCap[],
                  unsigned int pi_CapCnt,
                  IBP_timer ps_src_timeout,
                  IBP_timer ps_tgt_timeout,
                  ulong_t pl_size,
                  ulong_t pl_offset,
                  int dm_type[], int dm_port[], int dm_service)
{

    int li_return, retval, sizeans = 2,p;
    IURL *lp_iurl;
    char lc_cap_buf[MAX_CAP_LEN];
    char lc_line_buf[CMD_BUF_SIZE], *lc_line_buf2;
    IBP_depot ls_depot;
    ulong_t ll_turesize, li_SizeOfBuffer;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;
    char *ps_targets, *ps_ports, *ps_options, *ps_targsync, *ps_targtout;
    char ans[2];

  /**fprintf(stderr,"in IBP_mcopy call: <<<%s>>>\n",pc_SourceCap);*/
    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_src_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    if (pc_TargetCap == NULL) {
        IBP_errno = IBP_E_WRONG_CAP_FORMAT;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }

    /*
     * get parameters
     */
    if ((lp_iurl =
         create_read_IURL(pc_SourceCap,
                          ps_src_timeout->ClientTimeout)) == NULL)
        return (0);

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_src_timeout->ClientTimeout);

    ps_targets = DM_Array2String(pi_CapCnt, (void *) pc_TargetCap, HOSTS);
    ps_ports = DM_Array2String(pi_CapCnt, (void *) dm_port, PORTS);
    ps_options = DM_Array2String(pi_CapCnt, (void *) dm_type, OPTIONS);
    ps_targsync =
        DM_Array2String(pi_CapCnt, (void *) ps_tgt_timeout, SERVSYNCS);
    ps_targtout =
        DM_Array2String(pi_CapCnt, (void *) ps_tgt_timeout, TIMEOUTS);

    /*
     * send IBP_MCOPY commands to server
     */

    li_SizeOfBuffer = CMD_BUF_SIZE * pi_CapCnt;

    sprintf(lc_line_buf, "%d %d %d\n", IBPv031, IBP_MCOPY, li_SizeOfBuffer);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL)
        != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }
  /********
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL) 
      != IBP_OK ){
    free_IURL(lp_iurl);
    return(0);
  }
  */

    lc_line_buf2 = (char *) malloc(sizeof(char) * li_SizeOfBuffer);

    sprintf(lc_line_buf2,
            "%d %d +s %s +t%s +k %d +f %lu +z %lu +i %d %s %s +o%s +p %s +n %d +d %d\n",
            IBPv031, IBP_MCOPY, lp_iurl->key, ps_targets, lp_iurl->type_key,
            pl_offset, pl_size, ps_src_timeout->ServerSync, ps_targsync,
            ps_targtout, ps_options, ps_ports, pi_CapCnt, dm_service);

    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf2,
         strlen(lc_line_buf2), DATAVAR, NULL)
        != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */

    if (FillCommUnit
        (&ls_comm_session, COM_IN, lp_iurl->fd, lc_line_buf,
         sizeof(lc_line_buf), DATAVAR, (func_pt *) HandleMcopyResp)
        != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }
   


    free_IURL(lp_iurl);
    free(ps_targets);
    free(ps_ports);
    free(ps_options);
    free(ps_targsync);
    free(ps_targtout);
    free(lc_line_buf2);

    return (lp_comm_unit->data_size);

}

/* ---------------------------------------------------------------- */


/* ---------------- IBP DATAMOVER INTERNAL CMD -------------------- */
ulong_t IBP_datamover(IBP_cap pc_TargetCap,
                      IBP_cap pc_ReadCap,
                      IBP_timer ps_tgt_timeout,
                      ulong_t pl_size,
                      ulong_t pl_offset,
                      int dm_type, int dm_port, int dm_service)
{

    int li_return;
    IURL *lp_iurl;
    char lc_cap_buf[MAX_CAP_LEN];
    char lc_line_buf[CMD_BUF_SIZE], *lc_line_buf2;
    IBP_depot ls_depot;
    ulong_t ll_turesize, li_SizeOfBuffer;
    IBP_CommSession ls_comm_session;
    IBP_CommUnit_t *lp_comm_unit;

    /*
     * initialize error variable
     */
    IBP_errno = IBP_OK;

    /*
     * check time out
     */
    if ((li_return = check_timeout(ps_tgt_timeout)) != IBP_OK) {
        IBP_errno = li_return;
        return (0);
    }

    if (pc_TargetCap == NULL) {
        IBP_errno = IBP_E_WRONG_CAP_FORMAT;
        return (0);
    }

    /*
     * check data size 
     */
    if (pl_size <= 0) {
        IBP_errno = IBP_E_INV_PAR_SIZE;
        return (0);
    }

    if ((lp_iurl =
         create_read_IURL(pc_ReadCap, ps_tgt_timeout->ClientTimeout)) == NULL)
        return (0);

    /*
     * initialize communication session
     */
    InitCommSession(&ls_comm_session, ps_tgt_timeout->ClientTimeout);


    /*
     * send IBP_DATAMOVER command to server
     */

    li_SizeOfBuffer = CMD_BUF_SIZE;

    sprintf(lc_line_buf, "%d %d %d\n\x0",
            IBPv031, IBP_MCOPY, li_SizeOfBuffer, EOF);

  /***fprintf(stderr,"1) IBP_datamover sending 1: <%s>\n",lc_line_buf);*/
    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf,
         strlen(lc_line_buf), DATAVAR, NULL)
        != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

  /*******
  if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL) 
      != IBP_OK ){
    free_IURL(lp_iurl);
    return(0);
  }
  */

    lc_line_buf2 = (char *) malloc(sizeof(char) * li_SizeOfBuffer);

    sprintf(lc_line_buf2, "%s %s %d %lu %lu %d %d %d %d %d\n",
          /**IBPv031,
	     IBP_MCOPY,*/
            lp_iurl->key,
            capToString(pc_TargetCap),
            lp_iurl->type_key,
            pl_offset, pl_size,
            ps_tgt_timeout->ServerSync,
            ps_tgt_timeout->ClientTimeout, dm_type, dm_port, DM_SERVER);

  /***fprintf(stderr,"2) IBP_datamover sending: <%s>\n",lc_line_buf2); */
    if (FillCommUnit
        (&ls_comm_session, COM_OUT, lp_iurl->fd, lc_line_buf2,
         strlen(lc_line_buf2), DATAVAR, NULL)
        != IBP_OK) {
        free_IURL(lp_iurl);
        return (0);
    }

    /*
     * receive the server's response
     */

    /*   if (FillCommUnit(&ls_comm_session,COM_IN,lp_iurl->fd,lc_line_buf,sizeof(lc_line_buf),DATAVAR,NULL) 
       != IBP_OK ){
       free_IURL(lp_iurl);
       return(0);
       }
     */

    /*
     * do communication transaction
     */
    if ((lp_comm_unit = PerformCommunication(&ls_comm_session)) == NULL) {
        free_IURL(lp_iurl);
        return (0);
    }
    free_IURL(lp_iurl);

    return (lp_comm_unit->data_size);

}

/* --------------------------------------------------- */


/* ------------- DATA MOVER: Arrays/string conversion ------------- */
/* Array2String: for use by data mover client                       */
/* converts the given array into a formated string, depending on    */
/* the array "type" parameter, returns the string                   */
/* -----------------------------------------------------------------*/
char *DM_Array2String(int numelems, void *array[], int type)
{
    char *target, temp[16], *tempcap;
    int i;
    target = (char *) malloc(sizeof(char) * numelems * MAX_CAP_LEN);
    bzero(target, sizeof(char) * numelems * MAX_CAP_LEN);

    switch (type) {
    case HOSTS:
        /*strcat(target,"+t"); */
        for (i = 0; i < numelems; i++) {
            strcat(target, " ");
            tempcap = capToString((IBP_cap) array[i]);  /* check this IBP_cap * data type */
            strcat(target, tempcap);
        }
        break;
    case KEYS:
        /*strcat(target,"+k"); */
        for (i = 0; i < numelems; i++) {
            sprintf(temp, " %d", (int) array[i]);
            strcat(target, temp);
        }
        break;
    case PORTS:
        /*strcat(target,"+p"); */
        for (i = 0; i < numelems; i++) {
            sprintf(temp, " %d", (int) array[i]);
            strcat(target, temp);
        }
        break;
    case OPTIONS:
        /*strcat(target,"+o"); */
        for (i = 0; i < numelems; i++) {
            sprintf(temp, " %d", (int) array[i]);
            strcat(target, temp);
        }
        break;
    case SERVSYNCS:
        for (i = 0; i < numelems; i++) {
            sprintf(temp, " %d", (int) (((IBP_timer) array)[i].ServerSync));
            strcat(target, temp);
        }
        break;
    case TIMEOUTS:
        for (i = 0; i < numelems; i++) {
            sprintf(temp, " %d",
                    (int) (((IBP_timer) array)[i].ClientTimeout));
            strcat(target, temp);
        }
        break;
    }
    return target;
}



#ifndef IBP_WIN32

#ifdef HAVE_PTHREAD_H

pthread_once_t dpt_info_once = PTHREAD_ONCE_INIT;
static pthread_key_t dpt_info_key;

void _dpt_info_destructor(void *ptr)
{
    free(ptr);
}

void _dpt_info_once(void)
{
    pthread_key_create(&dpt_info_key, _dpt_info_destructor);
}

struct ibp_dptinfo *_glb_dpt_info(void)
{
    struct ibp_dptinfo *output;

    pthread_once(&dpt_info_once, _dpt_info_once);
    output = (struct ibp_dptinfo *) pthread_getspecific(dpt_info_key);
    if (output == NULL) {
        output = (struct ibp_dptinfo *) calloc(1, sizeof(struct ibp_dptinfo));
        pthread_setspecific(dpt_info_key, output);
    }
    return (output);
}

#endif

#endif /* IBP_WIN32 */
