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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "cspublic.h"
#include "ctlib.h"
#include "tds.h"
/* #include "fortify.h" */


static char software_version[] = "$Id: ctutil.c,v 1.18 2004-01-27 21:56:45 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/* error handler */
int
ctlib_handle_client_message(TDSCONTEXT * ctx_tds, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	CS_CLIENTMSG errmsg;
	CS_CONNECTION *con = NULL;
	CS_CONTEXT *ctx = NULL;
	int ret = (int) CS_SUCCEED;

	if (tds && tds->parent) {
		con = (CS_CONNECTION *) tds->parent;
	}

	memset(&errmsg, '\0', sizeof(errmsg));
	errmsg.msgnumber = msg->msg_number;
	strcpy(errmsg.msgstring, msg->message);
	errmsg.msgstringlen = strlen(msg->message);
	errmsg.osstring[0] = '\0';
	errmsg.osstringlen = 0;
	/* if there is no connection, attempt to call the context handler */
	if (!con) {
		ctx = (CS_CONTEXT *) ctx_tds->parent;
		if (ctx->_clientmsg_cb)
			ret = ctx->_clientmsg_cb(ctx, con, &errmsg);
	} else if (con->_clientmsg_cb)
		ret = con->_clientmsg_cb(con->ctx, con, &errmsg);
	else if (con->ctx->_clientmsg_cb)
		ret = con->ctx->_clientmsg_cb(con->ctx, con, &errmsg);
	return ret;
}

/* message handler */
int
ctlib_handle_server_message(TDSCONTEXT * ctx_tds, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	CS_SERVERMSG errmsg;
	CS_CONNECTION *con = NULL;
	CS_CONTEXT *ctx = NULL;
	int ret = (int) CS_SUCCEED;

	if (tds && tds->parent) {
		con = (CS_CONNECTION *) tds->parent;
	}

	memset(&errmsg, '\0', sizeof(errmsg));
	errmsg.msgnumber = msg->msg_number;
	strcpy(errmsg.text, msg->message);
	errmsg.state = msg->msg_state;
	errmsg.severity = msg->msg_level;
	errmsg.line = msg->line_number;
	if (msg->server) {
		errmsg.svrnlen = strlen(msg->server);
		strncpy(errmsg.svrname, msg->server, CS_MAX_NAME);
	}
	if (msg->proc_name) {
		errmsg.proclen = strlen(msg->proc_name);
		strncpy(errmsg.proc, msg->proc_name, CS_MAX_NAME);
	}
	/* if there is no connection, attempt to call the context handler */
	if (!con) {
		ctx = (CS_CONTEXT *) ctx_tds->parent;
		if (ctx->_servermsg_cb)
			ret = ctx->_servermsg_cb(ctx, con, &errmsg);
	} else if (con->_servermsg_cb) {
		ret = con->_servermsg_cb(con->ctx, con, &errmsg);
	} else if (con->ctx->_servermsg_cb) {
		ret = con->ctx->_servermsg_cb(con->ctx, con, &errmsg);
	}
	return ret;
}
