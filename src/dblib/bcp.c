/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004  Brian Bruns
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
#endif

#include <stdio.h>
#include <assert.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#include "tds.h"
#include "tdsiconv.h"
#include "tdsconvert.h"
#include "replacements.h"
#include "sybfront.h"
#include "sybdb.h"
#include "syberror.h"
#include "dblib.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

/*    was hard coded as 32768, but that made the local stack data size > 32K,
    which is not allowed on Mac OS 8/9. (mlilback, 11/7/01) */
#ifdef TARGET_API_MAC_OS8
#define ROWBUF_SIZE 31000
#else
#define ROWBUF_SIZE 32768
#endif

#define HOST_COL_CONV_ERROR 1
#define HOST_COL_NULL_ERROR 2

typedef struct _pbcb
{
	unsigned char *pb;
	int cb;
}
TDS_PBCB;

static char software_version[] = "$Id: bcp.c,v 1.96 2004-06-01 07:34:50 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static RETCODE _bcp_start_copy_in(DBPROCESS *);
static RETCODE _bcp_build_bulk_insert_stmt(TDS_PBCB *, BCP_COLINFO *, int);
static RETCODE _bcp_start_new_batch(DBPROCESS *);
static RETCODE _bcp_send_colmetadata(DBPROCESS *);
static int rtrim(char *, int);
static int _bcp_err_handler(DBPROCESS * dbproc, int bcp_errno);
static long int _bcp_measure_terminated_field(FILE * hostfile, BYTE * terminator, int term_len);

/* might be temporary */
int tds_do_until_done(TDSSOCKET * tds);


RETCODE
bcp_init(DBPROCESS * dbproc, const char *tblname, const char *hfile, const char *errfile, int direction)
{

	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;
	TDSRESULTINFO *resinfo;
	TDS_INT result_type;
	int i, rc, colsize;

	/* free allocated storage in dbproc & initialise flags, etc. */

	_bcp_clear_storage(dbproc);

	/* check validity of parameters */

	if (hfile != (char *) NULL) {

		dbproc->bcp.hostfile = (char *) malloc(strlen(hfile) + 1);
		strcpy(dbproc->bcp.hostfile, hfile);

		if (errfile != (char *) NULL) {
			dbproc->bcp.errorfile = (char *) malloc(strlen(errfile) + 1);
			strcpy(dbproc->bcp.errorfile, errfile);
		} else {
			dbproc->bcp.errorfile = (char *) NULL;
		}

	} else {
		dbproc->bcp.hostfile = (char *) NULL;
		dbproc->bcp.errorfile = (char *) NULL;
		dbproc->sendrow_init = 0;
	}

	if (tblname == (char *) NULL) {
		_bcp_err_handler(dbproc, SYBEBCITBNM);
		return (FAIL);
	}

	if (strlen(tblname) > 92) {	/* 30.30.30 */
		_bcp_err_handler(dbproc, SYBEBCITBLEN);
		return (FAIL);
	}

	dbproc->bcp.tablename = (char *) malloc(strlen(tblname) + 1);
	strcpy(dbproc->bcp.tablename, tblname);

	if (direction == DB_IN || direction == DB_OUT)
		dbproc->bcp.direction = direction;
	else {
		_bcp_err_handler(dbproc, SYBEBDIO);
		return (FAIL);
	}

	if (dbproc->bcp.direction == DB_IN) {
		if (tds_submit_queryf(tds, "select * from %s where 0 = 1", dbproc->bcp.tablename) == TDS_FAIL) {
			return FAIL;
		}

		while ((rc = tds_process_result_tokens(tds, &result_type, NULL))
		       == TDS_SUCCEED) {
		}
		if (rc != TDS_NO_MORE_RESULTS) {
			return FAIL;
		}

		if (!tds->res_info) {
			return FAIL;
		}

		resinfo = tds->res_info;

		dbproc->bcp.db_colcount = resinfo->num_cols;
		dbproc->bcp.db_columns = (BCP_COLINFO **) malloc(resinfo->num_cols * sizeof(BCP_COLINFO *));


		for (i = 0; i < dbproc->bcp.db_colcount; i++) {
			dbproc->bcp.db_columns[i] = (BCP_COLINFO *) malloc(sizeof(BCP_COLINFO));
			bcpcol = dbproc->bcp.db_columns[i];
			memset(bcpcol, '\0', sizeof(BCP_COLINFO));

			bcpcol->tab_colnum = i + 1;	/* turn offset into ordinal */
			bcpcol->db_type = resinfo->columns[i]->column_type;
			bcpcol->char_conv = resinfo->columns[i]->char_conv;
			bcpcol->db_length = resinfo->columns[i]->column_size;
			bcpcol->db_nullable = resinfo->columns[i]->column_nullable;

			/* curiosity test for text columns */
			colsize = tds_get_size_by_type(resinfo->columns[i]->column_type);
			if (colsize != resinfo->columns[i]->column_size && colsize != -1) {
				tdsdump_log(TDS_DBG_FUNC, "Hmm.  For column %d datatype %d, "
					    "server says size is %d and we'd expect %d bytes.\n",
					    i + 1, bcpcol->db_type, resinfo->columns[i]->column_size, colsize);
			}

			if (is_numeric_type(bcpcol->db_type)) {
				bcpcol->data = (BYTE *) malloc(sizeof(TDS_NUMERIC));
				((TDS_NUMERIC *) bcpcol->data)->precision = resinfo->columns[i]->column_prec;
				((TDS_NUMERIC *) bcpcol->data)->scale = resinfo->columns[i]->column_scale;
			} else {
				bcpcol->data = (BYTE *) malloc(bcpcol->db_length);
				if (bcpcol->data == (BYTE *) NULL) {
					fprintf(stderr, "Could not allocate %d bytes of memory\n", bcpcol->db_length);
					return FAIL;
				}
			}


			bcpcol->data_size = 0;

			if (IS_TDS7_PLUS(tds)) {
				bcpcol->db_usertype = resinfo->columns[i]->column_usertype;
				bcpcol->db_flags = resinfo->columns[i]->column_flags;
				bcpcol->on_server.column_type = resinfo->columns[i]->on_server.column_type;
				bcpcol->on_server.column_size = resinfo->columns[i]->on_server.column_size;
				bcpcol->db_prec = resinfo->columns[i]->column_prec;
				bcpcol->db_scale = resinfo->columns[i]->column_scale;
				memcpy(bcpcol->db_collate, resinfo->columns[i]->column_collation, 5);
				memcpy(bcpcol->db_name, resinfo->columns[i]->column_name, resinfo->columns[i]->column_namelen);
				bcpcol->db_name[resinfo->columns[i]->column_namelen] = 0;
				bcpcol->db_varint_size = resinfo->columns[i]->column_varint_size;
			}
		}

		if (hfile == (char *) NULL) {

			dbproc->bcp.host_colcount = dbproc->bcp.db_colcount;
			dbproc->bcp.host_columns = (BCP_HOSTCOLINFO **) malloc(dbproc->bcp.host_colcount * sizeof(BCP_HOSTCOLINFO *));

			for (i = 0; i < dbproc->bcp.host_colcount; i++) {
				dbproc->bcp.host_columns[i] = (BCP_HOSTCOLINFO *)
					malloc(sizeof(BCP_HOSTCOLINFO));
				memset(dbproc->bcp.host_columns[i], '\0', sizeof(BCP_HOSTCOLINFO));
			}
		}
	}

	return SUCCEED;
}


RETCODE
bcp_collen(DBPROCESS * dbproc, DBINT varlen, int table_column)
{
	BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->bcp.direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	if (table_column > dbproc->bcp.host_colcount)
		return FAIL;

	hostcol = dbproc->bcp.host_columns[table_column - 1];

	hostcol->column_len = varlen;

	return SUCCEED;
}


RETCODE
bcp_columns(DBPROCESS * dbproc, int host_colcount)
{

	int i;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	if (dbproc->bcp.hostfile == (char *) NULL) {
		_bcp_err_handler(dbproc, SYBEBIVI);
		return FAIL;
	}

	if (host_colcount < 1) {
		_bcp_err_handler(dbproc, SYBEBCFO);
		return FAIL;
	}

	dbproc->bcp.host_colcount = host_colcount;
	dbproc->bcp.host_columns = (BCP_HOSTCOLINFO **) malloc(host_colcount * sizeof(BCP_HOSTCOLINFO *));

	for (i = 0; i < host_colcount; i++) {
		dbproc->bcp.host_columns[i] = (BCP_HOSTCOLINFO *) malloc(sizeof(BCP_HOSTCOLINFO));
		memset(dbproc->bcp.host_columns[i], '\0', sizeof(BCP_HOSTCOLINFO));
	}

	return SUCCEED;
}

RETCODE
bcp_colfmt(DBPROCESS * dbproc, int host_colnum, int host_type, int host_prefixlen, DBINT host_collen, const BYTE * host_term,
	   int host_termlen, int table_colnum)
{
	BCP_HOSTCOLINFO *hostcol;

#ifdef MSDBLIB
	/* Microsoft specifies a "file_termlen" of zero if there's no terminator */
	if (host_termlen == 0)
		host_termlen = -1;
#endif
	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->bcp.hostfile == (char *) NULL) {
		_bcp_err_handler(dbproc, SYBEBIVI);
		return FAIL;
	}

	if (dbproc->bcp.host_colcount == 0) {
		_bcp_err_handler(dbproc, SYBEBCBC);
		return FAIL;
	}

	if (host_colnum < 1)
		return FAIL;

	if (host_prefixlen != 0 && host_prefixlen != 1 && host_prefixlen != 2 && host_prefixlen != 4 && host_prefixlen != -1) {
		_bcp_err_handler(dbproc, SYBEBCPREF);
		return FAIL;
	}

	if (table_colnum == 0 && host_type == 0) {
		_bcp_err_handler(dbproc, SYBEBCPCTYP);
		return FAIL;
	}

	if (host_prefixlen == 0 && host_collen == -1 && host_termlen == -1 && !is_fixed_type(host_type)) {
		_bcp_err_handler(dbproc, SYBEVDPT);
		return FAIL;
	}

	if (host_collen < -1) {
		_bcp_err_handler(dbproc, SYBEBCHLEN);
		return FAIL;
	}

	if (is_fixed_type(host_type) && (host_collen != -1 && host_collen != 0))
		return FAIL;

	/* 
	 * If there's a positive terminator length, we need a valid terminator pointer.
	 * If the terminator length is 0 or -1, then there's no terminator.
	 * FIXME: look up the correct error code for a bad terminator pointer or length and return that before arriving here.   
	 */
	assert((host_termlen > 0) ? (host_term != NULL) : 1);


	hostcol = dbproc->bcp.host_columns[host_colnum - 1];

	hostcol->host_column = host_colnum;
	hostcol->datatype = host_type;
	hostcol->prefix_len = host_prefixlen;
	hostcol->column_len = host_collen;
	if (host_term && host_termlen >= 0) {
		hostcol->terminator = (BYTE *) malloc(host_termlen + 1);
		memcpy(hostcol->terminator, host_term, host_termlen);
	}
	hostcol->term_len = host_termlen;
	hostcol->tab_colnum = table_colnum;

	return SUCCEED;
}



