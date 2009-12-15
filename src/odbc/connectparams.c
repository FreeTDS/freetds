/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005-2008  Frediano Ziglio
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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "tdsodbc.h"
#include "tdsstring.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: connectparams.c,v 1.79 2009-12-15 09:53:27 freddy77 Exp $");

static const char odbc_param_Servername[] = "Servername";
static const char odbc_param_Address[] = "Address";
static const char odbc_param_Server[] = "Server";
static const char odbc_param_Port[] = "Port";
static const char odbc_param_TDS_Version[] = "TDS_Version";
static const char odbc_param_Language[] = "Language";
static const char odbc_param_Database[] = "Database";
static const char odbc_param_TextSize[] = "TextSize";
static const char odbc_param_PacketSize[] = "PacketSize";
static const char odbc_param_ClientCharset[] = "ClientCharset";
static const char odbc_param_DumpFile[] = "DumpFile";
static const char odbc_param_DumpFileAppend[] = "DumpFileAppend";
static const char odbc_param_DebugFlags[] = "DebugFlags";
static const char odbc_param_Encryption[] = "Encryption";

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
parse_server(TDS_DBC *dbc, char *server, TDSCONNECTION * connection)
{
	char ip[64];
	char *p = (char *) strchr(server, '\\');

	if (p) {
		if (!tds_dstr_copy(&connection->instance_name, p+1)) {
			odbc_errs_add(&dbc->errs, "HY001", NULL);
			return 0;
		}
		*p = 0;
	}

	tds_lookup_host(server, ip);
	if (!tds_dstr_copy(&connection->ip_addr, ip)) {
		odbc_errs_add(&dbc->errs, "HY001", NULL);
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
 * Read connection information from given DSN
 * @param DSN           DSN name
 * @param connection    where to store connection info
 * @return 1 if success 0 otherwhise
 */
int
odbc_get_dsn_info(TDS_DBC *dbc, const char *DSN, TDSCONNECTION * connection)
{
	char tmp[FILENAME_MAX];
	int freetds_conf_less = 1;

	/* use old servername */
	if (myGetPrivateProfileString(DSN, odbc_param_Servername, tmp) > 0) {
		freetds_conf_less = 0;
		tds_dstr_copy(&connection->server_name, tmp);
		tds_read_conf_file(connection, tmp);
		if (myGetPrivateProfileString(DSN, odbc_param_Server, tmp) > 0) {
			odbc_errs_add(&dbc->errs, "HY000", "You cannot specify both SERVERNAME and SERVER");
			return 0;
		}
		if (myGetPrivateProfileString(DSN, odbc_param_Address, tmp) > 0) {
			odbc_errs_add(&dbc->errs, "HY000", "You cannot specify both SERVERNAME and ADDRESS");
			return 0;
		}
	}

	/* search for server (compatible with ms one) */
	if (freetds_conf_less) {
		int address_specified = 0;

		if (myGetPrivateProfileString(DSN, odbc_param_Address, tmp) > 0) {
			address_specified = 1;
			/* TODO parse like MS */
			tds_lookup_host(tmp, tmp);
			tds_dstr_copy(&connection->ip_addr, tmp);
		}
		if (myGetPrivateProfileString(DSN, odbc_param_Server, tmp) > 0) {
			tds_dstr_copy(&connection->server_name, tmp);
			if (!address_specified) {
				if (!parse_server(dbc, tmp, connection))
					return 0;
			}
		}
	}

	if (myGetPrivateProfileString(DSN, odbc_param_Port, tmp) > 0)
		tds_parse_conf_section(TDS_STR_PORT, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_TDS_Version, tmp) > 0)
		tds_parse_conf_section(TDS_STR_VERSION, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_Language, tmp) > 0)
		tds_parse_conf_section(TDS_STR_LANGUAGE, tmp, connection);

	if (tds_dstr_isempty(&connection->database)
	    && myGetPrivateProfileString(DSN, odbc_param_Database, tmp) > 0)
		tds_dstr_copy(&connection->database, tmp);

	if (myGetPrivateProfileString(DSN, odbc_param_TextSize, tmp) > 0)
		tds_parse_conf_section(TDS_STR_TEXTSZ, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_PacketSize, tmp) > 0)
		tds_parse_conf_section(TDS_STR_BLKSZ, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_ClientCharset, tmp) > 0)
		tds_parse_conf_section(TDS_STR_CLCHARSET, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_DumpFile, tmp) > 0)
		tds_parse_conf_section(TDS_STR_DUMPFILE, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_DumpFileAppend, tmp) > 0)
		tds_parse_conf_section(TDS_STR_APPENDMODE, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_DebugFlags, tmp) > 0)
		tds_parse_conf_section(TDS_STR_DEBUGFLAGS, tmp, connection);

	if (myGetPrivateProfileString(DSN, odbc_param_Encryption, tmp) > 0)
		tds_parse_conf_section(TDS_STR_ENCRYPTION, tmp, connection);

	return 1;
}

