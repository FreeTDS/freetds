/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2010, 2011  Frediano Ziglio
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _tds_h_
#define _tds_h_

#ifndef _freetds_config_h_
#error should include config.h before
#endif

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NET_INET_IN_H */
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

/* forward declaration */
typedef struct tdsiconvinfo TDSICONV;
typedef struct tds_connection TDSCONNECTION;
typedef struct tds_socket TDSSOCKET;
typedef struct tds_column TDSCOLUMN;
typedef struct tds_bcpinfo TDSBCPINFO;

#include <freetds/version.h>
#include <freetds/sysdep_private.h>
#include <freetds/thread.h>
#include <freetds/bool.h>
#include <freetds/macros.h>
#include <freetds/utils/string.h>
#include <freetds/utils/path.h>
#include <freetds/replacements.h>

#include <freetds/pushvis.h>

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

/**
 * A structure to hold all the compile-time settings.
 * This structure is returned by tds_get_compiletime_settings
 */

typedef struct tds_compiletime_settings
{
	const char *freetds_version;	/* release version of FreeTDS */
	const tds_dir_char *sysconfdir;		/* location of freetds.conf */
	const char *last_update;	/* latest software_version date among the modules */
	const char *tdsver;	/* TDS protocol version (4.2/4.6/5.0/7.0/7.1) 5.0 */
	bool msdblib;		/* for MS style dblib */
	bool sybase_compat;	/* enable increased Open Client binary compatibility */
	bool threadsafe;		/* compile for thread safety default=no */
	bool libiconv;		/* search for libiconv in DIR/include and DIR/lib */
	bool iodbc;		/* build odbc driver against iODBC in DIR */
	bool unixodbc;		/* build odbc driver against unixODBC in DIR */
	bool openssl;		/* build against OpenSSL */
	bool gnutls;		/* build against GnuTLS */
	bool mars;		/* MARS enabled */
	bool sspi;		/* SSPI enabled */
	bool kerberos;		/* Kerberos enabled */
} TDS_COMPILETIME_SETTINGS;

/**
 * @file tds.h
 * Main include file for libtds
 */

/**
 * \defgroup libtds LibTDS API
 * Callable functions in \c libtds.
 * 
 * The \c libtds library is for use internal to \em FreeTDS.  It is not
 * intended for use by applications.  Although any use is \em permitted, you're
 * encouraged to use one of the established public APIs instead, because their
 * interfaces are stable and documented by the vendors.  
 */

/* 
 * All references to data that touch the wire should use the following typedefs.  
 *
 * If you have problems on 64-bit machines and the code is 
 * using a native datatype, please change it to use
 * these. (In the TDS layer only, the API layers have their
 * own typedefs which equate to these).
 */
typedef char TDS_CHAR;				/*  8-bit char     */
typedef uint8_t TDS_UCHAR;			/*  8-bit uchar    */
typedef uint8_t  TDS_TINYINT;			/*  8-bit unsigned */
typedef int16_t  TDS_SMALLINT;			/* 16-bit int      */
typedef uint16_t TDS_USMALLINT;			/* 16-bit unsigned */
typedef int32_t TDS_INT;			/* 32-bit int      */
typedef uint32_t TDS_UINT;			/* 32-bit unsigned */
typedef int64_t TDS_INT8;			/* 64-bit integer  */
typedef uint64_t TDS_UINT8;			/* 64-bit unsigned */
typedef tds_sysdep_real32_type TDS_REAL;	/* 32-bit real     */
typedef tds_sysdep_real64_type TDS_FLOAT;	/* 64-bit real     */

#include <freetds/proto.h>

#define TDS_INVALID_TYPE ((TDS_SERVER_TYPE) 0)

/**
 * this structure is not directed connected to a TDS protocol but
 * keeps any DATE/TIME information.
 */
typedef struct
{
	TDS_UINT8   time;	/**< time, 7 digit precision */
	TDS_INT      date;	/**< date, 0 = 1900-01-01 */
	TDS_SMALLINT offset;	/**< time offset */
	TDS_USMALLINT time_prec:3;
	TDS_USMALLINT _tds_reserved:10;
	TDS_USMALLINT has_time:1;
	TDS_USMALLINT has_date:1;
	TDS_USMALLINT has_offset:1;
} TDS_DATETIMEALL;

/** Used by tds_datecrack */
typedef struct tdsdaterec
{
	TDS_INT year;	       /**< year */
	TDS_INT quarter;       /**< quarter (0-3) */
	TDS_INT month;	       /**< month number (0-11) */
	TDS_INT day;	       /**< day of month (1-31) */
	TDS_INT dayofyear;     /**< day of year  (1-366) */
	TDS_INT weekday;       /**< day of week  (0-6, 0 = sunday) */
	TDS_INT hour;	       /**< 0-23 */
	TDS_INT minute;	       /**< 0-59 */
	TDS_INT second;	       /**< 0-59 */
	TDS_INT decimicrosecond;   /**< 0-9999999 */
	TDS_INT timezone;      /**< -840 - 840 minutes from UTC */
} TDSDATEREC;

/**
 * The following little table is indexed by precision and will
 * tell us the number of bytes required to store the specified
 * precision.
 */
extern const int tds_numeric_bytes_per_prec[];

typedef int TDSRET;
#define TDS_NO_MORE_RESULTS  ((TDSRET)1)
#define TDS_SUCCESS          ((TDSRET)0)
#define TDS_FAIL             ((TDSRET)-1)
#define TDS_CANCELLED        ((TDSRET)-2)
#define TDS_FAILED(rc) ((rc)<0)
#define TDS_SUCCEED(rc) ((rc)>=0)
#define TDS_PROPAGATE(rc) \
	do { TDSRET _tds_ret = (rc); if (TDS_FAILED(_tds_ret)) return _tds_ret; } while(0)

#define TDS_INT_CONTINUE 1
#define TDS_INT_CANCEL 2
#define TDS_INT_TIMEOUT 3


#define TDS_NO_COUNT         -1

#define TDS_ROW_RESULT        4040
#define TDS_PARAM_RESULT      4042
#define TDS_STATUS_RESULT     4043
#define TDS_MSG_RESULT        4044
#define TDS_COMPUTE_RESULT    4045
#define TDS_CMD_DONE          4046
#define TDS_CMD_SUCCEED       4047
#define TDS_CMD_FAIL          4048
#define TDS_ROWFMT_RESULT     4049
#define TDS_COMPUTEFMT_RESULT 4050
#define TDS_DESCRIBE_RESULT   4051
#define TDS_DONE_RESULT       4052
#define TDS_DONEPROC_RESULT   4053
#define TDS_DONEINPROC_RESULT 4054
#define TDS_OTHERS_RESULT     4055

enum tds_token_results
{
	TDS_TOKEN_RES_OTHERS,
	TDS_TOKEN_RES_ROWFMT,
	TDS_TOKEN_RES_COMPUTEFMT,
	TDS_TOKEN_RES_PARAMFMT,
	TDS_TOKEN_RES_DONE,
	TDS_TOKEN_RES_ROW,
	TDS_TOKEN_RES_COMPUTE,
	TDS_TOKEN_RES_PROC,
	TDS_TOKEN_RES_MSG,
	TDS_TOKEN_RES_ENV,
};

#define TDS_TOKEN_FLAG(flag) TDS_RETURN_##flag = (1 << (TDS_TOKEN_RES_##flag*2)), TDS_STOPAT_##flag = (2 << (TDS_TOKEN_RES_##flag*2))

enum tds_token_flags
{
	TDS_HANDLE_ALL = 0,
	TDS_TOKEN_FLAG(OTHERS),
	TDS_TOKEN_FLAG(ROWFMT),
	TDS_TOKEN_FLAG(COMPUTEFMT),
	TDS_TOKEN_FLAG(PARAMFMT),
	TDS_TOKEN_FLAG(DONE),
	TDS_TOKEN_FLAG(ROW),
	TDS_TOKEN_FLAG(COMPUTE),
	TDS_TOKEN_FLAG(PROC),
	TDS_TOKEN_FLAG(MSG),
	TDS_TOKEN_FLAG(ENV),
	TDS_TOKEN_RESULTS = TDS_RETURN_ROWFMT|TDS_RETURN_COMPUTEFMT|TDS_RETURN_DONE|TDS_STOPAT_ROW|TDS_STOPAT_COMPUTE|TDS_RETURN_PROC,
	TDS_TOKEN_TRAILING = TDS_STOPAT_ROWFMT|TDS_STOPAT_COMPUTEFMT|TDS_STOPAT_ROW|TDS_STOPAT_COMPUTE|TDS_STOPAT_MSG|TDS_STOPAT_OTHERS
};

/**
 * Flags returned in TDS_DONE token
 */
enum tds_end
{
	  TDS_DONE_FINAL 	= 0x00	/**< final result set, command completed successfully. */
	, TDS_DONE_MORE_RESULTS = 0x01	/**< more results follow */
	, TDS_DONE_ERROR 	= 0x02	/**< error occurred */
	, TDS_DONE_INXACT 	= 0x04	/**< transaction in progress */
	, TDS_DONE_PROC 	= 0x08	/**< results are from a stored procedure */
	, TDS_DONE_COUNT 	= 0x10	/**< count field in packet is valid */
	, TDS_DONE_CANCELLED 	= 0x20	/**< acknowledging an attention command (usually a cancel) */
	, TDS_DONE_EVENT 	= 0x40	/*   part of an event notification. */
	, TDS_DONE_SRVERROR 	= 0x100	/**< SQL server server error */
	
