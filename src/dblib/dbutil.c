/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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

#include "tds.h"
#include "sybdb.h"
#include "syberror.h"
#include "dblib.h"
/* #include "fortify.h" */

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: dbutil.c,v 1.27.2.1 2005-01-06 01:51:18 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/*
 * test include consistency 
 * I don't think all compiler are able to compile this code... if not comment it
 */
#if ENABLE_EXTRA_CHECKS

#if defined(__GNUC__) && __GNUC__ >= 2
#define COMPILE_CHECK(name,check) \
    extern int name[(check)?1:-1] __attribute__ ((unused))
#else
#define COMPILE_CHECK(name,check) \
    extern int name[(check)?1:-1]
#endif

/* TODO test SYBxxx consistency */

#define TEST_ATTRIBUTE(t,sa,fa,sb,fb) \
	COMPILE_CHECK(t,sizeof(((sa*)0)->fa) == sizeof(((sb*)0)->fb) && (int)(&((sa*)0)->fa) == (int)(&((sb*)0)->fb))

TEST_ATTRIBUTE(t21,TDS_MONEY4,mny4,DBMONEY4,mny4);
TEST_ATTRIBUTE(t22,TDS_OLD_MONEY,mnyhigh,DBMONEY,mnyhigh);
TEST_ATTRIBUTE(t23,TDS_OLD_MONEY,mnylow,DBMONEY,mnylow);
TEST_ATTRIBUTE(t24,TDS_DATETIME,dtdays,DBDATETIME,dtdays);
TEST_ATTRIBUTE(t25,TDS_DATETIME,dttime,DBDATETIME,dttime);
TEST_ATTRIBUTE(t26,TDS_DATETIME4,days,DBDATETIME4,days);
TEST_ATTRIBUTE(t27,TDS_DATETIME4,minutes,DBDATETIME4,minutes);
TEST_ATTRIBUTE(t28,TDS_NUMERIC,precision,DBNUMERIC,precision);
TEST_ATTRIBUTE(t29,TDS_NUMERIC,scale,DBNUMERIC,scale);
TEST_ATTRIBUTE(t30,TDS_NUMERIC,array,DBNUMERIC,array);
#endif

/* 
 * The next 2 functions receive the info and error messages that come from the TDS layer.  
 * The address of this function is passed to the TDS layer in dbinit().  
 * It takes a pointer to a DBPROCESS, it's just that the TDS layer didn't 
 * know what it really was.  
 */
int
_dblib_handle_info_message(TDSCONTEXT * tds_ctx, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	DBPROCESS *dbproc = NULL;

	if (tds && tds->parent) {
		dbproc = (DBPROCESS *) tds->parent;
	}

	/* now check to see if the user supplied a function,
	 * if not, ignore the problem
	 */
	if (_dblib_msg_handler) {
		_dblib_msg_handler(dbproc,
				   msg->msg_number,
				   msg->msg_state,
				   msg->msg_level, msg->message, msg->server, msg->proc_name, msg->line_number);
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
_dblib_handle_err_message(TDSCONTEXT * tds_ctx, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	DBPROCESS *dbproc = NULL;
	int rc = INT_CANCEL;

	if (tds && tds->parent) {
		dbproc = (DBPROCESS *) tds->parent;
	}
	if (msg->msg_number > 0) {
		/* now check to see if the user supplied a function,
		 * if not, ignore the problem
		 */
		if (_dblib_err_handler) {
			rc = _dblib_err_handler(dbproc, msg->msg_level, msg->msg_number, msg->msg_state, msg->message, msg->server);
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
			if (!dbproc || !dbproc->msdblib)
				/* Sybase behavior */
				rc = INT_EXIT;
			else
				/* Microsoft behavior */
				rc = INT_CANCEL;
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

void
_dblib_setTDS_version(TDSLOGIN * tds_login, DBINT version)
{
	switch (version) {
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
