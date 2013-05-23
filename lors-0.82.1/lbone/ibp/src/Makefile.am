lib_LTLIBRARIES = libibp.la

libibp_la_SOURCES = ibp_ClientLib.c ibp_ComMod.c ibp_net.c ibp_uri.c ibp_errno.c

if CLIENTONLY
PRGS = 
else
PRGS = ibp_server_mt ibp-slm DM 
endif

bin_PROGRAMS = $(PRGS) ibp-dmtest ibp-test

ibp_server_mt_SOURCES = ibp_server_mt.c ibp_server_lib.c ibp_thread_log.c ibp_vol_allocation.c ibp-lib.c  ibp_unixlib.c ibp_queue.c jrb.c fields.c dlist.c 

ibp_server_mt_LDADD = libibp.la

ibp_slm_SOURCES = ibp-slm.c ibp-lib.c

ibp_slm_LDADD = libibp.la

DM_SOURCES = DM.c dlist.c fields.c jrb.c

DM_LDADD = libibp.la

ibp_test_SOURCES = ibp-smoketest.c 

ibp_test_LDADD = libibp.la

ibp_dmtest_SOURCES = ibp-dmtest2.c dlist.c fields.c jrb.c

ibp_dmtest_LDADD = libibp.la

sampledir = @prefix@/sample
sample_PROGRAM = ibp-dmtest ibp-test

if CLIENTONLY
DIRS = .
else
DIRS = . BLASTER TCP MULTICAST 
endif

SUBDIRS = $(DIRS)
