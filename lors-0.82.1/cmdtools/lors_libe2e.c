#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "lors_api.h"
#include "jval.h" 
#include "jrb.h"
#include "dllist.h"
#include "lors_error.h"
#include "exnode.h"
#include "libend2end.h"

#if 0

int lors_ConditionMapping(char *input_buffer,
			  unsigned long int input_length,
			  unsigned long int e2e_blocksize,
			  unsigned char **output_buffer,
			  unsigned long int *output_length,
			  Dllist functions)
{
  Dllist temp;
  LorsConditionStruct *aCondition;
  ExnodeMetadata *arguments;
  int r, count=0, first, last;
  char *result, *rtemp;
  unsigned long int physical_size, tlen;

  dll_traverse(temp,functions)
    count++;
  last = count;
  count = 0;

  r = ConditionMapping(input_buffer, input_length, &result, &physical_size, e2e_blocksize,0); 
  if(r != 0)
    goto RET_POINT;
  
  dll_traverse(temp,functions){
    aCondition = (LorsConditionStruct *)(temp->val.v);
    if(!strcmp(aCondition->function_name,"DES_ENC")){
      fprintf(stderr,"DES ENCRYPTION\n");
      arguments = (ExnodeMetadata *)aCondition->arguments;
      r = aCondition->condition_data(result, input_length, e2e_blocksize,
                                     &rtemp, &tlen, arguments);
      if(r!=0)
	goto RET_POINT;
      if(count == last-1){
	*output_buffer = rtemp;
	*output_length = tlen;
      }
      else{
	result = rtemp;
	physical_size = tlen;
      }
    }
    else if(!strcmp(aCondition->function_name,"MD5_CHK")){
      fprintf(stderr,"CHECKSUM\n");
      r = aCondition->condition_data(result,input_length,e2e_blocksize,&rtemp,&tlen,arguments);
      if(count == last-1){
	*output_buffer = rtemp;
	*output_length = physical_size;
      }
      else{
      	result = rtemp;
	physical_size = tlen;
      }
    }
    else if(!strcmp(aCondition->function_name,"XOR_ENC")){
      fprintf(stderr,"XOR ENCRYPT\n");
      arguments = (ExnodeMetadata *)aCondition->arguments;
      r = aCondition->condition_data(result,input_length,e2e_blocksize,&rtemp,&tlen,arguments);
      if(count == last-1){
	*output_buffer = rtemp;
	*output_length = tlen;
      }
      else{
	result = rtemp;
	physical_size = tlen;
      }
    }
    count++;
  }
 

 RET_POINT:
  return r;
}

int lors_UnconditionMapping(unsigned char *input_buffer,
			  unsigned long int input_length,
			  unsigned long int e2e_blocksize,
			  char **output_buffer,
			  unsigned long int *output_length,
			  Dllist functions)
{
  Dllist temp;
  LorsConditionStruct *aCondition;
  ExnodeMetadata *arguments;
  int r, count=0, first, last;
  unsigned char *result;
  char *rtemp;
  unsigned long int logical_size, tlen;

  dll_traverse(temp,functions)
    count++;
  last = count;
  count = 0;

  result = input_buffer;
  dll_rtraverse(temp,functions){
    aCondition = (LorsConditionStruct *)(temp->val.v);
    if(!strcmp(aCondition->function_name,"DES_ENC")){
      fprintf(stderr,"DES DECRYPTION\n");
      arguments = (ExnodeMetadata *)aCondition->arguments;
      r = aCondition->uncondition_data(result,input_length,e2e_blocksize,&rtemp,&tlen,arguments);
      result = rtemp;
      logical_size = tlen;
    }
    else if(!strcmp(aCondition->function_name,"MD5_CHK")){
      fprintf(stderr,"VERIFY CHECKSUM\n");
      r = aCondition->condition_data(result,input_length,e2e_blocksize,&rtemp,&tlen,arguments);
      result = rtemp;
      logical_size = tlen;
    }
    else if(!strcmp(aCondition->function_name,"XOR_ENC")){
      fprintf(stderr,"XOR ENCRYPT\n");
      arguments = (ExnodeMetadata *)aCondition->arguments;
      r = aCondition->condition_data(result,input_length,e2e_blocksize,&rtemp,&tlen,arguments);
      result = rtemp;
      logical_size = tlen;
    }
    count++;
  }

  r = UnconditionMapping(result, input_length, output_buffer, output_length, e2e_blocksize);

 RET_POINT:
  return r;
}
#endif

