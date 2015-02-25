#define IBP_REENTRANT
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _MINGW
#include <sys/mman.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <xndrc.h>
#include <lors_file_opts.h>

struct ibp_depot default_lbone_list [] = 
{
    { "acre.sinrg.cs.utk.edu", 6767},
    { "vertex.cs.utk.edu",    6767},
    { "galapagos.cs.utk.edu", 6767},
    { "adder.cs.utk.edu",     6767},
    { "",                     0}
    #define LBONE_CNT 4
};

struct ibp_depot default_depot_list [] = 
{
    { "dsj.sinrg.cs.utk.edu", 6714},
    { "pl2.cs.utk.edu",       6714},
    { "adder.cs.utk.edu",     6714},
    { "",                     0}
    #define DEPOT_CNT 3
};

   
int set_xndrc_defaults(XndRc *xndrc)
{

    int i;

    memset(xndrc,0,sizeof(XndRc));
    xndrc->lboneList = (IBP_depot *)malloc(sizeof(IBP_depot)*(LBONE_CNT+1));
    for (i=0; default_lbone_list[i].port != 0; i++)
    {
        xndrc->lboneList[i] = new_ibp_depot(default_lbone_list[i].host, 
                                            default_lbone_list[i].port);
    }
    xndrc->lboneList[i] = NULL;

    xndrc->depotList = (IBP_depot *)malloc(sizeof(IBP_depot)*(DEPOT_CNT+1));
    for (i=0; default_depot_list[i].port != 0; i++)
    {
        xndrc->depotList[i] = new_ibp_depot(default_depot_list[i].host,
                                            default_depot_list[i].port);
    }
    xndrc->depotList[i] = NULL;
   /******temp*****/
    xndrc->depotList1 = NULL; 
    /*(IBP_depot *)malloc(sizeof(IBP_depot)*(DEPOT_CNT+1));
    xndrc->depotList1 = NULL;
    (IBP_depot *)malloc(sizeof(IBP_depot)*(DEPOT_CNT+1));
    for (i=0; default_depot_list[i].port != 0; i++)
    {
        xndrc->depotList1[i] = new_ibp_depot(default_depot_list[i].host,
                                            default_depot_list[i].port);
    }
    xndrc->depotList1[i] = NULL;
    */

    xndrc->depotList2 = NULL; /*(IBP_depot *)malloc(sizeof(IBP_depot)*(DEPOT_CNT+1));
    for (i=0; default_depot_list[i].port != 0; i++)
    {
        xndrc->depotList2[i] = new_ibp_depot(default_depot_list[i].host,
                                            default_depot_list[i].port);
    }
    xndrc->depotList2[i] = NULL;    
    */

   /******till here ****/

    xndrc->location    =     NULL;
    xndrc->duration_days = (double)XND_DEFAULT_DURATION_DAYS;
    xndrc->storage_type =  (int)XND_DEFAULT_STORAGE_TYPE;
    xndrc->verbose =       (int)XND_DEFAULT_VERBOSE;
    xndrc->max_buffersize =        XND_DEFAULT_INTERNAL_BUFFERSIZE;
    xndrc->data_blocksize =     XND_DEFAULT_DATA_BLOCKSIZE;
    xndrc->e2e_blocksize =     XND_DEFAULT_E2E_BLOCKSIZE;
    xndrc->e2e_order[0]     =  DES_ENCRYPTION;
    xndrc->e2e_order[1]     =  ADD_CHECKSUM;
    xndrc->e2e_order[2]     =  -1;
    xndrc->fragments_per_file = XND_DEFAULT_FRAGMENTS_PER_FILE;
    xndrc->copies =             XND_DEFAULT_COPIES;
    xndrc->threads =            XND_DEFAULT_THREADS;
    xndrc->maxdepots =          XND_DEFAULT_MAXDEPOTS;
    xndrc->duration = (int) ( xndrc->duration_days * 24*60*60 ); 
    xndrc->resolution_file =(char *) strdup(XND_DEFAULT_RESOLUTION_FILE);
    xndrc->timeout =            XND_DEFAULT_TIMEOUT;

    return 0;
}



