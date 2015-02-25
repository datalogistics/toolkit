#ifndef __LORS_OPTS_H__
#define __LORS_OPTS_H__

#define HAS_OPTION(x,y)  ((x) & (y))

/* getdepotpool */
#define LORS_CHECKDEPOTS            0x00000001
#define LORS_SERVER_AND_LIST        0x00000002
#define LORS_DEPOTSTATUS            0x00000004
/* subext */
#define LORS_RETURN_ON_ANY_ERROR    0x00000008
#define LORS_RETRY_UNTIL_TIMEOUT    0x00000010

/* download */
#define LORS_RECOVER                0x00000020
/* copy*/
#define LORS_COPY                   0x00000040
#define LORS_MCOPY                  0x00000080
#define LORS_BALANCE                0x00000100

/* trim */
/* which */
#define LORS_TRIM_ALL               0x00000200
#define LORS_TRIM_DEAD              0x00000400
/* how */
#define LORS_TRIM_DECR              0x00000800
#define LORS_TRIM_KILL              0x00001000
#define LORS_TRIM_NOKILL            0x00002000

/* list */
#define LORS_LIST_LOGICAL           0x00004000
#define LORS_LIST_PHYSICAL          0x00008000
#define LORS_LIST_HUMAN             0x00010000

/* refresh */
#define LORS_REFRESH_MAX            0x00020000
#define LORS_REFRESH_EXTEND_BY      0x00040000
#define LORS_REFRESH_EXTEND_TO      0x00080000
#define LORS_REFRESH_ABSOLUTE       0x00100000
#define LORS_

/* mapping query */
#define LORS_QUERY_ALL              0x00200000
#define LORS_QUERY_LIVE             0x00400000
#define LORS_QUERY_DEAD             0x00800000
#define LORS_QUERY_REMOVE           0x01000000
#define LORS_

/* depot sort metric */
#define SORT_BY_PROXIMITY           0x02000000
#define SORT_BY_BANDWIDTH           0x04000000

#define LORS_FREE_MAPPINGS          0x08000000

/* set trim IBP caps */
#define LORS_READ_CAP               0x00000001
#define LORS_WRITE_CAP              0x00000002
#define LORS_MANAGE_CAP             0x00000004

#endif
