#!/bin/sh
set -e

SRCDIR=$(dirname "$0")

LC_ALL=`./get-utf8`
LC_CTYPE=$LC_ALL
export LC_ALL
export LC_CTYPE
export PATH="$SRCDIR:$PATH"

if which valgrind >/dev/null; then VALGRIND="valgrind -q --error-exitcode=1";fi
$VALGRIND ../KBtin -p -q "$SRCDIR"/setup <"$SRCDIR/data/$1".in|diff -u "$SRCDIR/data/$1".out -
