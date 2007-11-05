/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005, 2006, 2007  Ziglio Frediano
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

#ifdef WIN32
#include <process.h>
#endif

#include "tds.h"
#include "tdsiconv.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: login.c,v 1.166 2007-11-05 08:32:36 freddy77 Exp $");

static int tds_send_login(TDSSOCKET * tds, TDSCONNECTION * connection);
static int tds8_do_login(TDSSOCKET * tds, TDSCONNECTION * connection);
static int tds7_send_login(TDSSOCKET * tds, TDSCONNECTION * connection);

void
tds_set_version(TDSLOGIN * tds_login, short major_ver, short minor_ver)
{
	tds_login->major_version = major_ver;
	tds_login->minor_version = minor_ver;
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

void
tds_set_passwd(TDSLOGIN * tds_login, const char *password)
{
	if (password) {
		tds_dstr_zero(&tds_login->password);
		tds_dstr_copy(&tds_login->password, password);
	}
}
void
tds_set_bulk(TDSLOGIN * tds_login, TDS_TINYINT enabled)
{
	tds_login->bulk_copy = enabled ? 0 : 1;
}

void
tds_set_user(TDSLOGIN * tds_login, const char *username)
{
	tds_dstr_copy(&tds_login->user_name, username);
}

void
tds_set_host(TDSLOGIN * tds_login, const char *hostname)
{
	tds_dstr_copy(&tds_login->client_host_name, hostname);
}

void
tds_set_app(TDSLOGIN * tds_login, const char *application)
{
	tds_dstr_copy(&tds_login->app_name, application);
}

/**
 * \brief Set the servername in a TDSLOGIN structure
 *
 * Normally copies \a server into \tds_login.  If \a server does not point to a plausible name, the environment 
 * variables TDSQUERY and DSQUERY are used, in that order.  If they don't exist, the "default default" servername
 * is "SYBASE" (although the utility of that choice is a bit murky).  
 *
 * \param tds_login	points to a TDSLOGIN structure
 * \param server	the servername, or NULL, or a zero-length string
 * \todo open the log file earlier, so these messages can be seen.  
 */
void
tds_set_server(TDSLOGIN * tds_login, const char *server)
{
	if (!server || strlen(server) == 0) {
		server = getenv("TDSQUERY");
		tdsdump_log(TDS_DBG_INFO1, "Setting 'server_name' to '%s' from $TDSQUERY.\n", server);
	}
	if (!server || strlen(server) == 0) {
		server = getenv("DSQUERY");
		tdsdump_log(TDS_DBG_INFO1, "Setting 'server_name' to '%s' from $DSQUERY.\n", server);
	}
	if (!server || strlen(server) == 0) {
		server = "SYBASE";
		tdsdump_log(TDS_DBG_INFO1, "Setting 'server_name' to '%s' (compiled-in default).\n", server);
	}
	tds_dstr_copy(&tds_login->server_name, server);
}

void
tds_set_server_addr(TDSLOGIN * tds_login, const char *server_addr)
{
	if (server_addr) {
		tds_dstr_copy(&tds_login->server_addr, server_addr);
	}
}

void
tds_set_library(TDSLOGIN * tds_login, const char *library)
{
	tds_dstr_copy(&tds_login->library, library);
}

void
tds_set_client_charset(TDSLOGIN * tds_login, const char *charset)
{
	tds_dstr_copy(&tds_login->client_charset, charset);
}

void
tds_set_language(TDSLOGIN * tds_login, const char *language)
{
	tds_dstr_copy(&tds_login->language, language);
}

void
tds_set_capabilities(TDSLOGIN * tds_login, unsigned char *capabilities, int size)
{
	memcpy(tds_login->capabilities, capabilities, size > TDS_MAX_CAPABILITY ? TDS_MAX_CAPABILITY : size);
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

	if (tds->tds_ctx->msg_handler != tds_save_msg)
		return;

	ctx = (TDSSAVECONTEXT *) tds->tds_ctx;
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
			if (tds->tds_ctx->msg_handler)
				tds->tds_ctx->msg_handler(tds->tds_ctx, tds, &ctx->msgs[n].msg);
		} else {
			if (tds->tds_ctx->err_handler)
				tds->tds_ctx->err_handler(tds->tds_ctx, tds, &ctx->msgs[n].msg);
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
 * Do a connection to socket
 * @param tds connection structure. This should be a non-connected connection.
 * @param connection info for connection
 * @return TDS_FAIL or TDS_SUCCEED
 */
int
tds_connect(TDSSOCKET * tds, TDSCONNECTION * connection)
{
	int i, retval;
	int connect_timeout = 0;
	int db_selected = 0;
	char version[64];
	char *str;
	int len;
	
	/*
	 * A major version of 0 means try to guess the TDS version. 
	 * We try them in an order that should work. 
	 */
	const static struct tdsver {
		TDS_TINYINT major_version;
		TDS_TINYINT minor_version;
	} versions[] =
		{ { 8, 0 }
		, { 7, 0 }
		, { 5, 0 }
		, { 4, 2 }
		};

	if (connection->major_version == 0) {
		TDSSAVECONTEXT save_ctx;
		const TDSCONTEXT *old_ctx = tds->tds_ctx;
		typedef void (*env_chg_func_t) (TDSSOCKET * tds, int type, char *oldval, char *newval);
		env_chg_func_t old_env_chg = tds->env_chg_func;

		init_save_context(&save_ctx, old_ctx);
		tds->tds_ctx = &save_ctx.ctx;
		tds->env_chg_func = tds_save_env;
		for (i=0; i < TDS_VECTOR_SIZE(versions); ++i) {
			connection->major_version = versions[i].major_version;
			connection->minor_version = versions[i].minor_version;
			/* fprintf(stdout, "trying TDSVER %d.%d\n", connection->major_version, connection->minor_version); */
			reset_save_context(&save_ctx);
			retval = tds_connect(tds, connection);
			if (TDS_SUCCEED == retval)
				break;
			tds_close_socket(tds);
		}
		tds->env_chg_func = old_env_chg;
		tds->tds_ctx = old_ctx;
		replay_save_context(tds, &save_ctx);
		free_save_context(&save_ctx);
		return retval;
	}
	

	/*
	 * If a dump file has been specified, start logging
	 */
	if (!tds_dstr_isempty(&connection->dump_file)) {
		if (connection->debug_flags)
			tds_debug_flags = connection->debug_flags;
		tdsdump_open(tds_dstr_cstr(&connection->dump_file));
	}

	tds->connection = connection;

	tds->major_version = connection->major_version;
	tds->minor_version = connection->minor_version;
	tds->emul_little_endian = connection->emul_little_endian;
#ifdef WORDS_BIGENDIAN
	if (IS_TDS7_PLUS(tds)) {
		/* TDS 7/8 only supports little endian */
		tds->emul_little_endian = 1;
	}
#endif

	/* set up iconv if not already initialized*/
	if (tds->char_convs[client2ucs2]->to_wire == (iconv_t) -1) {
		if (!tds_dstr_isempty(&connection->client_charset)) {
			tds_iconv_open(tds, tds_dstr_cstr(&connection->client_charset));
		}
	}

	/* specified a date format? */
	/*
	 * if (connection->date_fmt) {
	 * tds->date_fmt=strdup(connection->date_fmt);
	 * }
	 */
	connect_timeout = connection->connect_timeout;

	/* Jeff's hack - begin */
	tds->query_timeout = connect_timeout ? connect_timeout : connection->query_timeout;
	/* end */

	/* verify that ip_addr is not empty */
	if (tds_dstr_isempty(&connection->ip_addr)) {
		tdsdump_log(TDS_DBG_ERROR, "IP address pointer is empty\n");
		if (!tds_dstr_isempty(&connection->server_name)) {
			tdsdump_log(TDS_DBG_ERROR, "Server %s not found!\n", tds_dstr_cstr(&connection->server_name));
		} else {
			tdsdump_log(TDS_DBG_ERROR, "No server specified!\n");
		}
		return TDS_FAIL;
	}

	if (!IS_TDS50(tds) && !tds_dstr_isempty(&connection->instance_name))
		connection->port = tds7_get_instance_port(tds_dstr_cstr(&connection->ip_addr), tds_dstr_cstr(&connection->instance_name));

	if (connection->port < 1) {
		tdsdump_log(TDS_DBG_ERROR, "invalid port number\n");
		return TDS_FAIL;
	}

	memcpy(tds->capabilities, connection->capabilities, TDS_MAX_CAPABILITY);

	retval = tds_version(tds, version);
	if (!retval)
		version[0] = '\0';

	if (tds_open_socket(tds, tds_dstr_cstr(&connection->ip_addr), connection->port, connect_timeout) != TDS_SUCCEED)
		return TDS_FAIL;
	tds_set_state(tds, TDS_IDLE);

	if (IS_TDS8_PLUS(tds)) {
		retval = tds8_do_login(tds, connection);
		db_selected = 1;
	} else if (IS_TDS7_PLUS(tds)) {
		retval = tds7_send_login(tds, connection);
		db_selected = 1;
	} else {
		tds->out_flag = TDS_LOGIN;
		retval = tds_send_login(tds, connection);
	}
	if (retval == TDS_FAIL || !tds_process_login_tokens(tds)) {
		tds_close_socket(tds);
		tdserror(tds->tds_ctx, tds, TDSEFCON, 0); 	/* "Adaptive Server connection failed" */
								/* If it's a bad login, the server will send that mesasge */
		return TDS_FAIL;
	}

	if (connection->text_size || (!db_selected && !tds_dstr_isempty(&connection->database))) {
		len = 64 + tds_quote_id(tds, NULL, tds_dstr_cstr(&connection->database),-1);
		if ((str = (char *) malloc(len)) == NULL)
			return TDS_FAIL;

		str[0] = 0;
		if (connection->text_size) {
			sprintf(str, "set textsize %d ", connection->text_size);
		}
		if (!db_selected && !tds_dstr_isempty(&connection->database)) {
			strcat(str, "use ");
			tds_quote_id(tds, strchr(str, 0), tds_dstr_cstr(&connection->database), -1);
		}
		retval = tds_submit_query(tds, str);
		free(str);
		if (retval != TDS_SUCCEED)
			return TDS_FAIL;

		if (tds_process_simple_query(tds) != TDS_SUCCEED)
			return TDS_FAIL;
	}

	tds->query_timeout = connection->query_timeout;
	tds->connection = NULL;
	return TDS_SUCCEED;
}

static int
tds_put_login_string(TDSSOCKET * tds, const char *buf, int n)
{
	int buf_len = (buf ? strlen(buf) : 0);

	return tds_put_buf(tds, (const unsigned char *) buf, n, buf_len);
}

static int
tds_send_login(TDSSOCKET * tds, TDSCONNECTION * connection)
{
#ifdef WORDS_BIGENDIAN
	static const unsigned char be1[] = { 0x02, 0x00, 0x06, 0x04, 0x08, 0x01 };
#endif
	static const unsigned char le1[] = { 0x03, 0x01, 0x06, 0x0a, 0x09, 0x01 };
	static const unsigned char magic2[] = { 0x00, 0x00 };

	static const unsigned char magic3[] = { 0x00, 0x00, 0x00 };

	/* these seem to endian flags as well 13,17 on intel/alpha 12,16 on power */

#ifdef WORDS_BIGENDIAN
	static const unsigned char be2[] = { 0x00, 12, 16 };
#endif
	static const unsigned char le2[] = { 0x00, 13, 17 };

	/* 
	 * the former byte 0 of magic5 causes the language token and message to be 
	 * absent from the login acknowledgement if set to 1. There must be a way 
	 * of setting this in the client layer, but I am not aware of any thing of
	 * the sort -- bsb 01/17/99
	 */
	static const unsigned char magic5[] = { 0x00, 0x00 };
	static const unsigned char magic6[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

	static const unsigned char magic42[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	static const unsigned char magic50[] = { 0x00, 0x00, 0x00, 0x00 };

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
	const char *server_charset;

	int len;
	char blockstr[16];

	if (strchr(tds_dstr_cstr(&connection->user_name), '\\') != NULL) {
		tdsdump_log(TDS_DBG_ERROR, "NT login not support using TDS 4.x or 5.0\n");
		return TDS_FAIL;
	}

	if (IS_TDS42(tds)) {
		memcpy(protocol_version, "\004\002\000\000", 4);
		memcpy(program_version, "\004\002\000\000", 4);
	} else if (IS_TDS46(tds)) {
		memcpy(protocol_version, "\004\006\000\000", 4);
		memcpy(program_version, "\004\002\000\000", 4);
	} else if (IS_TDS50(tds)) {
		memcpy(protocol_version, "\005\000\000\000", 4);
		memcpy(program_version, "\005\000\000\000", 4);
	} else {
		tdsdump_log(TDS_DBG_SEVERE, "Unknown protocol version!\n");
		exit(1);
	}
	/*
	 * the following code is adapted from  Arno Pedusaar's 
	 * (psaar@fenar.ee) MS-SQL Client. His was a much better way to
	 * do this, (well...mine was a kludge actually) so here's mostly his
	 */

	tds_put_login_string(tds, tds_dstr_cstr(&connection->client_host_name), TDS_MAX_LOGIN_STR_SZ);	/* client host name */
	tds_put_login_string(tds, tds_dstr_cstr(&connection->user_name), TDS_MAX_LOGIN_STR_SZ);	/* account name */
	tds_put_login_string(tds, tds_dstr_cstr(&connection->password), TDS_MAX_LOGIN_STR_SZ);	/* account password */
	tds_put_login_string(tds, "37876", TDS_MAX_LOGIN_STR_SZ);	/* host process */
#ifdef WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_n(tds, le1, 6);
	} else {
		tds_put_n(tds, be1, 6);
	}
#else
	tds_put_n(tds, le1, 6);
#endif
	tds_put_byte(tds, connection->bulk_copy);
	tds_put_n(tds, magic2, 2);
	if (IS_TDS42(tds)) {
		tds_put_int(tds, 512);
	} else {
		tds_put_int(tds, 0);
	}
	tds_put_n(tds, magic3, 3);
	tds_put_login_string(tds, tds_dstr_cstr(&connection->app_name), TDS_MAX_LOGIN_STR_SZ);
	tds_put_login_string(tds, tds_dstr_cstr(&connection->server_name), TDS_MAX_LOGIN_STR_SZ);
	if (IS_TDS42(tds)) {
		tds_put_login_string(tds, tds_dstr_cstr(&connection->password), 255);
	} else {
		len = tds_dstr_len(&connection->password);
		if (len > 253)
			len = 0;
		tds_put_byte(tds, 0);
		tds_put_byte(tds, len);
		tds_put_n(tds, tds_dstr_cstr(&connection->password), len);
		tds_put_n(tds, NULL, 253 - len);
		tds_put_byte(tds, len + 2);
	}

	tds_put_n(tds, protocol_version, 4);	/* TDS version; { 0x04,0x02,0x00,0x00 } */
	tds_put_login_string(tds, tds_dstr_cstr(&connection->library), 10);	/* client program name */
	if (IS_TDS42(tds)) {
		tds_put_int(tds, 0);
	} else {
		tds_put_n(tds, program_version, 4);	/* program version ? */
	}
#ifdef WORDS_BIGENDIAN
	if (tds->emul_little_endian) {
		tds_put_n(tds, le2, 3);
	} else {
		tds_put_n(tds, be2, 3);
	}
#else
	tds_put_n(tds, le2, 3);
#endif
	tds_put_login_string(tds, tds_dstr_cstr(&connection->language), TDS_MAX_LOGIN_STR_SZ);	/* language */
	tds_put_byte(tds, connection->suppress_language);
	tds_put_n(tds, magic5, 2);
	tds_put_byte(tds, connection->encryption_level ? 1 : 0);
	tds_put_n(tds, magic6, 10);

	/* use charset nearest to client or nothing */
	server_charset = NULL;
	if (!tds_dstr_isempty(&connection->server_charset))
		server_charset = tds_dstr_cstr(&connection->server_charset);
	else
		server_charset = tds_sybase_charset_name(tds_dstr_cstr(&connection->client_charset));
	if (!server_charset)
		server_charset = "";
	tds_put_login_string(tds, server_charset, TDS_MAX_LOGIN_STR_SZ);	/* charset */
	/* this is a flag, mean that server should use character set provided by client */
	/* TODO notify charset change ?? what's correct meaning ?? -- freddy77 */
	tds_put_byte(tds, 1);

	/* network packet size */
	if (connection->block_size < 1000000 && connection->block_size)
		sprintf(blockstr, "%d", connection->block_size);
	else
		strcpy(blockstr, "512");
	tds_put_login_string(tds, blockstr, 6);

	if (IS_TDS42(tds)) {
		tds_put_n(tds, magic42, 8);
	} else if (IS_TDS46(tds)) {
		tds_put_n(tds, magic42, 4);
	} else if (IS_TDS50(tds)) {
		tds_put_n(tds, magic50, 4);
		tds_put_byte(tds, TDS_CAPABILITY_TOKEN);
		tds_put_smallint(tds, TDS_MAX_CAPABILITY);
		tds_put_n(tds, tds->capabilities, TDS_MAX_CAPABILITY);
	}

	return tds_flush_packet(tds);
}


int
tds7_send_auth(TDSSOCKET * tds, const unsigned char *challenge, TDS_UINT flags)
{
	int current_pos;
	TDSANSWER answer;

	/* FIXME: stuff duplicate in tds7_send_login */
	const char *domain;
	const char *user_name;
	const char *p;
	int user_name_len;
	int host_name_len;
	int password_len;
	int domain_len;

	TDSCONNECTION *connection = tds->connection;

	/* check connection */
	if (!connection)
		return TDS_FAIL;

	/* parse a bit of config */
	user_name = tds_dstr_cstr(&connection->user_name);
	user_name_len = user_name ? strlen(user_name) : 0;
	host_name_len = tds_dstr_len(&connection->client_host_name);
	password_len = tds_dstr_len(&connection->password);

	/* parse domain\username */
	if ((p = strchr(user_name, '\\')) == NULL)
		return TDS_FAIL;

	domain = user_name;
	domain_len = p - user_name;

	user_name = p + 1;
	user_name_len = strlen(user_name);

	tds->out_flag = TDS7_AUTH;
	tds_put_n(tds, "NTLMSSP", 8);
	tds_put_int(tds, 3);	/* sequence 3 */

	/* FIXME *2 work only for single byte encodings */
	current_pos = 64 + (domain_len + user_name_len + host_name_len) * 2;

	tds_put_smallint(tds, 24);	/* lan man resp length */
	tds_put_smallint(tds, 24);	/* lan man resp length */
	tds_put_int(tds, current_pos);	/* resp offset */
	current_pos += 24;

	tds_put_smallint(tds, 24);	/* nt resp length */
	tds_put_smallint(tds, 24);	/* nt resp length */
	tds_put_int(tds, current_pos);	/* nt resp offset */

	current_pos = 64;

	/* domain */
	tds_put_smallint(tds, domain_len * 2);
	tds_put_smallint(tds, domain_len * 2);
	tds_put_int(tds, current_pos);
	current_pos += domain_len * 2;

	/* username */
	tds_put_smallint(tds, user_name_len * 2);
	tds_put_smallint(tds, user_name_len * 2);
	tds_put_int(tds, current_pos);
	current_pos += user_name_len * 2;

	/* hostname */
	tds_put_smallint(tds, host_name_len * 2);
	tds_put_smallint(tds, host_name_len * 2);
	tds_put_int(tds, current_pos);
	current_pos += host_name_len * 2;

	/* unknown */
	tds_put_smallint(tds, 0);
	tds_put_smallint(tds, 0);
	tds_put_int(tds, current_pos + (24 * 2));

	/* flags */
	tds_answer_challenge(tds_dstr_cstr(&connection->password), challenge, &flags, &answer);
	tds_put_int(tds, flags);

	tds_put_string(tds, domain, domain_len);
	tds_put_string(tds, user_name, user_name_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->client_host_name), host_name_len);

	tds_put_n(tds, answer.lm_resp, 24);
	tds_put_n(tds, answer.nt_resp, 24);

	/* for security reason clear structure */
	memset(&answer, 0, sizeof(TDSANSWER));

	return tds_flush_packet(tds);
}

/**
 * tds7_send_login() -- Send a TDS 7.0 login packet
 * TDS 7.0 login packet is vastly different and so gets its own function
 * \returns the return value is ignored by the caller. :-/
 */
static int
tds7_send_login(TDSSOCKET * tds, TDSCONNECTION * connection)
{
	int rc;

	static const unsigned char client_progver[] = { 6, 0x83, 0xf2, 0xf8 };

	static const unsigned char tds7Version[] = { 0x00, 0x00, 0x00, 0x70 };
	static const unsigned char tds8Version[] = { 0x01, 0x00, 0x00, 0x71 };
	static const unsigned char tds9Version[] = { 0x02, 0x00, 0x09, 0x72 };

	static const unsigned char connection_id[] = { 0x00, 0x00, 0x00, 0x00 };
	unsigned char option_flag1 = 0x00;
	unsigned char option_flag2 = tds->option_flag2;
	static const unsigned char sql_type_flag = 0x00;
	static const unsigned char reserved_flag = 0x00;

	static const unsigned char time_zone[] = { 0x88, 0xff, 0xff, 0xff };
	static const unsigned char collation[] = { 0x36, 0x04, 0x00, 0x00 };

	unsigned char hwaddr[6];

	/* 0xb4,0x00,0x30,0x00,0xe4,0x00,0x00,0x00; */
	char unicode_string[256];
	char *punicode;
	size_t unicode_left;
	int packet_size;
	int block_size;
	int current_pos;
	static const unsigned char ntlm_id[] = "NTLMSSP";
	int domain_login = 0;

	const char *domain = "";
	const char *user_name = tds_dstr_cstr(&connection->user_name);
	const char *p;
	int user_name_len = strlen(user_name);
	int host_name_len = tds_dstr_len(&connection->client_host_name);
	int app_name_len = tds_dstr_len(&connection->app_name);
	size_t password_len = tds_dstr_len(&connection->password);
	int server_name_len = tds_dstr_len(&connection->server_name);
	int library_len = tds_dstr_len(&connection->library);
	int language_len = tds_dstr_len(&connection->language);
	int database_len = tds_dstr_len(&connection->database);
	int domain_len = strlen(domain);
	int auth_len = 0;
#ifdef ENABLE_DEVELOPING
	int kerberos = 1;
	TDS_UCHAR *gss_packet = NULL;
#endif

	tds->out_flag = TDS7_LOGIN;

	/* avoid overflow limiting password */
	if (password_len > 128)
		password_len = 128;

	/* check override of domain */
	if ((p = strchr(user_name, '\\')) != NULL) {
		domain = user_name;
		domain_len = p - user_name;

		user_name = p + 1;
		user_name_len = strlen(user_name);

		domain_login = 1;
	}

	current_pos = IS_TDS90(tds) ? 86 + 8 : 86;	/* ? */

	packet_size = current_pos + (host_name_len + app_name_len + server_name_len + library_len + language_len + database_len) * 2;
	if (domain_login) {
#ifdef ENABLE_DEVELOPING
		if (kerberos)
			auth_len = tds_get_gss_packet(tds, &gss_packet);
		if (auth_len <= 0) {
			kerberos = 0;
			auth_len = 32 + host_name_len + domain_len;
		}
#else
		auth_len = 32 + host_name_len + domain_len;
#endif
		packet_size += auth_len;
	} else
		packet_size += (user_name_len + password_len) * 2;

	tdsdump_log(TDS_DBG_INFO2, "quietly sending TDS 7.0 login packet\n");
	tdsdump_off();

	tds_put_int(tds, packet_size);
	if (IS_TDS90(tds)) {
		tds_put_n(tds, tds9Version, 4);
	} else if (IS_TDS8_PLUS(tds)) {
		tds_put_n(tds, tds8Version, 4);
	} else {
		tds_put_n(tds, tds7Version, 4);
	}

	if (connection->block_size < 1000000)
		block_size = connection->block_size;
	else
		block_size = 4096;	/* SQL server default */
	tds_put_int(tds, block_size);	/* desired packet size being requested by client */

	tds_put_n(tds, client_progver, 4);	/* client program version ? */

	tds_put_int(tds, getpid());	/* process id of this process */

	tds_put_n(tds, connection_id, 4);

	option_flag1 |= 0x80;	/* enable warning messages if SET LANGUAGE issued   */
	option_flag1 |= 0x40;	/* change to initial database must succeed          */
	option_flag1 |= 0x20;	/* enable warning messages if USE <database> issued */

	tds_put_byte(tds, option_flag1);

	if (domain_login)
		option_flag2 |= 0x80;	/* enable domain login security                     */

	tds_put_byte(tds, option_flag2);

	tds_put_byte(tds, sql_type_flag);
	tds_put_byte(tds, reserved_flag);

	tds_put_n(tds, time_zone, 4);
	tds_put_n(tds, collation, 4);

	/* host name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, host_name_len);
	current_pos += host_name_len * 2;
	if (domain_login) {
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
		tds_put_smallint(tds, 0);
	} else {
		/* username */
		tds_put_smallint(tds, current_pos);
		tds_put_smallint(tds, user_name_len);
		current_pos += user_name_len * 2;
		/* password */
		tds_put_smallint(tds, current_pos);
		tds_put_smallint(tds, password_len);
		current_pos += password_len * 2;
	}
	/* app name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, app_name_len);
	current_pos += app_name_len * 2;
	/* server name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, server_name_len);
	current_pos += server_name_len * 2;
	/* unknown */
	tds_put_smallint(tds, 0);
	tds_put_smallint(tds, 0);
	/* library name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, library_len);
	current_pos += library_len * 2;
	/* language  - kostya@warmcat.excom.spb.su */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, language_len);
	current_pos += language_len * 2;
	/* database name */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, database_len);
	current_pos += database_len * 2;

	/* MAC address */
	tds_getmac(tds->s, hwaddr);
	tds_put_n(tds, hwaddr, 6);

	/* authentication stuff */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, auth_len);	/* this matches numbers at end of packet */
	current_pos += auth_len;

	/* unknown */
	tds_put_smallint(tds, current_pos);
	tds_put_smallint(tds, 0);

	if (IS_TDS90(tds)) {
		tds_put_smallint(tds, current_pos);
		tds_put_smallint(tds, 0);

		tds_put_int(tds, 0);
	}

	/* FIXME here we assume single byte, do not use *2 to compute bytes, convert before !!! */
	tds_put_string(tds, tds_dstr_cstr(&connection->client_host_name), host_name_len);
	if (!domain_login) {
		TDSICONV *char_conv = tds->char_convs[client2ucs2];
		tds_put_string(tds, tds_dstr_cstr(&connection->user_name), user_name_len);
		p = tds_dstr_cstr(&connection->password);
		punicode = unicode_string;
		unicode_left = sizeof(unicode_string);

		memset(&char_conv->suppress, 0, sizeof(char_conv->suppress));
		if (tds_iconv(tds, tds->char_convs[client2ucs2], to_server, &p, &password_len, &punicode, &unicode_left) ==
		    (size_t) - 1) {
			tdsdump_log(TDS_DBG_INFO1, "password \"%s\" could not be converted to UCS-2\n", p);
			assert(0);
		}
		password_len = punicode - unicode_string;
		tds7_crypt_pass((unsigned char *) unicode_string, password_len, (unsigned char *) unicode_string);
		tds_put_n(tds, unicode_string, password_len);
	}
	tds_put_string(tds, tds_dstr_cstr(&connection->app_name), app_name_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->server_name), server_name_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->library), library_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->language), language_len);
	tds_put_string(tds, tds_dstr_cstr(&connection->database), database_len);

