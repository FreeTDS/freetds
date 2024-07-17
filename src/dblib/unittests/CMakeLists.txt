add_library(d_common STATIC common.c common.h)

foreach(target t0001 t0002 t0003 t0004 t0005 t0006 t0007 t0008 t0009
	t0011 t0012 t0013 t0014 t0015 t0016 t0017 t0018 t0019 t0020
	dbsafestr t0022 t0023 rpc dbmorecmds bcp thread text_buffer
	done_handling timeout hang null null2 setnull numeric pending
	cancel spid canquery batch_stmt_ins_sel batch_stmt_ins_upd bcp_getl
	empty_rowsets string_bind colinfo bcp2 proc_limit)
	add_executable(d_${target} EXCLUDE_FROM_ALL ${target}.c)
	set_target_properties(d_${target} PROPERTIES OUTPUT_NAME ${target})
	target_link_libraries(d_${target} d_common tds_test_base sybdb
			      replacements tdsutils ${lib_NETWORK} ${lib_BASE})
	add_test(NAME d_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND d_${target})
	add_dependencies(check d_${target})
endforeach(target)
