/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008  Frediano Ziglio
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include <assert.h>

#include "tds.h"
#include "tds_checks.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

TDS_RCSID(var, "$Id: bulk.c,v 1.3 2008-12-15 13:21:45 freddy77 Exp $");

typedef struct tds_pbcb
{
	char *pb;
	unsigned int cb;
	unsigned int from_malloc;
} TDSPBCB;

static int tds_bcp_add_fixed_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset, unsigned char * rowbuffer, int start);
static int tds_bcp_add_variable_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset, TDS_UCHAR *rowbuffer, int start, int *pncols);

/** 
 * \return TDS_SUCCEED or TDS_FAIL.
 */
static int
tds_build_bulk_insert_stmt(TDSSOCKET * tds, TDSPBCB * clause, TDSCOLUMN * bcpcol, int first)
{
	char buffer[32];
	char *column_type = buffer;

	tdsdump_log(TDS_DBG_FUNC, "tds_build_bulk_insert_stmt(%p, %p, %p, %d)\n", tds, clause, bcpcol, first);

	/* TODO reuse function in tds/query.c */
	switch (bcpcol->on_server.column_type) {
	case SYBINT1:
		column_type = "tinyint";
		break;
	case SYBBIT:
		column_type = "bit";
		break;
	case SYBINT2:
		column_type = "smallint";
		break;
	case SYBINT4:
		column_type = "int";
		break;
	case SYBINT8:
		column_type = "bigint";
		break;
	case SYBDATETIME:
		column_type = "datetime";
		break;
	case SYBDATETIME4:
		column_type = "smalldatetime";
		break;
	case SYBREAL:
		column_type = "real";
		break;
	case SYBMONEY:
		column_type = "money";
		break;
	case SYBMONEY4:
		column_type = "smallmoney";
		break;
	case SYBFLT8:
		column_type = "float";
		break;

	case SYBINTN:
		switch (bcpcol->column_size) {
		case 1:
			column_type = "tinyint";
			break;
		case 2:
			column_type = "smallint";
			break;
		case 4:
			column_type = "int";
			break;
		case 8:
			column_type = "bigint";
			break;
		}
		break;

	case SYBBITN:
		column_type = "bit";
		break;
	case SYBFLTN:
		switch (bcpcol->column_size) {
		case 4:
			column_type = "real";
			break;
		case 8:
			column_type = "float";
			break;
		}
		break;
	case SYBMONEYN:
		switch (bcpcol->column_size) {
		case 4:
			column_type = "smallmoney";
			break;
		case 8:
			column_type = "money";
			break;
		}
		break;
	case SYBDATETIMN:
		switch (bcpcol->column_size) {
		case 4:
			column_type = "smalldatetime";
			break;
		case 8:
			column_type = "datetime";
			break;
		}
		break;
	case SYBDECIMAL:
		sprintf(column_type, "decimal(%d,%d)", bcpcol->column_prec, bcpcol->column_scale);
		break;
	case SYBNUMERIC:
		sprintf(column_type, "numeric(%d,%d)", bcpcol->column_prec, bcpcol->column_scale);
		break;

	case XSYBVARBINARY:
		sprintf(column_type, "varbinary(%d)", bcpcol->column_size);
		break;
	case XSYBVARCHAR:
		sprintf(column_type, "varchar(%d)", bcpcol->column_size);
		break;
	case XSYBBINARY:
		sprintf(column_type, "binary(%d)", bcpcol->column_size);
		break;
	case XSYBCHAR:
		sprintf(column_type, "char(%d)", bcpcol->column_size);
		break;
	case SYBTEXT:
		sprintf(column_type, "text");
		break;
	case SYBIMAGE:
		sprintf(column_type, "image");
		break;
	case XSYBNVARCHAR:
		sprintf(column_type, "nvarchar(%d)", bcpcol->column_size);
		break;
	case XSYBNCHAR:
		sprintf(column_type, "nchar(%d)", bcpcol->column_size);
		break;
	case SYBNTEXT:
		sprintf(column_type, "ntext");
		break;
	case SYBUNIQUE:
		sprintf(column_type, "uniqueidentifier  ");
		break;
	default:
		tdserror(tds->tds_ctx, tds, TDSEBPROBADTYP, errno);
		tdsdump_log(TDS_DBG_FUNC, "error: cannot build bulk insert statement. unrecognized server datatype %d\n",
			    bcpcol->on_server.column_type);
		return TDS_FAIL;
	}

	if (clause->cb < strlen(clause->pb)
	    + tds_quote_id(tds, NULL, bcpcol->column_name, bcpcol->column_namelen)
	    + strlen(column_type)
	    + ((first) ? 2u : 4u)) {
		char *temp = malloc(2 * clause->cb);

		if (!temp) {
			tdserror(tds->tds_ctx, tds, TDSEMEM, errno);
			return TDS_FAIL;
		}
		strcpy(temp, clause->pb);
		if (clause->from_malloc)
			free(clause->pb);
		clause->from_malloc = 1;
		clause->pb = temp;
		clause->cb *= 2;
	}

	if (!first)
		strcat(clause->pb, ", ");

	tds_quote_id(tds, strchr(clause->pb, 0), bcpcol->column_name, bcpcol->column_namelen);
	strcat(clause->pb, " ");
	strcat(clause->pb, column_type);

	return TDS_SUCCEED;
}

