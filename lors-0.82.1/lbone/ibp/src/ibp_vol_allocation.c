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


# include "ibp_thread.h"
# ifdef HAVE_PTHREAD_H
# include <pthread.h>
# endif

#include <sys/types.h>
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

/*****************************/
/* vol_cap_insert            */
/* It inserts the new volatile capability into the VolatileTree, with the */
/* multiplier of capability's size and lifetime */
/* return: none              */
/*****************************/

JRB vol_cap_insert(int pi_size, int pi_lifetime, Jval pu_val)
{
    int li_volvalue;
    JRB lu_new;

    pi_size = pi_size >> 10;
    if (pi_size == 0) {
        pi_size = 1;
    }

    pi_lifetime = pi_lifetime >> 10;
    if (pi_lifetime == 0) {
        pi_lifetime = 1;
    }

    li_volvalue = pi_size * pi_lifetime;
    lu_new = jrb_insert_int(glb.VolatileTree, li_volvalue, pu_val);

    return (lu_new);
}

/*****************************/
/* vol_space_check           */
/* It checks the space for the new volatile requirement. */
/* If there is no enough space, it may delete some more volatile caps */
/* return: none              */
/*****************************/

int volstorage_check(long pi_size, int pi_lifetime)
{
    long li_volvalue;
    long li_availstorage;
    int li_key;
    IBP_byteArray ls_ByteArray;
    IBP_request ls_req;
    JRB ls_rb;
    Jval lu_val;
    Jval lu_key;


    if (pi_size <= glb.VolStorage - glb.VolStorageUsed) {
        return (1);
    }

    if (pi_size > glb.VolStorage) {
        return (IBP_E_WOULD_EXCEED_LIMIT);
    }

    pi_size = pi_size >> 10;
    if (pi_size == 0) {
        pi_size = 1;
    }
    pi_lifetime = pi_lifetime >> 10;
    if (pi_lifetime == 0) {
        pi_lifetime = 1;
    }
    li_volvalue = pi_size * pi_lifetime;

    li_availstorage = glb.VolStorage - glb.VolStorageUsed;
  again:
    pthread_mutex_lock(&change_lock);
    jrb_rtraverse(ls_rb, glb.VolatileTree) {
        lu_key = ls_rb->key;
        li_key = (int) lu_key.i;
        if (li_key < li_volvalue) {
            break;
        }

        lu_val = jrb_val(ls_rb);
        ls_ByteArray = (IBP_byteArray) lu_val.s;

        if (ls_ByteArray->deleted == 1)
            continue;
        else if ((ls_ByteArray->writeLock == 0)
                 && (ls_ByteArray->readLock == 0)
                 && dl_empty(ls_ByteArray->pending)) {
            ls_ByteArray->deleted = 1;
            li_availstorage += ls_ByteArray->maxSize;
            pthread_mutex_unlock(&change_lock);
            jettisonByteArray(ls_ByteArray);
            goto again;
            if (li_availstorage >= pi_size) {
                break;
            }
        }
        else {
            ls_ByteArray->deleted = 1;
            ls_req = (IBP_request) calloc(1, sizeof(struct ibp_request));
            ls_req->request = IBP_DECR;
            ls_req->fd = -1;
            dl_insert_b(ls_ByteArray->pending, ls_req);
        }                       /* end if */
    }
    pthread_mutex_unlock(&change_lock);

    if (li_availstorage >= pi_size) {
        return (1);
    }
    else {
        return (IBP_E_WOULD_EXCEED_LIMIT);
    }
}


