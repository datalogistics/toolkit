/*
 *           IBP version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *         Authors: X. Li, A. Bassi, J. Plank, M. Beck
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
 * ibp_unixlib.c
 *
 * IBP Unix server lib code. Version name: primeur
 * Library of functions used by the server
 *
 */

# include "config-ibp.h"
# include "ibp_thread.h"
# ifdef HAVE_PTHREAD_H
# include <pthread.h>
# endif
# ifdef STDC_HEADERS
# include <assert.h>
# include <sys/types.h>
# endif
#include "ibp_server.h"

#ifdef HAVE_SYS_STATVFS_H 
#include <sys/statvfs.h>
#elif HAVE_SYS_VFS_H
#include <sys/vfs.h>
#else
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#endif

#include "ibp_resources.h"

/*****************************/
/* vol_cap_insert            */
/* It inserts the new volatile capability into the VolatileTree, with the */
/* multiplier of capability's size and lifetime */
/* return: none              */
/*****************************/
#if 0
JRB vol_cap_insert(int pi_size, int pi_lifetime, Jval pu_val){
    return NULL;
}
#endif
JRB vol_cap_insert(RESOURCE *rs ,int pi_size, int pi_lifetime, Jval pu_val)
{
  int li_volvalue;
  JRB lu_new;

  assert (rs != NULL);
  pi_size = pi_size >> 10;
  if (pi_size == 0)
  {
    pi_size = 1;
  }

  pi_lifetime = pi_lifetime >> 10;
  if (pi_lifetime == 0)
  {
    pi_lifetime = 1;
  }

  li_volvalue = pi_size * pi_lifetime;
  lu_new = jrb_insert_int (rs->VolatileTree, li_volvalue, pu_val);

  return(lu_new);
}
 
/*****************************/
/* vol_space_check           */
/* It checks the space for the new volatile requirement. */
/* If there is no enough space, it may delete some more volatile caps */
/* return: none              */
/*****************************/
#if 0
int volstorage_check(long pi_size, int pi_lifetime)
{
  long li_volvalue;
  long li_availstorage;
  int li_key;
  IBP_byteArray         ls_ByteArray;
  IBP_request           ls_req;
  JRB                   ls_rb; 
  Jval                  lu_val;
  Jval                  lu_key;


  if (pi_size <= glb.VolStorage - glb.VolStorageUsed)
  {
    return(1);
  }

  if (pi_size > glb.VolStorage)
  {
    return(IBP_E_WOULD_EXCEED_LIMIT);
  }

  pi_size = pi_size >> 10;
  if (pi_size == 0)
  {
    pi_size = 1;
  }
  pi_lifetime = pi_lifetime >> 10;
  if (pi_lifetime == 0)
  {
    pi_lifetime = 1;
  }
  li_volvalue = pi_size * pi_lifetime;

  li_availstorage = glb.VolStorage - glb.VolStorageUsed;
  jrb_rtraverse(ls_rb, glb.VolatileTree)
  {
    lu_key = ls_rb->key;
    li_key = (int) lu_key.i;
    if (li_key < li_volvalue)
    {  
      break;
    } 

    lu_val = jrb_val (ls_rb);
    ls_ByteArray = (IBP_byteArray) lu_val.s;

    if (ls_ByteArray->deleted == 1)
      continue;
    else if ((ls_ByteArray->writeLock == 0) && (ls_ByteArray->readLock == 0) \
           && dl_empty(ls_ByteArray->pending)) {
      li_availstorage += ls_ByteArray->maxSize;
      jettisonByteArray(ls_ByteArray);
      if (li_availstorage >= pi_size)
      {
        break;
      }
    }
    else {
      ls_ByteArray->deleted = 1;
      ls_req = (IBP_request) calloc (1, sizeof (struct ibp_request));
      ls_req->request = IBP_DECR;
      ls_req->fd = -1;
      dl_insert_b(ls_ByteArray->pending, ls_req);
    } /* end if */
  }

  if (li_availstorage >= pi_size)
  {
    return(1);
  }
  else
  {
    return(IBP_E_WOULD_EXCEED_LIMIT);
  } 
}
#endif 

#if 0
int delete_volatile_caps( long pi_size, int pi_mode){
    return 0;
}
#endif

int delete_volatile_caps(RESOURCE *rs, ibp_long pi_size,int pi_mode)
{
  IBP_byteArray         ls_ByteArray;
  JRB                   ls_rb;
  Jval                  lu_val;
  long                  tmp;

  assert (rs != NULL);

  if (pi_mode == 4)
  {
    tmp = rs->VolStorageUsed + rs->StableStorageUsed - rs->VolStorage;
    if (tmp > pi_size)
      pi_size = tmp;
    if (pi_size <= 0)
      return(0);
  }
again:
  pthread_mutex_lock(&(rs->change_lock));
  jrb_rtraverse(ls_rb, rs->VolatileTree)
  {
    lu_val = jrb_val (ls_rb);
    ls_ByteArray = (IBP_byteArray) lu_val.s;

    /*
    if (ls_ByteArray->deleted == 1)
      continue;
    else
    */
    if ((ls_ByteArray->writeLock == 0) && (ls_ByteArray->readLock == 0) \
           && dl_empty(ls_ByteArray->pending)) 
    {
      pi_size -= ls_ByteArray->maxSize;
      if (pi_mode == 4)
        ls_ByteArray->deleted = 2;
      else{
        ls_ByteArray->deleted = 1;
        pthread_mutex_unlock(&(rs->change_lock));
        jettisonByteArray(rs,ls_ByteArray);
        goto again;
      }
      if (pi_size <= 0)
      {
        break;
      }
    }
    else 
    {
      if (pi_mode == 4)
        ls_ByteArray->deleted = 2;
      else
        ls_ByteArray->deleted = 1;
      /*
      ls_req = (IBP_request) calloc (1, sizeof (struct ibp_request));
      ls_req->request = IBP_DECR;
      ls_req->fd = -1;
      dl_insert_b(ls_ByteArray->pending, ls_req);
      */
    } /* end if */
  }
  pthread_mutex_unlock(&(rs->change_lock));
  return(0);
}

