#!/bin/sh
set -e

SRCDIR=$(dirname "$0")

LC_ALL=`./get-utf8`
LC_CTYPE=$LC_ALL
export LC_ALL
export LC_CTYPE

../KBtin -p -q setup <"$SRCDIR/data/$1".in|diff -u "$SRCDIR/data/$1".out -
