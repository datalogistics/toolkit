#include <stdio.h>
#include <lors_api.h>
#include <lors_file.h>
#include <lors_util.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <socket_io.h>
#include <xndrc.h>
#include <lors_libe2e.h>
#include <lors_resolution.h>


#define lorsDemoPrint       fprintf

LorsConditionStruct     glob_lc[] = 
{
  {"checksum",      0, lors_checksum,      lors_verify_checksum, LORS_E2E_FIXED, 1, NULL},
  {"des_encrypt",   0, lors_des_encrypt,   lors_des_decrypt, LORS_E2E_FIXED,  1, NULL},
  {"xor_encrypt",   0, lors_xor_encrypt,   lors_xor_decrypt, LORS_E2E_FIXED,  0, NULL},
  {"zlib_compress", 0, lors_zlib_compress, lors_zlib_uncompress, LORS_E2E_VARIABLE, 1, NULL},
  {"aes_encrypt",   0, lors_aes_encrypt,   lors_aes_decrypt, LORS_E2E_FIXED, 1, NULL},
  {NULL,            0, NULL ,                          NULL,              0, 0, NULL},
};

int e2e_order2ConditionStruct(int *e2e_order, 
                              LorsConditionStruct **lc, 
                              ulong_t e2e_blocksize)
{
    LorsConditionStruct *mlc;
    int e2e_cnt = 0;
    int i;

    for(i=0; e2e_order[i] != -1 ;i++);
    e2e_cnt = i;

    mlc = (LorsConditionStruct *)calloc(1, sizeof(LorsConditionStruct)*(e2e_cnt+1));
    if ( mlc == NULL ) return LORS_NO_MEMORY;
    for(i=0; i < e2e_cnt; i++)
    {
        /* associate the real callback with the proper index */
        switch(e2e_order[i])
        {
            case ADD_CHECKSUM:
                mlc[i] = glob_lc[0];
                break;
            case DES_ENCRYPTION:
                mlc[i] = glob_lc[1];
                break;
            case XOR_ENCRYPTION:
                mlc[i] = glob_lc[2];
                break;
            case ZLIB_COMPRESS:
                mlc[i] = glob_lc[3];
                break;
            case AES_ENCRYPTION:
                mlc[i] = glob_lc[4];
                break;
            default:
                break;
        }

        /*fprintf(stderr, "order2: blocksize: %ld\n", e2e_blocksize);*/
        mlc[i].blocksize = e2e_blocksize;
        /*exnodeCreateMetadata(&mlc[i].arguments);*/
    }
    mlc[i].function_name = NULL;
    *lc = mlc;

    return LORS_SUCCESS;
}

void *lorsReadFile ( void *para )
{
    _LorsFileJob  *fileJob;
    int nRead = 0 , ret;
     
    assert(para != NULL );
    fileJob = (_LorsFileJob *)para;
     
    fileJob->status = LORS_SUCCESS;
    
    if ( (fileJob->size < 0 ) || (fileJob->buf_size < 0 ) || 
        (fileJob->buf == NULL) ||(fileJob->fd < 0 ) ||
        (fileJob->size > fileJob->buf_size ) ){
        fileJob->status = LORS_INVALID_PARAMETERS;
        return ;
    };
     
    nRead = 0;
    while ( nRead < fileJob->size )
    { 
        ret = read(fileJob->fd,
                    fileJob->buf+nRead,
                    fileJob->buf_size-nRead);

         /*fprintf(stderr, "READ Returned: %d\n", ret);*/
         if ( ret == -1 ) {
             if ( errno == EINTR ) {
                  continue;
             };
             fileJob->status = LORS_SYS_FAILED + errno;

             fprintf(stderr, "err: %d\n", errno);
             return;
         };
         if ( ret == 0 ) {
             fileJob->ret_size = nRead;
             if ( fileJob->ret_size ==  0 )
             {
                fileJob->status = LORS_EOF;
             } else {
                fileJob->status = LORS_SUCCESS;
             }
             return;
         };
         
         assert ( ret > 0 );
         nRead += ret;
     };

     fileJob->ret_size = nRead;
     return;
     
};

