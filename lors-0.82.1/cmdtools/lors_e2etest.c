#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lors_api.h>
#include <xndrc.h>
#include <popt.h>
#include <lors_file_opts.h>
#include <libend2end.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

int     e2e_type;
typedef struct __LorsDesStruct 
{
    pthread_t         tid;
    char             *enc;
    char             *dec;
    char             *input;
    unsigned long int length;
    unsigned long int blocksize;
    unsigned long int new_length;
    unsigned long int new_blocksize;
    char             *key;
    char             *key2;
} LorsDesStruct;
int touch_each_byte(char *in, unsigned long int length, char *key);

/* return in 'lrp'.
 * the DepotPool will be built on 'depot'
 * inputSet is set to 'set'.
 * outputSet is allocated for you.
 */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
void hex_key_gen(char *key, int len);
int createDesStruct(LorsDesStruct **lrp, ulong_t blocksize, ulong_t e2e_blocksize, int fd)
                    
{
    LorsDesStruct   *rp = NULL;
    int             lret = 0;
    char            key[8];


    rp = calloc(1, sizeof(LorsDesStruct));

    rp->length = blocksize;
    rp->blocksize = e2e_blocksize;

    fprintf(stderr, "length: %d blocksize: %d\n", rp->length, rp->blocksize);
    rp->input = calloc(1, rp->length);
    rp->dec = calloc(1, rp->length);
    gen_key(key);
    rp->key = strdup(key);

    /*rp->key2 = malloc(17);
    rp->key2[16] = '\0';
    hex_key_gen(rp->key2, 16);*/
    rp->key2 = strdup("9bf5ab652e143b74");

    /*fprintf(stderr, "key: %s\n", rp->key);*/
    lret = read(fd, rp->input, rp->length);
    if ( lret <= 0 )
    {
        perror("read");
        exit(EXIT_FAILURE);
    }
    memcpy(rp->dec, rp->input, rp->length);

    *lrp = rp;

    return LORS_SUCCESS;
}
void *lorsDesEnc(void *v)
{
    int lret;
    char *in, *enc;
    LorsDesStruct   *lrp;
    longlong            offset = 0;
    longlong            len;

    lrp = (LorsDesStruct *)v;

    if ( e2e_type == 0 )
    {
        lret = xor_EncryptMapping(lrp->dec, lrp->length, lrp->blocksize, 
                              &(lrp->enc),&(lrp->new_length), &(lrp->new_blocksize),
                              lrp->key);
    } else if ( e2e_type == 1 )
    {
        lret = new_xor_EncryptMapping(lrp->dec, lrp->length, lrp->blocksize, 
                              &(lrp->enc),&(lrp->new_length), &(lrp->new_blocksize),
                              lrp->key);
    } else if ( e2e_type == 2 ) {
        lret = des_EncryptMapping(lrp->dec, lrp->length, lrp->blocksize, 
                              &(lrp->enc),&(lrp->new_length), &(lrp->new_blocksize),
                              lrp->key);
    } else if ( e2e_type == 3 ) {

        byte_xor(lrp->dec, lrp->length, lrp->key);
        lrp->enc = lrp->dec;

    } else if ( e2e_type == 4 ) {

        lret = new_xor_EncryptMapping(lrp->dec, lrp->length, lrp->blocksize, 
                              &(lrp->enc),&(lrp->new_length), &(lrp->new_blocksize),
                              "abcd");
        /*int_xor(lrp->dec, lrp->length, lrp->key);*/
        lrp->enc = lrp->dec;

    } else if ( e2e_type == 5 ) {

        vector_xor(lrp->dec, lrp->length, lrp->key);
        lrp->enc = lrp->dec;
        
    } else if ( e2e_type == 6 ) {
        /*pthread_mutex_lock(&lock);*/
        lret = aes_EncryptMapping(lrp->dec, lrp->length, lrp->blocksize, 
                              &(lrp->enc),&(lrp->new_length), &(lrp->new_blocksize),
                              lrp->key2);
        /*pthread_mutex_unlock(&lock);*/
        /*fprintf(stderr, "lret: %d\n", lret);*/
    } else if ( e2e_type == 7 ) {
    } else {
        lrp->enc = lrp->dec;
    }

    return NULL;
}
void *lorsDesDec(void *v)
{
    int lret;
    char *dec, *enc;
    LorsDesStruct   *lrp;
    longlong            offset = 0;
    longlong            len;

    lrp = (LorsDesStruct *)v;

    if ( e2e_type == 0 )
    {
        lret = xor_DecryptMapping(lrp->enc,lrp->new_length, lrp->new_blocksize, 
            &(lrp->dec),&(lrp->length),lrp->key);
    } else if ( e2e_type == 1 ) 
    {
        lret = new_xor_DecryptMapping(lrp->enc,lrp->new_length, lrp->new_blocksize, 
            &(lrp->dec),&(lrp->length),lrp->key);
    } else if ( e2e_type == 2 ) {
        lret = des_DecryptMapping(lrp->enc,lrp->new_length, lrp->new_blocksize, 
            &(lrp->dec),&(lrp->length),lrp->key);
    } else if ( e2e_type == 3 ) {

        byte_xor(lrp->dec, lrp->length, lrp->key);
        lrp->dec = lrp->enc;

    } else if ( e2e_type == 4 ) {

        lret = new_xor_DecryptMapping(lrp->enc,lrp->new_length, lrp->new_blocksize, 
                        &(lrp->dec),&(lrp->length),
                        "abcd");
        /*int_xor(lrp->dec, lrp->length, lrp->key);*/
        lrp->dec = lrp->enc;

    } else if ( e2e_type == 5 ) {

        vector_xor(lrp->dec, lrp->length, lrp->key);
        lrp->dec = lrp->enc;

    } else if ( e2e_type == 6 ) {
        /*pthread_mutex_lock(&lock);*/
        lret = aes_DecryptMapping(lrp->enc,lrp->new_length, lrp->new_blocksize, 
            &(lrp->dec),&(lrp->length),lrp->key2);

        /*pthread_mutex_unlock(&lock);*/
        /*fprintf(stderr, "lret: %d\n", lret);*/
    } else if ( e2e_type == 7 ) {
    } else {
        lrp->dec = lrp->enc;
    }

    if ( lrp->dec == NULL )
    {
        char *s = NULL;
        fprintf(stderr, "decrypt returned NULL!!!! bah.\n");
        fprintf(stderr, "%c\n", *s);    /* segfault */
    }
    return NULL;
}
#if 0
#include <Carbon.h>
#endif
int vector_xor(char *in, unsigned long int length, char *key)
{
    int i, j;
#if 0
    vector signed char k1 = (vector signed char )( 0xf0, 0xbc, 0xf0, 0xbc, 
                                                   0xf0, 0xbc, 0xf0, 0xbc, 
                                                   0x6b, 0xf7, 0x6b, 0xf7, 
                                                   0x6b, 0xf7, 0x6b, 0xf7 );
    vector signed char block;
    longlong in_length;
    int  offset =0;

    in_length = length / 16;
    /*pthread_mutex_lock(&lock);*/
    for (i=0, j=0; i < in_length; i++)
    {
        /* read 16 bytes into vector block */
        block = vec_ld(0, (signed char *)(in+offset));
        /* xor block with the key */
        block = vec_xor(block, k1);
        /* store the block into the position we just read from */
        vec_st(block,0,(signed char *)(in+offset));
        offset += 16;
    }
    /*pthread_mutex_unlock(&lock);*/
#endif
    return 0;
}
int byte_xor(char *in, unsigned long int length, char *key)
{
    int i, j, len;

    len = strlen(key);
    pthread_mutex_lock(&lock);
    for (i=0, j=0; i < length; i++)
    {
        in[i] = in[i] ^ key[j];
        j++; if ( j > len ) j = 0;
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

#if 0
int int_xor(char *in, unsigned long int length, char *key)
{
    int i, j;
    int len;
    int in_key = 0xa317be9f;
    char c;
    int *in_int, in_length;
    in_int = (int *)in;

    in_length = length / sizeof(int);
    /*pthread_mutex_lock(&lock);*/
    for (i=0, j=0; i < in_length; i++)
    {
        in_int[i] = in_int[i] ^ in_key;
    }
    /*pthread_mutex_unlock(&lock);*/
    return 0;
}
#endif


/*extern pthread_mutex_t gmut;*/
int main(int argc, char **argv)
{
    LorsDesStruct   **lrp;
    pthread_mutex_t *lock = NULL;
    pthread_cond_t  *cond = NULL;
    int             i, fd, blocks;
    char            *s_data_blocksize = NULL;
    char            *s_e2e_blocksize = NULL;
    const char            *filename;
    XndRc           xndrc;
    poptContext     optCon;   
    LorsExnode      *exnode = NULL;
    LorsSet         *set = NULL;
    ulong_t         maxlength = 0, filesize = 0, read_len;
    ulong_t         diff;
    int             v = 0, lret;
    double          t0, t1, t2, t3;
    struct stat     mstat;

    struct poptOption optionsTable[] = {
    { "bs", 'b', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH, &s_data_blocksize,  
        DATA_BLOCKSIZE, "Specify the logical data blocksize of input file.", "'5M'"},
    { "e2e-blocksize", 'E', POPT_ARG_STRING,   &s_e2e_blocksize,     E2E_BLOCKSIZE, 
        "When specifying e2e conditioning, select an e2e bocksize "
        "which will evenly fit into your chosen Mapping Blocksize.", NULL},
    { "type", 'e', POPT_ARG_INT, &e2e_type, 10, "Specify which encryption method "
                    "is used", NULL},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }

    };
    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<filename>");
    if ( argc < 2 )
    {
        poptPrintUsage(optCon, stderr, 0);
        poptPrintHelp(optCon, stderr, 0);
                                 exit(EXIT_FAILURE);
    }
    parse_xndrc_files(&xndrc);

    while ( (v=poptGetNextOpt(optCon)) >= 0)
    {
        switch(v)
        {
            case DATA_BLOCKSIZE:
                if ( strtobytes(s_data_blocksize, &xndrc.data_blocksize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for "
                                    "internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case E2E_BLOCKSIZE:
                if ( strtobytes(s_e2e_blocksize, &xndrc.e2e_blocksize) != 0 )
                {
                    fprintf(stderr, "The e2e blocksize specified is too large for "
                            "internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 10:
                break;
            default:
                fprintf(stderr, "unknown: %d\n", v);
                break;
        }
    }
    if ( s_e2e_blocksize == NULL )
    {
        xndrc.e2e_blocksize = xndrc.data_blocksize;
    }

    srand(time(0));

    filename = poptGetArg(optCon);
    if ( filename == NULL )
    {
        fprintf(stderr, "error: provide filename\n");
        exit(EXIT_FAILURE);
    }


    lret = stat(filename, &mstat);
    if ( lret != 0 )
    {
        perror("stat");
        exit(EXIT_FAILURE);
    }
    filesize = mstat.st_size;
    blocks = (filesize/xndrc.data_blocksize+1);

    t0 = getTime();
    /*fprintf(stderr, "sizeof(LorsDesStruct): %d\n", sizeof(LorsDesStruct));*/
    fprintf(stderr, "blocks: %d\n", blocks);
    fprintf(stderr, "blocksize: %ld\n", xndrc.data_blocksize);
    fprintf(stderr, "e2e_blocksize: %ld\n", xndrc.e2e_blocksize);
    lrp = (LorsDesStruct **)calloc(1, sizeof(LorsDesStruct*)*blocks);
    for (i=0; i < blocks; i++)
    {
        lrp[i] = (LorsDesStruct *)calloc(1, sizeof(LorsDesStruct));
        /*lrp[i]->dec = NULL;*/
        /*lrp[i]->enc = NULL;*/
    }

#ifdef _MINGW
    fd = open(filename, O_RDONLY|O_BINARY);
#else
    fd = open(filename, O_RDONLY);
#endif
    if ( fd < 0 ) 
    {
        perror("open");
        exit(EXIT_FAILURE);
    }


    read_len = 0;
    for( read_len = 0, i=0; read_len < filesize; read_len+= xndrc.data_blocksize, i++)
    {
        fprintf(stderr, "Creating struct for block %d:\n", i);
        diff = filesize-read_len;
        createDesStruct(&(lrp[i]), 
                        (diff < xndrc.data_blocksize ? diff : xndrc.data_blocksize), 
                        xndrc.e2e_blocksize,
                        fd);
    }

    t1 = getTime();
    for (i=0; i < blocks; i++)
    {
        fprintf(stderr, "Encrypting: %d\n", i);
        pthread_create(&(lrp[i]->tid), NULL, lorsDesEnc, lrp[i]);
        if ( lret != 0 )
        {
            perror("create failed");
            fprintf(stderr, "%d\n", lret);
            exit(EXIT_FAILURE);
        }
        /*pthread_join(lrp[i]->tid, NULL);*/
    }
    for (i=0; i < blocks; i++)
    {
        /*fprintf(stderr, "Joined encryption: %d\n", i);*/
        pthread_join(lrp[i]->tid, NULL);
    }

    t2 = diffTime(t1);
    t1 = getTime();
    for (i=0; i < blocks; i++)
    {
        fprintf(stderr, "Decrypting: %d\n", i);
        lret=pthread_create(&(lrp[i]->tid), NULL, lorsDesDec, lrp[i]);
        if ( lret != 0 )
        {
            perror("create failed");
            fprintf(stderr, "%d\n", lret);
            exit(EXIT_FAILURE);
        }
    }
    for (i=0; i < blocks; i++)
    {
        /*fprintf(stderr, "Joined decryption: %d\n", i);*/
        /*fprintf(stderr, "lrp[i]->dec: 0x%x\n", lrp[i]->dec);*/
        pthread_join(lrp[i]->tid, NULL);
    }
    t3 = diffTime(t1);

    fprintf(stderr, "MESSAGE Time enc: %.4f dec: %.4f total: %.4f speed: %.4f Mbps %.4f MBps\n", 
                     t2, t3, t2+t3, 
                     (double)2*filesize*8.0/(1024.0*1024.0*(t2+t3)),
                     (double)2*filesize/(1024.0*1024.0*(t2+t3)));
    for (i=0; i < blocks; i++)
    {
        /*fprintf(stderr, "i: %d input: 0x%x dec: 0x%x length: %d\n",*/
                    /*i, lrp[i]->input, lrp[i]->dec, lrp[i]->length);*/
        if ( memcmp(lrp[i]->input, lrp[i]->dec, lrp[i]->length) != 0 )
        {
            fprintf(stderr, "ERROR: block %d differs from input\n", i);
            fprintf(stderr, "--------------------\n");
            write(1, lrp[i]->dec, lrp[i]->length);
            fprintf(stderr, "--------------------\n");
            write(1, lrp[i]->input, lrp[i]->length);
            fprintf(stderr, "--------------------\n");
            return 1;
        } 
#if 0
        else {
            int j;
            fprintf(stderr, "blocks %d match?\n", i);
            for (j=0; j < 10; j++)
            {
                fprintf(stderr, "%4d %4d %s\n", lrp[i]->input[j], lrp[i]->enc[j], 
                    (lrp[i]->input[j] == lrp[i]->enc[j] ? "yes" : "no") );
            }
        }
#endif
    }

    t2 = diffTime(t0);
    fprintf(stderr, "MESSAGE 3 Time total %.4f \n", t2);
    return 0;
}