int
tds_bcp_start_insert_stmt(TDSSOCKET * tds, TDSBCPINFO * bcpinfo)
{
	char *query;

	if (IS_TDS7_PLUS(tds)) {
		int i, firstcol, erc;
		char *hint;
		TDSCOLUMN *bcpcol;
		TDSPBCB colclause;
		char clause_buffer[4096] = { 0 };

		colclause.pb = clause_buffer;
		colclause.cb = sizeof(clause_buffer);
		colclause.from_malloc = 0;

		firstcol = 1;

		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
			bcpcol = bcpinfo->bindinfo->columns[i];

			if (bcpinfo->identity_insert_on) {
				if (!bcpcol->column_timestamp) {
					tds_build_bulk_insert_stmt(tds, &colclause, bcpcol, firstcol);
					firstcol = 0;
				}
			} else {
				if (!bcpcol->column_identity && !bcpcol->column_timestamp) {
					tds_build_bulk_insert_stmt(tds, &colclause, bcpcol, firstcol);
					firstcol = 0;
				}
			}
		}

		if (bcpinfo->hint) {
			if (asprintf(&hint, " with (%s)", bcpinfo->hint) < 0)
				hint = NULL;
		} else {
			hint = strdup("");
		}
		if (!hint) {
			if (colclause.from_malloc)
				TDS_ZERO_FREE(colclause.pb);
			return TDS_FAIL;
		}

		erc = asprintf(&query, "insert bulk %s (%s)%s", bcpinfo->tablename, colclause.pb, hint);

		free(hint);
		if (colclause.from_malloc)
			TDS_ZERO_FREE(colclause.pb);	/* just for good measure; not used beyond this point */

		if (erc < 0)
			return TDS_FAIL;
	} else {
		/* NOTE: if we use "with nodescribe" for following inserts server do not send describe */
		if (asprintf(&query, "insert bulk %s", bcpinfo->tablename) < 0)
			return TDS_FAIL;
	}

	tds_submit_query(tds, query);
	/* save the statement for later... */

	bcpinfo->insert_stmt = query;

	return TDS_SUCCEED;
}

/** 
 * \return TDS_SUCCEED or TDS_FAIL.
 */
