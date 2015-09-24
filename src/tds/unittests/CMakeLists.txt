include_directories(..)

add_library(t_common STATIC common.c common.h utf8.c)

if(NOT WIN32)
	set(unix_TESTS challenge)
endif(NOT WIN32)

foreach(target t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 dynamic1
    convert dataread utf8_1 utf8_2 utf8_3 numeric iconv_fread toodynamic
    readconf collations corrupt ${unix_TESTS})
	add_executable(t_${target} EXCLUDE_FROM_ALL ${target}.c)
	set_target_properties(t_${target} PROPERTIES OUTPUT_NAME ${target})
	target_link_libraries(t_${target} t_common tds replacements ${lib_NETWORK} ${lib_BASE})
	if(NOT ${target} STREQUAL "collations")
		add_test(NAME t_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND ${target})
	endif()
	add_dependencies(check t_${target})
endforeach(target)
