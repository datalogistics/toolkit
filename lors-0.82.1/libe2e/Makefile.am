lib_LIBRARIES = libend2end.a des/libdes.a md5/libmd5.a
includedir=@prefix@/include/@PACKAGE@
include_HEADERS = libend2end.h md5/md5.h des/des.h

AM_CPPFLAGS = -I. -I./aes -I./des -I./md5 -I@LORS_ZLIB_DIR@/include -D@OSDEF@

libend2end_a_SOURCES = e2e.c libend2end.h
libend2end_a_DEPENDENCIES = des/libdes.a md5/libmd5.a

LDADD = des/libdes.a md5/libmd5.a

des/libdes.a:
	cd des && make libdes.a
md5/libmd5.a:
	cd md5 && make libmd5.a


# this is not needed. automake is smart enought to install it
# and not build it.
# install-data-local:
# 	-@INSTALL@ -m 0644 des/libdes.a @prefix@/lib
# 	-@INSTALL@ -m 0644 md5/libmd5.a @prefix@/lib

clean-generic:
	cd des && make clean
	cd ..
	cd md5 && make clean
