foreach(target strings strtok_r strsep)
	add_executable(r_${target} ${target}.c)
	set_target_properties(r_${target} PROPERTIES OUTPUT_NAME ${target})
	target_link_libraries(r_${target} tds_test_base replacements tdsutils
			      ${lib_BASE})
	add_test(NAME r_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND r_${target})
	add_dependencies(check r_${target})
endforeach(target)
