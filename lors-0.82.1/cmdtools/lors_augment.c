/****working version of augment with copy **/

#include <stdio.h>
#include <lors_api.h>
#include <lors_opts.h>
#include <lors_file.h>
#include <popt.h>
#include <string.h>
#include <xndrc.h>


int main (int argc, char **argv)
{
    int i, ret;
    LorsExnode        *exnode = NULL ,*new_exnode = NULL;
    struct ibp_depot   lbone_server = {"galapagos.cs.utk.edu", 6767};
    IBP_depot          lbs = NULL,*dp_list = NULL,*dp_list1 =NULL,*dp_list2 = NULL;
    char        *s;
    int         depot_list = 0;
    int         mcopy_or_copy = 0;
    char        *lbonehost = NULL;
    char        *output_filename = NULL;
    char        *output_dir = NULL;
    char        *new_xndfile = NULL;
    const char        *filename;
    int          same_name=0;
    char        *location = "state= tn";
    int         storage_type = IBP_SOFT;
    int         maxdepots = 6;
    time_t      duration = 60*60*12;
    char        *s_duration = NULL;
    ulong_t     data_blocksize = 1024*1024*5;
    char        *s_data_blocksize = NULL;
    ulong_t     max_buffersize = 1024*1024*5;
    char        *s_max_buffersize = NULL;
    ulong_t     offset=0;
    long         length=-1;
    char        *s_offset = NULL, *s_length = NULL;
    int         copies = 1;
    int         threads = 10;
    int         timeout = 600;
    char        *s_timeout = "600s";
    int         encryption = 0;
    int         v;
    int         file_cnt = 0;
    int         try1 = 0;
    int         opts = 0;
    XndRc       xndrc;
    XndRc       drc;
    char        *rcfile=NULL;
    int         type_set = 0;
    int         fragments = 0;

    poptContext optCon;   /* context for parsing command-line options */
    struct poptOption optionsTable[] = {
  { "samename",   'f', POPT_ARG_NONE,   0,                  SAME_OUTPUTNAME, 
        "Specify the exNode filename by appending .xnd to the original filename.", NULL },
  { "outputfile", 'o', POPT_ARG_STRING, &output_filename,   OUTPUTNAME, 
        "Specify a specific name for the output exNode file.", NULL},
  { "newfile",    'n', POPT_ARG_STRING,    &new_xndfile,  NEWFILENAME, 
        "Specify an output filename which will contain only newly created copies.",NULL},
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
        "'m' 'h' 'd' for 'minute' 'hour' and 'days' respectively. Default is seconds."
        " (e.g. -d 1d or -d 1h)",   NULL},
  { "depot-list",       0, POPT_ARG_NONE,   0,                  DEPOT_LIST, 
        "Only use depots specified in .xndrc file.", NULL},
  { "mcopy",       0, POPT_ARG_NONE,   0,                  MCOPY, 
        "Use IBP_mcopy instead of IBP_copy for augmentation", NULL},
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
  { "balance",    'B', POPT_ARG_NONE,              0,           LORS_BALANCE, 
        "Enforce a minimum number of active copies across any offset of an exNode, adding"
        "copies only when the number of valid copies is less than that specified "
        " by the --copies parameter.  Dead mappings are not removed.", NULL},
  { "fragments",  'F', POPT_ARG_INT,     &fragments,      NUM_FRAGMENTS, 
        "Rather than specifying the logical data blocksize, specify the number of "
        "blocks into which the file should be divided.", "4" },
  { "threads",    't', POPT_ARG_INT,    &xndrc.threads,           THREADS, 
        "Specify the maximum number of threads to use to perform Upload.", NULL},
  { "timeout",    'T', POPT_ARG_STRING,    &s_timeout,           TIMEOUT, 
        "Specify the maximum time to allow for all operations.", "'3m'"},
  { "buffersize", 'M', POPT_ARG_STRING, &s_max_buffersize,  MAX_BUFFSIZE, 
        "Specify the maximum internal buffer to use on operations.", NULL},
  { "offset",     'O', POPT_ARG_STRING,    &s_offset,           FILE_OFFSET, 
        "Specify an offset relative to the exNode from which augmentation will begin",
        NULL},
  { "length",     'L', POPT_ARG_STRING,    &s_length,           FILE_LENGTH, 
        "Specify a length other than the logical extent of the exNode.", NULL},
  { "xndrc", 'x', POPT_ARG_STRING, &rcfile, RCFILE, 
        "You may specify an alternate source for the upload DepotList.", "<filename>"},
  { "demo",  'D', POPT_ARG_NONE, 0, LORS_ARG_DEMO, 
        "Add the messages understood by the LoRS View visualization.", NULL},
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
    memset(&drc, 0, sizeof(xndrc));
    memset(&xndrc, 0, sizeof(xndrc));
    parse_xndrc_files(&xndrc);

    g_lors_demo = xndrc.demo;

    while ( (v=poptGetNextOpt(optCon)) != -1 )
    {
        switch(v)
        {
            case DATA_BLOCKSIZE:
                if ( strtobytes(s_data_blocksize, &xndrc.data_blocksize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                /*fprintf(stderr,"blocksize = %lu\n",xndrc.data_blocksize);*/
                break;
            case MAX_BUFFSIZE:
                if ( strtobytes(s_max_buffersize, &xndrc.max_buffersize) != 0 )
                {
                    fprintf(stderr, "The blocksize specified is too large for internal datatype.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case LORS_BALANCE:
                opts |= LORS_BALANCE;
                /* TODO: run lorsSetStat */
                break;
            case MCOPY:
                mcopy_or_copy = 1;
                break;
            case DEPOT_LIST:
                depot_list =1;
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
            case LORS_VERSION:
            {
                const char *package;
                double version;
                lorsGetLibraryVersion(&package, &version);
                fprintf(stderr, "Version: %s %f\n", package, version);
                exit(EXIT_FAILURE);
                break;
            }
            case NEWFILENAME:
                break;
            case LORS_ARG_DEMO:
                g_lors_demo = (!g_lors_demo);
                break;
            case SAME_OUTPUTNAME:
                same_name = 1;
                break;
            default:
#if 0
                fprintf(stderr, "Unsupported option: %d\n", v);
#endif
            break;
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
        xndrc.depotList1 = drc.depotList1;
        xndrc.depotList2 = drc.depotList2;
    }

    g_db_level = xndrc.verbose;
    if ( v < -1 )
    {
        fprintf(stderr, "error: %d\n", v);
        fprintf(stderr, "%s: %s\n",
                 poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
                 poptStrerror(v));
    }


    if ( depot_list == 1 ){
         /*dp_list = xndrc.depotList; */
         dp_list1 = xndrc.depotList1;
         dp_list2 = xndrc.depotList2;
    }else {
         /*dp_list = NULL; */
         dp_list1 = NULL;
         dp_list2 = NULL;
    };

    if ( mcopy_or_copy == 1 ){
        opts = opts | LORS_MCOPY;
    }else{
        opts = opts | LORS_COPY;
    };

    while ( (filename = poptGetArg(optCon)) != NULL )
    {
        file_cnt++;
        if ( same_name == 1 )
        {
            int len;
            char *s;
            s = strrchr (filename, '/');
            if ( s == NULL ) s = filename;
            else s++;
            len = strlen(s) + 1; 
            output_filename = malloc(sizeof(char)*len);
            snprintf(output_filename, len, "%s", s);
        }
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
            lorsDebugPrint(D_LORS_VERBOSE,"%s %ld %ld %ld %d %s %d %s %d %d %d %ld %d %d\n",
                      filename, offset, length, xndrc.data_blocksize, xndrc.copies, 
                      lbone_server.host, lbone_server.port, xndrc.location, 
                      xndrc.maxdepots, xndrc.storage_type, xndrc.duration, 
                      xndrc.max_buffersize, xndrc.threads, xndrc.timeout);
            /*ret = lorsAugmentFile(&exnode, &new_exnode, filename, offset, length, 
                      xndrc.data_blocksize, xndrc.copies, &lbone_server, 
                      xndrc.depotList, xndrc.location, xndrc.maxdepots, 
                      xndrc.storage_type, xndrc.duration, 
                      xndrc.max_buffersize, xndrc.threads, xndrc.timeout, 0);*/ 
            ret = lorsAugmentFile(&exnode, &new_exnode, filename, offset, (longlong)length, 
                       xndrc.data_blocksize, xndrc.copies, fragments, lbs, 
                       dp_list1,dp_list2, xndrc.location, xndrc.maxdepots, 
                       xndrc.storage_type, xndrc.duration, 
                       xndrc.max_buffersize, xndrc.resolution_file, 
                       xndrc.threads, xndrc.timeout, opts); 
            if ( ret != LORS_SUCCESS && ret != LORS_PARTIAL)
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
                    if ( new_exnode != NULL){
                        if ( new_exnode->exnode != NULL ){
                            exnodeDestroyExnode(new_exnode->exnode);
                            new_exnode->exnode = NULL;
                        };
                        if ( new_exnode->mapping_map != NULL){
                            jrb_free_tree(new_exnode->mapping_map);
                            new_exnode->mapping_map = NULL;
                        };
                        free(new_exnode);
                        new_exnode = NULL;
                    };
                    if ( exnode != NULL ){
                        lorsExnodeFree(exnode);
                        exnode = NULL;
                    };
                    free_xndrc(&xndrc);
                    poptFreeContext(optCon);
                    exit(EXIT_FAILURE);
                }
            }
        } while ( ret != LORS_SUCCESS &&  ret!= LORS_PARTIAL);
        if ( new_xndfile != NULL )
        {
            /* TODO: copy filename from exnode to new_exnode */
            ExnodeMetadata *md, *md2;
            ExnodeValue    val;
            ExnodeType     type;
            int             lret = 0;
            lorsGetExnodeMetadata(exnode, &md);
            lret = exnodeGetMetadataValue(md, "filename", &val, &type);
            if ( lret == EXNODE_SUCCESS )
            {
                lorsGetExnodeMetadata(new_exnode, &md2);
                exnodeSetMetadataValue(md2, "filename", val, type, TRUE);
            } else {
                fprintf(stderr, "source exnode has no filename\n");
            }

            if ( (ret = lorsFileSerialize(new_exnode,new_xndfile,0,0)) != LORS_SUCCESS )
            {
                fprintf(stderr, "Serialize Failed.\n");
                if ( new_exnode != NULL){
                   if ( new_exnode->exnode != NULL ){
                       exnodeDestroyExnode(new_exnode->exnode);
                       new_exnode->exnode = NULL;
                   };
                   if ( new_exnode->mapping_map != NULL){
                       jrb_free_tree(new_exnode->mapping_map);
                       new_exnode->mapping_map = NULL;
                   };
                   free(new_exnode);
                   new_exnode = NULL;
                };
                continue;
            };
        }else
        {  /*   else     */
            ret = lorsFileSerialize(exnode, output_filename, 0, 0);
            if ( ret != LORS_SUCCESS )
            {
                 fprintf(stderr, "Serialize Failed.\n");
                if ( new_exnode != NULL){
                    if ( new_exnode->exnode != NULL ){
                       exnodeDestroyExnode(new_exnode->exnode);
                       new_exnode->exnode = NULL;
                    };
                    if ( new_exnode->mapping_map != NULL){
                        jrb_free_tree(new_exnode->mapping_map);
                        new_exnode->mapping_map = NULL;
                    };
                    free(new_exnode);
                    new_exnode = NULL;
                 };   
                 if (exnode != NULL){
                     lorsExnodeFree(exnode);
                     exnode = NULL;
                 };
                 continue;
           }
        }
      if ( exnode != NULL ){
          lorsExnodeFree(exnode);
          exnode = NULL;
      };
       if ( new_exnode != NULL){
           if ( new_exnode->exnode != NULL ){
                  exnodeDestroyExnode(new_exnode->exnode);
                  new_exnode->exnode = NULL;
           };
           if ( new_exnode->mapping_map != NULL){
                 jrb_free_tree(new_exnode->mapping_map);
                 new_exnode->mapping_map = NULL;
           };
           free(new_exnode);
           new_exnode = NULL;
      };
      if ( exnode != NULL ){
          lorsExnodeFree(exnode);
          exnode = NULL;
      };
    }

    
    free_xndrc(&xndrc);

    if ( file_cnt == 0 )
    {
        fprintf(stderr, "Error. You must also provide a filename on "
                "which lors_augment operates.\n");
        poptPrintHelp(optCon, stderr, 0);
        poptFreeContext(optCon);
        exit(EXIT_FAILURE);
    }

    poptFreeContext(optCon);
    

    return 0;
}
