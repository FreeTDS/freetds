/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2005-2015  Frediano Ziglio
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
#include <freetds/tds/iconv.h>
#include <freetds/utils.h>
#include <freetds/utils/md4.h>
#include <freetds/utils/md5.h>
#include <freetds/utils/hmac_md5.h>
#include <freetds/utils/des.h>
#include <freetds/replacements.h>
#include <freetds/tds/tls.h>

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
	/** NTLMv2 session base key, valid if has_session_key is set */
	unsigned char session_key[16];
	bool has_session_key;
} TDSANSWER;

/* NTLM negotiate flags, see MS-NLMP */
#define NTLMSSP_NEGOTIATE_UNICODE 0x00000001u
#define NTLMSSP_NEGOTIATE_SIGN 0x00000010u
#define NTLMSSP_NEGOTIATE_SEAL 0x00000020u
#define NTLMSSP_NEGOTIATE_NTLM 0x00000200u
#define NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED 0x00001000u
#define NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED 0x00002000u
#define NTLMSSP_NEGOTIATE_LOCAL_CALL 0x00004000u
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN 0x00008000u
#define NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY 0x00080000u
#define NTLMSSP_NEGOTIATE_VERSION 0x02000000u
#define NTLMSSP_NEGOTIATE_128 0x20000000u
#define NTLMSSP_NEGOTIATE_KEY_EXCH 0x40000000u

/* AV_PAIR identifiers, see MS-NLMP */
#define MSV_AV_EOL 0x0000
#define MSV_AV_FLAGS 0x0006
#define MSV_AV_TIMESTAMP 0x0007
#define MSV_AV_TARGET_NAME 0x0009
#define MSV_AV_CHANNEL_BINDINGS 0x000A

/* MsvAvFlags value, MIC is present */
#define MSV_AV_FLAG_MIC 0x00000002u

/* AUTHENTICATE_MESSAGE header sizes, without and with Version and MIC fields */
#define NTLM_AUTH_HEADER_LEN 64
#define NTLM_AUTH_HEADER_MIC_LEN 88
#define NTLM_AUTH_MIC_OFFSET 72

typedef struct
{
	uint8_t  response_type;
	uint8_t  max_response_type;
	uint16_t reserved1;
	uint32_t reserved2;
	uint64_t timestamp;
	uint8_t  challenge[8];
	uint32_t reserved3;
	/* target info block - variable length */
	uint8_t target_info[4];
} names_blob_prefix_t;

#define AV_PAIR_CBT_BYTES 20

typedef struct
{
	TDSAUTHENTICATION tds_auth;
	uint8_t cbt[16];
	bool has_cbt;
	/** service principal name, UTF-16LE */
	char *spn;
	size_t spn_len;
	/** CHALLENGE_MESSAGE received from server, used to compute MIC */
	unsigned char *challenge;
	size_t challenge_len;
} TDSNTLMAUTH;

static const unsigned char ntlm_id[] = "NTLMSSP";

static TDSRET
tds_answer_challenge(TDSSOCKET * tds,
		     TDSLOGIN * login,
		     const unsigned char *challenge,
		     uint32_t * flags,
		     const unsigned char *names_blob, int names_blob_len, TDSANSWER * answer, unsigned char **ntlm_v2_response);
static void tds_encrypt_answer(const unsigned char *hash, const unsigned char *challenge, unsigned char *answer);
static void tds_convert_key(const unsigned char *key_56, DES_KEY * ks);

#include "ucs2_upper.h"

/**
 * Convert a UTF-16 code unit to upper case, using Unicode simple case mapping
 * like Windows does.
 */
static uint16_t
ucs2_upper(uint16_t c)
{
	size_t low = 0, high = TDS_VECTOR_SIZE(ucs2_upper_ranges);

	if (c < 0x80)
		return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;

	/* binary search the range containing the character */
	while (low < high) {
		size_t mid = (low + high) / 2;
		const ucs2_upper_range *r = &ucs2_upper_ranges[mid];

		if (c < r->first) {
			high = mid;
		} else if (c > r->last) {
			low = mid + 1;
		} else {
			/* delta 0xffff (-1) ranges contain every other character */
			if (r->delta != 0xffff || ((c - r->first) & 1) == 0)
				return (uint16_t) (c + r->delta);
			return c;
		}
	}
	return c;
}

/**
 * Convert a UTF-16LE string to upper case.
 * Surrogates are not changed, characters outside the BMP have no simple mapping
 * to upper case in Windows.
 */
static void
ucs2le_to_upper(char *buf, size_t len)
{
	unsigned char *p = (unsigned char *) buf;

	for (; len >= 2; len -= 2, p += 2) {
		uint16_t c = TDS_GET_UA2LE(p);

		c = ucs2_upper(c);
		TDS_PUT_UA2LE(p, c);
	}
}