void *lorsWriteFile ( void *para )
{
     _LorsFileJobQueue *fjq;
     _LorsFileJob      *fj;
     int          nWrite,ret;
     char               *buf;

     assert(para != NULL );
     fjq = (_LorsFileJobQueue *)para;

     pthread_mutex_lock(&(fjq->lock));
     while (fjq->n_buf_filled < fjq->pre_buffer ) {
             pthread_cond_wait(&(fjq->no_empty_cond), &(fjq->lock));
     };
     pthread_mutex_unlock(&(fjq->lock));
     
     while ( 1 ){
        pthread_mutex_lock(&(fjq->lock));
        while ( fjq->n_buf_filled <= 0 ) { 
            pthread_cond_wait(&(fjq->no_empty_cond),&(fjq->lock));
        };
        pthread_mutex_unlock(&(fjq->lock));
        nWrite = 0;
        buf = fjq->queue[fjq->tail].buf;
        if ( fjq->queue[fjq->tail].size == 0 ) { 
            pthread_cond_signal(&(fjq->no_full_cond));
            return; 
        };
        while (nWrite < fjq->queue[fjq->tail].size ) {
        if(g_lors_demo) {
#ifdef _MINGW
        lorsDemoPrint(stderr, "DRAW Output %I64d %ld white\n", 
                fjq->queue[fjq->tail].offset, fjq->queue[fjq->tail].size);
        fflush(stderr);
#else
        lorsDemoPrint(stderr, "DRAW Output %lld %ld white\n", 
                fjq->queue[fjq->tail].offset, fjq->queue[fjq->tail].size);
#endif
        }
            ret = write(fjq->queue[fjq->tail].fd,buf+nWrite, fjq->queue[fjq->tail].size - nWrite);
            if ( ret < 0 ){
                if ( errno == EINTR ) {
                  continue;
                };
                pthread_mutex_lock(&(fjq->lock));
                fjq->status = LORS_SYS_FAILED + errno;
                fjq->n_buf_filled = 0;
                perror("Errors occur in writting output file:");
                pthread_cond_signal(&(fjq->no_full_cond));
                pthread_mutex_unlock(&(fjq->lock));
                return;
            };

            nWrite += ret;
            if ( ret == 0 && nWrite != fjq->queue[fjq->tail].size ){
                pthread_mutex_lock(&(fjq->lock));
                fjq->status = LORS_SYS_FAILED + errno;
                fjq->n_buf_filled = 0;
                perror("Errors occur in writting output file:");
                pthread_cond_signal(&(fjq->no_full_cond));
                pthread_mutex_unlock(&(fjq->lock));
                return;
            };
        };

        fjq->queue[fjq->tail].ret_size = nWrite;

        if ( g_lors_demo )
        {
#ifdef _MINGW
            lorsDemoPrint(stderr, "DRAW Output %I64d %ld white\n", 
                fjq->queue[fjq->tail].offset,fjq->queue[fjq->tail].size);
#else
            lorsDemoPrint(stderr, "DRAW Output %lld %ld white\n", 
                fjq->queue[fjq->tail].offset,fjq->queue[fjq->tail].size);
#endif
                fflush(stderr);
        }

        pthread_mutex_lock(&(fjq->lock));
        fjq->tail = ( fjq->tail + 1 ) % fjq->total_buffer;
        fjq->n_buf_filled -- ;
        assert (fjq->n_buf_filled >= 0 );
        pthread_cond_signal(&(fjq->no_full_cond));
        pthread_mutex_unlock(&(fjq->lock));
    };

};


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
                   ulong_t    max_internal_buffer,
                   char      *resolution_file,
                   int        nthreads,
                   int        timeout,
				   socket_io_handler      *handle,
					int        opts)
{
    struct stat          mystat;
    char                *file_shortname;
    char                *buffer;
    char                 *buf_array[2];
    int                  fd = -1;
    int                  ret;
    int                  sret = LORS_FAILURE;
    int                  cret = LORS_FAILURE;
    int                  ith = 0; 
    int                  wait_thread = 1;
    ulong_t              size = 0;
    ulong_t              max_cap_of_buffer;
    ulong_t              max_cap_of_file;
    ulong_t              written;
    LorsConditionStruct *lc = NULL;
    time_t               begin_time,left_time,pass_time;
    long                 file_read = 0;
    pthread_t            tid;
    double               duration_days;
    LboneResolution     *lr = NULL;
	LorsSet             *set = NULL,*copy_set = NULL;
    LorsDepotPool       *dpp = NULL;
    LorsExnode          *xnd = NULL;
    ExnodeMetadata      *emd = NULL;
    ExnodeValue          val;
    _LorsFileJob        fileJob;
    double              temp_size = 0;
    ulong_t             storage_size = 0;
	double              t1, t2;
    double              demo_len;
    int                 l_stdin =0;
    longlong            soffset = 0;

start_over:
    if ( g_lors_demo )
    {
        fprintf(stderr,"\nCLEAR\n");
            fflush(stderr);
    }

    begin_time = time(0);

    file_shortname = strrchr(filename, '/');
    if ( file_shortname == NULL )
    {
        file_shortname = filename;
    } else 
    {
        file_shortname++;
    }

    if ( strcmp(filename, "-") == 0 )
    {
        fd = 0;
        l_stdin = 1;
        if ( length == 0 )
        {
            fprintf(stderr, "Must have a length estimate.\n");
            return LORS_FAILURE;
        }
        if ( fragments > 0 )
        {
            fprintf(stderr, "Fragments are not recognized when reading from "
                            "standard input.\nUse blocksize instead.\n");
            return LORS_FAILURE;
        }
    } else {
        ret = stat(filename, &mystat);
        if ( ret == -1 )
        {
            fprintf(stderr, "lstat(%s) error:%s\n", filename, strerror(errno));
            return LORS_FAILURE;
        }
#ifdef _MINGW
        if ( !S_ISREG(mystat.st_mode) )
#else
        if ( !S_ISREG(mystat.st_mode) && !S_ISLNK(mystat.st_mode)  )
#endif
        {
            fprintf(stderr, "%s: not a regular file\n", filename);
            return LORS_FAILURE;
        }
#ifdef _MINGW
        fd = open(filename, O_RDONLY|O_BINARY);
#else
        fd = open(filename, O_RDONLY);
#endif
        if ( fd == -1)
        {
            fprintf(stderr, "open(%s) failed: %s\n", filename, strerror(errno));
            return LORS_SYS_FAILED - errno;
        }
        if ( length <= 0  )
        {
            /*fprintf(stderr, "length: %d \n", mystat.st_size);*/
            length = mystat.st_size;
        }
        if ( fragments > 0 )
        {
            data_blocksize = (mystat.st_size/fragments) + (mystat.st_size%fragments);
        } 
        if ( data_blocksize > max_internal_buffer )
        {
            max_internal_buffer = data_blocksize;
            /*return LORS_INVALID_PARAMETERS;*/
        }
    }

    fprintf(stderr, "TOOL Upload\n");
#ifdef _MINGW
    fprintf(stderr, "SIZE %I64d\n",length);
#else
    fprintf(stderr, "SIZE %lld\n",length);
#endif
    fprintf(stderr, "TITLE Filename %s\n", file_shortname);
    fprintf(stderr, "EXNODEFILE %s\n", ( output_filename != NULL ? output_filename: "(null)"));

    fflush(stderr);
    max = (max != 0 ? max : length/data_blocksize * copies+2);

	lorsDebugPrint(D_LORS_VERBOSE, "GetDepotPool\n");

    if ( length == -1 )
    {
        length = 100*1024*1024; /* look for 100MB*/
    }

    temp_size = (double)length/(double)copies;
    temp_size = ((double)temp_size)/(double)(max*1000.0*1000.0);
    temp_size += (double)data_blocksize / (1000.0*1000.0) + 1.0;

    storage_size = (ulong_t) temp_size;

    /*fprintf(stderr, "l1: %lld l2: %lld, max: %d, data_blocksize: %d,"
                      " copies: %d length: %lld\n", length/(1024*1024)+1, 
                      (copies*length)/(max*1024*1024)+data_blocksize/(1024*1024)+1,
                      max, data_blocksize, copies, length); */
    /*fprintf(stderr, "max: %d, data_blocksize: %d,"
                      " length: %lld\n", 
                      max, data_blocksize, length);*/
    
    if ( ( ret = lorsGetDepotPool(&dpp,
                          lbone_server->host,
                          lbone_server->port,
                          depot_list, 
                          max*2,
                          location,
                          /*(ulong_t)(((copies*length)/(max*1024*1024))+data_blocksize/(1024*1024)+1),  SA 01/19/03 */
                          storage_size,
                          storage_type,
                          duration,
                          nthreads,
                          timeout,
                          opts|LORS_CHECKDEPOTS)) != LORS_SUCCESS )
    {
         fprintf(stderr,"Error in get depotpool! ret = %d\n" , ret);
         return ret;
    };
    printDepotPool(dpp);

    if ( g_lors_demo )
    {
        t1 = getTime();
    }

    if ( resolution_file != NULL )
    {
        /*fprintf(stderr, "Reading Resolution File\n");*/
        ret = lorsCreateResolutionFromFile(resolution_file, &lr, 0);
        /*fprintf(stderr, "Done\n");*/
        if ( ret != LORS_SUCCESS )
        {
            fprintf(stderr, "Can't find resolution file; continuing without it.\n");
            lr = NULL;
        } else 
        {
            /* align depots in depot pool with data from resolution file */
            lorsUpdateDepotPoolFromResolution(NULL, dpp, lr, 0);
        }
    }

    lorsDebugPrint(D_LORS_VERBOSE, "LorsSetInit: %d\n", data_blocksize);
    /*ret = lorsSetCreate(&set, offset, length, data_blocksize, copies, 0);*/
    ret = lorsSetInit(&set, data_blocksize, 1, 0);
    /*fprintf(stderr, "data_blocksize: %ld\n", data_blocksize);*/
    if ( ret != LORS_SUCCESS )
    {
        return ret;
    }

    if ( copies > 1 ){
         ret = lorsSetInit(&copy_set,data_blocksize,copies-1,0);
         if ( ret != LORS_SUCCESS )
         {
              return ret;
         };
    };

    /* determine max data_blocksize'd multiple to fit in max_internal_buffer,
     * or if mystat.st_size is less than max_internal_buffer, let it fit. */

    if ( length < max_internal_buffer && l_stdin != 1)
    {
        size = length;
    } else 
    {
        /* TODO: for reading from stdin */
        if ( l_stdin == 1 )
        {
            max_cap_of_buffer = max_internal_buffer;
            /*length = 1;*/
        } else {
            max_cap_of_buffer = MIN (max_internal_buffer, length);
        }
        if ( max_cap_of_buffer < data_blocksize )
        {
            max_cap_of_buffer = data_blocksize;
        }
        size = (max_cap_of_buffer/data_blocksize)*data_blocksize;
    }

    /*fprintf(stderr, "size: %ld\n", size);*/

    if ( size == 0 )
    {
        /*fprintf(stderr, "size == 0 \n");*/
        return LORS_FAILURE;
    }
    
    /*fprintf(stderr, "length: %d max blocksize: %d\n", length, size);*/

    buf_array[0] = calloc(size,sizeof(char));
    buf_array[1] = calloc(size,sizeof(char));
    
    if ( (buf_array[0] == NULL) || (buf_array[1] == NULL) ) { return LORS_NO_MEMORY; }

    /*fprintf(stderr, "mallocing size: %d\n", size);*/

    written = 0;
    if ( offset != 0 && l_stdin != 1)
    {
        ret = lseek(fd, offset, SEEK_SET);
        if ( ret != offset ) 
        {
            perror("lseek failed");
            return LORS_SYS_FAILED + errno;
        }
    }
    /*if ( e2e_order != NULL ) e2e_order2ConditionStruct(e2e_order, &lc, -1);*/
    if ( e2e_order != NULL ) e2e_order2ConditionStruct(e2e_order, &lc, e2e_blocksize);

    ith = 0;
    /* TODO: size is unknown */
    /*fileJob.size = (length > size ? size : length);*/
    /*fileJob.size = 0;*/

    /*fprintf(stderr, "size: %d, buf_size: %d\n", size, fileJob.buf_size);*/
    fileJob.size = size;
    fileJob.buf = buf_array[ith];
    fileJob.fd = fd;
    fileJob.buf_size = size;

    lorsReadFile(&fileJob);
    if ( fileJob.status != LORS_SUCCESS ) {
        /*fprintf(stderr, "!= lors success on read file: %d\n", fileJob.status);*/
         return ( fileJob.status);
    };
    if ( fileJob.ret_size == 0 ) {
            perror("Zero bytes read from file");
            exit(EXIT_FAILURE);
    };
    ret = fileJob.ret_size;
    buffer = buf_array[ith];
    file_read += fileJob.ret_size;

	// register if connected
	if(handle->status == CONN_CONNECTED){
		socket_io_send_register(handle, filename, (size_t)length, (size_t)nthreads);
	}
    //fprintf(stderr, "entering while: %d\n", fileJob.status);
    while ( fileJob.status != LORS_EOF )
    /*while ( set->max_length < length )*/
    {
        if ( fileJob.status != LORS_EOF )
        /*if ( file_read < length ) */
        {
            wait_thread = 1;
            /*fileJob.size=((length-file_read) > size ? size : (length - file_read));*/
            fileJob.size = size;
            fileJob.buf = buf_array[(++ith)%2];
            fileJob.fd = fd;
            fileJob.buf_size = size;
            if ( pthread_create(&tid,NULL,lorsReadFile,&fileJob) != 0 ){
                 perror("pthread_create failed");
                 return ( LORS_SYS_FAILED + errno);
            };
        }else {
            wait_thread = 0;   
        };
        
        assert(ret > 0 );
#ifdef _MINGW
        lorsDebugPrint(D_LORS_VERBOSE, 
                       "BEFORE ret: %d, curr_length: %I64d, max_length: %I64d\n", 
                        ret, set->curr_length, set->max_length);
#else
        lorsDebugPrint(D_LORS_VERBOSE, 
                       "BEFORE ret: %d, curr_length: %lld, max_length: %lld\n", 
                        ret, set->curr_length, set->max_length);
#endif

        /* assign a timeout based on the size to upload */
        /* this still does not help partial stores; this must be handled
         *          internally, but it's not clear how. */
restore:
        timeout = _lorsCalcTimeout((longlong)ret);
        soffset = set->max_length;
        sret = lorsSetStore(set, dpp, buffer, soffset, ((longlong)ret), 
							lc, nthreads, timeout, handle, opts|LORS_RETRY_UNTIL_TIMEOUT);
        if ( sret != LORS_SUCCESS )
        {
#if 1
            fprintf(stderr, "Do OVER\n");
            if ( sret == LORS_PARTIAL )
            {
                /* destroy any generated mappings */
                sret = lorsSetTrim(set, soffset, (longlong)ret, nthreads, 
                            timeout, LORS_TRIM_ALL|LORS_TRIM_DECR);
                lorsSetFindMaxlength(set);
                max *= 2;
                /* this could be a refill request too */
                if ( ( sret = lorsGetDepotPool(&dpp,
                          lbone_server->host,
                          lbone_server->port,
                          depot_list, 
                          max*2,
                          location,
                          storage_size,
                          storage_type,
                          duration,
                          nthreads,
                          timeout,
                          opts|LORS_CHECKDEPOTS)) != LORS_SUCCESS )
                {
                    fprintf(stderr,"Error in get depotpool! ret = %d\n" , sret);
                    return sret;
                };
                printDepotPool(dpp);
                goto restore;
            } else {
                /* different type of error */
                fprintf(stderr, "Unrecoverable error. %d\n", sret);
                return sret;
            }
#endif
        }

        if ( wait_thread == 1 ){
            pthread_join(tid,NULL);
            if ( !(fileJob.status == LORS_SUCCESS || fileJob.status == LORS_EOF) ){
               return (fileJob.status);
            };
            if ( fileJob.ret_size == 0 ){

#ifdef _MINGW
                /*fprintf(stderr, "length: %I64d, max_length: %I64d\n", 
                        length, set->max_length);*/
#else
                /*fprintf(stderr, "length: %lld, max_length: %lld\n", 
                        length, set->max_length);*/
#endif
                /*perror("reading file:");*/
                break;
                /*exit(-1);*/
            };
            assert(fileJob.ret_size > 0);
            ret = fileJob.ret_size;
            file_read += ret;
            buffer = fileJob.buf;
        }else {
            /*fprintf(stderr, "max_length: %lld  length: %lld\n", 
              set->max_length, length);*/
        };
    }
    /*lorsPrintSet(set);*/
    /*fprintf(stderr, "ExnodeCreate\n");*/
    close(fd);

    if ( copies > 1 ){
        assert(copy_set != NULL);
        pass_time = time(0) - begin_time;
        if ( timeout > 0 ){
            if ( pass_time > timeout ){
                return ( LORS_TIMEOUT_EXPIRED );
            }else {
                left_time = timeout - pass_time;  
            };
        }else {
            left_time = 0;   
        };
        /* TODO: 'length' may need to be changed for stdin */
        cret = lorsSetCopy(set,copy_set,dpp,lr,offset,set->max_length,nthreads,left_time,LORS_COPY);
        if ( cret != LORS_SUCCESS ){
            if ( cret == LORS_PARTIAL )
            {
                goto upload_partial;
            }
            return (cret);
        };   
    };
upload_partial:
    if ( g_lors_demo )
    {
        t2 = diffTime(t1);
        /* TODO: verify for stdin */
        demo_len = set->max_length*copies;
        lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", demo_len*8.0/1024.0/1024.0/t2);
        lorsDemoPrint(stderr, "MESSAGE 1 Done\n");
        lorsDemoPrint(stderr, "MESSAGE 2\n");
        fflush(stderr);
    }
    lorsExnodeCreate(&xnd);
    lorsAppendSet(xnd, set);
    if (copies > 1 && sret != LORS_PARTIAL ){
        lorsAppendSet(xnd,copy_set);

        //fprintf(stderr, "p1\n");
        lorsSetFree(copy_set,0);
    };
	//fprintf(stderr, "p2\n");
    lorsSetFree(set,0);
    lorsGetExnodeMetadata(xnd, &emd);
    memset(&val, 0, sizeof(val));
    val.s = file_shortname;
	//fprintf(stderr, "p3\n");
    lorsFreeDepotPool(dpp);
	//fprintf(stderr, "p4\n");
    if ( lc != NULL ) { free(lc); } 

	//fprintf(stderr, "p5\n");
    free(buf_array[0]);
    free(buf_array[1]);

    exnodeSetMetadataValue(emd, "filename", val, STRING, TRUE);
	// add mode
	val.s = "file";	
	exnodeSetMetadataValue(emd, "mode", val, STRING, TRUE);
	// add duration
	val.d = (double)duration;
	exnodeSetMetadataValue(emd, "duration", val, DOUBLE, TRUE);
	// add size of file 
	val.i = length;	
	exnodeSetMetadataValue(emd, "size", val, INTEGER, TRUE);
	// add parent id
	//val.s = parent_id;
	//exnodeSetMetadataValue(emd, "parent", val, STRING, TRUE);
	// add created 
	val.i = (long long)time(NULL);
	exnodeSetMetadataValue(emd, "created", val, INTEGER, TRUE);
	// add modified
	val.i = (long long)time(NULL);
	exnodeSetMetadataValue(emd, "modified", val, INTEGER, TRUE);

	//fprintf(stderr, "p6\n");
	if(output_filename != NULL && strstr(output_filename,".uef")){
		ret = lorsUefSerialize(xnd, output_filename);
	}else if(output_filename != NULL && strstr(output_filename,"http://")){
		ret = lorsPostUnis(xnd, output_filename);
	}else{
		ret = lorsFileSerialize(xnd, output_filename, 0, 0);
	}
	
    if ( ret != LORS_SUCCESS )
    {
        fprintf(stderr, "Serialize Failed.\n");
        return LORS_FAILURE;
    }
    //fprintf(stderr, "p7\n");
    lorsExnodeFree(xnd);


    if ( sret == LORS_PARTIAL || cret == LORS_PARTIAL )
    {
        return LORS_PARTIAL;
    }

    fprintf(stderr, "End Success\n");
    return LORS_SUCCESS;
}

