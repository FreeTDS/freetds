cmake_minimum_required(VERSION 3.5)

project(FreeTDS)

option(WITH_OPENSSL        "Link in OpenSSL if found" ON)
option(ENABLE_ODBC_WIDE    "Enable ODBC wide character support" ON)
option(ENABLE_KRB5         "Enable Kerberos support" OFF)
option(ENABLE_ODBC_MARS    "Enable MARS" ON)
option(ENABLE_EXTRA_CHECKS "Enable internal extra checks, DO NOT USE in production" OFF)
option(ENABLE_MSDBLIB      "Enable MS style dblib" OFF)

if(COMMAND cmake_policy)
	cmake_policy(SET CMP0003 NEW)
	cmake_policy(SET CMP0005 NEW)
endif(COMMAND cmake_policy)

set(PACKAGE freetds)
unset(VERSION)
if(EXISTS "${CMAKE_SOURCE_DIR}/configure")
	file(STRINGS "${CMAKE_SOURCE_DIR}/configure" VER LIMIT_COUNT 1 REGEX "^[ 	]*VERSION[ 	]*=.*1[.]")
	if(VER MATCHES "VERSION[ 	]*=[ 	]*'?([^' 	]+)")
		set(VERSION ${CMAKE_MATCH_1})
	endif()
else()
	file(STRINGS "${CMAKE_SOURCE_DIR}/configure.ac" VER LIMIT_COUNT 1 REGEX "^AC_INIT\\(FreeTDS,[ 	]*1[.].*\\)$")
	if(VER MATCHES "AC_INIT\\(FreeTDS,[ 	]*(.+)\\)$")
		set(VERSION ${CMAKE_MATCH_1})
	endif()
endif()
if(DEFINED VERSION)
# versions to test
#	set(VERSION "0.92.dev.esyscmd(printf $(date +\"%Y%m%d\"))")
#	set(VERSION "0.92.dev.esyscmd(printf $(date +\"%y%m%d\"))") # FAIL
#	set(VERSION "0.95rc1")
#	set(VERSION "0.95")
#	set(VERSION "0.95.1")
	string(TIMESTAMP DATE "%Y%m%d")
	string(REPLACE ".esyscmd(printf $(date +\"%Y%m%d\"))" ".${DATE}" VERSION ${VERSION})
	string(REGEX REPLACE "^dev\\.([0-9]+\\.[0-9]+)\\." "\\1.dev." VERSION "${VERSION}")
	string(REGEX REPLACE "^([0-9]+\\.[0-9]+)(rc|RC)" "\\1.dev." VER "${VERSION}")
	string(REPLACE ".dev." ".9999." VER ${VER})
	if(VER MATCHES "[^0-9.]" OR VER MATCHES "\\.\\.")
		message(FATAL_ERROR "wrong VERSION format (${VERSION}) --> (${VER})")
	endif()
	if("${VER}.0" MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)")
		set(MAJOR ${CMAKE_MATCH_1})
		set(MINOR ${CMAKE_MATCH_2})
		set(SUBVERSION ${CMAKE_MATCH_3})
		if(SUBVERSION STREQUAL "9999")
			math(EXPR MINOR "${MINOR} - 1")
		endif()
	else()
		message(FATAL_ERROR "wrong VERSION format (${VERSION}) --> (${VER})")
	endif()
#	message(FATAL_ERROR "DEBUG ${MAJOR} ${MINOR} ${SUBVERSION}")
else()
	message(FATAL_ERROR "VERSION not found or wrong format")
endif()

# get build number from date
# use same formula used in configure.ac
unset(BUILD_NUMBER)
string(TIMESTAMP BUILD_NUMBER "( %Y - 2025 ) * 366 + 1%j - 1000")
math(EXPR BUILD_NUMBER "${BUILD_NUMBER}")

set(FREETDS_TOPDIR ${CMAKE_CURRENT_LIST_DIR})

enable_testing()
set(CMAKE_CTEST_COMMAND ctest)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

# TODO depends on configure
add_definitions(-D_REENTRANT -D_THREAD_SAFE)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckTypeSize)
include(CheckFunctionExists)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(CheckPrototypeDefinition)
include(GNUInstallDirs)

find_package(Perl)
find_program(GPERF NAMES gperf)

