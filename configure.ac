#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([satipclient], [1.0], [])
AM_INIT_AUTOMAKE(foreign)
AC_CONFIG_SRCDIR([timer.h])
AC_CONFIG_HEADERS([_config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for libraries.
AC_CHECK_LIB([pthread], [main])
AC_CHECK_LIB([rt], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([clock_gettime memset setenv socket strchr strdup strerror strstr])

AC_CONFIG_FILES([Makefile])

AC_ARG_WITH(boxtype,
	[  --with-boxtype=NAME box type [[none,dm7025,dm800...]]],
	[BOXTYPE="$withval"],[BOXTYPE="dm800"])
AC_SUBST(BOXTYPE)
AC_DEFINE_UNQUOTED(BOXTYPE,"$BOXTYPE",[box type])

AC_ARG_WITH(machinebuild,
	[  --with-machinebuild=NAME machine build [[none,atemeo5200,atemeo6000...]]],
	[MACHINEBUILD="$withval"],[MACHINEBUILD="atemeo5200"])
AC_SUBST(MACHINEBUILD)
AC_DEFINE_UNQUOTED(MACHINEBUILD,"$MACHINEBUILD",[machine build])

#if test "$BOXTYPE" == "odinm7" -o "$BOXTYPE" == "e3hd"; then
#	AC_DEFINE(HAVE_NO_MSG16, 1,[define then VTUNER use old MSG 1 Mode])
#fi

if test "$BOXTYPE" == "vusolo4k" -o "$BOXTYPE" == "vuuno4k" -o "$BOXTYPE" == "vuuno4kse" -o "$BOXTYPE" == "vuultimo4k" -o "$BOXTYPE" == "vuduo4k" -o "$BOXTYPE" == "vuduo4kse" -o "$BOXTYPE" == "vuzero4k" -o "$BOXTYPE" == "hd51" -o "$BOXTYPE" == "vs1500" -o "$BOXTYPE" == "sf4008" -o "$BOXTYPE" == "gb7252" -o "$BOXTYPE" == "gb72604" -o "$BOXTYPE" == "dags7252" -o "$BOXTYPE" == "xc7439" -o "$BOXTYPE" == "h7" -o "$BOXTYPE" == "8100s" -o "$BOXTYPE" == "osmio4k" -o "$BOXTYPE" == "osmio4kplus" -o "$BOXTYPE" == "osmini4k" -o "$BOXTYPE" == "cc1" -o "$BOXTYPE" == "sf8008" -o "$BOXTYPE" == "sf8008m" -o "$BOXTYPE" == "sf8008opt" -o "$BOXTYPE" == "sx988" -o "$BOXTYPE" == "ip8" -o "$BOXTYPE" == "ustym4kpro" -o "$BOXTYPE" == "ustym4kottpremium" -o "$BOXTYPE" == "ustym4ks2ottx" -o "$BOXTYPE" == "gbmv200" -o "$BOXTYPE" == "beyonwizv2" -o "$machine" == "hzero" -o "$machine" == "h8" -o "$BOXTYPE" == "h9" -o "$BOXTYPE" == "h9se" -o "$BOXTYPE" == "h9combo" -o "$BOXTYPE" == "h9combose" -o "$BOXTYPE" == "h10" -o "$BOXTYPE" == "h11" -o "$BOXTYPE" == "i55plus" -o "$BOXTYPE" == "i55se" -o "$BOXTYPE" == "hd60" -o "$BOXTYPE" == "hd61" -o "$BOXTYPE" == "hd66se" -o "$BOXTYPE" == "pulse4k" -o "$BOXTYPE" == "pulse4kmini" -o "$BOXTYPE" == "multibox" -o "$BOXTYPE" == "multiboxse" -o "$BOXTYPE" == "multiboxpro" -o "$BOXTYPE" == "viper4k" -o "$BOXTYPE" == "u5" -o "$BOXTYPE" == "u5pvr" -o "$BOXTYPE" == "u51" -o "$BOXTYPE" == "u52" -o "$BOXTYPE" == "u53" -o "$BOXTYPE" == "u532" -o "$BOXTYPE" == "u533" -o "$BOXTYPE" == "u54" -o "$BOXTYPE" == "u55" -o "$BOXTYPE" == "u56" -o "$BOXTYPE" == "u57" -o "$BOXTYPE" == "u571" -o "$BOXTYPE" == "u41" -o "$BOXTYPE" == "u42" -o "$BOXTYPE" == "u43" -o "$BOXTYPE" == "u45" -o "$BOXTYPE" == "dagsmv200" -o "$BOXTYPE" == "og2ott4k" -o "$BOXTYPE" == "sfx6008" -o "$machine" == "sx88v2"; then
	AC_DEFINE(VMSG_TYPE2, 1,[define vtuner_message type 2 see vtuner.h])
fi

if test "$BOXTYPE" == "vuduo" -o "$BOXTYPE" == "vusolo" -o "$BOXTYPE" == "vuduo2" -o "$BOXTYPE" == "vusolo2" -o "$BOXTYPE" == "vuuno" -o "$BOXTYPE" == "vuultimo" -o "$BOXTYPE" == "vusolose" -o "$BOXTYPE" == "vuzero" -o "$BOXTYPE" == "vusolo4k" -o "$BOXTYPE" == "vuuno4k" -o "$BOXTYPE" == "vuultimo4k"; then
	AC_DEFINE(VMSG_TYPE1, 1,[define vtuner_message type 1 see vtuner.h])
fi

AC_OUTPUT