static void _lorsFreeFileJobQueue( _LorsFileJobQueue *fjq ) {
    int i;
    
    if ( fjq != NULL ){
        if ( fjq->queue != NULL ){
            for ( i = 0; i < fjq->total_buffer ; i ++ ){
                if ( fjq->queue[i].buf != NULL ) { free ( fjq->queue[i].buf); };
            };
            free(fjq->queue);
        };
        free(fjq);
    };
};
        

static int _lorsInitFileJobQueue( _LorsFileJobQueue **file_job_queue,
                                  int               cache_num,
                                  int               prebuf_num,
                                  ulong_t           buffer_size){
    _LorsFileJobQueue  *fjq;
    int                i;
    
    assert ((cache_num > 0) && (prebuf_num >0 ) && ( cache_num > prebuf_num));
    if ( (fjq = (_LorsFileJobQueue *)calloc(1,sizeof(_LorsFileJobQueue))) == NULL ){
        return (LORS_NO_MEMORY);
    };

    pthread_mutex_init(&(fjq->lock),NULL);
    pthread_cond_init(&(fjq->no_empty_cond),NULL);
    pthread_cond_init(&(fjq->no_full_cond),NULL);

    fjq->total_buffer = cache_num;
    fjq->pre_buffer = prebuf_num;

    fjq->head = 0;
    fjq->tail = 0;
    fjq->status = LORS_SUCCESS;
    fjq->n_buf_filled = 0;
    if ( (fjq->queue = (_LorsFileJob *)calloc(cache_num,sizeof(_LorsFileJob))) == NULL ){
        _lorsFreeFileJobQueue(fjq);
        return (LORS_NO_MEMORY);
    };

    for ( i = 0 ; i < cache_num ; i ++ ){
        if ( (fjq->queue[i].buf = (char *)calloc(buffer_size,sizeof(char))) == NULL ){
            _lorsFreeFileJobQueue(fjq);
            return (LORS_NO_MEMORY);
        };
    };
    
    *file_job_queue = fjq;

    return (LORS_SUCCESS);
};