RETCODE
bcp_colfmt_ps(DBPROCESS * dbproc, int host_colnum, int host_type,
	      int host_prefixlen, DBINT host_collen, BYTE * host_term, int host_termlen, int table_colnum, DBTYPEINFO * typeinfo)
{
	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	return SUCCEED;
}



RETCODE
bcp_control(DBPROCESS * dbproc, int field, DBINT value)
{
	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	switch (field) {

	case BCPMAXERRS:
		dbproc->bcp.maxerrs = value;
		break;
	case BCPFIRST:
		dbproc->bcp.firstrow = value;
		break;
	case BCPLAST:
		dbproc->bcp.firstrow = value;
		break;
	case BCPBATCH:
		dbproc->bcp.batch = value;
		break;

	default:
		_bcp_err_handler(dbproc, SYBEIFNB);
		return FAIL;
	}
	return SUCCEED;
}

RETCODE
bcp_options(DBPROCESS * dbproc, int option, BYTE * value, int valuelen)
{
	int i;
	static const char *hints[] = { "ORDER", "ROWS_PER_BATCH", "KILOBYTES_PER_BATCH", "TABLOCK", "CHECK_CONSTRAINTS", NULL
	};

	if (!dbproc)
		return FAIL;

	switch (option) {
	case BCPLABELED:
		tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED bcp option: BCPLABELED\n");
		return FAIL;
	case BCPHINTS:
		if (!value || valuelen <= 0)
			return FAIL;

		if (dbproc->bcp.hint != NULL)	/* hint already set */
			return FAIL;

		for (i = 0; hints[i]; i++) {	/* do we know about this hint? */
			if (strncasecmp((char *) value, hints[i], strlen(hints[i])) == 0)
				break;
		}
		if (!hints[i]) {	/* no such hint */
			return FAIL;
		}

		/* 
		 * Store the bare hint, as passed from the application.  
		 * The process that constructs the "insert bulk" statement will incorporate the hint text.
		 */
		dbproc->bcp.hint = (char *) malloc(1 + valuelen);
		memcpy(dbproc->bcp.hint, value, valuelen);
		dbproc->bcp.hint[valuelen] = '\0';	/* null terminate it */
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED bcp option: %u\n", option);
		return FAIL;
	}

	return SUCCEED;
}

RETCODE
bcp_colptr(DBPROCESS * dbproc, BYTE * colptr, int table_column)
{
	BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	if (dbproc->bcp.direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	if (table_column > dbproc->bcp.host_colcount)
		return FAIL;

	hostcol = dbproc->bcp.host_columns[table_column - 1];

	hostcol->host_var = colptr;

	return SUCCEED;
}


DBBOOL
bcp_getl(LOGINREC * login)
{
	TDSLOGIN *tdsl = (TDSLOGIN *) login->tds_login;

	return (tdsl->bulk_copy);
}

static RETCODE
_bcp_exec_out(DBPROCESS * dbproc, DBINT * rows_copied)
{

	FILE *hostfile;
	int i;

	TDSSOCKET *tds;
	TDSRESULTINFO *resinfo;
	BCP_COLINFO *bcpcol = NULL;
	TDSCOLUMN *curcol = NULL;
	BCP_HOSTCOLINFO *hostcol;
	BYTE *src;
	int srctype;
	int buflen;
	int destlen;
	int plen;
	BYTE *outbuf;

	TDS_INT rowtype;
	TDS_INT computeid;
	TDS_INT result_type;

	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;

	TDSDATEREC when;

	int row_of_query;
	int rows_written;
	char *bcpdatefmt = NULL;

	tds = dbproc->tds_socket;
	assert(tds);

	if (!(hostfile = fopen(dbproc->bcp.hostfile, "w"))) {
		_bcp_err_handler(dbproc, SYBEBCUO);
		return FAIL;
	}

	bcpdatefmt = getenv("FREEBCP_DATEFMT");

	if (tds_submit_queryf(tds, "select * from %s", dbproc->bcp.tablename)
	    == TDS_FAIL) {
		return FAIL;
	}

	if (tds_process_result_tokens(tds, &result_type, NULL) == TDS_FAIL) {
		fclose(hostfile);
		return FAIL;
	}
	if (!tds->res_info) {
		fclose(hostfile);
		return FAIL;
	}

	resinfo = tds->res_info;

	dbproc->bcp.db_colcount = resinfo->num_cols;
	dbproc->bcp.db_columns = (BCP_COLINFO **) malloc(resinfo->num_cols * sizeof(BCP_COLINFO *));

	for (i = 0; i < resinfo->num_cols; i++) {
		dbproc->bcp.db_columns[i] = (BCP_COLINFO *) malloc(sizeof(BCP_COLINFO));
		memset(dbproc->bcp.db_columns[i], '\0', sizeof(BCP_COLINFO));
		dbproc->bcp.db_columns[i]->tab_colnum = i + 1;	/* turn offset into ordinal */
		dbproc->bcp.db_columns[i]->db_type = resinfo->columns[i]->column_type;
		dbproc->bcp.db_columns[i]->db_length = resinfo->columns[i]->column_size;
		if (is_numeric_type(resinfo->columns[i]->column_type))
			dbproc->bcp.db_columns[i]->data = (BYTE *) malloc(sizeof(TDS_NUMERIC));
		else
			dbproc->bcp.db_columns[i]->data = (BYTE *) malloc(dbproc->bcp.db_columns[i]->db_length);

		dbproc->bcp.db_columns[i]->db_nullable = resinfo->columns[i]->column_nullable;
		dbproc->bcp.db_columns[i]->data_size = 0;
	}

	row_of_query = 0;
	rows_written = 0;

	while (tds_process_row_tokens(tds, &rowtype, &computeid) == TDS_SUCCEED) {

		row_of_query++;

		if ((dbproc->bcp.firstrow == 0 && dbproc->bcp.lastrow == 0) ||
	   /** FIX this! */
		    ((dbproc->bcp.firstrow > 0 && row_of_query >= dbproc->bcp.firstrow)
		     && (dbproc->bcp.lastrow > 0 && row_of_query <= dbproc->bcp.lastrow))
			) {
			for (i = 0; i < dbproc->bcp.db_colcount; i++) {

				bcpcol = dbproc->bcp.db_columns[i];
				curcol = resinfo->columns[bcpcol->tab_colnum - 1];

				src = &resinfo->current_row[curcol->column_offset];

				if (is_blob_type(curcol->column_type)) {
					src = (BYTE *) ((TDSBLOB *) src)->textvalue;
				}

				srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);

				if (srctype != bcpcol->db_type) {
					bcpcol->db_type = srctype;
				}

				if (is_numeric_type(curcol->column_type))
					memcpy(bcpcol->data, src, sizeof(TDS_NUMERIC));
				else
					memcpy(bcpcol->data, src, curcol->column_cur_size);

				/* null columns have zero output */
				if (tds_get_null(resinfo->current_row, bcpcol->tab_colnum - 1))
					bcpcol->data_size = -1;
				else
					bcpcol->data_size = curcol->column_cur_size;

			}


			for (i = 0; i < dbproc->bcp.host_colcount; i++) {

				hostcol = dbproc->bcp.host_columns[i];
				if ((hostcol->tab_colnum < 1)
				    || (hostcol->tab_colnum > dbproc->bcp.db_colcount)) {
					continue;
				}

				if (hostcol->tab_colnum) {

					bcpcol = dbproc->bcp.db_columns[hostcol->tab_colnum - 1];
					curcol = resinfo->columns[bcpcol->tab_colnum - 1];

					if (bcpcol->tab_colnum != hostcol->tab_colnum) {
						return FAIL;
					}
				}
				assert(bcpcol);

				if (hostcol->datatype == 0)
					hostcol->datatype = bcpcol->db_type;

				/* work out how much space to allocate for output data */

				switch (hostcol->datatype) {

				case SYBINT1:
					buflen = destlen = 1;
					break;
				case SYBINT2:
					buflen = destlen = 2;
					break;
				case SYBINT4:
					buflen = destlen = 4;
					break;
				case SYBINT8:
					buflen = destlen = 8;
					break;
				case SYBREAL:
					buflen = destlen = 4;
					break;
				case SYBFLT8:
					buflen = destlen = 8;
					break;
				case SYBDATETIME:
					buflen = destlen = 8;
					break;
				case SYBDATETIME4:
					buflen = destlen = 4;
					break;
				case SYBBIT:
					buflen = destlen = 1;
					break;
				case SYBBITN:
					buflen = destlen = 1;
					break;
				case SYBMONEY:
					buflen = destlen = 8;
					break;
				case SYBMONEY4:
					buflen = destlen = 4;
					break;
				case SYBCHAR:
				case SYBVARCHAR:
					switch (bcpcol->db_type) {
					case SYBVARCHAR:
						buflen = curcol->column_cur_size + 1;
						destlen = -1;
						break;
					case SYBCHAR:
						buflen = curcol->column_cur_size + 1;
						if (bcpcol->db_nullable)
							destlen = -1;
						else
							destlen = -2;
						break;
					case SYBTEXT:
						buflen = curcol->column_cur_size + 1;
						destlen = -2;
						break;
					case SYBINT1:
						buflen = 4 + 1;	/*  255         */
						destlen = -1;
						break;
					case SYBINT2:
						buflen = 6 + 1;	/* -32768       */
						destlen = -1;
						break;
					case SYBINT4:
						buflen = 11 + 1;	/* -2147483648  */
						destlen = -1;
						break;
					case SYBINT8:
						buflen = 20 + 1;	/* -9223372036854775808  */
						destlen = -1;
						break;
					case SYBNUMERIC:
						buflen = 40 + 1;	/* 10 to the 38 */
						destlen = -1;
						break;
					case SYBDECIMAL:
						buflen = 40 + 1;	/* 10 to the 38 */
						destlen = -1;
						break;
					case SYBFLT8:
						buflen = 40 + 1;	/* 10 to the 38 */
						destlen = -1;
						break;
					case SYBDATETIME:
					case SYBDATETIME4:
						buflen = 255 + 1;
						destlen = -1;
						break;
					default:
						buflen = 255 + 1;
						destlen = -1;
						break;
					}
					break;
				default:
					buflen = destlen = 255;
				}

				outbuf = (BYTE *) malloc(buflen);
				buflen = 0;	/* so far: will hold size of output of dbconvert() */

				/* if we are converting datetime to string, need to override any 
				 * date time formats already established
				 */
				if ((bcpcol->db_type == SYBDATETIME || bcpcol->db_type == SYBDATETIME4)
				    && (hostcol->datatype == SYBCHAR || hostcol->datatype == SYBVARCHAR)) {
					memset(&when, 0, sizeof(when));
					tdsdump_log(TDS_DBG_FUNC, "%L buflen is %d\n", buflen);
					if (bcpcol->data_size > 0) {
						tds_datecrack(bcpcol->db_type, bcpcol->data, &when);
						if (bcpdatefmt) 
							buflen = tds_strftime((char *) outbuf, 256, bcpdatefmt, &when);
						else
							buflen = tds_strftime((char *) outbuf, 256, "%Y-%m-%d %H:%M:%S.%z", &when);
					}
				} else {
					/* 
					 * For null columns, the above work to determine the output buffer size is moot, 
					 * because bcpcol->data_size is zero, so dbconvert() won't write anything, and returns zero. 
					 */
					if (bcpcol->data_size == -1)
						buflen = -1;
					else
						buflen = dbconvert(dbproc,
							   bcpcol->db_type,
							   bcpcol->data, bcpcol->data_size, hostcol->datatype, outbuf, destlen);

					/* Special case: when outputting database varchar data  */
					/* (either varchar or nullable char) dbconvert may have */
					/* trimmed trailing blanks such that nothing is left.   */
					/* in this case we need to putput a single blank to the */
					/* output file...                                       */

					if (( bcpcol->db_type == SYBVARCHAR ||   /* database column variable char  */
						(bcpcol->db_type == SYBCHAR && bcpcol->db_nullable)
					   ) &&
						bcpcol->data_size > 0         &&   /* we had some data in the column */
						buflen == 0) {                     /* but we now don't have data...  */
						strcpy ((char *)outbuf, " ");
						buflen = 1;
					}
				}

				/* FIX ME -- does not handle prefix_len == -1 */
				/* The prefix */

				if ((plen = hostcol->prefix_len) == -1) {
					if (!(is_fixed_type(hostcol->datatype)))
						plen = 2;
					else if (curcol->column_nullable)
						plen = 1;
					else
						plen = 0;
				}
				switch (plen) {
				case 0:
					break;
				case 1:
					ti = buflen;
					fwrite(&ti, sizeof(ti), 1, hostfile);
					break;
				case 2:
					si = buflen;
					fwrite(&si, sizeof(si), 1, hostfile);
					break;
				case 4:
					li = buflen;
					fwrite(&li, sizeof(li), 1, hostfile);
					break;
				}

				/* The data */

				if (hostcol->column_len != -1) {
					buflen = buflen > hostcol->column_len ? hostcol->column_len : buflen;
				}

				if (buflen > 0)
					fwrite(outbuf, buflen, 1, hostfile);

				free(outbuf);

				/* The terminator */
				if (hostcol->terminator && hostcol->term_len > 0) {
					fwrite(hostcol->terminator, hostcol->term_len, 1, hostfile);
				}
			}
			rows_written++;
		}
	}
	if (fclose(hostfile) != 0) {
		_bcp_err_handler(dbproc, SYBEBCUC);
		return (FAIL);
	}

	printf("firstrow = %d row_of_query = %d rows written %d\n", dbproc->bcp.firstrow, row_of_query, rows_written);
	if (dbproc->bcp.firstrow > 0 && row_of_query < dbproc->bcp.firstrow) {
		/* 
		 * The table which bulk-copy is attempting to
		 * copy to a host-file is shorter than the
		 * number of rows which bulk-copy was instructed to skip.  
		 */
		_bcp_err_handler(dbproc, SYBETTS);
		return (FAIL);
	}

	*rows_copied = rows_written;
	return SUCCEED;
}

