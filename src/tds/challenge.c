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

#include <config.h>
#include "tds.h"
#ifdef HAVE_OPENSSL
#include <openssl/des.h>

/*
 * The following code is based on some psuedo-C code from ronald@innovation.ch
 */

static void tds_encrypt_answer(unsigned char *hash, unsigned char *challenge, unsigned char *answer);
static void tds_convert_key(unsigned char *key_56, des_key_schedule ks);

#define MAX_PW_SZ 14

char *tds_answer_challenge(char *passwd, char *challenge)
{
int   len;
int i;
const_des_cblock magic = { 0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25 };
des_cblock lo_in, lo_out, hi_in, hi_out;
des_key_schedule ks;
unsigned char hash[24];
unsigned char passwd_up[MAX_PW_SZ];
static unsigned char answer[24];

	/* convert password to upper and pad to 14 chars */
	memset(passwd_up, 0, MAX_PW_SZ);
	len = strlen(passwd);
	if (len>MAX_PW_SZ) len=MAX_PW_SZ;
	for (i=0; i<len; i++)
		passwd_up[i] = toupper(passwd[i]);


	/* hash the first 7 characters */
	memset(lo_in, 0, 8);
	memcpy(lo_in, passwd_up, 7);
	tds_convert_key(lo_in, ks);
	des_ecb_encrypt(&magic, &lo_out, ks, DES_ENCRYPT);

	/* hash the second 7 characters */
	memset(hi_in, 0, 8);
	memcpy(hi_in, &passwd_up[7], 7);
	tds_convert_key(hi_in, ks);
	des_ecb_encrypt(&magic, &hi_out, ks, DES_ENCRYPT);

	memset(hash, 0, 24);
	memcpy(hash, lo_out, 8);
	memcpy(&hash[8], hi_out, 8);

	tds_encrypt_answer(hash, challenge, answer);
	return answer;
}


/*
* takes a 21 byte array and treats it as 3 56-bit DES keys. The
* 8 byte plaintext is encrypted with each key and the resulting 24
* bytes are stored in the results array.
*/
static void tds_encrypt_answer(unsigned char *hash, unsigned char *challenge, unsigned char *answer)
{
des_key_schedule ks;

	tds_convert_key(hash, ks);
	des_ecb_encrypt((des_cblock*) challenge, (des_cblock*) answer, ks, DES_ENCRYPT);

	tds_convert_key(&hash[7], ks);
	des_ecb_encrypt((des_cblock*) challenge, (des_cblock*) (&answer[8]), ks, DES_ENCRYPT);

	tds_convert_key(&hash[14], ks);
	des_ecb_encrypt((des_cblock*) challenge, (des_cblock*) (&answer[16]), ks, DES_ENCRYPT);
}


/*
* turns a 56 bit key into the 64 bit, odd parity key and sets the key.
* The key schedule ks is also set.
*/
static void tds_convert_key(unsigned char *key_56, des_key_schedule ks)
{
des_cblock key;

	key[0] = key_56[0];
	key[1] = ((key_56[0] << 7) & 0xFF) | (key_56[1] >> 1);
	key[2] = ((key_56[1] << 6) & 0xFF) | (key_56[2] >> 2);
	key[3] = ((key_56[2] << 5) & 0xFF) | (key_56[3] >> 3);
	key[4] = ((key_56[3] << 4) & 0xFF) | (key_56[4] >> 4);
	key[5] = ((key_56[4] << 3) & 0xFF) | (key_56[5] >> 5);
	key[6] = ((key_56[5] << 2) & 0xFF) | (key_56[6] >> 6);
	key[7] =  (key_56[6] << 1) & 0xFF;

	des_set_odd_parity(&key);
	des_set_key(&key, ks);
}

#endif /* HAVE_OPENSSL */