macro(config_write str)
	file(APPEND "${CMAKE_BINARY_DIR}/include/config.h.in" "${str}")
endmacro()

file(WRITE "${CMAKE_BINARY_DIR}/include/config.h.in" "/* Automatic generated by cmake */\n\n#define _freetds_config_h_\n")

if(NOT WIN32)
	file(WRITE "${CMAKE_BINARY_DIR}/include/freetds/sysconfdir.h"
		"#define FREETDS_SYSCONFDIR \"${CMAKE_INSTALL_FULL_SYSCONFDIR}\"\n")
endif(NOT WIN32)

foreach(fn
	arpa/inet.h
	com_err.h
	dirent.h
	errno.h
	getopt.h
	inttypes.h
	langinfo.h
	libgen.h
	limits.h
	locale.h
	malloc.h
	netdb.h
	netinet/in.h
	netinet/tcp.h
	paths.h
	poll.h
	roken.h
	signal.h
	sql.h
	sqlext.h
	stddef.h
	stdlib.h
	stdint.h
	string.h
	strings.h
	sys/eventfd.h
	sys/ioctl.h
	sys/param.h
	sys/resource.h
	sys/select.h
	sys/socket.h
	sys/stat.h
	sys/time.h
	sys/types.h
	sys/wait.h
	unistd.h
	fcntl.h
	wchar.h
	windows.h)

	string(REGEX REPLACE "[/.]" "_" var "${fn}")
	string(TOUPPER "HAVE_${var}" var)
	CHECK_INCLUDE_FILE(${fn} ${var})
	config_write("/* Define to 1 if you have the <${fn}> header file. */\n")
	config_write("#cmakedefine ${var} 1\n\n")
endforeach(fn)

macro(CHECK_FUNCTION_EXISTS_DEFINE func)
	string(TOUPPER "HAVE_${func}" var)
	check_function_exists(${func} ${var})
	config_write("/* Define to 1 if you have function <${func}>. */\n")
	config_write("#cmakedefine ${var} 1\n\n")
endmacro()

if(WITH_OPENSSL)
	find_package(OpenSSL)
endif(WITH_OPENSSL)
if(OPENSSL_FOUND)
	config_write("#define HAVE_OPENSSL 1\n\n")
	include_directories(${OPENSSL_INCLUDE_DIR})
	set(CMAKE_REQUIRED_LIBRARIES ${OPENSSL_LIBRARIES})
	# This fixes an issue with OpenSSL compiled as static library
	if(WIN32)
		list(APPEND CMAKE_REQUIRED_LIBRARIES crypt32 ws2_32)
	endif(WIN32)
	check_function_exists_define(BIO_get_data)
	check_function_exists_define(RSA_get0_key)
	check_function_exists_define(ASN1_STRING_get0_data)
	check_function_exists_define(SSL_set_alpn_protos)
	set(CMAKE_REQUIRED_LIBRARIES)
endif(OPENSSL_FOUND)

set(CMAKE_THREAD_PREFER_PTHREAD ON)
find_package(Threads REQUIRED)

if(HAVE_WINDOWS_H)
	if(NOT HAVE_SQL_H)
		unset(HAVE_SQL_H CACHE)
		CHECK_INCLUDE_FILES("windows.h;sql.h" HAVE_SQL_H)
	endif()
	if(HAVE_SQL_H AND NOT HAVE_SQLEXT_H)
		unset(HAVE_SQLEXT_H CACHE)
		CHECK_INCLUDE_FILES("windows.h;sql.h;sqlext.h" HAVE_SQLEXT_H)
	endif()
	if(NOT HAVE_ODBCSS_H)
		unset(HAVE_ODBCSS_H CACHE)
		CHECK_INCLUDE_FILES("windows.h;sqltypes.h;odbcss.h" HAVE_ODBCSS_H)
	endif()
endif(HAVE_WINDOWS_H)

# find types
macro(ADD_HEADER VAR FN)
	if(${VAR})
		set(CMAKE_EXTRA_INCLUDE_FILES ${CMAKE_EXTRA_INCLUDE_FILES} ${FN})
	endif()
endmacro(ADD_HEADER)

