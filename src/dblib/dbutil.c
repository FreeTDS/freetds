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
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "sybdb.h"
#include "syberror.h"
#include "dblib.h"
/* #include "fortify.h" */


static char  software_version[]   = "$Id: dbutil.c,v 1.17 2002-10-13 23:28:12 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

/* The next 2 functions will be the reciever for the info and error messages
 * that come from the TDS layer.  The address of this function is passed to
 * the TDS layer in the dbinit function.  This way, when the TDS layer
 * recieves an informational message from the server that it can be dealt with
 * immediately (or so). It takes a pointer to a DBPROCESS, its just that the
 * TDS layer didn't what it really was */
int
dblib_handle_info_message(TDSCONTEXT *tds_ctx, TDSSOCKET *tds, TDSMSGINFO *msg)
{
DBPROCESS *dbproc = NULL;

	if (tds && tds->parent) {
		dbproc = (DBPROCESS*)tds->parent;
	}
	if (msg->msg_number > 0) {
		/* now check to see if the user supplied a function,
		 * if not, ignore the problem
		 */
		if (_dblib_msg_handler) {
			_dblib_msg_handler(dbproc,
					msg->msg_number,
					msg->msg_state,
					msg->msg_level, 
					msg->message,
					msg->server, 
					msg->proc_name,
					msg->line_number);
		}
	}
	if (msg->msg_level > 10) {
		/*
		 * Sybase docs say SYBESMSG is generated only in specific
		 * cases (severity greater than 16, or deadlock occurred, or
		 * a syntax error occurred.)  However, actual observed
		 * behavior is that SYBESMSG is always generated for
		 * server messages with severity greater than 10.
		 */
		tds_client_msg(tds_ctx, tds, SYBESMSG, EXSERVER, -1, -1,
			"General SQL Server error: Check messages from the SQL Server.");
	}
	return SUCCEED;
}

int
dblib_handle_err_message(TDSCONTEXT *tds_ctx, TDSSOCKET *tds, TDSMSGINFO *msg)
{
DBPROCESS *dbproc = NULL;
int rc = INT_CANCEL;

	if (tds && tds->parent) {
		dbproc = (DBPROCESS*)tds->parent;
	}
	if (msg->msg_number > 0) {
		/* now check to see if the user supplied a function,
		 * if not, ignore the problem
		 */
		if (_dblib_err_handler) {
			rc  = _dblib_err_handler(dbproc,
					msg->msg_level,
					msg->msg_number,
					msg->msg_state, 
					msg->message,
					msg->server); 
		}
	}

	/*
	 * Preprocess the return code to handle INT_TIMEOUT/INT_CONTINUE
	 * for non-SYBETIME errors in the strange and different ways as
	 * specified by Sybase and Microsoft.
	 */
	if (msg->msg_number != SYBETIME) {
		switch (rc) {
		case INT_TIMEOUT:
			rc = INT_EXIT;
			break;
		case INT_CONTINUE:
#ifndef MSDBLIB
			/* Sybase behavior */
			rc = INT_EXIT;
#else
			/* Microsoft behavior */
			rc = INT_CANCEL;
#endif
			break;
		default:
			break;
		}
	}

	switch (rc) {
	case INT_EXIT:
		exit(EXIT_FAILURE);
		break;
	case INT_CANCEL:
		return SUCCEED;
		break;
	case INT_TIMEOUT:
		/* XXX do something clever */
		return SUCCEED;
		break;
	case INT_CONTINUE:
		/* XXX do something clever */
		return SUCCEED;
		break;
	default:
		/* unknown return code from error handler */
		return FAIL;
		break;
	}

	/* notreached */
        return FAIL;
}

void dblib_setTDS_version(TDSLOGIN *tds_login, DBINT version)
{
	switch(version)
	{
		case DBVERSION_42:
			tds_set_version(tds_login, 4, 2);
			break;
		case DBVERSION_46:
			tds_set_version(tds_login, 4, 6);
			break;
		case DBVERSION_100:
			tds_set_version(tds_login, 5, 0);
			break;
	}
}
