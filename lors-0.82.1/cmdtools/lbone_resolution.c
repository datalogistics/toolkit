
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "lbone_client_lib.h"
#include <jrb.h>
#include <popt.h>
#include <lors_file.h>
#include <xndrc.h>
#include <lors_resolution.h>


Depot *CallLbone(LBONE_request        request,
                struct ibp_depot lboneDepot, char *lbonehost,
                XndRc xndrc, int use_soft);

int main(int argc, char **argv)
{
  int        i, j, count;
  int       ret = -1;
  LBONE_request        request;
  char       *temp = NULL, *lbonehost = NULL;
  const char       *source_depot;
  struct ibp_depot   lboneDepot = {"", 6767};
  Depot      lbone_server = NULL, *src_depots = NULL, *dst_depots = NULL;
  int       port = 6767;
  double    **resArray = NULL, metric = 0.0;
  Depot      srcList[2];
  LboneResolution *lr;
  JRB           sort_tree = NULL, sort_node = NULL;
  XndRc       xndrc;
  poptContext optCon;   /* context for parsing command-line options */
  int           mode= LBONE_RESOLUTION_GEO;
  int           try1 = 0;
  int           v = 0;
  int           use_lbone = 0, use_live=0, use_cache=0, use_list=0, 
                use_getcache=0, use_soft = 1, get_alldepots = 0;
  int cnt = 0;
  double max = 0, nmax = 0;
  double min = -1, nmin = 0;
  double    *proximity;
  int       first = 0;

  char          *cache_file = NULL, *list_file = NULL;

  /* lbone_resolution supports several different operations:
   * 1. Display the Resolution Arrows.
   *    Get data from cache or from network.
   *    From one depot to all others.  
   *        'Others' is specified by the given LBone Request parameters.
   * 2. output Resolution data suitable for the cache file.
   *
   *
   * Get DST depots from LBone or from File.
   *    -lbone or -list <file> ?
   * Get Resolution from LBone or from File.
   *    -live  or -cache <file>
   * Generate a Cache file.
   *    -getcache  (implies -lbone -live it does not make sense otherwise).
   */
#define RESOLUTION_LIVE         10
#define RESOLUTION_CACHE        11
#define RESOLUTION_LBONE        12
#define RESOLUTION_LIST         13
#define RESOLUTION_GETCACHE     14
#define RESOLUTION_GETALLDEPOTS 15

    struct poptOption optionsTable[] = {
  { "lbone-host", 'H', POPT_ARG_STRING, &lbonehost,         LBONESERVER, 
        "Specify an LBone Server for resource discover and proxmity resolution.", NULL},
  { "lbone-port", 'P', POPT_ARG_INT,    &lboneDepot.port, LBONEPORT, 
        "Specify the port number when using an LBone Server on a non standard port.", 
        "6767"},
  { "nws", 'n', POPT_ARG_NONE, 0,  LBONE_NWS, 
        "Return NWS metrics from the LBone.", NULL},
  { "geo", 'g', POPT_ARG_NONE, 0,  LBONE_GEO, 
        "Return Geographic metrics from the LBone.", NULL},
  { "d", 'd',   POPT_ARG_DOUBLE, &xndrc.duration_days, DURATION,
        "The duration in days argument as passed to the LBONE Query.", NULL},
  { "soft",       's', POPT_ARG_NONE,   0,                  STORAGE_TYPE_SOFT, 
        "Specify SOFT storage for storage allocations.", NULL},
  { "hard",       'h', POPT_ARG_NONE,   0,                  STORAGE_TYPE_HARD, 
        "Specify HARD storage for storage allocations.", NULL},
  { "maxdepots",  'm', POPT_ARG_INT,   &xndrc.maxdepots,          MAX_DEPOTS, 
        "Specify the maximum number of depots returned by the 'location' hint.", "6"},
  { "location",   'l', POPT_ARG_STRING, &xndrc.location,          LOCATION_HINT, 
        "Specify a location hint to pass the LBone Query.", "'x'"},
  { "lbone", 'b', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH, 0, RESOLUTION_LBONE,
        "Contact the lbone for the target DepotList", NULL},
  { "live", 'v', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH, 0,  RESOLUTION_LIVE,
        "Retrieve information from the LBone to guarantee the most uptodate data.  "
        "Excludes '-cache'.",NULL},
  { "list",  't', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH, &list_file, RESOLUTION_LIST,
        "Use the specified file as the target Depots.", NULL},
  { "cache", 'c', POPT_ARG_STRING|POPT_ARGFLAG_ONEDASH, &cache_file, RESOLUTION_CACHE,
        "Retrieve information from a Cache file to allow user defined metrics. "
        "Excludes '-live'.",NULL},
  { "getcache", 'g', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH, 0, RESOLUTION_GETCACHE,
        "A combination of -live and -lbone, to generate the cache file.", NULL},
  { "getdepotlist", 'a', POPT_ARG_NONE|POPT_ARGFLAG_ONEDASH, 0, RESOLUTION_GETALLDEPOTS,
        "Fetch and list all depots at a particular location.", NULL},
  { "version",    'v', POPT_ARG_NONE,   0,                  LORS_VERSION, 
        "Display Version information about this package.", NULL},
        POPT_AUTOHELP
        { NULL, 0, 0, NULL, 0 }
    };

    srcList[1] = NULL;
    parse_xndrc_files(&xndrc);
    optCon = poptGetContext(NULL, argc, (const char **)argv, optionsTable, 0);
    poptSetOtherOptionHelp(optCon, "<registered ibp_depot with nws_sensor>");

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
            case LBONE_GEO:
                mode = LBONE_RESOLUTION_GEO;
                break;
            case LBONE_NWS:
                mode = LBONE_RESOLUTION_NWS;
                break;
            case RESOLUTION_LBONE:
                use_lbone = 1;
                break;
            case RESOLUTION_LIVE:
                use_live = 1;
                break;
            case RESOLUTION_GETALLDEPOTS:
                get_alldepots=1;
                break;
            case RESOLUTION_GETCACHE:
                use_getcache = 1;
                use_live = 1;
                use_lbone = 1;
                break;
            case RESOLUTION_LIST:
                use_list = 1;
                break;
            case RESOLUTION_CACHE:
                use_cache = 1;
                break;
            case STORAGE_TYPE_HARD:
                use_soft = 0;
                break;
            case STORAGE_TYPE_SOFT:
                use_soft = 1;
                break;
            case LBONESERVER:
                strcpy(lboneDepot.host, lbonehost);
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
                break;
        }
    }

    /* set up IBP depot */

    /* one or many */
    /*get_src_depot(from cmdline or from lbone);*/
    if ( use_getcache )
    {
        src_depots = CallLbone(request, lboneDepot, lbonehost, xndrc, use_soft) ;
    } else 
    {
        source_depot= poptGetArg(optCon);
        if ( source_depot == NULL )
        {
            fprintf(stderr, "specify depotname\n");
            exit(EXIT_FAILURE);
        }
        srcList[0] = lors_strtodepot(source_depot);
        srcList[1] = NULL;
        src_depots = srcList;
    }
    if ( get_alldepots )
    {
        for (i=0; src_depots[i] != NULL; i++)
        {
            fprintf(stdout, "%s %d\n", src_depots[i]->host, src_depots[i]->port);
        }
        return 0;
    }

    /* often many, possibly as dictated by the lbonerequest */
    /*get_destination_depots(fromfile, from lbone,);*/
    if ( use_lbone )
    {
        dst_depots = CallLbone(request, lboneDepot, lbonehost, xndrc, use_soft) ;
    } else if ( use_list )
    {
        fprintf(stderr, "Error: unsupported.\n");
        exit(EXIT_FAILURE);
    } else if ( use_cache )
    {
        if ( lorsCreateDepotListFromFile(cache_file, &dst_depots, 1) != LORS_SUCCESS )
        {
            fprintf(stderr, "Could not open file 'cache_file' or bad parameters.\n");
            exit(EXIT_FAILURE);
        }
    } else 
    {
        fprintf(stderr, "???\n");
        exit(EXIT_FAILURE);
    }
    
    /* from depot-2depotResolution or from cache file.*/
    if ( use_live )
    {
        /* read resolution from lbone */
        fprintf(stderr, "use Live.\n");
        try1=0;
        if ( lbonehost == NULL )
        {
            lbone_server = xndrc.lboneList[try1];
            if ( lbone_server == NULL )
            {
                fprintf(stderr, "Exhaused all available LBone Depots.\n");
                exit(EXIT_FAILURE);
            }
            try1++;
        } else 
        {
            lbone_server = &lboneDepot;
            try1=0;
        }
        do 
        {
            fprintf(stderr, "Trying: %s:%d\n", lbone_server->host, lbone_server->port);
            ret = lorsCreateResolutionFromLists(&lr, lbone_server,
                    src_depots, dst_depots, 0, 60, mode);
            if ( ret != LORS_SUCCESS )
            {
                lbone_server = xndrc.lboneList[try1];
                if ( lbone_server == NULL )
                {
                    fprintf(stderr, "Exhaused all available LBone Depots.\n");
                    exit(EXIT_FAILURE);
                }
                try1++;
            }
        } while ( ret != LORS_SUCCESS );
        metric = 0;

        /* now for the dest */
        ret = lbone_getProximity1(lbone_server, xndrc.location, dst_depots, 
                            &proximity, 60);
        if ( ret != 0 )
        {
            fprintf(stderr, "getProximity1 error.\n");
            exit(EXIT_FAILURE);
        }
        for (i=0; dst_depots[i] != NULL; i++ )
        {
            metric = proximity[i];

            /*fprintf(stderr, "%s:%d %f\n", 
               dst_depots[i]->host, dst_depots[i]->port, metric);*/
            if ( lorsSetResolutionPoint(lr, NULL, dst_depots[i], metric) != LORS_SUCCESS )
            {
                fprintf(stderr, "suck\n");
                exit(EXIT_FAILURE);
            }
            metric = 0;
            /*lorsGetResolutionPoint(lr, NULL, dst_depots[i], &metric);*/
            /*fprintf(stderr, "metric is now: %f\n", metric);*/
        }
        /* now for the sources */
        ret = lbone_getProximity1(lbone_server, xndrc.location, src_depots, 
                            &proximity, 60);
        if ( ret != 0 )
        {
            fprintf(stderr, "getProximity1 error.\n");
            exit(EXIT_FAILURE);
        }
        for (i=0; src_depots[i] != NULL; i++ )
        {
            metric = proximity[i];

            /*fprintf(stderr, "%s:%d %f\n", 
               src_depots[i]->host, src_depots[i]->port, metric);*/
            if ( lorsSetResolutionPoint(lr, src_depots[i], NULL, metric) != LORS_SUCCESS )
            {
                fprintf(stderr, "suck\n");
                exit(EXIT_FAILURE);
            }
            metric = 0;
            /*lorsGetResolutionPoint(lr, src_depots[i], NULL, &metric);*/
            /*fprintf(stderr, "metric is now: %f\n", metric);*/
        }

    } else if ( use_cache )
    {
        /* read resolution from file */
        if ( lorsCreateResolutionFromFile(cache_file, &lr, 0) != LORS_SUCCESS )
        {
            fprintf(stderr, "failed to read cache file: %s\n", cache_file);
            exit(EXIT_FAILURE);
        }
    } else 
    {
        fprintf(stderr, "Unsupported: \n");
        exit(EXIT_FAILURE);
        
    }


    if ( use_getcache )
    {
        if ( mode & LBONE_RESOLUTION_GEO )
        {
            lorsNormalizeResolution(lr);
        }
        /*print cache file*/
        for(i=0; dst_depots[i] != NULL; i++)
        {
            lorsGetResolutionPoint(lr, NULL, dst_depots[i], &metric);
            fprintf(stdout, "localhost %s:%d %f\n", 
                    dst_depots[i]->host, dst_depots[i]->port, metric);
        }
        for(i=0; dst_depots[i] != NULL; i++)
        {
            lorsGetResolutionPoint(lr, src_depots[i], NULL, &metric);
            fprintf(stdout, "%s:%d localhost %f\n", 
                    dst_depots[i]->host, dst_depots[i]->port, metric);
        }

        for(i=0; src_depots[i] != NULL; i++)
        {
            for(j=0; dst_depots[j] != NULL; j++)
            {
                if ( strcmp(dst_depots[j]->host, src_depots[i]->host) != 0 )
                {
                    lorsGetResolutionPoint(lr, src_depots[i], dst_depots[j], &metric);
                    fprintf(stdout, "%s:%d %s:%d %.4f\n",
                        src_depots[i]->host, src_depots[i]->port,
                        dst_depots[j]->host, dst_depots[j]->port,
                        metric);
                }
            }
        }
    } else 
    {
        /* print arrows */
 
        /*fprintf(stderr, "From %d: %s\n", 0, srcList[0]->host);*/
        lorsResolutionMax(lr, &max);
        lorsResolutionMin(lr, &min);
 
        if ( mode & LBONE_RESOLUTION_GEO  && !use_cache)
        {
            lorsNormalizeResolution(lr);
        }
        /* find min and max of the subset */
        lorsResolutionMax(lr, &nmax);
        lorsResolutionMin(lr, &nmin);

        fprintf(stderr, "CLEAR\n");
        fprintf(stderr, "TITLE LBone Resolution measurements using %s metrics: from %.1f to %.1f %s\n",
                (mode == LBONE_RESOLUTION_NWS ? "NWS" : "Geographic"), 
                min, max,
                (mode == LBONE_RESOLUTION_NWS ? "Mbps" : "Miles"));
        fprintf(stderr, "NWS SCALE %f %f\n", nmin, nmax);
        for(i=0; src_depots[i] != NULL;i++)
        {
            for(j=0; dst_depots[j] != NULL;j++)
            {
                lorsGetResolutionPoint(lr, src_depots[i], dst_depots[j], &metric);

                /*fprintf(stdout, "METRIC: %f %f %f\n", min, max, metric);*/
                if ( metric > 0 )
                {
                    fprintf(stderr, "NWS Arrow %s:%d to %s:%d %f Mbps\n",
                        src_depots[i]->host, src_depots[i]->port, 
                        dst_depots[j]->host, dst_depots[j]->port, metric);

                }
            }
        }
        printf("\n");
    }
 
    /*free(lbone_server);*/

    return 0;
}


