# When releasing: remember to update ChangeLog.
AC_INIT([KBtin],[m4_esyscmd(./get_version -n)],[kilobyte@angband.pl])
AM_INIT_AUTOMAKE([-Wall dist-xz no-dist-gzip])
AC_CONFIG_SRCDIR([action.c])
AC_CONFIG_HEADERS([config.h])
# Use kernel-like CC instead of the usual verbose insanity.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_EGREP

# Checks for libraries.

AC_HEADER_SYS_WAIT
AC_HEADER_TIOCGWINSZ
AC_CHECK_HEADERS([strings.h string.h sys/time.h termios.h unistd.h stropts.h \
    pty.h libutil.h util.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_C_BIGENDIAN

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_VPRINTF
AC_SEARCH_LIBS(forkpty, util)
AC_CHECK_FUNCS([memcpy getpt ptsname _getpty grantpt \
	forkpty regcomp getaddrinfo wcwidth posix_openpt cfmakeraw])
AC_FUNC_STRLCPY
AC_ARG_LIB(mccp, zlib, ZLIB, zlib.h, z, gzread, [
Please install zlib and its headers.  On most systems, the package will be named
zlib-dev or libz-dev.  Or, you can "./configure --disable-mccp" to ignore this
issue.  That's kind of a bad idea, though, since MCCP usually greatly reduces
bandwidth-related lag.
Debian/Ubuntu: apt-get install zlib1g-dev
])
AC_ARG_LIB(ssl, gnutls, GNUTLS, gnutls/gnutls.h, gnutls, gnutls_global_init, [
Please install gnutls and its headers.  On most systems, the package will be named
libgnutls-dev or gnutls-dev.  Or, you can "./configure --disable-ssl" to compile
without support for encryption.
Debian/Ubuntu: apt-get install libgnutls-dev
BSD: cd /usr/ports/security/gnutls && make install
])
AM_ICONV
if [[ "x$am_cv_func_iconv" != "xyes" ]]
  then AC_MSG_ERROR([
Alas, iconv is not available.  On most systems it's available as part of
standard libc, yet for some reason you don't have it.  The easiest way to
get it is to install GNU libiconv.
BSD: cd /usr/ports/converters/libiconv && make install
])
fi

AC_PTYRANGES
AC_DEV_PTMX
AC_UCS_SIZE
AC_SCO
AC_SYS_LARGEFILE
AC_C_TRY_FLAG([-Wno-unused-parameter])
AC_C_TRY_FLAG([-Wno-implicit-fallthrough])
AC_DECL_OR_ZERO([O_BINARY], [fcntl.h])

# Enable some warnings.
AC_C_TRY_FLAG([-Wwrite-strings])
AC_C_TRY_FLAG([-Wstrict-prototypes])

AC_CONFIG_FILES([Makefile])

# Initialize the test suite.
AC_CONFIG_TESTDIR(tests)
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AM_MISSING_PROG([AUTOM4TE], [autom4te])

AC_OUTPUT
