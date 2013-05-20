#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ibp_nfu.h"
#include "ibp_ClientLib.h"
#include "ibp_ComMod.h"
#include "ibp_net.h"
#include "ibp_uri.h"

#define MAX_MSG_LEN 256

typedef struct
{
    char msg[MAX_MSG_LEN];
    char *data;
    int len;
    char intBuf[20];
}
_PARAMSG;

static int _checkParameters(int nPara, PARAMETER * paras);
static _PARAMSG *_serializeParameters(int nPara, PARAMETER * paras);

int IBP_nfu_op(IBP_depot depot, int opcode, int nPara, PARAMETER * paras,
                IBP_timer timeout)
{
    int i;
    _PARAMSG *paraMsg = NULL;
    char msg[MAX_MSG_LEN];
    char emptyMsg[20];
    IBP_CommSession cSession;
    IBP_CommUnit_t *cUnit;
    int sockfd = -1;
    int ret;
    int np,len,index;

    /*
     * initialize errno 
     */
    IBP_errno = IBP_OK;

    /*
     * check depot 
     */
    if ((IBP_errno = check_depot(depot->host, &depot->port)) != IBP_OK) {
        return (-1);
    }

    /*
     * check time out
     */
    if ((IBP_errno = check_timeout(timeout)) != IBP_OK) {
        return (-1);
    }

    /*
     * check parameters 
     */
    if ((IBP_errno = _checkParameters(nPara, paras)) != IBP_OK) {
        return (-1);
    }

    /*
     * create connection
     */
    if ((sockfd =
         connect_socket(depot->host, depot->port,
                        timeout->ClientTimeout)) == -1) {
        IBP_errno = IBP_E_CONNECTION;
        ret = -1;
        goto bail;
    }

    /*
     * serialize parameters 
     */
    if ((paraMsg = _serializeParameters(nPara, paras)) == NULL) {
        ret = -1;
        goto bail;
    }

    InitCommSession(&cSession, timeout->ClientTimeout);

    sprintf(msg, "%d %d %d %d \n", IBPv040, IBP_NFU, opcode, nPara);
    if (FillCommUnit
        (&cSession, COM_OUT, sockfd, msg, strlen(msg), DATAVAR,
         NULL) != IBP_OK) {
        ret = -1;
        goto bail;
    }

    for (i = 0; i < nPara; i++) {
        if (FillCommUnit
            (&cSession, COM_OUT, sockfd, paraMsg[i].msg,
             strlen(paraMsg[i].msg), DATAVAR, NULL) != IBP_OK) {
            ret = -1;
            goto bail;
        }
        /*
        if (paras[i].type == DATA_INT_32) {
            if (FillCommUnit
                (&cSession, COM_OUT, sockfd, paraMsg[i].intBuf,
                 strlen(paraMsg[i].intBuf), DATAVAR, NULL) != IBP_OK) {
                ret = -1;
                goto bail;
            }
        }*/

        if ( paraMsg[i].len > 0 ){
            if (FillCommUnit
                (&cSession, COM_OUT, sockfd, paraMsg[i].data, paraMsg[i].len,
                DATAVAR, NULL) != IBP_OK) {
                ret = -1;
                goto bail;
            }
        };
    }

    sprintf(emptyMsg, "\n");
    if (FillCommUnit
        (&cSession, COM_OUT, sockfd, emptyMsg, strlen(emptyMsg), DATAVAR,
         NULL) != IBP_OK) {
        ret = -1;
        goto bail;
    }


    if (FillCommUnit
        (&cSession, COM_IN, sockfd, msg, MAX_MSG_LEN - 1, DATAVAR, (func_pt *)HandleMsgResp) != IBP_OK) {
        ret = -1;
        goto bail;
    }

    if ((cUnit = PerformCommunication(&(cSession))) == NULL) {
        ret = -1;
        goto bail;
    }

    if (sscanf(cUnit->data_buf, "%d", &ret) != 1) {
        IBP_errno = IBP_E_BAD_FORMAT;
        ret = -1;
        goto bail;
    }

    if (ret != IBP_OK) {
        IBP_errno = ret;
        ret = -1;
        goto bail;
    }


    /*
     * get parameters from ibp depot
     */
    if ((ret = read_var_data(sockfd,MAX_MSG_LEN-1,msg,0)) == 0 &&
        IBP_errno != IBP_OK ){
        ret = -1;
        goto bail;
    };
    if (sscanf(msg,"%d",&np)  != 1 ){
        IBP_errno = IBP_E_BAD_FORMAT;
        ret = -1;
        goto bail;
    }
    for ( i = 0 ; i < np ; i++ ){
        if((ret = read_var_data(sockfd,MAX_MSG_LEN-1,msg,0) ==0)&&
           IBP_errno != IBP_OK ){
            ret = -1;
            goto bail;
        }
        if ( sscanf(msg,"%d %d",&index,&len) != 2 ){
            IBP_errno = IBP_E_BAD_FORMAT;
            ret = -1;
            goto bail;
        }
        if ( paras[index].ioType != IBP_VAL_OUT  &&
             paras[index].ioType != IBP_VAL_INOUT ){
            IBP_errno = IBP_E_BAD_FORMAT;
            ret = -1;
            goto bail;
        }
        if ((ret = read_fix_data(sockfd,len,paras[index].data,0) == 0) &&
            (IBP_errno != IBP_OK )){
            ret = -1;
            goto bail;
        }
        paras[index].len = len;
    }

    ret = IBP_OK; 

/*
    if (sscanf(cUnit->data_buf, "%d %d ", &ret, &size) != 2) {
        IBP_errno = IBP_E_BAD_FORMAT;
        ret = -1;
        goto bail;
    }

    ret = size;
*/
  bail:
    close_socket(sockfd);
    if (paraMsg != NULL) {
        free(paraMsg);
    }

    return (ret);

}