RETCODE
_bcp_read_hostfile(DBPROCESS * dbproc, FILE * hostfile, FILE * errfile, int *row_error)
{
	BCP_COLINFO *bcpcol = NULL;
	BCP_HOSTCOLINFO *hostcol;

	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;
	TDS_INT desttype;
	BYTE *coldata;

	int i, collen, data_is_null;

	/* for each host file column defined by calls to bcp_colfmt */

	for (i = 0; i < dbproc->bcp.host_colcount; i++) {
		tdsdump_log(TDS_DBG_FUNC, "%L parsing host column %d\n", i + 1);
		hostcol = dbproc->bcp.host_columns[i];

		data_is_null = 0;
		collen = 0;
		hostcol->column_error = 0;

		/* a prefix length, if extant, specifies how many bytes to read */
		if (hostcol->prefix_len > 0) {

			switch (hostcol->prefix_len) {
			case 1:
				if (fread(&ti, 1, 1, hostfile) != 1) {
					_bcp_err_handler(dbproc, SYBEBCRE);
					return (FAIL);
				}
				collen = ti;
				break;
			case 2:
				if (fread(&si, 2, 1, hostfile) != 1) {
					_bcp_err_handler(dbproc, SYBEBCRE);
					return (FAIL);
				}
				collen = si;
				break;
			case 4:
				if (fread(&li, 4, 1, hostfile) != 1) {
					_bcp_err_handler(dbproc, SYBEBCRE);
					return (FAIL);
				}
				collen = li;
				break;
			default:
				assert(hostcol->prefix_len <= 4);
				break;
			}

			if (collen == -1) {
				data_is_null = 1;
				collen = 0;
			}
		}

		/* if (Max) column length specified take that into consideration. */

		tdsdump_log(TDS_DBG_FUNC, "%L prefix_len = %d collen = %d \n", hostcol->prefix_len, collen);

		if (!data_is_null && hostcol->column_len >= 0) {
			if (hostcol->column_len == 0)
				data_is_null = 1;
			else {
				if (collen)
					collen = (hostcol->column_len < collen) ? hostcol->column_len : collen;
				else
					collen = hostcol->column_len;
			}
		}

		/* Fixed Length data - this overrides anything else specified */

		if (is_fixed_type(hostcol->datatype)) {
			collen = tds_get_size_by_type(hostcol->datatype);
		}

		/* 
		 * If this host file column contains table data,
		 * find the right element in the table/column list.  
		 */

		if (hostcol->tab_colnum) {
			bcpcol = dbproc->bcp.db_columns[hostcol->tab_colnum - 1];
			if (bcpcol->tab_colnum != hostcol->tab_colnum) {
				tdsdump_log(TDS_DBG_FUNC, "%L error: can't relate host column to table column\n");
				return FAIL;
			}
		}

		/*
		 * The data file either contains prefixes stating the length, or is delimited.  
		 * If delimited, we "measure" the field by looking for the terminator, then read it, 
		 * and set collen to the field's post-iconv size.  
		 */
		if (hostcol->term_len > 0) {
			int file_bytes_left;
			size_t col_bytes_left;
			iconv_t cd;

			collen = _bcp_measure_terminated_field(hostfile, hostcol->terminator, hostcol->term_len);
			if (collen == -1) {
				*row_error = TRUE;
				tdsdump_log(TDS_DBG_FUNC, "_bcp_measure_terminated_field returned -1!\n");
				/* FIXME emit message? _bcp_err_handler(dbproc, SYBEMEM); */
				return (FAIL);
			} else if (collen == 0)
				data_is_null = 1;


			tdsdump_log(TDS_DBG_FUNC, "_bcp_measure_terminated_field returned %d\n", collen);
			/* 
			 * Allocate a column buffer guaranteed to be big enough hold the post-iconv data.
			 */
			if (bcpcol->char_conv) {
				if (bcpcol->on_server.column_size > bcpcol->db_length)
					collen = (collen * bcpcol->on_server.column_size) / bcpcol->db_length;
				cd = bcpcol->char_conv->to_wire;
			} else {
				cd = (iconv_t) - 1;
			}

			coldata = (BYTE *) calloc(1, 1 + collen);
			if (coldata == NULL) {
				*row_error = TRUE;
				tdsdump_log(TDS_DBG_FUNC, "calloc returned NULL pointer!\n");
				_bcp_err_handler(dbproc, SYBEMEM);
				return (FAIL);
			}

			/* 
			 * Read and convert the data
			 */
			col_bytes_left = collen;
			file_bytes_left = tds_iconv_fread(cd, hostfile, collen, hostcol->term_len, coldata, &col_bytes_left);
			collen -= col_bytes_left;

			if (file_bytes_left != 0) {
				tdsdump_log(TDS_DBG_FUNC, "Error in %s, col %d: %d of %d bytes unread\nfile_bytes_left != 0!\n", 
							__FILE__, (i+1), file_bytes_left, collen);
				*row_error = TRUE;
				free(coldata);
				return FAIL;
			}

			/* TODO:  
			 *      Dates are a problem.  In theory, we should be able to read non-English dates, which
			 *      would contain non-ASCII characters.  One might suppose we should convert date
			 *      strings to ISO-8859-1 (or another canonical form) here, because tds_convert() can't be
			 *      expected to deal with encodings. But instead date strings are read verbatim and 
			 *      passed to tds_convert() without even waving to iconv().  For English dates, this works, 
			 *      because English dates expressed as UTF-8 strings are indistinguishable from the ASCII.  
			 */
		} else {	/* unterminated field */
			bcpcol = dbproc->bcp.db_columns[hostcol->tab_colnum - 1];
			if (collen == 0 || bcpcol->db_nullable) {
				if (collen != 0) {
					/* A fixed length type */
					TDS_TINYINT len;
					if (fread(&len, sizeof(len), 1, hostfile) != 1) {
						if (i != 0)
							_bcp_err_handler(dbproc, SYBEBCRE);
						return (FAIL);
					}
					collen = len == 255 ? -1 : len;
				} else {
					TDS_SMALLINT len;

					if (fread(&len, sizeof(len), 1, hostfile) != 1) {
						if (i != 0)
							_bcp_err_handler(dbproc, SYBEBCRE);
						return (FAIL);
					}
					collen = len;
				}
				if (collen == -1) {
					collen = 0;
					data_is_null = 1;
				}
			}
			coldata = (BYTE *) calloc(1, 1 + collen);
			if (coldata == NULL) {
				*row_error = TRUE;
				_bcp_err_handler(dbproc, SYBEMEM);
				return (FAIL);
			}

			if (collen) {
				/* 
				 * Read and convert the data
				 * TODO: Call tds_iconv_fread() instead of fread(3).  
				 *       The columns should each have their iconv cd set, and noncharacter data
				 *       should have -1 as the iconv cd, causing tds_iconv_fread() to not attempt
				 * 	 any conversion.  We do not need a datatype switch here to decide what to do.  
				 *	 As of 0.62, this *should* actually work.  All that remains is to change the
				 *	 call and test it. 
				 */
				if (fread(coldata, collen, 1, hostfile) != 1) {
					free(coldata);
					if (i == 0 && feof(hostfile))
						tdsdump_log(TDS_DBG_FUNC, "%L Normal end-of-file reached while loading bcp data file.\n");
					else
						_bcp_err_handler(dbproc, SYBEBCRE);
					return (FAIL);
				}
			}
		}

		/*
		 * If we read no bytes and we're at end of file AND this is the first column, 
		 * then we've stumbled across the finish line.  Tell the caller we failed to read 
		 * anything but encountered no error.
		 */
		if (i == 0 && collen == 0 && feof(hostfile)) {
			free(coldata);
			tdsdump_log(TDS_DBG_FUNC, "%L Normal end-of-file reached while loading bcp data file.\n");
			return (FAIL);
		}

		/* 
		 * At this point, however the field was read, however big it was, its address is coldata and its size is collen.
		 */
		if (hostcol->tab_colnum) {
			if (data_is_null) {
				bcpcol->data_size = -1;
			} else {
				desttype = tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length);

				/* special hack for text columns */
				if (bcpcol->db_length == 4096 && collen > bcpcol->db_length) {	/* "4096" might not really matter */
					BYTE *oldbuffer = bcpcol->data;
					switch (desttype) {
					case SYBTEXT:
					case SYBNTEXT:
					case SYBIMAGE:
					case SYBVARBINARY:
					case XSYBVARBINARY:
					case SYBLONGBINARY:	/* Reallocate enough space for the data from the file. */
						bcpcol->db_length = 8 + collen;	/* room to breathe */
						bcpcol->data = (BYTE *) realloc(bcpcol->data, bcpcol->db_length);
						assert(bcpcol->data);
						if (!bcpcol->data) {
							_bcp_err_handler(dbproc, SYBEMEM);
							free(oldbuffer);
							return FAIL;
						}
						break;
					default:
						break;
					}
				}
				/* end special hack for text columns */

				bcpcol->data_size =
					dbconvert(dbproc, hostcol->datatype, coldata, collen, desttype,
						  bcpcol->data, bcpcol->db_length);

				free(coldata);

				if (bcpcol->data_size == -1) {
					hostcol->column_error = HOST_COL_CONV_ERROR;
					*row_error = 1;
					tdsdump_log(TDS_DBG_FUNC, 
						    "%L _bcp_read_hostfile (bcp.c:%d) failed to convert %d bytes at offset 0x%x in the data file.\n", 
						    __LINE__, collen, ftell(hostfile) - collen);
				}
				
				/* trim trailing blanks from character data */
				if (desttype == SYBCHAR || desttype == SYBVARCHAR) {
					bcpcol->data_size = rtrim((char *) bcpcol->data, bcpcol->data_size);
				} 
			}
			if (bcpcol->data_size == -1) {	/* Are we trying to insert a NULL ? */
				if (!bcpcol->db_nullable) {
					/* too bad if the column is not nullable */
					hostcol->column_error = HOST_COL_NULL_ERROR;
					*row_error = 1;
					_bcp_err_handler(dbproc, SYBEBCNN);
				}
			}
		}
	}
	return SUCCEED;
}

