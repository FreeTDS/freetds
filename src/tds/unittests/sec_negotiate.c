/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2025 Frediano Ziglio
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

/*
 * Check sec_negotiate_openssl.h code
 */

/* With this macro we force OpenSSL to trigger errors using deprecated functions */
#define OPENSSL_NO_DEPRECATED

#include "common.h"

#if defined(HAVE_OPENSSL)

#include "../sec_negotiate_openssl.h"

/* *INDENT-OFF* */
static const char privkey[] =
"-----BEGIN PRIVATE KEY-----\n"
"MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCwdxLimeYPKNhb\n"
"V34M9iX2Kx35m437YNDkcNKhvpugZeclcmqLbijmPee5TuQYHQbv2sp0xfQhBR1C\n"
"Pe13VlpgpqwqIid8aTn6SrUusB0Q3jMnPMvFPN2H7AKgMZNt5xaKL2jK+N6urNJf\n"
"9EaOcMIORI8ypL2FjnJY4A5bbsKeCOuppfRLSIMZiM2XEdDG3nYubN8yChVrMAzL\n"
"qO6DRhNslMaJQQDyE8mCftzMyBNMfRcZ9+hu0oc2nRC4h9f2rXkQPk77+uj1dpV6\n"
"U9YuIZ0C3b3WouUOJz6u7uYOx9u7VCGGO7Vu0BYPbL7ksRLwxXqY66MFt+33i7Q8\n"
"EyFn3ZXPAgMBAAECggEAUohGdWQMRP/R/RqVEkPXqmQtH06BH9Z+rLEV2l83E1RF\n"
"wO5b5X1utIy0gadjp+F/mpPGR8pDrWPidNZY540kNPsPH5+cvyPJ4YWqar0kwvxh\n"
"iVL2bPfUFpur+LdnICpKEPQue2vdXm3m8MjjbQBQynKHVOTW3Q2r7mRQYrQRYu66\n"
"++cmruoILHJ8+ks364zku86NRqJ4zLM10s4zbx5JunEba+qSS4DR33wZppZT4vyc\n"
"30TNhFChc6Rx+ubXx3nv3+1T0AD2rcQICNukdPcLcQ5NuAgL7z1n9Af4+woCuwwU\n"
"esTgHTy2u9pObpMg7J6di6zC3ON2YSpw610F6B9g/QKBgQDhfKD5RknDc4VP/WE9\n"
"PLyile6DIyIluzQPcRl6S+rxbQ2XKOdiqukIGIJJPCq24qkEeSU+mNPJNwCPSGD/\n"
"7IYNmLPfDhxPiru4phQ+U5J3br2fqrMPRPUkv0XcWXqclWcyS0tfMOiWxogw9Yzv\n"
"X0y3yxog8F76His4mV7rMLrF4wKBgQDIWDkWcmD/AsLBVzkmYJQan4tgHI4h5eEi\n"
"PXMNF4qhtLzX5dTDDiJ89XXZkcaaaVkiFaHEyFNss9KHe2UbMfDex4WHSFxVERc+\n"
"ykVA0BxhEIi+pLD/20Q5M8Z6fR9UXJQArK9xcob4Pt2tqxwbyUzPw4fH6sIUnwK/\n"
"jAVAszXUJQKBgQDWDqEmY2KNKHKLICgaoTkWQ608UrMNDK18Z0rffYiZDoTTViJq\n"
"2YMFi3bLnVGTcpMvSu5fgWe0YgGnA/gJnHkaGTfQba3UmQhiX09iZ6XouXlMRRld\n"
"SoJKE0Z3y34Jfg+MgEwaHuz+jZQmnkTfzSGgbS/tyyLu4Ir5XSftr8HvLwKBgBJF\n"
"gYdbjR8UyGBLrSmj2z4GWPa+A/Rxe7PcuNZz1C/lROfHzTyw2FJfLI1YLy279+YU\n"
"5PkGcB1U1RmIFnOBfX9D4Riyb8FOWrleRyyfDkeH84C3knDzNWimIS7gpG/UNadO\n"
"GH4XPNn9GciR93FNTZURNxvzfBnXWq/PfFcnQPyFAoGBAMlNguY4QHABVcY8ZV+l\n"
"NI6n16VP/NN/vqUenwr38G3UUvorJ+jAC/BDLjQm7A+lBuoQtPoz9F7aNm2TpEco\n"
"THuiBaxi4xnmhucNIG+L2pzn/wHmu+twpkEgpy/KpP7zHK4BBeaCtGOab+i5cHMY\n"
"DLhQwWAzwX6FhrBbkFjYfpxJ\n"
"-----END PRIVATE KEY-----\n";

