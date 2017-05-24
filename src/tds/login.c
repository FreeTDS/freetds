/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005-2015  Ziglio Frediano
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

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#ifdef _WIN32
#include <process.h>
#endif

#include <freetds/tds.h>
#include <freetds/iconv.h>
#include <freetds/string.h>
#include <freetds/bytes.h>
#include <freetds/tls.h>
#include <freetds/stream.h>
#include <freetds/checks.h>
#include "replacements.h"

static TDSRET tds_send_login(TDSSOCKET * tds, TDSLOGIN * login);
static TDSRET tds71_do_login(TDSSOCKET * tds, TDSLOGIN * login);
static TDSRET tds7_send_login(TDSSOCKET * tds, TDSLOGIN * login);
static void tds7_crypt_pass(const unsigned char *clear_pass,
			    size_t len, unsigned char *crypt_pass);

void
tds_set_version(TDSLOGIN * tds_login, TDS_TINYINT major_ver, TDS_TINYINT minor_ver)
{
	tds_login->tds_version = ((TDS_USMALLINT) major_ver << 8) + minor_ver;
}

void
tds_set_packet(TDSLOGIN * tds_login, int packet_size)
{
	tds_login->block_size = packet_size;
}

void
tds_set_port(TDSLOGIN * tds_login, int port)
{
	tds_login->port = port;
}

bool
tds_set_passwd(TDSLOGIN * tds_login, const char *password)
{
	if (password) {
		tds_dstr_zero(&tds_login->password);
		return !!tds_dstr_copy(&tds_login->password, password);
	}
	return true;
}
void
tds_set_bulk(TDSLOGIN * tds_login, TDS_TINYINT enabled)
{
	tds_login->bulk_copy = enabled ? 1 : 0;
}

bool
tds_set_user(TDSLOGIN * tds_login, const char *username)
{
	return !!tds_dstr_copy(&tds_login->user_name, username);
}

bool
tds_set_host(TDSLOGIN * tds_login, const char *hostname)
{
	return !!tds_dstr_copy(&tds_login->client_host_name, hostname);
}

bool
tds_set_app(TDSLOGIN * tds_login, const char *application)
{
	return !!tds_dstr_copy(&tds_login->app_name, application);
}

/**
 * \brief Set the servername in a TDSLOGIN structure
 *
 * Normally copies \a server into \a tds_login.  If \a server does not point to a plausible name, the environment 
 * variables TDSQUERY and DSQUERY are used, in that order.  If they don't exist, the "default default" servername
 * is "SYBASE" (although the utility of that choice is a bit murky).  
 *
 * \param tds_login	points to a TDSLOGIN structure
 * \param server	the servername, or NULL, or a zero-length string
 * \todo open the log file earlier, so these messages can be seen.  
 */
bool
tds_set_server(TDSLOGIN * tds_login, const char *server)
{
#if 0
	/* Doing this in tds_alloc_login instead */
	static const char *names[] = { "TDSQUERY", "DSQUERY", "SYBASE" };
	int i;
	
	for (i=0; i < TDS_VECTOR_SIZE(names) && (!server || strlen(server) == 0); i++) {
		const char *source;
		if (i + 1 == TDS_VECTOR_SIZE(names)) {
			server = names[i];
			source = "compiled-in default";
		} else {
			server = getenv(names[i]);
			source = names[i];
		}
		if (server) {
			tdsdump_log(TDS_DBG_INFO1, "Setting TDSLOGIN::server_name to '%s' from %s.\n", server, source);
		}
	}
#endif
	if (server)
		return !!tds_dstr_copy(&tds_login->server_name, server);
	return true;
}

bool
tds_set_library(TDSLOGIN * tds_login, const char *library)
{
	return !!tds_dstr_copy(&tds_login->library, library);
}

bool
tds_set_client_charset(TDSLOGIN * tds_login, const char *charset)
{
	return !!tds_dstr_copy(&tds_login->client_charset, charset);
}

bool
tds_set_language(TDSLOGIN * tds_login, const char *language)
{
	return !!tds_dstr_copy(&tds_login->language, language);
}

struct tds_save_msg
{
	TDSMESSAGE msg;
	char type;
};

struct tds_save_env
{
	char *oldval;
	char *newval;
	int type;
};

typedef struct tds_save_context
{
	/* must be first !!! */
	TDSCONTEXT ctx;

	unsigned num_msg;
	struct tds_save_msg msgs[10];

	unsigned num_env;
	struct tds_save_env envs[10];
} TDSSAVECONTEXT;

static void
tds_save(TDSSAVECONTEXT *ctx, char type, TDSMESSAGE *msg)
{
	struct tds_save_msg *dest_msg;

	if (ctx->num_msg >= TDS_VECTOR_SIZE(ctx->msgs))
		return;

	dest_msg = &ctx->msgs[ctx->num_msg];
	dest_msg->type = type;
	dest_msg->msg = *msg;
#define COPY(name) if (msg->name) dest_msg->msg.name = strdup(msg->name);
	COPY(server);
	COPY(message);
	COPY(proc_name);
	COPY(sql_state);
#undef COPY
	++ctx->num_msg;
}

static int
tds_save_msg(const TDSCONTEXT *ctx, TDSSOCKET *tds, TDSMESSAGE *msg)
{
	tds_save((TDSSAVECONTEXT *) ctx, 0, msg);
	return 0;
}

