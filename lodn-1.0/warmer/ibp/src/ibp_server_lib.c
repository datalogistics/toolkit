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
# include <unistd.h>
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_thread.h"
# include "fs.h"
# include "ibp_request.h"
# include "ibp_log.h"
# include "ibp_mdns.h"

# ifdef HAVE_SYS_STATVFS_H /* for Solaris */
# include <sys/statvfs.h>
# elif HAVE_SYS_VFS_H  /* for Linux */
# include <sys/vfs.h>
# else  /* for macOS */
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

void generateKey(int *key, int n, int seed ){
  int i;
  unsigned int randseed;

  
  assert(key != NULL && n > 0 );
  pthread_mutex_lock(&(glb.keyLock));
  for ( i = 0 ; i < n ; i ++){
      randseed = random() + time(NULL) + seed;
      srandom(randseed);
      key[i] = random();
  }
  pthread_mutex_unlock(&(glb.keyLock));

  return ;
}

void reload_config_file()
{
  char path[2048];
  IS ls_in;
  
  sprintf(path,"%s%s",gc_cfgpath,gc_cfgname);
  if ( (ls_in = (IS) new_inputstruct(path)) == NULL ){
    return;
  }  
  while (get_line(ls_in) >=0 ) {
    if ( ls_in->NF < 2 ){
      continue;
    }
    if ( ls_in->fields[0][0]== '#' ){
      continue;
    }
    if ( strcmp(ls_in->fields[0],"ENABLEALLOC") == 0 ){
      glb.enableAlloc = atoi(ls_in->fields[1]);
    } 
  }
  jettison_inputstruct(ls_in);
  return;
}

int ibpSaveCapInfo(IBP_byteArray ba)
{
    FILE *fp;
    int  ret = IBP_OK;

    if ( (fp = fopen(ba->fname,"r+")) == NULL ){
        return (IBP_E_FILE_ACCESS);
    }
    if ( fseek(fp,sizeof(int),SEEK_SET) != 0 ){
        ret = IBP_E_FILE_ACCESS;
        goto bail;
    }
    if ( fwrite(ba,1,BASE_HEADER,fp) != BASE_HEADER){
        ret = IBP_E_FILE_ACCESS;
        goto bail;
    }

bail:
    if ( fp != NULL ){
        fclose(fp);
    }

    return(ret);
}

void printGLB (FILE *out ){
  FILE *fp;
  JRB node;
  RESOURCE *rs;
  char pad[]="    ";
  int i=0;

  if (out == NULL ) { 
      fp = stderr; 
  }else {
      fp = out;
  }
  fprintf(fp,"\n  Parameters used:\n");
  /*fprintf(fp,"%sVersion\t: %s  %s\n",pad,glb.versionMajor,glb.versionMinor);*/
  fprintf(fp,"%sHostname\t: %s\n",pad, glb.hostname);
  fprintf(fp,"%sPort Used\t: %d\n",pad, glb.IbpPort);
  fprintf(fp,"%sMin Threads\t: %d\n",pad,glb.IbpThreadMin);
  fprintf(fp,"%sMax Threads\t: %d\n",pad,glb.IbpThreadMax);
  fprintf(fp,"\n%sResource %d\t: <default>\n",pad,0);
  i = 1; 
  printResource(glb.dftRs,out,pad);
  jrb_rtraverse(node,glb.resources){
      rs = (RESOURCE *)((jrb_val(node)).v);
      if ( rs == glb.dftRs ){
          continue;
          fprintf(fp,"\n%sResource %d\t: <default>\n",pad,i++);
      }else {
        fprintf(fp,"\n%sResource %d\n",pad,i++);
      }
      printResource(rs,out,pad);
  }
}

/*****************************/
/* createcap                 */
/* This function creates IBP cap, envoked by handle_allocate()  */
/* return: the name of capability       */
/*****************************/
char* createcap (RESOURCE *rs , char *pc_Key, char *pc_RWM, char **rwmKey, int version) 
{

  char		*lc_buf;
  int     secLen;
  char    tmp[MAX_DIM_LEN + 1];
  int     *sec;
  int     i;

  assert( ( rs != NULL) && ( pc_Key != NULL) && 
          ( pc_RWM != NULL) && ( rwmKey != NULL));

#if 0
  switch ( version ){
    case IBPv031:
      secLen = MAX_RWM_KEY_LEN_OLD / MAX_DIM_LEN;
      break;
    case IBPv040:
      secLen = MAX_RWM_KEY_LEN / MAX_DIM_LEN;
      break;
    default:
      assert(0);
      break;
  }
#endif
  secLen = MAX_RWM_KEY_LEN / MAX_DIM_LEN;


  if ( ( sec = (int *)calloc(secLen, sizeof(int))) == NULL ){
    return (NULL);
  }

  if  ( (*rwmKey = (char*)calloc(MAX_RWM_KEY_LEN+1,sizeof(char))) == NULL ){
    free(sec);
    return (NULL);
  }
  if  ( ( lc_buf = (char*)calloc(MAX_CAP_LEN,sizeof(char))) == NULL ) {
    free(sec);
    free(*rwmKey);
    return (NULL);
  }
  generateKey(sec,secLen,0);

  rwmKey[0][0] = '\0';
  for ( i = 0 ; i < secLen ; i ++ ){
      sprintf(tmp,"%d",sec[i]);
      strcat(*rwmKey,tmp);
  }
  sprintf(lc_buf,"ibp://%s:%d/%d#%s/%s/%s",  
	glb.hostname, 
	glb.IbpPort,
  rs->RID,
	pc_Key, 
	*rwmKey, 
	pc_RWM);

  free(sec);
  return ( lc_buf);

#if 0
  lc_tmp = (char*)malloc(strlen(lc_buf)+1);
  strcpy(lc_tmp,lc_buf);
  return(lc_tmp);
#endif 

}

void releaseAllocate( RESOURCE *rs , IBP_byteArray ba){
    assert (rs != NULL);

    switch ( rs->type ){
        case DISK:
            if ( ba->fname != NULL){
                unlink(ba->fname);
            }
            break;
        case ULFS:
            ibpfs_delete(((FAT*)(rs->loc.ulfs.ptr)),ba->fname);
            break;
        case MEMORY:
            if ( ba->startArea != NULL) {
                memset(ba->startArea,'*',ba->maxSize);
                free(ba->startArea);
            }
            ba->startArea = NULL;
            break;
        default:
            return;
    }
    return;
}
/*****************************/
/* jettisonByteArray         */
/* It deletes the node in ByteArrayTree and releases its resource  */
/* return: none              */
/*****************************/
#if 0
void jettisonByteArray(IBP_byteArray ba){
    return ;
}
#endif
void jettisonByteArray( RESOURCE *rs, IBP_byteArray ba){

  JRB         temp;  
  char        lineBuf[CMD_BUF_SIZE];

  assert (rs != NULL);

  pthread_mutex_lock(&(rs->change_lock));
  ba->deleted = 1;
  if ( ba->reference > 0 ){
    pthread_mutex_unlock(&(rs->change_lock));
    return;
  }
  releaseAllocate(rs,ba); 

  /*unlink(ba->fname);        */ /*            Remove underlying file */
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

  if (ba->volTreeEntry != NULL) {        /* Remove entry from volatile */
    jrb_delete_node (ba->volTreeEntry);  /* tree if it is volatile */

# ifdef HAVE_PTHREAD_H 
    /*pthread_mutex_lock (&change_lock);*/
    rs->VolStorageUsed -= ba->maxSize;
    if ((thread_glb.StorageNeeded > 0) && \
       (thread_glb.StorageNeeded < rs->VolStorage-rs->VolStorageUsed-rs->StableStorageUsed))
      {
        pthread_cond_signal(&(thread_glb.Cond));
      }
    /*pthread_mutex_unlock (&change_lock);*/
# else
    rs->VolStorageUsed -= ba->maxSize;
# endif
  }
  else {
# ifdef HAVE_PTHREAD_H 
    /*pthread_mutex_lock (&change_lock);*/
    rs->StableStorageUsed -= ba->maxSize;
    if ((thread_glb.StorageNeeded > 0) && \
       (thread_glb.StorageNeeded < rs->VolStorage-rs->VolStorageUsed-rs->StableStorageUsed))
      {
        pthread_cond_signal(&(thread_glb.Cond));
      }
    /*pthread_mutex_unlock (&change_lock);*/
# else
    rs->StableStorageUsed -= ba->maxSize;
# endif
  }

  /*SaveInfo2LSS(rs,ba, 3);*/

  if (ba->transientTreeEntry != NULL)   /* Remove entry from transient */
    jrb_delete_node(ba->transientTreeEntry); /* tree if it is transient */
  temp = ba->masterTreeEntry;          /* Remove entry from master */

  free(temp->key.s);                 /* Byte Array tree */
# ifdef HAVE_PTHREAD_H 
    free((Thread_Info *)ba->thread_data);
# endif
  if ( ba->readKey_s != NULL ) { free(ba->readKey_s);}
  if ( ba->writeKey_s != NULL ) { free(ba->writeKey_s);}
  if ( ba->manageKey_s != NULL ) { free(ba->manageKey_s);}
  /*free(ba);*/
  jrb_delete_node(temp);
  pthread_mutex_unlock(&(rs->change_lock));
  SaveInfo2LSS(rs,ba,3);
  memset(ba,'*',sizeof(struct ibp_byteArray));
  free(ba);
  return;
}

