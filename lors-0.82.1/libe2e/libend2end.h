#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>
#ifndef _MINGW
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#define E2EDEBUG 0
#define MAXBLOCKSIZE 10000
#define HASH_SIZE 20

#define E2EFAIL -1
#define OK    1

#define E2EBLOCK_SIZE    (1024*64)   /* 1024*64 is default size */
#define KEY_SIZE      64    /* this is for DES encryption */
#define ENC_NONE      0     /* use no encryption at all */
#define ENC_DES       1     /* specifies DES encryption type */
#define ENC_XOR       2     /* specifies to use XOR encryption type this is default*/

/* ---------------------------------------------------------------------------------------
   the header format should look like this when everything is completed

   seq_num:real_size:encryption_type:compression_type:checksum
   8 byte    5byte     1 byte           1 byte        32 byte

   seq_num ->          is the sequence number
   real_size ->        size of the data in this block, in case there was padding
   compression_type -> if no compression its value is 0 otherwise from 1-9 indicating type
   encryption_type ->  if no encryption its value is 0 otherwise from 1-9 indicating type
   checksum ->         constant size of 32 byte in HEX representation
   : ->                field separator  "SEP"
   --------------------------------------------------------------------------------------- */
#define CH_SIZE       32
#define REAL_SIZE     12
#define SEQ_SIZE      0
#define COMP_TYPE     1
#define ENCR_TYPE     1
#define SEP           1
#define HEADER_SIZE   ((REAL_SIZE)+(SEP)+(ENCR_TYPE)+(SEP)+(COMP_TYPE)+(SEP)+(CH_SIZE))
#define HEADER_FRONT  ((HEADER_SIZE)-(CH_SIZE))

#define E2EMIN(a,b)  (((a)<(b))?(a):(b))
#define MOD(a,b)  ((a)-(((int)((a)/(b)))*(b)))

#define E2E_MEMERROR -1

/* ---------------------------------------------------------------------------------
   Header sizes definitions for the current e2e lib functions including : separator
   --------------------------------------------------------------------------------- */
#define REAL_SIZE_H ((REAL_SIZE) + SEP)
#define ENCRYPT_H   ((ENCR_TYPE) + SEP)
#define COMPR_H     ((REAL_SIZE) + SEP)
#define CHECKSUM_H  ((CH_SIZE) + SEP)

/* ---------------------------------------------------------------------------------
   E2E functions prototypes
   --------------------------------------------------------------------------------- */
char* create_header(char *value, int header_size);
void destroy_header(char *header);
unsigned char* padd_block(unsigned char *block, long int len, long int e2ebs);



int ConditionMapping(char *clear_mapping,
		     unsigned long int logical_size,
		     char **e2emapping,
		     unsigned long int *physical_size,
		     unsigned long int e2ebs,
		     int *header_size);

int ChecksumMapping(char *input_buffer,
		            unsigned long int input_length,
		            unsigned long int input_blocksize,
		            char **output_buffer,
		            unsigned long int *output_length,
		            unsigned long int *output_blocksize);
int VerifyChecksumMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length);
/*
int ChecksumMapping(unsigned char *mapping,
		    unsigned long int logical_size,
		    unsigned char **chmapping,
		    unsigned long int *new_size,
		    unsigned long int e2ebs,
		    int *header_size);


int VerifyChecksumMapping(unsigned char *chMapping,
			  unsigned long int physical_size,
			  unsigned char **clearMapping,
			  unsigned long int *out_size,
			  unsigned long int e2ebs);
*/

/*
int des_EncryptMapping(char *clearMapping,
		       unsigned long int logical_size,
		       unsigned char **encMapping,
		       unsigned long int *physical_size,
		       unsigned long int e2ebs,
		       char *key,
		       int *header_size);

int des_DecryptMapping(unsigned char *encMapping,
		       unsigned long int physical_size,
		       char **clearMapping,
		       unsigned long int *out_size,
		       unsigned long int e2ebs,
		       char *key);
*/
int des_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key);
int des_DecryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key);

int aes_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key);
int aes_DecryptMapping(unsigned char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key);

/*
int xor_EncryptMapping(char *clearMapping,
		       unsigned long int logical_size,
		       unsigned char **encMapping,
		       unsigned long int *physical_size,
		       unsigned long int e2ebs,
		       char *key,
		       int *header_size);

int xor_DecryptMapping(unsigned char *encMapping,
		       unsigned long int physical_size,
		       char **clearMapping,
		       unsigned long int *out_size,
		       unsigned long int e2ebs,
		       char *key);
*/

int xor_EncryptMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize,
		       char *key);
int xor_DecryptMapping(unsigned char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       char *key);

int UnconditionMapping(char *clearMapping,
		       unsigned long int physical_size,
		       char **userMapping,
		       unsigned long int *user_size,
		       unsigned long int e2ebs);


/*
int CompressMapping(char *clearMapping,
		    unsigned long int logical_size,
		    unsigned char **compMapping,
		    unsigned long int *physical_size,
		    unsigned long int e2ebs,
		    int *header_size);

int UncompressMapping(unsigned char *compMapping,
		      unsigned long int physical_size,
		      char **clearMapping,
		      unsigned long int *out_size,
						   unsigned long int e2ebs);*/

int CompressMapping(char *input_buffer,
		       unsigned long int input_length,
		       unsigned long int input_blocksize,
		       char **output_buffer,
		       unsigned long int *output_length,
		       unsigned long int *output_blocksize);
int UncompressMapping(unsigned char *input_buffer,
		      unsigned long int input_length,
		      char **output_buffer,
		      unsigned long int *output_length);

int logical2physical(unsigned long int original_phys_size,
		     unsigned long int original_e2ebs,
		     unsigned long int logical_offset,
		     unsigned long int logical_size,
		     unsigned long int *physical_offset,
		     unsigned long int *physical_size,
		     unsigned long int accum_bytes);