#ifdef ENABLE_DEVELOPING
	if (domain_login && !kerberos) {
#else
	if (domain_login) {
#endif
		/* from here to the end of the packet is the NTLMSSP authentication */
		tds_put_n(tds, ntlm_id, 8);
		/* sequence 1 client -> server */
		tds_put_int(tds, 1);
		/* flags */
		tds_put_int(tds, 0x08b201);

		/* domain info */
		tds_put_smallint(tds, domain_len);
		tds_put_smallint(tds, domain_len);
		tds_put_int(tds, 32 + host_name_len);

		/* hostname info */
		tds_put_smallint(tds, host_name_len);
		tds_put_smallint(tds, host_name_len);
		tds_put_int(tds, 32);

		/*
		 * here XP put version like 05 01 28 0a (5.1.2600),
		 * similar to GetVersion result
		 * and some unknown bytes like 00 00 00 0f
		 */

		/* hostname and domain */
		tds_put_n(tds, tds_dstr_cstr(&connection->client_host_name), host_name_len);
		tds_put_n(tds, domain, domain_len);
#ifdef ENABLE_DEVELOPING
	} else if (domain_login && kerberos) {
		tds_put_n(tds, gss_packet, auth_len);
		free(gss_packet);
#endif
	}

	rc = tds_flush_packet(tds);
	tdsdump_on();

	return rc;
}

