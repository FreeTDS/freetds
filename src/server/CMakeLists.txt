add_library(tdssrv STATIC
	query.c
	server.c
	login.c
)
if (NOT WIN32)
	set_target_properties(tdssrv PROPERTIES COMPILE_FLAGS -fPIC)
endif()

if(WIN32)
INSTALL(TARGETS tdssrv
	PUBLIC_HEADER DESTINATION include
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
endif(WIN32)
