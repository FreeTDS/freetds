/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2026 Frediano Ziglio
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
 * Check file streaming
 */

#include "common.h"

#include <assert.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <freetds/tds/stream.h>

static uint8_t data[1024 * 2];

TEST_MAIN()
{
	static const char terminators[3][4] = {
		"\n", "\r\n", "\n\n"
	};
	size_t data_len = 0;
	int i;
	FILE *f;
	TDSFILESTREAM stream[1];
	char buf[128];

	/* fill some data containing different line terminators */
	for (i = 0; data_len < sizeof(data) - 40; ++i)
		data_len += sprintf((char *) data + data_len, "This is line %d%s", i, terminators[i % 3]);

	f = fopen("file_stream.dat", "wb");
	assert(f);
	assert(fwrite(data, 1, data_len, f) == data_len);
	fclose(f);

	f = fopen("file_stream.dat", "rb");
	assert(f);
	fseek(f, 1234, SEEK_SET);

	assert(TDS_SUCCEED(tds_file_stream_init(stream, f)));
	assert(tds_file_stream_tell(stream) == 1234);
	assert(tds_file_stream_read_raw(stream, buf, 10) == 10);
	assert(memcmp(buf, "e 74\n\nThis", 10) == 0);
	assert(tds_file_stream_tell(stream) == 1244);
	assert(TDS_SUCCEED(tds_file_stream_seek_set(stream, 567)));
	assert(tds_file_stream_tell(stream) == 567);
	assert(tds_file_stream_read_raw(stream, buf, 20) == 20);
	assert(memcmp(buf, "e 34\r\nThis is line 3", 20) == 0);
	assert(tds_file_stream_tell(stream) == 587);
	assert(TDS_SUCCEED(tds_file_stream_close(stream)));

	unlink("file_stream.dat");

	return 0;
}
