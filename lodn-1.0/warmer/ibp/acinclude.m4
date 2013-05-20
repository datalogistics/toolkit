AC_DEFUN([TYPE_SOCKLEN_T],
[AC_CACHE_CHECK([for socklen_t], ac_cv_type_socklen_t,
[
  AC_TRY_COMPILE(
    [#include <sys/types.h>
     #include <sys/socket.h>],
     [socklen_t len = 42; return 0;],
     ac_cv_type_socklen_t=yes,
     ac_cv_type_socklen_t=no)
])
  if test $ac_cv_type_socklen_t != yes; then
     AC_DEFINE([socklen_t], int, [Define to int if socklen_t not found.])
  fi
])

AC_DEFUN([AC_FIND_FILE],
[AC_MSG_CHECKING( for $1)
if test -r $1; then
    AC_MSG_RESULT(found)
    $2
else
    AC_MSG_RESULT(no)
    $3
fi])

AC_DEFUN([AC_ENABLE_AUTHENTICATION],
[AC_ARG_ENABLE(authentication,
changequote(<<,>>)
<< --enable-authentication[=no] enable client authentication [ default=>>no],
changequote([,])
[if test "x$enableval" = "xno"; then
    enable_authentication=no
 else
    enable_authentication=yes
 fi
],
enable_authentication=no)
])


AC_DEFUN([AC_ENABLE_PERSISTENT],
[AC_ARG_ENABLE(persistent,
changequote(<<, >>)
<<   --enable-persistent[=yes]   enable persistent connect  [default=>>yes],
changequote([, ])
[if test "x$enableval" = "xno"; then
   enable_persistent=no
 else
   enable_persistent=yes
 fi
],
enable_persistent=yes)
])

AC_DEFUN([AC_ENABLE_DEBUG],
[AC_ARG_ENABLE(debug,
changequote(<<, >>)
<<   --enable-debug[=no]     enable debug  [default=>>no],
changequote([, ])
[if test "x$enableval" = "xyes"; then
   enable_debug=yes
 else
   enable_debug=no
 fi
],
enable_debug=no)
])

AC_DEFUN([AC_ENABLE_BONJOUR],
[AC_ARG_ENABLE(bonjour,
changequote(<<, >>)
<<   --enable-bonjour[=yes]     enable debug  [default=>>yes],
changequote([, ])
[if test "x$enableval" = "xyes"; then
   enable_bonjour=yes
 else
   enable_bonjour=no
 fi
],
enable_bonjour=yes)
])

AC_DEFUN([AC_ENABLE_IBPCLIENT_ONLY],
[AC_ARG_ENABLE(clientonly,
changequote(<<,>>)
<<   --enable-clientonly[=no]     build ibp client library only [default=>>no],
changequote([,])
[if test "x$enableval" = "xyes"; then
    enable_clientonly=yes
 else
    enable_clientonly=no
 fi
],
enable_clientonly=no)
])


AC_DEFUN([TYPE_INT64_T],
[AC_CACHE_CHECK([for int64_t],ac_cv_type_int64_t,
[
    AC_CHECK_SIZEOF(long,4)
    AC_CHECK_SIZEOF(long long, 8)
    if test "${ac_cv_sizeof_long}" = "8" ; then
        AC_CHECK_TYPE(int64_t, long)
        AC_CHECK_TYPE(u_int64_t, unsigned long)
        ac_cv_type_int64_t=yes
    elif test "${ac_cv_sizeof_long_long}" = "8"; then
        AC_CHECK_TYPE(int64_t, long long)
        AC_CHECK_TYPE(u_int64_t, unsigned long long)
        ac_cv_type_int64_t=yes
    else
        ac_cv_type_int64_t=no
    fi
])

if test "x$ac_cv_type_int64_t" = "xyes"; then
    AC_DEFINE([HAVE_INT64_T], 1, [Define to 1 if you have int64_t.])
fi
])
