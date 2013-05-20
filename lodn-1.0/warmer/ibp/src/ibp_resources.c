#include "config-ibp.h"
#ifdef STDC_HEADERS 
#include <stdio.h>
#include <sys/types.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif
#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "ibp_resources.h"
#include "ibp_server.h"
#include "fields.h"
#include "fs.h"

RESOURCE *crtResource ( ) {
    RESOURCE *rs;

    if ((rs = (RESOURCE *)calloc(1,sizeof(RESOURCE)))!= NULL ){
        rs->type = NONE;
        rs->RID = -1;
        rs->ByteArrayTree = make_jrb();
        rs->VolatileTree = make_jrb();
        rs->TransientTree = make_jrb();
        rs->StableStorage = 0;
        rs->StableStorageUsed = 0;
        rs->VolStorage = 0;
        rs->VolStorageUsed = 0;
        rs->MinFreeSize = 0;
        rs->zeroconf = 1;
        rs->subdirs = 16;
        rs->NextIndexNumber = 1;
        rs->MaxDuration = 0;
        rs->rcvFile[0] = '\0';
        rs->rcvFd = NULL;
        rs->rcvTimeFile[0] = '\0';
        rs->rcvTimeFd = NULL;
        rs->rcvOptTime = 0;
        rs->logFile[0] = '\0';
        rs->logFd = NULL;
        pthread_mutex_init(&(rs->alloc_lock),NULL);
        pthread_mutex_init(&(rs->recov_lock),NULL);
        pthread_mutex_init(&(rs->change_lock),NULL);
        pthread_mutex_init(&(rs->rlock),NULL);
        pthread_mutex_init(&(rs->wlock),NULL);
        pthread_mutex_init(&(rs->block),NULL);
        rs->IPv4 = NULL;
#ifdef AF_INET6
        rs->IPv6 = NULL;
#endif
    }
    return rs;
}

int isValidRID ( int  rid ){

  if ( rid < 0 ) { return 0; }
  /*
  while ( rid[i] != '\0' && i < MAX_RID_LEN ){
    if ( ( rid[i] >= 'a' && rid[i] <= 'z' ) ||
         ( rid[i] >= 'A' && rid[0] <= 'Z' ) ||
         ( rid[i] >= '0' && rid[i] <= '9' ) ||
          rid[i] == '_' || rid[i] == '-' ) {
      i++;
    }else{
      return ( 0 ) ;
    }
  }
  */

  return (1);
}
   
void  freeResource ( RESOURCE *rs ){
    IPv4_addr_t  *ip,*next;
#ifdef AF_INET6
    IPv6_addr_t  *ip6,*next6;
#endif
    
    if ( rs != NULL ){
        switch(rs->type){
        case DISK:
        case MEMORY:
            if ( rs->loc.dir != NULL ){
                free(rs->loc.dir);
            }
            break;
        case RMT_DEPOT:
            if ( rs->loc.rmtDepot.host != NULL){
                free(rs->loc.rmtDepot.host);
            }
            break;
        default:
            break;
        }
        next = rs->IPv4;
        while ( next != NULL ){
           ip = next;
           next = ip->next;
           free(ip);
        }
#ifdef AF_INET6
        next6 = rs->IPv6;
        while ( next6 != NULL ){
            ip6 = next6;
            next6 = ip6->next;
            free(ip6);
        }
#endif
        rs->rcvFile[0] = '\0';
        rs->rcvTimeFile[0] = '\0';
        rs->logFile[0] = '\0';
        if ( rs->rcvFd != NULL ) { fclose(rs->rcvFd); rs->rcvFd = NULL; }
        if ( rs->rcvTimeFd != NULL ) { fclose(rs->rcvTimeFd); rs->rcvTimeFd = NULL; }
        if ( rs->logFd != NULL ) { fclose(rs->logFd); rs->logFd = NULL; }

        if ( rs->ByteArrayTree != NULL ){
            jrb_free_tree(rs->ByteArrayTree);
            rs->ByteArrayTree = NULL ; 
        }
        if ( rs->VolatileTree != NULL ){
            jrb_free_tree(rs->VolatileTree);
            rs->VolatileTree = NULL ; 
        }
        if ( rs->TransientTree != NULL ){
            jrb_free_tree(rs->TransientTree);
            rs->TransientTree = NULL ; 
        }

        free(rs);
    }

    return;
}

