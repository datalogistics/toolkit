if BONJOUR
lib_LTLIBRARIES = libibp.la libmdns.la
else
lib_LTLIBRARIES = libibp.la
endif

libibp_la_SOURCES = ibp_ClientLib.c ibp_ComMod.c ibp_net.c ibp_uri.c ibp_errno.c ibp_nfu.c ibp_authentication.c
libibp_la_LDFLAGS = -release $(VERSION) 

if BONJOUR
libmdns_la_SOURCES = mDNS.c mDNSPosix.c mDNSUNP.c Responder.c
else
libmdns_la_SOURCES = 
endif

if CLIENTONLY
PRGS = 
else
PRGS = ibpd ibp-slm DM makefs readfat
endif

if DEBUG
EFENCE = -lefence
else
EFENCE =
endif

AM_CPPFLAGS = -g -D_LARGEFILE64_SOURCE -D__USE_LARGEFILE64 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE @MDNS_FLAGS@

bin_PROGRAMS = $(PRGS) ibp-dmtest ibp-test

ibpd_SOURCES = ibp_main.c ibp_handle.c ibp_request.c ibp_request_queue.c ibp_thread_pool.c ibp_connection.c ibp_log.c ibp_server_lib.c ibp_vol_allocation.c ibp-lib.c  ibp_unixlib.c ibp_resources.c ibp_ram.c ibp_load.c ibp_manage.c ibp_ct_copy.c ibp_copy.c ibp_nfud.c jrb.c fields.c dlist.c fat.c fs.c ibp_ulfs.c ibp_mDNS.c  ibp_server_auth.c

if BONJOUR
ibpd_LDADD = libibp.la libmdns.la ${EFENCE} 
else
ibpd_LDADD = libibp.la ${EFENCE}
endif


ibp_slm_SOURCES = ibp-slm.c ibp-lib.c

ibp_slm_LDADD = libibp.la

DM_SOURCES = DM.c dlist.c fields.c jrb.c

DM_LDADD = libibp.la

ibp_test_SOURCES = ibp-smoketest.c 

ibp_test_LDADD = libibp.la ${EFENCE}

ibp_dmtest_SOURCES = ibp-dmtest2.c dlist.c fields.c jrb.c

ibp_dmtest_LDADD = libibp.la

makefs_SOURCES = fs.c fat.c makefs.c

readfat_SOURCES = fs.c fat.c readfat.c

sampledir = @prefix@/sample
sample_PROGRAM = ibp-dmtest ibp-test

install-exec-hook:
	cd $(DESTDIR)$(bindir) && \
	mv -f ibpd ibpd-$(VERSION) && \
	$(LN_S) ibpd-$(VERSION) ibpd && \
	$(LN_S) -f ibpd-$(VERSION) ibp_server_mt 

if CLIENTONLY
DIRS = .
else
DIRS = . nfuops BLASTER TCP MULTICAST
endif

SUBDIRS = $(DIRS)
