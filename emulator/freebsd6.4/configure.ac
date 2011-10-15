dnl
dnl modelnet  configure.ac
dnl
dnl     Use autoconf to detect -ldl, fbsd make, and fbsd kernel sources.
dnl
dnl author David Becker 
dnl Copyright (c) 2003 Duke University  All rights reserved.
dnl See COPYING for license statement.
dnl


# Process this file with autoconf to produce a configure script.

AC_INIT(modelnet, prerelease, modelnet-bugs@some.list)
AC_PREREQ(2.53)
AC_REVISION($Revision: 1.27 $)

AM_INIT_AUTOMAKE(modelnet, 0.99)

AC_CONFIG_SRCDIR([src/allpairs.cc])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL

# Checks for libraries.

AC_CHECK_LIB(dl,dlsym, [ LIBDL=-ldl ], ,)
AC_SUBST(LIBDL)

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h netdb.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([dup2 gethostbyaddr gethostbyname gethostname socket strcasecmp strdup strtoul])

AC_PATH_PROG(DEBIANDOC, debiandoc2text, , $PATH)

DIRS=src

if test  x"$DEBIANDOC" != x 
then DIRS="$DIRS doc"
fi


dnl freebsd sysdir
AC_ARG_WITH(fbsdsys,dnl
[  --with-fbsdsys=DIR      Specify the location of FreeBSD kernel source],,
	test -z "$with_fbsdsys")

case $host_os in
     freebsd*)
     if test -f $srcdir/module/ip_modelnet.c
     then
	     MODULETYPE=fromsource
	     AC_MSG_CHECKING([for FreeBSD kernel source tree])
	     AC_SUBST(MODULETYPE)
	     SYSDIR=/usr/src/sys
	     if test -d "$with_fbsdsys"
	     then SYSDIR=$with_fbsdsys
	     fi
	     if test -d "$SYSDIR"
	     then
	       AC_MSG_RESULT([$SYSDIR])
	       AC_SUBST(SYSDIR)
	     else
	       AC_MSG_WARN([*** FreeBSD kernel source tree not found.  Use --with-fbsdsys=cwDIRPATH ***]) 
	fi
     fi
    AC_PATH_PROG(FBSDMAKE, make, /usr/bin/make, /usr/bin:$PATH)
    DIRS="$DIRS module"
    ;;
esac
dnl end freebsd sysdir


case $host_os in
    freebsd*)
	CPPFLAGS="$CPPFLAGS -I/usr/local/include"
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
	ICONV="-liconv"
	;;
esac

AC_LANG(C++)
AC_CHECK_HEADER(boost/graph/adjacency_list.hpp,,
	[case $host_os in
	freebsd*)
	   AC_MSG_WARN([*** Boost Graph Library not found. ***]) 
	   ac_cv_have_bgl=no
	   ;;
	*) AC_MSG_ERROR([*** Boost Graph Library not found. ***]) ;;
	esac
	]
	)
AC_CHECK_HEADER(xercesc/util/PlatformUtils.hpp,,
	[case $host_os in
	freebsd*)
	   AC_MSG_WARN([*** Xerces XML library not found. ***]) 
	   ac_cv_have_xerces=no
	   ;;
	*) AC_MSG_ERROR([*** Xerces XML library not found. ***]) ;;
	esac
	]
	)

if test "$ac_cv_have_bgl" != 'no' -a  "$ac_cv_have_xerces" != 'no'
then
	XMLBOOSTPROGS="allpairs"
fi

AC_SUBST(DIRS)
AC_SUBST(XMLBOOSTPROGS)
AC_SUBST(ICONV)


AC_CONFIG_FILES([Makefile doc/Makefile module/Makefile \
	debian/Makefile example/Makefile src/Makefile ])
AC_OUTPUT
