#include <lors_api.h>
#include "dllist.h"

#define LORS_E2E_FIXED          0x01
#define LORS_E2E_VARIABLE       0x02
int lors_ConditionMapping(char *inbuff,
			  unsigned long int inlen,
			  unsigned long int e2ebs,
			  unsigned char **outbuff,
			  unsigned long int *outlen,
			  Dllist functions);


int lors_UnconditionMapping(char *inbuff,
			    unsigned long int inlen,
			    unsigned long int e2ebs,
			    unsigned char **outbuff,
			    unsigned long int *outlen,
			    Dllist functions);


int lors_des_encrypt(char *input_buffer,
		             unsigned long int input_length,
		             unsigned long int e2e_blocksize,
		             char **output_buffer,
		             unsigned long int *output_length,
                     unsigned long int *output_blocksize,
		             ExnodeMetadata *arguments);


int lors_des_decrypt(char *input_buffer,
		             unsigned long int input_length,
		             unsigned long int e2e_blocksize,
		             char             **output_buffer,
		             unsigned long int *output_length,
		             unsigned long int *output_blocksize,
		             ExnodeMetadata *arguments);

int lors_checksum(char *input_buffer,
		  unsigned long int input_length,
		  unsigned long int e2e_blocksize,
		  char **output_buffer,
		  unsigned long int *output_length,
          unsigned long int *output_blocksize,
		  ExnodeMetadata *arguments);


int lors_verify_checksum(char *input_buffer,
			 unsigned long int input_length,
			 unsigned long int e2e_blocksize, /* ?? found in arguments */
			 char **output_buffer,
			 unsigned long int *output_length,
             unsigned long int *output_blocksize,
			 ExnodeMetadata *arguments);


int lors_xor_encrypt(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments);

int lors_xor_decrypt(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments);
int lors_zlib_compress(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments);
int lors_zlib_uncompress (char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments);


int lors_aes_decrypt (char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments);
int lors_aes_encrypt(char *input_buffer,
		     unsigned long int input_length,
		     unsigned long int e2e_blocksize,
		     char **output_buffer,
		     unsigned long int *output_length,
		     unsigned long int *output_blocksize,
		     ExnodeMetadata *arguments);
