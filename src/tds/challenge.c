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
#include "md4.h"
#include "md5.h"
#include "des.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: challenge.c,v 1.23.2.1 2006-09-13 07:49:42 freddy77 Exp $");

/**
 * \ingroup libtds
 * \defgroup auth Authentication
 * Functions for handling authentication.
 */

/**
 * \addtogroup auth
 *  \@{ 
 */

/*
 * The following code is based on some psuedo-C code from ronald@innovation.ch
 */

static void tds_encrypt_answer(const unsigned char *hash, const unsigned char *challenge, unsigned char *answer);
static void tds_convert_key(const unsigned char *key_56, DES_KEY * ks);

/**
 * Crypt a given password using schema required for NTLMv1 or NTLM2 authentication
 * @param passwd clear text domain password
 * @param challenge challenge data given by server
 * @param flags NTLM flags from server side
 * @param answer buffer where to store crypted password
 */
void
_tds_answer_challenge(const char *passwd, const unsigned char *challenge, TDS_UINT *flags, TDSANSWER * answer)
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

void
tds_answer_challenge(const char *passwd, const unsigned char *challenge, TDSANSWER * answer)
{
	TDS_UINT flags = 0;
	_tds_answer_challenge(passwd, challenge, &flags, answer);
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


/** \@} */
