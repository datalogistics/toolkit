/*
 *           IBP Client version 1.0:  Internet BackPlane Protocol
 *               University of Tennessee, Knoxville TN.
 *          Authors: Y. Zheng A. Bassi, W. Elwasif, J. Plank, M. Beck
 *                   (C) 1999 All Rights Reserved
 *
 *                              NOTICE
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted
 * provided that the above copyright notice appear in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * Neither the Institution (University of Tennessee) nor the Authors 
 * make any representations about the suitability of this software for 
 * any purpose. This software is provided ``as is'' without express or
 * implied warranty.
 *
 */  
    
# include "config-ibp.h"
# include "ibp_os.h"
    
# ifndef IBP_WIN32  /* unix-like OS */
#	include <errno.h>
#	ifdef HAVE_PTHREAD_H 
#		include <pthread.h>
#	endif 
# else
#	include <windows.h>
# endif
    
/*
 * definition of errno code
 */ 
# ifndef IBP_WIN32
#	ifndef HAVE_PTHREAD_H 
int IBP_errno;

#	endif
# else
    __declspec(thread)
     int IBP_errno = 0;

# endif
    
# ifndef IBP_WIN32  /* unix-like  OS */
void _set_errno_unix(int err)
{
    errno = err;
}
int _get_errno_unix()
{
    return (errno);
}


#	ifdef HAVE_PTHREAD_H 
    pthread_once_t errno_once = PTHREAD_ONCE_INIT;
static pthread_key_t errno_key;
void _errno_destructor(void *ptr)
{
    free(ptr);
}
void _errno_once(void)
{
    pthread_key_create(&errno_key, _errno_destructor);
}
int *_IBP_errno()
{
    int *output;
    pthread_once(&errno_once, _errno_once);
    output = pthread_getspecific(errno_key);
    if (output == NULL) {
        output = (int *) calloc(1, sizeof(int));
        pthread_setspecific(errno_key, output);
    }
    return (output);
}


#	endif
    
# else  /* windows OS */
     void _set_errno_win(int err)
{
    SetLastError(err);
}
int _get_errno_win()
{
    return (GetLastError());
}


# endif