RSTYPE getRsType( char *name){
    RSTYPE type;

    if ( name == NULL ) {
        type = NONE;
    }else if ( strcmp(name , RS_T_DISK) == 0 ){
        type = DISK;
    }else if ( strcmp(name, RS_T_MEMORY) == 0 ){
        type = MEMORY;
#ifdef SUPPORT_RMT_TYPE
    }else if ( strcmp(name, RS_T_RMT_DEPOT) == 0){
        type = RMT_DEPOT;
#endif
    }else if (strcmp(name,RS_T_ULFS) == 0 ){
        type = ULFS;
    }else {
        type = NONE;
    }

    return (type);
}

int getRsLoc ( RESOURCE *rs, char *strLoc ){
    int ret = IBP_OK;
    char *next, *ind;

    assert ( (rs != NULL ) && ( strLoc != NULL) && (rs->type != NONE));

    switch ( rs->type){
        case DISK:
            if ( strLoc[0] != DIR_SEPERATOR_CHR ){
                ret = E_ABSPATH;
            }else{
                if ( strLoc[strlen(strLoc)-1] != DIR_SEPERATOR_CHR){
                    strcat(strLoc,DIR_SEPERATOR_STR);
                }
                if ( (rs->loc.dir = strdup(strLoc)) == NULL ){
                    ret = E_ALLOC;
                }
            }
            break;
        case MEMORY:
            if ( (rs->loc.dir = strdup(RS_T_MEMORY)) == NULL ){
                ret = E_ALLOC;
            }
            break;
        case ULFS:
            if ( strLoc[0] != DIR_SEPERATOR_CHR ){
                ret = E_ABSPATH;
            }else{
                if ( (rs->loc.dir = strdup(strLoc)) == NULL ){
                    return (E_ALLOC);
                }
            }
            /*
            if ( ( rs->loc.ulfs.loc = strdup(strLoc)) == NULL ){
                return (E_ALLOC);
            }
            */
            break;
        case RMT_DEPOT:
            next = strLoc;
            if ( (ind = rindex(next,':')) == NULL ){
                fprintf(stderr,"[%d]: Malformed location\n",rs->RID);
                ret = E_CFGFILE;
                break;
            }
            *ind = '\0';
            if ( (rs->loc.rmtDepot.host = strdup(next)) == NULL ){
                ret = E_ALLOC;
                break;
            }
            next = ++ind ;
            ind = index(next,'#');
            if ( ind != NULL ){
                *ind = '\0';
            }
            if ( sscanf(next,"%d",&(rs->loc.rmtDepot.port)) != 1 ){
                fprintf(stderr,"[%d]: Invalid port number\n",rs->RID);
                ret = E_CFGFILE;
                break;
            }
            if ( ind != NULL ){
                next = ++ind;
                if ( 0 != ibpStrToInt(next,&(rs->loc.rmtDepot.RID))) {
                    fprintf(stderr,"[%s] Invalid location\n",next);
                    ret = E_CFGFILE;
                    break;
                }
            }
            break;
        default:
            assert(0);
    }

    return (ret);
}