static int
tds_save_err(const TDSCONTEXT *ctx, TDSSOCKET *tds, TDSMESSAGE *msg)
{
	tds_save((TDSSAVECONTEXT *) ctx, 1, msg);
	return TDS_INT_CANCEL;
}

static void
tds_save_env(TDSSOCKET * tds, int type, char *oldval, char *newval)
{
	TDSSAVECONTEXT *ctx;
	struct tds_save_env *env;

	if (tds_get_ctx(tds)->msg_handler != tds_save_msg)
		return;

	ctx = (TDSSAVECONTEXT *) tds_get_ctx(tds);
	if (ctx->num_env >= TDS_VECTOR_SIZE(ctx->envs))
		return;

	env = &ctx->envs[ctx->num_env];
	env->type = type;
	env->oldval = oldval ? strdup(oldval) : NULL;
	env->newval = newval ? strdup(newval) : NULL;
	++ctx->num_env;
}

static void
init_save_context(TDSSAVECONTEXT *ctx, const TDSCONTEXT *old_ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->ctx.locale = old_ctx->locale;
	ctx->ctx.msg_handler = tds_save_msg;
	ctx->ctx.err_handler = tds_save_err;
}

static void
replay_save_context(TDSSOCKET *tds, TDSSAVECONTEXT *ctx)
{
	unsigned n;

	/* replay all recorded messages */
	for (n = 0; n < ctx->num_msg; ++n)
		if (ctx->msgs[n].type == 0) {
			if (tds_get_ctx(tds)->msg_handler)
				tds_get_ctx(tds)->msg_handler(tds_get_ctx(tds), tds, &ctx->msgs[n].msg);
		} else {
			if (tds_get_ctx(tds)->err_handler)
				tds_get_ctx(tds)->err_handler(tds_get_ctx(tds), tds, &ctx->msgs[n].msg);
		}

	/* replay all recorded envs */
	for (n = 0; n < ctx->num_env; ++n)
		if (tds->env_chg_func)
			tds->env_chg_func(tds, ctx->envs[n].type, ctx->envs[n].oldval, ctx->envs[n].newval);
}

static void
reset_save_context(TDSSAVECONTEXT *ctx)
{
	unsigned n;

	/* free all messages */
	for (n = 0; n < ctx->num_msg; ++n)
		tds_free_msg(&ctx->msgs[n].msg);
	ctx->num_msg = 0;

	/* free all envs */
	for (n = 0; n < ctx->num_env; ++n) {
		free(ctx->envs[n].oldval);
		free(ctx->envs[n].newval);
	}
	ctx->num_env = 0;
}

static void
free_save_context(TDSSAVECONTEXT *ctx)
{
	reset_save_context(ctx);
}

/**
 * Retrieve and set @@spid
 * \tds
 */
static TDSRET
tds_set_spid(TDSSOCKET * tds)
{
	TDS_INT result_type;
	TDS_INT done_flags;
	TDSRET rc;
	TDSCOLUMN *curcol;

	CHECK_TDS_EXTRA(tds);

	while ((rc = tds_process_tokens(tds, &result_type, &done_flags, TDS_RETURN_ROW|TDS_RETURN_DONE)) == TDS_SUCCESS) {

		switch (result_type) {
		case TDS_ROW_RESULT:
			if (!tds->res_info)
				return TDS_FAIL;
			if (tds->res_info->num_cols != 1)
				break;
			curcol = tds->res_info->columns[0];
			switch (tds_get_conversion_type(curcol->column_type, curcol->column_size)) {
			case SYBINT2:
				tds->conn->spid = *((TDS_USMALLINT *) curcol->column_data);
				break;
			case SYBINT4:
				tds->conn->spid = *((TDS_UINT *) curcol->column_data);
				break;
			default:
				return TDS_FAIL;
			}
			break;

		case TDS_DONE_RESULT:
			if ((done_flags & TDS_DONE_ERROR) != 0)
				return TDS_FAIL;
			break;
		}
	}
	if (rc == TDS_NO_MORE_RESULTS)
		rc = TDS_SUCCESS;

	return rc;
}

/**
 * Do a connection to socket
 * @param tds connection structure. This should be a non-connected connection.
 * @return TDS_FAIL or TDS_SUCCESS if a connection was made to the server's port.
 * @return TDSERROR enumerated type if no TCP/IP connection could be formed. 
 * @param login info for login
 * @remark Possible error conditions:
 *		- TDSESOCK: socket(2) failed: insufficient local resources
 * 		- TDSECONN: connect(2) failed: invalid hostname or port (ETIMEDOUT, ECONNREFUSED, ENETUNREACH)
 * 		- TDSEFCON: connect(2) succeeded, login packet not acknowledged.  
 *		- TDS_FAIL: connect(2) succeeded, login failed.  
 */