/** 
 * Parse connection string and fill connection according
 * @param connect_string     connect string
 * @param connect_string_end connect string end (pointer to char past last)
 * @param connection         where to store connection info
 * @return 1 if success 0 otherwhise
 */
int
odbc_parse_connect_string(TDS_DBC *dbc, const char *connect_string, const char *connect_string_end, TDSCONNECTION * connection)
{
	const char *p, *end;
	DSTR *dest_s, value;
	enum { CFG_DSN = 1, CFG_SERVER = 2, CFG_SERVERNAME = 4 };
	unsigned int cfgs = 0;	/* flags for indicate second parse of string */
	char option[16];

	tds_dstr_init(&value);
	for (p = connect_string; p < connect_string_end && *p;) {
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
		if (*p == '{') {
			++p;
			/* search "};" */
			end = p;
			while ((end = (const char *) memchr(end, '}', connect_string_end - end)) != NULL) {
				if ((end + 1) != connect_string_end && end[1] == ';')
					break;
				++end;
			}
		} else {
			end = (const char *) memchr(p, ';', connect_string_end - p);
		}
		if (!end)
			end = connect_string_end;

		if (!tds_dstr_copyn(&value, p, end - p)) {
			odbc_errs_add(&dbc->errs, "HY001", NULL);
			return 0;
		}

		if (strcasecmp(option, "SERVER") == 0) {
			/* error if servername or DSN specified */
			if ((cfgs & (CFG_DSN|CFG_SERVERNAME)) != 0) {
				tds_dstr_free(&value);
				odbc_errs_add(&dbc->errs, "HY000", "Only one between SERVER, SERVERNAME and DSN can be specified");
				return 0;
			}
			if (!cfgs) {
				dest_s = &connection->server_name;
				/* not that safe cast but works -- freddy77 */
				if (!parse_server(dbc, (char *) tds_dstr_cstr(&value), connection)) {
					tds_dstr_free(&value);
					return 0;
				}
				cfgs = CFG_SERVER;
			}
		} else if (strcasecmp(option, "SERVERNAME") == 0) {
			if ((cfgs & (CFG_DSN|CFG_SERVER)) != 0) {
				tds_dstr_free(&value);
				odbc_errs_add(&dbc->errs, "HY000", "Only one between SERVER, SERVERNAME and DSN can be specified");
				return 0;
			}
			if (!cfgs) {
				tds_dstr_dup(&connection->server_name, &value);
				tds_read_conf_file(connection, tds_dstr_cstr(&value));
				cfgs = CFG_SERVERNAME;
				p = connect_string;
				continue;
			}
		} else if (strcasecmp(option, "DSN") == 0) {
			if ((cfgs & (CFG_SERVER|CFG_SERVERNAME)) != 0) {
				tds_dstr_free(&value);
				odbc_errs_add(&dbc->errs, "HY000", "Only one between SERVER, SERVERNAME and DSN can be specified");
				return 0;
			}
			if (!cfgs) {
				if (!odbc_get_dsn_info(dbc, tds_dstr_cstr(&value), connection)) {
					tds_dstr_free(&value);
					return 0;
				}
				cfgs = CFG_DSN;
				p = connect_string;
				continue;
			}
		} else if (strcasecmp(option, "DATABASE") == 0) {
			dest_s = &connection->database;
		} else if (strcasecmp(option, "UID") == 0) {
			dest_s = &connection->user_name;
		} else if (strcasecmp(option, "PWD") == 0) {
			dest_s = &connection->password;
		} else if (strcasecmp(option, "APP") == 0) {
			dest_s = &connection->app_name;
		} else if (strcasecmp(option, "WSID") == 0) {
			dest_s = &connection->client_host_name;
		} else if (strcasecmp(option, "LANGUAGE") == 0) {
			tds_parse_conf_section(TDS_STR_LANGUAGE, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_Port) == 0) {
			tds_parse_conf_section(TDS_STR_PORT, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_TDS_Version) == 0) {
			tds_parse_conf_section(TDS_STR_VERSION, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_TextSize) == 0) {
			tds_parse_conf_section(TDS_STR_TEXTSZ, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_PacketSize) == 0) {
			tds_parse_conf_section(TDS_STR_BLKSZ, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_ClientCharset) == 0) {
			tds_parse_conf_section(TDS_STR_CLCHARSET, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_DumpFile) == 0) {
			tds_parse_conf_section(TDS_STR_DUMPFILE, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_DumpFileAppend) == 0) {
			tds_parse_conf_section(TDS_STR_APPENDMODE, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_DebugFlags) == 0) {
			tds_parse_conf_section(TDS_STR_DEBUGFLAGS, tds_dstr_cstr(&value), connection);
		} else if (strcasecmp(option, odbc_param_Encryption) == 0) {
			tds_parse_conf_section(TDS_STR_ENCRYPTION, tds_dstr_cstr(&value), connection);
			/* TODO odbc_param_Address field */
		}

		/* copy to destination */
		if (dest_s) {
			DSTR tmp = *dest_s;
			*dest_s = value;
			value = tmp;
		}

		p = end;
		/* handle "" ";.." "};.." cases */
		if (p >= connect_string_end)
			break;
		if (*p == '}')
			++p;
		++p;
	}

	tds_dstr_free(&value);
	return 1;
}

