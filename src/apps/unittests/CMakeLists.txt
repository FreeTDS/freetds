include_directories(..)

foreach(target defncopy)
	add_executable(a_${target} EXCLUDE_FROM_ALL ${target}.c)
	set_target_properties(a_${target} PROPERTIES OUTPUT_NAME ${target})
	target_link_libraries(a_${target} tds_test_base replacements tdsutils
			      ${lib_NETWORK} ${lib_BASE})
	add_test(NAME a_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND a_${target})
	add_dependencies(check a_${target})
endforeach(target)
