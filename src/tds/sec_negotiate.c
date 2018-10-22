/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2015  Frediano Ziglio
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

#if HAVE_STDDEF_H
#include <stddef.h>
#endif /* HAVE_STDDEF_H */

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/time.h>
#include <freetds/tds.h>
#include <freetds/bytes.h>
#include <freetds/utils/string.h>
#include "replacements.h"

#ifdef HAVE_GNUTLS
#  include "sec_negotiate_gnutls.h"
#elif defined(HAVE_OPENSSL)
#  include "sec_negotiate_openssl.h"
#endif


/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{
 */

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)

typedef struct tds5_negotiate
{
	TDSAUTHENTICATION tds_auth;
	/** message type from server */
	unsigned msg_type;
} TDS5NEGOTIATE;

static TDSRET
tds5_negotiate_free(TDSCONNECTION * conn, TDSAUTHENTICATION * tds_auth)
{
	TDS5NEGOTIATE *auth = (TDS5NEGOTIATE *) tds_auth;

	free(auth->tds_auth.packet);
	free(auth);

	return TDS_SUCCESS;
}

void
tds5_negotiate_set_msg_type(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth, unsigned msg_type)
{
	TDS5NEGOTIATE *auth = (TDS5NEGOTIATE *) tds_auth;

	if (tds_auth && tds_auth->free == tds5_negotiate_free)
		auth->msg_type = msg_type;
}

static void
tds5_send_msg(TDSSOCKET *tds, uint16_t msg_type)
{
	tds_put_tinyint(tds, TDS_MSG_TOKEN);
	tds_put_tinyint(tds, 3);
	tds_put_tinyint(tds, 1);
	tds_put_smallint(tds, msg_type);
}

static TDSRET
tds5_negotiate_handle_next(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth, size_t len)
{
	TDS5NEGOTIATE *auth = (TDS5NEGOTIATE *) tds_auth;
	TDSPARAMINFO *info;
	void *rsa, *nonce = NULL;
	size_t rsa_len, nonce_len = 0;
	void *em;
	size_t em_size;
	TDSRET rc = TDS_FAIL;

	/* send next data for authentication */

	if (!tds->login)
		goto error;

	/* we only support RSA authentication, we should have send 2/3 parameters:
	 * 1- integer.. unknown actually 1 TODO
	 * 2- binary, rsa public key in PEM format
	 * 3- binary, nonce (optional)
	 */

	/* message not supported */
	if (auth->msg_type != 0x1e)
		goto error;

	info = tds->param_info;
	if (!info || info->num_cols < 2)
		goto error;

	if (info->columns[1]->column_type != SYBLONGBINARY)
		goto error;
	if (info->num_cols >= 3 && info->columns[2]->column_type != SYBLONGBINARY)
		goto error;
	rsa = ((TDSBLOB*) info->columns[1]->column_data)->textvalue;
	rsa_len = info->columns[1]->column_size;
	if (info->num_cols >= 3) {
		nonce = ((TDSBLOB*) info->columns[2]->column_data)->textvalue;
		nonce_len = info->columns[2]->column_size;
	}

	em = tds5_rsa_encrypt(rsa, rsa_len, nonce, nonce_len, tds_dstr_cstr(&tds->login->password), &em_size);
	if (!em)
		goto error;

	tds->out_flag = TDS_NORMAL;

	/* password */
	tds5_send_msg(tds, 0x1f);
	tds_put_n(tds, "\xec\x0e\x00\x01\x00\x00\x00\x00\x00\x00\x00\xe1\xff\xff\xff\x7f\x00", 0x11);
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	tds_put_int(tds, em_size);
	tds_put_n(tds, em, em_size);

	/* remote password */
	tds5_send_msg(tds, 0x20);
	tds_put_n(tds, "\xec\x17\x00\x02\x00\x00\x00\x00\x00\x00\x00\x27\xff\x00\x00\x00\x00\x00\x00\x00\xe1\xff\xff\xff\x7f\x00", 0x1a);
	tds_put_byte(tds, TDS5_PARAMS_TOKEN);
	tds_put_byte(tds, 0);
	tds_put_int(tds, em_size);
	tds_put_n(tds, em, em_size);

	free(em);

	rc = tds_flush_packet(tds);

error:
	tds5_negotiate_free(tds->conn, tds_auth);
	tds->conn->authentication = NULL;

	return rc;
}

/**
 * Initialize Sybase negotiate handling
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @return authentication info
 */
TDSAUTHENTICATION *
tds5_negotiate_get_auth(TDSSOCKET * tds)
{
	TDS5NEGOTIATE *auth;

	if (!tds->login)
		return NULL;

	auth = tds_new0(TDS5NEGOTIATE, 1);
	if (!auth)
		return NULL;

	auth->tds_auth.free = tds5_negotiate_free;
	auth->tds_auth.handle_next = tds5_negotiate_handle_next;

	return (TDSAUTHENTICATION *) auth;
}

#else /* not HAVE_GNUTLS or HAVE_OPENSSL */

void
tds5_negotiate_set_msg_type(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth, unsigned msg_type)
{
}

TDSAUTHENTICATION *
tds5_negotiate_get_auth(TDSSOCKET * tds)
{
	tdsdump_log(TDS_DBG_ERROR,
		"Sybase authentication not supported if GnuTLS or OpenSSL are not present\n");

	return NULL;
}

#endif

/** @} */