void jettisonByteArrayEx( RESOURCE *rs, IBP_byteArray ba){

  JRB         temp;  
  char        lineBuf[CMD_BUF_SIZE];

  assert (rs != NULL);

  ba->deleted = 1;
  if ( ba->reference > 0 ){
    return;
  }
  releaseAllocate(rs,ba); 

  /*unlink(ba->fname);        */ /*            Remove underlying file */
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

  if (ba->volTreeEntry != NULL) {        /* Remove entry from volatile */
    jrb_delete_node (ba->volTreeEntry);  /* tree if it is volatile */

# ifdef HAVE_PTHREAD_H 
    /*pthread_mutex_lock (&change_lock);*/
    rs->VolStorageUsed -= ba->maxSize;
    if ((thread_glb.StorageNeeded > 0) && \
       (thread_glb.StorageNeeded < rs->VolStorage-rs->VolStorageUsed-rs->StableStorageUsed))
      {
        pthread_cond_signal(&(thread_glb.Cond));
      }
    /*pthread_mutex_unlock (&change_lock);*/
# else
    rs->VolStorageUsed -= ba->maxSize;
# endif
  }
  else {
# ifdef HAVE_PTHREAD_H 
    /*pthread_mutex_lock (&change_lock);*/
    rs->StableStorageUsed -= ba->maxSize;
    if ((thread_glb.StorageNeeded > 0) && \
       (thread_glb.StorageNeeded < rs->VolStorage-rs->VolStorageUsed-rs->StableStorageUsed))
      {
        pthread_cond_signal(&(thread_glb.Cond));
      }
    /*pthread_mutex_unlock (&change_lock);*/
# else
    rs->StableStorageUsed -= ba->maxSize;
# endif
  }

  /*SaveInfo2LSS(rs,ba, 3);*/

  if (ba->transientTreeEntry != NULL)   /* Remove entry from transient */
    jrb_delete_node(ba->transientTreeEntry); /* tree if it is transient */
  temp = ba->masterTreeEntry;          /* Remove entry from master */

  free(temp->key.s);                 /* Byte Array tree */
# ifdef HAVE_PTHREAD_H 
    free((Thread_Info *)ba->thread_data);
# endif
  if ( ba->readKey_s != NULL ) { free(ba->readKey_s);}
  if ( ba->writeKey_s != NULL ) { free(ba->writeKey_s);}
  if ( ba->manageKey_s != NULL ) { free(ba->manageKey_s);}
  /*free(ba);*/
  jrb_delete_node(temp);
  SaveInfo2LSS(rs,ba,3);
  memset(ba,'*',sizeof(struct ibp_byteArray));
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
void truncatestorage( RESOURCE *rs ,int pi_type, void *pv_par) 
{

  int			lb_found = 0;
  int			li_VolStorage = 0;
  time_t		lt_Now = time(0);
  IBP_byteArray	        ls_ByteArray;
  JRB      	        ls_rb, ls_GlobalTree = NULL;
  Jval                  lu_val;

  assert ( rs != NULL);

  switch (pi_type) {
  case 1:  /* volatile */
    li_VolStorage = *((int *) pv_par);
    ls_GlobalTree = rs->VolatileTree;
    break;
  case 2: /* transient */
    lt_Now = *((time_t *) pv_par);
    ls_GlobalTree = rs->TransientTree;
    ls_rb = NULL;
    break;
  } /* end switch */

  while (lb_found != 1) {
    lb_found = 1;
    pthread_mutex_lock(&(rs->change_lock));
    jrb_traverse(ls_rb, ls_GlobalTree) {
      if ((pi_type == 1) && (rs->VolStorageUsed <= li_VolStorage)) {
        pthread_mutex_unlock(&(rs->change_lock));
	      return;
      }
      lu_val = jrb_val (ls_rb);
      ls_ByteArray = (IBP_byteArray) lu_val.s;
      /*
      ls_ByteArray = (IBP_byteArray) jrb_val (ls_rb);
      */
      if ((pi_type == 1) || (ls_ByteArray->attrib.duration < lt_Now)) {
        /*
	      if (ls_ByteArray->deleted == 1){
	        continue;
        } else
        */
        if ((ls_ByteArray->writeLock == 0) && 
            (ls_ByteArray->readLock == 0) &&
            (ls_ByteArray->reference == 0 )&&
		        dl_empty(ls_ByteArray->pending)) {
          	ls_ByteArray->deleted = 1;
	        jettisonByteArrayEx(rs,ls_ByteArray);
	        lb_found = 0;
	        break;
	      } else {
	        ls_ByteArray->deleted = 1;
          /*
	        ls_req = (IBP_request) calloc (1, sizeof (struct ibp_request));
	        ls_req->request = IBP_DECR;
	        ls_req->fd = -1;
	        dl_insert_b(ls_ByteArray->pending, ls_req);
          */
	      } /* end if */
      }
      else{
        pthread_mutex_unlock(&(rs->change_lock));
        return;
      }
    } /* end rb_traverse */
    pthread_mutex_unlock(&(rs->change_lock));
  } /* end while */
}


/*
 * validIPAddres check whether the connecting socket is allowed to allocate resource
 */

/*****************************/
/* validIPAddress            */
/* It check whether the connecting socket is allowed to allocate resource */
/* return: valid / invalid   */
/*****************************/
#if 0
int  validIPAddress (int pi_fd ){
    return 1;
}
#endif
int  validIPAddress ( RESOURCE *rs , int pi_fd ){
  char	*data;
  struct sockaddr *sa ;
  int 	len = IBP_K_MAXSOCKADDR;
  struct sockaddr_in *sa_in;
  IPv4_addr_t *tmp;
#ifdef AF_INET6
  int i;
  int matched;
  struct sockaddr_in6 *sa_in6;
  IPv6_addr_t *tmpv6;
#endif	
  int   li_ret = 0;

  assert ( rs != NULL); 
   
  data = malloc (IBP_K_MAXSOCKADDR);
  if ( getpeername(pi_fd,(struct sockaddr*) data,&len) < 0)
    return (0);
  sa = (struct sockaddr *)data;

  switch ( sa->sa_family ){
  case AF_INET:
    li_ret = 0;
    /*
     * check global access list 
     */ 
    sa_in = (struct sockaddr_in *)sa;
    if ( glb.IPv4 != NULL) {
        tmp = glb.IPv4;
        while ( tmp != NULL ){
            if ( (sa_in->sin_addr.s_addr & tmp->mask) == (tmp->addr & tmp->mask) ) {
                li_ret = 1;
	            break ;
            }
            tmp = tmp->next;	
        }
    }else {
        li_ret = 1;
    }
    if ( li_ret == 0 ) { break; }

    /*
     * check resource specified access list 
     */ 
    li_ret = 0;
    if ( rs->IPv4 != NULL ){
        tmp = rs->IPv4;
        while ( tmp != NULL ){
            if ( (sa_in->sin_addr.s_addr & tmp->mask) == (tmp->addr & tmp->mask) ) {
	            li_ret = 1;
	            break;
            }
            tmp = tmp->next;
        }
    }else {
        li_ret = 1;
    }
    break;

#ifdef AF_INET6
  case AF_INET6:
    sa_in6 = (struct sockaddr_in6 *)sa;
    li_ret = 0;

    /*
     * check global IPv6 access list 
     */ 
    if ( glb.IPv6 == NULL ) {
      li_ret = 1;
    }else {
        tmpv6 = glb.IPv6;
        while ( tmpv6 != NULL ){
            matched = 1;
            for(i=0;i<16;i++){
	            if ( (sa_in6->sin6_addr.s6_addr[i] && tmpv6->mask[i]) != (tmpv6->addr[i] && tmpv6->mask[i])){
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
    }
    if ( li_ret == 0 ) { break; }

    /*
     * check resource specified IPv6 access list
     */ 
    li_ret = 0;
    if ( rs->IPv6 == NULL ){
        li_ret = 1;
    }else {
        tmpv6 = rs->IPv6;
        while ( tmpv6 != NULL ){
            matched = 1;
            for(i=0;i<16;i++){
	            if ( (sa_in6->sin6_addr.s6_addr[i] && tmpv6->mask[i]) != (tmpv6->addr[i] && tmpv6->mask[i])){
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
    } 
    break;

#endif
  default:
    li_ret = 0;
    break;						
  }	
  free (data);
  return li_ret;
}

#if 0
int check_allocate( int pi_fd, int pi_nbt, int size ) {
    return 0;
}
#endif

/*****************************/
/* check_allocate            */
/* It check whether the allocation is permitted by allcation policy  */
/* return: valid / invalid   */
/*****************************/
int check_allocate( RESOURCE *rs , int pi_fd, int pi_nbt, int size)
{
  int li_duration;

  assert ( rs != NULL);

  switch (glb.AllocatePolicy) {
    case IBP_ALLPOL_SID:
      break;

    case IBP_ALLPOL_EXP:
      li_duration = ibp_cmd[pi_nbt].lifetime / 3600;
      if (li_duration < 1) {
        li_duration = 1;
      }
      if (ibp_cmd[pi_nbt].reliable == IBP_STABLE) {
        if (rs->StableStorage <= li_duration * size) {
          handle_error(IBP_E_WOULD_EXCEED_POLICY, &pi_fd);
          return(-1);
        }
      } 
      else {
        if (li_duration * size >= rs->VolStorage) {
          handle_error(IBP_E_WOULD_EXCEED_POLICY, &pi_fd);
          return(-1);
        }
      }
      break;
  }
  return 0;
}

#if 0
void handle_allocate( int pi_fd, int pi_nbt, ushort_t pf_AllocateType) {
    return;
}
#endif

/*****************************/
/* handle_allocate           */
/* It handles the user's allocation requirement, envoked by handle_require()  */
/* It firstly checks whether there is enough space. Then it builds the node for the requirement and sends the info back to user   */
/* return: none              */
/*****************************/
void handle_allocate ( RESOURCE *rs , int pi_fd, int pi_nbt, ushort_t pf_AllocationType)
{

  time_t                  lt_CurTime;
  char                    lc_FName[PATH_MAX];
  char                    *lc_Key = NULL;
  struct ibp_attributes   ls_NewAttr;
  FILE                    *lf_fp=NULL;
  char                     *li_ReadKey, *li_WriteKey, *li_ManageKey;
  char		          *lc_ReadCap = NULL;
  char                    *lc_WriteCap = NULL;
  char                    *lc_ManageCap = NULL;
  IBP_byteArray           ls_container = NULL;
  char                    *lc_generic;
  char                    lc_BigBuf[MAX_BUFFER];
  int                     li_HeaderLen;
  char                    lc_LineBuf[CMD_BUF_SIZE];
  char                    *lc_r,*lc_tmp,lc_ch;
  int                     li_tmp;
  int                     *li_sec;
  int	                  li_index,blksize;
  int                     li_ret, li_nextstep;
  int                     *n_key,n;
  char                    dirname[17];
  ulong_t                 ll_size = 0,ll_tmp;
  Jval                    lu_val;
  ushort_t                lf_ThreadLeavingMessage = 0;
  int                     secLen;
#ifdef HAVE_PTHREAD_H
  time_t                  left_time;
  Thread_Info             *thread_info;
  struct timespec         spec;
#endif

  /*
   * 0- Initialize
   */
  assert (rs != NULL );

#ifdef IBP_DEBUG
  fprintf (stderr, "T %d, handle allocate start\n", pi_nbt);
  if (pf_AllocationType != IBP_HALL_TYPE_PING) {
    exec_info[pi_nbt][0]='\0';
    strcat(exec_info[pi_nbt],"   enter handle_allocate() \n");
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

  secLen = MAX_KEY_NAME_LEN / MAX_DIM_LEN;
#if 0
  switch ( ibp_cmd[pi_nbt].ver ){
    secLen = MAX_KEY_NAME_LEN / MAX_DIM_LEN;
    case IBPv031:
      secLen = MAX_KEY_NAME_LEN_OLD / MAX_DIM_LEN;
      break; 
    case IBPv040:
      secLen = MAX_KEY_NAME_LEN / MAX_DIM_LEN ;
      break;
    default:
      assert(0);
      break;
  }
#endif

  if ( (li_sec=(int *)calloc(secLen,sizeof(int))) == NULL ){
    handle_error(E_ALLOC,&pi_fd);
    IBPErrorNb[pi_nbt] = E_ALLOC;
    li_nextstep = IBP_HALL_DONE;
  }

  while (li_nextstep != IBP_HALL_DONE) {
    switch (li_nextstep) {

    case IBP_HALL_PARAM_S:
#ifdef IBP_DEBUG
      if (pf_AllocationType != IBP_HALL_TYPE_PING) {
        strcat(exec_info[pi_nbt],"     check parameter\n");
      }
#endif

      /*
       * check ip address
       */

      if ( !validIPAddress(rs,pi_fd) ){
        handle_error(IBP_E_INV_IP_ADDR,&pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_INV_IP_ADDR;
        li_nextstep = IBP_HALL_DONE;
        continue;
      }
 
      /*
       * Check if parameters are OK
       */
    
      if (ibp_cmd[pi_nbt].size == 0) {
	      handle_error(IBP_E_INV_PAR_SIZE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_INV_PAR_SIZE;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      if ((rs->MaxDuration == 0) ||
	        ((rs->MaxDuration > 0) && 
	        ((ibp_cmd[pi_nbt].lifetime <= 0) ||
	        (rs->MaxDuration < ibp_cmd[pi_nbt].lifetime) ))) {
	      handle_error(IBP_E_LONG_DURATION, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_LONG_DURATION;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      strcat(exec_info[pi_nbt],"  1111   check parameter\n");
      if (check_allocate( rs, pi_fd, pi_nbt, ibp_cmd[pi_nbt].size) != 0) {
        IBPErrorNb[pi_nbt] = IBP_E_GENERIC;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      ls_NewAttr.duration = (ibp_cmd[pi_nbt].lifetime > 0 ? lt_CurTime + ibp_cmd[pi_nbt].lifetime : 0);
      ls_NewAttr.reliability = ibp_cmd[pi_nbt].reliable;
      ls_NewAttr.type = ibp_cmd[pi_nbt].type;

      strcat(exec_info[pi_nbt],"  2222   check parameter\n");
      if (check_size(rs,ibp_cmd[pi_nbt].size, ls_NewAttr.reliability) != 0) {
#ifdef HAVE_PTHREAD_H      
	/* server sync for handle_allocate */

        left_time = ibp_cmd[pi_nbt].ServerSync;
        if (thread_glb.StorageNeeded != 0) {
	        thread_glb.Waiting++;
	        spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
	        spec.tv_nsec = 0;
          if (pthread_cond_timedwait (&(thread_glb.Protect), &alloc_lock, &spec) != 0) {
	          thread_glb.Waiting--;
	          handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
            IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
	          li_nextstep = IBP_HALL_DONE;
	          continue;
	        } else {
	          thread_glb.Waiting--;
	          left_time -= (time(NULL) - lt_CurTime);
	        }
	      } 
	  
        if (check_size(rs,ibp_cmd[pi_nbt].size, ls_NewAttr.reliability) != 0) {
	        thread_glb.StorageNeeded = ibp_cmd[pi_nbt].size;
	        spec.tv_sec = time(NULL) + left_time;
	        spec.tv_nsec = 0;
	        if (pthread_cond_timedwait (&(thread_glb.Cond), &alloc_lock, &spec) != 0) 
          {
            if (check_size(rs,ibp_cmd[pi_nbt].size, ls_NewAttr.reliability) != 0) {
              handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
              IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
              li_nextstep = IBP_HALL_DONE;
            }
	          thread_glb.StorageNeeded = 0;
	          if (thread_glb.Waiting > 0) {
	            pthread_cond_signal(&(thread_glb.Protect));
	          }
	        }else {
	          thread_glb.StorageNeeded = 0;
	          if (thread_glb.Waiting > 0) {
	            pthread_cond_signal(&(thread_glb.Protect));
	          }
	        }
	      }
# else
	      handle_error(IBP_E_WOULD_EXCEED_LIMIT, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
#endif

      }

      strcat(exec_info[pi_nbt],"  3333   check parameter\n");
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
        strcat(exec_info[pi_nbt],"     prepare the file names and keys\n");
      }
#endif
      
      lc_r = malloc (MAX_KEY_LEN);
      lc_Key = malloc (MAX_KEY_LEN);
      if ((lc_r == NULL) ||
          (lc_Key == NULL)) {
	      handle_error (IBP_E_ALLOC_FAILED, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_ALLOC_FAILED;
	      if (lc_r != NULL)
	        free (lc_r);
	      lf_ThreadLeavingMessage = 1;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      sprintf(lc_Key, "IBP-");

      /* 
       * seeding the random numbers ...
       */

      generateKey(li_sec,secLen,pi_nbt);
#if 0
      ll_randseed = random() + pi_nbt + lt_CurTime;
      srandom (ll_randseed);
      ll_randseed = random();
#endif 
      lc_tmp = (char*)li_sec;
      li_tmp = strlen(lc_Key);
      for (li_index = 0; li_index < 90 ; li_index++) { 
          lc_ch = lc_tmp[li_index];
          lc_ch = lc_ch % 62;
          if ( lc_ch < 0 ) { lc_ch = -lc_ch ; };
          if ( lc_ch < 10 ){
            lc_ch = lc_ch + '0';
          }else if ( lc_ch < 36 ){
            lc_ch = lc_ch - 10 + 'a';
          }else{
            lc_ch = lc_ch - 36 +'A';
          }
          /*
          fprintf(stderr,"int = %d ch = %c\n",lc_ch,lc_ch);
          if ( !((lc_ch <='9' && lc_ch >= '0' ) ||
                 (lc_ch <='z' && lc_ch >='a') ||
                 (lc_ch <='Z' && lc_ch >='A'))){
            fprintf(stderr,"Haha int=%d",lc_ch);
            getchar();
          }
          */
          lc_Key[li_tmp++] = lc_ch;
	      /*
          sprintf(lc_r, "%c", lc_ch );
	      strcat(lc_Key, lc_r);
          */
      }
      lc_Key[li_tmp++]='\0';
      free (lc_r);
     
      switch ( rs->type ){
          case DISK:
            n_key = (int*)&(lc_Key[4]);
            n = (*n_key) % rs->subdirs;
            sprintf(dirname,"data%d/",n);
            strcpy(lc_FName, rs->loc.dir);
            strcat(lc_FName,dirname);
            strcat(lc_FName,lc_Key);
            break;
          case MEMORY:
            break;
          case ULFS:
            strcpy(lc_FName,lc_Key);
            break;
          default: 
            break;
      }

      li_nextstep += IBP_STEP;
      break;

#if 0
      if (ls_NewAttr.reliability == IBP_STABLE)
	      strcpy(lc_FName, glb.StableDir);
      else
	      strcpy(lc_FName, glb.VolDir);

      strcat(lc_FName, lc_Key);

      /* end long names section */
      li_nextstep += IBP_STEP;
      break;
#endif

    case IBP_HALL_NEWCONT_P:
      lc_Key = malloc (MAX_KEY_LEN);
      if (lc_Key == NULL) {
        fprintf(stderr, "IBP-0 couldn't be created. Memory allocation failure\n");
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      sprintf(lc_Key, "IBP-0");
      switch (rs->type){
          case DISK:
              strcpy(lc_FName,rs->loc.dir);
              strcat(lc_FName,lc_Key);
              break;
          case MEMORY:
              break;
          case ULFS:
              strcpy(lc_FName,lc_Key);
              break;
          default:
              break;
      }

#if 0
      strcpy(lc_FName, glb.StableDir);
      strcat(lc_FName, lc_Key);
#endif

      li_nextstep += IBP_STEP;
      break;

    case IBP_HALL_CREATE_S:
      if (ls_NewAttr.type == IBP_FIFO || ls_NewAttr.type == IBP_CIRQ) {
      }

      switch ( rs->type ){
          case DISK:
            if ((lf_fp = fopen(lc_FName,"w")) == NULL) {
	            unlink(lc_FName);
	            handle_error(IBP_E_FILE_ACCESS, &pi_fd);
              IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
	            lf_ThreadLeavingMessage = 1;
	            li_nextstep = IBP_HALL_DONE;
	            continue;
            }
            break;
          case MEMORY:
            break;
          case ULFS:
            blksize = ((FAT*)(rs->loc.ulfs.ptr))->blksize;
            ll_tmp = ibp_cmd[pi_nbt].size + (HEADER_LEN/blksize+1)*blksize;
            if ( ibpfs_allocate((FAT*)(rs->loc.ulfs.ptr),lc_FName,ll_tmp) != 0 ){
                handle_error(IBP_E_FILE_ACCESS,&pi_fd);
                IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
                lf_ThreadLeavingMessage = 1;
                li_nextstep = IBP_HALL_DONE;
                continue;
            }
            if ((lf_fp = (void*)ibpfs_open((FAT*)(rs->loc.ulfs.ptr),lc_FName)) == NULL ){
                ibpfs_delete((FAT*)(rs->loc.ulfs.ptr),lc_FName);
                handle_error(IBP_E_FILE_ACCESS,&pi_fd);
                IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
                lf_ThreadLeavingMessage = 1;
                li_nextstep = IBP_HALL_DONE;
                continue;
            }
            ibpfs_writeFAT((FAT*)(rs->loc.ulfs.ptr),((FAT*)(rs->loc.ulfs.ptr))->device);
            break;
          default:
            break;
      }

      lc_ReadCap = createcap (rs,lc_Key, "READ",&(li_ReadKey),ibp_cmd[pi_nbt].ver);
      lc_WriteCap = createcap (rs,lc_Key, "WRITE",&(li_WriteKey),ibp_cmd[pi_nbt].ver);
      lc_ManageCap = createcap (rs,lc_Key, "MANAGE",&(li_ManageKey),ibp_cmd[pi_nbt].ver);

      if ((lc_ReadCap == NULL) || (lc_WriteCap == NULL) || (lc_ManageCap == NULL)) {
	      handle_error (IBP_E_ALLOC_FAILED, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_ALLOC_FAILED;
	      lf_ThreadLeavingMessage = 1;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }
      ll_size = ibp_cmd[pi_nbt].size;
      li_nextstep += IBP_STEP;
      break;

    case IBP_HALL_CREATE_P:
      switch ( rs->type){
          case DISK:
            if ((lf_fp = fopen(lc_FName,"w")) == NULL) {
	            unlink(lc_FName);
              fprintf(stderr, "IBP-0 couldn't be created. File opening failure\n");
	            li_nextstep = IBP_HALL_DONE;
	            continue;
            }
            break;
          case MEMORY:
            break;
          default:
            break;
      }

      lc_ReadCap = (char*) calloc(MAX_CAP_LEN,sizeof(char));
      li_ReadKey = strdup("0");
      lc_WriteCap = createcap(rs,lc_Key,"WRITE",&(li_WriteKey),IBPv040);
      lc_ManageCap = createcap(rs,lc_Key,"MANAGE",&(li_ManageKey),IBPv040);
#if 0
      lc_ReadCap = malloc (PATH_MAX);
      lc_WriteCap = malloc (PATH_MAX);
      lc_ManageCap = malloc (PATH_MAX);
      sprintf(lc_ReadCap,"ibp://%s:%d/%s/%d/%s",  
	      glb.hostname, 
	      glb.IbpPort,
	      lc_Key, 
	      0, 
	      "READ");
      lc_WriteCap = malloc (PATH_MAX);
      sprintf(lc_WriteCap,"ibp://%s:%d/%s/%d/%s",  
	      glb.hostname, 
	      glb.IbpPort,
	      lc_Key, 
	      0, 
	      "WRITE");
      lc_ManageCap = malloc (PATH_MAX);
      sprintf(lc_ManageCap,"ibp://%s:%d/%s/%d/%s",  
	      glb.hostname, 
	      glb.IbpPort,
	      lc_Key, 
	      0, 
	      "MANAGE");
#endif
      if ((lc_ReadCap == NULL) || (lc_WriteCap == NULL) || (lc_ManageCap == NULL)) {
	      handle_error (IBP_E_ALLOC_FAILED, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_ALLOC_FAILED;
	      lf_ThreadLeavingMessage = 1;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      sprintf(lc_ReadCap,"ibp://%s:%d/%d#%s/%d/%s",  
	      glb.hostname, 
	      glb.IbpPort,
        rs->RID,
	      lc_Key, 
	      0, 
	      "READ");

      ll_size = 128; /* TO BE MODIFIED!!!!! */
      li_nextstep += IBP_STEP;
      
      break;


    case IBP_HALL_FILL_S:
    case IBP_HALL_FILL_P:

      /*
       * 2.2- filenames are OK, now prepare the container
       */

#ifdef IBP_DEBUG
      if (pf_AllocationType != IBP_HALL_TYPE_PING) {
        strcat(exec_info[pi_nbt],"     prepare the file container\n");
      }
#endif
      ls_container = (IBP_byteArray) calloc (1, sizeof(struct ibp_byteArray));
      
      ls_container->maxSize = ll_size;
      ls_container->currentSize = 0;
      ls_container->size = ll_size;
      ls_container->free = ll_size;
      ls_container->rdcount = 0;
      ls_container->wrcount = 0;

      memcpy(&(ls_container->attrib), &ls_NewAttr, sizeof(struct ibp_attributes));

      ls_container->fname = strdup (lc_FName);
      ls_container->key = strdup (lc_Key);
      ls_container->readKey_s = li_ReadKey;
      ls_container->writeKey_s = li_WriteKey;
      ls_container->manageKey_s = li_ManageKey;

      ls_container->readKey = atoi(li_ReadKey);
      ls_container->writeKey = atoi(li_WriteKey);
      ls_container->manageKey = atoi(li_ManageKey);

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
      thread_info = (Thread_Info *)malloc(sizeof(Thread_Info));
      if (thread_info == NULL) {
#ifdef IBP_DEBUG
	      fprintf(stderr, "T %d, thread_info couldn't be allocated, returning\n", pi_nbt);
#endif
	      handle_error (IBP_E_ALLOC_FAILED, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_ALLOC_FAILED;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }
      bzero (thread_info, sizeof(Thread_Info));
      thread_info->read = -1;

      ls_container->thread_data = (void *) thread_info;
#endif

      ls_container->pending = make_dl();
      
      lu_val.s = (char *) ls_container;
      pthread_mutex_lock(&(rs->change_lock));
      ls_container->masterTreeEntry = jrb_insert_str (rs->ByteArrayTree,
						      strdup(lc_Key),
						      lu_val);

      if (ls_NewAttr.reliability == IBP_VOLATILE) {
#if 0
#ifdef HAVE_PTHREAD_H
	      pthread_mutex_lock(&change_lock);
#endif
	      rs->VolStorageUsed += ll_size;
#ifdef HAVE_PTHREAD_H
	      pthread_mutex_unlock(&change_lock);
#endif
#endif
        rs->VolStorageUsed += ll_size;
        ls_container->volTreeEntry = vol_cap_insert(rs, ll_size, 
                                                    ibp_cmd[pi_nbt].lifetime,
                                                    lu_val);
      }
      else if (ls_NewAttr.reliability == IBP_STABLE) {
#if 0
#ifdef HAVE_PTHREAD_H
	      pthread_mutex_lock(&change_lock);
#endif
	      glb.StableStorageUsed += ll_size;
#ifdef HAVE_PTHREAD_H
	      pthread_mutex_unlock(&change_lock);
#endif
#endif
        rs->StableStorageUsed += ll_size;
      }
      if (ls_NewAttr.duration > 0){
	      ls_container->transientTreeEntry = jrb_insert_int (rs->TransientTree, 
							   ls_NewAttr.duration,
							   lu_val);
      }
      pthread_mutex_unlock(&(rs->change_lock));

      lc_generic = lc_BigBuf + sizeof(int); /* Reserve for total header size */
      memcpy(lc_generic, ls_container, BASE_HEADER);
      lc_generic += BASE_HEADER;
      sprintf(lc_generic,"%s\n", ls_container->fname);
      lc_generic += strlen(lc_generic);
      li_HeaderLen = lc_generic - lc_BigBuf;
      memcpy(lc_BigBuf, &li_HeaderLen, sizeof(int));
    
      switch ( rs->type ){
        case DISK:
          if (fwrite(lc_BigBuf, 1, li_HeaderLen, lf_fp) == 0){
	          jettisonByteArray(rs,ls_container);
	          unlink(lc_FName);
	          handle_error(IBP_E_FILE_ACCESS, &pi_fd);
            IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
	          fclose(lf_fp);
	          lf_ThreadLeavingMessage = 1;
	          li_nextstep = IBP_HALL_DONE;
	          break; 
          }

          fclose(lf_fp);
          li_nextstep += IBP_STEP;
          break;
        case ULFS:
          if (ibpfs_store((IBPFILE*)lf_fp,lc_BigBuf,li_HeaderLen,0) < 0 ){
              ibpfs_close((IBPFILE*)lf_fp);
              jettisonByteArray(rs,ls_container);
              ibpfs_delete((FAT*)(rs->loc.ulfs.ptr),lc_FName);
              lf_ThreadLeavingMessage = 1;
              handle_error(IBP_E_FILE_ACCESS,&pi_fd);
              IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
              li_nextstep = IBP_HALL_DONE;
              break;
          }

          ibpfs_close((IBPFILE*)lf_fp);
          li_nextstep += IBP_STEP;
          break;
        case MEMORY:
          if ( ( ls_container->startArea = calloc(ls_container->size,sizeof(char))) == NULL ){
              jettisonByteArray(rs,ls_container);
              handle_error(E_ALLOC,NULL);
              lf_ThreadLeavingMessage = 1;
	            li_nextstep = IBP_HALL_DONE;
              break;
          }
          /*fprintf(stderr," IMPDE startArea = %p  ##########\n",ls_container->startArea);*/
          memset(ls_container->startArea,'Y',ls_container->size);
          li_nextstep += IBP_STEP;
          break;
        default:
	        li_nextstep = IBP_HALL_DONE;
          break;
      }
      break;

    case IBP_HALL_CLOSE_S:
      /*
       * 3- Confirm the client, clean up and leave
       */
#ifdef IBP_DEBUG
      if (pf_AllocationType != IBP_HALL_TYPE_PING) {
        strcat(exec_info[pi_nbt],"     Confirm the client, clean up and leave\n");
      }
#endif

      sprintf (lc_LineBuf, "%d %s %s %s \n", 
	       IBP_OK,
	       lc_ReadCap, 
	       lc_WriteCap, 
	       lc_ManageCap);
#ifdef IBP_DEBUG
      fprintf(stderr, "T %d, allocate, line sent to client: %s\n", pi_nbt, lc_LineBuf);
#endif

      li_ret = Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
      if (li_ret != strlen(lc_LineBuf)) {
#ifdef IBP_DEBUG
	      fprintf(stderr, "T %d, Answering to client failed\n", pi_nbt);
#endif
	      handle_error (IBP_E_SOCK_WRITE, &pi_fd);
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
	      li_nextstep = IBP_HALL_DONE;
	      continue;
      }

      pthread_mutex_lock(&(rs->change_lock));
      ls_container->BackupIndex = rs->NextIndexNumber;
      rs->NextIndexNumber++;
      pthread_mutex_unlock(&(rs->change_lock));
      if (pf_AllocationType == IBP_HALL_TYPE_STD) {
        if (SaveInfo2LSS(rs,ls_container, 1) == -1) {
          ls_container->BackupIndex = -1;
        }
      }

      truncatestorage ( rs,2, (void *) &lt_CurTime);

#if 0
      /*
       * Update the log file
       */
      if ( rs->type == DISK || rs->type == ULFS ){
        if ((rs->logFd == NULL) ){
	        rs->logFd = fopen (rs->logFile, "a");
        }
        if ((rs->logFd == NULL)){
	        handle_error (E_OFILE, (void *) rs->logFd);
        }
        else {
	        ls_now = ctime (&lt_CurTime);
	        ls_now[24] = '\0';
	        fprintf (rs->logFd, "%s # 1 #  %s\n", ls_now, ls_container->fname);
	        fflush (gf_logfd);
#ifdef IBP_DEBUG    
	        fprintf (stderr, "%s # 1 # %s\n", ls_now, ls_container->fname);
#endif
        }
      }
#endif

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
    if ((ls_NewAttr.reliability == IBP_STABLE) && (thread_glb.Waiting > 0)) 
      pthread_cond_signal(&(thread_glb.Protect));
    if ((ls_NewAttr.reliability == IBP_VOLATILE) && (thread_glb.Waiting>0))
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
  if ( li_sec != NULL ) {
    free(li_sec);
  }
#ifdef IBP_DEBUG
  if (pf_AllocationType != IBP_HALL_TYPE_PING) {
    strcat(exec_info[pi_nbt],"     handle_allocate finished\n");
    fprintf(stderr," handle_alloc; %s\n",exec_info[pi_nbt]);
  }
#endif

  return;
}

#if 0
void handle_store_end (int pi_fd, int pi_nbt, IBP_byteArray ps_container){
  assert(0);
  return;
}
#endif
void handle_store_end ( RESOURCE *rs , int pi_fd, int pi_nbt, IBP_byteArray ps_container)
{
  char lc_LineBuf[CMD_BUF_SIZE];

  assert(rs != NULL);

  if (IBPErrorNb[pi_nbt] != IBP_OK) {
    sprintf(lc_LineBuf,"%d \n", IBPErrorNb[pi_nbt]);
    Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
  }
  ps_container->writeLock--;

#if 0
  /* check whether this capability has been deleted */
  if (ps_container->deleted == 1)
  {
    if ((ps_container->writeLock == 0) && (ps_container->readLock == 0))
    {
      SaveInfo2LSS(rs,ps_container, 2);
      jettisonByteArray(rs,ps_container);
    }
  }
#endif 

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_store finished\n");
  fprintf(stderr," T  %d, handle_store %s\n",pi_nbt,exec_info[pi_nbt]);
  fprintf(stderr, "T %d, handle_store, done\n",pi_nbt);
#endif
  return;
}

int storeToDisk( RESOURCE       *rs,
                 IBP_byteArray  ps_container,
                 int            pi_nbt,
                 int            pi_fd,
                 int            pi_srcfd,
                 ulong_t        ll_writeptr, 
                 unsigned long  ll_size,
                 unsigned long  ll_free,
                 unsigned long  ll_wrcount,
                 unsigned long  ll_rdcount,
                 unsigned long  *ll_written,
                 time_t         lt_expireTime,
                 char           *lc_LineBuf){

  FILE    *lf_f;
  int     li_HeaderLen;
  int     li_nextstep = IBP_OK;
  int     li_Ret;

#ifdef HAVE_PTHREAD_H
  Thread_Info           *info;
#endif

  lf_f = fopen(ps_container->fname, "r+");
  if (lf_f == NULL) {
#if 0
    IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
    handle_error(IBPErrorNb[pi_nbt], &pi_fd);
    handle_store_end_rs(rs,pi_fd, pi_nbt, ps_container);
#endif
    return (IBP_E_FILE_ACCESS) ;
  }

  if (fread(&li_HeaderLen, sizeof(int), 1, lf_f) == 0) {
#if 0
    IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
    handle_error(IBPErrorNb[pi_nbt], &pi_fd);
    handle_store_end_rs(rs,pi_fd, pi_nbt, ps_container);
#endif
    fclose(lf_f);
    return(IBP_E_FILE_ACCESS);
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
    if (fseek(lf_f, li_HeaderLen + ibp_cmd[pi_nbt].Offset, SEEK_SET) == -1) {  
#if 0
      IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
      handle_error(IBPErrorNb[pi_nbt], &pi_fd);
      fclose(lf_f);
#endif
      li_nextstep = DONE;
      IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
      break;
    }
    break;

  case IBP_BUFFER:
    if (fseek(lf_f, li_HeaderLen, SEEK_SET) == -1) {
#if 0
      IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
      handle_error(IBPErrorNb[pi_nbt], &pi_fd);
      fclose(lf_f);
#endif
      IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
      li_nextstep = DONE;
      break;
    }
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    if (fseek(lf_f, ll_writeptr + li_HeaderLen, SEEK_SET) == -1) {
#if 0
      IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
      handle_error(IBPErrorNb[pi_nbt], &pi_fd);
      fclose(lf_f);
#endif
      IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
      li_nextstep = DONE;
      break;
    }
    break;

  default:
    break;
  }


  if (li_nextstep == DONE ) {
#if 0
    handle_store_end(pi_fd, pi_nbt, ps_container);
#endif
    return(IBPErrorNb[pi_nbt]);
  }

  sprintf(lc_LineBuf, "%d \n", IBP_OK);
  if (Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf)) == -1) {
#if 0
    IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
    handle_error(IBPErrorNb[pi_nbt], &pi_fd);
    fclose(lf_f);
    handle_store_end(pi_fd, pi_nbt, ps_container);
#endif
    return (IBP_E_SOCK_WRITE);
  }


  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    li_Ret = Socket2File(pi_srcfd, lf_f, &ibp_cmd[pi_nbt].size, lt_expireTime);
    if (li_Ret != IBP_OK) {
      ftruncate(fileno(lf_f), ps_container->currentSize + li_HeaderLen);
      fclose(lf_f);
#if 0
      IBPErrorNb[pi_nbt] = li_Ret;
      handle_error(IBPErrorNb[pi_nbt], &pi_fd);
#endif
      IBPErrorNb[pi_nbt] = li_Ret;
      li_nextstep = DONE;
      break;
    }
    *ll_written = ibp_cmd[pi_nbt].size;
    break;

  case IBP_FIFO:
  case IBP_CIRQ:
    li_Ret = Socket2Queue (pi_srcfd,
                           lf_f,
                           ibp_cmd[pi_nbt].size,
                           ps_container->maxSize,
                           ll_writeptr,
                           ll_written,
                           li_HeaderLen,
                           lt_expireTime);
    if (li_Ret != IBP_OK) {
      IBPErrorNb[pi_nbt] = li_Ret;
      /*handle_error (IBPErrorNb[pi_nbt], &pi_fd);*/
      li_nextstep = DONE;
      break;
    }
    ps_container->size = ll_size;
    ps_container->wrcount = ll_wrcount;
    ps_container->rdcount = ll_rdcount;
    ps_container->free = ll_free;

    break;

  default:
    break;
  }

  if (li_nextstep == DONE ) {
    /*handle_store_end(pi_fd, pi_nbt, ps_container);*/
    return ( IBPErrorNb[pi_nbt]) ;
  }

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_store data transfered\n");
#endif

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
    if ( ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size > ps_container->currentSize ){
      ps_container->currentSize = ibp_cmd[pi_nbt].Offset + ibp_cmd[pi_nbt].size;
    }
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

  fseek(lf_f, sizeof(int), SEEK_SET);
  fwrite(ps_container, 1, BASE_HEADER, lf_f);
  fflush(lf_f);
  fclose(lf_f);

  return (IBP_OK);

}



/*****************************/
/* handle_store              */
/* It handles the user's store requirement, envoked by handle_require() */
/* It stores the user's data to a temporary file on the disk  */
/* return: none              */
/*****************************/
#if 0
void handle_store(int pi_fd, int pi_srcfd, int pi_nbt, IBP_byteArray ps_container){
  assert(0);
  return;
}
#endif
void handle_store(RESOURCE *rs ,int pi_fd, int pi_srcfd, int pi_nbt, IBP_byteArray ps_container)
{
  char                  lc_LineBuf[CMD_BUF_SIZE];
  int                   li_HeaderLen = 0, li_nextstep = 1;
  unsigned long         ll_size=0,ll_free=0,ll_wrcount=0,ll_rdcount=0,ll_written = 0;
  ulong_t               ll_writeptr = 0;
  time_t                lt_expireTime;  /* Server Timeout */

#ifdef HAVE_PTHREAD_H
  Thread_Info           *info;
  struct timespec       spec;
#endif


  assert ( rs != NULL );
  assert ( rs->type != RMT_DEPOT);

#ifdef IBP_DEBUG
  fprintf(stderr, "T %d, handle_store, beginning\n",pi_nbt);
  strcat(exec_info[pi_nbt],"  handle_store beginning\n");
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
    if (ibp_cmd[pi_nbt].size > ps_container->maxSize - ibp_cmd[pi_nbt].Offset) {
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
    ll_free    = ps_container->free;
    ll_size    = ps_container->size;
    ll_writeptr = ll_wrcount;

# ifdef IBP_DEBUG
    fprintf (stderr, "T %d, Lock file :%lu::%lu:%lu:%lu:\n", pi_nbt, ll_wrcount, ll_rdcount, ll_free, ll_size);
# endif
    break;

  default:
    li_nextstep = DONE;
    break;
  }

  if (li_nextstep == DONE ) {
    handle_store_end(rs,pi_fd, pi_nbt, ps_container);
    return;
  }

  switch (ps_container->attrib.type) {
  case IBP_BYTEARRAY:
  case IBP_BUFFER:
    break;

  case IBP_FIFO:
    if (ll_free < ibp_cmd[pi_nbt].size) {
# ifdef HAVE_PTHREAD_H
      info = (Thread_Info *)ps_container->thread_data;
      info->write = ibp_cmd[pi_nbt].size;
      spec.tv_sec = time(NULL) + ibp_cmd[pi_nbt].ServerSync;
      spec.tv_nsec = 0;
      pthread_mutex_lock (&filock);
      if (pthread_cond_timedwait(&(info->write_cond), &filock, &spec) != 0) {
        info->write = 0;
        IBPErrorNb[pi_nbt] = IBP_E_WOULD_EXCEED_LIMIT;
        handle_error(IBPErrorNb[pi_nbt], &pi_fd);
        /*          fclose (lf_FLock); */
        pthread_mutex_unlock (&filock);
        li_nextstep = DONE;
        break;
      }
      else {
        info->write = 0;
        /*          fclose (lf_FLock); */
        pthread_mutex_unlock(&filock);
        ll_wrcount = ps_container->wrcount;
        ll_rdcount = ps_container->rdcount;
        ll_free    = ps_container->free;
        ll_size    = ps_container->size;
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
  strcat(exec_info[pi_nbt],"  handle_store size determined\n");
#endif

  if (li_nextstep == DONE ) {
    handle_store_end(rs,pi_fd, pi_nbt, ps_container);
    return;
  }

  switch ( rs->type ){
    case DISK:
      IBPErrorNb[pi_nbt] = storeToDisk(rs,ps_container,pi_nbt,pi_fd,pi_srcfd,
                                       ll_writeptr,ll_size,ll_free,ll_wrcount,ll_rdcount,
                                       &ll_written,lt_expireTime,lc_LineBuf);
      if ( IBPErrorNb[pi_nbt] != IBP_OK ) {
        /* handle_error(IBPErrorNb[pi_nbt],&pi_fd); */
        handle_store_end(rs,pi_fd,pi_nbt,ps_container);
        return;
      }
      break;
    case ULFS:
      IBPErrorNb[pi_nbt] = storeToULFS(rs,ps_container,pi_nbt,pi_fd,pi_srcfd,
                                       ll_writeptr,ll_size,ll_free,ll_wrcount,ll_rdcount,
                                       &ll_written,lt_expireTime,lc_LineBuf);
      if ( IBPErrorNb[pi_nbt] != IBP_OK ) {
        /* handle_error(IBPErrorNb[pi_nbt],&pi_fd); */
        handle_store_end(rs,pi_fd,pi_nbt,ps_container);
        return;
      }
      break;
    case MEMORY:
      IBPErrorNb[pi_nbt] = storeInRam(rs,ps_container,li_HeaderLen,ll_writeptr,
                                      pi_fd,pi_srcfd,lt_expireTime,pi_nbt,
                                      &(ll_written),ll_size,ll_free,
                                      ll_rdcount,ll_wrcount);
      if ( IBPErrorNb[pi_nbt] != IBP_OK ){
        /* handle_error(IBPErrorNb[pi_nbt],&pi_fd); */
        handle_store_end(rs,pi_fd,pi_nbt,ps_container);
        return;
      }
      break;
    case RMT_DEPOT:
    default: 
      assert(0);
      break;
  }
      

  sprintf(lc_LineBuf,"%d %lu\n", IBP_OK, ll_written);
  Write (pi_fd, lc_LineBuf, strlen(lc_LineBuf));
 

  SaveInfo2LSS(rs,ps_container, 2);
  handle_store_end(rs,pi_fd, pi_nbt, ps_container);

  return;
}

/*****************************/
/* handle_status             */
/* It handles the user's status requirement, envoked by handle_require() */
/* There are two commands: INQ  and CHNG  */
/* return: none              */
/*****************************/
void handle_status (RESOURCE *rs , int pi_fd, int pi_nbt)
{

#define ST_BUF_LEN 256 
  
  char		        *lc_buffer;
  unsigned long   ll_NewStable, ll_NewVol;
  long            ll_NewDuration;
  ibp_ulong	      tmp_NewStable, tmp_NewVol;
  char		        tmpStb[50],tmpStbUsed[50],tmpVol[50],tmpVolUsed[50];
  ibp_ulong       lu_softsize;
#ifdef HAVE_STATVFS
#ifdef HAVE_STRUCT_STATVFS64_F_BSIZE
  struct statvfs64 ls_buf;
#else
  struct statvfs  ls_buf;
#endif
#elif HAVE_STATFS
  struct statfs  ls_buf;
#endif
  JRB             node; 
  RESOURCE        *tmpRs;
  Dlist           bufList,d_node;
  int             bufLen,type;
  char            *buf;
  ibp_long        FreeSz;
  ibp_ulong       HardConfigured,HardServed,HardUsed,HardAllocable,
                  TotalConfigured,TotalServed,TotalUsed,
                  SoftAllocable;
  JRB             j_node;
  bufList = make_dl();
  bufLen = 0;


  /*fprintf(stderr," IMPDE I am here \n");*/
#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_status beginning\n");
#endif
 
  lc_buffer = malloc (CMD_BUF_SIZE);
  if (lc_buffer == NULL) {
    handle_error (E_ALLOC, NULL);
    return;
  }
 
  if (ibp_cmd[pi_nbt].ManageCmd == IBP_ST_CHANGE) {
    assert(rs!=NULL);
    if (strcmp (ibp_cmd[pi_nbt].passwd, gc_passwd) != 0) {
      sprintf(lc_buffer, "%d \n", IBP_E_WRONG_PASSWD);
      if (Write (pi_fd, lc_buffer, strlen(lc_buffer)) == -1) {
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
        free (lc_buffer);
        return;
      }
    } else {
      sprintf(lc_buffer, "%d \n", IBP_OK);
      if (Write (pi_fd, lc_buffer, strlen(lc_buffer)) == -1) {
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
        close (pi_fd);
        free (lc_buffer);
        return;
      }
    if (ReadLine(pi_fd, lc_buffer, CMD_BUF_SIZE) == -1) {
      IBPErrorNb[pi_nbt] = IBP_E_SOCK_READ;
      free (lc_buffer);
      return;
    }

    /*fprintf(stderr, " IMPDE lc_buffer ( received ) = %s \n",lc_buffer);*/
    
    sscanf (lc_buffer, "%lu %lu %ld", 
                        &ll_NewStable, 
                        &ll_NewVol, 
                        &ll_NewDuration);
# ifdef HAVE_PTHREAD_H
    pthread_mutex_lock (&(rs->change_lock));
# endif
    tmp_NewStable = ll_NewStable;
    tmp_NewStable = tmp_NewStable << 20;
    tmp_NewVol = ll_NewVol;
    tmp_NewVol = tmp_NewVol << 20;
    if (tmp_NewStable < rs->StableStorageUsed){
      IBPErrorNb[pi_nbt] = IBP_E_WOULD_DAMAGE_DATA;
      sprintf(lc_buffer, "%d \n",IBPErrorNb[pi_nbt] );
      if ( Write(pi_fd,lc_buffer,strlen(lc_buffer)) == -1 ){
        free(lc_buffer);
        return;
      }
    }else{
      rs->StableStorage = tmp_NewStable;
    }
    if (tmp_NewVol < rs->VolStorageUsed){
      IBPErrorNb[pi_nbt] = IBP_E_WOULD_DAMAGE_DATA;
      sprintf(lc_buffer,"%d \n",IBPErrorNb[pi_nbt]);
      if ( Write(pi_fd,lc_buffer,strlen(lc_buffer)) == -1 ){
        free(lc_buffer);
        return;
      }
    } else {
      rs->VolStorage = tmp_NewVol;
    }

    /*
    if (ll_NewDuration > 0){
      ll_NewDuration *= (3600*24);
    }
    */
# ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock (&(rs->change_lock));
# endif

      rs->MaxDuration = ll_NewDuration;
    }


  }


  switch( ibp_cmd[pi_nbt].ver ){
    case IBPv040:
      if ( (buf = (char*)calloc(ST_BUF_LEN,sizeof(char))) == NULL ){
        handle_error(IBP_E_ALLOC_FAILED,&pi_fd);
        IBPErrorNb[pi_nbt] = E_ALLOC; 
        goto bail;
      }
      dl_insert_b(bufList,(void *)buf);
      sprintf(buf,"%s:%s:%s %s",ST_VERSION,glb.versionMajor,glb.versionMinor,ST_DATAMOVERTYPE);
      dl_traverse(d_node,glb.dataMovers){
        sprintf(buf,"%s:%d",buf,(int)dl_val(d_node));
      }
      if ( ! jrb_empty(glb.nfu_ops) ){
        sprintf(buf,"%s %s",buf,ST_NFU_OP);
        jrb_traverse(j_node,glb.nfu_ops){
            sprintf(buf,"%s:%d",buf,(int)(j_node->key.v));
        } 
      }
      sprintf(buf,"%s ",buf);
      bufLen += strlen(buf);
      break;
    default:
      break;
  }

  jrb_traverse(node,glb.resources ){
    if ( rs == NULL ){
      tmpRs = (RESOURCE *)((jrb_val(node)).v);
    }else { 
      tmpRs = rs ;
    }

#ifndef SUPPORT_RMT_DEPOT
    if ( tmpRs->type == RMT_DEPOT ){
      if ( rs != NULL ){
        break;
      }else {
        continue;
      }
    }
#endif

    if ( getRsFreeSize(tmpRs,&lu_softsize) < 0 ){
       handle_error(IBP_E_INTERNAL,&pi_fd);
       IBPErrorNb[pi_nbt] = IBP_E_FILE_ACCESS;
       goto bail;
    }
   
    FreeSz = lu_softsize - tmpRs->MinFreeSize;
    if (FreeSz < 0) { FreeSz = 0; }
    TotalConfigured = tmpRs->VolStorage;
    TotalUsed = tmpRs->StableStorageUsed + tmpRs->VolStorageUsed;
    if ( tmpRs->VolStorage > FreeSz  + TotalUsed ){
        TotalServed = FreeSz + TotalUsed;
    }else{
        TotalServed = tmpRs->VolStorage;
    }
    if ( tmpRs->VolStorage > FreeSz + TotalUsed ){
        SoftAllocable = FreeSz;
    }else{
        SoftAllocable = tmpRs->VolStorage - TotalUsed;
	SoftAllocable = ( (ibp_long)SoftAllocable > 0 ? SoftAllocable : 0 );
    }

    HardConfigured = tmpRs->StableStorage;
    HardUsed = tmpRs->StableStorageUsed;
    if ( tmpRs->StableStorage > FreeSz + HardUsed ){
        HardServed = FreeSz + HardUsed;
    }else{
        HardServed = HardConfigured;
    }
    if ( tmpRs->StableStorage > FreeSz + HardUsed ){
        HardAllocable = FreeSz;
    }else{
        HardAllocable = HardConfigured - HardUsed;
	HardAllocable = ( (ibp_long)HardAllocable > 0 ? HardAllocable : 0 );
    }

#if 0
# ifdef HAVE_PTHREAD_H
    pthread_mutex_lock (&change_lock);
# endif
#endif
    switch ( ibp_cmd[pi_nbt].ver ){
      case IBPv031:
        lu_softsize += tmpRs->StableStorageUsed + tmpRs->VolStorageUsed - tmpRs->MinFreeSize;
        if (tmpRs->VolStorage < lu_softsize ){
          lu_softsize = tmpRs->VolStorage;
        }
        if ( glb.enableAlloc == 1 ){
	        sprintf (lc_buffer, "%s %s %s %s %lu \n",
	              ibp_ltostr((tmpRs->StableStorage >> 20),tmpStb+49,10),
	              ibp_ltostr((tmpRs->StableStorageUsed >> 20),tmpStbUsed+49,10),
	              ibp_ltostr((lu_softsize >> 20),tmpVol+49,10),
	              ibp_ltostr((tmpRs->VolStorageUsed >> 20),tmpVolUsed+49,10),
	              tmpRs->MaxDuration);
/*
	        sprintf (lc_buffer, "%s %s %s %s %lu \n",
	              ibp_ltostr((HardServed >> 20),tmpStb+49,10),
	              ibp_ltostr((HardUsed >> 20),tmpStbUsed+49,10),
	              ibp_ltostr((TotalServed >> 20),tmpVol+49,10),
	              ibp_ltostr((TotalUsed >> 20),tmpVolUsed+49,10),
	              tmpRs->MaxDuration);
*/
        }else{
	        sprintf (lc_buffer, "%s %s %s %s %lu \n",
	              ibp_ltostr(0,tmpStb+49,10),
	              ibp_ltostr(0,tmpStbUsed+49,10),
	              ibp_ltostr(0,tmpVol+49,10),
	              ibp_ltostr(0,tmpVolUsed+49,10),
	              tmpRs->MaxDuration);
        }
        break;
      case IBPv040:
        if ( ( buf = (char*)calloc(ST_BUF_LEN,sizeof(char))) == NULL ){
          handle_error(IBP_E_ALLOC_FAILED,&pi_fd);
          IBPErrorNb[pi_nbt] = E_ALLOC;
          goto bail;
        }
        switch ( tmpRs->type ){
          case DISK:
            type = RS_DISK;
            break;
          case MEMORY:
            type = RS_RAM;
            break;
#ifdef SUPPORT_RMT_DEPOT
          case RMT_DEPOT:
              type = 3;
              break;
#endif
          default:
              assert(0);
              break;
        }
            
        dl_insert_b(bufList,(void*)buf);
        if ( glb.enableAlloc == 1 ){
          sprintf(buf,"%s:%d %s:%d %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%ld %s ",
                    ST_RESOURCEID,tmpRs->RID,
                    ST_RESOURCETYPE,type,
                    ST_CONFIG_TOTAL_SZ,TotalConfigured,
                    ST_SERVED_TOTAL_SZ,TotalServed,
                    ST_USED_TOTAL_SZ,TotalUsed,
                    ST_ALLOC_TOTAL_SZ,SoftAllocable,
                    ST_CONFIG_HARD_SZ,HardConfigured,
                    ST_SERVED_HARD_SZ,HardServed,
                    ST_USED_HARD_SZ,HardUsed,
                    ST_ALLOC_HARD_SZ,HardAllocable,
                    ST_DURATION,tmpRs->MaxDuration,
                    ST_RS_END);
        }else{
          sprintf(buf,"%s:%d %s:%d %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%lld %s:%ld %s ",
                    ST_RESOURCEID,tmpRs->RID,
                    ST_RESOURCETYPE,type,
                    ST_CONFIG_TOTAL_SZ,(ibp_ulong)0,
                    ST_SERVED_TOTAL_SZ,(ibp_ulong)0,
                    ST_USED_TOTAL_SZ,(ibp_ulong)0,
                    ST_ALLOC_TOTAL_SZ,(ibp_ulong)0,
                    ST_CONFIG_HARD_SZ,(ibp_ulong)0,
                    ST_SERVED_HARD_SZ,(ibp_ulong)0,
                    ST_USED_HARD_SZ,(ibp_ulong)0,
                    ST_ALLOC_HARD_SZ,(ibp_ulong)0,
                    ST_DURATION,tmpRs->MaxDuration,
                    ST_RS_END);
        }
        bufLen += strlen(buf);
        break;
    }
#if 0
# ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock (&change_lock);
# endif
#endif

    /*fprintf(stderr,"IMPDE lc_buffer = %s\n" , lc_buffer);*/
    if ( ibp_cmd[pi_nbt].ver == IBPv031 ){
      if ( Write (pi_fd, lc_buffer,strlen(lc_buffer)) == -1 ){
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
        goto bail;
      }
    }

    if ( rs != NULL ){
      break;
    }
  }


  /*fprintf(stderr,"IMPDE lc_buffer = %s\n" , lc_buffer);*/
  if ( ibp_cmd[pi_nbt].ver == IBPv040 ){ 
    bufLen = bufLen + 1;
    sprintf(lc_buffer,"%d %d \n",IBP_OK,bufLen);
    if ( Write(pi_fd, lc_buffer,strlen(lc_buffer)) == -1 ){
      IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
      goto bail;
    }
    dl_traverse(d_node,bufList){
      buf = (char*)dl_val(d_node);
      if ( Write(pi_fd,buf,strlen(buf)) == -1 ){
        IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
        goto bail;
      }
    }

    if ( Write (pi_fd,"\n",1) == -1 ){
      IBPErrorNb[pi_nbt] = IBP_E_SOCK_WRITE;
      goto bail; 
      return;
    }
  }

bail:
  dl_traverse(d_node,bufList){
    buf = (char *)dl_val(d_node);
    if (buf != NULL ){ free (buf); }
  }
  if ( lc_buffer != NULL ) { free (lc_buffer); }

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"  handle_status finished\n");
#endif
  return;

#undef ST_BUF_ELN

}

/*****************************/
/* handle_mcopy              */
/* return: none              */
/*****************************/
#if 0
void handle_mcopy (int pi_connfd, int pi_nbt){
  assert(0);
}
#endif

void handle_mcopy (RESOURCE *rs,int pi_connfd, int pi_nbt)
{
  char        *lc_buffer, *dm_buffer;
  int         pid, status, retval;
  char respBuf[CMD_BUF_SIZE];
  char *args[6], temp[8];
  char        fullPath[256];
  char        *ind;
  int         rid;
  JRB	      node;
  char	      *ptr;

  /*****sprintf (respBuf, "%d %lu \n", IBP_OK, 100);
  if(Write(pi_connfd, respBuf, strlen (respBuf)) < 0)
  perror("Write back to client failed"); */

  lc_buffer = (char *)malloc(sizeof(char) * (ibp_cmd[pi_nbt].size + 1));
  dm_buffer = (char *)malloc(sizeof(char) * (ibp_cmd[pi_nbt].size + 10));

  if ((lc_buffer == NULL) || (dm_buffer == NULL)) {
    fprintf(stderr,"there is no enought memory for DM\n");
    exit(1);
  }

  if (ReadLine(pi_connfd, lc_buffer, ibp_cmd[pi_nbt].size) == -1) {
    handle_error(IBP_E_SOCK_READ, NULL);
    ibp_cmd[pi_nbt].Cmd = IBP_NOP;
    return;
  }
  /**sprintf(dm_buffer,"%d %d%s", IBPv031, IBP_MCOPY, lc_buffer); */
  strcpy(dm_buffer, lc_buffer);

  /*
   * get rid of resource ID from source capability's key to 
   * be compatible with old version data mover
   */
  rid = -1;
  rs = glb.dftRs;
  if ( (ind = strstr(lc_buffer,"#")) != NULL ){
    *ind = '\0';
    ptr = ind--;
    while ( ptr >= lc_buffer && *ptr != ' ') { ptr--;}
	fprintf(stderr,"lc_buffer = %s\n",ptr);
	rid = atoi(ptr);	
	fprintf(stderr,"rid = %d\n",rid);
	node = jrb_find_int(glb.resources,rid);
	if ( node != NULL ){
		rs = (RESOURCE *)((jrb_val(node)).v);
	}
  };
  if ( (ind = strstr(dm_buffer,"#")) != NULL ){
    ptr = ind;
    while ( ptr >= dm_buffer &&  *ptr != ' '  ){
		*ptr = ' ';
        ptr--;
    }
  }


  pid = fork();
  if(pid < 0){
    perror("sim: error forking");
    exit(2);
  }
  if(pid != 0){
    waitpid(pid, &status, 0);
     if(WEXITSTATUS(status) == 1){
      sprintf (respBuf, "%d %d \n", IBP_OK, 100);
      if(Write(pi_connfd, respBuf, strlen (respBuf)) < 0)
        perror("Write back to client failed");
      close(pi_connfd);
    }
  }
  else if(pid == 0){
    sprintf(fullPath,"%s%s",glb.execPath,"DM");
    args[0] = fullPath;
    args[1] = dm_buffer;
    args[2] = rs->loc.dir;
    /*args[2] = glb.StableDir;*/
    sprintf(temp,"%d",glb.IbpPort);
    args[3] = temp;
    args[4] = NULL;
    if( (retval=execvp (fullPath, args)) < 0){
      fprintf(stderr,"ibp_server_mt: problem initializing DataMover process: ");
      perror(" ");
    }
  }

}

/*****************************/
/* decode_cmd                */
/* It decodes the user's requirement, envoked by handle_require() */
/* return: none              */
/*****************************/
void *decode_cmd (IBP_REQUEST *request, int pi_nbt)
{
  char                *lc_buffer, *lc_tmp, *lc_singleton;
  int		              li_nextstep = DONE, li_prot;
  IBP_byteArray       ls_container = NULL;
  JRB                 ls_rb;
  int                 li_index,ret;
  int                 pi_connfd;
  IBP_CONNECTION      *conn;
  Jval                lu_val;
  JRB                 node;
  int                 rid;
  char                *tmp;
  time_t              tt;
  char                tt_tmp[50];
  RESOURCE            *rs;

#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"   decode_cmd beginning\n");
#endif

  assert(request != NULL);
  ret = IBP_OK;

#if 0
  lc_save = malloc(CMD_BUF_SIZE);
  lc_buffer = lc_save;
#endif
  lc_buffer = request->arguments;
  lc_singleton = malloc(CMD_BUF_SIZE);
  lc_tmp = lc_singleton;
  conn = get_request_connection(request);
  pi_connfd = get_conn_fd(conn);

  if (  lc_singleton == NULL ){
    handle_error(E_ALLOC,NULL);
    ibp_cmd[pi_nbt].Cmd = IBP_NOP;
    goto bail;
  }
#if 0
  if (ReadComm(pi_connfd, lc_buffer, CMD_BUF_SIZE) == -1) {
    handle_error(IBP_E_SOCK_READ, NULL);
    ibp_cmd[pi_nbt].Cmd = IBP_NOP;
    goto bail;
  } else {
#ifdef IBP_DEBUG
    fprintf(stderr,"T %d, received request: %s\n", pi_nbt, lc_buffer);
    strcat(exec_info[pi_nbt],"     received request: ");
    strcat(exec_info[pi_nbt], lc_buffer);

#ifdef HAVE_PTHREAD_H
    tt = time(0);
    strcpy(tt_tmp,ctime(&tt));
    tt_tmp[strlen(tt_tmp)-1] = '\0';
    ibpWriteThreadLog("[%s]  received request : %s",tt_tmp,lc_buffer);
#endif
#endif
  }
  
#endif
 
  li_prot = request->version;
  if ( ( li_prot != IBPv031 ) && ( li_prot != IBPv040) ){
  /*if ( (li_prot = atoi (lc_singleton)) != IBPv031) */
#ifdef IBP_DEBUG
    fprintf (stderr,"T %d, protocol version not recognised: %d\n", pi_nbt, li_prot);
#endif
    handle_error (IBP_E_PROT_VERS,&pi_connfd );
    ibp_cmd[pi_nbt].Cmd = IBP_NOP;
    goto bail;
  }

  ibp_cmd[pi_nbt].ver = li_prot;

  ibp_cmd[pi_nbt].Cmd = request->msgID;
#ifdef IBP_DEBUG
  printf ("T %d will execute command %d\n",pi_nbt, ibp_cmd[pi_nbt].Cmd);
#endif

  switch (ibp_cmd[pi_nbt].Cmd) {
  case IBP_ALLOCATE:
    switch ( li_prot ){
      case IBPv031:
        ibp_cmd[pi_nbt].rs = glb.dftRs;
        li_nextstep = RELY;
        break;
      case IBPv040:
        li_nextstep = RESC;
        break;
    }
    break;
  case IBP_STORE:
  case IBP_WRITE:
  case IBP_DELIVER:
  case IBP_SEND:
  case IBP_REMOTE_STORE:
  case IBP_LOAD:
  case IBP_MANAGE:
    li_nextstep = KEY;
    break;
  case IBP_STATUS:
    switch ( li_prot ){
      case IBPv031:
        ibp_cmd[pi_nbt].rs = glb.dftRs;
        li_nextstep = DCD_CMD;
        break;
      case IBPv040:
        li_nextstep = RESC;
        break;
    }
    break;
  case IBP_NFU:
    /* get operator of nfu */
    lc_buffer = (char*)IBP_substr(lc_buffer,lc_singleton);
    ibp_cmd[pi_nbt].type = atoi(lc_singleton);

    /* get # of parameters */
    lc_buffer = (char *)IBP_substr(lc_buffer,lc_singleton);
    ibp_cmd[pi_nbt].size = atoi(lc_singleton);

    li_nextstep = DONE;
    break;
  case IBP_MCOPY:
    lc_buffer = (char *) IBP_substr (lc_buffer, lc_singleton);
    ibp_cmd[pi_nbt].size = atoi (lc_singleton);
    li_nextstep = DONE;
    break;
  default:
    ibp_cmd[pi_nbt].Cmd = IBP_NOP;
    break;
  }

  if (ibp_cmd[pi_nbt].Cmd == IBP_NOP)
    goto bail;
     
  while (li_nextstep != DONE ) {

    lc_buffer = (char *) IBP_substr (lc_buffer, lc_singleton);

    switch (li_nextstep) {
      case RESC:
        if ( 0 != ibpStrToInt(lc_singleton,&rid)){
            handle_error(IBP_E_UNKNOWN_RS,&pi_connfd);
            ibp_cmd[pi_nbt].Cmd = IBP_NOP;
            li_nextstep = DONE;
            continue;
            break;
        };
#if 0
        if ( strcmp(lc_singleton,DEFAULT_RID) == 0 ){
          ibp_cmd[pi_nbt].rs = glb.dftRs;
        }else{
#endif
        node = jrb_find_int(glb.resources,rid);
        if ( node != NULL ){
          ibp_cmd[pi_nbt].rs = (RESOURCE *)((jrb_val(node)).v);
        }else {
          switch(ibp_cmd[pi_nbt].Cmd){
          case IBP_ALLOCATE:
            handle_error(IBP_E_UNKNOWN_RS,&pi_connfd);
            ibp_cmd[pi_nbt].Cmd = IBP_NOP;
            li_nextstep = DONE;
            continue;
            break;
          case IBP_STATUS:
            if ( strcmp(lc_singleton,ALL_RID) == 0 ){
                ibp_cmd[pi_nbt].rs = NULL;
              }else {
                handle_error(IBP_E_UNKNOWN_RS,&pi_connfd);
                ibp_cmd[pi_nbt].Cmd = IBP_NOP;
                li_nextstep = DONE;
                continue;
              }
              break;
          }
        }
        if ( ibp_cmd[pi_nbt].rs != NULL ){
          switch ( ibp_cmd[pi_nbt].rs->type ){
            case DISK:
            case MEMORY:
            case ULFS:
#ifdef SUPPORT_RMT_DEPOT
            case RMT_DEPOT:
#endif
              break;
            default:
              handle_error(IBP_E_UNKNOWN_RS,&pi_connfd);
              ibp_cmd[pi_nbt].Cmd = IBP_NOP;
              li_nextstep= DONE;
              continue;
          }
        }

        switch( ibp_cmd[pi_nbt].Cmd ){
          case IBP_ALLOCATE:
            li_nextstep = RELY;
            break;
          case IBP_STATUS:
            li_nextstep = DCD_CMD;
            break;
        }

        break;

    case RELY:
      ibp_cmd[pi_nbt].reliable = atoi(lc_singleton);
#ifdef IBP_DEBUG
      fprintf(stderr, "T %d, Reliability : %d\n",pi_nbt, ibp_cmd[pi_nbt].reliable);
#endif

      if ((ibp_cmd[pi_nbt].reliable != IBP_STABLE) &&
	      (ibp_cmd[pi_nbt].reliable != IBP_VOLATILE)) {
	      handle_error (IBP_E_INVALID_PARAMETER, &pi_connfd);
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
      tmp = index(lc_singleton,DEFAULT_RID_CHR);
      if ( tmp != NULL ){
        rid = atoi(lc_singleton);
        lc_singleton = tmp+1;
        *tmp = '\0';
        node = jrb_find_int(glb.resources,rid);
        if ( node == NULL ){
          handle_error(IBP_E_UNKNOWN_RS,&pi_connfd);
          ibp_cmd[pi_nbt].Cmd = IBP_NOP;
          li_nextstep = DONE;
          continue;
        }else{
          ibp_cmd[pi_nbt].rs = (RESOURCE *)((jrb_val(node)).v);
        }
      }else{
        ibp_cmd[pi_nbt].rs = glb.dftRs;
      }
        
      strcpy(ibp_cmd[pi_nbt].key, lc_singleton);

#ifdef IBP_DEBUG
      fprintf (stderr, "T %d, Key : %s\n", pi_nbt, ibp_cmd[pi_nbt].key);
#endif

      /*
       * get the bytearray now
       */
      rs = ibp_cmd[pi_nbt].rs;

      pthread_mutex_lock(&(rs->change_lock));
      ls_rb = jrb_find_str(rs->ByteArrayTree,ibp_cmd[pi_nbt].key);

      if (ls_rb == NULL) {
        pthread_mutex_unlock(&(rs->change_lock));
	      handle_error(IBP_E_CAP_NOT_FOUND, &pi_connfd);
	      /*close(pi_connfd);*/
	      ibp_cmd[pi_nbt].Cmd = IBP_NOP;
	      li_nextstep = DONE;
	      continue;
      }

      lu_val = jrb_val (ls_rb);
      ls_container = (IBP_byteArray) lu_val.s;
      assert(ls_container != NULL );

      if (ls_container->deleted == 1) {
        pthread_mutex_unlock(&(rs->change_lock));
	      handle_error(IBP_E_CAP_NOT_FOUND, &pi_connfd);
        ls_container = NULL;
	      /*close(pi_connfd);*/
	      ibp_cmd[pi_nbt].Cmd = IBP_NOP;
	      li_nextstep = DONE;
	      continue;
      }
      ls_container->reference++;
      pthread_mutex_unlock(&(rs->change_lock));

      switch (ibp_cmd[pi_nbt].Cmd) {
      case IBP_STORE:
      case IBP_WRITE:
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
      printf("T %d, Type : %d\n",pi_nbt, ibp_cmd[pi_nbt].type);
#endif

      if ((ibp_cmd[pi_nbt].type != IBP_BYTEARRAY) &&
	        (ibp_cmd[pi_nbt].type != IBP_FIFO) &&
	        (ibp_cmd[pi_nbt].type != IBP_BUFFER) &&
	        (ibp_cmd[pi_nbt].type != IBP_CIRQ)) {
	      handle_error (IBP_E_INVALID_PARAMETER, &pi_connfd);
	      ibp_cmd[pi_nbt].Cmd = IBP_NOP;
	      li_nextstep = DONE;
	      continue;
      }

      li_nextstep = LIFE;
      break;

    case LIFE:
      ibp_cmd[pi_nbt].lifetime = atoi (lc_singleton);

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
      strcpy (ibp_cmd[pi_nbt].host, lc_singleton);

#ifdef IBP_DEBUG
      printf ("T %d, Host : %s\n", pi_nbt, ibp_cmd[pi_nbt].host);
#endif

      li_nextstep = PORT;
      break;

    case PORT:
      ibp_cmd[pi_nbt].port = atoi (lc_singleton);

#ifdef IBP_DEBUG
      printf ("T %d, Port: %hu\n", pi_nbt, ibp_cmd[pi_nbt].port);
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
      strcpy(ibp_cmd[pi_nbt].wKey,lc_singleton);

      /*ibp_cmd[pi_nbt].writekey = atoi (lc_singleton);*/

#ifdef IBP_DEBUG
      printf("T %d, Write Key: %s\n", pi_nbt, ibp_cmd[pi_nbt].wKey);
#endif

      /*if (ibp_cmd[pi_nbt].writekey != ls_container->writeKey) */
      if (strcmp(ibp_cmd[pi_nbt].wKey,ls_container->writeKey_s) != 0 ){ 
	      handle_error(IBP_E_INVALID_WRITE_CAP, &pi_connfd);
	      ibp_cmd[pi_nbt].Cmd = IBP_NOP;
	      li_nextstep = DONE;
	      continue;
      }
      switch (ibp_cmd[pi_nbt].Cmd){
      case IBP_WRITE:
        li_nextstep = OFFSET;
        break;
      case IBP_STORE:
        ibp_cmd[pi_nbt].Offset = ls_container->currentSize;
        li_nextstep = SIZE;
        break;
      }
      break;

    case RKEY:
      strcpy(ibp_cmd[pi_nbt].rKey,lc_singleton);
      /*ibp_cmd[pi_nbt].ReadKey = atoi (lc_singleton);*/

#ifdef IBP_DEBUG
      printf("T %d, Read Key: %s\n", pi_nbt, ibp_cmd[pi_nbt].rKey);
#endif

      /*if (ibp_cmd[pi_nbt].ReadKey != ls_container->readKey)*/
      if (strcmp(ibp_cmd[pi_nbt].rKey,ls_container->readKey_s) != 0 ){
	      handle_error (IBP_E_INVALID_READ_CAP, &pi_connfd);
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
        handle_error (IBP_E_INV_PAR_SIZE, &pi_connfd);
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
          handle_error (IBP_E_WOULD_EXCEED_LIMIT, &pi_connfd);
        }
        else {
          handle_error (IBP_E_INV_PAR_SIZE, &pi_connfd);
        }
        ibp_cmd[pi_nbt].Cmd = IBP_NOP;
        li_nextstep = DONE;
        continue;
      }

#ifdef IBP_DEBUG
      printf("T %d, Size : %lu Offset : %lu\n", pi_nbt, ibp_cmd[pi_nbt].size,ibp_cmd[pi_nbt].Offset);
#endif

      li_nextstep = DCD_SRVSYNC;
      break;

    case MKEY:
      /*ibp_cmd[pi_nbt].ManageKey = atoi (lc_singleton);*/
      strcpy(ibp_cmd[pi_nbt].mKey, lc_singleton);

#ifdef IBP_DEBUG
      printf("T %d, Manage Key : %s\n", pi_nbt, ibp_cmd[pi_nbt].mKey);
#endif

      li_nextstep = DCD_CMD;
      break;

    case DCD_CMD:
      ibp_cmd[pi_nbt].ManageCmd = atoi (lc_singleton);

#ifdef IBP_DEBUG
      printf("T %d, Manage/Status Command : %d\n", pi_nbt, ibp_cmd[pi_nbt].ManageCmd);
#endif
      switch (ibp_cmd[pi_nbt].Cmd) {
      case IBP_MANAGE:
	      li_nextstep = DCD_CAP;
	      break;
      case IBP_STATUS:
        if ( ( ibp_cmd[pi_nbt].ManageCmd == IBP_ST_CHANGE ) && ( ibp_cmd[pi_nbt].rs == NULL )){
          handle_error(IBP_E_BAD_FORMAT,&pi_connfd);
          ibp_cmd[pi_nbt].Cmd = IBP_NOP;
          li_nextstep = DONE;
          continue;
        }
	      li_nextstep = DCD_PWD;
	      break;
      }
      break;

    case DCD_CAP:
      ibp_cmd[pi_nbt].CapType = atoi (lc_singleton);

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
          handle_error (IBP_E_WOULD_EXCEED_LIMIT, &pi_connfd);
        }
        else {
          handle_error (IBP_E_INV_PAR_SIZE, &pi_connfd);
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
      strcpy (ibp_cmd[pi_nbt].Cap, lc_singleton);

      if ( (ret = setIBPURI(&(ibp_cmd[pi_nbt].url),lc_singleton)) != IBP_OK ){
        handle_error(ret,&pi_connfd);
        li_nextstep = DONE;
        ibp_cmd[pi_nbt].Cmd = IBP_NOP;
        continue;
      }

#ifdef IBP_DEBUG
      fprintf (stderr, "T %d, Cap: %s\n", pi_nbt,(char *) ibp_cmd[pi_nbt].Cap);
#endif

      li_nextstep = RKEY;
      break;

    case DCD_PWD:
      if (ibp_cmd[pi_nbt].ManageCmd == 2) {
        li_index = 0;
        while ((li_index < 16) && (lc_singleton[li_index] != '\n') && (lc_singleton[li_index] != '\0'))
	  li_index++;
        lc_singleton[li_index] = '\0';

        strcpy (ibp_cmd[pi_nbt].passwd, lc_singleton);        
# ifdef IBP_DEBUG
        fprintf (stderr, "T %d, Password: %s\n", pi_nbt, (char *) ibp_cmd[pi_nbt].passwd);
# endif
      }
      li_nextstep = DCD_SRVSYNC;
      break;

    case DCD_SRVSYNC:
      ibp_cmd[pi_nbt].ServerSync = atol (lc_singleton);

#ifdef IBP_DEBUG
      printf("T %d, ServerSync : %d\n", pi_nbt,ibp_cmd[pi_nbt].ServerSync);
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
      ibp_cmd[pi_nbt].TRGSYNC = atol (lc_singleton);

#ifdef IBP_DEBUG
      printf("T %d, Target ServerSync : %d\n", pi_nbt, ibp_cmd[pi_nbt].TRGSYNC);
#endif

      li_nextstep = DCD_TRGTOUT;
      break;

    case DCD_TRGTOUT:
      ibp_cmd[pi_nbt].TRGTOUT = atol (lc_singleton);

#ifdef IBP_DEBUG
      printf("T %d, Target ClientTimeout : %d\n", pi_nbt, ibp_cmd[pi_nbt].TRGTOUT);
#endif

      li_nextstep = DONE;
      break;

    default:
      li_nextstep = DONE;
      break;
    }
  }

bail:
  if ( lc_tmp != NULL ) { free(lc_tmp); }


#ifdef IBP_DEBUG
  strcat(exec_info[pi_nbt],"   decode finished\n");
#endif
  
  if ( (ibp_cmd[pi_nbt].Cmd == IBP_NOP) && ( ls_container != NULL)){
    pthread_mutex_lock(&((ibp_cmd[pi_nbt].rs)->change_lock));
    ls_container->reference--;
    pthread_mutex_unlock(&((ibp_cmd[pi_nbt].rs)->change_lock));
    if ( ibp_cmd[pi_nbt].url.rs != NULL && ibp_cmd[pi_nbt].url.ba != NULL ){
      pthread_mutex_lock(&((ibp_cmd[pi_nbt].url.rs)->change_lock));
      ibp_cmd[pi_nbt].url.ba->reference--;
      pthread_mutex_unlock(&((ibp_cmd[pi_nbt].url.rs)->change_lock));
    }
  }

  return ls_container;
}


int setIBPURI( IBP_URL *iu, IBP_cap cap ){
  char *str=NULL,*tmp;
  char *last;
  char dli1[]="/";
  char *token1;
  JRB  node;

  
  if ( (str = strdup(cap)) == NULL ){
    return (IBP_E_ALLOC_FAILED);
  }

  iu->ba = NULL;
 
  if (( token1 = strtok_r(str,dli1,&last)) == NULL ) {
    goto bail;
  }
  if ( strcasecmp(token1,"ibp:") != 0 ){
    goto bail;
  }

  if (( token1 = strtok_r(NULL,dli1,&last)) == NULL ) {
    goto bail;
  }
  if ( (tmp = rindex(token1,':')) == NULL ){
    goto bail;
  }
  *tmp='\0';
  tmp = tmp + 1;
  strcpy(iu->host,token1);
  iu->port = atoi(tmp);

  if (( token1 = strtok_r(NULL,dli1,&last)) == NULL ) {
    goto bail;
  }
  if ( (tmp = index(token1,'#')) == NULL ){
    iu->rid=0;
  }else{
    *tmp = '\0';
    tmp = tmp + 1;
    /*strcpy(iu->rid,token1);*/
    if ( 0 != ibpStrToInt(token1,&(iu->rid))){
        goto bail;
    }
    strcpy(iu->key,tmp);
  }

  if (( token1 = strtok_r(NULL,dli1,&last)) == NULL ) {
    goto bail;
  }
  strcpy(iu->attr_key,token1);
 

  if (( token1 = strtok_r(NULL,dli1,&last)) == NULL ) {
    goto bail;
  }
  strcpy(iu->type , token1);

  if ( (strcasecmp(iu->host,glb.hostname) == 0) && 
       (iu->port == glb.IbpPort)){
    if ( iu->rid == 0 ){
      iu->rs = glb.dftRs;
    }else{
      node = jrb_find_int(glb.resources, iu->rid);
      if ( node != NULL ){
        iu->rs = (RESOURCE*)((jrb_val(node)).v);
      }else{
        iu->rs = NULL;
      }
    }
    if (iu->rs != NULL ){
      pthread_mutex_lock(&(iu->rs->change_lock));
      node = jrb_find_str(iu->rs->ByteArrayTree,iu->key);
      if ( node != NULL ){
        iu->ba = (IBP_byteArray)((jrb_val(node)).s);
      }else{
        pthread_mutex_unlock(&(iu->rs->change_lock));
        free(str);
        return(IBP_E_CAP_NOT_FOUND);
      }
      if ( iu->ba->deleted == 1 ){
          pthread_mutex_unlock(&(iu->rs->change_lock));
          iu->ba = NULL;
          free(str);
          return ( IBP_E_CAP_NOT_FOUND);
      }
      iu->ba->reference++;
      pthread_mutex_unlock(&(iu->rs->change_lock));
    }
  }else{
    iu->rs = NULL;
    iu->ba = NULL;
  }
  
  if ( str != NULL ) { free(str); }
  return (IBP_OK);

bail:
  free(str);
  return (IBP_E_WRONG_CAP_FORMAT); 
} 

/* } */


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
  IS	      ls_in;
  int         li_return = 0;
  int         defaultRS = 1;
  int         rid = -1;
  RESOURCE     *rs;
  Jval         jv;
  IPv4_addr_t   *ps_newIPv4,*ps_tmpIPv4;
#ifdef AF_INET6
  IPv6_addr_t   *ps_newIPv6,*ps_tmpIPv6;
#endif  
  if ((ls_in = (IS) new_inputstruct(pc_path)) == NULL) {
    
    /* below modified by yong zheng to add hostname in config file and command line */
    fprintf(stderr,"config file %s open error\n",pc_path);
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
    while ((get_line(ls_in) >=0) && (li_return == 0)) {
      if (ls_in->NF < 2)
	continue;
      if (ls_in->fields[0][0] == '#' )
      	continue;
      if ( strcmp(ls_in->fields[0], RS_RESOURCEID) == 0 ){
          ls_in->fields[1][MAX_RID_LEN] = '\0';
          if ( defaultRS ){
              defaultRS = 0;
              if ( 0 != ibpStrToInt(ls_in->fields[1],&rid) ){
                 fprintf(stderr,"Invalid Resource ID: %s\n",ls_in->fields[1]);
                 exit(-1);
              }
              if ( rid != 0 ){
                fprintf(stderr,"The first resource must use 0 as resource id!\n");
                exit(-1);
              }
              rs = glb.dftRs;
#if 0
              if ( glb.dftRs->RID[0] == '\0' ){
                  rs = glb.dftRs;
              }else if ( strcmp(ls_in->fields[1],glb.dftRs->RID) == 0) {
                  rs = glb.dftRs;
              }else{
                  if ( isValidRsCfg(glb.dftRs) == IBP_OK ){
                    jv.v = (void *)(glb.dftRs);
                    jrb_insert_str(glb.resources,glb.dftRs->RID,jv);
                  }else {
                    li_return = -1;
                    continue;
                  }
                  if ( ( rs = crtResource()) == NULL ){
                    handle_error(E_ALLOC,NULL);
                    li_return = -1;
                    continue;
                  }
              }
#endif
          }else if ( (rs = crtResource ()) == NULL ){
              handle_error(E_ALLOC,NULL);
              li_return = -1;
              continue;
          }
          li_return = getRsCfg(rs,ls_in);
          if ( li_return != IBP_OK){
              /*handle_error(li_return,&ls_in);*/
              freeResource(rs);
              rs = NULL;
              li_return = -1;
              continue;
          }else {
              if ( jrb_find_int(glb.resources,rs->RID) != NULL ){
                fprintf(stderr," ERROR: Duplicated resource id <%d>\n",rs->RID);
                freeResource(rs);
                rs = NULL;
                li_return = -1;
                continue;
              }
              jv.v = (void*)rs;
              jrb_insert_int(glb.resources,rs->RID,jv);
              if ( glb.dftRs == NULL ){
                  glb.dftRs = rs ;
              }
              /*printResource(rs,stderr);*/
              /*fprintf(stderr,"line = %d\n",ls_in->line);*/
              /*getchar();*/
          }
          li_return = 0;
          continue;
#if 0
      }else if ((strcmp(ls_in->fields[0], "VOLSIZE") == 0) || \
            (strcmp(ls_in->fields[0], "SOFTSIZE") == 0)) {
	if (glb.VolStorage > 0)
	  continue;
	if ( (glb.VolStorage = ibp_getStrSize(ls_in->fields[1])) == 0 && errno != 0 ){  
	  handle_error (E_CFGFILE, &ls_in);
          li_return = -1;
	  continue;
	}
      } 

      else if ((strcmp(ls_in->fields[0], "STABLESIZE") == 0) || \
            (strcmp(ls_in->fields[0], "HARDSIZE") == 0)) {
	if (glb.StableStorage > 0)
	  continue;
	if ( (glb.StableStorage = ibp_getStrSize(ls_in->fields[1])) == 0 && errno != 0 ){  
	  handle_error (E_CFGFILE, &ls_in);
          li_return = -1;
          continue;
	}
      }

      else if (strcmp(ls_in->fields[0], "MINFREESIZE") == 0) {
        if ( (glb.MinFreeSize = ibp_getStrSize(ls_in->fields[1])) == 0 && errno != 0 ){
          handle_error (E_CFGFILE, &ls_in);
          li_return = -1;
          continue;
        }
      } else if ((strcmp(ls_in->fields[0], "STABLEDIR") == 0) || \
                (strcmp(ls_in->fields[0], "HARDDIR") == 0))  {
	if (glb.StableDir != NULL)
	  continue;
	lc_generic = ls_in->fields[1];
	if (lc_generic[0] != '/') {
	  handle_error (E_ABSPATH, &ls_in);
          li_return = -1;
          continue;
	}
	if (lc_generic[strlen(lc_generic)-1] != '/')
	  strcat(lc_generic,"/");
	glb.StableDir = strdup (lc_generic);
	if (glb.StableDir == NULL) {
	  handle_error (E_ALLOC, NULL);
	  li_return = -1;
	  continue;
	}
      }

      else if ((strcmp(ls_in->fields[0], "VOLDIR") == 0) || \
                 (strcmp(ls_in->fields[0], "SOFTDIR") == 0)) {
	if (glb.VolDir != NULL)
	  continue;
	lc_generic = ls_in->fields[1];
	if (lc_generic[0] != '/') {
	  handle_error (E_ABSPATH, &ls_in);
          li_return = -1;
          continue;
	}
	if (lc_generic[strlen(lc_generic)-1] != '/')
	  strcat(lc_generic,"/");
	glb.VolDir = strdup (lc_generic);
	if (glb.VolDir == NULL) {
	  handle_error (E_ALLOC, NULL);
	  li_return = -1;
	  continue;
	}
      }

      else if (strcmp(ls_in->fields[0], "MAXDURATION") == 0) {
	if (sscanf(ls_in->fields[1], "%f", &lf_ftemp) != 1) {
	  handle_error (E_CFGFILE, &ls_in);
	  li_return = -1;
          continue;
	}
	if (glb.MaxDuration == 0) {
	  if (lf_ftemp < 0)
	    glb.MaxDuration = -1;
	  else
	    glb.MaxDuration = (long) (lf_ftemp * 24 * 3600);
	}
#endif
      }

      else if (strcmp(ls_in->fields[0], "PASSWORD") == 0) {
	if (strcmp(gc_passwd, "ibp") == 0) {
	  strcpy (gc_passwd, ls_in->fields[1]);
	}
      }
      
      else if ( strcmp(ls_in->fields[0],"NFUCFGFILE") == 0 ){
        if ( glb.nfu_ConfigFile == NULL ){
            if ((glb.nfu_ConfigFile = 
                        strdup(ls_in->fields[1])) == NULL ){
                handle_error(E_ALLOC,NULL);
                li_return = -1;
                continue;
            }
        }
      }else if (strcmp(ls_in->fields[0],"QUEUELEN") == 0){
	    if ((glb.queue_len = atoi(ls_in->fields[1])) < 0 ){
	        handle_error(E_CFGFILE,&ls_in);
	        li_return = -1;
	        continue;
	    }
    }else if ( strcmp(ls_in->fields[0],"ENABLEAUTHEN") == 0){
        if ( glb.enableAuthen == 0 ){
            glb.enableAuthen = atoi(ls_in->fields[1]);
        }
    }else if ( strcmp(ls_in->fields[0],"DEPOTCERTFILE") == 0){
        glb.depotCertFile = strdup(ls_in->fields[1]);
        if ( glb.depotCertFile == NULL ){
            handle_error(E_ALLOC,NULL);
            li_return = -1;
            continue;
        }
    }else if ( strcmp(ls_in->fields[0],"DEPOTPRIVATEKEYFILE") == 0){
        glb.depotPrivateKeyFile = strdup(ls_in->fields[1]);
        if ( glb.depotPrivateKeyFile == NULL ){
            handle_error(E_ALLOC,NULL);
            li_return = -1;
            continue;
        }
    }else if ( strcmp(ls_in->fields[0],"CAFILE") == 0){
        if ( glb.cafile == NULL ){
            glb.cafile = strdup(ls_in->fields[1]);
            if ( glb.cafile == NULL ){
                handle_error(E_ALLOC,NULL);
                li_return = -1;
                continue;
            }
        }
    }else if (strcmp(ls_in->fields[0],"MAXIDLETIME") == 0){
	if ((glb.idle_time = atoi(ls_in->fields[1])) < 0 ){
	    glb.idle_time = IBP_CONN_MAX_IDLE_TIME;
	}
      }
      else if (strcmp(ls_in->fields[0], "ALLOCATEPOLICY") == 0) {
        if (sscanf(ls_in->fields[1], "%d", &glb.AllocatePolicy) != 1) {
          handle_error (E_CFGFILE, &ls_in);
          glb.AllocatePolicy = IBP_ALLPOL_SID;
          li_return = -1;
          continue;
        } else {
          if ((glb.AllocatePolicy < 0) || (glb.AllocatePolicy > 1)) {
            handle_error (E_CFGFILE, &ls_in);
            glb.AllocatePolicy = IBP_ALLPOL_SID;
            li_return = -1;
            continue;
          }
        }
      }
      else if ( strcmp(ls_in->fields[0],"ENABLEALLOC") == 0){
        glb.enableAlloc = atoi(ls_in->fields[1]);
      }
      else if ( strcmp(ls_in->fields[0],"HOSTNAME") == 0) {
	if ( glb.hostname != NULL )
	  continue;
        if ( (glb.hostname = strdup(ls_in->fields[1])) == NULL ){
	  handle_error(E_ALLOC,NULL);
	  li_return = -1;
        }
      }
#ifdef HAVE_PTHREAD_H
      else if ( strcmp(ls_in->fields[0],"THREADS") == 0) {
	if ( glb.IbpThreadMin > 0 )
	  continue;
	glb.IbpThreadNum = atoi(ls_in->fields[1]);
	glb.IbpThreadMin = atoi(ls_in->fields[1]);	
      }

      else if ( strcmp(ls_in->fields[0],"MAXTHREADS") == 0) {
	if ( glb.IbpThreadMax > 0 )
	  continue;
	if(atoi(ls_in->fields[1]) > MAX_THREADS){
	  glb.IbpThreadMax = MAX_THREADS; 
	}
	else{
	  glb.IbpThreadMax = atoi(ls_in->fields[1]); 
	}	
      }

#endif
      else if (strcmp(ls_in->fields[0], "CFGPORT") == 0) {
        if (glb.IbpPort != 0)
          continue;
	if (sscanf(ls_in->fields[1], "%d", &glb.IbpPort) != 1) {
          handle_error(E_PORT, (void *) &glb.IbpPort);
	  handle_error (E_CFGFILE, &ls_in);
          glb.IbpPort = IBP_DATA_PORT;
          li_return = -1;
          continue;
        }
        if ((glb.IbpPort < 1024) || (glb.IbpPort > 65535)) {
          handle_error(E_PORT, NULL);
	  handle_error (E_CFGFILE, &ls_in);
          glb.IbpPort = IBP_DATA_PORT;
          li_return = -1;
          continue;
        }
      }
      
     else if ( strcmp(ls_in->fields[0],"CL_IPv4") == 0 ){
	if (ls_in->NF < 3) {
		handle_error(E_CFGFILE,&ls_in);
		li_return = -1;
		continue;
	}
	
	if ( ( ps_newIPv4 = (IPv4_addr_t*)malloc(sizeof(IPv4_addr_t))) == NULL ) {
		 handle_error (E_ALLOC, NULL);
		 li_return = -1;
		 continue;
	}
	ps_newIPv4->next = NULL;
	if ( (ps_newIPv4->addr = inet_addr(ls_in->fields[1])) == -1 ){
		handle_error(E_CFGFILE,&ls_in);
		li_return = -1;
		free(ps_newIPv4);
		continue;	
	}
	if ( (ps_newIPv4->mask = inet_addr(ls_in->fields[2])) == -1){
		if ( strcmp(trim(ls_in->fields[2]),"255.255.255.255") != 0 ){
			handle_error(E_CFGFILE,&ls_in);
			li_return = -1;
			free(ps_newIPv4);
			continue;
		}
	}

	ps_tmpIPv4 = glb.IPv4;
	if ( ps_tmpIPv4 == NULL ){
		glb.IPv4 = ps_newIPv4;
	}else {
		while ( ps_tmpIPv4->next != NULL )  ps_tmpIPv4 = ps_tmpIPv4->next;      	
		ps_tmpIPv4->next = ps_newIPv4;	
	}
	
	continue;
      }
#ifdef AF_INET6      
     else if ( strcmp(ls_in->fields[0],"CL_IPv6") == 0 ) {
	if (ls_in->NF < 3) {
		handle_error(E_CFGFILE,&ls_in);
		li_return = -1;
		continue;
	}
	/* fprintf(stderr,"IPv6: %s %s\n",ls_in->fields[1],ls_in->fields[2]); */
	if ( ( ps_newIPv6 = (IPv6_addr_t*)malloc(sizeof(IPv6_addr_t))) == NULL ) {
		 handle_error (E_ALLOC, NULL);
		 li_return = -1;
		 continue;
	}
	ps_newIPv6->next = NULL;
	if ( inet_pton(AF_INET6,ls_in->fields[1],&(ps_newIPv6->addr)) == -1 ){
		handle_error(E_CFGFILE,&ls_in);
		li_return = -1;
		free(ps_newIPv6);
		continue;
	}
	if ( inet_pton(AF_INET6,ls_in->fields[2],&(ps_newIPv6->mask)) == -1 ) {
		handle_error(E_CFGFILE,&ls_in);
		li_return = -1;
		free(ps_newIPv6);
		continue;
	}
	ps_tmpIPv6 = glb.IPv6;
	if ( ps_tmpIPv6 == NULL ){
		glb.IPv6 = ps_newIPv6;
	}else {
		while ( ps_tmpIPv6->next != NULL )  ps_tmpIPv6 = ps_tmpIPv6->next;      	
		ps_tmpIPv6->next = ps_newIPv6;	
	}
	if ( inet_ntop(AF_INET6,&(ps_newIPv6->addr),ls_in->fields[1],46) == NULL ){
		fprintf(stderr,"Error occur in convert address!!");
		exit(-1);
	}
	if ( inet_ntop(AF_INET6,&(ps_newIPv6->mask),ls_in->fields[2],46) == NULL ){
		fprintf(stderr,"Error occur in convert address!!");
		exit(-1);
	}
	continue;
       }
#endif       
    } /* end while */
    jettison_inputstruct(ls_in);
  }

  if (li_return == -1)
    return li_return;

#if 0
  if (glb.VolStorage + glb.StableStorage <= 0) {
    handle_error (E_ZEROREQ, NULL);
    li_return = -1;
  }

  if (glb.StableDir == NULL)
    glb.StableDir = strdup("/tmp/");

  if (glb.VolDir == NULL)
    glb.VolDir = strdup("/tmp/");
#endif

  if (glb.IbpPort == 0)
    glb.IbpPort = IBP_DATA_PORT;

#if 0
  if (glb.MaxDuration == 0) 
    glb.MaxDuration = -1;
#endif

  return li_return;
}

/*****************************/
/* recover                   */
/* It recovers the old IBP file when the depot reboots */
/* return: OK / not          */
/*****************************/
#if 0
int recover (){
    assert (0);
}
#endif
int createSubDirs(RESOURCE *rs)
{   
    char *dir = NULL;
    int  n=0;

    if ( rs->type != DISK ) { return IBP_OK; }
    if ( NULL == (dir = (char*)calloc(sizeof(char),4096))){
        fprintf(stderr,"Out of Memory !\n");
        exit(-1);
    }
    for ( n = 0; n < rs->subdirs ; n++){
       sprintf(dir,"%s%s%d",rs->loc.dir,"data",n);
       if ( -1 == mkdir(dir,S_IRUSR|S_IWUSR|S_IXUSR|S_IXGRP|S_IRGRP|S_IROTH)){
         if ( errno == EEXIST){
            continue ;
         }else{
            fprintf(stderr," can't create data sub dir [%s]: %s!\n",dir,strerror(errno));
            free(dir);
            return(-1);
         }
       }
    }

    free(dir);
    return(IBP_OK);
}

void lockPidFile (RESOURCE *rs)
{
    char *fname;
    int size;
    int fd;
    struct flock lock;
    char  buf[11];

    size = strlen(rs->loc.dir) + 1 + strlen("pid.ibp");
    if ( NULL == (fname = (char*)calloc(sizeof(char),size))){
        fprintf(stderr,"Out of Memory!\n");
        exit(-1);
    }

    sprintf(fname,"%s%s",rs->loc.dir,"pid.ibp");
    if ( -1 == (fd = open(fname,O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH))){
        fprintf(stderr,"Can't create lock file <%s> !\n",fname);
        exit(-1);
    }

    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_whence = SEEK_SET;
    lock.l_len = 0;

    if ( fcntl(fd,F_SETLK,&lock) < 0){
        if ( errno == EACCES || errno == EAGAIN ){
            fprintf(stderr,"another IBP depot process is running on this resource, exit. \n");
            exit(0);
        }else{
            fprintf(stderr,"Can't lock file <%s> !\n",fname);
            exit(-1);
        }
    }

    if ( ftruncate(fd,0) < 0){
        fprintf(stderr,"can't clear file <%s>!\n",fname);
        exit(-1);
    }
    sprintf(buf,"%d",getpid());
    if ( write(fd,buf,strlen(buf)) != strlen(buf)){
        fprintf(stderr,"can't write pid to file <%s>!\n",fname);
        exit(-1);
    }

    free(fname);
    return ;
}

static int 
check_cap_file( RESOURCE *rs, IBP_byteArray ba)
{
    int             capfd;
    IBP_FILE_HEADER header;
    int             header_len;
    int             size;
    struct stat     sb;
    
    if ( (capfd = open(ba->fname,O_RDWR,0)) < 0){
        return -1;
    };
    header_len = sizeof(IBP_FILE_HEADER);
    if (read(capfd,&header,header_len) != header_len ){
        close(capfd);
        return -1;
    };
#if 0
    if ( header.headerLen - header_len > PATH_MAX ){
        name_len = PATH_MAX; 
    }else{
        name_len = header.headerLen - header_len; 
    };
    
    if ( read(capfd,buf,name_len) != name_len){
        close(capfd);
        fprintf(stderr,"Can't read cap file name !\n");
        return -1;
    };
    buf[PATH_MAX-1] = '\0';
    if ( strncmp(ba->fname,buf,strlen(ba->fname)) != 0 ){
        fprintf(stderr,"mismatch cap file name !\n");
        close(capfd);
        return -1;
    }
#endif
    if ( fstat(capfd,&sb) != 0 ){
        close(capfd);
        return -1;
    };
    assert(ba->writeKey == header.wKey);
    assert(ba->readKey == header.rKey);
    assert(ba->manageKey == header.mKey);

    size = sb.st_size - header.headerLen;
    if ( size != ba->currentSize ){
        /* something wrong */
        ba->currentSize = size;
        if ( ba->maxSize < ba->currentSize ){
            ba->maxSize = ba->currentSize;
        }
        ba->readRefCount = 1;
        ba->writeRefCount = 1;
        ba->deleted = 0;
        ba->reference = 0;
        if ( ba->attrib.duration > 0){
            ba->attrib.duration = time(0) + rs->MaxDuration;
        }
        header.maxSize = ba->maxSize;
        header.curSize = ba->currentSize;
        header.rCount = ba->readRefCount;
        header.wCount = ba->writeRefCount;
        header.delete = 0;

        /* write back to file */
        if ( lseek(capfd,0,SEEK_SET) == -1 ){
            fprintf(stderr,"Can't roll back to beginning of the file %s!\n",
                    ba->fname);
            close(capfd);
            return -1;
        }

        if ( write(capfd,&header,header_len) != header_len){
            fprintf(stderr,"Can't write header info to file %s!\n",
                   ba->fname);
            close(capfd);
            return (-1);
        }
    }
    

    close(capfd);

    return 0;
};

int recover (RESOURCE *rs ) 
{
  IBP_byteArray       ls_container;
  time_t	      lt_now;
  Jval                lu_val;
  char                *now,tmp_buf[30];

  char                  lc_oldlss[128];
  FILE                  *lf_oldlssfd;
  IBP_StableBackupInfo  ls_stableinfo;
  IBP_VariableBackupInfo  ls_variableinfo;
  int                   li_stablesize, li_variablesize;
  time_t                lt_currenttime;
  FILE                  *lf_tempfd,*fp;
  char                  ls_lockfname[PATH_MAX];
  time_t                lt_lasttime;
  time_t                lt_crashtime;
  char                  *new_file_name=NULL;
  int                   *tmp_key,n;
  char                  *ptr_key;
  int                   prefixLen;
  char                  *nameBuf,*tmp_ptr;

#ifdef HAVE_PTHREAD_H
  Thread_Info             *thread_info;
#endif

  assert(rs != NULL );
  prefixLen = strlen(rs->loc.dir);
  if ( NULL == (new_file_name = (char*)calloc(sizeof(char),4096))){
    fprintf(stderr,"Out of memory !\n");
    exit(-1);
  }
  time(&(lt_now));
  now = ctime(&(lt_now));
  now[24]='\0';
  fprintf(rs->logFd," %s:%d#%d  booted at %s\n",
                    glb.hostname,glb.IbpPort,rs->RID,now);
  fflush(rs->logFd);

  lockPidFile(rs);
  if ( IBP_OK != createSubDirs(rs) ){
      if ( new_file_name != NULL ) { free(new_file_name);}
    return (0);
  };

  if ((rs->rcvFd = fopen(rs->rcvFile,"r")) == NULL) {
    if ((rs->rcvFd= fopen(rs->rcvFile,"w")) == NULL) {
      fprintf(stderr," %d: LSS file <%s>  open error\n",rs->RID,rs->rcvFile);
    }
    fclose(rs->rcvFd);
    rs->rcvFd = NULL ;
      if ( new_file_name != NULL ) { free(new_file_name);}
    return(0);
  }
  fclose(rs->rcvFd);
  rs->rcvFd = NULL;

  sprintf(lc_oldlss,"%s%ld",rs->rcvFile ,time(NULL));
  if (link(rs->rcvFile,lc_oldlss) != 0) {
    fprintf(stderr," %d: recover error\n",rs->RID);
    unlink(rs->rcvFile);
      if ( new_file_name != NULL ) { free(new_file_name);}
    return(E_RECOVER);
  }
  unlink(rs->rcvFile);

  if ((rs->rcvFd  = fopen(rs->rcvFile ,"w")) == NULL) {
    fprintf(stderr," %d: LSS file <%s>  open error\n",rs->RID,rs->rcvFile);
    unlink(lc_oldlss);
      if ( new_file_name != NULL ) { free(new_file_name);}
    return(E_RECOVER);
  }

  if ((lf_oldlssfd = fopen(lc_oldlss,"r")) == NULL) {
    fprintf(stderr," %d: LSS file <%s>  open error\n",rs->RID, lc_oldlss);
    unlink(lc_oldlss);
    fclose(rs->rcvFd);
    rs->rcvFd = NULL ;
      if ( new_file_name != NULL ) { free(new_file_name);}
    return(E_RECOVER);
  }

  li_stablesize = sizeof(IBP_StableBackupInfo);
  li_variablesize = sizeof(IBP_VariableBackupInfo);
  lt_currenttime = time(NULL);

  if ((rs->rcvTimeFd  = fopen(rs->rcvTimeFile,"r")) == NULL) {
    lt_crashtime = rs->MaxDuration;
  }
  else {
    fscanf(rs->rcvTimeFd,"%u",&lt_lasttime);
    lt_crashtime = lt_currenttime - lt_lasttime;
    if ((lt_crashtime < 0) ) { /* || (lt_crashtime > rs->MaxDuration)) { */
       lt_crashtime = rs->MaxDuration;
    }
  }
  
  /* fclose(rs->rcvTimeFd); */ 
  rs->rcvTimeFd = NULL;
  rs->rcvOptTime  = lt_currenttime + glb.RecoverTime;

  while (1) {

    bzero(&ls_stableinfo,li_stablesize);
    bzero(&ls_variableinfo,li_variablesize);
    bzero(ls_lockfname,PATH_MAX);
    if (fread(&ls_stableinfo,li_stablesize,1,lf_oldlssfd) < 1) {
      break;
    }
    if (fread(&ls_variableinfo,li_variablesize,1,lf_oldlssfd) < 1) {
      fprintf(stderr," %d : LSS read error. recover abort\n",rs->RID);
      if ( new_file_name != NULL ) { free(new_file_name);}
      return(E_RECOVER);
    }
    /* if storage dir has been changed from last boot */
    if ( strncmp(ls_stableinfo.fname,rs->loc.dir,prefixLen) != 0 ){
        if ( (nameBuf = (char*)calloc(sizeof(char),4096)) == NULL ){
                fprintf(stderr,"Out of Memory!\n");
                exit(-1);
        }
        strncpy(nameBuf,ls_stableinfo.fname,4095);
        ptr_key = strrchr(nameBuf,'/');
        if ( ptr_key == NULL ) {
                free(nameBuf);
                continue;
        }
        sprintf(new_file_name,"%s%s",rs->loc.dir,(ptr_key+1));
        if ( (fp = fopen(new_file_name,"r")) == NULL ){
                tmp_ptr = ptr_key;
                *ptr_key='\0';
                ptr_key = strrchr(nameBuf,'/');
                if ( ptr_key != NULL ){
                        *tmp_ptr='/';
                        sprintf(new_file_name,"%s%s",rs->loc.dir,(ptr_key+1));
                        if ( (fp=fopen(new_file_name,"r")) == NULL ){
                                free(nameBuf);
                                continue;
                        }else{
                                fclose(fp);
                                strcpy(ls_stableinfo.fname,new_file_name);
                        }
                }else{
                        free(nameBuf);
                        continue;
                }
        }else{
                fclose(fp);
                strcpy(ls_stableinfo.fname,new_file_name);
        }
        free(nameBuf);
    }
    if (ls_variableinfo.valid != IBP_OK) {
      continue;
    }
    if (((ls_variableinfo.attrib.duration > 0) && ((ls_variableinfo.attrib.duration+lt_crashtime) < lt_currenttime)) || (glb.IBPRecover != 0)) {
      if (ls_variableinfo.valid != 1) {
        switch ( rs->type ){
        case DISK:
            unlink(ls_stableinfo.fname);
            sprintf(ls_lockfname,"%s-LOCK",ls_stableinfo.fname);
            unlink(ls_lockfname);
	    fprintf(stderr,"delete expired allocation: %s\n",ls_stableinfo.fname);
            break;
        case ULFS:
            ibpfs_delete(((FAT*)(rs->loc.ulfs.ptr)),ls_stableinfo.fname);
            break;
        default:
            break;
        }
        /*
        unlink(ls_stableinfo.fname);
        sprintf(ls_lockfname,"%s-LOCK",ls_stableinfo.fname);
        unlink(ls_lockfname);
        */
#ifdef IBP_DEBUG
        fprintf(stderr,"delete %s\n",ls_stableinfo.fname);
#endif
      }
      continue;
    }
    
    switch (rs->type ){
    case DISK: 
        if ( NULL != (ptr_key = strrchr(ls_stableinfo.fname,'/'))){
            ptr_key++;
            if ( 0 != strcmp(ptr_key,ls_stableinfo.key)){
		fprintf(stderr,"delete file key != filename %s\n",ls_stableinfo.fname);
                unlink(ls_stableinfo.fname);
                continue;
            }
        }else{
            unlink(ls_stableinfo.fname);
            continue;
        }
        if ((lf_tempfd = fopen(ls_stableinfo.fname,"r")) == NULL) {
            continue;
        }
        fclose(lf_tempfd);
        lf_tempfd = NULL;
        break;
    case ULFS:
        if ((lf_tempfd = (void*)ibpfs_open(((FAT*)(rs->loc.ulfs.ptr)),ls_stableinfo.fname)) == NULL ){
            continue;
        }
        ibpfs_close((IBPFILE*)lf_tempfd);
        break;
    default:
        break;
    }
    ls_container = (IBP_byteArray) calloc(1, sizeof(struct ibp_byteArray));
    if (ls_container == NULL) {
      handle_error (E_BOOT, NULL);
      free(ls_container);
      return(E_RECOVER);
    }

    ls_container->maxSize = ls_variableinfo.maxSize;
    ls_container->currentSize = ls_variableinfo.currentSize;
    ls_container->readRefCount = ls_variableinfo.readRefCount;
    ls_container->writeRefCount = ls_variableinfo.writeRefCount;
#if 0
    ls_container->writeKey = ls_stableinfo.writeKey;
    ls_container->readKey = ls_stableinfo.readKey;
    ls_container->manageKey = ls_stableinfo.manageKey;
#endif
    ls_container->deleted = 0;
    ls_container->reference = 0;
    ls_variableinfo.attrib.duration += lt_crashtime+glb.RecoverTime;
    if (ls_variableinfo.attrib.duration > lt_currenttime + rs->MaxDuration) {
      ls_variableinfo.attrib.duration = lt_currenttime + rs->MaxDuration;
    }
    ls_container->attrib.duration=ls_variableinfo.attrib.duration;
    ls_container->attrib.reliability = ls_variableinfo.attrib.reliability;
    ls_container->attrib.type = ls_variableinfo.attrib.type;
    ls_container->lock =ls_container->readLock =ls_container->writeLock = 0;
    ls_container->pending = make_dl();
    ls_container->key = strdup(ls_stableinfo.key);
    sprintf(tmp_buf,"%d",ls_stableinfo.readKey);
    ls_container->readKey_s = strdup(tmp_buf);
    ls_container->readKey = ls_stableinfo.readKey;
    sprintf(tmp_buf,"%d",ls_stableinfo.writeKey);
    ls_container->writeKey_s = strdup(tmp_buf);
    ls_container->writeKey = ls_stableinfo.writeKey;
    sprintf(tmp_buf,"%d",ls_stableinfo.manageKey);
    ls_container->manageKey_s = strdup(tmp_buf);
    ls_container->manageKey = ls_stableinfo.manageKey;
    if ( (ls_container->key == NULL) || ( ls_container->readKey_s == NULL ) ||
         (ls_container->writeKey_s == NULL) || ( ls_container->manageKey_s == NULL )){
      fprintf(stderr," Out of Memory\n");
      exit(-1);
    }
    /** relocate data file **/
    if ( strcmp("IBP-0",ls_container->key) != 0 ){
        tmp_key = (int*)(&(ls_container->key[4]));
        n=(*tmp_key) % rs->subdirs;
        sprintf(new_file_name,"%s%s%d/%s",rs->loc.dir,"data",n,ls_container->key);
        if ( strcmp(new_file_name,ls_stableinfo.fname) != 0 ){
            if ( 0 != rename(ls_stableinfo.fname,new_file_name) ){
                fprintf(stderr," unable to move file [%s] to %s !\n",ls_stableinfo.fname,new_file_name);
		perror("rename file");
                free(ls_container->key); free(ls_container->readKey_s);
                free(ls_container->writeKey_s); free(ls_container->manageKey_s);
                free(ls_container);
                unlink(ls_stableinfo.fname);
                ls_container = NULL;
                continue;
            }
        }
        ls_container->fname = strdup(new_file_name);
    }else{
        ls_container->fname = strdup(ls_stableinfo.fname);
    }

    if ( check_cap_file(rs,ls_container) != 0 ){
        free(ls_container->key); free(ls_container->readKey_s);
        free(ls_container->writeKey_s); free(ls_container->manageKey_s);
        free(ls_container);
        unlink(ls_stableinfo.fname);
        ls_container = NULL;
        continue;
    }
    ls_container->BackupIndex = rs->NextIndexNumber;
    rs->NextIndexNumber ++;
    ls_container->size = ls_variableinfo.size;
    ls_container->free = ls_variableinfo.free;

    ls_container->wrcount = ls_variableinfo.wrcount;
    ls_container->rdcount = ls_variableinfo.rdcount;
#if 0
    if ( ibpSaveCapInfo(ls_container) != IBP_OK){
        fprintf(stderr,"can save capability information on %s ! delete.\n",ls_container->fname);
        unlink(ls_container->fname);
        free(ls_container->key); free(ls_container->readKey_s);
        free(ls_container->writeKey_s); free(ls_container->manageKey_s);
        free(ls_container);
        ls_container = NULL;
        continue;
    }
#endif

    lu_val.s = (char *) ls_container;
    ls_container->masterTreeEntry= jrb_insert_str (rs->ByteArrayTree,
                                           strdup(ls_container->key),
                                           lu_val);

    if (ls_container->attrib.reliability == IBP_VOLATILE){
      rs->VolStorageUsed += ls_container->maxSize;
      ls_container->volTreeEntry = vol_cap_insert(rs,ls_container->maxSize,
                          ls_container->attrib.duration - lt_currenttime,
                          lu_val);
    }
    else if (ls_container->attrib.reliability == IBP_STABLE)
    {
      rs->StableStorageUsed += ls_container->maxSize;
    }

    if (ls_container->attrib.duration > 0)
      ls_container->transientTreeEntry = \
                jrb_insert_int(rs->TransientTree,ls_container->attrib.duration,lu_val);

    ls_container->thread_data = NULL;
#ifdef HAVE_PTHREAD_H
    thread_info = (Thread_Info *)malloc(sizeof(Thread_Info));
    if (thread_info == NULL) {
      fprintf(stderr, "No enough memory \n");
      exit(1);
    }
    bzero (thread_info, sizeof(Thread_Info));
    thread_info->read = -1;
    ls_container->thread_data = (void *) thread_info;
#endif
    SaveInfo2LSS(rs,ls_container,1);
#ifdef IBP_DEBUG
      fprintf(stderr,"recover %s\n",ls_stableinfo.fname);
#endif
  }

  fclose(lf_oldlssfd);
  unlink(lc_oldlss);
  fclose(rs->rcvFd);
  rs->rcvFd = NULL;

  check_size(rs, 0,4);

/*
  truncatestorage(1, (void *) &glb.VolStorage);
*/
  if ( new_file_name != NULL ){ free(new_file_name); }
  return 0;
}

void show_version_info(){
   fprintf(stdout,"\n\n  IBP Server, version %s %s\n",glb.versionMajor, glb.versionMinor);
   fprintf(stdout,"  Copyright (C) 2003 Logistical Computing and Internetworking LAB\n  University of Tennessee. \n");
}


/*****************************/
/* decodepar                 */
/* It reads the parameters from the command */
/* return: OK / not          */
/*****************************/
int decodepar (int argc, char **argv)
{
  int		      li_index, li_npar = argc - 1;
  int                 li_return = 0;
  long                ll_dur;

  if (li_npar == 0) {
    fprintf (stdout, "No parameters specified\n");
    return IBP_OK;
  }

  for (li_index = 1; li_index <= li_npar; li_index++) {
    if (strcmp(argv[li_index], "-help") == 0) {
      handle_error (E_USAGE, NULL);
      li_return = -1;
      break;
    }else if ( (strcmp(argv[li_index],"-V") == 0) ||
               (strcmp(argv[li_index],"--version") == 0 ) ){ 
        show_version_info();
        exit(0);
    }else if ((strcmp(argv[li_index], "-s") == 0) && (li_npar > li_index)) {
      if ( (glb.dftRs->StableStorage = ibp_getStrSize(argv[++li_index])) == 0 && errno != 0){
      	handle_error(E_CMDPAR,NULL);
	      li_return = -1;
	      break;
      }
    }  
    else if ((strcmp(argv[li_index], "-v") == 0) && (li_npar > li_index)) {
      if ((glb.dftRs->VolStorage = ibp_getStrSize(argv[++li_index])) == 0 && errno !=0){
      	handle_error(E_CMDPAR,NULL);
	      li_return = -1;
	      break;
      }
    }
    else if ((strcmp(argv[li_index],"--log") == 0 ) && (li_npar>= li_index)){
        glb.enableLog = 1;
    }
    else if ((strcmp(argv[li_index], "-m") == 0) && (li_npar > li_index)) {
      if ((glb.dftRs->MinFreeSize = ibp_getStrSize(argv[++li_index])) == 0 && errno !=0){
      	handle_error(E_CMDPAR,NULL);
	      li_return = -1;
	      break;
      }
    }
    else if ((strcmp(argv[li_index], "-rt") == 0) && (li_npar > li_index)) {
      if ((glb.RecoverTime=atoi(argv[++li_index])) == 0 && errno!=0)
      {
        handle_error(E_CMDPAR,NULL);
        li_return = -1;
        break;
      }
    }
    else if ((strcmp(argv[li_index],"--nfu-file") == 0) 
             && (li_npar > li_index)){
        if ((glb.nfu_ConfigFile = strdup(argv[++li_index])) == NULL ){
            handle_error(E_ALLOC,NULL);
            li_return = -1;
            break;
        }
    }
    else if ((strcmp(argv[li_index],"--enableauthen")) == 0 ){
        glb.enableAuthen = 1; 
    }
    else if ( (strcmp(argv[li_index],"--ca-file") == 0)){
        if ((glb.cafile = strdup(argv[++li_index])) == NULL ){
            handle_error(E_ALLOC,NULL);
            li_return = -1;
            break;
        }
    } 
    else if ((strcmp(argv[li_index],"--privatekeypasswd") == 0)){
       if ( (glb.depotPrivateKeyPasswd = strdup(argv[++li_index])) == NULL ){
            handle_error(E_ALLOC,NULL);
            li_return = -1;
            break;
       }
    }
    else if( ((strcmp(argv[li_index], "-dv") == 0) && (li_npar > li_index)) || 
             ((strcmp(argv[li_index], "-ds") == 0) && (li_npar > li_index)) ||
             ((strcmp(argv[li_index], "--loc") == 0 ) && ( li_npar > li_index))) {
        switch ( glb.dftRs->type ){
            case DISK:
                glb.dftRs->loc.dir = strdup (argv[++li_index] );
                if ( glb.dftRs->loc.dir == NULL ){
	                handle_error (E_ALLOC, NULL);
	                return -1;
                }
                break;
           case MEMORY:
                break;
           case RMT_DEPOT:
                if ( getRsLoc(glb.dftRs,argv[++li_index]) != IBP_OK ){
                    exit(-1);
                }
                break;
           default:
                fprintf(stderr," ERROR: No resource type is specified for '-dv' option !\n");
                exit (-1);
        }
    }
#if 0
    else if ((strcmp(argv[li_index], "-ds") == 0) && (li_npar > li_index)) {
      glb.StableDir = strdup (argv[++li_index]);
      if (glb.StableDir == NULL) {
	handle_error (E_ALLOC, NULL);
	return -1;
      }
    }
    else if ((strcmp(argv[li_index],"--rid") == 0) && (li_npar > li_index)){
      strncpy(glb.dftRs->RID,argv[++li_index],MAX_RID_LEN+1);    
    }
#endif
    else if((strcmp(argv[li_index],"--rtype") == 0) && ( li_npar > li_index)){
      if ( strlen(argv[++li_index]) > 1 ){
          fprintf(stderr," ERROR: Unsupported resource type: %s !\n",argv[li_index]);
          exit(-1);
      }
      switch ( argv[li_index][0] ) {
        case 'd':
          glb.dftRs->type = DISK;
          break;
        case 'm':
          glb.dftRs->type = MEMORY;
          break;
#ifdef SUPPORT_RMT_DEPOT
        case 'r':
          glb.dftRs->type = RMT_DEPOT;
          break;
#endif
        default:
          fprintf(stderr," ERROR: Unsupported resource type: %s !\n",argv[li_index]);
          exit(-1);
      }
    }
    else if ((strcmp(argv[li_index], "-l") == 0) && (li_npar > li_index)) {
      ll_dur = atoi(argv[++li_index]);
      if (ll_dur > 0) 
	    ll_dur *= (24 * 3600);
      glb.dftRs->MaxDuration = ll_dur;
    }
    else if ((strcmp(argv[li_index], "-pw") == 0) && (li_npar > li_index)) {
      strncpy (gc_passwd, argv[++li_index], 16);
      gc_passwd[16] = '\0';
    }
    else if ((strcmp(argv[li_index], "-cf") == 0) && (li_npar > li_index)) {
      if (gc_cfgpath[0] != '\0') {
        fprintf(stderr, "Multiple config file path define %s\n",gc_cfgpath);
        fprintf(stderr, "Multiple config file path define %s\n",gc_cfgpath);
        return -1;
      }

      strncpy (gc_cfgpath, argv[++li_index], PATH_MAX);
      if (gc_cfgpath[strlen(gc_cfgpath)-1] != '/')
        strcat(gc_cfgpath,"/");
    }
    else if ((strcmp(argv[li_index], "-c") == 0) && (li_npar > li_index)) {
      if (gc_cfgname[0] != '\0') {
        fprintf(stderr, "Multiple config file name define\n");
        return -1;
      }

      strncpy (gc_cfgname, argv[++li_index], PATH_MAX);
    }

    else if ((strcmp(argv[li_index], "-hn") == 0) && (li_npar > li_index)) {
      glb.hostname = strdup(argv[++li_index]);
      if ( glb.hostname == NULL ){
	 handle_error(E_ALLOC,NULL);
	 return -1;
      }
    }
    else if ((strcmp(argv[li_index], "-p") == 0) && (li_npar > li_index)) {
      glb.IbpPort = atoi(argv[++li_index]);
    }
    else if ((strcmp(argv[li_index],"-nr") == 0) && (li_npar >= li_index)){
      glb.IBPRecover = 1;
    }
#ifdef HAVE_PTHREAD_H
    else if ((strcmp(argv[li_index],"-tn") == 0) && (li_npar > li_index)){
      glb.IbpThreadMin = atoi(argv[++li_index]);
      glb.IbpThreadNum = atoi(argv[li_index]);  
    }
    
    else if ((strcmp(argv[li_index],"-tm") == 0) && (li_npar > li_index)){
      if(atoi(argv[++li_index]) > MAX_THREADS){
	glb.IbpThreadMax = MAX_THREADS;
      }
      else{
	glb.IbpThreadMax = atoi(argv[li_index]);
      }  
    }
    
    else if ((strcmp(argv[li_index],"-tl") == 0) && (li_npar >= li_index)){
      glb.IbpThreadLogEnable = 1;
    }
#endif
    else {
      handle_error (E_USAGE, NULL);
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

#if 0
  DIR         *ls_dir;

  /* check hard / soft / free storage size */
  if (glb.StableStorage < 0) {
    fprintf(stderr, "Stable/Hard Storage size config error\n");
    return(-1);
  }
  if (glb.VolStorage < 0) {
    fprintf(stderr, "Volatile/Soft Storage size config error\n");
    return(-1);
  }
  if (glb.VolStorage < glb.StableStorage) {
    fprintf(stderr,"Volatile/Soft Storage should be no less than Stable/Hard Storage\n");
    return(-1);
  }
  if (glb.MinFreeSize < 0) {
    fprintf(stderr, "Minimal Free Storage size config error\n");
    return(-1);
  }

  /* check max duration */
  if (glb.MaxDuration < -1) {
    fprintf(stderr, "Max Duration config error\n");
    return(-1);
  }

  /* check the directories */
  if (glb.StableDir != NULL) {
    if ((ls_dir = opendir(glb.StableDir)) == NULL) {
      fprintf(stderr, "Stable/Hard directory config error\n");
      return(-1);
    }
    closedir(ls_dir);
  } else {
    fprintf(stderr, "Stable/Hard directory config error\n");
    return(-1);
  }

  if (glb.VolDir != NULL) {
    if ((ls_dir = opendir(glb.VolDir)) == NULL) {
      fprintf(stderr, "Volatile/Soft directory config error\n");
      return(-1);
    }
    closedir(ls_dir);
  } else {
    fprintf(stderr, "Volatile/Soft directory config error\n");
    return(-1);
  }
#endif

  /* check the port # */
  if ((glb.IbpPort < 1024) || (glb.IbpPort > 65535)) {
    fprintf(stderr, "IBP Port config error: 1024-65535\n");
    return(-1);
  }

#ifdef HAVE_PTHREAD_H
  /* check the thread # */
  if (glb.IbpThreadNum > MAX_THREADS) {
    fprintf(stderr, "IBP Thread Number config error: %d\n",MAX_THREADS);
    return(-1);
  }
#endif

  return(0); 
}

/*****************************/
/* init_ibp_ping             */
/* It inserts the root into the ByteArrayTree */
/* return: none              */
/*****************************/
void init_ibp_ping() 
{

  JRB                 ls_rb,node;
  char                *lc_Key;
  RESOURCE            *rs;

  /*
   * Prepare the IBP-0 file
   */ 

  lc_Key = malloc (MAX_KEY_LEN);
  if (lc_Key == NULL) {
    fprintf (stderr, "IBP-0 retrieval failed. \n"); 
    return;
  }
  sprintf(lc_Key, "IBP-0");

  jrb_traverse(node,glb.resources){
    rs = (RESOURCE *)((jrb_val(node)).v);
    if ( rs->type == ULFS ) { continue; }
    ls_rb = jrb_find_str(rs->ByteArrayTree,lc_Key);
  
    if (ls_rb == NULL) {
#ifdef HAVE_PTHREAD_H
      pthread_mutex_lock(&(rs->alloc_lock));
#endif
      handle_allocate (rs,0, 0, IBP_HALL_TYPE_PING);
#ifdef HAVE_PTHREAD_H
      pthread_mutex_unlock(&(rs->alloc_lock));
#endif
    }
  }

  free (lc_Key);
  return;
}

void setDataMoverTypes ( globals *gl ) {

  dl_insert_b(gl->dataMovers,(void *)DM_TCP);
  dl_insert_b(gl->dataMovers,(void *)DM_RUDP);
  dl_insert_b(gl->dataMovers,(void *)DM_UUDP);

  return;
}

static void ibpKillMDNS(void)
{
    if (glb.mdnspid > 0){
        kill(glb.mdnspid,SIGINT);
    }
}

/*****************************/
/* boot                      */
/* This function is used to boot the IBP depot */
/* return: OK / not          */
/*****************************/
int boot(int argc, char **argv)
{
  int 		  li_retval;
#ifdef HAVE_GETRLIMIT
  struct rlimit   ls_limit;
#endif
  char		  *lc_generic, lc_path[PATH_MAX];
  time_t	  lt_Now;
  char        *cp;
  char        dots[]="....................."; 
  JRB        node;
  RESOURCE   *rs;

  /*
   * Initialize a few fields
   */

  memset(&glb, 0, sizeof(globals));
  glb.versionMajor = IBP_VER_MAJOR;
  glb.versionMinor = IBP_VER_MINOR;
#if 0
  glb.VolStorage = 0;
  glb.StableStorage = 0;
  glb.MinFreeSize = IBP_FREESIZE;
  glb.StableDir = NULL;
  glb.VolDir = NULL;
  glb.MaxDuration = 0;
#endif
  glb.IbpPort = 0;
  glb.IBPRecover = 0;
  glb.enableAlloc = 1;
  glb.RecoverTime = IBP_RECOVER_TIME;
  glb.resources = make_jrb();
  if( (glb.dftRs = crtResource()) == NULL ){
      handle_error(E_ALLOC,NULL);
      exit(-1);
  }
  glb.dftRs->RID = 0;
  glb.dataMovers = make_dl();
  gc_passwd = malloc (17);
  glb.cafile = NULL;
  glb.enableAuthen = 0;
  glb.depotCertFile = NULL;
  glb.depotPrivateKeyFile = NULL;
  glb.depotPrivateKeyPasswd = NULL;
  glb.enableLog = 0;
  glb.maxLogFileSize = 10*1024*1024;
  glb.queue_len = IBP_QUEUE_LEN_DEFAULT;
  glb.idle_time = IBP_CONN_MAX_IDLE_TIME;
  if (gc_passwd == NULL) {
    handle_error (E_ALLOC, NULL);
    exit(-1);
  }
  strcpy (gc_passwd, "ibp");

    glb.nfu_ops = make_jrb();
    glb.nfu_libs = make_jrb();
    glb.nfu_ConfigFile = NULL;

    glb.sys_page_size = getpagesize();

  setDataMoverTypes( &glb );
  

  /*
   * get path of the executable 
   */
  if ( (glb.execPath = strdup(argv[0])) == NULL ){
      handle_error(E_ALLOC,NULL);
      exit(-1);
  }
  cp = strrchr(glb.execPath,'/');
  if ( cp == NULL ){
      glb.execPath[0] = '\0' ;
  }else {
      cp[1]='\0';
  }

#ifdef HAVE_PTHREAD_H
  glb.IbpThreadNum = -1;
  glb.IbpThreadMin = -1;
  glb.IbpThreadMax = -1;
  glb.IbpThreadLogEnable = 0; 
  pthread_mutex_init(&(glb.keyLock),NULL);
  pthread_mutex_init(&(glb.glbLock),NULL);
#endif

  gi_recoverclosed = 1;  
  gc_cfgpath = calloc (sizeof(char), PATH_MAX);
  gc_cfgname = calloc (sizeof(char), PATH_MAX);
  if ((gc_cfgpath == NULL) || (gc_cfgname == NULL)) {
    handle_error (E_ALLOC, NULL);
    exit(-1);
  }

  /*
   * Check if calling parameters are correct
   */ 

  fprintf(stderr,"  Parsing parameter\t\t\t%s ",dots);
  li_retval = decodepar (argc, argv);
  if (li_retval == -1) {
    return E_DECODEPAR;
  }
  fprintf(stderr,"Done\n");

  if ((gc_cfgpath[0] == '\0') && ( gc_cfgname[0] == '\0')) {
    strcpy (gc_cfgpath, "/home/yong/projects/ibp-1.4.0.alpha/local/etc/");
  }
  if (gc_cfgname[0] == '\0') {
    strcpy (gc_cfgname, "ibp.cfg");
  }

  /*
   * Set the soft limit of open file to the maximum possible (the hard one)
   */
#ifdef HAVE_GETRLIMIT 
  if (getrlimit(RLIMIT_NOFILE, &ls_limit) == -1) {
    handle_error (E_GLIMIT, NULL);
    return E_BOOT;
  }
  ls_limit.rlim_cur = ls_limit.rlim_max;
  if (setrlimit(RLIMIT_NOFILE, &ls_limit) == -1) {
    handle_error (E_SLIMIT, NULL);
    return E_BOOT;
  }
#endif

  /*
   * initialize a few fileds
   */
#if 0
  glb.ByteArrayTree = make_jrb(); 
  glb.VolatileTree = make_jrb();
  glb.TransientTree = make_jrb();
#endif  
  time(&lt_Now);
  srandom ((unsigned int) lt_Now);


  /*
   * call getconfig, to fill up the parameters needed 
   */

  sprintf(lc_path,"%s%s", gc_cfgpath, gc_cfgname);
#ifdef IBP_DEBUG
  fprintf(stderr, "cfg file: %s\n",lc_path);
#endif
  fprintf(stderr,"  Reading configuration\t\t\t%s ",dots);
  li_retval = getconfig (lc_path);

  if (li_retval < 0) {
    return E_GETCONFIG;
  }

#if IBP_ENABLE_BONJOUR
  glb.mdnspid = ibpStartMDNS(glb.resources);
  if ( glb.mdnspid > 0 ){
    atexit(ibpKillMDNS); 
  }
#endif

  /*fprintf(stderr,"IMPDE after getconfig \n");*/
  
  /*
   * get infos about hostmane, home directory, etc etc ...
   */
  if ( glb.hostname == NULL ){
     /*
      * only search for hostname when can't get from config file or command line
      */
     lc_generic = (char *) getlocalhostname();
     if ((lc_generic != NULL) && (strcasecmp(lc_generic, "localhost") != 0)){
         glb.hostname = strdup(lc_generic);
	 if ( glb.hostname == NULL ){
	    handle_error(E_ALLOC,NULL);
	    return E_ALLOC;
	 }
     }
  }
  if ( glb.hostname == NULL ) {
	  handle_error(E_FQDN,NULL);
	  return(-1);
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
  fprintf(stderr,"Done\n");

  /*
   * reading nfu configuration
   */
  fprintf(stderr,"  Loading network function units\t%s ",dots);
  if ( load_nfu(glb.nfu_libs,glb.nfu_ops,glb.nfu_ConfigFile) != IBP_OK ){
      fprintf(stderr," Failed\n");
      fprintf(stderr," Unable to load network function units\n");
      exit(-1);
  }
  fprintf(stderr,"Done\n");

  /*fprintf(stderr,"IMPDE after boundarycheck()\n");*/

#ifdef HAVE_PTHREAD_H
  if (glb.IbpThreadNum <= 0)
    glb.IbpThreadNum = NB_COMM_THR_MIN;
  if (glb.IbpThreadMin <= 0)
    glb.IbpThreadMin = NB_COMM_THR_MIN;
  if (glb.IbpThreadMax <= 0)
    glb.IbpThreadMax = NB_COMM_THR_MAX;
  if ((IBPErrorNb = (int*)calloc(sizeof(int),glb.IbpThreadMax)) == NULL){
	  handle_error(E_ALLOC,NULL);
	  return(-1);
  } 
#else
  if ((IBPErrorNb = (int*)calloc(sizeof(int),1)) == NULL){
	  handle_error(E_ALLOC,NULL);
	  return(-1);
  }
#endif

  /* check ca certificate file */
#ifdef IBP_AUTHENTICATION
    if ( glb.enableAuthen ){
        if ( glb.cafile == NULL ){
            fprintf(stderr,"Can't find CA certificate file !\n");
            handle_error(E_ALLOC,NULL);
            return(-1);
        }
        ibpCreateSrvSSLCTX(glb.cafile);
    }
    if ( glb.depotCertFile != NULL && glb.depotPrivateKeyFile != NULL && 
         glb.depotPrivateKeyPasswd == NULL ){
        glb.depotPrivateKeyPasswd = strdup(getpass("Please input the password for depot certificate's private key file:"));
    }
    if ( glb.depotCertFile != NULL ){
        if ( IBP_setAuthenAttribute(glb.depotCertFile,glb.depotPrivateKeyFile,glb.depotPrivateKeyPasswd) != IBP_OK ){
            fprintf(stderr,"Setup depot certificate failed!\n");
            handle_error(E_ALLOC,NULL);
            return(-1);
        }
    }
#else 
    if ( glb.enableAuthen ){
        fprintf(stderr,"The depot doesn't support client authentication!\n");
        handle_error(E_ALLOC,NULL);
        return(-1);
    }
#endif
 
  /*queue=create_queue();*/

/*
  if (( glbServerConnList = IBP_createConnList(0)) == NULL ){
    handle_error(E_ALLOC,NULL);
    return(-1);
  };
*/
  IBP_createPipe();

  /*fprintf(stderr,"IMPDE before printGLB \n");*/
  /*printGLB(stderr);*/

#if 0 
  fprintf(stderr, "Parameters used:\n");
  fprintf(stderr, "\tHostname:\t\t\t%s\n", glb.hostname);
  fprintf(stderr, "\tPort Used:\t\t\t%d\n", glb.IbpPort);
  fprintf(stderr, "\tHard/Stable Storage size:\t%s\n", ibp_ultostr(glb.StableStorage,tmp+49,10));
  fprintf(stderr, "\tSoft/Volatile Storage size:\t%s\n", ibp_ultostr(glb.VolStorage,tmp+49,10));
  fprintf(stderr, "\tMinimal Free size:\t\t%s\n", ibp_ultostr(glb.MinFreeSize,tmp+49,10));
  fprintf(stderr, "\tStable Storage Dir:\t\t%s\n", glb.StableDir);
  fprintf(stderr, "\tVolatile Storage Dir:\t\t%s\n", glb.VolDir);
  fprintf(stderr, "\tMax Duration:\t\t\t%ld\n", glb.MaxDuration);
#endif

  /*
   * open the log file
   */

  gc_logfile = malloc(PATH_MAX);
  if (gc_cfgpath[strlen(gc_cfgpath)-1] != '/')
    sprintf(gc_logfile,"%s/ibp.log", gc_cfgpath);
  else
    sprintf(gc_logfile,"%sibp.log", gc_cfgpath);

  gs_logQueue = init_ibp_log_queue(gc_logfile,glb.maxLogFileSize);
  if (gs_logQueue == NULL) {
    handle_error(E_OFILE, (void *) gc_logfile);
    return E_BOOT;
  }
#if 0
  gc_recoverfile = malloc(PATH_MAX);
  gc_recovertimefile = malloc(PATH_MAX);
  if (gc_cfgpath[strlen(gc_cfgpath)-1] != '/')
  {
    sprintf(gc_recoverfile,"%s/ibp.lss", gc_cfgpath);
    sprintf(gc_recovertimefile,"%s/ibp.time", gc_cfgpath);
  }
  else
  {
    sprintf(gc_recoverfile,"%sibp.lss", gc_cfgpath);
    sprintf(gc_recovertimefile,"%s/ibp.time", gc_cfgpath);
  }

  lc_Now = ctime (&lt_Now);
  lc_Now[24] = '\0';
  fprintf (gf_logfd, "%s,  booted %s\n",glb.hostname, lc_Now);
  fflush (gf_logfd);

#endif

  /*
   * init the IBP-0 for ping
   */

  init_ibp_ping();

  /*
   * check if recover necessary
   */
  jrb_traverse(node,glb.resources ){
    rs = (RESOURCE *)((jrb_val(node)).v);
    fprintf(stderr,"  Recovering the resource <%d>\t\t%s ",rs->RID,dots); 
    switch ( rs->type ){
      case DISK:
        li_retval = recover(rs);
        if (li_retval < 0) {
          return E_RECOVER;
        }
        li_retval = check_unmanaged_capability(rs);
        if (li_retval < 0) {
          return E_RECOVER;
        }
        break;
      case MEMORY:
      case RMT_DEPOT:
      default:
        break;
    }
    fprintf(stderr,"Done\n");
  }

#if 0 
  jrb_traverse(node,glb.resources){
    rs = (RESOURCE *)((jrb_val(node)).v);
    switch ( rs->type ){
      case DISK:
        li_retval = check_unmanaged_capability();
        if (li_retval < 0) {
          return E_RECOVER;
        }
        break;
      case MEMORY:
      case RMT_DEPOT:
      default:
        break;
    }
  }
  fprintf(stderr,"Done\n");
#endif
  
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

  Listen (glb.IbpSocket);

#ifdef IBP_DEBUG
  printGLB(stderr);
#endif
  /*fprintf(fp,"%sVersion\t: %s  %s\n",pad,glb.versionMajor,glb.versionMinor);*/
  fprintf(stderr,"\n  IBP server  %s %s  is ready to accept requests now . \n",
                 glb.versionMajor, glb.versionMinor);

  return IBP_OK;
}

/*
IBP_connectList *IBP_createConnList( int size ) 
{
  int sz,i;
  IBP_connectList *list = NULL;
  
  if ( size <= 0 || size > FD_SETSIZE-2 ){
    sz = FD_SETSIZE-2; 
  }else{
    sz = size;
  }

  if ( (list = calloc(1,sizeof(IBP_connectList))) == NULL ){
    return (NULL);
  };

  if ( (list->list = calloc(sz,sizeof(IBP_connect))) == NULL ){
    free(list);
    return(NULL);
  };

  if ( (list->lock = calloc(1,sizeof(pthread_mutex_t))) == NULL ){
    free(list->list);
    free(list);
    return(NULL);
  }

  pthread_mutex_init(list->lock,NULL);
  
  for ( i = 0 ; i < sz; i ++){
    list->list[i].status = DIS_CONN;
    list->list[i].fd = -1;
    list->list[i].authenticated = 0;
    list->list[i].cn = NULL;
  };
  
  list->len = sz;
  return (list); 
}
*/

int IBP_createPipe()
{
  int fds[2];
  int val;

  if ( 0 != pipe(fds) ){
    perror("Can't create pipe:");
    exit(-1);
  }; 

  glbRdPipe = fds[0];
  glbWrtPipe = fds[1];

  val = fcntl(glbRdPipe,F_GETFL,0);
  if ( val == -1 ){
    perror("Can't get pipe information:");
    exit(-1);
  }  

  if ( -1 == fcntl(glbRdPipe,F_SETFL,val|O_NONBLOCK) ){
    perror("Can't set nonblock pipe:");
    exit(-1);
  }
  
  return 0; 
}

/*
void IBP_lockConnList( IBP_connectList *list)
{
  pthread_mutex_lock(list->lock);
}

void IBP_unlockConnList( IBP_connectList *list)
{
  pthread_mutex_unlock(list->lock);
}

void IBP_closeFD( IBP_connectList *connList, int fd )
{
   IBP_connect *conn;

   IBP_lockConnList(connList);
   if ( (conn = IBP_searchConn(connList,fd)) != NULL ){
     assert(conn->fd == fd);
     close(fd);
     conn->fd = -1;
     conn->status = DIS_CONN;
     conn->authenticated = 0;
     if ( conn->cn != NULL ){
        free(conn->cn);
        conn->cn = NULL;
     }
   } ;
   IBP_unlockConnList(connList);  
}

IBP_connect *IBP_searchConn(IBP_connectList *connList, int fd)
{
   int i;
   IBP_connect *conn = NULL;

   for ( i = 0 ; i < connList->len ; i++ ){
     if ( connList->list[i].status != DIS_CONN ){
       if ( connList->list[i].fd == fd ){
         conn = &(connList->list[i]);
         break;
       }
     }
   }
  
   return conn; 
}

IBP_connect *IBP_insertConn(IBP_connectList *connList, int fd)
{
  IBP_connect *conn=NULL;
  int idx = -1,i;
  time_t earliest=0x3fffffff;

  for ( i=0; i < connList->len ; i++){
    if ( connList->list[i].status == DIS_CONN ){
      conn = &(connList->list[i]);
      break;
    }
    if ( connList->list[i].status == IDLE ){
      if ( idx == -1 ){
        earliest = connList->list[i].idleTime;
        idx = i;
      }else if ( earliest > connList->list[i].idleTime){
        earliest = connList->list[i].idleTime;
        idx = i;
      }
    }
  };
  if ( conn == NULL && idx != -1 ){
    conn = &(connList->list[i]);
    close(conn->fd);
    conn->fd = -1;
    conn->status = DIS_CONN;
    conn->authenticated = 0;
    if ( conn->cn != NULL ){
        free(conn->cn);
        conn->cn = NULL;
     }
  };

  if ( conn != NULL ){
    conn->fd = fd;
    conn->status = IN_USE;
  }

  return conn;

}

void IBP_closeIdleConn( IBP_connectList *connList )
{
  int i;
  
  IBP_lockConnList(connList);
  for ( i = 0 ; i < connList->len ; i++ ){
    if ( connList->list[i].status == IDLE ){
      close(connList->list[i].fd);
      connList->list[i].fd = -1;
      connList->list[i].status = DIS_CONN;
      connList->list[i].authenticated = 0;
      if ( connList->list[i].cn != NULL ){
            free(connList->list[i].cn);
            connList->list[i].cn = NULL;
      }
    }
  }
  IBP_unlockConnList(connList);
}

int IBP_setFDSet( fd_set *set ,  IBP_connectList *connList )
{
  int maxfd = -1, i;
  
  FD_ZERO(set);
  FD_SET(glb.IbpSocket,set);
  if ( maxfd < glb.IbpSocket ) { maxfd = glb.IbpSocket ; }
  FD_SET(glbRdPipe,set);
  if ( maxfd < glbRdPipe) { maxfd = glbRdPipe; }
  
  for ( i = 0 ; i < connList->len ; i ++ ){
    if ( connList->list[i].status != IDLE ){
      continue;
    }
    assert(connList->list[i].fd >= 0 );
    FD_SET(connList->list[i].fd,set);
    if ( maxfd < connList->list[i].fd ) { maxfd = connList->list[i].fd; }
  } 
  
  return maxfd; 
} 
*/
