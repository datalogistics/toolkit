/*
 * ibp_ct_copy.c 
 * 
 * Functions used by cut through copy 
 *
 */
# include "config-ibp.h"
# include "ibp_server.h"
# include "ibp_resources.h"
# include "ibp_thread.h"
# ifdef STDC_HEADERS
# include <assert.h>
# endif
# include "fs.h"

static int _waitDataBA( IBP_byteArray       cap,
                        long int            offset,
                        long int            size,
                        int                 expireTime){
  int               ret = IBP_OK;
  Thread_Info       *info;
  struct timespec   spec;
  int               reval;

  info = (Thread_Info *)cap->thread_data;
  info->read_wait ++;
  if ( info->read != -1 ){
    pthread_mutex_lock(&filock);
    if ( expireTime > 0 ) { 
      spec.tv_sec = expireTime;
      spec.tv_nsec = 0;
      reval = pthread_cond_timedwait(&(info->read_protect),&filock,&spec);
    }else{
      reval = pthread_cond_wait(&(info->read_protect),&filock);
    }

   if ( reval != 0 ){
      pthread_mutex_unlock(&filock);
      if ( reval == ETIMEDOUT ){
        ret = IBP_E_SERVER_TIMEOUT;
      }else {
        ret = IBP_E_INTERNAL;
      }
      info->read_wait --;
      goto bail;
    }else{
      pthread_mutex_unlock(&filock); 
    }
  }
  if ( cap->currentSize < offset + size ) {
    info->read = offset + size;
    pthread_mutex_lock(&filock);
    if ( expireTime > 0 ){
      spec.tv_sec = expireTime;
      spec.tv_nsec = 0;
      reval =  pthread_cond_timedwait(&(info->read_cond),&filock,&spec);
    }else{
      reval = pthread_cond_wait(&(info->read_cond),&filock);
    }
    if ( reval != 0 ){
      info->read = -1;
      info->read_wait --;
      pthread_mutex_unlock(&filock);
      if ( reval == ETIMEDOUT ){ 
        ret = IBP_E_SERVER_TIMEOUT;
      }else{
        ret = IBP_E_INTERNAL;
      }
      if ( info->read_wait > 0 ){
        pthread_cond_signal(&(info->read_protect));
      }
    }else{
      info->read = -1;
      info->read_wait --;
      pthread_mutex_unlock(&filock);
      if ( info->read_wait > 0 ){
        pthread_cond_signal(&(info->read_protect));
      }
    }
  }

bail:

  return (ret);

}

static int _waitDataFI( IBP_byteArray     cap,
                        long int          offset,
                        long int          size,
                        int               expireTime){
    int             ret = IBP_OK;
    Thread_Info     *info;
    struct timespec spec;
    int             reval;

    info = (Thread_Info *)cap->thread_data;
    info->read = size;
    pthread_mutex_lock(&filock);
    if ( expireTime > 0 ){
      spec.tv_sec = expireTime;
      spec.tv_nsec = 0;
      reval = pthread_cond_timedwait(&(info->read_cond),&filock,&spec);
    }else{
      reval = pthread_cond_wait(&(info->read_cond),&filock);
    }
    if ( reval != 0 ){
      info->read = -1;
      pthread_mutex_unlock(&filock);
      if ( reval == ETIMEDOUT ){
        ret = IBP_E_SERVER_TIMEOUT;
      }else{
        ret = IBP_E_INTERNAL;
      }
      goto bail;
    }else{
      info->read = -1;
      pthread_mutex_unlock(&filock);
    }

bail:

    return (ret );

}




