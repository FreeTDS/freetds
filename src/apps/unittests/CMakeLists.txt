include_directories(..)

foreach(target defncopy)
	add_executable(a_${target} EXCLUDE_FROM_ALL ${target}.c)
	# use a different name for test and executable to avoid conflicts
	set_target_properties(a_${target} PROPERTIES OUTPUT_NAME tst_${target})
	target_link_libraries(a_${target} tds_test_base replacements tdsutils
			      ${lib_NETWORK} ${lib_BASE})
	add_test(NAME a_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND a_${target})
	set_tests_properties(a_${target} PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:tsql>;PATH=path_list_prepend:$<TARGET_FILE_DIR:sybdb>")
	add_dependencies(build_tests a_${target})
endforeach(target)
