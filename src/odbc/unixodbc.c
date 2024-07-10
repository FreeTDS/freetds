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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freetds/odbc.h>
#include <freetds/replacements.h>

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
	/** pointer to next property, NULL if last property */
	struct tODBCINSTPROPERTY *pNext;

	/** property name */
	char szName[INI_MAX_PROPERTY_NAME + 1];

	/** property value */
	char szValue[INI_MAX_PROPERTY_VALUE + 1];

	/** PROMPTTYPE_TEXTEDIT, PROMPTTYPE_LISTBOX, PROMPTTYPE_COMBOBOX, PROMPTTYPE_FILENAME */
	int nPromptType;

	/** array of pointers terminated with a NULL value in array */
	char **aPromptData;

	/** help on this property (driver setups should keep it short) */
	char *pszHelp;

	/** CALLER CAN STORE A POINTER TO ? HERE */
	void *pWidget;

	/** app should refresh widget ie Driver Setup has changed aPromptData or szValue */
	int bRefresh;

	/** for odbcinst internal use... only first property has valid one */
	void *hDLL;
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
	"7.3",
	"7.4",
	"8.0",
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
	strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
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
	strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup(comment);
	return hLastProperty;
}

static HODBCINSTPROPERTY
definePropertyHidden(HODBCINSTPROPERTY hLastProperty, const char *name, const char *value, const char *comment)
{
	hLastProperty = addProperty(hLastProperty);
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_HIDDEN;
	strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
	hLastProperty->pszHelp = (char *) strdup(comment);
	return hLastProperty;
}

static HODBCINSTPROPERTY
definePropertyList(HODBCINSTPROPERTY hLastProperty, const char *name, const char *value,
		   const void *list, int size, const char *comment)
{
	hLastProperty = addProperty(hLastProperty);
	hLastProperty->nPromptType = ODBCINST_PROMPTTYPE_LISTBOX;
	hLastProperty->aPromptData = malloc(size);
	memcpy(hLastProperty->aPromptData, list, size);
	strlcpy(hLastProperty->szName, name, INI_MAX_PROPERTY_NAME);
	strlcpy(hLastProperty->szValue, value, INI_MAX_PROPERTY_VALUE);
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
		" 7.2 MSSQL 2005\n"
		" 7.3 MSSQL 2008\n"
		" 7.4 MSSQL 2012, 2014, 2016 or 2019\n"
		" 8.0 MSSQL 2022"
		);

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

	hLastProperty = definePropertyString(hLastProperty, odbc_param_PacketSize, "",
		"Size of network packets.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_ClientCharset, "",
		"The client character set name to convert application characters to UCS-2 in TDS 7.0 and higher.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_DumpFile, "",
		"Specifies the location of a tds dump file and turns on logging.");

	hLastProperty = definePropertyBoolean(hLastProperty, odbc_param_DumpFileAppend, "",
		"Appends dump file instead of overwriting it. Useful for debugging when many processes are active.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_DebugFlags, "",
		"Sets granularity of logging. A set of bit that specify levels and information. "
		"See table below for bit specification.");

	hLastProperty = definePropertyList(hLastProperty, odbc_param_Encryption, TDS_STR_ENCRYPTION_OFF,
		aEncryption, sizeof(aEncryption),
		"The encryption method.");

	hLastProperty = definePropertyString(hLastProperty, odbc_param_Timeout, "10",
		"Connection timeout.");

	return 1;
}

#endif