static int _checkReadCapSpace ( IBP_byteArray     cap,
                                long int          offset,
                                long int          size,
                                int               expireTime ){
  int             ret = IBP_OK;

  assert( cap != NULL && size > 0 && expireTime >= 0 );

  cap->readLock ++;

  /*
   * wait for more data if necessary  
   */ 
  switch ( cap->attrib.type ){
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
      if (( size > 0) && ( cap->currentSize <  offset + size )) {
        if ( (ret = _waitDataBA(cap,offset,size,expireTime )) != IBP_OK ){
          goto bail;
        }
      }
      break;
    case IBP_FIFO:
    case IBP_CIRQ:
      if ( (size > 0 ) && (size > cap->size - cap->free )) {
        if ( ( ret = _waitDataFI(cap,offset,size,expireTime)) != IBP_OK ){
          goto bail;
        }
      }
      break;
  }

  
  return ( IBP_OK );

bail:

  
  cap->readLock--;
  return ( ret );
}

static int _checkWriteCapSpace (  IBP_byteArray   cap,
                                  long int        offset,
                                  long int        size,
                                  int             expireTime){
  
    int             ret = IBP_OK;
    int             reval;
    Thread_Info     *info;
    struct timespec spec;

    /*
     * acquire  write lock 
     */ 

    cap->writeLock ++;

    /*
     * set write lock 
     *
    cap->writeLock ++;
    */

    /*
     * check boundary 
     */
    switch ( cap->attrib.type ){
      case IBP_BYTEARRAY:
        if ( size > cap->maxSize - cap->currentSize ){
          ret = IBP_E_WOULD_EXCEED_LIMIT;
          goto bail;
        }
        break;
      case IBP_BUFFER:
        if ( size > cap->maxSize ){
          ret = IBP_E_WOULD_EXCEED_LIMIT;
          goto bail;
        }
        break;
      case IBP_CIRQ:
        if ( cap->size < size ){
          ret = IBP_E_WOULD_EXCEED_LIMIT;
          goto bail;
        }
      default:
        break;
    }

    /*
     * wait for more data if necessary 
     */
    switch ( cap->attrib.type ){
      case IBP_FIFO:
        if ( cap->free < size ){
          info = (Thread_Info *)cap->thread_data;
          info->write = size;
          pthread_mutex_lock(&filock);
          if ( expireTime > 0 ){
            spec.tv_sec = expireTime;
            spec.tv_nsec = 0;
            reval = pthread_cond_timedwait(&(info->write_cond),&filock,&spec);
          }else {
            reval = pthread_cond_wait(&(info->write_cond),&filock);
          }
          if ( reval != 0 ){
            info->write = 0;
            pthread_mutex_unlock(&filock);
            if ( reval == ETIMEDOUT ){
              ret = IBP_E_SERVER_TIMEOUT;
            }else{
              ret = IBP_E_INTERNAL;
            }
            goto bail;
          }else{
            info->write = 0;
            pthread_mutex_unlock(&filock);
          }
        }
        break;
      default:
        break;
    }


    return (IBP_OK);

bail:
    
    cap->writeLock --;
    return (ret);


}

