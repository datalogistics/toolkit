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
# include "ibp_thread.h"
# include "ibp_thread_log.h"

# ifdef HAVE_SYS_STATVFS_H      /* for Solaris */
# include <sys/statvfs.h>
# elif HAVE_SYS_VFS_H           /* for Linux */
# include <sys/vfs.h>
# else /* for macOS */
# ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
# endif /* HAVE_SYS_PARAM_H */
# ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
# endif /* HAVE_SYS_MOUNT_H */
# endif

/*
 *
 * level 4 - createcap
 *
 */

/*****************************/
/* copy_File2Socket          */
/* It copys ByteArray file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_File2Socket(FILE * pf_fp, IBP_cap ps_cap, IBP_timer ps_timer,
                     ulong_t pl_size, time_t pl_expireTime)
{
    long n1, readCount;
    char *dataBuf;
    long readChunk;
    int li_ret;

    dataBuf = (char *) malloc(DATA_BUF_SIZE);
    if (dataBuf == NULL) {
        IBP_errno = IBP_E_GENERIC;
        return (IBP_errno);
    }

    n1 = readCount = 0;
    while (readCount < pl_size) {
        if (pl_expireTime < time(NULL)) {
            free(dataBuf);
            IBP_errno = IBP_E_SERVER_TIMEOUT;
            return (IBP_errno);
        }

        readChunk = IBP_MIN(DATA_BUF_SIZE, pl_size - readCount);
        n1 = fread(dataBuf, 1, readChunk, pf_fp);
        if (n1 == 0) {
            free(dataBuf);
            return IBP_E_FILE_READ;
        }
        if ((li_ret = IBP_store(ps_cap, ps_timer, dataBuf, n1)) <= 0) {
            if (readCount == 0) {
                free(dataBuf);
                return (IBP_errno);
            }
            else {
                free(dataBuf);
                return (readCount);
            }
        }
        readCount += n1;
    }
    free(dataBuf);
    return (readCount);
}

/*****************************/
/* copy_Fifo2Socket          */
/* It copys fifo or CQ file to socket, used in handle_copy()      */
/* return -- IBP_OK or error number                */
/*****************************/
int copy_Fifo2Socket(FILE * pf_fp,
                     IBP_cap ps_cap,
                     IBP_timer ps_timer,
                     ulong_t pl_size,
                     ulong_t pl_fifoSize,
                     ulong_t pl_rdPtr, int pi_HeaderLen, time_t pl_expireTime)
{

    ulong_t readCount;
    ulong_t readChunk;
    ulong_t emptyCount = 0;
    long n1;
    char *dataBuf;
    int li_fd;
    int li_ret;

    dataBuf = (char *) malloc(DATA_BUF_SIZE);
    if (dataBuf == NULL) {
        IBP_errno = IBP_E_GENERIC;
        return (IBP_errno);
    }

    li_fd = fileno(pf_fp);
    n1 = readCount = 0;
    while (readCount < pl_size) {
        if (pl_expireTime < time(NULL)) {
            free(dataBuf);
            IBP_errno = IBP_E_SERVER_TIMEOUT;
            return (IBP_errno);
        }

        readChunk = IBP_MIN(DATA_BUF_SIZE, pl_size - readCount);        /* buffer overflow */
        readChunk = IBP_MIN(readChunk, pl_fifoSize - emptyCount);       /* read available */
        readChunk = IBP_MIN(readChunk, pl_fifoSize - pl_rdPtr); /* wrap around */

#ifdef IBP_DEBUG
        fprintf(stderr, "read chunk : %lu %lu\n", readChunk, pl_rdPtr);
#endif

        fseek(pf_fp, pi_HeaderLen + pl_rdPtr, SEEK_SET);
        n1 = fread(dataBuf, 1, readChunk, pf_fp);
        if (n1 < 0) {
            free(dataBuf);
            return IBP_E_FILE_READ;
        }
        pl_rdPtr = (pl_rdPtr + n1) % pl_fifoSize;
        emptyCount += n1;

        if ((li_ret = IBP_store(ps_cap, ps_timer, dataBuf, n1)) <= 0) {
            if (readCount == 0) {
                free(dataBuf);
                return (IBP_errno);
            }
            else {
                free(dataBuf);
                return (readCount);
            }
        }

        readCount += n1;
    }
    free(dataBuf);
    return (readCount);
}

/*****************************/
/* createcap                 */
/* This function creates IBP cap, envoked by handle_allocate()  */
/* return: the name of capability       */
/*****************************/
char *createcap(char *pc_Key, int *pi_Key, char *pc_RWM)
{

    char lc_buf[MAX_CAP_LEN];
    char *lc_tmp;
    *pi_Key = random();
    sprintf(lc_buf, "ibp://%s:%d/%s/%d/%s",
            glb.hostname, glb.IbpPort, pc_Key, *pi_Key, pc_RWM);
    lc_tmp = (char *) malloc(strlen(lc_buf) + 1);
    strcpy(lc_tmp, lc_buf);
    return (lc_tmp);

}

/*****************************/
/* jettisonByteArray         */
/* It deletes the node in ByteArrayTree and releases its resource  */
/* return: none              */
/*****************************/
void jettisonByteArray(IBP_byteArray ba)
{

    JRB temp;
    Dlist dtemp;
    char lineBuf[CMD_BUF_SIZE];
    IBP_request req;

    pthread_mutex_lock(&change_lock);
    ba->deleted = 1;
    if (ba->reference > 0) {
        /*ba->deleted = 1; */
        pthread_mutex_unlock(&change_lock);
        return;
    }
    unlink(ba->fname);          /* Remove underlying file */
    /* modification for IBP_CIRQ */


    free(ba->key);
    free(ba->fname);

    sprintf(lineBuf, "%d \n", IBP_E_CAP_NOT_FOUND);
/*
  while (!dl_empty(ba->pending)){
    dtemp = first(ba->pending);
    req = (IBP_request)dl_val(dtemp);
    Write (req->fd, lineBuf, strlen (lineBuf));
    close(req->fd);
    free(req);
    dl_delete_node(dtemp);
  }
  dl_traverse(dtemp, ba->pending)
  {
    req = (IBP_request)dl_val(dtemp);
    free(req);
    dtemp->val = NULL;
  } 
*/
    dl_delete_list(ba->pending);

    if (ba->volTreeEntry != NULL) {     /* Remove entry from volatile */
        jrb_delete_node(ba->volTreeEntry);      /* tree if it is volatile */

# ifdef HAVE_PTHREAD_H
        glb.VolStorageUsed -= ba->maxSize;
        if ((thread_glb.StorageNeeded > 0) &&
            (thread_glb.StorageNeeded <
             glb.VolStorage - glb.VolStorageUsed - glb.StableStorageUsed)) {
            pthread_cond_signal(&(thread_glb.Cond));
        }
# else
        glb.VolStorageUsed -= ba->maxSize;
# endif
    }
    else {
# ifdef HAVE_PTHREAD_H
        glb.StableStorageUsed -= ba->maxSize;
        if ((thread_glb.StorageNeeded > 0) &&
            (thread_glb.StorageNeeded <
             glb.VolStorage - glb.VolStorageUsed - glb.StableStorageUsed)) {
            pthread_cond_signal(&(thread_glb.Cond));
        }
# else
        glb.StableStorageUsed -= ba->maxSize;
# endif
    }

    /*SaveInfo2LSS(ba, 3); */

    if (ba->transientTreeEntry != NULL) /* Remove entry from transient */
        jrb_delete_node(ba->transientTreeEntry);        /* tree if it is transient */
    temp = ba->masterTreeEntry; /* Remove entry from master */

    free(temp->key.s);          /* Byte Array tree */
# ifdef HAVE_PTHREAD_H
    free((Thread_Info *) ba->thread_data);
# endif
    /*free(ba); */
    jrb_delete_node(temp);

    pthread_mutex_unlock(&change_lock);

    SaveInfo2LSS(ba, 3);
    /*memset(ba,0,sizeof( struct ibp_byteArray ));*/
    free(ba);
    return;
}


/*
 *
 * level 3 - truncatestorage, handle_allocate, handle_store, handle_lsd,
 *           handle_manage
 *
 */

/*****************************/
/* truncatestorage           */
/* It traverses the whole VolatileTree or TransientTree to search all nodes expired, and then deletes them */
/* return: none              */
/*****************************/
void truncatestorage(int pi_type, void *pv_par)
{

    int lb_found = 0;
    int li_VolStorage = 0;
    time_t lt_Now = time(0);
    IBP_byteArray ls_ByteArray;
    IBP_request ls_req;
    JRB ls_rb, ls_GlobalTree = NULL;
    Jval lu_val;

    switch (pi_type) {
    case 1:                    /* volatile */
        li_VolStorage = *((int *) pv_par);
        ls_GlobalTree = glb.VolatileTree;
        break;
    case 2:                    /* transient */
        lt_Now = *((time_t *) pv_par);
        ls_GlobalTree = glb.TransientTree;
        ls_rb = NULL;
        break;
    }                           /* end switch */

    while (lb_found != 1) {
        lb_found = 1;
        pthread_mutex_lock(&change_lock);
        jrb_traverse(ls_rb, ls_GlobalTree) {
            if ((pi_type == 1) && (glb.VolStorageUsed <= li_VolStorage)) {
                pthread_mutex_unlock(&change_lock);
                return;
            }
            lu_val = jrb_val(ls_rb);
            ls_ByteArray = (IBP_byteArray) lu_val.s;
            /*
               ls_ByteArray = (IBP_byteArray) jrb_val (ls_rb);
             */
            if ((pi_type == 1) || (ls_ByteArray->attrib.duration < lt_Now)) {
                /*
                   if (ls_ByteArray->deleted == 1)
                   continue;
                   else
                 */
                if ((ls_ByteArray->writeLock == 0)
                    && (ls_ByteArray->readLock == 0)
                    && dl_empty(ls_ByteArray->pending)) {
                    ls_ByteArray->deleted = 1;
                    pthread_mutex_unlock(&change_lock);
                    jettisonByteArray(ls_ByteArray);
                    pthread_mutex_lock(&change_lock);
                    lb_found = 0;
                    break;
                }
                else {
                    ls_ByteArray->deleted = 1;
                    /*
                       ls_req = (IBP_request) calloc (1, sizeof (struct ibp_request));
                       ls_req->request = IBP_DECR;
                       ls_req->fd = -1;
                       dl_insert_b(ls_ByteArray->pending, ls_req);
                     */
                }               /* end if */
            }
            else {
                pthread_mutex_unlock(&change_lock);
                return;
            }
        }                       /* end rb_traverse */
        pthread_mutex_unlock(&change_lock);
    }                           /* end while */
}


/*
 * validIPAddres check whether the connecting socket is allowed to allocate resource
 */

/*****************************/
/* validIPAddress            */
/* It check whether the connecting socket is allowed to allocate resource */
/* return: valid / invalid   */
/*****************************/
int validIPAddress(int pi_fd)
{
    char *data;
    struct sockaddr *sa;
    int len = IBP_K_MAXSOCKADDR;
    struct sockaddr_in *sa_in;
    IPv4_addr_t *tmp;
#ifdef AF_INET6
    int i;
    int matched;
    struct sockaddr_in6 *sa_in6;
    IPv6_addr_t *tmpv6;
#endif
    int li_ret = 0;

    data = malloc(IBP_K_MAXSOCKADDR);
    if (getpeername(pi_fd, (struct sockaddr *) data, &len) < 0)
        return (0);
    sa = (struct sockaddr *) data;

    switch (sa->sa_family) {
    case AF_INET:
        sa_in = (struct sockaddr_in *) sa;
        if (glb.IPv4 == NULL) {
            li_ret = 1;
            break;
        }

        tmp = glb.IPv4;
        li_ret = 0;
        while (tmp != NULL) {
            if ((sa_in->sin_addr.s_addr & tmp->mask) ==
                (tmp->addr & tmp->mask)) {
                li_ret = 1;
                break;
            }
            tmp = tmp->next;
        }
        break;

#ifdef AF_INET6
    case AF_INET6:
        sa_in6 = (struct sockaddr_in6 *) sa;
        if (glb.IPv6 == NULL) {
            li_ret = 1;
            break;
        }
        tmpv6 = glb.IPv6;
        li_ret = 0;
        while (tmpv6 != NULL) {
            matched = 1;
            for (i = 0; i < 16; i++) {
                if ((sa_in6->sin6_addr.s6_addr[i]
                     && tmpv6->mask[i]) != (tmpv6->addr[i]
                                            && tmpv6->mask[i])) {
                    matched = 0;
                    break;
                }
            }
            if (matched) {
                li_ret = 1;
                break;
            }

            tmpv6 = tmpv6->next;
        }
        break;

#endif
    default:
        li_ret = 0;
        break;
    }
    free(data);
    return li_ret;
}

/*****************************/
/* check_allocate            */
/* It check whether the allocation is permitted by allcation policy  */
/* return: valid / invalid   */
/*****************************/
int check_allocate(int pi_fd, int pi_nbt, int size)
{
    int li_duration;

    switch (glb.AllocatePolicy) {
    case IBP_ALLPOL_SID:
        break;

    case IBP_ALLPOL_EXP:
        li_duration = ibp_cmd[pi_nbt].lifetime / 3600;
        if (li_duration < 1) {
            li_duration = 1;
        }
        if (ibp_cmd[pi_nbt].reliable == IBP_STABLE) {
            if (glb.StableStorage <= li_duration * size) {
                handle_error(IBP_E_WOULD_EXCEED_POLICY, &pi_fd);
                return (-1);
            }
        }
        else {
            if (li_duration * size >= glb.VolStorage) {
                handle_error(IBP_E_WOULD_EXCEED_POLICY, &pi_fd);
                return (-1);
            }
        }
        break;
    }
    return 0;
}