static size_t
convert_to_usc2le_string(TDSSOCKET * tds, const char *s, size_t len, char *out)
{
	const char *ib;
	char *ob;
	size_t il, ol;

	TDSICONV * char_conv = tds->conn->char_convs[client2ucs2];

	/* char_conv is only mostly const */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS *) & char_conv->suppress;

	if (char_conv->flags == TDS_ENCODING_MEMCPY) {
		memcpy(out, s, len);
		return len;
	}

	/* convert */
	ib = s;
	il = len;
	ob = out;
	ol = len * 2;
	memset(suppress, 0, sizeof(char_conv->suppress));
	if (tds_iconv(tds, char_conv, to_server, &ib, &il, &ob, &ol) == (size_t) - 1)
		return (size_t) -1;

	return ob - out;
}

static TDSRET
make_ntlm_hash(TDSSOCKET *tds, const char *passwd, unsigned char ntlm_hash[16])
{
/*
 * Windows limits passwords to about 128 characters; a character can take
 * up to 4 bytes in client charset (like UTF-8), so allow 4 bytes per character.
 * Conversion needs an output buffer twice the input length.
 */
#define MAX_NTLM_PASSWD_BYTES (128 * 4)
	MD4_CTX context;
	size_t passwd_len = 0;
	char passwd_usc2le[MAX_NTLM_PASSWD_BYTES * 2];
	size_t passwd_usc2le_len = 0;

	passwd_len = strlen(passwd);

	if (passwd_len > MAX_NTLM_PASSWD_BYTES)
		passwd_len = MAX_NTLM_PASSWD_BYTES;

	passwd_usc2le_len = convert_to_usc2le_string(tds, passwd, passwd_len, passwd_usc2le);
	if (passwd_usc2le_len == (size_t) -1) {
		memset((char *) passwd_usc2le, 0, sizeof(passwd_usc2le));
		return TDS_FAIL;
	}

	/* compute NTLM hash */
	MD4Init(&context);
	MD4Update(&context, (unsigned char *) passwd_usc2le, passwd_usc2le_len);
	MD4Final(&context, ntlm_hash);

	/* with security is best be pedantic */
	memset((char *) passwd_usc2le, 0, passwd_usc2le_len);
	memset(&context, 0, sizeof(context));
	return TDS_SUCCESS;
}


static TDSRET
make_ntlm_v2_hash(TDSSOCKET * tds, const char *passwd, unsigned char ntlm_v2_hash[16])
{
	const char *user_name, *domain;
	size_t domain_len, user_name_len, len, buf_usc2le_len = 0;
	const char *p;

	unsigned char ntlm_hash[16];
	char *buf_usc2le;
	TDSRET res;

	user_name = tds_dstr_cstr(&tds->login->user_name);

	/* parse domain\username */
	p = strchr(user_name, '\\');

	domain = user_name;
	domain_len = p - user_name;

	user_name = p + 1;
	user_name_len = strlen(user_name);

	/* each byte can produce at most one UTF-16 code unit */
	buf_usc2le = tds_new(char, (user_name_len + domain_len) * 2 + 2);

	if (!buf_usc2le)
		return TDS_FAIL;

	/* user name is converted to upper case, after converting to UTF-16 */
	len = convert_to_usc2le_string(tds, user_name, user_name_len, buf_usc2le);
	if (len == (size_t) -1) {
		free(buf_usc2le);
		return TDS_FAIL;
	}
	ucs2le_to_upper(buf_usc2le, len);
	buf_usc2le_len = len;

	/* Target is supposed to be case-sensitive */

	len = convert_to_usc2le_string(tds, domain, domain_len, buf_usc2le + len);
	if (len == (size_t) -1) {
		memset(buf_usc2le, 0, buf_usc2le_len);
		free(buf_usc2le);
		return TDS_FAIL;
	}
	buf_usc2le_len += len;

	res = make_ntlm_hash(tds, passwd, ntlm_hash);
	hmac_md5(ntlm_hash, (const unsigned char *) buf_usc2le, buf_usc2le_len, ntlm_v2_hash);

	/* with security is best be pedantic */
	memset(&ntlm_hash, 0, sizeof(ntlm_hash));
	memset(buf_usc2le, 0, buf_usc2le_len);
	free(buf_usc2le);
	return res;
}


/*
 * hash - The NTLMv2 Hash.
 * client_data - The client data (blob or client nonce).
 * challenge - The server challenge from the Type 2 message.
 */
static unsigned char *
make_lm_v2_response(const unsigned char ntlm_v2_hash[16],
		    const unsigned char *client_data, int client_data_len, const unsigned char challenge[8])
{
	int mac_len = 16 + client_data_len;
	unsigned char *mac;

	mac = tds_new(unsigned char, mac_len);
	if (!mac)
		return NULL;

	memcpy(mac + 8, challenge, 8);
	memcpy(mac + 16, client_data, client_data_len);
	hmac_md5(ntlm_v2_hash, mac + 8, client_data_len + 8, mac);

	return mac;
}

