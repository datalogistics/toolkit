#include <stdio.h>
#include <lors_api.h>
#include <lors_file.h>
#include <popt.h>
#include <string.h>
#include <xndrc.h>


/* int segs2name (poptContext  *optCon, char **filename, int *seg_list); */
int main (int argc, char **argv)
{
    int i, ret;
    LorsExnode *exnode;
    char        *s;
    char        *output_filename = "-";
    const char        *filename;
    int          same_name=0;
    int         v;
    int         file_cnt = 0;
    int         opts = LORS_MANAGE_CAP;
    XndRc       xndrc;

    poptContext optCon;   /* context for parsing command-line options */
    struct poptOption optionsTable[] = {
  { "samename",   'f', POPT_ARG_NONE,   0,                  SAME_OUTPUTNAME, 
        "Save the modified exNode to the original exNode filename.", NULL },
  { "outputfile", 'o', POPT_ARG_STRING, &output_filename,   OUTPUTNAME, 
        "Specify the name for the output exNode file.", NULL},
  { "read-cap",   'r', POPT_ARG_NONE,   0,                  LORS_READ_CAP, 
        "Delete the read capabilities from the exNode.", NULL},
  { "write-cap",  'w', POPT_ARG_NONE,   0,                  LORS_WRITE_CAP, 
        "Delete the write capabilities from the exNode.", NULL},
  { "manage-cap", 'm', POPT_ARG_NONE,   0,                  LORS_MANAGE_CAP, 
        "Delete the manage capabilities from the exNode.", NULL},
  { "version",    'v', POPT_ARG_NONE,   0,                  LORS_VERSION, 
        "Display Version information about this package.", NULL},
  { "verbose",    'V', POPT_ARG_INT,    &xndrc.verbose,        VERBOSE, 
        "Modify the level or mode of Verbosity in output messages.", "0"},
  /* { "demo",  'D', POPT_ARG_NONE, 0, LORS_ARG_DEMO, 
        "Add the messages understood by the LoRS View visualization.", NULL}, */
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
            case LORS_READ_CAP:
            case LORS_WRITE_CAP:
            case LORS_MANAGE_CAP:
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
    while ( (filename = poptGetArg(optCon)) != NULL )
    {
        file_cnt++;
        /* output_filename= NULL;*/
	if (same_name == 1) output_filename = filename;
        ret = lorsModifyFile(filename, output_filename, opts);
        if ( ret != LORS_SUCCESS )
        {
            fprintf(stderr, "An error occured during modify: %d\n", ret);
        }
    }
    if ( file_cnt == 0 )
    {
        fprintf(stderr, "Error. You must also provide a filename on "
                "which lors_modify operates.\n");
        poptPrintHelp(optCon, stderr, 0);
        exit(EXIT_FAILURE);
    }
    free_xndrc(&xndrc);
    poptFreeContext(optCon);
    return 0;
}

