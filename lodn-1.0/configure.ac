#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(LoDN, 1.0)
AC_CONFIG_AUX_DIR([.])
AM_INIT_AUTOMAKE()

# Sets the initial prefix value
AC_PREFIX_DEFAULT(/usr/local)

# Gets the command line args
AC_ARG_WITH(perl, 
            [  --with-perl             Specify the path to the perl interpreter],
            [perlpath=$withval],[perlpath=$PATH])

AC_ARG_WITH(java, 
            [  --with-java             Specify the path to the java tools(jar, jarsigner, keytool)],
            [javapath=$withval],[javapath=$PATH])
            
AC_ARG_WITH(imagemagick, 
            [  --with-imagemagick             Specify the path to the imagemagick tools(convert)],
            [imagemagickpath=$withval],[imagemagickpath=$PATH])

AC_ARG_WITH(content-dir,
            [  --with-content-dir      Specify the repository for the exnodes],
            [LODN_CONTENT_DIR=$withval])

AC_ARG_WITH(lors,
            [  --with-lors      Specify the LoRS installation directory],
            [LORS=${withval}])

AC_ARG_WITH(libxml2,
            [  --with-libxml2      Specify the libxml2 installation directory],
            [LIBXML=${withval}])

AC_ARG_WITH(trash,
            [  --with-trash      Specify the directory where deleted exnode go],
            [LODN_TRASH=$withval])

AC_ARG_WITH(url,
            [  --with-url      Specify the complete URL path for lodn],
            [LODN_URL=$withval])

AC_ARG_WITH(email,
            [  --with-email      Specify the email address of the admin for lodn],
            [ADMIN_EMAIL=$withval])

AC_ARG_WITH(keystore,
            [  --with-keystore      Specify the full path to the java keystore that holds the certificate],
            [KEYSTORE=$withval])

AC_ARG_WITH(keystore-passwd,
            [  --with-keystore-passwd      Specify the keystore password],
            [KEYSTORE_PASSWD=$withval])

AC_ARG_WITH(keystore-alias,
            [  --with-keystore-alias      Specify the alias for the keystore entry to sign the jars with],
            [KEYSTORE_ALIAS=$withval])

#AC_ARG_WITH(server-cert,
#            [  --with-server-cert      Specify the full path to the server certificate],
#            [CERT=$withval])

#AC_ARG_WITH(ibp,
#            [  --with-ibp      Specify the ibp installation directory],
#            [IBP=${withval}])

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


AC_PROG_CC()

# Checks for perl
AC_PATH_PROG(PERL_BIN, perl,,[$perlpath])

if test "x$PERL_BIN" == "x"; then
   exit;
fi

PERL="#!$PERL_BIN";



# Checks for the java programs jar, jarsigner and keytool
AC_PATH_PROG(JAR_BIN, jar,,[${javapath}:${javapath}/bin])
AC_PATH_PROG(JARSIGNER_BIN, jarsigner,,[${javapath}:${javapath}/bin])
AC_PATH_PROG(KEYTOOL_BIN, keytool,,[${javapath}:${javapath}/bin])

if test "x$JAR_BIN" == "x"; then
   exit;
fi

if test "x$JARSIGNER_BIN" == "x"; then
   exit;
fi

if test "x$KEYTOOL_BIN" == "x"; then
   exit;
fi

# Checks for lors
if test "x${LORS}" == "x"; then
  echo "Mush specify installation directory of LoRS with --with-lors"; exit;
fi

# Checks for the image magick convert tool
AC_PATH_PROG(CONVERT_BIN, convert,,[${imagemagickpath}:${imagemagickpath}/bin])

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



# Checks the content dir
if test "x$LODN_CONTENT_DIR" == "x"; then
   echo "Must specify a content dir with --with-content-dir"; exit;
fi

# Checks the prefix value and sets it to the default value if not given
if test "x$prefix" == "xNONE"; then
   prefix=$ac_default_prefix;
fi

# Handles the trash dir
if test "x$LODN_TRASH" == "x"; then
   LODN_TRASH="${LODN_CONTENT_DIR}/.trash"
fi

# Tests the lodn URL
if test "x${LODN_URL}" == "x"; then
   echo "Must specify the URL for LoDN with --with-url"; exit;
fi

# Gets the hostname and domain name for the server
FQHN=`echo "$LODN_URL" | cut -d "/" -f3 | cut -d ":" -f1`;
LODN_HOST=`echo "$FQHN" | cut -d "." -f1`;
LODN_DOMAIN=`echo "$FQHN" | cut -d "." -s -f2-`;

if test "x${LODN_DOMAIN}" != "x"; then
   LODN_DOMAIN=".${LODN_DOMAIN}";
fi 

