
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "fields.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"

#include "LoDNLogging.h"
#include "LoDNSerializedWarmer.h"
#include "LoDNAuxillaryFunctions.h"


LoDNSerializedWarmer *makeLoDNSerializedWarmer(char *filename)
{
    /* Declarations */   
    LoDNSerializedWarmer *warmer = NULL;
    
   
    if(filename == NULL)
    {
        return NULL;   
    }
    
    if((warmer = (LoDNSerializedWarmer*)malloc(sizeof(LoDNSerializedWarmer))) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");
    }
    
    if((warmer->name = strdup(filename)) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
    }
   
    if((warmer->sites = make_jrb()) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error"); 
    }
    
    /* Sets the time of the read */
    warmer->mtime = 0;
    
    return warmer;
}



static void _freeLoDNSerializedWarmer(LoDNSerializedWarmer *warmer)
{
	/* Declarations */
	JRB   siteNode;
	JRB   depotNode;
	Site *currSite;
	    
		    
    if(warmer->sites != NULL)
    {
    	jrb_traverse(siteNode, warmer->sites)
    	{
    		currSite = (Site*)siteNode->val.v;
          
    		if(currSite != NULL)
    		{
    			if(currSite->depots != NULL)
    			{
    				jrb_traverse(depotNode, currSite->depots)
    				{
    					if(depotNode->key.s != NULL)
    					{
    						free(depotNode->key.s);
    					}
    				}
                  
    				jrb_free_tree(currSite->depots);
    			}
              
    			if(currSite->name != NULL)
    			{
    				free(currSite->name);
    			}
              
    			free(currSite);
    		}
    	}
      
    	jrb_free_tree(warmer->sites);
    }
   
    
    if(warmer->name != NULL)
    {
    	free(warmer->name);
    }
}



void freeLoDNSerializedWarmer(LoDNSerializedWarmer *warmer)
{
     
    if(warmer != NULL)
    {
    	_freeLoDNSerializedWarmer(warmer);

        free(warmer);   
    }
}



int LoDNSerializedWarmerReadFile(LoDNSerializedWarmer *warmer)
{
    /* Declarations*/
    IS    is = NULL;
    char *siteName = NULL;
    char *depotName = NULL;
    char *delimiter = NULL;
    int   port;
    JRB    siteNode;
    Site  *currSite = NULL;
    struct stat sb;
    
    
    /* Gets the statbuf for the warmer file */
    if(stat(warmer->name, &sb) != 0)
    {
    	logPrintError(0, __FILE__, __LINE__, 0, "Error stating %s\n", warmer->name);
    	return -1;
    }
    
    /* Opens a fields inputstruct for reading the input from depot file */
    if((is = new_inputstruct(warmer->name)) == NULL)
    {
        logPrintError(0, __FILE__, __LINE__, 0, "Error opening %s\n", warmer->name);
        goto ERROR_HANDLER;
    }
    
    /* Traverses every line in the file skipping blank lines and lines that start with '#'*/
    while(get_line(is) > -1)
    {
        if(is->NF > 0 && is->fields[0][0] != '#')
        {
            /* Gets the : delimiter */
            delimiter = strchr(is->fields[0], ':');
                      
            /* If the number of fields exceed one then report an error */
            if(is->NF > 1 || delimiter == NULL || delimiter == is->fields[0])
            {
                logPrintError(0, __FILE__, __LINE__, 0, "Error parsing file %s on line %d\n",
                              warmer->name, is->line);
                goto ERROR_HANDLER;
            }
            
            /* Site line */
            if(*(delimiter+1) == '\0')
            {
                if((strlen(is->fields[0])-1) == 0)
                {
                    logPrintError(0, __FILE__, __LINE__, 0, "Error parsing file %s on line %d no site specified\n",
                                  warmer->name, is->line);
                    goto ERROR_HANDLER;
                }
                
                /* Allocates a copy of the depot id */
                if((siteName = strndup(is->fields[0], strlen(is->fields[0])-1)) == NULL)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
                }
                
                /* Allocates the site struct for holding the depots and blocks */
                if((siteNode = jrb_find_str(warmer->sites, siteName)) == NULL)
                {
                    /* Allocates the site */
                    if((currSite = (Site*)malloc(sizeof(Site))) == NULL)
                    {
                        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
                    }
                    
                    /* Allocates a jrb tree for holding the depots */
                    if((currSite->depots = make_jrb()) == NULL)
                    {
                        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
                    }
                    
                    /* Assigns the name */
                    currSite->name = siteName;
                    
                    /* Inserts the site struct by its name */
                    if((siteNode = jrb_insert_str(warmer->sites, currSite->name, new_jval_v(currSite))) == NULL)
                    {
                        logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
                    }
                }else
                {
                    free(siteName);   
                }
                
                /* Gets a pointer to the actual tree held in the node */
                currSite = (Site*)siteNode->val.v;
                
            
            /* Depot line*/
            }else
            {
                /* Checks that there is a site */
                if(currSite == NULL)
                {
                    logPrintError(0, __FILE__, __LINE__, 0, "Error parsing file %s on line %d no site specified\n",
                                  warmer->name, is->line);
                    goto ERROR_HANDLER;
                }
              
                /* Checks that the port is valid */
                if(sscanf(delimiter+1, "%d", &port) != 1 || port < 0 || port > 65535)
                {
                    logPrintError(0, __FILE__, __LINE__, 0, "Error parsing file %s on line %d due to invalid port in %s\n",
                                    warmer->name, is->line);
                    goto ERROR_HANDLER;
                }
                
                /* Allocates a copy of the depot id */
                if((depotName = strdup(is->fields[0])) == NULL)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
                }
                
                /* Inserts the block tree into the depot tree by the name of the depot */
                if(jrb_insert_str(currSite->depots, depotName, JNULL) == NULL)
                {
                    logPrintError(0, __FILE__, __LINE__, 1, "Memory Allocation Error");  
                }
            } 
        }
    }
    
    /* Closes the resources of the fields struct */
    jettison_inputstruct(is);
    
    /* Sets the time of the read */
    warmer->mtime = sb.st_mtime;
    
    
    return 0;
    
    
    ERROR_HANDLER:
    
        if(is != NULL)
        {
            /* Closes the resources of the fields struct */
            jettison_inputstruct(is);
            
            if(warmer->sites != NULL)
            {
                JRB jnode1;
                JRB jnode2;
                Site *site1;
                
                while(!jrb_empty(warmer->sites))
                {
                    jnode1 = jrb_first(warmer->sites);   
                    
                    site1 = (Site*)jnode1->val.v;
                    
                    if(site1 == currSite)
                    {
                        currSite = NULL;   
                    }
                    
                    if(site1->name == siteName)
                    {
                        siteName = NULL;   
                    }
                    
                    while(!jrb_empty(site1->depots))
                    {
                        jnode2 = jrb_first(site1->depots);
                        
                        if(jnode2->key.s == depotName)
                        {
                            depotName = NULL;   
                        }
                        
                        free(jnode2->key.s);
                        jrb_delete_node(jnode2);
                    }
                    
                    jrb_free_tree(site1->depots);
                    free(site1->name);
                    free(site1);
                    jrb_delete_node(jnode1);
                }
            }
            
            if(currSite!= NULL)
            {
                free(currSite);   
            }
            
            if(siteName != NULL)
            {
                free(siteName);    
            }
            
            if(depotName != NULL)
            {
                free(depotName);   
            }
        }
    
        return -1;
}


