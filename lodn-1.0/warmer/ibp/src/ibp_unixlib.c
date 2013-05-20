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
# include <unistd.h>
# ifdef HAVE_PTHREAD_H
# include <pthread.h>
# endif
# ifdef STDC_HEADERS
# include <sys/types.h>
# include <assert.h> 
# endif
# ifdef HAVE_DIRENT_H
# include <dirent.h>
# endif
# include "ibp_unixlib.h"
# include "fat.h"
# include "fs.h"

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
#if 0
int SaveInfo2LSS(IBP_byteArray ps_container, int pi_mode){
    return 0;
}
#endif
int SaveInfo2LSS( RESOURCE *rs,IBP_byteArray ps_container, int pi_mode)
{

  IBP_StableBackupInfo ls_fixinfo;
  IBP_VariableBackupInfo ls_variableinfo;

  if ( rs->type != DISK && rs->type != ULFS ) {
      return (IBP_OK);
  }

  if (ps_container->BackupIndex == -1) {
    fprintf(stderr,"LSS record error. error ignore\n");
    return(-1);
  }

  if (pi_mode == IBP_FULLINFO) {
    strcpy(ls_fixinfo.key, ps_container->key);
#if 0    
    strcpy(ls_fixinfo.rKey,ps_container->readKey_s);
    strcpy(ls_fixinfo.wKey,ps_container->writeKey_s);
    strcpy(ls_fixinfo.mKey,ps_container->manageKey_s);
#endif
    strcpy(ls_fixinfo.fname, ps_container->fname);
    ls_fixinfo.readKey = atoi(ps_container->readKey_s);
    ls_fixinfo.writeKey = atoi(ps_container->writeKey_s);
    ls_fixinfo.manageKey = atoi(ps_container->manageKey_s);
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
    memcpy(&(ls_variableinfo.attrib), &(ps_container->attrib), sizeof(struct ibp_attributes));

#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(&(rs->recov_lock));
#endif
    if (rs->rcvFd == NULL) {
      if ((rs->rcvFd = fopen(rs->rcvFile,"r+")) == NULL) {
#ifdef IBP_DEBUG
        fprintf(stderr,"LSS file open error. error ignore\n");
#endif
#ifdef HAVE_PTHREAD_H
        pthread_mutex_unlock(&(rs->recov_lock));
#endif
        ps_container->BackupIndex = -1;
        return(-1);
      }
    }  
    fseek(rs->rcvFd, 0, SEEK_END);
    fwrite(&ls_fixinfo, sizeof(IBP_StableBackupInfo), 1, rs->rcvFd);
    fwrite(&ls_variableinfo, sizeof(IBP_VariableBackupInfo), 1, rs->rcvFd);
    fflush(rs->rcvFd);

#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&(rs->recov_lock));
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
    memcpy(&(ls_variableinfo.attrib), &(ps_container->attrib), sizeof(struct ibp_attributes)); 
#ifdef HAVE_PTHREAD_H
    pthread_mutex_lock(&(rs->recov_lock));
#endif
    if (rs->rcvFd == NULL) {
      if ((rs->rcvFd = fopen(rs->rcvFile,"r+")) == NULL) {
#ifdef IBP_DEBUG
        fprintf(stderr,"LSS file open error. error ignore\n");
#endif
#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&(rs->recov_lock));
#endif
        ps_container->BackupIndex = -1;
        return(-1);
      }
    }
    fseek(rs->rcvFd, (ps_container->BackupIndex*sizeof(IBP_StableBackupInfo) + (ps_container->BackupIndex - 1)*sizeof(IBP_VariableBackupInfo)), SEEK_SET);
    fwrite(&ls_variableinfo, sizeof(IBP_VariableBackupInfo), 1, rs->rcvFd);
    fflush(rs->rcvFd);
#ifdef HAVE_PTHREAD_H
    pthread_mutex_unlock(&(rs->recov_lock));
#endif
  }

  return(IBP_OK);
}

/*****************************/
/* check_stale_capability    */
/* It checkes the unmanaged capabilities and deletes them  */
/* return: success 0, failure -1       */
/*****************************/
#if 0
int check_unmanaged_capability(){
  return 0;
}
#endif