	/* after the above flags, a TDS_DONE packet has a field describing the state of the transaction */
	, TDS_DONE_NO_TRAN 	= 0	/* No transaction in effect */
	, TDS_DONE_TRAN_SUCCEED = 1	/* Transaction completed successfully */
	, TDS_DONE_TRAN_PROGRESS= 2	/* Transaction in progress */
	, TDS_DONE_STMT_ABORT 	= 3	/* A statement aborted */
	, TDS_DONE_TRAN_ABORT 	= 4	/* Transaction aborted */
};


/*
 * TDSERRNO is emitted by libtds to the client library's error handler
 * (which may in turn call the client's error handler).
 * These match the db-lib msgno, because the same values have the same meaning
 * in db-lib and ODBC.  ct-lib maps them to ct-lib numbers (todo). 
 */
typedef enum {	TDSEOK    = TDS_SUCCESS, 
		TDSEVERDOWN    =  100,
		TDSEINPROGRESS,
		TDSEICONVIU    = 2400, 
		TDSEICONVAVAIL = 2401, 
		TDSEICONVO     = 2402, 
		TDSEICONVI     = 2403, 
		TDSEICONV2BIG  = 2404,
		TDSEPORTINSTANCE	= 2500,
		TDSESYNC = 20001, 
		TDSEFCON = 20002, 
		TDSETIME = 20003, 
		TDSEREAD = 20004, 
		TDSEWRIT = 20006, 
		TDSESOCK = 20008, 
		TDSECONN = 20009, 
		TDSEMEM  = 20010,
		TDSEINTF = 20012,	/* Server name not found in interface file */
		TDSEUHST = 20013,	/* Unknown host machine name. */
		TDSEPWD  = 20014, 
		TDSESEOF = 20017, 
		TDSERPND = 20019, 
		TDSEBTOK = 20020, 
		TDSEOOB  = 20022, 
		TDSECLOS = 20056,
		TDSEUSCT = 20058, 
		TDSEUTDS = 20146, 
		TDSEEUNR = 20185, 
		TDSECAP  = 20203, 
		TDSENEG  = 20210, 
		TDSEUMSG = 20212, 
		TDSECAPTYP  = 20213, 
		TDSECONF = 20214,
		TDSEBPROBADTYP = 20250,
		TDSECLOSEIN = 20292 
} TDSERRNO;


enum {
	TDS_CUR_ISTAT_UNUSED    = 0x00,
	TDS_CUR_ISTAT_DECLARED  = 0x01,
	TDS_CUR_ISTAT_OPEN      = 0x02,
	TDS_CUR_ISTAT_CLOSED    = 0x04,
	TDS_CUR_ISTAT_RDONLY    = 0x08,
	TDS_CUR_ISTAT_UPDATABLE = 0x10,
	TDS_CUR_ISTAT_ROWCNT    = 0x20,
	TDS_CUR_ISTAT_DEALLOC   = 0x40
};

/* string types */
#define TDS_NULLTERM -9


typedef union tds_option_arg
{
	TDS_TINYINT ti;
	TDS_INT i;
	TDS_CHAR *c;
} TDS_OPTION_ARG;


typedef enum tds_encryption_level {
	TDS_ENCRYPTION_DEFAULT,
	TDS_ENCRYPTION_OFF,
	TDS_ENCRYPTION_REQUEST,
	TDS_ENCRYPTION_REQUIRE,
	TDS_ENCRYPTION_STRICT
} TDS_ENCRYPTION_LEVEL;

/*
 * TODO use system macros for optimization
 * See mcrypt for reference and linux kernel source for optimization
 * check if unaligned access and use fast write/read when implemented
 */
#define TDS_BYTE_SWAP16(value)                 \
         (((((uint16_t)value)<<8) & 0xFF00u) | \
          ((((uint16_t)value)>>8) & 0x00FFu))

#define TDS_BYTE_SWAP32(value)                     \
         (((((uint32_t)value)<<24) & 0xFF000000u)| \
          ((((uint32_t)value)<< 8) & 0x00FF0000u)| \
          ((((uint32_t)value)>> 8) & 0x0000FF00u)| \
          ((((uint32_t)value)>>24) & 0x000000FFu))

#define is_end_token(x) ((x) >= TDS_DONE_TOKEN && (x) <= TDS_DONEINPROC_TOKEN)

enum {
	TDS_TYPEFLAG_INVALID  = 0,
	TDS_TYPEFLAG_NULLABLE = 1,
	TDS_TYPEFLAG_FIXED    = 2,
	TDS_TYPEFLAG_VARIABLE = 4,
	TDS_TYPEFLAG_COLLATE  = 8,
	TDS_TYPEFLAG_ASCII    = 16,
	TDS_TYPEFLAG_UNICODE  = 32,
	TDS_TYPEFLAG_BINARY   = 64,
	TDS_TYPEFLAG_DATETIME = 128,
	TDS_TYPEFLAG_NUMERIC  = 256,
	TDS_TYPEFLAG_VARIANT  = 512,
};

extern const uint16_t tds_type_flags_ms[256];
#if 0
extern const uint16_t tds_type_flags_syb[256];
extern const char *const tds_type_names[256];
#endif

#define is_fixed_type(x)      ((tds_type_flags_ms[x] & TDS_TYPEFLAG_FIXED)    != 0)
#define is_nullable_type(x)   ((tds_type_flags_ms[x] & TDS_TYPEFLAG_NULLABLE) != 0)
#define is_variable_type(x)   ((tds_type_flags_ms[x] & TDS_TYPEFLAG_VARIABLE) != 0)
#define is_variant_inner_type(x)   ((tds_type_flags_ms[x] & TDS_TYPEFLAG_VARIANT) != 0)


#define is_blob_type(x)       ((x)==SYBTEXT || (x)==SYBIMAGE || (x)==SYBNTEXT)
#define is_blob_col(x)        ((x)->column_varint_size > 2)
/* large type means it has a two byte size field */
/* define is_large_type(x) (x>128) */
#define is_numeric_type(x)    ((x)==SYBNUMERIC || (x)==SYBDECIMAL)
/** return true if type is a datetime (but not date or time) */
#define is_datetime_type(x)   ((tds_type_flags_ms[x] & TDS_TYPEFLAG_DATETIME) != 0)
#define is_unicode_type(x)    ((tds_type_flags_ms[x] & TDS_TYPEFLAG_UNICODE) != 0)
#define is_collate_type(x)    ((tds_type_flags_ms[x] & TDS_TYPEFLAG_COLLATE) != 0)
#define is_ascii_type(x)      ((tds_type_flags_ms[x] & TDS_TYPEFLAG_ASCII) != 0)
#define is_binary_type(x)     ((tds_type_flags_ms[x] & TDS_TYPEFLAG_BINARY) != 0)
#define is_char_type(x)       ((tds_type_flags_ms[x] & (TDS_TYPEFLAG_ASCII|TDS_TYPEFLAG_UNICODE)) != 0)
#define is_similar_type(x, y) (is_char_type(x) && is_char_type(y))
inline static bool
is_tds_type_valid(int type)
{
	return (unsigned) type < 256u && tds_type_flags_ms[type] != 0;
}


#define TDS_MAX_CAPABILITY	32
#define MAXPRECISION 		77
#define TDS_MAX_CONN		4096
#define TDS_MAX_DYNID_LEN	30

/* defaults to use if no others are found */
#define TDS_DEF_SERVER		"SYBASE"
#define TDS_DEF_BLKSZ		512
#define TDS_DEF_CHARSET		"iso_1"
#define TDS_DEF_LANG		"us_english"
#if TDS50
#define TDS_DEFAULT_VERSION	0x500
#define TDS_DEF_PORT		4000
#elif TDS71
#define TDS_DEFAULT_VERSION	0x701
#define TDS_DEF_PORT		1433
#elif TDS72
#define TDS_DEFAULT_VERSION	0x702
#define TDS_DEF_PORT		1433
#elif TDS73
#define TDS_DEFAULT_VERSION	0x703
#define TDS_DEF_PORT		1433
#elif TDS74
#define TDS_DEFAULT_VERSION	0x704
#define TDS_DEF_PORT		1433
#else
#define TDS_DEFAULT_VERSION	0x000
#define TDS_DEF_PORT		1433
#endif

