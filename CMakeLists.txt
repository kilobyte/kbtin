cmake_minimum_required(VERSION 2.8.12)
cmake_policy(SET CMP0075 OLD) # hush the warning, either option works
project(kbtin C)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckFunctionExists)
include(FindIconv)

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

if(NOT Iconv_FOUND)
	message(FATAL_ERROR "libiconv is required")
endif()

if (MCCP)
	CHECK_LIBRARY_EXISTS(z gzread "" HAVE_LIB_ZLIB)
	if (NOT HAVE_LIB_ZLIB)
		message(FATAL_ERROR "MCCP compression requires zlib")
	endif()
	set(HAVE_ZLIB 1)
	set(libs ${libs} z)
else()
	unset(HAVE_ZLIB)
endif()

if (SSL)
	CHECK_LIBRARY_EXISTS(gnutls gnutls_global_init "" HAVE_LIB_GNUTLS)
	if (NOT HAVE_LIB_GNUTLS)
		message(FATAL_ERROR "SSL encryption requires libgnutls")
	endif()
	set(HAVE_GNUTLS 1)
	set(libs ${libs} gnutls)
else()
	unset(HAVE_GNUTLS)
endif()

configure_file(config.h.cmake ${CMAKE_BINARY_DIR}/config.h)

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
	llist.c
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
)

add_custom_target(protos ALL
    COMMAND ${CMAKE_SOURCE_DIR}/make_protos ${CMAKE_SOURCE_DIR} ${KBtin_sources}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "PROTOS")
set_target_properties(protos
    PROPERTIES ADDITIONAL_CLEAN_FILES protos)

add_custom_target(commands.h ALL
    DEPENDS commands make_commands
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/commands.h ${CMAKE_CURRENT_BINARY_DIR}/load_commands.h
    COMMAND ${CMAKE_SOURCE_DIR}/make_commands <${CMAKE_SOURCE_DIR}/commands
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "COMMANDS")

add_executable(KBtin ${KBtin_sources})
add_dependencies(KBtin protos commands.h)

target_link_libraries(KBtin ${libs})

install(TARGETS KBtin DESTINATION bin)

#enable_testing()
#add_subdirectory(tests)
