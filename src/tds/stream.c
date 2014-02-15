/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2013  Frediano Ziglio
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

/**
 * \file
 * \brief Handle stream of data
 */

#include <config.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include <assert.h>

#include <freetds/tds.h>
#include <freetds/iconv.h>
#include <freetds/stream.h>

/** \cond HIDDEN_SYMBOLS */
#if ENABLE_EXTRA_CHECKS
# define TEMP_INIT(s) char* temp = (char*)malloc(32); const size_t temp_size = 32
# define TEMP_FREE free(temp);
# define TEMP_SIZE temp_size
#else
# define TEMP_INIT(s) char temp[s]
# define TEMP_FREE ;
# define TEMP_SIZE sizeof(temp)
#endif
/** \endcond */

/**
 * Reads and writes from a stream converting characters
 * \tds
 * \param char_conv conversion structure
 * \param direction specify conversion to server or from server
 * \param istream input stream
 * \param ostream output stream
 * \return TDS_SUCCESS of TDS_FAIL
 */
TDSRET
tds_convert_stream(TDSSOCKET * tds, TDSICONV * char_conv, TDS_ICONV_DIRECTION direction,
	TDSINSTREAM * istream, TDSOUTSTREAM *ostream)
{
	TEMP_INIT(4096);
	/*
	 * temp (above) is the "preconversion" buffer, the place where the UCS-2 data
	 * are parked before converting them to ASCII.  It has to have a size,
	 * and there's no advantage to allocating dynamically.
	 * This also avoids any memory allocation error.
	 */
	const char *ib;
	size_t bufleft = 0;
	TDSRET res = TDS_FAIL;

	/* cast away const for message suppression sub-structure */
	TDS_ERRNO_MESSAGE_FLAGS *suppress = (TDS_ERRNO_MESSAGE_FLAGS*) &char_conv->suppress;

	memset(suppress, 0, sizeof(char_conv->suppress));
	for (ib = temp; ostream->buf_len; ib = temp + bufleft) {

		char *ob;
		int len, conv_errno;
		size_t ol;

		assert(ib >= temp);

		/* read a chunk of data */
		len = istream->read(istream, (char*) ib, TEMP_SIZE - bufleft);
		if (len == 0)
			res = TDS_SUCCESS;
		if (len <= 0)
			break;
		bufleft += len;

		/* Convert chunk */
		ib = temp; /* always convert from start of buffer */

convert_more:
		ob = ostream->buffer;
		ol = ostream->buf_len;
		/* FIXME not for last */
		suppress->einval = 1; /* EINVAL matters only on the last chunk. */
		ol = tds_iconv(tds, char_conv, direction, (const char **) &ib, &bufleft, &ob, &ol);
		conv_errno = errno;

		/* write converted chunk */
		len = ostream->write(ostream, ob - ostream->buffer);
		if (len < 0)
			break;

		if ((size_t) -1 == ol) {
			tdsdump_log(TDS_DBG_NETWORK, "Error: tds_convert_stream: tds_iconv returned errno %d, conv_errno %d\n", errno, conv_errno);
			if (conv_errno == E2BIG && ostream->buf_len && bufleft)
				goto convert_more;
			if (conv_errno != EILSEQ) {
				tdsdump_log(TDS_DBG_NETWORK, "Error: tds_convert_stream: "
							     "Gave up converting %u bytes due to error %d.\n",
							     (unsigned int) bufleft, errno);
				tdsdump_dump_buf(TDS_DBG_NETWORK, "Troublesome bytes:", ib, bufleft);
			}

			if (ib == temp) {	/* tds_iconv did not convert anything, avoid infinite loop */
				tdsdump_log(TDS_DBG_NETWORK, "No conversion possible: some bytes left.\n");
				break;
			}

			if (bufleft)
				memmove(temp, ib, bufleft);
		}
	}

	TEMP_FREE;
	return res;
}

/**
 * Reads and writes from a stream to another
 * \tds
 * \param istream input stream
 * \param ostream output stream
 * \return TDS_SUCCESS or TDS_FAIL
 */
