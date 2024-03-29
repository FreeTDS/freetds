add_subdirectory(unittests)

set(libs ${lib_NETWORK} ${lib_BASE})

foreach(target freebcp bsqldb defncopy datacopy)
	add_executable(${target} ${target}.c)
	target_link_libraries(${target} sybdb replacements tdsutils ${libs})
endforeach(target)


add_executable(tsql tsql.c)
target_link_libraries(tsql tds replacements tdsutils ${lib_READLINE} ${libs})

if(WIN32)
	set(libs odbc32 ${lib_NETWORK} ${lib_BASE})
endif(WIN32)

add_executable(bsqlodbc bsqlodbc.c)
target_link_libraries(bsqlodbc tdsodbc replacements tdsutils ${libs})

INSTALL(TARGETS tsql bsqlodbc defncopy freebcp datacopy bsqldb
	PUBLIC_HEADER DESTINATION include
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
