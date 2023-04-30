cmake_minimum_required(VERSION 2.8.12)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.12)
	cmake_policy(SET CMP0075 NEW) # hush the warning, either option works
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.19)
	cmake_policy(SET CMP0110 NEW)
endif()
project(kbtin C)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(FindIconv)
include(FindGnuTLS)
include(GNUInstallDirs)

unset(VERSION)
execute_process(COMMAND ./get_version
	OUTPUT_VARIABLE SRCVERSION
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif (NOT CMAKE_BUILD_TYPE)

option(MCCP "MCCP compression via zlib" ON)
option(SSL "SSL encryption via gnutls" ON)
set(DATA_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/kbtin"
	CACHE PATH "Directory for data (currently help file)")

set(CMAKE_C_FLAGS "-Wall -Werror=format-security ${CMAKE_C_FLAGS}")

set(libs m)

CHECK_INCLUDE_FILES(pty.h HAVE_PTY_H)
CHECK_INCLUDE_FILES(libutil.h HAVE_LIBUTIL_H)
CHECK_INCLUDE_FILES(util.h HAVE_UTIL_H)
CHECK_LIBRARY_EXISTS(util openpty "" HAVE_LIB_UTIL)
if (HAVE_LIB_UTIL)
	set(CMAKE_REQUIRED_LIBRARIES util)
	set(libs ${libs} util)
endif()
CHECK_LIBRARY_EXISTS(hs hs_compile "" HAVE_HS)
if (HAVE_HS)
	set(CMAKE_REQUIRED_LIBRARIES hs)
	set(libs ${libs} hs)
endif()

CHECK_INCLUDE_FILES(valgrind/valgrind.h HAVE_VALGRIND_VALGRIND_H)
CHECK_INCLUDE_FILES(unistd.h HAVE_UNISTD_H)
CHECK_INCLUDE_FILES(termios.h HAVE_TERMIOS_H)
CHECK_INCLUDE_FILES(stropts.h HAVE_STROPTS_H)
CHECK_FUNCTION_EXISTS(openpty HAVE_OPENPTY)
CHECK_FUNCTION_EXISTS(regcomp HAVE_REGCOMP)
CHECK_FUNCTION_EXISTS(grantpt HAVE_GRANTPT)
CHECK_FUNCTION_EXISTS(getpt HAVE_GETPT)
CHECK_FUNCTION_EXISTS(ptsname HAVE_PTSNAME)
CHECK_FUNCTION_EXISTS(forkpty HAVE_FORKPTY)
CHECK_FUNCTION_EXISTS(cfmakeraw HAVE_CFMAKERAW)
CHECK_FUNCTION_EXISTS(_getpty HAVE__GETPTY)
CHECK_FUNCTION_EXISTS(posix_openpt HAVE_POSIX_OPENPT)
CHECK_FUNCTION_EXISTS(strlcpy HAVE_STRLCPY)
CHECK_FUNCTION_EXISTS(wcwidth HAVE_WCWIDTH)

if(Iconv_FOUND)
	include_directories(SYSTEM ${Iconv_INCLUDE_DIRS})
	set(libs ${libs} ${Iconv_LIBRARY})
else()
	message(FATAL_ERROR "libiconv is required")
endif()

if (MCCP)
	CHECK_LIBRARY_EXISTS(z gzread "" HAVE_LIB_ZLIB)
	if (NOT HAVE_LIB_ZLIB)
		message(FATAL_ERROR "MCCP compression requires zlib ('cmake . -DMCCP=off' to disable)")
	endif()
	set(HAVE_ZLIB 1)
	set(libs ${libs} z)
else()
	unset(HAVE_ZLIB)
endif()

if (SSL)
	if (NOT GNUTLS_FOUND)
		message(FATAL_ERROR "SSL encryption requires libgnutls ('cmake . -DSSL=off' to disable")
	endif()
	set(HAVE_GNUTLS 1)
	include_directories(SYSTEM ${GNUTLS_INCLUDE_DIR})
	set(libs ${libs} ${GNUTLS_LIBRARIES})
else()
	unset(HAVE_GNUTLS)
endif()

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)
include_directories(${CMAKE_BINARY_DIR})

set(KBtin_sources
	action.c
	alias.c
	antisub.c
	files.c
	glob.c
	help.c
	highlight.c
	history.c
	eval.c
	main.c
	misc.c
	net.c
	parse.c
	path.c
	session.c
	substitute.c
	ticks.c
	utils.c
	string.c
	events.c
	user.c
	run.c
	colors.c
	routes.c
	bind.c
	telnet.c
	hash.c
	regexp.c
	hooks.c
	unicode.c
	user_pipe.c
	user_tty.c
	wcwidth.c
	chinese.c
	print.c
	ssl.c
	pty.c
	globals.c
	math.c
	slist.c
	lists.c
	vars.c
	tlist.c
	mudcolors.c
)

set(protos ${KBtin_sources})
list(TRANSFORM protos REPLACE "([^;]*)\\.c" "${CMAKE_BINARY_DIR}/protos/\\1.h")
add_custom_command(OUTPUT ${protos}
	COMMAND ${CMAKE_SOURCE_DIR}/make_protos ${CMAKE_SOURCE_DIR} ${KBtin_sources}
	DEPENDS make_protos ${KBtin_sources}
	COMMENT "PROTOS")
add_custom_target(protos DEPENDS ${protos})

add_custom_command(OUTPUT commands.h load_commands.h
	COMMAND ${CMAKE_SOURCE_DIR}/make_commands <${CMAKE_SOURCE_DIR}/commands
	DEPENDS commands make_commands
	COMMENT "COMMANDS")
add_custom_target(gen_commands DEPENDS commands.h load_commands.h)

add_executable(KBtin ${KBtin_sources})
add_dependencies(KBtin protos gen_commands)

target_link_libraries(KBtin ${libs})

add_custom_command(OUTPUT tin2ansi
	DEPENDS tools/tin2ansi.c colors.c
	COMMAND cc ${CMAKE_SOURCE_DIR}/tools/tin2ansi.c ${CMAKE_SOURCE_DIR}/colors.c -o tin2ansi
	COMMENT "tin2ansi")
add_custom_command(OUTPUT manual.html
	DEPENDS KBtin_help tin2ansi a2html fixmanual
	COMMAND ./tin2ansi <${CMAKE_SOURCE_DIR}/KBtin_help|${CMAKE_SOURCE_DIR}/a2html|${CMAKE_SOURCE_DIR}/fixmanual >manual.html
	COMMENT "HTML MANUAL")
add_custom_target(manual ALL DEPENDS manual.html)
add_custom_command(OUTPUT KBtin_help.gz
	DEPENDS KBtin_help
	COMMAND gzip -9 <${CMAKE_SOURCE_DIR}/KBtin_help >KBtin_help.gz
	COMMENT "GZIP HELP")
add_custom_target(helpgz ALL DEPENDS KBtin_help.gz)
add_custom_command(OUTPUT kbtin
	DEPENDS KBtin
	COMMAND ln -s KBtin kbtin
	COMMENT "SYMLINK KBTIN")
add_custom_target(symlink_kbtin ALL DEPENDS kbtin)

install(TARGETS KBtin DESTINATION bin)
install(FILES KBtin.6 kbtin.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
install(FILES ${CMAKE_BINARY_DIR}/KBtin_help.gz DESTINATION ${DATA_PATH})
install(FILES ${CMAKE_BINARY_DIR}/manual.html DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES ${CMAKE_BINARY_DIR}/kbtin DESTINATION bin)

enable_testing()
add_subdirectory(tests)
