set(libs ${lib_NETWORK} ${lib_BASE})

add_executable(tdspool main.c config.c member.c user.c util.c)
target_link_libraries(tdspool tdssrv tds replacements tdsutils ${libs})

INSTALL(TARGETS tdspool
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