/*****************************/
/* handle_allocate           */
/* It handles the user's allocation requirement, envoked by handle_require()  */
/* It firstly checks whether there is enough space. Then it builds the node for the requirement and sends the info back to user   */
/* return: none              */
/*****************************/
void handle_allocate(int pi_fd, int pi_nbt, ushort_t pf_AllocationType)
{

    time_t lt_CurTime;
    char *ls_now;
    char lc_FName[PATH_MAX];
    char *lc_Key = NULL;
    struct ibp_attributes ls_NewAttr;
    FILE *lf_fp;
    int li_ReadKey, li_WriteKey, li_ManageKey;
    char *lc_ReadCap = NULL;
    char *lc_WriteCap = NULL;
    char *lc_ManageCap = NULL;
    IBP_byteArray ls_container;
    char *lc_generic;
    char lc_BigBuf[MAX_BUFFER];
    int li_HeaderLen;
    char lc_LineBuf[CMD_BUF_SIZE];
    char *lc_r;
    int li_sec[20];
    int li_index;
    int li_ret, li_nextstep;
    ulong_t ll_randseed;
    ulong_t ll_size;
    Jval lu_val;
    ushort_t lf_ThreadLeavingMessage = 0;

#ifdef HAVE_PTHREAD_H
    time_t left_time;
    Thread_Info *thread_info;
    struct timespec spec;
#endif

    /*
     * 0- Initialize
     */

#ifdef IBP_DEBUG
    fprintf(stderr, "T %d, handle allocate start\n", pi_nbt);
    if (pf_AllocationType != IBP_HALL_TYPE_PING) {
        strcat(exec_info[pi_nbt], "   enter handle_allocate() \n");
    }
#endif

    IBPErrorNb[pi_nbt] = IBP_OK;

    time(&lt_CurTime);

    /*
     * 1- check parameters 
     */
    switch (pf_AllocationType) {

    case IBP_HALL_TYPE_STD:
        li_nextstep = IBP_HALL_PARAM_S;
        break;

    case IBP_HALL_TYPE_PING:
        li_nextstep = IBP_HALL_PARAM_P;
        break;

    default:
        li_nextstep = IBP_HALL_DONE;
        break;
    }

    while (li_nextstep != IBP_HALL_DONE) {
        switch (li_nextstep) {

        case IBP_HALL_PARAM_S:
#ifdef IBP_DEBUG
            if (pf_AllocationType != IBP_HALL_TYPE_PING) {
                strcat(exec_info[pi_nbt], "     check parameter\n");
            }
#endif

            /*
             * check ip address
             */

            if (!validIPAddress(pi_fd)) {
                handle_error(IBP_E_INV_IP_ADDR, &pi_fd);
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            /*
             * Check if parameters are OK
             */

            if (ibp_cmd[pi_nbt].size == 0) {
                handle_error(IBP_E_INV_PAR_SIZE, &pi_fd);
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            if ((glb.MaxDuration == 0) ||
                ((glb.MaxDuration > 0) &&
                 ((ibp_cmd[pi_nbt].lifetime <= 0) ||
                  (glb.MaxDuration < ibp_cmd[pi_nbt].lifetime)))) {
                handle_error(IBP_E_LONG_DURATION, &pi_fd);
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            strcat(exec_info[pi_nbt], "  1111   check parameter\n");
            if (check_allocate(pi_fd, pi_nbt, ibp_cmd[pi_nbt].size) != 0) {
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            ls_NewAttr.duration =
                (ibp_cmd[pi_nbt].lifetime >
                 0 ? lt_CurTime + ibp_cmd[pi_nbt].lifetime : 0);
            ls_NewAttr.reliability = ibp_cmd[pi_nbt].reliable;
            ls_NewAttr.type = ibp_cmd[pi_nbt].type;

            strcat(exec_info[pi_nbt], "  2222   check parameter\n");
            if (check_size(ibp_cmd[pi_nbt].size, ls_NewAttr.reliability) != 0) {
#ifdef HAVE_PTHREAD_H
                /* server sync for handle_allocate */

                left_time = ibp_cmd[pi_nbt].ServerSync;
                if (thread_glb.StorageNeeded != 0) {
                    thread_glb.Waiting++;
                    spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
                    spec.tv_nsec = 0;
                    if (pthread_cond_timedwait
                        (&(thread_glb.Protect), &alloc_lock, &spec) != 0) {
                        thread_glb.Waiting--;
                        handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
                        li_nextstep = IBP_HALL_DONE;
                        continue;
                    }
                    else {
                        thread_glb.Waiting--;
                        left_time -= (time(NULL) - lt_CurTime);
                    }
                }

                if (check_size(ibp_cmd[pi_nbt].size, ls_NewAttr.reliability)
                    != 0) {
                    thread_glb.StorageNeeded = ibp_cmd[pi_nbt].size;
                    spec.tv_sec = time(NULL) + left_time;
                    spec.tv_nsec = 0;
                    if (pthread_cond_timedwait
                        (&(thread_glb.Cond), &alloc_lock, &spec) != 0) {
                        if (check_size
                            (ibp_cmd[pi_nbt].size,
                             ls_NewAttr.reliability) != 0) {
                            handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
                            li_nextstep = IBP_HALL_DONE;
                        }
                        thread_glb.StorageNeeded = 0;
                        if (thread_glb.Waiting > 0) {
                            pthread_cond_signal(&(thread_glb.Protect));
                        }
                    }
                    else {
                        thread_glb.StorageNeeded = 0;
                        if (thread_glb.Waiting > 0) {
                            pthread_cond_signal(&(thread_glb.Protect));
                        }
                    }
                }
# else
                handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
                li_nextstep = IBP_HALL_DONE;
                continue;
#endif

            }
            strcat(exec_info[pi_nbt], "  3333   check parameter\n");
            li_nextstep += IBP_STEP;
            break;

        case IBP_HALL_PARAM_P:
            ls_NewAttr.duration = 0;
            ls_NewAttr.reliability = IBP_STABLE;
            ls_NewAttr.type = IBP_BYTEARRAY;

            li_nextstep += IBP_STEP;
            break;

        case IBP_HALL_NEWCONT_S:

            /*
             * 2- create the new container and initialise it
             */

            /*
             * 2.1- prepare the file names and keys
             */

            /* SECURITY - long names */
#ifdef IBP_DEBUG
            if (pf_AllocationType != IBP_HALL_TYPE_PING) {
                strcat(exec_info[pi_nbt],
                       "     prepare the file names and keys\n");
            }
#endif

            lc_r = malloc(MAX_KEY_LEN);
            lc_Key = malloc(MAX_KEY_LEN);
            if ((lc_r == NULL) || (lc_Key == NULL)) {
                handle_error(IBP_E_ALLOC_FAILED, &pi_fd);
                if (lc_r != NULL)
                    free(lc_r);
                lf_ThreadLeavingMessage = 1;
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            sprintf(lc_Key, "IBP-");

            /* 
             * seeding the random numbers ...
             */

            ll_randseed = random() + pi_nbt + lt_CurTime;
            srandom(ll_randseed);
            ll_randseed = random();

            for (li_index = 0; li_index < 16; li_index++) {
                li_sec[li_index] = random();
                sprintf(lc_r, "%d", li_sec[li_index]);
                strcat(lc_Key, lc_r);
            }

            free(lc_r);

            if (ls_NewAttr.reliability == IBP_STABLE)
                strcpy(lc_FName, glb.StableDir);
            else
                strcpy(lc_FName, glb.VolDir);

            strcat(lc_FName, lc_Key);

            /* end long names section */
            li_nextstep += IBP_STEP;
            break;

        case IBP_HALL_NEWCONT_P:
            lc_Key = malloc(MAX_KEY_LEN);
            if (lc_Key == NULL) {
                fprintf(stderr,
                        "IBP-0 couldn't be created. Memory allocation failure\n");
                li_nextstep = IBP_HALL_DONE;
                continue;
            }
            sprintf(lc_Key, "IBP-0");
            strcpy(lc_FName, glb.StableDir);
            strcat(lc_FName, lc_Key);

            li_nextstep += IBP_STEP;
            break;

        case IBP_HALL_CREATE_S:
            if (ls_NewAttr.type == IBP_FIFO || ls_NewAttr.type == IBP_CIRQ) {
            }

            if ((lf_fp = fopen(lc_FName, "w")) == NULL) {
                unlink(lc_FName);
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                lf_ThreadLeavingMessage = 1;
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            lc_ReadCap = createcap(lc_Key, &li_ReadKey, "READ");
            lc_WriteCap = createcap(lc_Key, &li_WriteKey, "WRITE");
            lc_ManageCap = createcap(lc_Key, &li_ManageKey, "MANAGE");

            if ((lc_ReadCap == NULL) || (lc_WriteCap == NULL)
                || (lc_ManageCap == NULL)) {
                handle_error(IBP_E_ALLOC_FAILED, &pi_fd);
                lf_ThreadLeavingMessage = 1;
                li_nextstep = IBP_HALL_DONE;
                continue;
            }
            ll_size = ibp_cmd[pi_nbt].size;
            li_nextstep += IBP_STEP;
            break;

        case IBP_HALL_CREATE_P:
            if ((lf_fp = fopen(lc_FName, "w")) == NULL) {
                unlink(lc_FName);
                fprintf(stderr,
                        "IBP-0 couldn't be created. File opening failure\n");
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            lc_ReadCap = malloc(PATH_MAX);
            sprintf(lc_ReadCap, "ibp://%s:%d/%s/%d/%s",
                    glb.hostname, glb.IbpPort, lc_Key, 0, "READ");
            lc_WriteCap = malloc(PATH_MAX);
            sprintf(lc_WriteCap, "ibp://%s:%d/%s/%d/%s",
                    glb.hostname, glb.IbpPort, lc_Key, 0, "WRITE");
            lc_ManageCap = malloc(PATH_MAX);
            sprintf(lc_ManageCap, "ibp://%s:%d/%s/%d/%s",
                    glb.hostname, glb.IbpPort, lc_Key, 0, "MANAGE");

            ll_size = 1000 * 1024;      /* TO BE MODIFIED!!!!! */
            li_nextstep += IBP_STEP;

            break;


        case IBP_HALL_FILL_S:
        case IBP_HALL_FILL_P:

            /*
             * 2.2- filenames are OK, now prepare the container
             */

#ifdef IBP_DEBUG
            if (pf_AllocationType != IBP_HALL_TYPE_PING) {
                strcat(exec_info[pi_nbt],
                       "     prepare the file container\n");
            }
#endif
            ls_container =
                (IBP_byteArray) calloc(1, sizeof(struct ibp_byteArray));

            ls_container->maxSize = ll_size;
            ls_container->currentSize = 0;
            ls_container->size = ll_size;
            ls_container->free = ll_size;
            ls_container->rdcount = 0;
            ls_container->wrcount = 0;

            memcpy(&(ls_container->attrib), &ls_NewAttr,
                   sizeof(struct ibp_attributes));

            ls_container->fname = strdup(lc_FName);
            ls_container->key = strdup(lc_Key);
            ls_container->readKey = li_ReadKey;
            ls_container->writeKey = li_WriteKey;
            ls_container->manageKey = li_ManageKey;

            ls_container->writeRefCount = 1;
            ls_container->readRefCount = 1;

            ls_container->lock = 0;
            ls_container->rl = 0;
            ls_container->wl = 0;
            ls_container->bl = 0;

            ls_container->reference = 0;

            ls_container->starttime = lt_CurTime;

            ls_container->thread_data = NULL;

#ifdef HAVE_PTHREAD_H
            thread_info = (Thread_Info *) malloc(sizeof(Thread_Info));
            if (thread_info == NULL) {
#ifdef IBP_DEBUG
                fprintf(stderr,
                        "T %d, thread_info couldn't be allocated, returning\n",
                        pi_nbt);
#endif
                handle_error(IBP_E_ALLOC_FAILED, &pi_fd);
                li_nextstep = IBP_HALL_DONE;
                continue;
            }
            bzero(thread_info, sizeof(Thread_Info));
            thread_info->read = -1;

            ls_container->thread_data = (void *) thread_info;
#endif

            ls_container->pending = make_dl();

            lu_val.s = (char *) ls_container;
            pthread_mutex_lock(&change_lock);
            ls_container->masterTreeEntry = jrb_insert_str(glb.ByteArrayTree,
                                                           strdup(lc_Key),
                                                           lu_val);

            if (ls_NewAttr.reliability == IBP_VOLATILE) {
                /*
                   #ifdef HAVE_PTHREAD_H
                   pthread_mutex_lock(&change_lock);
                   #endif
                 */
                glb.VolStorageUsed += ll_size;
/*
#ifdef HAVE_PTHREAD_H
	pthread_mutex_unlock(&change_lock);
#endif
*/
                ls_container->volTreeEntry = vol_cap_insert(ll_size,
                                                            ibp_cmd[pi_nbt].
                                                            lifetime, lu_val);
            }
            else if (ls_NewAttr.reliability == IBP_STABLE) {
/*
#ifdef HAVE_PTHREAD_H
	pthread_mutex_lock(&change_lock);
#endif
*/
                glb.StableStorageUsed += ll_size;
/*
#ifdef HAVE_PTHREAD_H
	pthread_mutex_unlock(&change_lock);
#endif
*/
            }
            if (ls_NewAttr.duration > 0) {
                ls_container->transientTreeEntry =
                    jrb_insert_int(glb.TransientTree, ls_NewAttr.duration,
                                   lu_val);
            }
            pthread_mutex_unlock(&change_lock);

            lc_generic = lc_BigBuf + sizeof(int);       /* Reserve for total header size */
            memcpy(lc_generic, ls_container, BASE_HEADER);
            lc_generic += BASE_HEADER;
            sprintf(lc_generic, "%s\n", ls_container->fname);
            lc_generic += strlen(lc_generic);
            li_HeaderLen = lc_generic - lc_BigBuf;
            memcpy(lc_BigBuf, &li_HeaderLen, sizeof(int));
            if (fwrite(lc_BigBuf, 1, li_HeaderLen, lf_fp) == 0) {
                jettisonByteArray(ls_container);
                unlink(lc_FName);
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                fclose(lf_fp);
                lf_ThreadLeavingMessage = 1;
                li_nextstep = DONE;
                return;
            }
            fclose(lf_fp);

            li_nextstep += IBP_STEP;
            break;

        case IBP_HALL_CLOSE_S:
            /*
             * 3- Confirm the client, clean up and leave
             */
#ifdef IBP_DEBUG
            if (pf_AllocationType != IBP_HALL_TYPE_PING) {
                strcat(exec_info[pi_nbt],
                       "     Confirm the client, clean up and leave\n");
            }
#endif

            sprintf(lc_LineBuf, "%d %s %s %s \n",
                    IBP_OK, lc_ReadCap, lc_WriteCap, lc_ManageCap);
#ifdef IBP_DEBUG
            fprintf(stderr, "T %d, allocate, line sent to client: %s\n",
                    pi_nbt, lc_LineBuf);
#endif

            li_ret = Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
            if (li_ret != strlen(lc_LineBuf)) {
#ifdef IBP_DEBUG
                fprintf(stderr, "T %d, Answering to client failed\n", pi_nbt);
#endif
                handle_error(IBP_E_SOCK_WRITE, &pi_fd);
                li_nextstep = IBP_HALL_DONE;
                continue;
            }

            ls_container->BackupIndex = glb.NextIndexNumber;
            glb.NextIndexNumber++;
            if (pf_AllocationType == IBP_HALL_TYPE_STD) {
                if (SaveInfo2LSS(ls_container, 1) == -1) {
           /*glb.NextIndexNumber--;*/
                    ls_container->BackupIndex = -1;
                }
            }

            truncatestorage(2, (void *) &lt_CurTime);

            /*
             * Update the log file
             */
            if (gf_logfd == NULL)
                gf_logfd = fopen(gc_logfile, "a");
            if (gf_logfd == NULL) {
                handle_error(E_OFILE, (void *) gc_logfile);
            }
            else {
                ls_now = ctime(&lt_CurTime);
                ls_now[24] = '\0';
                fprintf(gf_logfd, "%s # 1 #  %s\n", ls_now,
                        ls_container->fname);
                fflush(gf_logfd);
#ifdef IBP_DEBUG
                fprintf(stderr, "%s # 1 # %s\n", ls_now, ls_container->fname);
#endif
            }

            /*  jettisonByteArray(ls_container);   */
            lf_ThreadLeavingMessage = 1;
            li_nextstep = DONE;
            break;

        case IBP_HALL_CLOSE_P:
        default:
            li_nextstep = IBP_HALL_DONE;
            break;
        }
    }

    /*
     * Clean up before returning
     */

    if (lf_ThreadLeavingMessage == 1) {
#ifdef HAVE_PTHREAD_H
        if ((ls_NewAttr.reliability == IBP_STABLE)
            && (thread_glb.Waiting > 0))
            pthread_cond_signal(&(thread_glb.Protect));
        if ((ls_NewAttr.reliability == IBP_VOLATILE)
            && (thread_glb.Waiting > 0))
            pthread_cond_signal(&(thread_glb.Protect));
#endif
    }

    if (lc_Key != NULL)
        free(lc_Key);
    if (lc_ReadCap != NULL)
        free(lc_ReadCap);
    if (lc_WriteCap != NULL)
        free(lc_WriteCap);
    if (lc_ManageCap != NULL)
        free(lc_ManageCap);
#ifdef IBP_DEBUG
    if (pf_AllocationType != IBP_HALL_TYPE_PING) {
        strcat(exec_info[pi_nbt], "     handle_allocate finished\n");
    }
#endif
    return;
}

void handle_store_end(int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
    char lc_LineBuf[CMD_BUF_SIZE];

    if (IBPErrorNb[pi_nbt] != IBP_OK) {
        sprintf(lc_LineBuf, "%d \n", IBPErrorNb[pi_nbt]);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    }
    ps_container->writeLock--;

    /*
       ps_container->reference--;
       assert(ps_container->reference >= 0 );

       * check whether this capability has been deleted *
       if (ps_container->deleted == 1)
       {
       if ((ps_container->writeLock == 0) && (ps_container->readLock == 0))
       {
       SaveInfo2LSS(ps_container, 2);
       jettisonByteArray(ps_container);
       }
       }
     */

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_store finished\n");
    fprintf(stderr, "T %d, handle_store, done\n", pi_nbt);
#endif
    return;
}


/*****************************/
/* handle_store              */
/* It handles the user's store requirement, envoked by handle_require() */
/* It stores the user's data to a temporary file on the disk  */
/* return: none              */
/*****************************/
void handle_store(int pi_fd, int pi_srcfd, int pi_nbt,
                  IBP_byteArray ps_container)
{
    char lc_LineBuf[CMD_BUF_SIZE];
    int li_HeaderLen, li_Ret, li_nextstep = 1;
    FILE *lf_f = NULL;
    unsigned long ll_size, ll_free, ll_wrcount, ll_rdcount, ll_written = 0;
    ulong_t ll_writeptr = 0;
    time_t lt_expireTime;       /* Server Timeout */

#ifdef HAVE_PTHREAD_H
    Thread_Info *info;
    struct timespec spec;
#endif


#ifdef IBP_DEBUG
    fprintf(stderr, "T %d, handle_store, beginning\n", pi_nbt);
    strcat(exec_info[pi_nbt], "  handle_store beginning\n");
#endif

    if (ibp_cmd[pi_nbt].ServerSync != 0) {
        lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
    }
    else {
        lt_expireTime = 0;
    }

    /* Write lock added */
    ps_container->writeLock++;

    switch (ps_container->attrib.type) {

    case IBP_BYTEARRAY:
        if (ibp_cmd[pi_nbt].size >
            ps_container->maxSize - ps_container->currentSize) {
            IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = DONE;
        }
        break;

    case IBP_BUFFER:
        if (ibp_cmd[pi_nbt].size > ps_container->maxSize) {
            IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = DONE;
        }
        break;

    case IBP_FIFO:
    case IBP_CIRQ:
        ll_wrcount = ps_container->wrcount;
        ll_rdcount = ps_container->rdcount;
        ll_free = ps_container->free;
        ll_size = ps_container->size;
        ll_writeptr = ll_wrcount;

# ifdef IBP_DEBUG
        fprintf(stderr, "T %d, Lock file :%lu::%lu:%lu:%lu:\n", pi_nbt,
                ll_wrcount, ll_rdcount, ll_free, ll_size);
# endif
        break;

    default:
        li_nextstep = DONE;
        break;
    }

    if (li_nextstep == DONE) {
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
        break;

    case IBP_FIFO:
        if (ll_free < ibp_cmd[pi_nbt].size) {
# ifdef HAVE_PTHREAD_H
            info = (Thread_Info *) ps_container->thread_data;
            info->write = ibp_cmd[pi_nbt].size;
            spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
            spec.tv_nsec = 0;
            pthread_mutex_lock(&filock);
            if (pthread_cond_timedwait(&(info->write_cond), &filock, &spec) !=
                0) {
                info->write = 0;
                IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
                handle_error(IBPErrorNb[pi_nbt], &pi_fd);
                /*          fclose (lf_FLock); */
                pthread_mutex_unlock(&filock);
                li_nextstep = DONE;
                break;
            }
            else {
                info->write = 0;
                /*          fclose (lf_FLock); */
                pthread_mutex_unlock(&filock);
                ll_wrcount = ps_container->wrcount;
                ll_rdcount = ps_container->rdcount;
                ll_free = ps_container->free;
                ll_size = ps_container->size;
                ll_wrcount = (ll_wrcount + ibp_cmd[pi_nbt].size) % ll_size;
                ll_free -= ibp_cmd[pi_nbt].size;
            }
#else
            IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = DONE;
            break;
#endif
        }
        else {
            ll_wrcount = (ll_wrcount + ibp_cmd[pi_nbt].size) % ll_size;
            ll_free -= ibp_cmd[pi_nbt].size;
        }
        break;

    case IBP_CIRQ:
        if (ll_size < ibp_cmd[pi_nbt].size) {
            IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = DONE;
            break;
        }
        else {
            ll_wrcount = (ll_wrcount + ibp_cmd[pi_nbt].size) % ll_size;
            if (ll_free < ibp_cmd[pi_nbt].size) {
                ll_free = 0;
                ll_rdcount = ll_wrcount;
            }
            else
                ll_free -= ibp_cmd[pi_nbt].size;
        }
        break;

    default:
        break;
    }

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_store size determined\n");
#endif

    if (li_nextstep == DONE) {
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    lf_f = fopen(ps_container->fname, "r+");
    if (lf_f == NULL) {
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
        handle_error(IBPErrorNb[pi_nbt], &pi_fd);
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    if (fread(&li_HeaderLen, sizeof(int), 1, lf_f) == 0) {
        IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
        handle_error(IBPErrorNb[pi_nbt], &pi_fd);
        fclose(lf_f);
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
        if (fseek(lf_f, 0, SEEK_END) == -1) {
            IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            fclose(lf_f);
            li_nextstep = DONE;
            break;
        }
        break;

    case IBP_BUFFER:
        if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
            IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            fclose(lf_f);
            li_nextstep = DONE;
            break;
        }
        break;

    case IBP_FIFO:
    case IBP_CIRQ:
        if (fseek(lf_f, ll_writeptr + li_HeaderLen, SEEK_SET) == -1) {
            IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            fclose(lf_f);
            li_nextstep = DONE;
            break;
        }
        break;

    default:
        break;
    }

    if (li_nextstep == DONE) {
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    sprintf(lc_LineBuf, "%d \n", IBP_OK);
    if (Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
        handle_error(IBPErrorNb[pi_nbt], &pi_fd);
        fclose(lf_f);
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
        li_Ret =
            Socket2File(pi_srcfd, lf_f, &ibp_cmd[pi_nbt].size, lt_expireTime);
        if (li_Ret != IBP_OK) {
            ftruncate(fileno(lf_f), ps_container->currentSize + li_HeaderLen);
            fclose(lf_f);
            IBPErrorNb[pi_nbt] = li_Ret;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = DONE;
            break;
        }
        ll_written = ibp_cmd[pi_nbt].size;
        break;

    case IBP_FIFO:
    case IBP_CIRQ:
        li_Ret = Socket2Queue(pi_srcfd,
                              lf_f,
                              ibp_cmd[pi_nbt].size,
                              ps_container->maxSize,
                              ll_writeptr,
                              &ll_written, li_HeaderLen, lt_expireTime);
        if (li_Ret != IBP_OK) {
            IBPErrorNb[pi_nbt] = li_Ret;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = DONE;
            break;
        }
        ps_container->size = ll_size;
        ps_container->wrcount = ll_wrcount;
        ps_container->rdcount = ll_rdcount;
        ps_container->free = ll_free;

        break;

      deafult:
        break;
    }

    if (li_nextstep == DONE) {
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_store data transfered\n");
#endif

    switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
        ps_container->currentSize += ibp_cmd[pi_nbt].size;
#ifdef HAVE_PTHREAD_H
        info = (Thread_Info *) ps_container->thread_data;
        if ((info->read > -1) && (ps_container->currentSize >= info->read)) {
            pthread_cond_signal(&(info->read_cond));
        }
#endif
        break;

    case IBP_BUFFER:
        ftruncate(fileno(lf_f), ibp_cmd[pi_nbt].size + li_HeaderLen);
        ps_container->currentSize = ibp_cmd[pi_nbt].size;
#ifdef HAVE_PTHREAD_H
        info = (Thread_Info *) ps_container->thread_data;
        if ((info->read > -1) && (ps_container->currentSize >= info->read)) {
            pthread_cond_signal(&(info->read_cond));
        }
#endif
        break;

    case IBP_FIFO:
    case IBP_CIRQ:
#ifdef HAVE_PTHREAD_H
        info = (Thread_Info *) ps_container->thread_data;
        if ((info->read > -1) && (ll_size - ll_free >= info->read)) {
            pthread_cond_signal(&(info->read_cond));
        }
#endif
        break;

    default:
        break;
    }

    if (li_nextstep == DONE) {
        handle_store_end(pi_fd, pi_nbt, ps_container);
        return;
    }

    fseek(lf_f, sizeof(int), SEEK_SET);
    fwrite(ps_container, 1, BASE_HEADER, lf_f);
    fflush(lf_f);
    fclose(lf_f);
    sprintf(lc_LineBuf, "%d %lu\n", IBP_OK, ll_written);
    Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
    SaveInfo2LSS(ps_container, 2);

    handle_store_end(pi_fd, pi_nbt, ps_container);
    return;
}

void handle_lsd_end(FILE * pf_f, IBP_byteArray ps_container)
{
    fclose(pf_f);
    ps_container->readLock--;

    /*
       ps_container->reference--;
       assert(ps_container->reference >= 0 );
       * check whether this capability has been deleted *
       if (ps_container->deleted == 1)
       {
       if ((ps_container->writeLock == 0) && (ps_container->readLock == 0))
       {
       SaveInfo2LSS(ps_container, 2);
       jettisonByteArray(ps_container);
       }
       }
     */
    return;
}


/*****************************/
/* handle_lsd                */
/* It handles the user's load and delieve requirement, envoked by handle_require() */
/* It sends the data saved a temporary file on the disk to the user  */
/* return: none              */
/*****************************/
void handle_lsd(int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
    int li_HeaderLen, li_ret = IBP_OK;
    unsigned long int ll_TrueSize;
    char lc_LineBuf[CMD_BUF_SIZE];
    FILE *lf_f = NULL;
    int li_nextstep = 1;
    char lc_generic[MAX_CAP_LEN];
    char *lc_TargetKey, *lc_WriteTag, *lc_GenII;
    int li_fddata = 0, li_response;
    int li_lsd = 0;
    unsigned long ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
    char **lp_fields;
    int li_nfields;
    int li_timeout;
    time_t lt_expireTime;
#ifdef HAVE_PTHREAD_H
    Thread_Info *info;
    int left_time;
    int start_time;
    struct timespec spec;
#endif


#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_lsd beginning\n");
    fprintf(stderr, "T %d, handle lsd\n", pi_nbt);
#endif

    if (ibp_cmd[pi_nbt].ServerSync != 0) {
        lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
    }
    else {
        lt_expireTime = 0;
    }

    /*
     *  open the file and read the header
     */
    lf_f = fopen(ps_container->fname, "r");

    if (lf_f == NULL) {
        handle_error(IBP_E_FILE_OPEN, &pi_fd);
        ps_container->reference--;
        assert(ps_container->reference >= 0);
        return;
    }

    /* read lock added */
    ps_container->readLock++;

    if (fread(&li_HeaderLen, sizeof(int), 1, lf_f) == 0) {
        handle_error(IBP_E_FILE_ACCESS, &pi_fd);
        handle_lsd_end(lf_f, ps_container);
        return;
    }


    switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
        if (ibp_cmd[pi_nbt].size > 0) {
            ll_TrueSize = ibp_cmd[pi_nbt].size;

#ifdef HAVE_PTHREAD_H
            if (ps_container->currentSize <
                ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
                left_time = ibp_cmd[pi_nbt].ServerSync;
                info = (Thread_Info *) ps_container->thread_data;
                info->read_wait++;
                if (info->read != -1) {
                    start_time = time(NULL);
                    spec.tv_sec = time(NULL) + left_time;
                    spec.tv_nsec = 0;
                    pthread_mutex_lock(&filock);
                    if (pthread_cond_timedwait
                        (&(info->read_protect), &filock, &spec) != 0) {
                        handle_error(IBP_E_BAD_FORMAT, &pi_fd);
                        pthread_mutex_unlock(&filock);
                        info->read_wait--;
                        li_nextstep = HLSD_DONE;
                        break;
                    }
                    else {
                        left_time -= (time(NULL) - start_time);
                        pthread_mutex_unlock(&filock);
                    }
                }
                if (ps_container->currentSize <
                    ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
                    info->read =
                        ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size;
                    spec.tv_sec = time(NULL) + left_time;
                    spec.tv_nsec = 0;
                    pthread_mutex_lock(&filock);
                    if (pthread_cond_timedwait
                        (&(info->read_cond), &filock, &spec) != 0) {
                        info->read = -1;
                        info->read_wait--;
                        if (ps_container->currentSize <=
                            ibp_cmd[pi_nbt].Offset) {
                            pthread_mutex_unlock(&filock);
                            handle_error(IBP_E_SERVER_TIMEOUT, &pi_fd);
                            if (info->read_wait > 0) {
                                pthread_cond_signal(&(info->read_protect));
                            }
                            li_nextstep = HLSD_DONE;
                            break;
                        }
                        else {
                            info->read = -1;
                            info->read_wait--;
                            pthread_mutex_unlock(&filock);
                            if (info->read_wait > 0) {
                                pthread_cond_signal(&(info->read_protect));
                            }
                        }
                    }
                    else {
                        info->read = -1;
                        info->read_wait--;
                        pthread_mutex_unlock(&filock);
                        if (info->read_wait > 0) {
                            pthread_cond_signal(&(info->read_protect));
                        }
                    }
                }
                else {
                    info->read_wait--;
                    if (info->read_wait > 0) {
                        pthread_cond_signal(&(info->read_protect));
                    }
                }
            }

#endif
            ll_TrueSize =
                (ps_container->currentSize <=
                 ibp_cmd[pi_nbt].Offset +
                 ibp_cmd[pi_nbt].size ? (ps_container->currentSize >
                                         ibp_cmd[pi_nbt].
                                         Offset ? ps_container->currentSize -
                                         ibp_cmd[pi_nbt].
                                         Offset : 0) : ibp_cmd[pi_nbt].size);
            if (fseek(lf_f, li_HeaderLen + ibp_cmd[pi_nbt].Offset, SEEK_SET)
                == -1) {
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                li_nextstep = HLSD_DONE;
                break;
            }
        }
        else {
            if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                li_nextstep = HLSD_DONE;
                break;
            }
            ll_TrueSize = ps_container->currentSize;
        }
        break;

    case IBP_FIFO:
    case IBP_CIRQ:
        if (ps_container->key == NULL) {
            IBPErrorNb[pi_nbt] = IBP_E_INTERNAL;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }

        ll_wrcount = ps_container->wrcount;
        ll_rdcount = ps_container->rdcount;
        ll_free = ps_container->free;
        ll_size = ps_container->size;
        ll_readptr = ll_rdcount;

# ifdef IBP_DEBUG
        fprintf(stderr, "T %d, Lock file :%lu:%lu:%lu:%lu:\n", pi_nbt,
                ll_wrcount, ll_rdcount, ll_free, ll_size);
# endif

        if (ibp_cmd[pi_nbt].size > (ll_size - ll_free)) {
#ifdef HAVE_PTHREAD_H
            info = (Thread_Info *) ps_container->thread_data;
            info->read = ibp_cmd[pi_nbt].size;
            spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
            spec.tv_nsec = 0;
            pthread_mutex_lock(&filock);
            if (pthread_cond_timedwait(&(info->read_cond), &filock, &spec) !=
                0) {
                info->read = -1;
                pthread_mutex_unlock(&filock);
                if (ps_container->size - ps_container->free == 0) {
                    handle_error(IBP_E_GENERIC, &pi_fd);
                    li_nextstep = HLSD_DONE;
                    break;
                }
            }
            else {
                info->read = -1;
                pthread_mutex_unlock(&filock);
            }
# else
            handle_error(IBP_E_GENERIC, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
#endif
        }
        ll_wrcount = ps_container->wrcount;
        ll_rdcount = ps_container->rdcount;
        ll_free = ps_container->free;
        ll_size = ps_container->size;

        if (ibp_cmd[pi_nbt].size > 0) {
            if (ibp_cmd[pi_nbt].size <= (ll_size - ll_free)) {
                ll_TrueSize = ibp_cmd[pi_nbt].size;
            }
            else {
                ll_TrueSize = ll_size - ll_free;
            }

            if (fseek(lf_f, li_HeaderLen + ll_rdcount, SEEK_SET) == -1) {
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                li_nextstep = HLSD_DONE;
                break;
            }
        }
        else {
            if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                li_nextstep = HLSD_DONE;
                break;
            }
            ll_TrueSize = ll_size - ll_free;
        }
        break;

    default:
        break;
    }

    if (li_nextstep == HLSD_DONE) {
        handle_lsd_end(lf_f, ps_container);
        return;
    }


    switch (ibp_cmd[pi_nbt].Cmd) {
    case IBP_LOAD:
        sprintf(lc_LineBuf, "%d %lu \n", IBP_OK, ll_TrueSize);

        if (Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
            li_nextstep = HLSD_DONE;
            break;
        }
        if ((ibp_cmd[pi_nbt].size == -2) || (ll_TrueSize == 0)) {
            li_nextstep = HLSD_DONE;
            break;
        }
        li_fddata = pi_fd;
        break;

    case IBP_SEND_BU:
        strcpy(lc_generic, ibp_cmd[pi_nbt].Cap);
        li_nfields = GetCapFieldsNum(lc_generic, '/');
        if (li_nfields < 6) {
            handle_error(IBP_E_WRONG_CAP_FORMAT, &pi_nbt);
            li_nextstep = HLSD_DONE;
            break;
        }

        lp_fields = malloc(sizeof(char *) * (li_nfields));
        if (lp_fields == NULL) {
            handle_error(IBP_E_INTERNAL, &pi_nbt);
            li_nextstep = HLSD_DONE;
            break;
        }

        if (GetCapFields(lp_fields, lc_generic, '/') < 0) {
            handle_error(IBP_E_WRONG_CAP_FORMAT, &pi_nbt);
            li_nextstep = HLSD_DONE;
            break;
        }

        ibp_cmd[pi_nbt].host = lp_fields[2];
        lc_TargetKey = lp_fields[3];
        lc_GenII = lp_fields[4];
        lc_WriteTag = lp_fields[5];

        if (GetCapFields(lp_fields, ibp_cmd[pi_nbt].host, ':') < 0) {
            handle_error(IBP_E_WRONG_CAP_FORMAT, &pi_nbt);
            li_nextstep = HLSD_DONE;
            break;
        }

        ibp_cmd[pi_nbt].host = lp_fields[0];
        ibp_cmd[pi_nbt].port = atoi(lp_fields[1]);

        free(lp_fields);

        if ((ibp_cmd[pi_nbt].host == NULL) || (lc_TargetKey == NULL)
            || (lc_GenII == NULL)) {
            handle_error(IBP_E_WRONG_CAP_FORMAT, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }
        if (lc_WriteTag == NULL) {
            handle_error(IBP_E_CAP_NOT_WRITE, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }
        if (strcmp(lc_WriteTag, "WRITE") != 0) {
            handle_error(IBP_E_CAP_NOT_WRITE, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }
        if (sscanf(lc_GenII, "%d", &ibp_cmd[pi_nbt].writekey) != 1) {
            handle_error(IBP_E_WRONG_CAP_FORMAT, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }

        li_fddata = SocketConnect(ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

        if (li_fddata == -1) {
            handle_error(IBP_E_CONNECTION, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }
        li_timeout = 0;

        sprintf(lc_LineBuf, "%d %d %s %d %lu %d \n",
                IBPv031,
                IBP_STORE,
                lc_TargetKey,
                ibp_cmd[pi_nbt].writekey, ll_TrueSize, li_timeout);

        if (Write(li_fddata, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
            handle_error(IBP_E_SOCK_WRITE, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }

        if (ReadLine(li_fddata, lc_LineBuf, sizeof(lc_LineBuf)) == -1) {
            handle_error(IBP_E_SOCK_READ, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }

        if (sscanf(lc_LineBuf, "%d", &li_response) != 1) {
            handle_error(IBP_E_BAD_FORMAT, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }

        if (li_response != IBP_OK) {
            handle_error(li_response, &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }
        break;

    case IBP_DELIVER:
        li_fddata = SocketConnect(ibp_cmd[pi_nbt].host, ibp_cmd[pi_nbt].port);

        if (li_fddata == -1) {
            handle_error(IBP_E_CONNECTION, &pi_fd);
            li_nextstep = DONE;
            break;
        }
        break;

    default:
        break;

    }

    if (li_nextstep == HLSD_DONE) {
        handle_lsd_end(lf_f, ps_container);
        return;
    }

    switch (ps_container->attrib.type) {
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
        li_ret = File2Socket(lf_f, li_fddata, ll_TrueSize, lt_expireTime);
        SaveInfo2LSS(ps_container, 2);
        break;

    case IBP_FIFO:
        li_ret = Fifo2Socket(lf_f, li_fddata, ll_TrueSize,
                             ps_container->maxSize, ll_readptr, li_HeaderLen,
                             lt_expireTime);
        if (li_ret >= 0) {
            ll_rdcount = (ll_rdcount + ll_TrueSize) % ll_size;
            ll_free += ll_TrueSize;
            ps_container->size = ll_size;
            ps_container->wrcount = ll_wrcount;
            ps_container->rdcount = ll_rdcount;
            ps_container->free = ll_free;
            SaveInfo2LSS(ps_container, 2);
        }

# ifdef HAVE_PTHREAD_H
        info = (Thread_Info *) ps_container->thread_data;
        if ((info->write > 0) && (ll_free >= info->write)) {
            pthread_cond_signal(&(info->write_cond));
        }
# endif
        break;

    case IBP_CIRQ:
        if ((ibp_cmd[pi_nbt].Cmd == IBP_SEND)
            || (ibp_cmd[pi_nbt].Cmd == IBP_DELIVER))
            ibp_cmd[pi_nbt].Offset = 0;
        li_ret = Fifo2Socket(lf_f, li_fddata, ll_TrueSize,
                             ps_container->maxSize, ll_readptr, li_HeaderLen,
                             lt_expireTime);
        if (li_ret >= 0) {
            ll_rdcount = (ll_rdcount + ll_TrueSize) % ll_size;
            ll_free += ll_TrueSize;
            ps_container->size = ll_size;
            ps_container->wrcount = ll_wrcount;
            ps_container->rdcount = ll_rdcount;
            ps_container->free = ll_free;
            SaveInfo2LSS(ps_container, 2);
        }
        break;

    default:
        break;
    }

    if (li_nextstep == HLSD_DONE) {
        handle_lsd_end(lf_f, ps_container);
        return;
    }

    switch (ibp_cmd[pi_nbt].Cmd) {
    case IBP_LOAD:
        break;

    case IBP_SEND_BU:
        if (li_ret != IBP_OK) {
            IBPErrorNb[pi_nbt] = li_ret;
            sprintf(lc_LineBuf, "%d \n", IBPErrorNb[pi_nbt]);
            Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
            li_nextstep = HLSD_DONE;
            break;
        }

        if (ReadLine(li_fddata, lc_LineBuf, sizeof(lc_LineBuf)) == -1) {
            IBPErrorNb[pi_nbt] = IBP_E_SOCK_READ;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            close(li_fddata);
            li_nextstep = HLSD_DONE;
            break;
        }
        close(li_fddata);

        if (sscanf(lc_LineBuf, "%d %lu\n", &li_response, &ll_TrueSize) != 2) {
            IBPErrorNb[pi_nbt] = IBP_E_BAD_FORMAT;
            handle_error(IBPErrorNb[pi_nbt], &pi_fd);
            li_nextstep = HLSD_DONE;
            break;
        }

        sprintf(lc_LineBuf, "%d %lu\n", li_response, ll_TrueSize);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        break;

    case IBP_DELIVER:
        if (li_ret != IBP_OK) {
            IBPErrorNb[pi_nbt] = li_ret;
            li_response = IBPErrorNb[pi_nbt];
        }
        else
            li_response = IBP_OK;

        close(li_fddata);
        sprintf(lc_LineBuf, "%d %lu \n", li_response, ll_TrueSize);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        break;

    default:
        break;
    }

    handle_lsd_end(lf_f, ps_container);
#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_lsd finished\n");
#endif

    return;
}


/*****************************/
/* handle_copy               */
/* It handles the user's copy requirement, envoked by handle_require() */
/* It sends the data to another IBP depot  */
/* return: none              */
/*****************************/
void handle_copy(int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
    int li_HeaderLen, li_ret = IBP_OK;
    unsigned long int ll_TrueSize;
    char lc_LineBuf[CMD_BUF_SIZE];
    FILE *lf_f = NULL;
    int li_nextstep = 0;
    int li_response;
    int li_lsd = 0;
    unsigned long ll_size, ll_free, ll_wrcount, ll_rdcount, ll_readptr = 0;
    IBP_cap ls_cap;
    struct ibp_timer ls_timeout;
    time_t lt_expireTime;

#ifdef HAVE_PTHREAD_H
    Thread_Info *info;
    int left_time;
    int start_time;
    struct timespec spec;
#endif

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_copy beginning\n");
#endif

    if (ibp_cmd[pi_nbt].Cmd == IBP_SEND) {
        li_lsd = 20;
    }

    ps_container->readLock++;

#ifdef IBP_DEBUG
    fprintf(stderr, "T %d, handle copy\n", pi_nbt);
#endif

    if (ibp_cmd[pi_nbt].ServerSync > 0) {
        lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
    }
    else {
        ibp_cmd[pi_nbt].ServerSync = 3600 * 24;
        lt_expireTime = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
    }

    li_nextstep += ps_container->attrib.type;

    while (li_nextstep > HLSD_DONE) {
        switch (li_nextstep) {

        case HLSD_INIT_BA:
        case HLSD_INIT_BU:
        case HLSD_INIT_FI:
        case HLSD_INIT_CQ:

            /*
             *  open the file and read the header
             */

            lf_f = fopen(ps_container->fname, "r");

            if (lf_f == NULL) {
                handle_error(IBP_E_FILE_OPEN, &pi_fd);
                li_nextstep = HLSD_DONE;
                continue;
            }

            if (fread(&li_HeaderLen, sizeof(int), 1, lf_f) == 0) {
                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                li_nextstep = HLSD_DONE;
                continue;
            }

            li_nextstep += IBP_STEP;
            break;


        case HLSD_SEEK_BA:
        case HLSD_SEEK_BU:
            if (ibp_cmd[pi_nbt].size >= 0) {
                ll_TrueSize = ibp_cmd[pi_nbt].size;

#ifdef HAVE_PTHREAD_H
                if (ps_container->currentSize <
                    ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
                    left_time = ibp_cmd[pi_nbt].ServerSync;
                    info = (Thread_Info *) ps_container->thread_data;
                    info->read_wait++;
                    if (info->read != -1) {
                        start_time = time(NULL);
                        spec.tv_sec = time(NULL) + left_time;
                        spec.tv_nsec = 0;
                        pthread_mutex_lock(&filock);
                        if (pthread_cond_timedwait
                            (&(info->read_protect), &filock, &spec) != 0) {
                            handle_error(IBP_E_BAD_FORMAT, &pi_fd);
                            pthread_mutex_unlock(&filock);
                            info->read_wait--;
                            li_nextstep = HLSD_DONE;
                            break;
                        }
                        else {
                            left_time -= (time(NULL) - start_time);
                            pthread_mutex_unlock(&filock);
                        }
                    }
                    if (ps_container->currentSize <
                        ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size) {
                        info->read =
                            ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size;
                        spec.tv_sec = time(NULL) + left_time;
                        spec.tv_nsec = 0;
                        pthread_mutex_lock(&filock);
                        if (pthread_cond_timedwait
                            (&(info->read_cond), &filock, &spec) != 0) {
                            info->read = -1;
                            info->read_wait--;
                            if (ps_container->currentSize <=
                                ibp_cmd[pi_nbt].Offset) {
                                pthread_mutex_unlock(&filock);
                                handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                                if (info->read_wait > 0) {
                                    pthread_cond_signal(&
                                                        (info->read_protect));
                                }
                                li_nextstep = HLSD_DONE;
                                break;
                            }
                            else {
                                info->read = -1;
                                info->read_wait--;
                                pthread_mutex_unlock(&filock);
                                if (info->read_wait > 0) {
                                    pthread_cond_signal(&
                                                        (info->read_protect));
                                }
                            }
                        }
                        else {
                            info->read = -1;
                            info->read_wait--;
                            pthread_mutex_unlock(&filock);
                            if (info->read_wait > 0) {
                                pthread_cond_signal(&(info->read_protect));
                            }
                        }
                    }
                    else {
                        info->read_wait--;
                        if (info->read_wait > 0) {
                            pthread_cond_signal(&(info->read_protect));
                        }
                    }
                }

#endif
                ll_TrueSize =
                    (ps_container->currentSize <=
                     ibp_cmd[pi_nbt].Offset +
                     ibp_cmd[pi_nbt].size ? (ps_container->currentSize >
                                             ibp_cmd[pi_nbt].
                                             Offset ? ps_container->
                                             currentSize -
                                             ibp_cmd[pi_nbt].
                                             Offset : 0) : ibp_cmd[pi_nbt].
                     size);
                if (fseek
                    (lf_f, li_HeaderLen + ibp_cmd[pi_nbt].Offset,
                     SEEK_SET) == -1) {
                    handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                    li_nextstep = HLSD_DONE;
                    continue;
                }
            }
            else {
                if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
                    handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                    li_nextstep = HLSD_DONE;
                    continue;
                }
                ll_TrueSize = ps_container->currentSize;
            }
            li_nextstep += IBP_STEP;
            li_nextstep += li_lsd;

#ifdef IBP_DEBUG
            strcat(exec_info[pi_nbt], "  handle_copy size determined\n");
#endif

            break;

        case HLSD_SEEK_FI:
        case HLSD_SEEK_CQ:
            ll_wrcount = ps_container->wrcount;
            ll_rdcount = ps_container->rdcount;
            ll_free = ps_container->free;
            ll_size = ps_container->size;

            ll_readptr = ll_rdcount;
# ifdef IBP_DEBUG
            fprintf(stderr, "T %d, Lock file :%lu:%lu:%lu:%lu:\n", pi_nbt,
                    ll_wrcount, ll_rdcount, ll_free, ll_size);
# endif

            if (ibp_cmd[pi_nbt].size > (ll_size - ll_free)) {
#ifdef HAVE_PTHREAD_H
                info = (Thread_Info *) ps_container->thread_data;
                info->read = ibp_cmd[pi_nbt].size;
                spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
                spec.tv_nsec = 0;
                pthread_mutex_lock(&filock);
                if (pthread_cond_timedwait(&(info->read_cond), &filock, &spec)
                    != 0) {
                    info->read = -1;
                    handle_error(IBP_E_GENERIC, &pi_fd);
                    li_nextstep = HLSD_DONE;
                    pthread_mutex_unlock(&filock);
                    break;
                }
                else {
                    info->read = -1;
                    li_nextstep = ps_container->attrib.type;
                    pthread_mutex_unlock(&filock);
                    break;
                }
# else
                handle_error(IBP_E_GENERIC, &pi_fd);
                li_nextstep = HLSD_DONE;
                continue;
#endif
            }
            if (ibp_cmd[pi_nbt].size > 0) {
                ll_TrueSize =
                    (ll_size - ll_free <
                     ibp_cmd[pi_nbt].size) ? ll_size -
                    ll_free : ibp_cmd[pi_nbt].size;
                if (fseek(lf_f, li_HeaderLen + ll_rdcount, SEEK_SET) == -1) {
                    handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                    li_nextstep = HLSD_DONE;
                    continue;
                }
            }
            else {
                if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
                    handle_error(IBP_E_FILE_ACCESS, &pi_fd);
                    li_nextstep = HLSD_DONE;
                    continue;
                }
                ll_TrueSize = ll_size - ll_free;
            }

            li_nextstep += IBP_STEP;
            li_nextstep += li_lsd;
            break;

        case HSC_SEND_BA:
        case HSC_SEND_BU:

            ls_cap = (char *) malloc(strlen(ibp_cmd[pi_nbt].Cap) + 1);
            if (ls_cap == NULL) {
                handle_error(IBP_E_INTERNAL, &pi_fd);
                li_nextstep = HLSD_DONE;
                continue;
            }
            strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
            ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
            ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

#ifdef IBP_DEBUG
            strcat(exec_info[pi_nbt], "  before copy_File2Socket \n");
#endif

            li_ret =
                copy_File2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize,
                                 lt_expireTime);


#ifdef IBP_DEBUG
            strcat(exec_info[pi_nbt], "  after  copy_File2Socket\n");
#endif

            SaveInfo2LSS(ps_container, 2);

            li_nextstep += IBP_STEP;
            li_nextstep += IBP_STEP;
            break;

        case HSC_SEND_FI:

            ls_cap = (char *) malloc(strlen(ibp_cmd[pi_nbt].Cap) + 1);
            if (ls_cap == NULL) {
                handle_error(IBP_E_INTERNAL, &pi_fd);
                li_nextstep = HLSD_DONE;
                continue;
            }
            strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
            ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
            ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;
            li_ret =
                copy_Fifo2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize,
                                 ps_container->maxSize, ll_readptr,
                                 li_HeaderLen, lt_expireTime);

            if (li_ret > 0) {
                ll_rdcount = (ll_rdcount + li_ret) % ll_size;
                ll_free += li_ret;
                ps_container->size = ll_size;
                ps_container->wrcount = ll_wrcount;
                ps_container->rdcount = ll_rdcount;
                ps_container->free = ll_free;
                SaveInfo2LSS(ps_container, 2);
            }

            li_nextstep += IBP_STEP;
            li_nextstep += IBP_STEP;
# ifdef HAVE_PTHREAD_H
            info = (Thread_Info *) ps_container->thread_data;
            if ((info->write > 0) && (ll_free >= info->write)) {
                pthread_cond_signal(&(info->write_cond));
            }
# endif
            break;


        case HSC_SEND_CQ:

            ibp_cmd[pi_nbt].Offset = 0;
            ls_cap = (char *) malloc(strlen(ibp_cmd[pi_nbt].Cap) + 1);
            if (ls_cap == NULL) {
                handle_error(IBP_E_INTERNAL, &pi_fd);
                li_nextstep = HLSD_DONE;
                continue;
            }
            strcpy(ls_cap, ibp_cmd[pi_nbt].Cap);
            ls_timeout.ClientTimeout = ibp_cmd[pi_nbt].TRGTOUT;
            ls_timeout.ServerSync = ibp_cmd[pi_nbt].TRGSYNC;

            li_ret =
                copy_Fifo2Socket(lf_f, ls_cap, &ls_timeout, ll_TrueSize,
                                 ps_container->maxSize, ll_readptr,
                                 li_HeaderLen, lt_expireTime);

            if (li_ret > 0) {
                ll_rdcount = (ll_rdcount + li_ret) % ll_size;
                ll_free += li_ret;
                ps_container->size = ll_size;
                ps_container->wrcount = ll_wrcount;
                ps_container->rdcount = ll_rdcount;
                ps_container->free = ll_free;
                SaveInfo2LSS(ps_container, 2);
            }

            li_nextstep += IBP_STEP;
            li_nextstep += IBP_STEP;
            break;


        case HSC_AFTER_BA:
        case HSC_AFTER_BU:
        case HSC_AFTER_FI:
        case HSC_AFTER_CQ:
            if (li_ret < 0) {
                IBPErrorNb[pi_nbt] = li_ret;
                sprintf(lc_LineBuf, "%d \n", IBPErrorNb[pi_nbt]);
                Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
                li_nextstep = HLSD_DONE;
                continue;
            }

            li_response = IBP_OK;
            sprintf(lc_LineBuf, "%d %lu\n", li_response, li_ret);
            Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
            li_nextstep = HLSD_DONE;
            break;

        }
    }

    fclose(lf_f);

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_copy finished\n");
#endif
#ifdef IBP_DEBUG
    fprintf(stderr, "T %d, handle copy done\n", pi_nbt);
#endif
    ps_container->readLock--;

    /*
       ps_container->reference--;
       assert(ps_container->reference >= 0 );
       * check whether this capability has been deleted *
       if (ps_container->deleted == 1)
       {
       if ((ps_container->writeLock == 0) && (ps_container->readLock == 0))
       {
       SaveInfo2LSS(ps_container, 2);
       jettisonByteArray(ps_container);
       }
       }
     */
    if (ls_cap != NULL) {
        free(ls_cap);
    }

    return;

}

/*****************************/
/* handle_manage             */
/* It handles the user's manage requirement, envoked by handle_require() */
/* There are four management commands: INCR, DECR, PROBE and CHNG  */
/* return: the name of manage cap     */
/*****************************/
char *handle_manage(int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
    time_t lt_CurTime;
    time_t lt_NewDuration;
    char lc_LineBuf[CMD_BUF_SIZE];
    long ll_inc;
    IBP_request ls_request;
    char *lc_Return;
    Jval lu_val;
#ifdef HAVE_PTHREAD_H
    time_t left_time;
    struct timespec spec;
#endif

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_manage beginning\n");
#endif

#ifdef IBP_DEBUG
    fprintf(stderr, "in handle manage\n");
#endif

    lc_Return = malloc(PATH_MAX);
    strcpy(lc_Return, ps_container->fname);
    ps_container->reference--;
    assert(ps_container->reference >= 0);


    if ((ps_container->manageKey != ibp_cmd[pi_nbt].ManageKey)
        && (ibp_cmd[pi_nbt].ManageCmd != IBP_PROBE)) {
        handle_error(IBP_E_INVALID_MANAGE_CAP, &pi_fd);
        return lc_Return;
    }
    if ((ps_container->manageKey != ibp_cmd[pi_nbt].ManageKey) &&
        (ps_container->readKey != ibp_cmd[pi_nbt].ManageKey) &&
        (ps_container->writeKey != ibp_cmd[pi_nbt].ManageKey)) {
        handle_error(IBP_E_INVALID_MANAGE_CAP, &pi_fd);
        return lc_Return;
    }

    time(&lt_CurTime);
    switch (ibp_cmd[pi_nbt].ManageCmd) {

    case IBP_INCR:

        if (ibp_cmd[pi_nbt].CapType == IBP_READCAP)
            ps_container->readRefCount++;
        else if (ibp_cmd[pi_nbt].CapType == IBP_WRITECAP)
            ps_container->writeRefCount++;
        else {
            handle_error(IBP_E_INVALID_PARAMETER, &pi_fd);
            return lc_Return;
        }

        SaveInfo2LSS(ps_container, 2);

        sprintf(lc_LineBuf, "%d \n", IBP_OK);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        break;

    case IBP_DECR:

        if (ibp_cmd[pi_nbt].CapType == IBP_READCAP) {
            ps_container->readRefCount--;

            if (ps_container->readRefCount == 0) {
                if ((ps_container->writeLock == 0) &&
                    (ps_container->readLock == 0) &&
                    dl_empty(ps_container->pending)) {
                    SaveInfo2LSS(ps_container, 2);
                    jettisonByteArray(ps_container);
                }
                else {
                    ps_container->deleted = 1;
                }
            }
        }
        else if (ibp_cmd[pi_nbt].CapType == IBP_WRITECAP) {
            if (ps_container->writeRefCount == 0) {
                handle_error(IBP_E_CAP_ACCESS_DENIED, &pi_fd);
                return lc_Return;
            }
            ps_container->writeRefCount--;
        }
        else {
            handle_error(IBP_E_INVALID_PARAMETER, &pi_fd);
            return lc_Return;
        }


        sprintf(lc_LineBuf, "%d \n", IBP_OK);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        break;

    case IBP_PROBE:
        if (ps_container->deleted == 2) {
            handle_error(IBP_E_CAP_DELETING, &pi_fd);
            return lc_Return;
        }

        if ((ps_container->attrib.type == IBP_BYTEARRAY) ||
            (ps_container->attrib.type == IBP_BUFFER))
            sprintf(lc_LineBuf, "%d %d %d %lu %lu %ld %d %d \n",
                    IBP_OK,
                    ps_container->readRefCount,
                    ps_container->writeRefCount,
                    ps_container->currentSize,
                    ps_container->maxSize,
                    ps_container->attrib.duration - lt_CurTime,
                    ps_container->attrib.reliability,
                    ps_container->attrib.type);
        else
            sprintf(lc_LineBuf, "%d %d %d %lu %lu %lu %d %d \n",
                    IBP_OK,
                    ps_container->readRefCount,
                    ps_container->writeRefCount,
                    ps_container->size - ps_container->free,
                    ps_container->maxSize,
                    ps_container->attrib.duration - lt_CurTime,
                    ps_container->attrib.reliability,
                    ps_container->attrib.type);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        return lc_Return;

    case IBP_CHNG:
        if (((ps_container->attrib.type == IBP_FIFO) ||
             (ps_container->attrib.type == IBP_CIRQ)) &&
            (ibp_cmd[pi_nbt].NewSize != ps_container->maxSize)) {
            handle_error(IBP_E_WOULD_DAMAGE_DATA, &pi_fd);
            return lc_Return;
        }

        if ((glb.MaxDuration == 0) ||
            ((glb.MaxDuration > 0) && ((ibp_cmd[pi_nbt].lifetime <= 0) ||
                                       (glb.MaxDuration <
                                        ibp_cmd[pi_nbt].lifetime)))) {
            handle_error(IBP_E_LONG_DURATION, &pi_fd);
            return lc_Return;
        }

        lt_NewDuration =
            (ibp_cmd[pi_nbt].lifetime >
             0 ? lt_CurTime + ibp_cmd[pi_nbt].lifetime : 0);

        if (ibp_cmd[pi_nbt].reliable != ps_container->attrib.reliability) {
            handle_error(IBP_E_INV_PAR_ATRL, &pi_fd);
            return lc_Return;
        }

        if (check_allocate(pi_fd, pi_nbt, ibp_cmd[pi_nbt].NewSize) != 0) {
            return lc_Return;
        }

        ll_inc = ibp_cmd[pi_nbt].NewSize - ps_container->maxSize;

# ifdef HAVE_PTHREAD_H
        pthread_mutex_lock(&change_lock);
# endif
        if (ibp_cmd[pi_nbt].reliable == IBP_VOLATILE) {
            pthread_mutex_unlock(&change_lock);
            if (check_size(ll_inc, IBP_VOLATILE) != 0) {
# ifdef HAVE_PTHREAD_H
                /*pthread_mutex_unlock (&change_lock); */
# endif
                handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
                return lc_Return;
            }
            pthread_mutex_lock(&change_lock);
            glb.VolStorageUsed += ll_inc;
            ps_container->maxSize = ibp_cmd[pi_nbt].NewSize;
        }
        else {
            pthread_mutex_unlock(&change_lock);
            if (check_size(ll_inc, IBP_STABLE) != 0) {
# ifdef HAVE_PTHREAD_H
                /*pthread_mutex_unlock (&change_lock); */
# endif
                handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
                return lc_Return;
            }
            pthread_mutex_lock(&change_lock);
            glb.StableStorageUsed += ll_inc;
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
                    jrb_insert_int(glb.TransientTree, lt_NewDuration, lu_val);
            }
            ps_container->attrib.duration = lt_NewDuration;
        }
# ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(&change_lock);
# endif

        SaveInfo2LSS(ps_container, 2);

        sprintf(lc_LineBuf, "%d \n", IBP_OK);
        Write(pi_fd, lc_LineBuf, strlen(lc_LineBuf));
        break;

    default:
        handle_error(IBP_E_INVALID_CMD, &pi_fd);
        return lc_Return;
    }

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_manage done\n");
#endif
#ifdef IBP_DEBUG
    fprintf(stderr, " handle manage done\n");
#endif
    return lc_Return;
}

/*****************************/
/* handle_status             */
/* It handles the user's status requirement, envoked by handle_require() */
/* There are two commands: INQ  and CHNG  */
/* return: none              */
/*****************************/
void handle_status(int pi_fd, int pi_nbt)
{
    char *lc_buffer;
    unsigned long ll_NewStable, ll_NewVol;
    long ll_NewDuration;
    ibp_ulong tmp_NewStable, tmp_NewVol;
    char tmpStb[50], tmpStbUsed[50], tmpVol[50], tmpVolUsed[50];
    ibp_ulong lu_softsize;
#ifdef HAVE_STATVFS
#ifdef HAVE_STRUCT_STATVFS64_F_BSIZE
    struct statvfs64 ls_buf;
#else /*HAVE_STATVFS64 */
    struct statvfs ls_buf;
#endif
#elif HAVE_STATFS
    struct statfs ls_buf;
#endif


#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_status beginning\n");
#endif

    lc_buffer = malloc(CMD_BUF_SIZE);
    if (lc_buffer == NULL) {
        handle_error(E_ALLOC, NULL);
        return;
    }

    if (ibp_cmd[pi_nbt].ManageCmd == IBP_ST_CHANGE) {
        if (strcmp(ibp_cmd[pi_nbt].passwd, gc_passwd) != 0) {
            sprintf(lc_buffer, "%d \n", IBP_E_WRONG_PASSWD);
            if (Write(pi_fd, lc_buffer, strlen(lc_buffer)) == -1) {
                IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
                close(pi_fd);
                free(lc_buffer);
                return;
            }
        }
        else {
            sprintf(lc_buffer, "%d \n", IBP_OK);
            if (Write(pi_fd, lc_buffer, strlen(lc_buffer)) == -1) {
                IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
                close(pi_fd);
                free(lc_buffer);
                return;
            }

            if (ReadLine(pi_fd, lc_buffer, CMD_BUF_SIZE) == -1) {
                IBPErrorNb[pi_nbt] = IBP_E_SOCK_READ;
                close(pi_fd);
                free(lc_buffer);
                return;
            }

            sscanf(lc_buffer, "%lu %lu %ld",
                   &ll_NewStable, &ll_NewVol, &ll_NewDuration);
# ifdef HAVE_PTHREAD_H
            pthread_mutex_lock(&change_lock);
# endif
            tmp_NewStable = ll_NewStable;
            tmp_NewStable = tmp_NewStable << 20;
            tmp_NewVol = ll_NewVol;
            tmp_NewVol = tmp_NewVol << 20;
            if (tmp_NewStable < glb.StableStorageUsed)
                IBPErrorNb[pi_nbt] = IBP_E_WOULD_DAMAGE_DATA;
            else
                glb.StableStorage = tmp_NewStable;
            if (tmp_NewVol < glb.VolStorageUsed)
                IBPErrorNb[pi_nbt] = IBP_E_WOULD_DAMAGE_DATA;
            else
                glb.VolStorage = tmp_NewVol;

            if (ll_NewDuration > 0)
                ll_NewDuration *= (3600 * 24);
# ifdef HAVE_PTHREAD_H
            pthread_mutex_unlock(&change_lock);
# endif

            glb.MaxDuration = ll_NewDuration;
        }
    }

    /* get the free space size */
#ifdef HAVE_STATVFS
#ifdef HAVE_STATVFS64
    if (statvfs64(glb.VolDir,&ls_buf) != 0) {
#else /* HAVE_STATVFS64 */
    if (statvfs(glb.VolDir, &ls_buf) != 0) {
#endif
#elif HAVE_STATFS
    if (statfs(glb.VolDir, &ls_buf) != 0) {
#endif
        lu_softsize = glb.VolStorage;
    }
    else {
        lu_softsize = (ibp_ulong) ls_buf.f_bsize * (ibp_ulong) ls_buf.f_bfree;
        lu_softsize += glb.StableStorageUsed + glb.VolStorageUsed;
        if (glb.VolStorage < lu_softsize) {
            lu_softsize = glb.VolStorage;
        }
    }


# ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(&change_lock);
# endif
    sprintf(lc_buffer, "%s %s %s %s %ld %s %s \n",
            ibp_ultostr((glb.StableStorage >> 20), tmpStb + 49, 10),
            ibp_ultostr((glb.StableStorageUsed >> 20), tmpStbUsed + 49, 10),
            ibp_ultostr((lu_softsize >> 20), tmpVol + 49, 10),
            ibp_ultostr((glb.VolStorageUsed >> 20), tmpVolUsed + 49, 10),
            glb.MaxDuration, IBP_VER_MAJOR, IBP_VER_MINOR);

# ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&change_lock);
# endif

# ifdef IBP_DEBUG
    fprintf(stderr, "T %d, Status %s\n", pi_nbt, lc_buffer);
# endif

    if (Write(pi_fd, lc_buffer, strlen(lc_buffer) + 1) == -1) {
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
        close(pi_fd);
        free(lc_buffer);
        return;
    }

    free(lc_buffer);
#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "  handle_status finished\n");
#endif
    return;
}

/*****************************/
/* handle_mcopy              */
/* return: none              */
/*****************************/
void handle_mcopy(int pi_connfd, int pi_nbt)
{
    char *lc_buffer, *dm_buffer;
    int pid, status, retval, rr;
    char respBuf[CMD_BUF_SIZE];
    char *args[6], *path, temp[8];
    char fullPath[256];



  /*****sprintf (respBuf, "%d %lu \n", IBP_OK, 100);
  if(Write(pi_connfd, respBuf, strlen (respBuf)) < 0)
  perror("Write back to client failed"); */

    lc_buffer = (char *) malloc(sizeof(char) * (ibp_cmd[pi_nbt].size + 1));
    dm_buffer = (char *) malloc(sizeof(char) * (ibp_cmd[pi_nbt].size + 10));

    if ((lc_buffer == NULL) || (dm_buffer == NULL)) {
        fprintf(stderr, "there is no enought memory for DM\n");
        exit(1);
    }

    if (ReadLine(pi_connfd, lc_buffer, ibp_cmd[pi_nbt].size) == -1) {
        handle_error(IBP_E_SOCK_READ, &pi_connfd);
        ibp_cmd[pi_nbt].Cmd = IBP_NOP;
        return;
    }
  /**sprintf(dm_buffer,"%d %d%s", IBPv031, IBP_MCOPY, lc_buffer); */
    strcpy(dm_buffer, lc_buffer);

    pid = fork();
    if (pid < 0) {
        perror("sim: error forking");
        exit(2);
    }
    if (pid != 0) {
        waitpid(pid, &status, 0);
    /*if(WEXITSTATUS(status) == 1 || WEXITSTATUS(status)==-1){*/
        if (WEXITSTATUS(status) == 1) {
            sprintf(respBuf, "%d %lu \n", IBP_OK, 100);
            if (Write(pi_connfd, respBuf, strlen(respBuf)) < 0)
                perror("Write back to client failed");
            close(pi_connfd);
        }
        else if(WEXITSTATUS(status) != 2){
           handle_error(IBP_E_DATAMOVER_FAILURE,&pi_connfd);
           ibp_cmd[pi_nbt].Cmd = IBP_NOP;
           return;
        }
    }
    else if (pid == 0) {
        sprintf(fullPath, "%s%s", glb.execPath, "DM");
        args[0] = fullPath;
        args[1] = dm_buffer;
        args[2] = glb.StableDir;
        sprintf(temp, "%d", glb.IbpPort);
        args[3] = temp;
        args[4] = NULL;
        if ((retval = execvp(fullPath, args)) < 0) {
            fprintf(stderr,
                    "ibp_server_mt: problem initializing DataMover process: ");
            perror(" ");
        }
    }

}

/*****************************/
/* decode_cmd                */
/* It decodes the user's requirement, envoked by handle_require() */
/* return: none              */
/*****************************/
void *decode_cmd(int pi_connfd, int pi_nbt)
{
    char *lc_buffer, *lc_singleton, *lc_save;
    int li_nextstep = DONE, li_prot;
    IBP_byteArray ls_container = NULL;
    JRB ls_rb;
    int li_index;
    Jval lu_val;
    time_t tt;
    char tt_tmp[50];

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "   decode_cmd beginning\n");
#endif

    lc_save = malloc(CMD_BUF_SIZE);
    lc_buffer = lc_save;
    lc_singleton = malloc(CMD_BUF_SIZE);

    if (ReadComm(pi_connfd, lc_buffer, CMD_BUF_SIZE) == -1) {
        handle_error(IBP_E_SOCK_READ, NULL);
        ibp_cmd[pi_nbt].Cmd = IBP_NOP;
        return NULL;
    }
    else {
#ifdef IBP_DEBUG
        fprintf(stderr, "T %d, received request: %s\n", pi_nbt, lc_buffer);
        strcat(exec_info[pi_nbt], "     received request: ");
        strcat(exec_info[pi_nbt], lc_buffer);

#ifdef HAVE_PTHREAD_H
        tt = time(0);
        strcpy(tt_tmp, ctime(&tt));
        tt_tmp[strlen(tt_tmp) - 1] = '\0';
        ibpWriteThreadLog("[%s]  received request : %s", tt_tmp, lc_buffer);
#endif
#endif
    }

    lc_buffer = (char *) IBP_substr(lc_buffer, lc_singleton);

    if ((li_prot = atoi(lc_singleton)) != IBPv031) {
#ifdef IBP_DEBUG
        fprintf(stderr, "T %d, protocol version not recognised: %d\n", pi_nbt,
                li_prot);
#endif
        handle_error(IBP_E_PROT_VERS, NULL);
        ibp_cmd[pi_nbt].Cmd = IBP_NOP;
        return NULL;
    }

    lc_buffer = (char *) IBP_substr(lc_buffer, lc_singleton);

    ibp_cmd[pi_nbt].Cmd = atoi(lc_singleton);
#ifdef IBP_DEBUG
    printf("T %d will execute command %d\n", pi_nbt, ibp_cmd[pi_nbt].Cmd);
#endif

    switch (ibp_cmd[pi_nbt].Cmd) {
    case IBP_ALLOCATE:
        li_nextstep = RELY;
        break;
    case IBP_STORE:
    case IBP_DELIVER:
    case IBP_SEND:
    case IBP_REMOTE_STORE:
    case IBP_LOAD:
    case IBP_MANAGE:
        li_nextstep = KEY;
        break;
    case IBP_STATUS:
        li_nextstep = DCD_CMD;
        break;
    case IBP_MCOPY:
        lc_buffer = (char *) IBP_substr(lc_buffer, lc_singleton);
        ibp_cmd[pi_nbt].size = atoi(lc_singleton);
        li_nextstep = DONE;
        break;
    default:
        ibp_cmd[pi_nbt].Cmd = IBP_NOP;
        break;
    }

    if (ibp_cmd[pi_nbt].Cmd == IBP_NOP)
        return NULL;

    while (li_nextstep != DONE) {

        lc_buffer = (char *) IBP_substr(lc_buffer, lc_singleton);

        switch (li_nextstep) {

        case RELY:
            ibp_cmd[pi_nbt].reliable = atoi(lc_singleton);
#ifdef IBP_DEBUG
            fprintf(stderr, "T %d, Reliability : %d\n", pi_nbt,
                    ibp_cmd[pi_nbt].reliable);
#endif

            if ((ibp_cmd[pi_nbt].reliable != IBP_STABLE) &&
                (ibp_cmd[pi_nbt].reliable != IBP_VOLATILE)) {
                handle_error(IBP_E_INVALID_PARAMETER, &pi_connfd);
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

            switch (ibp_cmd[pi_nbt].Cmd) {
            case IBP_ALLOCATE:
                li_nextstep = TYPE;
                break;
            case IBP_MANAGE:
                li_nextstep = DCD_SRVSYNC;
                break;
            }
            break;

        case KEY:
            strcpy(ibp_cmd[pi_nbt].key, lc_singleton);

#ifdef IBP_DEBUG
            fprintf(stderr, "T %d, Key : %s\n", pi_nbt, ibp_cmd[pi_nbt].key);
#endif

            /*
             * get the bytearray now
             */

            pthread_mutex_lock(&change_lock);
            ls_rb = jrb_find_str(glb.ByteArrayTree, ibp_cmd[pi_nbt].key);

            if (ls_rb == NULL) {
                pthread_mutex_unlock(&change_lock);
                handle_error(IBP_E_CAP_NOT_FOUND, &pi_connfd);
                /*close(pi_connfd); */
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

            lu_val = jrb_val(ls_rb);
            ls_container = (IBP_byteArray) lu_val.s;

            if (ls_container->deleted == 1) {
                pthread_mutex_unlock(&change_lock);
                handle_error(IBP_E_CAP_NOT_FOUND, &pi_connfd);
                ls_container = NULL;
                /*close(pi_connfd); */
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }
            ls_container->reference++;
            pthread_mutex_unlock(&change_lock);

            switch (ibp_cmd[pi_nbt].Cmd) {
            case IBP_STORE:
                li_nextstep = WKEY;
                break;
            case IBP_LOAD:
                li_nextstep = RKEY;
                break;
            case IBP_REMOTE_STORE:
            case IBP_DELIVER:
                li_nextstep = HOST;
                break;
            case IBP_SEND:
                li_nextstep = DCD_IBPCAP;
                break;
            case IBP_MANAGE:
                li_nextstep = MKEY;
                break;
            }
            break;

        case TYPE:
            ibp_cmd[pi_nbt].type = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Type : %d\n", pi_nbt, ibp_cmd[pi_nbt].type);
#endif

            if ((ibp_cmd[pi_nbt].type != IBP_BYTEARRAY) &&
                (ibp_cmd[pi_nbt].type != IBP_FIFO) &&
                (ibp_cmd[pi_nbt].type != IBP_BUFFER) &&
                (ibp_cmd[pi_nbt].type != IBP_CIRQ)) {
                handle_error(IBP_E_INVALID_PARAMETER, &pi_connfd);
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

            li_nextstep = LIFE;
            break;

        case LIFE:
            ibp_cmd[pi_nbt].lifetime = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Lifetime : %d\n", pi_nbt, ibp_cmd[pi_nbt].lifetime);
#endif

            switch (ibp_cmd[pi_nbt].Cmd) {
            case IBP_ALLOCATE:
                li_nextstep = SIZE;
                break;
            case IBP_MANAGE:
                li_nextstep = RELY;
                break;
            }
            break;

        case HOST:
            strcpy(ibp_cmd[pi_nbt].host, lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Host : %s\n", pi_nbt, ibp_cmd[pi_nbt].host);
#endif

            li_nextstep = PORT;
            break;

        case PORT:
            ibp_cmd[pi_nbt].port = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Port: %hu\n", pi_nbt, ibp_cmd[pi_nbt].port);
#endif

            switch (ibp_cmd[pi_nbt].Cmd) {
            case IBP_REMOTE_STORE:
                li_nextstep = WKEY;
                break;
            case IBP_DELIVER:
                li_nextstep = RKEY;
                break;
            case IBP_SEND:
                li_nextstep = DCD_IBPCAP;
                break;
            }
            break;

        case WKEY:
            ibp_cmd[pi_nbt].writekey = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Write Key: %d\n", pi_nbt, ibp_cmd[pi_nbt].writekey);
#endif

            if (ibp_cmd[pi_nbt].writekey != ls_container->writeKey) {
                handle_error(IBP_E_INVALID_WRITE_CAP, &pi_connfd);
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

            li_nextstep = SIZE;
            break;

        case RKEY:
            ibp_cmd[pi_nbt].ReadKey = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Read Key: %d\n", pi_nbt, ibp_cmd[pi_nbt].ReadKey);
#endif

            if (ibp_cmd[pi_nbt].ReadKey != ls_container->readKey) {
                handle_error(IBP_E_INVALID_READ_CAP, &pi_connfd);
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

            li_nextstep = OFFSET;
            break;

        case OFFSET:
            errno = 0;
            ibp_cmd[pi_nbt].Offset = atol(lc_singleton);
            if (errno != 0) {
                handle_error(IBP_E_INV_PAR_SIZE, &pi_connfd);
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

#ifdef IBP_DEBUG
            printf("T %d, Offset: %lu\n", pi_nbt, ibp_cmd[pi_nbt].Offset);
#endif

            li_nextstep = SIZE;
            break;

        case SIZE:
            errno = 0;
            ibp_cmd[pi_nbt].size = atol(lc_singleton);
            if (errno != 0) {
                if (ibp_cmd[pi_nbt].size == LONG_MAX) {
                    handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_connfd);
                }
                else {
                    handle_error(IBP_E_INV_PAR_SIZE, &pi_connfd);
                }
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

#ifdef IBP_DEBUG
            printf("T %d, Size : %lu\n", pi_nbt, ibp_cmd[pi_nbt].size);
#endif

            li_nextstep = DCD_SRVSYNC;
            break;

        case MKEY:
            ibp_cmd[pi_nbt].ManageKey = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Manage Key : %d\n", pi_nbt,
                   ibp_cmd[pi_nbt].ManageKey);
#endif

            li_nextstep = DCD_CMD;
            break;

        case DCD_CMD:
            ibp_cmd[pi_nbt].ManageCmd = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Manage/Status Command : %d\n", pi_nbt,
                   ibp_cmd[pi_nbt].ManageCmd);
#endif
            switch (ibp_cmd[pi_nbt].Cmd) {
            case IBP_MANAGE:
                li_nextstep = DCD_CAP;
                break;
            case IBP_STATUS:
                li_nextstep = DCD_PWD;
                break;
            }
            break;

        case DCD_CAP:
            ibp_cmd[pi_nbt].CapType = atoi(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Cap Type : %d\n", pi_nbt, ibp_cmd[pi_nbt].CapType);
#endif

            switch (ibp_cmd[pi_nbt].ManageCmd) {
            case IBP_CHNG:
                li_nextstep = DCD_MAXSIZE;
                break;
            default:
                li_nextstep = DCD_SRVSYNC;
                break;
            }
            break;

        case DCD_MAXSIZE:
            errno = 0;
            ibp_cmd[pi_nbt].NewSize = atol(lc_singleton);
            if (errno != 0) {
                if (ibp_cmd[pi_nbt].NewSize == LONG_MAX) {
                    handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_connfd);
                }
                else {
                    handle_error(IBP_E_INV_PAR_SIZE, &pi_connfd);
                }
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
            }

#ifdef IBP_DEBUG
            printf("T %d, New size : %lu\n", pi_nbt, ibp_cmd[pi_nbt].NewSize);
#endif

            li_nextstep = LIFE;
            break;

        case DCD_IBPCAP:
            strcpy(ibp_cmd[pi_nbt].Cap, lc_singleton);

#ifdef IBP_DEBUG
            fprintf(stderr, "T %d, Cap: %s\n", pi_nbt,
                    (char *) ibp_cmd[pi_nbt].Cap);
#endif

            li_nextstep = RKEY;
            break;

        case DCD_PWD:
            if (ibp_cmd[pi_nbt].ManageCmd == 2) {
                li_index = 0;
                while ((li_index < 16) && (lc_singleton[li_index] != '\n')
                       && (lc_singleton[li_index] != '\0'))
                    li_index++;
                lc_singleton[li_index] = '\0';

                strcpy(ibp_cmd[pi_nbt].passwd, lc_singleton);
# ifdef IBP_DEBUG
                fprintf(stderr, "T %d, Password: %s\n", pi_nbt,
                        (char *) ibp_cmd[pi_nbt].passwd);
# endif
            }
            li_nextstep = DCD_SRVSYNC;
            break;

        case DCD_SRVSYNC:
            ibp_cmd[pi_nbt].ServerSync = atol(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, ServerSync : %d\n", pi_nbt,
                   ibp_cmd[pi_nbt].ServerSync);
#endif

            switch (ibp_cmd[pi_nbt].Cmd) {
            case IBP_SEND:
                li_nextstep = DCD_TRGSYNC;
                break;
            default:
                li_nextstep = DONE;
                break;
            }
            break;

        case DCD_TRGSYNC:
            ibp_cmd[pi_nbt].TRGSYNC = atol(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Target ServerSync : %d\n", pi_nbt,
                   ibp_cmd[pi_nbt].TRGSYNC);
#endif

            li_nextstep = DCD_TRGTOUT;
            break;

        case DCD_TRGTOUT:
            ibp_cmd[pi_nbt].TRGTOUT = atol(lc_singleton);

#ifdef IBP_DEBUG
            printf("T %d, Target ClientTimeout : %d\n", pi_nbt,
                   ibp_cmd[pi_nbt].TRGTOUT);
#endif

            li_nextstep = DONE;
            break;

        default:
            li_nextstep = DONE;
            break;
        }
    }

    free(lc_singleton);
    free(lc_save);

#ifdef IBP_DEBUG
    strcat(exec_info[pi_nbt], "   decode finished\n");
#endif

    if ((ibp_cmd[pi_nbt].Cmd == IBP_NOP) && (ls_container != NULL)) {
        pthread_mutex_lock(&change_lock);
        ls_container->reference--;
        assert(ls_container->reference >= 0);
        pthread_mutex_unlock(&change_lock);
    }

    return ls_container;
}


/*
 *
 * Boot part - subroutines boot, decodepar, getconfig, recover
 *
 */


/*****************************/
/* getconfig                */
/* It reads the configure form ibp.cfg  */
/* return: OK / not          */
/*****************************/
int getconfig(char *pc_path)
{
    IS ls_in;
    DIR *ls_dir;
    char *lc_generic;
    int li_return = 0;
    float lf_ftemp;
    IPv4_addr_t *ps_newIPv4, *ps_tmpIPv4;
#ifdef AF_INET6
    IPv6_addr_t *ps_newIPv6, *ps_tmpIPv6;
#endif
    if ((ls_in = (IS) new_inputstruct(pc_path)) == NULL) {

        /* below modified by yong zheng to add hostname in config file and command line */
        fprintf(stderr, "config file %s open error\n", pc_path);
        li_return = -1;

        /*
           if (glb.hostname == NULL){
           handle_error(E_FQDN, NULL);
           li_return = -1;
           }
         */
        /* use configuration parameters */
    }
    else {
        while ((get_line(ls_in) >= 0) && (li_return == 0)) {
            if (ls_in->NF < 2)
                continue;
            if (ls_in->fields[0][0] == '#')
                continue;
            if ((strcmp(ls_in->fields[0], "VOLSIZE") == 0) ||
                (strcmp(ls_in->fields[0], "SOFTSIZE") == 0)) {
                if (glb.VolStorage > 0)
                    continue;
                if ((glb.VolStorage = ibp_getStrSize(ls_in->fields[1])) == 0
                    && errno != 0) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    continue;
                }
            }

            else if ((strcmp(ls_in->fields[0], "STABLESIZE") == 0) ||
                     (strcmp(ls_in->fields[0], "HARDSIZE") == 0)) {
                if (glb.StableStorage > 0)
                    continue;
                if ((glb.StableStorage =
                     ibp_getStrSize(ls_in->fields[1])) == 0 && errno != 0) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    continue;
                }
            }

            else if (strcmp(ls_in->fields[0], "MINFREESIZE") == 0) {
                if ((glb.MinFreeSize = ibp_getStrSize(ls_in->fields[1])) == 0
                    && errno != 0) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    continue;
                }
            }

            else if ((strcmp(ls_in->fields[0], "STABLEDIR") == 0) ||
                     (strcmp(ls_in->fields[0], "HARDDIR") == 0)) {
                if (glb.StableDir != NULL)
                    continue;
                lc_generic = ls_in->fields[1];
                if (lc_generic[0] != '/') {
                    handle_error(E_ABSPATH, &ls_in);
                    li_return = -1;
                    continue;
                }
                if (lc_generic[strlen(lc_generic) - 1] != '/')
                    strcat(lc_generic, "/");
                glb.StableDir = strdup(lc_generic);
                if (glb.StableDir == NULL) {
                    handle_error(E_ALLOC, NULL);
                    li_return = -1;
                    continue;
                }
            }

            else if ((strcmp(ls_in->fields[0], "VOLDIR") == 0) ||
                     (strcmp(ls_in->fields[0], "SOFTDIR") == 0)) {
                if (glb.VolDir != NULL)
                    continue;
                lc_generic = ls_in->fields[1];
                if (lc_generic[0] != '/') {
                    handle_error(E_ABSPATH, &ls_in);
                    li_return = -1;
                    continue;
                }
                if (lc_generic[strlen(lc_generic) - 1] != '/')
                    strcat(lc_generic, "/");
                glb.VolDir = strdup(lc_generic);
                if (glb.VolDir == NULL) {
                    handle_error(E_ALLOC, NULL);
                    li_return = -1;
                    continue;
                }
            }

            else if (strcmp(ls_in->fields[0], "MAXDURATION") == 0) {
                if (sscanf(ls_in->fields[1], "%f", &lf_ftemp) != 1) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    continue;
                }
                if (glb.MaxDuration == 0) {
                    if (lf_ftemp < 0)
                        glb.MaxDuration = -1;
                    else
                        glb.MaxDuration = (long) (lf_ftemp * 24 * 3600);
                }
            }

            else if (strcmp(ls_in->fields[0], "PASSWORD") == 0) {
                if (strcmp(gc_passwd, "ibp") == 0) {
                    strcpy(gc_passwd, ls_in->fields[1]);
                }
            }

            else if (strcmp(ls_in->fields[0], "ALLOCATEPOLICY") == 0) {
                if (sscanf(ls_in->fields[1], "%d", &glb.AllocatePolicy) != 1) {
                    handle_error(E_CFGFILE, &ls_in);
                    glb.AllocatePolicy = IBP_ALLPOL_SID;
                    li_return = -1;
                    continue;
                }
                else {
                    if ((glb.AllocatePolicy < 0) || (glb.AllocatePolicy > 1)) {
                        handle_error(E_CFGFILE, &ls_in);
                        glb.AllocatePolicy = IBP_ALLPOL_SID;
                        li_return = -1;
                        continue;
                    }
                }
            }
            else if (strcmp(ls_in->fields[0], "HOSTNAME") == 0) {
                if (glb.hostname != NULL)
                    continue;
                if ((glb.hostname = strdup(ls_in->fields[1])) == NULL) {
                    handle_error(E_ALLOC, NULL);
                    li_return = -1;
                }
            }
#ifdef HAVE_PTHREAD_H
            else if (strcmp(ls_in->fields[0], "THREADS") == 0) {
                if (glb.IbpThreadMin > 0)
                    continue;
                glb.IbpThreadNum = atoi(ls_in->fields[1]);
                glb.IbpThreadMin = atoi(ls_in->fields[1]);
            }

            else if (strcmp(ls_in->fields[0], "MAXTHREADS") == 0) {
                if (glb.IbpThreadMax > 0)
                    continue;
                if (atoi(ls_in->fields[1]) > MAX_THREADS) {
                    glb.IbpThreadMax = MAX_THREADS;
                }
                else {
                    glb.IbpThreadMax = atoi(ls_in->fields[1]);
                }
            }

#endif
            else if (strcmp(ls_in->fields[0], "CFGPORT") == 0) {
                if (glb.IbpPort != 0)
                    continue;
                if (sscanf(ls_in->fields[1], "%d", &glb.IbpPort) != 1) {
                    handle_error(E_PORT, (void *) &glb.IbpPort);
                    handle_error(E_CFGFILE, &ls_in);
                    glb.IbpPort = IBP_DATA_PORT;
                    li_return = -1;
                    continue;
                }
                if ((glb.IbpPort < 1024) || (glb.IbpPort > 65535)) {
                    handle_error(E_PORT, NULL);
                    handle_error(E_CFGFILE, &ls_in);
                    glb.IbpPort = IBP_DATA_PORT;
                    li_return = -1;
                    continue;
                }
            }

            else if (strcmp(ls_in->fields[0], "CL_IPv4") == 0) {
                if (ls_in->NF < 3) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    continue;
                }

                if ((ps_newIPv4 =
                     (IPv4_addr_t *) malloc(sizeof(IPv4_addr_t))) == NULL) {
                    handle_error(E_ALLOC, NULL);
                    li_return = -1;
                    continue;
                }
                ps_newIPv4->next = NULL;
                if ((ps_newIPv4->addr = inet_addr(ls_in->fields[1])) == -1) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    free(ps_newIPv4);
                    continue;
                }
                if ((ps_newIPv4->mask = inet_addr(ls_in->fields[2])) == -1) {
                    if (strcmp(trim(ls_in->fields[2]), "255.255.255.255") !=
                        0) {
                        handle_error(E_CFGFILE, &ls_in);
                        li_return = -1;
                        free(ps_newIPv4);
                        continue;
                    }
                }

                ps_tmpIPv4 = glb.IPv4;
                if (ps_tmpIPv4 == NULL) {
                    glb.IPv4 = ps_newIPv4;
                }
                else {
                    while (ps_tmpIPv4->next != NULL)
                        ps_tmpIPv4 = ps_tmpIPv4->next;
                    ps_tmpIPv4->next = ps_newIPv4;
                }

                continue;
            }
#ifdef AF_INET6
            else if (strcmp(ls_in->fields[0], "CL_IPv6") == 0) {
                if (ls_in->NF < 3) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    continue;
                }
                /* fprintf(stderr,"IPv6: %s %s\n",ls_in->fields[1],ls_in->fields[2]); */
                if ((ps_newIPv6 =
                     (IPv6_addr_t *) malloc(sizeof(IPv6_addr_t))) == NULL) {
                    handle_error(E_ALLOC, NULL);
                    li_return = -1;
                    continue;
                }
                ps_newIPv6->next = NULL;
                if (inet_pton(AF_INET6, ls_in->fields[1], &(ps_newIPv6->addr))
                    == -1) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    free(ps_newIPv6);
                    continue;
                }
                if (inet_pton(AF_INET6, ls_in->fields[2], &(ps_newIPv6->mask))
                    == -1) {
                    handle_error(E_CFGFILE, &ls_in);
                    li_return = -1;
                    free(ps_newIPv6);
                    continue;
                }
                ps_tmpIPv6 = glb.IPv6;
                if (ps_tmpIPv6 == NULL) {
                    glb.IPv6 = ps_newIPv6;
                }
                else {
                    while (ps_tmpIPv6->next != NULL)
                        ps_tmpIPv6 = ps_tmpIPv6->next;
                    ps_tmpIPv6->next = ps_newIPv6;
                }
                if (inet_ntop
                    (AF_INET6, &(ps_newIPv6->addr), ls_in->fields[1],
                     46) == NULL) {
                    fprintf(stderr, "Error occur in convert address!!");
                    exit(-1);
                }
                if (inet_ntop
                    (AF_INET6, &(ps_newIPv6->mask), ls_in->fields[2],
                     46) == NULL) {
                    fprintf(stderr, "Error occur in convert address!!");
                    exit(-1);
                }
                continue;
            }
#endif
        }                       /* end while */
        jettison_inputstruct(ls_in);
    }

    if (li_return == -1)
        return li_return;

    if (glb.VolStorage + glb.StableStorage <= 0) {
        handle_error(E_ZEROREQ, NULL);
        li_return = -1;
    }

    if (glb.StableDir == NULL)
        glb.StableDir = strdup("/tmp/");

    if (glb.VolDir == NULL)
        glb.VolDir = strdup("/tmp/");

    if (glb.IbpPort == 0)
        glb.IbpPort = IBP_DATA_PORT;

    if (glb.MaxDuration == 0)
        glb.MaxDuration = -1;

    return li_return;
}

/*****************************/
/* recover                   */
/* It recovers the old IBP file when the depot reboots */
/* return: OK / not          */
/*****************************/
int recover()
{
    int li_nextstep;
    char *lc_DirName, lc_NameBuf[1024];
    DIR *ls_sdir;
    FILE *lf_fp;
    struct dirent *ls_entry;
    int li_HeaderLen, li_cont;
    IBP_byteArray ls_container;
    time_t lt_now;
    Jval lu_val;

    char lc_oldlss[128];
    FILE *lf_oldlssfd;
    IBP_StableBackupInfo ls_stableinfo;
    IBP_VariableBackupInfo ls_variableinfo;
    int li_stablesize, li_variablesize;
    time_t lt_currenttime;
    FILE *lf_tempfd;
    char ls_lockfname[PATH_MAX];
    time_t lt_lasttime;
    time_t lt_crashtime;

#ifdef HAVE_PTHREAD_H
    Thread_Info *thread_info;
#endif

    if ((gf_recoverfd = fopen(gc_recoverfile, "r")) == NULL) {
        if ((gf_recoverfd = fopen(gc_recoverfile, "w")) == 0) {
            fprintf(stderr, "LSS file open error\n");
        }
        fclose(gf_recoverfd);
        gf_recoverfd = NULL;
        return (0);
    }
    fclose(gf_recoverfd);
    gf_recoverfd = NULL;

    sprintf(lc_oldlss, "%s%ld", gc_recoverfile, time(NULL));
    if (link(gc_recoverfile, lc_oldlss) != 0) {
        fprintf(stderr, "recover error\n");
        unlink(gc_recoverfile);
        return (E_RECOVER);
    }
    unlink(gc_recoverfile);

    if ((gf_recoverfd = fopen(gc_recoverfile, "w")) == 0) {
        fprintf(stderr, "LSS file open error\n");
        unlink(lc_oldlss);
        return (E_RECOVER);
    }

    if ((lf_oldlssfd = fopen(lc_oldlss, "r")) == 0) {
        fprintf(stderr, "LSS file open error\n");
        unlink(lc_oldlss);
        fclose(gf_recoverfd);
        gf_recoverfd = NULL;
        return (E_RECOVER);
    }

    li_stablesize = sizeof(IBP_StableBackupInfo);
    li_variablesize = sizeof(IBP_VariableBackupInfo);
    lt_currenttime = time(NULL);

    if ((gf_recovertimefd = fopen(gc_recovertimefile, "r")) == NULL) {
        lt_crashtime = glb.MaxDuration;
    }
    else {
        fscanf(gf_recovertimefd, "%u", &lt_lasttime);
        lt_crashtime = lt_currenttime - lt_lasttime;
        if ((lt_crashtime < 0)) {       /*  || (lt_crashtime > glb.MaxDuration)) { */
            lt_crashtime = glb.MaxDuration;
        }
    }

    gf_recovertimefd = NULL;
    gt_recoverstoptime = lt_currenttime + glb.RecoverTime;

    while (1) {

        bzero(&ls_stableinfo, li_stablesize);
        bzero(&ls_variableinfo, li_variablesize);
        bzero(ls_lockfname, PATH_MAX);
        if (fread(&ls_stableinfo, li_stablesize, 1, lf_oldlssfd) < 1) {
            break;
        }
        if (fread(&ls_variableinfo, li_variablesize, 1, lf_oldlssfd) < 1) {
            fprintf(stderr, "LSS read error. recover abort\n");
            break;
        }
        if (ls_variableinfo.valid != IBP_OK) {
            continue;
        }
        if (((ls_variableinfo.attrib.duration > 0)
             && ((ls_variableinfo.attrib.duration + lt_crashtime) <
                 lt_currenttime)) || (glb.IBPRecover != 0)) {
            if (ls_variableinfo.valid != 1) {
                unlink(ls_stableinfo.fname);
                sprintf(ls_lockfname, "%s-LOCK", ls_stableinfo.fname);
                unlink(ls_lockfname);
#ifdef IBP_DEBUG
                fprintf(stderr, "delete %s\n", ls_stableinfo.fname);
#endif
            }
            continue;
        }

        if ((lf_tempfd = fopen(ls_stableinfo.fname, "r")) == NULL) {
            continue;
        }
        fclose(lf_tempfd);
        lf_tempfd = NULL;

        ls_container =
            (IBP_byteArray) calloc(1, sizeof(struct ibp_byteArray));
        if (ls_container == NULL) {
            handle_error(E_BOOT, NULL);
            free(ls_container);
            return (E_RECOVER);
        }

        ls_container->maxSize = ls_variableinfo.maxSize;
        ls_container->currentSize = ls_variableinfo.currentSize;
        ls_container->readRefCount = ls_variableinfo.readRefCount;
        ls_container->writeRefCount = ls_variableinfo.writeRefCount;
        ls_container->writeKey = ls_stableinfo.writeKey;
        ls_container->readKey = ls_stableinfo.readKey;
        ls_container->manageKey = ls_stableinfo.manageKey;
        ls_container->deleted = 0;
        ls_container->reference = 0;
        ls_variableinfo.attrib.duration += lt_crashtime + glb.RecoverTime;
        if (ls_variableinfo.attrib.duration >
            lt_currenttime + glb.MaxDuration) {
            ls_variableinfo.attrib.duration =
                lt_currenttime + glb.MaxDuration;
        }
        ls_container->attrib.duration = ls_variableinfo.attrib.duration;
        ls_container->attrib.reliability = ls_variableinfo.attrib.reliability;
        ls_container->attrib.type = ls_variableinfo.attrib.type;
        ls_container->lock = ls_container->readLock =
            ls_container->writeLock = 0;
        ls_container->pending = make_dl();
        ls_container->key = strdup(ls_stableinfo.key);
        ls_container->fname = strdup(ls_stableinfo.fname);
        ls_container->BackupIndex = glb.NextIndexNumber;
        glb.NextIndexNumber++;
        ls_container->size = ls_variableinfo.size;
        ls_container->free = ls_variableinfo.free;

        ls_container->wrcount = ls_variableinfo.wrcount;
        ls_container->rdcount = ls_variableinfo.rdcount;

        lu_val.s = (char *) ls_container;
        ls_container->masterTreeEntry = jrb_insert_str(glb.ByteArrayTree,
                                                       strdup(ls_container->
                                                              key), lu_val);

        if (ls_container->attrib.reliability == IBP_VOLATILE) {
            glb.VolStorageUsed += ls_container->maxSize;
            ls_container->volTreeEntry = vol_cap_insert(ls_container->maxSize,
                                                        ls_container->attrib.
                                                        duration -
                                                        lt_currenttime,
                                                        lu_val);
        }
        else if (ls_container->attrib.reliability == IBP_STABLE) {
            glb.StableStorageUsed += ls_container->maxSize;
        }

        if (ls_container->attrib.duration > 0)
            ls_container->transientTreeEntry =
                jrb_insert_int(glb.TransientTree,
                               ls_container->attrib.duration, lu_val);

        ls_container->thread_data = NULL;
#ifdef HAVE_PTHREAD_H
        thread_info = (Thread_Info *) malloc(sizeof(Thread_Info));
        if (thread_info == NULL) {
            fprintf(stderr, "No enough memory \n");
            exit(1);
        }
        bzero(thread_info, sizeof(Thread_Info));
        thread_info->read = -1;
        ls_container->thread_data = (void *) thread_info;
#endif
        SaveInfo2LSS(ls_container, 1);
#ifdef IBP_DEBUG
        fprintf(stderr, "recover %s\n", ls_stableinfo.fname);
#endif
    }

    fclose(lf_oldlssfd);
    unlink(lc_oldlss);
    fclose(gf_recoverfd);
    gf_recoverfd = NULL;

    check_size(0, 4);

/*
  truncatestorage(1, (void *) &glb.VolStorage);
*/
    return 0;
}

void show_version_info()
{
    fprintf(stdout, "IBP Server, version %s %s\n", glb.versionMajor,
            glb.versionMinor);
    fprintf(stdout,
            "\nCopyright (C) 2003 Logistical Computing and Internetworking LAB\nUniversity of Tennessee. \n");
}


/*****************************/
/* decodepar                 */
/* It reads the parameters from the command */
/* return: OK / not          */
/*****************************/
int decodepar(int argc, char **argv)
{
    int li_index, li_npar = argc - 1;
    int li_return = 0;
    long ll_dur;

    if (li_npar == 0) {
        fprintf(stdout, "No parameters specified\n");
        return IBP_OK;
    }

    for (li_index = 1; li_index <= li_npar; li_index++) {
        if (strcmp(argv[li_index], "-help") == 0) {
            handle_error(E_USAGE, NULL);
            li_return = -1;
            break;
        }
        else if ((strcmp(argv[li_index], "-V") == 0) ||
                 (strcmp(argv[li_index], "--version") == 0)) {
            show_version_info();
            exit(0);
        }
        else if ((strcmp(argv[li_index], "-s") == 0) && (li_npar > li_index)) {
            if ((glb.StableStorage = ibp_getStrSize(argv[++li_index])) == 0
                && errno != 0) {
                handle_error(E_CMDPAR, NULL);
                li_return = -1;
                break;
            }
        }
        else if ((strcmp(argv[li_index], "-v") == 0) && (li_npar > li_index)) {
            if ((glb.VolStorage = ibp_getStrSize(argv[++li_index])) == 0
                && errno != 0) {
                handle_error(E_CMDPAR, NULL);
                li_return = -1;
                break;
            }
        }
        else if ((strcmp(argv[li_index], "-rt") == 0) && (li_npar > li_index)) {
            if ((glb.RecoverTime = atoi(argv[++li_index])) == 0 && errno != 0) {
                handle_error(E_CMDPAR, NULL);
                li_return = -1;
                break;
            }
        }

        else if ((strcmp(argv[li_index], "-dv") == 0) && (li_npar > li_index)) {
            glb.VolDir = strdup(argv[++li_index]);
            if (glb.VolDir == NULL) {
                handle_error(E_ALLOC, NULL);
                return -1;
            }
        }
        else if ((strcmp(argv[li_index], "-ds") == 0) && (li_npar > li_index)) {
            glb.StableDir = strdup(argv[++li_index]);
            if (glb.StableDir == NULL) {
                handle_error(E_ALLOC, NULL);
                return -1;
            }
        }
        else if ((strcmp(argv[li_index], "-l") == 0) && (li_npar > li_index)) {
            ll_dur = atoi(argv[++li_index]);
            if (ll_dur > 0)
                ll_dur *= (24 * 3600);
            glb.MaxDuration = ll_dur;
        }
        else if ((strcmp(argv[li_index], "-pw") == 0) && (li_npar > li_index)) {
            strncpy(gc_passwd, argv[++li_index], 16);
            gc_passwd[16] = '\0';
        }
        else if ((strcmp(argv[li_index], "-cf") == 0) && (li_npar > li_index)) {
            if (gc_cfgpath[0] != '\0') {
                fprintf(stderr, "Multiple config file path define %s\n",
                        gc_cfgpath);
                fprintf(stderr, "Multiple config file path define %s\n",
                        gc_cfgpath);
                return -1;
            }

            strncpy(gc_cfgpath, argv[++li_index], PATH_MAX);
            if (gc_cfgpath[strlen(gc_cfgpath) - 1] != '/')
                strcat(gc_cfgpath, "/");
        }
        else if ((strcmp(argv[li_index], "-c") == 0) && (li_npar > li_index)) {
            if (gc_cfgname[0] != '\0') {
                fprintf(stderr, "Multiple config file name define\n");
                return -1;
            }

            strncpy(gc_cfgname, argv[++li_index], PATH_MAX);
        }

        else if ((strcmp(argv[li_index], "-hn") == 0) && (li_npar > li_index)) {
            glb.hostname = strdup(argv[++li_index]);
            if (glb.hostname == NULL) {
                handle_error(E_ALLOC, NULL);
                return -1;
            }
        }
        else if ((strcmp(argv[li_index], "-p") == 0) && (li_npar > li_index)) {
            glb.IbpPort = atoi(argv[++li_index]);
        }
        else if ((strcmp(argv[li_index], "-nr") == 0)
                 && (li_npar >= li_index)) {
            glb.IBPRecover = 1;
        }
#ifdef HAVE_PTHREAD_H
        else if ((strcmp(argv[li_index], "-tn") == 0) && (li_npar > li_index)) {
            glb.IbpThreadMin = atoi(argv[++li_index]);
            glb.IbpThreadNum = atoi(argv[li_index]);
        }

        else if ((strcmp(argv[li_index], "-tm") == 0) && (li_npar > li_index)) {
            if (atoi(argv[++li_index]) > MAX_THREADS) {
                glb.IbpThreadMax = MAX_THREADS;
            }
            else {
                glb.IbpThreadMax = atoi(argv[li_index]);
            }
        }

        else if ((strcmp(argv[li_index], "-tl") == 0)
                 && (li_npar >= li_index)) {
            glb.IbpThreadLogEnable = 1;
        }
#endif
        else {
            handle_error(E_USAGE, NULL);
            li_return = -1;
            break;
        }
    }
    return li_return;
}

/*****************************/
/* ibp_boundrycheck          */
/* It does boundry checking for the user configure */
/* return: 0  -- seccuss     */
/*         -1 -- failure     */
/*****************************/
int ibp_boundrycheck()
{
    DIR *ls_dir;

    /* check hard / soft / free storage size */
    if (glb.StableStorage < 0) {
        fprintf(stderr, "Stable/Hard Storage size config error\n");
        return (-1);
    }
    if (glb.VolStorage < 0) {
        fprintf(stderr, "Volatile/Soft Storage size config error\n");
        return (-1);
    }
    if (glb.VolStorage < glb.StableStorage) {
        fprintf(stderr,
                "Volatile/Soft Storage should be no less than Stable/Hard Storage\n");
        return (-1);
    }
    if (glb.MinFreeSize < 0) {
        fprintf(stderr, "Minimal Free Storage size config error\n");
        return (-1);
    }

    /* check max duration */
    if (glb.MaxDuration < -1) {
        fprintf(stderr, "Max Duration config error\n");
        return (-1);
    }

    /* check the directories */
    if (glb.StableDir != NULL) {
        if ((ls_dir = opendir(glb.StableDir)) == NULL) {
            fprintf(stderr, "Stable/Hard directory config error\n");
            return (-1);
        }
        closedir(ls_dir);
    }
    else {
        fprintf(stderr, "Stable/Hard directory config error\n");
        return (-1);
    }

    if (glb.VolDir != NULL) {
        if ((ls_dir = opendir(glb.VolDir)) == NULL) {
            fprintf(stderr, "Volatile/Soft directory config error\n");
            return (-1);
        }
        closedir(ls_dir);
    }
    else {
        fprintf(stderr, "Volatile/Soft directory config error\n");
        return (-1);
    }

    /* check the port # */
    if ((glb.IbpPort < 1024) || (glb.IbpPort > 65535)) {
        fprintf(stderr, "IBP Port config error: 1024-65535\n");
        return (-1);
    }

#ifdef HAVE_PTHREAD_H
    /* check the thread # */
    if (glb.IbpThreadNum > MAX_THREADS) {
        fprintf(stderr, "IBP Thread Number config error: %d\n", MAX_THREADS);
        return (-1);
    }
#endif

    return (0);
}

/*****************************/
/* init_ibp_ping             */
/* It inserts the root into the ByteArrayTree */
/* return: none              */
/*****************************/
void init_ibp_ping()
{

    JRB ls_rb;
    char *lc_Key;

    /*
     * Prepare the IBP-0 file
     */

    lc_Key = malloc(MAX_KEY_LEN);
    if (lc_Key == NULL) {
        fprintf(stderr, "IBP-0 retrieval failed. \n");
        return;
    }
    sprintf(lc_Key, "IBP-0\0");

    ls_rb = jrb_find_str(glb.ByteArrayTree, lc_Key);

    if (ls_rb == NULL) {
#ifdef HAVE_PTHREAD_H
        pthread_mutex_lock(&alloc_lock);
#endif
        handle_allocate(0, 0, IBP_HALL_TYPE_PING);
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(&alloc_lock);
#endif
    }

    free(lc_Key);
    return;
}

/*****************************/
/* boot                      */
/* This function is used to boot the IBP depot */
/* return: OK / not          */
/*****************************/
int boot(int argc, char **argv)
{
    int li_retval;
#ifdef HAVE_GETRLIMIT
    struct rlimit ls_limit;
#endif
    char *lc_generic, lc_path[PATH_MAX];
    char *lc_Now;
    time_t lt_Now;
    char tmp[50];
    char *cp;
    JRB ls_rb;

    /*
     * Initialize a few fields
     */

    memset(&glb, 0, sizeof(globals));
    glb.versionMajor = IBP_VER_MAJOR;
    glb.versionMinor = IBP_VER_MINOR;
    glb.VolStorage = 0;
    glb.StableStorage = 0;
    glb.MinFreeSize = IBP_FREESIZE;
    glb.StableDir = NULL;
    glb.VolDir = NULL;
    glb.MaxDuration = 0;
    glb.IbpPort = 0;
    glb.IBPRecover = 0;
    glb.NextIndexNumber = 1;
    glb.RecoverTime = IBP_RECOVER_TIME;
    gc_passwd = malloc(17);
    if (gc_passwd == NULL) {
        handle_error(E_ALLOC, NULL);
        exit(-1);
    }
    strcpy(gc_passwd, "ibp");
    pthread_mutex_init(&change_lock, NULL);

    /*
     * get path of the executable 
     */
    if ((glb.execPath = strdup(argv[0])) == NULL) {
        handle_error(E_ALLOC, NULL);
        exit(-1);
    }
    cp = strrchr(glb.execPath, '/');
    if (cp == NULL) {
        glb.execPath[0] = '\0';
    }
    else {
        cp[1] = '\0';
    }

#ifdef HAVE_PTHREAD_H
    glb.IbpThreadNum = -1;
    glb.IbpThreadMin = -1;
    glb.IbpThreadMax = -1;
    glb.IbpThreadLogEnable = 0;
#endif

    gi_recoverclosed = 1;
    gc_cfgpath = calloc(sizeof(char), PATH_MAX);
    gc_cfgname = calloc(sizeof(char), PATH_MAX);
    if ((gc_cfgpath == NULL) || (gc_cfgname == NULL)) {
        handle_error(E_ALLOC, NULL);
        exit(-1);
    }

    /*
     * Check if calling parameters are correct
     */

    li_retval = decodepar(argc, argv);
    if (li_retval == -1) {
        return E_DECODEPAR;
    }

    if ((gc_cfgpath[0] == '\0') && (gc_cfgname[1] == '\0')) {
        strcpy(gc_cfgpath, "/etc/");
    }
    if (gc_cfgname[0] == '\0') {
        strcpy(gc_cfgname, "ibp.cfg");
    }

    /*
     * Set the soft limit of open file to the maximum possible (the hard one)
     */
#ifdef HAVE_GETRLIMIT
    if (getrlimit(RLIMIT_NOFILE, &ls_limit) == -1) {
        handle_error(E_GLIMIT, NULL);
        return E_BOOT;
    }

    ls_limit.rlim_cur = ls_limit.rlim_max;
    if (setrlimit(RLIMIT_NOFILE, &ls_limit) == -1) {
        handle_error(E_SLIMIT, NULL);
        return E_BOOT;
    }
#endif

    /*
     * initialize a few fileds
     */

    glb.ByteArrayTree = make_jrb();
    glb.VolatileTree = make_jrb();
    glb.TransientTree = make_jrb();

    time(&lt_Now);
    srandom((unsigned int) lt_Now);


    /*
     * call getconfig, to fill up the parameters needed 
     */

    sprintf(lc_path, "%s%s", gc_cfgpath, gc_cfgname);
#ifdef IBP_DEBUG
    fprintf(stderr, "cfg file: %s\n", lc_path);
#endif
    li_retval = getconfig(lc_path);

    if (li_retval < 0) {
        return E_GETCONFIG;
    }

    /*
     * get infos about hostmane, home directory, etc etc ...
     */
    if (glb.hostname == NULL) {
        /*
         * only search for hostname when can't get from config file or command line
         */
        lc_generic = (char *) getlocalhostname();
        if ((lc_generic != NULL)
            && (strcasecmp(lc_generic, "localhost") != 0)) {
            glb.hostname = strdup(lc_generic);
            if (glb.hostname == NULL) {
                handle_error(E_ALLOC, NULL);
                return E_ALLOC;
            }
        }
    }
    if (glb.hostname == NULL) {
        handle_error(E_FQDN, NULL);
        return (-1);
    }

/*
  if (glb.StableStorage > glb.VolStorage) {
        fprintf(stderr,"VolStorage should be no less than StableStorage\n");
        return(-1);
  }
*/

    if (ibp_boundrycheck() != 0) {
        return E_GETCONFIG;
    }


#ifdef HAVE_PTHREAD_H
    if (glb.IbpThreadNum <= 0)
        glb.IbpThreadNum = NB_COMM_THR_MIN;
    if (glb.IbpThreadMin <= 0)
        glb.IbpThreadMin = NB_COMM_THR_MIN;
    if (glb.IbpThreadMax <= 0)
        glb.IbpThreadMax = NB_COMM_THR_MAX;
    if (glb.IbpThreadMin > glb.IbpThreadMax) {
        glb.IbpThreadMax = glb.IbpThreadMin;
    }
    if ((IBPErrorNb = (int *) calloc(sizeof(int), glb.IbpThreadMax)) == NULL) {
        handle_error(E_ALLOC, NULL);
        return (-1);
    }
#else
    if ((IBPErrorNb = (int *) calloc(sizeof(int), 1)) == NULL) {
        handle_error(E_ALLOC, NULL);
        return (-1);
    }
#endif

    queue = create_queue();

    fprintf(stderr, "Parameters used:\n");
    fprintf(stderr, "\tHostname:\t\t\t%s\n", glb.hostname);
    fprintf(stderr, "\tPort Used:\t\t\t%d\n", glb.IbpPort);
    fprintf(stderr, "\tHard/Stable Storage size:\t%s\n",
            ibp_ultostr(glb.StableStorage, tmp + 49, 10));
    fprintf(stderr, "\tSoft/Volatile Storage size:\t%s\n",
            ibp_ultostr(glb.VolStorage, tmp + 49, 10));
    fprintf(stderr, "\tMinimal Free size:\t\t%s\n",
            ibp_ultostr(glb.MinFreeSize, tmp + 49, 10));
    fprintf(stderr, "\tStable Storage Dir:\t\t%s\n", glb.StableDir);
    fprintf(stderr, "\tVolatile Storage Dir:\t\t%s\n", glb.VolDir);
    /*fprintf(stderr, "\tMax Duration:\t\t\t%ld\n", glb.MaxDuration); */
    if (glb.MaxDuration == -1) {
        fprintf(stderr, "\tDuration:\t\t\t Infinit\n");
    }
    else if (glb.MaxDuration == 0) {
        fprintf(stderr, "\tDuration:\t\t\t 0\n");
    }
    else {
        fprintf(stderr,
                "\tMax Duration:\t\t\t%d Day %d Hours %d Minute %d Second\n",
                glb.MaxDuration / (3600 * 24),
                (glb.MaxDuration % (3600 * 24)) / 3600,
                ((glb.MaxDuration % (3600 * 24)) % 3600) / 60,
                (((glb.MaxDuration % (3600 * 24)) % 3600) % 60));
    }

    /*
     * open the log file
     */
    gc_logfile = malloc(PATH_MAX);
    if (gc_cfgpath[strlen(gc_cfgpath) - 1] != '/')
        sprintf(gc_logfile, "%s/ibp.log", gc_cfgpath);
    else
        sprintf(gc_logfile, "%sibp.log", gc_cfgpath);

    gf_logfd = fopen(gc_logfile, "a");
    if (gf_logfd == NULL) {
        handle_error(E_OFILE, (void *) gc_logfile);
        return E_BOOT;
    }
    gc_recoverfile = malloc(PATH_MAX);
    gc_recovertimefile = malloc(PATH_MAX);
    if (gc_cfgpath[strlen(gc_cfgpath) - 1] != '/') {
        sprintf(gc_recoverfile, "%s/ibp.lss", gc_cfgpath);
        sprintf(gc_recovertimefile, "%s/ibp.time", gc_cfgpath);
    }
    else {
        sprintf(gc_recoverfile, "%sibp.lss", gc_cfgpath);
        sprintf(gc_recovertimefile, "%s/ibp.time", gc_cfgpath);
    }

#if 0
    gc_logfile = malloc(PATH_MAX);
    if (glb.VolDir[strlen(glb.VolDir) - 1] != '/')
        sprintf(gc_logfile, "%s/ibp.log", glb.VolDir);
    else
        sprintf(gc_logfile, "%sibp.log", glb.VolDir);

    gf_logfd = fopen(gc_logfile, "a");
    if (gf_logfd == NULL) {
        handle_error(E_OFILE, (void *) gc_logfile);
        return E_BOOT;
    }
    gc_recoverfile = malloc(PATH_MAX);
    gc_recovertimefile = malloc(PATH_MAX);
    if (glb.VolDir[strlen(glb.VolDir) - 1] != '/') {
        sprintf(gc_recoverfile, "%s/ibp.lss", glb.VolDir);
        sprintf(gc_recovertimefile, "%s/ibp.time", glb.VolDir);
    }
    else {
        sprintf(gc_recoverfile, "%sibp.lss", glb.VolDir);
        sprintf(gc_recovertimefile, "%s/ibp.time", glb.VolDir);
    }
#endif

    lc_Now = ctime(&lt_Now);
    lc_Now[24] = '\0';
    fprintf(gf_logfd, "%s,  booted %s\n", glb.hostname, lc_Now);
    fflush(gf_logfd);

    /*
     * init the IBP-0 for ping
     */

    init_ibp_ping();

    /*
     * check if recover necessary
     */
    li_retval = recover();

    if (li_retval < 0) {
        return E_RECOVER;
    }

    li_retval = check_unmanaged_capability();
    if (li_retval < 0) {
        return E_RECOVER;
    }

    /*
     * set the sockets up
     */

    glb.IbpSocket = serve_known_socket(glb.hostname, glb.IbpPort);
    if (glb.IbpSocket <= 0) {
        handle_error(E_CFGSOK, NULL);
        return E_BOOT;
    }

    FD_ZERO(&glb.MasterSet);
    FD_SET(glb.IbpSocket, &glb.MasterSet);

    Listen(glb.IbpSocket);
    return IBP_OK;
}
