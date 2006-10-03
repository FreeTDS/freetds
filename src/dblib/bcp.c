/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
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

#ifdef WIN32
#include <io.h>
#endif

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

TDS_RCSID(var, "$Id: bcp.c,v 1.150 2006-10-03 19:40:03 jklowden Exp $");

#ifdef HAVE_FSEEKO
typedef off_t offset_type;
#elif defined(WIN32)
/* win32 version */
typedef __int64 offset_type;
#define fseeko(f,o,w) (_lseeki64(fileno(f),o,w) == -1)
#define ftello(f) _telli64(fileno(f))
#else
/* use old version */
#define fseeko(f,o,w) fseek(f,o,w)
#define ftello(f) ftell(f)
typedef long offset_type;
#endif

static RETCODE _bcp_send_bcp_record(DBPROCESS * dbproc, int behaviour);
static RETCODE _bcp_build_bulk_insert_stmt(TDSSOCKET *, TDS_PBCB *, TDSCOLUMN *, int);
static RETCODE _bcp_free_storage(DBPROCESS * dbproc);
static void _bcp_free_columns(DBPROCESS * dbproc);
static RETCODE _bcp_get_col_data(DBPROCESS * dbproc, TDSCOLUMN *bindcol);
static RETCODE _bcp_send_colmetadata(DBPROCESS *);
static RETCODE _bcp_start_copy_in(DBPROCESS *);
static RETCODE _bcp_start_new_batch(DBPROCESS *);

static int rtrim(char *, int);
static offset_type _bcp_measure_terminated_field(FILE * hostfile, BYTE * terminator, int term_len);
static RETCODE _bcp_read_hostfile(DBPROCESS * dbproc, FILE * hostfile, int *row_error);
static int _bcp_readfmt_colinfo(DBPROCESS * dbproc, char *buf, BCP_HOSTCOLINFO * ci);
static RETCODE _bcp_get_term_var(BYTE * pdata, BYTE * term, int term_len);

static void bcp_row_free(TDSRESULTINFO* result, unsigned char *row);

/** 
 * \ingroup dblib_bcp
 * \brief Prepare for bulk copy operation on a table
 *
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param tblname the name of the table receiving or providing the data.
 * \param hfile the data file opposite the table, if any.  
 * \param errfile the "error file" captures messages and, if errors are encountered, 
 * 	copies of any rows that could not be written to the table.
 * \param direction one of 
 *		- \b DB_IN writing to the table
 *		- \b DB_OUT writing to the host file
 * 		.
 * \remarks bcp_init() sets the host file data format and acquires the table metadata.
 *	It is called before the other bulk copy functions. 
 *
 * 	When writing to a table, bcp can use as its data source a data file (\a hfile), 
 * 	or program data in an application's variables.  In the latter case, call bcp_bind() 
 *	to associate your data with the appropriate table column.  
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_bind(), bcp_done(), bcp_exec()
 */
RETCODE
bcp_init(DBPROCESS * dbproc, const char *tblname, const char *hfile, const char *errfile, int direction)
{
	TDSRESULTINFO *resinfo;
	TDSRESULTINFO *bindinfo;
	TDSCOLUMN *curcol;
	TDS_INT result_type;
	int i, rc;

	tdsdump_log(TDS_DBG_FUNC, "bcp_init(%p, %s, %s, %s, %d)\n", dbproc, tblname, hfile, errfile, direction);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(tblname, SYBENULP);

	/* Free previously allocated storage in dbproc & initialise flags, etc. */
	
	_bcp_free_storage(dbproc);

	/* 
	 * Validate other parameters 
	 */
	if (dbproc->tds_socket->major_version < 5) {
		dbperror(dbproc, SYBETDSVER, 0);
		return (FAIL);
	}

	if (tblname == NULL) {
		dbperror(dbproc, SYBEBCITBNM, 0);
		return (FAIL);
	}

	/* TODO even for TDS7+ ?? */
	if (strlen(tblname) > 92) {	/* 30.30.30 */
		dbperror(dbproc, SYBEBCITBLEN, 0);
		return (FAIL);
	}

	if (direction != DB_IN && direction != DB_OUT && direction != DB_QUERYOUT) {
		dbperror(dbproc, SYBEBDIO, 0);
		return (FAIL);
	}

	if (hfile != NULL) {

		dbproc->hostfileinfo = calloc(1, sizeof(BCP_HOSTFILEINFO));

		if (dbproc->hostfileinfo == NULL)
			goto memory_error;
		if ((dbproc->hostfileinfo->hostfile = strdup(hfile)) == NULL)
			goto memory_error;

		if (errfile != NULL)
			if ((dbproc->hostfileinfo->errorfile = strdup(errfile)) == NULL)
				goto memory_error;
	} else {
		dbproc->hostfileinfo = NULL;
	}

	/* Allocate storage */
	
	dbproc->bcpinfo = calloc(1, sizeof(DB_BCPINFO));
	if (dbproc->bcpinfo == NULL)
		goto memory_error;

	if ((dbproc->bcpinfo->tablename = strdup(tblname)) == NULL)
		goto memory_error;

	dbproc->bcpinfo->direction = direction;

	dbproc->bcpinfo->xfer_init  = 0;
	dbproc->bcpinfo->var_cols   = 0;
	dbproc->bcpinfo->bind_count = 0;

	if (direction == DB_IN) {
		TDSSOCKET *tds = dbproc->tds_socket;

		if (tds_submit_queryf(tds, "SET FMTONLY ON select * from %s SET FMTONLY OFF", 
								dbproc->bcpinfo->tablename) == TDS_FAIL) {
			/* Attempt to use Bulk Copy with a non-existent Server table (might be why ...) */
			dbperror(dbproc, SYBEBCNT, 0);
			return FAIL;
		}
	
		/* TODO check what happen if table is not present, cleanup on error */
		while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
		       == TDS_SUCCEED) {
		}
		if (rc != TDS_NO_MORE_RESULTS) {
			return FAIL;
		}
	
		if (!tds->res_info) {
			return FAIL;
		}
	
		/* TODO check what happen if table is not present, cleanup on error */
		resinfo = tds->res_info;
		if ((bindinfo = tds_alloc_results(resinfo->num_cols)) == NULL)
			goto memory_error;
	
		bindinfo->row_size = resinfo->row_size;
	
		dbproc->bcpinfo->bindinfo = bindinfo;
		dbproc->bcpinfo->bind_count = 0;
	
		/* Copy the column metadata */
		for (i = 0; i < bindinfo->num_cols; i++) {
	
			curcol = bindinfo->columns[i];
			
			/*
			 * TODO use memcpy ??
			 * curcol and resinfo->columns[i] are both TDSCOLUMN.  
			 * Why not "curcol = resinfo->columns[i];"?  Because the rest of TDSCOLUMN (below column_timestamp)
			 * isn't being used.  Perhaps this "upper" part of TDSCOLUMN should be a substructure.
			 * Or, see if the "lower" part is unused (and zeroed out) at this point, and just do one assignment.
			 */
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
				curcol->bcp_column_data = 
					tds_alloc_bcp_column_data(MAX(curcol->column_size,curcol->on_server.column_size));
			}
		}

		bindinfo->current_row = malloc(bindinfo->row_size);
		if (!bindinfo->current_row)
			goto memory_error;
		bindinfo->row_free = bcp_row_free;
		return SUCCEED;
	}

	return SUCCEED;

memory_error:
	_bcp_free_storage(dbproc);
	dbperror(dbproc, SYBEMEM, ENOMEM);
	return FAIL;
}


/** 
 * \ingroup dblib_bcp
 * \brief Set the length of a host variable to be written to a table.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param varlen size of the variable, in bytes, or
 * 	- \b 0 indicating NULL
 *	- \b -1 indicating size is determined by the prefix or terminator.  
 *		(If both a prefix and a terminator are present, bcp is supposed to use the smaller of the 
 *		 two.  This feature might or might not actually work.)
 * \param table_column the number of the column in the table (starting with 1, not zero).
 * 
 * \return SUCCEED or FAIL.
 * \sa 	bcp_bind(), bcp_colptr(), bcp_sendrow() 
 */
RETCODE
bcp_collen(DBPROCESS * dbproc, DBINT varlen, int table_column)
{
	TDSCOLUMN *curcol;

	tdsdump_log(TDS_DBG_FUNC, "bcp_collen(%p, %d, %d)\n", dbproc, varlen, table_column);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);

	if (dbproc->bcpinfo->direction != DB_IN) {
		dbperror(dbproc, SYBEBCPN, 0);
		return FAIL;
	}

	if (dbproc->hostfileinfo != NULL) {
		dbperror(dbproc, SYBEBCPI, 0);
		return FAIL;
	}

	if (table_column <= 0 || table_column > dbproc->bcpinfo->bindinfo->num_cols) {
		dbperror(dbproc, SYBECNOR, 0);
		return FAIL;
	}
	curcol = dbproc->bcpinfo->bindinfo->columns[table_column - 1];
	curcol->column_bindlen = varlen;

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp
 * \brief Indicate how many columns are to be found in the datafile.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param host_colcount count of columns in the datafile, irrespective of how many you intend to use. 
 * \remarks This function describes the file as it is, not how it will be used.  
 * 
 * \return SUCCEED or FAIL.  It's hard to see how it could fail.  
 * \sa 	bcp_colfmt() 	
 */
