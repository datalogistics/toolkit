ibp-install:
	cd ./ibp && make

bindir = @prefix@/bin
libdir = @prefix@/lib

SUBDIRS = client server cgi

install-exec-local:
	cd ibp && make install
#	#@INSTALL@ -m 0644 $(srcdir)/ibp/local/lib/libibp.a @prefix@/lib
#	#@RANLIB@ @prefix@/lib/libibp.a
#	#@INSTALL@ -m 0751 $(srcdir)/ibp/local/bin/ibp-smoketest @prefix@/bin
#	#@INSTALL@ -m 0751 $(srcdir)/ibp/local/bin/ibp-dmtest2 @prefix@/bin
#	#@INSTALL@ -m 0644 $(srcdir)/ibp/local/include/ibp_ClientLib.h @prefix@/include
#	#@INSTALL@ -m 0644 $(srcdir)/ibp/local/include/ibp_protocol.h @prefix@/include
#	#@INSTALL@ -m 0644 $(srcdir)/ibp/local/include/ibp_errno.h @prefix@/include
#	#@INSTALL@ -m 0644 $(srcdir)/ibp/local/include/ibp_os.h @prefix@/include
