
AC_PREREQ([2.69])
AC_INIT([nss_ndb], [1.0.24-dev], [pen@lysator.liu.se], [nss_ndb], [https://github.com/ptrrkssn/nss_ndb])

AC_CONFIG_SRCDIR([nss_ndb.c])
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_AUX_DIR([aux])

AC_PROG_CC(cc gcc)
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB([dl], [dlsym])
AC_CHECK_LIB([pthread], [pthread_create])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h sys/file.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime endgrent endpwent memchr memset strcasecmp strchr strdup strerror strncasecmp strndup strrchr dbopen])

AC_CONFIG_FILES([Makefile nss_ndb.8 makendb.8 nsstest.8 nss_ndb.conf.5 ports/Makefile.port])

AC_ARG_WITH([realm], AS_HELP_STRING([--with-realm[=NAME]], [Enable realm to strip (yes|no|NAME)]))
case "${with_realm}" in
  yes)
    AC_DEFINE_UNQUOTED([DEFAULT_REALM], [""], [Default realm to strip])
    ;;
  no|"")
    ;;
  *)
    AC_DEFINE_UNQUOTED([DEFAULT_REALM], ["${with_realm}"], [Default realm to strip])
    ;;
esac


AC_ARG_WITH([workgroup], AS_HELP_STRING([--with-workgroup[=NAME]], [Enable workgroup to strip (yes|no|NAME)]))
case "${with_workgroup}" in
  yes)
    AC_DEFINE_UNQUOTED([DEFAULT_WORKGROUP], [""], [Default workgroup to strip])
    ;;
  no|"")
    ;;
  *)
    AC_DEFINE_UNQUOTED([DEFAULT_WORKGROUP], ["${with_workgroup}"], [Default workgroup to strip])
    ;;
esac


AC_ARG_WITH([config-file], AS_HELP_STRING([--with-config-file[=ARG]], [Read configuration from file (yes|no)]))
case "${with_config_file}" in
  yes)
    AC_DEFINE([ENABLE_CONFIG_FILE], [1], [Enable configuration file])
    ;;
  no|"")
    ;;
  *)
    AC_MSG_ERROR([invalid option])
    ;;
esac

AC_ARG_WITH([config-var], AS_HELP_STRING([--with-config-var[=ARG]], [Read config from environment variable (yes|no|NAME)]))
case "${with_config_var}" in
  yes)
    AC_DEFINE_UNQUOTED([NSS_NDB_CONF_VAR], ["NSS_NDB_CONFIG"], [Name of configuration variable])
    ;;
  no|"")
    ;;
  *)
    AC_DEFINE_UNQUOTED([NSS_NDB_CONF_VAR], ["${with_config_var}"], [Name of configuration variable])
    ;;
esac

AC_ARG_WITH([db], AS_HELP_STRING([--with-db[=ARG]], [Build with modern BerkeleyDB (yes|no|VER)]))
case "$with_db" in
  yes)
    db_libs="db-6 db-5 db-4.8 db"
    db_hdrs="db6/db.h db5/db.h db48/db.h db.h"
    ;;
  no|"")
    ;;
  4.8|4)
    db_libs="db-4.8"
    db_hdrs="db48/db.h"
    ;;
  *)
    db_libs="db-${with_db}"
    db_hdrs="db${with_db}/db.h"
    ;;
esac

AS_IF([test "x$db_libs" != "x"], [AC_SEARCH_LIBS([db_create], [$db_libs])])
AS_IF([test "x$db_hdrs" != "x"], [AC_CHECK_HEADERS([$db_hdrs])])

AC_OUTPUT
