/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
 * Copyright (C) 2011  Frediano Ziglio
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

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/utils.h>
#include "cspublic.h"
#include "ctlib.h"
#include "syberror.h"
#include <freetds/tds.h>
#include <freetds/replacements.h>
/* #include "fortify.h" */


/*
 * test include consistency 
 * I don't think all compiler are able to compile this code... if not comment it
 */
#if ENABLE_EXTRA_CHECKS

#define TEST_EQUAL(t,a,b) TDS_COMPILE_CHECK(t,a==b)

TEST_EQUAL(t03,CS_NULLTERM,TDS_NULLTERM);
TEST_EQUAL(t04,CS_CMD_SUCCEED,TDS_CMD_SUCCEED);
TEST_EQUAL(t05,CS_CMD_FAIL,TDS_CMD_FAIL);
TEST_EQUAL(t06,CS_CMD_DONE,TDS_CMD_DONE);
TEST_EQUAL(t07,CS_NO_COUNT,TDS_NO_COUNT);
TEST_EQUAL(t08,CS_COMPUTE_RESULT,TDS_COMPUTE_RESULT);
TEST_EQUAL(t09,CS_PARAM_RESULT,TDS_PARAM_RESULT);
TEST_EQUAL(t10,CS_ROW_RESULT,TDS_ROW_RESULT);
TEST_EQUAL(t11,CS_STATUS_RESULT,TDS_STATUS_RESULT);
TEST_EQUAL(t12,CS_COMPUTEFMT_RESULT,TDS_COMPUTEFMT_RESULT);
TEST_EQUAL(t13,CS_ROWFMT_RESULT,TDS_ROWFMT_RESULT);
TEST_EQUAL(t14,CS_MSG_RESULT,TDS_MSG_RESULT);
TEST_EQUAL(t15,CS_DESCRIBE_RESULT,TDS_DESCRIBE_RESULT);
TEST_EQUAL(t16,CS_INT_CONTINUE,TDS_INT_CONTINUE);
TEST_EQUAL(t17,CS_INT_CANCEL,TDS_INT_CANCEL);
TEST_EQUAL(t18,CS_INT_TIMEOUT,TDS_INT_TIMEOUT);

#define TEST_ATTRIBUTE(t,sa,fa,sb,fb) \
	TDS_COMPILE_CHECK(t,sizeof(((sa*)0)->fa) == sizeof(((sb*)0)->fb) && TDS_OFFSET(sa,fa) == TDS_OFFSET(sb,fb))

TEST_ATTRIBUTE(t21,TDS_MONEY4,mny4,CS_MONEY4,mny4);
TEST_ATTRIBUTE(t22,TDS_OLD_MONEY,mnyhigh,CS_MONEY,mnyhigh);
TEST_ATTRIBUTE(t23,TDS_OLD_MONEY,mnylow,CS_MONEY,mnylow);
TEST_ATTRIBUTE(t24,TDS_DATETIME,dtdays,CS_DATETIME,dtdays);
TEST_ATTRIBUTE(t25,TDS_DATETIME,dttime,CS_DATETIME,dttime);
TEST_ATTRIBUTE(t26,TDS_DATETIME4,days,CS_DATETIME4,days);
TEST_ATTRIBUTE(t27,TDS_DATETIME4,minutes,CS_DATETIME4,minutes);
TEST_ATTRIBUTE(t28,TDS_NUMERIC,precision,CS_NUMERIC,precision);
TEST_ATTRIBUTE(t29,TDS_NUMERIC,scale,CS_NUMERIC,scale);
TEST_ATTRIBUTE(t30,TDS_NUMERIC,array,CS_NUMERIC,array);
TEST_ATTRIBUTE(t30,TDS_NUMERIC,precision,CS_DECIMAL,precision);
TEST_ATTRIBUTE(t31,TDS_NUMERIC,scale,CS_DECIMAL,scale);
TEST_ATTRIBUTE(t32,TDS_NUMERIC,array,CS_DECIMAL,array);
#endif