RETCODE
bcp_columns(DBPROCESS * dbproc, int host_colcount)
{
	int i;

	tdsdump_log(TDS_DBG_FUNC, "bcp_columns(%p, %d)\n", dbproc, host_colcount);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(dbproc->hostfileinfo, SYBEBIVI);

	if (host_colcount < 1) {
		dbperror(dbproc, SYBEBCFO, 0);
		return FAIL;
	}

	_bcp_free_columns(dbproc);

	/* TODO if already allocated ?? */
	dbproc->hostfileinfo->host_columns = (BCP_HOSTCOLINFO **) malloc(host_colcount * sizeof(BCP_HOSTCOLINFO *));
	if (dbproc->hostfileinfo->host_columns == NULL) {
		dbperror(dbproc, SYBEMEM, ENOMEM);
		return FAIL;
	}

	dbproc->hostfileinfo->host_colcount = host_colcount;

	for (i = 0; i < host_colcount; i++) {
		dbproc->hostfileinfo->host_columns[i] = (BCP_HOSTCOLINFO *) calloc(1, sizeof(BCP_HOSTCOLINFO));
		if (dbproc->hostfileinfo->host_columns[i] == NULL) {
			dbproc->hostfileinfo->host_colcount = i;
			_bcp_free_columns(dbproc);
			dbperror(dbproc, SYBEMEM, ENOMEM);
			return FAIL;
		}
	}

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp
 * \brief Specify the format of a datafile prior to writing to a table.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param host_colnum datafile column number (starting with 1, not zero).
 * \param host_type dataype token describing the data type in \a host_colnum.  E.g. SYBCHAR for character data.
 * \param host_prefixlen size of the prefix in the datafile column, if any.  For delimited files: zero.  
 *			May be 0, 1, 2, or 4 bytes.  The prefix will be read as an integer (not a character string) from the 
 * 			data file, and will be interpreted the data size of that column, in bytes.  
 * \param host_collen maximum size of datafile column, exclusive of any prefix/terminator.  Just the data, ma'am.  
 *		Special values:
 *			- \b 0 indicates NULL.  
 *			- \b -1 for fixed-length non-null datatypes
 *			- \b -1 for variable-length datatypes (e.g. SYBCHAR) where the length is established 
 *				by a prefix/terminator.  
 * \param host_term the sequence of characters that will serve as a column terminator (delimiter) in the datafile.  
 * 			Often a tab character, but can be any string of any length.  Zero indicates no terminator.  
 * 			Special characters:
 *				- \b '\\0' terminator is an ASCII NUL.
 *				- \b '\\t' terminator is an ASCII TAB.
 *				- \b '\\n' terminator is an ASCII NL.
 * \param host_termlen the length of \a host_term, in bytes. 
 * \param table_colnum Nth column, starting at 1, in the table that maps to \a host_colnum.  
 * 	If there is a column in the datafile that does not map to a table column, set \a table_colnum to zero.  
 *
 *\remarks  bcp_colfmt() is called once for each column in the datafile, as specified with bcp_columns().  
 * In so doing, you describe to FreeTDS how to parse each line of your datafile, and where to send each field.  
 *
 * When a prefix or terminator is used with variable-length data, \a host_collen may have one of three values:
 *		- \b positive indicating the maximum number of bytes to be used
 * 		- \b 0 indicating NULL
 *		- \b -1 indicating no maximum; all data, as described by the prefix/terminator will be used.  
 *		.
 * \return SUCCEED or FAIL.
 * \sa 	bcp_batch(), bcp_bind(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(),
 *	bcp_control(), bcp_done(), bcp_exec(), bcp_init(), bcp_sendrow
 */
RETCODE
bcp_colfmt(DBPROCESS * dbproc, int host_colnum, int host_type, int host_prefixlen, DBINT host_collen, const BYTE * host_term,
	   int host_termlen, int table_colnum)
{
	BCP_HOSTCOLINFO *hostcol;

	tdsdump_log(TDS_DBG_FUNC, "bcp_colfmt(%p, %d, %d, %d, %d, %p)\n", 
		    dbproc, host_colnum, host_type, host_prefixlen, (int) host_collen, host_term);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(dbproc->hostfileinfo, SYBEBIVI);

	/* Microsoft specifies a "file_termlen" of zero if there's no terminator */
	if (dbproc->msdblib && host_termlen == 0)
		host_termlen = -1;

	if (dbproc->hostfileinfo->host_colcount == 0) {
		dbperror(dbproc, SYBEBCBC, 0);
		return FAIL;
	}

	if (host_colnum < 1) {
		dbperror(dbproc, SYBEBCFO, 0);
		return FAIL;
	}

	if (host_prefixlen != 0 && host_prefixlen != 1 && host_prefixlen != 2 && host_prefixlen != 4 && host_prefixlen != -1) {
		dbperror(dbproc, SYBEBCPREF, 0);
		return FAIL;
	}

	if (table_colnum <= 0 && host_type == 0) {
		dbperror(dbproc, SYBEBCPCTYP, 0);
		return FAIL;
	}

	if (host_prefixlen == 0 && host_collen == -1 && host_termlen == -1 && !is_fixed_type(host_type)) {
		dbperror(dbproc, SYBEVDPT, 0);
		return FAIL;
	}

	if (host_collen < -1) {
		dbperror(dbproc, SYBEBCHLEN, 0);
		return FAIL;
	}

	/* No official error message.  Fix and warn. */
	if (is_fixed_type(host_type) && (host_collen != -1 && host_collen != 0)) {
		tdsdump_log(TDS_DBG_FUNC,
			    "bcp_colfmt: changing host_collen to -1 from %d for fixed type %d.\n", 
			    host_collen, host_type);
		host_collen = -1;
	}

	/* 
	 * If there's a positive terminator length, we need a valid terminator pointer.
	 * If the terminator length is 0 or -1, then there's no terminator.
	 * FIXME: look up the correct error code for a bad terminator pointer or length and return that before arriving here.   
	 */
	if (host_termlen > 0 && host_term == NULL) {
		dbperror(dbproc, SYBEVDPT, 0);	/* "all variable-length data must have either a length-prefix ..." */
		return FAIL;
	}

	hostcol = dbproc->hostfileinfo->host_columns[host_colnum - 1];

	/* TODO add precision scale and join with bcp_colfmt_ps */
	hostcol->host_column = host_colnum;
	hostcol->datatype = host_type;
	hostcol->prefix_len = host_prefixlen;
	hostcol->column_len = host_collen;
	if (host_term && host_termlen >= 0) {
		hostcol->terminator = (BYTE *) malloc(host_termlen);
		memcpy(hostcol->terminator, host_term, host_termlen);
	}
	hostcol->term_len = host_termlen;
	hostcol->tab_colnum = table_colnum;

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp
 * \brief Specify the format of a host file for bulk copy purposes, 
 * 	with precision and scale support for numeric and decimal columns.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param host_colnum 
 * \param host_type 
 * \param etc.
 * \todo Not implemented.
 * 
 * \return SUCCEED or FAIL.
 * \sa 	bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_collen(), bcp_colptr(), bcp_columns(),
 *	bcp_control(), bcp_done(), bcp_exec(), bcp_init(), bcp_sendrow
 */
RETCODE
bcp_colfmt_ps(DBPROCESS * dbproc, int host_colnum, int host_type,
	      int host_prefixlen, DBINT host_collen, BYTE * host_term, int host_termlen, int table_colnum, DBTYPEINFO * typeinfo)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED: bcp_colfmt_ps(%p, %d, %d)\n", dbproc, host_colnum, host_type);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	
	/* dbperror(dbproc, , 0);	 Illegal precision specified */

	/* TODO see bcp_colfmt */
	return FAIL;
}


/** 
 * \ingroup dblib_bcp
 * \brief Set BCP options for uploading a datafile
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param field symbolic constant indicating the option to be set, one of:
 *  		- \b BCPMAXERRS Maximum errors tolerated before quitting. The default is 10.
 *  		- \b BCPFIRST The first row to read in the datafile. The default is 1. 
 *  		- \b BCPLAST The last row to read in the datafile. The default is to copy all rows. A value of
 *                  	-1 resets this field to its default?
 *  		- \b BCPBATCH The number of rows per batch.  Default is 0, meaning a single batch. 
 * \param value The value for \a field.
 *
 * \remarks These options control the behavior of bcp_exec().  
 * When writing to a table from application host memory variables, 
 * program logic controls error tolerance and batch size. 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_done(), bcp_exec(), bcp_options()
 */
RETCODE
bcp_control(DBPROCESS * dbproc, int field, DBINT value)
{
	tdsdump_log(TDS_DBG_FUNC, "bcp_control(%p, %d, %d)\n", dbproc, field, value);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(dbproc->hostfileinfo, SYBEBIVI);

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
	case BCPKEEPIDENTITY:
		dbproc->bcpinfo->identity_insert_on = (value != 0);
		break;

	default:
		dbperror(dbproc, SYBEIFNB, 0);
		return FAIL;
	}
	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp
 * \brief Set "hints" for uploading a file.  A FreeTDS-only function.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param option symbolic constant indicating the option to be set, one of:
 * 		- \b BCPLABELED Not implemented.
 * 		- \b BCPHINTS The hint to be passed when the bulk-copy begins.  
 * \param value The string constant for \a option a/k/a the hint.  One of:
 * 		- \b ORDER The data are ordered in accordance with the table's clustered index.
 * 		- \b ROWS_PER_BATCH The batch size
 * 		- \b KILOBYTES_PER_BATCH The approximate number of kilobytes to use for a batch size
 * 		- \b TABLOCK Lock the table
 * 		- \b CHECK_CONSTRAINTS Apply constraints
 * \param valuelen The strlen of \a value.  
 * 
 * \return SUCCEED or FAIL.
 * \sa 	bcp_control(), 
 * 	bcp_exec(), 
 * \todo Simplify.  Remove \a valuelen, and dbproc->bcpinfo->hint = strdup(hints[i])
 */
RETCODE
bcp_options(DBPROCESS * dbproc, int option, BYTE * value, int valuelen)
{
	int i;
	static const char *const hints[] = {
		"ORDER", "ROWS_PER_BATCH", "KILOBYTES_PER_BATCH", "TABLOCK", "CHECK_CONSTRAINTS", NULL
	};

	tdsdump_log(TDS_DBG_FUNC, "bcp_options(%p, %d, %p, %d)\n", dbproc, option, value, valuelen);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(value, SYBENULP);

	switch (option) {
	case BCPLABELED:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED bcp option: BCPLABELED\n");
		break;
	case BCPHINTS:
		if (!value || valuelen <= 0)
			break;

		for (i = 0; hints[i]; i++) {	/* look up hint */
			if (strncasecmp((char *) value, hints[i], strlen(hints[i])) == 0) {
				dbproc->bcpinfo->hint = hints[i];	/* safe: hints[i] is static constant, above */
				return SUCCEED;
			}
		}
		tdsdump_log(TDS_DBG_FUNC, "failed, no such hint\n");
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED bcp option: %u\n", option);
		break;
	}
	return FAIL;
}

/** 
 * \ingroup dblib_bcp
 * \brief Override bcp_bind() by pointing to a different host variable.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param colptr The pointer, the address of your variable. 
 * \param table_column The 1-based column ordinal in the table.  
 * \remarks Use between calls to bcp_sendrow().  After calling bcp_colptr(), 
 * 		subsequent calls to bcp_sendrow() will bind to the new address.  
 * \return SUCCEED or FAIL.
 * \sa 	bcp_bind(), bcp_collen(), bcp_sendrow() 
 */
RETCODE
bcp_colptr(DBPROCESS * dbproc, BYTE * colptr, int table_column)
{
	TDSCOLUMN *curcol;

	tdsdump_log(TDS_DBG_FUNC, "bcp_colptr(%p, %p, %d)\n", dbproc, colptr, table_column);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(dbproc->bcpinfo->bindinfo, SYBEBCPI);
	/* colptr can be NULL */

	if (dbproc->bcpinfo->direction != DB_IN) {
		dbperror(dbproc, SYBEBCPN, 0);
		return FAIL;
	}
	if (table_column <= 0 || table_column > dbproc->bcpinfo->bindinfo->num_cols) {
		dbperror(dbproc, SYBEBCPN, 0);
		return FAIL;
	}
	
	curcol = dbproc->bcpinfo->bindinfo->columns[table_column - 1];
	curcol->column_varaddr = (TDS_CHAR *)colptr;

	return SUCCEED;
}


/** 
 * \ingroup dblib_bcp
 * \brief See if BCP_SETL() was used to set the LOGINREC for BCP work.  
 * 
 * \param login Address of the LOGINREC variable to be passed to dbopen(). 
 * 
 * \return TRUE or FALSE.
 * \sa 	BCP_SETL(), bcp_init(), dblogin(), dbopen()
 */
DBBOOL
bcp_getl(LOGINREC * login)
{
	TDSLOGIN *tdsl = login->tds_login;

	tdsdump_log(TDS_DBG_FUNC, "bcp_getl(%p)\n", login);

	return (tdsl->bulk_copy);
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 *
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param rows_copied 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
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

	TDS_INT result_type;

	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;

	TDSDATEREC when;

	int row_of_query;
	int rows_written;
	char *bcpdatefmt = NULL;
	int tdsret;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_exec_out(%p, %p)\n", dbproc, rows_copied);
	assert(dbproc);
	assert(rows_copied);

	tds = dbproc->tds_socket;
	assert(tds);

	if (!(hostfile = fopen(dbproc->hostfileinfo->hostfile, "w"))) {
		dbperror(dbproc, SYBEBCUO, errno);
		return FAIL;
	}

	bcpdatefmt = getenv("FREEBCP_DATEFMT");

	if (dbproc->bcpinfo->direction == DB_QUERYOUT ) {
		if (tds_submit_query(tds, dbproc->bcpinfo->tablename) == TDS_FAIL) {
			return FAIL;
		}
	} else {
		/* TODO quote if needed */
		if (tds_submit_queryf(tds, "select * from %s", dbproc->bcpinfo->tablename) == TDS_FAIL) {
			return FAIL;
		}
	}

	tdsret = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS);
	if (tdsret == TDS_FAIL || tdsret == TDS_CANCELLED) {
		fclose(hostfile);
		return FAIL;
	}
	if (!tds->res_info) {
		/* TODO flush/cancel to keep consistent state */
		fclose(hostfile);
		return FAIL;
	}

	resinfo = tds->res_info;

	row_of_query = 0;
	rows_written = 0;

	/*
	 * Before we start retrieving the data, go through the defined
	 * host file columns. If the host file column is related to a
	 * table column, then allocate some space sufficient to hold
	 * the resulting data (converted to whatever host file format)
	 */

	for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {

		hostcol = dbproc->hostfileinfo->host_columns[i];
		if (hostcol->tab_colnum < 1 || hostcol->tab_colnum > resinfo->num_cols)
			continue;

		curcol = resinfo->columns[hostcol->tab_colnum - 1];

		if (hostcol->datatype == 0)
			hostcol->datatype = curcol->column_type;

		/* work out how much space to allocate for output data */

		/* TODO use a function for fixed types */
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
				/* FIXME column_size ?? if 2gb ?? */
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

	/* fetch a row of data from the server */

	/*
	 * TODO above we allocate many buffer just to convert and store 
	 * to file.. avoid all that passages...
	 */

	while (tds_process_tokens(tds, &result_type, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE|TDS_RETURN_ROW|TDS_RETURN_COMPUTE) == TDS_SUCCEED) {

		if (result_type != TDS_ROW_RESULT && result_type != TDS_COMPUTE_RESULT)
			break;

		row_of_query++;

		/* skip rows outside of the firstrow/lastrow range , if specified */
		if (dbproc->hostfileinfo->firstrow <= row_of_query && 
						      row_of_query <= MAX(dbproc->hostfileinfo->lastrow, 0x7FFFFFFF)) {

			/* Go through the hostfile columns, finding those that relate to database columns. */
			for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
				size_t written = 0;
				hostcol = dbproc->hostfileinfo->host_columns[i];
				if (hostcol->tab_colnum < 1 || hostcol->tab_colnum > resinfo->num_cols) {
					continue;
				}
		
				curcol = resinfo->columns[hostcol->tab_colnum - 1];

				src = curcol->column_data;

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

					/*
					 * if we are converting datetime to string, need to override any 
					 * date time formats already established
					 */
					if ((srctype == SYBDATETIME || srctype == SYBDATETIME4)
					    && (hostcol->datatype == SYBCHAR || hostcol->datatype == SYBVARCHAR)) {
						tds_datecrack(srctype, src, &when);
						if (bcpdatefmt) 
							buflen = tds_strftime((TDS_CHAR *)hostcol->bcp_column_data->data, 256,
										 bcpdatefmt, &when);
						else
							buflen = tds_strftime((TDS_CHAR *)hostcol->bcp_column_data->data, 256,
										 "%Y-%m-%d %H:%M:%S.%z", &when);
					} else {
						/* 
						 * For null columns, the above work to determine the output buffer size is moot, 
						 * because bcpcol->data_size is zero, so dbconvert() won't write anything, 
						 * and returns zero. 
						 */
						/* TODO check for text !!! */
						buflen =  dbconvert(dbproc, srctype, src, srclen, hostcol->datatype, 
								    hostcol->bcp_column_data->data,
								    hostcol->bcp_column_data->datalen);
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
					if (is_blob_type(hostcol->datatype))
						plen = 4;
					else if (!(is_fixed_type(hostcol->datatype)))
						plen = 2;
					else if (curcol->column_nullable)
						plen = 1;
					else
						plen = 0;
					/* cache */
					hostcol->prefix_len = plen;
				}
				switch (plen) {
				case 0:
					break;
				case 1:
					ti = buflen;
					written = fwrite(&ti, sizeof(ti), 1, hostfile);
					break;
				case 2:
					si = buflen;
					written = fwrite(&si, sizeof(si), 1, hostfile);
					break;
				case 4:
					li = buflen;
					written = fwrite(&li, sizeof(li), 1, hostfile);
					break;
				}
				if( plen != 0 && written != 1 ) {
					dbperror(dbproc, SYBEBCWE, errno);
					return FAIL;
				}

				/* The data */
				if (hostcol->column_len != -1) {
					buflen = buflen > hostcol->column_len ? hostcol->column_len : buflen;
				}

				if (buflen > 0) {
					written = fwrite(hostcol->bcp_column_data->data, buflen, 1, hostfile);
					if (written < 1) {
						dbperror(dbproc, SYBEBCWE, errno);
						return FAIL;
					}
				}
						
				/* The terminator */
				if (hostcol->terminator && hostcol->term_len > 0) {
					written = fwrite(hostcol->terminator, hostcol->term_len, 1, hostfile);
					if (written < 1) {
						dbperror(dbproc, SYBEBCWE, errno);
						return FAIL;
					}
				}
			}
			rows_written++;
		}
	}
	if (fclose(hostfile) != 0) {
		dbperror(dbproc, SYBEBCUC, errno);
		return (FAIL);
	}

	if (dbproc->hostfileinfo->firstrow > 0 && row_of_query < dbproc->hostfileinfo->firstrow) {
		/* 
		 * The table which bulk-copy is attempting to
		 * copy to a host-file is shorter than the
		 * number of rows which bulk-copy was instructed to skip.  
		 */
		/* TODO reset TDSSOCKET state */
		dbperror(dbproc, SYBETTS, 0);
		return (FAIL);
	}

	*rows_copied = rows_written;
	return SUCCEED;
}

static RETCODE
_bcp_check_eof(DBPROCESS * dbproc, FILE *file, int icol)
{
	int errnum = errno;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_check_eof(%p, %p, %d)\n", dbproc, file, icol);
	assert(dbproc);
	assert(file);

	if (feof(file)) {
		if (icol == 0) {
			tdsdump_log(TDS_DBG_FUNC, "Normal end-of-file reached while loading bcp data file.\n");
			return (NO_MORE_ROWS);
		}
		dbperror(dbproc, SYBEBEOF, errnum);
		return (FAIL);
	} 
	dbperror(dbproc, SYBEBCRE, errnum);
	return (FAIL);
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param hostfile 
 * \param row_error 
 * 
 * \return MORE_ROWS, NO_MORE_ROWS, or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_read_hostfile(DBPROCESS * dbproc, FILE * hostfile, int *row_error)
{
	TDSCOLUMN *bcpcol = NULL;
	BCP_HOSTCOLINFO *hostcol;

	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_INT li;
	TDS_INT desttype;
	BYTE *coldata;

	int i, collen, data_is_null;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_read_hostfile(%p, %p, %p)\n", dbproc, hostfile, row_error);
	assert(dbproc);
	assert(hostfile);
	assert(row_error);

	/* for each host file column defined by calls to bcp_colfmt */

	for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
		tdsdump_log(TDS_DBG_FUNC, "parsing host column %d\n", i + 1);
		hostcol = dbproc->hostfileinfo->host_columns[i];

		data_is_null = 0;
		collen = 0;
		hostcol->column_error = 0;

		/* 
		 * If this host file column contains table data,
		 * find the right element in the table/column list.  
		 * FIXME I think tab_colnum can be out of range - freddy77
		 */
		if (hostcol->tab_colnum) {
			bcpcol = dbproc->bcpinfo->bindinfo->columns[hostcol->tab_colnum - 1];
		}

		/* detect prefix len */
		if (bcpcol && hostcol->prefix_len == -1) {
			int plen = bcpcol->column_varint_size;
			hostcol->prefix_len = plen == 5 ? 4 : plen;
		}

		/* a prefix length, if extant, specifies how many bytes to read */
		if (hostcol->prefix_len > 0) {

			switch (hostcol->prefix_len) {
			case 1:
				if (fread(&ti, 1, 1, hostfile) != 1)
					return _bcp_check_eof(dbproc, hostfile, i);
				collen = ti ? ti : -1;
				break;
			case 2:
				if (fread(&si, 2, 1, hostfile) != 1)
					return _bcp_check_eof(dbproc, hostfile, i);
				collen = si;
				break;
			case 4:
				if (fread(&li, 4, 1, hostfile) != 1)
					return _bcp_check_eof(dbproc, hostfile, i);
				collen = li;
				break;
			default:
				/* FIXME return error, remember that prefix_len can be 3 */
				assert(hostcol->prefix_len <= 4);
				break;
			}

			/* TODO test all NULL types */
			/* TODO for < -1 error */
			if (collen <= -1) {
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
		 * The data file either contains prefixes stating the length, or is delimited.  
		 * If delimited, we "measure" the field by looking for the terminator, then read it, 
		 * and set collen to the field's post-iconv size.  
		 */
		if (hostcol->term_len > 0) { /* delimited data file */
			int file_bytes_left, file_len;
			size_t col_bytes_left;
			offset_type len;
			iconv_t cd;

			len = _bcp_measure_terminated_field(hostfile, hostcol->terminator, hostcol->term_len);
			if (len > 0x7fffffffl || len < 0) {
				*row_error = TRUE;
				tdsdump_log(TDS_DBG_FUNC, "_bcp_measure_terminated_field returned -1!\n");
				dbperror(dbproc, SYBEBCOR, 0);
				return (FAIL);
			}
			collen = len;
			if (collen == 0)
				data_is_null = 1;


			tdsdump_log(TDS_DBG_FUNC, "_bcp_measure_terminated_field returned %d\n", collen);
			/* 
			 * Allocate a column buffer guaranteed to be big enough hold the post-iconv data.
			 */
			file_len = collen;
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
				dbperror(dbproc, SYBEMEM, errno);
				return (FAIL);
			}

			/* 
			 * Read and convert the data
			 */
			col_bytes_left = collen;
			/* TODO make tds_iconv_fread handle terminator directly to avoid fseek in _bcp_measure_terminated_field */
			file_bytes_left = tds_iconv_fread(cd, hostfile, file_len, hostcol->term_len, (TDS_CHAR *)coldata,
														 &col_bytes_left);
			collen -= col_bytes_left;

			/* tdsdump_log(TDS_DBG_FUNC, "collen is %d after tds_iconv_fread()\n", collen); */

			if (file_bytes_left != 0) {
				tdsdump_log(TDS_DBG_FUNC, "col %d: %d of %d bytes unread\nfile_bytes_left != 0!\n", 
							(i+1), file_bytes_left, collen);
				*row_error = TRUE;
				free(coldata);
				dbperror(dbproc, SYBEBCOR, 0);
				return FAIL;
			}

			/*
			 * TODO:  
			 *    Dates are a problem.  In theory, we should be able to read non-English dates, which
			 *    would contain non-ASCII characters.  One might suppose we should convert date
			 *    strings to ISO-8859-1 (or another canonical form) here, because tds_convert() can't be
			 *    expected to deal with encodings. But instead date strings are read verbatim and 
			 *    passed to tds_convert() without even waving to iconv().  For English dates, this works, 
			 *    because English dates expressed as UTF-8 strings are indistinguishable from the ASCII.  
			 */
		} else {	/* unterminated field */
#if 0
			bcpcol = dbproc->bcpinfo->bindinfo->columns[hostcol->tab_colnum - 1];
			if (collen == 0 || bcpcol->column_nullable) {
				if (collen != 0) {
					/* A fixed length type */
					TDS_TINYINT len;
					if (fread(&len, sizeof(len), 1, hostfile) != 1) {
						if (i != 0)
							dbperror(dbproc, SYBEBCRE, errno);
						return (FAIL);
					}
					if (len < 0)
						dbperror(dbproc, SYBEBCNL, errno);
						
					/* TODO 255 for NULL ?? check it, perhaps 0 */
					collen = len == 255 ? -1 : len;
				} else {
					TDS_SMALLINT len;

					if (fread(&len, sizeof(len), 1, hostfile) != 1) {
						if (i != 0)
							dbperror(dbproc, SYBEBCRE, errno);
						return (FAIL);
					}
					if (len < 0)
						dbperror(dbproc, SYBEBCNL, errno);
					collen = len;
				}
				/* TODO if collen < -1 error */
				if (collen <= -1) {
					collen = 0;
					data_is_null = 1;
				}

				tdsdump_log(TDS_DBG_FUNC, "Length read from hostfile: collen is now %d, data_is_null is %d\n", 
							collen, data_is_null);
			}
#endif

			coldata = (BYTE *) calloc(1, 1 + collen);
			if (coldata == NULL) {
				*row_error = TRUE;
				dbperror(dbproc, SYBEMEM, errno);
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
					return _bcp_check_eof(dbproc, hostfile, i);
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
			return NO_MORE_ROWS;
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

				if (bcpcol->column_size == 4096 && collen > bcpcol->column_size) { /* "4096" might not matter */
					BYTE *oldbuffer = bcpcol->bcp_column_data->data;
					switch (desttype) {
					case SYBTEXT:
					case SYBNTEXT:
					case SYBIMAGE:
					case SYBVARBINARY:
					case XSYBVARBINARY:
					case SYBLONGBINARY:	/* Reallocate enough space for the data from the file. */
						bcpcol->column_size = 8 + collen;	/* room to breathe */
						bcpcol->bcp_column_data->data = 
							(BYTE *) realloc(bcpcol->bcp_column_data->data, bcpcol->column_size);
						if (!bcpcol->bcp_column_data->data) {
							dbperror(dbproc, SYBEMEM, errno);
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

				/*
				 * FIXME bcpcol->bcp_column_data->data && bcpcol->column_size ??
				 * It seems a buffer overflow waiting...
				 */
				bcpcol->bcp_column_data->datalen =
					dbconvert(dbproc, hostcol->datatype, coldata, collen, desttype, 
									bcpcol->bcp_column_data->data, bcpcol->column_size);

				if (bcpcol->bcp_column_data->datalen == -1) {
					hostcol->column_error = HOST_COL_CONV_ERROR;
					*row_error = 1;
					/* FIXME possible integer overflow if off_t is 64bit and long int 32bit */
					tdsdump_log(TDS_DBG_FUNC, 
						"_bcp_read_hostfile failed to convert %d bytes at offset 0x%lx in the data file.\n", 
						    collen, (unsigned long int) ftello(hostfile) - collen);
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
						dbperror(dbproc, SYBEBCNN, 0);
					}
				}
			}
		}
		free(coldata);
	}
	return MORE_ROWS;
}

/*
 * Look for the next terminator in a host data file, and return the data size.  
 * \return size of field, excluding the terminator.  
 * \remarks The current offset will be unchanged.  If an error was encountered, the returned size will be -1.  
 * 	The caller should check for that possibility, but the appropriate message should already have been emitted.  
 * 	The caller can then use tds_iconv_fread() to read-and-convert the file's data 
 *	into host format, or, if we're not dealing with a character column, just fread(3).  
 */
/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param hostfile 
 * \param terminator 
 * \param term_len 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static offset_type
_bcp_measure_terminated_field(FILE * hostfile, BYTE * terminator, int term_len)
{
	char *sample;
	int errnum;
	int sample_size, bytes_read = 0;
	offset_type size;
	const offset_type initial_offset = ftello(hostfile);

	tdsdump_log(TDS_DBG_FUNC, "_bcp_measure_terminated_field(%p, %p, %d)\n", hostfile, terminator, term_len);

	sample = malloc(term_len);

	if (!sample) {
		dbperror(NULL, SYBEMEM, errno);
		return -1;
	}

	for (sample_size = 1; (bytes_read = fread(sample, sample_size, 1, hostfile)) != 0;) {

		bytes_read *= sample_size;

		/*
		 * Check for terminator.
		 */
		/*
		 * TODO use memchr for performance, 
		 * optimize this strange loop - freddy77
		 */
		if (*sample == *terminator) {
			if (sample_size == term_len) {
				/*
				 * If we read a whole terminator, compare the whole sequence and, if found, go home. 
				 */
				if (memcmp(sample, terminator, term_len) == 0) {
					free(sample);
					size = ftello(hostfile) - initial_offset;
					if (size < 0 || 0 != fseeko(hostfile, initial_offset, SEEK_SET)) {
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
					if (0 != fseeko(hostfile, -sample_size, SEEK_CUR)) {
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
		errnum = errno;
		if (initial_offset == ftello(hostfile)) {
			return 0;
		} else {
			/* a cheat: we don't have dbproc, so pass zero */
			dbperror(0, SYBEBEOF, errnum);
		}
	} else if (ferror(hostfile)) {
		dbperror(0, SYBEBCRE, errno);
	}

	return -1;
}

/**
 * Add fixed size columns to the row
 */
/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param behaviour 
 * \param rowbuffer 
 * \param start 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static int
_bcp_add_fixed_columns(DBPROCESS * dbproc, int behaviour, BYTE * rowbuffer, int start)
{
	TDS_NUMERIC *num;
	int row_pos = start;
	TDSCOLUMN *bcpcol;
	int cpbytes;
	int i, j;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_add_fixed_columns(%p, %d, %p, %d)\n", dbproc, behaviour, rowbuffer, start);
	assert(dbproc);
	assert(rowbuffer);

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
				dbperror(dbproc, SYBEBCNN, 0);
				return FAIL;
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

/*
 * Add variable size columns to the row
 */
/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param behaviour 
 * \param rowbuffer 
 * \param start 
 * \param var_cols 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
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

	tdsdump_log(TDS_DBG_FUNC, "_bcp_add_variable_columns(%p, %d, %p, %d, %p)\n", dbproc, behaviour, rowbuffer, start, var_cols);
	assert(dbproc);
	assert(rowbuffer);
	assert(var_cols);

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
				dbperror(dbproc, SYBEBCNN, 0);
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
					cpbytes = bcpcol->bcp_column_data->datalen > bcpcol->column_size ? 
						  bcpcol->column_size : bcpcol->bcp_column_data->datalen;
					memcpy(&rowbuffer[row_pos], bcpcol->bcp_column_data->data, cpbytes);
				}
			}

			/* if we have written data to the record for this column */

			if (cpbytes > 0) {

				/*
				 * update offset table. Each entry in the offset table is a single byte
				 * so can only hold a maximum value of 255. If the real offset is more
				 * than 255 we will have to add one or more entries in the adjust table
				 */

				offset_table[offset_pos++] = row_pos % 256;

				/* increment count of variable columns added to the record */

				num_cols++;

				/*
				 * how many times does 256 have to be added to the one byte offset to
				 * calculate the REAL offset...
				 */

				this_adjustment_increment = row_pos / 256;

				/* has this changed since we did the last column...      */

				if (this_adjustment_increment > last_adjustment_increment) {

					/*
					 * add n entries to the adjust table. each entry represents
					 * an adjustment of 256 bytes, and each entry holds the
					 * column number for which the adjustment needs to be made
					 */

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

/** 
 * \ingroup dblib_bcp
 * \brief Write data in host variables to the table.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * 
 * \remarks Call bcp_bind() first to describe the variables to be used.  
 *	Use bcp_batch() to commit sets of rows. 
 *	After sending the last row call bcp_done().
 * \return SUCCEED or FAIL.
 * \sa 	bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_collen(), bcp_colptr(), bcp_columns(), 
 * 	bcp_control(), bcp_done(), bcp_exec(), bcp_init(), bcp_moretext(), bcp_options()
 */
RETCODE
bcp_sendrow(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;

	tdsdump_log(TDS_DBG_FUNC, "bcp_sendrow(%p)\n", dbproc);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);

	tds = dbproc->tds_socket;

	if (dbproc->bcpinfo->direction != DB_IN) {
		dbperror(dbproc, SYBEBCPN, 0);
		return FAIL;
	}

	if (dbproc->hostfileinfo != NULL) {
		dbperror(dbproc, SYBEBCPB, 0);
		return FAIL;
	}

	/* 
	 * The first time sendrow is called after bcp_init,
	 * there is a certain amount of initialisation to be done.
	 */
	if (dbproc->bcpinfo->xfer_init == 0) {

		/* The start_copy function retrieves details of the table's columns */
		if (_bcp_start_copy_in(dbproc) == FAIL) {
			dbperror(dbproc, SYBEBULKINSERT, 0);
			return (FAIL);
		}

		/* set packet type to send bulk data */
		tds->out_flag = TDS_BULK;
		tds_set_state(tds, TDS_QUERYING);

		if (IS_TDS7_PLUS(tds)) {
			_bcp_send_colmetadata(dbproc);
		}

		dbproc->bcpinfo->xfer_init = 1;

	}

	return _bcp_send_bcp_record(dbproc, BCP_REC_FETCH_DATA);
}


/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param rows_copied 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_exec_in(DBPROCESS * dbproc, DBINT * rows_copied)
{
	FILE *hostfile, *errfile = NULL;
	TDSSOCKET *tds = dbproc->tds_socket;
	BCP_HOSTCOLINFO *hostcol;
	RETCODE ret;

	int i, row_of_hostfile, rows_written_so_far;
	int row_error, row_error_count;
	offset_type row_start, row_end;
	offset_type error_row_size;
	const size_t chunk_size = 0x20000u;
	
	tdsdump_log(TDS_DBG_FUNC, "_bcp_exec_in(%p, %p)\n", dbproc, rows_copied);
	assert(dbproc);
	assert(rows_copied);

	*rows_copied = 0;
	
	if (!(hostfile = fopen(dbproc->hostfileinfo->hostfile, "r"))) {
		dbperror(dbproc, SYBEBCUO, 0);
		return FAIL;
	}

	if (_bcp_start_copy_in(dbproc) == FAIL)
		return FAIL;

	tds->out_flag = TDS_BULK;
	tds_set_state(tds, TDS_QUERYING);

	if (IS_TDS7_PLUS(tds)) {
		_bcp_send_colmetadata(dbproc);
	}

	row_of_hostfile = 0;
	rows_written_so_far = 0;

	row_start = ftello(hostfile);
	row_error_count = 0;
	row_error = 0;

	while ((ret=_bcp_read_hostfile(dbproc, hostfile, &row_error)) == MORE_ROWS) {

		row_of_hostfile++;

		if (row_error) {
			int count;

			if (errfile == NULL && dbproc->hostfileinfo->errorfile) {
				if (!(errfile = fopen(dbproc->hostfileinfo->errorfile, "w"))) {
					dbperror(dbproc, SYBEBUOE, 0);
					return FAIL;
				}
			}

			if (errfile != NULL) {
				char *row_in_error = NULL;

				for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
					hostcol = dbproc->hostfileinfo->host_columns[i];
					if (hostcol->column_error == HOST_COL_CONV_ERROR) {
						count = fprintf(errfile, 
							"#@ data conversion error on host data file Row %d Column %d\n",
							row_of_hostfile, i + 1);
						if( count < 0 ) {
							dbperror(dbproc, SYBEBWEF, errno);
						}
					} else if (hostcol->column_error == HOST_COL_NULL_ERROR) {
						count = fprintf(errfile, "#@ Attempt to bulk-copy a NULL value into Server column"
								" which does not accept NULL values. Row %d, Column %d\n",
								row_of_hostfile, i + 1);
						if( count < 0 ) {
							dbperror(dbproc, SYBEBWEF, errno);
						}

					}
				}

				row_end = ftello(hostfile);

				/* error data can be very long so split in chunks */
				error_row_size = row_end - row_start;
				fseeko(hostfile, row_start, SEEK_SET);

				while (error_row_size > 0) {
					size_t chunk = error_row_size > chunk_size ? chunk_size : error_row_size;

					if (!row_in_error)
						row_in_error = malloc(chunk);

					if (fread(row_in_error, chunk, 1, hostfile) != 1) {
						printf("BILL fread failed after fseek\n");
					}
					count = fwrite(row_in_error, chunk, 1, errfile);
					if( count < chunk ) {
						dbperror(dbproc, SYBEBWEF, errno);
					}
					error_row_size -= chunk;
				}
				if (row_in_error)
					free(row_in_error);

				fseeko(hostfile, row_end, SEEK_SET);
				count = fprintf(errfile, "\n");
				if( count < 0 ) {
					dbperror(dbproc, SYBEBWEF, errno);
				}
			}
			row_error_count++;
			if (row_error_count > dbproc->hostfileinfo->maxerrs)
				break;
		} else {
			if (dbproc->hostfileinfo->firstrow <= row_of_hostfile && 
							      row_of_hostfile <= MAX(dbproc->hostfileinfo->lastrow, 0x7FFFFFFF)) {

				if (_bcp_send_bcp_record(dbproc, BCP_REC_NOFETCH_DATA) == SUCCEED) {
			
					rows_written_so_far++;
	
					if (dbproc->hostfileinfo->batch > 0 && rows_written_so_far == dbproc->hostfileinfo->batch) {
						rows_written_so_far = 0;
	
						tds_flush_packet(tds);
	
						tds_set_state(tds, TDS_PENDING);
	
						if (tds_process_simple_query(tds) != TDS_SUCCEED) {
							if (errfile)
								fclose(errfile);
							return FAIL;
						}
							
						*rows_copied += tds->rows_affected;
	
						dbperror(dbproc, SYBEBBCI, 0); /* batch copied to server */
	
						_bcp_start_new_batch(dbproc);
	
					}
				}
			}
		}

		row_start = ftello(hostfile);
		row_error = 0;
	}
	
	if( row_error_count == 0 && row_of_hostfile < dbproc->hostfileinfo->firstrow ) {
		dbperror(dbproc, SYBEBCRO, 0); /* The BCP hostfile '%1!' contains only %2! rows */
	}

	if (errfile) {
		if( 0 != fclose(errfile) )
			dbperror(dbproc, SYBEBUCE, 0);
		errfile = NULL;
	}

	if (fclose(hostfile) != 0) {
		dbperror(dbproc, SYBEBCUC, 0);
		return (FAIL);
	}

	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	*rows_copied += tds->rows_affected;

	return ret == NO_MORE_ROWS? SUCCEED : FAIL;	/* (ret is returned from _bcp_read_hostfile) */
}

/** 
 * \ingroup dblib_bcp
 * \brief Write a datafile to a table. 
 *
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param rows_copied bcp_exec will write the count of rows successfully written to this address. 
 *	If \a rows_copied is NULL, it will be ignored by db-lib. 
 *
 * \return SUCCEED or FAIL.
 * \sa 	bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_collen(), bcp_colptr(), bcp_columns(),
 *	bcp_control(), bcp_done(), bcp_init(), bcp_sendrow()
 */
RETCODE
bcp_exec(DBPROCESS * dbproc, DBINT *rows_copied)
{
	DBINT dummy_copied;
	RETCODE ret = 0;

	tdsdump_log(TDS_DBG_FUNC, "bcp_exec(%p, %p)\n", dbproc, rows_copied);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(dbproc->hostfileinfo, SYBEBCVH);

	if (rows_copied == NULL) /* NULL means we should ignore it */
		rows_copied = &dummy_copied;

	if (dbproc->bcpinfo->direction == DB_OUT || dbproc->bcpinfo->direction == DB_QUERYOUT) {
		ret = _bcp_exec_out(dbproc, rows_copied);
	} else if (dbproc->bcpinfo->direction == DB_IN) {
		ret = _bcp_exec_in(dbproc, rows_copied);
	}
	_bcp_free_storage(dbproc);
	
	return ret;
}

static void 
bcp_row_free(TDSRESULTINFO* result, unsigned char *row)
{
	if (result->current_row) {
		result->row_size = 0;
		TDS_ZERO_FREE(result->current_row);
	}
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_start_copy_in(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOLUMN *bcpcol;
	int i, firstcol;
	int fixed_col_len_tot     = 0;
	int variable_col_len_tot  = 0;
	int column_bcp_data_size  = 0;
	int bcp_record_size       = 0;
	char *query;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_start_copy_in(%p)\n", dbproc);
	assert(dbproc);

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
			/* FIXME remove memory leak */
			dbproc->bcpinfo->bindinfo->current_row = realloc(dbproc->bcpinfo->bindinfo->current_row, bcp_record_size);
			dbproc->bcpinfo->bindinfo->row_free = bcp_row_free;
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

			/*
			 * dont send the (meta)data for timestamp columns, or
			 * identity columns (unless indentity_insert is enabled
			 */

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
			dbproc->bcpinfo->bindinfo->row_free = bcp_row_free;
			if (dbproc->bcpinfo->bindinfo->current_row == NULL) {
				tdsdump_log(TDS_DBG_FUNC, "could not realloc current_row\n");
				return FAIL;
			}
			dbproc->bcpinfo->bindinfo->row_size = bcp_record_size;
		}
	}

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param tds 
 * \param clause 
 * \param bcpcol 
 * \param first 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_build_bulk_insert_stmt(TDSSOCKET * tds, TDS_PBCB * clause, TDSCOLUMN * bcpcol, int first)
{
	char buffer[32];
	char *column_type = buffer;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_build_bulk_insert_stmt(%p, %p, %p, %d)\n", tds, clause, bcpcol, first);

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
		dbperror( NULL, SYBEBPROBADTYP, 0);
		tdsdump_log(TDS_DBG_FUNC, "error: cannot build bulk insert statement. unrecognized server datatype %d\n",
				bcpcol->on_server.column_type);
		return FAIL;
	}

	if (clause->cb < strlen(clause->pb) 
		       + tds_quote_id(tds, NULL, bcpcol->column_name, bcpcol->column_namelen) 
		       + strlen(column_type) 
		       + ((first) ? 2 : 4)) {
		char *temp = (char *) malloc(2 * clause->cb);

		if (!temp) {
			dbperror(NULL, SYBEMEM, 0);
			return FAIL;
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

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_start_new_batch(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_start_new_batch(%p)\n", dbproc);
	assert(dbproc);

	tds_submit_query(tds, dbproc->bcpinfo->insert_stmt);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	/* TODO problem with thread safety */
	tds->out_flag = TDS_BULK;
	tds_set_state(tds, TDS_QUERYING);

	if (IS_TDS7_PLUS(tds)) {
		_bcp_send_colmetadata(dbproc);
	}
	
	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_send_colmetadata(DBPROCESS * dbproc)
{
	const static unsigned char colmetadata_token = 0x81;
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOLUMN *bcpcol;
	int i, num_cols;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_send_colmetadata(%p)\n", dbproc);
	assert(dbproc);

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

	/*
	 * dont send the (meta)data for timestamp columns, or
	 * identity columns (unless indentity_insert is enabled
	 */

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
			/* FIXME strlen return len in bytes not in characters required here */
			tds_put_smallint(tds, strlen(dbproc->bcpinfo->tablename));
			tds_put_string(tds, dbproc->bcpinfo->tablename, strlen(dbproc->bcpinfo->tablename));
		}
		/* FIXME column_namelen contains len in bytes not in characters required here */
		tds_put_byte(tds, bcpcol->column_namelen);
		tds_put_string(tds, bcpcol->column_name, bcpcol->column_namelen);

	}

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param buffer 
 * \param size 
 * \param f 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static char *
_bcp_fgets(char *buffer, size_t size, FILE *f)
{
	char *p = fgets(buffer, size, f);
	if (p == NULL) 
		return p;

	/* discard newline */
	p = strchr(buffer, 0) - 1;
	if (p >= buffer && *p == '\n')
		*p = 0;
	return buffer;
}

/** 
 * \ingroup dblib_bcp
 * \brief Read a format definition file.
 *
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param filename Name that will be passed to fopen(3).  
 * 
 * \remarks Reads a format file and calls bcp_columns() and bcp_colfmt() as needed. 
 *
 * \return SUCCEED or FAIL.
 * \sa 	bcp_colfmt(), bcp_colfmt_ps(), bcp_columns(), bcp_writefmt()
 */
RETCODE
bcp_readfmt(DBPROCESS * dbproc, char *filename)
{
	BCP_HOSTCOLINFO *hostcol;
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
	struct fflist *topptr = NULL;
	struct fflist *curptr = NULL;

	tdsdump_log(TDS_DBG_FUNC, "bcp_readfmt(%p, %s)\n", dbproc, filename);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(filename, SYBENULP);

	if ((ffile = fopen(filename, "r")) == NULL) {
		dbperror(dbproc, SYBEBUOF, 0);
		return (FAIL);
	}

	if ((_bcp_fgets(buffer, sizeof(buffer), ffile)) != NULL) {
		lf_version = atof(buffer);
	} else if (ferror(ffile)) {
		dbperror(dbproc, SYBEBRFF, errno);
		return FAIL;
	}

	if ((_bcp_fgets(buffer, sizeof(buffer), ffile)) != NULL) {
		li_numcols = atoi(buffer);
	} else if (ferror(ffile)) {
		dbperror(dbproc, SYBEBRFF, errno);
		return FAIL;
	}

	/* FIXME fix memory leak, if this function returns FAIL... */
	while ((_bcp_fgets(buffer, sizeof(buffer), ffile)) != NULL) {

		if (topptr == NULL) {	/* first time */
			if ((curptr = (struct fflist *) malloc(sizeof(struct fflist))) == NULL) {
				dbperror(dbproc, SYBEMEM, 0);
				return (FAIL);
			}
			topptr = curptr;
		} else {
			if ((curptr->nextptr = (struct fflist *) malloc(sizeof(struct fflist))) == NULL) {
				dbperror(dbproc, SYBEMEM, 0);
				return (FAIL);
			}
			curptr = curptr->nextptr;
		}
		curptr->nextptr = NULL;
		if (_bcp_readfmt_colinfo(dbproc, buffer, &(curptr->colinfo)))
			colinfo_count++;
		else
			return (FAIL);

	}
	if (ferror(ffile)) {
		dbperror(dbproc, SYBEBRFF, errno);
		return FAIL;
	}
	
	if (fclose(ffile) != 0) {
		dbperror(dbproc, SYBEBUCF, 0);
		return (FAIL);
	}

	if (colinfo_count != li_numcols)
		return (FAIL);

	if (bcp_columns(dbproc, li_numcols) == FAIL) {
		return (FAIL);
	}

	for (curptr = topptr; curptr->nextptr != NULL; curptr = curptr->nextptr) {
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

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param buf 
 * \param ci 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static int
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

	tdsdump_log(TDS_DBG_FUNC, "_bcp_readfmt_colinfo(%p, %s, %p)\n", dbproc, buf, ci);
	assert(dbproc);
	assert(buf);
	assert(ci);

	tok = strtok(buf, " \t");
	whichcol = HOST_COLUMN;

	/* TODO use a better way to get an int atoi is very error prone */
	while (tok != NULL && whichcol != NO_MORE_COLS) {
		switch (whichcol) {

		case HOST_COLUMN:
			ci->host_column = atoi(tok);

			if (ci->host_column < 1) {
				dbperror(dbproc, SYBEBIHC, 0);
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
				dbperror(dbproc, SYBEBUDF, 0);
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

			for (i = 0; *tok != '\"' && i < sizeof(term); i++) {
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

			ci->terminator = (BYTE *) malloc(i);
			memcpy((char *) ci->terminator, term, i);
			ci->term_len = i;

			whichcol = TAB_COLNUM;
			break;

		case TAB_COLNUM:
			ci->tab_colnum = atoi(tok);
			whichcol = NO_MORE_COLS;
			break;

		}
		tok = strtok(NULL, " \t");
	}
	if (whichcol == NO_MORE_COLS)
		return (TRUE);
	else
		return (FALSE);
}

/** 
 * \ingroup dblib_bcp
 * \brief Write a format definition file. Not Implemented. 
 *
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param filename Name that would be passed to fopen(3).  
 * 
 * \remarks Reads a format file and calls bcp_columns() and bcp_colfmt() as needed. 
 * \a FreeTDS includes freebcp, a utility to copy data to or from a host file. 
 *
 * \todo For completeness, \a freebcp ought to be able to create format files, but that functionality 
 * 	is currently lacking, as is bcp_writefmt().
 * \todo See the vendors' documentation for the format of these files.
 *
 * \return SUCCEED or FAIL.
 * \sa 	bcp_colfmt(), bcp_colfmt_ps(), bcp_columns(), bcp_readfmt()
 */
RETCODE
bcp_writefmt(DBPROCESS * dbproc, char *filename)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED: bcp_writefmt(%p, %s)\n", dbproc, filename);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(filename, SYBENULP);

#if 0
	dbperror(dbproc, SYBEBUFF, errno);	/* bcp: Unable to create format file */
	dbperror(dbproc, SYBEBWFF, errno);	/* I/O error while writing bcp format file */
#endif

	return FAIL;
}

/** 
 * \ingroup dblib_bcp
 * \brief Write some text or image data to the server.  Not implemented, sadly.  
 *
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param size How much to write, in bytes. 
 * \param text Address of the data to be written. 
 * \remarks For a SYBTEXT or SYBIMAGE column, bcp_bind() can be called with 
 *	a NULL varaddr parameter.  If it is, bcp_sendrow() will return control
 *	to the application after the non-text data have been sent.  The application then calls 
 *	bcp_moretext() -- usually in a loop -- to send the text data in manageable chunks.  
 * \todo implement bcp_moretext().
 * \return SUCCEED or FAIL.
 * \sa 	bcp_bind(), bcp_sendrow(), dbmoretext(), dbwritetext()
 */
RETCODE
bcp_moretext(DBPROCESS * dbproc, DBINT size, BYTE * text)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED: bcp_moretext(%p, %d, %p)\n", dbproc, size, text);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);
	CHECK_PARAMETER(text, SYBENULP);
	
#if 0
	dbperror(dbproc, SYBEBCMTXT, 0); 
		/* bcp_moretext may be used only when there is at least one text or image column in the server table */
	dbperror(dbproc, SYBEBTMT, 0); 
		/* Attempt to send too much text data via the bcp_moretext call */
#endif
	return FAIL;
}

/** 
 * \ingroup dblib_bcp
 * \brief Commit a set of rows to the table. 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \remarks If not called, bcp_done() will cause the rows to be saved.  
 * \return Count of rows saved, or -1 on error.
 * \sa 	bcp_bind(), bcp_done(), bcp_sendrow()
 */
DBINT
bcp_batch(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;
	int rows_copied = 0;

	tdsdump_log(TDS_DBG_FUNC, "bcp_batch(%p)\n", dbproc);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);

	tds = dbproc->tds_socket;
	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;

	rows_copied = tds->rows_affected;

	_bcp_start_new_batch(dbproc);

	return (rows_copied);
}

/** 
 * \ingroup dblib_bcp
 * \brief Conclude the transfer of data from program variables.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \remarks Do not overlook this function.  According to Sybase, failure to call bcp_done() 
 * "will result in unpredictable errors".
 * \return As with bcp_batch(), the count of rows saved, or -1 on error.
 * \sa 	bcp_batch(), bcp_bind(), bcp_moretext(), bcp_sendrow()
 */
DBINT
bcp_done(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;
	int rows_copied = -1;

	tdsdump_log(TDS_DBG_FUNC, "bcp_done(%p)\n", dbproc);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);

	/* TODO check proper tds state */

	tds = dbproc->tds_socket;
	tds_flush_packet(tds);

	tds_set_state(tds, TDS_PENDING);

	if (tds_process_simple_query(tds) != TDS_SUCCEED)
		return FAIL;
		
	rows_copied = tds->rows_affected;

	_bcp_free_storage(dbproc);

	return (rows_copied);
}

/** 
 * \ingroup dblib_bcp
 * \brief Bind a program host variable to a database column
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param varaddr address of host variable
 * \param prefixlen length of any prefix found at the beginning of \a varaddr, in bytes.  
 	Use zero for fixed-length datatypes. 
 * \param varlen bytes of data in \a varaddr.  Zero for NULL, -1 for fixed-length datatypes. 
 * \param terminator byte sequence that marks the end of the data in \a varaddr
 * \param termlen length of \a terminator
 * \param vartype datatype of the host variable
 * \param table_column Nth column, starting at 1, in the table.
 * 
 * \remarks The order of operation is:
 *	- bcp_init() with \a hfile == NULL and \a direction == DB_IN.
 * 	- bcp_bind(), once per column you want to write to
 *	- bcp_batch(), optionally, to commit a set of rows
 *	- bcp_done() 
 * \return SUCCEED or FAIL.
 * \sa 	bcp_batch(), bcp_colfmt(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), 
 * 	bcp_done(), bcp_exec(), bcp_moretext(), bcp_sendrow() 
 */
RETCODE
bcp_bind(DBPROCESS * dbproc, BYTE * varaddr, int prefixlen, DBINT varlen,
	 BYTE * terminator, int termlen, int vartype, int table_column)
{
	TDSCOLUMN *colinfo;

	tdsdump_log(TDS_DBG_FUNC, "bcp_bind(%p, %p, %d, %d)\n", dbproc, varaddr, prefixlen, varlen);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(dbproc->bcpinfo, SYBEBCPI);

	if (dbproc->hostfileinfo != NULL) {
		dbperror(dbproc, SYBEBCPB, 0);
		return FAIL;
	}

	if (dbproc->bcpinfo->direction != DB_IN) {
		dbperror(dbproc, SYBEBCPN, 0);
		return FAIL;
	}

	if (varlen < -1) {
		dbperror(dbproc, SYBEBCVLEN, 0);
		return FAIL;
	}

	if (prefixlen != 0 && prefixlen != 1 && prefixlen != 2 && prefixlen != 4) {
		dbperror(dbproc, SYBEBCBPREF, 0);
		return FAIL;
	}

	if (prefixlen == 0 && varlen == -1 && termlen == -1 && !is_fixed_type(vartype)) {
		tdsdump_log(TDS_DBG_FUNC, "bcp_bind(): non-fixed type %d requires prefix or terminator\n", vartype);
		return FAIL;
	}

	if (is_fixed_type(vartype) && (varlen != -1 && varlen != 0)) {
		dbperror(dbproc, SYBEBCIT, 0);
		return FAIL;
	}

	if (table_column <= 0 ||  table_column > dbproc->bcpinfo->bindinfo->num_cols) {
		dbperror(dbproc, SYBECNOR, 0);
		return FAIL;
	}
	
	if (varaddr == NULL && (prefixlen != 0 || termlen != 0)) {
		dbperror(dbproc, SYBEBCBNPR, 0);
		return FAIL;
	}

	colinfo = dbproc->bcpinfo->bindinfo->columns[table_column - 1];

	/* If varaddr is NULL and varlen greater than 0, the table column type must be SYBTEXT or SYBIMAGE 
		and the program variable type must be SYBTEXT, SYBCHAR, SYBIMAGE or SYBBINARY */
	if (varaddr == NULL && varlen > 0) {
		int fOK = (colinfo->column_type == SYBTEXT || colinfo->column_type == SYBIMAGE) &&
			  (vartype == SYBTEXT || vartype == SYBCHAR || vartype == SYBIMAGE || vartype == SYBBINARY );
		if( !fOK ) {
			dbperror(dbproc, SYBEBCBNTYP, 0);
			tdsdump_log(TDS_DBG_FUNC, "bcp_bind: SYBEBCBNTYP: column=%d and vartype=%d (should fail?)\n", 
							colinfo->column_type, vartype);
			/* return FAIL; */
		}
	}

	colinfo->column_varaddr  = (char *)varaddr;
	colinfo->column_bindtype = vartype;
	colinfo->column_bindlen  = varlen;
	colinfo->bcp_terminator =  malloc(termlen);
	memcpy(colinfo->bcp_terminator, terminator, termlen);
	colinfo->bcp_term_len = termlen;

	return SUCCEED;
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param behaviour 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_send_bcp_record(DBPROCESS * dbproc, int behaviour)
{
	TDSSOCKET  *tds = dbproc->tds_socket;
	TDSCOLUMN  *bindcol;

	static const TDS_TINYINT textptr_size = 16;
	static const unsigned char GEN_NULL = 0x00;

	static const unsigned char CHARBIN_NULL[] =  { 0xff, 0xff };
	static const unsigned char textptr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
						 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	static const unsigned char timestamp[]={ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

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

	tdsdump_log(TDS_DBG_FUNC, "_bcp_send_bcp_record(%p, %d)\n", dbproc, behaviour);
	CHECK_PARAMETER(dbproc, SYBENULL);

	record = dbproc->bcpinfo->bindinfo->current_row;
	old_record_size = dbproc->bcpinfo->bindinfo->row_size;
	new_record_size = 0;

	if (IS_TDS7_PLUS(tds)) {

		for (i = 0; i < dbproc->bcpinfo->bindinfo->num_cols; i++) {
	
			bindcol = dbproc->bcpinfo->bindinfo->columns[i];

			/*
			 * Don't send the (meta)data for timestamp columns or
			 * identity columns unless indentity_insert is enabled.
			 */

			if ((!dbproc->bcpinfo->identity_insert_on && bindcol->column_identity) || 
				bindcol->column_timestamp) {
				continue;
			}

			if (behaviour == BCP_REC_FETCH_DATA) { 
				if ((_bcp_get_col_data(dbproc, bindcol)) != SUCCEED) {
					tdsdump_log(TDS_DBG_INFO1, "_bcp_get_col_data (column %d) failed\n", i + 1);
		 			return FAIL;
				}
			}
			tdsdump_log(TDS_DBG_INFO1, "parsed column %d, length %d (%snull)\n", i + 1, 
				    bindcol->bcp_column_data->datalen, bindcol->bcp_column_data->null_column? "":"not ");
	
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
					dbperror(dbproc, SYBEBCNN, 0);
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
						tdsdump_log(TDS_DBG_INFO1, "numeric type prec = %d varint_1 = %d\n",
										 bindcol->column_prec, varint_1);
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
#endif
				if (is_numeric_type(bindcol->column_type)) {
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
		tds_put_n(tds, dbproc->bcpinfo->bindinfo->current_row, new_record_size);
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

	return SUCCEED;
}

/* 
 * For a bcp in from program variables, get the data from 
 * the host variable
 */
/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param bindcol 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
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

	tdsdump_log(TDS_DBG_FUNC, "_bcp_get_col_data(%p, %p)\n", dbproc, bindcol);
	CHECK_PARAMETER(dbproc, SYBENULL);
	CHECK_PARAMETER(bindcol, SYBENULP);

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
/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param pdata 
 * \param term 
 * \param term_len 
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
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

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param istr 
 * \param ilen 
 * 
 * \return modified length
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static int
rtrim(char *istr, int ilen)
{
	char *t;

	for (t = istr + ilen; --t > istr && *t == ' '; )
		*t = '\0';
	return t - istr + 1;
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static void
_bcp_free_columns(DBPROCESS * dbproc)
{
	int i;

	tdsdump_log(TDS_DBG_FUNC, "_bcp_free_columns(%p)\n", dbproc);
	assert(dbproc && dbproc->hostfileinfo);

	if (dbproc->hostfileinfo->host_columns) {
		for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
			if (dbproc->hostfileinfo->host_columns[i]->terminator)
				TDS_ZERO_FREE(dbproc->hostfileinfo->host_columns[i]->terminator);
			tds_free_bcp_column_data(dbproc->hostfileinfo->host_columns[i]->bcp_column_data);
			TDS_ZERO_FREE(dbproc->hostfileinfo->host_columns[i]);
		}
		TDS_ZERO_FREE(dbproc->hostfileinfo->host_columns);
		dbproc->hostfileinfo->host_colcount = 0;
	}
}

/** 
 * \ingroup dblib_bcp_internal
 * \brief 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * 
 * \return SUCCEED or FAIL.
 * \sa 	BCP_SETL(), bcp_batch(), bcp_bind(), bcp_colfmt(), bcp_colfmt_ps(), bcp_collen(), bcp_colptr(), bcp_columns(), bcp_control(), bcp_done(), bcp_exec(), bcp_getl(), bcp_init(), bcp_moretext(), bcp_options(), bcp_readfmt(), bcp_sendrow()
 */
static RETCODE
_bcp_free_storage(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "_bcp_free_storage(%p)\n", dbproc);
	assert(dbproc);

	if (dbproc->hostfileinfo) {
		if (dbproc->hostfileinfo->hostfile)
			TDS_ZERO_FREE(dbproc->hostfileinfo->hostfile);
	
		if (dbproc->hostfileinfo->errorfile)
			TDS_ZERO_FREE(dbproc->hostfileinfo->errorfile);

		/* free up storage that holds details of hostfile columns */

		_bcp_free_columns(dbproc);
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

