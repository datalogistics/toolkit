/* $Id: endlessSpinner.c,v 1.1 1998/02/16 18:54:42 jhayes Exp $ */

#include <stdio.h>
#include "spinner.h"

int main() {
  unsigned long u,w;
  while(1) {
    spin(20000, &u, &w);
    fprintf(stderr, "%f\n", (w!=0) ? (float)u/(float)w : 0.0);
  }
}
