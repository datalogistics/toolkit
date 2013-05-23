
/************************************************************************
 * Description:  contains all the checksum and encryption functions     *
 * over files and blocks                                                *
 ************************************************************************/

#include <zlib.h>
#include <zconf.h>
#include "global.h"
#include "md5.h"
#include "des.h"
#include "aes.h"

#include "libend2end.h"

double totalt;
double s1;


/* ---------------------------------------------------------------
   return the hex string as a string
   --------------------------------------------------------------- */
char *hash2hex(unsigned char *hashstr) {

    int         i;
    char        *hexstr=(char*)malloc(sizeof(char)*33);

    bzero(hexstr, sizeof(hexstr));

    for (i=0; i<16; i++) {
     sprintf(hexstr+2*i, "%02x", hashstr[i]);
    }
    hexstr[32]='\0';
    return hexstr;
}

/* ------------------------------------------------------------
   calculates MD5 checksum of a BLOCK_SIZE block
   ------------------------------------------------------------ */
char* CalculateChecksum(unsigned char *ablock, int size)
{
  int lenMD5;
  MD5_CTX context;
  char checksum[CH_SIZE],*hexChecksum;

  lenMD5 = size;
  MD5Init(&context);
  MD5Update(&context, ablock, lenMD5);
  MD5Final(checksum, &context);

  hexChecksum = (char *)hash2hex(checksum);

  return hexChecksum;
}


/* --------------------------------------------------------------------------
   calculate the checksum for a block and append it to it, checksum blocks
   of size E2EBLOCK_SIZE always, they are already padded
   -------------------------------------------------------------------------- */
char* gen_block_checksum(unsigned char *clearBlock,
			 long int e2ebs)
{

  unsigned char *theBlock, *newBlock, *checksum;
  int i, j, k, lenMD5, retval;
  double ti, tf;
  MD5_CTX context;
  unsigned char digest[16];
  unsigned char hbuffer[HASH_SIZE];
  char *hexChecksum, *hexChecksum2;
  unsigned char *padding=NULL;
  long int lenpadd;

#if 0
  theBlock = (unsigned char *) malloc(sizeof(unsigned char)*(e2ebs+1));
  memcpy(theBlock,clearBlock,size);
  theBlock[e2ebs] = 0;

  newBlock = (unsigned char *)malloc(sizeof(unsigned char) * (E2Eblock_size+CH_SIZE+1));
  bzero(*newBlock, e2ebs+CH_SIZE+1);
#endif

  lenMD5 = e2ebs; 

  MD5Init(&context);
  MD5Update(&context, clearBlock, lenMD5);
  MD5Final(digest, &context);

  hexChecksum = (char *)((char *)hash2hex(digest));

#if 0
  strncpy(*newBlock,hexChecksum,CH_SIZE);
  memcpy(*newBlock+CH_SIZE, theBlock, E2Eblock_size);
  *newBlock[E2Eblock_size+CH_SIZE] = 0;
  free(theBlock);
#endif

  return hexChecksum;
}


/* ---------------------------------------------------------------------
   verifies the integrity of a block by computing the checksum
   of the received block and comparing it to the received one
   --------------------------------------------------------------------- */
int VerifyBlockChecksum(unsigned char *clearBlock, char *myChecksum, int size)
{
  int retval;
  char *theChecksum;

  theChecksum = CalculateChecksum(clearBlock, size);
  if(!strncmp(theChecksum, myChecksum,CH_SIZE))
    retval = OK;
  else
    retval = E2EFAIL;
  /***fprintf(stderr,"VBC returns = %d   
   * clearBlock=%s myChecksum=<%s>    
   * theChecksum=<%s>\n", retval,clearBlock,myChecksum,theChecksum);***/
  /* TODO: check this for the truth */
  free(theChecksum);

  return retval;
}


/* -------------------------------------------------------------------
   look into a block with checksum included, and read it
   into a buffer
   ------------------------------------------------------------------- */
char* GetChecksum(char *chBlock)
{
  char *checksum, header[HEADER_SIZE];
  int i,j=0;

  checksum = (char*)malloc(sizeof(char)*(CH_SIZE+1));
  
  strncpy(header,chBlock,HEADER_SIZE);
  for(i=(HEADER_SIZE-CH_SIZE-1); i<(HEADER_SIZE-1); i++){
    checksum[j++] = header[i++];
  }

  checksum[CH_SIZE] = '\0';
  return checksum;
}



/* ------------------------------------------------------------------
   CH: takes off the checksum and header of a single block 
   of the size define
   ------------------------------------------------------------------ */
unsigned char* CreateCleanBlock(unsigned char *chBlock, int size)
{
  char *clearBlock;

  clearBlock = (unsigned char *)malloc(sizeof(unsigned char) * (size+1));

  memcpy(clearBlock,chBlock+HEADER_SIZE,size);
  clearBlock[size] = 0;

  return clearBlock;
}


int block_length_count(unsigned long int input_length,
                       unsigned long int input_blocksize,
                       int *blocks,
                       unsigned long int *last_length,
                       int  alignment)
{
    int m = 0;

    m = input_blocksize % alignment;
    if ( m != 0 ) {
        input_blocksize += (alignment - m);
    }
    *blocks = (input_length-1)/input_blocksize;
    *last_length = input_length % input_blocksize;
    if ( *last_length == 0 )
    {
        *last_length = input_blocksize;
    }
    return 0;
}


/* the conditioning functions will attempt to condition as much as possible
 * given the input length and blocksize.  As little padding as possible will
 * be introduced. */
int ChecksumMapping(char *input_buffer,
		            unsigned long int input_length,
		            unsigned long int input_blocksize,
		            char **output_buffer,
		            unsigned long int *output_length,
		            unsigned long int *output_blocksize)
#if 1
{
  int tblocks, length, count=0,retval=0;
  long int nread=0, physical_size;
  char *checksum, *header;
  unsigned long int     last_length = 0;
  unsigned long int blocksize = 0;

  /*if ( input_blocksize > input_length ) input_blocksize = input_length;*/

  block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);
  physical_size = (CHECKSUM_H+input_blocksize)*tblocks + CHECKSUM_H+last_length;
  /*fprintf(stderr, "input_length: %d input_blocksize: %d last_length: %d\n",
                    input_length, input_blocksize, last_length);*/

  *output_length = physical_size;
  /* TODO: in the smallest case when length < blocksize, this will be an error */
  /* perhaps the caller will need to verify that this is not so. */
  *output_blocksize = input_blocksize + CHECKSUM_H;

  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*(physical_size+1));
  if(*output_buffer == NULL){
    perror("ChecksumMapping");
    retval = -1;
    goto RET_POINT;
  }

  bzero(*output_buffer, physical_size+1);

  length = input_length;
  nread = 0;
  while ( length > 0 )
  {
      blocksize = (input_blocksize<length?input_blocksize:length);
      checksum = gen_block_checksum(input_buffer+nread, blocksize);
      header = create_header(checksum,CHECKSUM_H);

      /*fprintf(stderr,"length: %d nread: %ld e2ebs: %ld checksum: <%s>\n",*/
            /*length, nread, blocksize, checksum); */
      memcpy((*output_buffer)+(nread+(count*CHECKSUM_H)),header,CHECKSUM_H);
      memcpy((*output_buffer)+(nread+((count+1)*CHECKSUM_H)),
              input_buffer+nread,blocksize);
      nread  += blocksize;
      length -= blocksize;
      count++;
      destroy_header(header);
      free(checksum);
  }
  (*output_buffer)[physical_size] = 0;
  
  RET_POINT:
  return retval;
}
#endif

/*int VerifyChecksumMapping(unsigned char *chMapping,
		       unsigned long int physical_size,
		       unsigned char **clearMapping,
		       unsigned long int *out_size,
		       unsigned long int e2ebs)*/
int VerifyChecksumMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length)
{
  char checksum[CH_SIZE+1];
  int retval=0, tblocks, count=0, r;
  unsigned long int nread=0, length, tmemory, last_length;
  unsigned long int blocksize;


  /* TODO: Is this necessary? */
#if 0
  if(physical_size<e2ebs){  /* there is compression */
    tblocks = 1;
    tmemory = physical_size-CHECKSUM_H;
  }
  else{
#endif
    block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);
    /*fprintf(stderr, "input_blocksize: %d last_length: %d\n",*/
                    /*input_blocksize, last_length);*/
    /*tblocks = (input_length-1)/input_blocksize;*/
    /* TODO: again, if input_blocksize is > input_length, this will break */
    tmemory = input_blocksize-CHECKSUM_H;
  /*}*/

  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*((tblocks+1)*(tmemory)+1));
  if((*output_buffer) == NULL){
    retval = -1;
    perror("VerifyChecksumMapping");
    goto SALIR;
  }
  *output_length = tblocks*(tmemory)+(last_length-CHECKSUM_H);

  length = input_length;
  while(length > 0 )
  {
      blocksize = (input_blocksize < length ? input_blocksize : length);
      strncpy(checksum,input_buffer+nread,CH_SIZE);
      checksum[CH_SIZE] = 0;


      /*fprintf(stderr, "nread: %d %d blocksize: %d\n", nread, CHECKSUM_H, blocksize);*/

      /*fprintf(stderr, "length: %d nread: %d blocksize: %d checksum: <%s> checksum_h: %d\n",*/
              /*length, nread, blocksize, checksum, CHECKSUM_H);*/
      r = VerifyBlockChecksum(input_buffer+nread+CHECKSUM_H, checksum, blocksize-CHECKSUM_H);
      if(r != OK){
          retval=-1;
          free(*output_buffer);
          *output_buffer = NULL;
          fprintf(stderr,"CHECKSUM ERROR: data block [%d] is corrupted\n",count);
          fprintf(stderr, "%s: \n", checksum);
          goto SALIR;
      }
      /* paste decrypted block */
      memcpy((*output_buffer)+(nread-(CHECKSUM_H*count)),
              input_buffer+nread+CHECKSUM_H,
              (blocksize-CHECKSUM_H));  
      nread += blocksize;
      length -= blocksize;
      count++;
  }
  (*output_buffer)[*output_length] = 0;

 SALIR:
  /*if(tmpBlock != NULL)*/
    /*free(tmpBlock);*/
  return retval;
}


/* -------------------------------------------------------------------------
   SEQ NUM: verifies that the blocks of an arriving segment are
   correct, needs buffering
   ------------------------------------------------------------------------- */
int VerifySegmentSequence(int *blockSequences, 
			  int *expected, 
			  long int offset,
			  long int length,
			  long int E2Eblock_size)
{
  int startblock, endblock, nblocks,i;

  startblock = (int)(offset/E2Eblock_size);
  endblock = ((offset + length)/E2Eblock_size);
  nblocks = endblock - startblock + 1;

  for(i=0; i<nblocks; i++){
    if(blockSequences[i] != expected[i]){
      fprintf(stderr,"didn't receive expected block\n");
      return E2EFAIL;
    }
  }
}

/* --------------------------------------------------------------------------
   SEQ NUM:  given a segment that the user wants to download, first
   see what sequence numbers should be expected
   -------------------------------------------------------------------------- */
int* IdentifySegmentSequence(char *Segment,
			     long int offset,
			     long int length,
			     long int E2Eblock_size)
{
  int *sequences;
  int nblocks, startblock, endblock,i,c;

  nblocks = (int)((length-1)/E2Eblock_size) + 1;
  sequences = (int *)malloc(sizeof(int)*nblocks);

  startblock = (int)(offset / E2Eblock_size);
  endblock = (int)((offset+length-1) / E2Eblock_size);

  c = 0;
  for(i=startblock; i<endblock+1; i++){
    sequences[c++] = i;
  }
  return sequences;
}


/* -----------------------------------------------------------------------------
   SEQ NUM this function gets the sequence number from a ch block
   ----------------------------------------------------------------------------- */
int GetSequenceNumber(char *chBlock)
{
  int sequence, pos=0, i=0;
  char temp[9];
  
  while(chBlock[pos] != ':'){
    temp[i++] = chBlock[pos++];
  }
  temp[9]= '\0';
  sequence = atoi(temp);
}

/* -----------------------------------------------------------------------------
   SEQ NUM this function verifies that the block received is actually
   the one expected
   ----------------------------------------------------------------------------- */
int VerifySequenceNumber(char *chBlock,
			 int expected)
{
  int retval, sequence;
  sequence = GetSequenceNumber(chBlock);
  if(sequence == expected)
    retval = OK;
  else
    retval = E2EFAIL;
  return retval;
}



/* -----------------------------------------------------------------------
   get the whole blocks from a file, for a defined segment of 
   length, starting in offset, where the desired information can
   be found
   ----------------------------------------------------------------------- */
#if 0
char* GetWholeBlocks(char *path,
		     long int filesize,
		     long int offset, 
		     long int length,
		     long int E2Eblock_size)
{
  int numblocks, sourcefd, startblock,endblock, nblocks, i;
  unsigned long int totalsize;
  unsigned long int choffset, chlength;
  char *wholeBlocks, tmpBlock[E2Eblock_size+HEADER_SIZE];

  fprintf(stderr,"path=%s filesize=%d\n",path,filesize);

  numblocks = ((int)(filesize-1)/E2Eblock_size) + 1;
  totalsize = filesize + (numblocks*HEADER_SIZE);

  /***fprintf(stderr,"numblocks = %d      totalsize=%u\n",numblocks,totalsize);*/

  if((sourcefd = open(path, O_RDONLY)) < 0){
    perror("error opening file");
    return NULL;
  }
  
  if((wholeBlocks = (char *)malloc(sizeof(char) * (numblocks*(E2Eblock_size+HEADER_SIZE)+1))) ==NULL){
    perror("not enough memory to allocate temporal buffers");
    return NULL;
  }
  bzero(wholeBlocks,length); 

  startblock = (int)(offset / E2Eblock_size);
  endblock = (int)((offset+length-1) / E2Eblock_size)+1;
  nblocks = endblock - startblock;
  
  if((i=lseek(sourcefd, startblock*(E2Eblock_size+HEADER_SIZE), SEEK_SET)) < 0){
    perror("error opening file");
    return NULL;
  }
   fprintf(stderr,"startblocki=%d endblock=%d nblocks=%d \n",startblock,endblock,nblocks);

  for(i=0; i<nblocks; i++){
    if(read(sourcefd, tmpBlock, E2Eblock_size+HEADER_SIZE) < 0){
      perror("error reading file");
      return NULL;
    }
    strcat(wholeBlocks, tmpBlock);
  }
  return wholeBlocks;
}


#endif

/********************************************************************************
  from this point encryption functions right now is using DES
*********************************************************************************/
/*static des_cblock iv = {0,0,0,0,0,0,0,0};*/
char* GetKey(char *Block)
{
  char *key;
  key = (char *)malloc(sizeof(char)*KEY_SIZE);

  return key;
}

#include <pthread.h>
pthread_mutex_t gmut = PTHREAD_MUTEX_INITIALIZER;
int             gmut_init;
/* ----------------------------------------------------------------
    decrypt a block using DES
   ----------------------------------------------------------------*/
char* decrypt_DES(unsigned char *cryptBlock, char *thekey, long int size)
{
  unsigned char *clearBlock;
  des_cblock key;
  des_key_schedule ks;
  long int offset;
  static des_cblock iv = {0,0,0,0,0,0,0,0};  

  clearBlock = (char *)malloc(sizeof(char)*(size+1));
  clearBlock[size] = 0;

  pthread_mutex_lock(&gmut);
  des_string_to_key(thekey,(des_cblock *)key); 
  des_set_key((des_cblock *)key,ks);

  bzero(iv, sizeof(iv));
  for(offset=0; offset<size; offset+=8){
    des_cbc_encrypt((des_cblock *)(cryptBlock+offset), 
		    (des_cblock *)(clearBlock+offset), 
		    E2EMIN(size-offset,8), 
		    ks, &iv, 0);
    memcpy(iv, (des_cblock *)(cryptBlock+offset),8);
  }
  pthread_mutex_unlock(&gmut);

  return clearBlock;
}


/* ----------------------------------------------------------------
    encrypt a block using DES
   ----------------------------------------------------------------*/

unsigned char* encrypt_DES(char *clearBlock, char *thekey, unsigned long int e2ebs)
{
  unsigned char *encBlock;
  des_cblock key;
  des_key_schedule ks;
  long int offset;

  char *justatest;
  static des_cblock iv = {0,0,0,0,0,0,0,0};

  encBlock = (unsigned char *)malloc(sizeof(unsigned char)*e2ebs);
  bzero(encBlock,e2ebs);

  pthread_mutex_lock(&gmut);
  des_string_to_key(thekey,(des_cblock *)key); 
  des_set_key((des_cblock *)key,ks);
  
  bzero(iv, sizeof(iv));
  for(offset=0; offset<e2ebs; offset+=8){
    des_cbc_encrypt((des_cblock *)(clearBlock+offset),  
		    (des_cblock *)(encBlock+offset), 
		    E2EMIN(e2ebs-offset,8),
		    ks, &iv, 1);  
    memcpy(iv, (des_cblock *)(encBlock+offset),8);    
  }
  pthread_mutex_unlock(&gmut);

#if 0
  justatest = decrypt_DES(encBlock, thekey, e2ebs);
  fprintf(stderr,"\n\nTEST\n<<%s>>[%d]\n\n",justatest,strlen(justatest));
#endif
  return encBlock;
}

