include_directories(.)

if(PERL_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/configure")
	add_custom_command(OUTPUT iconv_charsets.h
		COMMAND ${PERL_EXECUTABLE} iconv_charsets.pl > iconv_charsets.h
		MAIN_DEPENDENCY iconv_charsets.pl
		WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
endif(PERL_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/configure")

include(CheckFunctionExists)

set(FUNCS asprintf vasprintf strtok_r readpassphrase
strlcpy strlcat basename getopt strsep)

set(add_SRCS)

foreach(func ${FUNCS})
	string(TOUPPER "HAVE_${func}" var)
	check_function_exists(${func} ${var})
	if(NOT ${var})
		set(add_SRCS ${add_SRCS} ${func}.c)
	endif()
endforeach(func)

add_library(replacements STATIC
	iconv.c
	gettimeofday.c
	poll.c
	socketpair.c
	getaddrinfo.c
	iconv_charsets.h
	${add_SRCS}
)
if (NOT WIN32)
	set_target_properties(replacements PROPERTIES COMPILE_FLAGS -fPIC)
endif()

add_subdirectory(unittests)


if(WIN32)
INSTALL(TARGETS replacements
	PUBLIC_HEADER DESTINATION include
		RUNTIME DESTINATION bin
		LIBRARY DESTINATION lib
		ARCHIVE DESTINATION lib
	)
endif(WIN32)
