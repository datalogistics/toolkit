/*
    ls_mappings.c
    Copyright (C) 2008  Christopher Sellers

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/******************************************************************************
 * Author: Christopher Sellers
 * Date:  Dec 4, 2007
 * Program: ls_mappings.c
 * 
 * Purpose:  Takes an exnode from the command line, reads in its metadata and
 *           queries the depots about the mappings.  It then prints the
 *           information to stderr in the same format as LoCI's lors_ls. 
 ******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h> 
#include <sys/param.h> 
#include <fcntl.h>
#include "libexnode/exnode.h"
#include "ibp.h"


/***---Constants---***/
#define IBP_URL 		"IBP://"



/***---Prototypes---***/
void processMapping(ExnodeMapping *mapping);



/***---Main---***/
int main(int argc, char *argv[])
{
	/* Declarations */
	int      			fd      = -1;
	char    			*buf     = NULL;
	size_t   			amtRead = 0;
	ssize_t  			count   = 0;
	struct  stat       	fileStatus;
	Exnode  			*exnode = NULL;
	ExnodeMetadata    	*exnodeMetadata = NULL;
    ExnodeEnumeration 	*exnodeEnum  = NULL;
    ExnodeMapping     	*mapping      = NULL;
    ExnodeValue        	value;
    ExnodeType         	type;
	char              	*filename;
	long long		  	filesize;
	char			  	*sep;
	int				  	retval;
	
    
	/* Error check the input */
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s exnode-file\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	
	/* Gets the file size of the exnode */
	if(stat(argv[1], &fileStatus) != 0)
	{
		fprintf(stderr, "Error obtaining attributes for exnode %s\n", argv[1]);
	    exit(EXIT_FAILURE);  
	}
	    
    /* Allocates a buffer to hold the exnode */
    if((buf = (char*)malloc(fileStatus.st_size*sizeof(char))) == NULL)
    {
        fprintf(stderr, "Memory Allocation Error\n");
        exit(EXIT_FAILURE);
    }
    
    /* Opens the exnode file for reading */
    if((fd = open(argv[1], O_RDONLY)) < 0)
    {
        fprintf(stderr, "Error opening exnode file %s\n", argv[1]);
        exit(EXIT_FAILURE);     
    }
    
    /* Reads the exnode file into the buffer */
    while(amtRead < fileStatus.st_size)
    { 
        /* Read as much as possible at a time */
        if((count = read(fd, buf+amtRead, fileStatus.st_size-amtRead)) < 0)
        {
            fprintf(stderr, "Error reading from exnode file %s\n", argv[1]);
            exit(EXIT_FAILURE);
        }
        
        /* Update the amount read */
        amtRead += count;
    }
    
    close(fd);
    
    /* Uses the buffer to create the exnode */
    if(exnodeDeserialize(buf, fileStatus.st_size, &exnode) != 0)
    {
        fprintf(stderr, "Error deserializing exnode file %s", argv[1]);
        exit(EXIT_FAILURE);
    }
    
    /* Gets the metadata */
    if(exnodeGetExnodeMetadata(exnode, &exnodeMetadata) != EXNODE_SUCCESS)
    {
    	fprintf(stderr, "Error getting metadata for exnode file %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }
     
    /* Uses the buffer to create the exnode */
    if(exnodeDeserialize(buf, fileStatus.st_size, &exnode) != 0)
    {
        fprintf(stderr, "Error deserializing exnode file %s\n", argv[1]);
        exit(EXIT_FAILURE);     
    }
    
    /* Gets the metadata */
    if(exnodeGetExnodeMetadata(exnode, &exnodeMetadata) != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Error getting metadata for exnode file %s\n", argv[1]);
        exit(EXIT_FAILURE);  
    }

    /* Gets the filename of the exnode */
     if(exnodeGetMetadataValue(exnodeMetadata, "filename", &value, &type) != EXNODE_SUCCESS ||
        value.s == NULL || type != STRING)
     {
         if((sep = strrchr(argv[1], '/')+1) == NULL+1)
         {
             sep = argv[1];
         }
         
         if((filename = strdup(sep)) == NULL)
         {
             fprintf(stderr, "Memory Allocation Error\n");   
             exit(EXIT_FAILURE);
         }
         
         if((sep = strrchr(filename, '.')) != NULL)
         {
             *sep = '\0';   
         }
        
     }else
     {
    	 /* Allocates the name of the exnode */
         if((filename = strdup(value.s)) == NULL)
         {
             fprintf(stderr, "Memory Allocation Error\n"); 
             exit(EXIT_FAILURE);
         }
     }
     
     /* Gets the exnodes filesize */
     if(exnodeGetMetadataValue(exnodeMetadata, "original_filesize", &value, &type) != EXNODE_SUCCESS ||
        type != INTEGER)
     {
    	 filesize = -1;
     }else
     {
    	 filesize = value.i;
     }
     
     /* Prints the data */
     fprintf(stderr, "SIZE %lld\n", filesize);
     fprintf(stderr, "EXNODEFILE %s\n", argv[1]);
     fprintf(stderr, "TITLE Filename %s\n", filename);
     fprintf(stderr, "MAXLENGTH %lld\n", filesize);
     
     /* Gets a list of the mappings in the exnode */
     if(exnodeGetMappings(exnode, &exnodeEnum) != 0)
     {
         fprintf(stderr, "Can't get exnode mappings for %s\n",  argv[1]);
         exit(EXIT_FAILURE);
     }
     
     /* Traverses each mapping in the exnode file printing data about the 
      * mapping */
     while ((retval = exnodeGetNextMapping(exnodeEnum, &mapping))==EXNODE_SUCCESS 
    		 && mapping != NULL)
     {
    	processMapping(mapping);
	}
         
     /* If the iteration fails then it aborts. */
     if(retval != EXNODE_SUCCESS)
     {
         fprintf(stderr, "Error traversing exnode mapping for %s\n", argv[1]);
         exit(EXIT_FAILURE);
     }
     
	
	/* Return Successfully */
	return EXIT_SUCCESS;
}



/***--------------------------processMapping()------------------------------***
 * Description: This function takes a mapping from a exnode, gets its 
 *              metadata and queries the depot about the status of the 
 *              mapping.  It this prints the results to the screen in the
 *              same format as lors_ls.
 * Input: mapping - pointer to the ExnodeMapping.
 ***------------------------------------------------------------------------***/
void processMapping(ExnodeMapping *mapping)
{
	/* Declarations */
	ExnodeMetadata *metadata= NULL;
	ExnodeValue value;
	ExnodeType type;
	int depotnamelen;
	char *readCap;
	char *writeCap;
	char *manageCap;
	char status[6] = "?---?";
	char depotname[256] = "Unknown";
	char *cptr = NULL;
	char *cptr2;
	long long offset = -1;
	long long length = -1;
	static int index = 0;
	struct ibp_timer timeout = { 60, 60 };
	struct ibp_capstatus  info;    
	char  *duration = "Unknown";
	int   available = -1;
	    
	
    /* Gets the metadata for the mapping */
    if(exnodeGetMappingMetadata(mapping, &metadata) != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Can't get exnode mapping metadata\n");
        exit(EXIT_FAILURE);
    }
    
	/* Gets the exnode offset from the Exnode mapping */
	if (exnodeGetMetadataValue(metadata, "exnode_offset", &value, &type)
			== EXNODE_SUCCESS)
	{
		offset = value.i;
	}

	/* Gets the logical length from the exnode */
	if(exnodeGetMetadataValue(metadata, "logical_length", &value, &type) 
			== EXNODE_SUCCESS)
	{
		length = value.i;
	}
	
	
    /* Gets the capablities for mapping */
    if(exnodeGetCapabilities(mapping, &readCap, &writeCap, &manageCap) != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Can't get exnode capabilities\n");
        exit(EXIT_FAILURE);
    }
    
    /* Checks that the readCap is there */
    if(readCap != NULL)
    {
    	status[1] = 'r';
    	cptr = readCap;
    }
    
    /* Checks that the readCap is there */
    if(writeCap != NULL)
    {
    	status[2] = 'w';
    	cptr = writeCap;
    }
    
    
    /* Checks that the readCap is there */
    if(manageCap != NULL)
    {
    	status[3] = 'm';
    	
    	cptr = manageCap;
    	
    	/* Uses the manage capability to get the status of the mapping on
    	 * the depot */
    	if(IBP_manage(manageCap, &timeout, IBP_PROBE, 0, &info) == 0)
    	{
    		/* Records if the mapping is IBP_HARD or IBP_SOFT */
    		switch(info.attrib.reliability)
    		{
    			case IBP_HARD:	status[0] = 'H'; break;
    			case IBP_SOFT:	status[0] = 'S'; break;
    		}
    		
    		/* Records the type of the mapping */
    		switch(info.attrib.type)
    		{
    			case IBP_BYTEARRAY: status[4] = 'a'; break;
    			case IBP_FIFO:		status[4] = 'f'; break;
    			case IBP_CIRQ:		status[4] = 'c'; break;
    			case IBP_BUFFER:	status[4] = 'b'; break;
    		}
    		
    		/* Since manage was successful then mapping is still available */
    		available = 1;
    		
    		/* Converts the duration into a cstring */
    		duration = ctime(&info.attrib.duration);
    	}
    }
    
    
    /* If there is a mapping to obtain the depot name from, it does so */
    if(cptr != NULL)
    {
    	/* Finds the end of the depotID string in the capability and copies
    	 * the depotID into depotname */
    	if((cptr2 = strchr(cptr+strlen(IBP_URL), '/')) != NULL)
    	{
    		depotnamelen = cptr2 - (cptr+strlen(IBP_URL));
    		strncpy(depotname, cptr+strlen(IBP_URL), (sizeof(depotname) < depotnamelen) ? sizeof(depotname) : depotnamelen);
    	}
    }
    
    /* Prints the results to the screen */
    fprintf(stderr, "%2d %s %2d  %8lld  %8lld\t%s %s", index, status, available, 
    		offset, length, depotname, duration);

    /* If the duration string doesn't alreay have a \n character then it
     * prints a newline */
    if(duration[strlen(duration)-1] != '\n')
    {
    	fprintf(stderr, "\n");
    }
    
    /* Increment the index of the mappings */
    index++;
}