Depot *CallLbone(LBONE_request        request,
                struct ibp_depot lboneDepot, char *lbonehost,
                XndRc xndrc, int use_soft)
{
    Depot *src_depots;
    Depot lbone_server;

    int try1;
    memset(&request, 0, sizeof(struct lbone_request));
    request.numDepots = xndrc.maxdepots;
    if ( use_soft ) 
    {
        request.stableSize = 0;
        request.volatileSize = 1;
    } else 
    {
        request.stableSize = 1;
        request.volatileSize = 0;
    }
    request.duration = xndrc.duration_days;
    request.location = xndrc.location;
    /*fprintf(stderr, "LBone Request:\n");
    fprintf(stderr, "  numDepots    = %d\n", request.numDepots);
    fprintf(stderr, "  stableSize   = %lu\n", request.stableSize);
    fprintf(stderr, "  volatileSize = %lu\n", request.volatileSize);
    fprintf(stderr, "  duration     = %f\n", request.duration);
    fprintf(stderr, "  location     = %s\n\n", request.location); */

    try1=0;
    if ( lbonehost == NULL )
    {
        lbone_server = xndrc.lboneList[try1];
        try1++;
    } else 
    {
        lbone_server = &lboneDepot;
        try1=0;
    }
    do 
    {
        /* GET DEPOTS */
        src_depots = lbone_getDepots(lbone_server, request, 60);
        if ( src_depots == NULL ) 
        {
            lbone_server = xndrc.lboneList[try1];
            try1++;
        }
    } while ( src_depots == NULL && try1 < 4 );

    return src_depots;
}

