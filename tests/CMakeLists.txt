set(CTEST_OUTPUT_ON_FAILURE ON)

add_executable(ttyrec2ansi ttyrec2ansi.cc)
add_executable(get-utf8 get-utf8.cc)

file(GLOB tests
	LIST_DIRECTORIES false
	CONFIGURE_DEPENDS
	${CMAKE_CURRENT_SOURCE_DIR}/data/*.in)
foreach(t ${tests})
	string(REGEX REPLACE ".*/([^/]*)\\.in$"
		"\\1" t
		${t})
	set(tn "${t}")
	if(CMAKE_VERSION VERSION_LESS 3.19)
		string(REPLACE " " "_" tn "${tn}")
		string(REPLACE "#" "♯" tn "${tn}")
		string(REPLACE "(" "⦅" tn "${tn}")
		string(REPLACE ")" "⦆" tn "${tn}")
	endif()
	add_test(NAME ${tn}
		COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/runtest.sh "${t}")
endforeach()

add_executable(kbitr kbitr.cc)
add_test(NAME kbtree_itr COMMAND ${CMAKE_CURRENT_BINARY_DIR}/kbitr)