/*
 * Look for the next terminator in a host data file, and return the data size.  
 * \return size of field, excluding the terminator.  
 * \remarks The current offset will be unchanged.  If an error was encountered, the returned size will be -1.  
 * 	The caller should check for that possibility, but the appropriate message should already have been emitted.  
 * 	The caller can then use tds_iconv_fread() to read-and-convert the file's data 
 *	into host format, or, if we're not dealing with a character column, just fread(3).  
 */
static long int
_bcp_measure_terminated_field(FILE * hostfile, BYTE * terminator, int term_len)
{
	char *sample;
	int sample_size, bytes_read = 0;
	long int size;

	const long int initial_offset = ftell(hostfile);

	sample = malloc(term_len);

	if (!sample) {
		_bcp_err_handler(NULL, SYBEMEM);
		return -1;
	}

	for (sample_size = 1; (bytes_read = fread(sample, sample_size, 1, hostfile)) != 0;) {

		bytes_read *= sample_size;

		/*
		 * Check for terminator.
		 */
		if (*sample == *terminator) {
			int found = 0;

			if (sample_size == term_len) {	
				/*
				 * If we read a whole terminator, compare the whole sequence and, if found, go home. 
				 */
				found = 0 == memcmp(sample, terminator, term_len);

				if (found) {
					free(sample);
					size = ftell(hostfile) - initial_offset;
					if (size < 0 || 0 != fseek(hostfile, initial_offset, SEEK_SET)) {
						/* FIXME emit message */
						return -1;
					}
					return size - term_len;
				}
				/* 
				 * If we tried to read a terminator and found something else, then we read a 
				 * terminator's worth of data.  Back up N-1 bytes, and revert to byte-at-a-time testing.
				 */
				if (sample_size > 1) { 
					sample_size--;
					if (-1 == fseek(hostfile, -sample_size, SEEK_CUR)) {
						/* FIXME emit message */
						return -1;
					}
				}
				sample_size = 1;
				continue;
			} else {
				/* 
				 * Found start of terminator, but haven't read a full terminator's length yet.  
				 * Back up, read a whole terminator, and try again.
				 */
				assert(bytes_read == 1);
				ungetc(*sample, hostfile);
				sample_size = term_len;
				continue;
			}
			assert(0);	/* should not arrive here */
		}
	}

	free(sample);

	/*
	 * To get here, we ran out of memory, or encountered an error (or EOF) with the file.  
	 * EOF is a surprise, because if we read a complete field with its terminator, 
	 * we would have returned without attempting to read past end of file.  
	 */

	if (feof(hostfile)) {
		if (initial_offset == ftell(hostfile)) {
			return 0;
		} else {
			/* a cheat: we don't have dbproc, so pass zero */
			_bcp_err_handler(0, SYBEBEOF);
		}
	} else if (ferror(hostfile)) {
		_bcp_err_handler(0, SYBEBCRE);
	}

	return -1;
}

/**
 * Add fixed size columns to the row
 */
static int
_bcp_add_fixed_columns(DBPROCESS * dbproc, BYTE * rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	BCP_COLINFO *bcpcol;
	int cpbytes;

	int i, j;

	for (i = 0; i < dbproc->bcp.db_colcount; i++) {
		bcpcol = dbproc->bcp.db_columns[i];


		if (!is_nullable_type(bcpcol->db_type) && !(bcpcol->db_nullable)) {

			if (!(bcpcol->db_nullable) && bcpcol->data_size == -1) {
				_bcp_err_handler(dbproc, SYBEBCNN);
				return FAIL;
			}

			if (is_numeric_type(bcpcol->db_type)) {
				num = (TDS_NUMERIC *) bcpcol->data;
				cpbytes = tds_numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
			} else {
				cpbytes = bcpcol->data_size > bcpcol->db_length ? bcpcol->db_length : bcpcol->data_size;
				memcpy(&rowbuffer[row_pos], bcpcol->data, cpbytes);

				/* CHAR data may need padding out to the database length with blanks */

				if (bcpcol->db_type == SYBCHAR && cpbytes < bcpcol->db_length) {
					for (j = cpbytes; j <  bcpcol->db_length; j++)
						rowbuffer[row_pos + j] = ' ';

				}
			}

			row_pos += bcpcol->db_length;
		}
	}
	return row_pos;
}

/*
 * Add variable size columns to the row
 */
static int
_bcp_add_variable_columns(DBPROCESS * dbproc, BYTE * rowbuffer, int start, int *var_cols)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;
	TDS_NUMERIC *num;
	int row_pos;
	int cpbytes;

	BYTE offset_table[256];
	BYTE adjust_table[256];

	int offset_pos     = 0;
	int adjust_pos     = 0;
	int num_cols       = 0;
	int last_adjustment_increment = 0;
	int this_adjustment_increment = 0;

	int i, adjust_table_entries_required;

	/* Skip over two bytes. These will be used to hold the entire record length */
	/* once the record has been completely built.                               */

	row_pos = start + 2;

	/* for each column in the target table */

	for (i = 0; i < dbproc->bcp.db_colcount; i++) {

		bcpcol = dbproc->bcp.db_columns[i];

		/* is this column of "variable" type, i.e. NULLable */
		/* or naturally variable length e.g. VARCHAR        */

		if (is_nullable_type(bcpcol->db_type) || bcpcol->db_nullable) {

			/* but if its a NOT NULL column, and we have no data */
			/* throw an error                                    */

			if (!(bcpcol->db_nullable) && bcpcol->data_size == -1) {
				_bcp_err_handler(dbproc, SYBEBCNN);
				return FAIL;
			}

			if (is_blob_type(bcpcol->db_type)) {
				cpbytes = 16;
				bcpcol->txptr_offset = row_pos;               /* save for data write */
			} else if (is_numeric_type(bcpcol->db_type)) {
				num = (TDS_NUMERIC *) bcpcol->data;
				cpbytes = tds_numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
			} else {
				/* compute the length to copy to the row ** buffer */
				if (bcpcol->data_size == -1) {
					cpbytes = 0;
				} else {
					cpbytes = bcpcol->data_size > bcpcol->db_length ? bcpcol->db_length : bcpcol->data_size;
					memcpy(&rowbuffer[row_pos], bcpcol->data, cpbytes);
				}

			}

			/* if we have written data to the record for this column */

			if (cpbytes > 0) {

				/* update offset table. Each entry in the offset table is a single byte */
				/* so can only hold a maximum value of 255. If the real offset is more  */
				/* than 255 we will have to add one or more entries in the adjust table */

				offset_table[offset_pos++] = row_pos % 256;

				/* increment count of variable columns added to the record */

				num_cols++;

				/* how many times does 256 have to be added to the one byte offset to   */
				/* calculate the REAL offset...                                         */

				this_adjustment_increment = row_pos / 256;

				/* has this changed since we did the last column...      */

				if (this_adjustment_increment > last_adjustment_increment) {

					/* add n entries to the adjust table. each entry represents */
					/* an adjustment of 256 bytes, and each entry holds the     */
					/* column number for which the adjustment needs to be made  */

					for ( adjust_table_entries_required = this_adjustment_increment - last_adjustment_increment;
						  adjust_table_entries_required > 0;
						  adjust_table_entries_required-- ) {
							adjust_table[adjust_pos++] = num_cols;
					}
					last_adjustment_increment = this_adjustment_increment;
				}

				row_pos += cpbytes;
			}
		}
	}

	if (num_cols) {	
		/* 
		 * If we have written any variable columns to the record, add entries 
		 * to the offset and adjust tables for the end of data offset (as above). 
		 */

		offset_table[offset_pos++] = row_pos % 256;

		/*
		 * Write the offset data etc. to the end of the record, starting with 
		 * a count of variable columns (plus 1 for the eod offset)       
		 */
		if (IS_TDS50(tds))
			rowbuffer[row_pos++] = num_cols + 1;
	
		/* write the adjust table (right to left) */
		for (i = adjust_pos - 1; i >= 0; i--) {
			rowbuffer[row_pos++] = adjust_table[i];
		}
	
		/* write the offset table (right to left) */
		for (i = offset_pos - 1; i >= 0; i--) {
			rowbuffer[row_pos++] = offset_table[i];
		}
	}

	*var_cols = num_cols;

	if (num_cols == 0) /* we haven't written anything */
		return start;
	else
		return row_pos;
}

