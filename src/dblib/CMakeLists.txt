add_subdirectory(unittests)

if(WIN32)
	add_definitions(-DDLL_EXPORT)
	set(win_SRCS winmain.c dblib.def dbopen.c)
endif()

# TODO add dbopen.c if necessary
add_library(sybdb SHARED
	dblib.c dbutil.c rpc.c bcp.c xact.c dbpivot.c buffering.h
	${win_SRCS}
)
target_link_libraries(sybdb tds replacements ${lib_NETWORK} ${lib_BASE})

add_library(db-lib STATIC
	dblib.c dbutil.c rpc.c bcp.c xact.c dbpivot.c buffering.h
)
target_link_libraries(db-lib tds replacements ${lib_NETWORK} ${lib_BASE})

if(NOT WIN32)
	set_target_properties(sybdb PROPERTIES SOVERSION "5.0.0")
endif()

if(MINGW OR CYGWIN)
	set_target_properties(sybdb PROPERTIES LINK_FLAGS "--static")
endif(MINGW OR CYGWIN)