static int
_ct_translate_severity(int tds_severity)
{
	switch (tds_severity) {
	case EXINFO:
		return CS_SV_INFORM; /* unused */
	case EXUSER:
		return CS_SV_CONFIG_FAIL;
	case EXNONFATAL:
		return CS_SV_INTERNAL_FAIL; /* unused */
	case EXCONVERSION:
		return CS_SV_API_FAIL;
	case EXSERVER:
		return CS_SV_INTERNAL_FAIL; /* unused */
	case EXTIME:
		return CS_SV_RETRY_FAIL;
	case EXPROGRAM:
		return CS_SV_API_FAIL;
	case EXRESOURCE:
		return CS_SV_RESOURCE_FAIL; /* unused */
	case EXCOMM:
		return CS_SV_COMM_FAIL;
	case EXFATAL:
		return CS_SV_FATAL; /* unused */
	case EXCONSISTENCY:
	default:
		return CS_SV_INTERNAL_FAIL;
	}
}

/* 
 * error handler 
 * This callback function should be invoked only from libtds through tds_ctx->err_handler.  
 */
int
_ct_handle_client_message(const TDSCONTEXT * ctx_tds, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	CS_CLIENTMSG errmsg;
	CS_CONNECTION *con = NULL;
	CS_CONTEXT *ctx = NULL;
	int ret = (int) CS_SUCCEED;

	tdsdump_log(TDS_DBG_FUNC, "_ct_handle_client_message(%p, %p, %p)\n", ctx_tds, tds, msg);

	if (tds && tds_get_parent(tds)) {
		con = (CS_CONNECTION *) tds_get_parent(tds);
	}

	memset(&errmsg, '\0', sizeof(errmsg));
	errmsg.msgnumber = msg->msgno;
	errmsg.severity = _ct_translate_severity(msg->severity);
	strlcpy(errmsg.msgstring, msg->message, sizeof(errmsg.msgstring));
	errmsg.msgstringlen = (CS_INT) strlen(errmsg.msgstring);

	if (msg->oserr) {
		char *osstr = sock_strerror(msg->oserr);

		errmsg.osstringlen = (CS_INT) strlen(osstr);
		strlcpy(errmsg.osstring, osstr, sizeof(errmsg.osstring));
		sock_strerror_free(osstr);
	} else {
		errmsg.osstring[0] = '\0';
		errmsg.osstringlen = 0;
	}

	/* if there is no connection, attempt to call the context handler */
	if (!con) {
		ctx = (CS_CONTEXT *) ctx_tds->parent;
		if (ctx->clientmsg_cb)
			ret = ctx->clientmsg_cb(ctx, con, &errmsg);
	} else if (con->clientmsg_cb)
		ret = con->clientmsg_cb(con->ctx, con, &errmsg);
	else if (con->ctx->clientmsg_cb)
		ret = con->ctx->clientmsg_cb(con->ctx, con, &errmsg);
		
	/*
	 * The return code from the error handler is either CS_SUCCEED or CS_FAIL.
	 * This function was called by libtds with some kind of communications failure, and there are
	 * no cases in which "succeed" could mean anything: In most cases, the function is going to fail
	 * no matter what.  
	 *
	 * Timeouts are a different matter; it's up to the client to decide whether to continue
	 * waiting or to abort the operation and close the socket.  ct-lib applications do their 
	 * own cancel processing -- they can call ct_cancel from within the error handler -- so 
	 * they don't need to return TDS_INT_TIMEOUT.  They can, however, return TDS_INT_CONTINUE
	 * or TDS_INT_CANCEL.  We map the client's return code to those. 
	 *
	 * Only for timeout errors does TDS_INT_CANCEL cause libtds to break the connection. 
	 */
	if (msg->msgno == TDSETIME) {
		switch (ret) {
		case CS_SUCCEED:	return TDS_INT_CONTINUE;
		case CS_FAIL:		return TDS_INT_CANCEL;
		}
	}
	return TDS_INT_CANCEL;
}

/*
 * interrupt handler, installed on demand to avoid gratuitously interfering
 * with tds_select's optimization for the no-handler case */
