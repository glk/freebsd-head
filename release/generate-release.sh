#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh [-r revision] svn-branch scratch-dir
#
# Environment variables:
#  CVSUP_HOST: Host of a cvsup server to obtain the ports and documentation
#   trees. This or CVSROOT must be set to include ports and documentation.
#  CVSROOT:    CVS root to obtain the ports and documentation trees. This or
#   CVSUP_HOST must be set to include ports and documentation.
#  CVS_TAG:    CVS tag for ports and documentation (HEAD by default)
#  SVNROOT:    SVN URL to FreeBSD source repository (by default, 
#   svn://svn.freebsd.org/base)
#  MAKE_FLAGS: optional flags to pass to make (e.g. -j)
#  RELSTRING:  optional base name for media images (e.g. FreeBSD-9.0-RC2-amd64)
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $FreeBSD$
#

usage()
{
	echo "Usage: $0 [-r revision] svn-branch scratch-dir"
	exit 1
}

args=`getopt r: $*`
if [ $? -ne 0 ]; then
	usage
fi
set -- $args
REVISION=
while true; do
	case "$1" in
	-r)
		REVISION="-r $2"
		shift; shift
		;;
	--)
		shift; break
		;;
	esac
done

if [ $# -lt 2 ]; then
	usage
fi

set -e # Everything must succeed

case $MAKE_FLAGS in
	*-j*)
		;;
	*)
		MAKE_FLAGS="$MAKE_FLAGS -j "$(sysctl -n hw.ncpu)
		;;
esac

mkdir -p $2/usr/src

svn co ${SVNROOT:-svn://svn.freebsd.org/base}/$1 $2/usr/src $REVISION
if [ ! -z $CVSUP_HOST ]; then
	cat > $2/docports-supfile << EOF
	*default host=$CVSUP_HOST
	*default base=/var/db
	*default prefix=/usr
	*default release=cvs tag=${CVS_TAG:-.}
	*default delete use-rel-suffix
	*default compress
	ports-all
	doc-all
EOF
elif [ ! -z $CVSROOT ]; then
	cd $2/usr
	cvs -R ${CVSARGS} -d ${CVSROOT} co -P -r ${CVS_TAG:-HEAD} ports
	cvs -R ${CVSARGS} -d ${CVSROOT} co -P -r ${CVS_TAG:-HEAD} doc
fi

cd $2/usr/src
make $MAKE_FLAGS buildworld
make installworld distribution DESTDIR=$2
mount -t devfs devfs $2/dev
trap "umount $2/dev" EXIT # Clean up devfs mount on exit

if [ ! -z $CVSUP_HOST ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Checkout ports and doc trees
	chroot $2 /usr/bin/csup /docports-supfile
fi

if [ -d $2/usr/doc ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Build ports to build release documentation
	chroot $2 /bin/sh -c 'pkg_add -r docproj || (cd /usr/ports/textproc/docproj && make install clean BATCH=yes WITHOUT_X11=yes JADETEX=no WITHOUT_PYTHON=yes)'
fi

chroot $2 make -C /usr/src $MAKE_FLAGS buildworld buildkernel
chroot $2 make -C /usr/src/release release
chroot $2 make -C /usr/src/release install DESTDIR=/R

: ${RELSTRING=`chroot $2 uname -s`-`chroot $2 uname -r`-`chroot $2 uname -p`}

cd $2/R
for i in release.iso bootonly.iso memstick; do
	mv $i $RELSTRING-$i
done
sha256 $RELSTRING-* > CHECKSUM.SHA256
md5 $RELSTRING-* > CHECKSUM.MD5