/*void read_xndrc_file(XndRc *xndrc, IS is)*/
int read_xndrc_file(XndRc *xndrc, char *filename)
{
    int         i                       = 0, ret=0;
    char        temp_str[1024];
    int         temp_port               = 0;
    double      temp_days               = 0;
    int         temp_verbose            = 0;
    ulong_t     temp_size               = 0;
    int         port                    = 0;
    int         temp_fragments          = 0;
    int         fragments_set           = 0;
    int         temp_copies             = 0;
    int         temp_threads            = 0;

    Dllist      depot_list = NULL, depot_node = NULL,depot_list1 = NULL,depot_list2=NULL;
    int         depot_cnt               = 0;
    int         depot1_cnt =0,depot2_cnt = 0;
    Dllist      lbone_list = NULL, 
                dl_node = NULL;
    int         lbone_cnt               = 0;

    IBP_depot   dp;
    IS          is;


    is = new_inputstruct(filename);
    if (is == NULL) { return -1; }

    while (get_line(is) >= 0)
    {
        if ( is->NF == 0 ) continue;
        if (is->fields[0][0] == '#')  /* ignore comment lines */
            continue;

        else if (strcmp(is->fields[0], "LBONE_SERVER") == 0) 
        {
            /* add to collection of lbone servers */
            if (is->NF != 3)
                continue;
            if (lbone_list == NULL ) lbone_list = new_dllist(); 

            port = atoi(is->fields[2]);
            dp = new_ibp_depot(is->fields[1], port);
            dll_append(lbone_list, new_jval_v(dp));
            lbone_cnt++;
            continue;
        }

        else if (strcmp(is->fields[0], "LOCATION") == 0) 
        {
            if (is->NF == 1)   /* ignore if keyword has no value */
                continue;
            memset(temp_str, 0, 1024);
            strcat(temp_str, is->fields[1]);
            for (i = 2; i < is->NF; i++)
            {
                if ((strlen(temp_str) + strlen(is->fields[i]) + 1) < 1024)
                {
                    strcat(temp_str, " ");
                    strcat(temp_str, is->fields[i]);
                }
            }
            if (xndrc->location != NULL)
                free(xndrc->location);
            xndrc->location = (char *) strdup(temp_str);
            continue;
        }
        else if (strcmp(is->fields[0], "DURATION") == 0) 
        {
            if ( is->NF != 2 )
                continue;
            ret = strtotimeout(is->fields[1], &xndrc->duration);
            if ( ret != 0 )
            {
                fprintf(stderr, "Could not convert DURATION. "
                        "Value too large for internal data type.\n");
                xndrc->duration = 24*60*60;
            }
        }

        else if (strcmp(is->fields[0], "DURATION_DAYS") == 0) 
        {
            if (is->NF != 2)
                continue;
            temp_days = atof(is->fields[1]);
            if (temp_days <= 0) 
                continue;
            else xndrc->duration_days = temp_days;
            xndrc->duration = (int)(xndrc->duration_days * 60 * 60 * 24 );
            continue;
        }

        else if (strcmp(is->fields[0], "RESOLUTION_FILE") == 0) 
        {
            if (is->NF != 2)
                continue;
            if (xndrc->resolution_file != NULL)
                free(xndrc->resolution_file);
            xndrc->resolution_file = (char *) strdup(is->fields[1]);
            continue;
        }

        else if (strcmp(is->fields[0], "STORAGE_TYPE") == 0) 
        {
            if (is->NF != 2)
                continue;
            if (strcasecmp(is->fields[1], "SOFT") == 0)
                xndrc->storage_type = IBP_SOFT;
            else
                xndrc->storage_type = IBP_HARD;
            continue;
        }

        else if (strcmp(is->fields[0], "VERBOSE") == 0) 
        {
            if (is->NF != 2)
                continue;
            temp_verbose = atoi(is->fields[1]);
            xndrc->verbose = temp_verbose;
            continue;
        }
        else if (strcmp(is->fields[0], "DEMO") == 0) 
        {
            if (is->NF != 2)
                continue;
            temp_verbose = atoi(is->fields[1]);
            xndrc->demo = temp_verbose;
            continue;
        }
        else if (strcmp(is->fields[0], "E2E_BLOCKSIZE") == 0) 
        {
            if (is->NF != 2)
                continue;
            ret = strtobytes(is->fields[1], &temp_size);
            if ( ret != 0 ) 
            {
                fprintf(stderr, "E2E_BLOCKSIZE conversion error. Using default.\n");
                temp_size = XND_DEFAULT_E2E_BLOCKSIZE;
            }
            xndrc->e2e_blocksize = temp_size;
            continue;
        }
        else if (strcmp(is->fields[0], "E2E_ORDER") == 0) 
        {
            int e2e_cnt = 0;
            /* each function is separated by a space, so fields can parse it*/
            if ( is->NF < 2 )
                continue;
            for(i=1; i < is->NF; i++, e2e_cnt++)
            {
                if ( strcasecmp(is->fields[i], "checksum") == 0 ) 
                {
                    xndrc->e2e_order[e2e_cnt] = ADD_CHECKSUM;
                } else 
                if ( strcasecmp(is->fields[i], "aes_encrypt") == 0 ) 
                {
                    xndrc->e2e_order[e2e_cnt] = AES_ENCRYPTION;
                } else 
                if ( strcasecmp(is->fields[i], "des_encrypt") == 0 ) 
                {
                    xndrc->e2e_order[e2e_cnt] = DES_ENCRYPTION;
                } else 
                if ( strcasecmp(is->fields[i], "xor_encrypt") == 0 ) 
                {
                    xndrc->e2e_order[e2e_cnt] = XOR_ENCRYPTION;
                } else 
                if ( strcasecmp(is->fields[i], "zlib_compress") == 0 ) 
                {
                    xndrc->e2e_order[e2e_cnt] = ZLIB_COMPRESS;
                } else 
                if ( strcasecmp(is->fields[i], "none") == 0 )
                {
                    xndrc->e2e_order[0] = -1;
                    break;
                } else
                {
                    fprintf(stderr, "Unrecognized EndtoEnd Condition: %s\n", 
                            is->fields[i]);
                    e2e_cnt--;
                }
            }
            xndrc->e2e_order[e2e_cnt] = -1;
        }
        else if (strcmp(is->fields[0], "DATA_BLOCKSIZE") == 0) 
        {
            if (is->NF != 2)
                continue;
            ret = strtobytes(is->fields[1], &temp_size);
            if ( ret != 0 ) 
            {
                fprintf(stderr, "DATA_BLOCKSIZE conversion error. Using default.\n");
                temp_size = XND_DEFAULT_DATA_BLOCKSIZE;
            }
            xndrc->data_blocksize = temp_size;
            continue;
        }

#if 0
        else if (strcmp(is->fields[0], "MULTITHREADED_BUFFER_SIZE") == 0) 
        {
            if (is->NF != 3)
                continue;
            temp_size = atoi(is->fields[1]);
            if (strcmp(is->fields[2], "M") == 0)
                temp_size *= (1024 * 1024);
            else if (strcmp(is->fields[2], "K") == 0)
                temp_size *= 1024;
            else continue;

            if ((temp_size >= 1024) && 
                (temp_size <= (160 * 1024 * 1024)))
                xndrc->mbuffer_size = temp_size;
            continue;
        }
#endif

        else if (strcmp(is->fields[0], "FRAGMENTS_PER_FILE") == 0) 
        {
            if (is->NF != 2)
                continue;
            temp_fragments = atoi(is->fields[1]);
            if (temp_fragments < 1)
                continue;
            xndrc->fragments_per_file = temp_fragments;
            fragments_set = 1;
            continue;
        }

        else if (strcmp(is->fields[0], "COPIES") == 0) 
        {
            if (is->NF != 2)
                continue;
            temp_copies = atoi(is->fields[1]);
            if ((temp_copies < 1) || (temp_copies > 1000))
                continue;
            xndrc->copies = temp_copies;
            continue;
        }

        else if (strcmp(is->fields[0], "THREADS") == 0) {
            if (is->NF != 2)
                continue;
            temp_threads = atoi(is->fields[1]);
            xndrc->threads = temp_threads;
            continue;
        }
        else if (strcmp(is->fields[0], "MAXDEPOTS") == 0) {
            if (is->NF != 2)
                continue;
            temp_size = atoi(is->fields[1]);
            xndrc->maxdepots = temp_size ;
            continue;
        }
        else if (strcmp(is->fields[0], "MAX_INTERNAL_BUFFER") == 0) {
            if (is->NF != 2)
                continue;
            ret = strtobytes(is->fields[1], &temp_size);
            if ( ret != 0 ) 
            {
                fprintf(stderr, "MAX_INTERNAL_BUFFER conversion error. Using default.\n");
                temp_size = XND_DEFAULT_INTERNAL_BUFFERSIZE;
            }
            xndrc->max_buffersize = temp_size ;
            continue;
        }
        else if (strcmp(is->fields[0], "TIMEOUT") == 0) {
            if (is->NF != 2)
                continue;
            if ( strtotimeout(is->fields[1], &temp_size) == -1 )
            {
                fprintf(stderr, "TIMEOUT value too large for internal data type. "
                        "Using default\n");
                temp_size = XND_DEFAULT_TIMEOUT;
            }
            xndrc->timeout = temp_size ;
            continue;
        }

#if 0
        else if (strcmp(is->fields[0], "UPLOAD_MODE") == 0) {
            if (is->NF != 2)
                continue;
        if (strcasecmp(is->fields[1], "UPLOAD")==0){
          xndrc->upld_mode = XNDUPLD_MOD_UPLOAD;
        }else if (strcasecmp(is->fields[1], "COPY")==0){
          xndrc->upld_mode = XNDUPLD_MOD_COPY;
        }else if (strcasecmp(is->fields[1], "MCOPY")==0){
          xndrc->upld_mode = XNDUPLD_MOD_MCOPY;
        }
            continue;
        }
#endif

        else if (strcmp(is->fields[0], "DEPOT") == 0) 
        {
              int port;
            IBP_depot dp;
            if (is->NF != 3)
                continue;
            if ( depot_list == NULL ) depot_list = new_dllist();

            port = atoi(is->fields[2]);
            dp = new_ibp_depot(is->fields[1], port);
            dll_append(depot_list, new_jval_v(dp));
            depot_cnt ++;
            continue;
        }
        else if(strcmp(is->fields[0],"ROUTE_DEPOT") == 0)
        {
             int port;
             IBP_depot dp;
             if(is->NF != 3)
                continue;
             if( depot_list1 == NULL ) depot_list1 = new_dllist();
              
              port = atoi(is->fields[2]);
              dp=new_ibp_depot(is->fields[1],port);
              dll_append(depot_list1,new_jval_v(dp));
              depot1_cnt ++;
              continue;
        }
        else if(strcmp(is->fields[0],"TARGET_DEPOT") == 0)
        {
             int port;
             IBP_depot dp;
             if(is->NF != 3)
                continue;
             if( depot_list2 == NULL ) depot_list2 = new_dllist();

              port = atoi(is->fields[2]);
              dp=new_ibp_depot(is->fields[1],port);
              dll_append(depot_list2,new_jval_v(dp));
              depot2_cnt ++;
              continue;
        }

    }

    /* translate the depot list */
    if ( lbone_list != NULL )
    {
        if ( xndrc->lboneList != NULL ) {
            for(i=0; xndrc->lboneList[i] != NULL; i++) {
                free(xndrc->lboneList[i]);
            }
            free(xndrc->lboneList);
        }
        xndrc->lboneList = (IBP_depot *)malloc(sizeof(IBP_depot)*(lbone_cnt+1));
        i = 0;
        dll_traverse(dl_node, lbone_list)
        {
            xndrc->lboneList[i] = (IBP_depot)dl_node->val.v;
            i++;
        }
        xndrc->lboneList[i] = NULL;
        free_dllist(lbone_list);
    }

    if ( depot_list != NULL )
    {
        if ( xndrc->depotList != NULL ) {
            for(i=0; xndrc->depotList[i] != NULL; i++) {
                free(xndrc->depotList[i]);
            }
            free(xndrc->depotList);
        }
        xndrc->depotList = (IBP_depot *)malloc(sizeof(IBP_depot)*(depot_cnt+1));
        i = 0;
        dll_traverse(depot_node, depot_list)
        {
            xndrc->depotList[i]=(IBP_depot)(dll_val(depot_node).v);
            i++;
        }
        xndrc->depotList[i] = NULL;
        free_dllist(depot_list);
    }
   /********temp***********/ 
    if( depot_list1 != NULL )
    {
        if ( xndrc->depotList1 != NULL ) free(xndrc->depotList1);
        xndrc->depotList1 = (IBP_depot *)malloc(sizeof(IBP_depot)*(depot1_cnt+1));
        
#if 0
        if(depot_list2 == NULL)
        { 
           if ( xndrc->depotList2 != NULL ) free(xndrc->depotList2);
           xndrc->depotList2 = (IBP_depot *)malloc(sizeof(IBP_depot)*(depot1_cnt+1)); 
           i = 0;
           dll_traverse(depot_node, depot_list1)
           {
              xndrc->depotList1[i]=(IBP_depot)(dll_val(depot_node).v);
              xndrc->depotList2[i]=(IBP_depot)(dll_val(depot_node).v);
              i++;
           }
            xndrc->depotList1[i] = NULL;
            xndrc->depotList2[i] = NULL;
        }
        else
#endif
        {
          i = 0;
          dll_traverse(depot_node, depot_list1)
          {
             xndrc->depotList1[i]=(IBP_depot)(dll_val(depot_node).v);
             i++;
          }
          xndrc->depotList1[i] = NULL;
        }
        free_dllist(depot_list1);
    }
    
    if( depot_list2 != NULL )
    {
        if ( xndrc->depotList2 != NULL ) free(xndrc->depotList2);
        xndrc->depotList2 = (IBP_depot *)malloc(sizeof(IBP_depot)*(depot2_cnt+1));
#if 0
        if(depot_list1 == NULL)
        {
           printf("if depotslist1 is NULL\n");
           if ( xndrc->depotList1 != NULL ) free(xndrc->depotList1);
           xndrc->depotList1 = (IBP_depot *)malloc(sizeof(IBP_depot)*(depot2_cnt+1));
           i = 0;
           dll_traverse(depot_node, depot_list2)
           {
              xndrc->depotList1[i]=(IBP_depot)(dll_val(depot_node).v);
              xndrc->depotList2[i]=(IBP_depot)(dll_val(depot_node).v);
              i++;
           }
            xndrc->depotList1[i] = NULL;
            xndrc->depotList2[i] = NULL;
        }
        else
#endif
        {   
          i = 0;
          dll_traverse(depot_node, depot_list2)
          {
             xndrc->depotList2[i]=(IBP_depot)(dll_val(depot_node).v);
             i++;
          }
          xndrc->depotList2[i] = NULL;
        }
        free_dllist(depot_list2);
    }
    
    /**********till here ********/
    jettison_inputstruct(is);
    return 0;
}


