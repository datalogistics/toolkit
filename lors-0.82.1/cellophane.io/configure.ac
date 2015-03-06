#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT(libcellophane, version-0.1, adorwat@indiana.edu)

CLEANFILES="*~ .\#*"
AC_SUBST(CLEANFILES)

AC_CONFIG_MACRO_DIR([config])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR(src/cellophane_io.c)
AC_CONFIG_HEADERS(config.h)
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_PROG_INSTALL

# Checks for header files.
AC_CHECK_HEADERS([netdb.h netinet/in.h stdlib.h string.h sys/socket.h unistd.h wchar.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero gethostbyname memset select socket strdup strstr])

# Checks for libraries.
AC_ARG_WITH(libcurl, [--with-libcurl=path   path to libcurl], ,
            with_libcurl=no)
if test "$with_libcurl" == "no"; then
   AC_CHECK_LIB([curl], [curl_easy_init], ,AC_MSG_ERROR([Can not buiild without libcurl]))
else
   AC_MSG_CHECKING( for libcurl)
   if test -f "$with_libcurl/include/curl/curl.h"; then
   	   AC_MSG_RESULT(yes)
	   AC_SUBST(LIBCURL_INC, "-I$with_libcurl/include")
	   AC_SUBST(LIBCURL_LIB, "-L$with_libcurl/lib -lcurl")
   else
	   AC_MSG_RESULT(no)
	   AC_MSG_ERROR([Can not buiild without libcurl])
   fi
fi


AC_ARG_WITH(libjansson, [--with-libjansson=path   path to libjansson], ,
            with_libjansson=no)
if test "$with_libjansson" == "no"; then
   AC_CHECK_LIB([jansson], [json_dumps], ,AC_MSG_ERROR([Can not buiild without libjansson]))
else
   AC_MSG_CHECKING( for libjansson)
   if test -f "$with_libjansson/include/jansson.h"; then
   	   AC_MSG_RESULT(yes)
	   AC_SUBST(LIBJANSSON_INC, "-I$with_libjansson/include")
	   AC_SUBST(LIBJANSSON_LIB, "-L$with_libjansson/lib -ljansson")
   else
	   AC_MSG_RESULT(no)
	   AC_MSG_ERROR([Can not buiild without libjansson])
   fi
fi


AC_CONFIG_FILES([Makefile
		include/Makefile])
AC_OUTPUT