RETCODE
bcp_sendrow(DBPROCESS * dbproc)
{

	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;

	int i, ret;

	/* FIX ME -- calculate dynamically */
	unsigned char rowbuffer[ROWBUF_SIZE];
	int row_pos;
	int row_sz_pos;
	TDS_SMALLINT row_size;

	int blob_cols = 0;
	int var_cols_written = 0;

	unsigned char row_token = 0xd1;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	if (dbproc->bcp.hostfile != (char *) NULL) {
		_bcp_err_handler(dbproc, SYBEBCPB);
		return FAIL;
	}

	if (dbproc->bcp.direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	/* 
	 * The first time sendrow is called after bcp_init,
	 * there is a certain amount of initialisation to be done.
	 */
	if (!(dbproc->sendrow_init)) {

		/* first call the start_copy function, which will */
		/* retrieve details of the database table columns */

		if (_bcp_start_copy_in(dbproc) == FAIL) {
			_bcp_err_handler(dbproc, SYBEBULKINSERT);
			return (FAIL);
		}

		/* set packet type to send bulk data */
		tds->out_flag = 0x07;

		if (IS_TDS7_PLUS(tds)) {
			_bcp_send_colmetadata(dbproc);
		}

		dbproc->sendrow_init = 1;

	}


	if (_bcp_get_prog_data(dbproc) == SUCCEED) {

		if (IS_TDS7_PLUS(tds)) {
			tds_put_byte(tds, row_token);	/* 0xd1 */

			for (i = 0; i < dbproc->bcp.db_colcount; i++) {

				/* send the data for each column */
				ret = tds7_put_bcpcol(tds, dbproc->bcp.db_columns[i]);

				if (ret == TDS_FAIL) {
					_bcp_err_handler(dbproc, SYBEBCNN);
				}

			}
			return SUCCEED;
		} else {	/* Not TDS 7 or 8 */

			memset(rowbuffer, '\0', ROWBUF_SIZE);	/* zero the rowbuffer */

			/* offset 0 = number of var columns */
			/* offset 1 = row number.  zeroed (datasever assigns) */
			row_pos = 2;

			if ((row_pos = _bcp_add_fixed_columns(dbproc, rowbuffer, row_pos)) == FAIL)
				return (FAIL);

			row_sz_pos = row_pos;

			/* potential variable columns to write */

			if (dbproc->var_cols) {

				if ((row_pos = _bcp_add_variable_columns(dbproc, rowbuffer, row_pos, &var_cols_written)) == FAIL)
					return (FAIL);
			}

			row_size = row_pos;

			if (var_cols_written) {
				memcpy(&rowbuffer[row_sz_pos], &row_size, sizeof(row_size));
				rowbuffer[0] = var_cols_written;
			}

			tds_put_smallint(tds, row_size);
			tds_put_n(tds, rowbuffer, row_size);

			/* row is done, now handle any text/image data */

			blob_cols = 0;

			for (i = 0; i < dbproc->bcp.db_colcount; i++) {
				bcpcol = dbproc->bcp.db_columns[i];
				if (is_blob_type(bcpcol->db_type)) {
					/* unknown but zero */
					tds_put_smallint(tds, 0);
					tds_put_byte(tds, bcpcol->db_type);
					tds_put_byte(tds, 0xff - blob_cols);
					/* offset of txptr we stashed during variable
					 * ** column processing */
					tds_put_smallint(tds, bcpcol->txptr_offset);
					tds_put_int(tds, bcpcol->data_size);
					tds_put_n(tds, bcpcol->data, bcpcol->data_size);
					blob_cols++;
				}
			}
			return SUCCEED;
		}
	}

	return FAIL;
}


static RETCODE
_bcp_exec_in(DBPROCESS * dbproc, DBINT * rows_copied)
{
	FILE *hostfile, *errfile = NULL;
	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;
	BCP_HOSTCOLINFO *hostcol;

	/* FIX ME -- calculate dynamically */
	unsigned char rowbuffer[ROWBUF_SIZE];
	int row_pos;

	/* end of data pointer...the last byte of var data before the adjust table */

	int row_sz_pos;
	TDS_SMALLINT row_size;

	int i, ret;
	int blob_cols = 0;
	int row_of_hostfile;
	int rows_written_so_far;
	int var_cols_written   = 0;

	int row_error, row_error_count;
	long row_start, row_end;
	int error_row_size;
	char *row_in_error;
	
	*rows_copied = 0;
	
	if (!(hostfile = fopen(dbproc->bcp.hostfile, "r"))) {
		_bcp_err_handler(dbproc, SYBEBCUO);
		return FAIL;
	}

	if (dbproc->bcp.errorfile) {
		if (!(errfile = fopen(dbproc->bcp.errorfile, "w"))) {
			_bcp_err_handler(dbproc, SYBEBUOE);
			return FAIL;
		}
	}

	if (_bcp_start_copy_in(dbproc) == FAIL)
		return (FAIL);

	tds->out_flag = 0x07;

	if (IS_TDS7_PLUS(tds)) {
		_bcp_send_colmetadata(dbproc);
	}

	row_of_hostfile = 0;
	rows_written_so_far = 0;

	row_start = ftell(hostfile);
	row_error_count = 0;
	row_error = 0;

	while (_bcp_read_hostfile(dbproc, hostfile, errfile, &row_error) == SUCCEED) {

		row_of_hostfile++;

		if (row_error) {

			if (errfile != (FILE *) NULL) {

				for (i = 0; i < dbproc->bcp.host_colcount; i++) {
					hostcol = dbproc->bcp.host_columns[i];
					if (hostcol->column_error == HOST_COL_CONV_ERROR) {
						fprintf(errfile, "#@ data conversion error on host data file Row %d Column %d\n",
							row_of_hostfile, i + 1);
					} else if (hostcol->column_error == HOST_COL_NULL_ERROR) {
						fprintf(errfile, "#@ Attempt to bulk-copy a NULL value into Server column"
							" which does not accept NULL values. Row %d, Column %d\n",
							row_of_hostfile, i + 1);
					}
				}

				row_end = ftell(hostfile);

				error_row_size = row_end - row_start;
				row_in_error = (BYTE *) malloc(error_row_size);

				fseek(hostfile, row_start, SEEK_SET);

				if (fread(row_in_error, error_row_size, 1, hostfile) != 1) {
					printf("BILL fread failed after fseek\n");
				}

				fseek(hostfile, row_end, SEEK_SET);
				fwrite(row_in_error, error_row_size, 1, errfile);
				fprintf(errfile, "\n");
				free(row_in_error);
			}
			row_error_count++;
			if (row_error_count > dbproc->bcp.maxerrs)
				break;
		} else {

			if ((dbproc->bcp.firstrow == 0 && dbproc->bcp.lastrow == 0) ||
			    ((dbproc->bcp.firstrow > 0 && row_of_hostfile >= dbproc->bcp.firstrow) &&
			     (dbproc->bcp.lastrow > 0 && row_of_hostfile <= dbproc->bcp.lastrow)
			    )
				) {

				if (IS_TDS7_PLUS(tds)) {

					tds_put_byte(tds, TDS_ROW_TOKEN);	/* 0xd1 */

					for (i = 0; i < dbproc->bcp.db_colcount; i++) {

						/* send the data for each column */
						ret = tds7_put_bcpcol(tds, dbproc->bcp.db_columns[i]);

						if (ret == TDS_FAIL) {
							_bcp_err_handler(dbproc, SYBEBCNN);
						}
					}
				} else {	/* Not TDS 7 or 8 */

					memset(rowbuffer, '\0', ROWBUF_SIZE);	/* zero the rowbuffer */
					var_cols_written = 0;

					/* offset 0 = number of var columns */
					/* offset 1 = row number.  zeroed (datasever assigns) */

					row_pos = 2;
					row_pos = _bcp_add_fixed_columns(dbproc, rowbuffer, row_pos);
					
					if (row_pos == FAIL)
						return FAIL;

					row_sz_pos = row_pos;

			        	/* potential variable columns to write */

					if (dbproc->var_cols) {
						row_pos = _bcp_add_variable_columns(dbproc, rowbuffer, row_pos, &var_cols_written);
						if (row_pos == FAIL)
							return FAIL;
					}

					row_size = row_pos;

					if (var_cols_written) {
						memcpy(&rowbuffer[row_sz_pos], &row_size, sizeof(row_size));
						rowbuffer[0] = var_cols_written;
					}

					tds_put_smallint(tds, row_size);
					tds_put_n(tds, rowbuffer, row_size);

					/* row is done, now handle any text/image data */

					blob_cols = 0;

					for (i = 0; i < dbproc->bcp.db_colcount; i++) {
						bcpcol = dbproc->bcp.db_columns[i];
						if (is_blob_type(bcpcol->db_type)) {
							/* unknown but zero */
							tds_put_smallint(tds, 0);
							tds_put_byte(tds, bcpcol->db_type);
							tds_put_byte(tds, 0xff - blob_cols);
							/* offset of txptr we stashed during variable column processing */
							tds_put_smallint(tds, bcpcol->txptr_offset);
							tds_put_int(tds, bcpcol->data_size);
							tds_put_n(tds, bcpcol->data, bcpcol->data_size);
							blob_cols++;
						}
					}
				}	/* Not TDS  7.0 or 8.0 */

				rows_written_so_far++;

				if (dbproc->bcp.batch > 0 && rows_written_so_far == dbproc->bcp.batch) {
					rows_written_so_far = 0;

					tds_flush_packet(tds);

					tds->state = TDS_QUERYING;

					if (tds_process_simple_query(tds) != TDS_SUCCEED)
						return FAIL;
						
					*rows_copied += tds->rows_affected;

					_bcp_err_handler(dbproc, SYBEBBCI); /* batch copied to server */

					_bcp_start_new_batch(dbproc);

				}
			}

		}

		row_start = ftell(hostfile);
		row_error = 0;
	}

	if (fclose(hostfile) != 0) {
		_bcp_err_handler(dbproc, SYBEBCUC);
		return (FAIL);
	}

	tds_flush_packet(tds);

	tds->state = TDS_QUERYING;

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	*rows_copied += tds->rows_affected;

	return SUCCEED;
}

RETCODE
bcp_exec(DBPROCESS * dbproc, DBINT * rows_copied)
{
	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	if (dbproc->bcp.hostfile) {
		RETCODE ret = 0;

		if (dbproc->bcp.direction == DB_OUT) {
			ret = _bcp_exec_out(dbproc, rows_copied);
		} else if (dbproc->bcp.direction == DB_IN) {
			ret = _bcp_exec_in(dbproc, rows_copied);
		}
		_bcp_clear_storage(dbproc);
		return ret;
	} else {
		_bcp_err_handler(dbproc, SYBEBCVH);
		return FAIL;
	}
}


static RETCODE
_bcp_start_copy_in(DBPROCESS * dbproc)
{

	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_COLINFO *bcpcol;

	int i;
	int firstcol;

	char *query;
	unsigned char clause_buffer[4096] = { 0 };

	TDS_PBCB colclause;

	colclause.pb = clause_buffer;
	colclause.cb = sizeof(clause_buffer);

	if (IS_TDS7_PLUS(tds)) {
		int erc;
		char *hint;

		firstcol = 1;

		for (i = 0; i < dbproc->bcp.db_colcount; i++) {
			bcpcol = dbproc->bcp.db_columns[i];

			if (IS_TDS7_PLUS(tds)) {
				erc = _bcp_build_bulk_insert_stmt(&colclause, bcpcol, firstcol);
				if (erc == FAIL) {
					tdsdump_log(TDS_DBG_FUNC, "%L error: _bcp_build_bulk_insert_stmt returned FAIL.\n", bcpcol->on_server.column_type);
					return erc;
				}
				firstcol = 0;
			}
		}

		if (dbproc->bcp.hint) {
			if (asprintf(&hint, " with (%s)", dbproc->bcp.hint) < 0) {
				return FAIL;
			}
		} else {
			hint = strdup("");
		}
		if (!hint)
			return FAIL;

		erc = asprintf(&query, "insert bulk %s (%s)%s", dbproc->bcp.tablename, colclause.pb, hint);

		free(hint);
		if (colclause.pb != clause_buffer) {
			free(colclause.pb);
			colclause.pb = NULL;	/* just for good measure; not used beyond this point */
		}

		if (erc < 0) {
			return FAIL;
		}

	} else {
		if (asprintf(&query, "insert bulk %s", dbproc->bcp.tablename) < 0) {
			return FAIL;
		}
	}

	tds_submit_query(tds, query);

	/* save the statement for later... */

	dbproc->bcp.insert_stmt = query;

	/*
	 * In TDS 5 we get the column information as a result set from the "insert bulk" command.
	 * We're going to ignore it.  
	 */
	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	/* 
	 * Work out the number of "variable" columns.  These are either nullable or of 
	 * varying length type e.g. varchar.   
	 */
	dbproc->var_cols = 0;
	for (i = 0; i < dbproc->bcp.db_colcount; i++) {

		bcpcol = dbproc->bcp.db_columns[i];
		if (is_nullable_type(bcpcol->db_type) || bcpcol->db_nullable) {
			dbproc->var_cols++;
		}
	}

	return SUCCEED;
}

/**
 * \todo support timestamp columns.
 * Things we know: 
 *  1. usertype is 80 (0x50)
 *  2. server type is 183 (0xAD)
 *  3. Microsoft bcp.exe (TDS 7.0) omits the timestamp column from the bulk insert statement.
 *  4. Something is munging the server type from 183 to 179.  set_cardinal_type()?  
 */
static RETCODE
_bcp_build_bulk_insert_stmt(TDS_PBCB * clause, BCP_COLINFO * bcpcol, int first)
{
	char buffer[32];
	char *column_type = buffer;

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
		switch (bcpcol->db_length) {
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
		switch (bcpcol->db_length) {
		case 4:
			column_type = "real";
			break;
		case 8:
			column_type = "float";
			break;
		}
		break;
	case SYBMONEYN:
		switch (bcpcol->db_length) {
		case 4:
			column_type = "smallmoney";
			break;
		case 8:
			column_type = "money";
			break;
		}
		break;
	case SYBDATETIMN:
		switch (bcpcol->db_length) {
		case 4:
			column_type = "smalldatetime";
			break;
		case 8:
			column_type = "datetime";
			break;
		}
		break;
	case SYBDECIMAL:
		sprintf(column_type, "decimal(%d,%d)", bcpcol->db_prec, bcpcol->db_scale);
		break;
	case SYBNUMERIC:
		sprintf(column_type, "numeric(%d,%d)", bcpcol->db_prec, bcpcol->db_scale);
		break;

	case XSYBVARBINARY:
		sprintf(column_type, "varbinary(%d)", bcpcol->db_length);
		break;
	case XSYBVARCHAR:
		sprintf(column_type, "varchar(%d)", bcpcol->db_length);
		break;
	case XSYBBINARY:
		sprintf(column_type, "binary(%d)", bcpcol->db_length);
		break;
	case XSYBCHAR:
		sprintf(column_type, "char(%d)", bcpcol->db_length);
		break;
	case SYBTEXT:
		sprintf(column_type, "text");
		break;
	case SYBIMAGE:
		sprintf(column_type, "image");
		break;
	case XSYBNVARCHAR:
		sprintf(column_type, "nvarchar(%d)", bcpcol->db_length);
		break;
	case XSYBNCHAR:
		sprintf(column_type, "nchar(%d)", bcpcol->db_length);
		break;
	case SYBNTEXT:
		sprintf(column_type, "ntext");
		break;
	case SYBUNIQUE:
		sprintf(column_type, "uniqueidentifier	");
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "%L error: cannot build bulk insert statement.  unrecognized server datatype %d\n", bcpcol->on_server.column_type);
		return FAIL;
	}

	if (clause->cb < strlen(clause->pb) + strlen(bcpcol->db_name) + strlen(column_type) + ((first) ? 2 : 4)) {
		unsigned char *temp = malloc(2 * clause->cb);

		assert(temp);
		if (!temp)
			return FAIL;
		strcpy(temp, clause->pb);
		clause->pb = temp;
		clause->cb *= 2;
	}

	if (!first)
		strcat(clause->pb, ", ");

	strcat(clause->pb, bcpcol->db_name);
	strcat(clause->pb, " ");
	strcat(clause->pb, column_type);

	return SUCCEED;
}