static int
tds_connect(TDSSOCKET * tds, TDSLOGIN * login, int *p_oserr)
{
	int erc = -TDSEFCON;
	int connect_timeout = 0;
	int db_selected = 0;
	struct addrinfo *addrs;
	int orig_port;

	/*
	 * A major version of 0 means try to guess the TDS version. 
	 * We try them in an order that should work. 
	 */
	const static TDS_USMALLINT versions[] =
		{ 0x703
		, 0x702
		, 0x701
		, 0x700
		, 0x500
		, 0x402
		};

	if (!login->valid_configuration) {
		tdserror(tds_get_ctx(tds), tds, TDSECONF, 0);
		return TDS_FAIL;
	}

	if (TDS_MAJOR(login) == 0) {
		unsigned int i;
		TDSSAVECONTEXT save_ctx;
		const TDSCONTEXT *old_ctx = tds_get_ctx(tds);
		typedef void (*env_chg_func_t) (TDSSOCKET * tds, int type, char *oldval, char *newval);
		env_chg_func_t old_env_chg = tds->env_chg_func;
		/* the context of a socket is const; we have to modify it to suppress error messages during multiple tries. */
		TDSCONTEXT *mod_ctx = (TDSCONTEXT *) tds_get_ctx(tds);
		err_handler_t err_handler = tds_get_ctx(tds)->err_handler;

		init_save_context(&save_ctx, old_ctx);
		tds_set_ctx(tds, &save_ctx.ctx);
		tds->env_chg_func = tds_save_env;
		mod_ctx->err_handler = NULL;

		for (i = 0; i < TDS_VECTOR_SIZE(versions); ++i) {
			login->tds_version = versions[i];
			reset_save_context(&save_ctx);

			erc = tds_connect(tds, login, p_oserr);
			if (TDS_FAILED(erc)) {
				tds_close_socket(tds);
			}
			
			if (erc != -TDSEFCON)	/* TDSEFCON indicates wrong TDS version */
				break;
		}
		
		mod_ctx->err_handler = err_handler;
		tds->env_chg_func = old_env_chg;
		tds_set_ctx(tds, old_ctx);
		replay_save_context(tds, &save_ctx);
		free_save_context(&save_ctx);
		
		if (TDS_FAILED(erc))
			tdserror(tds_get_ctx(tds), tds, -erc, *p_oserr);

		return erc;
	}
	

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&login->dump_file) && !tdsdump_isopen()) {
		if (login->debug_flags)
			tds_debug_flags = login->debug_flags;
		tdsdump_open(tds_dstr_cstr(&login->dump_file));
	}

	tds->login = login;

	tds->conn->tds_version = login->tds_version;
	tds->conn->emul_little_endian = login->emul_little_endian;
#ifdef WORDS_BIGENDIAN
	/*
	 * Enable automatically little endian emulation.
	 * TDS 7/8 only supports little endian.
	 * This is done even for 4.2 to ensure that when we connect to a
	 * MSSQL we use it and avoid to make additional checks for
	 * broken 7.0 servers.
	 * Note that 4.2 should not be used anymore for real jobs.
	 */
	if (IS_TDS7_PLUS(tds->conn) || IS_TDS42(tds->conn))
		tds->conn->emul_little_endian = 1;
#endif

	/* set up iconv if not already initialized*/
	if (tds->conn->char_convs[client2ucs2]->to.cd == (iconv_t) -1) {
		if (!tds_dstr_isempty(&login->client_charset)) {
			if (TDS_FAILED(tds_iconv_open(tds->conn, tds_dstr_cstr(&login->client_charset), login->use_utf16)))
				return -TDSEMEM;
		}
	}

	connect_timeout = login->connect_timeout;

	/* Jeff's hack - begin */
	tds->query_timeout = connect_timeout ? connect_timeout : login->query_timeout;
	/* end */

	/* verify that ip_addr is not empty */
	if (login->ip_addrs == NULL) {
		tdserror(tds_get_ctx(tds), tds, TDSEUHST, 0 );
		tdsdump_log(TDS_DBG_ERROR, "IP address pointer is empty\n");
		if (!tds_dstr_isempty(&login->server_name)) {
			tdsdump_log(TDS_DBG_ERROR, "Server %s not found!\n", tds_dstr_cstr(&login->server_name));
		} else {
			tdsdump_log(TDS_DBG_ERROR, "No server specified!\n");
		}
		return -TDSECONN;
	}

	tds->conn->capabilities = login->capabilities;

	erc = TDSEINTF;
	orig_port = login->port;
	for (addrs = login->ip_addrs; addrs != NULL; addrs = addrs->ai_next) {
		login->port = orig_port;

		if (!IS_TDS50(tds->conn) && !tds_dstr_isempty(&login->instance_name) && !login->port)
			login->port = tds7_get_instance_port(addrs, tds_dstr_cstr(&login->instance_name));

		if (login->port >= 1) {
			if ((erc = tds_open_socket(tds, addrs, login->port, connect_timeout, p_oserr)) == TDSEOK) {
				login->connected_addr = addrs;
				break;
			}
		} else {
			erc = TDSECONN;
		}
	}

	if (erc != TDSEOK) {
		if (login->port < 1)
			tdsdump_log(TDS_DBG_ERROR, "invalid port number\n");

		tdserror(tds_get_ctx(tds), tds, erc, *p_oserr);
		return -erc;
	}
		
	/*
	 * Beyond this point, we're connected to the server.  We know we have a valid TCP/IP address+socket pair.  
	 * Although network errors *might* happen, most problems from here on out will be TDS-level errors, 
	 * either TDS version problems or authentication problems.  
	 */
		
	tds_set_state(tds, TDS_IDLE);
	tds->conn->spid = -1;

	/* discard possible previous authentication */
	if (tds->conn->authentication) {
		tds->conn->authentication->free(tds->conn, tds->conn->authentication);
		tds->conn->authentication = NULL;
	}

	if (IS_TDS71_PLUS(tds->conn)) {
		erc = tds71_do_login(tds, login);
		db_selected = 1;
	} else if (IS_TDS7_PLUS(tds->conn)) {
		erc = tds7_send_login(tds, login);
		db_selected = 1;
	} else {
		tds->out_flag = TDS_LOGIN;
		erc = tds_send_login(tds, login);
	}
	if (TDS_FAILED(erc) || TDS_FAILED(tds_process_login_tokens(tds))) {
		tdsdump_log(TDS_DBG_ERROR, "login packet %s\n", TDS_SUCCEED(erc)? "accepted":"rejected");
		tds_close_socket(tds);
		tdserror(tds_get_ctx(tds), tds, TDSEFCON, 0); 	/* "Adaptive Server connection failed" */
		return -TDSEFCON;
	}

