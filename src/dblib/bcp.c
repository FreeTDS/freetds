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
#include "tdsutil.h"
#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "dblib.h"
#include <unistd.h>


static char  software_version[]   = "$Id: bcp.c,v 1.2 2001-10-24 23:19:44 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};


RETCODE BCP_SETL(LOGINREC *login, DBBOOL enable)
{
	tds_set_bulk(login->tds_login, enable);
	return SUCCEED;
}
RETCODE bcp_init(DBPROCESS *dbproc, char *tblname, char *hfile, char *errfile, int direction)
{
	dbproc->bcp_hostfile = (char *) malloc(strlen(hfile)+1);
	strcpy(dbproc->bcp_hostfile, hfile);
	dbproc->bcp_errorfile = (char *) malloc(strlen(errfile)+1);
	strcpy(dbproc->bcp_errorfile, errfile);
	dbproc->bcp_tablename = (char *) malloc(strlen(tblname)+1);
	strcpy(dbproc->bcp_tablename, tblname);
	dbproc->bcp_direction = direction;
	return SUCCEED;
}
RETCODE bcp_collen(DBPROCESS *dbproc, DBINT varlen, int table_column)
{
	return SUCCEED;
}
RETCODE bcp_columns(DBPROCESS *dbproc, int host_colcount)
{
int i;
	if (dbproc->bcp_columns) {
		for (i=0;i<dbproc->bcp_colcount;i++) {
			if (dbproc->bcp_columns[i]->terminator)
				free(dbproc->bcp_columns[i]->terminator);
			free(dbproc->bcp_columns[i]);
		}
		free(dbproc->bcp_columns);
	}
	dbproc->bcp_colcount = host_colcount;
	dbproc->bcp_columns = (BCP_COLINFO **) malloc(host_colcount * sizeof(BCP_COLINFO *));
	for (i=0;i<dbproc->bcp_colcount;i++) {
		dbproc->bcp_columns[i] = (BCP_COLINFO *) malloc(sizeof(BCP_COLINFO));
		memset(dbproc->bcp_columns[i], '\0', sizeof(BCP_COLINFO));
	}

	return SUCCEED;
}
RETCODE bcp_colfmt(DBPROCESS *dbproc, int host_colnum, int host_type, int host_prefixlen, DBINT host_collen, BYTE *host_term,int host_termlen, int table_colnum)
{
BCP_COLINFO *bcpcol;

	if (host_colnum < 1 || host_colnum > dbproc->bcp_colcount) 
		return FAIL;

	bcpcol = dbproc->bcp_columns[host_colnum - 1];
	bcpcol->datatype = host_type;
	bcpcol->prefix_len = host_prefixlen;
	bcpcol->column_len = host_collen;
	bcpcol->terminator = (BYTE *) malloc(host_termlen+1);
	memcpy(bcpcol->terminator, host_term, host_termlen);
	bcpcol->term_len = host_termlen;
	bcpcol->column = table_colnum;

	return SUCCEED;
}
RETCODE bcp_colfmt_ps(DBPROCESS *dbproc, int host_colnum, int host_type, int host_prefixlen, DBINT host_collen, BYTE *host_term,int host_termlen, int table_colnum, DBTYPEINFO *typeinfo)
{
	return SUCCEED;
}
RETCODE bcp_control(DBPROCESS *dbproc, int field, DBINT value)
{
	return SUCCEED;
}
RETCODE bcp_colptr(DBPROCESS *dbproc, BYTE *colptr, int table_column)
{
	return SUCCEED;
}
DBBOOL bcp_getl(LOGINREC *login)
{
	return SUCCEED;
}
static RETCODE _bcp_exec_out(DBPROCESS *dbproc, DBINT *rows_copied)
{
char query[256];
int ret;
int i;
TDSSOCKET *tds = dbproc->tds_socket;
TDSRESULTINFO *resinfo;
BCP_COLINFO *bcpcol;
TDSCOLINFO *curcol;
BYTE *src;
int srctype;
BYTE dest[256];
long len;
FILE *hostfile;
TDS_TINYINT ti;
TDS_SMALLINT si;
TDS_INT li;


	if (! (hostfile = fopen(dbproc->bcp_hostfile, "w"))) {
		return FAIL;
	}

	sprintf(query,"select * from %s", dbproc->bcp_tablename);
	tds_submit_query(tds,query);

	if (tds_process_result_tokens(tds) == TDS_FAIL) {
		fclose(hostfile);
		return FAIL;
	}
	if (!tds->res_info) {
		fclose(hostfile);
		return FAIL;
	}

	resinfo=tds->res_info;

	while (tds_process_row_tokens(tds)==TDS_SUCCEED) {
		for (i=0;i<dbproc->bcp_colcount;i++) {
			bcpcol=dbproc->bcp_columns[i];
			curcol=resinfo->columns[bcpcol->column - 1];
			if (is_blob_type(curcol->column_type)) {
				/* FIX ME -- no converts supported */
				src = curcol->column_textvalue;
				len = curcol->column_textsize;
			} else {
				src = &resinfo->current_row[curcol->column_offset];

				srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);
				len = tds_convert(srctype, 
					src, curcol->column_size, 
					bcpcol->datatype, dest, 255);
			}

			/* FIX ME -- does not handle prefix_len == -1 */
			/* The prefix */
			switch (bcpcol->prefix_len) {
				case 1:
					ti = len;
					fwrite(&ti,sizeof(ti),1,hostfile);
					break;
				case 2:
					si = len;
					fwrite(&si,sizeof(si),1,hostfile);
					break;
				case 4:
					li = len;
					fwrite(&li,sizeof(li),1,hostfile);
					break;
			}

			/* The data */
			if (is_blob_type(curcol->column_type)) {
				fwrite(src,len,1,hostfile);
			} else {
				if (bcpcol->column_len != -1) {
					len = len > bcpcol->column_len ?
						bcpcol->column_len : len;
				}
				fwrite(dest,len,1,hostfile);
			}

			/* The terminator */	
			if (bcpcol->terminator && bcpcol->term_len > 0) {
				fwrite(bcpcol->terminator,
					bcpcol->term_len, 1, hostfile);
			}
		}
	}
	fclose(hostfile);
	*rows_copied = resinfo->row_count;
	return SUCCEED;
}

