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

#define HOST_COL_CONV_ERROR 1
#define HOST_COL_NULL_ERROR 2

#define BCP_REC_FETCH_DATA   1
#define BCP_REC_NOFETCH_DATA 0

#ifndef MAX
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#endif

typedef struct _pbcb
{
	char *pb;
	int cb;
	unsigned int from_malloc;
}
TDS_PBCB;

static char software_version[] = "$Id: bcp.c,v 1.116 2005-02-01 13:01:08 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static RETCODE _bcp_build_bcp_record(DBPROCESS * dbproc, TDS_INT *record_len, int behaviour);
static RETCODE _bcp_build_bulk_insert_stmt(TDSSOCKET *, TDS_PBCB *, TDSCOLUMN *, int);
static RETCODE _bcp_free_storage(DBPROCESS * dbproc);
static RETCODE _bcp_get_col_data(DBPROCESS * dbproc, TDSCOLUMN *bindcol);
static RETCODE _bcp_send_colmetadata(DBPROCESS *);
static RETCODE _bcp_start_copy_in(DBPROCESS *);
static RETCODE _bcp_start_new_batch(DBPROCESS *);

static int rtrim(char *, int);
static int _bcp_err_handler(DBPROCESS * dbproc, int bcp_errno);
static long int _bcp_measure_terminated_field(FILE * hostfile, BYTE * terminator, int term_len);


RETCODE
bcp_init(DBPROCESS * dbproc, const char *tblname, const char *hfile, const char *errfile, int direction)
{

	TDSSOCKET *tds = dbproc->tds_socket;

	TDSRESULTINFO *resinfo;
	TDSRESULTINFO *bindinfo;
	TDSCOLUMN *curcol;

	TDS_INT result_type;
	int i, rc;

	/* free allocated storage in dbproc & initialise flags, etc. */

	_bcp_free_storage(dbproc);

	/* check validity of parameters */

	if (tblname == (char *) NULL) {
		_bcp_err_handler(dbproc, SYBEBCITBNM);
		return (FAIL);
	}

	if (strlen(tblname) > 92) {	/* 30.30.30 */
		_bcp_err_handler(dbproc, SYBEBCITBLEN);
		return (FAIL);
	}

	if (direction != DB_IN && direction != DB_OUT && direction != DB_QUERYOUT) {
		_bcp_err_handler(dbproc, SYBEBDIO);
		return (FAIL);
	}

	if (hfile != (char *) NULL) {

		dbproc->hostfileinfo = calloc(1, sizeof(BCP_HOSTFILEINFO));

		dbproc->hostfileinfo->hostfile = strdup(hfile);

		if (errfile != (char *) NULL)
			dbproc->hostfileinfo->errorfile = strdup(errfile);
	} else {
		dbproc->hostfileinfo = NULL;
	}

	dbproc->bcpinfo = malloc(sizeof(DB_BCPINFO));
	if (dbproc->bcpinfo == NULL) {
		return (FAIL);
	}

	memset(dbproc->bcpinfo, '\0', sizeof(DB_BCPINFO));

	dbproc->bcpinfo->tablename = strdup(tblname);

	dbproc->bcpinfo->direction = direction;

	dbproc->bcpinfo->xfer_init  = 0;
	dbproc->bcpinfo->var_cols   = 0;
	dbproc->bcpinfo->bind_count = 0;

	if (direction == DB_IN) {
		if (tds_submit_queryf(tds, "SET FMTONLY ON select * from %s SET FMTONLY OFF", dbproc->bcpinfo->tablename) == TDS_FAIL) {
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
		if ((bindinfo = tds_alloc_results(resinfo->num_cols)) == NULL) {
			return FAIL;
		}
	
		bindinfo->row_size = resinfo->row_size;
	
		for (i = 0; i < bindinfo->num_cols; i++) {
	
			curcol = bindinfo->columns[i];
			
			curcol->column_type = resinfo->columns[i]->column_type;
			curcol->column_usertype = resinfo->columns[i]->column_usertype;
			curcol->column_flags = resinfo->columns[i]->column_flags;
			curcol->column_size = resinfo->columns[i]->column_size;
			curcol->column_varint_size = resinfo->columns[i]->column_varint_size;
			curcol->column_prec = resinfo->columns[i]->column_prec;
			curcol->column_scale = resinfo->columns[i]->column_scale;
			curcol->column_namelen = resinfo->columns[i]->column_namelen;
			curcol->on_server.column_type = resinfo->columns[i]->on_server.column_type;
			curcol->on_server.column_size = resinfo->columns[i]->on_server.column_size;
			curcol->char_conv = resinfo->columns[i]->char_conv;
			memcpy(curcol->column_name, resinfo->columns[i]->column_name, resinfo->columns[i]->column_namelen);
			curcol->column_nullable = resinfo->columns[i]->column_nullable;
			curcol->column_identity = resinfo->columns[i]->column_identity;
			curcol->column_timestamp = resinfo->columns[i]->column_timestamp;
			
			memcpy(curcol->column_collation, resinfo->columns[i]->column_collation, 5);
			
			if (is_numeric_type(curcol->column_type)) {
				curcol->bcp_column_data = tds_alloc_bcp_column_data(sizeof(TDS_NUMERIC));
				((TDS_NUMERIC *) curcol->bcp_column_data->data)->precision = curcol->column_prec;
				((TDS_NUMERIC *) curcol->bcp_column_data->data)->scale = curcol->column_scale;
			} else {
				curcol->bcp_column_data = tds_alloc_bcp_column_data(curcol->on_server.column_size);
			}
		}
	
		bindinfo->current_row = tds_alloc_row(bindinfo);
	
		dbproc->bcpinfo->bindinfo = bindinfo;
		dbproc->bcpinfo->bind_count = 0;
	
	}

	return SUCCEED;
}


RETCODE
bcp_collen(DBPROCESS * dbproc, DBINT varlen, int table_column)
{
	TDSCOLUMN *curcol;

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->bcpinfo->direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	if (dbproc->hostfileinfo != NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (table_column < 0 || table_column > dbproc->bcpinfo->bindinfo->num_cols)
		return FAIL;

	curcol = dbproc->bcpinfo->bindinfo->columns[table_column - 1];
	curcol->column_bindlen = varlen;

	return SUCCEED;
}

RETCODE
bcp_columns(DBPROCESS * dbproc, int host_colcount)
{

	int i;

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->hostfileinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBIVI);
		return FAIL;
	}

	if (host_colcount < 1) {
		_bcp_err_handler(dbproc, SYBEBCFO);
		return FAIL;
	}

	dbproc->hostfileinfo->host_colcount = host_colcount;
	dbproc->hostfileinfo->host_columns = (BCP_HOSTCOLINFO **) malloc(host_colcount * sizeof(BCP_HOSTCOLINFO *));

	for (i = 0; i < host_colcount; i++) {
		dbproc->hostfileinfo->host_columns[i] = (BCP_HOSTCOLINFO *) malloc(sizeof(BCP_HOSTCOLINFO));
		memset(dbproc->hostfileinfo->host_columns[i], '\0', sizeof(BCP_HOSTCOLINFO));
	}

	return SUCCEED;
}