static RETCODE
_bcp_start_new_batch(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	tds_submit_query(tds, dbproc->bcp.insert_stmt);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	tds->out_flag = 0x07;

	if (IS_TDS7_PLUS(tds)) {
		_bcp_send_colmetadata(dbproc);
	}
	
	return SUCCEED;
}

static RETCODE
_bcp_send_colmetadata(DBPROCESS * dbproc)
{

	TDSSOCKET *tds = dbproc->tds_socket;
	unsigned char colmetadata_token = 0x81;
	BCP_COLINFO *bcpcol;
	int i;


	if (IS_TDS7_PLUS(tds)) {

		/* 
		 * Deep joy! For TDS 8 we have to send a colmetadata message followed by row data 
		 */
		tds_put_byte(tds, colmetadata_token);	/* 0x81 */

		tds_put_smallint(tds, dbproc->bcp.db_colcount);


		for (i = 0; i < dbproc->bcp.db_colcount; i++) {
			bcpcol = dbproc->bcp.db_columns[i];
			tds_put_smallint(tds, bcpcol->db_usertype);
			tds_put_smallint(tds, bcpcol->db_flags);
			tds_put_byte(tds, bcpcol->on_server.column_type);

			switch (bcpcol->db_varint_size) {
			case 4:
				tds_put_int(tds, bcpcol->db_length);
				break;
			case 2:
				tds_put_smallint(tds, bcpcol->db_length);
				break;
			case 1:
				tds_put_byte(tds, bcpcol->db_length);
				break;
			case 0:
				break;
			default:
				assert(bcpcol->db_varint_size <= 4);
			}

			if (is_numeric_type(bcpcol->db_type)) {
				tds_put_byte(tds, bcpcol->db_prec);
				tds_put_byte(tds, bcpcol->db_scale);
			}
			if (IS_TDS80(tds)
			    && is_collate_type(bcpcol->on_server.column_type)) {
				tds_put_n(tds, bcpcol->db_collate, 5);
			}
			if (is_blob_type(bcpcol->db_type)) {
				tds_put_smallint(tds, strlen(dbproc->bcp.tablename));
				tds_put_string(tds, dbproc->bcp.tablename, strlen(dbproc->bcp.tablename));
			}
			tds_put_byte(tds, strlen(bcpcol->db_name));
			tds_put_string(tds, bcpcol->db_name, strlen(bcpcol->db_name));

		}
	}
	return SUCCEED;
}


