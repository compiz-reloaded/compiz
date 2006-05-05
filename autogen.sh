#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

autoreconf -v --install || exit 1

intltoolize --version > /dev/null 2>&1 || {
    echo "intltoolize not found. Please install intltool";
    exit 1;
}
# work around bgo 323968
ln -s ../po config/po
intltoolize --force --copy --automake || exit 1
rm config/po
# work around another problem with older versions of intltool
sed -e 's/^mkinstalldirs.*/MKINSTALLDIRS=mkdir -p/' po/Makefile.in.in > \
	po/Makefile.in.in.tmp && mv po/Makefile.in.in.tmp po/Makefile.in.in

cd $ORIGDIR || exit $?

$srcdir/configure --enable-maintainer-mode "$@"