TDSRET
tds_copy_stream(TDSSOCKET * tds, TDSINSTREAM * istream, TDSOUTSTREAM * ostream)
{
	while (ostream->buf_len) {
		/* read a chunk of data */
		int len = istream->read(istream, ostream->buffer, ostream->buf_len);
		if (len == 0)
			return TDS_SUCCESS;
		if (len < 0)
			break;

		/* write chunk */
		len = ostream->write(ostream, len);
		if (len < 0)
			break;
	}
	return TDS_FAIL;
}

/**
 * Reads data from network for input stream
 */
static int
tds_data_stream_read(TDSINSTREAM *stream, void *ptr, size_t len)
{
	TDSDATASTREAM *s = (TDSDATASTREAM *) stream;
	if (len > s->wire_size)
		len = s->wire_size;
	tds_get_n(s->tds, ptr, len);
	s->wire_size -= len;
	return len;
}

/**
 * Initialize a data input stream.
 * This stream read data from network.
 * \param stream input stream to initialize
 * \tds
 * \param wire_size byte to read
 */
void
tds_data_stream_init(TDSDATASTREAM * stream, TDSSOCKET * tds, size_t wire_size)
{
	stream->stream.read = tds_data_stream_read;
	stream->wire_size = wire_size;
	stream->tds = tds;
}

/**
 * Writes data to a static allocated buffer
 */
static int
tds_static_stream_write(TDSOUTSTREAM *stream, size_t len)
{
	assert(stream->buf_len >= len);
	stream->buffer += len;
	stream->buf_len -= len;
	return len;
}

/**
 * Initialize an output stream for write into a static allocated buffer
 * \param stream stream to initialize
 * \param ptr buffer to write to
 * \param len buffer size in bytes
 */
void
tds_static_stream_init(TDSSTATICSTREAM * stream, void *ptr, size_t len)
{
	stream->stream.write = tds_static_stream_write;
	stream->stream.buffer = (char *) ptr;
	stream->stream.buf_len = len;
}

/**
 * Writes data to a dynamic allocated buffer
 */
static int
tds_dynamic_stream_write(TDSOUTSTREAM *stream, size_t len)
{
	TDSDYNAMICSTREAM *s = (TDSDYNAMICSTREAM *) stream;
	size_t wanted;

	s->size += len;
	/* TODO use a more smart algorithm */
	wanted = s->size + 2048;
	if (wanted > s->allocated) {
		void *p = realloc(*s->buf, wanted);
		if (!p) return -1;
		*s->buf = p;
		s->allocated = wanted;
	}
	assert(s->allocated > s->size);
	stream->buffer = (char *) *s->buf + s->size;
	stream->buf_len = s->allocated - s->size;
	return len;
}

/**
 * Initialize a dynamic output stream.
 * This stream write data into a dynamic allocated buffer.
 * \param stream stream to initialize
 * \param ptr pointer to pointer to buffer to fill. Buffer
 *        will be extended as needed
 * \param allocated bytes initialially allocated for the buffer.
 *        Useful to reuse buffers
 * \return TDS_SUCCESS on success, TDS_FAIL otherwise
 */
TDSRET
tds_dynamic_stream_init(TDSDYNAMICSTREAM * stream, void **ptr, size_t allocated)
{
#if ENABLE_EXTRA_CHECKS
	const size_t initial_size = 16;
#else
	const size_t initial_size = 1024;
#endif

	stream->stream.write = tds_dynamic_stream_write;
	stream->buf = ptr;
	if (allocated < initial_size) {
		if (*ptr) free(*ptr);
		*ptr = malloc(initial_size);
		if (!*ptr) return TDS_FAIL;
		allocated = initial_size;
	}
	stream->allocated = allocated;
	stream->size = 0;
	stream->stream.buffer = (char *) *ptr;
	stream->stream.buf_len = allocated;
	return TDS_SUCCESS;
}


