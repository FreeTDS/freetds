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
 * 29.FEB.04    SJK         Original.
 */

/* This file implements the a login dialog for the FreeTDS ODBC driver.
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <assert.h>
#include <shlobj.h>

#include "resource.h"

#include "tds.h"
#include "tdsodbc.h"
#include "tdsstring.h"
#include "tdsconvert.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/* This is defined in ... */
extern HINSTANCE hinstFreeTDS;

static char *
get_desktop_file(const char *file)
{
	LPITEMIDLIST pidl;
	char path[MAX_PATH];
	HRESULT hr;
	LPMALLOC pMalloc = NULL;
	char * res = NULL;

	hr = SHGetMalloc(&pMalloc);
	if (SUCCEEDED(hr)) {
		hr = SHGetSpecialFolderLocation(NULL, CSIDL_DESKTOPDIRECTORY, &pidl);
		if (SUCCEEDED(hr)) {
			if (SHGetPathFromIDList(pidl, path))
				asprintf(&res, "%s\\%s", path, file);
			(*pMalloc->lpVtbl->Free)(pMalloc, pidl);
		}
		(*pMalloc->lpVtbl->Release)(pMalloc);
	}
	return res;
}

/**
 * Callback function for the DSN Configuration dialog 
 * \param hDlg identifies the dialog
 * \param message what happened to the dialog
 * \param wParam varies with message
 * \param lParam pointer to TDSCONNECTION struct
 */
static BOOL CALLBACK
LoginDlgProc(HWND hDlg, UINT message, WPARAM wParam,	/* */
	     LPARAM lParam)
{
	TDSCONNECTION *connection;
	char tmp[100];

	switch (message) {

	case WM_INITDIALOG:
		/* lParam points to the TDSCONNECTION */
		connection = (TDSCONNECTION *) lParam;
		SetWindowLong(hDlg, GWL_USERDATA, lParam);

		/* copy info from TDSCONNECTION to the dialog */
		SendDlgItemMessage(hDlg, IDC_LOGINSERVER, WM_SETTEXT, 0, (LPARAM) tds_dstr_cstr(&connection->server_name));
		SendDlgItemMessage(hDlg, IDC_LOGINUID, WM_SETTEXT, 0, (LPARAM) tds_dstr_cstr(&connection->user_name));
		SendDlgItemMessage(hDlg, IDC_LOGINPWD, WM_SETTEXT, 0, (LPARAM) tds_dstr_cstr(&connection->password));
		SendDlgItemMessage(hDlg, IDC_LOGINDUMP, BM_SETCHECK, !tds_dstr_isempty(&connection->dump_file), 0L);

		/* adjust label of logging checkbox */
		SendDlgItemMessage(hDlg, IDC_LOGINDUMP, WM_SETTEXT, 0, (LPARAM) "\"FreeTDS.log\" on desktop");

		return TRUE;

	case WM_COMMAND:
		/* Dialog's user data points to TDSCONNECTION */
		connection = (TDSCONNECTION *) GetWindowLong(hDlg, GWL_USERDATA);

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
		SendDlgItemMessage(hDlg, IDC_LOGINUID, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		tds_dstr_copy(&connection->user_name, tmp);
		SendDlgItemMessage(hDlg, IDC_LOGINPWD, WM_GETTEXT, sizeof tmp, (LPARAM) tmp);
		tds_dstr_copy(&connection->password, tmp);
		if (SendDlgItemMessage(hDlg, IDC_LOGINDUMP, BM_GETCHECK, 0, 0)) {
			char * filename = get_desktop_file("FreeTDS.log");

			if (filename) {
				tds_dstr_copy(&connection->dump_file, filename);
				free(filename);
			}
		} else {
			tds_dstr_copy(&connection->dump_file, "");
		}

		/* And we're done */
		EndDialog(hDlg, TRUE);
		return TRUE;
	}
	return FALSE;
}


/**
 * Use a dialog window to prompt for user_name and password.  If the user hits
 * the [OK] button then store the entered values into the given TDSCONNECTION
 * structure and return TRUE.  If the user hits [CANCEL] then return FALSE.
 * \param hwndParent parent for dialog
 * \param connection where to store login info
 */
BOOL
get_login_info(HWND hwndParent, TDSCONNECTION * connection)
{
	return DialogBoxParam(hinstFreeTDS, MAKEINTRESOURCE(IDD_LOGIN), hwndParent, (DLGPROC) LoginDlgProc, (LPARAM) connection);
}