/* normalized strings from freetds.conf file */
#define TDS_STR_VERSION  "tds version"
#define TDS_STR_BLKSZ    "initial block size"
#define TDS_STR_SWAPDT   "swap broken dates"
#define TDS_STR_DUMPFILE "dump file"
#define TDS_STR_DEBUGLVL "debug level"
#define TDS_STR_DEBUGFLAGS "debug flags"
#define TDS_STR_TIMEOUT  "timeout"
#define TDS_STR_QUERY_TIMEOUT  "query timeout"
#define TDS_STR_CONNTIMEOUT "connect timeout"
#define TDS_STR_HOSTNAME "hostname"
#define TDS_STR_HOST     "host"
#define TDS_STR_PORT     "port"
#define TDS_STR_TEXTSZ   "text size"
/* for big endian hosts, obsolete, ignored */
#define TDS_STR_EMUL_LE	"emulate little endian"
#define TDS_STR_CHARSET	"charset"
#define TDS_STR_CLCHARSET	"client charset"
#define TDS_STR_USE_UTF_16	"use utf-16"
#define TDS_STR_LANGUAGE	"language"
#define TDS_STR_APPENDMODE	"dump file append"
#define TDS_STR_DATETIMEFMT	"date format"
#define TDS_STR_DATEFMT	"date-only format"
#define TDS_STR_TIMEFMT	"time-only format"
#define TDS_STR_INSTANCE "instance"
#define TDS_STR_ASA_DATABASE	"asa database"
#define TDS_STR_DATABASE	"database"
#define TDS_STR_ENCRYPTION	 "encryption"
#define TDS_STR_USENTLMV2	"use ntlmv2"
#define TDS_STR_USELANMAN	"use lanman"
/* conf values */
#define TDS_STR_ENCRYPTION_OFF	 "off"
#define TDS_STR_ENCRYPTION_REQUEST "request"
#define TDS_STR_ENCRYPTION_REQUIRE "require"
#define TDS_STR_ENCRYPTION_STRICT "strict"
/* Defines to enable optional GSSAPI delegation */
#define TDS_GSSAPI_DELEGATION "enable gssapi delegation"
/* Mutual authentication */
#define TDS_STR_MUTUAL_AUTHENTICATION "mutual authentication"
/* Kerberos realm name */
#define TDS_STR_REALM	"realm"
/* Kerberos SPN */
#define TDS_STR_SPN	"spn"
/* CA file */
#define TDS_STR_CAFILE	"ca file"
/* CRL file */
#define TDS_STR_CRLFILE	"crl file"
/* check SSL hostname */
#define TDS_STR_CHECKSSLHOSTNAME	"check certificate hostname"
/* database filename to attach on login (MSSQL) */
#define TDS_STR_DBFILENAME	"database filename"
/* Application Intent MSSQL 2012 support */
#define TDS_STR_READONLY_INTENT "read-only intent"
/* configurable cipher suite to send to openssl's SSL_set_cipher_list() function */
#define TLS_STR_OPENSSL_CIPHERS "openssl ciphers"
/* enable old TLS v1, required for instance if you are using a really old Windows XP */
#define TDS_STR_ENABLE_TLS_V1 "enable tls v1"


/* TODO do a better check for alignment than this */
typedef union
{
	void *p;
	int i;
	int64_t ui;
} tds_align_struct;

#define TDS_ALIGN_SIZE sizeof(tds_align_struct)

typedef struct tds_capability_type
{
	unsigned char type;
	unsigned char len; /* always sizeof(values) */
	unsigned char values[TDS_MAX_CAPABILITY/2-2];
} TDS_CAPABILITY_TYPE;

typedef struct tds_capabilities
{
	TDS_CAPABILITY_TYPE types[2];
} TDS_CAPABILITIES;

#define TDS_MAX_LOGIN_STR_SZ 128
typedef struct tds_login
{
	DSTR server_name;		/**< server name (in freetds.conf) */
	int port;			/**< port of database service */
	TDS_USMALLINT tds_version;	/**< TDS version */
	int block_size;
	DSTR language;			/* e.g. us-english */
	DSTR server_charset;		/**< charset of server e.g. iso_1 */
	TDS_INT connect_timeout;
	DSTR client_host_name;
	DSTR server_host_name;
	DSTR server_realm_name;		/**< server realm name (in freetds.conf) */
	DSTR server_spn;		/**< server SPN (in freetds.conf) */
	DSTR db_filename;		/**< database filename to attach (MSSQL) */
	DSTR cafile;			/**< certificate authorities file */
	DSTR crlfile;			/**< certificate revocation file */
	DSTR openssl_ciphers;
	DSTR app_name;
	DSTR user_name;	    	/**< account for login */
	DSTR password;	    	/**< password of account login */
	DSTR new_password;	    	/**< new password to set (TDS 7.2+) */

	DSTR library;	/* Ct-Library, DB-Library,  TDS-Library or ODBC */
	TDS_TINYINT encryption_level;

	TDS_INT query_timeout;
	TDS_CAPABILITIES capabilities;
	DSTR client_charset;
	DSTR database;

	struct addrinfo *ip_addrs;	  		/**< ip(s) of server */
	DSTR instance_name;
	tds_dir_char *dump_file;
	int debug_flags;
	int text_size;
	DSTR routing_address;
	uint16_t routing_port;

	unsigned char option_flag2;

	unsigned int bulk_copy:1;	/**< if bulk copy should be enabled */
	unsigned int suppress_language:1;
	unsigned int gssapi_use_delegation:1;
	unsigned int mutual_authentication:1;
	unsigned int use_ntlmv2:1;
	unsigned int use_ntlmv2_specified:1;
	unsigned int use_lanman:1;
	unsigned int mars:1;
	unsigned int use_utf16:1;
	unsigned int use_new_password:1;
	unsigned int valid_configuration:1;
	unsigned int check_ssl_hostname:1;
	unsigned int readonly_intent:1;
	unsigned int enable_tls_v1:1;
	unsigned int enable_tls_v1_specified:1;
	unsigned int server_is_valid:1;
} TDSLOGIN;

typedef struct tds_headers
{
	const char *qn_options;
	const char *qn_msgtext;
	TDS_INT qn_timeout;
	/* TDS 7.4+: trace activity ID char[20] */
} TDSHEADERS;

typedef struct tds_locale
{
	char *language;
	char *server_charset;
	char *datetime_fmt;
	char *date_fmt;
	char *time_fmt;
} TDSLOCALE;

/** 
 * Information about blobs (e.g. text or image).
 * current_row contains this structure.
 */
typedef struct tds_blob
{
	TDS_CHAR *textvalue;
	TDS_CHAR textptr[16];
	TDS_CHAR timestamp[8];
	bool valid_ptr;
} TDSBLOB;

/**
 * Store variant information
 */
typedef struct tds_variant
{
	/* this MUST have same position and place of textvalue in tds_blob */
	TDS_CHAR *data;
	TDS_INT size;
	TDS_INT data_len;
	TDS_SERVER_TYPE type;
	TDS_UCHAR collation[5];
} TDSVARIANT;

/**
 * Information relevant to libiconv.  The name is an iconv name, not 
 * the same as found in master..syslanguages. 
 */
typedef struct tds_encoding
{
	/** name of the encoding (ie UTF-8) */
	const char *name;
	unsigned char min_bytes_per_char;
	unsigned char max_bytes_per_char;
	/** internal numeric index into array of all encodings */
	unsigned char canonic;
} TDS_ENCODING;

typedef struct tds_bcpcoldata
{
	TDS_UCHAR *data;
	TDS_INT    datalen;
	bool       is_null;
} BCPCOLDATA;


typedef TDSRET  tds_func_get_info(TDSSOCKET *tds, TDSCOLUMN *col);
typedef TDSRET  tds_func_get_data(TDSSOCKET *tds, TDSCOLUMN *col);
typedef TDS_INT tds_func_row_len(TDSCOLUMN *col);
typedef TDSRET  tds_func_put_info(TDSSOCKET *tds, TDSCOLUMN *col);
typedef TDSRET  tds_func_put_data(TDSSOCKET *tds, TDSCOLUMN *col, int bcp7);
typedef int     tds_func_check(const TDSCOLUMN *col);

typedef struct tds_column_funcs
{
	tds_func_get_info *get_info;
	tds_func_get_data *get_data;
	tds_func_row_len  *row_len;
	/**
	 * Send metadata column information to server.
	 * \tds
	 * \param col  column to send
	 */
	tds_func_put_info *put_info;
	/**
	 * Send column data to server.
	 * Usually send parameters unless bcp7 is specified, in
	 * this case send BCP for TDS7+ (Sybase use a completely
	 * different format for BCP)
	 * \tds
	 * \param col  column to send
	 * \param bcp7 1 to send BCP column on TDS7+
	 */
	tds_func_put_data *put_data;
#if ENABLE_EXTRA_CHECKS
	/**
	 * Check column is valid.
	 * Some things should be checked:
	 * - column_type and on_server.column_type;
	 * - column_size and on_server.column_size;
	 * - column_cur_size;
	 * - column_prec and column_scale;
	 * - is_XXXX_type macros/functions (nullable/fixed/blob/variable);
	 * - tds_get_size_by_type;
	 * - tds_get_conversion_type.
	 *
	 * \tds
	 * \param col  column to check
	 */
	tds_func_check    *check;
#endif
#if 0
	TDSRET (*convert)(TDSSOCKET *tds, TDSCOLUMN *col);
#endif
} TDSCOLUMNFUNCS;

/** 
 * Metadata about columns in regular and compute rows 
 */
struct tds_column
{
	const TDSCOLUMNFUNCS *funcs;
	TDS_INT column_usertype;
	TDS_INT column_flags;

	TDS_INT column_size;		/**< maximun size of data. For fixed is the size. */

	TDS_SERVER_TYPE column_type;	/**< This type can be different from wire type because
	 				 * conversion (e.g. UCS-2->Ascii) can be applied.
					 * I'm beginning to wonder about the wisdom of this, however.
					 * April 2003 jkl
					 */
	TDS_TINYINT column_varint_size;	/**< size of length when reading from wire (0, 1, 2 or 4) */

	TDS_TINYINT column_prec;	/**< precision for decimal/numeric */
	TDS_TINYINT column_scale;	/**< scale for decimal/numeric */

	struct
	{
		TDS_SERVER_TYPE column_type;	/**< type of data, saved from wire */
		TDS_INT column_size;
	} on_server;

	TDSICONV *char_conv;	/**< refers to previously allocated iconv information */

	DSTR table_name;
	DSTR column_name;
	DSTR table_column_name;

	unsigned char *column_data;
	void (*column_data_free)(struct tds_column *column);
	unsigned char column_nullable:1;
	unsigned char column_writeable:1;
	unsigned char column_identity:1;
	unsigned char column_key:1;
	unsigned char column_hidden:1;
	unsigned char column_output:1;
	unsigned char column_timestamp:1;
	unsigned char column_computed:1;
	TDS_UCHAR column_collation[5];

	/* additional fields flags for compute results */
	TDS_SMALLINT column_operand;
	TDS_TINYINT column_operator;

