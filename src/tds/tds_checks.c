/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2004-2011  Frediano Ziglio
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

#undef NDEBUG

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <assert.h>

#include <freetds/tds.h>
#include <freetds/convert.h>
#include <freetds/utils/string.h>
#include <freetds/checks.h>

#if ENABLE_EXTRA_CHECKS
static void
tds_check_packet_extra(const TDSPACKET * packet)
{
	assert(packet);
	for (; packet; packet = packet->next) {
		assert(tds_packet_get_data_start(packet) == 0 || tds_packet_get_data_start(packet) == sizeof(TDS72_SMP_HEADER));
		assert(packet->data_len + tds_packet_get_data_start(packet) <= packet->capacity);
	}
}

void
tds_check_tds_extra(const TDSSOCKET * tds)
{
	const int invalid_state = 0;
	TDS_UINT i;
	TDSDYNAMIC *cur_dyn = NULL;
	TDSCURSOR *cur_cursor = NULL;

	assert(tds);

	/* test state and connection */
	switch (tds->state) {
	case TDS_DEAD:
	case TDS_WRITING:
	case TDS_SENDING:
	case TDS_PENDING:
	case TDS_IDLE:
	case TDS_READING:
		break;
	default:
		assert(invalid_state);
	}

	assert(tds->conn);

#if ENABLE_ODBC_MARS
	assert(tds->sid < tds->conn->num_sessions);
	assert(tds->sid == 0 || tds->conn->mars);
	if (tds->state != TDS_DEAD)
		assert(!TDS_IS_SOCKET_INVALID(tds_get_s(tds)));
#else
	assert(tds->state == TDS_DEAD || !TDS_IS_SOCKET_INVALID(tds_get_s(tds)));
	assert(tds->state != TDS_DEAD || TDS_IS_SOCKET_INVALID(tds_get_s(tds)));
#endif

	/* test env */
	tds_check_env_extra(&tds->conn->env);

	/* test buffers and positions */
	assert(tds->send_packet != NULL);
	assert(tds->send_packet->next == NULL);
	tds_check_packet_extra(tds->send_packet);
	tds_check_packet_extra(tds->recv_packet);

#if ENABLE_ODBC_MARS
	if (tds->conn->send_packets)
		assert(tds->conn->send_pos <= tds->conn->send_packets->data_len + tds->conn->send_packets->data_start);
	if (tds->conn->recv_packet)
		assert(tds->conn->recv_pos <= tds->conn->recv_packet->data_len + tds->conn->recv_packet->data_start);
	if (tds->conn->mars)
		assert(tds->send_packet->data_start == sizeof(TDS72_SMP_HEADER));
	else
		assert(tds->send_packet->data_start == 0);
#endif

	assert(tds->in_pos <= tds->in_len);
	assert(tds->in_len <= tds->recv_packet->capacity);
	/* TODO remove blocksize from env and use out_len ?? */
/*	assert(tds->out_pos <= tds->out_len); */
/* 	assert(tds->out_len == 0 || tds->out_buf != NULL); */
	assert(tds->send_packet->capacity >= tds->out_buf_max + TDS_ADDITIONAL_SPACE);
	assert(tds->out_buf == tds->send_packet->buf + tds_packet_get_data_start(tds->send_packet));
	assert(tds->out_buf + tds->out_buf_max + TDS_ADDITIONAL_SPACE <=
		tds->send_packet->buf + tds->send_packet->capacity);
	assert(tds->out_pos <= tds->out_buf_max + TDS_ADDITIONAL_SPACE);

	assert(tds->in_buf == tds->recv_packet->buf || tds->in_buf == tds->recv_packet->buf + sizeof(TDS72_SMP_HEADER));
	assert(tds->recv_packet->capacity > 0);

	/* test res_info */
	if (tds->res_info)
		tds_check_resultinfo_extra(tds->res_info);

	/* test num_comp_info, comp_info */
	assert(tds->num_comp_info >= 0);
	for (i = 0; i < tds->num_comp_info; ++i) {
		assert(tds->comp_info);
		tds_check_resultinfo_extra(tds->comp_info[i]);
	}

	/* param_info */
	if (tds->param_info)
		tds_check_resultinfo_extra(tds->param_info);

	/* test cursors */
	for (cur_cursor = tds->conn->cursors; cur_cursor != NULL; cur_cursor = cur_cursor->next)
		tds_check_cursor_extra(cur_cursor);

	/* test dynamics */
	for (cur_dyn = tds->conn->dyns; cur_dyn != NULL; cur_dyn = cur_dyn->next)
		tds_check_dynamic_extra(cur_dyn);

	/* test tds_ctx */
	tds_check_context_extra(tds_get_ctx(tds));

	/* TODO test char_conv_count, char_convs */

	/* we can't have compute and no results */
	assert(tds->num_comp_info == 0 || tds->res_info != NULL);

	/* we can't have normal and parameters results */
	/* TODO too strict ?? */
/*	assert(tds->param_info == NULL || tds->res_info == NULL); */

	if (tds->frozen) {
		TDSPACKET *pkt;

		assert(tds->frozen_packets != NULL);
		for (pkt = tds->frozen_packets; pkt; pkt = pkt->next) {
			if (pkt->next == NULL)
				assert(pkt == tds->send_packet);
		}
	} else {
		assert(tds->frozen_packets == NULL);
	}
}

