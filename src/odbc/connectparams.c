/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#include "tds.h"
#include "tdsodbc.h"
#include "tdsstring.h"
#include "connectparams.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: connectparams.c,v 1.46 2003-08-01 15:51:29 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

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

#endif

/** 
 * Read connection information from given DSN
 * @param DSN           DSN name
 * @param connect_info  where to store connection info
 * @return 1 if success 0 otherwhise */
int
odbc_get_dsn_info(const char *DSN, TDSCONNECTINFO * connect_info)
{
	char tmp[FILENAME_MAX];
	int freetds_conf_less = 1;
	int address_specified = 0;

	/* use old servername */
	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Servername", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		freetds_conf_less = 0;
		tds_read_conf_file(connect_info, tmp);
	}

	/* search for server (compatible with ms one) */
	if (freetds_conf_less) {
		tmp[0] = '\0';
		if (SQLGetPrivateProfileString(DSN, "Address", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
			address_specified = 1;
			/* TODO parse like MS */
			tds_lookup_host(tmp, tmp);
			tds_dstr_copy(&connect_info->ip_addr, tmp);
		}

		tmp[0] = '\0';
		if (SQLGetPrivateProfileString(DSN, "Server", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
			tds_dstr_copy(&connect_info->server_name, tmp);
			if (!address_specified) {
				tds_lookup_host(tds_dstr_cstr(&connect_info->server_name), tmp);
				tds_dstr_copy(&connect_info->ip_addr, tmp);
			}
		}
	}

	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Port", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		connect_info->port = atoi(tmp);
	}

	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "TDS_Version", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		tds_config_verstr(tmp, connect_info);
	}

	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Language", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		tds_dstr_copy(&connect_info->language, tmp);
	}

	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Database", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		tds_dstr_copy(&connect_info->database, tmp);
	}
#if 0
	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Authentication", "Server", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		if (!strcasecmp(tmp, "Server")) {
			connect_info->try_domain_login = 0;
			connect_info->try_server_login = 1;
		} else if (!strcasecmp(tmp, "Domain")) {
			connect_info->try_domain_login = 1;
			connect_info->try_server_login = 0;
		} else if (!strcasecmp(tmp, "Both")) {
			connect_info->try_server_login = 1;
			connect_info->try_domain_login = 1;
		} else {
			/* default to server authentication */
			connect_info->try_domain_login = 0;
			connect_info->try_server_login = 1;
		}
	}
#endif

	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Domain", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		tds_dstr_copy(&connect_info->default_domain, tmp);
	}

	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "TextSize", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		connect_info->text_size = atoi(tmp);
	}

	return 1;
}

/** 
 * Parse connection string and fill connect_info according
 * @param pszConnectString connect string
 * @param connect_info     where to store connection info
 * @return 1 if success 0 otherwhise */
int
tdoParseConnectString(const char *pszConnectString, TDSCONNECTINFO * connect_info)
{
	const char *p, *end;
	DSTR *dest_s, value;
	int reparse = 0;	/* flag for indicate second parse of string */
	char option[16];
	char tmp[256];

	tds_dstr_init(&value);
	for (p = pszConnectString; p && *p;) {
		dest_s = NULL;

		/* parse option */
		end = strchr(p, '=');
		if (!end)
			break;

		/* account for spaces between ;'s. */
		while (p < end && *p == ' ')
			++p;

		if ((end - p) >= (int) sizeof(option))
			option[0] = 0;
		else {
			strncpy(option, p, end - p);
			option[end - p] = 0;
		}

		/* parse value */
		p = end + 1;
		if (*p == '{') {
			++p;
			end = strstr(p, "};");
		} else {
			end = strchr(p, ';');
		}
		if (!end)
			end = strchr(p, 0);

		if (!tds_dstr_copyn(&value, p, end - p))
			return 0;

		if (strcasecmp(option, "SERVER") == 0) {
			/* ignore if servername specified */
			if (!reparse) {
				dest_s = &connect_info->server_name;
				tds_lookup_host(tds_dstr_cstr(&value), tmp);
				if (!tds_dstr_copy(&connect_info->ip_addr, tmp)) {
					tds_dstr_free(&value);
					return 0;
				}
			}
		} else if (strcasecmp(option, "SERVERNAME") == 0) {
			if (!reparse) {
				tds_read_conf_file(connect_info, tds_dstr_cstr(&value));
				reparse = 1;
				p = pszConnectString;
				continue;
			}
		} else if (strcasecmp(option, "DSN") == 0) {
			if (!reparse) {
				odbc_get_dsn_info(tds_dstr_cstr(&value), connect_info);
				reparse = 1;
				p = pszConnectString;
				continue;
			}
		} else if (strcasecmp(option, "DATABASE") == 0) {
			dest_s = &connect_info->database;
		} else if (strcasecmp(option, "UID") == 0) {
			dest_s = &connect_info->user_name;
		} else if (strcasecmp(option, "PWD") == 0) {
			dest_s = &connect_info->password;
		} else if (strcasecmp(option, "APP") == 0) {
			dest_s = &connect_info->app_name;
		} else if (strcasecmp(option, "WSID") == 0) {
			dest_s = &connect_info->host_name;
		} else if (strcasecmp(option, "LANGUAGE") == 0) {
			dest_s = &connect_info->language;
		} else if (strcasecmp(option, "Port") == 0) {
			connect_info->port = atoi(tds_dstr_cstr(&value));
		} else if (strcasecmp(option, "TDS_Version") == 0) {
			tds_config_verstr(tds_dstr_cstr(&value), connect_info);
		} else if (strcasecmp(option, "Domain") == 0) {
			dest_s = &connect_info->default_domain;
		} else if (strcasecmp(option, "TextSize") == 0) {
			connect_info->text_size = atoi(tds_dstr_cstr(&value));
			/* TODO "Address" field */
		}

		/* copy to destination */
		if (dest_s) {
			tds_dstr_set(dest_s, tds_dstr_cstr(&value));
			tds_dstr_init(&value);
		}

		p = end;
		/* handle "" ";.." "};.." cases */
		if (!*p)
			break;
		if (*p == '}')
			++p;
		++p;
	}

	tds_dstr_free(&value);
	return p != NULL;
}