#if ENABLE_ODBC_MARS
	/* initialize SID */
	if (IS_TDS72_PLUS(tds->conn) && login->mars) {
		tds->conn->sessions[0] = NULL;
		tds->conn->mars = 1;
		tds->sid = -1;
		tds_init_write_buf(tds);
	}
#endif

	if (login->text_size || (!db_selected && !tds_dstr_isempty(&login->database))
	    || tds->conn->spid == -1) {
		char *str;
		int len;

		len = 128 + tds_quote_id(tds, NULL, tds_dstr_cstr(&login->database),-1);
		if ((str = tds_new(char, len)) == NULL)
			return TDS_FAIL;

		str[0] = 0;
		if (login->text_size) {
			sprintf(str, "set textsize %d ", login->text_size);
		}
		if (tds->conn->spid == -1) {
			strcat(str, "select @@spid ");
		}
		/* Select proper database if specified.
		 * SQL Anywhere does not support multiple databases and USE statement
		 * so don't send the request to avoid connection failures */
		if (!db_selected && !tds_dstr_isempty(&login->database) &&
		    (tds->conn->product_name == NULL || strcasecmp(tds->conn->product_name, "SQL Anywhere") != 0)) {
			strcat(str, "use ");
			tds_quote_id(tds, strchr(str, 0), tds_dstr_cstr(&login->database), -1);
		}
		erc = tds_submit_query(tds, str);
		free(str);
		if (TDS_FAILED(erc))
			return erc;

		if (tds->conn->spid == -1)
			erc = tds_set_spid(tds);
		else
			erc = tds_process_simple_query(tds);
		if (TDS_FAILED(erc))
			return erc;
	}

	tds->query_timeout = login->query_timeout;
	tds->login = NULL;
	return TDS_SUCCESS;
}

int
tds_connect_and_login(TDSSOCKET * tds, TDSLOGIN * login)
{
	int oserr = 0;
	return tds_connect(tds, login, &oserr);
}

static int
tds_put_login_string(TDSSOCKET * tds, const char *buf, int n)
{
	const int buf_len = buf ? (int)strlen(buf) : 0;
	return tds_put_buf(tds, (const unsigned char *) buf, n, buf_len);
}