int
tds_bcp_send_record(TDSSOCKET *tds, TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset)
{
	TDSCOLUMN  *bindcol;

	static const TDS_TINYINT textptr_size = 16;
	static const unsigned char GEN_NULL = 0x00;

	static const unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
	static const unsigned char textptr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
											 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	static const unsigned char timestamp[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	unsigned char *record;
	TDS_INT	 old_record_size;
	TDS_INT	 new_record_size;
	TDS_INT	     varint_4;
	TDS_SMALLINT varint_2;
	TDS_TINYINT  varint_1;

	int row_pos;
	int row_sz_pos;
	TDS_SMALLINT row_size;
	int blob_cols = 0;
	int var_cols_written = 0;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_send_bcp_record(offset %d)\n", offset);

	record = bcpinfo->bindinfo->current_row;
	old_record_size = bcpinfo->bindinfo->row_size;
	new_record_size = 0;

	if (IS_TDS7_PLUS(tds)) {

		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
	
			bindcol = bcpinfo->bindinfo->columns[i];

			/*
			 * Don't send the (meta)data for timestamp columns or
			 * identity columns unless indentity_insert is enabled.
			 */

			if ((!bcpinfo->identity_insert_on && bindcol->column_identity) || 
				bindcol->column_timestamp) {
				continue;
			}

			if ((get_col_data(bcpinfo, bindcol, offset)) != TDS_SUCCEED) {
				tdsdump_log(TDS_DBG_INFO1, "get_col_data (column %d) failed\n", i + 1);
	 			return TDS_FAIL;
			}
			tdsdump_log(TDS_DBG_INFO1, "gotten column %d length %d null %d\n",
					i + 1, bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->is_null);
	
			if (bindcol->bcp_column_data->is_null) {
				if (bindcol->column_nullable) {
					switch (bindcol->on_server.column_type) {
					case XSYBCHAR:
					case XSYBVARCHAR:
					case XSYBBINARY:
					case XSYBVARBINARY:
					case XSYBNCHAR:
					case XSYBNVARCHAR:
						memcpy(record, CHARBIN_NULL, 2);
						record +=2;
						new_record_size +=2;
						break;
					default:
						*record = GEN_NULL;
						record++;
						new_record_size ++;
						break;
					}
				} else {
					/* No value or default value available and NULL not allowed. */
					null_error(bcpinfo, i, offset);
					return TDS_FAIL;
				}
			} else {

				switch (bindcol->column_varint_size) {
				case 4:
					if (is_blob_type(bindcol->on_server.column_type)) {
						*record = textptr_size; record++;
						memcpy(record, textptr, 16); record += 16;
						memcpy(record, timestamp, 8); record += 8;
						new_record_size += 25;
					}
					varint_4 = bindcol->bcp_column_data->datalen;
#if WORDS_BIGENDIAN
					tds_swap_datatype(SYBINT4, (unsigned char *)&varint_4);
#endif
					memcpy(record, &varint_4, 4); record += 4; new_record_size +=4;
					break;
				case 2:
					varint_2 = bindcol->bcp_column_data->datalen;
#if WORDS_BIGENDIAN
					tds_swap_datatype(SYBINT2, (unsigned char *)&varint_2);
#endif
					memcpy(record, &varint_2, 2); record += 2; new_record_size +=2;
					break;
				case 1:
					varint_1 = bindcol->bcp_column_data->datalen;
					if (is_numeric_type(bindcol->on_server.column_type)) {
						varint_1 = tds_numeric_bytes_per_prec[bindcol->column_prec];
						tdsdump_log(TDS_DBG_INFO1, "numeric type prec = %d varint_1 = %d\n",
										 bindcol->column_prec, varint_1);
					} else {
						varint_1 = bindcol->bcp_column_data->datalen;
						tdsdump_log(TDS_DBG_INFO1, "varint_1 = %d\n", varint_1);
					}
					*record = varint_1; record++; new_record_size++;
					break;
				case 0:
					break;
				}

				tdsdump_log(TDS_DBG_INFO1, "new_record_size = %d datalen = %d \n", 
							new_record_size, bindcol->bcp_column_data->datalen);

#if WORDS_BIGENDIAN
				tds_swap_datatype(tds_get_conversion_type(bindcol->column_type, bindcol->bcp_column_data->datalen),
									bindcol->bcp_column_data->data);
#endif
				if (is_numeric_type(bindcol->on_server.column_type)) {
					TDS_NUMERIC *num = (TDS_NUMERIC *) bindcol->bcp_column_data->data;
					int size;
					tdsdump_log(TDS_DBG_INFO1, "numeric type prec = %d\n", num->precision);
					if (IS_TDS7_PLUS(tds))
						tds_swap_numeric(num);
					size = tds_numeric_bytes_per_prec[num->precision];
					memcpy(record, num->array, size);
					record += size; 
					new_record_size += size;
				} else {
					memcpy(record, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
					record += bindcol->bcp_column_data->datalen;
					new_record_size += bindcol->bcp_column_data->datalen;
				}

			}
			tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n",
					old_record_size, new_record_size);
		}

		tds_put_byte(tds, TDS_ROW_TOKEN);   /* 0xd1 */
		tds_put_n(tds, bcpinfo->bindinfo->current_row, new_record_size);
	}  /* IS_TDS7_PLUS */
	else {
		memset(record, '\0', old_record_size);	/* zero the rowbuffer */

		/*
		 * offset 0 = number of var columns
		 * offset 1 = row number.  zeroed (datasever assigns)
		 */
		row_pos = 2;

		if ((row_pos = tds_bcp_add_fixed_columns(bcpinfo, get_col_data, null_error, offset, record, row_pos)) == TDS_FAIL)
			return TDS_FAIL;

		row_sz_pos = row_pos;

		/* potential variable columns to write */

		if (bcpinfo->var_cols) {
			if ((row_pos = tds_bcp_add_variable_columns(bcpinfo, get_col_data, null_error, offset, record, row_pos, &var_cols_written)) == TDS_FAIL)
				return TDS_FAIL;
		}

		row_size = row_pos;

		if (var_cols_written) {
			memcpy(&record[row_sz_pos], &row_size, sizeof(row_size));
			record[0] = var_cols_written;
		}

		tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n", old_record_size, row_size);

		tds_put_smallint(tds, row_size);
		tds_put_n(tds, record, row_size);

		/* row is done, now handle any text/image data */

		blob_cols = 0;

		for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
			bindcol = bcpinfo->bindinfo->columns[i];
			if (is_blob_type(bindcol->column_type)) {
				if ((get_col_data(bcpinfo, bindcol, offset)) != TDS_SUCCEED) {
					return TDS_FAIL;
				}
				/* unknown but zero */
				tds_put_smallint(tds, 0);
				tds_put_byte(tds, bindcol->column_type);
				tds_put_byte(tds, 0xff - blob_cols);
				/*
				 * offset of txptr we stashed during variable
				 * column processing 
				 */
				tds_put_smallint(tds, bindcol->column_textpos);
				tds_put_int(tds, bindcol->bcp_column_data->datalen);
				tds_put_n(tds, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
				blob_cols++;

			}
		}
	}

	return TDS_SUCCEED;
}

