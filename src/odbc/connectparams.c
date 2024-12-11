/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005-2024  Frediano Ziglio
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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include <freetds/odbc.h>
#include <freetds/utils/string.h>
#include <freetds/utils/path.h>
#include <freetds/utils.h>
#include <freetds/replacements.h>

#define ODBC_PARAM(p) const char odbc_param_##p[] = #p;
ODBC_PARAM_LIST
#undef ODBC_PARAM

#ifdef _WIN32
#define ODBC_PARAM(p) odbc_param_##p,
static const char *odbc_param_names[] = {
	ODBC_PARAM_LIST
};
#undef ODBC_PARAM
#endif

#if !HAVE_SQLGETPRIVATEPROFILESTRING

/*
 * Last resort place to check for INI file. This is usually set at compile time
 * by build scripts.
 */
#ifndef SYS_ODBC_INI
#define SYS_ODBC_INI "/etc/odbc.ini"
#endif

/**
 * Call this to get the INI file containing Data Source Names.
 * @note rules for determining the location of ODBC config may be different 
 * then what you expect - at this time they differ from unixODBC 
 *
 * @return file opened or NULL if error
 * @retval 1 worked
 */
static FILE *tdoGetIniFileName(void);

/* avoid name collision with system headers */
#define SQLGetPrivateProfileString tds_SQLGetPrivateProfileString

/**
 * SQLGetPrivateProfileString
 *
 * PURPOSE
 *
 *  This is an implementation of a common MS API call. This implementation 
 *  should only be used if the ODBC sub-system/SDK does not have it.
 *  For example; unixODBC has its own so those using unixODBC should NOT be
 *  using this implementation because unixODBC;
 *  - provides caching of ODBC config data 
 *  - provides consistent interpretation of ODBC config data (i.e, location)
 *
 * ARGS
 *
 *  see ODBC documentation
 *                      
 * RETURNS
 *
 *  see ODBC documentation
 *
 * NOTES:
 *
 *  - the spec is not entirely implemented... consider this a lite version
 *  - rules for determining the location of ODBC config may be different then what you 
 *    expect see tdoGetIniFileName().
 *
 */
static int SQLGetPrivateProfileString(LPCSTR pszSection, LPCSTR pszEntry, LPCSTR pszDefault, LPSTR pRetBuffer, int nRetBuffer,
				      LPCSTR pszFileName);
#endif

#if defined(FILENAME_MAX) && FILENAME_MAX < 512
#undef FILENAME_MAX
#define FILENAME_MAX 512
#endif

static int
parse_server(TDS_ERRS *errs, char *server, TDSLOGIN * login)
{
	char *p = (char *) strchr(server, '\\');

	if (p) {
		if (!tds_dstr_copy(&login->instance_name, p+1)) {
			odbc_errs_add(errs, "HY001", NULL);
			return 0;
		}
		*p = 0;
	} else {
		p = (char *) strchr(server, ',');
		if (p && atoi(p+1) > 0) {
			login->port = atoi(p+1);
			*p = 0;
		}
	}

	if (TDS_SUCCEED(tds_lookup_host_set(server, &login->ip_addrs)))
		if (!tds_dstr_copy(&login->server_host_name, server)) {
			odbc_errs_add(errs, "HY001", NULL);
			return 0;
		}

	return 1;
}

static int
myGetPrivateProfileString(const char *DSN, const char *key, char *buf)
{
	buf[0] = '\0';
	return SQLGetPrivateProfileString(DSN, key, "", buf, FILENAME_MAX, "odbc.ini");
}

/**
 * Converts Encrypt option to Encryption option.
 * Encryption is FreeTDS specific and older than Encrypt.
 */
static const char *
odbc_encrypt2encryption(const char *encrypt)
{
	if (strcasecmp(encrypt, "strict") == 0)
		return TDS_STR_ENCRYPTION_STRICT;

	if (strcasecmp(encrypt, "mandatory") == 0
	    || strcasecmp(encrypt, "true") == 0
	    || strcasecmp(encrypt, "yes") == 0)
		return TDS_STR_ENCRYPTION_REQUIRE;

	if (strcasecmp(encrypt, "optional") == 0
	    || strcasecmp(encrypt, "false") == 0
	    || strcasecmp(encrypt, "no") == 0)
		return TDS_STR_ENCRYPTION_REQUEST;

	return "invalid_encrypt";
}

/** 
 * Read connection information from given DSN
 * @param DSN           DSN name
 * @param login    where to store connection info
 * @return true if success false otherwhise
 */
