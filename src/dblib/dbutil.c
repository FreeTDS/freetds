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

#include <config.h>
#include "sybdb.h"
#include "dblib.h"
/* #include "fortify.h" */


static char  software_version[]   = "$Id: dbutil.c,v 1.10 2002-09-06 21:22:45 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


extern MHANDLEFUNC g_dblib_msg_handler;
extern EHANDLEFUNC g_dblib_err_handler;

/* The next 2 functions will be the reciever for the info and error messages
 * that come from the TDS layer.  The address of this function is passed to
 * the TDS layer in the dbinit function.  This way, when the TDS layer
 * recieves an informational message from the server that it can be dealt with
 * immediately (or so). It takes a pointer to a DBPROCESS, its just that the
 * TDS layer didn't what it really was */
int dblib_handle_info_message(TDSCONTEXT *tds_ctx, TDSSOCKET *tds, TDSMSGINFO *msg)
{
	DBPROCESS *dbproc = NULL;

	if (tds && tds->parent) {
		dbproc = (DBPROCESS*)tds->parent;
	}
	if( msg->msg_number > 0 )
	{
		/* now check to see if the user supplied a function, if not ignore the
		 * problem */
		if(g_dblib_msg_handler)
		{
			g_dblib_msg_handler(dbproc,
					msg->msg_number,
					msg->msg_state,
					msg->msg_level, 
					msg->message,
					msg->server, 
					msg->proc_name,
					msg->line_number);
		}
		else
		{
#if 0
			fprintf (stderr, "INFO..No User supplied info msg handler..Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
					msg->msg_number,
					msg->msg_level, 
					msg->msg_state,
					msg->server, 
					msg->line_number,  
					msg->message);
#endif
		}

		/* and now clean up the structure for next time */
		tds_reset_msg_info(msg);
	}
        return 1;
}

int dblib_handle_err_message(TDSCONTEXT *tds_ctx, TDSSOCKET *tds, TDSMSGINFO *msg)
{
	DBPROCESS *dbproc = NULL;

	if (tds && tds->parent) {
		dbproc = (DBPROCESS*)tds->parent;
	}
	if( msg->msg_number > 0 )
	{
		/* now check to see if the user supplied a function, if not ignore the
		 * problem */
		if(g_dblib_err_handler)
		{
			g_dblib_err_handler(dbproc,
					msg->msg_level,
					msg->msg_number,
					msg->msg_state, 
					msg->message,
					msg->server); 
		}
		else
		{
#if 0
			fprintf (stderr, "ERR..No User supplied err msg handler..Msg %d, Level %d, State %d, Server %s, Line %d\n%s\n",
					msg->msg_number,
					msg->msg_level, 
					msg->msg_state,
					msg->server, 
					msg->line_number,  
					msg->message);
#endif
		}

		/* and now clean up the structure for next time */
		tds_reset_msg_info(msg);
	}
        return 1;
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