/* ----------------------------------------------------------------
    encrypt a block using XOR
   ----------------------------------------------------------------*/
char* encrypt_XOR(char *buffer, char *key, unsigned long int e2ebs)
{
  char *encrypted;
  int i,j,l;
  
  l = strlen(key);
  encrypted = (char *)malloc(sizeof(char)*e2ebs);
  for(i=0, j=0; i<e2ebs; i++){
    if(j == 0)      /* this is legacy of a bug for compatiblity*/
      j = l-1;
    encrypted[i] = buffer[i] ^ key[j];
    j++; if ( j > l ) j=0;
  }
  
  return encrypted;
}
int   new_int_XOR(char *buffer, char *key, unsigned long int length)
{
    int         *int_buffer;
    int          int_key;
    int         i, j, remainder;

    /* avoid dependence on htonl() */
    int_key = (key[0]<<24) + (key[1]<<16) + (key[2]<<8) + key[3];

    int_buffer = (int *)buffer;
    for(i=0; i< length/4; i++)
    {
        int_buffer[i] = int_buffer[i] ^ int_key;
    }
    /* apply encrytpion to the remainder bytes */
    remainder = length %4;
    if ( remainder )
    {
        for(j=0; j < remainder; j++)
        {
            buffer[i*4+j] = buffer[i*4+j] ^ key[j];
        }
    }
    return 0;
}
char* new_encrypt_XOR(char *buffer, char *key, unsigned long int length)
{
    char *encrypted;
    int i,j,l;
  
    l = strlen(key);
    /*encrypted = (char *)malloc(sizeof(char)*e2ebs);*/
    for(i=0, j=0; i<length; i++)
    {
        /*j = i%l;*/
        if(j == 0) j = l-1;
        buffer[i] = buffer[i] ^ key[j];
        j++ ; if ( j > l) j=0;
    }
    return buffer;
}


/* ----------------------------------------------------------------
    decrypt a block using XOR
   ----------------------------------------------------------------*/
char* decrypt_XOR(char *buffer, char *key, unsigned long int size)
{
  char *decrypted;
  int i,j,l;
  l = strlen(key);
  decrypted = (char *)malloc(sizeof(char)*size);
  for(i=0, j=0; i<size; i++){
    if(j == 0)      /* this is legacy of a bug for compatiblity*/
      j = l-1;
    decrypted[i] = buffer[i] ^ key[j];
    j++; if ( j > l ) j=0;
  }
  /***printf("DECR = %s\n",decrypted);*/
  return decrypted;
}
char* new_decrypt_XOR(char *buffer, char *key, unsigned long int size)
{
  char *decrypted;
  int i,j,l;
  l = strlen(key);
  /*decrypted = (char *)malloc(sizeof(char)*size);*/
  for(i=0; i<size; i++){
    j = i%l;
    if(j == 0) 
      j = l-1;
    buffer[i] = buffer[i] ^ key[j];
  }
  /***printf("DECR = %s\n",decrypted);*/
  return buffer;
}

/* ----------------------------------------------------------------
   INTERNAL: old generic encrypt a block
   ---------------------------------------------------------------- */
unsigned char* EncryptBlock(char *clearBlock,
			    char *thekey,
			    int encryptType,
			    unsigned long int e2ebs)
{
  unsigned char *cryptBlock;
  unsigned char *theBlock;
  unsigned char *padding=NULL;
  long int lenpadd;
  
  if(e2ebs == 0){
    fprintf(stderr,"Error block is empty!\n");
    return NULL;
  }
  fprintf(stderr,"#");

  switch(encryptType){
    case ENC_DES:
      cryptBlock = encrypt_DES(theBlock, thekey, e2ebs);
      if(cryptBlock == NULL)
	fprintf(stderr,"DES: Error trying to encrypt a block\n");
      break;
    case ENC_XOR:
      cryptBlock = encrypt_XOR(theBlock, thekey, e2ebs);
      if(cryptBlock == NULL) 
	fprintf(stderr,"XOR: Error trying to encrypt a block\n");
      break;
    case ENC_NONE:
      cryptBlock = (char *)malloc(sizeof(char *)*(e2ebs+1));
      memcpy(cryptBlock,theBlock,e2ebs);
      cryptBlock[e2ebs] = 0;
      break;
    default:
      break;
  }
  return cryptBlock;
}


/* ----------------------------------------------------------------
   INTERNAL: old generic decrypt a segment
   ---------------------------------------------------------------- */
char* DecryptBlock(unsigned char *cryptBlock,
		   char *thekey,
		   int encryptType,
		   int e2ebs)
{
  char *clearBlock;

  switch(encryptType){
    case ENC_DES:
      clearBlock = decrypt_DES(cryptBlock,thekey,e2ebs);
      if(clearBlock == NULL)
	fprintf(stderr,"DES: Error trying to decrypt a block\n");
      break;
    case ENC_XOR:
      clearBlock = decrypt_XOR(cryptBlock,thekey,e2ebs);
      if(clearBlock == NULL)
	fprintf(stderr,"XOR: Error trying to decrypt a block\n");
      break;
    case ENC_NONE:
      clearBlock = cryptBlock;
      break;
  }

  return clearBlock;
}


/* ----------------------------------------------------------------
   ENCRYPTION: new encrypt a mapping using DES
   ----------------------------------------------------------------*/
/*int des_EncryptMapping(char *clearMapping,
		       unsigned long int logical_size,
		       unsigned char **encMapping,
		       unsigned long int *physical_size,
		       unsigned long int e2ebs,
		       char *key,
		       int *header_size)*/
int des_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key)
{
  char *tmpBlock, *encBlock, thekey[8], header[8], padding[8], value[3];
  int retval=0, tblocks, count=0, pad, pad1, pad2, i;
  unsigned long int nread=0, length = 0, out_offset = 0;
  unsigned long int last_length = 0, blocksize = 0;

  block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);

  /*tblocks = (logical_size -1)/e2ebs + 1;*/
  /*length = e2ebs;*/
  /*fprintf(stderr,"tblocks=%d  size=%d  e2ebs=%d\n",tblocks,logical_size,e2ebs);*/
  pad1 = 8 - (input_blocksize-((input_blocksize/8)*8));
  if(pad1 != 8){
    for(i=0;i<pad1;i++) padding[i] = '0';
    padding[pad1]=0;
  } else {
      pad1=0;
  }

  /* this may be different for the last_length */

  /*fprintf(stderr, "input_blocksize: %d last_length: %d\n", */
                    /*input_blocksize, last_length);*/
  pad2 = 8 - (last_length - ((last_length/8)*8));
  if ( pad2 != 8 )
  {
    for(i=0;i<pad2;i++) padding[i] = '0';
    padding[pad2]=0;
  } else {
      pad2 = 0;
  }

  tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*(input_blocksize+pad1+1));
  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*
                                      (tblocks*(input_blocksize+ENCRYPT_H+pad1)+
                                               (last_length+ENCRYPT_H+pad2)+1));

  *output_length = (tblocks*(input_blocksize+ENCRYPT_H+pad1)+
                            (last_length+ENCRYPT_H+pad2));
  *output_blocksize = input_blocksize+ENCRYPT_H+pad1;
  /**header_size = ENCRYPT_H+pad;*/
  /*sprintf(value,"%d:",pad1);  */
  /*value[3]=0;*/
  /*header = create_header(value,ENCRYPT_H);*/

  length = input_length;
  nread = 0;
  while ( length > 0 )
  {
      blocksize = (input_blocksize < length ? input_blocksize : length);
      pad = ( blocksize == length  ? pad2 : pad1);
      sprintf(header, "%d:", pad);
      /*fprintf(stderr, "length: %d nread: %d blocksize: %d padding: %d\n",*/
                       /*length, nread, blocksize, pad);*/

      memcpy(tmpBlock,input_buffer+nread, blocksize);
      memcpy(tmpBlock+blocksize,padding,pad);
      tmpBlock[blocksize+pad] = 0;

      /*memcpy((*output_buffer)+(nread+(count*(ENCRYPT_H+pad))),header,ENCRYPT_H);*/
      memcpy((*output_buffer)+out_offset,header,ENCRYPT_H);
      encBlock = encrypt_DES(tmpBlock, key, blocksize+pad);
   
      /* TODO: make less dependant on 'pad' */ 
      /* paste encrypted block */
      /*memcpy((*output_buffer)+(nread+((count+1)*(ENCRYPT_H))+(count*pad)), 
       * encBlock,blocksize+pad);  */
      memcpy((*output_buffer)+out_offset+ENCRYPT_H, encBlock,blocksize+pad);  

      nread += blocksize;
      out_offset += blocksize+pad+ENCRYPT_H;
      count++;
      bzero(tmpBlock,blocksize+pad);   
      free(encBlock);
      length -= blocksize;
  }
  
  free(tmpBlock); 
  return retval;

}