add_header(HAVE_WCHAR_H wchar.h)
add_header(HAVE_WINDOWS_H windows.h)
add_header(HAVE_SQL_H sql.h)
add_header(HAVE_SQLEXT_H sqlext.h)
foreach(fn char double float int short long "long double" "long long" "SQLWCHAR" "void *" wchar_t __int64 __int128
	  SQLLEN SQLROWOFFSET SQLROWSETSIZE SQLSETPOSIROW)
	string(REGEX REPLACE "[/. ]" "_" var "${fn}")
	string(REPLACE "*" "P" var "${var}")
	string(TOUPPER "HAVE_${var}" have)
	string(TOUPPER "SIZEOF_${var}" var)
	CHECK_TYPE_SIZE(${fn} ${var})
	if(HAVE_${var})
		set(${have} 1 CACHE INTERNAL "Have type ${fn}")
	else()
		set(${var} 0)
	endif()
	config_write("#define ${var} \\@${var}\\@\n\n")
endforeach(fn)

foreach(fn SQLLEN SQLROWOFFSET SQLROWSETSIZE SQLSETPOSIROW)
	config_write("/* Define to 1 if you have type <${fn}>. */\n#cmakedefine HAVE_${fn} 1\n\n")
endforeach(fn)

check_prototype_definition(SQLColAttribute
"SQLRETURN  SQL_API SQLColAttribute (SQLHSTMT StatementHandle,
	SQLUSMALLINT ColumnNumber, SQLUSMALLINT FieldIdentifier,
	SQLPOINTER CharacterAttribute, SQLSMALLINT BufferLength,
	SQLSMALLINT *StringLength, SQLLEN * NumericAttribute)"
	SQL_SUCCESS
	"${CMAKE_EXTRA_INCLUDE_FILES}"
	TDS_SQLCOLATTRIBUTE_SQLLEN)
config_write("/* Define to 1 if last argument of SQLColAttribute it's SQLLEN * */\n#cmakedefine TDS_SQLCOLATTRIBUTE_SQLLEN 1\n\n")

check_prototype_definition(SQLParamOptions
	"SQLRETURN SQL_API SQLParamOptions(SQLHSTMT hstmt, SQLULEN crow, SQLULEN *pirow)"
	SQL_SUCCESS
	"${CMAKE_EXTRA_INCLUDE_FILES}"
	TDS_SQLPARAMOPTIONS_SQLLEN)
config_write("/* Define to 1 if SQLParamOptions accept SQLULEN as arguments */\n#cmakedefine TDS_SQLPARAMOPTIONS_SQLLEN 1\n\n")

set(CMAKE_EXTRA_INCLUDE_FILES)

macro(SELECT_TYPE VAR SIZE TYPES ERROR)
	set(${VAR} "")
	foreach(t ${TYPES})
		string(REGEX REPLACE "[/. ]" "_" var "${t}")
		string(REPLACE "*" "P" var "${var}")
		string(TOUPPER "SIZEOF_${var}" var)
		if(${${var}} EQUAL ${SIZE})
			set(${VAR} ${t})
			break()
		endif()
	endforeach(t)
	if(${VAR} STREQUAL "")
		message(FATAL_ERROR ${ERROR})
	endif()
endmacro(SELECT_TYPE)

SELECT_TYPE(tds_sysdep_int16_type 2 "short;int" "No 16-bit int found.")
SELECT_TYPE(tds_sysdep_int32_type 4 "short;int;long" "No 32-bit int found.")
SELECT_TYPE(tds_sysdep_int64_type 8 "long;long long;__int64" "No 64-bit int found.")
SELECT_TYPE(tds_sysdep_real32_type 4 "float;double" "No 32-bit real found.")
SELECT_TYPE(tds_sysdep_real64_type 8 "float;double;long double" "No 64-bit real found.")
SELECT_TYPE(tds_sysdep_intptr_type ${SIZEOF_VOID_P} "short;int;long;__int64" "No intptr type found.")

# find functions
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_THREAD_LIBS_INIT})
set(FUNCS asprintf vasprintf strtok_r strtok_s readpassphrase
	strlcpy strlcat basename getopt strsep
	vsnprintf _vsnprintf snprintf _snprintf _vscprintf gettimeofday
	nl_langinfo locale_charset setenv putenv
	getuid getpwuid getpwuid_r fstat alarm fork
	gethrtime localtime_r setitimer
	_fseeki64 _ftelli64 setrlimit
	inet_ntoa_r getipnodebyaddr getipnodebyname
	getaddrinfo inet_ntop gethostname poll socketpair
	clock_gettime fseeko pthread_cond_timedwait pthread_cond_timedwait_relative_np
	pthread_condattr_setclock _lock_file _unlock_file usleep nanosleep
	readdir_r eventfd daemon system mallinfo mallinfo2 _heapwalk)

