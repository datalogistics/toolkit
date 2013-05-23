#include <stdio.h>
#include <lors_api.h>
#include <lors_file.h>
#include <popt.h>
#include <string.h>
#include <signal.h>

#include <xndrc.h>
void sigpipe_handle(int d)
{
    fprintf(stderr, "You've got sigpipe: %d\n", d);
#ifndef _MINGW
    signal(SIGPIPE, sigpipe_handle);
#endif
    /*exit(EXIT_FAILURE);*/
}
#if 0
void sigint_handle(int d)
{
    fprintf(stderr, "You've got sigint: %d\n", d);
    signal(SIGINT, sigint_handle);
    /*exit(EXIT_FAILURE);*/
}
#endif

int main (int argc, char **argv)
{
    int i, ret;
    LorsExnode *exnode;
    struct ibp_depot   lbone_server = {"dsj.sinrg.cs.utk.edu", 6767};
    IBP_depot          lbs = NULL,*dp_list = NULL;
    char        *s;
    char        *lbonehost = NULL;
    char        *output_filename = NULL;
    const char  *filename = NULL;
    int          same_name=0;
    char        *s_data_blocksize = NULL;
    char        *s_e2e_blocksize = NULL;
    char        *s_max_buffersize = NULL;
    ulong_t     offset=0;
    long        length=-1;
    char *      s_offset=NULL;
    char *      s_length=NULL;
    char        *s_timeout = "600s";
    char        *s_duration = "1d";
    int         encryption = 0;
    int         v;
    int         none=0;
    int         depot_list = 0;
    int         file_cnt = 0;
    XndRc       xndrc;
    XndRc       drc;
    int         *e2e_order_tmp = NULL;
    int         e2e_cnt = 0;
    int         try1 = 0;
    int         type_set = 0;
    int         fragments=0;
    char        *rcfile=NULL;

    poptContext optCon;   /* context for parsing command-line options */

    struct poptOption optionsTable[] = {
  { "samename",   'f', POPT_ARG_NONE,   0,                  SAME_OUTPUTNAME, 
        "Specify the exNode filename by appending .xnd to the original filename.", NULL },
  { "stdin",   'i', POPT_ARG_NONE,   0,                  READ_STDIN, 
        "Read from standard input rather than a specified filename.", NULL },
  { "outputfile", 'o', POPT_ARG_STRING, &output_filename,   OUTPUTNAME, 
        "Specify a specific name for the output exNode file.", NULL},
  { "version",    'v', POPT_ARG_NONE,   0,                  LORS_VERSION, 
        "Display Version information about this package.", NULL},
  { "verbose",    'V', POPT_ARG_INT,    &xndrc.verbose,        VERBOSE, 
        "Modify the level or mode of Verbosity in output messages.", "0"},
  { "lbone-host", 'H', POPT_ARG_STRING, &lbonehost,         LBONESERVER, 
        "Specify an L-Bone Server for resource discover and proxmity resolution.", NULL},
  { "lbone-port", 'P', POPT_ARG_INT,    &lbone_server.port, LBONEPORT, 
        "Specify the port number when using an L-Bone Server on a non standard port.", 
        "6767"},
  { "location",   'l', POPT_ARG_STRING, &xndrc.location,          LOCATION_HINT, 
        "Specify a location hint to pass the L-Bone Query.", "\"state= TN\""},
  { "duration",   'd', POPT_ARG_STRING,     &s_duration,          DURATION, 
        "Specify the amount of time to allocate storage. The available modifiers are "
        "'m' 'h' 'd' for 'minute' 'hour' and 'days' respectively. Default is seconds. "
        "(e.g. -d 1d or -d 1h)", NULL},
  { "soft",       's', POPT_ARG_NONE,   0,                  STORAGE_TYPE_SOFT, 
        "Specify SOFT storage for storage allocations.", NULL},
  { "hard",       'h', POPT_ARG_NONE,   0,                  STORAGE_TYPE_HARD, 
        "Specify HARD storage for storage allocations.", NULL},
  { "maxdepots",  'm', POPT_ARG_INT,   &xndrc.maxdepots,          MAX_DEPOTS, 
        "Specify the maximum number of depots returned by the 'location' hint.", "6"},
  { "bs", 'b', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH, &s_data_blocksize,  DATA_BLOCKSIZE, 
        "Specify the logical data blocksize of input file.", "'5M'"},
  { "copies",     'c', POPT_ARG_INT,    &xndrc.copies,            COPIES, 
        "Specify the number of copies.", "1"},
  { "fragments",  'F', POPT_ARG_INT,     &fragments,      NUM_FRAGMENTS, 
        "Rather than specifying the logical data blocksize, specify the number of "
        "blocks into which the file should be divided.", "4" },
  { "depot-list",        0, POPT_ARG_NONE,   0,        DEPOT_LIST, 
        "Only use depots specified in .xndrc file", NULL},
  { "threads",    't', POPT_ARG_INT,    &xndrc.threads,           THREADS, 
        "Specify the maximum number of threads to use to perform Upload.", NULL},
  { "timeout",    'T', POPT_ARG_STRING,    &s_timeout,           TIMEOUT, 
        "Specify the maximum time to allow for all operations.", "'3m'"},
  { "buffersize", 'M', POPT_ARG_STRING, &s_max_buffersize,  MAX_BUFFSIZE, 
        "Specify the maximum internal buffer to use on operations.", NULL},
  { "offset",     'O', POPT_ARG_STRING,    &s_offset,           FILE_OFFSET, 
        "Specify an offset relative to the exNode from which download will begin",NULL},
  { "length",     'L', POPT_ARG_STRING,    &s_length,           FILE_LENGTH, 
        "Specify a length other than the logical extent of the exNode.", NULL},
  { "none",        'n', POPT_ARG_NONE,   0,   LORS_NO_E2E, 
        "Turn off all e2e services.", NULL},

  { "des",        'e', POPT_ARG_NONE|POPT_ARGFLAG_OR,   &encryption,   DES_ENCRYPTION, 
        "Turn on des encryption.", NULL},
  { "aes",        'a', POPT_ARG_NONE|POPT_ARGFLAG_OR,   &encryption,   AES_ENCRYPTION, 
        "Turn on aes encryption.", NULL},
  { "compress",   'z', POPT_ARG_NONE|POPT_ARGFLAG_OR,   &encryption,   ZLIB_COMPRESS, 
        "Turn on zlib compression.", NULL},
  { "xor",        'x', POPT_ARG_NONE|POPT_ARGFLAG_OR,   &encryption,   XOR_ENCRYPTION, 
        "Turn on xor encryption.", NULL},
  { "checksum",   'k', POPT_ARG_NONE|POPT_ARGFLAG_OR,   &encryption,   ADD_CHECKSUM, 
        "Turn on checksum.", NULL},

  { "e2e-blocksize", 'E', POPT_ARG_STRING,   &s_e2e_blocksize,     E2E_BLOCKSIZE, 
        "When specifying e2e conditioning, select an e2e bocksize "
        "which will evenly fit into your chosen Mapping Blocksize.", NULL},
  { "xndrc", 'x', POPT_ARG_STRING, &rcfile, RCFILE, 
        "You may specify an alternate source for the upload DepotList.", "<filename>"},
  { "demo",  'D', POPT_ARG_NONE, 0, LORS_ARG_DEMO, 
        "Add the messages understood by the LoRS View visualization.", NULL},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };

    memset(&drc, 0, sizeof(XndRc));
    memset(&xndrc, 0, sizeof(XndRc));

    parse_xndrc_files(&xndrc);
    lbs = &lbone_server;
    g_lors_demo = xndrc.demo;

    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<filename>");
#ifndef _MINGW
    signal(SIGPIPE, sigpipe_handle);
#endif
    /*signal(SIGINT, sigint_handle);*/


    if ( argc < 2 )
    {
        poptPrintUsage(optCon, stderr, 0);
        poptPrintHelp(optCon, stderr, 0);
                                 exit(EXIT_FAILURE);
    }

    while ( (v=poptGetNextOpt(optCon)) >= 0)
    {
        switch(v)
        {
            case DATA_BLOCKSIZE:
                if ( strtobytes(s_data_blocksize, &xndrc.data_blocksize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                if ( fragments != 0 )
                {
                    fprintf(stderr, "Specify either Fragments or Blocksize, not both.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case NUM_FRAGMENTS:
                if ( s_data_blocksize != NULL )
                {
                    fprintf(stderr, "Specify either Fragments or Blocksize, not both.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case MAX_BUFFSIZE:
                if ( strtobytes(s_max_buffersize, &xndrc.max_buffersize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for "
                            "internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case AES_ENCRYPTION:
                fprintf(stderr, "AES: encryption\n");
                xndrc.e2e_order[e2e_cnt] = AES_ENCRYPTION;
                e2e_cnt++;
                break;
            case DES_ENCRYPTION:
                xndrc.e2e_order[e2e_cnt] = DES_ENCRYPTION;
                e2e_cnt++;
                break;
            case XOR_ENCRYPTION:
                xndrc.e2e_order[e2e_cnt] = XOR_ENCRYPTION;
                e2e_cnt++;
                break;
            case ADD_CHECKSUM:
                xndrc.e2e_order[e2e_cnt] = ADD_CHECKSUM;
                e2e_cnt++;
                break;
            case ZLIB_COMPRESS:
                xndrc.e2e_order[e2e_cnt] = ZLIB_COMPRESS;
                e2e_cnt++;
                break;
            case LORS_NO_E2E:
                none = 1;
                break;
            case FILE_OFFSET:
                if ( strtobytes(s_offset, &offset) != 0 )
                {
                    fprintf(stderr, "The offset specified is too large for "
                            "internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case FILE_LENGTH:
                if ( strtobytes(s_length, &length) != 0 )
                {
                    fprintf(stderr, "The length specified is too large for "
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
            case DEPOT_LIST:
                depot_list = 1;
                break;
            case TIMEOUT:
                if ( strtotimeout(s_timeout, &xndrc.timeout) != 0 )
                {
                    fprintf(stderr, "Value too large for type.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case LBONESERVER:
                strncpy(lbone_server.host, lbonehost, 255);
                break;
            case STORAGE_TYPE_HARD:
                if ( type_set > 0 )
                {
                    fprintf(stderr, "Set one of either Hard or Soft storage.\n");
                    exit(EXIT_FAILURE);
                }
                xndrc.storage_type = IBP_HARD;
                type_set++;
                break;
            case STORAGE_TYPE_SOFT:
                if ( type_set > 0 )
                {
                    fprintf(stderr, "Set one of either Hard or Soft storage.\n");
                    exit(EXIT_FAILURE);
                }
                xndrc.storage_type = IBP_SOFT;
                type_set++;
                break;
            case DURATION:
                if ( strtotimeout(s_duration, &xndrc.duration) != 0 )
                {
                    fprintf(stderr, "Value too large for type.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case LORS_VERSION:
            {
                const char *package;
                double version;
                lorsGetLibraryVersion(&package, &version);
                fprintf(stderr, "Version: %s %f\n", package, version);
                exit(EXIT_FAILURE);
                break;
            }
            case LORS_ARG_DEMO:
                g_lors_demo = (!g_lors_demo);
                break;
            case READ_STDIN:
                filename = "-";
                break;
            case SAME_OUTPUTNAME:
                same_name = 1;
                break;
#if 0
            default:
                fprintf(stderr, "Unsupported option: %d\n", v);
#endif
        }

    }
    if ( rcfile != NULL )
    {
        if ( read_xndrc_file(&drc, rcfile) != 0 )
        {
            fprintf(stderr, "Unable to open specified rcfile\n");
            exit(EXIT_FAILURE);
        }
        xndrc.depotList = drc.depotList;
    }

    
    g_db_level = xndrc.verbose;
    if ( v < -1 )
    {
        fprintf(stderr, "error: %d\n", v);
        fprintf(stderr, "%s: %s\n",
                 poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
                 poptStrerror(v));
    }

    /*g_db_level = D_LORS_TERSE | D_LORS_VERBOSE | D_LORS_ERR_MSG ;*/

    if ( depot_list == 1 ){
        dp_list = xndrc.depotList;
    }else {
        dp_list = NULL;
    };
    /* setup default e2e conditioning for upload */
    if ( none > 0 )
    {
        xndrc.e2e_order[0] = -1;
        e2e_cnt = 0;
    } else if ( e2e_cnt == 0 )
    {
        for(; xndrc.e2e_order[e2e_cnt] != -1; e2e_cnt++);
    }

    if ( filename != NULL )
    {
        goto in_while;
    }
    while ( (filename = poptGetArg(optCon)) != NULL )
    {
in_while:
        file_cnt++;
        if ( same_name == 1 )
        {
            int len;
            char *s;
            s = strrchr (filename, '/');
            if ( s == NULL ) s = filename;
            else s++;
            len = strlen(s) + strlen(".xnd") + 1;
            output_filename = malloc(sizeof(char)*len);
            snprintf(output_filename, len, "%s.xnd", s);
        }
        lorsDebugPrint(D_LORS_VERBOSE, "%s %ld %ld %ld %d %s %d %s %d %d %d %ld %d %d\n",
                      filename, offset, length, xndrc.data_blocksize, xndrc.copies, 
                      lbone_server.host, lbone_server.port, xndrc.location, 
                      xndrc.maxdepots, xndrc.storage_type, xndrc.duration, 
                      xndrc.max_buffersize, xndrc.threads, xndrc.timeout);
        if ( e2e_cnt > 0 )
        {
            xndrc.e2e_order[e2e_cnt] = -1;
            e2e_order_tmp = xndrc.e2e_order;
        }
        /* start from first lbone server in xndrc file */
        if ( lbonehost == NULL )
        {
            lbs = xndrc.lboneList[try1];
            try1 = 1;
        } else {
            lbs = &lbone_server;
            try1 = 0;
        }
        do 
        {
            /*fprintf(stderr, "e2e_blocksize: %ld\n", xndrc.e2e_blocksize);*/
            ret = lorsUploadFile(filename, output_filename, offset, (longlong)length, 
                             xndrc.data_blocksize, xndrc.copies, fragments, 
                             xndrc.e2e_blocksize, e2e_order_tmp, lbs, dp_list, 
                             xndrc.location, xndrc.maxdepots, xndrc.storage_type, 
                             xndrc.duration, xndrc.max_buffersize, 
                             xndrc.resolution_file, xndrc.threads, 
                             xndrc.timeout, 0); 
            if ( ret != LORS_SUCCESS && ret != LORS_PARTIAL )
            {
                if ( ret == LORS_LBONE_FAILED && xndrc.lboneList[try1] != NULL )
                {
                    fprintf(stderr, "%s: %d\n", xndrc.lboneList[try1]->host, 
                            xndrc.lboneList[try1]->port);
                    fprintf(stderr, "L-Bone failure: trying next server\n");
                    lbs = xndrc.lboneList[try1];
                    try1++;
                } else 
                {
                    fprintf(stderr, "LORS_ERROR: %d\n", ret);
                    exit(EXIT_FAILURE);
                }
            }
        } while ( ret != LORS_SUCCESS && ret != LORS_PARTIAL );
        free(output_filename);
    }
    if ( file_cnt == 0 )
    {
        fprintf(stderr, "Error. You must also provide a filename on "
                "which lors_upload operates.\n");
        poptPrintHelp(optCon, stderr, 0);
        exit(EXIT_FAILURE);
    }
    free_xndrc(&xndrc);
    poptFreeContext(optCon);
    if ( ret == LORS_PARTIAL ) return 1;
    return 0;
}