void collectAllocation( RESOURCE *rs, char *filename, char *key, char *topDir)
{       
    char  newName[PATH_MAX];
    int   *keyPtr;
    int   i;    FILE  *fp;
    IBP_FILE_HEADER header;
    struct stat     sb;
    struct ibp_byteArray *ba = NULL;
    char  tmp[50];
    Jval    val;
    int     size;
#ifdef HAVE_PTHREAD_H
    Thread_Info *thread_info;
#endif
  
    if ( (fp = fopen(filename,"r+")) == NULL ){
        fprintf(stderr,"can't open allocation %s, delete!\n",filename);
        goto bail;
    }
    if ( fread(&header,sizeof(IBP_FILE_HEADER),1,fp) < 1 ){
        fprintf(stderr,"can't read from %s !\n",filename);
        fclose(fp);
        goto bail;
    }
  
    if ( (header.delete != 1 && header.delete != 0 && header.delete != 2) ||
         (header.attr.type != IBP_BYTEARRAY) || 
         (header.attr.reliability != IBP_STABLE && header.attr.reliability != IBP_VOLATILE)){
        fprintf(stderr,"unknown allocation formation: %s !\n",filename);
        fclose(fp);
        goto bail;
    }

    if ( (ba = (struct ibp_byteArray*)calloc(1,sizeof(struct ibp_byteArray))) == NULL ){
        fprintf(stderr,"out of memory !\n");
        exit(-1);
    }

    if ( 0 != fstat(fileno(fp),&sb) ){
        fprintf(stderr,"Can't get size of %s\n",filename);
        fclose(fp);
        goto bail;
    }
    size = sb.st_size - header.headerLen;
    if ( size != header.curSize ){
        header.curSize = size;
        if ( header.curSize > header.maxSize ){
            header.maxSize = header.curSize;
        }
        header.rCount = 1;
        header.wCount = 1;
        if ( header.attr.duration > 0){
            header.attr.duration = time(0) + rs->MaxDuration;
        }
        
        /* write back to cap file */
        if ( 0 != fseek(fp,0L,SEEK_SET) ){
            fprintf(stderr,"Can't roll back to beginning of %s!\n",filename);
            fclose(fp);
            goto bail;
        }
        if ( fwrite(&header,sizeof(IBP_FILE_HEADER),1,fp) != 1 ){
            fprintf(stderr,"Can't write header info to %s!\n",filename);
            fclose(fp);
            goto bail;
        }
    }
    ba->maxSize = header.maxSize;
    ba->currentSize = header.curSize;
    ba->readRefCount = header.rCount;
    ba->writeRefCount = header.wCount;
    ba->deleted = 0;
    ba->reference = 0;
    ba->attrib.duration = header.attr.duration;
    if ( ba->attrib.duration > 0 ){
        ba->attrib.duration = time(0) + rs->MaxDuration;
    }
    ba->attrib.reliability = header.attr.reliability;
    ba->attrib.type = header.attr.type;
    ba->lock = 0;
    ba->readLock = 0;
    ba->writeLock = 0;
    ba->pending = make_dl();
    ba->key = strdup(key);
    sprintf(tmp,"%d",header.rKey);
    ba->readKey_s = strdup(tmp);
    ba->readKey = header.rKey;
    sprintf(tmp,"%d",header.wKey);
    ba->writeKey_s = strdup(tmp);
    ba->writeKey = header.wKey;
    sprintf(tmp,"%d",header.mKey);
    ba->manageKey_s = strdup(tmp);
    ba->manageKey = header.mKey;
    if ( ba->key == NULL ||
         ba->readKey_s == NULL ||
         ba->writeKey_s == NULL ||
         ba->manageKey_s == NULL ){
        fprintf(stderr,"Out of memory !\n");
        exit(-1);
    }

    keyPtr = (int *)(&(key[4]));
    i = (*keyPtr) % rs->subdirs;
    sprintf(newName,"%s%s%d/%s",topDir,"data",i,key);
    if ( strcmp(newName,filename) != 0 ){
        if ( 0 != rename(filename,newName) ){
            fprintf(stderr,"unable to move file [%s] to [%s] !\n",filename,newName);
            free(ba->key); free(ba->readKey_s); free(ba->writeKey_s);
            free(ba->manageKey_s);
            free(ba);
            fclose(fp);
            goto bail;
        }
        ba->fname = strdup(newName);
    }else{
        ba->fname = strdup(filename);
    }

    ba->BackupIndex = rs->NextIndexNumber;
    rs->NextIndexNumber ++;
    ba->size = header.maxSize;
    ba->free = header.maxSize - header.curSize;
    ba->wrcount  = 1;
    ba->rdcount =  1;

    val.s = (char*)ba;

    ba->masterTreeEntry = jrb_insert_str(rs->ByteArrayTree,strdup(ba->key),val);
    if ( ba->attrib.reliability == IBP_VOLATILE ){
        rs->VolStorageUsed += ba->maxSize;
        ba->volTreeEntry = vol_cap_insert(rs,ba->maxSize,ba->attrib.duration - time(0), val);
    }else if ( ba->attrib.reliability == IBP_STABLE){
        rs->StableStorageUsed += ba->maxSize;
    }

    if ( ba->attrib.duration > 0 ){
        ba->transientTreeEntry = jrb_insert_int(rs->TransientTree,ba->attrib.duration,val);
    }

    ba->thread_data = NULL;
#ifdef HAVE_PTHREAD_H
    thread_info = (Thread_Info *)malloc(sizeof(Thread_Info));
    if ( thread_info == NULL ){
        fprintf(stderr,"Out of Memory !\n");
        exit(-1);
    }
    bzero(thread_info,sizeof(Thread_Info));
    thread_info->read = -1;
    ba->thread_data = (void*)thread_info;
#endif
    SaveInfo2LSS(rs,ba,1);
    fclose(fp);

    return;
bail:
    unlink(filename);
    return;
}