static TDSRET
tds_answer_challenge_ntlmv2(TDSSOCKET * tds,
		     TDSLOGIN * login,
		     const unsigned char *challenge,
		     uint32_t * flags,
		     const unsigned char *names_blob, int names_blob_len, TDSANSWER * answer, unsigned char **ntlm_v2_response)
{
	TDSRET res;
	const char *passwd = tds_dstr_cstr(&login->password);

	/* NTLMv2 */
	unsigned char *lm_v2_response;
	unsigned char ntlm_v2_hash[16];
	const names_blob_prefix_t *names_blob_prefix;

	if (!names_blob)
		return TDS_FAIL;

	res = make_ntlm_v2_hash(tds, passwd, ntlm_v2_hash);
	if (TDS_FAILED(res))
		return res;

	/* LMv2 response */
	/* Take client's challenge from names_blob */
	names_blob_prefix = (const names_blob_prefix_t *) names_blob;
	lm_v2_response = make_lm_v2_response(ntlm_v2_hash, names_blob_prefix->challenge, 8, challenge);
	if (!lm_v2_response)
		return TDS_FAIL;
	memcpy(answer->lm_resp, lm_v2_response, 24);
	free(lm_v2_response);

	/* NTLMv2 response */
	/* Size of lm_v2_response is 16 + names_blob_len */
	*ntlm_v2_response = make_lm_v2_response(ntlm_v2_hash, names_blob, names_blob_len, challenge);
	if (!*ntlm_v2_response)
		return TDS_FAIL;

	/* SessionBaseKey = HMAC_MD5(ResponseKeyNT, NTProofStr), NTProofStr is the first 16 bytes of the response */
	hmac_md5(ntlm_v2_hash, *ntlm_v2_response, 16, answer->session_key);
	answer->has_session_key = true;

	memset(ntlm_v2_hash, 0, sizeof(ntlm_v2_hash));

	/* local not supported */
	*flags &= ~NTLMSSP_NEGOTIATE_LOCAL_CALL;
	return TDS_SUCCESS;
}

/**
 * Crypt a given password using schema required for NTLMv1 or NTLM2 authentication
 * @param passwd clear text domain password
 * @param challenge challenge data given by server
 * @param flags NTLM flags from server side
 * @param answer buffer where to store crypted password
 */
static TDSRET
tds_answer_challenge(TDSSOCKET * tds,
		     TDSLOGIN * login,
		     const unsigned char *challenge,
		     uint32_t * flags,
		     const unsigned char *names_blob, int names_blob_len, TDSANSWER * answer, unsigned char **ntlm_v2_response)
{
#define MAX_PW_SZ 14
	const char *passwd = tds_dstr_cstr(&login->password);
	DES_KEY ks;
	unsigned char hash[24], ntlm2_challenge[16];
	TDSRET res;

	memset(answer, 0, sizeof(TDSANSWER));

	if (login->use_ntlmv2) {
		return tds_answer_challenge_ntlmv2(tds, login, challenge, flags,
						   names_blob, names_blob_len, answer, ntlm_v2_response);
	} else if ((*flags & 0x80000) != 0) {
		/* NTLM2 */
		MD5_CTX md5_ctx;

		tds_random_buffer(hash, 8);
		memset(hash + 8, 0, 16);
		memcpy(answer->lm_resp, hash, 24);

		MD5Init(&md5_ctx);
		MD5Update(&md5_ctx, challenge, 8);
		MD5Update(&md5_ctx, hash, 8);
		MD5Final(&md5_ctx, ntlm2_challenge);
		challenge = ntlm2_challenge;
		memset(&md5_ctx, 0, sizeof(md5_ctx));
	} else if (login->use_lanman) {
		/* LM */
		size_t len, i;
		unsigned char passwd_buf[MAX_PW_SZ];
		static const des_cblock magic = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };

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
		memset(passwd_buf, 0, sizeof(passwd_buf));
	} else {
		memset(answer->lm_resp, 0, sizeof(answer->lm_resp));
	}
	*flags = 0x8201;

	/* NTLM/NTLM2 response */
	res = make_ntlm_hash(tds, passwd, hash);
	memset(hash + 16, 0, 5);

	tds_encrypt_answer(hash, challenge, answer->nt_resp);

	/* with security is best be pedantic */
	memset(&ks, 0, sizeof(ks));
	memset(hash, 0, sizeof(hash));
	memset(ntlm2_challenge, 0, sizeof(ntlm2_challenge));
	return res;
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
	tds_des_set_key(ks, key);

	memset(&key, 0, sizeof(key));
}

/**
 * RC4 encryption, used to encrypt NTLM exported session key.
 * @param key     encryption key
 * @param key_len encryption key length
 * @param in      data to encrypt
 * @param len     length of data
 * @param out     output buffer, len bytes
 */