void
tds_check_context_extra(const TDSCONTEXT * ctx)
{
	assert(ctx);
}

void
tds_check_env_extra(const TDSENV * env)
{
	assert(env);

	assert(env->block_size >= 0 && env->block_size <= 65536);
}

void
tds_check_column_extra(const TDSCOLUMN * column)
{
	int size;
	TDSCONNECTION conn;
	int varint_ok;
	int column_varint_size;

	assert(column);
	column_varint_size = column->column_varint_size;

	/* 8 is for varchar(max) or similar */
	assert(column_varint_size == 8  ||
	       (column_varint_size < 5  &&  column_varint_size != 3));

	assert(column->column_scale <= column->column_prec);
	assert(column->column_prec <= MAXPRECISION);

	/* I don't like this that much... freddy77 */
	if (column->column_type == 0)
		return;
	assert(column->funcs);
	assert(column->column_type > 0);

	/* specific checks, if true fully checked */
	if (column->funcs->check(column))
		return;

	/* check type and server type same or SQLNCHAR -> SQLCHAR */
#define SPECIAL(ttype, server_type, varint) \
	if (column->column_type == ttype && column->on_server.column_type == server_type && column_varint_size == varint) {} else
	SPECIAL(SYBTEXT, XSYBVARCHAR, 8)
	SPECIAL(SYBTEXT, XSYBNVARCHAR, 8)
	SPECIAL(SYBIMAGE, XSYBVARBINARY, 8)
	assert(tds_get_cardinal_type(column->on_server.column_type, column->column_usertype) == column->column_type
		|| (tds_get_conversion_type(column->on_server.column_type, column->column_size) == column->column_type
		&& column_varint_size == 1 && is_fixed_type(column->column_type)));

	varint_ok = 0;
	if (column_varint_size == 8) {
		if (column->on_server.column_type == XSYBVARCHAR
		    || column->on_server.column_type == XSYBVARBINARY
		    || column->on_server.column_type == XSYBNVARCHAR)
			varint_ok = 1;
	} else if (is_blob_type(column->column_type)) {
		assert(column_varint_size >= 4);
	} else if (column->column_type == SYBVARIANT) {
		assert(column_varint_size == 4);
	}
	conn.tds_version = 0x500;
	varint_ok = varint_ok || tds_get_varint_size(&conn, column->on_server.column_type) == column_varint_size;
	conn.tds_version = 0x700;
	varint_ok = varint_ok || tds_get_varint_size(&conn, column->on_server.column_type) == column_varint_size;
	assert(varint_ok);

	assert(!is_numeric_type(column->column_type));
	assert(column->column_cur_size <= column->column_size);

	/* check size of fixed type correct */
	size = tds_get_size_by_type(column->column_type);
	/* these peculiar types are variable but have only a possible size */
	if ((size > 0 && (column->column_type != SYBBITN && column->column_type != SYBDATEN && column->column_type != SYBTIMEN))
	    || column->column_type == SYBVOID) {
		/* check macro */
		assert(is_fixed_type(column->column_type));
		/* check current size */
		assert(size == column->column_size);
		/* check cases where server need nullable types */
		if (column->column_type != column->on_server.column_type && (column->column_type != SYBINT8 || column->on_server.column_type != SYB5INT8)) {
			assert(!is_fixed_type(column->on_server.column_type));
			assert(column_varint_size == 1);
			assert(column->column_size == column->column_cur_size || column->column_cur_size == -1);
		} else {
			assert(column_varint_size == 0
				|| (column->column_type == SYBUNIQUE && column_varint_size == 1));
			assert(column->column_size == column->column_cur_size
				|| (column->column_type == SYBUNIQUE && column->column_cur_size == -1));
		}
		assert(column->column_size == column->on_server.column_size);
	} else {
		assert(!is_fixed_type(column->column_type));
		assert(is_char_type(column->column_type) || (column->on_server.column_size == column->column_size || column->on_server.column_size == 0));
		assert(column_varint_size != 0);
	}

	/* check size of nullable types (ie intN) it's supported */
	if (tds_get_conversion_type(column->column_type, 4) != column->column_type) {
		/* check macro */
		assert(is_nullable_type(column->column_type));
		/* check that size it's correct for this type of nullable */
		assert(tds_get_conversion_type(column->column_type, column->column_size) != column->column_type);
		/* check current size */
		assert(column->column_size >= column->column_cur_size || column->column_cur_size == -1);
		/* check same type and size on server */
		assert(column->column_type == column->on_server.column_type);
		assert(column->column_size == column->on_server.column_size);
	}
	assert(column->column_iconv_left >= 0 && column->column_iconv_left <= sizeof(column->column_iconv_buf));
}

