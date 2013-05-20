/*
 *           IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *         Authors: A. Bassi, W. Elwasif, J. Plank, M. Beck
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
 * ibp_server.c
 *
 * IBP server code. Version name: primeur
 * Library of functions used by the server
 *
 */
# include "config-ibp.h"
# ifdef STDC_HEADERS 
# include <sys/types.h>
# include <assert.h>
# endif /* STDC_HEADERS */
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_thread.h"

/*****************************/
/* handle_manage             */
/* It handles the user's manage requirement, envoked by handle_require() */
/* There are four management commands: INCR, DECR, PROBE and CHNG  */
/* return: the name of manage cap     */
/*****************************/
char *handle_manage_disk ( RESOURCE *rs , int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  time_t          lt_CurTime;
  time_t          lt_NewDuration;
  char            lc_LineBuf[CMD_BUF_SIZE];
  long            ll_inc;
  char            *lc_Return;
  Jval            lu_val;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_manage beginning\n");
#endif

#ifdef IBP_DEBUG
  fprintf (stderr, "in handle manage\n");
#endif

  lc_Return = malloc (PATH_MAX);
  strcpy (lc_Return, ps_container->fname);
  ps_container->reference --;

  if ((strcmp(ps_container->manageKey_s,ibp_cmd[pi_nbt].mKey) != 0) && (ibp_cmd[pi_nbt].ManageCmd != IBP_PROBE)) {
    handle_error (IBP_E_INVALID_MANAGE_CAP, &pi_fd);
    return lc_Return;
  }
  if (strcmp(ps_container->manageKey_s,ibp_cmd[pi_nbt].mKey) != 0 &&
      strcmp(ps_container->readKey_s,ibp_cmd[pi_nbt].mKey) != 0 &&
      strcmp(ps_container->writeKey_s,ibp_cmd[pi_nbt].mKey) != 0) {
    handle_error (IBP_E_INVALID_MANAGE_CAP, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
    return lc_Return;
  }
  
  time(&lt_CurTime);
  switch(ibp_cmd[pi_nbt].ManageCmd) {
   
  case IBP_INCR:

    if (ibp_cmd[pi_nbt].CapType == IBP_READCAP)
      ps_container->readRefCount++;
    else if (ibp_cmd[pi_nbt].CapType == IBP_WRITECAP)
      ps_container->writeRefCount++;
    else {
      handle_error (IBP_E_INVALID_PARAMETER, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
      return lc_Return;
    }

    SaveInfo2LSS(rs,ps_container, 2);

    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  case IBP_DECR:

    if (ibp_cmd[pi_nbt].CapType == IBP_READCAP) {
      ps_container->readRefCount--;

      if (ps_container->readRefCount == 0) {	
        if ((ps_container->writeLock == 0) && 
	    (ps_container->readLock == 0) &&
	    dl_empty(ps_container->pending)) {
          SaveInfo2LSS(rs,ps_container, 2);
	  jettisonByteArray(rs,ps_container);
	}
	else {
	  ps_container->deleted = 1;
	}
      }
    }   
    else if (ibp_cmd[pi_nbt].CapType == IBP_WRITECAP) {
      if (ps_container->writeRefCount == 0) {
	      handle_error(IBP_E_CAP_ACCESS_DENIED, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
	      return lc_Return;
      }
      ps_container->writeRefCount--;
    }
    else {
      handle_error (IBP_E_INVALID_PARAMETER, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
      return lc_Return;
    }


    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  case IBP_PROBE:
    if (ps_container->deleted == 2) {
      handle_error (IBP_E_CAP_DELETING, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
      return lc_Return;
    }

    if ((ps_container->attrib.type == IBP_BYTEARRAY) ||
            (ps_container->attrib.type == IBP_BUFFER))
      sprintf (lc_LineBuf,"%d %d %d %lu %lu %ld %d %d \n",
	       IBP_OK,
	       ps_container->readRefCount,
	       ps_container->writeRefCount,
	       ps_container->currentSize,
	       ps_container->maxSize,
	       ps_container->attrib.duration - lt_CurTime,
	       ps_container->attrib.reliability,
	       ps_container->attrib.type);
    else
      sprintf (lc_LineBuf,"%d %d %d %lu %lu %lu %d %d \n",
	       IBP_OK,
	       ps_container->readRefCount,
	       ps_container->writeRefCount, 
	       ps_container->size - ps_container->free,
	       ps_container->maxSize,
	       ps_container->attrib.duration - lt_CurTime, 
	       ps_container->attrib.reliability, 
	       ps_container->attrib.type); 
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    return lc_Return;

  case IBP_CHNG:
    if (((ps_container->attrib.type == IBP_FIFO) || 
	 (ps_container->attrib.type == IBP_CIRQ)) && 
	(ibp_cmd[pi_nbt].NewSize != ps_container->maxSize)) {
      handle_error (IBP_E_WOULD_DAMAGE_DATA, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
      return lc_Return;
    }

    if ((rs->MaxDuration == 0) ||
	((rs->MaxDuration > 0) && ( (ibp_cmd[pi_nbt].lifetime <= 0) ||
		    (rs->MaxDuration < ibp_cmd[pi_nbt].lifetime) ))) {
      handle_error (IBP_E_LONG_DURATION, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
      return lc_Return;
    }
      
    lt_NewDuration = (ibp_cmd[pi_nbt].lifetime > 0 ? lt_CurTime + ibp_cmd[pi_nbt].lifetime : 0);

    if (ibp_cmd[pi_nbt].reliable != ps_container->attrib.reliability) {
      handle_error (IBP_E_INV_PAR_ATRL, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_INVALID_MANAGE_CAP;
      return lc_Return;
    }

    if (check_allocate(rs,pi_fd, pi_nbt, ibp_cmd[pi_nbt].NewSize) != 0) {
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }

    ll_inc = ibp_cmd[pi_nbt].NewSize - ps_container->maxSize;

# ifdef HAVE_PTHREAD_H
    pthread_mutex_lock (&(rs->change_lock));
# endif
    if (ibp_cmd[pi_nbt].reliable == IBP_VOLATILE) {
      pthread_mutex_unlock(&(rs->change_lock));
      if (check_size(rs,ll_inc, IBP_VOLATILE) != 0)
      {
# ifdef HAVE_PTHREAD_H
        /*pthread_mutex_unlock (&change_lock);*/
# endif
	handle_error (IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	return lc_Return;
      }
      pthread_mutex_lock(&(rs->change_lock));
      rs->VolStorageUsed += ll_inc;
      ps_container->maxSize = ibp_cmd[pi_nbt].NewSize;
    }
    else {
      pthread_mutex_unlock(&(rs->change_lock));
      if (check_size(rs,ll_inc, IBP_STABLE) != 0)
      {
# ifdef HAVE_PTHREAD_H
        /*pthread_mutex_unlock (&change_lock);*/
# endif
        handle_error (IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
        return lc_Return;
      }
      pthread_mutex_lock(&(rs->change_lock));
      rs->StableStorageUsed += ll_inc;
      ps_container->maxSize = ibp_cmd[pi_nbt].NewSize;
    }

    if (lt_NewDuration != ps_container->attrib.duration) {
      if (ps_container->attrib.duration != 0) {
	      jrb_delete_node(ps_container->transientTreeEntry);
	      ps_container->transientTreeEntry = NULL;
      }
      if (lt_NewDuration != 0) {
	      lu_val.s = (char *) ps_container;
	      ps_container->transientTreeEntry =
	      jrb_insert_int (rs->TransientTree, lt_NewDuration, lu_val);
      }
      ps_container->attrib.duration = lt_NewDuration;
    }

# ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock (&(rs->change_lock));
# endif

    SaveInfo2LSS(rs,ps_container, 2);

    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  default:
    handle_error (IBP_E_INVALID_CMD, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
    return lc_Return;
  }

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_manage done\n");
#endif
#ifdef IBP_DEBUG
  fprintf (stderr, " handle manage done\n");
#endif
  return lc_Return;
}


/*****************************/
/* handle_manage_ram          */
/* It handles the user's manage requirement, envoked by handle_require() */
/* There are four management commands: INCR, DECR, PROBE and CHNG  */
/* return: the name of manage cap     */
/*****************************/
char *handle_manage_ram ( RESOURCE *rs,int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  time_t         lt_CurTime;
  time_t         lt_NewDuration;
  char           lc_LineBuf[CMD_BUF_SIZE];
  long           ll_inc;
  char           *lc_Return;
  Jval           lu_val;

  assert (rs != NULL );
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_manage beginning\n");
#endif

#ifdef IBP_DEBUG
  fprintf (stderr, "in handle manage\n");
#endif

#if 0
  lc_Return = malloc (PATH_MAX);
  strcpy (lc_Return, ps_container->fname);
#endif
  ps_container->reference--;
  lc_Return = NULL;

  if ((ps_container->manageKey != ibp_cmd[pi_nbt].ManageKey) && (ibp_cmd[pi_nbt].ManageCmd != IBP_PROBE)) {
    handle_error (IBP_E_INVALID_MANAGE_CAP, &pi_fd);
    return lc_Return;
  }
  if ((ps_container->manageKey != ibp_cmd[pi_nbt].ManageKey) &&
      (ps_container->readKey != ibp_cmd[pi_nbt].ManageKey) &&
      (ps_container->writeKey != ibp_cmd[pi_nbt].ManageKey)) {
    handle_error (IBP_E_INVALID_MANAGE_CAP, &pi_fd);
    IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
    return lc_Return;
  }
  
  time(&lt_CurTime);
  switch(ibp_cmd[pi_nbt].ManageCmd) {
   
  case IBP_INCR:

    if (ibp_cmd[pi_nbt].CapType == IBP_READCAP)
      ps_container->readRefCount++;
    else if (ibp_cmd[pi_nbt].CapType == IBP_WRITECAP)
      ps_container->writeRefCount++;
    else {
      handle_error (IBP_E_INVALID_PARAMETER, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }

    SaveInfo2LSS(rs,ps_container, 2);

    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  case IBP_DECR:

    if (ibp_cmd[pi_nbt].CapType == IBP_READCAP) {
      ps_container->readRefCount--;

      if (ps_container->readRefCount == 0) {	
        if ((ps_container->writeLock == 0) && 
	    (ps_container->readLock == 0) &&
	    dl_empty(ps_container->pending)) {
          SaveInfo2LSS(rs,ps_container, 2);
	        jettisonByteArray(rs,ps_container);
	      }
	      else {
	        ps_container->deleted = 1;
	      }
      }
    }   
    else if (ibp_cmd[pi_nbt].CapType == IBP_WRITECAP) {
      if (ps_container->writeRefCount == 0) {
	      handle_error(IBP_E_CAP_ACCESS_DENIED, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      return lc_Return;
      }
      ps_container->writeRefCount--;
    }
    else {
      handle_error (IBP_E_INVALID_PARAMETER, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }


    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  case IBP_PROBE:
    if (ps_container->deleted == 2) {
      handle_error (IBP_E_CAP_DELETING, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }

    if ((ps_container->attrib.type == IBP_BYTEARRAY) || 
            (ps_container->attrib.type == IBP_BUFFER))
      sprintf (lc_LineBuf,"%d %d %d %lu %lu %ld %d %d \n",
	       IBP_OK,
	       ps_container->readRefCount,
	       ps_container->writeRefCount,
	       ps_container->currentSize,
	       ps_container->maxSize,
	       ps_container->attrib.duration - lt_CurTime,
	       ps_container->attrib.reliability,
	       ps_container->attrib.type);
    else
      sprintf (lc_LineBuf,"%d %d %d %lu %lu %lu %d %d \n",
	       IBP_OK,
	       ps_container->readRefCount,
	       ps_container->writeRefCount, 
	       ps_container->size - ps_container->free,
	       ps_container->maxSize,
	       ps_container->attrib.duration - lt_CurTime, 
	       ps_container->attrib.reliability, 
	       ps_container->attrib.type); 
/*
    printf("CapStatus:%s\n",lc_LineBuf);    
*/
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    return lc_Return;

  case IBP_CHNG:
#if 0
    /* ******** all change request will be denied for RAM depot ********* */
    handle_error (IBP_E_WOULD_DAMAGE_DATA, &pi_fd);
    return lc_Return;
    /* ********************* */
#endif

    if ( ibp_cmd[pi_nbt].NewSize != ps_container->maxSize){
      handle_error(IBP_E_WOULD_DAMAGE_DATA,&pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }
    /*
    if (((ps_container->attrib.type == IBP_FIFO) || 
	    (ps_container->attrib.type == IBP_CIRQ)) && 
	    (ibp_cmd[pi_nbt].NewSize != ps_container->maxSize)) {

      handle_error (IBP_E_WOULD_DAMAGE_DATA, &pi_fd);
      return lc_Return;
    }
    */

    if ((rs->MaxDuration == 0) ||
	    ((rs->MaxDuration > 0) && ( (ibp_cmd[pi_nbt].lifetime <= 0) ||
		  (rs->MaxDuration < ibp_cmd[pi_nbt].lifetime) ))) {

      handle_error (IBP_E_LONG_DURATION, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }
      
    lt_NewDuration = (ibp_cmd[pi_nbt].lifetime > 0 ? lt_CurTime + ibp_cmd[pi_nbt].lifetime : 0);

    if (ibp_cmd[pi_nbt].reliable != ps_container->attrib.reliability) {
      handle_error (IBP_E_INV_PAR_ATRL, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }

    if (check_allocate(rs,pi_fd, pi_nbt, ibp_cmd[pi_nbt].NewSize) != 0) {
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
      return lc_Return;
    }

    ll_inc = ibp_cmd[pi_nbt].NewSize - ps_container->maxSize;
#if 0
# ifdef IBP_THREAD
    pthread_mutex_lock (&change_lock);
# endif
    if (ibp_cmd[pi_nbt].reliable == IBP_VOLATILE) {
      if (check_size(rs,ll_inc, IBP_VOLATILE) != 0)
      {
# ifdef IBP_THREAD
        pthread_mutex_unlock (&change_lock);
# endif
	handle_error (IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
	return lc_Return;
      }
      rs->VolStorageUsed += ll_inc;
      ps_container->maxSize = ibp_cmd[pi_nbt].NewSize;
    }
    else {
      if (check_size(rs,ll_inc, IBP_STABLE) != 0)
      {
# ifdef IBP_THREAD
        pthread_mutex_unlock (&change_lock);
# endif
        handle_error (IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
        return lc_Return;
      }
      rs->StableStorageUsed += ll_inc;
      ps_container->maxSize = ibp_cmd[pi_nbt].NewSize;
    }
# ifdef IBP_THREAD
    pthread_mutex_unlock (&change_lock);
# endif
#endif

# ifdef IBP_THREAD
    pthread_mutex_lock (&(rs->change_lock));
# endif
    if (lt_NewDuration != ps_container->attrib.duration) {
      if (ps_container->attrib.duration != 0) {
	      jrb_delete_node(ps_container->transientTreeEntry);
	      ps_container->transientTreeEntry = NULL;
      }
      if (lt_NewDuration != 0) {
	      lu_val.s = (char *) ps_container;
	      ps_container->transientTreeEntry =
	      jrb_insert_int (rs->TransientTree, lt_NewDuration, lu_val);
      }
      ps_container->attrib.duration = lt_NewDuration;
    }
# ifdef IBP_THREAD
    pthread_mutex_unlock (&(rs->change_lock));
# endif

    SaveInfo2LSS(rs,ps_container, 2);

    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    break;

  default:
    handle_error (IBP_E_INVALID_CMD, &pi_fd);
      IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
    return lc_Return;
  }

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_manage done\n");
#endif
#ifdef IBP_DEBUG
  fprintf (stderr, " handle manage done\n");
#endif
  return lc_Return;
}