bool
odbc_get_dsn_info(TDS_ERRS *errs, const char *DSN, TDSLOGIN * login)
{
	char tmp[FILENAME_MAX];
	bool freetds_conf_less = true;

	/* use old servername */
	if (myGetPrivateProfileString(DSN, odbc_param_Servername, tmp) > 0) {
		freetds_conf_less = false;
		if (!tds_dstr_copy(&login->server_name, tmp)) {
			odbc_errs_add(errs, "HY001", NULL);
			return false;
		}
		tds_read_conf_file(login, tmp);
		if (myGetPrivateProfileString(DSN, odbc_param_Server, tmp) > 0) {
			odbc_errs_add(errs, "HY000", "You cannot specify both SERVERNAME and SERVER");
			return false;
		}
		if (myGetPrivateProfileString(DSN, odbc_param_Address, tmp) > 0) {
			odbc_errs_add(errs, "HY000", "You cannot specify both SERVERNAME and ADDRESS");
			return false;
		}
	}

	/* search for server (compatible with ms one) */
	if (freetds_conf_less) {
		bool address_specified = false;

		if (myGetPrivateProfileString(DSN, odbc_param_Address, tmp) > 0) {
			address_specified = true;
			/* TODO parse like MS */

			if (TDS_FAILED(tds_lookup_host_set(tmp, &login->ip_addrs))) {
				odbc_errs_add(errs, "HY000", "Error parsing ADDRESS attribute");
				return false;
			}
		}
		if (myGetPrivateProfileString(DSN, odbc_param_Server, tmp) > 0) {
			if (!tds_dstr_copy(&login->server_name, tmp)) {
				odbc_errs_add(errs, "HY001", NULL);
				return false;
			}
			if (!address_specified) {
				if (!parse_server(errs, tmp, login))
					return false;
			}
		}
	}

	if (myGetPrivateProfileString(DSN, odbc_param_Port, tmp) > 0)
		tds_parse_conf_section(TDS_STR_PORT, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_TDS_Version, tmp) > 0)
		tds_parse_conf_section(TDS_STR_VERSION, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_Language, tmp) > 0)
		tds_parse_conf_section(TDS_STR_LANGUAGE, tmp, login);

	if (tds_dstr_isempty(&login->database)
	    && myGetPrivateProfileString(DSN, odbc_param_Database, tmp) > 0)
		if (!tds_dstr_copy(&login->database, tmp)) {
			odbc_errs_add(errs, "HY001", NULL);
			return false;
		}

	if (myGetPrivateProfileString(DSN, odbc_param_TextSize, tmp) > 0)
		tds_parse_conf_section(TDS_STR_TEXTSZ, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_PacketSize, tmp) > 0)
		tds_parse_conf_section(TDS_STR_BLKSZ, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_ClientCharset, tmp) > 0)
		tds_parse_conf_section(TDS_STR_CLCHARSET, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_DumpFile, tmp) > 0)
		tds_parse_conf_section(TDS_STR_DUMPFILE, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_DumpFileAppend, tmp) > 0)
		tds_parse_conf_section(TDS_STR_APPENDMODE, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_DebugFlags, tmp) > 0)
		tds_parse_conf_section(TDS_STR_DEBUGFLAGS, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_Encryption, tmp) > 0)
		tds_parse_conf_section(TDS_STR_ENCRYPTION, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_Encrypt, tmp) > 0)
		tds_parse_conf_section(TDS_STR_ENCRYPTION, odbc_encrypt2encryption(tmp), login);

	if (myGetPrivateProfileString(DSN, odbc_param_UseNTLMv2, tmp) > 0)
		tds_parse_conf_section(TDS_STR_USENTLMV2, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_REALM, tmp) > 0)
		tds_parse_conf_section(TDS_STR_REALM, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_ServerSPN, tmp) > 0)
		tds_parse_conf_section(TDS_STR_SPN, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_Trusted_Connection, tmp) > 0
	    && tds_config_boolean(odbc_param_Trusted_Connection, tmp, login)) {
		tds_dstr_empty(&login->user_name);
		tds_dstr_empty(&login->password);
	}

	if (myGetPrivateProfileString(DSN, odbc_param_MARS_Connection, tmp) > 0
	    && tds_config_boolean(odbc_param_MARS_Connection, tmp, login)) {
		login->mars = 1;
	}

	if (myGetPrivateProfileString(DSN, odbc_param_AttachDbFilename, tmp) > 0)
		tds_parse_conf_section(TDS_STR_DBFILENAME, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_Timeout, tmp) > 0)
		tds_parse_conf_section(TDS_STR_TIMEOUT, tmp, login);

	if (myGetPrivateProfileString(DSN, odbc_param_HostNameInCertificate, tmp) > 0
	    && (tmp[0] && strcmp(tmp, "null") != 0)) {
		if (!tds_dstr_copy(&login->certificate_host_name, tmp)) {
			odbc_errs_add(errs, "HY001", NULL);
			return false;
		}
	}

	return true;
}

