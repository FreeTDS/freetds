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

#include <freetds/tds.h>

#ifdef HAVE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>
#ifndef HAVE_GNUTLS_RND
#include <gcrypt.h>
#endif
#elif defined(HAVE_OPENSSL)
#include <openssl/rand.h>
#endif

void
tds_random_buffer(unsigned char *out, int len)
{
	int i;

#if defined(HAVE_GNUTLS) && defined(HAVE_GNUTLS_RND)
	if (gnutls_rnd(GNUTLS_RND_RANDOM, out, len) >= 0)
		return;
	if (gnutls_rnd(GNUTLS_RND_NONCE, out, len) >= 0)
		return;
#elif defined(HAVE_GNUTLS)
	void *p = gcry_random_bytes(len, GCRY_STRONG_RANDOM);
	if (p) {
		memcpy(out, p, len);
		free(p);
		return;
	}
#elif defined(HAVE_OPENSSL)
	if (RAND_bytes(out, len) == 1)
		return;
	if (RAND_pseudo_bytes(out, len) >= 0)
		return;
#endif

	/* TODO find a better random... */
	for (i = 0; i < len; ++i)
		out[i] = rand() / (RAND_MAX / 256);
}
