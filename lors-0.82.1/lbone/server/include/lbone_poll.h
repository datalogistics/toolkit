#include <time.h>
#include "lbone_base.h"

#define DEPOTIDLE               "idle"
#define DEPOT_IDLE              "idle"
#define DEPOTUPDATE             "changing"
#define DEPOT_LOCKED            "locked"
#define DEPOTUNAVAIL            "unavailable"
#define DEPOT_UNAVAIL           "unavailable"

#define IDLE			 0
#define LOCKED			-1
#define UNAVAIL			-2

#define TEMPLATELOST            -1
#define DEPOTLOST               -2
#define BOTHLOST                -3
#define DEPOTCHANGING           -4