int delete_volatile_caps(long pi_size, int pi_mode)
{
    IBP_byteArray ls_ByteArray;
    IBP_request ls_req;
    JRB ls_rb;
    Jval lu_val;
    Jval lu_key;
    long tmp;

    if (pi_mode == 4) {
        tmp = glb.VolStorageUsed + glb.StableStorageUsed - glb.VolStorage;
        if (tmp > pi_size)
            pi_size = tmp;
        if (pi_size <= 0)
            return (0);
    }

  again:
    pthread_mutex_lock(&change_lock);
    jrb_rtraverse(ls_rb, glb.VolatileTree) {
        lu_val = jrb_val(ls_rb);
        ls_ByteArray = (IBP_byteArray) lu_val.s;
        /*
           if (ls_ByteArray->deleted == 1)
           continue;
           else */
        if ((ls_ByteArray->writeLock == 0) && (ls_ByteArray->readLock == 0)
            && dl_empty(ls_ByteArray->pending)) {
            pi_size -= ls_ByteArray->maxSize;
            if (pi_mode == 4)
                ls_ByteArray->deleted = 2;
            else {
                ls_ByteArray->deleted = 1;
                pthread_mutex_unlock(&change_lock);
                jettisonByteArray(ls_ByteArray);
                goto again;
            }
            if (pi_size <= 0) {
                break;
            }
        }
        else {
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
        }                       /* end if */
    }
    pthread_mutex_unlock(&change_lock);
    return (0);
}

int check_size(long pi_size, int pi_mode)
{
    ibp_ulong li_freesize;

#ifdef HAVE_STATVFS
#ifdef HAVE_STRUCT_STATVFS64_F_BSIZE
    struct statvfs64 ls_buf;
    if (statvfs64(glb.VolDir,&ls_buf) != 0)
#else /* HAVE_STATVFS64 */
    struct statvfs ls_buf;
    if (statvfs(glb.VolDir, &ls_buf) != 0)
#endif
#elif HAVE_STATFS
    struct statfs ls_buf;
    if (statfs(glb.VolDir, &ls_buf) != 0)
#endif

    {
        return (IBP_E_ALLOC_FAILED);
    }
    li_freesize = ls_buf.f_bsize * ls_buf.f_bfree;

    if ((pi_mode == 4) && (ls_buf.f_bfree < glb.MinFreeSize / ls_buf.f_bsize)) {
        delete_volatile_caps(glb.MinFreeSize - li_freesize, pi_mode);
        return (0);
    }
    if (ls_buf.f_bfree < (glb.MinFreeSize + pi_size) / ls_buf.f_bsize) {
        if (ls_buf.f_bfree < glb.MinFreeSize / ls_buf.f_bsize) {
            delete_volatile_caps(glb.MinFreeSize - li_freesize, pi_mode);
        }
        return (IBP_E_WOULD_EXCEED_LIMIT);
    }

    if (pi_mode == IBP_ROUTINE_CHECK) {
        return (0);
    }
    if (pi_size + glb.StableStorageUsed + glb.VolStorageUsed > glb.VolStorage) {
        return (IBP_E_WOULD_EXCEED_LIMIT);
    }
    if ((pi_mode == IBP_STABLE)
        && (pi_size + glb.StableStorageUsed > glb.StableStorage)) {
        return (IBP_E_WOULD_EXCEED_LIMIT);
    }

    return (0);
}

int recover_finish()
{
    IBP_byteArray ls_ByteArray;
    IBP_request ls_req;
    JRB ls_rb;
    Jval lu_val;

  again:
    pthread_mutex_lock(&change_lock);
    jrb_rtraverse(ls_rb, glb.VolatileTree) {
        lu_val = jrb_val(ls_rb);
        ls_ByteArray = (IBP_byteArray) lu_val.s;

        if (ls_ByteArray->deleted == 2) {
            if ((ls_ByteArray->writeLock == 0)
                && (ls_ByteArray->readLock == 0)
                && dl_empty(ls_ByteArray->pending)) {
                ls_ByteArray->deleted = 1;
                pthread_mutex_unlock(&change_lock);
                jettisonByteArray(ls_ByteArray);
                goto again;
            }
            else {
                ls_ByteArray->deleted = 1;
/*
        ls_req = (IBP_request) calloc (1, sizeof (struct ibp_request));
        ls_req->request = IBP_DECR;
        ls_req->fd = -1;
        dl_insert_b(ls_ByteArray->pending, ls_req);
*/
            }                   /* end if */
        }
    }
    pthread_mutex_unlock(&change_lock);
    return (0);
}
