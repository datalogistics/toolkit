/*
 * ibp_nfud.h
 * 
 * Yong Zheng.
 * (c) 2003 
 */
#ifndef _IBP_NFUD_H
#define _IBP_NFUD_H

#ifdef HAVE_CONFIG_H
#include "config-ibp.h"
#endif
#include "ibp_nfu.h"
#include "ibp_server.h"
#include "ibp_resources.h"
#include "ibp_nfu_para.h"

#define IBP_E_NFU_UNKNOWN_PARA_IO_TYPE    -30001;
#define IBP_E_NFU_UNKNOWN_PARA_DATA_TYPE  -30002;
#define IBP_E_NFU_INVALID_PARA            -30002;
#define IBP_E_NFU_DUPLICATE_CAP           -30004;
#define IBP_E_NFU_INVALID_RET_PARA        -30005;

#define IBP_NFU_MAX_PAGES                 256

#if 0
typedef struct {
  void  *data;
  int   len;
}NFU_PARA;
#endif 

typedef struct {
  IOTYPE        io_type;
  void          *data;
  int           len;
  int           begin_offset;
  int           cur_offset; 
  RESOURCE      *rs;
  IBP_byteArray ba;
  void          *buf;
  int           fd;

    char        *mmap_ptr;
    size_t      mmap_len;
    int         header_len;
}S_PARA;

typedef struct {
  OP  op;
  int is_thread;
}NFU_OP;

/*typedef int (*OP)( int nPara, NFU_PARA *paras );*/

#endif 