static int _ulfsToRam ( IBPFILE      *fd,
                        char      *buf,
                        long int  readPtr,
                        long int  size,
                        long int  wrtPtr,
                        long int  *retSz,
                        long int  headLen){ 
  int   ret = IBP_OK;
  char  *ptr;
  int   nread = 0,left,n;
  u_int64_t offset;
  assert(fd != NULL && readPtr >= 0 && size > 0 && wrtPtr >=0 &&
         retSz != NULL && headLen >= 0 && buf != NULL);

  /*
  if ( fseek(fd,headLen+readPtr,SEEK_SET) == -1 ){
    return ( IBP_E_FILE_ACCESS );
  }
  */
  offset = headLen + readPtr;

  ptr = buf + wrtPtr;
  left = size;
  while ( nread < size ){
  /*fprintf(stderr,"offset = %lld size = %d\n",offset,size);*/
   n = ibpfs_load(fd,ptr,left,offset);
   if ( n < 0 ){
     ret = IBP_E_FILE_READ;
     goto bail;
   }
   offset += n;
   ptr = ptr+n;
   nread += n;
   left = size - nread;
  }

  *retSz = nread;

  return (IBP_OK);

bail:

  *retSz = 0;
  return (ret);

}
static int _diskToRam ( FILE      *fd,
                        char      *buf,
                        long int  readPtr,
                        long int  size,
                        long int  wrtPtr,
                        long int  *retSz,
                        long int  headLen){ 
  int   ret = IBP_OK;
  char  *ptr;
  int   nread = 0,left,n;

  assert(fd != NULL && readPtr >= 0 && size > 0 && wrtPtr >=0 &&
         retSz != NULL && headLen >= 0 && buf != NULL);

  if ( fseek(fd,headLen+readPtr,SEEK_SET) == -1 ){
    return ( IBP_E_FILE_ACCESS );
  }

  ptr = buf + wrtPtr;
  left = size;
  while ( nread < size ){
   n = fread(ptr,1,left,fd);
   if ( n == 0 ){
     ret = IBP_E_FILE_READ;
     goto bail;
   }
   ptr = ptr+n;
   nread += n;
   left = size - nread;
  }

  *retSz = nread;

  return (IBP_OK);

bail:

  *retSz = 0;
  return (ret);

}

static int _ramToRam( char      *srcBuf,
                      char      *dstBuf,
                      long int  readPtr,
                      long int  size,
                      long int  wrtPtr,
                      long int  *retSz ){
  char  *srcPtr, *dstPtr;

  srcPtr = (char*)(srcBuf + readPtr);
  dstPtr = (char*)(dstBuf + wrtPtr);

  memcpy(dstPtr,srcPtr,size);

  *retSz = size;

  return (IBP_OK);

}

static int _ramToULFS ( char    *srcBuf,
                        IBPFILE    *dstFd,
                        long int  readPtr,
                        long int  size,
                        long int  wrtPtr,
                        int       headLen,
                        long int  *retSz){
  int   nwritten = 0,n,left;
  char  *ptr;
  u_int64_t offset;

  /*
  if ( fseek(dstFd,headLen + wrtPtr,SEEK_SET) == -1 ){
    return ( IBP_E_FILE_ACCESS );
  }
  */
  offset = headLen + wrtPtr;

  ptr = srcBuf + readPtr;
  left = size;
  while ( left > 0 ){
    n = ibpfs_store(dstFd,ptr,left,offset);
    if ( n < 0 ){
      *retSz = 0;
      return ( IBP_E_FILE_WRITE);
    }
    offset += n;
    nwritten += n;
    left = size - left;
    ptr += n;
  }

  *retSz = size;
  return ( IBP_OK);

}
static int _ramToDisk ( char    *srcBuf,
                        FILE    *dstFd,
                        long int  readPtr,
                        long int  size,
                        long int  wrtPtr,
                        int       headLen,
                        long int  *retSz){
  int   nwritten = 0,n,left;
  char  *ptr;

  if ( fseek(dstFd,headLen + wrtPtr,SEEK_SET) == -1 ){
    return ( IBP_E_FILE_ACCESS );
  }

  ptr = srcBuf + readPtr;
  left = size;
  while ( left > 0 ){
    n = fwrite(ptr,1,left,dstFd);
    if ( n < 0 ){
      *retSz = 0;
      return ( IBP_E_FILE_WRITE);
    }
    nwritten += n;
    left = size - left;
    ptr += n;
  }

  *retSz = size;
  return ( IBP_OK);

}


