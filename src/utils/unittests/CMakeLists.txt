include_directories(..)

add_library(tds_test_base STATIC
	test_base.c
)

if(NOT WIN32)
	set(unix_TESTS challenge)
endif(NOT WIN32)

foreach(target passarg condition mutex1 dlist bytes smp path ${unix_TESTS})
	add_executable(u_${target} EXCLUDE_FROM_ALL ${target}.c)
	set_target_properties(u_${target} PROPERTIES OUTPUT_NAME ${target})
	target_link_libraries(u_${target} tds_test_base tdsutils ${lib_NETWORK}
			      ${lib_BASE})
	add_test(NAME u_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND u_${target})
	add_dependencies(check u_${target})
endforeach(target)