int lorsDownloadFile(char       *exnode_uri, 
                     char       *filename, 
                     longlong   offset,
                     longlong   mlength,
                     ulong_t       data_blocksize,
                     ulong_t    max_internal_buffer,
                     int        pre_buffer,
                     int        dp_threads,
                     int        job_threads,
                     int        progress,
                     int        cache,
                     char       *lbone_host,
                     int        lbone_port,
                     char       *location,
                     char       *resolution_file,
                     int        nthreads,
                     int        timeout,
					 socket_io_handler      *handle,
                     int        opts)
{
    /* we will want to be able to specify, perthread blocksize, determined
     * from 'length/nthreads'  yet a thread is assigned the lesser of
     * length/nthreads or the distance to the next boundary or
     * 'max_internal_buffer'
     */
    LorsMapping *lm;
    LorsSet     *set;
    LorsExnode  *exnode;
    LorsDepotPool *dpp;
    ExnodeMetadata *emd;
    ExnodeValue     val;
    ExnodeType      type;
    _LorsFileJob    fileJob;
    int         fd;
    int         lret, sret;
    LboneResolution *lr = NULL;
    longlong    written = 0;
    longlong    buf_length = 0, len = 0;
    longlong    length = 0;
    longlong    dret = 0;
    char        *buf;
    char        *buf_array[2];
    pthread_t   tid;
    _LorsFileJobQueue  *file_job_queue = NULL ;
    double      t1, t2;
    double      demo_len;
	
	

#if 0
    if ( _lorsInitFileJobQueue(&file_job_queue,cache,pre_buffer,max_internal_buffer) != LORS_SUCCESS ){
        fprintf(stderr,"Failed to initialize file job queue!\n");
        exit(-1);
    };
#endif
    
    /* added for ls/download combo - may be removed. 11/11/02*/
    /*lorsListFile(exnode_uri, nthreads, timeout, opts);*/

	if(strstr(exnode_uri, "http://")){
		lret = lorsUrleDeserialize(&exnode, exnode_uri, 0);
	}else{
		lret = lorsFileDeserialize(&exnode, exnode_uri, 0);
	}

    if ( lret != LORS_SUCCESS ) 
    {
        fprintf(stderr, "deserialize failed! : %s\n", exnode_uri);
        exit(EXIT_FAILURE);
    }
    if ( filename == NULL )
    {
        lorsGetExnodeMetadata(exnode, &emd);
        if ( emd == NULL ) 
        {
            fprintf(stderr, "error emd == NULL !\n");
            exit(EXIT_FAILURE);
        }

        /*fprintf(stderr, "0x%x 0x%x 0x%x\n", emd, &val, &type);*/
        lret = 1;
        lret = exnodeGetMetadataValue(emd, "filename", &val, &type);
        if ( lret != LORS_SUCCESS )
        {
            /* unknown name */

            fprintf(stderr, "Could not find filename in exnode: using 'output.file'\n");
            filename = "output.file";
        }
        filename = val.s;
    }

    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr,"MESSAGE 2 Download\n");
        fflush(stderr);
    }

    lret = lorsUpdateDepotPool(exnode, &dpp, NULL, 0, 
                                location, 1, 60, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "UpdateDepotPool failed. %d\n", lret);
        return lret;
    }

    if ( resolution_file != NULL )
    {
        /*fprintf(stderr, "Reading Resolution File------------\n");*/
        lret = lorsCreateResolutionFromFile(resolution_file, &lr, 0);
        /*fprintf(stderr, "Done\n");*/
        if ( lret != LORS_SUCCESS )
        {
            fprintf(stderr, "Can't find resolution file; continuing without it.\n");
            lr = NULL;
        } else 
        {
            /* align depots in depot pool with data from resolution file for
             * download */

            /*fprintf(stderr, "Updating DepotPool to resolution\n");*/
            lorsUpdateDepotPoolFromResolution(dpp, NULL, lr, 0);
        }
    } 
    if ( g_lors_demo )
    {
        t1 = getTime();
    }
   
    if ( mlength == 0 )
    {
        length = exnode->logical_length;
    } else 
    {
        length = mlength;
    }

    emd = exnode->md;
    lret = exnodeGetMetadataValue(emd, "filename", &val, &type);
    if ( lret == EXNODE_SUCCESS )
    {
        fprintf(stderr, "TOOL Download\n");
        fprintf(stderr, "EXNODEFILE %s\n", (filename == NULL ? val.s : filename ));
        fprintf(stderr, "TITLE Filename %s\n", val.s);
#ifdef _MINGW
        fprintf(stderr, "MAXLENGTH %I64d\n", exnode->logical_length);
#else
        fprintf(stderr, "MAXLENGTH %lld\n", exnode->logical_length);
#endif
        free(val.s);
    } else 
    {
        fprintf(stderr, "Your exnode is missing a filename!\n");
    }

    lret = lorsQuery(exnode, &set, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        return lret;
    }
    if ( strcmp(filename, "-") == 0 )
    {
        fd = 1;
    } else 
    {
        if ( HAS_OPTION(LORS_RECOVER, opts) )
        {

#ifdef _MINGW
            fd = open(filename, O_WRONLY|O_BINARY, 0666);
#else
            fd = open(filename, O_WRONLY, 0666);
#endif
            if ( fd == -1 )
            {
                goto failed_open;
            }
            /* seek to the end, treat size as offset */
            offset = lseek(fd, 0, SEEK_END);
            length -= offset;

            fprintf(stderr, "offset: %lld\n", offset);
            if ( offset == -1 )
            {
                perror("failed to seek to end of existing file");
                exit(EXIT_FAILURE);
            }

        } else 
        {
failed_open:
#ifdef _MINGW
            fd = open(filename, O_TRUNC|O_CREAT|O_WRONLY|O_BINARY, 0666);
#else
            fd = open(filename, O_TRUNC|O_CREAT|O_WRONLY, 0666);
#endif
            if ( fd == -1 )
            {
                perror("open failed");
                return LORS_SYS_FAILED;
            }
        }
    }

    /* the lesser of the two */
    buf_length = ( max_internal_buffer < length ? max_internal_buffer : length);
    /*
    * SetLoad will return what ever you ask for. however it may end up
    * downloading more than you request in order to satisfy the request.  it
    * is best to align your downloads on the blocksize of the mappings. 
    *
    * An option to enforce this would be nice, or possibly, to enforce it by
    * default.
    */
    
	/*Start report status here*/
	if(handle->status == CONN_CONNECTED){
		//fprintf(stderr, "Registering \n");
			socket_io_send_register(handle, filename, (size_t)length, (size_t)nthreads);
	}
    written = 0;
    while ( written < length )
    {

        /*fprintf(stderr, "written: %lld length: %lld\n", written, length);*/
        len = length - written;
#ifdef _MINGW
        lorsDebugPrint(D_LORS_VERBOSE, 
                          "SETLOAD: offset: %I64d, length: %I64d, nthreads: %d\n",
                          offset+written, len, nthreads);
#else
        lorsDebugPrint(D_LORS_VERBOSE, 
                          "SETLOAD: offset: %lld, length: %lld, nthreads: %d\n",
                          offset+written, len, nthreads);
#endif

 #ifdef _CYGWIN
         /* win32 fix: keeps threads from hanging. */
         if ( cache < nthreads ) cache = nthreads;
 #endif

#ifdef _MINGW

        if(cache < nthreads) cache = nthreads;
#endif

        dret = lorsSetRealTimeLoad(set, NULL,fd, offset+written, 
								   len,data_blocksize,pre_buffer,cache,
								   glob_lc, nthreads, 
								   timeout,dp_threads,job_threads,progress, handle, opts);
        /* TODO check for timeout? */
        if ( dret <= 0 )
        {
            fprintf(stderr, "lorsSetLoad failure. %d\n", dret);
			/*fprintf(stderr, "How many times to try again??\n");*/
            fprintf(stderr, "End Failure\n");
            return LORS_FAILURE;
        };

        written += dret;
        
    };
    if ( lr != NULL )
    {
        /*fprintf(stderr, "FREEING resolution\n");*/
        lorsFreeResolution(lr);
    }

    close(fd);

    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr,"MESSAGE 1 Done\n");
        lorsDemoPrint(stderr,"MESSAGE 2\n");
        t2 = diffTime(t1);
        demo_len = length;
        lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", demo_len*8.0/1024.0/1024.0/t2); 
        fflush(stderr);
    }

    lorsSetFree(set,0);
    lorsFreeDepotPool(dpp);
    lorsExnodeFree(exnode);

    fprintf(stderr, "End Success\n");
    return LORS_SUCCESS;
}

