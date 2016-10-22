#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=$(dirname "$0")
[ -z "$srcdir" ] && srcdir="."
cd "$srcdir"

PKG_NAME="compiz"

if [ ! -f "$srcdir/configure.ac" ]; then
    echo -n "**Error**: Directory "\`"$srcdir"\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
fi

mkdir -p m4
aclocal -I m4 --install || exit 1
autoreconf --verbose --force --install || exit 1
intltoolize --copy --force --automake || exit 1

cd "$OLDPWD" || exit $?
if [ -z "$NOCONFIGURE" ]; then
    "$srcdir/configure" "$@" || exit 1
fi