void handle_ct_copy( int sockfd, int nbt, IBP_byteArray srcCap ){
  IBP_byteArray dstCap = NULL;
  RESOURCE      *srcRs,*dstRs;
  int           ret;
  time_t        expireTime;
  char          ansBuf[100];
  int           flagSrc, flagDst;
  long int      avaiSz,readPtr,writePtr;
  long int      wrcnt, rdcnt, cfFree,cfSz;
  FILE          *srcfd = NULL, *dstfd = NULL;
  char          *buf = NULL;
  int           srcHeadLen, dstHeadLen;
  int           rdLen, wrtLen;
  int           rdOff, wrtOff;
  long int      retSz,nW,mW;
  int           rdLeft, toWrt;
  Thread_Info   *info;
  FAT           *fat;

  assert( ibp_cmd[nbt].url.rs != NULL );
  assert( ibp_cmd[nbt].url.ba != NULL );

  dstRs = ibp_cmd[nbt].url.rs;
  srcRs = ibp_cmd[nbt].rs;
  flagSrc = IBP_E_GENERIC;
  flagDst = IBP_E_GENERIC;

  /*
   * set up expire time
   */ 
  if ( ibp_cmd[nbt].ServerSync > 0 ){
    expireTime = time(0) + ibp_cmd[nbt].ServerSync;
  }else{
    expireTime = 0;
  }

  /* 
   * check destination capability 
   */
  dstCap = ibp_cmd[nbt].url.ba;
  if ( dstCap->deleted == 1 ){
    ret = IBP_E_CAP_NOT_FOUND;
    goto bail;
  }

  /*
   * check write key
   */ 
  if (strcmp(dstCap->writeKey_s, ibp_cmd[nbt].url.attr_key) != 0){
    ret = IBP_E_INVALID_WRITE_CAP;
    goto bail;
  }

  /*
   * check source capability capacity
   */
  if ( ( flagSrc = _checkReadCapSpace(srcCap,ibp_cmd[nbt].Offset,ibp_cmd[nbt].size,expireTime)) != IBP_OK){
    ret = flagSrc;
    goto bail;
  }

  /*
   * check destination capability capacity 
   *
  if ( ( flagDst = _checkWriteCapSpace(dstCap,0,ibp_cmd[nbt].size,expirtTime)) != IBP_OK ){
    ret = flagDst;
    goto bail;
  }
  */

  /*
   * calculate  available data size 
   */
  switch ( srcCap->attrib.type ){
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
      if ( ibp_cmd[nbt].size > 0 ){ 
        if ( srcCap->currentSize <= ibp_cmd[nbt].Offset + ibp_cmd[nbt].size ){
          if ( srcCap->currentSize > ibp_cmd[nbt].Offset ){
            avaiSz = srcCap->currentSize - ibp_cmd[nbt].Offset;
          }else{
            avaiSz = 0;
          }
        }else{
          avaiSz = ibp_cmd[nbt].size ;
        }
      }else {
        if ( srcCap->currentSize < ibp_cmd[nbt].Offset ) {
          avaiSz = 0;
        }else{
          avaiSz = srcCap->currentSize - ibp_cmd[nbt].Offset;
        }
      }
      break;
    case IBP_FIFO:
    case IBP_CIRQ:
      wrcnt = srcCap->wrcount;
      rdcnt = srcCap->rdcount;
      cfFree = srcCap->free;
      cfSz = srcCap->size;

      if ( ibp_cmd[nbt].size > 0 ) {
        if ( ibp_cmd[nbt].size <= ( cfSz - cfFree)){
          avaiSz = ibp_cmd[nbt].size;
        }else{
          avaiSz = cfSz - cfFree;
        }
      }else{
        avaiSz = cfSz - cfFree;
      }
      break;
  }

  if ( avaiSz <= 0 ){
    goto end;
  }

  /*
   * check destination capability capacity 
   */
  if ( ( flagDst = _checkWriteCapSpace(dstCap,0,avaiSz,expireTime)) != IBP_OK ){
    ret = flagDst;
    goto bail;
  }

  /*
   * calculate start offset of source capability
   */
  switch ( srcCap->attrib.type ) {
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
      readPtr = ibp_cmd[nbt].Offset;
      break;
    case IBP_FIFO:
    case IBP_CIRQ:
      readPtr = rdcnt;
      break;
  }

  /*
   * calculate start offset of destination capability
   */ 
  switch ( srcCap->attrib.type ){
    case IBP_BYTEARRAY:
      writePtr = dstCap->currentSize;
      break;
    case IBP_BUFFER:
      writePtr = 0;
      break;
    case IBP_CIRQ:
      writePtr = dstCap->wrcount;
      break;
    case IBP_FIFO:
      writePtr = dstCap->wrcount;
      break;
  }

  /*
   * open files ( disk type resource) or  buffers ( ram type resource ) 
   */ 
  
  /*
   * soruce cap 
   */
  switch ( srcRs->type ){
    case DISK:
      if ( (srcfd = fopen(srcCap->fname,"r")) == NULL ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
      }
      if ( fread(&srcHeadLen,sizeof(int),1,srcfd) == 0 ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
      }
      assert( srcHeadLen >= 0 );
      break;
    case ULFS:
      fat = (FAT*)(srcRs->loc.ulfs.ptr);
      if ((srcfd = (FILE*)ibpfs_open(fat,srcCap->fname)) == NULL ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
      }
      if ( ibpfs_load((IBPFILE*)srcfd,&srcHeadLen,sizeof(int),0) < 0 ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
      }
      srcHeadLen = (srcHeadLen/fat->blksize + 1)*fat->blksize;
    case MEMORY:
#if 0
      buf = srcCap->startArea;
      assert(buf != NULL );
#endif
      break;
    default:
      break;
  }

  /*
   * destination cap 
   */
  switch ( dstRs->type ){
    case DISK:
      if ((dstfd = fopen(dstCap->fname,"r+")) == NULL ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
      }
      if ( fread(&dstHeadLen,sizeof(int),1,dstfd) == 0 ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
      }
      assert ( dstHeadLen >= 0 );
      /*
      if ( srcRs->type == DISK ){
        if ( (buf = (char*)calloc(DATA_BUF_SIZE,sizeof(char))) == NULL ){
          ret = IBP_E_ALLOC_FAILED;
          goto bail;
        }
      }
      */
      break;
   case ULFS:
     fat = (FAT *)(dstRs->loc.ulfs.ptr);
     if ((dstfd = (FILE*)ibpfs_open(fat,dstCap->fname)) == NULL){
        ret = IBP_E_FILE_OPEN;
        goto bail;
     }
     if ( ibpfs_load((IBPFILE*)dstfd,&dstHeadLen,sizeof(int),0) < 0 ){
        ret = IBP_E_FILE_OPEN;
        goto bail;
     }
     dstHeadLen = (dstHeadLen/fat->blksize + 1) * fat->blksize;
     break;
   default:
     break;
  }

  /*
   * transfer data
   */
  if ( srcRs->type == DISK && dstRs->type == MEMORY &&
       (srcCap->attrib.type == IBP_BYTEARRAY || srcCap->attrib.type == IBP_BUFFER ) &&
       (dstCap->attrib.type == IBP_BYTEARRAY || dstCap->attrib.type == IBP_BUFFER )){
    /*
     * short cut for disk to memory copy 
     */ 
    if ((ret =  _diskToRam(srcfd,dstCap->startArea,readPtr,avaiSz,writePtr,&retSz,srcHeadLen)) != IBP_OK ){
      goto bail;
    }
  }else if ( srcRs->type == MEMORY && dstRs->type == MEMORY &&
         (srcCap->attrib.type == IBP_BYTEARRAY || srcCap->attrib.type == IBP_BUFFER) &&
         (dstCap->attrib.type == IBP_BYTEARRAY || dstCap->attrib.type == IBP_BUFFER)){
    /*
     * short cut for memory to memory copy 
     */ 
    if ((ret = _ramToRam(srcCap->startArea,dstCap->startArea,readPtr,avaiSz,writePtr,&retSz )) != IBP_OK ){
      goto bail;
    }
  }else if ( srcRs->type == MEMORY && dstRs->type == DISK &&
         (srcCap->attrib.type == IBP_BYTEARRAY || srcCap->attrib.type == IBP_BUFFER) &&
         (dstCap->attrib.type == IBP_BYTEARRAY || dstCap->attrib.type == IBP_BUFFER )){
    /*
     * short cut for memory to disk copy
     */ 
    if ((ret = _ramToDisk(srcCap->startArea,dstfd,readPtr,avaiSz,writePtr,dstHeadLen,&retSz)) != IBP_OK ){
      goto bail;
    }
  }else {
    /* 
     * general case
     */ 
    if ( (buf = (char*)calloc(DATA_BUF_SIZE,sizeof(char))) == NULL ){
      ret = IBP_E_ALLOC_FAILED;
      goto bail;
    }
    wrtLen = 0 ;
    rdLen = 0;
    rdLeft = avaiSz;
    while ( wrtLen < avaiSz ){
      if ( srcRs->type == MEMORY && 
          ((srcCap->attrib.type == IBP_BYTEARRAY) || (srcCap->attrib.type == IBP_BUFFER)) ){
        if ( rdLeft > DATA_BUF_SIZE ){
          rdLen = DATA_BUF_SIZE;
        }else{
          rdLen = rdLeft;
        }
        rdOff = readPtr + avaiSz - rdLeft;
        if ( (ret = _ramToRam(srcCap->startArea,buf,rdOff,rdLen,0,&retSz)) != IBP_OK ){
          goto bail;
        }
      }else if ( srcRs->type == DISK && 
                ((srcCap->attrib.type == IBP_BYTEARRAY) || ( srcCap->attrib.type == IBP_BUFFER))){
        if ( rdLeft > DATA_BUF_SIZE ){
          rdLen = DATA_BUF_SIZE;
        }else{
          rdLen = rdLeft;
        }
        rdOff = readPtr + avaiSz -rdLeft;
        if ( ( ret = _diskToRam(srcfd,buf,rdOff,rdLen,0,&retSz,srcHeadLen )) != IBP_OK ){
          goto bail;
        }
      }else if ( srcRs->type == MEMORY && 
                ( (srcCap->attrib.type == IBP_FIFO) || (srcCap->attrib.type == IBP_CIRQ))){
        rdOff = (readPtr + avaiSz - rdLeft) % srcCap->maxSize;
        rdLen = IBP_MIN(DATA_BUF_SIZE,rdLeft);
        rdLen = IBP_MIN(rdLen,(srcCap->maxSize - (avaiSz -rdLeft)));
        rdLen = IBP_MIN(rdLen,(srcCap->maxSize - rdOff));
        if ( (ret = _ramToRam(srcCap->startArea,buf,rdOff,rdLen,0,&retSz )) != IBP_OK ){
          goto bail;
        }
      }else if ( srcRs->type == DISK && 
               ( (srcCap->attrib.type == IBP_FIFO) || (srcCap->attrib.type == IBP_CIRQ))){
        rdOff = (readPtr + avaiSz - rdLeft ) % srcCap->maxSize ;
        rdLen = IBP_MIN(DATA_BUF_SIZE,rdLeft);
        rdLen = IBP_MIN(rdLen,(srcCap->maxSize-(avaiSz - rdLeft)));
        rdLen = IBP_MIN(rdLen,(srcCap->maxSize-rdOff));
        if ( (ret = _diskToRam(srcfd,buf,rdOff,rdLen,0,&retSz,srcHeadLen)) != IBP_OK ){
          goto bail;
        }
      }else if ( srcRs->type == ULFS ){
        if  (rdLeft > DATA_BUF_SIZE){
            rdLen = DATA_BUF_SIZE;
        }else{
            rdLen = rdLeft;
        }
        rdOff = readPtr + avaiSz - rdLeft;
        if ( (ret = _ulfsToRam((IBPFILE*)srcfd,buf,rdOff,rdLen,0,&retSz,srcHeadLen)) != IBP_OK ){
            goto bail;
        }
      }
      rdLeft = rdLeft - retSz;
      
      /*
       * write to destination cap 
       */
      toWrt = retSz;
      assert(toWrt > 0 );

      if ( dstRs->type == MEMORY &&
          ((dstCap->attrib.type == IBP_BYTEARRAY ) || (dstCap->attrib.type == IBP_BUFFER))){
        wrtOff = writePtr + wrtLen ;
        if ( (ret = _ramToRam(buf,dstCap->startArea,0,toWrt,wrtOff,&retSz)) != IBP_OK ){
          goto bail;
        }
      }else if ( dstRs->type == DISK &&
               ((dstCap->attrib.type == IBP_BYTEARRAY) || (dstCap->attrib.type == IBP_BUFFER))){
        wrtOff = writePtr + wrtLen;
        if ((ret = _ramToDisk(buf,dstfd,0,toWrt,wrtOff,dstHeadLen,&retSz)) != IBP_OK ){
          goto bail;
        }
      }else if ( dstRs->type == MEMORY &&
               ((dstCap->attrib.type == IBP_FIFO) || (dstCap->attrib.type == IBP_CIRQ))){
        nW = 0;
        wrtOff = (writePtr + wrtLen ) % dstCap->maxSize;
        while ( nW < toWrt ){
          mW = IBP_MIN( (toWrt - nW), (dstCap->maxSize - wrtOff));
          if ((ret = _ramToRam(buf,dstCap->startArea,nW,mW,wrtOff,&retSz)) != IBP_OK){
            goto bail;
          }
          assert( retSz == mW );
          nW += mW;
          wrtOff = (wrtOff + mW) % dstCap->maxSize ;
        }
        retSz = toWrt;
      }else if ( dstRs->type == DISK &&
               ((dstCap->attrib.type == IBP_FIFO) || (dstCap->attrib.type == IBP_CIRQ))){
        nW = 0;
        wrtOff = ( writePtr + wrtLen) % dstCap->maxSize;
        while ( nW < toWrt){
          mW = IBP_MIN( ( toWrt - nW ), (dstCap->maxSize - wrtOff));
          if (( ret = _ramToDisk(buf,dstfd,nW,mW,wrtOff,dstHeadLen,&retSz)) != IBP_OK){
            goto bail;
          }
          assert( retSz == mW );
          nW += mW;
          wrtOff = ( wrtOff + mW ) % dstCap->maxSize;
        }
        retSz = toWrt;
      }else if ( dstRs->type == ULFS ){
        wrtOff = writePtr + wrtLen;
        if ( (ret = _ramToULFS(buf,(IBPFILE*)dstfd,0,toWrt,wrtOff,dstHeadLen,&retSz)) != IBP_OK ){
            goto bail;
        }
      }

      assert(retSz == toWrt);
      wrtLen += retSz;
    }
    
  }

end:

  /*
   *  save source cap state
   */
  switch ( srcCap->attrib.type){
    case IBP_BYTEARRAY:
    case IBP_BUFFER:
      SaveInfo2LSS(srcRs,srcCap,2);
      break;
    case IBP_FIFO:
      srcCap->rdcount = ( srcCap->rdcount + avaiSz ) % srcCap->maxSize;
      srcCap->free  = srcCap->free + avaiSz;
      srcCap->wrcount = ( srcCap->wrcount + avaiSz ) % srcCap->maxSize;
      SaveInfo2LSS(srcRs,srcCap,2);

      info = (Thread_Info*)srcCap->thread_data;
      if ( ( info->write > 0 ) && ( srcCap->free >= info->write)){
        pthread_cond_signal(&(info->write_cond));
      }
      break;
    case IBP_CIRQ:
      srcCap->rdcount = ( srcCap->rdcount + avaiSz ) % srcCap->maxSize;
      srcCap->free  = srcCap->free + avaiSz;
      srcCap->wrcount = ( srcCap->wrcount + avaiSz ) % srcCap->maxSize;
      SaveInfo2LSS(srcRs,srcCap,2);
      break;
    default:
      break; 
  }

  /*
   * save destination cap state
   */
  info = (Thread_Info *)dstCap->thread_data;
  switch ( dstCap->attrib.type ){
    case IBP_BYTEARRAY:
      dstCap->currentSize += avaiSz;
      if ( (info->read > -1 ) && ( dstCap->currentSize >= info->read)){
        pthread_cond_signal(&(info->read_cond));
      }
      break;
    case IBP_BUFFER:
      dstCap->currentSize = avaiSz;
      if ( ( info->read > -1 ) && ( dstCap->currentSize >= info->read)){
        pthread_cond_signal(&(info->read_cond));
      }
      break;
    case IBP_FIFO:
      dstCap->wrcount = ( dstCap->wrcount + avaiSz ) % dstCap->maxSize;
      dstCap->rdcount = dstCap->rdcount;
      dstCap->free = dstCap->free - avaiSz ;
      if ((info->read > -1 ) && ( dstCap->maxSize - dstCap->free >= info->read )){
        pthread_cond_signal(&(info->read_cond));
      }
      break;
  }

  SaveInfo2LSS(dstRs,dstCap,2);

  /*
   * clean up source cap 
   */
  switch( srcRs->type ){
    case DISK:
      assert(srcfd != NULL  );
      fclose(srcfd);
      break;
    case MEMORY:
      break;
    default:
      break;
  }
  srcCap->readLock --;

  /*
   * clean up destination cap
   */
  switch ( dstRs->type ){
    case DISK:
      assert(dstfd != NULL);
      fseek(dstfd,sizeof(int),SEEK_SET);
      fwrite(dstCap,1,BASE_HEADER,dstfd);
      fflush(dstfd);
      fclose(dstfd);
      break;
    default:
      break;
  }

  dstCap->writeLock --;

  if ( buf != NULL ) { free (buf ); }
  /*
   * send status code back to client
   */
  sprintf(ansBuf,"%d %lu \n",IBP_OK,avaiSz);
  Write(sockfd,ansBuf,strlen(ansBuf));

  return ;


  /*
   * handle error 
   */ 
bail:
  SaveInfo2LSS(srcRs,srcCap,2);
  SaveInfo2LSS(dstRs,dstCap,2);
  if ( flagSrc == IBP_OK ){
    srcCap->readLock --;
#if 0
    if  ( srcCap->deleted == 1 ){
      if ( (srcCap->writeLock == 0 ) && ( srcCap->readLock == 0)){
        jettisonByteArray(srcRs,srcCap);
        srcCap == NULL ;
      }
    }
    if ( srcCap->attrib.type != IBP_BYTEARRAY  && srcCap != NULL ){
      pthread_mutex_lock(&rlock);
      srcCap->rl = 0;
      pthread_mutex_unlock(&rlock);
    }
#endif
  }
  if ( flagDst == IBP_OK ){
    dstCap->writeLock --;
  }
  switch(srcRs->type){
  case DISK:
    if ( srcfd != NULL ) { fclose ( srcfd ); }
    break;
  case ULFS:
    if ( srcfd != NULL) { ibpfs_close((IBPFILE*)srcfd); }
    break;
  default:
    break;
  }
  switch(dstRs->type){
  case DISK:
    if ( dstfd != NULL ) { fclose ( dstfd ); }
    break;
  case ULFS:
    if (dstfd != NULL ) { ibpfs_close((IBPFILE*)dstfd); }
    break;
  default:
    break;
  }
  if ( buf != NULL ) { free(buf); }
 
  /*
   * send status code back to client
   */
  sprintf(ansBuf,"%d %d \n",ret,0);
  Write(sockfd,ansBuf,strlen(ansBuf));
  IBPErrorNb[nbt] = ret;
  return;

}