/*int des_DecryptMapping(unsigned char *encMapping,
		       unsigned long int physical_size,
		       char **clearMapping,
		       unsigned long int *out_size,
		       unsigned long int e2ebs,
		       char *key)*/
int des_DecryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key)
{
  char *tmpBlock, *encBlock, t[2];
  int retval=0, tblocks, count=0, pad;
  unsigned long int nread=0, length, tmemory, out_offset=0, last_length = 0;
  unsigned long int blocksize = 0;


#if 0
  if(physical_size<e2ebs){  /* there is compression */
    tblocks = 1;
    tmemory = physical_size-ENCRYPT_H;
  }
  else{
#endif
  block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);

  /*fprintf(stderr, "input_length: %d input_blocksize: %d last_length:
   * %d\n",*/
                    /*input_length, input_blocksize, last_length);*/

    /*tblocks = physical_size/(e2ebs);*/
    /*tmemory = e2ebs-ENCRYPT_H;*/
  tmemory = input_blocksize - ENCRYPT_H;
  /*}*/

  /***  fprintf(stderr,"  tblocks=%d physical_size=%d  e2ebs=%d\n",
   *    tblocks,physical_size,e2ebs);*/
 
  tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*((tmemory)+1));
  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*
                            (tblocks*tmemory)+
                            (last_length-ENCRYPT_H)+1);

  length = input_length;

  /**out_size = tblocks*(tmemory-pad);*/

  nread = 0;
  while ( length > 0 )
  {
      blocksize = ( input_blocksize < length ? input_blocksize : length );
      pad = atoi(input_buffer+nread);

      /*fprintf(stderr, "length: %d nread: %d blocksize: %d padding: %d\n",*/
                       /*length, nread, blocksize, pad);*/

      memcpy(tmpBlock,input_buffer+nread+ENCRYPT_H,blocksize-ENCRYPT_H); 
      tmpBlock[blocksize-ENCRYPT_H] = 0;

      encBlock = decrypt_DES(tmpBlock, key, blocksize-ENCRYPT_H);
      /* paste decrypted block */
      /*memcpy((*output_buffer)+(nread-((ENCRYPT_H+pad)*count)),encBlock,(tmemory-pad));  */
      memcpy((*output_buffer)+(out_offset),encBlock,(blocksize-ENCRYPT_H-pad));  
      nread += blocksize;
      out_offset += blocksize-pad-ENCRYPT_H; /* clear */
      /*fprintf(stderr, "out_offset: %d\n", out_offset);*/
      count++;
      length -= blocksize;

      bzero(tmpBlock,(tmemory));
      free(encBlock);
  }
  *output_length = (out_offset); 
  (*output_buffer)[*output_length] = 0;
  free(tmpBlock);

  return retval;
}



/* ----------------------------------------------------------------
   ENCRYPTION: new encrypt a mapping using XOR
   ----------------------------------------------------------------*/
/*int xor_EncryptMapping(char *clearMapping,
		       unsigned long int logical_size,
		       unsigned char **encMapping,
		       unsigned long int *physical_size,
		       unsigned long int e2ebs,
		       char *key,
		       int *header_size)*/
int xor_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key)
{
  char *tmpBlock, *encBlock, *header;
  int retval=0, tblocks, count=0;
  unsigned long int nread=0, length = 0, last_length = 0;
  unsigned long int blocksize = 0;

  block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);

  /*tblocks = (logical_size -1)/e2ebs + 1;*/
  /*tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*(e2ebs+ENCRYPT_H+1));*/
  tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*(input_blocksize+1));
  *output_length = (tblocks *input_blocksize + last_length);
  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*((*output_length)+1));
  *output_blocksize = input_blocksize;

  length = input_length;
  /*fprintf(stderr, "\n");*/
  /**header_size = ENCRYPT_H;*/

  while ( length > 0 )
  {
      blocksize = (input_blocksize < length ? input_blocksize : length );

      memcpy(tmpBlock,input_buffer+nread,blocksize);
      tmpBlock[blocksize] = 0;
      /* encrypted block */
      encBlock = encrypt_XOR(tmpBlock, key, blocksize);
      memcpy((*output_buffer)+(nread), encBlock, blocksize);  

      nread += blocksize;
      length -= blocksize;
      count++;
      bzero(tmpBlock,blocksize);
      free(encBlock);
  }
  /**physical_size = (e2ebs+ENCRYPT_H)*tblocks;*/

  free(tmpBlock);

#if 0
  while(count < tblocks){
    /***fprintf(stderr,"ENC: count=%d nread = %d   size= %d HEADER_SZ= %d \n",
	count,nread,logical_size,HEADER_SIZE); */
    memcpy(tmpBlock,clearMapping+nread,e2ebs);
    tmpBlock[e2ebs] = 0;
   
    /*fprintf(stderr,"tmpBl=<<%s>>\n",tmpBlock);*/
    header = create_header("0:",ENCRYPT_H);
    memcpy((*encMapping)+(nread+(count*ENCRYPT_H)),header,ENCRYPT_H);  /* header to new buff */
    encBlock = encrypt_XOR(tmpBlock, key, e2ebs);
    memcpy((*encMapping)+(nread+((count+1)*ENCRYPT_H)), encBlock, e2ebs);  /* encrypted block */

    nread += length;
    count++;
    bzero(tmpBlock,e2ebs);
    destroy_header(header);
  }
#endif

  return retval;
}
int new_xor_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       unsigned char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key)
{
    char *tmpBlock, *encBlock, *header;
    int retval=0, tblocks, count=0;
    unsigned long int nread=0, length = 0, last_length = 0;
    unsigned long int blocksize = 0;
    int   key_len;

    block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);

    *output_length = input_length;
    *output_buffer = input_buffer;
    *output_blocksize = input_blocksize;

    length = input_length;
    key_len = strlen(key);
    if ( key_len == 7 ) 
    {
        while ( length > 0 )
        {
            blocksize = (input_blocksize < length ? input_blocksize : length );
            new_encrypt_XOR(input_buffer+nread, key, blocksize);
            nread += blocksize;
            length -= blocksize;
        }
    } else if (key_len == 4) {
        new_int_XOR(input_buffer, key, input_length);
    } else 
    {
        fprintf(stderr, "unknown key length: %d\n", key_len);
        retval = -1;
    }

    return retval;
}




/* ----------------------------------------------------------------
   ENCRYPTION: new XOR decrypt a mapping (keeps the headers on)
   ----------------------------------------------------------------*/
int xor_DecryptMapping(unsigned char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key)
{
  char *encBlock, *clear_buff, *tmpBlock, *enc_buff;
  int retval=0, tblocks, count=0;
  unsigned long int nread=0, length, tmemory, blocksize, last_length;


    block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);
    tmemory = input_blocksize;

    /*fprintf(stderr, "input: %d\n", input_length);*/
    /*fprintf(stderr, "inputblock: %d\n", input_blocksize);*/
    /*fprintf(stderr, "tblocks: %d\n", tblocks);*/
    /*fprintf(stderr, "last_length: %d\n", last_length);*/

    tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*((tmemory)+1));
    *output_length = tblocks*input_blocksize + last_length;
    *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*(*output_length)+1);

    /*fprintf(stderr, "output_length: %d\n", *output_length);*/
    /*fprintf(stderr, "output_buffer: 0x%x\n", *output_buffer);*/

    length = input_length;

    while ( length > 0 )
    {
      blocksize = (input_blocksize < length ? input_blocksize : length);
      memcpy(tmpBlock,input_buffer+nread,blocksize);
      tmpBlock[blocksize] = 0;
   
      /*fprintf(stderr, "tmpblock: 0x%x\n", tmpBlock);*/
      /*fprintf(stderr, "key: %s\n", key);*/
      /*fprintf(stderr, "blocksize: %d\n", blocksize);*/
      encBlock = decrypt_XOR(tmpBlock, key, blocksize);
      /* paste the encrypted block */
      memcpy((*output_buffer)+(nread), encBlock, blocksize);  

      nread += blocksize;
      count++;
      bzero(tmpBlock,blocksize);
      free(encBlock);

      length -= blocksize;
  }
  (*output_buffer)[*output_length] = 0;
  free(tmpBlock);
#if 0
  while(count < tblocks){
    /***fprintf(stderr,"count=%d nread = %d   size= %d HEADER_SZ= %d \n",
	count,nread,logical_size,HEADER_SIZE);*/

    memcpy(tmpBlock,encMapping+nread+ENCRYPT_H,(tmemory));
    tmpBlock[e2ebs-ENCRYPT_H] = 0;
   
    encBlock = decrypt_XOR(tmpBlock, key, (tmemory));
    memcpy((*clearMapping)+(nread-(ENCRYPT_H*count)), encBlock, (tmemory));  /* paste the encrypted block */

    nread += length;
    count++;
    bzero(tmpBlock,tmemory);
  }