void free_xndrc(XndRc *xndrc)
{
    int i;
  if (xndrc->location != NULL) {free( xndrc->location); };
  if (xndrc->resolution_file!=NULL) { free(xndrc->resolution_file); }
  if (xndrc->depotList!=NULL)
  {
    for (i=0; xndrc->depotList[i]!=NULL; i++)
    {
        free (xndrc->depotList[i]);
    }
    free (xndrc->depotList);
  }
  if (xndrc->lboneList!=NULL){
    for (i=0; xndrc->lboneList[i]!=NULL; i++)
    {
        free (xndrc->lboneList[i]);
    }
    free (xndrc->lboneList);
  }
  /**************temp**************/
  if (xndrc->depotList1!=NULL)
  {
    for (i=0; xndrc->depotList1[i]!=NULL; i++)
    {
        free (xndrc->depotList1[i]);
    }
    free (xndrc->depotList1);
  }
    if (xndrc->depotList2!=NULL)
  {
    for (i=0; xndrc->depotList2[i]!=NULL; i++)
    {
        free (xndrc->depotList2[i]);
    }
    free (xndrc->depotList2);
  }
  /************till here *********/
  /*free( xndrc );*/
  
  return;
}


/**
 * The xndrc file automates the process of retrieving default values for the
 * various arguments to the @ref xnd_node_api. 
 * This command looks first in the user's $HOME directory for .xndrc.  If this
 * file is present, it is opened and parsed for directives.  If not it
 * continues.
 * Next, the command attempts to open .xndrc in the $PWD. and scans the file
 * for directives.
 * The directives take precidence in the order scanned.
 * All command line processing of the application should be handled by the
 * programmer.
 *
 * @brief Parse user defined config files.
 * @return XndRc *struct.
 *
 * @ingroup xnd_seg_api
 * 
 */

int parse_xndrc_files(XndRc *xndrc)
{
    char        *home                   = NULL;
    char        temp_str[1024];
    IS          is;

    /*xndrc = (XndRc *) malloc(sizeof(struct xndrc_info));*/
    /*memset(xndrc, 0, sizeof(struct xndrc_info));*/
    set_xndrc_defaults(xndrc);

    /*
     *  Look for .xndrc file in the home directory
     */

    home = getenv("HOME");
    if (home != NULL)
    {
        memset(temp_str, 0, 1024);
        strcpy(temp_str, home);
        if (temp_str[strlen(temp_str) - 1] != '/')
            strcat(temp_str, "/");
        strcat(temp_str, ".xndrc");

        read_xndrc_file(xndrc, temp_str);
    }

    /*
     *  Now, look for .xndrc file in the current directory
     */
    read_xndrc_file(xndrc, ".xndrc");

    /*
     *  Lastly, parse the command line options
     */

    return 0;
}