/* there is more information to consider. 
 * how to print the mapping data.
 * the exnode md.
 * the mapping md
 * the mapping functions.
 * the funciton md?? bleh.
 * Vrwma   0  1    i2dsi.ibiblio.org:6714   4665      0 Fri Aug  23 02:03:58 2002
 * <stuff> <id> <refcnt> <hostname:port>   <offset> <length> <expdate> 
 * <id> offset length hostname:port
 *      <stuff> <refcnt> expdate>
 *      function
 *      metadata
 */
int lorsListFile(char *filename, int threads, int timeout, int options)
{
    LorsExnode *exnode = NULL;
    LorsSet     *set = NULL;
    LorsEnum    list = NULL, iter= NULL;
    LorsMapping *lm;
    ExnodeMetadata *emd;
    ExnodeValue     val;
    ExnodeType      type;
    int         lret, sret, i;
    char        read = 'r', write = 'w', manage = 'm';

    if ( g_lors_demo )
    {
        if ( !(options&LORS_LIST_HUMAN) )
            lorsDemoPrint(stderr,"\nCLEAR\n");
        fflush(stderr);
    }

    lret = lorsFileDeserialize(&exnode, filename, NULL);
    if ( lret != LORS_SUCCESS )
    {
        return LORS_XML_ERROR;
    }
    lret = lorsQuery(exnode, &set, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        return lret;
    }

    lorsGetExnodeMetadata(exnode, &emd);
    if ( emd == NULL ) 
    {
        fprintf(stderr, "error emd == NULL !\n");
        exit(EXIT_FAILURE);
    }

    lret = exnodeGetMetadataValue(emd, "filename", &val, &type);
    if ( lret == EXNODE_SUCCESS )
    {
        fprintf(stderr, "TOOL List\n");
#ifdef _MINGW
        fprintf(stderr, "SIZE %I64d\n", exnode->logical_length);
#else
        fprintf(stderr, "SIZE %lld\n", exnode->logical_length);
#endif
        fprintf(stderr, "EXNODEFILE %s\n", filename);
        fprintf(stderr, "TITLE Filename %s\n", val.s);
#ifdef _MINGW
        fprintf(stderr, "MAXLENGTH %I64d\n", exnode->logical_length);
#else
        fprintf(stderr, "MAXLENGTH %lld\n", exnode->logical_length);
#endif
        if ( type == STRING ) {
            free(val.s);
        };
    } else 
    {
        fprintf(stderr, "Your exnode is missing a filename!\n");
    }

    lret = lorsSetStat(set, threads, timeout, options);
    if ( lret != LORS_SUCCESS )
    {
        return lret;
    }
    lorsSetEnum(set, &list);
    i = 0;
    if ( !g_lors_demo )
    {
        /* incase --with-demo was not spescified, lors_ls should print something */
        options |= LORS_LIST_HUMAN;
    }
    if ( options & LORS_LIST_HUMAN )
    {
        do 
        {
            char    *s;
            lret = lorsEnumNext(list, &iter, &lm);
            if ( lret == LORS_END ) break;

            if ( lm->capstat.attrib.duration == -1 )
            {
                s = "Unknown\n";
            } else 
            {
                s = ctime((time_t*)(&lm->capstat.attrib.duration));
            }

            if (lm->capset.readCap == NULL) read = '-';
            else if (strlen(lm->capset.readCap) == 0) read = '-';
            if (lm->capset.writeCap == NULL) write = '-';
            else if (strlen(lm->capset.writeCap) == 0) write = '-';
            if (lm->capset.manageCap == NULL) manage = '-';
            else if (strlen(lm->capset.manageCap) == 0) manage = '-';

            fprintf(stdout, "%2d %c%c%c%c%c ", 
                        i, (lm->capstat.attrib.reliability == IBP_HARD ?'H' :
                         (lm->capstat.attrib.reliability == IBP_SOFT ? 'S' : '?')),
                          read, write, manage, 
                        (lm->capstat.attrib.type == IBP_BYTEARRAY ? 'a' : '?'));
            fprintf(stdout, "%2d ", lm->capstat.readRefCount);
#ifdef _MINGW
            fprintf(stdout, "%9I64d %9I64d ", lm->exnode_offset,
                         (options & LORS_LIST_PHYSICAL ? 
                             (longlong)(lm->alloc_length) : lm->logical_length));
#else
            fprintf(stdout, "%9lld %9lld ", lm->exnode_offset,
                         (options & LORS_LIST_PHYSICAL ? 
                             (longlong)(lm->alloc_length) : lm->logical_length));
#endif
            fprintf(stdout, "%s:%d ", lm->depot.host, lm->depot.port);
            fprintf(stdout, "%s", s);

            s = NULL;
            i++;
        } while ( lret != LORS_END );
    }

    lorsEnumFree(list);
    lorsSetFree(set,0);
    lorsExnodeFree(exnode);

    return LORS_SUCCESS;
}
int lorsTrimFile(char *filename, char *outfilename, int *seg_list, int threads, int timeout, int options)
{
    LorsExnode *exnode = NULL;
    LorsSet     *set = NULL, *tmpset = NULL;
    LorsEnum    list = NULL, iter= NULL;
    LorsMapping *lm = NULL;
    ExnodeMetadata *emd = NULL;
    ExnodeValue     val;
    ExnodeType      type;
    int         lret, sret, i;
    int         cnt=0;

    lret = lorsFileDeserialize(&exnode, filename, NULL);
    if ( lret != LORS_SUCCESS )
    {
        return LORS_XML_ERROR;
    }
    lret = lorsQuery(exnode, &tmpset, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        return lret;
    }

    if ( seg_list[0] != -1 )
    {
        lret = lorsSetEnum(tmpset, &list);
        cnt = 0;
        lorsSetInit(&set, 0, 0, 0);
        do 
        {
            lret = lorsEnumNext(list, &iter, &lm);
            if (lret == LORS_END ) break;
            for(i=0; seg_list[i] != -1; i++)
            {
                if ( cnt == seg_list[i])
                {

                    lorsSetAddMapping(set, lm);
                    /*TODO: broken. */
                    lorsDetachMapping(exnode, lm);
                    break;
                }
            }
            cnt++;
        } while ( lm != NULL);
        lorsEnumFree(list);
    } else 
    {
        lret = lorsQuery(exnode, &set, 0, exnode->logical_length, LORS_QUERY_REMOVE);
        if ( lret != LORS_SUCCESS )
        {
            fprintf(stderr, "lorsQuery failed.\n");
            return lret;
        }
    }

    lret = exnodeGetMetadataValue(exnode->md, "filename", &val, &type);
    if ( lret == EXNODE_SUCCESS )
    {
        fprintf(stderr, "TOOL Trim\n");
        fprintf(stderr, "EXNODEFILE %s\n", filename);
        fprintf(stderr, "TITLE Filename %s\n", val.s);
#ifdef _MINGW
        fprintf(stderr, "MAXLENGTH %I64d\n", exnode->logical_length);
#else
        fprintf(stderr, "MAXLENGTH %lld\n", exnode->logical_length);
#endif
        free(val.s);
    } else 
    {
        fprintf(stderr, "Your exnode is missing a filename!\n");
    }

    if ( options & LORS_TRIM_DEAD )
    {
        lret = lorsSetStat(set, threads, timeout, options);
        if ( lret != LORS_SUCCESS )
        {
            return lret;
        }
    }

    if ( !(options & LORS_TRIM_DEAD) )
    {
        options |= LORS_TRIM_ALL;
    }
    lret = lorsSetTrim(set, 0, set->max_length, threads, timeout, options);
    if ( lret != LORS_SUCCESS )
    {
        lorsDebugPrint(1, "End Failure\n");
        return lret;
    }
    lorsAppendSet(exnode, set);

    if (outfilename == NULL ) 
    {
        outfilename = filename;
    } else if ( strcmp(outfilename, "-") == 0 ) 
    {
        outfilename = NULL;
    }

    /* count number of mappings to reserialize.  
     * if Zero, then delete  file */
    iter = NULL;
    lorsExnodeEnum(exnode, &list);
    cnt = -1;
    do {
        lret = lorsEnumNext(list, &iter, &lm);
        cnt++;
    } while (lm != NULL);

    if ( cnt == 0 )
    {
        unlink(outfilename);
    } else 
    {
        lret = lorsFileSerialize(exnode, outfilename, 0, 0);
        if ( lret != LORS_SUCCESS )
        {
            return LORS_XML_ERROR;
        }
    }

    lorsSetFree(tmpset, 0);
    lorsSetFree(set, 0);
    lorsExnodeFree(exnode);

    fprintf(stderr, "End Success\n");
    return LORS_SUCCESS;
}
int lorsRefreshFile(char *filename, time_t duration, int threads, int timeout, int options)
{
    LorsExnode *exnode = NULL;
    LorsSet     *set = NULL;
    LorsEnum    list = NULL, iter= NULL;
    LorsMapping *lm;
    ExnodeMetadata *emd;
    ExnodeValue     val;
    ExnodeType      type;
    int         lret, sret, i;

    lret = lorsFileDeserialize(&exnode, filename, NULL);
    if ( lret != LORS_SUCCESS )
    {
         lret  = LORS_XML_ERROR;
         goto bail;
    }
    lret = lorsQuery(exnode, &set, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        goto bail;
    }
    lorsGetExnodeMetadata(exnode, &emd);
    if ( emd == NULL ) 
    {
        fprintf(stderr, "error emd == NULL !\n");
        goto bail;
    }

    lret = exnodeGetMetadataValue(emd, "filename", &val, &type);
    if ( lret != EXNODE_SUCCESS )
    {
        fprintf(stderr, "Unknown filename. Ignoring..\n");
    }
#if 1
    if ( lret == EXNODE_SUCCESS )
    {
        fprintf(stderr, "CLEAR\n");
        fprintf(stderr, "TOOL Refresh\n");
        fprintf(stderr, "EXNODEFILE %s\n", filename);
        fprintf(stderr, "TITLE Filename %s\n", val.s);
#ifdef _MINGW
        fprintf(stderr, "MAXLENGTH %I64d\n", exnode->logical_length);
#else
        fprintf(stderr, "MAXLENGTH %lld\n", exnode->logical_length);
#endif
        free(val.s);
    } else 
    {
        fprintf(stderr, "Your exnode is missing a filename!\n");
    }
#endif
    lret = lorsSetStat(set, threads, timeout, options|LORS_LIST_HUMAN);
    if ( lret != LORS_SUCCESS )
    {
        goto bail;
    }

    lret = lorsSetRefresh(set, 0, set->max_length, duration, 
                          threads, timeout, options);
    if ( lret == LORS_PARTIAL )
    {
        fprintf(stderr, "Partial Success.\n");
    } else if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "refresh failed.\n");
        goto bail;
    }
    lret = lorsSetStat(set, threads, timeout, options);
    if ( lret != LORS_SUCCESS )
    {
        goto bail;
    }