static void
tds_rc4(const unsigned char *key, size_t key_len, const unsigned char *in, size_t len, unsigned char *out)
{
	unsigned char s[256], t;
	unsigned int i, j;
	size_t n;

	for (i = 0; i < 256; ++i)
		s[i] = (unsigned char) i;
	for (i = j = 0; i < 256; ++i) {
		j = (j + s[i] + key[i % key_len]) & 0xff;
		t = s[i];
		s[i] = s[j];
		s[j] = t;
	}
	for (i = j = 0, n = 0; n < len; ++n) {
		i = (i + 1) & 0xff;
		j = (j + s[i]) & 0xff;
		t = s[i];
		s[i] = s[j];
		s[j] = t;
		out[n] = in[n] ^ s[(s[i] + s[j]) & 0xff];
	}
	memset(s, 0, sizeof(s));
}

/**
 * Compute NTLM message integrity code.
 * MIC = HMAC_MD5(ExportedSessionKey, NEGOTIATE_MESSAGE + CHALLENGE_MESSAGE + AUTHENTICATE_MESSAGE)
 * where AUTHENTICATE_MESSAGE has the MIC field set to zero.
 * @return false on memory error
 */
static bool
tds_ntlm_mic(const unsigned char key[16], const unsigned char *negotiate, size_t negotiate_len,
	     const unsigned char *challenge, size_t challenge_len, const unsigned char *authenticate, size_t authenticate_len,
	     unsigned char mic[16])
{
	unsigned char *buf;
	size_t len = negotiate_len + challenge_len + authenticate_len;

	buf = tds_new(unsigned char, len);

	if (!buf)
		return false;
	memcpy(buf, negotiate, negotiate_len);
	memcpy(buf + negotiate_len, challenge, challenge_len);
	memcpy(buf + negotiate_len + challenge_len, authenticate, authenticate_len);
	hmac_md5(key, buf, len, mic);
	memset(buf, 0, len);
	free(buf);
	return true;
}

/**
 * Fill a field (length, maximum length and offset) of a NTLM message header and copy its data in the payload.
 * @param msg       message buffer
 * @param field_pos position of the field in the header
 * @param data_pos  position of data in the payload, updated
 * @param data      field data
 * @param len       field data length
 */
static void
put_ntlm_field(unsigned char *msg, size_t field_pos, size_t *data_pos, const void *data, size_t len)
{
	TDS_PUT_UA2LE(msg + field_pos, len);
	TDS_PUT_UA2LE(msg + field_pos + 2, len);
	TDS_PUT_UA4LE(msg + field_pos + 4, *data_pos);
	memcpy(msg + *data_pos, data, len);
	*data_pos += len;
}

/**
 * Send AUTHENTICATE_MESSAGE.
 * @param challenge      server challenge (8 bytes)
 * @param flags          negotiate flags from server
 * @param names_blob     NTLMv2 client blob, NULL if not available
 * @param names_blob_len length of names_blob
 * @param use_mic        add message integrity code, names_blob must have MsvAvFlags with MIC bit set
 */