static TDSRET
tds_send_login(TDSSOCKET * tds, TDSLOGIN * login)
{
#ifdef WORDS_BIGENDIAN
	static const unsigned char be1[] = { 0x02, 0x00, 0x06, 0x04, 0x08, 0x01 };
	static const unsigned char be2[] = { 0x00, 12, 16 };
#endif
	static const unsigned char le1[] = { 0x03, 0x01, 0x06, 0x0a, 0x09, 0x01 };
	static const unsigned char le2[] = { 0x00, 13, 17 };

	/*
	 * capabilities are now part of the tds structure.
	 * unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
	 */
	/*
	 * This is the original capabilities packet we were working with (sqsh)
	 * unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x0A,104,0x00,0x00,0x00};
	 * original with 4.x messages
	 * unsigned char capabilities[]= {0x01,0x07,0x03,109,127,0xFF,0xFF,0xFF,0xFE,0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
	 * This is isql 11.0.3
	 * unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x0D};
	 * like isql but with 5.0 messages
	 * unsigned char capabilities[]= {0x01,0x07,0x00,96, 129,207, 0xFF,0xFE,62,  0x02,0x07,0x00,0x00,0x00,120,192,0x00,0x00};
	 */

	unsigned char protocol_version[4];
	unsigned char program_version[4];

	int len;
	char blockstr[16];

	/* override lservname field for ASA servers */	
	const char *lservname = getenv("ASA_DATABASE")? getenv("ASA_DATABASE") : tds_dstr_cstr(&login->server_name);

	if (strchr(tds_dstr_cstr(&login->user_name), '\\') != NULL) {
		tdsdump_log(TDS_DBG_ERROR, "NT login not support using TDS 4.x or 5.0\n");
		return TDS_FAIL;
	}
	if (tds_dstr_isempty(&login->user_name)) {
		tdsdump_log(TDS_DBG_ERROR, "Kerberos login not support using TDS 4.x or 5.0\n");
		return TDS_FAIL;
	}
	if (login->encryption_level != TDS_ENCRYPTION_OFF) {
		if (IS_TDS42(tds->conn)) {
			tdsdump_log(TDS_DBG_ERROR, "Encryption not support using TDS 4.x\n");
			return TDS_FAIL;
		}
		tds->conn->authentication = tds5_negotiate_get_auth(tds);
		if (!tds->conn->authentication)
			return TDS_FAIL;
	}

	if (IS_TDS42(tds->conn)) {
		memcpy(protocol_version, "\004\002\000\000", 4);
		memcpy(program_version, "\004\002\000\000", 4);
	} else if (IS_TDS46(tds->conn)) {
		memcpy(protocol_version, "\004\006\000\000", 4);
		memcpy(program_version, "\004\002\000\000", 4);
	} else if (IS_TDS50(tds->conn)) {
		memcpy(protocol_version, "\005\000\000\000", 4);
		memcpy(program_version, "\005\000\000\000", 4);
	} else {
		tdsdump_log(TDS_DBG_SEVERE, "Unknown protocol version!\n");
		return TDS_FAIL;
	}
	/*
	 * the following code is adapted from  Arno Pedusaar's 
	 * (psaar@fenar.ee) MS-SQL Client. His was a much better way to
	 * do this, (well...mine was a kludge actually) so here's mostly his
	 */

	tds_put_login_string(tds, tds_dstr_cstr(&login->client_host_name), TDS_MAXNAME);	/* client host name */
	tds_put_login_string(tds, tds_dstr_cstr(&login->user_name), TDS_MAXNAME);	/* account name */
	/* account password */
	if (login->encryption_level != TDS_ENCRYPTION_OFF) {
		tds_put_login_string(tds, NULL, TDS_MAXNAME);
	} else {
		tds_put_login_string(tds, tds_dstr_cstr(&login->password), TDS_MAXNAME);
	}
	sprintf(blockstr, "%d", (int) getpid());
	tds_put_login_string(tds, blockstr, TDS_MAXNAME);	/* host process */
#ifdef WORDS_BIGENDIAN
	if (tds->conn->emul_little_endian) {
		tds_put_n(tds, le1, 6);
	} else {
		tds_put_n(tds, be1, 6);
	}
#else
	tds_put_n(tds, le1, 6);
#endif
	tds_put_byte(tds, !login->bulk_copy);
	tds_put_n(tds, NULL, 2);
	if (IS_TDS42(tds->conn)) {
		tds_put_int(tds, 512);
	} else {
		tds_put_int(tds, 0);
	}
	tds_put_n(tds, NULL, 3);
	tds_put_login_string(tds, tds_dstr_cstr(&login->app_name), TDS_MAXNAME);
	tds_put_login_string(tds, lservname, TDS_MAXNAME);
	if (IS_TDS42(tds->conn)) {
		tds_put_login_string(tds, tds_dstr_cstr(&login->password), 255);
	} else if (login->encryption_level) {
		tds_put_n(tds, NULL, 256);
	} else {
		len = (int)tds_dstr_len(&login->password);
		if (len > 253)
			len = 0;
		tds_put_byte(tds, 0);
		tds_put_byte(tds, len);
		tds_put_n(tds, tds_dstr_cstr(&login->password), len);
		tds_put_n(tds, NULL, 253 - len);
		tds_put_byte(tds, len + 2);
	}

	tds_put_n(tds, protocol_version, 4);	/* TDS version; { 0x04,0x02,0x00,0x00 } */
	tds_put_login_string(tds, tds_dstr_cstr(&login->library), TDS_PROGNLEN);	/* client program name */
	if (IS_TDS42(tds->conn)) {
		tds_put_int(tds, 0);
	} else {
		tds_put_n(tds, program_version, 4);	/* program version ? */
	}
#ifdef WORDS_BIGENDIAN
	if (tds->conn->emul_little_endian) {
		tds_put_n(tds, le2, 3);
	} else {
		tds_put_n(tds, be2, 3);
	}
#else
	tds_put_n(tds, le2, 3);
#endif
	tds_put_login_string(tds, tds_dstr_cstr(&login->language), TDS_MAXNAME);	/* language */
	tds_put_byte(tds, login->suppress_language);

	/* oldsecure(2), should be zero, used by old software */
	tds_put_n(tds, NULL, 2);
	/* seclogin(1) bitmask */
	tds_put_byte(tds, login->encryption_level ? TDS5_SEC_LOG_ENCRYPT2|TDS5_SEC_LOG_NONCE : 0);
	/* secbulk(1)
	 * halogin(1) type of ha login
	 * hasessionid(6) id of session to reconnect
	 * secspare(2) not used
	 */
	tds_put_n(tds, NULL, 10);

	/* use empty charset to handle conversions on client */
	tds_put_login_string(tds, "", TDS_MAXNAME);	/* charset */
	/* this is a flag, mean that server should use character set provided by client */
	/* TODO notify charset change ?? what's correct meaning ?? -- freddy77 */
	tds_put_byte(tds, 1);

	/* network packet size */
	if (login->block_size < 65536u && login->block_size >= 512)
		sprintf(blockstr, "%d", login->block_size);
	else
		strcpy(blockstr, "512");
	tds_put_login_string(tds, blockstr, TDS_PKTLEN);

	if (IS_TDS42(tds->conn)) {
		tds_put_n(tds, NULL, 8);
	} else if (IS_TDS46(tds->conn)) {
		tds_put_n(tds, NULL, 4);
	} else if (IS_TDS50(tds->conn)) {
		/* just padding to 8 bytes */
		tds_put_n(tds, NULL, 4);
		tds_put_byte(tds, TDS_CAPABILITY_TOKEN);
		tds_put_smallint(tds, sizeof(tds->conn->capabilities));
		tds_put_n(tds, &tds->conn->capabilities, sizeof(tds->conn->capabilities));
	}

	return tds_flush_packet(tds);
}

/**
 * tds7_send_login() -- Send a TDS 7.0 login packet
 * TDS 7.0 login packet is vastly different and so gets its own function
 * \returns the return value is ignored by the caller. :-/
 */
