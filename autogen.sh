#!/bin/sh

libtoolize --install
autoheader
aclocal
automake --foreign --add-missing
autoconf

./configure
