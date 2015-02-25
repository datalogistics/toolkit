#ifndef __LORS_FILE_H__

#define __LORS_FILE_H__
#include <lors_api.h>
#include <lors_file_opts.h>

typedef struct {
    int  size;
    int  buf_size;
    char  *buf;
    int   fd;
    int   ret_size;
    int   status;
    longlong offset;  
} _LorsFileJob;

typedef struct {
    _LorsFileJob *queue;
    int          total_buffer;
    int          pre_buffer;
    pthread_mutex_t lock;
    pthread_cond_t  no_full_cond;
    pthread_cond_t  no_empty_cond;
    int          head;
    int          tail;
    int          n_buf_filled;
    int          status;
    int          errornumber;
}_LorsFileJobQueue;


void *lorsReadFile ( void *para );
void *lorsWriteFile ( void *para );
int lorsPipeFile(char      *filename,
                 char      *output_filename,
                 longlong   offset,
                 longlong   length,
                 ulong_t    data_blocksize, 
                 int        copies, 
                 ulong_t    e2e_blocksize, 
                 int       *e2e_order, 
                 IBP_depot  lbone_server, 
                 IBP_depot *depot_list,
                 char      *location,
                 int        depot_cnt,
                 int        storage_type,
                 time_t     duration,
                 ulong_t    max_internal_buffer,
                 char      *resolution_file,
                 int        timeout, 
                 int        opts);

int lorsUploadFile( char      *filename, 
                    char      *output_filename,
                   longlong   offset,
                   longlong   length,
                   ulong_t    data_blocksize, 
                   int        copies, 
                   int        fragments,
                   ulong_t    e2e_blocksize, 
                   int       *e2e_order, 
                   IBP_depot  lbone_server, 
                   IBP_depot *depot_list,
                   char      *location,
                   int        max,
                   int        storage_type,
                   time_t     duration,
                   ulong_t max_internal_buffer,
                   char      *resolution_file,
                   int        nthreads,
                   int        timeout, 
                   int        opts);

int lorsDownloadFile(char       *exnode_filename, 
                     char       *filename, 
                     longlong   offset,
                     longlong   length,
                     ulong_t    data_blocksize,
                     ulong_t    max_internal_buffer,
                     int        pre_buffer,
                     int        dp_threads,
                     int        job_threads,
                     int        progress,
                     int        cache_buffer,
                     char       *lbone_host,
                     int        lbone_port,
                     char       *location,
                     char       *resolution_file,
                     int        nthreads,
                     int        timeout, 
                     int        opts);

int lorsRefreshFile(char *filename, 
                 time_t duration, 
                 int threads, 
                 int timeout, 
                 int options);

int lorsListFile(char *filename, 
                 int threads, 
                 int timeout, 
                 int options);

#if 0
int lorsAugmentFile(LorsExnode **new_exnode,
                    LorsExnode **aug_exnode,
                   char      *exnode_filename, 
                   ulong_t   offset,
                   ulong_t   length,
                   ulong_t    data_blocksize, 
                   int        copies, 
                   int        fragments,
                   IBP_depot  lbone_server, 
                   IBP_depot *depot_list,
                   char      *location,
                   int        max,
                   int        storage_type,
                   time_t     duration,
                   ulong_t    max_internal_buffer,
                   int        nthreads,
                   int        timeout, 
                   int        opts);
#endif


int lorsAugmentFile(LorsExnode **new_exnode,
                    LorsExnode **aug_exnode,
                   char      *exnode_filename,
                   longlong   offset,
                   longlong   length,
                   ulong_t    data_blocksize,
                   int        copies,
                   int        fragments,
                   IBP_depot  lbone_server,
                   IBP_depot *depot_list1,
                   IBP_depot *depot_list2,
                   char      *location,
                   int        max,
                   int        storage_type,
                   time_t     duration,
                   ulong_t    max_internal_buffer,
                   char      *resolution_file,
                   int        nthreads,
                   int        timeout,
                   int        opts);

#endif

