add_subdirectory(unittests)

if(PERL_FOUND AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tds_willconvert.h")
	if (NOT GPERF)
	    message(FATAL_ERROR "gperf not found!")
	endif()
	# get name of null device
	set(NULLNAME "/dev/null")
	if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
		set(NULLNAME "NUL:")
	endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

	set(ENC_SOURCE_DIR "${CMAKE_SOURCE_DIR}/include/freetds")

	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tds_willconvert.h"
		COMMAND ${PERL_EXECUTABLE} tds_willconvert.pl ../../include/freetds/proto.h > "${CMAKE_CURRENT_BINARY_DIR}/tds_willconvert.h"
		MAIN_DEPENDENCY tds_willconvert.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
	add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/include/freetds/encodings.h"
		COMMAND ${PERL_EXECUTABLE} "${ENC_SOURCE_DIR}/encodings.pl" "${ENC_SOURCE_DIR}" ${GPERF} > encodings.h 2> ${NULLNAME}
		MAIN_DEPENDENCY "${ENC_SOURCE_DIR}/encodings.pl"
		DEPENDS "${ENC_SOURCE_DIR}/alternative_character_sets.h" "${ENC_SOURCE_DIR}/character_sets.h"
		WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/include/freetds")
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/num_limits.h"
		COMMAND ${PERL_EXECUTABLE} num_limits.pl > "${CMAKE_CURRENT_BINARY_DIR}/num_limits.h"
		MAIN_DEPENDENCY num_limits.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/tds_types.h"
		COMMAND ${PERL_EXECUTABLE} types.pl ../../misc/types.csv ../../include/freetds/proto.h > "${CMAKE_CURRENT_BINARY_DIR}/tds_types.h"
		MAIN_DEPENDENCY types.pl
		DEPENDS ../../misc/types.csv ../../include/freetds/proto.h
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
	add_custom_target(encodings_h DEPENDS
		"${CMAKE_CURRENT_BINARY_DIR}/tds_willconvert.h"
		"${CMAKE_CURRENT_BINARY_DIR}/num_limits.h"
		"${CMAKE_CURRENT_BINARY_DIR}/tds_types.h"
		"${CMAKE_BINARY_DIR}/include/freetds/encodings.h")
else(PERL_FOUND AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tds_willconvert.h")
	add_custom_target(encodings_h DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/tds_willconvert.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/num_limits.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/tds_types.h"
		"${CMAKE_SOURCE_DIR}/include/freetds/encodings.h")
endif(PERL_FOUND AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tds_willconvert.h")

if(WIN32)
	set(add_SRCS sspi.c)
else(WIN32)
	set(add_SRCS challenge.c)
endif(WIN32)

add_library(tds STATIC
	mem.c token.c util.c login.c read.c
        write.c convert.c numeric.c config.c query.c iconv.c
        locale.c vstrbuild.c
        getmac.c data.c net.c tls.c
        tds_checks.c log.c
        bulk.c packet.c stream.c random.c
        sec_negotiate_gnutls.h sec_negotiate_openssl.h sec_negotiate.c gssapi.c
	${add_SRCS}
)
target_include_directories(tds PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(tds encodings_h)
if (NOT WIN32)
	set_target_properties(tds PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(WIN32)
INSTALL(TARGETS tds
	PUBLIC_HEADER DESTINATION include
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
endif(WIN32)
