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

static char software_version[] = "$Id: connectparams.c,v 1.36 2003-04-01 21:43:22 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#ifndef HAVEODBCINST

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

	/* use old servername */
	tmp[0] = '\0';
	if (SQLGetPrivateProfileString(DSN, "Servername", "", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		freetds_conf_less = 0;
		tds_read_conf_file(connect_info, tmp);
	}

	/* search for server (compatible with ms one) */
	tmp[0] = '\0';
	if (freetds_conf_less && SQLGetPrivateProfileString(DSN, "Server", "localhost", tmp, FILENAME_MAX, "odbc.ini") > 0) {
		tds_dstr_copy(&connect_info->server_name, tmp);
		tds_lookup_host(connect_info->server_name, NULL, tmp, NULL);
		tds_dstr_copy(&connect_info->ip_addr, tmp);
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
	return 1;
}

/** 
 * Parse connection string and fill connect_info according
 * @param pszConnectString connect string
 * @param connect_info     where to store connection info
 * @return 1 if success 0 otherwhise */
int
tdoParseConnectString(char *pszConnectString, TDSCONNECTINFO * connect_info)
{
	char *p, *end;
	char **dest_s;
	int reparse = 0;	/* flag for indicate second parse of string */
	char option[16];
	char tmp[256];
	char temp_c;

	pszConnectString = strdup(pszConnectString);	/* copy the string so we can hack at it */
	for (p = pszConnectString;;) {
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

		temp_c = *end;
		*end = 0;
		if (strcasecmp(option, "SERVER") == 0) {
			/* ignore if servername specified */
			if (!reparse) {
				dest_s = &connect_info->server_name;
				tds_lookup_host(p, NULL, tmp, NULL);
				*end = temp_c;
				if (!tds_dstr_copy(&connect_info->ip_addr, tmp)) {
					free(pszConnectString); 
					return 0;
				}
			}
		} else if (strcasecmp(option, "SERVERNAME") == 0) {
			if (!reparse) {
				tds_read_conf_file(connect_info, p);
				reparse = 1;
				p = pszConnectString;
				*end = temp_c;
				continue;
			}
		} else if (strcasecmp(option, "DSN") == 0) {
			if (!reparse) {
				odbc_get_dsn_info(p, connect_info);
				reparse = 1;
				p = pszConnectString;
				*end = temp_c;
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
			connect_info->port = atoi(p);
		} else if (strcasecmp(option, "TDS_Version") == 0) {
			tds_config_verstr(p, connect_info);
		}
		*end = temp_c;

		/* copy to destination */
		if (dest_s) {
			if (!tds_dstr_copyn(dest_s, p, end - p)){
				free(pszConnectString); 
				return 0;
			}
		}

		p = end;
		/* handle "" ";.." "};.." cases */
		if (!*p)
			break;
		if (*p == '}')
			++p;
		++p;
	}

	free(pszConnectString); 
	return 1;
}

/* TODO: now even iODBC support SQLGetPrivateProfileString, best check */
#ifndef UNIXODBC

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

#endif
