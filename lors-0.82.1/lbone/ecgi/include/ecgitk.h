/*
   ecgitk.h

   ECgiLib -- Easy CGI Library -- Main header file
   http://www.global-owl.de/ecgi/

   Copyright (C) 2000 by Sven Dawitz <Sven at Dawitz.de>
   based on cgic by Todor Prokopov <koprok@newmail.net>

   Distributed under the GNU General Public License, see the file COPYING for
   more details.
*/

/* not yet finished or dokumented ... */

#include <time.h>

int ctkRedirect(const char *format, ...);
const char *ctkGetSessionID();
const char *ctkTimeToHDate(time_t time);
time_t ctkHDateToTime(const char *hdate);
/*
const char *ctkTimeToHTime(time_t time);
const char *ctkTimeToHDateTime(time_t time);
time_t ctkHTimeToTime(const char *htime);
time_t ctkHDateTimeToTime(const char *hdatetime);
*/