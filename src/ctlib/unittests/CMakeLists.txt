add_library(c_common STATIC common.c common.h)

foreach(target t0001 t0002 t0003 t0004
	t0005 cs_convert t0007 t0008
	t0009 connect_fail ct_options
	lang_ct_param array_bind cs_diag
	get_send_data rpc_ct_param rpc_ct_setparam
	ct_diagclient ct_diagserver ct_diagall
	cs_config cancel blk_in
	blk_out ct_cursor ct_cursors
	ct_dynamic blk_in2 data datafmt rpc_fail row_count
	all_types long_binary will_convert
	variant errors ct_command timeout has_for_update)
	add_executable(c_${target} EXCLUDE_FROM_ALL ${target}.c)
	set_target_properties(c_${target} PROPERTIES OUTPUT_NAME ${target})
	if (target STREQUAL "all_types")
		target_link_libraries(c_${target} c_common ct-static
				      t_common tds_test_base tds replacements
				      tdsutils ${lib_NETWORK} ${lib_BASE})
	else()
		target_link_libraries(c_${target} c_common tds_test_base ct
				      replacements tdsutils ${lib_NETWORK}
				      ${lib_BASE})
	endif()
	add_test(NAME c_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND c_${target})
	add_dependencies(check c_${target})
endforeach(target)
