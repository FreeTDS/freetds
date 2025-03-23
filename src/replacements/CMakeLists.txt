include_directories(.)

if(PERL_FOUND AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/iconv_charsets.h")
	add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/iconv_charsets.h"
		COMMAND ${PERL_EXECUTABLE} iconv_charsets.pl > "${CMAKE_CURRENT_BINARY_DIR}/iconv_charsets.h"
		MAIN_DEPENDENCY iconv_charsets.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
	add_custom_target(iconv_charsets_h DEPENDS
		"${CMAKE_CURRENT_BINARY_DIR}/iconv_charsets.h")
else(PERL_FOUND AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/iconv_charsets.h")
	add_custom_target(iconv_charsets_h DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/iconv_charsets.h")
endif(PERL_FOUND AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/iconv_charsets.h")

include(CheckFunctionExists)

set(FUNCS asprintf vasprintf strtok_r readpassphrase
strlcpy strlcat basename getopt strsep setenv
poll socketpair getaddrinfo)

set(add_SRCS)

foreach(func ${FUNCS})
	string(TOUPPER "HAVE_${func}" var)
	check_function_exists(${func} ${var})
	if(NOT ${var})
		set(add_SRCS ${add_SRCS} ${func}.c)
	endif()
endforeach(func)

if (WIN32)
	set(add_SRCS ${add_SRCS} gettimeofday.c)
endif()

add_library(replacements STATIC
	iconv.c
	${add_SRCS}
)
target_include_directories(replacements PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
add_dependencies(replacements iconv_charsets_h)
if (NOT WIN32)
	set_target_properties(replacements PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

add_subdirectory(unittests)


INSTALL(TARGETS replacements
	PUBLIC_HEADER DESTINATION include
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