/*
** _bcp_start_copy() sends the 'bulk insert' command, processes the result
** set, and stores results to the BCP_COLINFO structure
*/
static RETCODE _bcp_start_copy(DBPROCESS *dbproc)
{
TDSSOCKET *tds = dbproc->tds_socket;
TDSRESULTINFO *resinfo;
BCP_COLINFO *bcpcol;
TDSCOLINFO *curcol;
int colid;
int i;
char query[256];
int marker;

if (IS_TDS42(tds)) {
	sprintf(query,"select * from %s where 0=1",dbproc->bcp_tablename);
	tds_submit_query(tds,query);

	if (tds_process_result_tokens(tds) == TDS_FAIL) {
		return FAIL;
	}
	if (!tds->res_info) {
		return FAIL;
	}
	resinfo = tds->res_info;

	for (i=0;i<resinfo->num_cols;i++) {
		/* FIX ME -- assumes file and table in same order */
		curcol = resinfo->columns[i];
		bcpcol=dbproc->bcp_columns[i];

		bcpcol->db_type = curcol->column_type;
		bcpcol->db_length = curcol->column_size;
	}
} 
	sprintf(query,"insert bulk %s",dbproc->bcp_tablename);
	tds_submit_query(tds,query);

if (IS_TDS50(tds)) {

	if (tds_process_result_tokens(tds) == TDS_FAIL) {
		return FAIL;
	}
	if (!tds->res_info) {
		return FAIL;
	}

	resinfo = tds->res_info;
	while (tds_process_row_tokens(tds) == SUCCEED) {
		curcol = resinfo->columns[4];
		colid = resinfo->current_row[curcol->column_offset];
		bcpcol = NULL;
		for (i=0;i<dbproc->bcp_colcount;i++) {
			if (dbproc->bcp_columns[i]->column == colid)
				bcpcol=dbproc->bcp_columns[i];
		}
		if (!bcpcol) {
			fprintf(stderr,"Error: bcp_colfmt not called for database column %d\n",colid); 
		} else {
			/* db_type */
			curcol = resinfo->columns[5];
			bcpcol->db_type = resinfo->current_row[curcol->column_offset];
			/* db_length */
			curcol = resinfo->columns[6];
			memcpy(&bcpcol->db_length,
				&resinfo->current_row[curcol->column_offset],4);
			/* db_offset */
			curcol = resinfo->columns[8];
			memcpy(&bcpcol->db_offset,
				&resinfo->current_row[curcol->column_offset],2);
		}
	}
} else {
	marker = tds_get_byte(tds);
	tds_process_default_tokens(tds,marker);
	if (!is_end_token(marker)) {
		return FAIL;
	}
}
	return SUCCEED;

}