	/* FIXME this is data related, not column */
	/** size written in variable (ie: char, text, binary). -1 if NULL. */
	TDS_INT column_cur_size;

	/* related to binding or info stored by client libraries */
	/* FIXME find a best place to store these data, some are unused */
	TDS_SMALLINT column_bindtype;
	TDS_SMALLINT column_bindfmt;
	TDS_UINT column_bindlen;
	TDS_SMALLINT *column_nullbind;
	TDS_CHAR *column_varaddr;
	TDS_INT *column_lenbind;
	TDS_INT column_textpos;
	TDS_INT column_text_sqlgetdatapos;
	TDS_CHAR column_text_sqlputdatainfo;
	unsigned char column_iconv_left;
	char column_iconv_buf[9];

	BCPCOLDATA *bcp_column_data;
	/**
	 * The length, in bytes, of any length prefix this column may have.
	 * For example, strings in some non-C programming languages are
	 * made up of a one-byte length prefix, followed by the string
	 * data itself.
	 * If the data do not have a length prefix, set prefixlen to 0.
	 * Currently not very used in code, however do not remove.
	 */
	TDS_INT bcp_prefix_len;
	TDS_INT bcp_term_len;
	TDS_CHAR *bcp_terminator;
};


/** Hold information for any results */
typedef struct tds_result_info
{
	/* TODO those fields can became a struct */
	TDSCOLUMN **columns;
	TDS_USMALLINT num_cols;
	TDS_USMALLINT computeid;
	TDS_INT ref_count;
	TDSSOCKET *attached_to;
	unsigned char *current_row;
	void (*row_free)(struct tds_result_info* result, unsigned char *row);
	TDS_INT row_size;

	TDS_SMALLINT *bycolumns;
	TDS_USMALLINT by_cols;
	bool rows_exist;
	/* TODO remove ?? used only in dblib */
	bool more_results;
} TDSRESULTINFO;

/** values for tds->state */
typedef enum tds_states
{
	TDS_IDLE,	/**< no data expected */
	TDS_WRITING,	/**< client is writing data */
	TDS_SENDING,	/**< client would send data */
	TDS_PENDING,	/**< cilent is waiting for data */
	TDS_READING,	/**< client is reading data */
	TDS_DEAD	/**< no connection */
} TDS_STATE;

typedef enum tds_operations
{
	TDS_OP_NONE		= 0,

	/* mssql operations */
	TDS_OP_CURSOR		= TDS_SP_CURSOR,
	TDS_OP_CURSOROPEN	= TDS_SP_CURSOROPEN,
	TDS_OP_CURSORPREPARE	= TDS_SP_CURSORPREPARE,
	TDS_OP_CURSOREXECUTE	= TDS_SP_CURSOREXECUTE,
	TDS_OP_CURSORPREPEXEC	= TDS_SP_CURSORPREPEXEC,
	TDS_OP_CURSORUNPREPARE	= TDS_SP_CURSORUNPREPARE,
	TDS_OP_CURSORFETCH	= TDS_SP_CURSORFETCH,
	TDS_OP_CURSOROPTION	= TDS_SP_CURSOROPTION,
	TDS_OP_CURSORCLOSE	= TDS_SP_CURSORCLOSE,
	TDS_OP_EXECUTESQL	= TDS_SP_EXECUTESQL,
	TDS_OP_PREPARE		= TDS_SP_PREPARE,
	TDS_OP_EXECUTE		= TDS_SP_EXECUTE,
	TDS_OP_PREPEXEC		= TDS_SP_PREPEXEC,
	TDS_OP_PREPEXECRPC	= TDS_SP_PREPEXECRPC,
	TDS_OP_UNPREPARE	= TDS_SP_UNPREPARE,

	/* sybase operations */
	TDS_OP_DYN_DEALLOC	= 100,
} TDS_OPERATION;

#define TDS_DBG_LOGIN   __FILE__, ((__LINE__ << 4) | 11)
#define TDS_DBG_HEADER  __FILE__, ((__LINE__ << 4) | 10)
#define TDS_DBG_FUNC    __FILE__, ((__LINE__ << 4) |  7)
#define TDS_DBG_INFO2   __FILE__, ((__LINE__ << 4) |  6)
#define TDS_DBG_INFO1   __FILE__, ((__LINE__ << 4) |  5)
#define TDS_DBG_NETWORK __FILE__, ((__LINE__ << 4) |  4)
#define TDS_DBG_WARN    __FILE__, ((__LINE__ << 4) |  3)
#define TDS_DBG_ERROR   __FILE__, ((__LINE__ << 4) |  2)
#define TDS_DBG_SEVERE  __FILE__, ((__LINE__ << 4) |  1)

#define TDS_DBGFLAG_FUNC    0x80
#define TDS_DBGFLAG_INFO2   0x40
#define TDS_DBGFLAG_INFO1   0x20
#define TDS_DBGFLAG_NETWORK 0x10
#define TDS_DBGFLAG_WARN    0x08
#define TDS_DBGFLAG_ERROR   0x04
#define TDS_DBGFLAG_SEVERE  0x02
#define TDS_DBGFLAG_ALL     0xfff
#define TDS_DBGFLAG_LOGIN   0x0800
#define TDS_DBGFLAG_HEADER  0x0400
#define TDS_DBGFLAG_PID     0x1000
#define TDS_DBGFLAG_TIME    0x2000
#define TDS_DBGFLAG_SOURCE  0x4000
#define TDS_DBGFLAG_THREAD  0x8000

#if 0
/**
 * An attempt at better logging.
 * Using these bitmapped values, various logging features can be turned on and off.
 * It can be especially helpful to turn packet data on/off for security reasons.
 */
enum TDS_DBG_LOG_STATE
{
	  TDS_DBG_LOGIN =  (1 << 0)	/**< for diagnosing login problems;                                       
				 	   otherwise the username/password information is suppressed. */
	, TDS_DBG_API =    (1 << 1)	/**< Log calls to client libraries */
	, TDS_DBG_ASYNC =  (1 << 2)	/**< Log asynchronous function starts or completes. */
	, TDS_DBG_DIAG =   (1 << 3)	/**< Log client- and server-generated messages */
	, TDS_DBG_error =  (1 << 4)
	/* TODO:  ^^^^^ make upper case when old #defines (above) are removed */
	/* Log FreeTDS runtime/logic error occurs. */
	, TDS_DBG_PACKET = (1 << 5)	/**< Log hex dump of packets to/from the server. */
	, TDS_DBG_LIBTDS = (1 << 6)	/**< Log calls to (and in) libtds */
	, TDS_DBG_CONFIG = (1 << 7)	/**< replaces TDSDUMPCONFIG */
	, TDS_DBG_DEFAULT = 0xFE	/**< all above except login packets */
};
#endif

typedef struct tds_result_info TDSCOMPUTEINFO;

typedef TDSRESULTINFO TDSPARAMINFO;

typedef struct tds_message
{
	TDS_CHAR *server;
	TDS_CHAR *message;
	TDS_CHAR *proc_name;
	TDS_CHAR *sql_state;
	TDS_INT msgno;
	TDS_INT line_number;
	/* -1 .. 255 */
	TDS_SMALLINT state;
	TDS_TINYINT priv_msg_type;
	TDS_TINYINT severity;
	/* for library-generated errors */
	int oserr;
} TDSMESSAGE;

typedef struct tds_upd_col
{
	struct tds_upd_col *next;	
	TDS_INT colnamelength;
	char * columnname;
} TDSUPDCOL;

typedef enum {
	  TDS_CURSOR_STATE_UNACTIONED = 0   	/* initial value */
	, TDS_CURSOR_STATE_REQUESTED = 1	/* called by ct_cursor */ 
	, TDS_CURSOR_STATE_SENT = 2		/* sent to server */
	, TDS_CURSOR_STATE_ACTIONED = 3		/* acknowledged by server */
} TDS_CURSOR_STATE;

typedef struct tds_cursor_status
{
	TDS_CURSOR_STATE declare;
	TDS_CURSOR_STATE cursor_row;
	TDS_CURSOR_STATE open;
	TDS_CURSOR_STATE fetch;
	TDS_CURSOR_STATE close; 
	TDS_CURSOR_STATE dealloc;
} TDS_CURSOR_STATUS;

typedef enum tds_cursor_operation
{
	TDS_CURSOR_POSITION = 0,
	TDS_CURSOR_UPDATE = 1,
	TDS_CURSOR_DELETE = 2,
	TDS_CURSOR_INSERT = 4
} TDS_CURSOR_OPERATION;

typedef enum tds_cursor_fetch
{
	TDS_CURSOR_FETCH_NEXT = 1,
	TDS_CURSOR_FETCH_PREV,
	TDS_CURSOR_FETCH_FIRST,
	TDS_CURSOR_FETCH_LAST,
	TDS_CURSOR_FETCH_ABSOLUTE,
	TDS_CURSOR_FETCH_RELATIVE
} TDS_CURSOR_FETCH;

/**
 * Holds information about a cursor
 */
typedef struct tds_cursor
{
	struct tds_cursor *next;	/**< next in linked list, keep first */
	TDS_INT ref_count;		/**< reference counter so client can retain safely a pointer */
	char *cursor_name;		/**< name of the cursor */
	TDS_INT cursor_id;		/**< cursor id returned by the server after cursor declare */
	TDS_TINYINT options;		/**< read only|updatable TODO use it */
	/**
	 * true if cursor was marker to be closed when connection is idle
	 */
	bool defer_close;
	char *query;                 	/**< SQL query */
	/* TODO for updatable columns */
	/* TDS_TINYINT number_upd_cols; */	/**< number of updatable columns */
	/* TDSUPDCOL *cur_col_list; */	/**< updatable column list */
	TDS_INT cursor_rows;		/**< number of cursor rows to fetch */
	/* TDSPARAMINFO *params; */	/** cursor parameter */
	TDS_CURSOR_STATUS status;
	TDS_USMALLINT srv_status;
	TDSRESULTINFO *res_info;	/** row fetched from this cursor */
	TDS_INT type, concurrency;
} TDSCURSOR;

