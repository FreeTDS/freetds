/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2006   Frediano Ziglio
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
#include <tdsiconv.h>

#if HAVE_UNISTD_H
#undef getpid
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <assert.h>

/* test tds_iconv_fread */

static char software_version[] = "$Id: iconv_fread.c,v 1.3 2007-04-03 15:11:49 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	iconv_t cd = tds_sys_iconv_open("ISO-8859-1", "UTF-8");
	static const char out_file[] = "iconv_fread.out";
	char buf[256];
	int i;
	FILE *f;

	if (cd == (iconv_t) - 1) {
		fprintf(stderr, "Error creating conversion, giving up!\n");
		return 0;
	}

	f = fopen(out_file, "w+b");
	if (!f) {
		fprintf(stderr, "Error opening file!\n");
		return 1;
	}

	for (i = 0; i < 32; ++i) {
		char out[512];
		size_t out_len = sizeof(out), res;
		const unsigned char x = 0x90;

		/* write test string to file */
		if (fseek(f, 0L, SEEK_SET)) {
			fprintf(stderr, "Error seeking!\n");
			return 1;
		}
		memset(buf, 'a', i);
		buf[i] = 0xC0 + (x >> 6);
		buf[i+1] = 0x80 + (x & 0x3f);

		fwrite(buf, 1, i+2, f);
		if (fseek(f, 0L, SEEK_SET)) {
			fprintf(stderr, "Error seeking!\n");
			return 1;
		}

		/* convert it */
		memset(out, 'x', sizeof(out));
		res = tds_iconv_fread(cd, f, i+2, 0, out, &out_len);
		printf("res %u out_len %u\n", (unsigned int) res, (unsigned int) out_len);

		/* test */
		memset(buf, 'a', i);
		buf[i] = 0x90;
		assert(res == 0);
		assert(sizeof(out) - out_len == i+1);
		assert(memcmp(out, buf, i+1) == 0);
	}
	fclose(f);
	unlink(out_file);

	tds_sys_iconv_close(cd);
	return 0;
}