void gen_key(char *key)
{
   int i, r;
   time_t tt;
   static once;

   if ( once == 0 )
   {
      srand(time(0));
      once++;
   }
   for (i=0; i<7; i++){
     r = (int)(rand()*94.0/RAND_MAX + 33); /* random from 33 to 126*/
     if(r==60 || r==62){
       i--;
       continue;
     }
     else
       key[i] = r;
   }
   key[7] = 0;
}

/* len should be a multiple of 4 */
void hex_key_gen(char *key, int len)
{
   int i, r, offset = 0;
   static once;

   if ( once == 0 )
   {
      srand(time(0));
      once++;
   }
   for (i=0; i<len; i+=4){
     r = rand();
     offset += sprintf(key+offset, "%04x", r);
   }
   key[len] = 0;
}

/* key is assumed to be 'len' bytes long, and a multiple of 2 */
void make_binary_key(char *key, int len)
{
    int i, j, r, offset = 0;
    static once;

    if ( once == 0 )
    {
        srand(time(0));
        once++;
    }
    for (i=0; i<len; i+=2){
        r = rand();
        for (j=0; j<2; j++)
        {
            key[i+j] = (char*)(&r)[j];
        }
    }
    return;
}

void make_hex_from_binary(char *bin_key, int len, char **hex_key)
{
    int     i, offset;
    char    *tmp_key;

    tmp_key = (char *)calloc(sizeof(char), len*2);
    if ( tmp_key == NULL )
    {
        *hex_key = NULL;
    }
    offset = 0;
    for(i=0; i < len; i++)
    {
        offset += sprintf(tmp_key+offset, "%02x", (unsigned char)bin_key[i]);
    }
    *hex_key = tmp_key;
    return;
}

void make_binary_from_hex(char *hex_key, char **bin_key, int *len)
{
    char        *tmp_bin;
    int         slen;
    int         i, offset;
    int         d;

    slen=strlen(hex_key);
    offset = 0;
    tmp_bin = (char *)calloc(sizeof(char), slen/2);
    for(i=0; i < slen; i+=2)
    {
        sscanf(hex_key+offset, "%2x", &d);
        tmp_bin[i/2] = (unsigned char)d;
        offset += 2;
    }
    *bin_key = tmp_bin;
    *len = slen/2;

    return;
}


int lors_des_encrypt(char *input_buffer,
		             unsigned long int input_length,
		             unsigned long int e2e_blocksize,
		             char **output_buffer,
		             unsigned long int *output_length,
                     unsigned long int *output_blocksize,
		             ExnodeMetadata *arguments)
{
  int         r;
  char  deskey[8];

  ExnodeValue val;
  ExnodeType  type;
  int         header_length = 0;

  memset(&val, 0, sizeof(ExnodeValue));
  gen_key(deskey);

  r = des_EncryptMapping(input_buffer,input_length, e2e_blocksize, 
                         output_buffer, output_length, output_blocksize, deskey);
  val.s = deskey;
  exnodeSetMetadataValue(arguments, "KEY", val, STRING, TRUE);

  /*
  val.i = header_length;
  exnodeSetMetadataValue(arguments, "header_length", val, INTEGER, TRUE);
  */


  /**output_blocksize = e2e_blocksize + header_length;*/
  /*assert( (*output_blocksize == *output_length) );*/
  

  lorsDebugPrint(D_LORS_VERBOSE, "in_len: %d, in_bs: %d,  out_len: %d, out_bs: %d\n",
                    input_length, e2e_blocksize, *output_length, *output_blocksize);
  /* TODO: make sure this is correct for all cases of e2e_blocksize. */
  /*fprintf(stderr, "output_blocksize: %d, output_length: %d, header_len: %d\n",*/
                    /**output_blocksize, *output_length, header_length);*/

  val.i = *output_blocksize; /*e2e_blocksize + header_length;*/
  exnodeSetMetadataValue(arguments, "blocksize", val, INTEGER, TRUE);

  lorsDebugPrint(D_LORS_VERBOSE, "DES_ENCRYPTED: %d \n", r);

  if(!r)
    r = LORS_SUCCESS;
 RET_POINT:
  return r;
} 


int lors_des_decrypt(char *input_buffer,
		             unsigned long int input_length,
		             unsigned long int e2e_blocksize,
		             char             **output_buffer,
		             unsigned long int *output_length,
		             unsigned long int *output_blocksize,
		             ExnodeMetadata *arguments)
{
    int r;
  
    ExnodeValue val;
    ExnodeType type;
    ulong_t   blocksize;
    char    *key;

    memset(&val, 0, sizeof(ExnodeValue));
    r = exnodeGetMetadataValue(arguments, "KEY", &val, &type);
    key = val.s;
    if(r!= EXNODE_SUCCESS ){
        fprintf(stderr,"lors_libe2e: wrong argument");
        goto RET_POINT;
    }

    exnodeGetMetadataValue(arguments, "blocksize", &val, &type);
    blocksize = (ulong_t)val.i; 

    r = des_DecryptMapping(input_buffer,input_length,blocksize,output_buffer,output_length,key);
    lorsDebugPrint(D_LORS_VERBOSE, "DES_DECRYPT: %d \n", r);
    /***fprintf(stderr,"[%d]DECRYPTED: <%s> \n%d\n",r,*output_buffer,*output_length);*/
    if(!r)
        r=LORS_SUCCESS;
    RET_POINT:
        return r;
}

