/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2002  James K. Lowden
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
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "ctpublic.h"
#include "ctlib.h"
#include "tdsstring.h"
#include "replacements.h"

static char software_version[] = "$Id: ct.c,v 1.113 2004-01-26 08:44:27 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };


/**
 * Read a row of data
 * @return 0 on success
 */
static int _ct_fetch_cursor(CS_COMMAND * cmd, CS_INT type, CS_INT offset, CS_INT option, CS_INT * rows_read);
static int _ct_bind_data(CS_COMMAND * cmd, CS_INT offset);
static int _ct_get_client_type(int datatype, int usertype, int size);
static int _ct_fetchable_results(CS_COMMAND * cmd);
static int _ct_process_return_status(TDSSOCKET * tds);

static int _ct_fill_param(CS_PARAM * param, CS_DATAFMT * datafmt, CS_VOID * data,
			  CS_INT * datalen, CS_SMALLINT * indicator, CS_BYTE byvalue);

/* Added for CT_DIAG */
/* Code changes starts here - CT_DIAG - 01 */

static CS_INT ct_diag_storeclientmsg(CS_CONTEXT * context, CS_CONNECTION * conn, CS_CLIENTMSG * message);
static CS_INT ct_diag_storeservermsg(CS_CONTEXT * context, CS_CONNECTION * conn, CS_SERVERMSG * message);
static CS_INT ct_diag_countmsg(CS_CONTEXT * context, CS_INT type, CS_INT * count);
static CS_INT ct_diag_clearmsg(CS_CONTEXT * context, CS_INT type);
static CS_INT ct_diag_getclientmsg(CS_CONTEXT * context, CS_INT idx, CS_CLIENTMSG * message);
static CS_INT ct_diag_getservermsg(CS_CONTEXT * context, CS_INT idx, CS_SERVERMSG * message);

/* Code changes ends here - CT_DIAG - 01 */

/* Added code for RPC functionality -SUHA */
/* RPC Code changes starts here */

static void rpc_clear(CSREMOTE_PROC * rpc);
static void param_clear(CSREMOTE_PROC_PARAM * pparam);

static TDSPARAMINFO *paraminfoalloc(TDSSOCKET * tds, CS_PARAM * first_param);

/* RPC Code changes ends here */

static const char *
_ct_get_layer(int layer)
{
	switch (layer) {
	case 1:
		return "user api layer";
		break;
	default:
		break;
	}
	return "unrecognized layer";
}

static const char *
_ct_get_origin(int origin)
{
	switch (origin) {
	case 1:
		return "external error";
		break;
	case 2:
		return "internal CT-Library error";
		break;
	case 4:
		return "common library error";
		break;
	case 5:
		return "intl library error";
		break;
	default:
		break;
	}
	return "unrecognized origin";
}

static const char *
_ct_get_user_api_layer_error(int error)
{
	switch (error) {
	case 137:
		return "A bind count of %1! is not consistent with the count supplied for existing binds. The current bind count is %2!.";
		break;
	default:
		break;
	}
	return "unrecognized error";
}

static char *
_ct_get_msgstr(const char *funcname, int layer, int origin, int severity, int number)
{
	char *m;

	if (asprintf(&m,
		     "%s: %s: %s: %s", funcname, _ct_get_layer(layer), _ct_get_origin(origin), _ct_get_user_api_layer_error(number)
	    ) < 0) {
		return NULL;
	}
	return m;
}

static void
_ctclient_msg(CS_CONNECTION * con, const char *funcname, int layer, int origin, int severity, int number, const char *fmt, ...)
{
	CS_CONTEXT *ctx = con->ctx;
	va_list ap;
	CS_CLIENTMSG cm;
	char *msgstr;

	va_start(ap, fmt);

	if (ctx->_clientmsg_cb) {
		cm.severity = severity;
		cm.msgnumber = (((layer << 24) & 0xFF000000)
				| ((origin << 16) & 0x00FF0000)
				| ((severity << 8) & 0x0000FF00)
				| ((number) & 0x000000FF));
		msgstr = _ct_get_msgstr(funcname, layer, origin, severity, number);
		tds_vstrbuild(cm.msgstring, CS_MAX_MSG, &(cm.msgstringlen), msgstr, CS_NULLTERM, fmt, CS_NULLTERM, ap);
		cm.msgstring[cm.msgstringlen] = '\0';
		free(msgstr);
		cm.osnumber = 0;
		cm.osstring[0] = '\0';
		cm.osstringlen = 0;
		cm.status = 0;
		/* cm.sqlstate */
		cm.sqlstatelen = 0;
		ctx->_clientmsg_cb(ctx, con, &cm);
	}

	va_end(ap);
}

CS_RETCODE
ct_exit(CS_CONTEXT * ctx, CS_INT unused)
{
	tdsdump_log(TDS_DBG_FUNC, "%L ct_exit()\n");
	return CS_SUCCEED;
}

CS_RETCODE
ct_init(CS_CONTEXT * ctx, CS_INT version)
{
	/* uncomment the next line to get pre-login trace */
	/* tdsdump_open("/tmp/tds2.log"); */
	tdsdump_log(TDS_DBG_FUNC, "%L ct_init()\n");
	ctx->tds_ctx->msg_handler = ctlib_handle_server_message;
	ctx->tds_ctx->err_handler = ctlib_handle_client_message;
	return CS_SUCCEED;
}

CS_RETCODE
ct_con_alloc(CS_CONTEXT * ctx, CS_CONNECTION ** con)
{
	tdsdump_log(TDS_DBG_FUNC, "%L ct_con_alloc()\n");
	*con = (CS_CONNECTION *) malloc(sizeof(CS_CONNECTION));
	memset(*con, '\0', sizeof(CS_CONNECTION));
	(*con)->tds_login = tds_alloc_login();

	/* so we know who we belong to */
	(*con)->ctx = ctx;

	/* set default values */
	tds_set_library((*con)->tds_login, "CT-Library");
	/* tds_set_client_charset((*con)->tds_login, "iso_1"); */
	/* tds_set_packet((*con)->tds_login, TDS_DEF_BLKSZ); */
	return CS_SUCCEED;
}