bail:
    if ( set != NULL ){
        lorsSetFree(set,0);
        set = NULL;
    };
    if ( exnode != NULL ){
        lorsExnodeFree(exnode);
        exnode = NULL;
    };
    

    return lret;
}

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
                     int        opts)
{
    struct stat     mystat;
    char           *file_shortname;
    char           *buffer;
    int             fd;
    int             ret;
    ulong_t         size;
    ulong_t max_cap_of_buffer;
    ulong_t max_cap_of_file;
    ulong_t written;
    LorsExnode      *exnode = NULL;
    LorsSet         *set = NULL ;
    LorsSet         *new_set = NULL,*copy_set = NULL;
    int             lret = LORS_SUCCESS;
    ulong_t         storage_size = 0;
    double              temp_size = 0;
    LorsDepotPool   *dpp = NULL;
    LorsDepotPool   *dpp2 = NULL;
    LorsExnode      *xnd = NULL ,*aug_xnd = NULL;
    ExnodeMetadata  *emd;
    ExnodeValue     val;
    ExnodeType      type;
    LboneResolution *lr = NULL;

    double t1, t2;
    double demo_len;

    if ( g_lors_demo )
    {
        t1 = getTime();
    }
      
    lret = lorsFileDeserialize(&exnode, exnode_filename, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "deserialize failed! : %s\n", exnode_filename);
        goto bail;
    }              

    lret = lorsExnodeCreate(&aug_xnd);
    if ( lret != LORS_SUCCESS ){
        fprintf(stderr,"Exnode create failed!\n");
        goto bail;
    };


    if ( length == -1 )
    {
        length = (ulong_t)exnode->logical_length - offset;
    }                
   
    if ( fragments > 0 )
    {
        data_blocksize = (length/fragments) + (length%fragments);
    }
    max = (max != 0 ? max : length/data_blocksize * copies+2);

    temp_size = (double)length/(double)copies;
    temp_size = ((double)temp_size)/(double)(max*1000.0*1000.0);
    temp_size += (double)data_blocksize / (1000.0*1000.0) + 1.0;
    storage_size = (ulong_t) temp_size;
    
    lret = lorsGetDepotPool(&dpp,
                          lbone_server->host,
                          lbone_server->port,
                          depot_list1,
                          max*2,
                          location,
                          storage_size,
                          storage_type,
                          duration,
                          nthreads,
                          timeout,
                          opts|LORS_CHECKDEPOTS);

    if ( lret != LORS_SUCCESS )
    {
         fprintf(stderr,"Error in get depotpool!\n");
         goto bail;
    };

   if ((copies > 1) && HAS_OPTION(LORS_MCOPY, opts))
   {
       lret = lorsGetDepotPool(&dpp2,
                          lbone_server->host,
                          lbone_server->port,
                          depot_list2,
                          max*2,
                          location,
                          storage_size,
                          /*(ulong_t)(((copies*length)/(max*1024*1024))+data_blocksize/(1024*1024)+1),*/
                          /*length/data_blocksize+1,*/
                          storage_type,
                          duration,
                          nthreads,
                          timeout,
                          opts|LORS_CHECKDEPOTS);
    
       if ( lret != LORS_SUCCESS )
       {
          fprintf(stderr,"Error in get depotpool!\n");
          goto bail;
       };
    }
    if ( resolution_file != NULL )
    {

        /*fprintf(stderr, "RESFILE: %s\n", resolution_file);*/
        ret = lorsCreateResolutionFromFile(resolution_file, &lr, 0);
        if ( ret != LORS_SUCCESS )
        {
            /*fprintf(stderr, "FAILED RETURN %d\n", ret);*/
            fprintf(stderr, "Can't find resolution file; continuing without it.\n");
            lr = NULL;
        } else 
        {
            /* align depots in depot pool with data from resolution file */
            /*lorsUpdateDepotPoolFromResolution(NULL, dpp, lr, 0);*/
        }
    }


    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr,"MESSAGE 2 Augmenting\n");
        fflush(stderr);
    }
    lret = lorsQuery(exnode, &set, offset, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        goto bail;
    }
    lret = exnodeGetMetadataValue(exnode->md, "filename", &val, &type);
    if ( lret == EXNODE_SUCCESS )
    {
        fprintf(stderr, "TOOL Augment\n");
        fprintf(stderr, "EXNODEFILE %s\n", exnode_filename);
        fprintf(stderr, "TITLE Filename %s\n", val.s);
#ifdef _MINGW
        fprintf(stderr, "MAXLENGTH %I64d\n", exnode->logical_length);
#else
        fprintf(stderr, "MAXLENGTH %lld\n", exnode->logical_length);
#endif
    } else
    {
        fprintf(stderr, "Your exnode is missing a filename!\n");
    }

    if ( HAS_OPTION(LORS_BALANCE, opts) )
    {
        lorsDemoPrint(stderr,"SIZE %llu\n",exnode->logical_length);
        lorsSetStat(set, nthreads, timeout, opts|LORS_LIST_LOGICAL);
    }

    if( HAS_OPTION(LORS_MCOPY,opts))
    {
        if ( HAS_OPTION(LORS_BALANCE, opts) )
        {
            lret = lorsSetInit(&new_set, data_blocksize, copies, 0);
        } else {
            lret = lorsSetInit(&new_set, data_blocksize, 1, 0);
        }
        if ( lret != LORS_SUCCESS ) {
           lorsFreeDepotPool(dpp);
           goto bail;
        };
        if ( HAS_OPTION(LORS_BALANCE, opts) )
        {
            timeout = _lorsCalcTimeout(length);
            fprintf(stderr, "Timeout Balance Calc: %d\n", timeout);
            lret = lorsSetCopy(set,new_set,dpp,lr, offset,
                 length,nthreads,timeout,opts);
            if ( lret != LORS_SUCCESS ) {
                fprintf(stderr,"lorsSetCopy failed. : %d\n", lret);
                goto bail;
            };
            
        } else {
            if (copies > 1) {
               lret = lorsSetInit(&copy_set,data_blocksize,copies-1,0);
               if( lret != LORS_SUCCESS ) {
                      return lret;
               };
            };
            timeout = _lorsCalcTimeout(length);

            fprintf(stderr, "Timeout Calc: %d\n", timeout);
            lret = lorsSetCopy(set,new_set,dpp,lr, offset,
                 length,nthreads,timeout,opts);
            if ( lret != LORS_SUCCESS ) {
                fprintf(stderr,"lorsSetCopy failed. : %d\n", lret);
                goto bail;
            };
         
            if ( copies > 1 ) {
               assert(copy_set !=NULL);
               timeout = _lorsCalcTimeout(length);
               lret=lorsSetCopy(new_set,copy_set,dpp2,lr,offset,length,
                                nthreads,timeout,opts);
               if( lret != LORS_SUCCESS) {
                     fprintf(stderr,"lorsSetCopy failed : %d\n", lret);
                     return lret;
               };
            };
        }
        lorsAppendSet(exnode, new_set);
        lorsAppendSet(aug_xnd,new_set);

        if(copies > 1 && !HAS_OPTION(LORS_BALANCE, opts)) {
            lorsAppendSet(exnode,copy_set);
            lorsAppendSet(aug_xnd,copy_set);
        };
        *new_exnode = exnode;
        *aug_exnode = aug_xnd;
    }
    else
    {
        lret = lorsSetInit(&new_set, data_blocksize, copies, 0);
        if ( lret != LORS_SUCCESS ) {
           lorsFreeDepotPool(dpp);
           goto bail;
        }
        timeout = _lorsCalcTimeout(length);
        fprintf(stderr, "Timeout Calc: %d: length %lld\n", timeout, length);
        lret = lorsSetCopy(set,new_set,dpp,lr,offset,
                           length,nthreads,timeout,opts);
        if ( lret != LORS_SUCCESS ) {
            if ( lret != LORS_PARTIAL )
            {
                fprintf(stderr,"lorsSetCopy failed. : %d\n", lret);
                goto bail;
            }
        };
               
        lorsAppendSet(exnode, new_set);
        lorsAppendSet(aug_xnd,new_set);
        *new_exnode = exnode;
        *aug_exnode = aug_xnd;
    }
    if ( g_lors_demo )
    {
        t2 = diffTime(t1);
        demo_len = length*copies;
        lorsDemoPrint(stderr, "MESSAGE 3 %0.4f Mbps\n", demo_len*8.0/1024.0/1024.0/t2);
        lorsDemoPrint(stderr, "MESSAGE 1 Done\n");
        lorsDemoPrint(stderr, "MESSAGE 2\n");
        fflush(stderr);
    }
    
    lorsDebugPrint(1, "End Success\n");
        
