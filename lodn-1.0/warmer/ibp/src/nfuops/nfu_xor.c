#include <stdio.h>
#include "../include/ibp_nfu_para.h"

int do_xor( int npara , NFU_PARA *paras){
    int i,j;
    char c;
    char *src1,*src2,*dst;

    if ( npara != 3 ){
        return (-1);
    };

    src1 = (char*)(paras[0].data);
    src2 = (char*)(paras[1].data);
    dst = (char*)(paras[2].data);
    for ( j=0, i = 0; i < paras[0].len ; i ++ ){
        dst[i] = src1[i]^src2[j];
        j++;
        if ( j >= paras[1].len ){
            j = 0;
        };
    };

    return (0);
};
