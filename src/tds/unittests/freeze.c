/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2020  Frediano Ziglio
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
 * Purpose: test freeze functionality
 */
#include "common.h"
#include <assert.h>
#include <freetds/bytes.h>

#if HAVE_UNISTD_H
#undef getpid
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <freetds/replacements.h>
#include <freetds/utils.h>

#ifdef TDS_HAVE_MUTEX
#ifdef _WIN32
#define SHUT_WR SD_SEND
#endif

typedef struct {
	uint8_t *buf;
	size_t len;
	size_t capacity;
} buffer;

#define BUF_INITIALIZER { NULL, 0, 0 }

static void
buf_append(buffer *buf, const void *data, size_t data_len)
{
	size_t min_size = buf->len + data_len;
	if (min_size > buf->capacity) {
		buf->capacity *= 2;
		if (buf->capacity < min_size)
			buf->capacity = min_size;
		assert(TDS_RESIZE(buf->buf, buf->capacity) != NULL);
	}
	memcpy(buf->buf + buf->len, data, data_len);
	buf->len += data_len;
}

static void
buf_free(buffer *buf)
{
	free(buf->buf);
	buf->buf = NULL;
	buf->len = buf->capacity = 0;
}

static TDSSOCKET *tds = NULL;
static buffer thread_buf = BUF_INITIALIZER;
static TDS_SYS_SOCKET server_socket = INVALID_SOCKET;
static bool was_shutdown = false;

static void shutdown_server_socket(void);

#define BLOCK_SIZE (tds->conn->env.block_size)

/* thread to read data from main thread */
static TDS_THREAD_PROC_DECLARE(fake_thread_proc, arg)
{
	TDS_SYS_SOCKET s = TDS_PTR2INT(arg);
#if ENABLE_ODBC_MARS
	unsigned seq = 0;
	unsigned total_len = 0;
	TDS72_SMP_HEADER mars;
#endif

	for (;;) {
		char buf[4096];
		int len = READSOCKET(s, buf, sizeof(buf));
		if (len <= 0)
			break;
		buf_append(&thread_buf, buf, len);
		tdsdump_dump_buf(TDS_DBG_INFO1, "received", buf, len);
#if ENABLE_ODBC_MARS
		total_len += len;
		while (tds->conn->mars && total_len >= BLOCK_SIZE + sizeof(mars)) {
			mars.signature = TDS72_SMP;
			mars.type = TDS_SMP_ACK;
			mars.sid = 0;
			TDS_PUT_A4LE(&mars.size, 16);
			TDS_PUT_A4LE(&mars.seq, 4);
			TDS_PUT_A4LE(&mars.wnd, 4 + seq);
			WRITESOCKET(s, &mars, sizeof(mars));
			total_len -= BLOCK_SIZE + sizeof(mars);
			seq++;
		}
#endif
	}

	/* close socket to cleanup and signal main thread */
	CLOSESOCKET(s);
	return TDS_THREAD_RESULT(0);
}

/* remove all headers (TDS and MARS) and do some checks on them */
static void
strip_headers(buffer *buf)
{
	uint8_t final = 0;
	uint8_t *p = buf->buf;
	uint8_t *dst = p;
	uint8_t *end = buf->buf + buf->len;
	while (p < end) {
		size_t len;

		assert(final == 0);
		if (p[0] == TDS72_SMP) {
			assert(end - p >= 8); /* to read SMP part */
			len = TDS_GET_UA4LE(p+4);
			assert(len >= 16);
			assert(p + len <= end);
			p += 16;
			len -= 16;
			assert(end - p >= 4); /* to read TDS header part */
			assert(len == TDS_GET_UA2BE(p+2));
		} else {
			assert(end - p >= 4); /* to read TDS header part */
			len = TDS_GET_UA2BE(p+2);
		}
		assert(len > 8);
		assert(p + len <= end);
		final = p[1];
		memmove(dst, p + 8, len - 8);
		dst += len - 8;
		p += len;
	}
	assert(final == 1 || was_shutdown);
	buf->len = dst - buf->buf;
}

static buffer buf = BUF_INITIALIZER;

/* Append some data to buffer and TDS.
 * If data is NULL append some random data */
static void
append(const void *data, size_t size)
{
	uint8_t rand_buf[2048];
	if (!data) {
		size_t n;
		assert(size <= sizeof(rand_buf));
		for (n = 0; n < size; ++n)
			rand_buf[n] = rand();
		data = rand_buf;
	}
	tds_put_n(tds, data, size);
	buf_append(&buf, data, size);
}