RETCODE _bcp_read_hostfile(DBPROCESS *dbproc, FILE *hostfile)
{
TDSSOCKET *tds = dbproc->tds_socket;
TDSRESULTINFO *resinfo;
BCP_COLINFO *bcpcol;
TDSCOLINFO *curcol;
int i;
TDS_TINYINT ti;
TDS_SMALLINT si;
TDS_INT li;
int collen;
int bytes_read;

	for (i=0;i<dbproc->bcp_colcount;i++) {
		bcpcol = dbproc->bcp_columns[i];
		collen = 0;
		/* FIX ME -- handle the prefix_len == -1 case */
		/* Prefix */
		if (bcpcol->prefix_len) {
			switch(bcpcol->prefix_len) {
				case 1:
					fread(&ti, 1, 1, hostfile);
					collen = ti;
					break;
				case 2:
					fread(&si, 1, 1, hostfile);
					collen = si;
					break;
				case 4:
					fread(&li, 1, 1, hostfile);
					collen = li;
					break;
			}
		}
		/* FIX ME -- handle null case */
		/* Column Length */
		if (bcpcol->column_len>0)
			collen = bcpcol->column_len;

		/* Fixed Length data */
		if (!collen && is_fixed_type(bcpcol->datatype)) {
			collen = get_size_by_type(bcpcol->datatype);
		}

		if (bcpcol->data) {
			free(bcpcol->data);
		}
		bcpcol->data_size = collen;
		bcpcol->data = (BYTE *) malloc(collen + 1);
		if (collen) {
			bytes_read = fread(bcpcol->data,collen,1,hostfile);
			if (!bytes_read)
				return FAIL;
		} else {
			bcpcol->data[0]='\0'; /* debugging */
		}
	}
	return SUCCEED;
}

/*
** Add fixed size columns to the row
*/
static int _bcp_add_fixed_columns(DBPROCESS *dbproc, BYTE *rowbuffer, int start)
{
int row_pos = start;
BCP_COLINFO *bcpcol;
int i;
int cpbytes;

	for(i=0;i<dbproc->bcp_colcount;i++) {
		/* FIX ME -- assumes number and order of columns 
		** in file matches that of the database */
		bcpcol	= dbproc->bcp_columns[i];
		if (!is_nullable_type(bcpcol->db_type)) {
			/* compute the length to copy to the row 
			** buffer */
			cpbytes = bcpcol->data_size > bcpcol->db_length 
				? bcpcol->db_length : bcpcol->data_size;
			memcpy(&rowbuffer[row_pos],bcpcol->data,cpbytes);

			if (row_pos != bcpcol->db_offset) {
				fprintf(stderr,"Error: computed offset does not match one returned from database engine\n");
			}
			row_pos += bcpcol->db_length;
		}
	}
	return row_pos;
}