/**
 * Add fixed size columns to the row
 */
static int
tds_bcp_add_fixed_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset, unsigned char * rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	TDSCOLUMN *bcpcol;
	int cpbytes;
	int i, j;

	assert(bcpinfo);
	assert(rowbuffer);

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_add_fixed_columns(%p, %p, %p, %d, %p, %d)\n", bcpinfo, get_col_data, null_error, offset, rowbuffer, start);

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {

		bcpcol = bcpinfo->bindinfo->columns[i];

		if (!is_nullable_type(bcpcol->column_type) && !(bcpcol->column_nullable)) {

			tdsdump_log(TDS_DBG_FUNC, "tds_bcp_add_fixed_columns column %d is a fixed column\n", i + 1);

			if ((get_col_data(bcpinfo, bcpcol, offset)) != TDS_SUCCEED) {
				tdsdump_log(TDS_DBG_INFO1, "get_col_data (column %d) failed\n", i + 1);
		 		return TDS_FAIL;
			}

			if (bcpcol->bcp_column_data->is_null) {
				/* No value or default value available and NULL not allowed. */
				null_error(bcpinfo, i, offset);
				return TDS_FAIL;
			}

			if (is_numeric_type(bcpcol->column_type)) {
				num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
				cpbytes = tds_numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
			} else {
				cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ?
					  bcpcol->column_size : bcpcol->bcp_column_data->datalen;
				memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);

				/* CHAR data may need padding out to the database length with blanks */

				if (bcpcol->column_type == SYBCHAR && cpbytes < bcpcol->column_size) {
					for (j = cpbytes; j <  bcpcol->column_size; j++)
						rowbuffer[row_pos + j] = ' ';

				}
			}

			row_pos += bcpcol->column_size;
		}
	}
	return row_pos;
}

