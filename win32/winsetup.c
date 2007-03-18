/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2004  Frediano Ziglio
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

/*
 * PROGRAMMER   NAME              CONTACT
 *==============================================================
 * SJK          Steve Kirkendall  kirkenda@cs.pdx.edu
 *
 ***************************************************************
 * DATE         PROGRAMMER  CHANGE
 *==============================================================
 * 25.FEB.04    SJK         Original.
 */

/* This file implements the setup API for FreeTDS.  Specifically,
 * this includes the ConfigDSN() and ConfigDriver() functions.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>
#include <ctype.h>
#include <assert.h>

#include "resource.h"

#include "tds.h"
#include "tdsodbc.h"
#include "tdsstring.h"
#include "tdsconvert.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

typedef struct
{
	DSTR origdsn;		/**< original name of the data source */
	DSTR dsn;		/**< edited name of the data source */
	TDSCONNECTION *connection;	/**< everything else */
} DSNINFO;

/* This is defined in ... */
extern HINSTANCE hinstFreeTDS;


/** Create an empty DSNINFO struct */
static DSNINFO *
alloc_dsninfo(void)
{
	DSNINFO *di;

	/* allocate & initialize it */
	di = (DSNINFO *) malloc(sizeof(DSNINFO));
	tds_dstr_init(&di->origdsn);
	tds_dstr_init(&di->dsn);
	di->connection = tds_alloc_connection(NULL);

	return di;
}


/** Destroy a DSNINFO struct, freeing all memory associated with it */
static void
free_dsninfo(DSNINFO * di)
{				/* the DSNINFO struct to be freed */
	tds_free_connection(di->connection);
	tds_dstr_free(&di->origdsn);
	tds_dstr_free(&di->dsn);
	free(di);
}


/**
 * Parse a DSN string which is delimited with NULs instead of semicolons.
 * This uses odbc_parse_connect_string() internally, and also adds support
 * for parsing the DSN and driver
 * \param attribs 0-delimited string, with \0\0 terminator
 * \param di where to store the results
 */
static void
parse_wacky_dsn_string(LPCSTR attribs, DSNINFO * di)
{
	LPCSTR str;
	char *build;

	/* for each field... */
	for (str = attribs; *str; str += strlen(str) + 1) {
		if (!strncasecmp(str, "DSN=", 4)) {
			tds_dstr_copy(&di->origdsn, str + 4);
			tds_dstr_copy(&di->dsn, str + 4);
		}
	}

	/* allocate space for a ;-delimited version */
	build = (char *) malloc(str - attribs);

	/* copy the fields into the new buffer with ;'s */
	*build = '\0';
	for (str = attribs; *str; str += strlen(str) + 1) {
		if (*build)
			strcat(build, ";");
		strcat(build, str);
	}

	/* let odbc_parse_connect_string() parse the ;-delimited version */
	odbc_parse_connect_string(build, build + strlen(build), di->connection);
}


/**
 * Update the attributes.  Return TRUE if successful, else FALSE.  The names
 * written here correspond to the names read by odbc_get_dsn_info().
 */
#define WRITESTR(n,s) if (!SQLWritePrivateProfileString(section, (n), (s), odbcini)) return FALSE
#define FIELD_STRING(f) tds_dstr_cstr(&di->connection->f)
static BOOL
write_all_strings(DSNINFO * di)
{
	char odbcini[FILENAME_MAX];
	char tmp[100];
	const char *section = tds_dstr_cstr(&di->dsn);

	strcpy(odbcini, "odbc.ini");

	WRITESTR("Server", FIELD_STRING(server_name));
	WRITESTR("Language", FIELD_STRING(language));
	WRITESTR("Database", FIELD_STRING(database));

	sprintf(tmp, "%u", di->connection->port);
	WRITESTR("Port", tmp);

	sprintf(tmp, "%d.%d", di->connection->major_version, di->connection->minor_version);
	WRITESTR("TDS_Version", tmp);

	sprintf(tmp, "%u", di->connection->text_size);
	WRITESTR("TextSize", tmp);

	sprintf(tmp, "%u", di->connection->block_size);
	WRITESTR("PacketSize", tmp);

	return TRUE;
}



/**
 * Go looking for trouble.  Return NULL if the info is okay, or an error message
 * if something needs to change.
 */
