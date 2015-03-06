#/bin/sh

libtoolize --install
aclocal
autoheader
autoreconf -if
automake --add-missing