static int _checkParameters(int nPara, PARAMETER * paras)
{
    int i;
    int ret = IBP_OK;

    for (i = 0; i < nPara; i++) {
        if (paras[i].ioType != IBP_REF_RD &&
            paras[i].ioType != IBP_REF_WR &&
            paras[i].ioType != IBP_REF_RDWR &&
            paras[i].ioType != IBP_VAL_IN &&
            paras[i].ioType != IBP_VAL_OUT &&
            paras[i].ioType != IBP_VAL_INOUT) {
            IBP_errno = IBP_E_INVALID_PARAMETER;
            ret = IBP_errno;
            goto bail;
        }

        /*
        switch (paras[i].type) {
        case DATA_CAP:
            if ((IBP_cap) (paras[i].data) == NULL) {
                IBP_errno = IBP_E_INVALID_PARAMETER;
                ret = IBP_errno;
                goto bail;
            }
            if (paras[i].offset < 0 || paras[i].len < 0 || paras[i].cnt < 1) {
                IBP_errno = IBP_E_INVALID_PARAMETER;
                ret = IBP_errno;
                goto bail;
            }
            if (gCnt < 0) {
                gCnt = paras[i].cnt;
            }
            else {
                if ((paras[i].cnt != gCnt) && (paras[i].ioType != IO_IMMED)) {
                    IBP_errno = IBP_E_INVALID_PARAMETER;
                    ret = IBP_errno;
                    goto bail;
                }
            }
            break;
        case DATA_INT_32:
            break;
        case DATA_BUF:
            if (paras[i].len < 0) {
                IBP_errno = IBP_E_INVALID_PARAMETER;
                ret = IBP_errno;
                goto bail;
            }
            break;
        default:
            IBP_errno = IBP_E_INVALID_PARAMETER;
            ret = IBP_errno;
            goto bail;
        }
        */
    }
    ret = IBP_OK;

  bail:
    return (ret);

}

static _PARAMSG *_serializeParameters(int nPara, PARAMETER * paras)
{
    _PARAMSG *pmsg;
    int i;
    int dataLen=0;

    if ((pmsg = (_PARAMSG *) calloc(sizeof(_PARAMSG), nPara)) == NULL) {
        IBP_errno = IBP_E_ALLOC_FAILED;
        return (NULL);
    }

    for (i = 0; i < nPara; i++) {
        switch (paras[i].ioType) {
        case IBP_REF_RD:
        case IBP_REF_WR:
        case IBP_REF_RDWR:
            pmsg[i].data = paras[i].data;
            dataLen = strlen((IBP_cap) (paras[i].data));
            pmsg[i].len = dataLen;
            break;
        case IBP_VAL_IN:
        case IBP_VAL_INOUT:
            dataLen = paras[i].len;
            pmsg[i].len = dataLen;
            pmsg[i].data = paras[i].data;
            break;
        case IBP_VAL_OUT:
            pmsg[i].len = 0;
            /*pmsg[i].len = paras[i].len;*/
            dataLen = 0;
            pmsg[i].data = NULL;
            break;
        }
        sprintf(pmsg[i].msg, "%d %d %d %d \n",
                paras[i].ioType,
                paras[i].offset,
                paras[i].len,
                dataLen);
    }

    return (pmsg);

}
