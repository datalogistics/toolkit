#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(warmer, 1.0)
AC_CONFIG_AUX_DIR([.])

# Gets the target
AC_CANONICAL_TARGET()

# Initialize automake
AM_INIT_AUTOMAKE()

# Sets the initial prefix value
AC_PREFIX_DEFAULT(/usr/local)



# Gets the command line args
AC_ARG_WITH(lors,
            [  --with-lors      Specify the LoRS installation directory],
            [LORS=${withval}])

AC_ARG_WITH(libxml2,
            [  --with-libxml2      Specify the libxml2 installation directory],
            [LIBXML=${withval}])


AC_ARG_WITH(ibp,
            [  --with-ibp      Specify the ibp installation directory],
            [IBP=${withval}])

AC_ARG_WITH(iconv,
            [  --with-iconv      Specify the iconv library installation directory],
            [ICONV=${withval}])
            
AC_ARG_WITH(warmer-threads,
            [  --with-warmer-threads  Specify the number of warmer threads to use],
            [WARMER_THREADS=${withval}])

AC_ARG_WITH(warmer-period,
            [  --with-warmer-period  Specify how often the warmer runs],
            [WARMER_PERIOD=${withval}])
            
AC_ARG_WITH(warmer-logfile,
            [  --with-warmer-logfile  Specify where the warmer's logfile is],
            [WARMER_LOGFILE=${withval}])
            
AC_ARG_WITH(warmer-verbose,
            [  --with-warmer-verbose  Specify the level of the warmer's verbosity],
            [WARMER_VERBOSE=${withval}])
            
AC_ARG_WITH(phoebus,
            [  --with-phoebus  Specify whether to use phoebus],
            [PHOEBUS=${withval}])



AC_PROG_CC()

# OS Detection
case "$target_os" in
linux*)
    CFLAGS="$CFLAGS -DLINUX"
    ;;
openbsd*)
    CFLAGS="$CFLAGS -DBSD"
    ;;
darwin*)
    CFLAGS="$CFLAGS -DDARWIN"
esac


# Checks for lors
if test "x${LORS}" == "x"; then
  echo "Mush specify installation directory of LoRS with --with-lors"; exit;
fi

# Checks the lors directory
if test  ! -d "${LORS}" ; then
   echo "LoRS installation directory invalid"; exit;
fi

# Sets the lors Tools bin directory
LORS_TOOLS_BIN="${LORS}/bin";
LORS_INCLUDE_DIR="${LORS}/include";
LORS_LIB_DIR="${LORS}/lib";

# Checks the lors bin directory
if test  ! -d "${LORS_TOOLS_BIN}" ; then
   echo "LoRS bin directory invalid"; exit;
fi

# Checks the lors include directory
if test  ! -d "${LORS_INCLUDE_DIR}" ; then
   echo "LoRS include directory invalid"; exit;
fi

# Checks the lors lib directory
if test  ! -d "${LORS_LIB_DIR}" ; then
   echo "LoRS lib directory invalid"; exit;
fi

# Checks for the lors tools
for tool in lors_augment lors_download lors_ls lors_modify lors_refresh lors_trim lors_upload; do
   if test ! -x "${LORS_TOOLS_BIN}/${tool}" -o ! -f "${LORS_TOOLS_BIN}/${tool}"; then
      echo "$tool not found"; exit;
   fi
done

# Checks the libxml2 and attempts to get the include and lib directories for libxml2
if test "x${LIBXML}" !=  "x"; then

   comma=`echo ${LIBXML} | cut -d ',' -f 1-`;

   # If a comma is present then treat the first portion as the include directory and
   # the second portion as the lib directory otherwise just append the include and lib directory
   if test "x${comma}" != "x"; then
       LIBXML_INCLUDE_DIR=`echo ${LIBXML} | cut -d ',' -f1`;
       LIBXML_LIB_DIR=`echo ${LIBXML} | cut -d ',' -f2`;
   else
       LIBXML_INCLUDE_DIR="${LIBXML}/include";
       LIBXML_LIB_DIR="${LIBXML}/lib";
   fi 

   if test ! -d "${LIBXML_LIB_DIR}" -o ! -d "${LIBXML_INCLUDE_DIR}"; then
      echo "libxml2 is not a valid path libxml2"; exit;
   fi

else

   # Attempt to find the the xml-config program to get the the libxml2 configuration data
   AC_PATH_PROG(XML_CONFIG, xml2-config, no)

   # If the xml-config program exists then use it to get the include and lib directory for libxml2
   if test "$XML_CONFIG" != "no"; then
      LIBXML_INCLUDE_DIR=`$XML_CONFIG --cflags`; 
      LIBXML_LIB_DIR=`$XML_CONFIG --libs`; 
   fi
fi



# Checks the prefix value and sets it to the default value if not given
if test "x$prefix" == "xNONE"; then
   prefix=$ac_default_prefix;
fi



# Checks for IBP
if test "x${IBP}" == "x"; then
  IBP=$LORS
fi

# Checks the IBP directory
if test  ! -d "${IBP}" ; then
   echo "IBP installation directory invalid"; exit;
fi         

