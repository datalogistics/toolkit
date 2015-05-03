#ifndef __LORS_FILE_OPT_H__
#define __LORS_FILE_OPT_H__

#include <lors_opts.h>

#define HARD                  IBP_STABLE
#define SOFT                  IBP_VOLATILE

#define LORS_VERSION          0x00000003
#define VERBOSE               0x00000005
#define LBONESERVER           0x00000006
#define LBONEPORT             0x00000007
#define DURATION              0x00000009
#define LOCATION_HINT         0x00000011
#define STORAGE_TYPE_SOFT     0x00000012
#define STORAGE_TYPE_HARD     0x00000013

#define COPIES                0x00000014
#define DATA_BLOCKSIZE        0x00000015
#define MAX_BUFFERSIZE        0x00000016
#define E2E_BLOCKSIZE         0x00000017

#define DEPOT_LIST            0x00000018
#define RCFILE                0x00000019
#define NUM_FRAGMENTS         0x00000021
#define LORS_ARG_DEMO         0x00000022


#define TRIM_SEGMENTS         0x00000023
#define TRIM_DEAD             LORS_TRIM_DEAD
#define TRIM_DECR             LORS_TRIM_DECR
#define TRIM_KILL             LORS_TRIM_KILL
#define TRIM_NOKILL           LORS_TRIM_NOKILL
#define REFRESH_MAX           LORS_REFRESH_MAX
#define REFRESH_ADD           LORS_REFRESH_EXTEND_BY
#define REFRESH_SET           LORS_REFRESH_EXTEND_TO
#define REFRESH_ABS           LORS_REFRESH_ABSOLUTE

#define REDUNDANCY            0x00000024
#define PROGRESS              0x00000025
#define THREADDEPOT           0x00000026

#define DES_ENCRYPTION        0x00000027
#define AES_ENCRYPTION        0x00000028
#define XOR_ENCRYPTION        0x00000029
#define ADD_CHECKSUM          0x00000030
#define ZLIB_COMPRESS         0x00000031
#define LORS_NO_E2E           0x00000032

#define SAME_OUTPUTNAME       0x00000033
#define OUTPUTNAME            0x00000034
#define MAX_DEPOTS            0x00000035
#define TIMEOUT               0x00000036
#define THREADS               0x00000037
#define FILE_OFFSET           0x00000038
#define FILE_LENGTH           0x00000039
#define PREBUFFER_SIZE        0x00000041

#define NEWFILENAME           0x00000042

#define BUFFER_SIZE           0x00000043
#define ALL                   0x00000044
#define DEPOTS                0x00000045
#define CACHE_SIZE            0x00000046
#define MCOPY                 0x00000047
#define LBONE_GEO             0x00000048
#define LBONE_NWS             0x00000049
#define READ_STDIN            0x0000004a
#define VISUALIZE_PROGRESS    0x0000004b
#define UPLOAD_DIR            0x0000004c
 
#define MAX_BUFFSIZE            (100 * 1024 * 1024)
#define MIN_BUFFSIZE            (1024)

/* DEBUG levels */
#define D_HOST          	0x0001
#define D_PROCESS          	(0x0002)
#define D_IBP_API           	0x0004
#define D_IBP_VERBOSE      	(0x0008|D_IBP_API)
#define D_LBONE_API         	0x0010
#define D_LBONE_VERBOSE    	(0x0020|D_LBONE_API)
#define D_SYS_API           	0x0040
#define D_SYS_VERBOSE      	(0x0080|D_SYS_API)
#define D_XND_API           	0x0100
#define D_XND_VERBOSE      	(0x0200|D_XND_API)
#define D_NWS_API           	0x0400
#define D_NWS_VERBOSE      	(0x0800|D_NWS_API)
#define D_MISC              	0x1000
#define D_LOCAL_PARAM       	0x2000
#define MAX_DEBUG           	0x4000
#define D_MTHREADS_API		0x8000
#define D_MTHREADS_VERBOSE   	(0x10000|D_MTHREADS_API)

extern int debug_level;
int dbprintf(int db_level, char *format, ... );

#endif
