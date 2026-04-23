add_subdirectory(unittests)

if(WIN32)
	set(add_SRCS "")
else(WIN32)
	set(add_SRCS md4.c des.c hmac_md5.c)
endif(WIN32)

add_library(tdsutils STATIC
	dlist.c
	getpassarg.c
	sleep.c
	tds_cond.c
	threadsafe.c
	bjoern-utf8.c
	tdsstring.c
	strndup.c
	net.c
	smp.c
	path.c
	ascii.c
	md5.c
	xstrdup.c
	${add_SRCS}
)
if (NOT WIN32)
	set_target_properties(tdsutils PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(WIN32)
INSTALL(TARGETS tdsutils
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
endif(WIN32)
