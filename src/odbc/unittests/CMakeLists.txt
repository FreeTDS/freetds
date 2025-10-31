include_directories(..)

set(tests
	t0001 t0002 t0003
	moreresults connect print
	date norowset funccall
	lang_error tables
	binary_test moreandcount
	earlybind putdata params
	raiserror getdata
	transaction type genparams
	preperror prepare_results
	testodbc data error
	rebindpar rpc convert_error
	typeinfo const_params
	insert_speed compute
	timeout array array_out
	cursor1 scroll cursor2
	describecol copydesc
	prepclose warning
	paramcore timeout2 timeout3
	connect2 timeout4 freeclose
	cursor3 cursor4 cursor5
	attributes hidden blob1
	cancel wchar rowset transaction2
	cursor6 cursor7 utf8 utf8_2
	stats descrec peter test64
	prepare_warn long_error mars1
	array_error closestmt bcp
	all_types utf8_3 empty_query
	transaction3 transaction4
	utf8_4 qn connection_string_parse
	tvp tokens
	describeparam
	reexec
)

if(WIN32)
	set(libs odbc32 ${lib_ODBCINST})
else()
	set(libs tdsodbc ${lib_ODBCINST})
endif()

add_library(o_common STATIC common.c common.h c2string.c parser.c parser.h
	fake_thread.c fake_thread.h)

set(static_tests all_types utf8_4 connection_string_parse)
set(unicode_tests utf8 oldpwd)
foreach(target ${tests})
	add_executable(o_${target} EXCLUDE_FROM_ALL ${target}.c)
	set_target_properties(o_${target} PROPERTIES OUTPUT_NAME ${target})
	if (target IN_LIST static_tests)
		target_link_libraries(o_${target} o_common tds_test_base
				      tdsodbc_static t_common tds replacements
				      tdsutils ${lib_ODBCINST} ${lib_NETWORK}
				      ${lib_BASE})
	else()
		target_link_libraries(o_${target} o_common tds_test_base
				      replacements tdsutils ${libs}
				      ${lib_NETWORK} ${lib_BASE})
	endif()
	if (ENABLE_ODBC_WIDE AND NOT target IN_LIST unicode_tests)
		set_property(TARGET o_${target} APPEND PROPERTY COMPILE_DEFINITIONS UNICODE=1 _UNICODE=1_)
	endif()
	if (target STREQUAL "tokens")
		set_property(TARGET o_${target} APPEND PROPERTY LINK_LIBRARIES tdssrv tds)
	endif()
	add_test(NAME o_${target} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} COMMAND o_${target})
	add_dependencies(check o_${target})
endforeach(target)