bail: 
    if ( (exnode != NULL) && ( lret != LORS_SUCCESS && lret != LORS_PARTIAL) ){
        lorsExnodeFree(exnode);
        exnode = NULL; 
    };
    if ( (aug_xnd != NULL) && ( lret != LORS_SUCCESS && lret != LORS_PARTIAL) ){
        lorsExnodeFree(aug_xnd);
        aug_xnd = NULL;
    };
    if ( dpp != NULL){
        lorsFreeDepotPool(dpp);
        dpp = NULL;
    };
    if ( set != NULL){
        lorsSetFree(set,0);
        set = NULL;
    };
    if ( new_set != NULL ){  
        lorsSetFree(new_set,0);
        new_set = NULL;
    };
    return lret;
}




static int _listExnode(LorsExnode *xnd)
{
    LorsExnode *exnode = NULL;
    LorsSet     *set = NULL;
    LorsEnum    list = NULL, iter= NULL;
    LorsMapping *lm;
    ExnodeMetadata *emd;
    ExnodeValue     val;
    ExnodeType      type;
    int         lret, sret, i;

    assert ( xnd != NULL );
    exnode = xnd ;
    lret = lorsQuery(exnode, &set, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        return lret;
    }

    if ( g_lors_demo )
    {
        lorsDemoPrint(stderr,"SIZE %llu\n",exnode->logical_length);
        lorsDemoPrint(stderr,"MESSAGE 2 Listing\n"); 
        fflush(stderr);
    }
    lorsGetExnodeMetadata(exnode, &emd);
    if ( emd == NULL ) 
    {
        fprintf(stderr, "error emd == NULL !\n");
        exit(EXIT_FAILURE);
    }

    lorsSetEnum(set, &list);
    i = 0;
    do 
    {
        lret = lorsEnumNext(list, &iter, &lm);
        if ( lret == LORS_END ) break;

        if ( g_lors_demo )
        {
#ifdef _MINGW
            lorsDemoPrint(stderr,"DRAW MappingBegin %s:%4d %d %I64d %ld %s:%4d\n",
                      lm->depot.host,lm->depot.port,
                      lm->id,lm->exnode_offset,lm->logical_length,
                      lm->depot.host,lm->depot.port);
            lorsDemoPrint(stderr,"DRAW MappingEnd %d %I64d %ld %s:%d\n",
                      lm->id,lm->exnode_offset,lm->logical_length,
                      lm->depot.host,lm->depot.port);
#else
            lorsDemoPrint(stderr,"DRAW MappingBegin %s:%4d %d %lld %ld %s:%4d\n",
                      lm->depot.host,lm->depot.port,
                      lm->id,lm->exnode_offset,lm->logical_length,
                      lm->depot.host,lm->depot.port);
            lorsDemoPrint(stderr,"DRAW MappingEnd %d %lld %ld %s:%d\n",
                      lm->id,lm->exnode_offset,lm->logical_length,
                      lm->depot.host,lm->depot.port);
#endif
            fflush(stderr);
        }
    } while ( lret != LORS_END );
    lorsEnumFree(list);

    return LORS_SUCCESS;
};



