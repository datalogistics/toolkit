SUBDIRS = include

lib_LTLIBRARIES = libcellophane.la

ACLOCAL_AMFLAGS = -I config

AM_CFLAGS = -Iinclude @LIBCURL_INC@ @LIBJANSSON_INC@

libcellophane_la_SOURCES = src/cellophane_io.c \
							src/md5.c	\
							src/payload.c

libcellophane_la_LIBADD = @LIBCURL_LIB@ @LIBJANSSON_LIB@