#endif
  return retval;
}
int new_xor_DecryptMapping(unsigned char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key)
{
    char *encBlock, *clear_buff, *tmpBlock, *enc_buff;
    int retval=0, tblocks, count=0;
    unsigned long int nread=0, length, tmemory, blocksize, last_length;
    int key_len;

    block_length_count(input_length, input_blocksize, &tblocks, &last_length, input_blocksize);
    *output_length = input_length;
    *output_buffer = input_buffer;

    length = input_length;
    key_len = strlen(key);

    if ( key_len == 7 ) 
    {
        while ( length > 0 )
        {
            blocksize = (input_blocksize < length ? input_blocksize : length);
            encBlock = new_decrypt_XOR(input_buffer+nread, key, blocksize);
            nread += blocksize;
            length -= blocksize;
        }
    } else if ( key_len == 4 )
    {
        new_int_XOR(input_buffer, key, input_length);
    } else {
        fprintf(stderr, "unknown key length: %d\n", key_len);
        retval = -1;
    }
    return retval;
}


/* --------------------------------------------------------
   Compress function: compress only by mapping!
   -------------------------------------------------------- */
/*int CompressMapping(char *clearMapping,
		       unsigned long int logical_size,
		       unsigned char **compMapping,
		       unsigned long int *physical_size,
		       unsigned long int e2ebs,
		       int *header_size)*/
int CompressMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize)
{
  unsigned char *tmpBlock, *header;
  int retval=0, tblocks, r=0, diff, i;
  unsigned long int nread=0, length;
  char temp[REAL_SIZE], val[REAL_SIZE];

  if(input_blocksize != input_length){
    fprintf(stderr,"CompressMapping: cannot do compression with submappings\n");
    return -1;
  }
  

  /*fprintf(stderr, "compressing: input_blocksize: %d, input_length\n", */
          /*input_blocksize, input_length);*/
  tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*(input_blocksize+1+(int)(input_blocksize*0.1)+12));
 
  /**header_size = COMPR_H;  */
  /* compression assumes whole mapping so tblocks=1 */  

  sprintf(val,"%d",input_length);

  /*fprintf(stderr, "input_len: str: %s\n", val);*/
  if(strlen(val)<REAL_SIZE){
    diff = REAL_SIZE - strlen(val);
    bzero(temp,REAL_SIZE);
    for(i=0; i<diff; i++) strcat(temp,"0");
    strncat(temp,val,strlen(val));
    temp[REAL_SIZE]=0;
  }
  header = create_header(temp,COMPR_H); 
  /*fprintf(stderr, "temp: str: %s\n", temp);*/
  /*fprintf(stderr, "header: str: %s\n", header);*/
  
  r = compress((Bytef *)tmpBlock, &length,(Bytef *)input_buffer,input_length);
  if(r<0){
    fprintf(stderr,"CompressMapping: compression function error\n");
    retval = -1;
    goto COMP_EXIT;
  }

  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*
                                (length+COMPR_H+1));
  memcpy((*output_buffer),header,COMPR_H);  /* header to new buff */
  memcpy((*output_buffer)+COMPR_H, tmpBlock, length);  /* encrypted block */
  (*output_buffer)[length+COMPR_H] = 0;
  destroy_header(header);
  *output_length = length + COMPR_H;
  *output_blocksize = length + COMPR_H;
  
 COMP_EXIT:
  free(tmpBlock);
  return retval;
}



/* --------------------------------------------------------
   Uncompress function: works only on whole mappings!
   -------------------------------------------------------- */
/*int UncompressMapping(unsigned char *compMapping,
		      unsigned long int physical_size,
		      char **clearMapping,
		      unsigned long int *out_size,*/
int UncompressMapping(unsigned char *input_buffer,
		      unsigned long int input_length,
		      char **output_buffer,
		      unsigned long int *output_length)
{
  char *encBlock, *clear_buff,*enc_buff, temp[REAL_SIZE+1];
  char *tmpBlock;
  int retval=0, tblocks, count=0, r=0;
  unsigned long int nread=0, length, org_len;

  temp[REAL_SIZE]='\0';
  strncpy(temp,input_buffer,REAL_SIZE);
  org_len = atol(temp);

  /*fprintf(stderr, "original length: %d: %s\n", org_len, temp);*/

  tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*((input_length-COMPR_H)+1));
  clear_buff = (unsigned char *)malloc(sizeof(unsigned char)*((org_len)+1));
  *output_buffer = (unsigned char *)malloc(sizeof(unsigned char)*((org_len)+1));

  memcpy(tmpBlock,input_buffer+COMPR_H,(input_length-COMPR_H));
  tmpBlock[input_length-COMPR_H] = 0;

  r = uncompress((Bytef *)clear_buff,&length,(Bytef *)tmpBlock,(input_length-COMPR_H));
  if(r<0){
    fprintf(stderr,"UncompressMapping: compression function error: %d\n", r);
    retval = -1;
    goto UNCOMP_EXIT;
  }
  memcpy(*output_buffer,clear_buff, org_len);  /* paste the uncompressed block */
  (*output_buffer)[org_len] = 0;
  *output_length = org_len;
 
 UNCOMP_EXIT:
  free(clear_buff);
  free(tmpBlock);
  return retval;
}


/********************************************************************************************/
/* -----------------------------------------------------------------------------------
   GENERAL: converts logical-to-physical values such as offset and lengths within
            a mapping (logical data is relative to the mapping)
   ----------------------------------------------------------------------------------- */
int logical2physical(unsigned long int original_phys_size,
		     unsigned long int original_e2ebs,
		     unsigned long int logical_offset,
		     unsigned long int logical_size,
		     unsigned long int *physical_offset,
		     unsigned long int *physical_size,
		     unsigned long int accum_bytes)
{
  int retval=0;
  long int count=0, startblock, endblock, i=0, tblocks=0, nblocks=0;
  unsigned long int accum=0, real_len, offset=0, length, logical_end, current_e2ebs=0;
  char the_len[9], *superbuffer;
  

  current_e2ebs = original_e2ebs + accum_bytes;

  tblocks = (int)((original_phys_size-1)/(original_e2ebs)) + 1;

  startblock = (int)(logical_offset / original_e2ebs);
  endblock = (int)((logical_offset+logical_size) / original_e2ebs);
  if(endblock >= tblocks){
    fprintf(stderr,"Error:  Length requested exceeded actual size of file\n");
    return -1;
  }
  nblocks = endblock-startblock + 1;
  *physical_size = current_e2ebs*nblocks;
  *physical_offset = current_e2ebs*startblock;

#if 0
  fprintf(stderr,"startbl=%d   endbl=%d   nblocks=%d   phys_size=%d  phys_off=%d\n",
	  startblock,endblock,nblocks, *physical_size, *physical_offset);
#endif

  return retval;
}


/* --------------------------------------------------------------------------------
   GENERAL: this function appends a "clear" header into a block of e2ebs already
   padded, each e2e funciton will append it's own header with a : to the right
   -------------------------------------------------------------------------------- */
#if 0
char* append_header(unsigned char *e2eblock, 
		    long int e2ebs, 
		    char *header,
		    int header_size)
{
  unsigned char *Block;
  /***fprintf(stderr,"header=<%s> e2ebs=%d  len=%d\n",he
      ader,e2ebs,strlen(header)); */

  Block = (unsigned char *)malloc(sizeof(unsigned char) * (e2ebs + header_size + 1));
  memcpy(Block, header, header_size);
  /***fprintf(stderr,"Block <%s> HDS=%d head=<%s>\n",Block,header_size,header); */
  memcpy(Block+header_size, e2eblock, e2ebs);
  Block[header_size + e2ebs] = 0;

  return Block;
}
#endif

/* --------------------------------------------------------------------------------
   GENERAL: this function calculates the padding bytes and returns a padded block
   -------------------------------------------------------------------------------- */
unsigned char* padd_block(unsigned char *block, 
		 long int len, 
		 long int e2ebs)
{  
  unsigned char *theBlock;
  unsigned char *padding=NULL;
  long int lenpadd;

  /***fprintf(stderr,"block = <%s>   len = %d  strlen=%d\n",block,len,strlen(block)); */
  theBlock = (unsigned char *) malloc(sizeof(unsigned char)*(e2ebs + 1));
  memcpy(theBlock, block, len);

  if(len < e2ebs){
    lenpadd = e2ebs - len;
    /***fprintf(stderr,"lenpadd= %d\n",lenpadd); */
    padding = (unsigned char *)malloc(sizeof(unsigned char)*lenpadd);
    memset(padding,'0',sizeof(char)*lenpadd);
    memcpy(theBlock+len, padding, lenpadd);
    theBlock[e2ebs] = 0;
  }

  if(padding != NULL)
    free(padding);

  return theBlock;
}