/* Append a number for buffer only */
static void
append_num(uint64_t num, unsigned size)
{
	uint8_t num_buf[8];
	unsigned n;

	assert(size == 1 || size == 2 || size == 4 || size == 8);
	for (n = 0; n < size; ++n) {
		num_buf[n] = num & 0xff;
		num >>= 8;
	}
	assert(num == 0);
	buf_append(&buf, num_buf, size);
}

/* base tests
 * - lengths;
 * - nested freeze;
 * - some packet wrapping case;
 */
static void
test1(void)
{
	TDSFREEZE outer, inner;
	unsigned written, left;

	/* just to not start at 0 */
	append("test", 4);

	/* test writing data as UTF16 */
	tds_freeze(tds, &outer, 2);
	append_num(3, 2);
	append("a\0b\0c", 6);
	assert(tds_freeze_written(&outer) == 8);
	written = tds_freeze_written(&outer) / 2 - 1;
	tds_freeze_close_len(&outer, written);

	/* nested freeze */
	tds_freeze(tds, &outer, 0);
	assert(tds_freeze_written(&outer) == 0);
	append("test", 4);
	tds_freeze(tds, &inner, 0);
	assert(tds_freeze_written(&outer) == 4);
	assert(tds_freeze_written(&inner) == 0);
	tds_put_smallint(tds, 1234);
	append_num(1234, 2);
	append("test", 4);
	assert(tds_freeze_written(&outer) == 10);
	assert(tds_freeze_written(&inner) == 6);
	append(NULL, 600 - 5);
	append("hello", 5);
	tdsdump_log(TDS_DBG_INFO2, "%u\n", (unsigned) tds_freeze_written(&outer));
	assert(tds_freeze_written(&outer) == 610);
	assert(tds_freeze_written(&inner) == 606);

	/* check values do not change before and after close */
	tds_freeze_close(&inner);
	assert(tds_freeze_written(&outer) == 610);

	/* test wrapping packets */
	append(NULL, 600 - 5);
	append("hello", 5);
	tdsdump_log(TDS_DBG_INFO2, "%u\n", (unsigned) tds_freeze_written(&outer));
	assert(tds_freeze_written(&outer) == 610 + 600);

	/* append data in the additional part to check it */
	left = tds->out_buf_max - tds->out_pos;
	append(NULL, left - 3);
	tds_put_int(tds, 0x12345678);
	assert(tds->out_pos > tds->out_buf_max);
	append_num(0x12345678, 4);

	tds_freeze_close(&outer);
}

static void
test_len1(void)
{
	TDSFREEZE outer;

	/* simple len, small */
	tds_freeze(tds, &outer, 4);
	append_num(10, 4);
	append(NULL, 10);
	tds_freeze_close(&outer);

	/* simple len, large */
	tds_freeze(tds, &outer, 2);
	append_num(1234, 2);
	append(NULL, 1234);
	tds_freeze_close(&outer);

	/* simple len, large */
	tds_freeze(tds, &outer, 4);
	append_num(1045, 4);
	append(NULL, 1045);
	tds_freeze_close(&outer);
}

/* similar to test_len1 with other APIs */
static void
test_len2(void)
{
	TDS_START_LEN_UINT(tds) {
		append_num(4 + 10 + 2 + 1234 + 4 + 1045, 4);

		/* simple len, small */
		TDS_START_LEN_UINT(tds) {
			append_num(10, 4);
			append(NULL, 10);
		} TDS_END_LEN

		/* simple len, large */
		TDS_START_LEN_USMALLINT(tds) {
			append_num(1234, 2);
			append(NULL, 1234);
		} TDS_END_LEN

		/* simple len, large */
		TDS_START_LEN_UINT(tds) {
			append_num(1045, 4);
			append(NULL, 1045);
		} TDS_END_LEN
	} TDS_END_LEN
}

/* check if sending packet is failing */
static void
test_failure(size_t len)
{
	TDSFREEZE outer;
	size_t old_len;

	append(NULL, 123);
	tds_freeze(tds, &outer, 2);
	old_len = buf.len;
	if (len)
		append(NULL, len);
	tds_freeze_abort(&outer);
	memset(buf.buf + old_len, 0xab, buf.capacity - old_len);
	buf.len = old_len;
}

static void
test_failure1(void)
{
	test_failure(0);
}

static void
test_failure2(void)
{
	test_failure(BLOCK_SIZE * 3 + 56);
}

