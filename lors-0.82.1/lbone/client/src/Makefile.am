lib_LIBRARIES = libfdr.a liblbone.a
bin_PROGRAMS = client client_mt slm client_mt2 client_mt3 client_res client_res_nws

libfdr_a_SOURCES = jrb.c jval.c dllist.c fields.c
liblbone_a_SOURCES = lbone_client_lib.c lbone_socket.c

IBPINC = @IBPDIR@/include
IBPLIB = @IBPDIR@/lib/libibp.a

AM_CFLAGS = -g -D@OSDEF@
AM_CPPFLAGS = -I../include -I$(IBPINC)


client_SOURCES = client.c
client_LDADD = liblbone.a libfdr.a $(IBPLIB)

client_mt_SOURCES = client_mt.c
client_mt_LDADD = liblbone.a libfdr.a $(IBPLIB)

client_mt2_SOURCES = client_mt2.c
client_mt2_LDADD = liblbone.a libfdr.a $(IBPLIB)

client_mt3_SOURCES = client_mt3.c
client_mt3_LDADD = liblbone.a libfdr.a $(IBPLIB)

client_res_SOURCES = client_res.c
client_res_LDADD = liblbone.a libfdr.a $(IBPLIB)

client_res_nws_SOURCES = client_res_nws.c
client_res_nws_LDADD = liblbone.a libfdr.a $(IBPLIB)

slm_SOURCES = slm.c
slm_LDADD = liblbone.a libfdr.a $(IBPLIB)