# TODO
set(HAVE_GETADDRINFO 1 CACHE INTERNAL "")

foreach(func ${FUNCS})
	check_function_exists_define(${func})
endforeach(func)

macro(CHECK_STDIO_FUNCTION_EXISTS FUNCTION)
	string(TOUPPER "HAVE_${FUNCTION}" VARIABLE)
	string(TOUPPER "CHECK_${FUNCTION}" CHECK)
	if (NOT ${VARIABLE})
		set(MACRO_CHECK_FUNCTION_DEFINITIONS
			"-D${CHECK}=1 ${CMAKE_REQUIRED_FLAGS}")
		message(STATUS "Looking for ${FUNCTION}")
		try_compile(${VARIABLE}
			${CMAKE_BINARY_DIR}
			${CMAKE_SOURCE_DIR}/misc/cmake_checks.c
			COMPILE_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS}
			CMAKE_FLAGS -DCOMPILE_DEFINITIONS:STRING=${MACRO_CHECK_FUNCTION_DEFINITIONS}
			OUTPUT_VARIABLE OUTPUT)
		if(${VARIABLE})
			set(${VARIABLE} 1 CACHE INTERNAL "Have function ${FUNCTION}")
			message(STATUS "Looking for ${FUNCTION} - found")
			file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeOutput.log
				"Determining if the function ${FUNCTION} exists passed with the following output:\n"
				"${OUTPUT}\n\n")
		else()
			message(STATUS "Looking for ${FUNCTION} - not found")
			set(${VARIABLE} "" CACHE INTERNAL "Have function ${FUNCTION}")
			file(APPEND ${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeError.log
				"Determining if the function ${FUNCTION} exists failed with the following output:\n"
				"${OUTPUT}\n\n")
		endif()
	endif()
endmacro()

foreach(func asprintf vasprintf vsnprintf _vsnprintf snprintf _snprintf _vscprintf)
	check_stdio_function_exists(${func})
endforeach(func)

set(CMAKE_REQUIRED_LIBRARIES)

check_struct_has_member("struct tm" "tm_zone" "time.h" HAVE_STRUCT_TM_TM_ZONE)
config_write("#cmakedefine HAVE_STRUCT_TM_TM_ZONE 1\n\n")

macro(SEARCH_LIBRARY FUNC HAVE VAR LIBS)
	foreach(lib ${LIBS})
		if(NOT ${HAVE})
			unset(${HAVE} CACHE)
			check_library_exists(${lib} ${FUNC} "" ${HAVE})
			if(${HAVE})
				unset(${VAR} CACHE)
				set(${VAR} ${lib} CACHE INTERNAL "")
			endif()
		endif()
	endforeach(lib)
endmacro(SEARCH_LIBRARY)

check_library_exists(readline readline "" HAVE_READLINE)
if(HAVE_READLINE)
	config_write("#cmakedefine HAVE_READLINE 1\n\n")
	search_library(tgetent HAVE_TGETENT lib_READLINE "readline;ncurses;curses;termcap")
	set(lib_READLINE readline ${lib_READLINE})
endif(HAVE_READLINE)

config_write("#cmakedefine HAVE_SQLGETPRIVATEPROFILESTRING 1\n\n")
if(WIN32)
	set(lib_ODBCINST odbccp32)
	check_library_exists(legacy_stdio_definitions _vsnwprintf_s "" LEGACY_STDIO)
	if(LEGACY_STDIO)
		set(lib_ODBCINST ${lib_ODBCINST} legacy_stdio_definitions)
	endif(LEGACY_STDIO)
else()
	search_library(SQLGetPrivateProfileString HAVE_SQLGETPRIVATEPROFILESTRING lib_ODBCINST "odbcinst;iodbcinst")
endif()

# flags
foreach(flag ODBC_WIDE EXTRA_CHECKS KRB5 ODBC_MARS)
	config_write("#cmakedefine ENABLE_${flag} 1\n\n")
endforeach(flag)

if (ENABLE_MSDBLIB)
	set(dblib_define "#define MSDBLIB 1")
else()
	set(dblib_define "#define SYBDBLIB 1")
endif()

config_write("#define VERSION \"\\@VERSION\\@\"\n\n")

config_write("#define FREETDS_TOPDIR \"${FREETDS_TOPDIR}\"\n\n")

# TODO allow to configure default TDS protocol version

config_write("#if defined(_MSC_VER) && !defined(__cplusplus)
#  define inline __inline
#endif
#if defined(_MSC_VER)
#  define _CRT_SECURE_NO_WARNINGS 1
#endif\n\n")

if(WIN32)
	config_write("/* Define to 1 if you want to use SSPI for Win32 */\n#define HAVE_SSPI 1\n\n")
	config_write("/* define to format string used for 64bit integers */\n#define TDS_I64_PREFIX \"I64\"\n\n")
	config_write("/* Define to 1 if you have the SQLGetPrivateProfileString function. */\n#define HAVE_SQLGETPRIVATEPROFILESTRING 1\n\n")
	if(MINGW)
		config_write("#define _GNU_SOURCE 1\n\n")
	endif(MINGW)

else(WIN32)
	# this section make just work on Linux, many definition are not checked
	config_write("#define HAVE_FUNC_GETPWUID_R_5 1\n\n")
	config_write("#define HAVE_FUNC_LOCALTIME_R_TM 1\n\n")
	config_write("/* define to format string used for 64bit integers */\n#define TDS_I64_PREFIX \"ll\"\n\n")
	config_write("#define UNIXODBC 1\n\n#define _GNU_SOURCE 1\n\n")

	if(NOT OPENSSL_FOUND)
		include(FindGnuTLS)
		if(GNUTLS_FOUND)
			config_write("#define HAVE_GNUTLS 1\n")
			set(CMAKE_REQUIRED_LIBRARIES ${GNUTLS_LIBRARIES})
			check_function_exists_define(gnutls_certificate_set_verify_function)
			check_function_exists_define(gnutls_record_disable_padding)
			check_function_exists_define(gnutls_rnd)
			check_function_exists_define(gnutls_alpn_set_protocols)
			check_function_exists_define(gnutls_set_default_priority_append)
			set(CMAKE_REQUIRED_LIBRARIES)
			pkg_check_modules(NETTLEDEP gnutls)
			if (";${NETTLEDEP_STATIC_LIBRARIES};" MATCHES ";nettle;")
				set(lib_GNUTLS ${GNUTLS_LIBRARIES})
				config_write("#define GNUTLS_USE_NETTLE 1\n\n")
				pkg_check_modules(NETTLE nettle)
				if(NETTLE_FOUND)
					set(lib_GNUTLS ${GNUTLS_LIBRARIES} ${NETTLE_LIBRARIES})
					config_write("#define HAVE_NETTLE 1\n\n")
					if (";${NETTLEDEP_STATIC_LIBRARIES};" MATCHES ";hogweed;")
						set(lib_GNUTLS ${lib_GNUTLS} hogweed)
					endif()
					if (";${NETTLEDEP_STATIC_LIBRARIES};" MATCHES ";gmp;")
						set(lib_GNUTLS ${lib_GNUTLS} gmp)
						config_write("#define HAVE_GMP 1\n\n")
					endif()
				endif(NETTLE_FOUND)
			else()
				set(lib_GNUTLS ${GNUTLS_LIBRARIES} gcrypt tasn1)
			endif()
		endif(GNUTLS_FOUND)
	endif(NOT OPENSSL_FOUND)

	if(NOT HAVE_CLOCK_GETTIME AND NOT HAVE_GETHRTIME)
		search_library(clock_gettime HAVE_CLOCK_GETTIME lib_RT "rt;posix4")
		config_write("#cmakedefine HAVE_CLOCK_GETTIME 1\n\n")
	endif(NOT HAVE_CLOCK_GETTIME AND NOT HAVE_GETHRTIME)

	if(CMAKE_USE_PTHREADS_INIT)
		config_write("#define HAVE_PTHREAD 1\n\n")
		config_write("#define TDS_HAVE_PTHREAD_MUTEX 1\n\n")
	endif(CMAKE_USE_PTHREADS_INIT)

config_write("

#define TIME_WITH_SYS_TIME 1
//#define _ALL_SOURCE 1
#define __EXTENSIONS__ 1

// readline
#define HAVE_RL_INHIBIT_COMPLETION 1
#define HAVE_RL_ON_NEW_LINE 1
#define HAVE_RL_RESET_LINE_STATE 1

// iconv
#define HAVE_ICONV 1
#define ICONV_CONST
")

endif(WIN32)

set(lib_BASE ${lib_RT} ${CMAKE_THREAD_LIBS_INIT})
if(EXISTS "${CMAKE_SOURCE_DIR}/iconv/lib/iconv.lib")
	set(lib_BASE ${lib_BASE} ${CMAKE_THREAD_LIBS_INIT} "${CMAKE_SOURCE_DIR}/iconv/lib/iconv.lib")
	config_write("#define HAVE_ICONV 1\n\n")
	config_write("#define ICONV_CONST \n\n")
endif()

if(WIN32)
	set(lib_NETWORK ws2_32 crypt32)
else(WIN32)
	# TODO check libraries
	set(lib_NETWORK gssapi_krb5)
endif(WIN32)

if(OPENSSL_FOUND)
	set(lib_NETWORK ${lib_NETWORK} ${OPENSSL_LIBRARIES})
elseif(GNUTLS_FOUND)
	set(lib_NETWORK ${lib_NETWORK} ${lib_GNUTLS})
endif(OPENSSL_FOUND)

include_directories(${CMAKE_BINARY_DIR}/include include iconv/include)

add_subdirectory(src/utils)
add_subdirectory(src/replacements)
add_subdirectory(src/tds)
add_subdirectory(src/ctlib)
add_subdirectory(src/dblib)
add_subdirectory(src/odbc)
add_subdirectory(src/apps)
add_subdirectory(src/server)
add_subdirectory(src/pool)

configure_file(${CMAKE_BINARY_DIR}/include/config.h.in ${CMAKE_BINARY_DIR}/include/config.h)
configure_file(${CMAKE_SOURCE_DIR}/include/tds_sysdep_public.h.in ${CMAKE_BINARY_DIR}/include/tds_sysdep_public.h)
configure_file(${CMAKE_SOURCE_DIR}/include/freetds/sysdep_types.h.in ${CMAKE_BINARY_DIR}/include/freetds/sysdep_types.h)
configure_file(${CMAKE_SOURCE_DIR}/include/freetds/version.h.in ${CMAKE_BINARY_DIR}/include/freetds/version.h)
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/configure")
	configure_file(${CMAKE_SOURCE_DIR}/src/odbc/version.rc.in ${CMAKE_SOURCE_DIR}/src/odbc/version.rc)
endif(NOT EXISTS "${CMAKE_SOURCE_DIR}/configure")


set(FREETDS_PUBLIC_INCLUDE
	${CMAKE_SOURCE_DIR}/include/bkpublic.h
	${CMAKE_SOURCE_DIR}/include/cspublic.h
	${CMAKE_SOURCE_DIR}/include/cstypes.h
	${CMAKE_SOURCE_DIR}/include/ctpublic.h
	${CMAKE_SOURCE_DIR}/include/sqldb.h
	${CMAKE_SOURCE_DIR}/include/sqlfront.h
	${CMAKE_SOURCE_DIR}/include/sybdb.h
	${CMAKE_SOURCE_DIR}/include/sybfront.h
	${CMAKE_SOURCE_DIR}/include/syberror.h
	${CMAKE_BINARY_DIR}/include/tds_sysdep_public.h)

set(FREETDS_PUBLIC_INCLUDE_2
	${CMAKE_BINARY_DIR}/include/freetds/version.h)

set(FREETDS_CONF_FILES
	${CMAKE_SOURCE_DIR}/freetds.conf
	${CMAKE_SOURCE_DIR}/locales.conf
	${CMAKE_SOURCE_DIR}/src/pool/pool.conf)

install(FILES ${FREETDS_PUBLIC_INCLUDE} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${FREETDS_PUBLIC_INCLUDE_2} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/freetds)
install(FILES ${FREETDS_CONF_FILES} DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR})
