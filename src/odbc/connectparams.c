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
#include <sys/stat.h>
#include <sqltypes.h>

#include "tds.h"
#include "tdsstring.h"
#include "connectparams.h"
#include "replacements.h"

#ifndef HAVEODBCINST

/*
 * Last resort place to check for INI file. This is usually set at compile time
 * by build scripts.
 */
#ifndef SYS_ODBC_INI
#define SYS_ODBC_INI "/etc/odbc.ini"
#endif

/*
 * Internal buffer used when reading INI files. This should probably
 * be removed to make safer for threading.
 */
#define max_line 256
static char line[max_line];

/*****************************
 * tdoGetIniFileName
 *
 * PURPOSE
 *
 *  Call this to get the INI file name containing Data Source Names.
 *
 * ARGS
 *
 *  ppszFileName (W)    - send in a pointer with no allocation and get back
 *                        the file name - remember to free() it when done!
 *                      
 * RETURNS
 *
 *  0                   - failed
 *  1                   - worked
 *
 * NOTES:
 *
 *  - rules for determining the location of ODBC config may be different then what you    
 *    expect - at this time they differ from unixODBC 
 *
 *****************************/
static int tdoGetIniFileName( char **ppszFileName );

/*****************************
 * tdoFileExists
 *
 * PURPOSE
 *
 *  Call this to see if a file exists.
 *
 * ARGS
 *
 *  pszFileName (R)     - file to check for
 *                      
 * RETURNS
 *
 *  0                   - stat failed
 *  1                   - stat worked
 *
 *****************************/
static int tdoFileExists    ( const char *pszFileName );

/*****************************
 * tdoFindSection
 *
 * PURPOSE
 *
 *  Call this to goto a section in an INI file.
 *
 * ARGS
 *
 *  hFile (R)           - open file
 *  pszSection (R)      - desired section
 *                      
 * RETURNS
 *
 *  0                   - stat failed
 *  1                   - stat worked
 *
 *****************************/
static int tdoFindSection   ( FILE *hFile, const char *pszSection );

/*****************************
 * tdoGetNextEntry
 *
 * PURPOSE
 *
 *  Call this to goto, and read, the next entry in an INI file.
 *
 * ARGS
 *
 *  hFile (R)           - open file
 *  ppszKey (W)         - name of entry
 *  ppszValue (W)       - value of entry
 *                      
 * RETURNS
 *
 *  0                   - failed
 *  1                   - worked
 *
 * NOTE
 *
 *  - IF worked THEN ppszKey and ppszValue will be refs into an internal buffer
 *    as such they are not valid very long (after next call) and should not be 
 *    free()'d.
 *
 *****************************/
static int tdoGetNextEntry  ( FILE *hFile, char **ppszKey, char **ppszValue );

#endif