/*
** Add variable size columns to the row
*/
static int _bcp_add_variable_columns(DBPROCESS *dbproc, BYTE *rowbuffer, int start)
{
int row_pos = start;
BCP_COLINFO *bcpcol;
int i;
int cpbytes;
int eod_ptr; 
BYTE offset_table[256];
int offset_pos = 0;
BYTE adjust_table[256];
int adjust_pos = 0;
int num_cols = 0;

	for(i=0;i<dbproc->bcp_colcount;i++) {
		/* FIX ME -- see fixed columns */
		bcpcol	= dbproc->bcp_columns[i];
		if (is_nullable_type(bcpcol->db_type)) {
			if (is_blob_type(bcpcol->db_type)) {
				/* no need to copy they are all zero bytes */
				cpbytes = 16;
				/* save for data write */
				bcpcol->txptr_offset = row_pos;
			} else {
				/* compute the length to copy to the row 
				** buffer */
				cpbytes = bcpcol->data_size > bcpcol->db_length 
					? bcpcol->db_length : bcpcol->data_size;
				memcpy(&rowbuffer[row_pos],bcpcol->data,cpbytes);
			}

			/* update offset and adjust tables (if necessary) */
			offset_table[offset_pos++] = row_pos % 256;
			if (row_pos > 255 && 
			 (adjust_pos==0 || 
			 row_pos/256 != adjust_table[adjust_pos])) {
				adjust_table[adjust_pos++]=row_pos / 256;
			}

			num_cols++;
			row_pos += cpbytes;
		}
	}
	eod_ptr = row_pos;

	/* write the marker */
	rowbuffer[row_pos++]=num_cols + 1;

	/* write the adjust table (right to left) */
	for (i=adjust_pos-1;i>=0;i--) {
		fprintf(stderr,"adjust %d\n",adjust_table[i]);
		rowbuffer[row_pos++]=adjust_table[i];
	}

	/* the EOD (end of data) pointer */
	rowbuffer[row_pos++]=eod_ptr;
	
	/* write the offset table (right to left) */
	for (i=offset_pos-1;i>=0;i--) {
		fprintf(stderr,"offset %d\n",offset_table[i]);
		rowbuffer[row_pos++]=offset_table[i];
	}

	return row_pos;
}
static RETCODE _bcp_exec_in(DBPROCESS *dbproc, DBINT *rows_copied)
{
FILE *hostfile;
TDSSOCKET *tds = dbproc->tds_socket;
TDSRESULTINFO *resinfo;
BCP_COLINFO *bcpcol;
TDSCOLINFO *curcol;
int i;
int collen;
/* FIX ME -- calculate dynamically */
unsigned char rowbuffer[32768];
int row_pos;
/* end of data pointer...the last byte of var data before the adjust table */
int var_cols;
int row_sz_pos;
TDS_SMALLINT row_size;
unsigned char magic1[] = {0x07,0x00,0x08,0x00,0x00,0x00,0x00,0x00};
int marker;
int blob_cols = 0;

	if (! (hostfile = fopen(dbproc->bcp_hostfile, "r"))) {
		return FAIL;
	}

	if (_bcp_start_copy(dbproc)==FAIL) {
		fclose(hostfile);
		return FAIL;
	}
	resinfo = tds->res_info;

	for(i=0;i<dbproc->bcp_colcount;i++) {
		bcpcol	= dbproc->bcp_columns[i];
		if (is_nullable_type(bcpcol->db_type)) 
			var_cols++;
	}
	/* set packet type to send bulk data */
	tds->out_flag = 0x07;
	while (_bcp_read_hostfile(dbproc,hostfile)==SUCCEED) {
		/* zero the rowbuffer */
		memset(rowbuffer,'\0',32768);

		/* offset 0 = number of var columns */
		/* offset 1 = row number.  zeroed (datasever assigns) */
		row_pos = 2;
		rowbuffer[0] = var_cols;

		row_pos = _bcp_add_fixed_columns(dbproc, rowbuffer, row_pos);
if (IS_TDS42(tds)) {
		row_pos += 2; /* Microsoft mystery bytes */
}
		if (var_cols) {
			row_sz_pos = row_pos;
			row_pos += 2; /* row length */
			row_pos = _bcp_add_variable_columns(dbproc, rowbuffer, row_pos);
			row_size = row_pos;
		}
		memcpy(&rowbuffer[row_sz_pos],&row_size,sizeof(row_size));
		row_size = row_pos;
		tds_put_smallint(tds,row_size);
		tds_put_n(tds,rowbuffer,row_size);

		/* row is done, now handle any text/image data */
		blob_cols = 0;
		for (i=0;i<dbproc->bcp_colcount;i++) {
			bcpcol=dbproc->bcp_columns[i];
			if (is_blob_type(bcpcol->db_type)) {
				/* unknown but zero */
				tds_put_smallint(tds,0);
				tds_put_byte(tds,bcpcol->db_type);
				tds_put_byte(tds,0xff - blob_cols);
				/* offset of txptr we stashed during variable
				** column processing */
				tds_put_smallint(tds, bcpcol->txptr_offset);
				tds_put_int(tds, bcpcol->data_size);
				tds_put_n(tds,bcpcol->data, bcpcol->data_size);
				blob_cols++;
			}
		}
	}

	fclose(hostfile);

	/* tds_put_n(tds,magic1,8); */
	tds_flush_packet(tds);
	
	do {
		marker = tds_get_byte(tds);
		if (marker==TDS_DONE_TOKEN) {
			*rows_copied = tds_process_end(tds,marker,NULL,NULL);
		} else {
			tds_process_default_tokens(tds,marker);
		}
	} while (marker!=TDS_DONE_TOKEN);

	return SUCCEED;
}
RETCODE bcp_exec(DBPROCESS *dbproc, DBINT *rows_copied)
{
	if (dbproc->bcp_hostfile) {
		if (dbproc->bcp_direction==DB_OUT) {
			return _bcp_exec_out(dbproc,rows_copied);
		} else if (dbproc->bcp_direction==DB_IN) {
			return _bcp_exec_in(dbproc,rows_copied);
		} 
	}
	return FAIL;
}
RETCODE bcp_sendrow(DBPROCESS *dbproc)
{
	return SUCCEED;
}
RETCODE bcp_readfmt(DBPROCESS *dbproc, char *filename)
{
	return SUCCEED;
}
RETCODE bcp_writefmt(DBPROCESS *dbproc, char *filename)
{
	return SUCCEED;
}
RETCODE bcp_moretext(DBPROCESS *dbproc, DBINT size, BYTE *text)
{
	return SUCCEED;
}
RETCODE bcp_batch(DBPROCESS *dbproc)
{
	return SUCCEED;
}
RETCODE bcp_done(DBPROCESS *dbproc)
{
	return SUCCEED;
}
RETCODE bcp_bind(DBPROCESS *dbproc, BYTE *varaddr, int prefixlen, DBINT varlen,
	BYTE *terminator, int termlen, int type, int table_column)
{
	return SUCCEED;
}