int isValidRsCfg ( RESOURCE *rs ){
    DIR  *dir;
    int  fd;

    assert ( rs != NULL );
    /*
    if ( rs->RID < 0 ){
        fprintf(stderr," ERROR: No resource ID is specified !\n");
        return (E_CFGFILE);
    }
    */
    if ( ! isValidRID(rs->RID) ) {
      fprintf(stderr," ERROR: Invalid character  Resource ID : %d !\n",rs->RID );
      return ( E_CFGFILE);
    }
    
    if ( rs->type == NONE ) {
        fprintf(stderr," ERROR: No resource type is specified for resource [%d] ! \n",
                       rs->RID);
        return ( E_CFGFILE);
    } ;

    switch ( rs->type) {
        case DISK:
            /*
             * check storage dir
             */ 
            if ( rs->loc.dir == NULL ){
                fprintf(stderr," ERROR: No resource location is specfied for resource [%d] !\n",
                               rs->RID);
                return ( E_CFGFILE);
            }
            if ( ( dir = opendir(rs->loc.dir)) == NULL ){
                fprintf(stderr," ERROR: Invalid resource location [%s] for resource [%d] !\n",
                               rs->loc.dir,rs->RID);
                return ( E_CFGFILE);
            }

            /*
             * check log file 
             */
            if ( rs->loc.dir[strlen(rs->loc.dir)-1] == DIR_SEPERATOR_CHR ){
                sprintf(rs->logFile,"%sibp.log",rs->loc.dir);
            }else{
                sprintf(rs->logFile,"%s%cibp.log", rs->loc.dir, DIR_SEPERATOR_CHR);
            }
            if ( ( rs->logFd = fopen(rs->logFile, "a")) == NULL ){
                fprintf(stderr," ERROR: Can't open log file for resource [%d] !\n",
                               rs->RID);
                return (E_CFGFILE);
            }

            /*
             * generate  recover file name
             */
            if ( rs->loc.dir[strlen(rs->loc.dir)-1] == DIR_SEPERATOR_CHR ){
                sprintf(rs->rcvFile,"%sibp.lss",rs->loc.dir);
            }else{
                sprintf(rs->rcvFile,"%s%cibp.lss",rs->loc.dir,DIR_SEPERATOR_CHR);
            }

            /*
             * generate recover time file name
             */
            if ( rs->loc.dir[strlen(rs->loc.dir)-1] == DIR_SEPERATOR_CHR ){
                sprintf(rs->rcvTimeFile,"%sibp.time",rs->loc.dir);
            }else{
                sprintf(rs->rcvTimeFile,"%s%cibp.time",rs->loc.dir,DIR_SEPERATOR_CHR);
            }
            break;
        case ULFS:
            /* check storage dir */
            if ( rs->loc.dir == NULL ){
                fprintf(stderr," ERROR: No resource location is specified for resource [%d] !\n", rs->RID);
                return (E_CFGFILE);
            }
            if ( (fd = open(rs->loc.dir,O_RDWR)) < 0 ){
                fprintf(stderr," ERROR: Invalid resource location [%s] for resource [%d] !\n",rs->loc.dir, rs->RID);
                return(E_CFGFILE);
            }

            /* check log file */
            sprintf(rs->logFile,"%src%d.log",gc_cfgpath,rs->RID);
            if ((rs->logFd = fopen(rs->logFile,"a")) == NULL){
                fprintf(stderr," ERROR: Can't open log file for resource [%d]!\n", rs->RID);
                return (E_CFGFILE);
            }
            sprintf(rs->rcvFile,"%src%d.lss",gc_cfgpath,rs->RID);
            sprintf(rs->rcvTimeFile,"%src%d.time",gc_cfgpath,rs->RID);

            /* open resource */
            if ( (rs->loc.ulfs.ptr = ibpfs_init(rs->loc.dir)) == NULL ){
                fprintf(stderr," ERROR: Can't open resource [%d] !\n",rs->RID);
                return (E_CFGFILE);
            }
            break;
#ifdef SUPPORT_RMT_DEPOT
        case RMT_DEPOT:
            if ( rs->loc.rmtDepot.host == NULL ){
                fprintf(stderr," ERROR: No remote resource location is specified for resource [%d]!\n",rs->RID);
                return ( E_CFGFILE);
            } ;
            rs->VolStorage = 0;
            rs->StableStorage = 0;
            rs->MaxDuration = 0;
            rs->MinFreeSize = 0;
            break;
#endif
        case MEMORY:
            if ( rs->loc.dir == NULL ){
                rs->loc.dir = strdup("MEMORY");
            }
            break;
        default:
            return ( E_CFGFILE );
    }

    if ( ((rs->type == MEMORY) || ( rs->type == DISK)) &&
         (( rs->VolStorage < 0 || rs->StableStorage < 0 || rs->MinFreeSize < 0 ))) {
        fprintf( stderr," ERROR: Invalide storage size for resource [%d] \n",rs->RID);
        return ( E_CFGFILE);
    }

    if ( ((rs->type == MEMORY) || ( rs->type == DISK)) && 
         ( rs->VolStorage + rs->StableStorage <= 0 )){
        fprintf(stderr," ERROR: No resource size is specified for resource [%d]! \n",rs->RID);
        return ( E_CFGFILE);
    }
    if ( ((rs->type == MEMORY) || ( rs->type == DISK)) && 
         ( rs->VolStorage < rs->StableStorage)) {
        fprintf(stderr," ERROR: Resource [%d]'s Volatile/Soft storage should be no less than Stable/Hard storage\n"
                       ,rs->RID);
        return ( E_CFGFILE);
    }

    if ( ((rs->type == MEMORY) || ( rs->type == DISK)) && 
         ( rs->MaxDuration < -1 )) {
        fprintf( stderr," ERROR: Invalide MAXDURATION for resource [%d] ! \n", rs->RID);
        return ( E_CFGFILE);
    }

    if ( ((rs->type == MEMORY) || ( rs->type == DISK)) &&
         (rs->MaxDuration == 0 )){
        rs->MaxDuration = -1;
    }
    
    return (IBP_OK);
    
}