/**
 * Current environment as reported by the server
 */
typedef struct tds_env
{
	/** packet size (512-65535) */
	int block_size;
	char *language;
	/** character set encoding */
	char *charset;
	/** database name */
	char *database;
} TDSENV;

/**
 * Holds information for a dynamic (also called prepared) query.
 */
typedef struct tds_dynamic
{
	struct tds_dynamic *next;	/**< next in linked list, keep first */
	TDS_INT ref_count;		/**< reference counter so client can retain safely a pointer */
	/** numeric id for mssql7+*/
	TDS_INT num_id;
	/** 
	 * id of dynamic.
	 * Usually this id correspond to server one but if not specified
	 * is generated automatically by libTDS
	 */
	char id[30];
	/**
	 * this dynamic query cannot be prepared so libTDS have to construct a simple query.
	 * This can happen for instance is tds protocol doesn't support dynamics or trying
	 * to prepare query under Sybase that have BLOBs as parameters.
	 */
	TDS_TINYINT emulated;
	/**
	 * true if dynamic was marker to be closed when connection is idle
	 */
	bool defer_close;
	/* int dyn_state; */ /* TODO use it */
	TDSPARAMINFO *res_info;	/**< query results */
	/**
	 * query parameters.
	 * Mostly used executing query however is a good idea to prepare query
	 * again if parameter type change in an incompatible way (ie different
	 * types or larger size). Is also better to prepare a query knowing
	 * parameter types earlier.
	 */
	TDSPARAMINFO *params;
	/** saved query, we need to know original query if prepare is impossible */
	char *query;
} TDSDYNAMIC;

typedef enum {
	TDS_MULTIPLE_QUERY,
	TDS_MULTIPLE_EXECUTE,
	TDS_MULTIPLE_RPC
} TDS_MULTIPLE_TYPE;

typedef struct tds_multiple
{
	TDS_MULTIPLE_TYPE type;
	unsigned int flags;
} TDSMULTIPLE;

/* forward declaration */
typedef struct tds_context TDSCONTEXT;
typedef int (*err_handler_t) (const TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);

struct tds_context
{
	TDSLOCALE *locale;
	void *parent;
	/* handlers */
	int (*msg_handler) (const TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);
	int (*err_handler) (const TDSCONTEXT *, TDSSOCKET *, TDSMESSAGE *);
	int (*int_handler) (void *);
	bool money_use_2_digits;
};

enum TDS_ICONV_ENTRY
{ 
	  client2ucs2
	, client2server_chardata
	, initial_char_conv_count	/* keep last */
};

typedef struct tds_authentication
{
	uint8_t *packet;
	int packet_len;
	/* TDS_MSG_TOKEN type, for TDS5 */
	uint16_t msg_type;
	TDSRET (*free)(TDSCONNECTION* conn, struct tds_authentication * auth);
	TDSRET (*handle_next)(TDSSOCKET * tds, struct tds_authentication * auth, size_t len);
} TDSAUTHENTICATION;

typedef struct tds_packet
{
	struct tds_packet *next;
	uint16_t sid;

#if ENABLE_ODBC_MARS
	/**
	 * Data before TDS data, currently can be 0 or sizeof(TDS72_SMP_HEADER)
	 */
	uint8_t data_start;
#endif

	/**
	 * data length, this does not account SMP header, only TDS part
	 */
	unsigned data_len;
	unsigned capacity;
	unsigned char buf[1];
} TDSPACKET;

#if ENABLE_ODBC_MARS
#define tds_packet_zero_data_start(pkt) do { (pkt)->data_start = 0; } while(0)
#define tds_packet_get_data_start(pkt) ((pkt)->data_start)
#else
#define tds_packet_zero_data_start(pkt) do { } while(0)
#define tds_packet_get_data_start(pkt) 0
#endif

typedef struct tds_poll_wakeup
{
	TDS_SYS_SOCKET s_signal, s_signaled;
} TDSPOLLWAKEUP;

/* field related to connection */
struct tds_connection
{
	TDS_USMALLINT tds_version;
	TDS_UINT product_version;	/**< version of product (Sybase/MS and full version) */
	char *product_name;

	TDS_SYS_SOCKET s;		/**< tcp socket, INVALID_SOCKET if not connected */
	TDSPOLLWAKEUP wakeup;
	const TDSCONTEXT *tds_ctx;

	/** environment is shared between all sessions */
	TDSENV env;

	/**
	 * linked list of cursors allocated for this connection
	 * contains only cursors allocated on the server
	 */
	TDSCURSOR *cursors;
	/**
	 * list of dynamic allocated for this connection
	 * contains only dynamic allocated on the server
	 */
	TDSDYNAMIC *dyns;

	int char_conv_count;
	TDSICONV **char_convs;

	TDS_UCHAR collation[5];
	TDS_UCHAR tds72_transaction[8];

	TDS_CAPABILITIES capabilities;
	unsigned int use_iconv:1;
	unsigned int tds71rev1:1;
	unsigned int pending_close:1;	/**< true is connection has pending closing (cursors or dynamic) */
	unsigned int encrypt_single_packet:1;
#if ENABLE_ODBC_MARS
	unsigned int mars:1;

	TDSSOCKET *in_net_tds;
	TDSPACKET *packets;
	TDSPACKET *recv_packet;
	TDSPACKET *send_packets;
	unsigned send_pos, recv_pos;

#define BUSY_SOCKET ((TDSSOCKET*)(TDS_UINTPTR)1)
#define TDSSOCKET_VALID(tds) (((TDS_UINTPTR)(tds)) > 1)
	struct tds_socket **sessions;
	unsigned num_sessions;
#endif
	tds_mutex list_mtx;

	unsigned num_cached_packets;
	TDSPACKET *packet_cache;

	int spid;
	int client_spid;

	/**
	 * Ratio between bytes allocated for a NCHAR type and type length (Sybase).
	 * For instance in case a NVARCHAR(3) takes 9 bytes it's 3.
	 */
	uint8_t ncharsize;

	/**
	 * Ratio between bytes allocated for a UNICHAR type and type length (Sybase).
	 * For instance in case a UNIVARCHAR(2) takes 4 bytes it's 2.
	 * It really should be only 2.
	 */
	uint8_t unicharsize;

	void *tls_session;
#if defined(HAVE_GNUTLS)
	void *tls_credentials;
#elif defined(HAVE_OPENSSL)
	void *tls_ctx;
#else
	void *tls_dummy;
#endif
	TDSAUTHENTICATION *authentication;
	char *server;
};

/**
 * Information for a server connection
 */
struct tds_socket
{
#if ENABLE_ODBC_MARS
	TDSCONNECTION *conn;
#else
	TDSCONNECTION conn[1];
#endif

	void *parent;

	/** Input buffer.
	 * Points to receiving packet buffer.
	 * As input buffer contains just the raw packet actually this pointer
	 * is the address of recv_packet->buf.
	 */
	unsigned char *in_buf;

	/** Output buffer.
	 * Points to sending packet buffer.
	 * Output buffer can contain additional data before the raw TDS packet
	 * so this buffer can point some bytes after send_packet->buf.
	 * Specifically this will point to send_packet->buf + send_packet->data_start.
	 */
	unsigned char *out_buf;

	/** Maximum size of packet pointed by out_buf.
	 * The buffer is actually a bit larger to make possible to do some
	 * optimizations (at least TDS_ADDITIONAL_SPACE bytes).
	 */
	unsigned int out_buf_max;
	unsigned in_pos;		/**< current position in in_buf */
	unsigned out_pos;		/**< current position in out_buf */
	unsigned in_len;		/**< input buffer length */
	unsigned char in_flag;		/**< input buffer type */
	unsigned char out_flag;		/**< output buffer type */

	unsigned frozen;
	/**
	 * list of packets frozen, points to first one.
	 * send_packet is the last packet in the list.
	 */
	TDSPACKET *frozen_packets;

#if ENABLE_ODBC_MARS
	/** SID of MARS session.
	 * ==0  Not in a MARS session or first session
	 * >0   SID of MARS session valid.
	 */
	uint16_t sid;

	/**
	 * This condition will be signaled by the network thread on packet
	 * received or sent for this session
	 */
	tds_condition packet_cond;

	/**
	 * Packet we are trying to send to network.
	 * This field should be protected by conn->list_mtx
	 */
	TDSPACKET *sending_packet;
	TDS_UINT recv_seq;
	TDS_UINT send_seq;
	TDS_UINT recv_wnd;
	TDS_UINT send_wnd;
#endif
	/* packet we received */
	TDSPACKET *recv_packet;
	/** packet we are preparing to send */
	TDSPACKET *send_packet;

	/**
	 * Current query information. 
	 * Contains information in process, both normal and compute results.
	 * This pointer shouldn't be freed; it's just an alias to another structure.
	 */
	TDSRESULTINFO *current_results;
	TDSRESULTINFO *res_info;
	TDS_UINT num_comp_info;
	TDSCOMPUTEINFO **comp_info;
	TDSPARAMINFO *param_info;
	TDSCURSOR *cur_cursor;		/**< cursor in use */
	bool bulk_query;		/**< true is query sent was a bulk query so we need to switch state to QUERYING */
	bool has_status; 		/**< true is ret_status is valid */
	bool in_row;			/**< true if we are getting rows */
	volatile 
	unsigned char in_cancel; 	/**< indicate we are waiting a cancel reply; discard tokens till acknowledge; 
	1 mean we have to send cancel packet, 2 already sent. */
	TDS_INT ret_status;     	/**< return status from store procedure */
	TDS_STATE state;