/* -------------------------------------------------------------------------------
   GENERAL: this just builds a header, and appends : to the right of the string
            and returns to caller, caller is responsible for freeing this memory, 
            destructor provided below
   ------------------------------------------------------------------------------- */
char* create_header(char *value, int header_size)
{
  char *header;
  /*** fprintf(stderr,"createheader=%d\n",header_size);*/
  header = (char *)malloc(sizeof(char)*(header_size+1));  /* ':' included in header_size */
  if(header == NULL){
    perror("create_header");
    return NULL;
  }
  strncpy(header,value,header_size-1);
  strncpy(header+(header_size-1),":",SEP);
  header[header_size] = 0;
  /*** fprintf(stderr,"create_header=[%s]\n",header); */
  return header;
}

void destroy_header(char *header)
{
  free(header);
}


/* ------------------------------------------------------------------------------
   GENERAL: given a clear buffer (mapping) counts how many e2e blocks contains,
   padds properly and adds clear headers, returns the processed buffer
   ------------------------------------------------------------------------------ */
int ConditionMapping(char *clear_mapping,
		     unsigned long int logical_size,
		     char **e2emapping,
		     unsigned long int *physical_size,
		     unsigned long int e2ebs,
		     int *header_size)
{
  long int nread=0, n=0, length=0;
  int count=1, retval=0, tblocks=0;
  unsigned char *tmpBlock, *wholeBlock, *x;
  char *header,temp[REAL_SIZE],val[REAL_SIZE];
  int diff,i=0;

  tmpBlock = (unsigned char*)malloc(sizeof(unsigned char)*(e2ebs+1));
  tblocks = (logical_size -1)/e2ebs + 1;
  *physical_size = (REAL_SIZE_H+e2ebs)*tblocks;
  *header_size = REAL_SIZE_H;

  *e2emapping = (unsigned char *)malloc(sizeof(unsigned char)*((tblocks*(REAL_SIZE_H+e2ebs))+1));
  bzero(*e2emapping, (tblocks*(REAL_SIZE_H+e2ebs)+1));
  wholeBlock = (unsigned char *)malloc(sizeof(unsigned char) * (e2ebs + REAL_SIZE_H + 1));

  while(nread < logical_size){
    bzero(tmpBlock, e2ebs+1);
    length = E2EMIN(e2ebs, logical_size-nread);
    memcpy(tmpBlock,clear_mapping+nread,length);
    tmpBlock[length] = 0;
    
    x = padd_block(tmpBlock,length,e2ebs);
    
    sprintf(val,"%d",length);
    if(strlen(val)<REAL_SIZE){
      diff = REAL_SIZE - strlen(val);
      bzero(temp,REAL_SIZE);
      for(i=0; i<diff; i++) strcat(temp,"0");
      strncat(temp,val,strlen(val));
      temp[REAL_SIZE]=0;
    }
    /***fprintf(stderr,"REAL_size value=<%s>\n",temp);*/
    header = create_header(temp,REAL_SIZE_H); 

    memcpy(wholeBlock, header, REAL_SIZE_H);
    memcpy(wholeBlock+REAL_SIZE_H, x, e2ebs);
    wholeBlock[REAL_SIZE_H + e2ebs] = 0;

    memcpy(*e2emapping+(nread+((REAL_SIZE_H)*(count-1))),
	   wholeBlock,
	   (e2ebs+REAL_SIZE_H));                          /* concatenate header */
    if(*e2emapping == NULL)
      retval = E2E_MEMERROR;
    nread += length;
    count++;    
  }
  destroy_header(header);
  free(x);
  free(tmpBlock);
  return retval;
}




/* ------------------------------------------------------------------------------
   GENERAL: strip off the last header and padding from a clear buffer before 
            returning to the user, last header contains actual size of data
	    w/o padding, NOTE that e2ebs includes the size of this header
   ------------------------------------------------------------------------------ */
int UnconditionMapping(char *clearMapping,
		       unsigned long int physical_size,
		       char **userMapping,
		       unsigned long int *user_size,
		       unsigned long int e2ebs)
{
  char *tmpBlock, *chBlock, real_size[REAL_SIZE+1];
  int retval=0, tblocks, count=0;
  unsigned long int nread=0, length, real_len, offset=0;
  

  tblocks = physical_size/(e2ebs);
  tmpBlock = (unsigned char *)malloc(sizeof(unsigned char)*(e2ebs-REAL_SIZE_H+1));
  *userMapping = (unsigned char *)malloc(sizeof(unsigned char)*(tblocks*(e2ebs-REAL_SIZE_H)+1));

  length = e2ebs;

  /***fprintf(stderr, "\nUNCONDITION tblocks=%d e2ebs=%d !!!!\n",tblocks,e2ebs);*/
  while(count < tblocks){
    /***fprintf(stderr,"UNCOND: count=%d nread = %d   size= %d e2ebs=%d  ",
	count,nread,physical_size,e2ebs); */

    strncpy(real_size,clearMapping+nread,REAL_SIZE);
    real_size[REAL_SIZE] = 0;

    real_len = atoi(real_size);
    memcpy(tmpBlock,clearMapping+nread+REAL_SIZE_H,real_len); 
    tmpBlock[real_len] = 0;   

    /***fprintf(stderr,"intlen=%d   tmpblock=<%s> \n",real_len,tmpBlock); */
    memcpy(*userMapping+offset,tmpBlock,real_len);

    nread += length;
    offset += real_len;
    count++;
    bzero(tmpBlock,e2ebs-REAL_SIZE_H);
  }
  (*userMapping)[offset] = 0;
  *user_size = offset;

 SALIR:
  free(tmpBlock);
  return retval;
}



/* ------------------------------------------------------------------------------
   GENERAL: receives a clear segment and completely digests it to transform
            to a checksumed, encrypted and compressed
   ------------------------------------------------------------------------------ */
#if 0
unsigned char* DigestBufferSegment(char *sourceSegment, 
				   long int size,
				   int encrypType,
				   int compType,
				   char *encrypKey,
				   long int e2ebs)
{
  unsigned long int tblocks=0, nread=0, n=0, length=0;
  int count=1, headsize;
  char tempBlock[e2ebs+1];
  char *header;
  unsigned char *encryptedBlock, *dataSegment, *chblock;

  tblocks = (size -1)/e2ebs + 1;
#if E2EDEBUG
  fprintf(stderr,"tblocks = %d\n",tblocks);
#endif
  dataSegment = (unsigned char *)malloc(sizeof(unsigned char)*(tblocks*(HEADER_SIZE+e2ebs+CH_SIZE)));
  bzero(tempBlock, e2ebs+1);
  bzero(dataSegment, tblocks*(HEADER_SIZE+e2ebs+CH_SIZE));

  while(nread < size){
    length = E2EMIN(e2ebs, size-nread);
    memcpy(tempBlock,sourceSegment+nread,length);
    tempBlock[length] = 0;

    /*/    header = create_header(encrypType,compType,length); */
    headsize = strlen(header);
    s1 = cpuseconds();
    encryptedBlock = EncryptBlock(tempBlock,encrypKey,encrypType, e2ebs);  /* will padd in here */
    s1 = cpuseconds() - s1;
    totalt += s1;
    /**fprintf(stderr,"Encrypting time=%f   Total buffering=%f\n",s1,totalt); */

    /*/chblock = gen_block_checksum(encryptedBlock,e2ebs, e2ebs);  */
    
    memcpy(dataSegment+(nread+((headsize+CH_SIZE)*(count-1))),
	   header,
	   headsize);                                                  /* concatenate header */
    memcpy(dataSegment+(nread+(headsize*count) + (CH_SIZE*(count-1))),
	   chblock, 
	   CH_SIZE+e2ebs);                                     /* concatenate ch + block */

    nread += length;
    count++;
    bzero(tempBlock,e2ebs);
    free(header);
    free(encryptedBlock);
    free(chblock);
  }
  return dataSegment;
}



/* -------------------------------------------------------------------------------
   GENERAL: given an offset and length from user vew transform and get all
   the blocks that contain that information
   ------------------------------------------------------------------------------- */
