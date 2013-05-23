#include <stdio.h>
#include <lors_api.h>
#include <lors_file.h>
#include <popt.h>
#include <string.h>
#include <xndrc.h>


int segs2name (poptContext  *optCon, char **filename, int *seg_list);
int main (int argc, char **argv)
{
    int i, ret;
    LorsExnode *exnode;
    struct ibp_depot   lbone_server = {"dsj.sinrg.cs.utk.edu", 6767};
    char        *s;
    char        *lbonehost = NULL;
    char        *output_filename = "-";
    const char        *filename;
    int          same_name=0;
    char        *location = "state= tn";
    int         storage_type = IBP_SOFT;
    int         maxdepots = 6;
    time_t      duration = 0;
    char        *segments;
    ulong_t     offset=0, length=-1;
    int         copies = 1;
    int         threads = 10;
    int         timeout = 600;
    char        *s_timeout = "600s";
    int         encryption = 0;
    int         v;
    int         file_cnt = 0;
    double      days = 0;
    int         opts = LORS_TRIM_NOKILL;
    int         have_segs = 0;
    int         seg_list[1024];
    int         seg_cnt = 0;
    XndRc       xndrc;

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
  { "threads",    't', POPT_ARG_INT,    &xndrc.threads,           THREADS, 
        "Specify the maximum number of threads to use to perform Listing.", NULL},
  { "timeout",    'T', POPT_ARG_STRING,    &s_timeout,           TIMEOUT, 
        "Specify the maximum time to allow for all operations.", "'3m'"},
  { "mappings",   'm', POPT_ARG_NONE, 0,          TRIM_SEGMENTS, 
        "Specify the mappings to trim from the exnode.", NULL},
  { "decrement",  'd', POPT_ARG_NONE,          0,           LORS_TRIM_DECR, 
        "Decrement the read reference count before removing from the exnode.", NULL},
  { "non-decrement",'n',POPT_ARG_NONE,       0,           LORS_TRIM_NOKILL, 
        "Do nothing to specified mappings before removing from the exnode.", NULL},
  { "unavailable",'u', POPT_ARG_NONE,          0,           LORS_TRIM_DEAD, 
        "Remove only unavailable mappings from the exnode.", NULL},
  { "demo",  'D', POPT_ARG_NONE, 0, LORS_ARG_DEMO, 
        "Add the messages understood by the LoRS View visualization.", NULL},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };

    /* the default for 'ALL' */
    seg_list[0] = -1;

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
            case TRIM_SEGMENTS:
                have_segs = 1;
                break;
            case LORS_TRIM_DECR:
            case LORS_TRIM_NOKILL:
            case LORS_TRIM_DEAD:
                opts |= v;
                break;
            case SAME_OUTPUTNAME:
                same_name = 1;
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
                break;
            }
            default:
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
    if ( have_segs )
    {
        ret = segs2name(&optCon, &filename, seg_list);
        if ( ret != 0 )
        {
            fprintf(stderr, "cmdline error\n");
            poptPrintHelp(optCon, stderr, 0);
            exit(EXIT_FAILURE);
        }
        if ( filename != NULL )
        {
            file_cnt++;
	    
	    if ( output_filename != NULL && file_cnt != 1 && same_name == 0 )
	    {
		fprintf(stderr, "Parameter: -o valid for first filename only.\n");
		output_filename = NULL;
	    }

            if ( same_name == 1 && file_cnt == 1 ) 
            { 
                output_filename = NULL; 
            } 

            ret = lorsTrimFile(filename, output_filename, seg_list, 
                           xndrc.threads, xndrc.timeout, opts); 
            if ( ret != LORS_SUCCESS )
            {
                fprintf(stderr, "An error occured during trim: %d\n", ret);
            }
        }
    } else{
        while ( (filename = poptGetArg(optCon)) != NULL )
        {
            file_cnt++;
            if ( output_filename != NULL && file_cnt != 1 )
            {
                fprintf(stderr, "Parameter: -o valid for first filename only.\n");
                output_filename= NULL;
            } 
            if ( same_name == 1 && file_cnt == 1 ) 
            {
                output_filename = NULL;
            }
            ret = lorsTrimFile(filename, output_filename, seg_list, 
                           xndrc.threads, xndrc.timeout, opts); 
            if ( ret != LORS_SUCCESS )
            {
                fprintf(stderr, "An error occured during trim: %d\n", ret);
            }
        }
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

int segs2name(poptContext  *optCon, char **filename, int *seg_list)
{
    int seg_cnt = 0;
    const char *s;
    *filename = NULL;
    do 
    {
        s = poptGetArg(*optCon);
        if ( s == NULL ) break;
        if ( !isdigit(s[0]) ) 
        {
            *filename = (char *)s;
            continue;
        }
        seg_list[seg_cnt] = atoi(s);
        seg_cnt++;
    } while ( seg_cnt < 1024);
    if ( seg_list != NULL )
    {
        seg_list[seg_cnt] = -1;
    }
    if ( seg_cnt == 0 && seg_list != NULL )
    {
        return -1;
    }
    return 0;
}