# Gets the CGI bin directory
CGIBIN_DIR=/`echo "$LODN_URL" | cut -d "/" -f4-`/cgi-bin;

if test "x${CGIBIN_DIR}" == "x"; then
   echo "Error configuring cgi-bin directory. Be sure that LODN_URL was specified correctly." exit;
fi

# Tests that the email for the admin was given
if test "x${ADMIN_EMAIL}" == "x"; then
  echo "Must specify the email address of the lodn admin with --with-email"; exit;
fi

# Tests for the server certificate
#if test "x${CERT}" == "x"; then
#   echo "Must specify the server certficate with --with-server-cert.  This is used to sign the jar files"; exit;
#fi

if test "x${KEYSTORE}" == "x"; then
   echo "Must specify the java keystore with --with-keystore.  This is used for signing the jar files"; exit;
fi

if test "x${KEYSTORE_PASSWD}" == "x"; then
   echo "Must specify the keystore password with --with-keystore-passwd.  This is used for signing the jar files"; exit;
fi

if test "x${KEYSTORE_ALIAS}" == "x"; then
   echo "Must specify the keystore alias for the certficate entry with --with-server-cert.  This is used for signing the jar files"; exit;
fi

# Checks for IBP
#if test "x${IBP}" == "x"; then
#  IBP=$LORS
#fi

# Checks the IBP directory
#if test  ! -d "${IBP}" ; then
#   echo "IBP installation directory invalid"; exit;
#fi         

# Sets the IBP lib and include directories
#IBP_INCLUDE_DIR="${IBP}/include";
#IBP_LIB_DIR="${IBP}/lib";
#IBP_TOOLS_BIN="${IBP}/bin";

# Checks the IBP include directory
#if test  ! -d "${IBP_INCLUDE_DIR}" ; then
#   echo "IBP include directory invalid"; exit;
#fi

# Checks the IBP lib directory
#if test  ! -d "${IBP_LIB_DIR}" ; then
#   echo "IBP lib directory invalid"; exit;
#fi     

# Checks the IBP bin directory
#if test  ! -d "${IBP_TOOLS_BIN}" ; then
#   echo "IBP bin directory invalid"; exit;
#fi     

# Checks for ICONV 
if test "x${ICONV}" != "x"; then

  # Checks the ICONV directory
  if test  ! -d "${ICONV}" ; then
    echo "ICONV installation directory invalid"; exit;
  fi        

  LDFLAGS="$LDFLAGS -Wl,-rpath -Wl,${ICONV} -L${ICONV}";
  LIBS="$LIBS -liconv"
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
	if test `echo ${WARMER_VERBOSE} | grep "^[0-9]*$"` != "${WARMER_VERBOSE}"; then
		echo "Verbosity must be a number"; exit;	
	fi

else
	WARMER_VERBOSE=0;
fi


# Names the CGI scripts
CGI_SCRIPTS="control.cgi create_account.cgi directory.cgi gcontrol.cgi gdirectory.cgi glisting.cgi listing.cgi lodn_accounts.cgi lodn_augment.cgi lodn_browser.cgi lodn_create.cgi lodn_delete.cgi lodn_file_status.cgi lodn_genjnlp.cgi lodn_genjnlp_osx.cgi lodn_genurl.cgi lodn_getro.cgi lodn_getxnd.cgi lodn_publish.cgi lodn_published.cgi lodn_readme.cgi lodn_rename_directory.cgi lodn_rename_exnode.cgi lodn_unpublish.cgi lodn_upjnlp.cgi lodn_upjnlp_osx.cgi lodn_upload.cgi lodn_user_delete.cgi lodn_view.cgi lodn_warmer_control.cgi lodn_warmer_depots.cgi lodn_xnd_download.cgi lodn_xnd_upload.cgi login.cgi logout.cgi warmer_info.cgi"

# Names the JAR files
JAR_FILES="Exnode.jar IBP.jar LBone.jar LoDN.jar LoDNClient.jar LoDNHelp.jar LoDNPublisher.jar cli.jar forms-1.0.4.jar jhall.jar"

LODN_PATH="${prefix}/lodn"
LODN_ETC="${LODN_PATH}/etc"
LODN_CFG="\"${LODN_ETC}/lodn.cfg\""
LODN_WEB="${LODN_PATH}/web"
LODN_TOOLS="${LODN_PATH}/tools"