static TDSRET
tds7_send_auth(TDSSOCKET *tds, TDSNTLMAUTH *auth, const unsigned char *challenge, uint32_t flags,
	       const unsigned char *names_blob, int names_blob_len, bool use_mic)
{
	TDSANSWER answer;

	/* FIXME: stuff duplicate in tds7_send_login */
	const char *domain;
	const char *user_name;
	const char *host_name;
	const char *p;
	char *convert_buffer;
	size_t user_name_len, host_name_len, domain_len;
	TDSRET rc;

	unsigned char *ntlm_v2_response = NULL;
	unsigned int ntlm_response_len = 24;
	const unsigned int lm_response_len = 24;
	unsigned int session_key_len = 0;
	unsigned char exported_key[16], encrypted_key[16];
	unsigned char *msg;
	size_t header_len, msg_len, data_pos;

	TDSLOGIN *login = tds->login;

	/* check connection */
	if (!login)
		return TDS_FAIL;

	/* parse a bit of config */
	user_name = tds_dstr_cstr(&login->user_name);
	user_name_len = tds_dstr_len(&login->user_name);
	host_name = tds_dstr_cstr(&login->client_host_name);
	host_name_len = tds_dstr_len(&login->client_host_name);

	/* convert strings */
	convert_buffer = tds_new(char, (user_name_len + host_name_len) * 2);
	if (!convert_buffer)
		return TDS_FAIL;

	user_name_len = convert_to_usc2le_string(tds, user_name, user_name_len, convert_buffer);
	user_name = convert_buffer;
	if (user_name_len != (size_t) -1) {
		host_name_len = convert_to_usc2le_string(tds, host_name, host_name_len, convert_buffer + user_name_len);
		host_name = convert_buffer + user_name_len;
	}
	if (user_name_len == (size_t) -1 || host_name_len == (size_t) -1) {
		free(convert_buffer);
		return TDS_FAIL;
	}

	/* parse domain\username */
	p = user_name;
	for (;;) {
		if (p >= user_name + user_name_len) {
			free(convert_buffer);
			return TDS_FAIL;
		}
		if (p[0] == '\\' && p[1] == 0)
			break;
		p += 2;
	}

	domain = user_name;
	domain_len = p - user_name;

	user_name = p + 2;
	user_name_len = domain + user_name_len - user_name;

	rc = tds_answer_challenge(tds, login, challenge, &flags, names_blob, names_blob_len, &answer, &ntlm_v2_response);
	if (TDS_FAILED(rc)) {
		free(convert_buffer);
		return rc;
	}

	ntlm_response_len = ntlm_v2_response ? 16 + names_blob_len : 24;

	/*
	 * compute exported session key, encrypting a random one if key exchange was negotiated;
	 * key exchange is done only if signing or sealing is negotiated too (MS-NLMP 3.1.5.1.2)
	 */
	if (answer.has_session_key) {
		if ((flags & NTLMSSP_NEGOTIATE_KEY_EXCH) != 0 && (flags & (NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_SEAL)) != 0) {
			tds_random_buffer(exported_key, sizeof(exported_key));
			tds_rc4(answer.session_key, sizeof(answer.session_key), exported_key, sizeof(exported_key), encrypted_key);
			session_key_len = sizeof(encrypted_key);
		} else {
			memcpy(exported_key, answer.session_key, sizeof(exported_key));
		}
	} else {
		flags &= ~NTLMSSP_NEGOTIATE_KEY_EXCH;
		if (use_mic) {
			free(ntlm_v2_response);
			free(convert_buffer);
			return TDS_FAIL;
		}
	}

	if (use_mic) {
		/* MIC requires Version field, server sent a timestamp, LmChallengeResponse should be zero */
		header_len = NTLM_AUTH_HEADER_MIC_LEN;
		flags |= NTLMSSP_NEGOTIATE_VERSION;
		memset(answer.lm_resp, 0, sizeof(answer.lm_resp));
	} else {
		header_len = NTLM_AUTH_HEADER_LEN;
		flags &= ~NTLMSSP_NEGOTIATE_VERSION;
	}

	msg_len = header_len + domain_len + user_name_len + host_name_len + lm_response_len + ntlm_response_len + session_key_len;
	msg = tds_new0(unsigned char, msg_len);

	if (!msg) {
		if (ntlm_v2_response) {
			memset(ntlm_v2_response, 0, ntlm_response_len);
			free(ntlm_v2_response);
		}
		memset(&answer, 0, sizeof(answer));
		free(convert_buffer);
		return TDS_FAIL;
	}

	memcpy(msg, ntlm_id, 8);
	TDS_PUT_UA4LE(msg + 8, 3);	/* sequence 3 */

	data_pos = header_len;
	/* Target Name - domain or server name */
	put_ntlm_field(msg, 28, &data_pos, domain, domain_len);
	put_ntlm_field(msg, 36, &data_pos, user_name, user_name_len);
	/* Workstation Name */
	put_ntlm_field(msg, 44, &data_pos, host_name, host_name_len);
	/* LM/LMv2 Response */
	put_ntlm_field(msg, 12, &data_pos, answer.lm_resp, lm_response_len);
	/* NTLM/NTLMv2 Response */
	put_ntlm_field(msg, 20, &data_pos, ntlm_v2_response ? ntlm_v2_response : answer.nt_resp, ntlm_response_len);
	/* Encrypted Random Session Key */
	put_ntlm_field(msg, 52, &data_pos, encrypted_key, session_key_len);

	TDS_PUT_UA4LE(msg + 60, flags);

	if (use_mic) {
		/* same version we sent in NEGOTIATE_MESSAGE */
		memcpy(msg + 64, auth->tds_auth.packet + 32, 8);
		if (!tds_ntlm_mic(exported_key, auth->tds_auth.packet, auth->tds_auth.packet_len,
				  auth->challenge, auth->challenge_len, msg, msg_len, msg + NTLM_AUTH_MIC_OFFSET))
			rc = TDS_FAIL;
	}

	if (TDS_SUCCEED(rc)) {
		tds->out_flag = TDS7_AUTH;
		tds_put_n(tds, msg, msg_len);
	}

	/* for security reason clear sensitive data */
	if (ntlm_v2_response) {
		memset(ntlm_v2_response, 0, ntlm_response_len);
		free(ntlm_v2_response);
	}
	memset(&answer, 0, sizeof(answer));
	memset(exported_key, 0, sizeof(exported_key));
	memset(encrypted_key, 0, sizeof(encrypted_key));
	memset(msg, 0, msg_len);
	free(msg);
	free(convert_buffer);

	if (TDS_FAILED(rc))
		return rc;
	return tds_flush_packet(tds);
}

static TDSRET
tds_ntlm_free(TDSCONNECTION *conn TDS_UNUSED, TDSAUTHENTICATION *tds_auth)
{
	TDSNTLMAUTH *auth = (TDSNTLMAUTH *) tds_auth;

	free(auth->tds_auth.packet);
	free(auth->spn);
	free(auth->challenge);
	free(auth);

	return TDS_SUCCESS;
}

/**
 * put a 8 byte filetime from a time_t
 * This takes GMT as input
 */
