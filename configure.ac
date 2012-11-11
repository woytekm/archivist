AC_INIT([archivist], m4_esyscmd([./version.sh | tr -d '\n']), [woytekm@gmail.com])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC

m4_define([myver], m4_esyscmd([./version.sh | tr -d '\n']))
AC_DEFINE(ARCHIVIST_VERSION, ["myver"], [My version number])

AC_ARG_ENABLE([debug],[  --enable-debug   include debug symbol information in exec], [])
AC_ARG_ENABLE([mysql],[  --enable-mysql   enable mysql support], [])

if test "$enable_mysql" = "yes"
 then
  CPPFLAGS="$CPPFLAGS -DUSE_MYSQL"
  CFLAGS="$CFLAGS `mysql_config --cflags`"
  LDFLAGS="$LDFLAGS `mysql_config --libs`" 
 fi

if test "$enable_debug" = "yes"
 then
   CFLAGS="$CFLAGS -g"
 fi

# following APR, APR-util checks were copied from DBSlayer's configure.ac. 

dnl apache run time apr is required
AC_ARG_WITH(apr-1-config,
	[  --with-apr-1-config=PATH   The path to apr-1-config if not in $PATH], [
 	 APRCONFIG=$withval
	], [
	  APRCONFIG=apr-1-config-was-not-found-by-configure
	])
	
if test "x$APRCONFIG" = "xapr-1-config-was-not-found-by-configure"; then
  AC_PATH_PROG( APRCONFIG, apr-1-config, apr-1-config-was-not-found-by-configure,
              $PATH:/usr/bin:/usr/local/bin:/usr/local/apr/bin)
 fi
	
AC_SUBST(APRCONFIG)
	