RETCODE
bcp_readfmt(DBPROCESS * dbproc, char *filename)
{
	FILE *ffile;
	char buffer[1024];

	float lf_version = 0.0;
	int li_numcols = 0;
	int colinfo_count = 0;

	struct fflist
	{
		struct fflist *nextptr;
		BCP_HOSTCOLINFO colinfo;
	};

	struct fflist *topptr = (struct fflist *) NULL;
	struct fflist *curptr = (struct fflist *) NULL;

	BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if ((ffile = fopen(filename, "r")) == (FILE *) NULL) {
		_bcp_err_handler(dbproc, SYBEBUOF);
		return (FAIL);
	}

	if ((fgets(buffer, sizeof(buffer), ffile)) != (char *) NULL) {
		buffer[strlen(buffer) - 1] = '\0';	/* discard newline */
		lf_version = atof(buffer);
	}

	if ((fgets(buffer, sizeof(buffer), ffile)) != (char *) NULL) {
		buffer[strlen(buffer) - 1] = '\0';	/* discard newline */
		li_numcols = atoi(buffer);
	}

	while ((fgets(buffer, sizeof(buffer), ffile)) != (char *) NULL) {

		buffer[strlen(buffer) - 1] = '\0';	/* discard newline */


		if (topptr == (struct fflist *) NULL) {	/* first time */
			if ((topptr = (struct fflist *) malloc(sizeof(struct fflist))) == (struct fflist *) NULL) {
				fprintf(stderr, "out of memory\n");
				return (FAIL);
			}
			curptr = topptr;
			curptr->nextptr = NULL;
			if (_bcp_readfmt_colinfo(dbproc, buffer, &(curptr->colinfo)))
				colinfo_count++;
			else
				return (FAIL);
		} else {
			if ((curptr->nextptr = (struct fflist *) malloc(sizeof(struct fflist))) == (struct fflist *) NULL) {
				fprintf(stderr, "out of memory\n");
				return (FAIL);
			}
			curptr = curptr->nextptr;
			curptr->nextptr = NULL;
			if (_bcp_readfmt_colinfo(dbproc, buffer, &(curptr->colinfo)))
				colinfo_count++;
			else
				return (FAIL);
		}

	}
	if (fclose(ffile) != 0) {
		_bcp_err_handler(dbproc, SYBEBUCF);
		return (FAIL);
	}

	if (colinfo_count != li_numcols)
		return (FAIL);

	if (bcp_columns(dbproc, li_numcols) == FAIL) {
		return (FAIL);
	}

	for (curptr = topptr; curptr->nextptr != (struct fflist *) NULL; curptr = curptr->nextptr) {
		hostcol = &(curptr->colinfo);
		if (bcp_colfmt(dbproc, hostcol->host_column, hostcol->datatype,
			       hostcol->prefix_len, hostcol->column_len,
			       hostcol->terminator, hostcol->term_len, hostcol->tab_colnum) == FAIL) {
			return (FAIL);
		}
	}
	hostcol = &(curptr->colinfo);
	if (bcp_colfmt(dbproc, hostcol->host_column, hostcol->datatype,
		       hostcol->prefix_len, hostcol->column_len,
		       hostcol->terminator, hostcol->term_len, hostcol->tab_colnum) == FAIL) {
		return (FAIL);
	}

	return (SUCCEED);
}

int
_bcp_readfmt_colinfo(DBPROCESS * dbproc, char *buf, BCP_HOSTCOLINFO * ci)
{

	char *tok;
	int whichcol;
	char term[30];
	int i;

	enum nextcol
	{
		HOST_COLUMN,
		DATATYPE,
		PREFIX_LEN,
		COLUMN_LEN,
		TERMINATOR,
		TAB_COLNUM,
		NO_MORE_COLS
	};

	tok = strtok(buf, " \t");
	whichcol = HOST_COLUMN;

	while (tok != (char *) NULL && whichcol != NO_MORE_COLS) {
		switch (whichcol) {

		case HOST_COLUMN:
			ci->host_column = atoi(tok);

			if (ci->host_column < 1) {
				_bcp_err_handler(dbproc, SYBEBIHC);
				return (FALSE);
			}

			whichcol = DATATYPE;
			break;

		case DATATYPE:
			if (strcmp(tok, "SYBCHAR") == 0)
				ci->datatype = SYBCHAR;
			else if (strcmp(tok, "SYBTEXT") == 0)
				ci->datatype = SYBTEXT;
			else if (strcmp(tok, "SYBBINARY") == 0)
				ci->datatype = SYBBINARY;
			else if (strcmp(tok, "SYBIMAGE") == 0)
				ci->datatype = SYBIMAGE;
			else if (strcmp(tok, "SYBINT1") == 0)
				ci->datatype = SYBINT1;
			else if (strcmp(tok, "SYBINT2") == 0)
				ci->datatype = SYBINT2;
			else if (strcmp(tok, "SYBINT4") == 0)
				ci->datatype = SYBINT4;
			else if (strcmp(tok, "SYBINT8") == 0)
				ci->datatype = SYBINT8;
			else if (strcmp(tok, "SYBFLT8") == 0)
				ci->datatype = SYBFLT8;
			else if (strcmp(tok, "SYBREAL") == 0)
				ci->datatype = SYBREAL;
			else if (strcmp(tok, "SYBBIT") == 0)
				ci->datatype = SYBBIT;
			else if (strcmp(tok, "SYBNUMERIC") == 0)
				ci->datatype = SYBNUMERIC;
			else if (strcmp(tok, "SYBDECIMAL") == 0)
				ci->datatype = SYBDECIMAL;
			else if (strcmp(tok, "SYBMONEY") == 0)
				ci->datatype = SYBMONEY;
			else if (strcmp(tok, "SYBDATETIME") == 0)
				ci->datatype = SYBDATETIME;
			else if (strcmp(tok, "SYBDATETIME4") == 0)
				ci->datatype = SYBDATETIME4;
			else {
				_bcp_err_handler(dbproc, SYBEBUDF);
				return (FALSE);
			}

			whichcol = PREFIX_LEN;
			break;

		case PREFIX_LEN:
			ci->prefix_len = atoi(tok);
			whichcol = COLUMN_LEN;
			break;
		case COLUMN_LEN:
			ci->column_len = atoi(tok);
			whichcol = TERMINATOR;
			break;
		case TERMINATOR:

			if (*tok++ != '\"')
				return (FALSE);

			for (i = 0; *tok != '\"' && i < 30; i++) {
				if (*tok == '\\') {
					tok++;
					switch (*tok) {
					case 't':
						term[i] = '\t';
						break;
					case 'n':
						term[i] = '\n';
						break;
					case 'r':
						term[i] = '\r';
						break;
					case '\\':
						term[i] = '\\';
						break;
					case '0':
						term[i] = '\0';
						break;
					default:
						return (FALSE);
					}
					tok++;
				} else
					term[i] = *tok++;
			}

			if (*tok != '\"')
				return (FALSE);

			term[i] = '\0';
			ci->terminator = (BYTE *) malloc(i + 1);
			strcpy((char *) ci->terminator, term);
			ci->term_len = strlen(term);

			whichcol = TAB_COLNUM;
			break;

		case TAB_COLNUM:
			ci->tab_colnum = atoi(tok);
			whichcol = NO_MORE_COLS;
			break;

		}
		tok = strtok((char *) NULL, " \t");
	}
	if (whichcol == NO_MORE_COLS)
		return (TRUE);
	else
		return (FALSE);
}

RETCODE
bcp_writefmt(DBPROCESS * dbproc, char *filename)
{
	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	return SUCCEED;
}

RETCODE
bcp_moretext(DBPROCESS * dbproc, DBINT size, BYTE * text)
{
	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	return SUCCEED;
}

DBINT
bcp_batch(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	int rows_copied = 0;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return -1;
	}

	tds_flush_packet(tds);

	tds->state = TDS_QUERYING;

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	rows_copied = tds->rows_affected;

	_bcp_start_new_batch(dbproc);

	return (rows_copied);
}

/* end the transfer of data from program variables */

DBINT
bcp_done(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	int rows_copied = -1;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return -1;
	}
	tds_flush_packet(tds);

	tds->state = TDS_QUERYING;

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;
	rows_copied = tds->rows_affected;

	_bcp_clear_storage(dbproc);

	return (rows_copied);

}

/* bind a program host variable to a database column */

RETCODE
bcp_bind(DBPROCESS * dbproc, BYTE * varaddr, int prefixlen, DBINT varlen,
	 BYTE * terminator, int termlen, int type, int table_column)
{

	BCP_HOSTCOLINFO *hostcol;

	if (dbproc->bcp.direction == 0) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->bcp.hostfile != (char *) NULL) {
		_bcp_err_handler(dbproc, SYBEBCPB);
		return FAIL;
	}

	if (dbproc->bcp.direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	if (varlen < -1) {
		_bcp_err_handler(dbproc, SYBEBCVLEN);
		return FAIL;
	}

	if (prefixlen != 0 && prefixlen != 1 && prefixlen != 2 && prefixlen != 4) {
		_bcp_err_handler(dbproc, SYBEBCBPREF);
		return FAIL;
	}

	if (prefixlen == 0 && varlen == -1 && termlen == -1 && !is_fixed_type(type)) {
		return FAIL;
	}

	if (is_fixed_type(type) && (varlen != -1 && varlen != 0)) {
		return FAIL;
	}

	if (table_column > dbproc->bcp.host_colcount) {
		return FAIL;
	}

	if (varaddr == (BYTE *) NULL && (prefixlen != 0 || termlen != 0)
		) {
		_bcp_err_handler(dbproc, SYBEBCBNPR);
		return FAIL;
	}


	hostcol = dbproc->bcp.host_columns[table_column - 1];

	hostcol->host_var = varaddr;
	hostcol->datatype = type;
	hostcol->prefix_len = prefixlen;
	hostcol->column_len = varlen;
	hostcol->terminator = (BYTE *) malloc(termlen + 1);
	memcpy(hostcol->terminator, terminator, termlen);
	hostcol->term_len = termlen;
	hostcol->tab_colnum = table_column;

	return SUCCEED;
}

/* 
 * For a bcp in from program variables, collate all the data
 * into the column arrays held in dbproc.
 */
RETCODE
_bcp_get_prog_data(DBPROCESS * dbproc)
{
	BCP_COLINFO *bcpcol = NULL;
	BCP_HOSTCOLINFO *hostcol;

	int i;
	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;
	TDS_INT desttype;
	int collen;
	int data_is_null;
	int bytes_read;
	int converted_data_size;

	BYTE *dataptr;

	/* for each host file column defined by calls to bcp_colfmt */

	for (i = 0; i < dbproc->bcp.host_colcount; i++) {

		hostcol = dbproc->bcp.host_columns[i];

		dataptr = (BYTE *) hostcol->host_var;

		data_is_null = 0;
		collen = 0;

		/* If a prefix length specified, read the correct  amount of data. */

		if (hostcol->prefix_len > 0) {

			switch (hostcol->prefix_len) {
			case 1:
				memcpy(&ti, dataptr, 1);
				dataptr += 1;
				collen = ti;
				break;
			case 2:
				memcpy(&si, dataptr, 2);
				dataptr += 2;
				collen = si;
				break;
			case 4:
				memcpy(&li, dataptr, 4);
				dataptr += 4;
				collen = li;
				break;
			}
			if (collen == 0)
				data_is_null = 1;

		}

		/* if (Max) column length specified take that into consideration. */

		if (!data_is_null && hostcol->column_len >= 0) {
			if (hostcol->column_len == 0)
				data_is_null = 1;
			else {
				if (collen)
					collen = (hostcol->column_len < collen) ? hostcol->column_len : collen;
				else
					collen = hostcol->column_len;
			}
		}

		/* Fixed Length data - this overrides anything else specified */

		if (is_fixed_type(hostcol->datatype)) {
			collen = tds_get_size_by_type(hostcol->datatype);
		}

		/* 
		 * If this host file column contains table data,
		 * find the right element in the table/column list 
		 */
		if (hostcol->tab_colnum) {
			bcpcol = dbproc->bcp.db_columns[hostcol->tab_colnum - 1];
			if (bcpcol->tab_colnum != hostcol->tab_colnum) {
				_bcp_err_handler(dbproc, SYBEBIHC); /* probably bogus */
				return FAIL;
			}
		}

		/* read the data, finally */

		if (hostcol->term_len > 0) {	/* terminated field */
			bytes_read = _bcp_get_term_var(dataptr, hostcol->terminator, hostcol->term_len);

			if (collen)
				collen = (bytes_read < collen) ? bytes_read : collen;
			else
				collen = bytes_read;

			if (collen == 0)
				data_is_null = 1;

			if (hostcol->tab_colnum) {
				if (data_is_null) {
					bcpcol->data_size = -1;
				} else {
					desttype = tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length);

					if ((converted_data_size =
					     dbconvert(dbproc, hostcol->datatype,
						       (BYTE *) dataptr, collen,
						       desttype, bcpcol->data, bcpcol->db_length)) == FAIL) {
						return (FAIL);
					}

					bcpcol->data_size = converted_data_size;
				}
			}
		} else {	/* no terminator */

			if (hostcol->tab_colnum) {
				if (data_is_null) {
					bcpcol->data_size = -1;
				} else {
					desttype = tds_get_conversion_type(bcpcol->db_type, bcpcol->db_length);

					if ((converted_data_size =
					     dbconvert(dbproc, hostcol->datatype,
						       (BYTE *) dataptr, collen,
						       desttype, bcpcol->data, bcpcol->db_length)) == FAIL) {
						return (FAIL);
					}

					bcpcol->data_size = converted_data_size;
				}
			}
		}

		if (bcpcol->data_size == -1) {	/* Are we trying to insert a NULL? */
			if (!bcpcol->db_nullable) {
				/* too bad if the column is not nullable */
				_bcp_err_handler(dbproc, SYBEBCNN);
				return (FAIL);
			}
		}
	}
	return SUCCEED;
}