	TDS_INT query_timeout;
	TDS_INT8 rows_affected;		/**< rows updated/deleted/inserted/selected, TDS_NO_COUNT if not valid */

	TDSDYNAMIC *cur_dyn;		/**< dynamic structure in use */

	TDSLOGIN *login;	/**< config for login stuff. After login this field is NULL */

	void (*env_chg_func) (TDSSOCKET * tds, int type, char *oldval, char *newval);
	TDS_OPERATION current_op;

	int option_value;
	tds_mutex wire_mtx;
};

#define tds_get_ctx(tds) ((tds)->conn->tds_ctx)
#define tds_set_ctx(tds, val) do { ((tds)->conn->tds_ctx) = (val); } while(0)
#define tds_get_parent(tds) ((tds)->parent)
#define tds_set_parent(tds, val) do { ((tds)->parent) = (val); } while(0)
#define tds_get_s(tds) ((tds)->conn->s)
#define tds_set_s(tds, val) do { ((tds)->conn->s) = (val); } while(0)

typedef struct tds_tvp_row
{
	TDSPARAMINFO *params;
	struct tds_tvp_row *next;
} TDS_TVP_ROW;

typedef struct tds_tvp
{
	char *schema;
	char *name;
	TDSPARAMINFO *metadata;
	TDS_TVP_ROW *row;
} TDS_TVP;


/* config.c */
const TDS_COMPILETIME_SETTINGS *tds_get_compiletime_settings(void);
typedef bool (*TDSCONFPARSE) (const char *option, const char *value, void *param);
bool tds_read_conf_section(FILE * in, const char *section, TDSCONFPARSE tds_conf_parse, void *parse_param);
bool tds_read_conf_file(TDSLOGIN * login, const char *server);
bool tds_parse_conf_section(const char *option, const char *value, void *param);
TDSLOGIN *tds_read_config_info(TDSSOCKET * tds, TDSLOGIN * login, TDSLOCALE * locale);
void tds_fix_login(TDSLOGIN* login);
TDS_USMALLINT * tds_config_verstr(const char *tdsver, TDSLOGIN* login);
struct addrinfo *tds_lookup_host(const char *servername);
TDSRET tds_lookup_host_set(const char *servername, struct addrinfo **addr);
const char *tds_addrinfo2str(struct addrinfo *addr, char *name, int namemax);

TDSRET tds_set_interfaces_file_loc(const char *interfloc);
extern const char STD_DATETIME_FMT[];
int tds_parse_boolean(const char *value, int default_value);
int tds_config_boolean(const char *option, const char *value, TDSLOGIN * login);

TDSLOCALE *tds_get_locale(void);
TDSRET tds_alloc_row(TDSRESULTINFO * res_info);
TDSRET tds_alloc_compute_row(TDSCOMPUTEINFO * res_info);
BCPCOLDATA * tds_alloc_bcp_column_data(unsigned int column_size);
TDSDYNAMIC *tds_lookup_dynamic(TDSCONNECTION * conn, const char *id);
/*@observer@*/ const char *tds_prtype(int token);
int tds_get_varint_size(TDSCONNECTION * conn, int datatype);
TDS_SERVER_TYPE tds_get_cardinal_type(TDS_SERVER_TYPE datatype, int usertype);


/* iconv.c */
TDSRET tds_iconv_open(TDSCONNECTION * conn, const char *charset, int use_utf16);
void tds_iconv_close(TDSCONNECTION * conn);
void tds_srv_charset_changed(TDSCONNECTION * conn, const char *charset);
void tds7_srv_charset_changed(TDSCONNECTION * conn, TDS_UCHAR collate[5]);
int tds_iconv_alloc(TDSCONNECTION * conn);
void tds_iconv_free(TDSCONNECTION * conn);
TDSICONV *tds_iconv_from_collate(TDSCONNECTION * conn, TDS_UCHAR collate[5]);


/* mem.c */
void tds_free_socket(TDSSOCKET * tds);
void tds_free_all_results(TDSSOCKET * tds);
void tds_free_results(TDSRESULTINFO * res_info);
void tds_free_param_results(TDSPARAMINFO * param_info);
void tds_free_param_result(TDSPARAMINFO * param_info);
void tds_free_msg(TDSMESSAGE * message);
void tds_cursor_deallocated(TDSCONNECTION *conn, TDSCURSOR *cursor);
void tds_release_cursor(TDSCURSOR **pcursor);
void tds_free_bcp_column_data(BCPCOLDATA * coldata);
TDSRESULTINFO *tds_alloc_results(TDS_USMALLINT num_cols);
TDSCOMPUTEINFO **tds_alloc_compute_results(TDSSOCKET * tds, TDS_USMALLINT num_cols, TDS_USMALLINT by_cols);
TDSCONTEXT *tds_alloc_context(void * parent);
void tds_free_context(TDSCONTEXT * locale);
TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO * old_param);
void tds_free_input_params(TDSDYNAMIC * dyn);
void tds_release_dynamic(TDSDYNAMIC ** dyn);
inline static void
tds_release_cur_dyn(TDSSOCKET * tds)
{
	tds_release_dynamic(&tds->cur_dyn);
}
void tds_dynamic_deallocated(TDSCONNECTION *conn, TDSDYNAMIC *dyn);
void tds_set_cur_dyn(TDSSOCKET *tds, TDSDYNAMIC *dyn);
TDSSOCKET *tds_realloc_socket(TDSSOCKET * tds, size_t bufsize);
char *tds_alloc_client_sqlstate(int msgno);
char *tds_alloc_lookup_sqlstate(TDSSOCKET * tds, int msgno);
TDSLOGIN *tds_alloc_login(int use_environment);
TDSDYNAMIC *tds_alloc_dynamic(TDSCONNECTION * conn, const char *id);
void tds_free_login(TDSLOGIN * login);
TDSLOGIN *tds_init_login(TDSLOGIN * login, TDSLOCALE * locale);
TDSLOCALE *tds_alloc_locale(void);
void *tds_alloc_param_data(TDSCOLUMN * curparam);
void tds_free_locale(TDSLOCALE * locale);
TDSCURSOR * tds_alloc_cursor(TDSSOCKET * tds, const char *name, TDS_INT namelen, const char *query, TDS_INT querylen);
void tds_free_row(TDSRESULTINFO * res_info, unsigned char *row);
TDSSOCKET *tds_alloc_socket(TDSCONTEXT * context, unsigned int bufsize);
TDSSOCKET *tds_alloc_additional_socket(TDSCONNECTION *conn);
void tds_set_current_results(TDSSOCKET *tds, TDSRESULTINFO *info);
void tds_detach_results(TDSRESULTINFO *info);
void * tds_realloc(void **pp, size_t new_size);
#define TDS_RESIZE(p, n_elem) \
	tds_realloc((void **) &(p), sizeof(*(p)) * (size_t) (n_elem))

TDSPACKET *tds_alloc_packet(void *buf, unsigned len);
TDSPACKET *tds_realloc_packet(TDSPACKET *packet, unsigned len);
void tds_free_packets(TDSPACKET *packet);
TDSBCPINFO *tds_alloc_bcpinfo(void);
void tds_free_bcpinfo(TDSBCPINFO *bcpinfo);
void tds_deinit_bcpinfo(TDSBCPINFO *bcpinfo);
void tds_deinit_tvp(TDS_TVP *table);


/* login.c */
void tds_set_packet(TDSLOGIN * tds_login, int packet_size);
void tds_set_port(TDSLOGIN * tds_login, int port);
bool tds_set_passwd(TDSLOGIN * tds_login, const char *password) TDS_WUR;
void tds_set_bulk(TDSLOGIN * tds_login, bool enabled);
bool tds_set_user(TDSLOGIN * tds_login, const char *username) TDS_WUR;
bool tds_set_app(TDSLOGIN * tds_login, const char *application) TDS_WUR;
bool tds_set_host(TDSLOGIN * tds_login, const char *hostname) TDS_WUR;
bool tds_set_library(TDSLOGIN * tds_login, const char *library) TDS_WUR;
bool tds_set_server(TDSLOGIN * tds_login, const char *server) TDS_WUR;
bool tds_set_client_charset(TDSLOGIN * tds_login, const char *charset) TDS_WUR;
bool tds_set_language(TDSLOGIN * tds_login, const char *language) TDS_WUR;
void tds_set_version(TDSLOGIN * tds_login, TDS_TINYINT major_ver, TDS_TINYINT minor_ver);
TDSRET tds_connect_and_login(TDSSOCKET * tds, TDSLOGIN * login);


/* query.c */
void tds_start_query(TDSSOCKET *tds, unsigned char packet_type);