int
_ct_handle_interrupt(void * ptr)
{
	CS_CONNECTION *con = (CS_CONNECTION *) ptr;
	if (con->interrupt_cb)
		return (*con->interrupt_cb)(con);
	else if (con->ctx->interrupt_cb)
		return (*con->ctx->interrupt_cb)(con);
	else
		return TDS_INT_CONTINUE;
}

/* message handler */
TDSRET
_ct_handle_server_message(const TDSCONTEXT * ctx_tds, TDSSOCKET * tds, TDSMESSAGE * msg)
{
	CS_SERVERMSG_INTERNAL errmsg;
	CS_CONNECTION *con = NULL;
	CS_CONTEXT *ctx;
	CS_SERVERMSG_COMMON2 *common2;
	CS_RETCODE ret = CS_SUCCEED;

	tdsdump_log(TDS_DBG_FUNC, "_ct_handle_server_message(%p, %p, %p)\n", ctx_tds, tds, msg);

	if (tds && tds_get_parent(tds))
		con = (CS_CONNECTION *) tds_get_parent(tds);

	ctx = con ? con->ctx : (CS_CONTEXT *) ctx_tds->parent;

	memset(&errmsg, '\0', sizeof(errmsg));
	errmsg.common.msgnumber = msg->msgno;
	strlcpy(errmsg.common.text, msg->message, sizeof(errmsg.common.text));
	errmsg.common.textlen = (CS_INT) strlen(errmsg.common.text);
	errmsg.common.state = msg->state;
	errmsg.common.severity = msg->severity;

#define MIDDLE_PART(part) do { \
	common2 = (CS_SERVERMSG_COMMON2 *) &(errmsg.part.line); \
	if (msg->server) { \
		errmsg.part.svrnlen = (CS_INT) strlen(msg->server); \
		strlcpy(errmsg.part.svrname, msg->server, sizeof(errmsg.part.svrname)); \
	} \
	if (msg->proc_name) { \
		errmsg.part.proclen = (CS_INT) strlen(msg->proc_name); \
		strlcpy(errmsg.part.proc, msg->proc_name, sizeof(errmsg.part.proc)); \
	} \
} while(0)

	if (ctx->use_large_identifiers)
		MIDDLE_PART(large);
	else
		MIDDLE_PART(small);
#undef MIDDLE_PART

	common2->sqlstate[0] = 0;
	if (msg->sql_state)
		strlcpy((char *) common2->sqlstate, msg->sql_state, sizeof(common2->sqlstate));
	common2->sqlstatelen = (CS_INT) strlen((char *) common2->sqlstate);
	common2->line = msg->line_number;

	/* if there is no connection, attempt to call the context handler */
	if (!con) {
		if (ctx->servermsg_cb)
			ret = ctx->servermsg_cb(ctx, con, &errmsg.user);
	} else if (con->servermsg_cb) {
		ret = con->servermsg_cb(ctx, con, &errmsg.user);
	} else if (ctx->servermsg_cb) {
		ret = ctx->servermsg_cb(ctx, con, &errmsg.user);
	}
	return ret == CS_SUCCEED ? TDS_SUCCESS : TDS_FAIL;
}

/**
 * Check if a give version supports large identifiers.
 */
bool
_ct_is_large_identifiers_version(CS_INT version)
{
	switch (version) {
	case 112:
	case 1100:
	case 12500:
	case 15000:
		return false;
	}
	return true;
}

const CS_DATAFMT_COMMON *
_ct_datafmt_common(CS_CONTEXT * ctx, const CS_DATAFMT * datafmt)
{
	if (!datafmt)
		return NULL;
	if (ctx->use_large_identifiers)
		return (const CS_DATAFMT_COMMON *) &(((const CS_DATAFMT_LARGE *) datafmt)->datatype);
	return (const CS_DATAFMT_COMMON *) &(((const CS_DATAFMT_SMALL *) datafmt)->datatype);
}

/**
 * Converts CS_DATAFMT input parameter to CS_DATAFMT_LARGE.
 * @param ctx      CTLib context
 * @param datafmt  Input parameter
 * @param fmtbuf   Buffer to use in case conversion is required
 * @return parameter converted to large
 */