int check_unmanaged_cap_ulfs(RESOURCE *rs)
{
    FAT *fat;
    int  i;
    JRB ls_node;

    assert(rs->type == ULFS);

    fat = (FAT*)(rs->loc.ulfs.ptr);
    for ( i = 0 ; i < fat->size; i++ ){
       if ( fat->records[i].filename[0] == '\0' ) { continue; }
       ls_node = jrb_find_str(rs->ByteArrayTree,fat->records[i].filename);
       if (ls_node == NULL ){
           ibpfs_delete(fat,fat->records[i].filename);
        }
    }
}

int check_unmanaged_capability( RESOURCE *rs )
{
  int i = 0;
  int skip;
  DIR *ld_dir;
  struct dirent *ls_dp;
  char lc_path[PATH_MAX], lc_dirpath[PATH_MAX];
  JRB ls_node;

  assert( rs != NULL );
  if ( rs->type == ULFS ) { return check_unmanaged_cap_ulfs(rs); }
  if ( rs->type != DISK ) { return 0; }
  for (i=0; i<=rs->subdirs; i++)
  {
    skip = 0;
    if ( i == rs->subdirs ){
        sprintf(lc_dirpath,"%s", rs->loc.dir);
    }else {
        sprintf(lc_dirpath,"%s%s%d/",rs->loc.dir,"data",i);
    }
    ld_dir = opendir(lc_dirpath);
    if (ld_dir == NULL) {
        fprintf(stderr," Can't open storage dir <%s>!\n",rs->loc.dir);
        continue;
    }

    while (ld_dir) {
      errno = 0;
      lc_path[0] = '\0';
      if ((ls_dp = readdir(ld_dir)) != NULL) {
        if ((strcmp(ls_dp->d_name,".") == 0) || \
                (strcmp(ls_dp->d_name,"..") == 0)) {
          continue;
        }
        if ( strncmp(ls_dp->d_name,"IBP-",4) != 0 ){
          continue;
        }
        if ( strcmp(ls_dp->d_name,"IBP-0") == 0 ){
            continue;
        }
        sprintf(lc_path,"%s%s", lc_dirpath, ls_dp->d_name);
        ls_node = jrb_find_str(rs->ByteArrayTree,ls_dp->d_name);
        if (ls_node == NULL) {
          collectAllocation(rs,lc_path,ls_dp->d_name,rs->loc.dir);
        }
      }
      else {
        closedir(ld_dir);
        ld_dir = NULL;
      }
    }
  }
  return(0);
}
  

      

