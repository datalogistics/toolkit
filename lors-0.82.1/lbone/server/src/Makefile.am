if SERVER
    PG = lbone_server
else
    PG =
endif
sbin_PROGRAMS = $(PG)


IBPINC = @IBPDIR@/include
IBPLIB = @IBPDIR@/lib/libibp.a
LBONELIB = ../../client/src/liblbone.a
FDRLIB = ../../client/src/libfdr.a

NWSINC = $(NWSDIR)/Include
NWSLIB = $(NWSDIR)/libnws.a

AM_CFLAGS = -g -D@OSDEF@ -D_SERVER
AM_CPPFLAGS = -I$(NWSINC) -I../../client/include -I../include -I$(IBPINC)

lbone_server_SOURCES = lbone_server.c lbone_error.c lbone_nws_query.c lbone_poll.c lbone_search.c ngc.c
lbone_server_LDADD = $(NWSLIB) $(IBPLIB) $(FDRLIB) $(LBONELIB)
