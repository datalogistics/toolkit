#include <stdio.h>
#include <lors_api.h>
#include <lors_file.h>
#include <popt.h>
#include <string.h>
#include <xndrc.h>


int main (int argc, char **argv)
{
    int i, ret;
    LorsExnode *exnode;
    struct ibp_depot   lbone_server = {"dsj.sinrg.cs.utk.edu", 6767};
    char        *s;
    char        *lbonehost = NULL;
    char        *output_filename = NULL;
    char        *filename;
    int          same_name=0;
    char        *location = "state= tn";
    int         storage_type = IBP_SOFT;
    int         maxdepots = 6;
    double      duration = 2;
    ulong_t     data_blocksize = 1024*1024*5;
    char        *s_data_blocksize = NULL;
    ulong_t     max_buffersize = 1024*1024*5;
    char        *s_max_buffersize = NULL;
    ulong_t     offset=0, length=-1;
    int         copies = 1;
    int         threads = 10;
    int         timeout = 600;
    char        *s_timeout = "600s";
    int         encryption = 0;
    int         how = 0;
    int         v;
    int         file_cnt = 0;
    XndRc       xndrc;
    int         opts = 0;

    poptContext optCon;   /* context for parsing command-line options */
    struct poptOption optionsTable[] = {
  { "version",    'v', POPT_ARG_NONE,   0,                  LORS_VERSION, 
        "Display Version information about this package.", NULL},
  { "verbose",    'V', POPT_ARG_INT,    &xndrc.verbose,        VERBOSE, 
        "Modify the level or mode of Verbosity in output messages.", "0"},
  { "logical",    'l', POPT_ARG_NONE,    0,        LORS_LIST_LOGICAL, 
        "Modify the level or mode of Verbosity in output messages.", "0"},
  { "physical",   'p', POPT_ARG_NONE,    0,        LORS_LIST_PHYSICAL,
        "Modify the level or mode of Verbosity in output messages.", "0"},
  { "human",   'h', POPT_ARG_NONE,    0,        LORS_LIST_HUMAN,
        "Display output in human readable form, rather than demo form.", NULL},
  { "threads",    't', POPT_ARG_INT,    &xndrc.threads,           THREADS, 
        "Specify the maximum number of threads to use to perform Listing.", NULL},
  { "timeout",    'T', POPT_ARG_STRING,    &s_timeout,           TIMEOUT, 
        "Specify the maximum time to allow for all operations.", "'3m'"},
  { "demo",  'D', POPT_ARG_NONE, 0, LORS_ARG_DEMO, 
        "Add the messages understood by the LoRS View visualization.", NULL},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };

    parse_xndrc_files(&xndrc);
    g_lors_demo = xndrc.demo;

    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<filename>");

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
            case TIMEOUT:
                if ( strtotimeout(s_timeout, &xndrc.timeout) != 0 )
                {
                    fprintf(stderr, "Value too large for type.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case LORS_LIST_LOGICAL:
                opts |= LORS_LIST_LOGICAL;
                break;
            case LORS_LIST_PHYSICAL:
                opts |= LORS_LIST_PHYSICAL;
                break;
            case LORS_LIST_HUMAN:
                opts |= LORS_LIST_HUMAN;
                break;
            case LORS_ARG_DEMO:
                g_lors_demo = (!g_lors_demo);
                break;
            case LORS_VERSION:
            {
                const char *package;
                double version;
                lorsGetLibraryVersion(&package, &version);
                fprintf(stderr, "Version: %s %f\n", package, version);
                exit(EXIT_FAILURE);
            }
                break;
            default:
                break;
                /*fprintf(stderr, "Unsupported option: %d\n", v);*/
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
        file_cnt++;
        /*fprintf(stderr, "%s %d %d\n", filename, threads, timeout);*/
        ret = lorsListFile(filename, xndrc.threads, xndrc.timeout, opts); 
        if ( ret != LORS_SUCCESS )
        {
            fprintf(stderr, "LORS_ERROR: %d\n", ret);
            continue;
        }
    }
    if ( file_cnt == 0 )
    {
        fprintf(stderr, "Error. You must also provide a filename on "
                "which lors_upload operates.\n");
        poptPrintHelp(optCon, stderr, 0);
        exit(EXIT_FAILURE);
    } else 
    {
        free_xndrc(&xndrc);
    }
    poptFreeContext(optCon);
    sleep(1);
    return 0;
}
