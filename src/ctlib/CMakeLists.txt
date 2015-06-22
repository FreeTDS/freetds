add_subdirectory(unittests)

set(static_lib_name ct)
if(WIN32)
	add_definitions(-DDLL_EXPORT)
	set(win_SRCS winmain.c ctlib.def)
	set(static_lib_name libct)
endif()

add_library(ct SHARED
	ct.c cs.c blk.c ctutil.c
	${win_SRCS}
)

target_link_libraries(ct tds replacements ${lib_NETWORK} ${lib_BASE})

add_library(ct-static STATIC
	ct.c cs.c blk.c ctutil.c
)
# See http://www.cmake.org/Wiki/CMake_FAQ#How_do_I_make_my_shared_and_static_libraries_have_the_same_root_name.2C_but_different_suffixes.3F
SET_TARGET_PROPERTIES(ct-static PROPERTIES OUTPUT_NAME ${static_lib_name})
SET_TARGET_PROPERTIES(ct PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(ct-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(ct-static tds replacements ${lib_NETWORK} ${lib_BASE})

if(NOT WIN32)
	set_target_properties(ct PROPERTIES SOVERSION "4.0.0")
endif()

if(MINGW OR CYGWIN)
	set_target_properties(ct PROPERTIES LINK_FLAGS "--static")
endif(MINGW OR CYGWIN)
