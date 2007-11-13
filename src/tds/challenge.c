/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2005  Frediano Ziglio
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
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <ctype.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include "tds.h"
#include "tdsbytes.h"
#include "tdsstring.h"
#include "md4.h"
#include "md5.h"
#include "des.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: challenge.c,v 1.28 2007-11-13 09:14:57 freddy77 Exp $");

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 * @{ 
 */

/*
 * The following code is based on some psuedo-C code from ronald@innovation.ch
 */

typedef struct tds_answer
{
	unsigned char lm_resp[24];
	unsigned char nt_resp[24];
} TDSANSWER;

static void tds_answer_challenge(const char *passwd, const unsigned char *challenge, TDS_UINT *flags, TDSANSWER * answer);
static void tds_encrypt_answer(const unsigned char *hash, const unsigned char *challenge, unsigned char *answer);
static void tds_convert_key(const unsigned char *key_56, DES_KEY * ks);

/**
 * Crypt a given password using schema required for NTLMv1 or NTLM2 authentication
 * @param passwd clear text domain password
 * @param challenge challenge data given by server
 * @param flags NTLM flags from server side
 * @param answer buffer where to store crypted password
 */
static void
tds_answer_challenge(const char *passwd, const unsigned char *challenge, TDS_UINT *flags, TDSANSWER * answer)
{
#define MAX_PW_SZ 14
	int len;
	int i;
	static const des_cblock magic = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
	DES_KEY ks;
	unsigned char hash[24], ntlm2_challenge[16];
	unsigned char passwd_buf[256];
	MD4_CTX context;

	memset(answer, 0, sizeof(TDSANSWER));

	if (!(*flags & 0x80000)) {
		/* convert password to upper and pad to 14 chars */
		memset(passwd_buf, 0, MAX_PW_SZ);
		len = strlen(passwd);
		if (len > MAX_PW_SZ)
			len = MAX_PW_SZ;
		for (i = 0; i < len; i++)
			passwd_buf[i] = toupper((unsigned char) passwd[i]);

		/* hash the first 7 characters */
		tds_convert_key(passwd_buf, &ks);
		tds_des_ecb_encrypt(&magic, sizeof(magic), &ks, (hash + 0));

		/* hash the second 7 characters */
		tds_convert_key(passwd_buf + 7, &ks);
		tds_des_ecb_encrypt(&magic, sizeof(magic), &ks, (hash + 8));

		memset(hash + 16, 0, 5);

		tds_encrypt_answer(hash, challenge, answer->lm_resp);
	} else {
		MD5_CTX md5_ctx;

		/* NTLM2 */
		/* TODO find a better random... */
		for (i = 0; i < 8; ++i)
			hash[i] = rand() / (RAND_MAX/256);
		memset(hash + 8, 0, 16);
		memcpy(answer->lm_resp, hash, 24);

		MD5Init(&md5_ctx);
		MD5Update(&md5_ctx, challenge, 8);
		MD5Update(&md5_ctx, hash, 8);
		MD5Final(&md5_ctx, ntlm2_challenge);
		challenge = ntlm2_challenge;
		memset(&md5_ctx, 0, sizeof(md5_ctx));
	}
	*flags = 0x8201;

	/* NTLM/NTLM2 response */
	len = strlen(passwd);
	if (len > 128)
		len = 128;
	/*
	 * TODO we should convert this to ucs2le instead of
	 * using it blindly as iso8859-1
	 */
	for (i = 0; i < len; ++i) {
		passwd_buf[2 * i] = passwd[i];
		passwd_buf[2 * i + 1] = 0;
	}

	/* compute NTLM hash */
	MD4Init(&context);
	MD4Update(&context, passwd_buf, len * 2);
	MD4Final(&context, hash);
	memset(hash + 16, 0, 5);

	tds_encrypt_answer(hash, challenge, answer->nt_resp);

	/* with security is best be pedantic */
	memset(&ks, 0, sizeof(ks));
	memset(hash, 0, sizeof(hash));
	memset(passwd_buf, 0, sizeof(passwd_buf));
	memset(ntlm2_challenge, 0, sizeof(ntlm2_challenge));
	memset(&context, 0, sizeof(context));
}


/*
* takes a 21 byte array and treats it as 3 56-bit DES keys. The
* 8 byte plaintext is encrypted with each key and the resulting 24
* bytes are stored in the results array.
*/
static void
tds_encrypt_answer(const unsigned char *hash, const unsigned char *challenge, unsigned char *answer)
{
	DES_KEY ks;

	tds_convert_key(hash, &ks);
	tds_des_ecb_encrypt(challenge, 8, &ks, answer);

	tds_convert_key(&hash[7], &ks);
	tds_des_ecb_encrypt(challenge, 8, &ks, &answer[8]);

	tds_convert_key(&hash[14], &ks);
	tds_des_ecb_encrypt(challenge, 8, &ks, &answer[16]);

	memset(&ks, 0, sizeof(ks));
}


/*
* turns a 56 bit key into the 64 bit, odd parity key and sets the key.
* The key schedule ks is also set.
*/
static void
tds_convert_key(const unsigned char *key_56, DES_KEY * ks)
{
	des_cblock key;

	key[0] = key_56[0];
	key[1] = ((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1);
	key[2] = ((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2);
	key[3] = ((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3);
	key[4] = ((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4);
	key[5] = ((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5);
	key[6] = ((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6);
	key[7] = (key_56[6] << 1) & 0xFF;

	tds_des_set_odd_parity(key);
	tds_des_set_key(ks, key, sizeof(key));

	memset(&key, 0, sizeof(key));
}


static int
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

typedef struct tds_ntlm_auth
{
	TDSAUTHENTICATION tds_auth;
} TDSNTLMAUTH;

static int
tds_ntlm_free(TDSSOCKET * tds, TDSAUTHENTICATION * tds_auth)
{
	TDSNTLMAUTH *auth = (TDSNTLMAUTH *) tds_auth;

	free(auth->tds_auth.packet);
	free(auth);

	return TDS_SUCCEED;
}

static int
tds_ntlm_handle_next(TDSSOCKET * tds, struct tds_authentication * auth, size_t len)
{
	unsigned char nonce[8];
	TDS_UINT flags;
	int where;

	/* at least 32 bytes (till context) */
	if (len < 32)
		return TDS_FAIL;

	/* TODO check first 2 values */
	tds_get_n(tds, NULL, 8);	/* NTLMSSP\0 */
	tds_get_int(tds);	/* sequence -> 2 */
	tds_get_n(tds, NULL, 4);	/* domain len (2 time) */
	tds_get_int(tds);	/* domain offset */
	flags = tds_get_int(tds);	/* flags */
	tds_get_n(tds, nonce, 8);
	tdsdump_dump_buf(TDS_DBG_INFO1, "TDS_AUTH_TOKEN nonce", nonce, 8);
	where = 32;

	/*
	 * tds_get_string(tds, domain, domain_len); 
	 * tdsdump_log(TDS_DBG_INFO1, "TDS_AUTH_TOKEN domain %s\n", domain);
	 * where += strlen(domain);
	 */

	if (len < where)
		return TDS_FAIL;

	/* discard context, target and data informations */
	tds_get_n(tds, NULL, len - where);
	tdsdump_log(TDS_DBG_INFO1, "Draining %d bytes\n", len - where);

	return tds7_send_auth(tds, nonce, flags);
}

/**
 * Build a NTLMSPP packet to send to server
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @return authentication info
 */
TDSAUTHENTICATION * 
tds_ntlm_get_auth(TDSSOCKET * tds)
{
	static const unsigned char ntlm_id[] = "NTLMSSP";

	const char *domain;
	const char *user_name;
	const char *p;
	TDS_UCHAR *packet;
	int host_name_len;
	int domain_len;
	int auth_len;
	struct tds_ntlm_auth *auth;

	if (!tds->connection)
		return NULL;

	user_name = tds_dstr_cstr(&tds->connection->user_name);
	host_name_len = tds_dstr_len(&tds->connection->client_host_name);

	/* check override of domain */
	if ((p = strchr(user_name, '\\')) == NULL)
		return NULL;

	domain = user_name;
	domain_len = p - user_name;

	auth = (struct tds_ntlm_auth *) calloc(1, sizeof(struct tds_ntlm_auth));

	if (!auth)
		return NULL;

	auth->tds_auth.free = tds_ntlm_free;
	auth->tds_auth.handle_next = tds_ntlm_handle_next;

	auth->tds_auth.packet_len = auth_len = 32 + host_name_len + domain_len;
	auth->tds_auth.packet = packet = malloc(auth_len);
	if (!packet) {
		free(auth);
		return NULL;
	}

	/* built NTLMSSP authentication packet */
	memcpy(packet, ntlm_id, 8);
	/* sequence 1 client -> server */
	TDS_PUT_A4LE(packet + 8, 1);
	/* flags */
	TDS_PUT_A4LE(packet + 12, 0x08b201);

	/* domain info */
	TDS_PUT_A2LE(packet + 16, domain_len);
	TDS_PUT_A2LE(packet + 18, domain_len);
	TDS_PUT_A4LE(packet + 20, 32 + host_name_len);

	/* hostname info */
	TDS_PUT_A2LE(packet + 24, host_name_len);
	TDS_PUT_A2LE(packet + 26, host_name_len);
	TDS_PUT_A4LE(packet + 28, 32);

	/*
	 * here XP put version like 05 01 28 0a (5.1.2600),
	 * similar to GetVersion result
	 * and some unknown bytes like 00 00 00 0f
	 */

	/* hostname and domain */
	memcpy(packet + 32, tds_dstr_cstr(&tds->connection->client_host_name), host_name_len);
	memcpy(packet + 32 + host_name_len, domain, domain_len);

	return (TDSAUTHENTICATION *) auth;
}

/** @} */

