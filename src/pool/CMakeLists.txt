set(libs ${lib_NETWORK} ${lib_BASE})

add_executable(tdspool main.c config.c member.c user.c util.c)
target_link_libraries(tdspool tdssrv tds replacements tdsutils ${libs})