#if !HAVE_SQLGETPRIVATEPROFILESTRING

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
		strncpy(p->buffer, value, p->buffer_len);
		p->buffer[p->buffer_len - 1] = '\0';

		p->ret_val = strlen(p->buffer);
		p->found = 1;
	}
}

int
SQLGetPrivateProfileString(LPCSTR pszSection, LPCSTR pszEntry, LPCSTR pszDefault, LPSTR pRetBuffer, int nRetBuffer,
			   LPCSTR pszFileName)
{
	FILE *hFile;
	ProfileParam param;

	if (!pszSection) {
		/* spec says return list of all section names - but we will just return nothing */
		fprintf(stderr, "[FreeTDS][ODBC][%s][%d] WARNING: Functionality for NULL pszSection not implemented.\n", __FILE__,
			__LINE__);
		return 0;
	}

	if (!pszEntry) {
		/* spec says return list of all key names in section - but we will just return nothing */
		fprintf(stderr, "[FreeTDS][ODBC][%s][%d] WARNING: Functionality for NULL pszEntry not implemented.\n", __FILE__,
			__LINE__);
		return 0;
	}

	if (nRetBuffer < 1)
		fprintf(stderr, "[FreeTDS][ODBC][%s][%d] WARNING: No space to return a value because nRetBuffer < 1.\n", __FILE__,
			__LINE__);

	if (pszFileName && *pszFileName == '/')
		hFile = fopen(pszFileName, "r");
	else
		hFile = tdoGetIniFileName();

	if (hFile == NULL) {
		fprintf(stderr, "[FreeTDS][ODBC][%s][%d] ERROR: Could not open configuration file\n", __FILE__, __LINE__);
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
		strncpy(pRetBuffer, pszDefault, nRetBuffer);
		pRetBuffer[nRetBuffer - 1] = '\0';

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

static const char *aTDSver[] = {
	"",
	"4.2",
	"5.0",
	"7.0",
	"8.0",
	NULL
};

static const char *aLanguage[] = {
	"us_english",
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

int
ODBCINSTGetProperties(HODBCINSTPROPERTY hLastProperty)
{
	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Servername", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("Name of FreeTDS connection to connect to.\n"
						 "This server name refer to entry in freetds.conf file, not real server name.\n"
						 "This property cannot be used with Server property.");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Server", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("Name of server to connect to.\n"
						 "This should be the name of real server.\n"
						 "This property cannot be used with Servername property.");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Address", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("The hostname or ip address of the server.");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Port", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "1433", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("TCP/IP Port to connect to.");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Database", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("Default database.");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_LISTBOX;
	hLastProperty->aPromptData = malloc(sizeof(aTDSver));
	memcpy(hLastProperty->aPromptData, aTDSver, sizeof(aTDSver));
	strncpy(hLastProperty->szName, "TDS_Version", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "4.2", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("The TDS protocol version.\n"
						 " 4.2 MSSQL 6.5 or Sybase < 10.x\n"
						 " 5.0 Sybase >= 10.x\n" " 7.0 MSSQL 7 or MSSQL 2000\n" " 8.0 MSSQL 2000");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_COMBOBOX;
	hLastProperty->aPromptData = malloc(sizeof(aLanguage));
	memcpy(hLastProperty->aPromptData, aLanguage, sizeof(aLanguage));
	strncpy(hLastProperty->szName, "Language", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "us_english", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("The default language setting.");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_HIDDEN;
	strncpy(hLastProperty->szName, "TextSize", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("Text datatype limit.");

	/* ??? in odbc.ini ??? */
/*
	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "UID", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("User ID (Beware of security issues).");

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "PWD", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("Password (Beware of security issues).");
*/

/*
	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_LISTBOX;
	hLastProperty->aPromptData = malloc(sizeof(aAuth));
	memcpy(hLastProperty->aPromptData, aAuth, sizeof(aAuth));
	strncpy(hLastProperty->szName, "Authentication", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "Server", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("The server authentication mechanism.");
*/

	hLastProperty->pNext = (HODBCINSTPROPERTY) malloc(sizeof(ODBCINSTPROPERTY));
	hLastProperty = hLastProperty->pNext;
	memset(hLastProperty, 0, sizeof(ODBCINSTPROPERTY));
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_TEXTEDIT;
	strncpy(hLastProperty->szName, "Domain", INI_MAX_PROPERTY_NAME);
	strncpy(hLastProperty->szValue, "", INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup("The default domain to use when using Domain Authentication.");

	return 1;
}

#endif