const CS_DATAFMT_LARGE *
_ct_datafmt_conv_in(CS_CONTEXT * ctx, const CS_DATAFMT * datafmt, CS_DATAFMT_LARGE *fmtbuf)
{
	const CS_DATAFMT_SMALL *small;

	if (!datafmt)
		return NULL;

	/* read directly from input */
	if (ctx->use_large_identifiers)
		return (CS_DATAFMT_LARGE *) datafmt;

	/* convert small format to large */
	small = (const CS_DATAFMT_SMALL *) datafmt;

	strlcpy(fmtbuf->name, small->name, sizeof(fmtbuf->name));
	fmtbuf->namelen = (CS_INT) strlen(fmtbuf->name);
	*((CS_DATAFMT_COMMON *) &fmtbuf->datatype) = *((CS_DATAFMT_COMMON *) &small->datatype);
	return fmtbuf;
}

/**
 * Prepares to Convert CS_DATAFMT output parameter to CS_DATAFMT_LARGE.
 * @param ctx      CTLib context
 * @param datafmt  Input parameter
 * @param fmtbuf   Buffer to use in case conversion is required
 * @return parameter converted to large
 */
CS_DATAFMT_LARGE *
_ct_datafmt_conv_prepare(CS_CONTEXT * ctx, CS_DATAFMT * datafmt, CS_DATAFMT_LARGE *fmtbuf)
{
	if (!datafmt)
		return NULL;

	/* write directly to output */
	if (ctx->use_large_identifiers)
		return (CS_DATAFMT_LARGE *) datafmt;

	return fmtbuf;
}

/**
 * Converts CS_DATAFMT output parameter to CS_DATAFMT_LARGE after setting it.
 * @param datafmt  Input parameter
 * @param fmtbuf   Buffer to use in case conversion is required. You should pass
 *                 value returned by _ct_datafmt_conv_prepare().
 */
void
_ct_datafmt_conv_back(CS_DATAFMT * datafmt, CS_DATAFMT_LARGE *fmtbuf)
{
	CS_DATAFMT_SMALL *small;

	/* already right format */
	if ((void *) datafmt == (void*) fmtbuf)
		return;

	/* convert large format to small */
	small = (CS_DATAFMT_SMALL *) datafmt;

	strlcpy(small->name, fmtbuf->name, sizeof(small->name));
	small->namelen = (CS_INT) strlen(small->name);
	*((CS_DATAFMT_COMMON *) &small->datatype) = *((CS_DATAFMT_COMMON *) &fmtbuf->datatype);
}

/**
 * Get length of a string buffer
 *
 * @return length of string or original negative value if error.
 */
CS_INT
_ct_get_string_length(const char *buf, CS_INT buflen)
{
	if (buflen >= 0)
		return buflen;

	if (buflen == CS_NULLTERM)
		return (CS_INT) strlen(buf);

	return buflen;
}

CS_RETCODE
_ct_props_dstr(CS_CONNECTION * con TDS_UNUSED, DSTR *s, CS_INT action, CS_VOID * buffer, CS_INT buflen, CS_INT * out_len)
{
	if (action == CS_SET) {
		buflen = _ct_get_string_length(buffer, buflen);
		if (buflen < 0) {
			/* TODO what error ?? */
			/* _ctclient_msg(NULL, con, "ct_con_props(SET,APPNAME)", 1, 1, 1, 5, "%d, %s", buflen, "buflen"); */
			return CS_FAIL;
		}
		if (tds_dstr_copyn(s, buffer, buflen) != NULL)
			return CS_SUCCEED;
	} else if (action == CS_GET) {
		if (out_len)
			*out_len = tds_dstr_len(s);
		strlcpy((char *) buffer, tds_dstr_cstr(s), buflen);
		return CS_SUCCEED;
	} else if (action == CS_CLEAR) {
		tds_dstr_empty(s);
		return CS_SUCCEED;
	}
	return CS_FAIL;
}