static void
unix_to_nt_time(uint64_t * nt, struct timeval *tv)
{
	/* C time start on 1970, nt time on 1600 */
#define TIME_FIXUP_CONSTANT (((uint64_t) 134774U) * 86400U)

	uint64_t t2;

	t2 = tv->tv_sec;
	t2 += TIME_FIXUP_CONSTANT;
	t2 *= 1000u * 1000u * 10u;
	t2 += tv->tv_usec * 10u;

	*nt = t2;
}

/**
 * Compute channel binding token (MD5 of gss_channel_bindings_struct).
 * @param app_data     channel binding application data, like "tls-unique:" followed by data
 * @param app_data_len application data length
 * @param cbt          output token
 */
static void
tds_calc_cbt(const void *app_data, size_t app_data_len, unsigned char cbt[16])
{
	MD5_CTX md5_ctx;
	unsigned char channel_binding_struct[20];

	/* Build channel binding structure */
	/* initiator_address: 8 bytes of zeros */
	/* acceptor_address: 8 bytes of zeros */
	/* application_data: 4 bytes length (little endian) + data */
	memset(channel_binding_struct, 0, sizeof(channel_binding_struct));
	TDS_PUT_UA4LE(channel_binding_struct + 16, app_data_len);

	/* Calculate MD5 hash */
	MD5Init(&md5_ctx);
	MD5Update(&md5_ctx, channel_binding_struct, sizeof(channel_binding_struct));
	MD5Update(&md5_ctx, app_data, app_data_len);
	MD5Final(&md5_ctx, cbt);
}

static void
get_cbt(TDSSOCKET *tds, TDSNTLMAUTH *auth)
{
	unsigned char app_data[256];
	size_t app_data_len;

	app_data_len = tds_ssl_get_cb(tds->conn, app_data, sizeof(app_data));
	if (app_data_len == 0)
		return;

	tdsdump_dump_buf(TDS_DBG_INFO1, "Channel Binding application data", app_data, app_data_len);
	tds_calc_cbt(app_data, app_data_len, auth->cbt);
	auth->has_cbt = true;
	tdsdump_dump_buf(TDS_DBG_INFO1, "Channel Binding Token", auth->cbt, 16);
}

/**
 * Compute the service principal name to send to the server.
 * SQL Server checks it when Extended Protection is enabled.
 */
static void
get_spn(TDSSOCKET *tds, TDSNTLMAUTH *auth)
{
	char *spn;
	size_t spn_len;

	spn = tds_get_spn(tds);
	if (!spn)
		return;

	tdsdump_log(TDS_DBG_INFO1, "NTLM target name %s\n", spn);
	spn_len = strlen(spn);
	auth->spn = tds_new(char, spn_len * 2);

	if (auth->spn) {
		auth->spn_len = convert_to_usc2le_string(tds, spn, spn_len, auth->spn);
		/* AvLen is 16 bit */
		if (auth->spn_len == (size_t) -1 || auth->spn_len > 0xfff0) {
			free(auth->spn);
			auth->spn = NULL;
			auth->spn_len = 0;
		}
	}
	free(spn);
}

/**
 * Add an AV_PAIR to target_info in names_blob.
 * target_info is at the end of names_blob and ends with a MsvAvEOL terminator.
 * @param names_blob     names_blob buffer, must have space for the new pair
 * @param names_blob_len current length (will be updated)
 * @param av_id          AvId of the new pair
 * @param data           data of the new pair
 * @param data_len       length of data
 */
static void
add_av_pair(unsigned char *names_blob, int *names_blob_len, uint16_t av_id, const void *data, size_t data_len)
{
	/* The -4 is to override the old terminator */
	unsigned char *av_pair = names_blob + *names_blob_len - 4;

	TDS_PUT_UA2LE(av_pair, av_id);
	TDS_PUT_UA2LE(av_pair + 2, data_len);
	memcpy(av_pair + 4, data, data_len);
	memset(av_pair + 4 + data_len, 0, 4);	/* Terminator */

	*names_blob_len += 4 + (int) data_len;
}

/**
 * Add channel binding token (CBT) and target name AV_PAIRs to target_info in names_blob
 * @param auth           NTLM authentication data
 * @param names_blob     names_blob buffer
 * @param names_blob_len current length (will be updated)
 */
static void
add_av_pairs(TDSNTLMAUTH *auth, unsigned char *names_blob, int *names_blob_len)
{
	if (auth->has_cbt)
		add_av_pair(names_blob, names_blob_len, MSV_AV_CHANNEL_BINDINGS, auth->cbt, sizeof(auth->cbt));
	if (auth->spn)
		add_av_pair(names_blob, names_blob_len, MSV_AV_TARGET_NAME, auth->spn, auth->spn_len);

	tdsdump_dump_buf(TDS_DBG_INFO1, "New names_blob\n", names_blob, *names_blob_len);
}

