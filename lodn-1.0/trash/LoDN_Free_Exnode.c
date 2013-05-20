/*
 *  warmer.c
 *
 *  Copyright (C) Chris Sellers - September 2006
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
/****************************************************************************************
 * Program:     LoDN_Free_Allocations
 * Description: This program takes a list of exnode files and for each exnode file, it
 *              iterates over the list of IBP allocation mappings decrementing the 
 *              reference count of each via IBP_manage.  Once the reference count reaches
 *              zero the depot will free the allocation.
 ****************************************************************************************/
 
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include "libexnode/exnode.h"
#include "ibp.h"



/***---Constants----***/
#define IBP_TIMER   60
#define MAX_CACHE_ADDR 512


/***--IBP DNS Cache--***/

#if IBPv031 != 0
typedef struct
{
    char hostName[256];
    struct addrinfo *addr;
    int ref;
    time_t lastUsed;
} CACHEADDR;

typedef struct
{
    #ifndef IBP_WIN32
    #ifdef HAVE_PTHREAD_H
    pthread_mutex_t * lock;
    #endif                          /*  */
    #else                           /*  */
    #endif                          /*  */
    CACHEADDR addrs[MAX_CACHE_ADDR];
}DNSCACHE;
#endif


/***----Globals-----***/
#if IBPv031 != 0
extern DNSCACHE *glbCache;  
#endif


/***---Prototypes---***/
int  deserializeExnode(char *exnode_filename, Exnode **exnode);
void freeExnodeMappings(char *exnodeFilename);



/***-------Main-----***/
int main(int argc, char *argv[])
{
    /* Declarations */
    int i;
    
    
    /* Prints a useful help message */
    if(argc < 2 || strcmp("--help", argv[1]) == 0 || strcmp("-h", argv[1]) == 0)
    {
        printf("Usage: LoDN_Free_Exnode exnode-file [exnode-files...]\n");
        exit(EXIT_SUCCESS);
    }
    
    /* Iterates over the list of exnodes */
    for(i=1; i<argc; i++)
    {
        freeExnodeMappings(argv[i]);          
    }
    
    /* Return successuflly to the parent process */
    return EXIT_SUCCESS;   
}



/****-----------------------------deserialize_exnode()--------------------------------****
 * Description: This function takes an exnode file and reads into a buffer so 
 *              exnodeDeserialize() can create an exnode struct. 
 * Input: exnode_filename - cstring that holds the name of the exnode file.
 * Output: exnode - double pointer to an exnode struct that gets created from the file.
 *         It returns 0 on success and -1 otherwise.
 ****---------------------------------------------------------------------------------****/