TDSRET tds_submit_query(TDSSOCKET * tds, const char *query);
TDSRET tds_submit_query_params(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params, TDSHEADERS * head);
TDSRET tds_submit_queryf(TDSSOCKET * tds, const char *queryf, ...);
TDSRET tds_submit_prepare(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params);
TDSRET tds_submit_execdirect(TDSSOCKET * tds, const char *query, TDSPARAMINFO * params, TDSHEADERS * head);
TDSRET tds71_submit_prepexec(TDSSOCKET * tds, const char *query, const char *id, TDSDYNAMIC ** dyn_out, TDSPARAMINFO * params);
TDSRET tds_submit_execute(TDSSOCKET * tds, TDSDYNAMIC * dyn);
TDSRET tds_send_cancel(TDSSOCKET * tds);
const char *tds_next_placeholder(const char *start);
int tds_count_placeholders(const char *query);
int tds_needs_unprepare(TDSCONNECTION * conn, TDSDYNAMIC * dyn);
TDSRET tds_deferred_unprepare(TDSCONNECTION * conn, TDSDYNAMIC * dyn);
TDSRET tds_submit_unprepare(TDSSOCKET * tds, TDSDYNAMIC * dyn);
TDSRET tds_submit_rpc(TDSSOCKET * tds, const char *rpc_name, TDSPARAMINFO * params, TDSHEADERS * head);
TDSRET tds_submit_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD command, TDS_OPTION option, TDS_OPTION_ARG *param, TDS_INT param_size);
TDSRET tds_submit_begin_tran(TDSSOCKET *tds);
TDSRET tds_submit_rollback(TDSSOCKET *tds, bool cont);
TDSRET tds_submit_commit(TDSSOCKET *tds, bool cont);
TDSRET tds_disconnect(TDSSOCKET * tds);
size_t tds_quote_id(TDSSOCKET * tds, char *buffer, const char *id, int idlen);
size_t tds_quote_id_rpc(TDSSOCKET * tds, char *buffer, const char *id, int idlen);
size_t tds_quote_string(TDSSOCKET * tds, char *buffer, const char *str, int len);
const char *tds_skip_comment(const char *s);
const char *tds_skip_quoted(const char *s);
size_t tds_fix_column_size(TDSSOCKET * tds, TDSCOLUMN * curcol);
const char *tds_convert_string(TDSSOCKET * tds, TDSICONV * char_conv, const char *s, int len, size_t *out_len);
void tds_convert_string_free(const char *original, const char *converted);
#if !ENABLE_EXTRA_CHECKS
#define tds_convert_string_free(original, converted) \
	do { if (original != converted) free((char*) converted); } while(0)
#endif
TDSRET tds_get_column_declaration(TDSSOCKET * tds, TDSCOLUMN * curcol, char *out);

TDSRET tds_cursor_declare(TDSSOCKET * tds, TDSCURSOR * cursor, bool *send);
TDSRET tds_cursor_setrows(TDSSOCKET * tds, TDSCURSOR * cursor, bool *send);
TDSRET tds_cursor_open(TDSSOCKET * tds, TDSCURSOR * cursor, TDSPARAMINFO *params, bool *send);
TDSRET tds_cursor_fetch(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_FETCH fetch_type, TDS_INT i_row);
TDSRET tds_cursor_get_cursor_info(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_UINT * row_number, TDS_UINT * row_count);
TDSRET tds_cursor_close(TDSSOCKET * tds, TDSCURSOR * cursor);
TDSRET tds_cursor_dealloc(TDSSOCKET * tds, TDSCURSOR * cursor);
TDSRET tds_deferred_cursor_dealloc(TDSCONNECTION *conn, TDSCURSOR * cursor);
TDSRET tds_cursor_update(TDSSOCKET * tds, TDSCURSOR * cursor, TDS_CURSOR_OPERATION op, TDS_INT i_row, TDSPARAMINFO * params);
TDSRET tds_cursor_setname(TDSSOCKET * tds, TDSCURSOR * cursor);

TDSRET tds_multiple_init(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDS_MULTIPLE_TYPE type, TDSHEADERS * head);
TDSRET tds_multiple_done(TDSSOCKET *tds, TDSMULTIPLE *multiple);
TDSRET tds_multiple_query(TDSSOCKET *tds, TDSMULTIPLE *multiple, const char *query, TDSPARAMINFO * params);
TDSRET tds_multiple_execute(TDSSOCKET *tds, TDSMULTIPLE *multiple, TDSDYNAMIC * dyn);


/* token.c */
TDSRET tds_process_cancel(TDSSOCKET * tds);
TDSRET tds_process_login_tokens(TDSSOCKET * tds);
TDSRET tds_process_simple_query(TDSSOCKET * tds);
int tds5_send_optioncmd(TDSSOCKET * tds, TDS_OPTION_CMD tds_command, TDS_OPTION tds_option, TDS_OPTION_ARG * tds_argument,
			TDS_INT * tds_argsize);
TDSRET tds_process_tokens(TDSSOCKET * tds, /*@out@*/ TDS_INT * result_type, /*@out@*/ int *done_flags, unsigned flag);


/* data.c */
void tds_set_param_type(TDSCONNECTION * conn, TDSCOLUMN * curcol, TDS_SERVER_TYPE type);
void tds_set_column_type(TDSCONNECTION * conn, TDSCOLUMN * curcol, TDS_SERVER_TYPE type);
#ifdef WORDS_BIGENDIAN
void tds_swap_datatype(int coltype, void *b);
#endif


/* tds_convert.c */
TDSRET tds_datecrack(TDS_INT datetype, const void *di, TDSDATEREC * dr);
TDS_SERVER_TYPE tds_get_conversion_type(TDS_SERVER_TYPE srctype, int colsize);
extern const char tds_hex_digits[];


/* write.c */
int tds_init_write_buf(TDSSOCKET * tds);
int tds_put_n(TDSSOCKET * tds, const void *buf, size_t n);
int tds_put_string(TDSSOCKET * tds, const char *buf, int len);
int tds_put_int(TDSSOCKET * tds, TDS_INT i);
int tds_put_int8(TDSSOCKET * tds, TDS_INT8 i);
int tds_put_smallint(TDSSOCKET * tds, TDS_SMALLINT si);
/** Output a tinyint value */
#define tds_put_tinyint(tds, ti) tds_put_byte(tds,ti)
int tds_put_byte(TDSSOCKET * tds, unsigned char c);
TDSRET tds_flush_packet(TDSSOCKET * tds);
int tds_put_buf(TDSSOCKET * tds, const unsigned char *buf, int dsize, int ssize);


/* read.c */
unsigned char tds_get_byte(TDSSOCKET * tds);
void tds_unget_byte(TDSSOCKET * tds);
unsigned char tds_peek(TDSSOCKET * tds);
TDS_USMALLINT tds_get_usmallint(TDSSOCKET * tds);
#define tds_get_smallint(tds) ((TDS_SMALLINT) tds_get_usmallint(tds))
TDS_UINT tds_get_uint(TDSSOCKET * tds);
#define tds_get_int(tds) ((TDS_INT) tds_get_uint(tds))
TDS_UINT8 tds_get_uint8(TDSSOCKET * tds);
#define tds_get_int8(tds) ((TDS_INT8) tds_get_uint8(tds))
size_t tds_get_string(TDSSOCKET * tds, size_t string_len, char *dest, size_t dest_size);
TDSRET tds_get_char_data(TDSSOCKET * tds, char *dest, size_t wire_size, TDSCOLUMN * curcol);
bool tds_get_n(TDSSOCKET * tds, /*@out@*/ /*@null@*/ void *dest, size_t n);
int tds_get_size_by_type(TDS_SERVER_TYPE servertype);
DSTR* tds_dstr_get(TDSSOCKET * tds, DSTR * s, size_t len);


/* util.c */
int tdserror (const TDSCONTEXT * tds_ctx, TDSSOCKET * tds, int msgno, int errnum);
TDS_STATE tds_set_state(TDSSOCKET * tds, TDS_STATE state);
void tds_swap_bytes(void *buf, int bytes);
unsigned int tds_gettime_ms(void);


/* log.c */
typedef struct tdsdump_off_item {
	struct tdsdump_off_item *next;
	tds_thread_id thread_id;
} TDSDUMP_OFF_ITEM;
void tdsdump_off(TDSDUMP_OFF_ITEM *off_item);
void tdsdump_on(TDSDUMP_OFF_ITEM *off_item);
int tdsdump_isopen(void);
#include <freetds/popvis.h>
int tdsdump_open(const tds_dir_char *filename);
#include <freetds/pushvis.h>
void tdsdump_close(void);
void tdsdump_dump_buf(const char* file, unsigned int level_line, const char *msg, const void *buf, size_t length);
void tdsdump_col(const TDSCOLUMN *col);
#undef tdsdump_log
void tdsdump_log(const char* file, unsigned int level_line, const char *fmt, ...)
#if defined(__GNUC__) && __GNUC__ >= 2
#if defined(__MINGW32__)
	__attribute__ ((__format__ (ms_printf, 3, 4)))
#else
	__attribute__ ((__format__ (__printf__, 3, 4)))
#endif
#endif
;
#define TDSDUMP_LOG_FAST if (TDS_UNLIKELY(tds_write_dump)) tdsdump_log
#define tdsdump_log TDSDUMP_LOG_FAST
#define TDSDUMP_BUF_FAST if (TDS_UNLIKELY(tds_write_dump)) tdsdump_dump_buf
#define tdsdump_dump_buf TDSDUMP_BUF_FAST

extern int tds_write_dump;
extern int tds_debug_flags;
extern int tds_g_append_mode;