int tdoParseConnectString( char *pszConnectString, TDSCONNECTINFO* connect_info)
{
char *p,*end; /*,*dest; */
char **dest_s;
int *dest_i;
char *tdsver;

	for(p=pszConnectString;;) {
		end = strchr(p,'=');
		if (!end) break;

		dest_s = NULL;
		dest_i = NULL;
		tds_dstr_init(&tdsver);
		*end = 0;
		/* TODO 
		trusted_connection = yes/no
		*/
		if (strcasecmp(p,"server")==0) {
			dest_s = &connect_info->server_name;
/*		} else if (strcasecmp(p,"servername")==0) {
			dest = pszServer; */
		} else if (strcasecmp(p,"database")==0) {
			dest_s = &connect_info->database;
		} else if (strcasecmp(p,"UID")==0) {
			dest_s = &connect_info->user_name;
		} else if (strcasecmp(p,"PWD")==0) {
			dest_s = &connect_info->password;
		} else if (strcasecmp(p,"APP")==0) {
			dest_s = &connect_info->app_name;
		} else if (strcasecmp(p,"port")==0) {
			dest_i = &connect_info->port;
		} else if (strcasecmp(p,"tds_version")==0) {
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
			/* TODO check memory problems */
			tds_dstr_copyn(dest_s,p,cplen);
		} else if (dest_i) {
			/* TODO finish */
			*dest_i = atoi(p);
		}
		if (dest_s == &tdsver) {
			tds_config_verstr(tdsver,connect_info);
			tds_dstr_free(&tdsver);
		
		}
		if (dest_s == &connect_info->server_name) {
			char tmp[256];
			tds_lookup_host (connect_info->server_name, NULL, tmp, NULL);
			tds_dstr_copy(&connect_info->ip_addr,tmp);
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

int SQLGetPrivateProfileString( LPCSTR  pszSection,
                                LPCSTR  pszEntry,
                                LPCSTR  pszDefault,
                                LPSTR   pRetBuffer,
                                int     nRetBuffer,
                                LPCSTR  pszFileName
                              )
{
    FILE *  hFile;
    char *  pszKey;
    char *  pszValue;
    char *  pszRealFileName;
    int     nRetVal = 0;

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
        pszRealFileName = strdup( pszFileName );
    else if ( tdoGetIniFileName( &pszRealFileName ) )
        ;
    else
    {
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] ERROR: Unable to determine location of ODBC config data. Try setting ODBCINI environment variable.\n", __FILE__, __LINE__ );
        return 0;
    }

    if ( (hFile = fopen( pszRealFileName, "r" )) == NULL )
    {
        free( pszRealFileName );
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] ERROR: Could not open %s\n", __FILE__, __LINE__, pszRealFileName );
        return 0;
    }

    /* goto to start of section */
    if ( !tdoFindSection( hFile, pszSection ) )
    {
        goto SQLGetPrivateProfileStringExit;
    }

    /* scan entries for pszEntry */
    while ( tdoGetNextEntry( hFile, &pszKey, &pszValue ) )
    {
        if ( strcasecmp( pszKey, pszEntry ) == 0 )
        {
            strncpy( pRetBuffer, pszValue, nRetBuffer );
            pRetBuffer[nRetBuffer-1] = '\0';

            nRetVal = strlen( pRetBuffer );
            goto SQLGetPrivateProfileStringExit;
        }
    }

    if ( pszDefault && nRetBuffer > 0 )
    {
        strncpy( pRetBuffer, pszDefault, nRetBuffer );
        pRetBuffer[nRetBuffer-1] = '\0';

        nRetVal = strlen( pRetBuffer );
        goto SQLGetPrivateProfileStringExit;
    }

SQLGetPrivateProfileStringExit:
    free( pszRealFileName );
    fclose( hFile );   
    return nRetVal;
}

static int tdoGetIniFileName( char **ppszFileName )
{
    char *pszEnvVar;

    /*
     * First, try the ODBCINI environment variable
     */
    if ( (pszEnvVar = getenv( "ODBCINI" )) != NULL)
    {
        if ( tdoFileExists( pszEnvVar ) )
        {
            *ppszFileName = strdup( pszEnvVar );
            return 1;
        }
    }

    /*
     * Second, try the HOME environment variable
     */
    if ( (pszEnvVar = getenv( "HOME" )) != NULL)
    {
        char pszFileName[FILENAME_MAX+1];

        sprintf( pszFileName, "%s/.odbc.ini", pszEnvVar );

        if ( tdoFileExists( pszFileName ) )
        {
            *ppszFileName = strdup( pszFileName );
            return 1;
        }
    }

    /*
     * As a last resort, try SYS_ODBC_INI
     */
    if ( tdoFileExists( SYS_ODBC_INI ) )
    {
        *ppszFileName = strdup( SYS_ODBC_INI );
        return 1;
    }

    return 0;
}

static int tdoFileExists( const char *pszFileName )
{
    struct stat statFile;

    return ( stat( pszFileName, &statFile ) == 0 );
}

static int tdoFindSection( FILE *hFile, const char *pszSection )
{
    char*   s;
    char    sectionPattern[max_line];
    int     len;

    strcpy( sectionPattern, "[" );
    strcat( sectionPattern, pszSection );
    strcat( sectionPattern, "]" );

    s = fgets( line, max_line, hFile );
    while ( s != NULL )
    {
        /*
         * Get rid of the newline character
         */
        len = strlen( line );
        if (len > 0) line[strlen (line) - 1] = '\0';
        /*
         * look for the section header
         */
        if ( strcmp( line, sectionPattern ) == 0 )
            return 1;

        s = fgets( line, max_line, hFile );
    }

    return 0;
}

static int tdoGetNextEntry( FILE *hFile, char **ppszKey, char **ppszValue )
{
    char* s;
    int len;
    char equals[] = "="; /* used for separator for strtok */
    char* token;
    char* ptr;

    if ( ppszKey == NULL || ppszValue == NULL)
    {
        fprintf( stderr, "[FreeTDS][ODBC][%s][%d] ERROR: Invalid argument.\n", __FILE__, __LINE__ );
        return 0;
    }

    s = fgets( line, max_line, hFile );
    if (s == NULL)
    {
        perror( "[FreeTDS][ODBC] ERROR: fgets" );
        return 0;
    }

    /*
     * Get rid of the newline character
     */
    len = strlen (line);
    if (len > 0) line[strlen (line) - 1] = '\0';
    /*
     * Extract name from name = value
     */
    if ((token = strtok_r(line, equals, &ptr)) == NULL) return 0;

    len = strlen (token);
    while (len > 0 && isspace(token[len-1]))
    {
        len--;
        token[len] = '\0';
    }
    *ppszKey = token;

    /*
     * extract value from name = value
     */
    token = strtok_r(NULL, equals, &ptr);
    if (token == NULL) return 0;
    while (*token && isspace(token[0]))
        token++;

    *ppszValue = token;

    return 1;
}


#endif