int lors_checksum(char *input_buffer,
		  unsigned long int input_length,
		  unsigned long int e2e_blocksize,
		  char **output_buffer,
		  unsigned long int *output_length,
          unsigned long int *output_blocksize,
		  ExnodeMetadata *arguments)
{
  int                   r;
  unsigned char        *result;
  /*int                   header_len;*/
  ExnodeValue           val;
  r = ChecksumMapping(input_buffer, input_length, e2e_blocksize,  
                      output_buffer, output_length, output_blocksize);

  memset(&val, 0, sizeof(ExnodeValue));
#if 0
  val.i = *output_length;
  exnodeSetMetadataValue(arguments, "output_length", val, INTEGER, TRUE);
#endif

  /*
  val.i = header_len;
  exnodeSetMetadataValue(arguments, "header_length", val, INTEGER, TRUE);

  *output_blocksize = e2e_blocksize + header_len;
  */
  val.i = *output_blocksize;
  exnodeSetMetadataValue(arguments, "blocksize", val, INTEGER, TRUE);

  lorsDebugPrint(D_LORS_VERBOSE, "CHECKSUMMED: %d \n", r);
  /**output_length = input_length; */
  r=LORS_SUCCESS;
  return r;
}


int lors_verify_checksum(char *input_buffer,
			 unsigned long int input_length,
			 unsigned long int e2e_blocksize, /* ?? found in arguments */
			 char **output_buffer,
			 unsigned long int *output_length,
             unsigned long int *output_blocksize,
			 ExnodeMetadata *arguments)
{
    int r;
    ExnodeValue   val;
    ExnodeType    type = NONE;
    ulong_t       blocksize;
    val.i = 0;
  
    memset(&val, 0, sizeof(ExnodeValue));
    exnodeGetMetadataValue(arguments, "blocksize", &val, &type);
    blocksize = (ulong_t)val.i; 
    /*exnodeGetMetadataValue(arguments, "header_length", &val, &type);*/

    r = VerifyChecksumMapping(input_buffer, input_length, blocksize, output_buffer, 
                              output_length);
    lorsDebugPrint(D_LORS_VERBOSE, "UN-CHECKSUM: %d\n",r);
  
    if(!r)
      r=LORS_SUCCESS;
    return r;
}


int lors_xor_encrypt(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments)
{
  int r;
  ExnodeValue val;
  ExnodeType type;
  char  xorkey[8];

  gen_key(xorkey);
  /*xorkey[4] = '\0';*/ /* truncate to 4 bytes, for integer xor  */

  memset(&val, 0, sizeof(ExnodeValue));
  r = xor_EncryptMapping(input_buffer,input_length, e2e_blocksize, 
                         output_buffer, output_length, 
                         output_blocksize, xorkey);
  val.s = xorkey;
  exnodeSetMetadataValue(arguments, "KEY", val, STRING, TRUE);

  /*
  val.i = header_length;
  exnodeSetMetadataValue(arguments, "header_length", val, INTEGER, TRUE);
  */

  
  lorsDebugPrint(D_LORS_VERBOSE, "output_blocksize: %d, output_length: %d\n",
                    *output_blocksize, *output_length);

  val.i = *output_blocksize;
  exnodeSetMetadataValue(arguments, "blocksize", val, INTEGER, TRUE);

  lorsDebugPrint(D_LORS_VERBOSE, "XOR_ENCRYPTED: %d \n", r);
  if(!r)
    r = LORS_SUCCESS;
 RET_POINT:
  return r;

}

int lors_xor_decrypt(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments)
{
    int r;
    ExnodeValue val;
    ExnodeType type;
    char *key;
    ulong_t blocksize;

    memset(&val, 0, sizeof(ExnodeValue));
    r = exnodeGetMetadataValue(arguments, "KEY", &val, &type);
    if(r!=0){
        fprintf(stderr,"lors_libe2e: wrong argument");
        goto RET_POINT;
    }
    key = val.s;

    exnodeGetMetadataValue(arguments, "blocksize", &val, &type);
    blocksize = (ulong_t)val.i; 

    r = xor_DecryptMapping(input_buffer, input_length,
                         blocksize, output_buffer, output_length, key);
    lorsDebugPrint(D_LORS_VERBOSE, "XOR_DECRYPT: %d \n", r);
    if(!r)
        r=LORS_SUCCESS;
    RET_POINT:
      return r;
}