char* UndigestBufferSegment(unsigned char *chBuffer,        /* name of checksumed file */
			    long int buffersize,   /* size w headers */
			    long int offset,       /* start position original buffer */
			    long int length,       /* how much to read of original buffer */
			    int  encryptype,       /* encryption type */
			    int  comptype,
			    char *key,             /* encryption key */
			    long int e2ebs)
{
  long int ns=0, nt, i, buffoffset, chunksize, nblocks;
  int startblock, endblock, blockcount=0;
  int realcontentlen=e2ebs;                /* real length of block, in case of padding */
  char *buffptr, *superbuffer, header[HEADER_SIZE], templen[REAL_SIZE+1];
  unsigned char block[e2ebs+HEADER_SIZE+1], *clearblock;
  char towriteblock[e2ebs+1], *decryptedblock; /*/[e2ebs+1]; */
  int retval;
  int pos=0,j, c;                                  /* position within the buffer */
  long int realsize;                               /* the real size of n blocks without headers */
  long int prevlen = 0;

  nblocks = (int)((buffersize-1)/(e2ebs+HEADER_SIZE)) + 1;
  if(length < e2ebs)
    superbuffer = (char *)malloc(sizeof(char)*(e2ebs+1));
  else
    superbuffer = (char *)malloc(sizeof(char)*(length+1));
  realsize = (e2ebs * buffersize)/(HEADER_SIZE + e2ebs);

  startblock = (int)(offset / e2ebs);
  endblock = (int)((offset+length) / e2ebs);
  if(endblock > nblocks){
    fprintf(stderr,"Error:  Length requested exceeded actual size of file\n");
    return NULL;
  }
  buffoffset = offset - (startblock*e2ebs);
  chunksize = nblocks *(HEADER_SIZE + e2ebs);

#if E2EDEBUG
  fprintf(stderr,"startblock=%d  endblock=%d   lenght=%d   chunksize=%d  buffoffset=%d  nblock=%d\n",
	  startblock,endblock,length,chunksize,buffoffset,nblocks); 
#endif 

  for(i=0; i<nblocks; i++){
    bzero(block,e2ebs+HEADER_SIZE+1); 
    pos = (e2ebs+HEADER_SIZE)*i;
    memcpy(block, chBuffer+pos, e2ebs+HEADER_SIZE);
    block[e2ebs+HEADER_SIZE] = 0;

    clearblock = CreateCleanBlock(block, e2ebs);   

    strncpy(header, block, HEADER_SIZE);                                   /* obtain the header from current block */
    header[HEADER_SIZE] = 0;
#if E2EDEBUG
    fprintf(stderr,"header = %s key =%s\n",header+(HEADER_SIZE-CH_SIZE),key);
#endif
    strncpy(templen, header, REAL_SIZE);                                   /* read real content length from header */
    templen[REAL_SIZE] = 0;                                                /* obtain the real length in case of padding */
    realcontentlen = atoi(templen);

    retval = VerifyBlockChecksum(clearblock, header+(HEADER_SIZE-CH_SIZE), e2ebs);
    if(retval != OK){
      fprintf(stderr,"corrupted segment!\n");
      return NULL;
    }

    decryptedblock = DecryptBlock(clearblock, key, encryptype, e2ebs);   
    decryptedblock[realcontentlen] = 0;                                     /* decrypt current block */
  
    /**  printf("WHAT IS DECRYPTEDBLOCK \n-->>%s<<--\n  DECLEN=%d\n",decryptedblock,strlen(decryptedblock));*/



 if((i+1) == nblocks){                                               /* some block in end */
      memcpy(towriteblock,decryptedblock,(buffoffset+length)-(e2ebs*i));
      towriteblock[(buffoffset+length)-(e2ebs*i)] = 0;
      memcpy(superbuffer+prevlen, towriteblock, (buffoffset+length)-(e2ebs*i));
      prevlen += (buffoffset+length)-(e2ebs*i);
#if 0
      fprintf(stderr,"\nLAST BLOCK DATA: len=%d   total accum prevlen=%d\n<<<%s>>>\n", realcontentlen,prevlen,superbuffer);
#endif
    }
    else if(i == 0){                                                             /* this is first block */
      prevlen = e2ebs-(offset-(e2ebs*startblock));
      memcpy(towriteblock,decryptedblock+(offset-(e2ebs*startblock)), prevlen );
      memcpy(superbuffer, towriteblock, prevlen );
#if E2EDEBUG
      fprintf(stderr,"\nFIRST BLOCK DATA: len=%d diff = %d\n", realcontentlen, prevlen );
#endif
    }
    else{                                                                    /* some block in middle */
      memcpy(towriteblock,decryptedblock,e2ebs);
      memcpy(superbuffer+prevlen, towriteblock, e2ebs);
      prevlen += e2ebs;
#if E2EDEBUG
      fprintf(stderr,"\nMID BLOCK DATA: len=%d prevlen=%d \n", realcontentlen,prevlen);
#endif
    }

  }
  superbuffer[length] = 0;
#if E2EDEBUG
  fprintf(stderr,"superbuffer: \n!!!!!!!! %d !!!!!!!\n%s\n!!!!!!!!!!!!!!!!!!!!\n",
	  strlen(superbuffer),superbuffer);
#endif
  return superbuffer;   /* return data corresponding to the segment */
}

#endif
void fillrand(char *buf, int len)
{
    int i;
    for (i=0; i < len; i++)
    {
        buf[i] = (char)(rand()&0x000000FF);
    }
}

/*
 * input_length + BLOCK_SIZE
 *
 * or (input_blocksize + BLOCK_SIZE) * ((input_length+1)/input_blocksize)
 *
 */

int aes_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key)
{
    char            *buf = NULL, *dbuf = NULL;
    char            *output = NULL, *input = NULL;
    int             blocksize = 0;
    int             ret = 0;
    int             i = 0;
    int             tblocks = 0; 
    unsigned long int   last_length = 0;
    char            *c_key = NULL;
#ifdef _AES_H
    aes_ctx         ctx[1];

    buf = (char *)calloc(BLOCK_SIZE, sizeof(char));
    dbuf = (char *)calloc(2*BLOCK_SIZE, sizeof(char));
    c_key = (char *)calloc(BLOCK_SIZE, sizeof(char));
    /*
    if ( strlen(key) < BLOCK_SIZE )
    {
        fprintf(stderr, "len: %d\n", strlen(key));
        free(buf); free(dbuf); free(c_key);
        fprintf(stderr, "encryption key is not the proper length.\n");
        return -1;
    }
    */
    memcpy(c_key, key, BLOCK_SIZE);

    ctx->n_rnd = 0;  /* initialize to zero; */
    ctx->n_blk = 0;

    /* new */ /* these must be aligned to 16 bytes. */
    block_length_count(input_length, input_blocksize, &tblocks, &last_length, 16);
    /* the blocksize must be aligned to 16 bytes also, so we have to
     * recalculate input_blocksize; this should maybe be done in
     * block_length_count? */
    { 
        int m;
        m = input_blocksize % 16;
        if ( m != 0 ) {
            input_blocksize += (16 - m);
        }
    }
    *output_length = (tblocks*(input_blocksize+BLOCK_SIZE)) + 
                              (last_length+BLOCK_SIZE); 
    /* this number may be different */
    *output_blocksize = input_blocksize+BLOCK_SIZE;

    fprintf(stderr, "output_length: %d, output_blocksize: %d\n",
            *output_length, *output_blocksize);
    *output_buffer = (char *)calloc(*output_length, 1);

    output = *output_buffer;
    input = input_buffer;

    aes_enc_key(c_key, BLOCK_SIZE, ctx);
    /* this may go in the loop? */
    /* and does ctx need to be reset for each block? */
    fillrand(dbuf, BLOCK_SIZE);

    for (i=0; i < tblocks; i++)
    {
        ret = aes_EncryptBlock(input, input_blocksize, output, &blocksize,
                        buf, dbuf, ctx);
        if ( ret != 0 )
        {
            fprintf(stderr, "aes_EncryptBlock error\n");
            exit(1);
        }
        input += input_blocksize;
        output += blocksize;
    }
    /* encrypt the last_length block */
    ret = aes_EncryptBlock(input, last_length, output, &blocksize, 
            buf, dbuf, ctx);
    if ( ret != 0 )
    {
        fprintf(stderr, "aes_EncryptBlock error\n");
        exit(1);
    }
    free(dbuf);
    free(buf);
    free(c_key);
#endif
    return 0;
}