#if !HAVE_SQLGETPRIVATEPROFILESTRING

#ifdef WIN32
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

static void
tdoParseProfile(const char *option, const char *value, void *param)
{
	ProfileParam *p = (ProfileParam *) param;

	if (strcasecmp(p->entry, option) == 0) {
		tds_strlcpy(p->buffer, value, p->buffer_len);

		p->ret_val = strlen(p->buffer);
		p->found = 1;
	}
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
		tds_strlcpy(pRetBuffer, pszDefault, nRetBuffer);

		param.ret_val = strlen(pRetBuffer);
	}

	fclose(hFile);
	return param.ret_val;
}

static FILE *
tdoGetIniFileName()
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

#ifdef UNIXODBC

/*
 * Begin BIG Hack.
 *  
 * We need these from odbcinstext.h but it wants to 
 * include <log.h> and <ini.h>, which are not in the 
 * standard include path.  XXX smurph
 * confirmed by unixODBC stuff, odbcinstext.h shouldn't be installed. freddy77
 */
#define     INI_MAX_LINE            1000
#define     INI_MAX_OBJECT_NAME     INI_MAX_LINE
#define     INI_MAX_PROPERTY_NAME   INI_MAX_LINE
#define     INI_MAX_PROPERTY_VALUE  INI_MAX_LINE

#define	ODBCINST_PROMPTTYPE_LABEL		0	/* readonly */
#define	ODBCINST_PROMPTTYPE_TEXTEDIT	1
#define	ODBCINST_PROMPTTYPE_LISTBOX		2
#define	ODBCINST_PROMPTTYPE_COMBOBOX	3
#define	ODBCINST_PROMPTTYPE_FILENAME	4
#define	ODBCINST_PROMPTTYPE_HIDDEN	    5

typedef struct tODBCINSTPROPERTY
{
	struct tODBCINSTPROPERTY *pNext;	/* pointer to next property, NULL if last property                                                                              */

	char szName[INI_MAX_PROPERTY_NAME + 1];	/* property name                                                                                                                                                */
	char szValue[INI_MAX_PROPERTY_VALUE + 1];	/* property value                                                                                                                                               */
	int nPromptType;	/* PROMPTTYPE_TEXTEDIT, PROMPTTYPE_LISTBOX, PROMPTTYPE_COMBOBOX, PROMPTTYPE_FILENAME    */
	char **aPromptData;	/* array of pointers terminated with a NULL value in array.                                                     */
	char *pszHelp;		/* help on this property (driver setups should keep it short)                                                   */
	void *pWidget;		/* CALLER CAN STORE A POINTER TO ? HERE                                                                                                 */
	int bRefresh;		/* app should refresh widget ie Driver Setup has changed aPromptData or szValue                 */
	void *hDLL;		/* for odbcinst internal use... only first property has valid one                                               */
}
ODBCINSTPROPERTY, *HODBCINSTPROPERTY;

/* 
 * End BIG Hack.
 */

int ODBCINSTGetProperties(HODBCINSTPROPERTY hLastProperty);

static const char *const aTDSver[] = {
	"",
	"4.2",
	"5.0",
	"7.0",
	"7.1",
	"7.2",
	NULL
};

static const char *const aLanguage[] = {
	"us_english",
	NULL
};

static const char *const aEncryption[] = {
	TDS_STR_ENCRYPTION_OFF,
	TDS_STR_ENCRYPTION_REQUEST,
	TDS_STR_ENCRYPTION_REQUIRE,
	NULL
};

static const char *const aBoolean[] = {
	"Yes",
	"No",
	NULL
};