/**
 * Get the data for bcp-in from program variables, where the program data
 * have been identified as character terminated,  
 * This is a low-level, internal function.  Call it correctly.  
 */
RETCODE
_bcp_get_term_var(BYTE * pdata, BYTE * term, int term_len)
{
	int bufpos;

	assert(term_len > 0);

	/* if bufpos becomes negative, we probably failed to find the terminator */
	for (bufpos = 0; bufpos >= 0 && memcmp(pdata, term, term_len) != 0; pdata++) {
		bufpos++;
	}
	
	assert(bufpos > 0);
	return bufpos;
}

static int
rtrim(char *istr, int ilen)
{
	char *t;
	int olen = ilen;

	for (t = istr + (ilen - 1); *t == ' '; t--) {
		*t = '\0';
		olen--;
	}
	return olen;
}

RETCODE
_bcp_clear_storage(DBPROCESS * dbproc)
{

	int i;

	if (dbproc->bcp.hostfile) {
		free(dbproc->bcp.hostfile);
		dbproc->bcp.hostfile = (char *) NULL;
	}

	if (dbproc->bcp.errorfile) {
		free(dbproc->bcp.errorfile);
		dbproc->bcp.errorfile = (char *) NULL;
	}

	if (dbproc->bcp.tablename) {
		free(dbproc->bcp.tablename);
		dbproc->bcp.tablename = (char *) NULL;
	}

	if (dbproc->bcp.insert_stmt) {
		free(dbproc->bcp.insert_stmt);
		dbproc->bcp.insert_stmt = (char *) NULL;
	}

	dbproc->bcp.direction = 0;

	if (dbproc->bcp.db_columns) {
		for (i = 0; i < dbproc->bcp.db_colcount; i++) {
			if (dbproc->bcp.db_columns[i]->data) {
				free(dbproc->bcp.db_columns[i]->data);
				dbproc->bcp.db_columns[i]->data = NULL;
			}
			free(dbproc->bcp.db_columns[i]);
			dbproc->bcp.db_columns[i] = NULL;
		}
		free(dbproc->bcp.db_columns);
		dbproc->bcp.db_columns = NULL;
	}

	dbproc->bcp.db_colcount = 0;

	/* free up storage that holds details of hostfile columns */

	if (dbproc->bcp.host_columns) {
		for (i = 0; i < dbproc->bcp.host_colcount; i++) {
			if (dbproc->bcp.host_columns[i]->terminator) {
				free(dbproc->bcp.host_columns[i]->terminator);
				dbproc->bcp.host_columns[i]->terminator = NULL;
			}
			free(dbproc->bcp.host_columns[i]);
			dbproc->bcp.host_columns[i] = NULL;
		}
		free(dbproc->bcp.host_columns);
		dbproc->bcp.host_columns = NULL;
	}

	dbproc->bcp.host_colcount = 0;

	dbproc->var_cols = 0;
	dbproc->sendrow_init = 0;

	return (SUCCEED);

}

static int
_bcp_err_handler(DBPROCESS * dbproc, int bcp_errno)
{
	char buffer[80];
	const char *errmsg = NULL;
	char *p;
	int severity;
	int erc;

	switch (bcp_errno) {


	case SYBEMEM:
		errmsg = "Unable to allocate sufficient memory.";
		severity = EXRESOURCE;
		break;

	case SYBETTS:
		errmsg = "The table which bulk-copy is attempting to copy to a "
			"host-file is shorter than the number of rows which bulk-copy " "was instructed to skip.";
		severity = EXUSER;
		break;

	case SYBEBDIO:
		errmsg = "Bad bulk-copy direction. Must be either IN or OUT.";
		severity = EXPROGRAM;
		break;

	case SYBEBCVH:
		errmsg = "bcp_exec() may be called only after bcp_init() has " "been passed a valid host file.";
		severity = EXPROGRAM;
		break;

	case SYBEBIVI:
		errmsg = "bcp_columns(), bcp_colfmt() and * bcp_colfmt_ps() may be used "
			"only after bcp_init() has been passed a valid input file.";
		severity = EXPROGRAM;
		break;

	case SYBEBCBC:
		errmsg = "bcp_columns() must be called before bcp_colfmt().";
		severity = EXPROGRAM;
		break;

	case SYBEBCFO:
		errmsg = "Bcp host-files must contain at least one column.";
		severity = EXUSER;
		break;

	case SYBEBCPB:
		errmsg = "bcp_bind(), bcp_moretext() and bcp_sendrow() * may NOT be used "
			"after bcp_init() has been passed a non-NULL input file name.";
		severity = EXPROGRAM;
		break;

	case SYBEBCPN:
		errmsg = "bcp_bind(), bcp_collen(), bcp_colptr(), bcp_moretext() and "
			"bcp_sendrow() may be used only after bcp_init() has been called " "with the copy direction set to DB_IN.";
		severity = EXPROGRAM;
		break;

	case SYBEBCPI:
		errmsg = "bcp_init() must be called before any other bcp routines.";
		severity = EXPROGRAM;
		break;

	case SYBEBCITBNM:
		errmsg = "bcp_init(): tblname parameter cannot be NULL.";
		severity = EXPROGRAM;
		break;

	case SYBEBCITBLEN:
		errmsg = "bcp_init(): tblname parameter is too long.";
		severity = EXPROGRAM;
		break;

	case SYBEBCBNPR:
		errmsg = "bcp_bind(): if varaddr is NULL, prefixlen must be 0 and no " "terminator should be ** specified.";
		severity = EXPROGRAM;
		break;

	case SYBEBCBPREF:
		errmsg = "Illegal prefix length. Legal values are 0, 1, 2 or 4.";
		severity = EXPROGRAM;
		break;

	case SYBEVDPT:
		errmsg = "For bulk copy, all variable-length data * must have either " "a length-prefix or a terminator specified.";
		severity = EXUSER;
		break;

	case SYBEBCPCTYP:
		errmsg = "bcp_colfmt(): If table_colnum is 0, ** host_type cannot be 0.";
		severity = EXPROGRAM;
		break;

	case SYBEBCHLEN:
		errmsg = "host_collen should be greater than or equal to -1.";
		severity = EXPROGRAM;
		break;

	case SYBEBCPREF:
		errmsg = "Illegal prefix length. Legal values are -1, 0, 1, 2 or 4.";
		severity = EXPROGRAM;
		break;

	case SYBEBCVLEN:
		errmsg = "varlen should be greater than or equal to -1.";
		severity = EXPROGRAM;
		break;

	case SYBEBCUO:
		errmsg = "Unable to open host data-file.";
		severity = EXRESOURCE;
		break;

	case SYBEBUOE:
		erc = asprintf(&p, "Unable to open bcp error file \"%s\".", dbproc->bcp.errorfile);
		if (p && erc != -1) {
			erc = _dblib_client_msg(dbproc, bcp_errno, EXRESOURCE, p);
			free(p);
			return erc;
		}
		/* try to print silly message if unable to allocate memory */
		errmsg = "Unable to open error file.";
		severity = EXRESOURCE;
		break;

	case SYBEBUOF:
		errmsg = "Unable to open format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEBUDF:
		errmsg = "Unrecognized datatype found in format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEBIHC:
		errmsg = "Incorrect host-column number found in bcp format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEBCUC:
		errmsg = "Unable to close host data-file.";
		severity = EXRESOURCE;
		break;

	case SYBEBUCE:
		errmsg = "Unable to close error file.";
		severity = EXPROGRAM;
		break;

	case SYBEBUCF:
		errmsg = "Unable to close format-file.";
		severity = EXPROGRAM;
		break;

	case SYBEIFNB:
		errmsg = "Illegal field number passed to bcp_control().";
		severity = EXPROGRAM;
		break;

	case SYBEBCRE:
		errmsg = "I/O error while reading bcp data-file.";
		severity = EXNONFATAL;
		break;

	case SYBEBCNN:
		errmsg = "Attempt to bulk-copy a NULL value into Server column which does not accept NULL values.";
		severity = EXUSER;
		break;

	case SYBEBBCI:
		errmsg = "Batch successfully bulk-copied to SQL Server.";
		severity = EXINFO;
		break;

	case SYBEBEOF:
		errmsg = "Unexpected EOF encountered in BCP data-file.";
		severity = EXUSER;	/* ? */
		break;

	case SYBEBULKINSERT:	 
		errmsg = "Unrecognized bcp datatype in server table.";
		severity = EXCONSISTENCY;
		break;

	case SYBEBPROEXTRES:	 /* also used when unable to build bulk insert statement */
		errmsg = "bcp protocol error: unexpected set of results received.";
		severity = EXCONSISTENCY;
		break;

	default:
		sprintf(buffer, "Unknown bcp error (#%d)", bcp_errno);
		errmsg = buffer;
		severity = EXCONSISTENCY;
		break;
	}

	assert(errmsg);

	return _dblib_client_msg(dbproc, bcp_errno, severity, errmsg);
}
