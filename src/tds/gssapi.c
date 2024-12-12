/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2007-2011  Frediano Ziglio
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

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_NETDB_H
#include <netdb.h>
#endif /* HAVE_NETDB_H */

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif /* HAVE_SYS_SOCKET_H */

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif /* HAVE_SYS_TYPES_H */

#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif /* HAVE_NETINET_IN_H */

#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif /* HAVE_ARPA_INET_H */

#if HAVE_COM_ERR_H
#include <com_err.h>
#endif /* HAVE_COM_ERR_H */

#ifdef ENABLE_KRB5

#ifdef __APPLE__
#define KERBEROS_APPLE_DEPRECATED(x)
#define GSSKRB_APPLE_DEPRECATED(x)
#undef __API_DEPRECATED
#define __API_DEPRECATED(x, y)
#endif
#include <gssapi/gssapi_krb5.h>

#include <freetds/tds.h>
#include <freetds/utils/string.h>
#include <freetds/replacements.h>

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{ 
 */

typedef struct tds_gss_auth
{
	TDSAUTHENTICATION tds_auth;
	gss_ctx_id_t gss_context;
	gss_name_t target_name;
	char *sname;
	OM_uint32 last_stat;
} TDSGSSAUTH;

static TDSRET
tds_gss_free(TDSCONNECTION * conn TDS_UNUSED, struct tds_authentication * tds_auth)
{
	TDSGSSAUTH *auth = (TDSGSSAUTH *) tds_auth;
	OM_uint32 min_stat;

	if (auth->tds_auth.packet) {
		gss_buffer_desc send_tok;

		send_tok.value = (void *) auth->tds_auth.packet;
		send_tok.length = auth->tds_auth.packet_len;
		gss_release_buffer(&min_stat, &send_tok);
	}

	gss_release_name(&min_stat, &auth->target_name);
	free(auth->sname);
	if (auth->gss_context != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&min_stat, &auth->gss_context, GSS_C_NO_BUFFER);
	free(auth);

	return TDS_SUCCESS;
}

static TDSRET tds_gss_continue(TDSSOCKET * tds, struct tds_gss_auth *auth, gss_buffer_desc *token_ptr);

static TDSRET
tds7_gss_handle_next(TDSSOCKET * tds, struct tds_authentication * auth, size_t len)
{
	TDSRET res;
	gss_buffer_desc recv_tok;

	if (((struct tds_gss_auth *) auth)->last_stat != GSS_S_CONTINUE_NEEDED)
		return TDS_FAIL;

	if (auth->packet) {
		OM_uint32 min_stat;
		gss_buffer_desc send_tok;

		send_tok.value = (void *) auth->packet;
		send_tok.length = auth->packet_len;
		gss_release_buffer(&min_stat, &send_tok);
		auth->packet = NULL;
	}

	recv_tok.length = len;
	recv_tok.value = tds_new(char, len);
	if (!recv_tok.value)
		return TDS_FAIL;
	tds_get_n(tds, recv_tok.value, len);

	res = tds_gss_continue(tds, (struct tds_gss_auth *) auth, &recv_tok);
	free(recv_tok.value);
	TDS_PROPAGATE(res);

	if (auth->packet_len) {
		tds->out_flag = TDS7_AUTH;
		tds_put_n(tds, auth->packet, auth->packet_len);
		return tds_flush_packet(tds);
	}
	return TDS_SUCCESS;
}

static TDSRET
tds5_gss_handle_next(TDSSOCKET * tds, struct tds_authentication * auth, size_t len TDS_UNUSED)
{
	gss_buffer_desc recv_tok;
	TDSPARAMINFO *info;
	TDSCOLUMN *col;

	if (((struct tds_gss_auth *) auth)->last_stat != GSS_S_CONTINUE_NEEDED)
		return TDS_FAIL;

	if (auth->packet) {
		OM_uint32 min_stat;
		gss_buffer_desc send_tok;

		send_tok.value = (void *) auth->packet;
		send_tok.length = auth->packet_len;
		gss_release_buffer(&min_stat, &send_tok);
		auth->packet = NULL;
	}

	/* parse from saved message */
	if (auth->msg_type != TDS5_MSG_SEC_OPAQUE)
		goto error;
	auth->msg_type = 0;

	info = tds->param_info;
	if (!info || info->num_cols < 5)
		goto error;

	/* check first column is int and TDS5_SEC_VERSION */
	col = info->columns[0];
	if (tds_get_conversion_type(col->on_server.column_type, col->on_server.column_size) != SYBINT4)
		goto error;
	if (*((TDS_INT *) col->column_data) != TDS5_SEC_VERSION)
		goto error;

	/* check second column is int and TDS5_SEC_SECSESS */
	col = info->columns[1];
	if (tds_get_conversion_type(col->on_server.column_type, col->on_server.column_size) != SYBINT4)
		goto error;
	if (*((TDS_INT *) col->column_data) != TDS5_SEC_SECSESS)
		goto error;

	col = info->columns[3];
	if (col->column_type != SYBLONGBINARY)
		goto error;
	recv_tok.value = ((TDSBLOB*) col->column_data)->textvalue;
	recv_tok.length = col->column_size;

	TDS_PROPAGATE(tds_gss_continue(tds, (struct tds_gss_auth *) auth, &recv_tok));

	tds->out_flag = TDS_NORMAL;
	TDS_PROPAGATE(tds5_gss_send(tds));

	return tds_flush_packet(tds);

error:
	return TDS_FAIL;
}

/**
 * Build a GSSAPI packet to send to server
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @return size of packet
 */
TDSAUTHENTICATION * 
tds_gss_get_auth(TDSSOCKET * tds)
{
	/*
	 * TODO
	 * There are some differences between this implementation and MS on
	 * - MS use SPNEGO with 3 mechnisms (MS KRB5, KRB5, NTLMSSP)
	 * - MS seems to use MUTUAL flag
	 * - name type is "Service and Instance (2)" and not "Principal (1)"
	 * check for memory leaks
	 * check for errors in many functions
	 * a bit more verbose
	 * dinamically load library ??
	 */
	gss_buffer_desc send_tok;
	OM_uint32 maj_stat, min_stat;
#ifdef __APPLE__
	/* some MacOS header defines gss_OID_desc with a wrong byte alignment, use external
	 * library definition. */
#  define nt_principal (*(gss_OID_desc *) GSS_KRB5_NT_PRINCIPAL_NAME)
#else
	/* same as GSS_KRB5_NT_PRINCIPAL_NAME but do not require .so library */
	static gss_OID_desc nt_principal = { 10, (void*) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x01" };
#endif
	const char *server_name;
	/* Storage for getaddrinfo calls */
	struct addrinfo *addrs = NULL;
	int len = 0;

	struct tds_gss_auth *auth;

	if (!tds->login)
		return NULL;

	auth = tds_new0(struct tds_gss_auth, 1);
	if (!auth)
		return NULL;

	auth->tds_auth.free = tds_gss_free;
	auth->tds_auth.handle_next = IS_TDS50(tds->conn) ? tds5_gss_handle_next : tds7_gss_handle_next;
	auth->gss_context = GSS_C_NO_CONTEXT;
	auth->last_stat = GSS_S_COMPLETE;

	server_name = tds_dstr_cstr(&tds->login->server_host_name);
	if (IS_TDS7_PLUS(tds->conn) && strchr(server_name, '.') == NULL) {
		struct addrinfo hints;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_flags = AI_V4MAPPED|AI_ADDRCONFIG|AI_CANONNAME|AI_FQDN;
		if (!getaddrinfo(server_name, NULL, &hints, &addrs) && addrs->ai_canonname
		    && strchr(addrs->ai_canonname, '.') != NULL)
			server_name = addrs->ai_canonname;
	}

	if (!tds_dstr_isempty(&tds->login->server_spn)) {
		auth->sname = strdup(tds_dstr_cstr(&tds->login->server_spn));
	} else if (IS_TDS7_PLUS(tds->conn)) {
		if (tds_dstr_isempty(&tds->login->server_realm_name)) {
			len = asprintf(&auth->sname, "MSSQLSvc/%s:%d", server_name, tds->login->port);
		} else {
			len = asprintf(&auth->sname, "MSSQLSvc/%s:%d@%s", server_name, tds->login->port,
				       tds_dstr_cstr(&tds->login->server_realm_name));
		}
	} else {
		/* TDS 5.0, Sybase */
		server_name = tds_dstr_cstr(&tds->login->server_name);
		if (tds_dstr_isempty(&tds->login->server_realm_name)) {
			len = asprintf(&auth->sname, "%s", server_name);
		} else {
			len = asprintf(&auth->sname, "%s@%s", server_name,
				       tds_dstr_cstr(&tds->login->server_realm_name));
		}
	}
	if (addrs)
		freeaddrinfo(addrs);
	if (len < 0 || auth->sname == NULL) {
		tds_gss_free(tds->conn, (TDSAUTHENTICATION *) auth);
		return NULL;
	}
	tdsdump_log(TDS_DBG_NETWORK, "using kerberos name %s\n", auth->sname);

	/*
	 * Import the name into target_name.  Use send_tok to save
	 * local variable space.
	 */
	send_tok.value = auth->sname;
	send_tok.length = strlen(auth->sname);
	maj_stat = gss_import_name(&min_stat, &send_tok, &nt_principal, &auth->target_name);

	switch (maj_stat) {
	case GSS_S_COMPLETE: 
		tdsdump_log(TDS_DBG_NETWORK, "gss_import_name: GSS_S_COMPLETE: gss_import_name completed successfully.\n");
		if (TDS_FAILED(tds_gss_continue(tds, auth, GSS_C_NO_BUFFER))) {
			tds_gss_free(tds->conn, (TDSAUTHENTICATION *) auth);
			return NULL;
		}
		break;
	case GSS_S_BAD_NAMETYPE: 
		tdsdump_log(TDS_DBG_NETWORK, "gss_import_name: GSS_S_BAD_NAMETYPE: The input_name_type was unrecognized.\n");
		break;
	case GSS_S_BAD_NAME: 
		tdsdump_log(TDS_DBG_NETWORK, "gss_import_name: GSS_S_BAD_NAME: The input_name parameter could not be interpreted as a name of the specified type.\n");
		break;
	case GSS_S_BAD_MECH:
		tdsdump_log(TDS_DBG_NETWORK, "gss_import_name: GSS_S_BAD_MECH: The input name-type was GSS_C_NT_EXPORT_NAME, but the mechanism contained within the input-name is not supported.\n");
		break;
	default:
		tdsdump_log(TDS_DBG_NETWORK, "gss_import_name: unexpected error %d.\n", maj_stat);
		break;
	}

	if (GSS_ERROR(maj_stat)) {
		tds_gss_free(tds->conn, (TDSAUTHENTICATION *) auth);
		return NULL;
	}

	return (TDSAUTHENTICATION *) auth;
}

#ifndef HAVE_ERROR_MESSAGE
static const char *
tds_error_message(OM_uint32 e)
{
	const char *m = strerror(e);
	if (m == NULL)
		return "";
	return m;
}
#define error_message tds_error_message
#endif

static TDSRET
tds_gss_continue(TDSSOCKET * tds, struct tds_gss_auth *auth, gss_buffer_desc *token_ptr)
{
	gss_buffer_desc send_tok;
	OM_uint32 maj_stat, min_stat = 0;
	OM_uint32 ret_flags;
	int gssapi_flags;
	const char *msg = "???";
	gss_OID pmech = GSS_C_NULL_OID;

	auth->last_stat = GSS_S_COMPLETE;

	send_tok.value = NULL;
	send_tok.length = 0;

	/*
	 * Perform the context-establishement loop.
	 *
	 * On each pass through the loop, token_ptr points to the token
	 * to send to the server (or GSS_C_NO_BUFFER on the first pass).
	 * Every generated token is stored in send_tok which is then
	 * transmitted to the server; every received token is stored in
	 * recv_tok, which token_ptr is then set to, to be processed by
	 * the next call to gss_init_sec_context.
	 * 
	 * GSS-API guarantees that send_tok's length will be non-zero
	 * if and only if the server is expecting another token from us,
	 * and that gss_init_sec_context returns GSS_S_CONTINUE_NEEDED if
	 * and only if the server has another token to send us.
	 */

	/*
	 * We always want to ask for the replay, and integ flags.
	 * We may ask for delegation based on config in the tds.conf and other conf files.
	 */
	gssapi_flags = GSS_C_REPLAY_FLAG | GSS_C_INTEG_FLAG;

	if (tds->login->gssapi_use_delegation)
		gssapi_flags |= GSS_C_DELEG_FLAG;
	if (tds->login->mutual_authentication || IS_TDS7_PLUS(tds->conn))
		gssapi_flags |= GSS_C_MUTUAL_FLAG;

	maj_stat = gss_init_sec_context(&min_stat, GSS_C_NO_CREDENTIAL, &auth->gss_context, auth->target_name, 
					GSS_C_NULL_OID,
					gssapi_flags,
					0, NULL,	/* no channel bindings */
					token_ptr, 
					&pmech,	
					&send_tok, &ret_flags, NULL);	/* ignore time_rec */

	tdsdump_log(TDS_DBG_NETWORK, "gss_init_sec_context: actual mechanism at %p\n", pmech);
	if (pmech && pmech->elements) {
		tdsdump_dump_buf(TDS_DBG_NETWORK, "actual mechanism", pmech->elements, pmech->length);
	}
	
	auth->last_stat = maj_stat;
	
	switch (maj_stat) {
	case GSS_S_COMPLETE: 
		msg = "GSS_S_COMPLETE: gss_init_sec_context completed successfully.";
		break;
	case GSS_S_CONTINUE_NEEDED: 
		msg = "GSS_S_CONTINUE_NEEDED: gss_init_sec_context() routine must be called again.";
		break;
	case GSS_S_FAILURE: 
		msg = "GSS_S_FAILURE: The routine failed for reasons that are not defined at the GSS level.";
		tdsdump_log(TDS_DBG_NETWORK, "gss_init_sec_context: min_stat %ld \"%s\"\n", 
						(long) min_stat, error_message(min_stat));
		break;
	case GSS_S_BAD_BINDINGS: 
		msg = "GSS_S_BAD_BINDINGS: The channel bindings are not valid.";
		break;
	case GSS_S_BAD_MECH: 
		msg = "GSS_S_BAD_MECH: The request security mechanism is not supported.";
		break;
	case GSS_S_BAD_NAME: 
		msg = "GSS_S_BAD_NAME: The target_name parameter is not valid.";
		break;
	case GSS_S_BAD_SIG: 
		msg = "GSS_S_BAD_SIG: The input token contains an incorrect integrity check value.";
		break;
	case GSS_S_CREDENTIALS_EXPIRED: 
		msg = "GSS_S_CREDENTIALS_EXPIRED: The supplied credentials are no longer valid.";
		break;
	case GSS_S_DEFECTIVE_CREDENTIAL: 
		msg = "GSS_S_DEFECTIVE_CREDENTIAL: Consistency checks performed on the credential failed.";
		break;
	case GSS_S_DEFECTIVE_TOKEN: 
		msg = "GSS_S_DEFECTIVE_TOKEN: Consistency checks performed on the input token failed.";
		break;
	case GSS_S_DUPLICATE_TOKEN: 
		msg = "GSS_S_DUPLICATE_TOKEN: The token is a duplicate of a token that has already been processed.";
		break;
	case GSS_S_NO_CONTEXT: 
		msg = "GSS_S_NO_CONTEXT: The context handle provided by the caller does not refer to a valid security context.";
		break;
	case GSS_S_NO_CRED: 
		msg = "GSS_S_NO_CRED: The supplied credential handle does not refer to a valid credential, the supplied credential is not";
		break;
	case GSS_S_OLD_TOKEN: 
		msg = "GSS_S_OLD_TOKEN: The token is too old to be checked for duplication against previous tokens which have already been processed.";
		break;
	}
	
	if (GSS_ERROR(maj_stat)) {
		gss_release_buffer(&min_stat, &send_tok);
		tdsdump_log(TDS_DBG_NETWORK, "gss_init_sec_context: %s\n", msg);
		return TDS_FAIL;
	}

	auth->tds_auth.packet = (uint8_t *) send_tok.value;
	auth->tds_auth.packet_len = send_tok.length;

	return TDS_SUCCESS;
}

static void
tds5_send_msg(TDSSOCKET *tds, uint16_t msg_type)
{
	tds_put_tinyint(tds, TDS_MSG_TOKEN);
	tds_put_tinyint(tds, 3); /* length */
	tds_put_tinyint(tds, 1); /* status, 1=has params */
	tds_put_smallint(tds, msg_type);
}

TDSRET
tds5_gss_send(TDSSOCKET *tds)
{
	uint32_t flags = TDS5_SEC_NETWORK_AUTHENTICATION;

	if (!tds->conn->authentication)
		return TDS_FAIL;

	if (tds->login) {
		if (tds->login->gssapi_use_delegation)
			flags |= TDS5_SEC_DELEGATION;
		if (tds->login->mutual_authentication)
			flags |= TDS5_SEC_MUTUAL_AUTHENTICATION;
	}

	tds5_send_msg(tds, TDS5_MSG_SEC_OPAQUE);

	tds_put_byte(tds, TDS5_PARAMFMT_TOKEN);
	TDS_START_LEN_USMALLINT(tds) {
		tds_put_smallint(tds, 5); /* # parameters */

		tds_put_n(tds, NULL, 6); /* name len + output + usertype */
		tds_put_tinyint(tds, SYBINTN);
		tds_put_tinyint(tds, 4);
		tds_put_tinyint(tds, 0); /* locale len */

		tds_put_n(tds, NULL, 6); /* name len + output + usertype */
		tds_put_tinyint(tds, SYBINTN);
		tds_put_tinyint(tds, 4);
		tds_put_tinyint(tds, 0); /* locale len */

		tds_put_n(tds, NULL, 6); /* name len + output + usertype */
		tds_put_tinyint(tds, SYBVARBINARY);
		tds_put_tinyint(tds, 255);
		tds_put_tinyint(tds, 0); /* locale len */

		tds_put_n(tds, NULL, 6); /* name len + output + usertype */
		tds_put_tinyint(tds, SYBLONGBINARY);
		tds_put_int(tds, 0x7fffffff);
		tds_put_tinyint(tds, 0); /* locale len */

		tds_put_n(tds, NULL, 6); /* name len + output + usertype */
		tds_put_tinyint(tds, SYBINTN);
		tds_put_tinyint(tds, 4);
		tds_put_tinyint(tds, 0); /* locale len */
	} TDS_END_LEN

	tds_put_byte(tds, TDS5_PARAMS_TOKEN);

	tds_put_tinyint(tds, 4);
	tds_put_int(tds, TDS5_SEC_VERSION);

	tds_put_tinyint(tds, 4);
	tds_put_int(tds, TDS5_SEC_SECSESS);

	tds_put_tinyint(tds, 12);
	tds_put_n(tds, "\x06\x0a\x2b\x06\x01\x04\x01\x87\x01\x04\x06\x06", 12); /* KRB5 Sybase OID */

	tds_put_int(tds, tds->conn->authentication->packet_len);
	tds_put_n(tds, tds->conn->authentication->packet, tds->conn->authentication->packet_len);

	tds_put_tinyint(tds, 4);
	tds_put_int(tds, flags);

	return TDS_SUCCESS;
}

/** @} */

#endif
