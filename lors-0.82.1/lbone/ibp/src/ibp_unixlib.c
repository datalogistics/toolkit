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
# ifdef STDC_HEADERS
# include <sys/types.h>
# endif
# ifdef HAVE_DIRENT_H
# include <dirent.h>
# endif
# include "ibp_unixlib.h"

/*
# define IBP_FULLINFO    1
# define IBP_PARTINFO    2
# define IBP_FINISHINFO  3
*/


/*****************************/
/* SaveInfo2LSS              */
/* It saves the latest infomation to ibp.lss, which is used for recovery  */
/* return: none              */
/*****************************/
int SaveInfo2LSS(IBP_byteArray ps_container, int pi_mode)
{

    IBP_StableBackupInfo ls_fixinfo;
    IBP_VariableBackupInfo ls_variableinfo;


    if (ps_container->BackupIndex == -1) {
        fprintf(stderr, "LSS record error. error ignore\n");
        return (-1);
    }

    if (pi_mode == IBP_FULLINFO) {
        strcpy(ls_fixinfo.key, ps_container->key);
        strcpy(ls_fixinfo.fname, ps_container->fname);
        ls_fixinfo.readKey = ps_container->readKey;
        ls_fixinfo.writeKey = ps_container->writeKey;
        ls_fixinfo.manageKey = ps_container->manageKey;
        ls_fixinfo.starttime = ps_container->starttime;

        ls_variableinfo.maxSize = ps_container->maxSize;
        ls_variableinfo.currentSize = ps_container->currentSize;
        ls_variableinfo.size = ps_container->size;
        ls_variableinfo.free = ps_container->free;
        ls_variableinfo.wrcount = ps_container->wrcount;
        ls_variableinfo.rdcount = ps_container->rdcount;
        ls_variableinfo.valid = IBP_OK;
        ls_variableinfo.readRefCount = ps_container->readRefCount;
        ls_variableinfo.writeRefCount = ps_container->writeRefCount;
        memcpy(&(ls_variableinfo.attrib), &(ps_container->attrib),
               sizeof(struct ibp_attributes));

#ifdef HAVE_PTHREAD_H
        pthread_mutex_lock(&recover_lock);
#endif
        if (gf_recoverfd == NULL) {
            if ((gf_recoverfd = fopen(gc_recoverfile, "r+")) == NULL) {
                fprintf(stderr, "LSS file open error. error ignore\n");
#ifdef HAVE_PTHREAD_H
                pthread_mutex_unlock(&recover_lock);
#endif
                ps_container->BackupIndex = -1;
/*        glb.NextIndexNumber--;   */
                return (-1);
            }
        }
        fseek(gf_recoverfd, 0, SEEK_END);
        fwrite(&ls_fixinfo, sizeof(IBP_StableBackupInfo), 1, gf_recoverfd);
        fwrite(&ls_variableinfo, sizeof(IBP_VariableBackupInfo), 1,
               gf_recoverfd);
        fflush(gf_recoverfd);

#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(&recover_lock);
#endif

    }
    else {
        ls_variableinfo.maxSize = ps_container->maxSize;
        ls_variableinfo.currentSize = ps_container->currentSize;
        ls_variableinfo.size = ps_container->size;
        ls_variableinfo.free = ps_container->free;
        ls_variableinfo.wrcount = ps_container->wrcount;
        ls_variableinfo.rdcount = ps_container->rdcount;
        ls_variableinfo.readRefCount = ps_container->readRefCount;
        ls_variableinfo.writeRefCount = ps_container->writeRefCount;
        if (pi_mode == 3) {
            ls_variableinfo.valid = 0;
        }
        else {
            ls_variableinfo.valid = IBP_OK;
        }
        memcpy(&(ls_variableinfo.attrib), &(ps_container->attrib),
               sizeof(struct ibp_attributes));
#ifdef HAVE_PTHREAD_H
        pthread_mutex_lock(&recover_lock);
#endif
        if (gf_recoverfd == NULL) {
            if ((gf_recoverfd = fopen(gc_recoverfile, "r+")) == NULL) {
                fprintf(stderr, "LSS file open error. error ignore\n");
#ifdef HAVE_PTHREAD_H
                pthread_mutex_unlock(&recover_lock);
#endif
                ps_container->BackupIndex = -1;
                return (-1);
            }
        }
        fseek(gf_recoverfd,
              (ps_container->BackupIndex * sizeof(IBP_StableBackupInfo) +
               (ps_container->BackupIndex -
                1) * sizeof(IBP_VariableBackupInfo)), SEEK_SET);
        fwrite(&ls_variableinfo, sizeof(IBP_VariableBackupInfo), 1,
               gf_recoverfd);
        fflush(gf_recoverfd);
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(&recover_lock);
#endif
    }

    return (IBP_OK);
}

/*****************************/
/* check_stale_capability    */
/* It checkes the unmanaged capabilities and deletes them  */
/* return: success 0, failure -1       */
/*****************************/
int check_unmanaged_capability()
{
    int i = 0;
    int skip;
    DIR *ld_dir;
    struct dirent *ls_dp;
    char lc_path[PATH_MAX], *lc_dirpath;
    JRB ls_node;

    for (i = 0; i < 3; i++) {
        skip = 0;
        switch (i) {
            /* first check Stable directory */
        case 0:
            lc_dirpath = glb.StableDir;
            ld_dir = opendir(glb.StableDir);
            if (ld_dir == NULL) {
                return (-1);
            }
            break;
        case 1:
            lc_dirpath = glb.VolDir;
            if (strcmp(glb.StableDir, glb.VolDir) == 0) {
                skip = 1;
            }
            else {
                ld_dir = opendir(glb.VolDir);
                if (ld_dir == NULL) {
                    return (-1);
                }
            }
            break;
        default:
            skip = 1;
            break;
        }

        if (skip == 1) {
            continue;
        }

        while (ld_dir) {
            errno = 0;
            lc_path[0] = '\0';
            if ((ls_dp = readdir(ld_dir)) != NULL) {
                if ((strcmp(ls_dp->d_name, ".") == 0) ||
                    (strcmp(ls_dp->d_name, "..") == 0)) {
                    continue;
                }
                if (strncmp(ls_dp->d_name, "IBP-", 4) != 0) {
                    continue;
                }
                sprintf(lc_path, "%s%s", lc_dirpath, ls_dp->d_name);
                ls_node = jrb_find_str(glb.ByteArrayTree, ls_dp->d_name);
                if (ls_node == NULL) {
                    unlink(lc_path);
                    fprintf(stderr, "Delete unmanaged %s\n", lc_path);
                }
            }
            else {
                closedir(ld_dir);
                ld_dir = NULL;
            }
        }
    }
    return (0);
}