void
tds_check_resultinfo_extra(const TDSRESULTINFO * res_info)
{
	int i;

	assert(res_info);
	assert(res_info->num_cols >= 0);
	assert(res_info->ref_count > 0);
	for (i = 0; i < res_info->num_cols; ++i) {
		assert(res_info->columns);
		tds_check_column_extra(res_info->columns[i]);
		assert(res_info->columns[i]->column_data != NULL || res_info->row_size == 0);
	}

	assert(res_info->row_size >= 0);

	assert(res_info->computeid >= 0);

	assert(res_info->by_cols >= 0);
	assert(res_info->by_cols == 0 || res_info->bycolumns);
}

void
tds_check_cursor_extra(const TDSCURSOR * cursor)
{
	assert(cursor);

	assert(cursor->ref_count > 0);

	if (cursor->res_info)
		tds_check_resultinfo_extra(cursor->res_info);
}

void
tds_check_dynamic_extra(const TDSDYNAMIC * dyn)
{
	assert(dyn);

	assert(dyn->ref_count > 0);

	if (dyn->res_info)
		tds_check_resultinfo_extra(dyn->res_info);
	if (dyn->params)
		tds_check_resultinfo_extra(dyn->params);

	assert(!dyn->emulated || dyn->query);
}

void
tds_check_freeze_extra(const TDSFREEZE * freeze)
{
	TDSPACKET *pkt;

	assert(freeze);
	assert(freeze->tds != NULL);
	assert(freeze->size_len <= 4 && freeze->size_len != 3);

	tds_check_tds_extra(freeze->tds);

	/* check position */
	if (freeze->pkt == freeze->tds->send_packet)
		assert(freeze->tds->out_pos >= freeze->pkt_pos);
	else
		assert(freeze->pkt->data_len >= freeze->pkt_pos);

	/* check packet is in list */
	for (pkt = freeze->tds->frozen_packets; ; pkt = pkt->next) {
		assert(pkt);
		if (pkt == freeze->pkt)
			break; /* found */
	}
}

void
tds_extra_assert_check(const char *fn, int line, int cond, const char *cond_str)
{
	if (cond)
		return;

	fprintf(stderr, "%s:%d: Failed checking condition '%s'\n", fn, line, cond_str);

	abort();
}

#endif /* ENABLE_EXTRA_CHECKS */
