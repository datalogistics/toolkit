noinst_LTLIBRARIES = libtcp.la

libtcp_la_SOURCES = subs.c

bin_PROGRAMS = serverTCP clientTCP mclientTCP smclientTCP pmclientTCP

serverTCP_SOURCES = serverTCP.c 

serverTCP_LDADD = ../libibp.la libtcp.la

clientTCP_SOURCES = clientTCP.c

clientTCP_LDADD = ../libibp.la libtcp.la

mclientTCP_SOURCES = mclientTCP.c 

mclientTCP_LDADD = ../libibp.la libtcp.la

smclientTCP_SOURCES = smclientTCP.c

smclientTCP_LDADD = ../libibp.la libtcp.la

pmclientTCP_SOURCES = pmclientTCP.c

pmclientTCP_LDADD = ../libibp.la libtcp.la

EXTRA_DIST = mylib.h