static void
fill_names_blob_prefix(names_blob_prefix_t * prefix)
{
	struct timeval tv;
	uint64_t nttime = 0;

	gettimeofday(&tv, NULL);
	unix_to_nt_time(&nttime, &tv);

	prefix->response_type = 0x01;
	prefix->max_response_type = 0x01;
	prefix->reserved1 = 0x0000;
	prefix->reserved2 = 0x00000000;
#ifdef WORDS_BIGENDIAN
	tds_swap_bytes(&nttime, 8);
#endif
	prefix->timestamp = nttime;
	tds_random_buffer(prefix->challenge, sizeof(prefix->challenge));

	prefix->reserved3 = 0x00000000;
}

/**
 * Build NTLMv2 client blob (names_blob) from server target information.
 * Server AV_PAIRs are kept, MsvAvFlags (if MIC is used), channel binding token and
 * target name are added. The server timestamp, if present, is used.
 * @param auth            NTLM authentication data
 * @param target_info     target information from CHALLENGE_MESSAGE
 * @param target_info_len length of target_info
 * @param mic_allowed     whether we can provide a MIC
 * @param p_names_blob    output names_blob, to be freed
 * @param p_names_blob_len output length of names_blob
 * @param p_use_mic       output, whether MIC should be sent
 */
static TDSRET
build_names_blob(TDSNTLMAUTH *auth, const unsigned char *target_info, size_t target_info_len, bool mic_allowed,
		 unsigned char **p_names_blob, int *p_names_blob_len, bool *p_use_mic)
{
	const unsigned char *timestamp = NULL;
	size_t flags_pos = 0, pos = 0, pairs_len, alloc_len;
	const size_t prefix_len = TDS_OFFSET(names_blob_prefix_t, target_info);
	unsigned char *names_blob;
	int names_blob_len;
	bool use_mic;

	/* parse AV_PAIRs, checking they are well formed */
	for (;;) {
		unsigned int av_id, av_len;

		if (target_info_len - pos < 4)
			return TDS_FAIL;
		av_id = TDS_GET_UA2LE(target_info + pos);
		av_len = TDS_GET_UA2LE(target_info + pos + 2);
		if (av_id == MSV_AV_EOL)
			break;
		if (target_info_len - pos - 4 < av_len)
			return TDS_FAIL;
		if (av_id == MSV_AV_TIMESTAMP && av_len == 8)
			timestamp = target_info + pos + 4;
		if (av_id == MSV_AV_FLAGS && av_len == 4)
			flags_pos = pos + 4;
		pos += 4 + av_len;
	}
	pairs_len = pos;

	/* MIC should be provided if server sent a timestamp */
	use_mic = mic_allowed && timestamp != NULL;

	/* prefix + pairs + MsvAvFlags + CBT + target name + MsvAvEOL + Z(4) */
	alloc_len = prefix_len + pairs_len + 8 + AV_PAIR_CBT_BYTES + 4 + auth->spn_len + 4 + 4;
	names_blob = tds_new0(unsigned char, alloc_len);

	if (!names_blob)
		return TDS_FAIL;

	fill_names_blob_prefix((names_blob_prefix_t *) names_blob);
	if (timestamp)
		memcpy(names_blob + TDS_OFFSET(names_blob_prefix_t, timestamp), timestamp, 8);

	/* copy server AV_PAIRs, the terminator is already zeroed */
	memcpy(names_blob + prefix_len, target_info, pairs_len);
	names_blob_len = (int) (prefix_len + pairs_len + 4);

	if (use_mic) {
		if (flags_pos) {
			unsigned char *p = names_blob + prefix_len + flags_pos;

			TDS_PUT_UA4LE(p, TDS_GET_UA4LE(p) | MSV_AV_FLAG_MIC);
		} else {
			unsigned char av_flags[4];

			TDS_PUT_UA4LE(av_flags, MSV_AV_FLAG_MIC);
			add_av_pair(names_blob, &names_blob_len, MSV_AV_FLAGS, av_flags, sizeof(av_flags));
		}
	}

	/* Add channel binding token (CBT) and target name AV_PAIRs */
	add_av_pairs(auth, names_blob, &names_blob_len);

	/* blob ends with Z(4) after AV_PAIRs, already zeroed */
	names_blob_len += 4;

	*p_names_blob = names_blob;
	*p_names_blob_len = names_blob_len;
	*p_use_mic = use_mic;
	return TDS_SUCCESS;
}