int getRsCfg(RESOURCE *rs,IS cfgFile){
    int ret = IBP_OK;
    int opened = 1;
    float dur;
    IPv4_addr_t  *ip, *tmpIp;
#ifdef AF_INET6
    IPv6_addr_t  *ip6, *tmpIp6;
#endif
    char    *ep;

    assert( rs != NULL);
    assert( strcmp(cfgFile->fields[0],RS_RESOURCEID) == 0 );

    if ( 0 != ibpStrToInt(cfgFile->fields[1],&(rs->RID))) {
        fprintf(stderr,"ERROR: Invalid resource id : %s\n",cfgFile->fields[1]);
        return(E_CFGFILE);

    };
            
    /*strncpy(rs->RID,cfgFile->fields[1],MAX_RID_LEN+1);*/
    
    while ((get_line(cfgFile) >= 0 ) && ( ret == IBP_OK)){
        if ( cfgFile->NF >= 1 ){
            if ( strcmp(cfgFile->fields[0], RS_END) == 0 ){
                opened = 0;
                break;
            }
        }
        if ( cfgFile->NF < 2 ) {
            continue;
        }
        if ( cfgFile->fields[0][0] == '#' ){
            continue;
        }
        if (strcmp(cfgFile->fields[0],RS_TYPE) == 0){
            if ( rs->type != NONE ){
                if ( ( rs == glb.dftRs ) ){
                    continue;
                }else {
                    fprintf(stderr," ERROR: Duplicated resource type at line %d in config file\n", 
                               cfgFile->line);
                    ret = E_CFGFILE;
                    break;
                }
            }
            if ( ( rs->type = getRsType(cfgFile->fields[1])) == NONE ){
                fprintf(stderr," ERROR: Unknown resource type at line %d in config file\n",
                               cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
        }else if ( strcmp(cfgFile->fields[0],RS_ZEROCONF) == 0){
            rs->zeroconf = atoi(cfgFile->fields[1]);
        }else if ( strcmp(cfgFile->fields[0],RS_SUBDIRS) == 0){
            rs->subdirs = strtol(cfgFile->fields[1],&ep,10);
            if ( ep[0] != '\0'){
                fprintf(stderr," ERROR: Invalid number of sub dirs at line %d in config file \n",cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
            if ( rs->subdirs < 0 ) { rs->subdirs = 16; }
            if ( rs->subdirs > 512) { rs->subdirs = 512; }
        }else if ( strcmp(cfgFile->fields[0],RS_LOCATION) == 0 ){
            if ( rs->loc.dir != NULL ){
                if ( rs == glb.dftRs ){
                    continue;
                }else {
                    fprintf(stderr," ERROR: Duplicated resource location at line %d in config file\n"
                               ,cfgFile->line);
                    ret = E_CFGFILE;
                    break;
                }
            }
            if ( rs->type == NONE ){
                fprintf(stderr," ERROR: Please specify resource type before resource location\n");
                ret = E_CFGFILE;
                break;
            }
            if ( (ret = getRsLoc(rs,cfgFile->fields[1])) != IBP_OK){
                break;
            }
        }else if ( ( strcmp(cfgFile->fields[0],RS_STABLESIZE) == 0 ) ||
                   ( strcmp(cfgFile->fields[0],RS_HARDSIZE) == 0)){
            if ( rs->StableStorage > 0 ) {
                if ( rs == glb.dftRs ){
                    continue;
                }else {
                    fprintf(stderr," ERROR: Duplicated hard storage size at line %d in config file\n"
                               , cfgFile->line);
                    ret = E_CFGFILE;
                    break;
                }
            }
            if ( ( rs->StableStorage = ibp_getStrSize(cfgFile->fields[1])) == 0  && 
                   errno != 0 ){
                fprintf(stderr," ERROR: Malformed hard storage size at line %d in config file\n"
                        ,cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
        }else if ( ( strcmp(cfgFile->fields[0],RS_VOLSIZE) == 0 ) ||
                   ( strcmp(cfgFile->fields[0],RS_SOFTSIZE) == 0)){
            if ( rs->VolStorage > 0 ) {
                if ( rs == glb.dftRs ){
                    continue;
                }else {
                    fprintf(stderr," ERROR: Duplicated soft storage size at line %d in config file\n"
                               , cfgFile->line);
                    ret = E_CFGFILE;
                    break;
                }
            }
            if ( ( rs->VolStorage = ibp_getStrSize(cfgFile->fields[1])) == 0  && 
                   errno != 0 ){
                fprintf(stderr,"[%d]: Malformed soft storage size at line %d in config file\n"
                        ,rs->RID,cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
        }else if  ( strcmp(cfgFile->fields[0],RS_MINFREESIZE) == 0 ) {
            if ( rs->MinFreeSize > 0 ) {
                if ( rs == glb.dftRs ){
                    continue;
                }else {
                    fprintf(stderr," ERROR: Duplicated min free storage size at line %d in config file\n"
                               , cfgFile->line);
                    ret = E_CFGFILE;
                    break;
                }
            }
            if ( ( rs->MinFreeSize = ibp_getStrSize(cfgFile->fields[1])) == 0  && 
                   errno != 0 ){
                fprintf(stderr," ERROR: Malformed min free storage size at line %d in config file\n"
                        ,cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
        }else if ( strcmp(cfgFile->fields[0],RS_MAXDURATION) == 0 ){
            if ( rs->MaxDuration != 0 ){
                if ( rs == glb.dftRs ){
                    continue;
                }else {
                    fprintf(stderr," ERROR: Duplicated max duration at line %d in config file\n",
                                cfgFile->line);
                    ret = E_CFGFILE;
                    break;
                }
            }
            if ( sscanf(cfgFile->fields[1],"%f",&dur) != 1 ){
                fprintf(stderr," ERROR: Invalid value of max duration at line %d in config file\n"
                               ,cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
            if ( dur < 0 ) {
                rs->MaxDuration = -1;
            }else{
                rs->MaxDuration = (long)(dur * 24 * 3600);
            }   
        }else if ( strcmp(cfgFile->fields[0],RS_IPV4) == 0 ){
            if (cfgFile->NF < 3){
                fprintf(stderr," ERROR: Malformed access list at line %d in config file\n",
                               cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
            if ( (ip = (IPv4_addr_t*)calloc(1,sizeof(IPv4_addr_t))) == NULL ){
                ret = E_ALLOC;
                break;
            }
            ip->next = NULL;
            if ( (ip->addr = inet_addr(cfgFile->fields[1])) == -1 ){
                fprintf(stderr," ERROR: Invalid IP address at line %d in config file\n",
                               cfgFile->line);
                ret = E_CFGFILE;
                free(ip);
                break;
            }
            if ( (ip->mask = inet_addr(cfgFile->fields[2])) == -1){
                if ( strcmp(cfgFile->fields[2],"255.255.255.255") != 0 ){
                    fprintf(stderr," ERROR: Invalid IP net mask at line %d in config file\n",
                                   cfgFile->line);
                    ret = E_CFGFILE;
                    free(ip);
                    break;
                }
            }
            tmpIp = rs->IPv4;
            if ( tmpIp == NULL ){
                rs->IPv4 = ip;
            }else{
                while ( tmpIp->next != NULL ) {
                    tmpIp = tmpIp->next;
                }
                tmpIp->next = ip;
            }
#ifdef AF_INET6
        }else if ( strcmp(cfgFile->fields[0],RS_IPV6) == 0 ){
            if ( cfgFile->NF < 3 ){
                fprintf(stderr," ERROR: Malformed access list at line %d in config file\n",
                               cfgFile->line);
                ret = E_CFGFILE;
                break;
            }
            if ( (ip6 = (IPv6_addr_t*)calloc(1,sizeof(IPv6_addr_t))) == NULL ){
                ret = E_ALLOC;
                break;
            }
            ip6->next = NULL;
            if ( inet_pton(AF_INET6,cfgFile->fields[1],&(ip6->addr)) == -1 ){
                fprintf(stderr," ERROR: Invalid IPv6 address at line %d at config file\n",
                               cfgFile->line);
                ret = E_CFGFILE;
                free(ip6);
                break;
            }
            if ( inet_pton(AF_INET6,cfgFile->fields[2],&(ip6->mask)) == -1 ){
                fprintf(stderr," ERROR: Invalid IPv6 net mask at line %d at config file\n",
                                cfgFile->line);
                free(ip6);
                ret = E_CFGFILE;
                break;
            }
            tmpIp6 = rs->IPv6;
            if ( tmpIp6 == NULL ){
                rs->IPv6 = ip6;
            }else {
                while ( tmpIp6->next != NULL ) {
                    tmpIp6 = tmpIp6->next;
                }
                tmpIp6->next = ip6;
            }
#endif
        }else {
            fprintf(stderr," ERROR: Unrecognized entry at line %d in config file for resource [%d]\n",
                            cfgFile->line,rs->RID);
            ret = E_CFGFILE;
            break;
        }
    }


    if ( ret == IBP_OK ){
        ret = isValidRsCfg(rs);  
    }else {
        if ( opened && (get_line(cfgFile)) < 0  ){
            fprintf(stderr,"%d  ERROR: Missing RESOURCE_END entry\n",rs->RID);
            ret = E_CFGFILE;
        }
    }
    return (ret);
}

void printResource( RESOURCE *rs, FILE *out , char *pad ){
    char  tmp[50],tmp1[50];
    IPv4_addr_t *ip;
#ifdef AF_INET6
    IPv6_addr_t *ip6;
#endif

    if ( rs == NULL ) return ;

    fprintf(out,"%sID\t\t: %d\n",pad,rs->RID);
    switch ( rs->type){
    case MEMORY:
        fprintf(out,"%sType\t: MEMORY\n",pad);
        fprintf(out,"%sLocation\t: NULL\n",pad);
        break;
    case DISK:
        fprintf(out,"%sType\t: DISK\n",pad);
        fprintf(out,"%sLocation\t: %s\n",pad,rs->loc.dir);
        break;
    case RMT_DEPOT:
        fprintf(out,"%sType\t: Remote Depot\n",pad);
        fprintf(out,"%sLocation\t: %s:%d#%d\n",pad,
                        rs->loc.rmtDepot.host,
                        rs->loc.rmtDepot.port,
                        rs->loc.rmtDepot.RID);
        break;
    default:
        break;
    }
    fprintf(out,"%sStb\t\t: %s Bytes\n",pad,ibp_ultostr(rs->StableStorage,tmp+49,10));
    fprintf(out,"%sStb Used\t: %s Bytes\n",pad,ibp_ultostr(rs->StableStorageUsed,tmp+49,10));
    fprintf(out,"%sVol\t\t: %s Bytes\n",pad,ibp_ultostr(rs->VolStorage,tmp+49,10));
    fprintf(out,"%sVol Used\t: %s Bytes\n",pad,ibp_ultostr(rs->VolStorageUsed,tmp+49,10));
    fprintf(out,"%sMin Free\t: %s Bytes\n",pad,ibp_ultostr(rs->MinFreeSize,tmp+49,10));
    if ( rs->MaxDuration == -1 ){
        fprintf(out,"%sDuration\t: Infinit\n",pad);
    }else if ( rs->MaxDuration == 0 ){
        fprintf(out,"%sDuration\t: 0\n",pad);
    }else {
        fprintf(out,"%sDuration\t: %ld Day %ld Hours %ld Minute %ld Second\n",pad,
                rs->MaxDuration / (3600*24) ,
                (rs->MaxDuration % 3600*24) / 3600 ,
                ((rs->MaxDuration % 3600*24) % 3600)/60,
                (((rs->MaxDuration % 3600*24) % 3600) % 60));
    }     
    ip = rs->IPv4;
    while (ip != NULL){
       inet_ntop(AF_INET,&(ip->addr),tmp,49);
       inet_ntop(AF_INET,&(ip->mask),tmp1,49);
       fprintf(out,"%sIP List\t: %s\tmask\t%s\n",pad,tmp,tmp1);
       ip = ip->next;
    }
#ifdef AF_INET6
    ip6 = rs->IPv6;
    while ( ip6 != NULL ){
        inet_ntop(AF_INET6,&(ip6->addr),tmp,49);
        inet_ntop(AF_INET6,&(ip6->mask),tmp1,49);
        fprintf(out,"%sIPv6 List\t: %s\tmask\t%s\n",pad,tmp, tmp1);
        ip6 = ip6->next;
    }
#endif

    return ; 

}