echo "LODN_PATH=${LODN_PATH}"
echo "LODN_ETC=${LODN_ETC}"
echo "LODN_WEB=$LODN_WEB"
echo "LORS_TOOLS=$LORS_TOOLS"
echo "LIBXML_INCLUDE_DIR=$LIBXML_INCLUDE_DIR"
echo "LIBXML_LIB_DIR=$LIBXML_LIB_DIR"
echo "CONTENT_DIR=$LODN_CONTENT_DIR"
echo "LODN_TRASH=$LODN_TRASH"
echo "LODN_URL: $LODN_URL";
echo "LODN_HOST=$LODN_HOST"
echo "LODN_DOMAIN=$LODN_DOMAIN"
echo "ADMIN_EMAIL=$ADMIN_EMAIL"
echo "PERL=$PERL"
echo "PERL_BIN=$PERL_BIN"
echo "CGIBIN_DIR=$CGIBIN_DIR";
#echo "CERTFICATE=$CERT";
echo "KEYSTORE=$KEYSTORE";
echo "KEYSTORE_PASSWD=$KEYSTORE_PASSWD";
echo "KEYSTORE_ALIAS=$KEYSTORE_ALIAS";
echo "$CGI_SCRIPTS"
echo "$JAR_FILES"

# Checks for programs.

# Checks for libraries.

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.


# Sets the substitution values 
AC_SUBST(PERL)
AC_SUBST(PERL_BIN)
AC_SUBST(LODN_PATH)
AC_SUBST(LODN_ETC)
AC_SUBST(LODN_WEB)
AC_SUBST(LODN_TOOLS)
AC_SUBST(LODN_CFG)
AC_SUBST(CGIBIN_DIR)
AC_SUBST(LORS_TOOLS_BIN)
AC_SUBST(LORS_INCLUDE_DIR)
AC_SUBST(LORS_LIB_DIR)
AC_SUBST(LIBXML)
AC_SUBST(LIBXML_INCLUDE_DIR)
AC_SUBST(LIBXML_LIB_DIR)
AC_SUBST(LODN_CONTENT_DIR)
AC_SUBST(LODN_TRASH)
AC_SUBST(LODN_URL)
AC_SUBST(LODN_HOST)
AC_SUBST(LODN_DOMAIN)
AC_SUBST(ADMIN_EMAIL)
AC_SUBST(CGI_SCRIPTS)
AC_SUBST(JAR_FILES)
AC_SUBST(JAR_BIN)
AC_SUBST(JARSIGNER_BIN)
AC_SUBST(KEYTOOL_BIN)
#AC_SUBST(CERT)
AC_SUBST(KEYSTORE)
AC_SUBST(KEYSTORE_PASSWD)
AC_SUBST(KEYSTORE_ALIAS)
#AC_SUBST(IBP_INCLUDE_DIR)
#AC_SUBST(IBP_LIB_DIR)
#AC_SUBST(IBP_TOOLS_BIN)
AC_SUBST(WARMER_THREADS)
AC_SUBST(WARMER_PERIOD)
AC_SUBST(WARMER_LOGFILE)
AC_SUBST(WARMER_LOGFILE_DIR)
AC_SUBST(WARMER_VERBOSE)
AC_SUBST(CONVERT_BIN)

AC_CONFIG_FILES([Makefile cgi/Makefile etc/Makefile jars/Makefile web/Makefile web/doc/Makefile web/images/Makefile tools/Makefile trash/Makefile])

AC_CONFIG_SUBDIRS([warmer])

AC_OUTPUT([web/doc/Publish.htm etc/lodn.cfg cgi/control.cgi cgi/create_account.cgi cgi/directory.cgi cgi/gcontrol.cgi cgi/gdirectory.cgi cgi/glisting.cgi cgi/listing.cgi cgi/lodn_accounts.cgi cgi/lodn_augment.cgi cgi/lodn_browser.cgi cgi/lodn_create.cgi cgi/lodn_delete.cgi cgi/lodn_file_status.cgi cgi/lodn_genjnlp.cgi cgi/lodn_genjnlp_osx.cgi cgi/lodn_genurl.cgi cgi/lodn_getro.cgi cgi/lodn_getxnd.cgi cgi/lodn_publish.cgi cgi/lodn_published.cgi cgi/lodn_readme.cgi cgi/lodn_rename_directory.cgi cgi/lodn_rename_exnode.cgi cgi/lodn_unpublish.cgi cgi/lodn_upjnlp.cgi cgi/lodn_upjnlp_osx.cgi cgi/lodn_upload.cgi cgi/lodn_user_delete.cgi cgi/lodn_view.cgi cgi/lodn_warmer_control.cgi cgi/lodn_warmer_depots.cgi cgi/lodn_xnd_download.cgi cgi/lodn_xnd_upload.cgi cgi/login.cgi cgi/logout.cgi cgi/warmer_info.cgi tools/auth.pl tools/view_account_requests.pl tools/lodn_passwd.pl tools/lodn_useradd.pl tools/lodn_userdel.pl tools/lodn_usermod.pl trash/free_trash.pl warmer/warmerGenerator.sh warmer/checkWarmer.sh])