if test "x$APRCONFIG" = "xapr-1-config-was-not-found-by-configure"; then
  AC_MSG_ERROR([apr-1-config wasn't found please install http://apr.apache.org ])
else
  APR_CFLAGS=`$APRCONFIG --cflags --cppflags`
  APR_INCLUDES=`$APRCONFIG --includes`
  APR_LIBS=`$APRCONFIG --link-ld --libs`
 fi
	
AC_SUBST(APR_CFLAGS)
AC_SUBST(APR_INCLUDES)
AC_SUBST(APR_LIBS)

AC_MSG_CHECKING(for APR CFLAGS)
AC_MSG_RESULT($APR_CFLAGS)
AC_MSG_CHECKING(for APR includes)
AC_MSG_RESULT($APR_INCLUDES)
AC_MSG_CHECKING(for APR libraries)
AC_MSG_RESULT($APR_LIBS)
	
dnl apache run time apr utility is required
AC_ARG_WITH(apu-1-config,
	[  --with-apu-1-config=PATH   The path to apu-1-config if not in $PATH], [
	  APUCONFIG=$withval
	], [
	  APUCONFIG=apu-1-config-was-not-found-by-configure
	])
	
if test "x$APUCONFIG" = "xapu-1-config-was-not-found-by-configure"; then
  AC_PATH_PROG( APUCONFIG, apu-1-config, apu-1-config-was-not-found-by-configure,
              $PATH:/usr/bin:/usr/local/bin:/usr/local/apr/bin)
 fi
	
AC_SUBST(APUCONFIG)
if test "x$APUCONFIG" = "xapu-1-config-was-not-found-by-configure"; then
  AC_MSG_ERROR([apu-1-config wasn't found please install http://apr.apache.org ])
else
  APU_INCLUDES=`$APUCONFIG --includes`
  APU_LIBS=`$APUCONFIG --link-ld --libs`
 fi
AC_SUBST(APU_INCLUDES)
AC_SUBST(APU_LIBS)

AC_MSG_CHECKING(for APR Utility includes)
AC_MSG_RESULT($APU_INCLUDES)
AC_MSG_CHECKING(for APR Utility libraries)
AC_MSG_RESULT($APU_LIBS)

# APR checks end

LDFLAGS="$LDFLAGS $APR_LIBS $APU_LIBS"

CPPFLAGS="$CPPFLAGS -I/usr/include/subversion-1 -I/usr/local/include/subversion-1 $APR_INCLUDES $APU_INCLUDES"

CFLAGS="$CFLAGS $APR_CFLAGS"

AC_PYTHON_DEVEL

CPPFLAGS="$CPPFLAGS $PYTHON_CPPFLAGS"
LDFLAGS="$LDFLAGS $PYTHON_LDFLAGS"
LIBS="$LIBS $PYTHON_LDFLAGS $PYTHON_EXTRA_LDFLAGS $PYTHON_EXTRA_LIBS"

AC_CHECK_HEADERS([sys/procfs.h],[],[])
AC_CHECK_HEADERS([sys/time.h])
AC_CHECK_HEADERS([stdarg.h],[],[])

AC_CHECK_HEADER([regex.h],[],[echo "Cannot find regex.h header file (GNU regex).";exit -1])

AC_CHECK_LIB([kvm],[kvm_open])

AC_CHECK_LIB([nsl],[gethostbyname],[],[])

AC_CHECK_LIB([nsl],[getaddrinfo],[],[])

AC_CHECK_LIB([socket],[getaddrinfo],[],[])

AC_CHECK_LIB([socket],[bind],[],[])

AC_CHECK_LIB([apr-1],[apr_array_make],[],[echo "Error! No libapr-1 found. APR does not seem to be installed...";exit -1])
AC_CHECK_LIB([aprutil-1],[apr_file_open],[],[echo "Error! No libaprutil-1 found. APR does not seem to be installed...";exit -1])

#AC_CHECK_HEADER([svn_client.h],[],[echo "Cannot find Subversion development headers.";exit -1])

AC_CHECK_LIB([svn_client-1],[svn_client_diff4],[],[
    echo "Could not find svn_client_diff4 routine... checking why...";
    AC_CHECK_LIB([svn_client-1],[main],[
    echo "You have libsvn-1 library, but it's probably wrong version - must be greater than 1.6 !";exit -1],[
    echo "It looks like subversion libraries are not installed at all...";exit -1])
    ])

AC_CHECK_LIB([svn_subr-1],[svn_auth_open],[],[echo "Error! No libsvn_subr-1 found. Subversion does not seem to be installed...";exit -1])

AC_CHECK_LIB([snmp],[snmp_sess_session],[],[
            AC_CHECK_LIB([netsnmp],[snmp_sess_session],[],[echo "No net-snmp library found.";exit -1])
            ])

AC_CHECK_LIB([m],[cos],[],[echo "Error! No libm library found.";exit -1])

if test "$enable_mysql" = "yes"
 then
   AC_CHECK_LIB([mysqlclient],[mysql_real_connect],[],[echo "Error! No mysqlclient library found.";exit -1])

   #mysql checks - once again borrowed from dbslayer's configure.ac

   AC_ARG_WITH(mysql-config,[  --with-mysql-config=PATH   The path to mysql-config if not in $PATH], 
                             [MYSQLCONFIG=$withval], [MYSQLCONFIG=mysql-config-was-not-found-by-configure])

   if test "x$MYSQLCONFIG" = "xmysql-config-was-not-found-by-configure"; then
	AC_PATH_PROG( MYSQLCONFIG, mysql_config, mysql_config-was-not-found-by-configure,
                      $PATH:/usr/bin:/usr/local/bin)
   fi

   AC_SUBST(MYSQLCONFIG)
   if test "x$MYSQLCONFIG" = "xmysql_config-was-not-found-by-configure"; then
        AC_MSG_ERROR([mysql_config wasn't found please install http://www.mysql.com ])
   else
        MYSQL_CFLAGS=`$MYSQLCONFIG --cflags`
        MYSQL_INCLUDES=`$MYSQLCONFIG --include`
        MYSQL_LIBS=`$MYSQLCONFIG --libs_r`
   fi

   AC_SUBST(MYSQL_CFLAGS)
   AC_SUBST(MYSQL_INCLUDES)
   AC_SUBST(MYSQL_LIBS)

   AC_MSG_CHECKING(for MySQL CFLAGS)
   AC_MSG_RESULT($MYSQL_CFLAGS)
   AC_MSG_CHECKING(for MySQL includes)
   AC_MSG_RESULT($MYSQL_INCLUDES)
   AC_MSG_CHECKING(for MySQL libraries)
   AC_MSG_RESULT($MYSQL_LIBS)

   LDFLAGS="$LDFLAGS $MYSQL_LIBS"
   CFLAGS="$CFLAGS $MYSQL_CFLAGS"

 fi

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile helpers/Makefile misc/Makefile])
AC_OUTPUT

