add_subdirectory(unittests)

if(WIN32)
	set(add_SRCS md5.c)
else(WIN32)
	set(add_SRCS md4.c md5.c des.c hmac_md5.c)
endif(WIN32)

add_library(tdsutils STATIC
	getpassarg.c
	sleep.c
	tds_cond.c
	win_mutex.c
	${add_SRCS}
)
if (NOT WIN32)
	set_target_properties(tdsutils PROPERTIES COMPILE_FLAGS -fPIC)
endif()