CS_RETCODE
ct_callback(CS_CONTEXT * ctx, CS_CONNECTION * con, CS_INT action, CS_INT type, CS_VOID * func)
{
	int (*funcptr) (void *, void *, void *) = (int (*)(void *, void *, void *)) func;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_callback() action = %s\n", CS_GET ? "CS_GET" : "CS_SET");
	/* one of these has to be defined */
	if (!ctx && !con)
		return CS_FAIL;

	if (action == CS_GET) {
		switch (type) {
		case CS_CLIENTMSG_CB:
			*(void **) func = (CS_VOID *) (con ? con->_clientmsg_cb : ctx->_clientmsg_cb);
			return CS_SUCCEED;
		case CS_SERVERMSG_CB:
			*(void **) func = (CS_VOID *) (con ? con->_servermsg_cb : ctx->_servermsg_cb);
			return CS_SUCCEED;
		default:
			fprintf(stderr, "Unknown callback %d\n", type);
			*(void **) func = (CS_VOID *) NULL;
			return CS_SUCCEED;
		}
	}
	/* CS_SET */
	switch (type) {
	case CS_CLIENTMSG_CB:
		if (con)
			con->_clientmsg_cb = (CS_CLIENTMSG_FUNC) funcptr;
		else
			ctx->_clientmsg_cb = (CS_CLIENTMSG_FUNC) funcptr;
		break;
	case CS_SERVERMSG_CB:
		if (con)
			con->_servermsg_cb = (CS_SERVERMSG_FUNC) funcptr;
		else
			ctx->_servermsg_cb = (CS_SERVERMSG_FUNC) funcptr;
		break;
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_con_props(CS_CONNECTION * con, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * out_len)
{
	CS_INT intval = 0, maxcp;
	TDSSOCKET *tds;
	TDSLOGIN *tds_login;
	char *set_buffer = NULL;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_con_props() action = %s property = %d\n", CS_GET ? "CS_GET" : "CS_SET", property);

	tds = con->tds_socket;
	tds_login = con->tds_login;

	if (action == CS_SET) {
		if (property == CS_USERNAME || property == CS_PASSWORD || property == CS_APPNAME || property == CS_HOSTNAME) {
			if (buflen == CS_NULLTERM) {
				maxcp = strlen((char *) buffer);
				set_buffer = (char *) malloc(maxcp + 1);
				strcpy(set_buffer, (char *) buffer);
			} else if (buflen == CS_UNUSED) {
				return CS_SUCCEED;
			} else {
				set_buffer = (char *) malloc(buflen + 1);
				strncpy(set_buffer, (char *) buffer, buflen);
				set_buffer[buflen] = '\0';
			}
		}

		/* XXX "login" properties shouldn't be set after
		 * login.  I don't know if it should fail silently
		 * or return an error.
		 */
		switch (property) {
		case CS_USERNAME:
			tds_set_user(tds_login, set_buffer);
			break;
		case CS_PASSWORD:
			tds_set_passwd(tds_login, set_buffer);
			break;
		case CS_APPNAME:
			tds_set_app(tds_login, set_buffer);
			break;
		case CS_HOSTNAME:
			tds_set_host(tds_login, set_buffer);
			break;
		case CS_PORT:
			tds_set_port(tds_login, *((int *) buffer));
			break;
		case CS_LOC_PROP:
			con->locale = (CS_LOCALE *) buffer;
			break;
		case CS_USERDATA:
			if (con->userdata) {
				free(con->userdata);
			}
			con->userdata = (void *) malloc(buflen + 1);
			tdsdump_log(TDS_DBG_INFO2, "%L setting userdata orig %d new %d\n", buffer, con->userdata);
			con->userdata_len = buflen;
			memcpy(con->userdata, buffer, buflen);
			break;
		case CS_BULK_LOGIN:
			memcpy(&intval, buffer, sizeof(intval));
			if (intval)
				tds_set_bulk(tds_login, 1);
			else
				tds_set_bulk(tds_login, 0);
			break;
		case CS_PACKETSIZE:
			memcpy(&intval, buffer, sizeof(intval));
			tds_set_packet(tds_login, (short) intval);
			break;
		case CS_TDS_VERSION:
			/* FIX ME
			 * (a) We don't support all versions in tds/login.c -
			 *     I tried to pick reasonable versions.
			 * (b) Might need support outside of tds/login.c
			 * (c) It's a "negotiated" property so probably
			 *     needs tds_process_env_chg() support
			 * (d) Minor - we don't check against context
			 *     which should limit the acceptable values
			 */
			if (*(int *) buffer == CS_TDS_40) {
				tds_set_version(tds_login, 4, 2);
			} else if (*(int *) buffer == CS_TDS_42) {
				tds_set_version(tds_login, 4, 2);
			} else if (*(int *) buffer == CS_TDS_46) {
				tds_set_version(tds_login, 4, 6);
			} else if (*(int *) buffer == CS_TDS_495) {
				tds_set_version(tds_login, 4, 6);
			} else if (*(int *) buffer == CS_TDS_50) {
				tds_set_version(tds_login, 5, 0);
			} else if (*(int *) buffer == CS_TDS_70) {
				tds_set_version(tds_login, 7, 0);
			} else {
				return CS_FAIL;
			}
			break;
		default:
			tdsdump_log(TDS_DBG_ERROR, "%L Unknown property %d\n", property);
			break;
		}
		if (set_buffer)
			free(set_buffer);
	} else if (action == CS_GET) {
		switch (property) {
		case CS_USERNAME:
			maxcp = tds_dstr_len(&tds_login->user_name);
			if (out_len)
				*out_len = maxcp;
			if (maxcp >= buflen)
				maxcp = buflen - 1;
			strncpy((char *) buffer, tds_dstr_cstr(&tds_login->user_name), maxcp);
			((char *) buffer)[maxcp] = '\0';
			break;
		case CS_PASSWORD:
			maxcp = tds_dstr_len(&tds_login->password);
			if (out_len)
				*out_len = maxcp;
			if (maxcp >= buflen)
				maxcp = buflen - 1;
			strncpy((char *) buffer, tds_dstr_cstr(&tds_login->password), maxcp);
			((char *) buffer)[maxcp] = '\0';
			break;
		case CS_APPNAME:
			maxcp = tds_dstr_len(&tds_login->app_name);
			if (out_len)
				*out_len = maxcp;
			if (maxcp >= buflen)
				maxcp = buflen - 1;
			strncpy((char *) buffer, tds_dstr_cstr(&tds_login->app_name), maxcp);
			((char *) buffer)[maxcp] = '\0';
			break;
		case CS_HOSTNAME:
			maxcp = tds_dstr_len(&tds_login->host_name);
			if (out_len)
				*out_len = maxcp;
			if (maxcp >= buflen)
				maxcp = buflen - 1;
			strncpy((char *) buffer, tds_dstr_cstr(&tds_login->host_name), maxcp);
			((char *) buffer)[maxcp] = '\0';
			break;
		case CS_SERVERNAME:
			maxcp = tds_dstr_len(&tds_login->server_name);
			if (out_len)
				*out_len = maxcp;
			if (maxcp >= buflen)
				maxcp = buflen - 1;
			strncpy((char *) buffer, tds_dstr_cstr(&tds_login->server_name), maxcp);
			((char *) buffer)[maxcp] = '\0';
			break;
		case CS_LOC_PROP:
			buffer = (CS_VOID *) con->locale;
			break;
		case CS_USERDATA:
			tdsdump_log(TDS_DBG_INFO2, "%L fetching userdata %d\n", con->userdata);
			maxcp = con->userdata_len;
			if (out_len)
				*out_len = maxcp;
			if (maxcp > buflen)
				maxcp = buflen;
			memcpy(buffer, con->userdata, maxcp);
			break;
		case CS_CON_STATUS:
			if (!(IS_TDSDEAD(tds)))
				intval |= CS_CONSTAT_CONNECTED;
			else
				intval &= ~CS_CONSTAT_CONNECTED;
			if (tds && tds->state == TDS_DEAD)
				intval |= CS_CONSTAT_DEAD;
			else
				intval &= ~CS_CONSTAT_DEAD;
			memcpy(buffer, &intval, sizeof(intval));
			break;
		case CS_BULK_LOGIN:
			if (tds_login->bulk_copy)
				intval = CS_FALSE;
			else
				intval = CS_TRUE;
			memcpy(buffer, &intval, sizeof(intval));
			break;
		case CS_PACKETSIZE:
			if (tds && tds->env)
				intval = tds->env->block_size;
			else
				intval = tds_login->block_size;
			memcpy(buffer, &intval, sizeof(intval));
			if (out_len)
				*out_len = sizeof(intval);
			break;
		case CS_TDS_VERSION:
			switch (tds_version(tds, NULL)) {
			case 40:
				(*(int *) buffer = CS_TDS_40);
				break;
			case 42:
				(*(int *) buffer = CS_TDS_42);
				break;
			case 46:
				(*(int *) buffer = CS_TDS_46);
				break;
			case 40 + 95:
				(*(int *) buffer = CS_TDS_495);
				break;
			case 50:
				(*(int *) buffer = CS_TDS_50);
				break;
			case 70:
				(*(int *) buffer = CS_TDS_70);
				break;
			case 80:
				(*(int *) buffer = CS_TDS_80);
				break;
			default:
				return CS_FAIL;
			}
			break;
		case CS_PARENT_HANDLE:
			*(CS_CONTEXT **) buffer = con->ctx;
			break;

		default:
			tdsdump_log(TDS_DBG_ERROR, "%L Unknown property %d\n", property);
			break;
		}
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_connect(CS_CONNECTION * con, CS_CHAR * servername, CS_INT snamelen)
{
	char *server;
	int needfree = 0;
	CS_CONTEXT *ctx;
	TDSCONNECTINFO *connect_info;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_connect() servername = %s\n", servername ? servername : "NULL");

	if (snamelen == 0 || snamelen == CS_UNUSED) {
		server = NULL;
	} else if (snamelen == CS_NULLTERM) {
		server = (char *) servername;
	} else {
		server = (char *) malloc(snamelen + 1);
		needfree++;
		strncpy(server, servername, snamelen);
		server[snamelen] = '\0';
	}
	tds_set_server(con->tds_login, server);
	ctx = con->ctx;
	if (!(con->tds_socket = tds_alloc_socket(ctx->tds_ctx, 512)))
		return CS_FAIL;
	tds_set_parent(con->tds_socket, (void *) con);
	if (!(connect_info = tds_read_config_info(NULL, con->tds_login, ctx->tds_ctx->locale))) {
		tds_free_socket(con->tds_socket);
		con->tds_socket = NULL;
		return CS_FAIL;
	}
	if (tds_connect(con->tds_socket, connect_info) == TDS_FAIL) {
		con->tds_socket = NULL;
		tds_free_connect(connect_info);
		if (needfree)
			free(server);
		tdsdump_log(TDS_DBG_FUNC, "%L leaving ct_connect() returning %d\n", CS_FAIL);
		return CS_FAIL;
	}
	tds_free_connect(connect_info);

	if (needfree)
		free(server);

	tdsdump_log(TDS_DBG_FUNC, "%L leaving ct_connect() returning %d\n", CS_SUCCEED);
	return CS_SUCCEED;
}

CS_RETCODE
ct_cmd_alloc(CS_CONNECTION * con, CS_COMMAND ** cmd)
{

	tdsdump_log(TDS_DBG_FUNC, "%L ct_cmd_alloc()\n");

	*cmd = (CS_COMMAND *) malloc(sizeof(CS_COMMAND));
	memset(*cmd, '\0', sizeof(CS_COMMAND));

	/* so we know who we belong to */
	(*cmd)->con = con;

	return CS_SUCCEED;
}

CS_RETCODE
ct_command(CS_COMMAND * cmd, CS_INT type, const CS_VOID * buffer, CS_INT buflen, CS_INT option)
{
	int query_len;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_command()\n");
	/* starting a command invalidates the previous command. This means any
	 * input params go away. */
	if (cmd->input_params && !((CS_LANG_CMD == type) && (CS_MORE == option))) {
		param_clear(cmd->input_params);
		cmd->input_params = NULL;
	}
	/* TODO some type require different handling, save type and use it */
	switch (type) {
	case CS_LANG_CMD:
		switch (option) {
		case CS_MORE:	/* The text in buffer is only part of the language command to be executed. */
		case CS_END:	/* The text in buffer is the last part of the language command to be executed. */
		case CS_UNUSED:	/* Equivalent to CS_END. */
			if (buflen == CS_NULLTERM) {
				query_len = strlen((const char *) buffer);
			} else {
				query_len = buflen;
			}
			if (cmd->query)
				free(cmd->query);
			/* small fix for no crash */
			if (query_len == CS_UNUSED) {
				cmd->query = NULL;
				return CS_FAIL;
			}
			/* TODO some type pass NULL or INT values... */
			cmd->query = (char *) malloc(query_len + 1);
			strncpy(cmd->query, (const char *) buffer, query_len);
			cmd->query[query_len] = '\0';

			break;
		default:
			return CS_FAIL;
		}
		break;

	case CS_RPC_CMD:

		/* Code changed for RPC functionality -SUHA */
		/* RPC code changes starts here */

		if (cmd == NULL)
			return CS_FAIL;

		rpc_clear(cmd->rpc);
		cmd->rpc = (CSREMOTE_PROC *) malloc(sizeof(CSREMOTE_PROC));
		if (cmd->rpc == (CSREMOTE_PROC *) NULL)
			return CS_FAIL;

		memset(cmd->rpc, 0, sizeof(CSREMOTE_PROC));

		if (buflen == CS_NULLTERM) {
			cmd->rpc->name = strdup(buffer);
			if (cmd->rpc->name == (char *) NULL)
				return CS_FAIL;
		} else if (buflen > 0) {
			cmd->rpc->name = (char *) malloc(buflen + 1);
			if (cmd->rpc->name == (char *) NULL)
				return CS_FAIL;
			memset(cmd->rpc->name, 0, buflen + 1);
			strncpy(cmd->rpc->name, (const char *) buffer, buflen);
		} else {
			return CS_FAIL;
		}

		cmd->rpc->param_list = NULL;

		tdsdump_log(TDS_DBG_INFO1, "%L ct_command() added rpcname \"%s\"\n", cmd->rpc->name);

		/* FIXME: I don't know the value for RECOMPILE, NORECOMPILE options. Hence assigning zero  */
		switch (option) {
		case CS_RECOMPILE:	/* Recompile the stored procedure before executing it. */
			cmd->rpc->options = 0;
			break;
		case CS_NO_RECOMPILE:	/* Do not recompile the stored procedure before executing it. */
			cmd->rpc->options = 0;
			break;
		case CS_UNUSED:	/* Equivalent to CS_NO_RECOMPILE. */
			cmd->rpc->options = 0;
			break;
		default:
			return CS_FAIL;
		}
		break;
		/* RPC code changes ends here */

	case CS_SEND_DATA_CMD:
		switch (option) {
		case CS_COLUMN_DATA:	/* The data will be used for a text or image column update. */
			cmd->send_data_started = 0;
			break;
		case CS_BULK_DATA:	/* For internal Sybase use only. The data will be used for a bulk copy operation. */
		default:
			return CS_FAIL;
		}
		break;

	case CS_SEND_BULK_CMD:
		switch (option) {
		case CS_BULK_INIT:	/* For internal Sybase use only. Initialize a bulk copy operation. */
		case CS_BULK_CONT:	/* For internal Sybase use only. Continue a bulk copy operation. */
		default:
			return CS_FAIL;
		}
		break;

	default:
		return CS_FAIL;
	}

	cmd->command_type = type;


	return CS_SUCCEED;
}

CS_RETCODE
ct_send_dyn(CS_COMMAND * cmd)
{
	TDSDYNAMIC *dyn;

	if (cmd->dynamic_cmd == CS_PREPARE) {
		cmd->dynamic_cmd = 0;
		if (tds_submit_prepare(cmd->con->tds_socket, cmd->query, cmd->dyn_id, NULL, NULL) == TDS_FAIL)
			return CS_FAIL;
		else
			return CS_SUCCEED;
	} else if (cmd->dynamic_cmd == CS_EXECUTE) {
		cmd->dynamic_cmd = 0;
		dyn = tds_lookup_dynamic(cmd->con->tds_socket, cmd->dyn_id);
		if (!dyn || tds_submit_execute(cmd->con->tds_socket, dyn) == TDS_FAIL)
			return CS_FAIL;
		else
			return CS_SUCCEED;
	}
	return CS_FAIL;
}

CS_RETCODE
ct_send(CS_COMMAND * cmd)
{
	TDSSOCKET *tds;
	CS_RETCODE ret;
	CSREMOTE_PROC **rpc;
	TDSPARAMINFO *pparam_info;

	tds = cmd->con->tds_socket;
	tdsdump_log(TDS_DBG_FUNC, "%L ct_send()\n");

	cmd->results_state = _CS_RES_INIT;

	if (cmd->dynamic_cmd)
		return ct_send_dyn(cmd);

	/* Code changed for RPC functionality -SUHA */
	/* RPC code changes starts here */

	if (cmd->command_type == CS_RPC_CMD) {
		/* sanity */
		if (cmd == NULL || cmd->rpc == NULL	/* ct_command should allocate pointer */
		    || cmd->rpc->name == NULL) {	/* can't be ready without a name      */
			return CS_FAIL;
		}

		rpc = &(cmd->rpc);
		pparam_info = paraminfoalloc(tds, cmd->rpc->param_list);
		ret = tds_submit_rpc(tds, cmd->rpc->name, pparam_info);

		tds_free_param_results(pparam_info);

		if (ret == TDS_FAIL) {
			return CS_FAIL;
		}

		return CS_SUCCEED;
	}

	/* RPC Code changes ends here */

	if (cmd->command_type == CS_LANG_CMD) {
		ret = CS_FAIL;
		if (cmd->input_params) {
			pparam_info = paraminfoalloc(tds, cmd->input_params);
			ret = tds_submit_query_params(tds, cmd->query, pparam_info);
			tds_free_param_results(pparam_info);
		} else {
			ret = tds_submit_query(tds, cmd->query);
		}
		if (ret == TDS_FAIL) {
			tdsdump_log(TDS_DBG_WARN, "%L ct_send() failed\n");
			return CS_FAIL;
		} else {
			tdsdump_log(TDS_DBG_INFO2, "%L ct_send() succeeded\n");
			return CS_SUCCEED;
		}
	}

	/* Code added for CURSOR support */

	if (cmd->command_type == CS_CUR_CMD) {
		/* sanity */
		/* ct_cursor declare should allocate cursor pointer 
		   cursor stmt cannot be NULL 
		   cursor name cannot be NULL  */ 

		int something_to_send = 0;
		int cursor_open_sent  = 0;

		if (cmd == NULL || tds->cursor == NULL  || 
			tds->cursor->query == NULL || tds->cursor->cursor_name == NULL ) { 
			return CS_FAIL;  
		}

		if (tds->cursor->status.declare == _CS_CURS_TYPE_REQUESTED) {
			ret =  tds_cursor_declare(tds, &something_to_send);
			if (ret == CS_SUCCEED){
				tds->cursor->status.declare = _CS_CURS_TYPE_SENT; /* Cursor is declared */
			}
			else {
				tdsdump_log(TDS_DBG_WARN, "%L ct_send(): cursor declare failed \n");		  
				return CS_FAIL;
			}
		}
	
		if (tds->cursor->status.cursor_row == _CS_CURS_TYPE_REQUESTED && 
			tds->cursor->status.declare == _CS_CURS_TYPE_SENT) {

 			ret = tds_cursor_setrows(tds, &something_to_send);
			if (ret == CS_SUCCEED){
				tds->cursor->status.cursor_row = _CS_CURS_TYPE_SENT; /* Cursor rows set */
			}
			else {
				tdsdump_log(TDS_DBG_WARN, "%L ct_send(): cursor set rows failed\n");
				return CS_FAIL;
			}
		}

		if (tds->cursor->status.open == _CS_CURS_TYPE_REQUESTED && 
			tds->cursor->status.declare == _CS_CURS_TYPE_SENT) {

			ret = tds_cursor_open(tds, &something_to_send);
 			if (ret == CS_SUCCEED){
				tds->cursor->status.open = _CS_CURS_TYPE_SENT;
				cursor_open_sent = 1;
			}
			else {
				tdsdump_log(TDS_DBG_WARN, "%L ct_send(): cursor open failed\n");
				return CS_FAIL;
			}
		}

		if (something_to_send) {
			tdsdump_log(TDS_DBG_WARN, "%L ct_send(): sending cursor commands\n");
			tds->state = TDS_QUERYING;
			tds_flush_packet(tds);
			something_to_send = 0;

			/* reinsert ct-send_cursor handling here */

			return CS_SUCCEED;
		}

		if (tds->cursor->status.close == _CS_CURS_TYPE_REQUESTED){
			ret = tds_cursor_close(tds);
			tds->cursor->status.close = _CS_CURS_TYPE_SENT;
			if (tds->cursor->status.dealloc == _CS_CURS_TYPE_REQUESTED)
				tds->cursor->status.dealloc = _CS_CURS_TYPE_SENT;
		}

		if (tds->cursor->status.dealloc == _CS_CURS_TYPE_REQUESTED){
			ret = tds_cursor_dealloc(tds);
			tds_free_all_results(tds);
		}
		
		return CS_SUCCEED;
	}

	if (cmd->command_type == CS_SEND_DATA_CMD) {
		tds->state = TDS_QUERYING;
		tds_flush_packet(tds);
	}

	return CS_SUCCEED;
}

CS_RETCODE
ct_results_dyn(CS_COMMAND * cmd, CS_INT * result_type)
{
	TDSSOCKET *tds;
	TDSDYNAMIC *dyn;

	tds = cmd->con->tds_socket;

	if (cmd->dynamic_cmd == CS_DESCRIBE_INPUT) {
		dyn = tds->cur_dyn;
		if (dyn->dyn_state) {
			dyn->dyn_state = 0;
			return CS_END_RESULTS;
		} else {
			dyn->dyn_state++;
			*result_type = CS_DESCRIBE_RESULT;
			return CS_SUCCEED;
		}
	}
	return CS_FAIL;
}

CS_RETCODE
ct_results(CS_COMMAND * cmd, CS_INT * result_type)
{
	TDSSOCKET *tds;
	CS_CONTEXT *context;

	int tdsret;
	int rowtype;
	int computeid;
	CS_INT res_type;
	CS_INT done_flags;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_results()\n");

	cmd->bind_count = CS_UNUSED;

	context = cmd->con->ctx;

	if (cmd->dynamic_cmd) {
		return ct_results_dyn(cmd, result_type);
	}

	tds = cmd->con->tds_socket;
	cmd->row_prefetched = 0;

	/*
	 * depending on the current results state, we may
	 * not need to call tds_process_result_tokens...
	 */

	switch (cmd->results_state) {
	case _CS_RES_CMD_SUCCEED:
		*result_type = CS_CMD_SUCCEED;
		cmd->results_state = _CS_RES_CMD_DONE;
		return CS_SUCCEED;
	case _CS_RES_CMD_DONE:
		*result_type = CS_CMD_DONE;
		cmd->results_state = _CS_RES_INIT;
		return CS_SUCCEED;
	case _CS_RES_INIT:				/* first time in after ct_send */
	case _CS_RES_RESULTSET_EMPTY:	/* we returned a format result */
	default:
		break;
	}

	/*
	 * see what "result" tokens we have. a "result" in ct-lib terms also
	 * includes row data. Some result types always get reported back  to
	 * the calling program, others are only reported back if the relevant
	 * config flag is set.
	 */

	for (;;) {

		tdsret = tds_process_result_tokens(tds, &res_type, &done_flags);

		tdsdump_log(TDS_DBG_FUNC, "%L ct_results() process_result_tokens returned %d (type %d) \n",
			    tdsret, res_type);

		switch (tdsret) {

		case TDS_SUCCEED:

			cmd->curr_result_type = res_type;

			switch (res_type) {

			case CS_COMPUTEFMT_RESULT:
			case CS_ROWFMT_RESULT:

				/*
				 * set results state to indicate that we
				 * have a result set (empty for the moment)
				 * If the CS_EXPOSE_FMTS  property has been
				 * set in ct_config(), we need to return an
				 * appropraite format result, otherwise just
				 * carry on and get the next token.....
				 */

				cmd->results_state = _CS_RES_RESULTSET_EMPTY;

				if (context->config.cs_expose_formats) {
					*result_type = res_type;
					return CS_SUCCEED;
				}
				break;

			case CS_ROW_RESULT:

				/*
				 * we've hit a data row. pass back that fact
				 * to the calling program. set results state
				 * to show that the result set has rows...
				 */

				cmd->results_state = _CS_RES_RESULTSET_ROWS;
				if (cmd->command_type == CS_CUR_CMD) {
					*result_type = CS_CURSOR_RESULT;
				} else {
					*result_type = CS_ROW_RESULT;
				}
				return CS_SUCCEED;
				break;

			case CS_COMPUTE_RESULT:

				/*
				 * we've hit a compute data row. We have to get hold of this
				 * data now, as it's necessary  to tie this data back to its
				 * result format...the user may call ct_res_info() & friends
				 * after getting back a compute "result".
				 *
				 * but first, if we've hit this compute row without having
				 * hit a data row first, we need to return a  CS_ROW_RESULT
				 * before letting them have the compute row...
				 */

				if (cmd->results_state == _CS_RES_RESULTSET_EMPTY) {
					*result_type = CS_ROW_RESULT;
					tds->curr_resinfo = tds->res_info;
					cmd->results_state = _CS_RES_RESULTSET_ROWS;
					return CS_SUCCEED;
				}

				tdsret = tds_process_row_tokens(tds, &rowtype, &computeid);

				/* set results state to show that the result set has rows... */

				cmd->results_state = _CS_RES_RESULTSET_ROWS;

				*result_type = res_type;
				if (tdsret == TDS_SUCCEED) {
					if (rowtype == TDS_COMP_ROW) {
						cmd->row_prefetched = 1;
						return CS_SUCCEED;
					} else {
						/* this couldn't really happen, but... */
						return CS_FAIL;
					}
				} else
					return CS_FAIL;
				break;

			case TDS_DONE_RESULT:

				/*
				 * A done token signifies the end of a logical
				 * command. There are three possibilities...
				 * 1. Simple command with no result set, i.e.
				 *    update, delete, insert
				 * 2. Command with result set but no rows
				 * 3. Command with result set and rows
				 * in these cases we need to:
				 * 1. return CS_CMD_FAIL/SUCCED depending on
				 *    the status returned in done_flags
				 * 2. "manufacture" a CS_ROW_RESULT return,
				 *    and set the results state to DONE
				 * 3. return with CS_CMD_DONE and reset the
				 *    results_state
				 */ 

				tdsdump_log(TDS_DBG_FUNC, "%L ct_results() results state = %d\n",cmd->results_state);
				switch (cmd->results_state) {

				case _CS_RES_INIT:  
				case _CS_RES_STATUS:  
					if (done_flags & TDS_DONE_ERROR)
						*result_type = CS_CMD_FAIL;
					else
						*result_type = CS_CMD_SUCCEED;
					cmd->results_state = _CS_RES_CMD_DONE;
					break;

				case _CS_RES_RESULTSET_EMPTY:
					if (cmd->command_type == CS_CUR_CMD) {
						*result_type = CS_CURSOR_RESULT;
						cmd->results_state = _CS_RES_RESULTSET_ROWS;
					} else {
						*result_type = CS_ROW_RESULT;
						cmd->results_state = _CS_RES_CMD_DONE;
					}
					break;

				case _CS_RES_RESULTSET_ROWS:
					*result_type = CS_CMD_DONE;
					cmd->results_state = _CS_RES_INIT;
					break;

				}
				return CS_SUCCEED;
				break;
				 
			case TDS_DONEINPROC_RESULT:

				/*
				 * A doneinproc token may signify the end of a
				 * logical command if the command had a result
				 * set. Otherwise it is ignored....
				 */

				switch (cmd->results_state) {
				case _CS_RES_INIT:   /* command had no result set */
					break;
				case _CS_RES_RESULTSET_EMPTY:
					if (cmd->command_type == CS_CUR_CMD) {
						*result_type = CS_CURSOR_RESULT;
					} else {
						*result_type = CS_ROW_RESULT;
					}
					cmd->results_state = _CS_RES_CMD_DONE;
					return CS_SUCCEED;
					break;
				case _CS_RES_RESULTSET_ROWS:
					*result_type = CS_CMD_DONE;
					cmd->results_state = _CS_RES_INIT;
					return CS_SUCCEED;
					break;
				}
				break;

			case TDS_DONEPROC_RESULT:

				/*
				 * A DONEPROC result means the end of a logical
				 * command only if it was one of the commands
				 * directly sent from ct_send, not as a result
				 * of a nested stored procedure call. We know
				 * if this is the case if a STATUS_RESULT was
				 * received immediately prior to the DONE_PROC
				 */

				if (cmd->results_state == _CS_RES_STATUS) { 
					if (done_flags & TDS_DONE_ERROR)
						*result_type = CS_CMD_FAIL;
					else
						*result_type = CS_CMD_SUCCEED;
					cmd->results_state = _CS_RES_CMD_DONE;
					return CS_SUCCEED;
				}

				break;

			case CS_PARAM_RESULT:
				cmd->row_prefetched = 1;
				*result_type = res_type;
				return CS_SUCCEED;
				break;

			case CS_STATUS_RESULT:
				_ct_process_return_status(tds);
				cmd->row_prefetched = 1;
				*result_type = res_type;
				cmd->results_state = _CS_RES_STATUS;
				return CS_SUCCEED;
				break;
				
			default:
				*result_type = res_type;
				return CS_SUCCEED;
				break;
			}

			break;

		case TDS_NO_MORE_RESULTS:
			return CS_END_RESULTS;
			break;

		case TDS_FAIL:
		default:
			return CS_FAIL;
			break;

		}  /* switch (tdsret) */
	}      /* for (;;)        */
}


CS_RETCODE
ct_bind(CS_COMMAND * cmd, CS_INT item, CS_DATAFMT * datafmt, CS_VOID * buffer, CS_INT * copied, CS_SMALLINT * indicator)
{
	TDSCOLINFO *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	CS_CONNECTION *con = cmd->con;
	CS_INT bind_count;

	tds = (TDSSOCKET *) cmd->con->tds_socket;
	resinfo = tds->curr_resinfo;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_bind() datafmt count = %d column_number = %d\n", datafmt->count, item);

	/* check item value */
	if (!resinfo || item <= 0 || item > resinfo->num_cols)
		return CS_FAIL;

	colinfo = resinfo->columns[item - 1];

	/* check whether the request is for array binding and ensure that user */
	/* supplies the same datafmt->count to the subsequent ct_bind calls    */

	bind_count = (datafmt->count == 0) ? 1 : datafmt->count;

	/* first bind for this result set */

	if (cmd->bind_count == CS_UNUSED) {
		cmd->bind_count = bind_count;
	} else {
		/* all subsequent binds for this result set - the bind counts must be the same */
		if (cmd->bind_count != bind_count) {
			_ctclient_msg(con, "ct_bind", 1, 1, 1, 137, "%d, %d", bind_count, cmd->bind_count);
			return CS_FAIL;
		}
	}

	/* bind the column_varaddr to the address of the buffer */

	colinfo = resinfo->columns[item - 1];
	colinfo->column_varaddr = (char *) buffer;
	colinfo->column_bindtype = datafmt->datatype;
	colinfo->column_bindfmt = datafmt->format;
	colinfo->column_bindlen = datafmt->maxlength;
	if (indicator) {
		colinfo->column_nullbind = (TDS_CHAR *) indicator;
	}
	if (copied) {
		colinfo->column_lenbind = (TDS_CHAR *) copied;
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_fetch(CS_COMMAND * cmd, CS_INT type, CS_INT offset, CS_INT option, CS_INT * rows_read)
{
	TDS_INT rowtype;
	TDS_INT computeid;
	TDS_INT ret;
	TDS_INT marker;
	TDS_INT temp_count;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_fetch()\n");

	/* We'll call a special function for fetches from a cursor            */
	/* the processing is too incompatible to patch into a single function */

	if (cmd->command_type == CS_CUR_CMD) {
		return _ct_fetch_cursor(cmd, type, offset, option, rows_read);
	}

	if (rows_read)
		*rows_read = 0;

	/* taking a copy of the cmd->bind_count value. */
	temp_count = cmd->bind_count;

	if ( cmd->bind_count == CS_UNUSED ) 
		cmd->bind_count = 1;

	/* compute rows and parameter results have been pre-fetched by ct_results() */

	if (cmd->row_prefetched) {
		cmd->row_prefetched = 0;
		cmd->get_data_item = 0;
		cmd->get_data_bytes_returned = 0;
		if (_ct_bind_data(cmd, 0))
			return CS_ROW_FAIL;
		if (rows_read)
			*rows_read = 1;
		return CS_SUCCEED;
	}

	if (cmd->results_state == _CS_RES_CMD_DONE)
		return CS_END_DATA;
	if (cmd->curr_result_type == CS_COMPUTE_RESULT)
		return CS_END_DATA;
	if (cmd->curr_result_type == CS_CMD_FAIL)
		return CS_CMD_FAIL;


	marker = tds_peek(cmd->con->tds_socket);
	if ((cmd->curr_result_type == CS_ROW_RESULT && marker != TDS_ROW_TOKEN) ||
		(cmd->curr_result_type == CS_STATUS_RESULT && marker != TDS_RETURNSTATUS_TOKEN) )
		return CS_END_DATA;

	/* Array Binding Code changes start here */

	for (temp_count = 0; temp_count < cmd->bind_count; temp_count++) {

		ret = tds_process_row_tokens_ct(cmd->con->tds_socket, &rowtype, &computeid);

		tdsdump_log(TDS_DBG_FUNC, "%L inside ct_fetch()process_row_tokens returned %d\n", ret);

		switch (ret) {
			case TDS_SUCCEED: 
				cmd->get_data_item = 0;
				cmd->get_data_bytes_returned = 0;
				if (rowtype == TDS_REG_ROW || rowtype == TDS_COMP_ROW) {
					if (_ct_bind_data(cmd, temp_count))
						return CS_ROW_FAIL;
					if (rows_read)
						*rows_read = *rows_read + 1;
				}
				break;
		
			case TDS_NO_MORE_ROWS: 
				return CS_END_DATA;
				break;

			default:
				return CS_FAIL;
				break;
		}

		/* have we reached the end of the rows ? */

		marker = tds_peek(cmd->con->tds_socket);

		if (cmd->curr_result_type == CS_ROW_RESULT && marker != TDS_ROW_TOKEN)
			break;

	} 

	/* Array Binding Code changes end here */

	return CS_SUCCEED;
}

static CS_RETCODE
_ct_fetch_cursor(CS_COMMAND * cmd, CS_INT type, CS_INT offset, CS_INT option, CS_INT * rows_read)
{
	TDSSOCKET * tds;
	TDS_INT restype;
	TDS_INT rowtype;
	TDS_INT computeid;
	TDS_INT ret;
	TDS_INT temp_count;
	TDS_INT done_flags;
	TDS_INT rows_this_fetch = 0;

	tdsdump_log(TDS_DBG_FUNC, "%L _ct_fetch_cursor()\n");

	tds = cmd->con->tds_socket;

	if (rows_read)
		*rows_read = 0;

	/* taking a copy of the cmd->bind_count value. */
	temp_count = cmd->bind_count;

	if ( cmd->bind_count == CS_UNUSED ) 
		cmd->bind_count = 1;

	/* currently we are placing this restriction on cursor fetches.  */
	/* the alternatives are too awful to contemplate at the moment   */
	/* i.e. buffering all the rows from the fetch internally...      */

	if (cmd->bind_count < tds->cursor->cursor_rows) {
		tdsdump_log(TDS_DBG_WARN, "%L _ct_fetch_cursor(): bind count must equal cursor rows \n");
		return CS_FAIL;
	}

	if ( tds_cursor_fetch(tds) == CS_SUCCEED) {
		tds->cursor->status.fetch = _CS_CURS_TYPE_SENT;
	}
	else {
		tdsdump_log(TDS_DBG_WARN, "%L ct_fetch(): cursor fetch failed\n");
		return CS_FAIL;
	}

	while ((tds_process_result_tokens(tds, &restype, &done_flags)) == TDS_SUCCEED) {
		switch (restype) {
			case CS_ROWFMT_RESULT:
				break;
			case CS_ROW_RESULT:
				for (temp_count = 0; temp_count < cmd->bind_count; temp_count++) {
			
					ret = tds_process_row_tokens_ct(tds, &rowtype, &computeid);
			
					tdsdump_log(TDS_DBG_FUNC, "%L _ct_fetch_cursor() tds_process_row_tokens returned %d\n", ret);
			
					if (ret == TDS_SUCCEED) {
						cmd->get_data_item = 0;
						cmd->get_data_bytes_returned = 0;
						if (rowtype == TDS_REG_ROW) {
							if (_ct_bind_data(cmd, temp_count))
								return CS_ROW_FAIL;
							if (rows_read)
								*rows_read = *rows_read + 1;
							rows_this_fetch++;
						}
					}
					else {
						if (ret == TDS_NO_MORE_ROWS) {
							break;
						} else {
							return CS_FAIL;
						}
					}
				} 
				break;
			case TDS_DONE_RESULT:
				break;
		}
	}
	if (rows_this_fetch)
		return CS_SUCCEED;
	else {
		cmd->results_state = _CS_RES_CMD_SUCCEED;
		return CS_END_DATA;
	}

}


static int
_ct_bind_data(CS_COMMAND * cmd, CS_INT offset)
{
	int i;
	TDSCOLINFO *curcol;
	TDSSOCKET *tds = cmd->con->tds_socket;
	TDSRESULTINFO *resinfo = tds->curr_resinfo;
	unsigned char *src;
	unsigned char *dest, *temp_add;
	int result = 0;
	TDS_INT srctype, srclen, desttype, len;
	CS_CONTEXT *ctx = cmd->con->ctx;
	CS_DATAFMT srcfmt, destfmt;

	tdsdump_log(TDS_DBG_FUNC, "%L _ct_bind_data()\n");

	for (i = 0; i < resinfo->num_cols; i++) {
		curcol = resinfo->columns[i];

		tdsdump_log(TDS_DBG_FUNC, "%L _ct_bind_data(): column_type: %d column_len: %d\n",
			curcol->column_type,
			curcol->column_cur_size
		);

		if (curcol->column_hidden) 
			continue;

		if (curcol->column_nullbind) {
			if (tds_get_null(resinfo->current_row, i)) {
				*((CS_SMALLINT *) curcol->column_nullbind) = -1;
			} else {
				*((CS_SMALLINT *) curcol->column_nullbind) = 0;
			}
		}
		/* printf("%d %s\n",i,resinfo->columns[i]->column_value); */

		srctype = curcol->column_type;
		desttype = _ct_get_server_type(curcol->column_bindtype);

		/* Array Binding Code changes start here */
		/* retrieve the initial bound column_varaddress */
		temp_add = (unsigned char *) curcol->column_varaddr;

		/* check for array binding cmd->bind_count >0 and skip for the */
		/* first row ie for resinfo->row_count == 1, for which the address is temp_add itself. */

		dest = temp_add + (offset * curcol->column_bindlen);

		/* Array Binding Code changes end here */

		if (dest && !tds_get_null(resinfo->current_row, i)) {

			srctype = _ct_get_client_type(curcol->column_type, curcol->column_usertype, curcol->column_size);

			src = &(resinfo->current_row[curcol->column_offset]);
			if (is_blob_type(curcol->column_type))
				src = (unsigned char *) ((TDSBLOBINFO *) src)->textvalue;

			srclen = curcol->column_cur_size;
			srcfmt.datatype = srctype;
			srcfmt.maxlength = srclen;
			srcfmt.locale = cmd->con->locale;

			destfmt.datatype = curcol->column_bindtype;
			destfmt.maxlength = curcol->column_bindlen;
			destfmt.locale = cmd->con->locale;
			destfmt.format = curcol->column_bindfmt;
			/* if convert return FAIL mark error but process other columns */
			if ((result= cs_convert(ctx, &srcfmt, (CS_VOID *) src, &destfmt, (CS_VOID *) dest, &len) != CS_SUCCEED)) {
				tdsdump_log(TDS_DBG_FUNC, "%L cs_convert-result = %d\n", result);
				result = 1;
				len = 0;
				tdsdump_log(TDS_DBG_INFO1, "%L \n  convert failed for %d \n", srcfmt.datatype);
			}

			if (curcol->column_lenbind) {

				*((CS_INT *) curcol->column_lenbind) = len;
			}

		} else {
			if (curcol->column_lenbind)
				*((CS_INT *) curcol->column_lenbind) = 0;
		}
	}
	return result;
}

CS_RETCODE
ct_cmd_drop(CS_COMMAND * cmd)
{
	tdsdump_log(TDS_DBG_FUNC, "%L ct_cmd_drop()\n");
	if (cmd) {
		if (cmd->query)
			free(cmd->query);
		if (cmd->input_params)
			param_clear(cmd->input_params);
		free(cmd);
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_close(CS_CONNECTION * con, CS_INT option)
{
	tdsdump_log(TDS_DBG_FUNC, "%L ct_close()\n");
	tds_free_socket(con->tds_socket);
	con->tds_socket = NULL;
	return CS_SUCCEED;
}


CS_RETCODE
ct_con_drop(CS_CONNECTION * con)
{
	tdsdump_log(TDS_DBG_FUNC, "%L ct_con_drop()\n");
	if (con) {
		if (con->userdata)
			free(con->userdata);
		if (con->tds_login)
			tds_free_login(con->tds_login);
		free(con);
	}
	return CS_SUCCEED;
}


static int
_ct_get_client_type(int datatype, int usertype, int size)
{
	tdsdump_log(TDS_DBG_FUNC, "%L _ct_get_client_type(type %d, user %d, size %d)\n", datatype, usertype, size);
	switch (datatype) {
	case SYBBIT:
	case SYBBITN:
		return CS_BIT_TYPE;
		break;
	case SYBCHAR:
	case SYBVARCHAR:
		return CS_CHAR_TYPE;
		break;
	case SYBINT8:
		return CS_LONG_TYPE;
		break;
	case SYBINT4:
		return CS_INT_TYPE;
		break;
	case SYBINT2:
		return CS_SMALLINT_TYPE;
		break;
	case SYBINT1:
		return CS_TINYINT_TYPE;
		break;
	case SYBINTN:
		switch (size) {
		case 8:
			return CS_LONG_TYPE;
		case 4:
			return CS_INT_TYPE;
		case 2:
			return CS_SMALLINT_TYPE;
		case 1:
			return CS_TINYINT_TYPE;
		default:
			fprintf(stderr, "Unknown size %d for SYBINTN\n", size);
		}
		break;
	case SYBREAL:
		return CS_REAL_TYPE;
		break;
	case SYBFLT8:
		return CS_FLOAT_TYPE;
		break;
	case SYBFLTN:
		if (size == 4) {
			return CS_REAL_TYPE;
		} else if (size == 8) {
			return CS_FLOAT_TYPE;
		} else {
			fprintf(stderr, "Error! unknown float size of %d\n", size);
		}
	case SYBMONEY:
		return CS_MONEY_TYPE;
		break;
	case SYBMONEY4:
		return CS_MONEY4_TYPE;
		break;
	case SYBMONEYN:
		if (size == 4) {
			return CS_MONEY4_TYPE;
		} else if (size == 8) {
			return CS_MONEY_TYPE;
		} else {
			fprintf(stderr, "Error! unknown money size of %d\n", size);
		}
	case SYBDATETIME:
		return CS_DATETIME_TYPE;
		break;
	case SYBDATETIME4:
		return CS_DATETIME4_TYPE;
		break;
	case SYBDATETIMN:
		if (size == 4) {
			return CS_DATETIME4_TYPE;
		} else if (size == 8) {
			return CS_DATETIME_TYPE;
		} else {
			fprintf(stderr, "Error! unknown date size of %d\n", size);
		}
		break;
	case SYBNUMERIC:
		return CS_NUMERIC_TYPE;
		break;
	case SYBDECIMAL:
		return CS_DECIMAL_TYPE;
		break;
	case SYBBINARY:
		return CS_BINARY_TYPE;
		break;
	case SYBIMAGE:
		return CS_IMAGE_TYPE;
		break;
	case SYBVARBINARY:
		return CS_VARBINARY_TYPE;
		break;
	case SYBTEXT:
		return CS_TEXT_TYPE;
		break;
	case SYBUNIQUE:
		return CS_UNIQUE_TYPE;
		break;
	case SYBLONGBINARY:
		if (usertype == USER_UNICHAR_TYPE || usertype == USER_UNIVARCHAR_TYPE)
			return CS_UNICHAR_TYPE;
		return CS_CHAR_TYPE;
		break;
	}

	return CS_FAIL;
}

int
_ct_get_server_type(int datatype)
{
	tdsdump_log(TDS_DBG_FUNC, "%L _ct_get_server_type(%d)\n", datatype);
	switch (datatype) {
	case CS_IMAGE_TYPE:
		return SYBIMAGE;
		break;
	case CS_BINARY_TYPE:
		return SYBBINARY;
		break;
	case CS_BIT_TYPE:
		return SYBBIT;
		break;
	case CS_CHAR_TYPE:
		return SYBCHAR;
		break;
	case CS_LONG_TYPE:
		return SYBINT8;
		break;
	case CS_INT_TYPE:
		return SYBINT4;
		break;
	case CS_SMALLINT_TYPE:
		return SYBINT2;
		break;
	case CS_TINYINT_TYPE:
		return SYBINT1;
		break;
	case CS_REAL_TYPE:
		return SYBREAL;
		break;
	case CS_FLOAT_TYPE:
		return SYBFLT8;
		break;
	case CS_MONEY_TYPE:
		return SYBMONEY;
		break;
	case CS_MONEY4_TYPE:
		return SYBMONEY4;
		break;
	case CS_DATETIME_TYPE:
		return SYBDATETIME;
		break;
	case CS_DATETIME4_TYPE:
		return SYBDATETIME4;
		break;
	case CS_NUMERIC_TYPE:
		return SYBNUMERIC;
		break;
	case CS_DECIMAL_TYPE:
		return SYBDECIMAL;
		break;
	case CS_VARBINARY_TYPE:
		return SYBVARBINARY;
		break;
	case CS_TEXT_TYPE:
		return SYBTEXT;
		break;
	case CS_UNIQUE_TYPE:
		return SYBUNIQUE;
		break;
	case CS_LONGBINARY_TYPE:	/* vicm */
		return SYBLONGBINARY;
		break;
	case CS_UNICHAR_TYPE:
		return SYBVARCHAR;
	default:
		return -1;
		break;
	}
}

CS_RETCODE
ct_cancel(CS_CONNECTION * conn, CS_COMMAND * cmd, CS_INT type)
{
	CS_RETCODE ret;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_cancel()\n");
	if (type == CS_CANCEL_CURRENT) {
		if (conn || !cmd)
			return CS_FAIL;
		if (!_ct_fetchable_results(cmd)) {
			return CS_SUCCEED;
		}
		do {
			ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, NULL);
		} while ((ret == CS_SUCCEED) || (ret == CS_ROW_FAIL));
		if (cmd->con->tds_socket) {
			tds_free_all_results(cmd->con->tds_socket);
		}
		if (ret == CS_END_DATA) {
			return CS_SUCCEED;
		}
		return CS_FAIL;
	}

	if ((conn && cmd) || (!conn && !cmd)) {
		return CS_FAIL;
	}
	if (cmd)
		conn = cmd->con;
	if (conn && !IS_TDSDEAD(conn->tds_socket)) {
		tds_send_cancel(conn->tds_socket);
		tds_process_cancel(conn->tds_socket);
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_describe(CS_COMMAND * cmd, CS_INT item, CS_DATAFMT * datafmt)
{
	TDSSOCKET *tds;
	TDSRESULTINFO *resinfo;
	TDSCOLINFO *curcol;
	int len;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_describe()\n");
	tds = cmd->con->tds_socket;

	if (cmd->dynamic_cmd) {
		resinfo = tds->cur_dyn->res_info;
	} else {
		resinfo = cmd->con->tds_socket->curr_resinfo;;
	}

	if (item < 1 || item > resinfo->num_cols)
		return CS_FAIL;
	curcol = resinfo->columns[item - 1];
	len = curcol->column_namelen;
	if (len >= CS_MAX_NAME)
		len = CS_MAX_NAME - 1;
	strncpy(datafmt->name, curcol->column_name, len);
	/* name is always null terminated */
	datafmt->name[len] = 0;
	datafmt->namelen = len;
	/* need to turn the SYBxxx into a CS_xxx_TYPE */
	datafmt->datatype = _ct_get_client_type(curcol->column_type, curcol->column_usertype, curcol->column_size);
	tdsdump_log(TDS_DBG_INFO1, "%L ct_describe() datafmt->datatype = %d server type %d\n", datafmt->datatype,
		    curcol->column_type);
	/* FIXME is ok this value for numeric/decimal? */
	datafmt->maxlength = curcol->column_size;
	datafmt->usertype = curcol->column_usertype;
	datafmt->precision = curcol->column_prec;
	datafmt->scale = curcol->column_scale;

	/* There are other options that can be returned, but these are the
	 ** only two being noted via the TDS layer. */
	datafmt->status = 0;
	if (curcol->column_nullable)
		datafmt->status |= CS_CANBENULL;
	if (curcol->column_identity)
		datafmt->status |= CS_IDENTITY;
	if (strcmp(datafmt->name, "txts") == 0)
		datafmt->status |= CS_TIMESTAMP;

	datafmt->count = 1;
	datafmt->locale = NULL;

	return CS_SUCCEED;
}

CS_RETCODE
ct_res_info_dyn(CS_COMMAND * cmd, CS_INT type, CS_VOID * buffer, CS_INT buflen, CS_INT * out_len)
{
	TDSSOCKET *tds = cmd->con->tds_socket;
	TDSDYNAMIC *dyn;
	CS_INT int_val;

	switch (type) {
	case CS_NUMDATA:
		dyn = tds->cur_dyn;
		int_val = dyn->res_info->num_cols;
		memcpy(buffer, &int_val, sizeof(CS_INT));
		break;
	default:
		fprintf(stderr, "Unknown type in ct_res_info_dyn: %d\n", type);
		return CS_FAIL;
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_res_info(CS_COMMAND * cmd, CS_INT type, CS_VOID * buffer, CS_INT buflen, CS_INT * out_len)
{
	TDSSOCKET *tds = cmd->con->tds_socket;
	TDSRESULTINFO *resinfo = tds->curr_resinfo;
	TDSCOLINFO *curcol;
	CS_INT int_val;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_res_info()\n");
	if (cmd->dynamic_cmd) {
		return ct_res_info_dyn(cmd, type, buffer, buflen, out_len);
	}
	switch (type) {
	case CS_NUMDATA:
		int_val = 0;
		if (resinfo) {
			for (i = 0; i < resinfo->num_cols; i++) {
				curcol = resinfo->columns[i];
				if (!curcol->column_hidden) {
					int_val++;
				}
			}
		}
		tdsdump_log(TDS_DBG_FUNC, "%L ct_res_info(): Number of columns is %d\n", int_val);
		memcpy(buffer, &int_val, sizeof(CS_INT));
		break;
	case CS_ROW_COUNT:
		int_val = tds->rows_affected;
		tdsdump_log(TDS_DBG_FUNC, "%L ct_res_info(): Number of rows is %d\n", int_val);
		memcpy(buffer, &int_val, sizeof(CS_INT));
		break;
	default:
		fprintf(stderr, "Unknown type in ct_res_info: %d\n", type);
		return CS_FAIL;
		break;
	}
	return CS_SUCCEED;

}

CS_RETCODE
ct_config(CS_CONTEXT * ctx, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	CS_RETCODE ret = CS_SUCCEED;
	CS_INT *buf = (CS_INT *) buffer;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_config() action = %s property = %d\n",
		    CS_GET ? "CS_GET" : CS_SET ? "CS_SET" : CS_SUPPORTED ? "CS_SUPPORTED" : "CS_CLEAR", property);

	switch (property) {
	case CS_EXPOSE_FMTS:
		switch (action) {
		case CS_SUPPORTED:
			*buf = CS_TRUE;
			break;
		case CS_SET:
			if (*buf != CS_TRUE && *buf != CS_FALSE)
				ret = CS_FALSE;
			else
				ctx->config.cs_expose_formats = *buf;
			break;
		case CS_GET:
			if (buf)
				*buf = ctx->config.cs_expose_formats;
			else
				ret = CS_FALSE;
			break;
		case CS_CLEAR:
			ctx->config.cs_expose_formats = CS_FALSE;
			break;
		default:
			ret = CS_FALSE;
		}
		break;
	default:
		ret = CS_SUCCEED;
		break;
	}

	return ret;
}

CS_RETCODE
ct_cmd_props(CS_COMMAND * cmd, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L ct_cmd_props() action = %s property = %d\n", CS_GET ? "CS_GET" : "CS_SET", property);
	if (action == CS_GET) {
		switch (property) {
		case CS_PARENT_HANDLE:
			*(CS_CONNECTION **) buffer = cmd->con;
			break;
		default:
			break;
		}
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_compute_info(CS_COMMAND * cmd, CS_INT type, CS_INT colnum, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	TDSSOCKET *tds = cmd->con->tds_socket;
	TDSRESULTINFO *resinfo = tds->curr_resinfo;
	TDSCOLINFO *curcol;
	CS_INT int_val;
	CS_SMALLINT *dest_by_col_ptr;
	CS_TINYINT *src_by_col_ptr;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_compute_info() type = %d, colnum = %d\n", type, colnum);

	switch (type) {
	case CS_BYLIST_LEN:
		if (!resinfo) {
			int_val = 0;
		} else {
			int_val = resinfo->by_cols;
		}
		memcpy(buffer, &int_val, sizeof(CS_INT));
		if (outlen)
			*outlen = sizeof(CS_INT);
		break;
	case CS_COMP_BYLIST:
		if (buflen < (resinfo->by_cols * sizeof(CS_SMALLINT))) {
			return CS_FAIL;
		} else {
			dest_by_col_ptr = (CS_SMALLINT *) buffer;
			src_by_col_ptr = resinfo->bycolumns;
			for (i = 0; i < resinfo->by_cols; i++) {
				*dest_by_col_ptr = *src_by_col_ptr;
				dest_by_col_ptr++;
				src_by_col_ptr++;
			}
			if (outlen)
				*outlen = (resinfo->by_cols * sizeof(CS_SMALLINT));
		}
		break;
	case CS_COMP_COLID:
		if (!resinfo) {
			int_val = 0;
		} else {
			curcol = resinfo->columns[colnum - 1];
			int_val = curcol->column_operand;
		}
		memcpy(buffer, &int_val, sizeof(CS_INT));
		if (outlen)
			*outlen = sizeof(CS_INT);
		break;
	case CS_COMP_ID:
		if (!resinfo) {
			int_val = 0;
		} else {
			int_val = resinfo->computeid;
		}
		memcpy(buffer, &int_val, sizeof(CS_INT));
		if (outlen)
			*outlen = sizeof(CS_INT);
		break;
	case CS_COMP_OP:
		if (!resinfo) {
			int_val = 0;
		} else {
			curcol = resinfo->columns[colnum - 1];
			int_val = curcol->column_operator;
		}
		memcpy(buffer, &int_val, sizeof(CS_INT));
		if (outlen)
			*outlen = sizeof(CS_INT);
		break;
	default:
		fprintf(stderr, "Unknown type in ct_compute_info: %d\n", type);
		return CS_FAIL;
		break;
	}
	return CS_SUCCEED;
}

CS_RETCODE
ct_get_data(CS_COMMAND * cmd, CS_INT item, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{
	TDSSOCKET *tds = cmd->con->tds_socket;
	TDSRESULTINFO *resinfo = tds->curr_resinfo;
	TDSCOLINFO *curcol;
	TDSBLOBINFO *blob_info;
	unsigned char *src;
	TDS_INT srclen;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_get_data() item = %d buflen = %d\n", item, buflen);

	/* basic validations... */

	if (item < 1 || item > resinfo->num_cols)
		return CS_FAIL;
	if (buffer == NULL)
		return CS_FAIL;
	if (buflen == CS_UNUSED)
		return CS_FAIL;

	/* This is a new column we are being asked to return */

	if (item != cmd->get_data_item) {
		/* reset these values */
		cmd->get_data_item = item;
		cmd->get_data_bytes_returned = 0;

		/* get at the source data and length */
		curcol = resinfo->columns[item - 1];

		src = &(resinfo->current_row[curcol->column_offset]);
		if (is_blob_type(curcol->column_type))
			src = (unsigned char *) ((TDSBLOBINFO *) src)->textvalue;

		srclen = curcol->column_cur_size;

		/* now populate the io_desc structure for this data item */

		if (cmd->iodesc)
			free(cmd->iodesc);
		cmd->iodesc = malloc(sizeof(CS_IODESC));

		cmd->iodesc->iotype = CS_IODATA;
		cmd->iodesc->datatype = curcol->column_type;
		cmd->iodesc->locale = cmd->con->locale;
		cmd->iodesc->usertype = curcol->column_usertype;
		cmd->iodesc->total_txtlen = curcol->column_cur_size;
		cmd->iodesc->offset = curcol->column_offset;
		cmd->iodesc->log_on_update = CS_FALSE;

		sprintf(cmd->iodesc->name, "%*.*s.%*.*s",
			curcol->table_namelen, curcol->table_namelen, curcol->table_name,
			curcol->column_namelen, curcol->column_namelen, curcol->column_name);

		cmd->iodesc->namelen = strlen(cmd->iodesc->name);

		blob_info = (TDSBLOBINFO *) & (resinfo->current_row[curcol->column_offset]);
		memcpy(cmd->iodesc->timestamp, blob_info->timestamp, CS_TS_SIZE);
		cmd->iodesc->timestamplen = CS_TS_SIZE;
		memcpy(cmd->iodesc->textptr, blob_info->textptr, CS_TP_SIZE);
		cmd->iodesc->textptrlen = CS_TP_SIZE;

		/* if we have enough buffer to cope with all the data */
		if (buflen >= srclen) {
			memcpy(buffer, src, srclen);
			cmd->get_data_bytes_returned = srclen;
			if (outlen)
				*outlen = srclen;
			if (item < resinfo->num_cols)
				return CS_END_ITEM;
			else
				return CS_END_DATA;
		} else {
			memcpy(buffer, src, buflen);
			cmd->get_data_bytes_returned = buflen;
			if (outlen)
				*outlen = buflen;
			return CS_SUCCEED;
		}
	} else {
		/* get at the source data */
		curcol = resinfo->columns[item - 1];
		src = &(resinfo->current_row[curcol->column_offset]);
		if (is_blob_type(curcol->column_type))
			src = (unsigned char *) ((TDSBLOBINFO *) src)->textvalue;

		/* and adjust the data and length based on */
		/* what we may have already returned       */

		src += cmd->get_data_bytes_returned;
		srclen = curcol->column_cur_size - cmd->get_data_bytes_returned;

		if (buflen >= srclen) {
			memcpy(buffer, src, srclen);
			cmd->get_data_bytes_returned += srclen;
			if (outlen)
				*outlen = srclen;
			if (item < resinfo->num_cols)
				return CS_END_ITEM;
			else
				return CS_END_DATA;
		} else {
			memcpy(buffer, src, buflen);
			cmd->get_data_bytes_returned += buflen;
			if (outlen)
				*outlen = buflen;
			return CS_SUCCEED;
		}
	}

	return CS_SUCCEED;
}

CS_RETCODE
ct_send_data(CS_COMMAND * cmd, CS_VOID * buffer, CS_INT buflen)
{
	TDSSOCKET *tds = cmd->con->tds_socket;
	char writetext_cmd[512];

	char textptr_string[35];	/* 16 * 2 + 2 (0x) + 1 */
	char timestamp_string[19];	/* 8 * 2 + 2 (0x) + 1 */
	char *c;
	int s;
	char hex2[3];

	tdsdump_log(TDS_DBG_FUNC, "%L ct_send_data()\n");

	/* basic validations */

	if (cmd->command_type != CS_SEND_DATA_CMD)
		return CS_FAIL;

	if (!cmd->iodesc)
		return CS_FAIL;

	/* first ct_send_data for this column */

	if (!cmd->send_data_started) {

		/* turn the timestamp and textptr into character format */

		c = textptr_string;

		for (s = 0; s < cmd->iodesc->textptrlen; s++) {
			sprintf(hex2, "%02x", cmd->iodesc->textptr[s]);
			*c++ = hex2[0];
			*c++ = hex2[1];
		}
		*c = '\0';

		c = timestamp_string;

		for (s = 0; s < cmd->iodesc->timestamplen; s++) {
			sprintf(hex2, "%02x", cmd->iodesc->timestamp[s]);
			*c++ = hex2[0];
			*c++ = hex2[1];
		}
		*c = '\0';

		/* submit the "writetext bulk" command */

		sprintf(writetext_cmd, "writetext bulk %s 0x%s timestamp = 0x%s %s",
			cmd->iodesc->name,
			textptr_string, timestamp_string, ((cmd->iodesc->log_on_update == CS_TRUE) ? "with log" : "")
			);

		if (tds_submit_query(tds, writetext_cmd) != TDS_SUCCEED) {
			return CS_FAIL;
		}

		/* read the end token */
		if (tds_process_simple_query(tds) != TDS_SUCCEED)
			return CS_FAIL;

		cmd->send_data_started = 1;
		tds->out_flag = 0x07;
		tds_put_int(tds, cmd->iodesc->total_txtlen);
	}

	tds_put_bulk_data(tds, buffer, buflen);

	return CS_SUCCEED;
}

CS_RETCODE
ct_data_info(CS_COMMAND * cmd, CS_INT action, CS_INT colnum, CS_IODESC * iodesc)
{
	TDSSOCKET *tds = cmd->con->tds_socket;
	TDSRESULTINFO *resinfo = tds->curr_resinfo;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_data_info() colnum %d\n", colnum);

	switch (action) {
	case CS_SET:

		if (cmd->iodesc)
			free(cmd->iodesc);
		cmd->iodesc = malloc(sizeof(CS_IODESC));

		cmd->iodesc->iotype = CS_IODATA;
		cmd->iodesc->datatype = iodesc->datatype;
		cmd->iodesc->locale = cmd->con->locale;
		cmd->iodesc->usertype = iodesc->usertype;
		cmd->iodesc->total_txtlen = iodesc->total_txtlen;
		cmd->iodesc->offset = iodesc->offset;
		cmd->iodesc->log_on_update = iodesc->log_on_update;
		strcpy(cmd->iodesc->name, iodesc->name);
		cmd->iodesc->namelen = iodesc->namelen;
		memcpy(cmd->iodesc->timestamp, iodesc->timestamp, CS_TS_SIZE);
		cmd->iodesc->timestamplen = CS_TS_SIZE;
		memcpy(cmd->iodesc->textptr, iodesc->textptr, CS_TP_SIZE);
		cmd->iodesc->textptrlen = CS_TP_SIZE;
		break;

	case CS_GET:

		if (colnum < 1 || colnum > resinfo->num_cols)
			return CS_FAIL;
		if (colnum != cmd->get_data_item)
			return CS_FAIL;

		iodesc->iotype = cmd->iodesc->iotype;
		iodesc->datatype = cmd->iodesc->datatype;
		iodesc->locale = cmd->iodesc->locale;
		iodesc->usertype = cmd->iodesc->usertype;
		iodesc->total_txtlen = cmd->iodesc->total_txtlen;
		iodesc->offset = cmd->iodesc->offset;
		iodesc->log_on_update = CS_FALSE;
		strcpy(iodesc->name, cmd->iodesc->name);
		iodesc->namelen = cmd->iodesc->namelen;
		memcpy(iodesc->timestamp, cmd->iodesc->timestamp, cmd->iodesc->timestamplen);
		iodesc->timestamplen = cmd->iodesc->timestamplen;
		memcpy(iodesc->textptr, cmd->iodesc->textptr, cmd->iodesc->textptrlen);
		iodesc->textptrlen = cmd->iodesc->textptrlen;
		break;

	default:
		return CS_FAIL;
	}

	return CS_SUCCEED;
}

CS_RETCODE
ct_capability(CS_CONNECTION * con, CS_INT action, CS_INT type, CS_INT capability, CS_VOID * value)
{
	TDSLOGIN *login;
	int idx = 0;
	unsigned char bitmask = 0;
	unsigned char *mask;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_capability()\n");
	login = (TDSLOGIN *) con->tds_login;
	mask = login->capabilities;

	switch (capability) {
	case CS_DATA_NOBOUNDARY:
		idx = 13;
		bitmask = 0x01;
		break;
	case CS_DATA_NOTDSDEBUG:
		idx = 13;
		bitmask = 0x02;
		break;
	case CS_RES_NOSTRIPBLANKS:
		idx = 13;
		bitmask = 0x04;
		break;
	case CS_DATA_NOINT8:
		idx = 13;
		bitmask = 0x08;
		break;
	case CS_DATA_NOINTN:
		idx = 14;
		bitmask = 0x01;
		break;
	case CS_DATA_NODATETIMEN:
		idx = 14;
		bitmask = 0x02;
		break;
	case CS_DATA_NOMONEYN:
		idx = 14;
		bitmask = 0x04;
		break;
	case CS_CON_NOOOB:
		idx = 14;
		bitmask = 0x08;
		break;
	case CS_CON_NOINBAND:
		idx = 14;
		bitmask = 0x10;
		break;
	case CS_PROTO_NOTEXT:
		idx = 14;
		bitmask = 0x20;
		break;
	case CS_PROTO_NOBULK:
		idx = 14;
		bitmask = 0x40;
		break;
	case CS_DATA_NOSENSITIVITY:
		idx = 14;
		bitmask = 0x80;
		break;
	case CS_DATA_NOFLT4:
		idx = 15;
		bitmask = 0x01;
		break;
	case CS_DATA_NOFLT8:
		idx = 15;
		bitmask = 0x02;
		break;
	case CS_DATA_NONUM:
		idx = 15;
		bitmask = 0x04;
		break;
	case CS_DATA_NOTEXT:
		idx = 15;
		bitmask = 0x08;
		break;
	case CS_DATA_NOIMAGE:
		idx = 15;
		bitmask = 0x10;
		break;
	case CS_DATA_NODEC:
		idx = 15;
		bitmask = 0x20;
		break;
	case CS_DATA_NOLCHAR:
		idx = 15;
		bitmask = 0x40;
		break;
	case CS_DATA_NOLBIN:
		idx = 15;
		bitmask = 0x80;
		break;
	case CS_DATA_NOCHAR:
		idx = 16;
		bitmask = 0x01;
		break;
	case CS_DATA_NOVCHAR:
		idx = 16;
		bitmask = 0x02;
		break;
	case CS_DATA_NOBIN:
		idx = 16;
		bitmask = 0x04;
		break;
	case CS_DATA_NOVBIN:
		idx = 16;
		bitmask = 0x08;
		break;
	case CS_DATA_NOMNY8:
		idx = 16;
		bitmask = 0x10;
		break;
	case CS_DATA_NOMNY4:
		idx = 16;
		bitmask = 0x20;
		break;
	case CS_DATA_NODATE8:
		idx = 16;
		bitmask = 0x40;
		break;
	case CS_DATA_NODATE4:
		idx = 16;
		bitmask = 0x80;
		break;
	case CS_RES_NOMSG:
		idx = 17;
		bitmask = 0x02;
		break;
	case CS_RES_NOEED:
		idx = 17;
		bitmask = 0x04;
		break;
	case CS_RES_NOPARAM:
		idx = 17;
		bitmask = 0x08;
		break;
	case CS_DATA_NOINT1:
		idx = 17;
		bitmask = 0x10;
		break;
	case CS_DATA_NOINT2:
		idx = 17;
		bitmask = 0x20;
		break;
	case CS_DATA_NOINT4:
		idx = 17;
		bitmask = 0x40;
		break;
	case CS_DATA_NOBIT:
		idx = 17;
		bitmask = 0x80;
		break;
	default:
		return CS_FAIL;
	}			/* end capability */

	assert(13 <= idx && idx <= 17);
	assert(bitmask);

	if (type == CS_CAP_RESPONSE) {
		switch (action) {
		case CS_SET:
			/* Having established the offset and the bitmask, we can now turn the capability on or off */
			switch (*(CS_BOOL *) value) {
			case CS_TRUE:
				mask[idx] |= bitmask;
				break;
			case CS_FALSE:
				mask[idx] &= ~bitmask;
				break;
			default:
				return CS_FAIL;
			}
			break;
		case CS_GET:
			*(CS_BOOL *) value = (mask[idx] & bitmask) ? CS_TRUE : CS_FALSE;
			break;
		default:
			return CS_FAIL;
		}
		return CS_SUCCEED;
	}
	/* 
	 * End handling CS_CAP_RESPONSE (returned)
	 */

	/* 
	 * Begin handling CS_CAP_REQUEST
	 * These capabilities describe the types of requests that a server can support. 
	 */
	switch (capability) {
	case CS_PROTO_DYNPROC:
		*(CS_BOOL *) value = mask[2] & 0x01 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_FLTN:
		*(CS_BOOL *) value = mask[2] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_BITN:
		*(CS_BOOL *) value = mask[2] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_INT8:
		*(CS_BOOL *) value = mask[2] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_VOID:
		*(CS_BOOL *) value = mask[2] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CON_INBAND:
		*(CS_BOOL *) value = mask[3] & 0x01 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CON_LOGICAL:
		*(CS_BOOL *) value = mask[3] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_PROTO_TEXT:
		*(CS_BOOL *) value = mask[3] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_PROTO_BULK:
		*(CS_BOOL *) value = mask[3] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_URGNOTIF:
		*(CS_BOOL *) value = mask[3] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_SENSITIVITY:
		*(CS_BOOL *) value = mask[3] & 0x20 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_BOUNDARY:
		*(CS_BOOL *) value = mask[3] & 0x40 ? CS_TRUE : CS_FALSE;
		break;
	case CS_PROTO_DYNAMIC:
		*(CS_BOOL *) value = mask[3] & 0x80 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_MONEYN:
		*(CS_BOOL *) value = mask[4] & 0x01 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CSR_PREV:
		*(CS_BOOL *) value = mask[4] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CSR_FIRST:
		*(CS_BOOL *) value = mask[4] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CSR_LAST:
		*(CS_BOOL *) value = mask[4] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CSR_ABS:
		*(CS_BOOL *) value = mask[4] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CSR_REL:
		*(CS_BOOL *) value = mask[4] & 0x20 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CSR_MULTI:
		*(CS_BOOL *) value = mask[4] & 0x40 ? CS_TRUE : CS_FALSE;
		break;
	case CS_CON_OOB:
		*(CS_BOOL *) value = mask[4] & 0x80 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_NUM:
		*(CS_BOOL *) value = mask[5] & 0x01 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_TEXT:
		*(CS_BOOL *) value = mask[5] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_IMAGE:
		*(CS_BOOL *) value = mask[5] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_DEC:
		*(CS_BOOL *) value = mask[5] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_LCHAR:
		*(CS_BOOL *) value = mask[5] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_LBIN:
		*(CS_BOOL *) value = mask[5] & 0x20 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_INTN:
		*(CS_BOOL *) value = mask[5] & 0x40 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_DATETIMEN:
		*(CS_BOOL *) value = mask[5] & 0x80 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_BIN:
		*(CS_BOOL *) value = mask[6] & 0x01 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_VBIN:
		*(CS_BOOL *) value = mask[6] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_MNY8:
		*(CS_BOOL *) value = mask[6] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_MNY4:
		*(CS_BOOL *) value = mask[6] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_DATE8:
		*(CS_BOOL *) value = mask[6] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_DATE4:
		*(CS_BOOL *) value = mask[6] & 0x20 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_FLT4:
		*(CS_BOOL *) value = mask[6] & 0x40 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_FLT8:
		*(CS_BOOL *) value = mask[6] & 0x80 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_MSG:
		*(CS_BOOL *) value = mask[7] & 0x01 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_PARAM:
		*(CS_BOOL *) value = mask[7] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_INT1:
		*(CS_BOOL *) value = mask[7] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_INT2:
		*(CS_BOOL *) value = mask[7] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_INT4:
		*(CS_BOOL *) value = mask[7] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_BIT:
		*(CS_BOOL *) value = mask[7] & 0x20 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_CHAR:
		*(CS_BOOL *) value = mask[7] & 0x40 ? CS_TRUE : CS_FALSE;
		break;
	case CS_DATA_VCHAR:
		*(CS_BOOL *) value = mask[7] & 0x80 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_LANG:
		*(CS_BOOL *) value = mask[8] & 0x02 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_RPC:
		*(CS_BOOL *) value = mask[8] & 0x04 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_NOTIF:
		*(CS_BOOL *) value = mask[8] & 0x08 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_MSTMT:
		*(CS_BOOL *) value = mask[8] & 0x10 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_BCP:
		*(CS_BOOL *) value = mask[8] & 0x20 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_CURSOR:
		*(CS_BOOL *) value = mask[8] & 0x40 ? CS_TRUE : CS_FALSE;
		break;
	case CS_REQ_DYN:
		*(CS_BOOL *) value = mask[8] & 0x80 ? CS_TRUE : CS_FALSE;
		break;
	default:
		return CS_FAIL;
		break;
	}			/* end capability */

	assert(*(CS_BOOL *) value);

	/* CS_CAP_RESPONSE is read-only */
	if (type == CS_CAP_RESPONSE && action == CS_GET) {
		return CS_SUCCEED;
	}

	return CS_FAIL;
}				/* end ct_capability( */

CS_RETCODE
ct_dynamic(CS_COMMAND * cmd, CS_INT type, CS_CHAR * id, CS_INT idlen, CS_CHAR * buffer, CS_INT buflen)
{
	int query_len, id_len;
	TDSDYNAMIC *dyn;
	TDSSOCKET *tds;

	/* this call resets the command, clearing any params */
	if (cmd->input_params) {
		param_clear(cmd->input_params);
		cmd->input_params = NULL;
	}
	cmd->command_type = CS_DYNAMIC_CMD;
	cmd->dynamic_cmd = type;
	switch (type) {
	case CS_PREPARE:
		/* store away the id */
		if (idlen == CS_NULLTERM) {
			id_len = strlen(id);
		} else {
			id_len = idlen;
		}
		if (cmd->dyn_id)
			free(cmd->dyn_id);
		cmd->dyn_id = (char *) malloc(id_len + 1);
		strncpy(cmd->dyn_id, (char *) id, id_len);
		cmd->dyn_id[id_len] = '\0';

		/* now the query */
		if (buflen == CS_NULLTERM) {
			query_len = strlen(buffer);
		} else {
			query_len = buflen;
		}
		if (cmd->query)
			free(cmd->query);
		cmd->query = (char *) malloc(query_len + 1);
		strncpy(cmd->query, (char *) buffer, query_len);
		cmd->query[query_len] = '\0';

		break;
	case CS_DEALLOC:
		break;
	case CS_DESCRIBE_INPUT:
		break;
	case CS_EXECUTE:
		/* store away the id */
		if (idlen == CS_NULLTERM) {
			id_len = strlen(id);
		} else {
			id_len = idlen;
		}
		if (cmd->dyn_id)
			free(cmd->dyn_id);
		cmd->dyn_id = (char *) malloc(id_len + 1);
		strncpy(cmd->dyn_id, (char *) id, id_len);
		cmd->dyn_id[id_len] = '\0';

		/* free any input parameters */
		tds = cmd->con->tds_socket;
		dyn = tds_lookup_dynamic(tds, cmd->dyn_id);
		break;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L ct_dynamic()\n");
	return CS_SUCCEED;
}

CS_RETCODE
ct_param(CS_COMMAND * cmd, CS_DATAFMT * datafmt, CS_VOID * data, CS_INT datalen, CS_SMALLINT indicator)
{
	TDSSOCKET *tds;
	TDSDYNAMIC *dyn;
	CSREMOTE_PROC *rpc;
	CS_PARAM **pparam;
	CS_PARAM *param;


	tdsdump_log(TDS_DBG_FUNC, "%L ct_param()\n");
	tdsdump_log(TDS_DBG_INFO1, "%L ct_param() data addr = %d data length = %d\n", data, datalen);

	if (cmd == NULL)
		return CS_FAIL;

	switch (cmd->command_type) {
	case CS_RPC_CMD:
		if (cmd->rpc == NULL) {
			fprintf(stdout, "RPC is NULL ct_param\n");
			return CS_FAIL;
		}

		param = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
		memset(param, 0, sizeof(CSREMOTE_PROC_PARAM));

		if (CS_SUCCEED != _ct_fill_param(param, datafmt, data, &datalen, &indicator, 1)) {
			free(param);
			return CS_FAIL;
		}

		rpc = cmd->rpc;
		pparam = &rpc->param_list;
		if (*pparam == NULL) {
			*pparam = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
		} else {
			while ((*pparam)->next != NULL) {
				pparam = &(*pparam)->next;
			}

			(*pparam)->next = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
			pparam = &(*pparam)->next;
		}
		*pparam = param;
		(*pparam)->next = NULL;
		tdsdump_log(TDS_DBG_INFO1, " ct_param() added rpc parameter %s \n", (*param).name);
		return CS_SUCCEED;
		break;

	case CS_LANG_CMD:
		/* only accept CS_INPUTVALUE as the status */
		if (CS_INPUTVALUE != datafmt->status) {
			tdsdump_log(TDS_DBG_ERROR, "illegal datafmt->status(%d) passed to ct_param()\n", datafmt->status);
			return CS_FAIL;
		}

		param = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
		memset(param, 0, sizeof(CSREMOTE_PROC_PARAM));

		if (CS_SUCCEED != _ct_fill_param(param, datafmt, data, &datalen, &indicator, 1)) {
			free(param);
			return CS_FAIL;
		}

		if (NULL == cmd->input_params)
			cmd->input_params = param;
		else {
			pparam = &cmd->input_params;
			while ((*pparam)->next)
				pparam = &(*pparam)->next;
			(*pparam)->next = param;
		}
		tdsdump_log(TDS_DBG_INFO1, "ct_param() added input value\n");
		return CS_SUCCEED;
		break;

	case CS_DYNAMIC_CMD:
		tds = cmd->con->tds_socket;

		dyn = tds_lookup_dynamic(tds, cmd->dyn_id);

		/* TODO */
		return CS_FAIL;
		break;
	}
	/* TODO */
	return CS_FAIL;
}

CS_RETCODE
ct_setparam(CS_COMMAND * cmd, CS_DATAFMT * datafmt, CS_VOID * data, CS_INT * datalen, CS_SMALLINT * indicator)
{
	CSREMOTE_PROC *rpc;
	CSREMOTE_PROC_PARAM **pparam;
	CSREMOTE_PROC_PARAM *param;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_setparam()\n");

	/* Code changed for RPC functionality - SUHA */
	/* RPC code changes starts here */

	if (cmd == NULL)
		return CS_FAIL;

	if (cmd->command_type == CS_RPC_CMD) {

		if (cmd->rpc == NULL) {
			fprintf(stdout, "RPC is NULL ct_param\n");
			return CS_FAIL;
		}

		param = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
		memset(param, 0, sizeof(CSREMOTE_PROC_PARAM));

		if (CS_SUCCEED != _ct_fill_param(param, datafmt, data, datalen, indicator, 0))
			return CS_FAIL;

		rpc = cmd->rpc;
		pparam = &rpc->param_list;
		tdsdump_log(TDS_DBG_INFO1, " ct_setparam() reached here\n", (*param).name);
		if (*pparam == NULL) {
			*pparam = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
		} else {
			while ((*pparam)->next != NULL) {
				pparam = &(*pparam)->next;
			}

			(*pparam)->next = (CSREMOTE_PROC_PARAM *) malloc(sizeof(CSREMOTE_PROC_PARAM));
			pparam = &(*pparam)->next;
		}
		*pparam = param;
		(*pparam)->next = NULL;
		tdsdump_log(TDS_DBG_INFO1, " ct_setparam() added parameter %s \n", (*param).name);
		return CS_SUCCEED;
	}

	/* TODO */
	return CS_FAIL;
}

CS_RETCODE
ct_options(CS_CONNECTION * con, CS_INT action, CS_INT option, CS_VOID * param, CS_INT paramlen, CS_INT * outlen)
{
	TDS_OPTION_CMD tds_command = 0;
	TDS_OPTION tds_option = 0;
	TDS_OPTION_ARG tds_argument;
	TDS_INT tds_argsize = 0;

	const char *action_string = NULL;
	int i;

	/* boolean options can all be treated the same way */
	static const struct TDS_BOOL_OPTION_MAP
	{
		CS_INT option;
		TDS_OPTION tds_option;
	} tds_bool_option_map[] = {
		  { CS_OPT_ANSINULL,       TDS_OPT_ANSINULL       }
		, { CS_OPT_ANSINULL,       TDS_OPT_ANSINULL       }
		, { CS_OPT_CHAINXACTS,     TDS_OPT_CHAINXACTS     }
		, { CS_OPT_CURCLOSEONXACT, TDS_OPT_CURCLOSEONXACT }
		, { CS_OPT_FIPSFLAG,       TDS_OPT_FIPSFLAG       }
		, { CS_OPT_FORCEPLAN,      TDS_OPT_FORCEPLAN      }
		, { CS_OPT_FORMATONLY,     TDS_OPT_FORMATONLY     }
		, { CS_OPT_GETDATA,        TDS_OPT_GETDATA        }
		, { CS_OPT_NOCOUNT,        TDS_OPT_NOCOUNT        }
		, { CS_OPT_NOEXEC,         TDS_OPT_NOEXEC         }
		, { CS_OPT_PARSEONLY,      TDS_OPT_PARSEONLY      }
		, { CS_OPT_QUOTED_IDENT,   TDS_OPT_QUOTED_IDENT   }
		, { CS_OPT_RESTREES,       TDS_OPT_RESTREES       }
		, { CS_OPT_SHOWPLAN,       TDS_OPT_SHOWPLAN       }
		, { CS_OPT_STATS_IO,       TDS_OPT_STAT_IO        }
		, { CS_OPT_STATS_TIME,     TDS_OPT_STAT_TIME      }
	};

	if (param == NULL)
		return CS_FAIL;

	/* 
	 * Set the tds command 
	 */
	switch (action) {
	case CS_GET:
		tds_command = TDS_OPT_LIST;	/* will be acknowledged by TDS_OPT_INFO */
		action_string = "CS_GET";
		tds_argsize = 0;
		break;
	case CS_SET:
		tds_command = TDS_OPT_SET;
		action_string = "CS_SET";
		break;
	case CS_CLEAR:
		tds_command = TDS_OPT_DEFAULT;
		action_string = "CS_CLEAR";
		tds_argsize = 0;
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "%L ct_options: invalid action = %d\n", action);
		return CS_FAIL;
	}

	assert(tds_command && action_string);

	tdsdump_log(TDS_DBG_FUNC, "%L ct_options: %s, option = %d\n", action_string, option);

	/*
	 * Set the tds option
	 *      The following TDS options apparently cannot be set with this function.  
	 *      TDS_OPT_CHARSET
	 *      TDS_OPT_CURREAD
	 *      TDS_OPT_IDENTITYOFF
	 *      TDS_OPT_IDENTITYON
	 *      TDS_OPT_CURWRITE
	 *      TDS_OPT_NATLANG
	 *      TDS_OPT_ROWCOUNT
	 *      TDS_OPT_TEXTSIZE
	 */

	/* 
	 * First, take care of the easy cases, the booleans.  
	 */
	for (i = 0; i < TDS_VECTOR_SIZE(tds_bool_option_map); i++) {
		if (tds_bool_option_map[i].option == option) {
			tds_option = tds_bool_option_map[i].tds_option;
			break;
		}
	}

	if (tds_option != 0) {	/* found a boolean */
		switch (*(CS_BOOL *) param) {
		case CS_TRUE:
			tds_argument.ti = 1;
			break;
		case CS_FALSE:
			tds_argument.ti = 0;
			break;
		default:
			return CS_FAIL;
		}
		tds_argsize = (action == CS_SET) ? 1 : 0;

		goto SEND_OPTION;
	}

	/*
	 * Non-booleans are more complicated.
	 */
	switch (option) {
	case CS_OPT_ANSIPERM:
	case CS_OPT_STR_RTRUNC:
		/* no documented tds option */
		switch (*(CS_BOOL *) param) {
		case CS_TRUE:
		case CS_FALSE:
			break;	/* end valid choices */
		default:
			return CS_FAIL;
		}
		break;
	case CS_OPT_ARITHABORT:
		switch (*(CS_BOOL *) param) {
		case CS_TRUE:
			tds_option = TDS_OPT_ARITHABORTON;
			break;
		case CS_FALSE:
			tds_option = TDS_OPT_ARITHABORTOFF;
			break;
		default:
			return CS_FAIL;
		}
		tds_argument.i = TDS_OPT_ARITHOVERFLOW | TDS_OPT_NUMERICTRUNC;
		tds_argsize = (action == CS_SET) ? 4 : 0;
		break;
	case CS_OPT_ARITHIGNORE:
		switch (*(CS_BOOL *) param) {
		case CS_TRUE:
			tds_option = TDS_OPT_ARITHIGNOREON;
			break;
		case CS_FALSE:
			tds_option = TDS_OPT_ARITHIGNOREOFF;
			break;
		default:
			return CS_FAIL;
		}
		tds_argument.i = TDS_OPT_ARITHOVERFLOW | TDS_OPT_NUMERICTRUNC;
		tds_argsize = (action == CS_SET) ? 4 : 0;
		break;
	case CS_OPT_AUTHOFF:
		tds_option = TDS_OPT_AUTHOFF;
		tds_argument.c = (TDS_CHAR *) param;
		tds_argsize = (action == CS_SET) ? paramlen : 0;
		break;
	case CS_OPT_AUTHON:
		tds_option = TDS_OPT_AUTHON;
		tds_argument.c = (TDS_CHAR *) param;
		tds_argsize = (action == CS_SET) ? paramlen : 0;
		break;

	case CS_OPT_DATEFIRST:
		tds_option = TDS_OPT_DATEFIRST;
		switch (*(char *) param) {
		case CS_OPT_SUNDAY:
			tds_argument.ti = TDS_OPT_SUNDAY;
			break;
		case CS_OPT_MONDAY:
			tds_argument.ti = TDS_OPT_MONDAY;
			break;
		case CS_OPT_TUESDAY:
			tds_argument.ti = TDS_OPT_TUESDAY;
			break;
		case CS_OPT_WEDNESDAY:
			tds_argument.ti = TDS_OPT_WEDNESDAY;
			break;
		case CS_OPT_THURSDAY:
			tds_argument.ti = TDS_OPT_THURSDAY;
			break;
		case CS_OPT_FRIDAY:
			tds_argument.ti = TDS_OPT_FRIDAY;
			break;
		case CS_OPT_SATURDAY:
			tds_argument.ti = TDS_OPT_SATURDAY;
			break;
		default:
			return CS_FAIL;
		}
		tds_argument.ti = *(char *) param;
		tds_argsize = (action == CS_SET) ? 1 : 0;
		break;
	case CS_OPT_DATEFORMAT:
		tds_option = TDS_OPT_DATEFORMAT;
		switch (*(char *) param) {
		case CS_OPT_FMTMDY:
			tds_argument.ti = TDS_OPT_FMTMDY;
			break;
		case CS_OPT_FMTDMY:
			tds_argument.ti = TDS_OPT_FMTDMY;
			break;
		case CS_OPT_FMTYMD:
			tds_argument.ti = TDS_OPT_FMTYMD;
			break;
		case CS_OPT_FMTYDM:
			tds_argument.ti = TDS_OPT_FMTYDM;
			break;
		case CS_OPT_FMTMYD:
			tds_argument.ti = TDS_OPT_FMTMYD;
			break;
		case CS_OPT_FMTDYM:
			tds_argument.ti = TDS_OPT_FMTDYM;
			break;
		default:
			return CS_FAIL;
		}
		tds_argument.ti = *(char *) param;
		tds_argsize = (action == CS_SET) ? 1 : 0;
		break;
	case CS_OPT_ISOLATION:
		tds_option = TDS_OPT_ISOLATION;
		switch (*(char *) param) {
		case CS_OPT_LEVEL0:	/* CS_OPT_LEVEL0 requires SQL Server version 11.0 or later or Adaptive Server. */
			/* no documented value */
			tds_option = 0;
			tds_argument.ti = 0;
			break;
		case CS_OPT_LEVEL1:
			tds_argument.ti = TDS_OPT_LEVEL1;
		case CS_OPT_LEVEL3:
			tds_argument.ti = TDS_OPT_LEVEL3;
			break;
		default:
			return CS_FAIL;
		}
		tds_argsize = (action == CS_SET) ? 1 : 0;
		break;
	case CS_OPT_TRUNCIGNORE:
		tds_option = TDS_OPT_TRUNCABORT;	/* note inverted sense */
		switch (*(CS_BOOL *) param) {
		case CS_TRUE:
		case CS_FALSE:
			break;
		default:
			return CS_FAIL;
		}
		tds_argument.ti = !*(char *) param;
		tds_argsize = (action == CS_SET) ? 1 : 0;
		break;
	default:
		return CS_FAIL;	/* invalid option */
	}

      SEND_OPTION:

	tdsdump_log(TDS_DBG_FUNC, "%L ct_option: UNIMPLEMENTED %d\n", option);

	tdsdump_log(TDS_DBG_FUNC, "\ttds_send_optioncmd will be option(%d) arg(%x) arglen(%d)\n", tds_option, tds_argument.i,
		    tds_argsize);

	return CS_SUCCEED;	/* return succeed for now unless inputs are wrong */
	return CS_FAIL;

}				/* end ct_options() */

CS_RETCODE
ct_poll(CS_CONTEXT * ctx, CS_CONNECTION * connection, CS_INT milliseconds, CS_CONNECTION ** compconn, CS_COMMAND ** compcmd,
	CS_INT * compid, CS_INT * compstatus)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED ct_poll()\n");
	return CS_FAIL;
}

CS_RETCODE
ct_cursor(CS_COMMAND * cmd, CS_INT type, CS_CHAR * name, CS_INT namelen, CS_CHAR * text, CS_INT tlen, CS_INT option)
{
	TDSSOCKET *tds;

	tds = cmd->con->tds_socket;
	cmd->command_type = CS_CUR_CMD;

	tdsdump_log(TDS_DBG_FUNC, "%L ct_cursor() : type = %d \n", type);

	switch (type) {
	case CS_CURSOR_DECLARE:

		tds->cursor = tds_alloc_cursor( name, namelen == CS_NULLTERM ? strlen(name) + 1 : namelen,
						text, tlen == CS_NULLTERM ? strlen(text) + 1 : tlen);
		if (tds->cursor) {

	  		tds->cursor->cursor_rows = 1;
	   		tds->cursor->options = option;
			tds->cursor->status.declare    = _CS_CURS_TYPE_REQUESTED;
			tds->cursor->status.cursor_row = _CS_CURS_TYPE_UNACTIONED;
			tds->cursor->status.open       = _CS_CURS_TYPE_UNACTIONED;
			tds->cursor->status.fetch      = _CS_CURS_TYPE_UNACTIONED;
			tds->cursor->status.close      = _CS_CURS_TYPE_UNACTIONED;
			tds->cursor->status.dealloc    = _CS_CURS_TYPE_UNACTIONED;
			return CS_SUCCEED;
		} else {
			return CS_FAIL;
		}
		break;
		
 	case CS_CURSOR_ROWS:
	
		if (tds->cursor != NULL) {

			if (tds->cursor->status.declare == _CS_CURS_TYPE_REQUESTED || 
				tds->cursor->status.declare == _CS_CURS_TYPE_SENT) {

				tds->cursor->cursor_rows = option;
				tds->cursor->status.cursor_row = _CS_CURS_TYPE_REQUESTED;
				
				return CS_SUCCEED;
			}
			else {
				tds->cursor->status.cursor_row  = _CS_CURS_TYPE_UNACTIONED;
				tdsdump_log(TDS_DBG_FUNC, "%L ct_cursor() : cursor not declared\n");
				return CS_FAIL;
			}
		}
		break;

	case CS_CURSOR_OPEN:

		if (tds->cursor != NULL) {
			if (tds->cursor->status.declare == _CS_CURS_TYPE_REQUESTED || 
				tds->cursor->status.declare == _CS_CURS_TYPE_SENT ) {
	
				tds->cursor->status.open  = _CS_CURS_TYPE_REQUESTED;
		
				return CS_SUCCEED;
			}
			else {
				tds->cursor->status.open = _CS_CURS_TYPE_UNACTIONED;
				tdsdump_log(TDS_DBG_FUNC, "%L ct_cursor() : cursor not declared\n");
				return CS_FAIL;
			}
		}
		break;

	case CS_CURSOR_CLOSE:

		tds->cursor->status.cursor_row = _CS_CURS_TYPE_UNACTIONED;
		tds->cursor->status.open       = _CS_CURS_TYPE_UNACTIONED;
		tds->cursor->status.fetch      = _CS_CURS_TYPE_UNACTIONED;
		tds->cursor->status.close      = _CS_CURS_TYPE_REQUESTED;
		if (option == CS_DEALLOC) {
		 	tds->cursor->status.dealloc   = _CS_CURS_TYPE_REQUESTED;
		}
		return CS_SUCCEED;

	case CS_CURSOR_DEALLOC:

		tds->cursor->status.dealloc   = _CS_CURS_TYPE_REQUESTED;
		return CS_SUCCEED;

	case CS_IMPLICIT_CURSOR:
		tdsdump_log(TDS_DBG_INFO1, "CS_IMPLICIT_CURSOR: Option not implemented\n");
		return CS_FAIL;
	case CS_CURSOR_OPTION:
		tdsdump_log(TDS_DBG_INFO1, "CS_CURSOR_OPTION: Option not implemented\n");
		return CS_FAIL;
	case CS_CURSOR_UPDATE:
		tdsdump_log(TDS_DBG_INFO1, "CS_CURSOR_UPDATE: Option not implemented\n");
		return CS_FAIL;
	case CS_CURSOR_DELETE:
		tdsdump_log(TDS_DBG_INFO1, "CS_CURSOR_DELETE: Option not implemented\n");
		return CS_FAIL;

	}

	return CS_FAIL;
}

static int
_ct_fetchable_results(CS_COMMAND * cmd)
{
	switch (cmd->curr_result_type) {
	case CS_COMPUTE_RESULT:
	case CS_CURSOR_RESULT:
	case CS_PARAM_RESULT:
	case CS_ROW_RESULT:
	case CS_STATUS_RESULT:
		return 1;
	}
	return 0;
}

static int
_ct_process_return_status(TDSSOCKET * tds)
{
	TDSRESULTINFO *info;
	TDSCOLINFO *curcol;

	enum
	{ num_cols = 1 };

	assert(tds);
	tds_free_all_results(tds);

	/* allocate the columns structure */
	tds->curr_resinfo = tds->res_info = tds_alloc_results(num_cols);

	if (!tds->res_info)
		return TDS_FAIL;

	info = tds->res_info;

	curcol = info->columns[0];

	tds_set_column_type(curcol, SYBINT4);

	tdsdump_log(TDS_DBG_INFO1, "%L generating return status row. type = %d(%s), varint_size %d\n",
		    curcol->column_type, tds_prtype(curcol->column_type), curcol->column_varint_size);

	tds_add_row_column_size(info, curcol);

	info->current_row = tds_alloc_row(info);

	if (!info->current_row)
		return TDS_FAIL;

	assert(0 <= curcol->column_offset && curcol->column_offset < info->row_size);

	*(TDS_INT *) (info->current_row + curcol->column_offset) = tds->ret_status;

	return TDS_SUCCEED;
}

/* Code added for RPC functionality  - SUHA */
/* RPC code changes starts here */

static const unsigned char *
paramrowalloc(TDSPARAMINFO * params, TDSCOLINFO * curcol, void *value, int size)
{
	const unsigned char *row = tds_alloc_param_row(params, curcol);

	if (!row)
		return NULL;
	memcpy(&params->current_row[curcol->column_offset], value, size);

	return row;
}

/** 
 * Allocate memory and copy the rpc information into a TDSPARAMINFO structure.
 */
static TDSPARAMINFO *
paraminfoalloc(TDSSOCKET * tds, CS_PARAM * first_param)
{
	int i;
	CS_PARAM *p;
	TDSCOLINFO *pcol;
	TDSPARAMINFO *params = NULL;

	int temp_type;
	CS_BYTE *temp_value;
	CS_INT temp_datalen;
	int param_is_null;


	/* sanity */
	if (first_param == NULL)
		return NULL;

	for (i = 0, p = first_param; p != NULL; p = p->next, i++) {
		const unsigned char *prow;

		if (!(params = tds_alloc_param_result(params))) {
			fprintf(stderr, "out of rpc memory!");
			return NULL;
		}

		/* The parameteter data has been passed by reference */
		/* i.e. using ct_setparam rather than ct_param       */

		if (p->param_by_value == 0) {

			param_is_null = 0;
			temp_datalen = 0;
			temp_value = NULL;
			temp_type = p->type;

			/* here's one way of passing a null parameter */

			if (*(p->ind) == -1) {
				temp_value = NULL;
				temp_datalen = 0;
				param_is_null = 1;
			} else {

				/* and here's another... */
				if ((*(p->datalen) == 0 || *(p->datalen) == CS_UNUSED) && p->value == NULL) {
					temp_value = NULL;
					temp_datalen = 0;
					param_is_null = 1;
				} else {

					/* datafmt.datalen is ignored for fixed length types */

					if (is_fixed_type(temp_type)) {
						temp_datalen = tds_get_size_by_type(temp_type);
					} else {
						temp_datalen = (*(p->datalen) == CS_UNUSED) ? 0 : *(p->datalen);
					}

					if (temp_datalen && p->value) {
						temp_value = p->value;
					} else {
						temp_value = NULL;
						temp_datalen = 0;
						param_is_null = 1;
					}
				}
			}

			if (param_is_null) {
				switch (temp_type) {
				case SYBINT1:
				case SYBINT2:
				case SYBINT4:
				/* TODO check if supported ?? */
				case SYBINT8:
					temp_type = SYBINTN;
					break;
				case SYBDATETIME:
				case SYBDATETIME4:
					temp_type = SYBDATETIMN;
					break;
				case SYBFLT8:
					temp_type = SYBFLTN;
					break;
				case SYBBIT:
					temp_type = SYBBITN;
					break;
				case SYBMONEY:
				case SYBMONEY4:
					temp_type = SYBMONEYN;
					break;
				default:
					break;
				}
			}
		} else {
			temp_type = p->type;
			temp_value = p->value;
			temp_datalen = *(p->datalen);
		}

		pcol = params->columns[i];

		/* meta data */
		pcol->column_namelen = 0;
		if (p->name) {
			/* TODO seem complicate ... routine or something ? */
			strncpy(pcol->column_name, p->name, sizeof(pcol->column_name));
			pcol->column_name[sizeof(pcol->column_name) - 1] = 0;
			pcol->column_namelen = strlen(pcol->column_name);
		}

		tds_set_param_type(tds, pcol, temp_type);

		if (pcol->column_varint_size) {
			if (p->maxlen < 0)
				return NULL;
			pcol->column_size = p->maxlen;
		}

		if (p->status == CS_RETURN)
			pcol->column_output = 1;
		else
			pcol->column_output = 0;

		/* actual data */
		pcol->column_cur_size = temp_datalen;
		tdsdump_log(TDS_DBG_FUNC, "%L paraminfoalloc: status = %d, maxlen %d \n", p->status, p->maxlen);
		tdsdump_log(TDS_DBG_FUNC,
			    "%L paraminfoalloc: name = %*.*s, varint size %d column_type %d column_cur_size %d column_output = %d\n",
			    pcol->column_namelen, pcol->column_namelen, pcol->column_name,
			    pcol->column_varint_size, pcol->column_type, pcol->column_cur_size, pcol->column_output);
		prow = paramrowalloc(params, pcol, temp_value, temp_datalen);
		if (!prow) {
			fprintf(stderr, "out of memory for rpc row!");
			return NULL;
		}
	}

	return params;

}

static void
rpc_clear(CSREMOTE_PROC * rpc)
{
	if (rpc == NULL)
		return;

	param_clear(rpc->param_list);

	assert(rpc->name);
	free(rpc->name);
	free(rpc);
}

/**
 * recursively erase the parameter list
 */
static void
param_clear(CS_PARAM * pparam)
{
	if (pparam == NULL)
		return;

	if (pparam->next) {
		param_clear(pparam->next);
		pparam->next = NULL;
	}

	/* free self after clearing children */

	if (pparam->name)
		free(pparam->name);
	if (pparam->param_by_value)
		free(pparam->value);
	free(pparam->datalen);
	free(pparam->ind);

	free(pparam);
}

/* RPC Code changes ends here */


static int
_ct_fill_param(CS_PARAM * param, CS_DATAFMT * datafmt, CS_VOID * data, CS_INT * datalen, CS_SMALLINT * indicator, CS_BYTE byvalue)
{
	int param_is_null = 0;

	if (datafmt->namelen == CS_NULLTERM) {
		param->name = strdup(datafmt->name);
		if (param->name == (char *) NULL)
			return CS_FAIL;
	} else if (datafmt->namelen > 0) {
		param->name = malloc(datafmt->namelen + 1);
		if (param->name == NULL)
			return CS_FAIL;
		memset(param->name, 0, datafmt->namelen + 1);
		strncpy(param->name, datafmt->name, datafmt->namelen);
	}

	param->status = datafmt->status;
	tdsdump_log(TDS_DBG_INFO1, " _ct_fill_param() status = %d \n", param->status);

	/* translate datafmt.datatype, e.g. CS_SMALLINT_TYPE */
	/* to Server type, e.g. SYBINT2                      */

	param->type = _ct_get_server_type(datafmt->datatype);

	param->maxlen = datafmt->maxlength;

	if (is_fixed_type(param->type)) {
		param->maxlen = tds_get_size_by_type(param->type);
	}

	param->param_by_value = byvalue;

	if (byvalue) {
		param->datalen = malloc(sizeof(CS_INT));
		if (param->datalen == NULL)
			return CS_FAIL;

		*(param->datalen) = *datalen;

		param->ind = malloc(sizeof(CS_INT));
		if (param->ind == NULL)
			return CS_FAIL;

		*(param->ind) = *indicator;

		/* here's one way of passing a null parameter */

		if (*indicator == -1) {
			param->value = NULL;
			*(param->datalen) = 0;
			param_is_null = 1;
		} else {

			/* and here's another... */
			if ((*datalen == 0 || *datalen == CS_UNUSED) && data == NULL) {
				param->value = NULL;
				*(param->datalen) = 0;
				param_is_null = 1;
			} else {

				/* datafmt.datalen is ignored for fixed length types */

				if (is_fixed_type(param->type)) {
					*(param->datalen) = tds_get_size_by_type(param->type);
				} else {
					*(param->datalen) = (*datalen == CS_UNUSED) ? 0 : *datalen;
				}

				if (*(param->datalen) && data) {
					param->value = malloc(*(param->datalen));
					if (param->value == NULL)
						return CS_FAIL;
					memcpy(param->value, data, *(param->datalen));
					param->param_by_value = 1;
				} else {
					param->value = NULL;
					*(param->datalen) = 0;
					param_is_null = 1;
				}
			}
		}

		if (param_is_null) {
			switch (param->type) {
			case SYBINT1:
			case SYBINT2:
			case SYBINT4:
			/* TODO check if supported ?? */
			case SYBINT8:
				param->type = SYBINTN;
				break;
			case SYBDATETIME:
			case SYBDATETIME4:
				param->type = SYBDATETIMN;
				break;
			case SYBFLT8:
				param->type = SYBFLTN;
				break;
			case SYBBIT:
				param->type = SYBBITN;
				break;
			case SYBMONEY:
			case SYBMONEY4:
				param->type = SYBMONEYN;
				break;
			default:
				break;
			}
		}
	} else {		/* not by value, i.e. by reference */
		param->datalen = datalen;
		param->ind = indicator;
		param->value = data;
	}
	return CS_SUCCEED;
}

/* Code added for ct_diag implementation */
/* Code changes start here - CT_DIAG - 02*/

CS_RETCODE
ct_diag(CS_CONNECTION * conn, CS_INT operation, CS_INT type, CS_INT idx, CS_VOID * buffer)
{
	switch (operation) {
	case CS_INIT:
		if (conn->ctx->cs_errhandletype == _CS_ERRHAND_CB) {
			/* contrary to the manual page you don't seem to */
			/* be able to turn on inline message handling    */
			/* using cs_diag, once a callback is installed!  */
			return CS_FAIL;
		}

		conn->ctx->cs_errhandletype = _CS_ERRHAND_INLINE;

		if (conn->ctx->cs_diag_msglimit_client == 0)
			conn->ctx->cs_diag_msglimit_client = CS_NO_LIMIT;

		if (conn->ctx->cs_diag_msglimit_server == 0)
			conn->ctx->cs_diag_msglimit_server = CS_NO_LIMIT;

		if (conn->ctx->cs_diag_msglimit_total == 0)
			conn->ctx->cs_diag_msglimit_total = CS_NO_LIMIT;

		conn->ctx->_clientmsg_cb = (CS_CLIENTMSG_FUNC) ct_diag_storeclientmsg;
		conn->ctx->_servermsg_cb = (CS_SERVERMSG_FUNC) ct_diag_storeservermsg;

		break;

	case CS_MSGLIMIT:
		if (conn->ctx->cs_errhandletype != _CS_ERRHAND_INLINE)
			return CS_FAIL;

		if (type == CS_CLIENTMSG_TYPE)
			conn->ctx->cs_diag_msglimit_client = *(CS_INT *) buffer;

		if (type == CS_SERVERMSG_TYPE)
			conn->ctx->cs_diag_msglimit_server = *(CS_INT *) buffer;

		if (type == CS_ALLMSG_TYPE)
			conn->ctx->cs_diag_msglimit_total = *(CS_INT *) buffer;

		break;

	case CS_CLEAR:
		if (conn->ctx->cs_errhandletype != _CS_ERRHAND_INLINE)
			return CS_FAIL;
		return (ct_diag_clearmsg(conn->ctx, type));
		break;

	case CS_GET:
		if (conn->ctx->cs_errhandletype != _CS_ERRHAND_INLINE)
			return CS_FAIL;

		if (buffer == NULL)
			return CS_FAIL;

		if (type == CS_CLIENTMSG_TYPE) {
			if (idx == 0
			    || (conn->ctx->cs_diag_msglimit_client != CS_NO_LIMIT && idx > conn->ctx->cs_diag_msglimit_client))
				return CS_FAIL;

			return (ct_diag_getclientmsg(conn->ctx, idx, (CS_CLIENTMSG *) buffer));
		}

		if (type == CS_SERVERMSG_TYPE) {
			if (idx == 0
			    || (conn->ctx->cs_diag_msglimit_server != CS_NO_LIMIT && idx > conn->ctx->cs_diag_msglimit_server))
				return CS_FAIL;
			return (ct_diag_getservermsg(conn->ctx, idx, (CS_SERVERMSG *) buffer));
		}

		break;

	case CS_STATUS:
		if (conn->ctx->cs_errhandletype != _CS_ERRHAND_INLINE)
			return CS_FAIL;
		if (buffer == NULL)
			return CS_FAIL;

		return (ct_diag_countmsg(conn->ctx, type, (CS_INT *) buffer));
		break;
	}
	return CS_SUCCEED;
}

static CS_INT
ct_diag_storeclientmsg(CS_CONTEXT * context, CS_CONNECTION * conn, CS_CLIENTMSG * message)
{
	struct cs_diag_msg_client **curptr;
	struct cs_diag_msg_svr **scurptr;

	CS_INT msg_count = 0;

	curptr = &(conn->ctx->clientstore);

	scurptr = &(conn->ctx->svrstore);

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (*curptr != (struct cs_diag_msg_client *) NULL) {
		msg_count++;
		curptr = &((*curptr)->next);
	}

	/* messages over and above the agreed limit */
	/* are simply discarded...                  */

	if (conn->ctx->cs_diag_msglimit_client != CS_NO_LIMIT && msg_count >= conn->ctx->cs_diag_msglimit_client) {
		return CS_FAIL;
	}

	/* messages over and above the agreed TOTAL limit */
	/* are simply discarded */

	if (conn->ctx->cs_diag_msglimit_total != CS_NO_LIMIT) {
		while (*scurptr != (struct cs_diag_msg_svr *) NULL) {
			msg_count++;
			scurptr = &((*scurptr)->next);
		}
		if (msg_count >= conn->ctx->cs_diag_msglimit_total) {
			return CS_FAIL;
		}
	}

	*curptr = (struct cs_diag_msg_client *) malloc(sizeof(struct cs_diag_msg_client));
	if (*curptr == (struct cs_diag_msg_client *) NULL) {
		return CS_FAIL;
	} else {
		(*curptr)->next = (struct cs_diag_msg_client *) NULL;
		(*curptr)->clientmsg = malloc(sizeof(CS_CLIENTMSG));
		if ((*curptr)->clientmsg == (CS_CLIENTMSG *) NULL) {
			return CS_FAIL;
		} else {
			memcpy((*curptr)->clientmsg, message, sizeof(CS_CLIENTMSG));
		}
	}

	return CS_SUCCEED;
}

static CS_INT
ct_diag_storeservermsg(CS_CONTEXT * context, CS_CONNECTION * conn, CS_SERVERMSG * message)
{
	struct cs_diag_msg_svr **curptr;
	struct cs_diag_msg_client **ccurptr;

	CS_INT msg_count = 0;

	curptr = &(conn->ctx->svrstore);
	ccurptr = &(conn->ctx->clientstore);

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (*curptr != (struct cs_diag_msg_svr *) NULL) {
		msg_count++;
		curptr = &((*curptr)->next);
	}

	/* messages over and above the agreed limit */
	/* are simply discarded...                  */

	if (conn->ctx->cs_diag_msglimit_server != CS_NO_LIMIT && msg_count >= conn->ctx->cs_diag_msglimit_server) {
		return CS_FAIL;
	}

	/* messages over and above the agreed TOTAL limit */
	/* are simply discarded...                  */

	if (conn->ctx->cs_diag_msglimit_total != CS_NO_LIMIT) {
		while (*ccurptr != (struct cs_diag_msg_client *) NULL) {
			msg_count++;
			ccurptr = &((*ccurptr)->next);
		}
		if (msg_count >= conn->ctx->cs_diag_msglimit_total) {
			return CS_FAIL;
		}
	}

	*curptr = (struct cs_diag_msg_svr *) malloc(sizeof(struct cs_diag_msg_svr));
	if (*curptr == (struct cs_diag_msg_svr *) NULL) {
		return CS_FAIL;
	} else {
		(*curptr)->next = (struct cs_diag_msg_svr *) NULL;
		(*curptr)->servermsg = malloc(sizeof(CS_SERVERMSG));
		if ((*curptr)->servermsg == (CS_SERVERMSG *) NULL) {
			return CS_FAIL;
		} else {
			memcpy((*curptr)->servermsg, message, sizeof(CS_SERVERMSG));
		}
	}

	return CS_SUCCEED;
}

static CS_INT
ct_diag_getclientmsg(CS_CONTEXT * context, CS_INT idx, CS_CLIENTMSG * message)
{
	struct cs_diag_msg_client *curptr;
	CS_INT msg_count = 0, msg_found = 0;

	curptr = context->clientstore;

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (curptr != (struct cs_diag_msg_client *) NULL) {
		msg_count++;
		if (msg_count == idx) {
			msg_found++;
			break;
		}
		curptr = curptr->next;
	}

	if (msg_found) {
		memcpy(message, curptr->clientmsg, sizeof(CS_CLIENTMSG));
		return CS_SUCCEED;
	}
	return CS_NOMSG;
}

static CS_INT
ct_diag_getservermsg(CS_CONTEXT * context, CS_INT idx, CS_SERVERMSG * message)
{
	struct cs_diag_msg_svr *curptr;
	CS_INT msg_count = 0, msg_found = 0;

	curptr = context->svrstore;

	/* if we already have a list of messages, */
	/* go to the end of the list...           */

	while (curptr != (struct cs_diag_msg_svr *) NULL) {
		msg_count++;
		if (msg_count == idx) {
			msg_found++;
			break;
		}
		curptr = curptr->next;
	}

	if (msg_found) {
		memcpy(message, curptr->servermsg, sizeof(CS_SERVERMSG));
		return CS_SUCCEED;
	} else {
		return CS_NOMSG;
	}
}

static CS_INT
ct_diag_clearmsg(CS_CONTEXT * context, CS_INT type)
{
	struct cs_diag_msg_client *curptr, *freeptr;
	struct cs_diag_msg_svr *scurptr, *sfreeptr;

	if (type == CS_CLIENTMSG_TYPE || type == CS_ALLMSG_TYPE) {
		curptr = context->clientstore;
		context->clientstore = NULL;

		while (curptr != (struct cs_diag_msg_client *) NULL) {
			freeptr = curptr;
			curptr = freeptr->next;
			if (freeptr->clientmsg)
				free(freeptr->clientmsg);
			free(freeptr);
		}
	}

	if (type == CS_SERVERMSG_TYPE || type == CS_ALLMSG_TYPE) {
		scurptr = context->svrstore;
		context->svrstore = NULL;

		while (scurptr != (struct cs_diag_msg_svr *) NULL) {
			sfreeptr = scurptr;
			scurptr = sfreeptr->next;
			if (sfreeptr->servermsg)
				free(sfreeptr->servermsg);
			free(sfreeptr);
		}
	}
	return CS_SUCCEED;
}

static CS_INT
ct_diag_countmsg(CS_CONTEXT * context, CS_INT type, CS_INT * count)
{
	struct cs_diag_msg_client *curptr;
	struct cs_diag_msg_svr *scurptr;

	CS_INT msg_count = 0;

	if (type == CS_CLIENTMSG_TYPE || type == CS_ALLMSG_TYPE) {
		curptr = context->clientstore;

		while (curptr != (struct cs_diag_msg_client *) NULL) {
			msg_count++;
			curptr = curptr->next;
		}
	}

	if (type == CS_SERVERMSG_TYPE || type == CS_ALLMSG_TYPE) {
		scurptr = context->svrstore;

		while (scurptr != (struct cs_diag_msg_svr *) NULL) {
			msg_count++;
			scurptr = scurptr->next;
		}
	}
	*count = msg_count;

	return CS_SUCCEED;
}

/* Code changes ends here - CT_DIAG - 02*/