static const char pubkey[] =
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIIBCgKCAQEAsHcS4pnmDyjYW1d+DPYl9isd+ZuN+2DQ5HDSob6boGXnJXJqi24o\n"
"5j3nuU7kGB0G79rKdMX0IQUdQj3td1ZaYKasKiInfGk5+kq1LrAdEN4zJzzLxTzd\n"
"h+wCoDGTbecWii9oyvjerqzSX/RGjnDCDkSPMqS9hY5yWOAOW27CngjrqaX0S0iD\n"
"GYjNlxHQxt52LmzfMgoVazAMy6jug0YTbJTGiUEA8hPJgn7czMgTTH0XGffobtKH\n"
"Np0QuIfX9q15ED5O+/ro9XaVelPWLiGdAt291qLlDic+ru7mDsfbu1Qhhju1btAW\n"
"D2y+5LES8MV6mOujBbft94u0PBMhZ92VzwIDAQAB\n"
"-----END RSA PUBLIC KEY-----\n";
/* *INDENT-ON* */

#define NONCE "hello"
#define PASSWORD "my_password"

TEST_MAIN()
{
	size_t em_size = 0;
	void *out;
	BIO *bufio;
	EVP_PKEY *key = NULL;
	EVP_PKEY_CTX *ctx;
	char buf[1024];
	size_t buflen;

	out = tds5_rsa_encrypt(pubkey, strlen(pubkey), NONCE, strlen(NONCE), PASSWORD, &em_size);
	if (!out) {
		fprintf(stderr, "Error encrypting\n");
		return 1;
	}

	/* Decrypt and check content is nonce followed by password */
	bufio = BIO_new_mem_buf((void *) privkey, -1);
	if (!bufio) {
		fprintf(stderr, "error allocating BIO\n");
		return 1;
	}

	key = PEM_read_bio_PrivateKey(bufio, &key, NULL, NULL);
	if (!key) {
		fprintf(stderr, "error creating key\n");
		return 1;
	}
	ctx = EVP_PKEY_CTX_new(key, NULL);
	if (!ctx) {
		fprintf(stderr, "error creating context\n");
		return 1;
	}
	if (EVP_PKEY_decrypt_init(ctx) <= 0
	    || EVP_PKEY_CTX_ctrl(ctx, EVP_PKEY_RSA, -1, EVP_PKEY_CTRL_RSA_PADDING, RSA_PKCS1_OAEP_PADDING, NULL) <= 0) {
		fprintf(stderr, "error setting context\n");
		return 1;
	}
	buflen = sizeof(buf) - 1;
	if (EVP_PKEY_decrypt(ctx, (void *) buf, &buflen, out, em_size) <= 0 || buflen >= sizeof(buf)) {
		fprintf(stderr, "error decrypting\n");
		return 1;
	}
	buf[buflen] = 0;
	if (strcmp(buf, NONCE PASSWORD) != 0) {
		fprintf(stderr, "Wrong exit buffer len %u buffer %s\n", (unsigned) buflen, buf);
		return 1;
	}

	EVP_PKEY_CTX_free(ctx);
	EVP_PKEY_free(key);
	BIO_free(bufio);
	free(out);
	return 0;
}
#else
TEST_MAIN()
{
	return 0;
}
#endif
