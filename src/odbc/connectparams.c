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
#include <sqltypes.h>

#include "tds.h"
#include "tdsstring.h"
#include "connectparams.h"
#include "replacements.h"

static char  software_version[]   = "$Id: connectparams.c,v 1.19 2002-10-27 19:59:17 freddy77 Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

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
static FILE* tdoGetIniFileName(void);

#endif

/** 
 * Parse connection string and fill connect_info according
 * @param pszConnectString connect string
 * @param connect_info     where to store connection info
 * @return 1 if success 0 otherwhise */
int tdoParseConnectString( char *pszConnectString, TDSCONNECTINFO* connect_info)
{
char *p,*end; /*,*dest; */
char **dest_s;
int *dest_i;
char *tdsver;
char *server_name;
int reparse = 0; /* flag for indicate second parse of string */

	for(p=pszConnectString;;) {
		end = strchr(p,'=');
		if (!end) break;

		dest_s = NULL;
		dest_i = NULL;
		tds_dstr_init(&tdsver);
		tds_dstr_init(&server_name);
		*end = 0;
		if (strcasecmp(p,"SERVER")==0) {
			/* ignore if servername specified */
			if (reparse) dest_s = &connect_info->server_name;
		} else if (strcasecmp(p,"SERVERNAME")==0) {
			if (!reparse) dest_s = &server_name;
		} else if (strcasecmp(p,"DATABASE")==0) {
			dest_s = &connect_info->database;
		} else if (strcasecmp(p,"UID")==0) {
			dest_s = &connect_info->user_name;
		} else if (strcasecmp(p,"PWD")==0) {
			dest_s = &connect_info->password;
		} else if (strcasecmp(p,"APP")==0) {
			dest_s = &connect_info->app_name;
		} else if (strcasecmp(p,"WSID")==0) {
			dest_s = &connect_info->host_name;
		} else if (strcasecmp(p,"LANGUAGE")==0) {
			dest_s = &connect_info->language;
		} else if (strcasecmp(p,"Port")==0) {
			dest_i = &connect_info->port;
		} else if (strcasecmp(p,"TDS_Version")==0) {
			dest_s = &tdsver;
		}
		*end = '=';

		/* parse value */
		p = end + 1;
		if (*p == '{') {
			++p;
			end = strstr(p,"};");
		} else {
			end = strchr(p,';');
		}
		if (!end) end = strchr(p,0);
		
		/* copy to destination */
		if (dest_s) {
			int cplen = end - p;
			if (!tds_dstr_copyn(dest_s,p,cplen))
				return 0;
		} else if (dest_i) {
			*dest_i = atoi(p);
		}
		if (dest_s == &tdsver) {
			tds_config_verstr(tdsver,connect_info);
			tds_dstr_free(&tdsver);
		
		}
		if (dest_s == &connect_info->server_name) {
			char tmp[256];
			tds_lookup_host (connect_info->server_name, NULL, tmp, NULL);
			if (!tds_dstr_copy(&connect_info->ip_addr,tmp))
				return 0;
		}
		if (dest_s == &server_name) {
			tds_read_conf_file(connect_info, server_name);
			tds_dstr_free(&server_name);
			reparse = 1;
			p = pszConnectString;
			continue;
		}

		p = end;
		/* handle "" ";.." "};.." cases */
		if (!*p) break;
		if (*p == '}') ++p;
		++p;
	}

	return 1;
}

/* TODO: now even iODBC support SQLGetPrivateProfileString, best check */
#ifndef UNIXODBC

typedef struct {
	LPCSTR entry;
	LPSTR buffer;
	int buffer_len;
	int ret_val;
	int found;
} ProfileParam;

static void 
tdoParseProfile(const char* option, const char* value, void *param)
{
	ProfileParam *p = (ProfileParam*)param;

	if ( strcasecmp( p->entry, option ) == 0 )
	{
		strncpy( p->buffer, value, p->buffer_len);
		p->buffer[p->buffer_len-1] = '\0';

		p->ret_val = strlen( p->buffer );
		p->found = 1;
	}
}

int SQLGetPrivateProfileString( LPCSTR  pszSection,
                                LPCSTR  pszEntry,
                                LPCSTR  pszDefault,
                                LPSTR   pRetBuffer,
                                int     nRetBuffer,
                                LPCSTR  pszFileName
                              )
{
    FILE *  hFile;
	ProfileParam param;

    if ( !pszSection )
    {
        /* spec says return list of all section names - but we will just return nothing */
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] WARNING: Functionality for NULL pszSection not implemented.\n", __FILE__, __LINE__ );
        return 0;
    }

    if ( !pszEntry )
    {
        /* spec says return list of all key names in section - but we will just return nothing */
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] WARNING: Functionality for NULL pszEntry not implemented.\n", __FILE__, __LINE__ );
        return 0;
    }

    if ( nRetBuffer < 1 )
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] WARNING: No space to return a value because nRetBuffer < 1.\n", __FILE__, __LINE__ );

    if ( pszFileName && *pszFileName == '/' )
		hFile = fopen(pszFileName, "r");
    else 
		hFile = tdoGetIniFileName();

    if ( hFile == NULL )
    {
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] ERROR: Could not open configuration file\n", __FILE__, __LINE__);
        return 0;
    }

	param.entry = pszEntry;
	param.buffer = pRetBuffer;
	param.buffer_len = nRetBuffer;
	param.ret_val = 0;
	param.found = 0;

	pRetBuffer[0] = 0;
	tds_read_conf_section(hFile, pszSection, tdoParseProfile, &param);

	if ( pszDefault && !param.found )
    {
        strncpy( pRetBuffer, pszDefault, nRetBuffer );
        pRetBuffer[nRetBuffer-1] = '\0';

		param.ret_val = strlen( pRetBuffer );
    }

	fclose( hFile );
	return param.ret_val;
}

static FILE* tdoGetIniFileName()
{
	FILE* ret = NULL;
    char *pszEnvVar;

    /*
     * First, try the ODBCINI environment variable
     */
    if ( (pszEnvVar = getenv( "ODBCINI" )) != NULL)
		ret = fopen(pszEnvVar, "r");

    /*
     * Second, try the HOME environment variable
     */
	if ( !ret && (pszEnvVar = getenv( "HOME" )) != NULL)
    {
        char pszFileName[FILENAME_MAX+1];

        sprintf( pszFileName, "%s/.odbc.ini", pszEnvVar );

		ret = fopen(pszFileName, "r");
    }

    /*
     * As a last resort, try SYS_ODBC_INI
     */
	if (!ret)
		ret = fopen(SYS_ODBC_INI, "r");

	return ret;
}

#endif