/* test if server close connection */
static void
test_shutdown(void)
{
	TDSFREEZE outer;

	append(NULL, BLOCK_SIZE + 17);
	tds_freeze(tds, &outer, 4);
	append(NULL, BLOCK_SIZE * 2 + 67);
	shutdown_server_socket();
	was_shutdown = true;
	tds_freeze_close(&outer);
	buf.len = BLOCK_SIZE - 8;
}

/* test freeze cross packet boundary */
static void
test_cross(unsigned num_bytes)
{
	TDSFREEZE outer;
	unsigned left;

	left = tds->out_buf_max - tds->out_pos;
	append(NULL, left - num_bytes);
	append_num(1234, 4);
	tds_put_int(tds, 1234);
	tds_freeze(tds, &outer, 2);
	append_num(1045, 2);
	append(NULL, 1045);
	tds_freeze_close(&outer);
}

/* this will make the first packet a bit bigger than final */
static void
test_cross1(void)
{
	test_cross(1);
}

/* this will make the first packet exact as big as final */
static void
test_cross2(void)
{
	test_cross(4);
}

/* test freeze just when another packet is finished,
 * we should not generate an empty final one */
static void
test_end(void)
{
	TDSFREEZE outer, inner;
	unsigned left;

	left = tds->out_buf_max - tds->out_pos;
	append(NULL, left - 1);
	tds_freeze(tds, &outer, 0);
	append_num(123, 1);
	tds_put_byte(tds, 123);
	tds_freeze(tds, &inner, 0);
	tds_freeze_close(&inner);
	tds_freeze_close(&outer);
}

/* close the socket, force thread to stop also */
static void
shutdown_server_socket(void)
{
	char sock_buf[32];

	shutdown(server_socket, SHUT_WR);
	while (READSOCKET(server_socket, sock_buf, sizeof(sock_buf)) > 0)
		continue;
}

static void
test(int mars, void (*real_test)(void))
{
	TDSCONTEXT *ctx;
	TDS_SYS_SOCKET sockets[2];
	tds_thread fake_thread;

#if !ENABLE_ODBC_MARS
	if (mars)
		return;
#endif

	ctx = tds_alloc_context(NULL);
	assert(ctx);
	tds = tds_alloc_socket(ctx, 512);
	assert(tds);

	/* provide connection to a fake remove server */
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) >= 0);
	tds_socket_set_nosigpipe(sockets[0], 1);
	was_shutdown = false;
	tds->state = TDS_IDLE;
	tds_set_s(tds, sockets[0]);

	if (tds_thread_create(&fake_thread, fake_thread_proc, TDS_INT2PTR(sockets[1])) != 0) {
		perror("tds_thread_create");
		exit(1);
	}
	server_socket = sockets[0];

#if ENABLE_ODBC_MARS
	if (mars) {
		tds->conn->mars = 1;
		assert(tds_realloc_socket(tds, tds->out_buf_max));
		tds_init_write_buf(tds);
	}
#endif

	real_test();

	tds_flush_packet(tds);

	/* flush all data and wait for other thread to finish cleanly */
	shutdown_server_socket();
	tds_thread_join(fake_thread, NULL);

	tdsdump_dump_buf(TDS_DBG_INFO1, "sent buffer", buf.buf, buf.len);

	strip_headers(&thread_buf);
	tdsdump_dump_buf(TDS_DBG_INFO1, "thread buffer", thread_buf.buf, thread_buf.len);
	assert(buf.len == thread_buf.len);
	assert(memcmp(buf.buf, thread_buf.buf, buf.len) == 0);

	tds_free_socket(tds);
	tds = NULL;
	tds_free_context(ctx);

	buf_free(&buf);
	buf_free(&thread_buf);
}

TEST_MAIN()
{
	int mars;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	tdsdump_open(tds_dir_getenv(TDS_DIR("TDSDUMP")));

	for (mars = 0; mars < 2; ++mars) {
		test(mars, test1);
		test(mars, test_len1);
		test(mars, test_len2);
		test(mars, test_failure1);
		test(mars, test_failure2);
		test(mars, test_shutdown);
		test(mars, test_cross1);
		test(mars, test_cross2);
		test(mars, test_end);
	}

	return 0;
}
#else	/* !TDS_HAVE_MUTEX */
TEST_MAIN()
{
	printf("Not possible for this platform.\n");
	return 0; /* TODO 77 ? */
}
#endif
