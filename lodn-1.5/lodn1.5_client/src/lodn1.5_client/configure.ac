#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(LoDN C Client, 1.5.0.0a, sellers@cs.utk.edu)
AC_CONFIG_SRCDIR([src/lodn_errno.c])
AC_CONFIG_HEADER([include/config.h])

# Checks for programs.
AC_PROG_CC

# Adds auotmake and libtool
AM_INIT_AUTOMAKE
AC_PROG_LIBTOOL

# SSL
AC_ARG_ENABLE(ssl, [--enable-ssl  disable support for SSL],
                USE_SSL=$enableval, USE_SSL=yes)

if [[ x"${USE_SSL}" = "xyes" ]]; then
    AC_DEFINE(USE_SSL,,[SSL/TLS with openssl requested])
    LDFLAGS="$LDFLAGS -lssl -lcrypto"
fi


AM_CONDITIONAL(SSL, test x"${USE_SSL}" = "xyes")



CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE"



# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_mutex_init, LDFLAGS="$LDFLAGS -lpthread",
  LDFLAGS="$LDFLAGS -pthread")
 
# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netdb.h netinet/in.h stdlib.h string.h sys/param.h sys/socket.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([socket strdup])

AC_CONFIG_FILES([Makefile include/Makefile include/lodn_client/Makefile 
                 src/Makefile cmdtools/Makefile])
AC_OUTPUT
