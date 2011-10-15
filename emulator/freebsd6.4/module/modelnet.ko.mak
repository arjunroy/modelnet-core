#
# modelnet  modelnet.ko.mak
#
#     Use FreeBSD bsd.kmod.mk system to build module
#
# author Kevin Walsh
#
# Copyright (c) 2003 Duke University  All rights reserved.
# See COPYING for license statement.
#

KMOD=modelnet
NOMAN=
SRCS=ip_modelnet.c mn_pathtable.c mn_remote.c udp_send.c udp_rewrite.c \
     mn_xtq.c mn_xcp.c vers.c

CFLAGS+=-I.. -I$(.CURDIR) -DMODELNET -Wall # -Werror

# -pg -GPROF is for profiling and debugging
#CFLAGS+= -pg -DGPROF

# -g is for debugging only
CFLAGS+= -g

# for optimization (O0 to O3)
CFLAGS+= -O3

# use the freebsd make tools for making the module
.include <bsd.kmod.mk>

