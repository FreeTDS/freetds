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
#include "common.h"

#include <ctype.h>
#include <assert.h>

static char software_version[] = "$Id: utf8_1.c,v 1.1 2003-10-05 16:47:18 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/* Some no-ASCII strings (XML coding) */
static const char english[] = "English";
static const char spanish[] = "Espa&#241;ol";
static const char french[] = "Fran&#231;ais";
static const char portuguese[] = "Portugu&#234;s";
static const char russian[] = "&#1056;&#1091;&#1089;&#1089;&#1082;&#1080;&#1081;";
static const char arabic[] = "&#x0627;&#x0644;&#x0639;&#x0631;&#x0628;&#x064a;&#x0629;";
static const char chinese[] = "&#x7b80;&#x4f53;&#x4e2d;&#x6587;";
static const char japanese[] = "&#26085;&#26412;&#35486;";
static const char hebrew[] = "&#x05e2;&#x05d1;&#x05e8;&#x05d9;&#x05ea;";

static const char *strings[] = {
	english,
	spanish,
	french,
	portuguese,
	russian,
	arabic,
	chinese,
	japanese,
	hebrew,
	NULL
};

static char *
to_utf8(const char *src, char *dest)
{
	unsigned char *p = (unsigned char *) dest;

	for (; *src;) {
		if (src[0] == '&' && src[1] == '#') {
			const char *end = strchr(src, ';');
			char tmp[16];
			int radix = 10;
			int n;

			assert(end);
			src += 2;
			if (toupper(*src) == 'X') {
				radix = 16;
				++src;
			}
			memcpy(tmp, src, end - src);
			tmp[end - src] = 0;
			n = strtol(tmp, NULL, radix);
			assert(n > 0 && n < 0x10000);
			if (n >= 0x1000) {
				*p++ = 0xe0 | (n >> 12);
				*p++ = 0x80 | ((n >> 6) & 0x3f);
				*p++ = 0x80 | (n & 0x3f);
			} else if (n >= 0x80) {
				*p++ = 0xc0 | (n >> 6);
				*p++ = 0x80 | (n & 0x3f);
			} else {
				*p++ = (unsigned char) n;
			}
			src = end + 1;
		} else {
			*p++ = *src++;
		}
	}
	*p = 0;
	return dest;
}

int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int ret;
	int verbose = 0;
	char tmp[64];
	const char **s;

/*	fprintf(stdout, "%s: Testing login, logout\n", __FILE__);
	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}*/

	/* do sone select to test results */
	for (s = strings; *s; ++s)
		printf("%s\n", to_utf8(*s, tmp));

/*	try_tds_logout(login, tds, verbose); */
	return 0;
}
