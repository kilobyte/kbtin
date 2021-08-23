#!/bin/sh
set -e

aclocal
autoheader
if [ ! -e config.rpath ]
  then
    cp -p /usr/share/gettext/config.rpath config.rpath ||
    cp -p /usr/local/share/gettext/config.rpath config.rpath ||
    cp -p /data/data/com.termux/files/usr/share/gettext/config.rpath config.rpath
fi
automake --add-missing --copy
autoconf