static const char *
validate(DSNINFO * di)
{
	if (!SQLValidDSN(tds_dstr_cstr(&di->dsn)))
		return "Invalid DSN";
	if (!IS_TDS42(di->connection) && !IS_TDS46(di->connection)
	    && !IS_TDS50(di->connection) && !IS_TDS7_PLUS(di->connection))
		return "Bad Protocol version";
	if (tds_dstr_isempty(&di->connection->server_name))
		return "Address is required";
	if (di->connection->port < 1 || di->connection->port > 65535)
		return "Bad port - Try 1433 or 4000";
	return NULL;
}


/** 
 * Callback function for the DSN Configuration dialog
 * \param hDlg identifies the dialog
 * \param message what happened to the dialog
 * \param wParam varies with message
 * \param lParam pointer to DSNINFO struct
 */
static BOOL CALLBACK
DSNDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	DSNINFO *di;
	char tmp[100];
	const char *pstr;
	int major, minor, i;
	static const char *protocols[] = {
		"TDS 4.2", "TDS 4.6", "TDS 5.0", "TDS 7.0", "TDS 8.0", NULL
	};

	switch (message) {

	case WM_INITDIALOG:
		/* lParam points to the DSNINFO */
		di = (DSNINFO *) lParam;
		SetWindowLong(hDlg, GWL_USERDATA, lParam);

		/* Stuff legal protocol names into IDC_PROTOCOL */
		for (i = 0; protocols[i]; i++) {
			SendDlgItemMessage(hDlg, IDC_PROTOCOL, CB_ADDSTRING, 0, (LPARAM) protocols[i]);
		}

		/* copy info from DSNINFO to the dialog */
		SendDlgItemMessage(hDlg, IDC_DSNNAME, WM_SETTEXT, 0, (LPARAM) tds_dstr_cstr(&di->dsn));
		sprintf(tmp, "TDS %d.%d", di->connection->major_version, di->connection->minor_version);
		SendDlgItemMessage(hDlg, IDC_PROTOCOL, CB_SELECTSTRING, -1, (LPARAM) tmp);
		SendDlgItemMessage(hDlg, IDC_ADDRESS, WM_SETTEXT, 0, (LPARAM) tds_dstr_cstr(&di->connection->server_name));
		sprintf(tmp, "%u", di->connection->port);
		SendDlgItemMessage(hDlg, IDC_PORT, WM_SETTEXT, 0, (LPARAM) tmp);
		SendDlgItemMessage(hDlg, IDC_DATABASE, WM_SETTEXT, 0, (LPARAM) tds_dstr_cstr(&di->connection->database));

		return TRUE;

	case WM_COMMAND:
		/* Dialog's user data points to DSNINFO */
		di = (DSNINFO *) GetWindowLong(hDlg, GWL_USERDATA);

		/* The wParam indicates which button was pressed */
		if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(hDlg, FALSE);
			return TRUE;
		} else if (LOWORD(wParam) != IDOK) {
			/* Anything but IDCANCEL or IDOK is handled elsewhere */
			break;
		}
		/* If we get here, then the user hit the [OK] button */

		/* get values from dialog */
		SendDlgItemMessage(hDlg, IDC_DSNNAME, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		tds_dstr_copy(&di->dsn, tmp);
		SendDlgItemMessage(hDlg, IDC_PROTOCOL, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		minor = 0;
		if (sscanf(tmp, "%*[^0-9]%d.%d", &major, &minor) > 1) {
			di->connection->major_version = major;
			di->connection->minor_version = minor;
		}
		SendDlgItemMessage(hDlg, IDC_ADDRESS, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		tds_dstr_copy(&di->connection->server_name, tmp);
		SendDlgItemMessage(hDlg, IDC_PORT, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		di->connection->port = atoi(tmp);
		SendDlgItemMessage(hDlg, IDC_DATABASE, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		tds_dstr_copy(&di->connection->database, tmp);

		/* validate */
		SendDlgItemMessage(hDlg, IDC_HINT, WM_SETTEXT, 0, (LPARAM) "VALIDATING... please be patient");
		pstr = validate(di);
		if (pstr != NULL) {
			SendDlgItemMessage(hDlg, IDC_HINT, WM_SETTEXT, 0, (LPARAM) pstr);
			return TRUE;
		}
		SendDlgItemMessage(hDlg, IDC_HINT, WM_SETTEXT, 0, (LPARAM) "");

		/* No problems -- we're done */
		EndDialog(hDlg, TRUE);
		return TRUE;
	}
	return FALSE;
}


/**
 * Add, remove, or modify a data source
 * \param hwndParent parent for dialog, NULL for batch ops
 * \param fRequest request type
 * \param lpszDriver driver name (for humans, not DLL name)
 * \param lpszAttributes attribute list
 */
BOOL INSTAPI
ConfigDSN(HWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszAttributes)
{
	int result;
	DSNINFO *di;
	const char *errmsg;

	/*
	 * Initialize Windows sockets.  This is necessary even though
	 * ConfigDSN() only looks up addresses and names, and never actually
	 * uses any sockets.
	 */
	INITSOCKET();

	/* Create a blank connection struct */
	di = alloc_dsninfo();

	/*
	 * Parse the attribute string.  If this contains a DSN name, then it
	 * also reads the current parameters of that DSN.
	 */
	parse_wacky_dsn_string(lpszAttributes, di);

	/* Maybe allow the user to edit it */
	if (hwndParent && fRequest != ODBC_REMOVE_DSN) {
		result = DialogBoxParam(hinstFreeTDS, MAKEINTRESOURCE(IDD_DSN), hwndParent, (DLGPROC) DSNDlgProc, (LPARAM) di);
		if (result < 0) {
			DWORD errorcode = GetLastError();
			char buf[1000];

			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, errorcode, 0, buf, 1000, NULL);
		}

		/* if user hit [Cancel] then clean up and return FALSE */
		if (result == 0) {
			goto Fail;
		}
	}

	switch (fRequest) {
	case ODBC_ADD_DSN:
		errmsg = validate(di);
		if (errmsg != NULL) {
			SQLPostInstallerError(ODBC_ERROR_REQUEST_FAILED, errmsg);
			goto Fail;
		}
		if (!SQLWriteDSNToIni(tds_dstr_cstr(&di->dsn), lpszDriver)) {
			goto Fail;
		}
		if (!write_all_strings(di)) {
			goto Fail;
		}
		break;
	case ODBC_CONFIG_DSN:
		errmsg = validate(di);
		if (errmsg != NULL) {
			SQLPostInstallerError(ODBC_ERROR_REQUEST_FAILED, errmsg);
			goto Fail;
		}

		/*
		 * if the DSN name has changed, then delete the old entry and
		 * add the new one.
		 */
		if (strcasecmp(tds_dstr_cstr(&di->origdsn), tds_dstr_cstr(&di->dsn))) {
			if (!SQLRemoveDSNFromIni(tds_dstr_cstr(&di->origdsn))
			    || !SQLWriteDSNToIni(tds_dstr_cstr(&di->dsn), lpszDriver)) {
				goto Fail;
			}
		}
		if (!write_all_strings(di)) {
			goto Fail;
		}
		break;
	case ODBC_REMOVE_DSN:
		if (!SQLRemoveDSNFromIni(tds_dstr_cstr(&di->dsn))) {
			goto Fail;
		}
		break;
	}

	/* Clean up and return TRUE, indicating that the change took place */
	free_dsninfo(di);
	DONESOCKET();
	return TRUE;

      Fail:
	free_dsninfo(di);
	DONESOCKET();
	return FALSE;
}

/** Add or remove an ODBC driver */
BOOL INSTAPI
ConfigDriver(HWND hwndParent, WORD fRequest, LPCSTR lpszDriver, LPCSTR lpszArgs, LPSTR lpszMsg, WORD cbMsgMax, WORD * pcbMsgOut)
{
	/* TODO finish ?? */
	switch (fRequest) {
	case ODBC_INSTALL_DRIVER:
		/* FIXME possible buffer overflow */
		strcpy(lpszMsg, "Hello");
		*pcbMsgOut = strlen(lpszMsg);
		break;
	case ODBC_REMOVE_DRIVER:
		/* FIXME possible buffer overflow */
		strcpy(lpszMsg, "Goodbye");
		*pcbMsgOut = strlen(lpszMsg);
		break;
	}
	return TRUE;
}

BOOL INSTAPI
ConfigTranslator(HWND hwndParent, DWORD * pvOption)
{
	return TRUE;
}