/*
static const char *aAuth[] = {
	"Server",
	"Domain",
	"Both",
	NULL
};
*/

static HODBCINSTPROPERTY
addProperty(HODBCINSTPROPERTY hLastProperty)
{
	hLastProperty->pNext = (HODBCINSTPROPERTY) calloc(1, sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	return hLastProperty;
}

static HODBCINSTPROPERTY
definePropertyString(HODBCINSTPROPERTY hLastProperty, const char *name, const char *value, const char *comment)
{
	hLastProperty = addProperty(hLastProperty);
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	tds_strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	tds_strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup(comment);
	return hLastProperty;
}

static HODBCINSTPROPERTY
definePropertyBoolean(HODBCINSTPROPERTY hLastProperty, const char *name, const char *value, const char *comment)
{
	hLastProperty = addProperty(hLastProperty);
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_LISTBOX;
	hLastProperty->aPromptData = malloc(sizeof(aBoolean));
	memcpy(hLastProperty->aPromptData, aBoolean, sizeof(aBoolean));
	tds_strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	tds_strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup(comment);
	return hLastProperty;
}

static HODBCINSTPROPERTY
definePropertyHidden(HODBCINSTPROPERTY hLastProperty, const char *name, const char *value, const char *comment)
{
	hLastProperty = addProperty(hLastProperty);
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_HIDDEN;
	tds_strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	tds_strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup(comment);
	return hLastProperty;
}

static HODBCINSTPROPERTY
definePropertyList(HODBCINSTPROPERTY hLastProperty, const char *name, const char *value, const void *list, int size, const char *comment)
{
	hLastProperty = addProperty(hLastProperty);
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_LISTBOX;
	hLastProperty->aPromptData = malloc(size);
	memcpy(hLastProperty->aPromptData, list, size);
	tds_strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	tds_strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup(comment);
	return hLastProperty;
}

int
ODBCINSTGetProperties(HODBCINSTPROPERTY hLastProperty)
{
	hLastProperty = definePropertyString(hLastProperty, odbc_param_Servername, "", 
		"Name of FreeTDS connection to connect to.\n"
		"This server name refer to entry in freetds.conf file, not real server name.\n"
		"This property cannot be used with Server property.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_Server, "", 
		"Name of server to connect to.\n"
		"This should be the name of real server.\n"
		"This property cannot be used with Servername property.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_Address, "", 
		"The hostname or ip address of the server.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_Port, "1433", 
		"TCP/IP Port to connect to.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_Database, "", 
		"Default database.");

	hLastProperty = definePropertyList(hLastProperty, odbc_param_TDS_Version, "4.2", (void*) aTDSver, sizeof(aTDSver),
		"The TDS protocol version.\n"
		" 4.2 MSSQL 6.5 or Sybase < 10.x\n"
		" 5.0 Sybase >= 10.x\n"
		" 7.0 MSSQL 7\n"
		" 7.1 MSSQL 2000\n"
		" 7.2 MSSQL 2005");

	hLastProperty = definePropertyList(hLastProperty, odbc_param_Language, "us_english", (void*) aLanguage, sizeof(aLanguage),
		"The default language setting.");

	hLastProperty = definePropertyHidden(hLastProperty, odbc_param_TextSize, "", 
		"Text datatype limit.");

	/* ??? in odbc.ini ??? */
/*
	hLastProperty = definePropertyString(hLastProperty, odbc_param_UID, "", 
		"User ID (Beware of security issues).");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_PWD, "", 
		"Password (Beware of security issues).");
*/

/*
	hLastProperty = definePropertyList(hLastProperty, odbc_param_Authentication, "Server", aAuth, sizeof(aAuth),
		"The server authentication mechanism.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_Domain, "", 
		"The default domain to use when using Domain Authentication.");
*/

	hLastProperty = definePropertyString(hLastProperty, odbc_param_PacketSize, "", 
		"Size of network packets.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_ClientCharset, "", 
		"The client character set name to convert application characters to UCS-2 in TDS 7.0 and higher.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_DumpFile, "",
		"Specifies the location of a tds dump file and turns on logging.");

	hLastProperty = definePropertyBoolean(hLastProperty, odbc_param_DumpFileAppend, "",
		"Appends dump file instead of overwriting it. Useful for debugging when many processes are active.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_DebugFlags, "", 
		"Sets granularity of logging. A set of bit that specify levels and informations. See table below for bit specification.");

	hLastProperty = definePropertyList(hLastProperty, odbc_param_Encryption, TDS_STR_ENCRYPTION_OFF, aEncryption, sizeof(aEncryption),
		"The encryption method.");

	return 1;
}

#endif
