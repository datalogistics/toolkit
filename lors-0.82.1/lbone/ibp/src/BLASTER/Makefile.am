noinst_LTLIBRARIES = libudp.la

libudp_la_SOURCES = blastsubs.c

bin_PROGRAMS = blasterd blaster tblaster 

blasterd_SOURCES = blasterd.c 

blasterd_LDADD = ../libibp.la libudp.la  

blaster_SOURCES = blaster.c

blaster_LDADD = ../libibp.la libudp.la

tblaster_SOURCES = tblaster.c

tblaster_LDADD = ../libibp.la libudp.la

EXTRA_DIST = udplib.h