/**
 * tds7_crypt_pass() -- 'encrypt' TDS 7.0 style passwords.
 * the calling function is responsible for ensuring crypt_pass is at least 
 * 'len' characters
 */
unsigned char *
tds7_crypt_pass(const unsigned char *clear_pass, int len, unsigned char *crypt_pass)
{
	int i;

	for (i = 0; i < len; i++)
		crypt_pass[i] = ((clear_pass[i] << 4) | (clear_pass[i] >> 4)) ^ 0xA5;
	return crypt_pass;
}

static int
tds8_do_login(TDSSOCKET * tds, TDSCONNECTION * connection)
{
#if !defined(HAVE_GNUTLS) && !defined(HAVE_OPENSSL)
	/*
	 * In case we do not have SSL enabled do not send pre-login so
	 * if server has certificate but no force encryption login success
	 */
	return tds7_send_login(tds, connection);
#else
	int i, len, ret;
	const char *instance_name = tds_dstr_isempty(&connection->instance_name) ? "MSSQLServer" : tds_dstr_cstr(&connection->instance_name);
	int instance_name_len = strlen(instance_name) + 1;
	TDS_CHAR crypt_flag;
	int start_pos = 21;

#define START_POS 21
#define UI16BE(n) ((n) >> 8), ((n) & 0xffu)
#define SET_UI16BE(i,n) do { buf[i] = ((n) >> 8); buf[i+1] = ((n) & 0xffu); } while(0)
	TDS_UCHAR buf[] = {
		/* netlib version */
		0, UI16BE(START_POS), UI16BE(6),
		/* encryption */
		1, UI16BE(START_POS + 6), UI16BE(1),
		/* instance */
		2, UI16BE(START_POS + 6 + 1), UI16BE(0),
		/* process id */
		3, UI16BE(0), UI16BE(4),
		/* ???? unknown ??? */
		4, UI16BE(0), UI16BE(1),
		/* end */
		0xff
	};
	static const TDS_UCHAR netlib8[] = { 8, 0, 1, 0x55, 0, 0 };
	static const TDS_UCHAR netlib9[] = { 9, 0, 0,    0, 0, 0 };

	TDS_UCHAR *p;

	SET_UI16BE(13, instance_name_len);
	if (!IS_TDS90(tds)) {
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

	/* do prelogin */
	tds->out_flag = TDS8_PRELOGIN;

	tds_put_n(tds, buf, start_pos);
	/* netlib version */
	tds_put_n(tds, IS_TDS90(tds) ? netlib9 : netlib8, 6);
	/* encryption */
	tds_put_byte(tds, connection->encryption_level ? 1 : 0);
	/* instance */
	tds_put_n(tds, instance_name, instance_name_len);
	/* pid */
	tds_put_int(tds, getpid());
	/* ???? unknown ??? */
	if (IS_TDS90(tds))
		tds_put_byte(tds, 0);
	if (tds_flush_packet(tds) == TDS_FAIL)
		return TDS_FAIL;

	/* now process reply from server */
	len = tds_read_packet(tds);
	if (len <= 0 || tds->in_flag != 4)
		return TDS_FAIL;
	len = tds->in_len - tds->in_pos;

	/* the only thing we care is flag */
	p = tds->in_buf + tds->in_pos;
	/* default 2, no certificate, no encryption */
	crypt_flag = 2;
	for (i = 0;; i += 5) {
		TDS_UCHAR type;
		int off, l;

		if (i >= len)
			return TDS_FAIL;
		type = p[i];
		if (type == 0xff)
			break;
		/* check packet */
		if (i+4 >= len)
			return TDS_FAIL;
		off = (((int) p[i+1]) << 8) | p[i+2];
		l = (((int) p[i+3]) << 8) | p[i+4];
		if (off > len || (off+l) > len)
			return TDS_FAIL;
		if (type == 1 && l >= 1) {
			crypt_flag = p[off];
		}
	}
	/* we readed all packet */
	tds->in_pos += len;
	/* TODO some mssql version do not set last packet, update tds according */

	tdsdump_log(TDS_DBG_INFO1, "detected flag %d\n", crypt_flag);

	/* if server do not has certificate do normal login */
	if (crypt_flag == 2) {
		/* unless we wanted encryption and got none, then fail */
		if (connection->encryption_level >= TDS_ENCRYPTION_REQUIRE)
			return TDS_FAIL;

		return tds7_send_login(tds, connection);
	}

	/*
	 * if server has a certificate it require at least a crypted login
	 * (even if data is not encrypted)
	 */

	/* here we have to do encryption ... */

	if (tds_ssl_init(tds) != TDS_SUCCEED)
		return TDS_FAIL;

	ret = tds7_send_login(tds, connection);

	/* if flag is 0 it means that after login server continue not encrypted */
	if (crypt_flag == 0 || ret != TDS_SUCCEED)
		tds_ssl_deinit(tds);

	return ret;
#endif
}