int deserializeExnode(char *exnode_filename, Exnode **exnode)
{
    /* Declarations */
    int      fd         = -1;;
    char    *buf        = NULL;
    size_t   amtRead    = 0;
    ssize_t  count      = 0;
    off_t    exnodeSize = 0;
    struct stat statbuf;
    
    
    /* Opens the exnode file for reading */
    if((fd = open(exnode_filename, O_RDONLY)) < 0)
    {
        fprintf(stderr, "Error: Error opening exnode file %s at %s:%d\n",
                     exnode_filename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER;     
    }
    
    /* Gets the size of the exnode file */
    if(fstat(fd, &statbuf) != 0)
    {
        fprintf(stderr, "Warmer Error: Error obtaining statbuf for %s at %s:%d\n",
                     exnode_filename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER;     
    }
    
    exnodeSize = statbuf.st_size;
    
    /* Allocates a buffer to hold the exnode */
    if((buf = (char*)malloc(exnodeSize*sizeof(char))) == NULL)
    {
        fprintf(stderr, "Warmer Error: Memory Allocation Error at %s:%d\n",
                     __FILE__,  __LINE__-3);
        goto ERROR_HANDLER;
    }
    
    /* Reads the exnode file into the buffer */
    while(amtRead < exnodeSize)
    { 
        /* Read as much as possible at a time */
        if((count = read(fd, buf+amtRead, exnodeSize-amtRead)) < 0)
        {
            fprintf(stderr, "Warmer Error: Error reading from exnode file %s at %s:%d\n",
                        exnode_filename, __FILE__, __LINE__-3);
            goto ERROR_HANDLER;
        }
        
        /* Update the amount read */
        amtRead += count;
    }
    
    /* Uses the buffer to create the exnode */
    if(exnodeDeserialize(buf, exnodeSize, exnode) != 0)
    {
        fprintf(stderr, "Warmer Error: Error deserializing exnode file %s at %s:%d\n\n",
                        exnode_filename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER;     
    }
    
    /* Fress the resources */
    free(buf);  
    close(fd);
    
    /* Return successfully */
    return 0;
    
    
    /* Error handler */
    ERROR_HANDLER:
    
        /* Frees all of the resources */
        if(fd != -1 )
        {
            close(fd); 
            
            if(buf != NULL)
            {
                free(buf);
            }   
        }
        
        /* Return unsuccessfully */
        return -1;
}



/****-----------------------------freeExnodeMappings()--------------------------------****
 * Description: This function takes an exnode file and iterates through all of the 
 *              mappings lowering the reference count of each allocation so that once
 *              an allocation hits zero, the depot will free the allocation. 
 * Input: exnodeFilename - cstring that holds the name of the exnode file.
 ****---------------------------------------------------------------------------------****/ 
void freeExnodeMappings(char *exnodeFilename)
{
    /* Declarations */
    Exnode                  *exnode         = NULL;
    ExnodeEnumeration       *exnode_enum    = NULL;
    ExnodeMapping           *mapping        = NULL;
    struct ibp_set_of_caps   caps           = {NULL, NULL, NULL};      
    struct ibp_timer         timer          = { IBP_TIMER, IBP_TIMER };
    int                      retval         = 0;
   
    
    /* Loads the exnode file into memory and referencable from exnode. */
    if(deserializeExnode(exnodeFilename, &exnode) != 0)
    {
        goto ERROR_HANDLER;
    }
    
    /* Gets a list of the mappings in the exnode */
    if(exnodeGetMappings(exnode, &exnode_enum) != 0)
    {
        fprintf(stderr, "Error: Can't get exnode mappings for %s at %s:%d\n",
                    exnodeFilename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER;
    }
    
    /* Iterates over the list of IBP mappings and decrementing the read count */
    while((retval = exnodeGetNextMapping(exnode_enum, &mapping))==EXNODE_SUCCESS &&
            mapping != NULL)
    {
        /* Initializes the caps */
        caps.manageCap = NULL;
        caps.readCap   = NULL;
        caps.writeCap  = NULL;
        
        /* Gets the IBP capabilites */
        if(exnodeGetCapabilities(mapping, &caps.readCap, &caps.writeCap, 
                             &caps.manageCap) != EXNODE_SUCCESS)
        {
            fprintf(stderr, "Error: Can't get exnode capabilities in %s at %s:%d\n",
                    exnodeFilename, __FILE__, __LINE__-3);
            continue;
        }
       
        /* Clears the IBP DNS Cache for ibp 3.1 error */ 
        #if IBPv031 != 0
        if(glbCache != NULL)
        {
            #ifdef HAVE_PTHREAD_H
            free(glbCache->lock);
            #endif

            free(glbCache);
            glbCache = NULL;
        }
        #endif

        /* Uses the management capability to decrement the read count so that that the
         * depot will delete the the allocation once it hits zero */
        if(IBP_manage(caps.manageCap, &timer, IBP_DECR, IBP_READCAP, NULL) != 0)
        {
            fprintf(stderr, "Error: Can't decrement read capability %s in %s due to IBP error %d at %s:%d\n",
                        caps.readCap, exnodeFilename, IBP_errno,  __FILE__, __LINE__-3);   
        }
        
        /* Frees the caps */
        free(caps.manageCap);
        free(caps.readCap);
        free(caps.writeCap);
    }
    
     /* If the iteration fails then it aborts. */
    if(retval != EXNODE_SUCCESS)
    {
        fprintf(stderr, "Error: Error traversing exnode mapping for %s at %s:%d\n",
                    exnodeFilename, __FILE__, __LINE__-3);
        goto ERROR_HANDLER; 
    }
    
    
    /* Frees the exnode resources */
    exnodeDestroyEnumeration(exnode_enum);
    exnodeDestroyExnode(exnode);
    
    return;
    
    
    /* Frees the resources on error */
    ERROR_HANDLER:
        
        if(exnode != NULL)
        {
            exnodeDestroyExnode(exnode);
            
            if(exnode_enum != NULL)
            {
                exnodeDestroyEnumeration(exnode_enum);
                
                if(caps.manageCap != NULL)
                {
                    free(caps.manageCap);   
                }
                
                if(caps.readCap  != NULL)
                {
                    free(caps.readCap);    
                }
                
                if(caps.writeCap != NULL)
                {
                    free(caps.writeCap);   
                }
            }
        }
}
