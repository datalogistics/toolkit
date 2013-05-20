#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(fdr, 1.0)
AC_CONFIG_SRCDIR([src/dllist.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_CC_C_O

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_INT64_T


# Checks for library functions.
AC_FUNC_MALLOC

AC_CONFIG_FILES([Makefile src/Makefile include/Makefile])

AC_OUTPUT()