static TDSRET
tds_ntlm_handle_next(TDSSOCKET *tds, TDSAUTHENTICATION *tds_auth, size_t len)
{
	TDSNTLMAUTH *auth = (TDSNTLMAUTH *) tds_auth;
	const unsigned char *msg;
	uint32_t flags;
	size_t data_block_offset;

	int names_blob_len = 0;
	unsigned char *names_blob = NULL;
	bool use_mic = false;

	TDSRET rc;

	/* at least 32 bytes (till context) */
	if (len < 32 || len > 0xffff)
		return TDS_FAIL;

	/* read the entire CHALLENGE_MESSAGE, it is needed to compute MIC */
	free(auth->challenge);
	auth->challenge_len = 0;
	auth->challenge = tds_new(unsigned char, len);

	if (!auth->challenge)
		return TDS_FAIL;
	if (!tds_get_n(tds, auth->challenge, len))
		return TDS_FAIL;
	auth->challenge_len = len;
	msg = auth->challenge;

	if (memcmp(msg, ntlm_id, 8) != 0)
		return TDS_FAIL;
	if (TDS_GET_UA4LE(msg + 8) != 2)	/* sequence -> 2 */
		return TDS_FAIL;
	data_block_offset = TDS_GET_UA4LE(msg + 16);	/* domain offset */
	flags = TDS_GET_UA4LE(msg + 20);
	tdsdump_dump_buf(TDS_DBG_INFO1, "TDS_AUTH_TOKEN nonce", msg + 24, 8);

	/* Version 1 -- The Context, Target Information, and OS Version structure are all omitted */
	/* Version 2 -- The Context and Target Information fields are present, but the OS Version structure is not. */
	if (data_block_offset >= 48 && len >= 48) {
		size_t target_info_len, target_info_offset;

		target_info_len = TDS_GET_UA2LE(msg + 40);
		target_info_offset = TDS_GET_UA4LE(msg + 44);

		/* read Target Info if possible */
		if (target_info_len > 0 && target_info_offset >= 48 && target_info_offset <= len
		    && target_info_len <= len - target_info_offset) {
			rc = build_names_blob(auth, msg + target_info_offset, target_info_len, tds->login->use_ntlmv2,
					      &names_blob, &names_blob_len, &use_mic);
			if (TDS_FAILED(rc))
				return rc;
		}
	}

	rc = tds7_send_auth(tds, auth, msg + 24, flags, names_blob, names_blob_len, use_mic);

	if (names_blob) {
		memset(names_blob, 0, names_blob_len);
		free(names_blob);
	}

	return rc;
}

/**
 * Build a NTLMSPP packet to send to server
 * @param tds     A pointer to the TDSSOCKET structure managing a client/server operation.
 * @return authentication info
 */
TDSAUTHENTICATION * 
tds_ntlm_get_auth(TDSSOCKET * tds)
{
	const char *domain;
	const char *user_name;
	const char *p;
	uint8_t *packet;
	int host_name_len;
	int domain_len;
	int auth_len;
	uint32_t flags;
	TDSNTLMAUTH *auth;

	if (!tds->login)
		return NULL;

	user_name = tds_dstr_cstr(&tds->login->user_name);
	host_name_len = (int)tds_dstr_len(&tds->login->client_host_name);

	/* check override of domain */
	if ((p = strchr(user_name, '\\')) == NULL)
		return NULL;

	domain = user_name;
	domain_len = (int) (p - user_name);

	auth = tds_new0(TDSNTLMAUTH, 1);

	if (!auth)
		return NULL;

	auth->tds_auth.free = tds_ntlm_free;
	auth->tds_auth.handle_next = tds_ntlm_handle_next;

	auth->tds_auth.packet_len = auth_len = 40 + host_name_len + domain_len;
	auth->tds_auth.packet = packet = tds_new(uint8_t, auth_len);
	if (!packet) {
		free(auth);
		return NULL;
	}

	/* built NTLMSSP authentication packet */
	memcpy(packet, ntlm_id, 8);
	/* sequence 1 client -> server */
	TDS_PUT_A4(packet + 8, TDS_HOST4LE(1));
	/* flags */
	/* key exchange requires signing (we don't sign messages, TDS uses TLS for integrity) */
	flags = NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_NEGOTIATE_NTLM
		| NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED | NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED
		| NTLMSSP_NEGOTIATE_ALWAYS_SIGN | NTLMSSP_NEGOTIATE_EXTENDED_SESSIONSECURITY
		| NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_KEY_EXCH | NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_VERSION;
	TDS_PUT_A4(packet + 12, TDS_HOST4LE(flags));

	/* domain info */
	TDS_PUT_A2LE(packet + 16, domain_len);
	TDS_PUT_A2LE(packet + 18, domain_len);
	TDS_PUT_A4LE(packet + 20, 40 + host_name_len);

	/* hostname info */
	TDS_PUT_A2LE(packet + 24, host_name_len);
	TDS_PUT_A2LE(packet + 26, host_name_len);
	TDS_PUT_A4  (packet + 28, TDS_HOST4LE(40));

	/*
	 * Version: product major and minor version, build number (16 bit),
	 * 3 reserved bytes and NTLM revision (15).
	 * Report Windows 10.0 build 20348 (Windows Server 2022), it's informational only.
	 */
	packet[32] = 10;
	packet[33] = 0;
	TDS_PUT_A2LE(packet + 34, 20348);
	TDS_PUT_A4(packet + 36, TDS_HOST4LE(0x0f000000));

	/* hostname and domain */
	memcpy(packet + 40, tds_dstr_cstr(&tds->login->client_host_name), host_name_len);
	memcpy(packet + 40 + host_name_len, domain, domain_len);

	get_cbt(tds, auth);
	get_spn(tds, auth);

	return (TDSAUTHENTICATION *) auth;
}

/** @} */

