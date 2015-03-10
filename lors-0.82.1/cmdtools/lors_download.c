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

int main (int argc, char **argv)
{
    int i, ret;
    LorsExnode *exnode;
    struct ibp_depot   lbone_server = {"dsj.sinrg.cs.utk.edu", 6767};
    IBP_depot       lbs;
    char        *s;
    char        *lbonehost = NULL;
    char        *output_filename = "-";
    int         same_output = 0;
    const char  *filename;
    char        *location = "state= tn";
    int         storage_type = IBP_SOFT;
    int         maxdepots = 6;
    double      duration = 2;
    ulong_t     data_blocksize = 1024*1024*5;
    char        *s_data_blocksize = NULL;
    ulong_t     max_buffersize = 1024*1024*45;
    char        *s_max_buffersize = NULL;
    ulong_t     prebuffer = 1;
    char        *s_prebuffer = NULL;
    int         cache = 2;
    ulong_t     offset=0, length=0;
    int         copies = 1;
    int         threads = 10;
    int         timeout = 600;
    char        *s_timeout = "600s";
    int         encryption = 0;
    int         v;
    int         try1 = 0;
    int         file_cnt = 0;
    int         dp_threads = -1;
    int         job_threads = 2;
    int         progress = 5;
    XndRc       xndrc;
    int         opts = 0;
	char       *report_host = NULL;

    poptContext optCon;   /* context for parsing command-line options */
    struct poptOption optionsTable[] = {
  { "samename",   'f', POPT_ARG_NONE,   0,                  SAME_OUTPUTNAME, 
        "Specify the exNode filename by appending .xnd to the original filename.", NULL },
  { "outputfile", 'o', POPT_ARG_STRING, &output_filename,   OUTPUTNAME, 
        "Specify a specific name for the output exNode file.", NULL},
  { "version",    'v', POPT_ARG_NONE,   0,                  LORS_VERSION, 
        "Display Version information about this package.", NULL},
  { "verbose",    'V', POPT_ARG_INT,    &xndrc.verbose,        VERBOSE, 
        "Modify the level or mode of Verbosity in output messages.", "0"},
  { "thread-depot", 'a', POPT_ARG_INT,    &dp_threads, THREADDEPOT, 
        "Specify the maxmium connection to any single IBP depot.", 
        "-1"},
  { "redundancy", 'r', POPT_ARG_INT,    &job_threads, REDUNDANCY , 
        "Specify the maxmium number of threads working on a single download job.", 
        "1"},
  { "resume", 'R', POPT_ARG_NONE,    0, LORS_RECOVER, 
       "Download will resume working on a partially downloaded file if one is detected.", 
        NULL},
  { "progress", 'p', POPT_ARG_INT,    &progress, PROGRESS, 
        "The progress number specifies how many completed blocks are allowed before "
        "a download of a pending block is duplicated.", 
        "0"},
  /*{ "location",   'l', POPT_ARG_STRING, &xndrc.location,          LOCATION_HINT, 
        "Specify a location hint to pass the LBone Query.", "'x'"},*/
  { "bs", 'b', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH, &s_data_blocksize,  DATA_BLOCKSIZE, 
        "Specify the logical blocksize of transfers.", "'5M'"},
  { "threads",    't', POPT_ARG_INT,    &xndrc.threads,           THREADS, 
        "Specify the maximum number of threads to use to perform Download.", NULL},
  { "timeout",    'T', POPT_ARG_STRING,    &s_timeout,           TIMEOUT, 
        "Specify the maximum time to allow for all operations.", "'3m'"},
  { "prebuffer", 'q', POPT_ARG_INT, &prebuffer,  PREBUFFER_SIZE, 
        "Specify the number of blocks to prebuffer before outputting to file.", NULL},
  { "cache", 'C', POPT_ARG_INT, &cache,  CACHE_SIZE, 
        "Specify the number of block-sized memory buffers to use during download.", NULL},
  { "buffersize", 'M', POPT_ARG_STRING, &s_max_buffersize,  MAX_BUFFSIZE, 
        "Specify the maximum internal buffer size to use on operations.", NULL},
  { "offset",     'O', POPT_ARG_LONG,    &offset,           FILE_OFFSET, 
        "Specify an offset relative to the exNode from which download will begin",NULL},
  { "length",     'L', POPT_ARG_LONG,    &length,           FILE_LENGTH, 
        "Specify a length other than the logical extent of the exNode.", NULL},
  { "demo",  'D', POPT_ARG_NONE, 0, LORS_ARG_DEMO, 
        "Add the messages understood by the LoRS View visualization.", NULL},
  { "visualize-progress",  'X', POPT_ARG_STRING, &report_host, VISUALIZE_PROGRESS, 
        "Http link to visualizer with port e.g. http://dlt.incntre.iu.edu:8000 .", NULL},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };


    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<filename>");
#ifndef _MINGW
    signal(SIGPIPE, sigpipe_handle);
#endif

    if ( argc < 2 )
    {
        poptPrintUsage(optCon, stderr, 0);
        poptPrintHelp(optCon, stderr, 0);
        exit(EXIT_FAILURE);
    }
    lbs = &lbone_server;
    parse_xndrc_files(&xndrc);
    g_lors_demo = xndrc.demo;

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
                break;
            case MAX_BUFFSIZE:
                if ( strtobytes(s_max_buffersize, &xndrc.max_buffersize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
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
                xndrc.storage_type = IBP_HARD;
                break;
            case STORAGE_TYPE_SOFT:
                xndrc.storage_type = IBP_SOFT;
                break;
            case LORS_RECOVER:
                opts |= LORS_RECOVER;
                break;
            case PREBUFFER_SIZE:
                if ( prebuffer <= 0 ){
                    fprintf(stderr,"Invalid pre-buffer number !\n");
                    exit(-1);
                };
                break;
            case CACHE_SIZE:
                if ( cache < 0 ) { cache = 0 ; };
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
            case SAME_OUTPUTNAME:
                same_output = 1;
                break;
            default:
                /* TODO: be more specific: */
                /*fprintf(stderr, "Unsupported option: %d\n", v);*/
                break;
        }

    }
    g_db_level = xndrc.verbose;
    if ( v < -1 )
    {
        fprintf(stderr, "error: %d\n", v);
        fprintf(stderr, "%s: %s\n",
                 poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
                 poptStrerror(v));
    }


    while ( (filename = poptGetArg(optCon)) != NULL )
    {
        longlong x = 0;
        longlong y = 0;
        file_cnt++;
        if ( same_output == 1 ) { output_filename = NULL; }

        lorsDebugPrint(D_LORS_VERBOSE, "%s %s %d %d %ld %d %d\n",
                      filename, output_filename, offset, length, 
                      xndrc.max_buffersize, xndrc.threads, xndrc.timeout);

        lorsDebugPrint(D_LORS_VERBOSE, "offset: %d, length: %d\n", offset, length);
        lorsDebugPrint(D_LORS_VERBOSE, "File: %s--------------------------------------------\n", filename);
        
        x = length;
        y = offset;

        if ( lbonehost == NULL )
        {
            lbs = xndrc.lboneList[0]; 
            try1 = 1;
        } else 
        {
            lbs = &lbone_server;
            try1 = 0;
        }
        do 
        {
            ret = lorsDownloadFile(filename, output_filename, y, x,
                               xndrc.data_blocksize,
                               xndrc.max_buffersize,
                               prebuffer,
                               dp_threads,
                               job_threads,
                               progress,
                               cache,
                               lbs->host,
                               lbs->port,
                               xndrc.location,xndrc.resolution_file,
                               xndrc.threads, xndrc.timeout, 
							   report_host, "12345AS", opts); 
         
            if ( ret != LORS_SUCCESS )
            {
                if ( ret == LORS_LBONE_FAILED && xndrc.lboneList[try1] != NULL )
                {
                    fprintf(stderr, "%s: %d\n", xndrc.lboneList[try1]->host, 
                            xndrc.lboneList[try1]->port);
                    fprintf(stderr, "LBone failure: trying next server\n");
                    lbs = xndrc.lboneList[try1];
                    try1++;
                } else 
                {
                    fprintf(stderr, "LORS_ERROR: %d\n", ret);
                    exit(EXIT_FAILURE);
                }
            }
        } while ( ret != LORS_SUCCESS );
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
    return 0;
}