/**
 * Swap two DSTR
 */
static void
odbc_dstr_swap(DSTR *a, DSTR *b)
{
	DSTR tmp = *a;
	*a = *b;
	*b = tmp;
}

static const char *
parse_value(TDS_ERRS *errs, const char *p, const char *connect_string_end, DSTR *value)
{
	const char *end;
	char *dst;

	/* easy case, just ';' terminated */
	if (p == connect_string_end || *p != '{') {
		end = (const char *) memchr(p, ';', connect_string_end - p);
		if (!end)
			end = connect_string_end;
		if (!tds_dstr_copyn(value, p, end - p)) {
			odbc_errs_add(errs, "HY001", NULL);
			return NULL;
		}
		return end;
	}

	++p;
	/* search "};" */
	end = p;
	for (;;) {
		/* search next '}' */
		end = (const char *) memchr(end, '}', connect_string_end - end);
		if (end == NULL) {
			odbc_errs_add(errs, "HY000", "Syntax error in connection string");
			return NULL;
		}
		end++;

		/* termination ? */
		if (end == connect_string_end || end[0] == ';') {
			end--;
			break;
		}

		/* wrong syntax ? */
		if (end[0] != '}') {
			odbc_errs_add(errs, "HY000", "Syntax error in connection string");
			return NULL;
		}
		end++;
	}
	if (!tds_dstr_alloc(value, end - p)) {
		odbc_errs_add(errs, "HY001", NULL);
		return NULL;
	}
	dst = tds_dstr_buf(value);
	for (; p < end; ++p) {
		char ch = *p;
		*dst++ = ch;
		if (ch == '}')
			++p;
	}
	tds_dstr_setlen(value, dst - tds_dstr_buf(value));

	return end + 1;
}

/** 
 * Parse connection string and fill login according
 * @param connect_string     connect string
 * @param connect_string_end connect string end (pointer to char past last)
 * @param login         where to store connection info
 * @return true if success false otherwise
 */