/* net.c */
TDSERRNO tds_open_socket(TDSSOCKET * tds, struct addrinfo *ipaddr, unsigned int port, int timeout, int *p_oserr);
void tds_close_socket(TDSSOCKET * tds);
int tds7_get_instance_ports(FILE *output, struct addrinfo *addr);
int tds7_get_instance_port(struct addrinfo *addr, const char *instance);
char *tds_prwsaerror(int erc);
void tds_prwsaerror_free(char *s);
int tds_connection_read(TDSSOCKET * tds, unsigned char *buf, int buflen);
int tds_connection_write(TDSSOCKET *tds, const unsigned char *buf, int buflen, int final);
#define TDSSELREAD  POLLIN
#define TDSSELWRITE POLLOUT
int tds_select(TDSSOCKET * tds, unsigned tds_sel, int timeout_seconds);
void tds_connection_close(TDSCONNECTION *conn);
int tds_goodread(TDSSOCKET * tds, unsigned char *buf, int buflen);
int tds_goodwrite(TDSSOCKET * tds, const unsigned char *buffer, size_t buflen);
void tds_socket_flush(TDS_SYS_SOCKET sock);
int tds_socket_set_nonblocking(TDS_SYS_SOCKET sock);
int tds_wakeup_init(TDSPOLLWAKEUP *wakeup);
void tds_wakeup_close(TDSPOLLWAKEUP *wakeup);
void tds_wakeup_send(TDSPOLLWAKEUP *wakeup, char cancel);
inline static TDS_SYS_SOCKET
tds_wakeup_get_fd(const TDSPOLLWAKEUP *wakeup)
{
	return wakeup->s_signaled;
}


/* packet.c */
int tds_read_packet(TDSSOCKET * tds);
TDSRET tds_write_packet(TDSSOCKET * tds, unsigned char final);
#if ENABLE_ODBC_MARS
int tds_append_cancel(TDSSOCKET *tds);
TDSRET tds_append_syn(TDSSOCKET *tds);
TDSRET tds_append_fin(TDSSOCKET *tds);
#else
int tds_put_cancel(TDSSOCKET * tds);
#endif

typedef struct tds_freeze {
	/** which socket we refer to */
	TDSSOCKET *tds;
	/** first packet frozen */
	TDSPACKET *pkt;
	/** position in pkt */
	unsigned pkt_pos;
	/** length size (0, 1, 2 or 4) */
	unsigned size_len;
} TDSFREEZE;

void tds_freeze(TDSSOCKET *tds, TDSFREEZE *freeze, unsigned size_len);
size_t tds_freeze_written(TDSFREEZE *freeze);
TDSRET tds_freeze_abort(TDSFREEZE *freeze);
TDSRET tds_freeze_close(TDSFREEZE *freeze);
TDSRET tds_freeze_close_len(TDSFREEZE *freeze, int32_t size);

inline static void
tds_set_current_send_packet(TDSSOCKET *tds, TDSPACKET *pkt)
{
	tds->send_packet = pkt;
	tds->out_buf = pkt->buf + tds_packet_get_data_start(pkt);
}

/* Macros to allow some indentation of the packets.
 *
 * The 3 nested fake loops require some explanation:
 * - first is to allows to declare variables;
 * - second is to force using brackets;
 * - third is to avoids that a break inside will skip the close.
 */
#define TDS_START_LEN_GENERIC(tds_socket, len) do { \
	TDSFREEZE current_freeze[1]; \
	tds_freeze((tds_socket), current_freeze, (len)); do { do
#define TDS_END_LEN while(0); } while(tds_freeze_close(current_freeze), 0); } while(0);

#define TDS_START_LEN_TINYINT(tds_socket) TDS_START_LEN_GENERIC(tds_socket, 1)
#define TDS_START_LEN_USMALLINT(tds_socket) TDS_START_LEN_GENERIC(tds_socket, 2)
#define TDS_START_LEN_UINT(tds_socket) TDS_START_LEN_GENERIC(tds_socket, 4)

/* vstrbuild.c */
TDSRET tds_vstrbuild(char *buffer, int buflen, int *resultlen, const char *text, int textlen, const char *formats, int formatlen,
		  va_list ap);


/* numeric.c */
char *tds_money_to_string(const TDS_MONEY * money, char *s, bool use_2_digits);
TDS_INT tds_numeric_to_string(const TDS_NUMERIC * numeric, char *s);
TDS_INT tds_numeric_change_prec_scale(TDS_NUMERIC * numeric, unsigned char new_prec, unsigned char new_scale);


/* getmac.c */
void tds_getmac(TDS_SYS_SOCKET s, unsigned char mac[6]);


/* challenge.c */
#ifndef HAVE_SSPI
TDSAUTHENTICATION * tds_ntlm_get_auth(TDSSOCKET * tds);
TDSAUTHENTICATION * tds_gss_get_auth(TDSSOCKET * tds);
#else
TDSAUTHENTICATION * tds_sspi_get_auth(TDSSOCKET * tds);
#endif
TDSRET tds5_gss_send(TDSSOCKET *tds);


/* random.c */
void tds_random_buffer(unsigned char *out, int len);


/* sec_negotiate.c */
TDSAUTHENTICATION * tds5_negotiate_get_auth(TDSSOCKET * tds);
inline static void
tds5_negotiate_set_msg_type(TDSAUTHENTICATION * tds_auth, unsigned msg_type)
{
	if (tds_auth)
		tds_auth->msg_type = msg_type;
}


/* bulk.c */

/** bcp direction */
enum tds_bcp_directions
{
	TDS_BCP_IN = 1,
	TDS_BCP_OUT = 2,
	TDS_BCP_QUERYOUT = 3
};

typedef struct tds5_colinfo
{
	TDS_TINYINT type;
	TDS_TINYINT status;
	TDS_SMALLINT offset;
	TDS_INT length;
} TDS5COLINFO;

struct tds_bcpinfo
{
	const char *hint;
	void *parent;
	DSTR tablename;
	TDS_CHAR *insert_stmt;
	TDS_INT direction;
	TDS_INT identity_insert_on;
	TDS_INT xfer_init;
	TDS_INT bind_count;
	TDSRESULTINFO *bindinfo;
	TDS5COLINFO *sybase_colinfo;
	TDS_INT sybase_count;
};

TDSRET tds_bcp_init(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
typedef TDSRET (*tds_bcp_get_col_data) (TDSBCPINFO *bulk, TDSCOLUMN *bcpcol, int offset);
typedef void (*tds_bcp_null_error)   (TDSBCPINFO *bulk, int index, int offset);
TDSRET tds_bcp_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset);
TDSRET tds_bcp_done(TDSSOCKET *tds, int *rows_copied);
TDSRET tds_bcp_start(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);
TDSRET tds_bcp_start_copy_in(TDSSOCKET *tds, TDSBCPINFO *bcpinfo);

TDSRET tds_bcp_fread(TDSSOCKET * tds, TDSICONV * conv, FILE * stream,
		     const char *terminator, size_t term_len, char **outbuf, size_t * outbytes);

TDSRET tds_writetext_start(TDSSOCKET *tds, const char *objname, const char *textptr, const char *timestamp, int with_log, TDS_UINT size);
TDSRET tds_writetext_continue(TDSSOCKET *tds, const TDS_UCHAR *text, TDS_UINT size);
TDSRET tds_writetext_end(TDSSOCKET *tds);


inline static bool
tds_capability_enabled(const TDS_CAPABILITY_TYPE *cap, unsigned cap_num)
{
	return (cap->values[sizeof(cap->values)-1-(cap_num>>3)] >> (cap_num&7)) & 1;
}
#define tds_capability_has_req(conn, cap) \
	tds_capability_enabled(&conn->capabilities.types[0], cap)
#define tds_capability_has_res(conn, cap) \
	tds_capability_enabled(&conn->capabilities.types[1], cap)

#define IS_TDS42(x) (x->tds_version==0x402)
#define IS_TDS46(x) (x->tds_version==0x406)
#define IS_TDS50(x) (x->tds_version==0x500)
#define IS_TDS70(x) (x->tds_version==0x700)
#define IS_TDS71(x) (x->tds_version==0x701)
#define IS_TDS72(x) (x->tds_version==0x702)
#define IS_TDS73(x) (x->tds_version==0x703)

#define IS_TDS50_PLUS(x) ((x)->tds_version>=0x500)
#define IS_TDS7_PLUS(x)  ((x)->tds_version>=0x700)
#define IS_TDS71_PLUS(x) ((x)->tds_version>=0x701)
#define IS_TDS72_PLUS(x) ((x)->tds_version>=0x702)
#define IS_TDS73_PLUS(x) ((x)->tds_version>=0x703)
#define IS_TDS74_PLUS(x) ((x)->tds_version>=0x704)

#define TDS_MAJOR(x) ((x)->tds_version >> 8)
#define TDS_MINOR(x) ((x)->tds_version & 0xff)

#define IS_TDSDEAD(x) (((x) == NULL) || (x)->state == TDS_DEAD)

/** Check if product is Sybase (such as Adaptive Server Enterrprice). x should be a TDSSOCKET*. */
#define TDS_IS_SYBASE(x) (!((x)->conn->product_version & 0x80000000u))
/** Check if product is Microsft SQL Server. x should be a TDSSOCKET*. */
#define TDS_IS_MSSQL(x) (((x)->conn->product_version & 0x80000000u)!=0)

/** Calc a version number for mssql. Use with TDS_MS_VER(7,0,842).
 * For test for a range of version you can use check like
 * if (tds->product_version >= TDS_MS_VER(7,0,0) && tds->product_version < TDS_MS_VER(8,0,0)) */
#define TDS_MS_VER(maj,min,x) (0x80000000u|((maj)<<24)|((min)<<16)|(x))

/* TODO test if not similar to ms one*/
/** Calc a version number for Sybase. */
#define TDS_SYB_VER(maj,min,x) (((maj)<<24)|((min)<<16)|(x)<<8)

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#include <freetds/popvis.h>

#define TDS_PUT_INT(tds,v) tds_put_int((tds), ((TDS_INT)(v)))
#define TDS_PUT_SMALLINT(tds,v) tds_put_smallint((tds), ((TDS_SMALLINT)(v)))
#define TDS_PUT_BYTE(tds,v) tds_put_byte((tds), ((unsigned char)(v)))

#endif /* _tds_h_ */
