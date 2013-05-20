/*
 * ibp_nfu_para.h
 * 
 * Yong Zheng.
 * (c) 2003 
 */
#ifndef _IBP_NFU_PARA_H
#define _IBP_NFU_PARA_H

typedef struct {
  void  *data;
  int   len;
}NFU_PARA;

typedef int (*OP)( int nPara, NFU_PARA *paras );

#endif 
