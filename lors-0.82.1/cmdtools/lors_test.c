#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <lors_api.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if 0
/* 
 * this function is not valid for E2E encoded mappings.  Behavior is
 * unpredictable and will certainly fail, if e2e mappings are given to it.
 *
 * set->data_blocksize must be set.
 */

int main (int argc, char **argv)
{
    LorsSet             *set;
    LorsDepotPool       *dp;
    LorsExnode          *ex;
    int                  ret;
    char                *buffer;
    int                 fd;
    longlong            size;
    struct stat         mstat;
    char                *str;
    ExnodeMetadata      *emd = NULL;
    ExnodeValue          val;

    ret = stat(argv[1], &mstat);
    if ( ret == -1 ) { perror("error1"); exit(EXIT_FAILURE); }

#ifdef _MINGW
    fd = open(argv[1], O_RDONLY|O_BINARY);
#else
    fd = open(argv[1], O_RDONLY);
#endif

    if ( fd == -1 ) { perror("error2"); exit(EXIT_FAILURE);  }

    size = mstat.st_size;
    buffer = malloc(sizeof(char)*size);
    ret = lorsGetDepotPool(&dp, "dsj.sinrg.cs.utk.edu", 6767, NULL,
                               6, 
                               "zip= 37996", 
                               (ulong_t)(size/(1024*1024)+1), 
                               IBP_SOFT, 
                               60*60, 10, 20, LORS_CHECKDEPOTS);
    if ( ret != LORS_SUCCESS ) {  fprintf(stderr, "error4: %d", ret); exit(EXIT_FAILURE);  }

    ret = lorsSetInit(&set, size/3, 1, 0);
    if ( ret != LORS_SUCCESS ) {  perror("error5"); exit(EXIT_FAILURE);  }

    ret = read(fd, buffer, size);
    if ( ret != size) {  perror("error6"); exit(EXIT_FAILURE);  }

    ret = lorsSetStore(set, dp, buffer, set->max_length, size, NULL, 6, 20, 0);
    if ( ret != LORS_SUCCESS ) {  perror("error7"); exit(EXIT_FAILURE);  }

    str = "AAAAAAAABBBBBBBBBCCCCCCCCDDDDDDDD\n";
    ret = lorsSetUpdate(set, dp, str, 100, strlen(str), 6, 20, 0);
    if ( ret != LORS_SUCCESS ) {  fprintf(stderr, "error7: %d", ret); exit(EXIT_FAILURE);  }

#ifdef _MINGW
    fprintf(stderr, "max_length: %I64d\n", set->max_length);
#else
    fprintf(stderr, "max_length: %lld\n", set->max_length);
#endif

    ret = lorsExnodeCreate(&ex);
    if ( ret != LORS_SUCCESS ) {  perror("error8"); exit(EXIT_FAILURE);  }

    ret = lorsAppendSet(ex, set);
    if ( ret != LORS_SUCCESS ) {  perror("error9"); exit(EXIT_FAILURE);  }

    lorsGetExnodeMetadata(ex, &emd);
    val.s = strdup(argv[1]);
    exnodeSetMetadataValue(emd, "filename", val, STRING, TRUE);

    ret = lorsFileSerialize(ex, argv[2], 0, 0);
    if ( ret != LORS_SUCCESS ) {  perror("error10"); exit(EXIT_FAILURE);  }

    return 0;
}
#endif

#if 1
int main (int argc, char **argv)
{
        LorsSet             *se;
        LorsDepotPool       *dp;
        LorsExnode          *ex;
        int                  ret;
        char                *buffer;
        int                 fd;
        longlong            size;
        struct stat         mstat;

        ret = stat(argv[1], &mstat);
        if ( ret == -1 ) { perror("error1"); exit(EXIT_FAILURE); }
#ifdef _MINGW
        fd = open(argv[1], O_RDONLY|O_BINARY);
#else
        fd = open(argv[1], O_RDONLY);
#endif
        if ( fd == -1 ) { perror("error2"); exit(EXIT_FAILURE);  }

        size = mstat.st_size;
        buffer = malloc(sizeof(char)*size);
        if ( buffer == NULL ) { perror("error3"); exit(EXIT_FAILURE); }

        g_db_level = -1;
        ret = lorsGetDepotPool(&dp, "dsj.sinrg.cs.utk.edu", 6767, NULL,
                               6, 
                               "zip= 37996", 
                               (ulong_t)(size/(1024*1024)+1), 
                               IBP_SOFT, 
                               60*60, 10, 20, LORS_CHECKDEPOTS);
        if ( ret != LORS_SUCCESS ) {  perror("error4"); exit(EXIT_FAILURE);  }

        ret = lorsSetInit(&se, size/3, 2, 0);
        if ( ret != LORS_SUCCESS ) {  perror("error5"); exit(EXIT_FAILURE);  }

        ret = read(fd, buffer, size);
        if ( ret != size) {  

            fprintf(stderr, "size: %d ret: %d\n", size, ret);
            fprintf(stderr, "errno: %d\n", errno);
            perror("error6"); exit(EXIT_FAILURE);  
        }

        ret = lorsSetStore(se, dp, buffer, se->max_length, size, NULL, 6, 20, NULL, 0);

        if ( ret != LORS_SUCCESS ) {  perror("error7"); exit(EXIT_FAILURE);  }

        ret = lorsExnodeCreate(&ex);
        if ( ret != LORS_SUCCESS ) {  perror("error8"); exit(EXIT_FAILURE);  }

        ret = lorsAppendSet(ex, se);
        if ( ret != LORS_SUCCESS ) {  perror("error9"); exit(EXIT_FAILURE);  }

        ret = lorsFileSerialize(ex, argv[2], 0, 0);
        if ( ret != LORS_SUCCESS ) {  perror("error10"); exit(EXIT_FAILURE);  }

        return 0;

}
#endif

#if 0
    int main (int argc, char **argv)
    {

        LorsSet       *se;
        LorsSet       *sel, *sel2;
        LorsDepotPool       *dp;
        LorsExnode          *ex;
        int                  ret;
        int                 written = 0;
        char                *buffer;
        int                 fd;
        longlong            size;
        struct stat         mstat;

        ret = lorsFileDeserialize(&ex, argv[1], NULL);
        if ( ret != LORS_SUCCESS ) { exit(EXIT_FAILURE); }

#ifdef _MINGW
        fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC|O_BINARY, 0644);
#else
        fd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC, 0644);
#endif

        if ( fd == -1 ) { perror("error1"); exit(EXIT_FAILURE);  }

        size = ex->logical_length;
        buffer = malloc(sizeof(char)*size);
        if ( buffer == NULL ) { perror("error2"); exit(EXIT_FAILURE); }

        ret = lorsUpdateDepotPool(ex, &dp, NULL, 0, NULL, 1, 60, 0);

        ret = lorsQuery(ex, &se, 0, ex->logical_length, 0);
        written = 0;
        while ( written < size )
        {

            fprintf(stderr, "offset: %d len: %ld\n",
                            written, (long)(size-written));
            ret = lorsSetLoad(se, buffer, 
                              (longlong)written, (long)(size-written), 
                              512*1024,
                              NULL, 10, 
                              100, 0);

            fprintf(stderr, "returned: %d\n", ret);
            if ( ret != (size-written) ) { printf("less than requested\n"); exit(EXIT_FAILURE); }
            written += ret;
            ret = write(fd, buffer, ret);

        }
        close(fd);
    }

#endif