static TDSRET
tds7_send_login(TDSSOCKET * tds, TDSLOGIN * login)
{
	static const unsigned char 
		client_progver[] = {   6, 0x83, 0xf2, 0xf8 }, 

		connection_id[] = { 0x00, 0x00, 0x00, 0x00 }, 
		collation[] = { 0x36, 0x04, 0x00, 0x00 };

	enum {
		tds70Version = 0x70000000,
		tds71Version = 0x71000001,
		tds72Version = 0x72090002,
		tds73Version = 0x730B0003,
		tds74Version = 0x74000004,
	};
	TDS_UCHAR sql_type_flag = 0x00;
	TDS_INT time_zone = -120;
	TDS_INT tds7version = tds70Version;

	TDS_INT block_size = 4096;
	
	unsigned char option_flag1 = TDS_SET_LANG_ON | TDS_USE_DB_NOTIFY | TDS_INIT_DB_FATAL;
	unsigned char option_flag2 = login->option_flag2;
	unsigned char option_flag3 = 0;

	unsigned char hwaddr[6];
	size_t packet_size, current_pos;
	TDSRET rc;

	void *data = NULL;
	TDSDYNAMICSTREAM data_stream;
	TDSSTATICINSTREAM input;

	const char *user_name = tds_dstr_cstr(&login->user_name);
	unsigned char *pwd;

	/* FIXME: These are defined as size_t, but should be TDS_SMALLINT. */
	size_t user_name_len = strlen(user_name);
	size_t auth_len = 0;

	/* fields */
	enum {
		HOST_NAME,
		USER_NAME,
		PASSWORD,
		APP_NAME,
		SERVER_NAME,
		LIBRARY_NAME,
		LANGUAGE,
		DATABASE_NAME,
		DB_FILENAME,
		NEW_PASSWORD,
		NUM_DATA_FIELDS
	};
	struct {
		const void *ptr;
		unsigned pos, len;
	} data_fields[NUM_DATA_FIELDS], *field;

	tds->out_flag = TDS7_LOGIN;

	current_pos = packet_size = IS_TDS72_PLUS(tds->conn) ? 86 + 8 : 86;	/* ? */

	/* check ntlm */
#ifdef HAVE_SSPI
	if (strchr(user_name, '\\') != NULL || user_name_len == 0) {
		tdsdump_log(TDS_DBG_INFO2, "using SSPI authentication for '%s' account\n", user_name);
		tds->conn->authentication = tds_sspi_get_auth(tds);
		if (!tds->conn->authentication)
			return TDS_FAIL;
		auth_len = tds->conn->authentication->packet_len;
		packet_size += auth_len;
#else
	if (strchr(user_name, '\\') != NULL) {
		tdsdump_log(TDS_DBG_INFO2, "using NTLM authentication for '%s' account\n", user_name);
		tds->conn->authentication = tds_ntlm_get_auth(tds);
		if (!tds->conn->authentication)
			return TDS_FAIL;
		auth_len = tds->conn->authentication->packet_len;
		packet_size += auth_len;
	} else if (user_name_len == 0) {
# ifdef ENABLE_KRB5
		/* try kerberos */
		tdsdump_log(TDS_DBG_INFO2, "using GSS authentication\n");
		tds->conn->authentication = tds_gss_get_auth(tds);
		if (!tds->conn->authentication)
			return TDS_FAIL;
		auth_len = tds->conn->authentication->packet_len;
		packet_size += auth_len;
# else
		tdsdump_log(TDS_DBG_ERROR, "requested GSS authentication but not compiled in\n");
		return TDS_FAIL;
# endif
#endif
	}


	/* initialize ouput buffer for strings */
	rc = tds_dynamic_stream_init(&data_stream, &data, 0);
	if (TDS_FAILED(rc))
		return rc;

#define SET_FIELD_DSTR(field, dstr) do { \
	data_fields[field].ptr = tds_dstr_cstr(&(dstr)); \
	data_fields[field].len = tds_dstr_len(&(dstr)); \
	} while(0)

	/* setup data fields */
	SET_FIELD_DSTR(HOST_NAME, login->client_host_name);
	if (tds->conn->authentication) {
		data_fields[USER_NAME].len = 0;
		data_fields[PASSWORD].len = 0;
	} else {
		SET_FIELD_DSTR(USER_NAME, login->user_name);
		SET_FIELD_DSTR(PASSWORD, login->password);
	}
	SET_FIELD_DSTR(APP_NAME, login->app_name);
	SET_FIELD_DSTR(SERVER_NAME, login->server_name);
	SET_FIELD_DSTR(LIBRARY_NAME, login->library);
	SET_FIELD_DSTR(LANGUAGE, login->language);
	SET_FIELD_DSTR(DATABASE_NAME, login->database);
	SET_FIELD_DSTR(DB_FILENAME, login->db_filename);
	data_fields[NEW_PASSWORD].len = 0;
	if (IS_TDS72_PLUS(tds->conn) && login->use_new_password) {
		option_flag3 |= TDS_CHANGE_PASSWORD;
		SET_FIELD_DSTR(NEW_PASSWORD, login->new_password);
	}

	/* convert data fields */
	for (field = data_fields; field < data_fields + TDS_VECTOR_SIZE(data_fields); ++field) {
		size_t data_pos;

		data_pos = data_stream.size;
		field->pos = current_pos + data_pos;
		if (field->len) {
			tds_staticin_stream_init(&input, field->ptr, field->len);
			rc = tds_convert_stream(tds, tds->conn->char_convs[client2ucs2], to_server, &input.stream, &data_stream.stream);
			if (TDS_FAILED(rc)) {
				free(data);
				return TDS_FAIL;
			}
		}
		field->len = data_stream.size - data_pos;
	}
	pwd = (unsigned char *) data + data_fields[PASSWORD].pos - current_pos;
	tds7_crypt_pass(pwd, data_fields[PASSWORD].len, pwd);
	pwd = (unsigned char *) data + data_fields[NEW_PASSWORD].pos - current_pos;
	tds7_crypt_pass(pwd, data_fields[NEW_PASSWORD].len, pwd);
	packet_size += data_stream.size;

#if !defined(TDS_DEBUG_LOGIN)
	tdsdump_log(TDS_DBG_INFO2, "quietly sending TDS 7+ login packet\n");
	tdsdump_off();
#endif
	TDS_PUT_INT(tds, packet_size);
	switch (login->tds_version) {
	case 0x700:
		tds7version = tds70Version;
		break;
	case 0x701:
		tds7version = tds71Version;
		break;
	case 0x702:
		tds7version = tds72Version;
		break;
	case 0x703:
		tds7version = tds73Version;
		break;
	case 0x704:
		tds7version = tds74Version;
		break;
	default:
		assert(0 && 0x700 <= login->tds_version && login->tds_version <= 0x704);
	}
	
	tds_put_int(tds, tds7version);

	if (4096 <= login->block_size && login->block_size < 65536u)
		block_size = login->block_size;

	tds_put_int(tds, block_size);	/* desired packet size being requested by client */

	if (block_size > tds->out_buf_max)
		tds_realloc_socket(tds, block_size);

	tds_put_n(tds, client_progver, sizeof(client_progver));	/* client program version ? */

	tds_put_int(tds, getpid());	/* process id of this process */

	tds_put_n(tds, connection_id, sizeof(connection_id));

	if (!login->bulk_copy)
		option_flag1 |= TDS_DUMPLOAD_OFF;
		
	tds_put_byte(tds, option_flag1);

	if (tds->conn->authentication)
		option_flag2 |= TDS_INTEGRATED_SECURITY_ON;

	tds_put_byte(tds, option_flag2);

	if (login->readonly_intent && IS_TDS71_PLUS(tds->conn))
		sql_type_flag |= TDS_READONLY_INTENT;
	tds_put_byte(tds, sql_type_flag);

	if (IS_TDS73_PLUS(tds->conn))
		option_flag3 |= TDS_UNKNOWN_COLLATION_HANDLING;
	tds_put_byte(tds, option_flag3);

	tds_put_int(tds, time_zone);
	tds_put_n(tds, collation, sizeof(collation));

#define PUT_STRING_FIELD_PTR(field) do { \
	TDS_PUT_SMALLINT(tds, data_fields[field].pos); \
	TDS_PUT_SMALLINT(tds, data_fields[field].len / 2u); \
	} while(0)

	/* host name */
	PUT_STRING_FIELD_PTR(HOST_NAME);
	if (tds->conn->authentication) {
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
	} else {
		/* username */
		PUT_STRING_FIELD_PTR(USER_NAME);
		/* password */
		PUT_STRING_FIELD_PTR(PASSWORD);
	}
	/* app name */
	PUT_STRING_FIELD_PTR(APP_NAME);
	/* server name */
	PUT_STRING_FIELD_PTR(SERVER_NAME);
	/* unknown */
	tds_put_smallint(tds, 0);
	tds_put_smallint(tds, 0);
	/* library name */
	PUT_STRING_FIELD_PTR(LIBRARY_NAME);
	/* language  - kostya@warmcat.excom.spb.su */
	PUT_STRING_FIELD_PTR(LANGUAGE);
	/* database name */
	PUT_STRING_FIELD_PTR(DATABASE_NAME);

	/* MAC address */
	tds_getmac(tds_get_s(tds), hwaddr);
	tds_put_n(tds, hwaddr, 6);

	/* authentication stuff */
	TDS_PUT_SMALLINT(tds, current_pos + data_stream.size);
	TDS_PUT_SMALLINT(tds, auth_len);	/* this matches numbers at end of packet */

	/* db file */
	PUT_STRING_FIELD_PTR(DB_FILENAME);

	if (IS_TDS72_PLUS(tds->conn)) {
		/* new password */
		PUT_STRING_FIELD_PTR(NEW_PASSWORD);

		/* SSPI long */
		tds_put_int(tds, 0);
	}

	tds_put_n(tds, data, data_stream.size);

	if (tds->conn->authentication)
		tds_put_n(tds, tds->conn->authentication->packet, auth_len);

	rc = tds_flush_packet(tds);
	tdsdump_on();

	free(data);
	return rc;
}

