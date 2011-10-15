#
# modelnet  module/Makefile.am
#
# this uses FreeBSD make to build the module and its user space interface
#
# author David Becker 
# Copyright (c) 2003 Duke University  All rights reserved.
# See COPYING for license statement.
#

## Process this file with automake to produce Makefile.in

bin_SCRIPTS = modelload modelmod ipfwload modelstat

EXTRA_DIST = modelload modelmod ipfwload modelstat $(MODULESRC)

MODULESRC = modelnet.ko.mak \
	mkvers ip_modelnet.c ip_modelnet.h  mn_remote.c mn_pathtable.c \
	mn_xtq.c mn_xtq.h mn_xcp.c mn_xcp.h udp_rewrite.c udp_rewrite.h \
        udp_send.c udp_send.h 

FBSDMAKE=/usr/bin/make



all-local: modelnet.ko
install-exec-local: modelnet.ko
	$(mkinstalldirs) $(DESTDIR)$(libdir) $(DESTDIR)$(bindir)
	$(INSTALL) $^ $(DESTDIR)$(libdir)

clean-local:
	rm -f @ machine modelnet modelnet.kld modelnet.ko setdef* *.o

modelnet.ko:  $(MODULETYPE)

fromsource: $(MODULESRC) vers.c
	objdir=`pwd` && \
	cd $(srcdir) && \
	MFLAGS= MAKEFLAGS= MAKEOBJDIR=$$objdir @FBSDMAKE@ SYSDIR=@SYSDIR@ -f modelnet.ko.mak

vers.c: $(MODULESRC)
	$(srcdir)/mkvers > $@

dist-hook:
	if [ -f  modelnet.ko ] ; then cp modelnet.ko $(top_distdir)/module; fi