#ifdef _AES_H
int aes_EncryptBlock(char *input_buffer, int input_length, 
                     char *output_buffer, int *output_length,
                     char *buf, char *dbuf, aes_ctx *ctx)
{
    int             i;
    int             offset = 0;
    int             len = 0;
    int             rlen = 0;
    char            *output, *input;

    input = input_buffer;
    output = output_buffer;

    *output_length = input_length + BLOCK_SIZE;

    offset = 0;
    rlen = input_length;

    if (input_length <= BLOCK_SIZE)
    {
        /* TODO: make this work for sub-input_length data.  so it can operate
         * on input_blocksize lengths */

        /* read the bytes of the file into the buffer and verify length */
        memcpy(dbuf+BLOCK_SIZE, input, input_length);
        len = input_length;
        rlen -= len;
        if(rlen > 0)
            return -1;

        /* set final bytes with zeroes */
        for(i = len; i < BLOCK_SIZE; ++i)
            dbuf[i + BLOCK_SIZE] = 0;

        /* xor the file bytes with the IV bytes */
        for(i = 0; i < BLOCK_SIZE; ++i)
            dbuf[i + BLOCK_SIZE] ^= dbuf[i];

        /* encrypt the top 16 bytes of the buffer */
        aes_enc_blk(dbuf + BLOCK_SIZE, dbuf + len, ctx);

        len += BLOCK_SIZE;
        /* write the IV and the encrypted file bytes */
        memcpy(output, dbuf, len);
        output+=len;

        /*return -1;*/
    } else {
        /* save the IV */
        memcpy(output, dbuf, BLOCK_SIZE);
        output+=BLOCK_SIZE;
        /* read data a block at a time. */
        while ( rlen > 0 )
        {
            /* read a block and reduce the remaining byte count */
            len = BLOCK_SIZE; 
            /*(input_length-offset < BLOCK_SIZE ? input_length-offset :BLOCK_SIZE);*/
            memcpy(buf, input, len);
            input+= len;
            /*offset += len;*/
            rlen -= len;

            /* verify length of block  */
            if(len != BLOCK_SIZE) 
                return -1;

            /* do CBC chaining prior to encryption */
            for(i = 0; i < BLOCK_SIZE; ++i)
                buf[i] ^= dbuf[i];

            /* encrypt the block */
            aes_enc_blk(buf, dbuf, ctx);

            /* if there is only one more block do ciphertext stealing */
            if(rlen > 0 && rlen < BLOCK_SIZE)
            {
                /* move the previous ciphertext to top half of double buffer */
                /* since rlen bytes of this are output last */
                for(i = 0; i < BLOCK_SIZE; ++i)
                    dbuf[i + BLOCK_SIZE] = dbuf[i];

                /* read last part of plaintext into bottom half of buffer */
                memcpy(dbuf, input, rlen);
                input+=rlen;
                /*if(fread(dbuf, 1, rlen, fin) != rlen)*/
                    /*return READ_ERROR;*/

                /* clear the remainder of the bottom half of buffer */
                for(i = 0; i < BLOCK_SIZE - rlen; ++i)
                    dbuf[rlen + i] = 0;

                /* do CBC chaining from previous ciphertext */
                for(i = 0; i < BLOCK_SIZE; ++i)
                    dbuf[i] ^= dbuf[i + BLOCK_SIZE];

                /* encrypt the final block */
                aes_enc_blk(dbuf, dbuf, ctx);

                /* set the length of the final write */
                len = rlen + BLOCK_SIZE; rlen = 0;
            }

            /* write the encrypted block */
            memcpy(output, dbuf, len);
            output += len;
            /*if(fwrite(dbuf, 1, len, fout) != len)*/
        }
    }
    return 0;
}
#endif

int aes_DecryptMapping(unsigned char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key)
{
    char            *buf1, *buf2, *dbuf;
    char            *b1, *b2, *bt;
    char            *output;
    char            *input;
    int             tblocks;
    int             ret, blocksize;
    unsigned long   i, last_length; 
    char           *c_key;
#ifdef _AES_H
    aes_ctx         ctx[1];

    ctx->n_rnd = 0;  /* initialize to zero; */
    ctx->n_blk = 0;

    buf1 = calloc(BLOCK_SIZE, sizeof(char));
    buf2 = calloc(BLOCK_SIZE, sizeof(char));
    dbuf = calloc(2*BLOCK_SIZE, sizeof(char));
    c_key = (char *)calloc(BLOCK_SIZE, sizeof(char));

    block_length_count(input_length, input_blocksize, &tblocks, &last_length, 16);

    *output_length = (tblocks*(input_blocksize - BLOCK_SIZE)) + 
                              (last_length - BLOCK_SIZE);
    *output_buffer = (char *)calloc(*output_length, sizeof(char));

    output = *output_buffer;
    input = input_buffer;

    memcpy(c_key, key, BLOCK_SIZE);
    aes_dec_key(c_key, BLOCK_SIZE, ctx);

    for (i=0; i < tblocks; i++)
    {
        ret = aes_DecryptBlock(input, input_blocksize, output, &blocksize,
                buf1, buf2, dbuf, ctx);
        if ( ret != 0 )
        {
            fprintf(stderr, "aes_DecryptBlock error\n");
            exit(1);
        }
        input += input_blocksize;
        output += blocksize;
    }
    ret = aes_DecryptBlock(input, last_length, output, &blocksize,
                buf1, buf2, dbuf, ctx);
    if ( ret != 0 )
    {
        fprintf(stderr, "aes_DecryptBlock error\n");
        exit(1);
    }

    free(buf1);
    free(buf2);
    free(dbuf);
    free(c_key);
#endif
    return 0;
}

#ifdef _AES_H
int aes_DecryptBlock(unsigned char *input_buffer,
		       unsigned long int input_length,
		       char *output_buffer,
		       unsigned long int *output_length,
		       char *buf1, char *buf2, 
               char *dbuf, aes_ctx *ctx)
{
    int             i;
    int             len, rlen;
    char            *output;
    char            *input;
    char            *b1, *b2, *bt;

    rlen = input_length;
    input = input_buffer;
    output = output_buffer;

    /* this may not work entirely */
    *output_length = input_length - BLOCK_SIZE;

    if(rlen <= 2 * BLOCK_SIZE)
    { 
        /* read the bytes of the input */
        /*len = (unsigned long)fread(dbuf, 1, 2 * BLOCK_SIZE, fin);*/
        memcpy(dbuf, input, rlen);
        len = rlen;
        rlen -= len;
        if(rlen > 0)
            return -1;

        /* set the original file length */
        len -= BLOCK_SIZE;

        /* decrypt from position len to position len + BLOCK_SIZE */
        aes_dec_blk(dbuf + len, dbuf + BLOCK_SIZE, ctx);

        /* undo CBC chaining */
        for(i = 0; i < len; ++i)
            dbuf[i] ^= dbuf[i + BLOCK_SIZE];

        /* output decrypted bytes */
        memcpy(output, dbuf, len);
        output += len;

    } else {
        /* we need two input buffers because we have to keep the previous */
        /* ciphertext block - the pointers b1 and b2 are swapped once per */
        /* loop so that b2 points to new ciphertext block and b1 to the */
        /* last ciphertext block */

        rlen -= BLOCK_SIZE; b1 = buf1; b2 = buf2;

        /* input the IV */
        memcpy(b1, input, BLOCK_SIZE);
        input += BLOCK_SIZE;
        /*if(fread(b1, 1, BLOCK_SIZE, fin) != BLOCK_SIZE)*/
            /*return READ_ERROR;*/
        
        /* read the encrypted file a block at a time */
        while(rlen > 0 )
        {
            /* input a block and reduce the remaining byte count */
            len = BLOCK_SIZE;
            memcpy(b2, input, BLOCK_SIZE);
            input += BLOCK_SIZE;
            /*len = (unsigned long)fread(b2, 1, BLOCK_SIZE, fin);*/
            rlen -= len;

            /* verify the length of the read operation */
            /*if(len != BLOCK_SIZE)*/
                /*return READ_ERROR;*/

            /* decrypt input buffer */
            aes_dec_blk(b2, dbuf, ctx);

            /* if there is only one more block do ciphertext stealing */
            if(rlen > 0 && rlen < BLOCK_SIZE)
            {
                /* read last ciphertext block */
                memcpy(b2, input, rlen);
                input+= rlen;
                /*if(fread(b2, 1, rlen, fin) != rlen)*/
                    /*return READ_ERROR;*/

                /* append high part of last decrypted block */
                for(i = rlen; i < BLOCK_SIZE; ++i)
                    b2[i] = dbuf[i];

                /* decrypt last block of plaintext */
                for(i = 0; i < rlen; ++i)
                    dbuf[i + BLOCK_SIZE] = dbuf[i] ^ b2[i];

                /* decrypt last but one block of plaintext */
                aes_dec_blk(b2, dbuf, ctx);

                /* adjust length of last output block */
                len = rlen + BLOCK_SIZE; rlen = 0;
            }

            /* unchain CBC using the last ciphertext block */
            for(i = 0; i < BLOCK_SIZE; ++i)
                dbuf[i] ^= b1[i];

            /* write decrypted block */
            memcpy(output, dbuf, len);
            output+=len;
            /*if(fwrite(dbuf, 1, len, fout) != len)*/
                /*return WRITE_ERROR;*/

            /* swap the buffer pointers */
            bt = b1, b1 = b2, b2 = bt;
        }
        /*fprintf(stderr, "len: %d diff len: %d\n", */
                /*input_length, (int)(input-input_buffer));*/
    }
    return 0;
}

#endif