int lorsModifyFile(char *filename, char *outfilename, int options)
{
    LorsExnode *exnode = NULL;
    LorsSet     *set = NULL;
    LorsEnum    list = NULL, iter= NULL;
    LorsMapping *lm = NULL;
    ExnodeMetadata *emd = NULL;
    ExnodeValue     val;
    ExnodeType      type;
    int         lret, sret, i;

    lret = lorsFileDeserialize(&exnode, filename, NULL);
    if ( lret != LORS_SUCCESS )
    {
        return LORS_XML_ERROR;
    }

    lret = lorsQuery(exnode, &set, 0, exnode->logical_length, 0);
    if ( lret != LORS_SUCCESS )
    {
        fprintf(stderr, "lorsQuery failed.\n");
        return lret;
    }

    lret = exnodeGetMetadataValue(exnode->md, "filename", &val, &type);
    if ( lret == EXNODE_SUCCESS )
    {
        fprintf(stderr, "TOOL MODIFY\n");
        fprintf(stderr, "EXNODEFILE %s\n", filename);
        fprintf(stderr, "TITLE Filename %s\n", val.s);
        fprintf(stderr, "MAXLENGTH %lld\n", exnode->logical_length);
        free(val.s);
    } else 
    {
        fprintf(stderr, "Your exnode is missing a filename!\n");
    }

    lret = lorsSetTrimCaps(set, options);
    if ( lret != LORS_SUCCESS )
    {
        lorsDebugPrint(1, "End Failure\n");
        return lret;
    }

    if (outfilename == NULL ) 
    {
        outfilename = filename;
    } else if ( strcmp(outfilename, "-") == 0 ) 
    {
        outfilename = NULL;
    }

    lret = lorsFileSerialize(exnode, outfilename, 0, 0);
    if ( lret != LORS_SUCCESS )
    {
        return LORS_XML_ERROR;
    }

    lorsSetFree(set, 0);
    lorsExnodeFree(exnode);

    fprintf(stderr, "End Success\n");
    return LORS_SUCCESS;
}