/**
 * Add variable size columns to the row
 *
 * \param rowbuffer The row image that will be sent to the server. 
 * \param start Where to begin copying data into the rowbuffer. 
 * \param pncols Address of output variable holding the count of columns added to the rowbuffer.  
 * 
 * \return length of (potentially modified) rowbuffer, or TDS_FAIL.
 */
static int
tds_bcp_add_variable_columns(TDSBCPINFO *bcpinfo, tds_bcp_get_col_data get_col_data, tds_bcp_null_error null_error, int offset, TDS_UCHAR* rowbuffer, int start, int *pncols)
{
	TDS_SMALLINT offsets[256];
	int i, row_pos;
	int ncols = 0;

	assert(bcpinfo);
	assert(rowbuffer);
	assert(pncols);

	tdsdump_log(TDS_DBG_FUNC, "%4s %8s %18s %18s %8s\n", 	"col", 
								"type", 
								"is_nullable_type", 
								"column_nullable", 
								"is null" );
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		TDSCOLUMN *bcpcol = bcpinfo->bindinfo->columns[i];
		tdsdump_log(TDS_DBG_FUNC, "%4d %8d %18s %18s %8s\n", 	i, 
									bcpcol->column_type,  
									is_nullable_type(bcpcol->column_type)? "yes" : "no", 
									bcpcol->column_nullable? "yes" : "no", 
									bcpcol->bcp_column_data->is_null? "yes" : "no" );
	}

	/* the first two bytes of the rowbuffer are reserved to hold the entire record length */
	row_pos = start + 2;
	offsets[0] = row_pos;

	tdsdump_log(TDS_DBG_FUNC, "%4s %8s %8s %8s\n", "col", "ncols", "row_pos", "cpbytes");

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		int cpbytes = 0;
		TDSCOLUMN *bcpcol = bcpinfo->bindinfo->columns[i];

		/*
		 * Is this column of "variable" type, i.e. NULLable
		 * or naturally variable length e.g. VARCHAR
		 */
		if (is_nullable_type(bcpcol->column_type) || bcpcol->column_nullable) {

			tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d %8d\n", i, ncols, row_pos, cpbytes);

			if ((get_col_data(bcpinfo, bcpcol, offset)) != TDS_SUCCEED)
		 		return TDS_FAIL;

			/* If it's a NOT NULL column, and we have no data, throw an error. */
			if (!(bcpcol->column_nullable) && bcpcol->bcp_column_data->is_null) {
				/* No value or default value available and NULL not allowed. */
				null_error(bcpinfo, i, offset);
				return TDS_FAIL;
			}

			/* move the column buffer into the rowbuffer */
			if (!bcpcol->bcp_column_data->is_null) {
				if (is_blob_type(bcpcol->column_type)) {
					cpbytes = 16;
					bcpcol->column_textpos = row_pos;               /* save for data write */
				} else if (is_numeric_type(bcpcol->column_type)) {
						TDS_NUMERIC *num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
					cpbytes = tds_numeric_bytes_per_prec[num->precision];
					memcpy(&rowbuffer[row_pos], num->array, cpbytes);
				} else {
					cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ?
					bcpcol->column_size : bcpcol->bcp_column_data->datalen;
					memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
				}
			}

			row_pos += cpbytes;
			offsets[++ncols] = row_pos;
			tdsdump_dump_buf(TDS_DBG_NETWORK, "BCP row buffer so far", rowbuffer,  row_pos);
		}
	}

	tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d\n", i, ncols, row_pos);

	/*
	 * The rowbuffer ends with an offset table and, optionally, an adjustment table.  
	 * The offset table has 1-byte elements that describe the locations of the start of each column in
	 * the rowbuffer.  If the largest offset is greater than 255, another table -- the adjustment table --
	 * is inserted just before the offset table.  It holds the high bytes. 
	 * 
	 * Both tables are laid out in reverse:
	 * 	#elements, offset N+1, offset N, offset N-1, ... offset 0
	 * E.g. for 2 columns you have 4 data points:
	 *	1.  How many elements (4)
	 *	2.  Start of column 3 (non-existent, "one off the end")
	 *	3.  Start of column 2
	 *	4.  Start of column 1
	 *  The length of each column is computed by subtracting its start from the its successor's start. 
	 *
	 * The algorithm below computes both tables. If the adjustment table isn't needed, the 
	 * effect is to overwrite it with the offset table.  
	 */
	while (ncols && offsets[ncols] == offsets[ncols-1])
		ncols--;	/* trailing NULL columns are not sent and are not included in the offset table */

	if (ncols) {
		TDS_UCHAR *padj = rowbuffer + row_pos;
		TDS_UCHAR *poff = offsets[ncols] > 0xFF? padj + ncols + 1 : padj;

		*padj++ = 1 + ncols;
		*poff++ = 1 + ncols;
		
		for (i=0; i <= ncols; i++) {
			padj[i] = offsets[ncols-i] >> 8;
			poff[i] = offsets[ncols-i] & 0xFF;
		}
		row_pos = poff + ncols + 1 - rowbuffer;
	}

	tdsdump_log(TDS_DBG_FUNC, "%4d %8d %8d\n", i, ncols, row_pos);
	tdsdump_dump_buf(TDS_DBG_NETWORK, "BCP row buffer", rowbuffer,  row_pos);

	*pncols = ncols;

	return ncols == 0? start : row_pos;
}

