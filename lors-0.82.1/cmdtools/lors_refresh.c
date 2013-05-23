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
    const char        *filename;
    int          same_name=0;
    char        *location = "state= tn";
    int         storage_type = IBP_SOFT;
    int         maxdepots = 6;
    time_t      duration = 0;
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
    int         v;
    int         file_cnt = 0;
    double      days = 0;
    int         opts =0;
    XndRc       xndrc;

    poptContext optCon;   /* context for parsing command-line options */
    struct poptOption optionsTable[] = {
  { "version",    'v', POPT_ARG_NONE,   0,                       LORS_VERSION, 
        "Display Version information about this package.", NULL},
  { "verbose",    'V', POPT_ARG_INT,    &xndrc.verbose,          VERBOSE, 
        "Modify the level or mode of Verbosity in output messages.", "0"},
  { "threads",    't', POPT_ARG_INT,    &xndrc.threads,          THREADS, 
        "Specify the maximum number of threads to use to perform Listing.", NULL},
  { "timeout",    'T', POPT_ARG_STRING,    &s_timeout,           TIMEOUT, 
        "Specify the maximum time to allow for all operations.", "'3m'"},
  { "maximum",    'm', POPT_ARG_NONE,               0,           LORS_REFRESH_MAX, 
        "Set expiration time of mappings to the maximum time allowed by Depot.", NULL},
  { "add",        'd', POPT_ARG_STRING,    &s_timeout,           LORS_REFRESH_EXTEND_BY, 
        "Extend the expiration by the specified amount of time. The available modifiers "
        "are 'm' 'h' 'd' for 'minute' 'hour' and 'days' respectively.  Default is seconds.", NULL},
  { "set",        's', POPT_ARG_STRING,    &s_timeout,           LORS_REFRESH_EXTEND_TO, 
        "Set expiration to be specified amount of time from the present.  The available"
        " modifiers are 'm' 'h' 'd'.  Default is seconds.", NULL},
  { "absolute",   'a', POPT_ARG_INT,        &duration,           LORS_REFRESH_ABSOLUTE, 
        "Set expiration to an absolute time.", "<abs time>"},
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
        free_xndrc(&xndrc);
        poptFreeContext(optCon);
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
                    free_xndrc(&xndrc);
                    poptFreeContext(optCon);
                    exit(EXIT_FAILURE);
                }
                break;
            case LORS_REFRESH_MAX:
                opts |= v;
                break;
            case LORS_REFRESH_EXTEND_BY:
            case LORS_REFRESH_EXTEND_TO:
                if ( strtotimeout(s_timeout, &duration) != 0 )
                {
                    fprintf(stderr, "Value too large for type.\n");
                    free_xndrc(&xndrc);
                    poptFreeContext(optCon);
                    exit(EXIT_FAILURE);
                }
            case LORS_REFRESH_ABSOLUTE:
                opts |= v;
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
                free_xndrc(&xndrc);
                poptFreeContext(optCon);
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

        /* translate days into seconds. */

        /*fprintf(stderr, "days:%f\n", days);*/
        /*duration = (time_t)(days*24.0*60.0*60.0);*/
        /*fprintf(stderr, "duration:%d\n", duration);*/
        ret = lorsRefreshFile(filename, duration, xndrc.threads, xndrc.timeout, opts); 
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
        free_xndrc(&xndrc);
        poptFreeContext(optCon);
        exit(EXIT_FAILURE);
    }
    
    free_xndrc(&xndrc);
    poptFreeContext(optCon);
    return 0;
}
