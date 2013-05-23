EXEEXT=.cgi
if SERVER
    PG = lbone_list_view lbone_depot_view lbone_depot_modify lbone_depot_delete
else
    PG =
endif
bin_PROGRAMS = $(PG)

IBPINC = @IBPDIR@/include
IBPLIB = @IBPDIR@/lib/libibp.a
LBONELIB = ../../client/src/liblbone.a
FDRLIB = ../../client/src/libfdr.a

CGILIB = @CGIDIR@/libecgi.a
CGIINC = @CGIDIR@/include

AM_CFLAGS = -g -D@OSDEF@ -D_SERVER
AM_CPPFLAGS = -I$(CGIINC) -I$(IBPINC) -I../../client/include -I../../server/include -I../include 

# program name suffix to add ".cgi"

## INCLDIR = $(PWD)/../include
## INCLUDES = -I$(INCLDIR) -I@LDAPDIR@/include -I@CGIDIR@ -I@CGIDIR@/include

lbone_list_view_SOURCES = lbone_list_view.c lbone_base.c lbone_cgi_search.c ngc.c
lbone_list_view_LDADD = $(IBPLIB) $(FDRLIB) $(CGILIB)
lbone_depot_view_SOURCES = lbone_base.c lbone_depot_view.c lbone_poll.c \
                           lbone_cgi_search.c ngc.c
lbone_depot_view_LDADD = $(IBPLIB) $(FDRLIB) $(CGILIB)
lbone_depot_modify_SOURCES = lbone_base.c lbone_depot_modify.c lbone_poll.c \
                             lbone_cgi_search.c ngc.c
lbone_depot_modify_LDADD = $(IBPLIB) $(FDRLIB) $(CGILIB)
lbone_depot_delete_SOURCES = lbone_base.c lbone_depot_delete.c lbone_poll.c \
                             lbone_cgi_search.c ngc.c
lbone_depot_delete_LDADD = $(IBPLIB) $(FDRLIB) $(CGILIB)