int lors_zlib_compress(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments)
{
  int r = 0;
  ExnodeValue val;
  ExnodeType type;
  /*int header_length = 0;*/

  memset(&val, 0, sizeof(ExnodeValue));
  /* e2e_blocksize is ignored.  the input length equals the blocksize */
  r = CompressMapping(input_buffer,input_length, 
                      input_length, output_buffer, output_length, 
                      output_blocksize);

  /*
  val.i = header_length;
  exnodeSetMetadataValue(arguments, "header_length", val, INTEGER, TRUE);
  */
  /**output_blocksize = e2e_blocksize + header_length;*/

  val.i = *output_blocksize;
  exnodeSetMetadataValue(arguments, "blocksize", val, INTEGER, TRUE);

  lorsDebugPrint(D_LORS_VERBOSE, "ZLIB_COMPRESSED\n");
  if(!r)
    r = LORS_SUCCESS;
 RET_POINT:
  return r;

}
int lors_zlib_uncompress (char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments)
{
    int r;
    ExnodeValue val;
    ExnodeType type;
    ulong_t blocksize;

    memset(&val, 0, sizeof(ExnodeValue));

    r = UncompressMapping(input_buffer, input_length,
                          output_buffer, output_length);
    lorsDebugPrint(D_LORS_VERBOSE, "ZLIB_UNCOMPRESS\n");
    if(!r)
        r=LORS_SUCCESS;
    RET_POINT:
      return r;
}

int lors_aes_encrypt(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments)
{
    char        aeskey[16+1];
    char        *hex_aeskey = NULL;

    ExnodeValue val;
    ExnodeType  type;
    int         header_length = 0;
    int         ret;
    int         i;

    memset(&val, 0, sizeof(ExnodeValue));
    /*hex_key_gen(aeskey, 16);*/
    make_binary_key(aeskey, 16);


    fprintf(stderr, "aes_binkey:\n");
    for (i=0; i < 16; i++)
    {
        fprintf(stderr, "%02x", (unsigned char)aeskey[i]);
    }
    fprintf(stderr, "\n");

    ret = aes_EncryptMapping(input_buffer,input_length, e2e_blocksize, 
                output_buffer, output_length, output_blocksize, aeskey);

    if ( ret != 0 )
    {
        return -1;
    }
    make_hex_from_binary(aeskey, 16, &hex_aeskey);
    /*val.s = aeskey;*/
    val.s = hex_aeskey;


    fprintf(stderr, "hex_key: %s\n", hex_aeskey);
    exnodeSetMetadataValue(arguments, "KEY", val, STRING, TRUE);
    free(hex_aeskey);

    lorsDebugPrint(D_LORS_VERBOSE, "in_len: %d, in_bs: %d,  out_len: %d, out_bs: %d\n",
                    input_length, e2e_blocksize, *output_length, *output_blocksize);

    val.i = *output_blocksize; /*e2e_blocksize + header_length;*/
    exnodeSetMetadataValue(arguments, "blocksize", val, INTEGER, TRUE);

    return ret;
}

int lors_aes_decrypt (char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments)
{
    int         ret;
  
    ExnodeValue val;
    ExnodeType  type;
    ulong_t     blocksize;
    char        *key;
    char        *bin_key;
    int         len;
    int         i;

    memset(&val, 0, sizeof(ExnodeValue));
    ret = exnodeGetMetadataValue(arguments, "KEY", &val, &type);
    if(ret != EXNODE_SUCCESS ){
        fprintf(stderr,"lors_libe2e: missing KEY parameter from serialization.");
        return -1;
    }
    key = val.s;
    make_binary_from_hex(key, &bin_key, &len);

    if ( len != 16 )
    {
        fprintf(stderr, "encryption key is not the proper length.\n");
        return -1;
    }
    fprintf(stderr, "bin_key:\n");
    for (i=0; i < 16; i++)
    {
        fprintf(stderr, "%02x", (unsigned char)bin_key[i]);
    }
    fprintf(stderr, "\n");

    ret = exnodeGetMetadataValue(arguments, "blocksize", &val, &type);
    blocksize = (ulong_t)val.i; 

    ret = aes_DecryptMapping(input_buffer,input_length,blocksize,
                        output_buffer,output_length,bin_key);

    free(bin_key);
    if ( ret != LORS_SUCCESS )
    {
        fprintf(stderr, "aes_DecryptMapping() failed\n");
    }

    return ret;
}
