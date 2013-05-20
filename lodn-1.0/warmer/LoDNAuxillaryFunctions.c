/******************************************************************************
 * Name: LoDNAuxillaryFunctions.c
 * Description: This file implements additional functions that may be necessary
 * 
 * Developer: Christopher Sellers
 ******************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#ifndef _GNU_SOURCE 


/****-------------------------------strndup()------------------------------****
 * Description: This function replaces the normal version of strndup if 
 *              strndup is not available.
 * Input: s - cstring to copy upto n bytes of.
 *        n - holds the max bytes to copy.
 ****----------------------------------------------------------------------****/
char *strndup(const char *s, size_t n)
{
    /* Declarations */
    char *newS;
    int len; 

    /* Length of the input cstring */
    len = strlen(s);

    /* Sets n to the lesser of the length of the string and the intial value of
     * n */
    n = (n < len) ? n : len;

    /* Allocates a new cstring that is large enough to hold the new string */
    if((newS = (char*)malloc((n+1) * sizeof(char))) == NULL)
    {
       return NULL;
    }

    /* Copies the first n bytes of the input string into the new string */
    strncpy(newS, s, n);

    /* Ends the string */
    newS[n] = '\0';

    /* Returns the new string */
    return newS;
}

#endif