bool
odbc_parse_connect_string(TDS_ERRS *errs, const char *connect_string, const char *connect_string_end, TDSLOGIN * login,
			  TDS_PARSED_PARAM *parsed_params)
{
	const char *p, *end;
	DSTR *dest_s, value = DSTR_INITIALIZER;
	enum { CFG_DSN = 1, CFG_SERVERNAME = 2 };
	unsigned int cfgs = 0;	/* flags for indicate second parse of string */
	char option[24];
	int trusted = 0;

	if (parsed_params)
		memset(parsed_params, 0, sizeof(*parsed_params)*ODBC_PARAM_SIZE);

	for (p = connect_string; p < connect_string_end && *p;) {
		int num_param = -1;

		dest_s = NULL;

		/* handle empty options */
		while (p < connect_string_end && *p == ';')
			++p;

		/* parse option */
		end = (const char *) memchr(p, '=', connect_string_end - p);
		if (!end)
			break;

		/* account for spaces between ;'s. */
		while (p < end && *p == ' ')
			++p;

		if ((end - p) >= (int) sizeof(option))
			option[0] = 0;
		else {
			memcpy(option, p, end - p);
			option[end - p] = 0;
		}

		/* parse value */
		p = end + 1;
		end = parse_value(errs, p, connect_string_end, &value);
		if (!end)
			goto Cleanup;

#define CHK_PARAM(p) (strcasecmp(option, odbc_param_##p) == 0 && (num_param=ODBC_PARAM_##p) >= 0)
		if (CHK_PARAM(Server)) {
			dest_s = &login->server_name;
			if (!parse_server(errs, tds_dstr_buf(&value), login))
				goto Cleanup;
		} else if (CHK_PARAM(Servername)) {
			if ((cfgs & CFG_DSN) != 0) {
				odbc_errs_add(errs, "HY000", "Only one between SERVERNAME and DSN can be specified");
				goto Cleanup;
			}
			if (!cfgs) {
				odbc_dstr_swap(&login->server_name, &value);
				tds_read_conf_file(login, tds_dstr_cstr(&login->server_name));
				cfgs = CFG_SERVERNAME;
				p = connect_string;
				continue;
			}
		} else if (CHK_PARAM(DSN)) {
			if ((cfgs & CFG_SERVERNAME) != 0) {
				odbc_errs_add(errs, "HY000", "Only one between SERVERNAME and DSN can be specified");
				goto Cleanup;
			}
			if (!cfgs) {
				if (!odbc_get_dsn_info(errs, tds_dstr_cstr(&value), login))
					goto Cleanup;
				cfgs = CFG_DSN;
				p = connect_string;
				continue;
			}
		} else if (CHK_PARAM(Database)) {
			dest_s = &login->database;
		} else if (CHK_PARAM(UID)) {
			dest_s = &login->user_name;
		} else if (CHK_PARAM(PWD)) {
			dest_s = &login->password;
		} else if (CHK_PARAM(APP)) {
			dest_s = &login->app_name;
		} else if (CHK_PARAM(WSID)) {
			dest_s = &login->client_host_name;
		} else if (CHK_PARAM(Language)) {
			tds_parse_conf_section(TDS_STR_LANGUAGE, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(Port)) {
			tds_parse_conf_section(TDS_STR_PORT, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(TDS_Version)) {
			tds_parse_conf_section(TDS_STR_VERSION, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(TextSize)) {
			tds_parse_conf_section(TDS_STR_TEXTSZ, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(PacketSize)) {
			tds_parse_conf_section(TDS_STR_BLKSZ, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(ClientCharset)
			   ||  strcasecmp(option, "client_charset") == 0) {
			num_param = ODBC_PARAM_ClientCharset;
			tds_parse_conf_section(TDS_STR_CLCHARSET, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(DumpFile)) {
			tds_parse_conf_section(TDS_STR_DUMPFILE, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(DumpFileAppend)) {
			tds_parse_conf_section(TDS_STR_APPENDMODE, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(DebugFlags)) {
			tds_parse_conf_section(TDS_STR_DEBUGFLAGS, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(Encryption)) {
			tds_parse_conf_section(TDS_STR_ENCRYPTION, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(Encrypt)) {
			tds_parse_conf_section(TDS_STR_ENCRYPTION, odbc_encrypt2encryption(tds_dstr_cstr(&value)), login);
		} else if (CHK_PARAM(UseNTLMv2)) {
			tds_parse_conf_section(TDS_STR_USENTLMV2, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(REALM)) {
			tds_parse_conf_section(TDS_STR_REALM, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(ServerSPN)) {
			tds_parse_conf_section(TDS_STR_SPN, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(Trusted_Connection)) {
			trusted = tds_config_boolean(option, tds_dstr_cstr(&value), login);
			tdsdump_log(TDS_DBG_INFO1, "trusted %s -> %d\n", tds_dstr_cstr(&value), trusted);
			num_param = -1;
			/* TODO odbc_param_Address field */
		} else if (CHK_PARAM(MARS_Connection)) {
			if (tds_config_boolean(option, tds_dstr_cstr(&value), login))
				login->mars = 1;
		} else if (CHK_PARAM(AttachDbFilename)) {
			dest_s = &login->db_filename;
		} else if (CHK_PARAM(ApplicationIntent)) {
			const char *readonly_intent;

			if (strcasecmp(tds_dstr_cstr(&value), "ReadOnly") == 0) {
				readonly_intent = "yes";
			} else if (strcasecmp(tds_dstr_cstr(&value), "ReadWrite") == 0) {
				readonly_intent = "no";
			} else {
				tdsdump_log(TDS_DBG_ERROR, "Invalid ApplicationIntent %s\n", tds_dstr_cstr(&value));
				goto Cleanup;
			}

			tds_parse_conf_section(TDS_STR_READONLY_INTENT, readonly_intent, login);
			tdsdump_log(TDS_DBG_INFO1, "Application Intent %s\n", readonly_intent);
		} else if (CHK_PARAM(Timeout)) {
			tds_parse_conf_section(TDS_STR_TIMEOUT, tds_dstr_cstr(&value), login);
		} else if (CHK_PARAM(HostNameInCertificate)) {
			dest_s = &login->certificate_host_name;
		}

		if (num_param >= 0 && parsed_params) {
			parsed_params[num_param].p = p;
			parsed_params[num_param].len = end - p;
		}

		/* copy to destination */
		if (dest_s)
			odbc_dstr_swap(dest_s, &value);

		p = end;
		/* handle "" ";.." cases */
		if (p >= connect_string_end)
			break;
		++p;
	}

	if (trusted) {
		if (parsed_params) {
			parsed_params[ODBC_PARAM_Trusted_Connection].p = "Yes";
			parsed_params[ODBC_PARAM_Trusted_Connection].len = 3;
			parsed_params[ODBC_PARAM_UID].p = NULL;
			parsed_params[ODBC_PARAM_PWD].p = NULL;
		}
		tds_dstr_empty(&login->user_name);
		tds_dstr_empty(&login->password);
	}

	tds_dstr_free(&value);
	return true;

Cleanup:
	tds_dstr_free(&value);
	return false;
}

#ifdef _WIN32
int
odbc_build_connect_string(TDS_ERRS *errs, TDS_PARSED_PARAM *params, char **out)
{
	unsigned n;
	size_t len = 1;
	char *p;

	/* compute string size */
	for (n = 0; n < ODBC_PARAM_SIZE; ++n) {
		if (params[n].p)
			len += strlen(odbc_param_names[n]) + params[n].len + 2;
	}

	/* allocate */
	p = tds_new(char, len);
	if (!p) {
		odbc_errs_add(errs, "HY001", NULL);
		return 0;
	}
	*out = p;

	/* build it */
	for (n = 0; n < ODBC_PARAM_SIZE; ++n) {
		if (params[n].p)
			p += sprintf(p, "%s=%.*s;", odbc_param_names[n], (int) params[n].len, params[n].p);
	}
	*p = 0;
	return 1;
}
#endif

#if !HAVE_SQLGETPRIVATEPROFILESTRING

#ifdef _WIN32
#  error There is something wrong  in configuration...
#endif

typedef struct
{
	LPCSTR entry;
	LPSTR buffer;
	int buffer_len;
	int ret_val;
	int found;
}
ProfileParam;

static bool
tdoParseProfile(const char *option, const char *value, void *param)
{
	ProfileParam *p = (ProfileParam *) param;

	if (strcasecmp(p->entry, option) == 0) {
		strlcpy(p->buffer, value, p->buffer_len);

		p->ret_val = strlen(p->buffer);
		p->found = 1;
	}
	return true;
}

static int
SQLGetPrivateProfileString(LPCSTR pszSection, LPCSTR pszEntry, LPCSTR pszDefault, LPSTR pRetBuffer, int nRetBuffer,
			   LPCSTR pszFileName)
{
	FILE *hFile;
	ProfileParam param;

	tdsdump_log(TDS_DBG_FUNC, "SQLGetPrivateProfileString(%p, %p, %p, %p, %d, %p)\n", 
			pszSection, pszEntry, pszDefault, pRetBuffer, nRetBuffer, pszFileName);

	if (!pszSection) {
		/* spec says return list of all section names - but we will just return nothing */
		tdsdump_log(TDS_DBG_WARN, "WARNING: Functionality for NULL pszSection not implemented.\n");
		return 0;
	}

	if (!pszEntry) {
		/* spec says return list of all key names in section - but we will just return nothing */
		tdsdump_log(TDS_DBG_WARN, "WARNING: Functionality for NULL pszEntry not implemented.\n");
		return 0;
	}

	if (nRetBuffer < 1)
		tdsdump_log(TDS_DBG_WARN, "WARNING: No space to return a value because nRetBuffer < 1.\n");

	if (pszFileName && *pszFileName == '/')
		hFile = fopen(pszFileName, "r");
	else
		hFile = tdoGetIniFileName();

	if (hFile == NULL) {
		tdsdump_log(TDS_DBG_ERROR, "ERROR: Could not open configuration file\n");
		return 0;
	}

	param.entry = pszEntry;
	param.buffer = pRetBuffer;
	param.buffer_len = nRetBuffer;
	param.ret_val = 0;
	param.found = 0;

	pRetBuffer[0] = 0;
	tds_read_conf_section(hFile, pszSection, tdoParseProfile, &param);

	if (pszDefault && !param.found) {
		strlcpy(pRetBuffer, pszDefault, nRetBuffer);

		param.ret_val = strlen(pRetBuffer);
	}

	fclose(hFile);
	return param.ret_val;
}

static FILE *
tdoGetIniFileName(void)
{
	FILE *ret = NULL;
	char *p;
	char *fn;

	/*
	 * First, try the ODBCINI environment variable
	 */
	if ((p = getenv("ODBCINI")) != NULL)
		ret = fopen(p, "r");

	/*
	 * Second, try the HOME environment variable
	 */
	if (!ret && (p = tds_get_homedir()) != NULL) {
		fn = NULL;
		if (asprintf(&fn, "%s/.odbc.ini", p) > 0) {
			ret = fopen(fn, "r");
			free(fn);
		}
		free(p);
	}

	/*
	 * As a last resort, try SYS_ODBC_INI
	 */
	if (!ret)
		ret = fopen(SYS_ODBC_INI, "r");

	return ret;
}

#endif /* !HAVE_SQLGETPRIVATEPROFILESTRING */
