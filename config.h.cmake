#ifndef CONFIG_H
#define CONFIG_H

#define VERSION "@SRCVERSION@"

#cmakedefine HAVE_PTY_H
#cmakedefine HAVE_LIBUTIL_H
#cmakedefine HAVE_UTIL_H
#cmakedefine HAVE_VALGRIND_VALGRIND_H
#cmakedefine HAVE_TERMIOS_H
#cmakedefine HAVE_STROPTS_H
#cmakedefine HAVE_OPENPTY
#cmakedefine HAVE_REGCOMP
#cmakedefine HAVE_GRANTPT
#cmakedefine HAVE_GETPT
#cmakedefine HAVE_PTSNAME
#cmakedefine HAVE_FORKPTY
#cmakedefine HAVE_CFMAKERAW
#cmakedefine HAVE__GETPTY
#cmakedefine HAVE_POSIX_OPENPT
#cmakedefine HAVE_STRLCPY
#cmakedefine HAVE_WCWIDTH
#cmakedefine HAVE_GETENTROPY

#cmakedefine HAVE_ZLIB
#cmakedefine HAVE_GNUTLS
#cmakedefine HAVE_SIMD

#cmakedefine DATA_PATH "@DATA_PATH@"

#endif