RETCODE
bcp_colfmt(DBPROCESS * dbproc, int host_colnum, int host_type, int host_prefixlen, DBINT host_collen, const BYTE * host_term,
	   int host_termlen, int table_colnum)
{
	BCP_HOSTCOLINFO *hostcol;

	/* Microsoft specifies a "file_termlen" of zero if there's no terminator */
	if (dbproc->msdblib && host_termlen == 0)
		host_termlen = -1;

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->hostfileinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBIVI);
		return FAIL;
	}

	if (dbproc->hostfileinfo->host_colcount == 0) {
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


	hostcol = dbproc->hostfileinfo->host_columns[host_colnum - 1];

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
	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	return SUCCEED;
}



RETCODE
bcp_control(DBPROCESS * dbproc, int field, DBINT value)
{
	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	switch (field) {

	case BCPMAXERRS:
		dbproc->hostfileinfo->maxerrs = value;
		break;
	case BCPFIRST:
		dbproc->hostfileinfo->firstrow = value;
		break;
	case BCPLAST:
		dbproc->hostfileinfo->firstrow = value;
		break;
	case BCPBATCH:
		dbproc->hostfileinfo->batch = value;
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
	static const char *const hints[] = {
		"ORDER", "ROWS_PER_BATCH", "KILOBYTES_PER_BATCH", "TABLOCK", "CHECK_CONSTRAINTS", NULL
	};

	if (!dbproc)
		return FAIL;

	switch (option) {
	case BCPLABELED:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED bcp option: BCPLABELED\n");
		return FAIL;
	case BCPHINTS:
		if (!value || valuelen <= 0)
			return FAIL;

		if (dbproc->bcpinfo->hint != NULL)	/* hint already set */
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
		dbproc->bcpinfo->hint = (char *) malloc(1 + valuelen);
		memcpy(dbproc->bcpinfo->hint, value, valuelen);
		dbproc->bcpinfo->hint[valuelen] = '\0';	/* null terminate it */
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED bcp option: %u\n", option);
		return FAIL;
	}

	return SUCCEED;
}

RETCODE
bcp_colptr(DBPROCESS * dbproc, BYTE * colptr, int table_column)
{
	TDSCOLUMN *curcol;

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	if (dbproc->bcpinfo->direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	if (table_column < 0 || table_column > dbproc->bcpinfo->bindinfo->num_cols)
		return FAIL;

	curcol = dbproc->bcpinfo->bindinfo->columns[table_column - 1];
	curcol->column_varaddr = (TDS_CHAR *)colptr;

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
	TDSCOLUMN *curcol = NULL;
	BCP_HOSTCOLINFO *hostcol;
	BYTE *src;
	int srctype;
	int srclen;
	int buflen;
	int destlen;
	int plen;

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

	if (!(hostfile = fopen(dbproc->hostfileinfo->hostfile, "w"))) {
		_bcp_err_handler(dbproc, SYBEBCUO);
		return FAIL;
	}

	bcpdatefmt = getenv("FREEBCP_DATEFMT");

	if (dbproc->bcpinfo->direction == DB_QUERYOUT ) {
		if (tds_submit_queryf(tds, "%s", dbproc->bcpinfo->tablename)
		    == TDS_FAIL) {
			return FAIL;
		}
	} else {
		if (tds_submit_queryf(tds, "select * from %s", dbproc->bcpinfo->tablename)
		    == TDS_FAIL) {
			return FAIL;
		}
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

	row_of_query = 0;
	rows_written = 0;

	/* before we start retrieving the data, go through the defined */
	/* host file columns. If the host file column is related to a  */
	/* table column, then allocate some space sufficient to hold   */
	/* the resulting data (converted to whatever host file format) */

	for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {

		hostcol = dbproc->hostfileinfo->host_columns[i];
		if ((hostcol->tab_colnum < 1)
		    || (hostcol->tab_colnum > resinfo->num_cols)
           ) {
			continue;
		}

		if (hostcol->tab_colnum) {

			curcol = resinfo->columns[hostcol->tab_colnum - 1];

			if (hostcol->datatype == 0)
				hostcol->datatype = curcol->column_type;

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
				switch (curcol->column_type) {
				case SYBVARCHAR:
					buflen = curcol->column_size + 1;
					destlen = -1;
					break;
				case SYBCHAR:
					buflen = curcol->column_size + 1;
					if (curcol->column_nullable)
						destlen = -1;
					else
						destlen = -2;
					break;
				case SYBTEXT:
					buflen = curcol->column_size + 1;
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

			hostcol->bcp_column_data = tds_alloc_bcp_column_data(buflen);
			hostcol->bcp_column_data->datalen = destlen;

		}
	}

	/* fetch a row of data from the server */

	while (tds_process_row_tokens(tds, &rowtype, &computeid) == TDS_SUCCEED) {

		row_of_query++;

		/* skip rows outside of the firstrow/lastrow range , if specified */
		if (dbproc->hostfileinfo->firstrow <= row_of_query && 
						      row_of_query <= MAX(dbproc->hostfileinfo->lastrow, 0x7FFFFFFF)) {

			/* Go through the hostfile columns, finding those that relate to database columns. */
			for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
		
				hostcol = dbproc->hostfileinfo->host_columns[i];
				if (hostcol->tab_colnum < 1 || hostcol->tab_colnum > resinfo->num_cols) {
					continue;
				}
		
				curcol = resinfo->columns[hostcol->tab_colnum - 1];

				src = &resinfo->current_row[curcol->column_offset];

				if (is_blob_type(curcol->column_type)) {
					src = (BYTE *) ((TDSBLOB *) src)->textvalue;
				}

				srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);

				if (curcol->column_cur_size < 0) {
					srclen = 0;
					hostcol->bcp_column_data->null_column = 1;
				} else {
					if (is_numeric_type(curcol->column_type))
						srclen = sizeof(TDS_NUMERIC);
					else
						srclen = curcol->column_cur_size;
					hostcol->bcp_column_data->null_column = 0;
				}

				if (hostcol->bcp_column_data->null_column) {
					buflen = 0;
				} else {

					/* if we are converting datetime to string, need to override any 
					 * date time formats already established
					 */
					if ((srctype == SYBDATETIME || srctype == SYBDATETIME4)
					    && (hostcol->datatype == SYBCHAR || hostcol->datatype == SYBVARCHAR)) {
						memset(&when, 0, sizeof(when));

						tds_datecrack(srctype, src, &when);
						if (bcpdatefmt) 
							buflen = tds_strftime((TDS_CHAR *)hostcol->bcp_column_data->data, 256, bcpdatefmt, &when);
						else
							buflen = tds_strftime((TDS_CHAR *)hostcol->bcp_column_data->data, 256, "%Y-%m-%d %H:%M:%S.%z", &when);
					} else {
						/* 
						 * For null columns, the above work to determine the output buffer size is moot, 
						 * because bcpcol->data_size is zero, so dbconvert() won't write anything, and returns zero. 
						 */
						buflen =  dbconvert(dbproc, srctype, src, srclen, hostcol->datatype, 
								    hostcol->bcp_column_data->data, hostcol->bcp_column_data->datalen);
						/* 
						 * Special case:  When outputting database varchar data 
						 * (either varchar or nullable char) dbconvert may have
						 * trimmed trailing blanks such that nothing is left.  
						 * In this case we need to put a single blank to the output file.
						 */
						if (( curcol->column_type == SYBVARCHAR || 
							 (curcol->column_type == SYBCHAR && curcol->column_nullable)
						    ) && srclen > 0 && buflen == 0) {
							strcpy ((char *)hostcol->bcp_column_data->data, " ");
							buflen = 1;
						}
					}
				}

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
					fwrite(hostcol->bcp_column_data->data, buflen, 1, hostfile);

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

	if (dbproc->hostfileinfo->firstrow > 0 && row_of_query < dbproc->hostfileinfo->firstrow) {
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
	TDSCOLUMN *bcpcol = NULL;
	BCP_HOSTCOLINFO *hostcol;

	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;
	TDS_INT desttype;
	BYTE *coldata;

	int i, collen, data_is_null;

	/* for each host file column defined by calls to bcp_colfmt */

	for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
		tdsdump_log(TDS_DBG_FUNC, "parsing host column %d\n", i + 1);
		hostcol = dbproc->hostfileinfo->host_columns[i];

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

		/* if (Max) column length specified take that into consideration. (Meaning what, exactly?) */

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

		tdsdump_log(TDS_DBG_FUNC, "prefix_len = %d collen = %d \n", hostcol->prefix_len, collen);

		/* Fixed Length data - this overrides anything else specified */

		if (is_fixed_type(hostcol->datatype)) {
			collen = tds_get_size_by_type(hostcol->datatype);
		}

		/* 
		 * If this host file column contains table data,
		 * find the right element in the table/column list.  
		 */

		if (hostcol->tab_colnum) {
			bcpcol = dbproc->bcpinfo->bindinfo->columns[hostcol->tab_colnum - 1];
		}

		/*
		 * The data file either contains prefixes stating the length, or is delimited.  
		 * If delimited, we "measure" the field by looking for the terminator, then read it, 
		 * and set collen to the field's post-iconv size.  
		 */
		if (hostcol->term_len > 0) { /* delimited data file */
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
				if (bcpcol->on_server.column_size > bcpcol->column_size)
					collen = (collen * bcpcol->on_server.column_size) / bcpcol->column_size;
				cd = bcpcol->char_conv->to_wire;
				tdsdump_log(TDS_DBG_FUNC, "Adjusted collen is %d.\n", collen);

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
			file_bytes_left = tds_iconv_fread(cd, hostfile, collen, hostcol->term_len, (TDS_CHAR *)coldata, &col_bytes_left);
			collen -= col_bytes_left;

			/* tdsdump_log(TDS_DBG_FUNC, "collen is %d after tds_iconv_fread()\n", collen); */

			if (file_bytes_left != 0) {
				tdsdump_log(TDS_DBG_FUNC, "col %d: %d of %d bytes unread\nfile_bytes_left != 0!\n", 
							(i+1), file_bytes_left, collen);
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
			bcpcol = dbproc->bcpinfo->bindinfo->columns[hostcol->tab_colnum - 1];
			if (collen == 0 || bcpcol->column_nullable) {
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

				tdsdump_log(TDS_DBG_FUNC, "Length read from hostfile: collen is now %d, data_is_null is %d\n", collen, data_is_null);
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
				tdsdump_log(TDS_DBG_FUNC, "Reading %d bytes from hostfile.\n", collen);
				if (fread(coldata, collen, 1, hostfile) != 1) {
					free(coldata);
					if (i == 0 && feof(hostfile))
						tdsdump_log(TDS_DBG_FUNC, "Normal end-of-file reached while loading bcp data file.\n");
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
			tdsdump_log(TDS_DBG_FUNC, "Normal end-of-file reached while loading bcp data file.\n");
			return (FAIL);
		}

		/* 
		 * At this point, however the field was read, however big it was, its address is coldata and its size is collen.
		 */
		tdsdump_log(TDS_DBG_FUNC, "Data read from hostfile: collen is now %d, data_is_null is %d\n", collen, data_is_null);
		if (hostcol->tab_colnum) {
			if (data_is_null) {
				bcpcol->bcp_column_data->null_column = 1;
				bcpcol->bcp_column_data->datalen = 0;
			} else {
				bcpcol->bcp_column_data->null_column = 0;
				desttype = tds_get_conversion_type(bcpcol->column_type, bcpcol->column_size);

				/* special hack for text columns */

				if (bcpcol->column_size == 4096 && collen > bcpcol->column_size) {	/* "4096" might not really matter */
					BYTE *oldbuffer = bcpcol->bcp_column_data->data;
					switch (desttype) {
					case SYBTEXT:
					case SYBNTEXT:
					case SYBIMAGE:
					case SYBVARBINARY:
					case XSYBVARBINARY:
					case SYBLONGBINARY:	/* Reallocate enough space for the data from the file. */
						bcpcol->column_size = 8 + collen;	/* room to breathe */
						bcpcol->bcp_column_data->data = (BYTE *) realloc(bcpcol->bcp_column_data->data, bcpcol->column_size);
						if (!bcpcol->bcp_column_data->data) {
							_bcp_err_handler(dbproc, SYBEMEM);
							free(oldbuffer);
							free(coldata);
							return FAIL;
						}
						break;
					default:
						break;
					}
				}
				/* end special hack for text columns */

				bcpcol->bcp_column_data->datalen =
					dbconvert(dbproc, hostcol->datatype, coldata, collen, desttype,
						  bcpcol->bcp_column_data->data, bcpcol->column_size);

				if (bcpcol->bcp_column_data->datalen == -1) {
					hostcol->column_error = HOST_COL_CONV_ERROR;
					*row_error = 1;
					tdsdump_log(TDS_DBG_FUNC, 
						    "_bcp_read_hostfile failed to convert %d bytes at offset 0x%lx in the data file.\n", 
						    collen, (unsigned long int) ftell(hostfile) - collen);
				}

				/* trim trailing blanks from character data */
				if (desttype == SYBCHAR || desttype == SYBVARCHAR) {
					bcpcol->bcp_column_data->datalen = rtrim((char *) bcpcol->bcp_column_data->data, 
											  bcpcol->bcp_column_data->datalen);
				}
			}
			if (!hostcol->column_error) {
				if (bcpcol->bcp_column_data->datalen <= 0) {	/* Are we trying to insert a NULL ? */
					if (!bcpcol->column_nullable) {
						/* too bad if the column is not nullable */
						hostcol->column_error = HOST_COL_NULL_ERROR;
						*row_error = 1;
						_bcp_err_handler(dbproc, SYBEBCNN);
					}
				}
			}
		}
		free(coldata);
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
			assert(0); /* should not arrive here */
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
_bcp_add_fixed_columns(DBPROCESS * dbproc, int behaviour, BYTE * rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	TDSCOLUMN *bcpcol;
	int cpbytes;

	int i, j;

	for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {

		bcpcol = dbproc->bcpinfo->bindinfo->columns[i];

		if (!is_nullable_type(bcpcol->column_type) && !(bcpcol->column_nullable)) {

			tdsdump_log(TDS_DBG_FUNC, "_bcp_add_fixed_columns column %d is a fixed column\n", i + 1);

			if (behaviour == BCP_REC_FETCH_DATA) { 
				if ((_bcp_get_col_data(dbproc, bcpcol)) != SUCCEED) {
					tdsdump_log(TDS_DBG_INFO1, "bcp_get_colData (column %d) failed\n", i + 1);
		 			return FAIL;
				}
			}

			if (bcpcol->bcp_column_data->null_column) {
				_bcp_err_handler(dbproc, SYBEBCNN);
				return FAIL;
			}

			if (is_numeric_type(bcpcol->column_type)) {
				num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
				cpbytes = tds_numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
			} else {
				cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ? bcpcol->column_size : bcpcol->bcp_column_data->datalen;
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

/*
 * Add variable size columns to the row
 */
static int
_bcp_add_variable_columns(DBPROCESS * dbproc, int behaviour, BYTE * rowbuffer, int start, int *var_cols)
{
	TDSCOLUMN *bcpcol;
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

	for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {

		bcpcol = dbproc->bcpinfo->bindinfo->columns[i];

		/* is this column of "variable" type, i.e. NULLable */
		/* or naturally variable length e.g. VARCHAR        */

		if (is_nullable_type(bcpcol->column_type) || bcpcol->column_nullable) {

			tdsdump_log(TDS_DBG_FUNC, "_bcp_add_variable_columns column %d is a variable column\n", i + 1);

			if (behaviour == BCP_REC_FETCH_DATA) { 
				if ((_bcp_get_col_data(dbproc, bcpcol)) != SUCCEED) {
		 			return FAIL;
				}
			}

			/* but if its a NOT NULL column, and we have no data */
			/* throw an error                                    */

			if (!(bcpcol->column_nullable) && bcpcol->bcp_column_data->null_column) {
				_bcp_err_handler(dbproc, SYBEBCNN);
				return FAIL;
			}

			if (is_blob_type(bcpcol->column_type)) {
				cpbytes = 16;
				bcpcol->column_textpos = row_pos;               /* save for data write */
			} else if (is_numeric_type(bcpcol->column_type)) {
				num = (TDS_NUMERIC *) bcpcol->bcp_column_data->data;
				cpbytes = tds_numeric_bytes_per_prec[num->precision];
				memcpy(&rowbuffer[row_pos], num->array, cpbytes);
			} else {
				/* compute the length to copy to the row ** buffer */
				if (bcpcol->bcp_column_data->null_column) {
					cpbytes = 0;
				} else {
					cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ? bcpcol->column_size : bcpcol->bcp_column_data->datalen;
					memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
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
	int record_len;

	unsigned char row_token = 0xd1;

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->bcpinfo->direction != DB_IN) {
		_bcp_err_handler(dbproc, SYBEBCPN);
		return FAIL;
	}

	if (dbproc->hostfileinfo != NULL) {
		_bcp_err_handler(dbproc, SYBEBCPB);
		return FAIL;
	}

	/* 
	 * The first time sendrow is called after bcp_init,
	 * there is a certain amount of initialisation to be done.
	 */
	if (dbproc->bcpinfo->xfer_init == 0) {

		/* first call the start_copy function, which will */
		/* retrieve details of the database table columns */

		if (_bcp_start_copy_in(dbproc) == FAIL) {
			_bcp_err_handler(dbproc, SYBEBULKINSERT);
			return (FAIL);
		}

		/* set packet type to send bulk data */
		tds->out_flag = 0x07;
		tds_set_state(tds, TDS_QUERYING);

		if (IS_TDS7_PLUS(tds)) {
			_bcp_send_colmetadata(dbproc);
		}

		dbproc->bcpinfo->xfer_init = 1;

	}

	if (_bcp_build_bcp_record(dbproc, &record_len, BCP_REC_FETCH_DATA) == SUCCEED) {

		if (IS_TDS7_PLUS(tds)) {
			tds_put_byte(tds, row_token);   /* 0xd1 */
			tds_put_n(tds, dbproc->bcpinfo->bindinfo->current_row, record_len);
		}
	}

	return SUCCEED;
}


static RETCODE
_bcp_exec_in(DBPROCESS * dbproc, DBINT * rows_copied)
{
	FILE *hostfile, *errfile = NULL;
	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_HOSTCOLINFO *hostcol;

	int i;
	int record_len;
	int row_of_hostfile;
	int rows_written_so_far;

	int row_error, row_error_count;
	long row_start, row_end;
	int error_row_size;
	char *row_in_error;
	
	*rows_copied = 0;
	
	if (!(hostfile = fopen(dbproc->hostfileinfo->hostfile, "r"))) {
		_bcp_err_handler(dbproc, SYBEBCUO);
		return FAIL;
	}

	if (dbproc->hostfileinfo->errorfile) {
		if (!(errfile = fopen(dbproc->hostfileinfo->errorfile, "w"))) {
			_bcp_err_handler(dbproc, SYBEBUOE);
			return FAIL;
		}
	}

	if (_bcp_start_copy_in(dbproc) == FAIL) {
		fclose(errfile);
		return (FAIL);
	}

	tds->out_flag = 0x07;
	tds_set_state(tds, TDS_QUERYING);

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

				for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
					hostcol = dbproc->hostfileinfo->host_columns[i];
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
				row_in_error = malloc(error_row_size);

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
			if (row_error_count > dbproc->hostfileinfo->maxerrs)
				break;
		} else {
			if (dbproc->hostfileinfo->firstrow <= row_of_hostfile && 
							      row_of_hostfile <= MAX(dbproc->hostfileinfo->lastrow, 0x7FFFFFFF)) {

				if (_bcp_build_bcp_record(dbproc, &record_len, BCP_REC_NOFETCH_DATA) == SUCCEED) {
			
					if (IS_TDS7_PLUS(tds)) {
						tds_put_byte(tds, TDS_ROW_TOKEN);   /* 0xd1 */
						tds_put_n(tds, dbproc->bcpinfo->bindinfo->current_row, record_len);
					}

					rows_written_so_far++;
	
					if (dbproc->hostfileinfo->batch > 0 && rows_written_so_far == dbproc->hostfileinfo->batch) {
						rows_written_so_far = 0;
	
						tds_flush_packet(tds);
	
						tds_set_state(tds, TDS_PENDING);
	
						if (tds_process_simple_query(tds) != TDS_SUCCEED) {
							fclose(errfile);
							return FAIL;
						}
							
						*rows_copied += tds->rows_affected;
	
						_bcp_err_handler(dbproc, SYBEBBCI); /* batch copied to server */
	
						_bcp_start_new_batch(dbproc);
	
					}
				}
			}
		}

		row_start = ftell(hostfile);
		row_error = 0;
	}

	if (errfile) {
		fclose(errfile);
		errfile = NULL;
	}

	if (fclose(hostfile) != 0) {
		_bcp_err_handler(dbproc, SYBEBCUC);
		return (FAIL);
	}

	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	*rows_copied += tds->rows_affected;

	return SUCCEED;
}

RETCODE
bcp_exec(DBPROCESS * dbproc, DBINT * rows_copied)
{
	RETCODE ret = 0;

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	if (dbproc->hostfileinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCVH);
		return FAIL;
	}

	if (dbproc->bcpinfo->direction == DB_OUT || dbproc->bcpinfo->direction == DB_QUERYOUT) {
		ret = _bcp_exec_out(dbproc, rows_copied);
	} else if (dbproc->bcpinfo->direction == DB_IN) {
		ret = _bcp_exec_in(dbproc, rows_copied);
	}
	_bcp_free_storage(dbproc);
	return ret;
}


static RETCODE
_bcp_start_copy_in(DBPROCESS * dbproc)
{

	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOLUMN *bcpcol;

	int i;
	int firstcol;

	int fixed_col_len_tot     = 0;
	int variable_col_len_tot  = 0;
	int column_bcp_data_size  = 0;
	int bcp_record_size       = 0;

	char *query;

	if (IS_TDS7_PLUS(tds)) {
		int erc;
		char *hint;
		TDS_PBCB colclause;
		char clause_buffer[4096] = { 0 };

		colclause.pb = clause_buffer;
		colclause.cb = sizeof(clause_buffer);
		colclause.from_malloc = 0;

		firstcol = 1;

		for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
			bcpcol = dbproc->bcpinfo->bindinfo->columns[i];

			if (dbproc->bcpinfo->identity_insert_on) {
				if (!bcpcol->column_timestamp) {
					_bcp_build_bulk_insert_stmt(tds, &colclause, bcpcol, firstcol);
					firstcol = 0;
				}
			} else {
				if (!bcpcol->column_identity && !bcpcol->column_timestamp) {
					_bcp_build_bulk_insert_stmt(tds, &colclause, bcpcol, firstcol);
					firstcol = 0;
				}
			}
		}

		if (dbproc->bcpinfo->hint) {
			if (asprintf(&hint, " with (%s)", dbproc->bcpinfo->hint) < 0) {
				return FAIL;
			}
		} else {
			hint = strdup("");
		}
		if (!hint)
			return FAIL;

		erc = asprintf(&query, "insert bulk %s (%s) %s", dbproc->bcpinfo->tablename, colclause.pb, hint);

		free(hint);
		if (colclause.from_malloc)
			TDS_ZERO_FREE(colclause.pb);	/* just for good measure; not used beyond this point */

		if (erc < 0) {
			return FAIL;
		}

	} else {
		if (asprintf(&query, "insert bulk %s", dbproc->bcpinfo->tablename) < 0) {
			return FAIL;
		}
	}

	tds_submit_query(tds, query);

	/* save the statement for later... */

	dbproc->bcpinfo->insert_stmt = query;

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
	dbproc->bcpinfo->var_cols = 0;

	if (IS_TDS50(tds)) {
		for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
	
			bcpcol = dbproc->bcpinfo->bindinfo->columns[i];

			/* work out storage required for thsi datatype */
			/* blobs always require 16, numerics vary, the */
			/* rest can be taken from the server           */

			if (is_blob_type(bcpcol->on_server.column_type))
				column_bcp_data_size  = 16;
			else if (is_numeric_type(bcpcol->on_server.column_type))
				column_bcp_data_size  = tds_numeric_bytes_per_prec[bcpcol->column_prec];
			else
				column_bcp_data_size  = bcpcol->column_size;

			/* now add that size into either fixed or variable */
			/* column totals...                                */

			if (is_nullable_type(bcpcol->on_server.column_type) || bcpcol->column_nullable) {
				dbproc->bcpinfo->var_cols++;
				variable_col_len_tot += column_bcp_data_size;
			}
			else {
				fixed_col_len_tot += column_bcp_data_size;
			}
		}

		/* this formula taken from sybase manual... */

		bcp_record_size =  	4 +
							fixed_col_len_tot +
							variable_col_len_tot +
							( (int)(variable_col_len_tot / 256 ) + 1 ) +
							(dbproc->bcpinfo->var_cols + 1) +
							2;

		tdsdump_log(TDS_DBG_FUNC, "current_record_size = %d\n", dbproc->bcpinfo->bindinfo->row_size);
		tdsdump_log(TDS_DBG_FUNC, "bcp_record_size     = %d\n", bcp_record_size);

		if (bcp_record_size > dbproc->bcpinfo->bindinfo->row_size) {
			dbproc->bcpinfo->bindinfo->current_row = realloc(dbproc->bcpinfo->bindinfo->current_row, bcp_record_size);
			if (dbproc->bcpinfo->bindinfo->current_row == NULL) {
				tdsdump_log(TDS_DBG_FUNC, "could not realloc current_row\n");
				return FAIL;
			}
			dbproc->bcpinfo->bindinfo->row_size = bcp_record_size;
		}
	}
	if (IS_TDS7_PLUS(tds)) {
		for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
	
			bcpcol = dbproc->bcpinfo->bindinfo->columns[i];

			/* dont send the (meta)data for timestamp columns, or   */
			/* identity columns (unless indentity_insert is enabled */

			if ((!dbproc->bcpinfo->identity_insert_on && bcpcol->column_identity) || 
				bcpcol->column_timestamp) {
				continue;
			}

			switch (bcpcol->column_varint_size) {
				case 4:
					if (is_blob_type(bcpcol->column_type)) {
						bcp_record_size += 25;
					}
					bcp_record_size += 4;
					break;
				case 2:
					bcp_record_size +=2;
					break;
				case 1:
					bcp_record_size++;
					break;
				case 0:
					break;
			}

			if (is_numeric_type(bcpcol->column_type)) {
				bcp_record_size += tds_numeric_bytes_per_prec[bcpcol->column_prec];
			} else {
				bcp_record_size += bcpcol->column_size;
			}
		}
		tdsdump_log(TDS_DBG_FUNC, "current_record_size = %d\n", dbproc->bcpinfo->bindinfo->row_size);
		tdsdump_log(TDS_DBG_FUNC, "bcp_record_size     = %d\n", bcp_record_size);

		if (bcp_record_size > dbproc->bcpinfo->bindinfo->row_size) {
			dbproc->bcpinfo->bindinfo->current_row = realloc(dbproc->bcpinfo->bindinfo->current_row, bcp_record_size);
			if (dbproc->bcpinfo->bindinfo->current_row == NULL) {
				tdsdump_log(TDS_DBG_FUNC, "could not realloc current_row\n");
				return FAIL;
			}
			dbproc->bcpinfo->bindinfo->row_size = bcp_record_size;
		}
	}

	return SUCCEED;
}

static RETCODE
_bcp_build_bulk_insert_stmt(TDSSOCKET * tds, TDS_PBCB * clause, TDSCOLUMN * bcpcol, int first)
{
	char buffer[32];
	char *column_type = buffer;

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
		tdsdump_log(TDS_DBG_FUNC, "error: cannot build bulk insert statement. unrecognized server datatype %d\n", 
					bcpcol->on_server.column_type);
		return FAIL;
	}

	if (clause->cb < strlen(clause->pb) + tds_quote_id(tds, NULL, bcpcol->column_name, bcpcol->column_namelen) + strlen(column_type) + ((first) ? 2 : 4)) {
		char *temp = (char *) malloc(2 * clause->cb);

		if (!temp)
			return FAIL;
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

	return SUCCEED;
}

static RETCODE
_bcp_start_new_batch(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	tds_submit_query(tds, dbproc->bcpinfo->insert_stmt);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	tds->out_flag = 0x07;
	tds_set_state(tds, TDS_QUERYING);

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
	TDSCOLUMN *bcpcol;
	int i;
	int num_cols;

	/* 
	 * Deep joy! For TDS 8 we have to send a colmetadata message followed by row data 
	 */
	tds_put_byte(tds, colmetadata_token);	/* 0x81 */

	num_cols = 0;
	for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
		bcpcol = dbproc->bcpinfo->bindinfo->columns[i];
		if ((!dbproc->bcpinfo->identity_insert_on && bcpcol->column_identity) ||
			bcpcol->column_timestamp) {
			continue;
		}
		num_cols++;
	}

	tds_put_smallint(tds, num_cols);

	for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
		bcpcol = dbproc->bcpinfo->bindinfo->columns[i];

        /* dont send the (meta)data for timestamp columns, or   */
        /* identity columns (unless indentity_insert is enabled */

        if ((!dbproc->bcpinfo->identity_insert_on && bcpcol->column_identity) ||
            bcpcol->column_timestamp) {
            continue;
        }

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
		if (IS_TDS80(tds)
		    && is_collate_type(bcpcol->on_server.column_type)) {
			tds_put_n(tds, bcpcol->column_collation, 5);
		}
		if (is_blob_type(bcpcol->on_server.column_type)) {
			tds_put_smallint(tds, strlen(dbproc->bcpinfo->tablename));
			tds_put_string(tds, dbproc->bcpinfo->tablename, strlen(dbproc->bcpinfo->tablename));
		}
		tds_put_byte(tds, bcpcol->column_namelen);
		tds_put_string(tds, bcpcol->column_name, bcpcol->column_namelen);

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

	if (dbproc->bcpinfo == NULL) {
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
	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}
	return SUCCEED;
}

RETCODE
bcp_moretext(DBPROCESS * dbproc, DBINT size, BYTE * text)
{
	if (dbproc->bcpinfo == NULL) {
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

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return -1;
	}

	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

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

	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return -1;
	}
	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;
	rows_copied = tds->rows_affected;

	_bcp_free_storage(dbproc);

	return (rows_copied);

}

/* bind a program host variable to a database column */

RETCODE
bcp_bind(DBPROCESS * dbproc, BYTE * varaddr, int prefixlen, DBINT varlen,
	 BYTE * terminator, int termlen, int vartype, int table_column)
{

	TDSCOLUMN *colinfo;

	if (dbproc->bcpinfo == NULL) {
	if (dbproc->bcpinfo == NULL) {
		_bcp_err_handler(dbproc, SYBEBCPI);
		return FAIL;
	}

	if (dbproc->hostfileinfo != NULL) {
		_bcp_err_handler(dbproc, SYBEBCPB);
		return FAIL;
	}

	if (dbproc->bcpinfo->direction != DB_IN) {
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

	if (prefixlen == 0 && varlen == -1 && termlen == -1 && !is_fixed_type(vartype)) {
		return FAIL;
	}

	if (is_fixed_type(vartype) && (varlen != -1 && varlen != 0)) {
		return FAIL;
	}

	if (table_column > dbproc->bcpinfo->bindinfo->num_cols)
		return FAIL;
	}

	if (varaddr == (BYTE *) NULL && (prefixlen != 0 || termlen != 0)
		) {
		_bcp_err_handler(dbproc, SYBEBCBNPR);
		return FAIL;
	}

	colinfo = dbproc->bcpinfo->bindinfo->columns[table_column - 1];

	colinfo->column_varaddr  = (char *)varaddr;
	colinfo->column_bindtype = vartype;
	colinfo->column_bindlen  = varlen;
	colinfo->bcp_terminator =  malloc(termlen + 1);
	memcpy(colinfo->bcp_terminator, terminator, termlen);
	colinfo->bcp_term_len = termlen;

	return SUCCEED;
}

static RETCODE
_bcp_build_bcp_record(DBPROCESS * dbproc, TDS_INT *record_len, int behaviour)
{
	TDSSOCKET  *tds = dbproc->tds_socket;
	TDSCOLUMN  *bindcol;

	static const unsigned char CHARBIN_NULL[] = { 0xff, 0xff };
	static const unsigned char GEN_NULL = 0x00;
	static const unsigned char textptr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
											 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	static const unsigned char timestamp[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	static const TDS_TINYINT textptr_size = 16;

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

	tdsdump_log(TDS_DBG_FUNC, "_bcp_build_bcp_record\n");

	record = dbproc->bcpinfo->bindinfo->current_row;
	old_record_size = dbproc->bcpinfo->bindinfo->row_size;
	new_record_size = 0;

	if (IS_TDS7_PLUS(tds)) {

		for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
	
			bindcol = dbproc->bcpinfo->bindinfo->columns[i];

			/* dont send the (meta)data for timestamp columns, or   */
			/* identity columns (unless indentity_insert is enabled */

			if ((!dbproc->bcpinfo->identity_insert_on && bindcol->column_identity) || 
				bindcol->column_timestamp) {
				continue;
			}

			if (behaviour == BCP_REC_FETCH_DATA) { 
				if ((_bcp_get_col_data(dbproc, bindcol)) != SUCCEED) {
					tdsdump_log(TDS_DBG_INFO1, "bcp_get_colData (column %d) failed\n", i + 1);
		 			return FAIL;
				}
				tdsdump_log(TDS_DBG_INFO1, "gotten column %d length %d null %d\n",
							i + 1, bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->null_column);
			}
			tdsdump_log(TDS_DBG_INFO1, "column %d length %d null %d\n",
						i + 1, bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->null_column);
	
			if (bindcol->bcp_column_data->null_column) {
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
					_bcp_err_handler(dbproc, SYBEBCNN);
					return FAIL;
				}
			} else {

				switch (bindcol->column_varint_size) {
				case 4:
					if (is_blob_type(bindcol->column_type)) {
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
					if (is_numeric_type(bindcol->column_type)) { 
						varint_1 = tds_numeric_bytes_per_prec[bindcol->column_prec];
						tdsdump_log(TDS_DBG_INFO1, "numeric type prec = %d varint_1 = %d\n", bindcol->column_prec, varint_1);
					}
					else {
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
#else
				if (is_numeric_type(bindcol->column_type)) {
					tds_swap_datatype(tds_get_conversion_type(bindcol->column_type, bindcol->column_size),
										bindcol->bcp_column_data->data);
				}
#endif
				tdsdump_log(TDS_DBG_INFO1, "new_record_size = %d datalen = %d \n", 
							new_record_size, bindcol->bcp_column_data->datalen);


				if (is_numeric_type(bindcol->column_type)) {
					TDS_NUMERIC *num = (TDS_NUMERIC *) bindcol->bcp_column_data->data;
					tdsdump_log(TDS_DBG_INFO1, "numeric type prec = %d\n", num->precision);
					memcpy(record, num->array, tds_numeric_bytes_per_prec[num->precision]);
					record += tds_numeric_bytes_per_prec[num->precision]; 
					new_record_size += tds_numeric_bytes_per_prec[num->precision];
				} else {
					tdsdump_log(TDS_DBG_INFO1, "new_record_size = %d datalen = %d \n", 
								new_record_size, bindcol->bcp_column_data->datalen);
					memcpy(record, bindcol->bcp_column_data->data, bindcol->bcp_column_data->datalen);
					record += bindcol->bcp_column_data->datalen;
					new_record_size += bindcol->bcp_column_data->datalen;
				}

			}
			tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n",
					old_record_size, new_record_size);
		}
	}  /* IS_TDS7_PLUS */
	else {
			memset(record, '\0', old_record_size);	/* zero the rowbuffer */

			/* offset 0 = number of var columns */
			/* offset 1 = row number.  zeroed (datasever assigns) */
			row_pos = 2;

			if ((row_pos = _bcp_add_fixed_columns(dbproc, behaviour, record, row_pos)) == FAIL)
				return FAIL;

			row_sz_pos = row_pos;

			/* potential variable columns to write */

			if (dbproc->bcpinfo->var_cols) {
				if ((row_pos = _bcp_add_variable_columns(dbproc, behaviour, record, row_pos, &var_cols_written)) == FAIL)
					return FAIL;
			}

			row_size = row_pos;

			if (var_cols_written) {
				memcpy(&record[row_sz_pos], &row_size, sizeof(row_size));
				record[0] = var_cols_written;
			}

			tdsdump_log(TDS_DBG_INFO1, "old_record_size = %d new size = %d \n",
					old_record_size, row_size);

			tds_put_smallint(tds, row_size);
			tds_put_n(tds, record, row_size);

			/* row is done, now handle any text/image data */

			blob_cols = 0;

			for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
				bindcol = dbproc->bcpinfo->bindinfo->columns[i];
				if (is_blob_type(bindcol->column_type)) {
					if (behaviour == BCP_REC_FETCH_DATA) { 
						if ((_bcp_get_col_data(dbproc, bindcol)) != SUCCEED) {
				 			return FAIL;
						}
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
			return SUCCEED;
	}
	*record_len = new_record_size;

	return SUCCEED;
}

/* 
 * For a bcp in from program variables, get the data from 
 * the host variable
 */
static RETCODE
_bcp_get_col_data(DBPROCESS * dbproc, TDSCOLUMN *bindcol)
{
	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;
	TDS_INT desttype;
	int collen;
	int data_is_null;
	int bytes_read;
	int converted_data_size;

	BYTE *dataptr;


	dataptr = (BYTE *) bindcol->column_varaddr;

	data_is_null = 0;
	collen = 0;

	/* If a prefix length specified, read the correct  amount of data. */

	if (bindcol->bcp_prefix_len > 0) {

		switch (bindcol->bcp_prefix_len) {
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

	if (!data_is_null && bindcol->column_bindlen >= 0) {
		if (bindcol->column_bindlen == 0)
			data_is_null = 1;
		else {
			if (collen)
				collen = (bindcol->column_bindlen < collen) ? bindcol->column_bindlen : collen;
			else
				collen = bindcol->column_bindlen;
		}
	}

	/* Fixed Length data - this overrides anything else specified */

	if (is_fixed_type(bindcol->column_bindtype)) {
		collen = tds_get_size_by_type(bindcol->column_bindtype);
	}

	/* read the data, finally */

	if (bindcol->bcp_term_len > 0) {	/* terminated field */
		bytes_read = _bcp_get_term_var(dataptr, (BYTE *)bindcol->bcp_terminator, bindcol->bcp_term_len);

		if (collen)
			collen = (bytes_read < collen) ? bytes_read : collen;
		else
			collen = bytes_read;

		if (collen == 0)
			data_is_null = 1;
	}

	if (data_is_null) {
		bindcol->bcp_column_data->datalen = 0;
		bindcol->bcp_column_data->null_column = 1;
	} else {
		desttype = tds_get_conversion_type(bindcol->column_type, bindcol->column_size);

		if ((converted_data_size =
		     dbconvert(dbproc, bindcol->column_bindtype,
			       (BYTE *) dataptr, collen,
			       desttype, bindcol->bcp_column_data->data, bindcol->column_size)) == FAIL) {
			return (FAIL);
		}

		bindcol->bcp_column_data->datalen = converted_data_size;
		bindcol->bcp_column_data->null_column = 0;
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

static RETCODE
_bcp_free_storage(DBPROCESS * dbproc)
{

	int i;

	if (dbproc->hostfileinfo) {
		if (dbproc->hostfileinfo->hostfile)
			TDS_ZERO_FREE(dbproc->hostfileinfo->hostfile);
	
		if (dbproc->hostfileinfo->errorfile)
			TDS_ZERO_FREE(dbproc->hostfileinfo->errorfile);

		/* free up storage that holds details of hostfile columns */
	
		if (dbproc->hostfileinfo->host_columns) {
			for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
				if (dbproc->hostfileinfo->host_columns[i]->terminator)
					TDS_ZERO_FREE(dbproc->hostfileinfo->host_columns[i]->terminator);
				tds_free_bcp_column_data(dbproc->hostfileinfo->host_columns[i]->bcp_column_data);
				TDS_ZERO_FREE(dbproc->hostfileinfo->host_columns[i]);
			}
			TDS_ZERO_FREE(dbproc->hostfileinfo->host_columns);
		}
		TDS_ZERO_FREE(dbproc->hostfileinfo);
	}


	if (dbproc->bcpinfo) {
		if (dbproc->bcpinfo->tablename)
			TDS_ZERO_FREE(dbproc->bcpinfo->tablename);

		if (dbproc->bcpinfo->insert_stmt)
			TDS_ZERO_FREE(dbproc->bcpinfo->insert_stmt);

		if (dbproc->bcpinfo->bindinfo) {
			tds_free_results(dbproc->bcpinfo->bindinfo);
			dbproc->bcpinfo->bindinfo = NULL;
		}

		TDS_ZERO_FREE(dbproc->bcpinfo);
	}

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
		erc = asprintf(&p, "Unable to open bcp error file \"%s\".", dbproc->hostfileinfo->errorfile);
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