/** 
 * \return TDS_SUCCEED or TDS_FAIL.
 */
int
tds_bcp_send_colmetadata(TDSSOCKET *tds, TDSBCPINFO *bcpinfo)
{
	const static unsigned char colmetadata_token = 0x81;
	TDSCOLUMN *bcpcol;
	int i, num_cols;

	tdsdump_log(TDS_DBG_FUNC, "tds_bcp_send_colmetadata(%p, %p)\n", tds, bcpinfo);
	assert(tds && bcpinfo);

	/* 
	 * Deep joy! For TDS 8 we have to send a colmetadata message followed by row data 
	 */
	tds_put_byte(tds, colmetadata_token);	/* 0x81 */

	num_cols = 0;
	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		bcpcol = bcpinfo->bindinfo->columns[i];
		if ((!bcpinfo->identity_insert_on && bcpcol->column_identity) || 
			bcpcol->column_timestamp) {
			continue;
		}
		num_cols++;
	}

	tds_put_smallint(tds, num_cols);

	for (i = 0; i < bcpinfo->bindinfo->num_cols; i++) {
		bcpcol = bcpinfo->bindinfo->columns[i];

		/*
		 * dont send the (meta)data for timestamp columns, or
		 * identity columns (unless indentity_insert is enabled
		 */

		if ((!bcpinfo->identity_insert_on && bcpcol->column_identity) || 
			bcpcol->column_timestamp) {
			continue;
		}

		if (IS_TDS90(tds))
			tds_put_int(tds, bcpcol->column_usertype);
		else
			tds_put_smallint(tds, bcpcol->column_usertype);
		tds_put_smallint(tds, bcpcol->column_flags);
		tds_put_byte(tds, bcpcol->on_server.column_type);

		switch (bcpcol->column_varint_size) {
		case 4:
			tds_put_int(tds, bcpcol->column_size);
			break;
		case 2:
			tds_put_smallint(tds, bcpcol->column_size);
			break;
		case 1:
			tds_put_byte(tds, bcpcol->column_size);
			break;
		case 0:
			break;
		default:
			break;
		}

		if (is_numeric_type(bcpcol->on_server.column_type)) {
			tds_put_byte(tds, bcpcol->column_prec);
			tds_put_byte(tds, bcpcol->column_scale);
		}
		if (IS_TDS8_PLUS(tds)
			&& is_collate_type(bcpcol->on_server.column_type)) {
			tds_put_n(tds, bcpcol->column_collation, 5);
		}
		if (is_blob_type(bcpcol->on_server.column_type)) {
			/* FIXME strlen return len in bytes not in characters required here */
			tds_put_smallint(tds, strlen(bcpinfo->tablename));
			tds_put_string(tds, bcpinfo->tablename, strlen(bcpinfo->tablename));
		}
		/* FIXME support multibyte string */
		tds_put_byte(tds, bcpcol->column_namelen);
		tds_put_string(tds, bcpcol->column_name, bcpcol->column_namelen);

	}

	return TDS_SUCCEED;
}