# Sets the IBP lib and include directories
IBP_INCLUDE_DIR="${IBP}/include";
IBP_LIB_DIR="${IBP}/lib";
IBP_TOOLS_BIN="${IBP}/bin";

# Checks the IBP include directory
if test  ! -d "${IBP_INCLUDE_DIR}" ; then
   echo "IBP include directory invalid"; exit;
fi

# Checks the IBP lib directory
if test  ! -d "${IBP_LIB_DIR}" ; then
   echo "IBP lib directory invalid"; exit;
fi     

# Checks the IBP bin directory
if test  ! -d "${IBP_TOOLS_BIN}" ; then
   echo "IBP bin directory invalid"; exit;
fi     

# Checks for ICONV 
if test "x${ICONV}" != "x"; then

  # Checks the ICONV directory
  if test  ! -d "${ICONV}" ; then
    echo "ICONV installation directory invalid"; exit;
  fi        

  ICONV_LIB_DIR="-Wl,-rpath -Wl,${ICONV} -L${ICONV}";
  ICONV_LIB="$LIBS -liconv"
else
  AC_CHECK_LIB(iconv, iconv_open)
fi

# Assigns a default number of threads if not specified on the command line
if test "x${WARMER_THREADS}" == "x"; then
   WARMER_THREADS=40;
fi


# Checks that the number of threads is indeed a number
WARMER_THREADS=`echo $WARMER_THREADS | grep "^[[0-9]]\{1,\}\$"`;
if test "x${WARMER_THREADS}" == "x"; then
   echo "Number of warmer threads must be a positive integer"; exit;
fi

# Assigns a default warming period if not given
if test "x${WARMER_PERIOD}" == "x"; then
   WARMER_PERIOD="6h";
fi

# Checks that the format of the warmer period is valid
if test x`echo $WARMER_PERIOD | grep -e "^\([[0-9]]\{1,\}d\)\?\([[0-9]]\{1,\}h\)\?\([[0-9]]\{1,\}m\)\?\([[0-9]]\{1,\}s\)\?$"` == "x"; then
   echo "Warmer period not valid must be in days,hours,minutes and seconds"; exit;
fi 

# Handles the warmer log file
if test "x${WARMER_LOGFILE}" != "x"; then

	WARMER_LOGFILE_DIR=""; 
	index=1; 
	last=""; 
	
	# Finds the directory and stores it in $WARMER_LOGFILE_DIR
	while test "x${WARMER_LOGFILE}" != "x${last}"; do 
		WARMER_LOGFILE_DIR=$last; 
		last=`echo $WARMER_LOGFILE | cut -f-$index -d '/'`; 
		index=`expr $index + 1`; 
	done 
	
fi

# Tests the verbose level
if test "x${WARMER_VERBOSE}" != "x"; then

    # Checks that its a number
	if test `echo ${WARMER_VERBOSE} | grep "^[[0-9]]*$"` != "${WARMER_VERBOSE}"; then
		echo "Verbosity must be a number"; exit;	
	fi

else
	WARMER_VERBOSE=0;
fi

# Checks if 
AM_CONDITIONAL([PHOEBUS], [ test "x${PHOEBUS}" != "x" -a "x${PHOEBUS}" != "xno" ])

if test "x${PHOEBUS}" != "x" -a "x${PHOEBUS}" != "xno"; then

    CFLAGS="$CFLAGS -DPHOEBUS"
    
    PHOEBUS_ROUTE_FILE="${prefix}/lodn/etc/phoebus_routefile"
 
    AC_SUBST(PHOEBUS_ROUTE_FILE)
    

fi




LODN_TOOLS="${prefix}/lodn/tools"



echo "LORS_TOOLS=$LORS_TOOLS"
echo "LIBXML_INCLUDE_DIR=$LIBXML_INCLUDE_DIR"
echo "LIBXML_LIB_DIR=$LIBXML_LIB_DIR"

# Sets the substitution values 
AC_SUBST(LORS_TOOLS_BIN)
AC_SUBST(LORS_INCLUDE_DIR)
AC_SUBST(LORS_LIB_DIR)
AC_SUBST(LIBXML_INCLUDE_DIR)
AC_SUBST(LIBXML_LIB_DIR)
AC_SUBST(IBP_INCLUDE_DIR)
AC_SUBST(IBP_LIB_DIR)
AC_SUBST(IBP_TOOLS_BIN)
AC_SUBST(ICONV_LIB_DIR)
AC_SUBST(ICONV_LIB)
AC_SUBST(LODN_TOOLS)
AC_SUBST(WARMER_THREADS)
AC_SUBST(WARMER_PERIOD)
AC_SUBST(WARMER_LOGFILE)
AC_SUBST(WARMER_LOGFILE_DIR)
AC_SUBST(WARMER_VERBOSE)
AC_SUBST(CFLAGS)

AC_CONFIG_FILES([Makefile])

# Configure IBP for the warmer
AC_CONFIG_SUBDIRS([ibp])
AC_CONFIG_SUBDIRS([libexnode])
AC_CONFIG_SUBDIRS([fdr])
AC_CONFIG_SUBDIRS([lbone])

AC_OUTPUT([phoebus.h])

