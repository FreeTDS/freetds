add_subdirectory(unittests)

# TODO checks
add_definitions(-DUNIXODBC -DHAVE_SQLGETPRIVATEPROFILESTRING)

if(PERL_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/src/odbc/odbc_export.h")
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/odbc_export.h"
		COMMAND ${PERL_EXECUTABLE} odbc_export.pl odbc.c > "${CMAKE_CURRENT_BINARY_DIR}/odbc_export.h"
		MAIN_DEPENDENCY odbc_export.pl
		DEPENDS odbc.c
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/error_export.h"
		COMMAND ${PERL_EXECUTABLE} odbc_export.pl error.c > "${CMAKE_CURRENT_BINARY_DIR}/error_export.h"
		MAIN_DEPENDENCY odbc_export.pl
		DEPENDS error.c
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
	add_custom_target(odbc_exports_h DEPENDS
		"${CMAKE_CURRENT_BINARY_DIR}/odbc_export.h"
		"${CMAKE_CURRENT_BINARY_DIR}/error_export.h")
else(PERL_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/src/odbc/odbc_export.h")
	add_custom_target(odbc_exports_h DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/odbc_export.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/error_export.h")
endif(PERL_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/src/odbc/odbc_export.h")

if(WIN32)
	set(win_SRCS winlogin.c winsetup.c winmain.c odbc_w.def setup.rc)
endif()
set(libs ${lib_ODBCINST})

add_library(tdsodbc SHARED
	odbc.c connectparams.c convert_tds2sql.c
	descriptor.c prepare_query.c odbc_util.c bcp.c
	native.c sql2tds.c error.c odbc_checks.c sqlwchar.c sqlwparams.h
	odbc_data.c unixodbc.c ${win_SRCS}
)
target_include_directories(tdsodbc PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(tdsodbc odbc_exports_h encodings_h)
target_compile_definitions(tdsodbc PUBLIC DLL_EXPORT=1)
# TODO libiconv, odbcinstlib and other dynamics
target_link_libraries(tdsodbc tds replacements tdsutils ${libs} ${lib_NETWORK} ${lib_BASE})

if(MINGW OR CYGWIN)
	set_target_properties(tdsodbc PROPERTIES LINK_FLAGS "--static -Wl,--kill-at -Wl,--enable-stdcall-fixup")
endif(MINGW OR CYGWIN)

add_library(tdsodbc_static STATIC
	odbc.c connectparams.c convert_tds2sql.c
	descriptor.c prepare_query.c odbc_util.c bcp.c
	native.c sql2tds.c error.c odbc_checks.c sqlwchar.c sqlwparams.h
	odbc_data.c unixodbc.c ${win_SRCS}
)
target_include_directories(tdsodbc_static PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(tdsodbc_static odbc_exports_h encodings_h)
if (NOT WIN32)
	set_target_properties(tdsodbc_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

INSTALL(TARGETS tdsodbc
	PUBLIC_HEADER DESTINATION include
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