/**
 * tds7_crypt_pass() -- 'encrypt' TDS 7.0 style passwords.
 * the calling function is responsible for ensuring crypt_pass is at least 
 * 'len' characters
 */
static void
tds7_crypt_pass(const unsigned char *clear_pass, size_t len, unsigned char *crypt_pass)
{
	size_t i;

	for (i = 0; i < len; i++)
		crypt_pass[i] = ((clear_pass[i] << 4) | (clear_pass[i] >> 4)) ^ 0xA5;
}

static TDSRET
tds71_do_login(TDSSOCKET * tds, TDSLOGIN* login)
{
	int i, pkt_len;
	const char *instance_name = tds_dstr_isempty(&login->instance_name) ? "MSSQLServer" : tds_dstr_cstr(&login->instance_name);
	int instance_name_len = strlen(instance_name) + 1;
	TDS_CHAR crypt_flag;
	unsigned int start_pos = 21;
	TDSRET ret;

#define START_POS 21
#define UI16BE(n) ((n) >> 8), ((n) & 0xffu)
#define SET_UI16BE(i,n) TDS_PUT_UA2BE(&buf[i],n)
	TDS_UCHAR buf[] = {
		/* netlib version */
		0, UI16BE(START_POS), UI16BE(6),
		/* encryption */
		1, UI16BE(START_POS + 6), UI16BE(1),
		/* instance */
		2, UI16BE(START_POS + 6 + 1), UI16BE(0),
		/* process id */
		3, UI16BE(0), UI16BE(4),
		/* MARS enables */
		4, UI16BE(0), UI16BE(1),
		/* end */
		0xff
	};
	static const TDS_UCHAR netlib8[] = { 8, 0, 1, 0x55, 0, 0 };
	static const TDS_UCHAR netlib9[] = { 9, 0, 0,    0, 0, 0 };

	TDS_UCHAR *p;

	SET_UI16BE(13, instance_name_len);
	if (!IS_TDS72_PLUS(tds->conn)) {
		SET_UI16BE(16, START_POS + 6 + 1 + instance_name_len);
		buf[20] = 0xff;
	} else {
		start_pos += 5;
#undef  START_POS
#define START_POS 26
		SET_UI16BE(1, START_POS);
		SET_UI16BE(6, START_POS + 6);
		SET_UI16BE(11, START_POS + 6 + 1);
		SET_UI16BE(16, START_POS + 6 + 1 + instance_name_len);
		SET_UI16BE(21, START_POS + 6 + 1 + instance_name_len + 4);
	}

	assert(start_pos >= 21 && start_pos <= sizeof(buf));
	assert(buf[start_pos-1] == 0xff);

	/*
	 * fix a problem with mssql2k which doesn't like
	 * packet splitted during SSL handshake
	 */
	if (tds->out_buf_max < 4096)
		tds_realloc_socket(tds, 4096);

	/* do prelogin */
	tds->out_flag = TDS71_PRELOGIN;

	tds_put_n(tds, buf, start_pos);
	/* netlib version */
	tds_put_n(tds, IS_TDS72_PLUS(tds->conn) ? netlib9 : netlib8, 6);
	/* encryption */
#if !defined(HAVE_GNUTLS) && !defined(HAVE_OPENSSL)
	/* not supported */
	tds_put_byte(tds, 2);
#else
	tds_put_byte(tds, login->encryption_level >= TDS_ENCRYPTION_REQUIRE ? 1 : 0);
#endif
	/* instance */
	tds_put_n(tds, instance_name, instance_name_len);
	/* pid */
	tds_put_int(tds, getpid());
	/* MARS (1 enabled) */
	if (IS_TDS72_PLUS(tds->conn))
#if ENABLE_ODBC_MARS
		tds_put_byte(tds, login->mars);
	login->mars = 0;
#else
		tds_put_byte(tds, 0);
#endif
	ret = tds_flush_packet(tds);
	if (TDS_FAILED(ret))
		return ret;

	/* now process reply from server */
	ret = tds_read_packet(tds);
	if (ret <= 0 || tds->in_flag != TDS_REPLY)
		return TDS_FAIL;
	pkt_len = tds->in_len - tds->in_pos;

	/* the only thing we care is flag */
	p = tds->in_buf + tds->in_pos;
	/* default 2, no certificate, no encryption */
	crypt_flag = 2;
	for (i = 0;; i += 5) {
		TDS_UCHAR type;
		int off, len;

		if (i >= pkt_len)
			return TDS_FAIL;
		type = p[i];
		if (type == 0xff)
			break;
		/* check packet */
		if (i+4 >= pkt_len)
			return TDS_FAIL;
		off = TDS_GET_UA2BE(&p[i+1]);
		len = TDS_GET_UA2BE(&p[i+3]);
		if (off > pkt_len || (off+len) > pkt_len)
			return TDS_FAIL;
		if (type == 1 && len >= 1) {
			crypt_flag = p[off];
		}
#if ENABLE_ODBC_MARS
		if (IS_TDS72_PLUS(tds->conn) && type == 4 && len >= 1)
			login->mars = p[off];
#endif
	}
	/* we readed all packet */
	tds->in_pos += pkt_len;
	/* TODO some mssql version do not set last packet, update tds according */

	tdsdump_log(TDS_DBG_INFO1, "detected flag %d\n", crypt_flag);

	/* if server do not has certificate do normal login */
	if (crypt_flag == 2) {
		/* unless we wanted encryption and got none, then fail */
		if (login->encryption_level >= TDS_ENCRYPTION_REQUIRE)
			return TDS_FAIL;

		return tds7_send_login(tds, login);
	}

	/*
	 * if server has a certificate it require at least a crypted login
	 * (even if data is not encrypted)
	 */

	/* here we have to do encryption ... */

	ret = tds_ssl_init(tds);
	if (TDS_FAILED(ret))
		return ret;

	/* server just encrypt the first packet */
	if (crypt_flag == 0)
		tds->conn->encrypt_single_packet = 1;

	ret = tds7_send_login(tds, login);

	/* if flag is 0 it means that after login server continue not encrypted */
	if (crypt_flag == 0 || TDS_FAILED(ret))
		tds_ssl_deinit(tds->conn);

	return ret;
}

