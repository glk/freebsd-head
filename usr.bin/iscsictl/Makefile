# $FreeBSD$

PROG=		iscsictl
SRCS=		iscsictl.c periphs.c parse.y token.l y.tab.h
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../sys/dev/iscsi
MAN=		iscsi.conf.5 iscsictl.8

LIBADD=		cam util

YFLAGS+=	-v
LFLAGS+=	-i
CLEANFILES=	y.tab.c y.tab.h y.output

WARNS=		6
NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