#if 0
int check_size( long pi_size, int pi_mode){
    return 0;
}
#endif

int getRsFreeSize(RESOURCE *rs, ibp_long *freeSize ){

#ifdef HAVE_STATVFS
  struct statvfs ls_buf;
#elif HAVE_STATFS
  struct statfs ls_buf;
#endif
 
  assert ( (rs != NULL) && ( freeSize != NULL) );

  switch ( rs->type ){ 
  case DISK:
#ifdef HAVE_STATVFS 
      if (statvfs(rs->loc.dir, &ls_buf) != 0) 
#elif HAVE_STATFS  
      if (statfs(rs->loc.dir, &ls_buf) != 0)
#endif
      {
          return -1;
      }
      *freeSize = (ibp_long)ls_buf.f_bsize *(ibp_long)ls_buf.f_bfree;
      break;
  case MEMORY:
  case ULFS:
      *freeSize = rs->VolStorage - rs->VolStorageUsed - rs->StableStorageUsed;
      break;
  default:
      break;
  }

  return 0;
}

int check_size( RESOURCE *rs, long pi_size, int pi_mode)
{
  ibp_long   li_freesize;
  time_t     _lt_now = time(0);
  double     _f1, _f2, _res; 

  assert ( rs != NULL );

  if ( getRsFreeSize(rs, &li_freesize ) < 0 ){
      return (IBP_E_ALLOC_FAILED);
  }

#if 0
#ifdef HAVE_STATVFS 
#ifdef HAVE_STRUCT_STATVFS64_F_BSIZE
  struct statvfs64 ls_buf;
  if (statvfs64(glb.VolDir,&ls_buf) != 0)
#else
  struct statvfs ls_buf;
  if (statvfs(glb.VolDir, &ls_buf) != 0) 
#endif
#elif HAVE_STATFS  
  struct statfs ls_buf;
  if (statfs(glb.VolDir, &ls_buf) != 0)
#endif 

  {
    return(IBP_E_ALLOC_FAILED);
  }
  li_freesize = ls_buf.f_bsize * ls_buf.f_bfree;
#endif

  /*if ((pi_mode == 4) && (ls_buf.f_bfree < glb.MinFreeSize/ls_buf.f_bsize))*/
  if ((pi_mode == 4) && (li_freesize < rs->MinFreeSize))
  {
    delete_volatile_caps(rs,rs->MinFreeSize - li_freesize, pi_mode);
    return(0);
  }
  if ( li_freesize < (rs->MinFreeSize+pi_size))
  {
    if (li_freesize < rs->MinFreeSize)
    {
      delete_volatile_caps(rs,rs->MinFreeSize - li_freesize, pi_mode);
    }
    return(IBP_E_WOULD_EXCEED_LIMIT);
  }

  if (pi_mode == IBP_ROUTINE_CHECK)
  {
    _f1 = (float)(rs->StableStorage>>20);
    _f2 = (float)(rs->StableStorageUsed>>20);
    _res = _f2/_f1;
    if ( _res > 0.7 ){
      truncatestorage(rs,2,(void*)&_lt_now);
    }
    return(0);
  }
  if (pi_size + rs->StableStorageUsed + rs->VolStorageUsed > rs->VolStorage)
  {
    return(IBP_E_WOULD_EXCEED_LIMIT);
  }
  if ((pi_mode == IBP_STABLE) && (pi_size + rs->StableStorageUsed > rs->StableStorage))
  {
    truncatestorage(rs,2,(void*)&_lt_now);
    if ( (pi_size + rs->StableStorageUsed) > rs->StableStorage ){
    	return(IBP_E_WOULD_EXCEED_LIMIT);
    }
  }

  return(0);
}
           
int recover_finish()
{
  IBP_byteArray         ls_ByteArray;
  JRB                   ls_rb;
  Jval                  lu_val;
  RESOURCE              *rs;
  JRB                   node;

  if ( glb.IBPRecover == 1 ){
    return 0;
  }


  jrb_traverse(node, glb.resources ){
    rs = (RESOURCE *)((jrb_val(node)).v);
    switch(rs->type){
      case DISK:
again:
        pthread_mutex_lock(&(rs->change_lock));
        jrb_rtraverse(ls_rb, rs->VolatileTree)
        {
          lu_val = jrb_val (ls_rb);
          ls_ByteArray = (IBP_byteArray) lu_val.s;

          if (ls_ByteArray->deleted == 2)
          {
            if ((ls_ByteArray->writeLock == 0) && (ls_ByteArray->readLock == 0) \
              && dl_empty(ls_ByteArray->pending))
            {
              ls_ByteArray->deleted = 1;
              pthread_mutex_unlock(&(rs->change_lock));
              jettisonByteArray(rs,ls_ByteArray);
              goto again;
            }
            else
            {
              ls_ByteArray->deleted = 1;
/*
              ls_req = (IBP_request) calloc (1, sizeof (struct ibp_request));
              ls_req->request = IBP_DECR;
              ls_req->fd = -1;
              dl_insert_b(ls_ByteArray->pending, ls_req);
*/
            } /* end if */
          }
        }
        pthread_mutex_unlock(&(rs->change_lock));
        break;
      case MEMORY:
      case RMT_DEPOT:
      default:
        break;
    }
  }
  return(0);
}
   
  
