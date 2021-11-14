set(CTEST_OUTPUT_ON_FAILURE ON)

add_executable(ttyrec2ansi ttyrec2ansi.c)
add_executable(get-utf8 get-utf8.c)

file(GLOB tests
	LIST_DIRECTORIES false
	CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/data/*.in)
foreach(t ${tests})
	string(REGEX REPLACE ".*/([^/]*)\\.in$"
		"\\1" t
		${t})
	add_test(NAME ${t}
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh "${t}")
endforeach()
