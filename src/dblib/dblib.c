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
#endif /* HAVE_CONFIG_H */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <stdarg.h>
#include <assert.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#define SYBDBLIB 1
#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "syberror.h"
#include "dblib.h"
#include "tdsconvert.h"
#include "replacements.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: dblib.c,v 1.213 2005-04-14 13:28:38 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int _db_get_server_type(int bindtype);
static int _get_printable_size(TDSCOLUMN * colinfo);
static char *_dbprdate(char *timestr);
static char *tds_prdatatype(TDS_SERVER_TYPE datatype_token);

static void _set_null_value(BYTE * varaddr, int datatype, int maxlen);
static void copy_data_to_host_var(DBPROCESS *, int, const BYTE *, DBINT, int, BYTE *, DBINT, int, DBSMALLINT *);
static void buffer_struct_print(DBPROC_ROWBUF *buf);

/**
 * @file dblib.c
 * Main implementation file for \c db-lib.
 */
/**
 * @file bcp.c
 * Implementation of \c db-lib bulk copy functions.
 */
/**
 * \defgroup dblib_api db-lib API
 * Functions callable by \c db-lib client programs
 *
 * The \c db_lib interface is implemented by both Sybase and Microsoft.  FreeTDS seeks to implement 
 * first the intersection of the functions defined by the vendors.  
 */
/**
 * \ingroup dblib_api
 * \defgroup dblib_internal db-lib internals
 * Functions called within \c db-lib for self-help.  
 */
/**
 * \ingroup dblib_api
 * \defgroup dblib_bcp db-lib bulk copy functions.  
 * Functions to bulk-copy (a/k/a \em bcp) data to/from the database.  
*/

/* info/err message handler functions (or rather pointers to them) */
MHANDLEFUNC _dblib_msg_handler = NULL;
EHANDLEFUNC _dblib_err_handler = NULL;

typedef struct dblib_context
{
	TDSCONTEXT *tds_ctx;
	TDSSOCKET **connection_list;
	int connection_list_size;
	int connection_list_size_represented;
	char *recftos_filename;
	int recftos_filenum;
}
DBLIBCONTEXT;

static DBLIBCONTEXT g_dblib_ctx;

static int g_dblib_version =
#ifdef TDS42
	DBVERSION_42;
#endif
#ifdef TDS50
DBVERSION_100;
#endif
#ifdef TDS46
DBVERSION_46;
#endif
#ifdef TDS70
DBVERSION_70;
#endif
#ifdef TDS80
DBVERSION_80;
#endif

static int g_dblib_login_timeout = -1;	/* not used unless positive */
static int g_dblib_query_timeout = -1;	/* not used unless positive */

static int
dblib_add_connection(DBLIBCONTEXT * ctx, TDSSOCKET * tds)
{
	int i = 0;
	const int list_size = ctx->connection_list_size_represented;

	while (i < list_size && ctx->connection_list[i])
		i++;
	if (i == list_size) {
		fprintf(stderr, "Max connections reached, increase value of TDS_MAX_CONN\n");
		return 1;
	} else {
		ctx->connection_list[i] = tds;
		return 0;
	}
}

static void
dblib_del_connection(DBLIBCONTEXT * ctx, TDSSOCKET * tds)
{
	int i = 0;
	const int list_size = ctx->connection_list_size;

	while (i < list_size && ctx->connection_list[i] != tds)
		i++;
	if (i == list_size) {
		/* connection wasn't on the free list...now what */
	} else {
		/* remove it */
		ctx->connection_list[i] = NULL;
	}
}

static void
buffer_init(DBPROC_ROWBUF * buf)
{
	memset(buf, 0xad, sizeof(*buf));

	buf->buffering_on = 0;
	buf->first_in_buf = 0;
	buf->newest = -1;
	buf->oldest = 0;
	buf->elcount = 0;
	buf->element_size = 0;
	buf->rows_in_buf = 0;
	buf->rows = NULL;
	buf->next_row = 1;
}

static void
buffer_clear(DBPROC_ROWBUF * buf)
{
	buf->next_row = 1;
	buf->first_in_buf = 0;
	buf->newest = -1;
	buf->oldest = 0;
	buf->rows_in_buf = 0;
	if (buf->rows)
		TDS_ZERO_FREE(buf->rows);
}


static void
buffer_free(DBPROC_ROWBUF * buf)
{
	if (buf->rows != NULL)
		TDS_ZERO_FREE(buf->rows);
}

static void
buffer_delete_rows(DBPROC_ROWBUF * buf,	int count)
{
	assert(count <= buf->elcount);	/* possibly a little to pedantic */

	if (count < 0 || count > buf->rows_in_buf) {
		count = buf->rows_in_buf;
	}


	buf->oldest = (buf->oldest + count) % buf->elcount;
	buf->rows_in_buf -= count;
	buf->first_in_buf = count == buf->rows_in_buf ? buf->next_row - 1 : buf->first_in_buf + count;

	if(buf->first_in_buf < 0) {
		printf("count (to delete) = %d\n", count);
		buffer_struct_print(buf);
		assert(buf->first_in_buf >= 0);
	}
}

static void 
buffer_struct_print(DBPROC_ROWBUF *buf)
{
	assert(buf);

	printf("buffering_on = %d\n", 	buf->buffering_on);
	printf("first_in_buf = %d\n", 	buf->first_in_buf);
	printf("next_row = %d\n",	buf->next_row);
	printf("newest = %d\n", 	buf->newest);
	printf("oldest = %d\n", 	buf->oldest);
	printf("elcount = %d\n", 	buf->elcount);
	printf("element_size = %d\n", 	buf->element_size);
	printf("rows_in_buf = %d\n", 	buf->rows_in_buf);
	printf("rows = %p\n", 		buf->rows);
}




static int
buffer_start_resultset(DBPROC_ROWBUF * buf,	/* (U) buffer to clear */
		       int element_size)
{				/*                     */
	int space_needed = -1;

	assert(element_size > 0);

	if (buf->rows != NULL) {
		/* TODO why ?? memory bug chech ?? security ?? */
		memset(buf->rows, 0xad, buf->element_size * buf->rows_in_buf);
		free(buf->rows);
	}

	buf->first_in_buf = 0;
	buf->next_row = 1;
	buf->newest = -1;
	buf->oldest = 0;
	buf->elcount = buf->buffering_on ? buf->elcount : 1;
	buf->element_size = element_size;
	buf->rows_in_buf = 0;
	space_needed = element_size * buf->elcount;
	buf->rows = malloc(space_needed);

	return buf->rows == NULL ? FAIL : SUCCEED;
}

static int
buffer_index_of_resultset_row(DBPROC_ROWBUF * buf,	/* (U) buffer to clear                 */
			      int row_number)
{				/* (I)                                 */
	int result = -1;

	if (row_number < buf->first_in_buf) {
		result = -1;
	} else if (row_number > ((buf->rows_in_buf + buf->first_in_buf) - 1)) {
		result = -1;
	} else {
		result = ((row_number - buf->first_in_buf)
			  + buf->oldest) % buf->elcount;
	}
	return result;
}


static void *
buffer_row_address(DBPROC_ROWBUF * buf,	/* (U) buffer to clear                 */
		   int idx)
{				/* (I) raw index of row to return      */
	void *result = NULL;

	assert(idx >= 0);
	assert(idx < buf->elcount);

	if (idx >= buf->elcount || idx < 0) {
		result = NULL;
	} else {
		int offset = buf->element_size * (idx % buf->elcount);

		result = (char *) buf->rows + offset;
	}
	return result;
}


static void
buffer_add_row(DBPROC_ROWBUF * buf,	/* (U) buffer to add row into  */
	       void *row,	/* (I) pointer to the row data */
	       int row_size)
{
	void *dest = NULL;

	assert(row_size > 0);
	assert(row_size <= buf->element_size);

	assert(buf->elcount >= 1);

	buf->newest = (buf->newest + 1) % buf->elcount;
	if (buf->rows_in_buf == 0 && buf->first_in_buf == 0) {
		buf->first_in_buf = 1;
	}
	buf->rows_in_buf++;

	/* 
	 * if we have wrapped around we need to adjust oldest
	 * and rows_in_buf
	 */
	if (buf->rows_in_buf > buf->elcount) {
		buf->oldest = (buf->oldest + 1) % buf->elcount;
		buf->first_in_buf++;
		buf->rows_in_buf--;
	}

	assert(buf->elcount >= buf->rows_in_buf);
	assert(buf->rows_in_buf == 0 || (((buf->oldest + buf->rows_in_buf) - 1) % buf->elcount) == buf->newest);
	assert(buf->rows_in_buf > 0 || (buf->first_in_buf == buf->next_row - 1));
	assert(buf->rows_in_buf == 0 || (buf->first_in_buf <= buf->next_row));
	assert(buf->next_row - 1 <= (buf->first_in_buf + buf->rows_in_buf));

	dest = buffer_row_address(buf, buf->newest);
	memcpy(dest, row, row_size);
}


static int
buffer_is_full(DBPROC_ROWBUF * buf)
{
	return buf->rows_in_buf == buf->elcount;
}

static void
buffer_set_buffering(DBPROC_ROWBUF * buf, int nrows)
{
	/* 
	 * XXX If the user calls this routine in the middle of 
	 * a result set and changes the size of the buffering
	 * they are pretty much toast.
	 *
	 * We need to figure out what to do if the user shrinks the 
	 * size of the row buffer.  What rows should be thrown out, 
	 * what should happen to the current row, etc?
	 */

	assert(nrows >= 0);

	buf->buffering_on = nrows > 0;
	
	if (!buf->buffering_on) {	/* turn off buffering */
		buffer_delete_rows(buf, buf->rows_in_buf);
		buf->elcount = 1;
		return;
	}

	buffer_clear(buf);
	buffer_free(buf);

	buf->elcount = nrows;	/* set the buffer size (number of rows to buffer) */

	if (buf->element_size > 0) {
		buf->rows = malloc(buf->element_size * buf->elcount);
	} else {
		buf->rows = NULL;
	}
}

static void
buffer_transfer_bound_data(TDS_INT res_type, TDS_INT compute_id, DBPROC_ROWBUF * buf,	/* (U)                                         */
			   DBPROCESS * dbproc,	/* (I)                                         */
			   int row_num)
{				/* (I) resultset row number                    */

	int i;
	TDSCOLUMN *curcol;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int srctype;
	BYTE *src;
	int desttype;

	tds = dbproc->tds_socket;
	if (res_type == TDS_ROW_RESULT) {
		resinfo = tds->res_info;
	} else {		/* TDS_COMPUTE_RESULT */
		for (i = 0;; ++i) {
			if (i >= tds->num_comp_info)
				return;
			resinfo = (TDSRESULTINFO *) tds->comp_info[i];
			if (resinfo->computeid == compute_id)
				break;
		}
	}

	for (i = 0; i < resinfo->num_cols; i++) {
		curcol = resinfo->columns[i];
		if (curcol->column_nullbind) {
			if (curcol->column_cur_size < 0) {
				*(DBINT *)(curcol->column_nullbind) = -1;
			} else {
				*(DBINT *)(curcol->column_nullbind) = 0;
			}
		}
		if (curcol->column_varaddr) {
			DBINT srclen;
			int idx = buffer_index_of_resultset_row(buf, row_num);

			assert(idx >= 0);
			/* XXX now what? */

			src = ((BYTE *) buffer_row_address(buf, idx)) + curcol->column_offset;
			srclen = curcol->column_cur_size;
			if (is_blob_type(curcol->column_type)) {
				src = (BYTE *) ((TDSBLOB *) src)->textvalue;
			}
			desttype = _db_get_server_type(curcol->column_bindtype);
			srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);

			if (srclen < 0) {
				_set_null_value((BYTE *) curcol->column_varaddr, desttype, curcol->column_bindlen);
			} else {
				copy_data_to_host_var(dbproc, 
								srctype, src, srclen, 
								desttype, (BYTE *) curcol->column_varaddr, curcol->column_bindlen,
								curcol->column_bindtype, curcol->column_nullbind);

			}
		}
	}
}	/* end buffer_transfer_bound_data()  */

static void
db_env_chg(TDSSOCKET * tds, int type, char *oldval, char *newval)
{
	DBPROCESS *dbproc;

	if (tds == NULL) {
		return;
	}
	dbproc = (DBPROCESS *) tds->parent;
	if (dbproc == NULL) {
		return;
	}
	dbproc->envchange_rcv |= (1 << (type - 1));
	switch (type) {
	case TDS_ENV_DATABASE:
		strncpy(dbproc->dbcurdb, newval, DBMAXNAME);
		dbproc->dbcurdb[DBMAXNAME] = '\0';
		break;
	case TDS_ENV_CHARSET:
		strncpy(dbproc->servcharset, newval, DBMAXNAME);
		dbproc->servcharset[DBMAXNAME] = '\0';
		break;
	default:
		break;
	}
	return;
}

static int
dblib_query_timeout(void *param)
{
	TDSSOCKET * tds = (TDSSOCKET *) param;
	DBPROCESS *dbproc;

	if (!tds || !tds->parent)
		return TDS_INT_CONTINUE;

	dbproc = (DBPROCESS *) tds->parent;
	if (dbproc->dbchkintr == NULL || !((*dbproc->dbchkintr) (dbproc)) )
		return TDS_INT_CONTINUE;

	if (dbproc->dbhndlintr == NULL)
		return TDS_INT_CONTINUE;

	return ((*dbproc->dbhndlintr) (dbproc));
}

/**
 * \ingroup dblib_api
 * \brief Initialize db-lib.  
 * Call this function before trying to use db-lib in any way.  
 * Allocates various internal structures and reads \c locales.conf (if any) to determine the default
 * date format.  
 * \retval SUCCEED normal.  
 * \retval FAIL cannot allocate an array of \c TDS_MAX_CONN \c TDSSOCKET pointers.  
 */
RETCODE
dbinit(void)
{
	/* 
	 * DBLIBCONTEXT stores a list of current connections so they may be closed
	 * with dbexit() 
	 */
	memset(&g_dblib_ctx, '\0', sizeof(DBLIBCONTEXT));

	g_dblib_ctx.connection_list = (TDSSOCKET **) calloc(TDS_MAX_CONN, sizeof(TDSSOCKET *));
	if (g_dblib_ctx.connection_list == NULL) {
		tdsdump_log(TDS_DBG_FUNC, "dbinit: out of memory\n");
		return FAIL;
	}
	g_dblib_ctx.connection_list_size = TDS_MAX_CONN;
	g_dblib_ctx.connection_list_size_represented = TDS_MAX_CONN;


	g_dblib_ctx.tds_ctx = tds_alloc_context();
	tds_ctx_set_parent(g_dblib_ctx.tds_ctx, &g_dblib_ctx);

	/* 
	 * Set the functions in the TDS layer to point to the correct info/err
	 * message handler functions 
	 */
	g_dblib_ctx.tds_ctx->msg_handler = _dblib_handle_info_message;
	g_dblib_ctx.tds_ctx->err_handler = _dblib_handle_err_message;


	if (g_dblib_ctx.tds_ctx->locale && !g_dblib_ctx.tds_ctx->locale->date_fmt) {
		/* set default in case there's no locale file */
		g_dblib_ctx.tds_ctx->locale->date_fmt = strdup("%b %e %Y %I:%M:%S:%z%p");
	}

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Allocate a \c LOGINREC structure.  
 * A \c LOGINREC structure is passed to \c dbopen() to create a connection to the database. 
 * Does not communicate to the server; interacts strictly with library.  
 * \retval NULL the \c LOGINREC cannot be allocated.
 * \retval LOGINREC* to valid memory, otherwise.  
 */
LOGINREC *
dblogin(void)
{
	LOGINREC *loginrec;

	if ((loginrec = (LOGINREC *) malloc(sizeof(LOGINREC))) == NULL) {
		return NULL;
	}
	if ((loginrec->tds_login = tds_alloc_login()) == NULL) {
		free(loginrec);
		return NULL;
	}

	/* set default values for loginrec */
	tds_set_library(loginrec->tds_login, "DB-Library");
	/* tds_set_client_charset(loginrec->tds_login, "iso_1"); */
	/* tds_set_packet(loginrec->tds_login, TDS_DEF_BLKSZ); */

	return loginrec;
}

/**
 * \ingroup dblib_api
 * \brief free the \c LOGINREC
 */
void
dbloginfree(LOGINREC * login)
{
	if (login) {
		tds_free_login(login->tds_login);
		free(login);
	}
}

/** \internal
 * \ingroup dblib_internal 
 * \brief Set the value of a string in a \c LOGINREC structure.  
 * Called by various macros to populate \a login.  
 * \param login the \c LOGINREC* to modify.
 * \param value the value to set it to.  
 * \param which the field to set.  
 * \retval SUCCEED the value was set.
 * \retval FAIL \c DBSETHID or other invalid \a which was tried.  
 */
RETCODE
dbsetlname(LOGINREC * login, const char *value, int which)
{
	switch (which) {
	case DBSETHOST:
		tds_set_host(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETUSER:
		tds_set_user(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETPWD:
		tds_set_passwd(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETAPP:
		tds_set_app(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETCHARSET:
		tds_set_client_charset(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETNATLANG:
		tds_set_language(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETHID:
	default:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetlname() which = %d\n", which);
		return FAIL;
		break;
	}
}

/** \internal
 * \ingroup dblib_internal
 * \brief Set an integer value in a \c LOGINREC structure.  
 * Called by various macros to populate \a login.  
 * \param login the \c LOGINREC* to modify.
 * \param value the value to set it to.  
 * \param which the field to set.  
 * \retval SUCCEED the value was set.
 * \retval FAIL anything other than \c DBSETPACKET was passed for \a which.  
 */
RETCODE
dbsetllong(LOGINREC * login, long value, int which)
{
	switch (which) {
	case DBSETPACKET:
		tds_set_packet(login->tds_login, value);
		return SUCCEED;
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetllong() which = %d\n", which);
		return FAIL;
		break;
	}
}

/** \internal
 * \ingroup dblib_internal
 * \brief Set an integer value in a \c LOGINREC structure.  
 * Called by various macros to populate \a login.  
 * \param login the \c LOGINREC* to modify.
 * \param value the value to set it to.  
 * \param which the field to set.  
 * \retval SUCCEED the value was set.
 * \retval FAIL anything other than \c DBSETHIER was passed for \a which.  
 */
RETCODE
dbsetlshort(LOGINREC * login, int value, int which)
{
	switch (which) {
	case DBSETHIER:
	default:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetlshort() which = %d\n", which);
		return FAIL;
		break;
	}
}

/** \internal
 * \ingroup dblib_internal
 * \brief Set a boolean value in a \c LOGINREC structure.  
 * Called by various macros to populate \a login.  
 * \param login the \c LOGINREC* to modify.
 * \param value the value to set it to.  
 * \param which the field to set.  
 * \retval SUCCEED the value was set.
 * \retval FAIL invalid value passed for \a which.  
 */
RETCODE
dbsetlbool(LOGINREC * login, int value, int which)
{
	switch (which) {
	case DBSETBCP:
		tds_set_bulk(login->tds_login, (TDS_TINYINT) value);
		return SUCCEED;
		break;
	case DBSETNOSHORT:
	case DBSETENCRYPT:
	case DBSETLABELED:
	default:
		tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetlbool() which = %d\n", which);
		return FAIL;
		break;

	}
}

/**
 * \ingroup dblib_api
 * \brief Set TDS version for future connections
 */
RETCODE 
dbsetlversion (LOGINREC * login, BYTE version)
{
	if (login == NULL || login->tds_login == NULL)
		return FAIL;
		
	switch (version) {
	case DBVER42:
		login->tds_login->major_version = 4;
		login->tds_login->minor_version = 2;
		return SUCCEED;
	case DBVER60:
		login->tds_login->major_version = 6;
		login->tds_login->minor_version = 0;
		return SUCCEED;
	}
	
	return FAIL;
}

static void
dbstring_free(DBSTRING ** dbstrp)
{
	DBSTRING *curr, *next;
	if (!dbstrp)
		return;

	curr = *dbstrp;
	*dbstrp = NULL;
	for (; curr; ) {
		next = curr->strnext;
		if (curr->strtext)
			free(curr->strtext);
		free(curr);
		curr = next;
	}
}

static RETCODE
dbstring_concat(DBSTRING ** dbstrp, const char *p)
{
	DBSTRING **strp = dbstrp;

	while (*strp != NULL) {
		strp = &((*strp)->strnext);
	}
	if ((*strp = (DBSTRING *) malloc(sizeof(DBSTRING))) == NULL) {
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return FAIL;
	}
	(*strp)->strtotlen = strlen(p);
	if (((*strp)->strtext = (BYTE *) malloc((*strp)->strtotlen)) == NULL) {
		TDS_ZERO_FREE(*strp);
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return FAIL;
	}
	memcpy((*strp)->strtext, p, (*strp)->strtotlen);
	(*strp)->strnext = NULL;
	return SUCCEED;
}

static RETCODE
dbstring_assign(DBSTRING ** dbstrp, const char *p)
{
	dbstring_free(dbstrp);
	return dbstring_concat(dbstrp, p);
}

static DBINT
dbstring_length(DBSTRING * dbstr)
{
	DBINT len = 0;
	DBSTRING *next;

	for (next = dbstr; next != NULL; next = next->strnext) {
		len += next->strtotlen;
	}
	return len;
}

static int
dbstring_getchar(DBSTRING * dbstr, int i)
{

	if (dbstr == NULL) {
		return -1;
	}
	if (i < 0) {
		return -1;
	}
	if (i < dbstr->strtotlen) {
		return dbstr->strtext[i];
	}
	return dbstring_getchar(dbstr->strnext, i - dbstr->strtotlen);
}

static char *
dbstring_get(DBSTRING * dbstr)
{
	DBSTRING *next;
	int len;
	char *ret;
	char *cp;

	if (dbstr == NULL) {
		return NULL;
	}
	len = dbstring_length(dbstr);
	if ((ret = (char *) malloc(len + 1)) == NULL) {
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return NULL;
	}
	cp = ret;
	for (next = dbstr; next != NULL; next = next->strnext) {
		memcpy(cp, next->strtext, next->strtotlen);
		cp += next->strtotlen;
	}
	*cp = '\0';
	return ret;
}

static const char *const opttext[DBNUMOPTIONS] = {
	"parseonly",
	"estimate",
	"showplan",
	"noexec",
	"arithignore",
	"nocount",
	"arithabort",
	"textlimit",
	"browse",
	"offsets",
	"statistics",
	"errlvl",
	"confirm",
	"spid",
	"buffer",
	"noautofree",
	"rowcount",
	"textsize",
	"language",
	"dateformat",
	"prpad",
	"prcolsep",
	"prlinelen",
	"prlinesep",
	"lfconvert",
	"datefirst",
	"chained",
	"fipsflagger",
	"transaction isolation level",
	"auth",
	"identity_insert",
	"no_identity_column",
	"cnv_date2char_short",
	"client cursors",
	"set time",
	"quoted_identifier"
};

static DBOPTION *
init_dboptions(void)
{
	DBOPTION *dbopts;
	int i;

	dbopts = (DBOPTION *) malloc(sizeof(DBOPTION) * DBNUMOPTIONS);
	if (dbopts == NULL) {
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return NULL;
	}
	for (i = 0; i < DBNUMOPTIONS; i++) {
		strncpy(dbopts[i].opttext, opttext[i], MAXOPTTEXT);
		dbopts[i].opttext[MAXOPTTEXT - 1] = '\0';
		dbopts[i].optparam = NULL;
		dbopts[i].optstatus = 0;	/* XXX */
		dbopts[i].optactive = FALSE;
		dbopts[i].optnext = NULL;
	}
	dbstring_assign(&(dbopts[DBPRPAD].optparam), " ");
	dbstring_assign(&(dbopts[DBPRCOLSEP].optparam), " ");
	dbstring_assign(&(dbopts[DBPRLINELEN].optparam), "80");
	dbstring_assign(&(dbopts[DBPRLINESEP].optparam), "\n");
	dbstring_assign(&(dbopts[DBCLIENTCURSORS].optparam), " ");
	dbstring_assign(&(dbopts[DBSETTIME].optparam), " ");
	return dbopts;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Form a connection with the server.
 *   
 * Called by the \c dbopen() macro, normally.  If FreeTDS was configured with \c --enable-msdblib, this
 * function is called by (exported) \c dbopen() function.  \c tdsdbopen is so-named to avoid
 * namespace conflicts with other database libraries that use the same function name.  
 * \param login \c LOGINREC* carrying the account information.
 * \param server name of the dataserver to connect to.  
 * \return valid pointer on successful login.  
 * \retval NULL insufficient memory, unable to connect for any reason.
 * \todo use \c asprintf() to avoid buffer overflow.
 * \todo separate error messages for \em no-such-server and \em no-such-user. 
 */
DBPROCESS *
tdsdbopen(LOGINREC * login, char *server, int msdblib)
{
	DBPROCESS *dbproc;
	TDSCONNECTION *connection;

	dbproc = (DBPROCESS *) malloc(sizeof(DBPROCESS));
	if (dbproc == NULL) {
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return NULL;
	}
	memset(dbproc, '\0', sizeof(DBPROCESS));
	dbproc->msdblib = msdblib;

	dbproc->dbopts = init_dboptions();
	if (dbproc->dbopts == NULL) {
		free(dbproc);
		return NULL;
	}
	dbproc->dboptcmd = NULL;

	dbproc->avail_flag = TRUE;

	dbproc->command_state = DBCMDNONE;

	tds_set_server(login->tds_login, server);

	dbproc->tds_socket = tds_alloc_socket(g_dblib_ctx.tds_ctx, 512);
	tds_set_parent(dbproc->tds_socket, (void *) dbproc);
	dbproc->tds_socket->option_flag2 &= ~0x02;	/* we're not an ODBC driver */
	dbproc->tds_socket->env_chg_func = db_env_chg;
	dbproc->envchange_rcv = 0;
	dbproc->dbcurdb[0] = '\0';
	dbproc->servcharset[0] = '\0';

	connection = tds_read_config_info(NULL, login->tds_login, g_dblib_ctx.tds_ctx->locale);
	if (!connection)
		return NULL;

	/* override connection timeout if dbsetlogintime() was called */
	if (g_dblib_login_timeout >= 0) {
		connection->connect_timeout = g_dblib_login_timeout;
	}

	/* override query timeout if dbsettime() was called */
	if (g_dblib_query_timeout >= 0) {
		connection->timeout = g_dblib_query_timeout;
	}

	dbproc->dbchkintr = NULL;
	dbproc->dbhndlintr = NULL;
	dbproc->tds_socket->query_timeout_param = dbproc->tds_socket;
	dbproc->tds_socket->query_timeout_func = dblib_query_timeout;

	if (tds_connect(dbproc->tds_socket, connection) == TDS_FAIL) {
		tds_free_socket(dbproc->tds_socket);
		dbproc->tds_socket = NULL;
		tds_free_connection(connection);
		return NULL;
	}
	tds_free_connection(connection);
	dbproc->dbbuf = NULL;
	dbproc->dbbufsz = 0;

	if (dbproc->tds_socket) {
		/* tds_set_parent( dbproc->tds_socket, dbproc); */
		dblib_add_connection(&g_dblib_ctx, dbproc->tds_socket);
	} else {
		fprintf(stderr, "DB-Library: Login incorrect.\n");
		free(dbproc);	/* memory leak fix (mlilback, 11/17/01) */
		return NULL;
	}

	buffer_init(&(dbproc->row_buf));

	if (g_dblib_ctx.recftos_filename != NULL) {
		char *temp_filename = NULL;
		const int len = asprintf(&temp_filename, "%s.%d", 
					 g_dblib_ctx.recftos_filename, g_dblib_ctx.recftos_filenum);
		if (len >= 0) {
			dbproc->ftos = fopen(temp_filename, "w");
			if (dbproc->ftos != NULL) {
				fprintf(dbproc->ftos, "/* dbopen() at %s */\n", _dbprdate(temp_filename));
				fflush(dbproc->ftos);
				g_dblib_ctx.recftos_filenum++;
			}
			free(temp_filename);
		}
	}
	return dbproc;
}

/**
 * \ingroup dblib_api
 * \brief \c printf-like way to form SQL to send to the server.  
 *
 * Forms a command string and writes to the command buffer with dbcmd().  
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param fmt <tt> man vasprintf</tt> for details.  
 * \retval SUCCEED success.
 * \retval FAIL insufficient memory, or dbcmd() failed.
 * \sa dbcmd(), dbfreebuf(), dbgetchar(), dbopen(), dbstrcpy(), dbstrlen().
 */
RETCODE
dbfcmd(DBPROCESS * dbproc, const char *fmt, ...)
{
	va_list ap;
	char *s;
	int len;
	RETCODE ret;

	va_start(ap, fmt);
	len = vasprintf(&s, fmt, ap);
	va_end(ap);

	if (len < 0)
		return FAIL;

	ret = dbcmd(dbproc, s);
	free(s);

	return ret;
}

/**
 * \ingroup dblib_api
 * \brief \c Append SQL to the command buffer.  
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param cmdstring SQL to copy.  
 * \retval SUCCEED success.
 * \retval FAIL insufficient memory.  
 * \remarks set command state to \c  DBCMDPEND unless the command state is DBCMDSENT, in which case 
 * it frees the command buffer.  This latter may or may not be the Right Thing to do.  
 * \sa dbfcmd(), dbfreebuf(), dbgetchar(), dbopen(), dbstrcpy(), dbstrlen().
 */
RETCODE
dbcmd(DBPROCESS * dbproc, const char *cmdstring)
{
	int newsz;
	void *p;

	if (dbproc == NULL) {
		return FAIL;
	}

	dbproc->avail_flag = FALSE;

	tdsdump_log(TDS_DBG_FUNC, "dbcmd() bufsz = %d\n", dbproc->dbbufsz);
	if (dbproc->command_state == DBCMDSENT) {
		if (!dbproc->noautofree) {
			dbfreebuf(dbproc);
		}
	}

	if (dbproc->dbbufsz == 0) {
		dbproc->dbbuf = (unsigned char *) malloc(strlen(cmdstring) + 1);
		if (dbproc->dbbuf == NULL) {
			return FAIL;
		}
		strcpy((char *) dbproc->dbbuf, cmdstring);
		dbproc->dbbufsz = strlen(cmdstring) + 1;
	} else {
		newsz = strlen(cmdstring) + dbproc->dbbufsz;
		if ((p = realloc(dbproc->dbbuf, newsz)) == NULL) {
			return FAIL;
		}
		dbproc->dbbuf = (unsigned char *) p;
		strcat((char *) dbproc->dbbuf, cmdstring);
		dbproc->dbbufsz = newsz;
	}

	dbproc->command_state = DBCMDPEND;

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief send the SQL command to the server and wait for an answer.  
 * 
 * Please be patient.  This function waits for the server to respond.   \c dbsqlexec is equivalent
 * to dbsqlsend() followed by dbsqlok(). 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval SUCCEED query was processed without errors.
 * \retval FAIL was returned by dbsqlsend() or dbsqlok().
 * \sa dbcmd(), dbfcmd(), dbnextrow(), dbresults(), dbretstatus(), dbsettime(), dbsqlok(), dbsqlsend()
 * \todo We need to observe the timeout value and abort if this times out.
 */
RETCODE
dbsqlexec(DBPROCESS * dbproc)
{
	RETCODE rc = FAIL;
	TDSSOCKET *tds;

	if (dbproc == NULL) {
		return FAIL;
	}
	tdsdump_log(TDS_DBG_FUNC, "in dbsqlexec()\n");

	tds = dbproc->tds_socket;
	if (IS_TDSDEAD(tds))
		return FAIL;

	if (SUCCEED == (rc = dbsqlsend(dbproc))) {
		/* 
		 * XXX We need to observe the timeout value and abort 
		 * if this times out.
		 */
		rc = dbsqlok(dbproc);
	}
	return rc;
}

/**
 * \ingroup dblib_api
 * \brief Change current database. 
 * 
 * Analagous to the unix command \c cd, dbuse() makes \a name the default database.  Waits for an answer
 * from the server.  
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param name database to use.
 * \retval SUCCEED query was processed without errors.
 * \retval FAIL query was not processed
 * \todo \a name should be quoted.
 * \sa dbchange(), dbname().
 */
RETCODE
dbuse(DBPROCESS * dbproc, char *name)
{
	RETCODE rc;
	char *query;
	tdsdump_log(TDS_DBG_FUNC, "dbuse()\n");

	if (!dbproc || !dbproc->tds_socket)
		return FAIL;

	/* quote name */
	query = (char *) malloc(tds_quote_id(dbproc->tds_socket, NULL, name, -1) + 6);
	if (!query)
		return FAIL;
	strcpy(query, "use ");
	/* TODO PHP suggest to quote by yourself with []... what should I do ?? quote or not ?? */
	if (name[0] == '[' && name[strlen(name)-1] == ']')
		strcat(query, name);
	else
		tds_quote_id(dbproc->tds_socket, query + 4, name, -1);

	rc = SUCCEED;
	if ((dbcmd(dbproc, query) == FAIL)
	    || (dbsqlexec(dbproc) == FAIL)
	    || (dbresults(dbproc) == FAIL)
	    || (dbcanquery(dbproc) == FAIL))
		rc = FAIL;
	free(query);
	return rc;
}

static void
free_linked_dbopt(DBOPTION * dbopt)
{
	if (dbopt == NULL) {
		return;
	}
	if (dbopt->optnext) {
		free_linked_dbopt(dbopt->optnext);
	}
	dbstring_free(&(dbopt->optparam));
	free(dbopt);
}

/**
 * \ingroup dblib_api
 * \brief Close a connection to the server and free associated resources.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbexit(), dbopen().
 */
void
dbclose(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;
	int i;
	char timestr[256];

	tds = dbproc->tds_socket;
	if (tds) {
		buffer_free(&(dbproc->row_buf));
		tds_free_socket(tds);
	}

	if (dbproc->ftos != NULL) {
		fprintf(dbproc->ftos, "/* dbclose() at %s */\n", _dbprdate(timestr));
		fclose(dbproc->ftos);
	}

	if (dbproc->bcpinfo) {
		if (dbproc->bcpinfo->tablename)
			free(dbproc->bcpinfo->tablename);
	}
	if (dbproc->hostfileinfo) {
		if (dbproc->hostfileinfo->hostfile)
			free(dbproc->hostfileinfo->hostfile);
		if (dbproc->hostfileinfo->errorfile)
			free(dbproc->hostfileinfo->errorfile);
		if (dbproc->hostfileinfo->host_columns) {
			for (i = 0; i < dbproc->hostfileinfo->host_colcount; i++) {
				if (dbproc->hostfileinfo->host_columns[i]->terminator)
					free(dbproc->hostfileinfo->host_columns[i]->terminator);
				free(dbproc->hostfileinfo->host_columns[i]);
			}
			free(dbproc->hostfileinfo->host_columns);
		}
	}

	for (i = 0; i < DBNUMOPTIONS; i++) {
		free_linked_dbopt(dbproc->dbopts[i].optnext);
		dbstring_free(&(dbproc->dbopts[i].optparam));
	}
	free(dbproc->dbopts);

	dbstring_free(&(dbproc->dboptcmd));

	dbfreebuf(dbproc);
	dblib_del_connection(&g_dblib_ctx, dbproc->tds_socket);
	free(dbproc);

	return;
}

/**
 * \ingroup dblib_api
 * \brief Close server connections and free all related structures.  
 * 
 * \sa dbclose(), dbinit(), dbopen().
 * \todo breaks if ctlib/dblib used in same process.
 */
void
dbexit()
{
	TDSSOCKET *tds;
	DBPROCESS *dbproc;
	int i;
	const int list_size = g_dblib_ctx.connection_list_size;


	/* FIX ME -- this breaks if ctlib/dblib used in same process */
	for (i = 0; i < list_size; i++) {
		tds = g_dblib_ctx.connection_list[i];
		if (tds) {
			dbproc = (DBPROCESS *) tds->parent;
			if (dbproc) {
				dbclose(dbproc);
			}
		}
	}
	if (g_dblib_ctx.connection_list) {
		TDS_ZERO_FREE(g_dblib_ctx.connection_list);
		g_dblib_ctx.connection_list_size = 0;
	}
	if (g_dblib_ctx.tds_ctx) {
		tds_free_context(g_dblib_ctx.tds_ctx);
		g_dblib_ctx.tds_ctx = NULL;
	}
}

/**
 * \ingroup dblib_api
 * \brief Return number of regular columns in a result set.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbcollen(), dbcolname(), dbnumalts().
 */

RETCODE
dbresults(DBPROCESS * dbproc)
{
	RETCODE retcode = FAIL;
	TDSSOCKET *tds;
	int result_type;
	int done_flags;

	tdsdump_log(TDS_DBG_FUNC, "dbresults()\n");

	if (dbproc == NULL)
		return FAIL;

	buffer_clear(&(dbproc->row_buf));

	tds = dbproc->tds_socket;

	if (IS_TDSDEAD(tds))
		return FAIL;

	switch ( dbproc->dbresults_state ) {

	case _DB_RES_SUCCEED:
		dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
		return SUCCEED;
		break;
	case _DB_RES_RESULTSET_ROWS:
		/* dbresults called while rows outstanding.... */
		_dblib_client_msg(dbproc, 20019, 7, "Attempt to initiate a new SQL Server operation with results pending.");
		return FAIL;
		break;
	case _DB_RES_NO_MORE_RESULTS:
		return NO_MORE_RESULTS;
		break;
	case _DB_RES_NEXT_RESULT:
	case _DB_RES_INIT:
		tds_free_all_results(tds);
		break;
	default:
		break;
	}

	for (;;) {

		retcode = tds_process_tokens(tds, &result_type, &done_flags, TDS_TOKEN_RESULTS);

		tdsdump_log(TDS_DBG_FUNC, "dbresults() process_result_tokens returned result_type = %d retcode = %d\n", 
								  result_type, retcode);

		switch (retcode) {

		case TDS_SUCCEED:

			switch (result_type) {
	
			case TDS_ROWFMT_RESULT:
				retcode = buffer_start_resultset(&(dbproc->row_buf), tds->res_info->row_size);
				dbproc->dbresults_state = _DB_RES_RESULTSET_EMPTY;
				break;
	
			case TDS_COMPUTEFMT_RESULT:
				break;
	
			case TDS_ROW_RESULT:
			case TDS_COMPUTE_RESULT:
	
				dbproc->dbresults_state = _DB_RES_RESULTSET_ROWS;
				return SUCCEED;
				break;
	
			case TDS_DONE_RESULT:

				/* A done token signifies the end of a logical command.
				 * There are three possibilities:
				 * 1. Simple command with no result set, i.e. update, delete, insert
				 * 2. Command with result set but no rows
				 * 3. Command with result set and rows
				 */

				switch (dbproc->dbresults_state) {

				case _DB_RES_INIT:
				case _DB_RES_NEXT_RESULT:
					dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
					if (done_flags & TDS_DONE_ERROR)
						return FAIL;
					else
						return SUCCEED;
					break;

				case _DB_RES_RESULTSET_EMPTY:
				case _DB_RES_RESULTSET_ROWS:
					dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
					return SUCCEED;
					break;
				default:
					assert(0);
					break;
				}
				

			case TDS_DONEPROC_RESULT:
			case TDS_DONEINPROC_RESULT:

				/* We should only return SUCCEED on a command within a */
				/* stored procedure, if the command returned a result */
				/* set...                                             */

				switch (dbproc->dbresults_state) {

				case _DB_RES_INIT :  
				case _DB_RES_NEXT_RESULT : 
					dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
					break;

				case _DB_RES_RESULTSET_EMPTY :
				case _DB_RES_RESULTSET_ROWS : 
					dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
					return SUCCEED;
					break;
				}
				break;

			case TDS_MSG_RESULT:
			case TDS_DESCRIBE_RESULT:
			case TDS_STATUS_RESULT:
			case TDS_PARAM_RESULT:
			default:
				break;
			}

			break;

		case TDS_NO_MORE_RESULTS:
			switch(dbproc->dbresults_state) {
			case _DB_RES_INIT:  /* dbsqlok has eaten our end token */
				dbproc->dbresults_state = _DB_RES_NO_MORE_RESULTS;
				return SUCCEED;
				break;
			default:
				dbproc->dbresults_state = _DB_RES_NO_MORE_RESULTS;
				return NO_MORE_RESULTS;
				break;
			}
			break;

		case TDS_FAIL:
			dbproc->dbresults_state = _DB_RES_INIT;
			return FAIL;
			break;
			
		default:
			tdsdump_log(TDS_DBG_FUNC, "dbresults() does not recognize return code from process_result_tokens\n");
			assert(0);
			return FAIL;
			break;
		}
	}
}


/**
 * \ingroup dblib_api
 * \brief Return number of regular columns in a result set.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbcollen(), dbcolname(), dbnumalts().
 */
int
dbnumcols(DBPROCESS * dbproc)
{
	if (dbproc && dbproc->tds_socket && dbproc->tds_socket->res_info)
		return dbproc->tds_socket->res_info->num_cols;
	return 0;
}

/**
 * \ingroup dblib_api
 * \brief Return name of a regular result column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting with 1.  
 * \return pointer to ASCII null-terminated string, the name of the column. 
 * \retval NULL \a column is not in range.
 * \sa dbcollen(), dbcoltype(), dbdata(), dbdatlen(), dbnumcols().
 * \todo call the error handler with 10011 (SQLECNOR)
 * \bug Relies on ASCII column names, post iconv conversion.  
 *      Will not work as described for UTF-8 or UCS-2 clients.  
 *      But maybe it shouldn't.  
 */
char *
dbcolname(DBPROCESS * dbproc, int column)
{
	TDSRESULTINFO *resinfo;

	if (!dbproc || !dbproc->tds_socket || !dbproc->tds_socket->res_info)
		return NULL;
	resinfo = dbproc->tds_socket->res_info;

	if (column < 1 || column > resinfo->num_cols)
		return NULL;

	assert(resinfo->columns[column - 1]->column_name[resinfo->columns[column - 1]->column_namelen] == 0);
	return resinfo->columns[column - 1]->column_name;
}

/**
 * \ingroup dblib_api
 * \brief Read a row from the row buffer.
 * 
 * When row buffering is enabled (DBBUFFER option is on), the client can use dbgetrow() to re-read a row previously fetched 
 * with dbnextrow().  The effect is to move the row pointer -- analagous to fseek() -- back to \a row.  
 * Calls to dbnextrow() read from \a row + 1 until the buffer is exhausted, at which point it resumes
 * its normal behavior, except that as each row is fetched from the server, it is added to the row
 * buffer (in addition to being returned to the client).  When the buffer is filled, dbnextrow()  returns 
 * \c FAIL until the buffer is at least partially emptied with dbclrbuf().
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param row Nth row to read, starting with 1.
 * \retval REG_ROW returned row is a regular row.
 * \returns computeid when returned row is a compute row.
 * \retval NO_MORE_ROWS no such row in the row buffer.  Current row is unchanged.
 * \retval FAIL unsuccessful; row buffer may be full.  
 * \sa dbaltbind(), dbbind(), dbclrbuf(), DBCURROW(), DBFIRSTROW(), DBLASTROW(), dbnextrow(), dbsetrow().
 */
RETCODE
dbgetrow(DBPROCESS * dbproc, DBINT row)
{
	RETCODE result = FAIL;
	int idx = buffer_index_of_resultset_row(&(dbproc->row_buf), row);

	if (-1 == idx) {
		result = NO_MORE_ROWS;
	} else {
		dbproc->row_buf.next_row = row;
		buffer_transfer_bound_data(TDS_ROW_RESULT, 0, &(dbproc->row_buf), dbproc, row);
		dbproc->row_buf.next_row++;
		result = REG_ROW;
	}

	return result;
}

/**
 * \ingroup dblib_api
 * \brief Read result row into the row buffer and into any bound host variables.

 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval REG_ROW regular row has been read.
 * \returns computeid when a compute row is read. 
 * \retval BUF_FULL reading next row would cause the buffer to be exceeded (and buffering is turned on).
 * No row was read from the server
 * \sa dbaltbind(), dbbind(), dbcanquery(), dbclrbuf(), dbgetrow(), dbprrow(), dbsetrow().
 */
RETCODE
dbnextrow(DBPROCESS * dbproc)
{
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	RETCODE result = FAIL;
	TDS_INT res_type;
	TDS_INT computeid;

	tdsdump_log(TDS_DBG_FUNC, "dbnextrow()\n");

	if (dbproc == NULL)
		return FAIL;
		
	tds = dbproc->tds_socket;
	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_FUNC, "leaving dbnextrow() returning %d\n", FAIL);
		return FAIL;
	}

	resinfo = tds->res_info;

	/* no result set or result set empty (no rows) */

	tdsdump_log(TDS_DBG_FUNC, "dbnextrow() dbresults_state = %d\n", dbproc->dbresults_state);

	if (!resinfo || dbproc->dbresults_state != _DB_RES_RESULTSET_ROWS) {
		tdsdump_log(TDS_DBG_FUNC, "leaving dbnextrow() returning %d\n", NO_MORE_ROWS);
		return dbproc->row_type = NO_MORE_ROWS;
	}

	if (dbproc->row_buf.buffering_on && buffer_is_full(&(dbproc->row_buf))
	    && (-1 == buffer_index_of_resultset_row(&(dbproc->row_buf), dbproc->row_buf.next_row))) {
		result = BUF_FULL;
	} else {
		/* If no rows are read, DBROWTYPE() will report NO_MORE_ROWS. */
		dbproc->row_type = NO_MORE_ROWS; 
		computeid = REG_ROW;

		/*
		 * Try to get the dbproc->row_buf.next_row item into the row buffer.
		 */
		if (-1 != buffer_index_of_resultset_row(&(dbproc->row_buf), dbproc->row_buf.next_row)) {
			/*
			 * Cool, the item we want is already there
			 */
			result = dbproc->row_type = REG_ROW;
			res_type = TDS_ROW_RESULT;
		} else {

			/* Get the row from the TDS stream.  */

			switch (tds_process_tokens(dbproc->tds_socket, &res_type, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE|TDS_RETURN_ROW|TDS_RETURN_COMPUTE)) {
			case TDS_SUCCEED:
				if (res_type == TDS_ROW_RESULT || res_type == TDS_COMPUTE_RESULT) {
					if (res_type == TDS_COMPUTE_RESULT)
						computeid = tds->current_results->computeid;
					/* Add the row to the row buffer */
					resinfo = tds->current_results;
					buffer_add_row(&(dbproc->row_buf), resinfo->current_row, resinfo->row_size);
					result = dbproc->row_type = (res_type == TDS_ROW_RESULT)? REG_ROW : computeid;
					break;
				}
			case TDS_NO_MORE_RESULTS:
				dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
				result = NO_MORE_ROWS;
				break;
			default:
				result = FAIL;
				break;
			}
		}

		if (res_type == TDS_ROW_RESULT || res_type == TDS_COMPUTE_RESULT) {
			/*
			 * Transfer the data from the row buffer to the bound variables.  
			 */
			buffer_transfer_bound_data(res_type, computeid, &(dbproc->row_buf), dbproc, dbproc->row_buf.next_row);
			dbproc->row_buf.next_row++;
		}
	}
	
	tdsdump_log(TDS_DBG_FUNC, "leaving dbnextrow() returning %d\n", result);
	return result;
} /* dbnextrow()  */

static int
_db_get_server_type(int bindtype)
{
	switch (bindtype) {
	case CHARBIND:
	case STRINGBIND:
	case NTBSTRINGBIND:
		return SYBCHAR;
		break;
	case FLT8BIND:
		return SYBFLT8;
		break;
	case REALBIND:
		return SYBREAL;
		break;
	case INTBIND:
		return SYBINT4;
		break;
	case SMALLBIND:
		return SYBINT2;
		break;
	case TINYBIND:
		return SYBINT1;
		break;
	case DATETIMEBIND:
		return SYBDATETIME;
		break;
	case SMALLDATETIMEBIND:
		return SYBDATETIME4;
		break;
	case MONEYBIND:
		return SYBMONEY;
		break;
	case SMALLMONEYBIND:
		return SYBMONEY4;
		break;
	case BINARYBIND:
		return SYBBINARY;
		break;
	case VARYCHARBIND:
		return SYBVARCHAR;
		break;
	case BITBIND:
		return SYBBIT;
		break;
	case NUMERICBIND:
		return SYBNUMERIC;
		break;
	case DECIMALBIND:
		return SYBDECIMAL;
		break;
	default:
		return -1;
		break;
	}
}

/**
 * \ingroup dblib_api
 * \brief Convert one datatype to another.
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param srctype datatype of the data to convert. 
 * \param src buffer to convert
 * \param srclen length of \a src
 * \param desttype target datatype
 * \param dest output buffer
 * \param destlen size of \a dest
 * \returns	On success, the count of output bytes in \dest, else -1. On failure, it will call any user-supplied error handler. 
 * \remarks 	 Causes of failure: 
 * 		- No such conversion unavailable. 
 * 		- Character data output was truncated, or numerical data overflowed or lost precision. 
 * 		- In converting character data to one of the numeric types, the string could not be interpreted as a number.  
 *		
 * Conversion functions are handled in the TDS layer.
 * 
 * The main reason for this is that \c ct-lib and \c ODBC (and presumably \c DBI) need
 * to be able to do conversions between datatypes. This is possible because
 * the format of complex data (dates, money, numeric, decimal) is defined by
 * its representation on the wire; thus what we call \c DBMONEY is exactly its
 * format on the wire. CLIs that need a different representation (ODBC?) 
 * need to convert from this format anyway, so the code would already be in
 * place.
 * 
 * Each datatype is also defined by its Server-type so all CLIs should be 
 * able to map native types to server types as well.
 *
 * tds_convert() copies from src to dest and returns the output data length,
 * period.  All padding and termination is the responsibility of the API library
 * and is done post-conversion.  The peculiar rule in dbconvert() is that
 * a \a destlen of -1 and a \a desttype of \c SYBCHAR means the output buffer
 * should be null-terminated.
 *  
 * \sa dbaltbind(), dbaltbind_ps(), dbbind(), dbbind_ps(), dbconvert_ps(), dberrhandle(), dbsetnull(), dbsetversion(), dbwillconvert().
 * \todo What happens if client does not reset values? 
 * \todo Microsoft and Sybase define this function differently.  
 */
DBINT
dbconvert(DBPROCESS * dbproc, int srctype, const BYTE * src, DBINT srclen, int desttype, BYTE * dest, DBINT destlen)
{
	TDSSOCKET *tds = NULL;

	CONV_RESULT dres;
	DBINT ret;
	int i;
	int len;
	DBNUMERIC *num;

	tdsdump_log(TDS_DBG_INFO1, "dbconvert(%d [%s] len %d => %d [%s] len %d)\n", 
		     srctype, tds_prdatatype(srctype), srclen, desttype, tds_prdatatype(desttype), destlen);

	if (dbproc) {
		tds = dbproc->tds_socket;
	}

	if (src == NULL || srclen == 0) {

		/* FIX set appropriate NULL value for destination type */
		if (destlen > 0)
			memset(dest, '\0', destlen);
		if (destlen == -1 || destlen == -2)
			*dest = '\0';
		return 0;
	}

	/* srclen of -1 means the source data is definitely NULL terminated */
	if (srclen == -1)
		srclen = strlen((const char *) src);

	if (dest == NULL) {
		/* FIX call error handler */
		return -1;
	}


	/* oft times we are asked to convert a data type to itself */

	if (srctype == desttype) {
		ret = -2;  /* to make sure we always set it */
		tdsdump_log(TDS_DBG_INFO1, "dbconvert() srctype == desttype\n");
		switch (desttype) {

		case SYBBINARY:
		case SYBVARBINARY:
		case SYBIMAGE:
			if (srclen > destlen && destlen >= 0) {
				_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
				ret = -1;
			} else {
				memcpy(dest, src, srclen);
				if (srclen < destlen)
					memset(dest + srclen, 0, destlen - srclen);
				ret = srclen;
			}
			break;

		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:
			/* srclen of -1 means the source data is definitely NULL terminated */
			if (srclen == -1)
				srclen = strlen((const char *) src);

			switch (destlen) {
			case  0:	/* nothing to copy */
				ret = 0;
				break;
			case -1:	/* rtrim and null terminate */
				while (srclen && src[srclen - 1] == ' ') {
					--srclen;
				}
				/* fall thru */
			case -2:	/* just null terminate */
				memcpy(dest, src, srclen);
				dest[srclen] = '\0';
				ret = srclen;
				break;
			default:
				assert(destlen > 0);
				if (destlen < 0 || srclen > destlen) {
					_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
					ret = -1;
				} else {
					memcpy(dest, src, srclen);
					for (i = srclen; i < destlen; i++)
						dest[i] = ' ';
					ret = srclen;
				}
				break;
			}
			break;
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBINT8:
		case SYBFLT8:
		case SYBREAL:
		case SYBBIT:
		case SYBBITN:
		case SYBMONEY:
		case SYBMONEY4:
		case SYBDATETIME:
		case SYBDATETIME4:
		case SYBUNIQUE:
			ret = tds_get_size_by_type(desttype);
			memcpy(dest, src, ret);
			break;

		case SYBNUMERIC:
		case SYBDECIMAL:
			memcpy(dest, src, sizeof(DBNUMERIC));
			ret = sizeof(DBNUMERIC);
			break;

		default:
			ret = -1;
			break;
		}
		assert(ret > -2);
		return ret;
	}
	/* end srctype == desttype */
	assert(srctype != desttype);

	/*
	 * Character types need no conversion.  Just move the data.
	 */
	if (is_similar_type(srctype, desttype)) {
		if (src && dest && srclen > 0 && destlen >= srclen) {
			memcpy(dest, src, srclen);
			return srclen;
		}
	}

	/* FIXME what happen if client do not reset values ??? */
	/* FIXME act differently for ms and sybase */
	if (is_numeric_type(desttype)) {
		num = (DBNUMERIC *) dest;
		if (num->precision == 0)
			dres.n.precision = 18;
		else
			dres.n.precision = num->precision;
		if (num->scale == 0)
			dres.n.scale = 0;
		else
			dres.n.scale = num->scale;
	}

	tdsdump_log(TDS_DBG_INFO1, "dbconvert() calling tds_convert\n");

	len = tds_convert(g_dblib_ctx.tds_ctx, srctype, (const TDS_CHAR *) src, srclen, desttype, &dres);
	tdsdump_log(TDS_DBG_INFO1, "dbconvert() called tds_convert returned %d\n", len);

	switch (len) {
	case TDS_CONVERT_NOAVAIL:
		_dblib_client_msg(NULL, SYBERDCN, EXCONVERSION, "Requested data-conversion does not exist.");
		return -1;
		break;
	case TDS_CONVERT_SYNTAX:
		_dblib_client_msg(NULL, SYBECSYN, EXCONVERSION, "Attempt to convert data stopped by syntax error in source field.");
		return -1;
		break;
	case TDS_CONVERT_NOMEM:
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return -1;
		break;
	case TDS_CONVERT_OVERFLOW:
		_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data conversion resulted in overflow.");
		return -1;
		break;
	case TDS_CONVERT_FAIL:
		return -1;
		break;
	default:
		if (len < 0) {
			return -1;
		}
		break;
	}

	switch (desttype) {
	case SYBBINARY:
	case SYBVARBINARY:
	case SYBIMAGE:
		if (len > destlen && destlen >= 0) {
			_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
			ret = -1;
		} else {
			memcpy(dest, dres.ib, len);
			free(dres.ib);
			if (len < destlen)
				memset(dest + len, 0, destlen - len);
			ret = len;
		}
		break;
	case SYBINT1:
		memcpy(dest, &(dres.ti), 1);
		ret = 1;
		break;
	case SYBINT2:
		memcpy(dest, &(dres.si), 2);
		ret = 2;
		break;
	case SYBINT4:
		memcpy(dest, &(dres.i), 4);
		ret = 4;
		break;
	case SYBINT8:
		memcpy(dest, &(dres.bi), 8);
		ret = 8;
		break;
	case SYBFLT8:
		memcpy(dest, &(dres.f), 8);
		ret = 8;
		break;
	case SYBREAL:
		memcpy(dest, &(dres.r), 4);
		ret = 4;
		break;
	case SYBBIT:
	case SYBBITN:
		memcpy(dest, &(dres.ti), 1);
		ret = 1;
		break;
	case SYBMONEY:
		memcpy(dest, &(dres.m), sizeof(TDS_MONEY));
		ret = sizeof(TDS_MONEY);
		break;
	case SYBMONEY4:
		memcpy(dest, &(dres.m4), sizeof(TDS_MONEY4));
		ret = sizeof(TDS_MONEY4);
		break;
	case SYBDATETIME:
		memcpy(dest, &(dres.dt), sizeof(TDS_DATETIME));
		ret = sizeof(TDS_DATETIME);
		break;
	case SYBDATETIME4:
		memcpy(dest, &(dres.dt4), sizeof(TDS_DATETIME4));
		ret = sizeof(TDS_DATETIME4);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		memcpy(dest, &(dres.n), sizeof(TDS_NUMERIC));
		ret = sizeof(TDS_NUMERIC);
		break;
	case SYBUNIQUE:
		memcpy(dest, &(dres.u), sizeof(TDS_UNIQUE));
		ret = sizeof(TDS_UNIQUE);
		break;
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
		tdsdump_log(TDS_DBG_INFO1, "dbconvert() outputting %d bytes character data destlen = %d \n", len, destlen);

		if (destlen < -2)
			destlen = 0;	/* failure condition */

		switch (destlen) {
		case 0:
			ret = FAIL;
			break;
		case -1:	/* rtrim and null terminate */
			for (i = len - 1; i >= 0 && dres.c[i] == ' '; --i) {
				len = i;
			}
			memcpy(dest, dres.c, len);
			dest[len] = '\0';
			ret = len;
			break;
		case -2:	/* just null terminate */
			memcpy(dest, dres.c, len);
			dest[len] = 0;
			ret = len;
			break;
		default:
			assert(destlen > 0);
			if (destlen < 0 || len > destlen) {
				_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
				ret = -1;
				tdsdump_log(TDS_DBG_INFO1, "%d bytes type %d -> %d, destlen %d < %d required\n",
					    srclen, srctype, desttype, destlen, len);
				break;
			}
			/* else pad with blanks */
			memcpy(dest, dres.c, len);
			for (i = len; i < destlen; i++)
				dest[i] = ' ';
			ret = len;

			break;
		}

		free(dres.c);

		break;
	default:
		tdsdump_log(TDS_DBG_INFO1, "error: dbconvert(): unrecognized desttype %d \n", desttype);
		ret = -1;
		break;

	}
	return (ret);
}

/**
 * \ingroup dblib_api
 * \brief cf. dbconvert(), above
 * 
 * \em Sybase: Convert numeric types.
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param srctype datatype of the data to convert. 
 * \param src buffer to convert
 * \param srclen length of \a src
 * \param desttype target datatype
 * \param dest output buffer
 * \param destlen size of \a dest
 * \param typeinfo address of a \c DBTYPEINFO structure that governs the precision & scale of the output, may be \c NULL.
 * \sa dbaltbind(), dbaltbind_ps(), dbbind(), dbbind_ps(), dbconvert(), dberrhandle(), dbsetnull(), dbsetversion(), dbwillconvert().
 */
DBINT
dbconvert_ps(DBPROCESS * dbproc,
	     int srctype, BYTE * src, DBINT srclen, int desttype, BYTE * dest, DBINT destlen, DBTYPEINFO * typeinfo)
{
	DBNUMERIC *s;
	DBNUMERIC *d;

	if (is_numeric_type(desttype)) {
		if (typeinfo == NULL) {
			if (is_numeric_type(srctype)) {
				s = (DBNUMERIC *) src;
				d = (DBNUMERIC *) dest;
				d->precision = s->precision;
				d->scale = s->scale;
			} else {
				d = (DBNUMERIC *) dest;
				d->precision = 18;
				d->scale = 0;
			}
		} else {
			d = (DBNUMERIC *) dest;
			d->precision = typeinfo->precision;
			d->scale = typeinfo->scale;
		}
	}

	return dbconvert(dbproc, srctype, src, srclen, desttype, dest, destlen);
}

/**
 * \ingroup dblib_api
 * \brief Tie a host variable to a result set column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth column, starting at 1.
 * \param vartype datatype of the host variable that will receive the data
 * \param varlen size of host variable pointed to \a varaddr
 * \param varaddr address of host variable
 * \retval SUCCEED everything worked.
 * \retval FAIL no such \a column or no such conversion possible, or target buffer too small.
 * \sa 
 */
RETCODE
dbbind(DBPROCESS * dbproc, int column, int vartype, DBINT varlen, BYTE * varaddr)
{
	TDSCOLUMN *colinfo = NULL;
	TDSRESULTINFO *resinfo = NULL;
	TDSSOCKET *tds = NULL;
	int srctype = -1;
	int desttype = -1;
	int okay = TRUE;	/* so far, so good */
	TDS_SMALLINT num_cols = 0;

	tdsdump_log(TDS_DBG_INFO1, "dbbind() column = %d %d %d\n", column, vartype, varlen);
	dbproc->avail_flag = FALSE;
	/* 
	 * Note on logic-  I'm using a boolean variable 'okay' to tell me if
	 * everything that has happened so far has gone okay.  Basically if 
	 * something happened that wasn't okay we don't want to keep doing 
	 * things, but I also don't want to have a half dozen exit points from 
	 * this function.  So basically I've wrapped each set of operation in a 
	 * "if (okay)" statement.  Once okay becomes false we skip everything 
	 * else.
	 */
	okay = (dbproc != NULL && dbproc->tds_socket != NULL && varaddr != NULL);

	if (okay) {
		tds = dbproc->tds_socket;
		resinfo = tds->res_info;
	}

	if (resinfo) {
		num_cols = resinfo->num_cols;
	}
	okay = okay && ((column >= 1) && (column <= num_cols));
	if (!okay) {
		_dblib_client_msg(dbproc, SYBEABNC, EXPROGRAM, "Attempt to bind to a non-existent column.");
		return FAIL;
	}

	if (okay) {
		colinfo = resinfo->columns[column - 1];
		srctype = tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
		desttype = _db_get_server_type(vartype);

		tdsdump_log(TDS_DBG_INFO1, "dbbind() srctype = %d desttype = %d \n", srctype, desttype);

		okay = okay && dbwillconvert(srctype, _db_get_server_type(vartype));
	}

	if (okay) {
		colinfo->column_varaddr = (char *) varaddr;
		colinfo->column_bindtype = vartype;
		colinfo->column_bindlen = varlen;
	}

	return okay ? SUCCEED : FAIL;
}				/* dbbind()  */

/**
 * \ingroup dblib_api
 * \brief set name and location of the \c interfaces file FreeTDS should use to look up a servername.
 * 
 * Does not affect lookups or location of \c freetds.conf.  
 * \param filename name of \c interfaces 
 * \sa dbopen()
 */
void
dbsetifile(char *filename)
{
	tds_set_interfaces_file_loc(filename);
}

/**
 * \ingroup dblib_api
 * \brief Tie a null-indicator to a regular result column.
 * 
 * 
 * When a row is fetched, the indicator variable tells the state of the column's data.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth column in the result set, starting with 1.
 * \param indicator address of host variable.
 * \retval SUCCEED variable accepted.
 * \retval FAIL \a indicator is NULL or \a column is out of range. 
 * \remarks Contents of \a indicator are set with \c dbnextrow().  Possible values are:
 * -  0 \a column bound successfully
 * - -1 \a column is NULL.
 * - >0 true length of data, had \a column not been truncated due to insufficient space in the columns bound host variable .  
 * \sa dbanullbind(), dbbind(), dbdata(), dbdatlen(), dbnextrow().
 * \todo Never fails, but only because failure conditions aren't checked.  
 */
RETCODE
dbnullbind(DBPROCESS * dbproc, int column, DBINT * indicator)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	/*
	 *  XXX Need to check for possibly problems before assuming
	 *  everything is okay
	 */
	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	colinfo->column_nullbind = (TDS_SMALLINT *)indicator;

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Tie a null-indicator to a compute result column.
 * 
 * 
 * When a row is fetched, the indicator variable tells the state of the column's data.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid identifies which one of potientially many compute rows is meant. The first compute
 * clause has \a computeid == 1.  
 * \param column Nth column in the result set, starting with 1.
 * \param indicator address of host variable.
 * \retval SUCCEED variable accepted.
 * \retval FAIL \a indicator is NULL or \a column is out of range. 
 * \remarks Contents of \a indicator are set with \c dbnextrow().  Possible values are:
 * -  0 \a column bound successfully
 * - -1 \a column is NULL.
 * - >0 true length of data, had \a column not been truncated due to insufficient space in the columns bound host variable .  
 * \sa dbadata(), dbadlen(), dbaltbind(), dbnextrow(), dbnullbind().
 * \todo Never fails, but only because failure conditions aren't checked.  
 */
RETCODE
dbanullbind(DBPROCESS * dbproc, int computeid, int column, DBINT * indicator)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *curcol;
	TDS_SMALLINT compute_id;
	int i;

	compute_id = computeid;
	tdsdump_log(TDS_DBG_FUNC, "in dbanullbind(%d,%d)\n", compute_id, column);

	tdsdump_log(TDS_DBG_FUNC, "in dbanullbind() num_comp_info = %d\n", tds->num_comp_info);
	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return FAIL;
		info = tds->comp_info[i];
		tdsdump_log(TDS_DBG_FUNC, "in dbanullbind() found computeid = %d\n", info->computeid);
		if (info->computeid == compute_id)
			break;
	}
	tdsdump_log(TDS_DBG_FUNC, "in dbanullbind() num_cols = %d\n", info->num_cols);

	if (column < 1 || column > info->num_cols)
		return FAIL;

	curcol = info->columns[column - 1];
	/*
	 *  XXX Need to check for possibly problems before assuming
	 *  everything is okay
	 */
	curcol->column_nullbind = (TDS_SMALLINT *)indicator;

	return SUCCEED;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get count of rows processed
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \returns 
 * 	- for insert/update/delete, count of rows affected.
 * 	- for select, count of rows returned, after all rows have been fetched.  
 * \sa DBCOUNT(), dbnextrow(), dbresults().
 */
DBINT
dbcount(DBPROCESS * dbproc)
{
	if (!dbproc || !dbproc->tds_socket || dbproc->tds_socket->rows_affected == TDS_NO_COUNT)
		return -1;
	return dbproc->tds_socket->rows_affected;
}

/**
 * \ingroup dblib_api
 * \brief Clear \a n rows from the row buffer.
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server. 
 * \param n number of rows to remove, >= 0.
 * \sa dbgetrow(), dbnextrow(), dbsetopt().
 */
void
dbclrbuf(DBPROCESS * dbproc, DBINT n)
{
	if (n <= 0)
		return;

	if (dbproc->row_buf.buffering_on) {
		if (n >= dbproc->row_buf.rows_in_buf) {
			buffer_delete_rows(&(dbproc->row_buf), dbproc->row_buf.rows_in_buf - 1);
		} else {
			buffer_delete_rows(&(dbproc->row_buf), n);
		}
	}
}

/**
 * \ingroup dblib_api
 * \brief Test whether or not a datatype can be converted to another datatype
 * 
 * \param srctype type converting from
 * \param desttype type converting to
 * \remarks dbwillconvert() lies sometimes.  Some datatypes \em should be convertible but aren't yet in
 our implementation.  
 * \retval TRUE convertible, or should be. Legal unimplemented conversions return \em TRUE.  
 * \retval FAIL not convertible.  
 * \sa dbaltbind(), dbbind(), dbconvert(), dbconvert_ps(), \c src/dblib/unittests/convert().c().
 */
DBBOOL
dbwillconvert(int srctype, int desttype)
{
	return tds_willconvert(srctype, desttype);
}

/**
 * \ingroup dblib_api
 * \brief Get the datatype of a regular result set column. 
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \returns \c SYB* datetype token value, or zero if \a column out of range
 * \sa dbcollen(), dbcolname(), dbdata(), dbdatlen(), dbnumcols(), dbprtype(), dbvarylen().
 * \todo Check that \a column is in range.  Sybase says failure is -1, not zero. 
 */
int
dbcoltype(DBPROCESS * dbproc, int column)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	switch (colinfo->column_type) {
	case SYBVARCHAR:
		return SYBCHAR;
	case SYBVARBINARY:
		return SYBBINARY;
	default:
		return tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
	}
	return 0;		/* something went wrong */
}

/**
 * \ingroup dblib_api
 * \brief Get user-defined datatype of a regular result column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \returns \c SYB* datetype token value, or -1 if \a column out of range
 * \sa dbaltutype(), dbcoltype().
 * \todo Check that \a column is in range.  
 */
int
dbcolutype(DBPROCESS * dbproc, int column)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	return colinfo->column_usertype;
}

/**
 * \ingroup dblib_api
 * \brief Get precision and scale information for a regular result column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \sa dbcollen(), dbcolname(), dbcoltype(), dbdata(), dbdatlen(), dbnumcols(), dbprtype(), dbvarylen().
 */
DBTYPEINFO *
dbcoltypeinfo(DBPROCESS * dbproc, int column)
{
	/* moved typeinfo from static into dbproc structure to make thread safe.  (mlilback 11/7/01) */
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	dbproc->typeinfo.precision = colinfo->column_prec;
	dbproc->typeinfo.scale = colinfo->column_scale;
	return &dbproc->typeinfo;
}

/**
 * \ingroup dblib_api
 * \brief Get a bunch of column attributes with a single call (Microsoft-compatibility feature).  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param type must be CI_REGULAR.  (CI_ALTERNATE and CI_CURSOR are defined by the vendor, but are not yet implemented).
 * \param column Nth in the result set, starting from 1.
 * \param computeid (ignored)
 * \param pdbcol address of structure to be populated by this function.  
 * \return SUCCEED or FAIL. 
 * \sa dbcolbrowse(), dbqual(), dbtabbrowse(), dbtabcount(), dbtabname(), dbtabsource(), dbtsnewlen(), dbtsnewval(), dbtsput().
 * \todo Support compute and cursor rows. 
 */
DBINT	
dbcolinfo (DBPROCESS *dbproc, CI_TYPE type, DBINT column, DBINT computeid, DBCOL *pdbcol )
{
	DBTYPEINFO *ps;

	if (!dbproc || !pdbcol)
		return FAIL;

	strcpy(pdbcol->Name, dbcolname(dbproc, column));
	strcpy(pdbcol->ActualName, dbcolname(dbproc, column));
	
	pdbcol->Type = dbcoltype(dbproc, column);
	pdbcol->UserType = dbcolutype(dbproc, column);
	pdbcol->MaxLength = dbcollen(dbproc, column);
	pdbcol->Null = pdbcol->VarLength = dbvarylen(dbproc, column);

	ps = dbcoltypeinfo(dbproc, column);

	if( ps ) {
		pdbcol->Precision = ps->precision;
		pdbcol->Scale = ps->scale;
	}

	if( computeid ) {
		strcpy(pdbcol->TableName, dbcolname(dbproc, column));
	}

	return SUCCEED;
}


/**
 * \ingroup dblib_api
 * \brief Get base database column name for a result set column.  
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param colnum Nth in the result set, starting from 1.
 * \return pointer to ASCII null-terminated string, the name of the column. 
 * \sa dbcolbrowse(), dbqual(), dbtabbrowse(), dbtabcount(), dbtabname(), dbtabsource(), dbtsnewlen(), dbtsnewval(), dbtsput().
 */
char *
dbcolsource(DBPROCESS * dbproc, int colnum)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;

	/* check valid state */
	if (!dbproc || !dbproc->tds_socket || !dbproc->tds_socket->res_info)
		return NULL;
	resinfo = dbproc->tds_socket->res_info;

	/* check column index */
	if (colnum < 1 || colnum > resinfo->num_cols)
		return NULL;
	colinfo = resinfo->columns[colnum - 1];
	assert(colinfo->column_name[colinfo->column_namelen] == 0);
	return colinfo->column_name;
}

/**
 * \ingroup dblib_api
 * \brief Get size of a regular result column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \return size of the column (not of data in any particular row). 
 * \sa dbcolname(), dbcoltype(), dbdata(), dbdatlen(), dbnumcols().
 */
DBINT
dbcollen(DBPROCESS * dbproc, int column)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return -1;
	colinfo = resinfo->columns[column - 1];
	return colinfo->column_size;
}

/* dbvarylen(), pkleef@openlinksw.com 01/21/02 */
/**
 * \ingroup dblib_api
 * \brief Determine whether a column can vary in size.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \retval TRUE datatype of column can vary in size, or is nullable. 
 * \retval FALSE datatype of column is fixed and is not nullable. 
 * \sa dbcollen(), dbcolname(), dbcoltype(), dbdata(), dbdatlen(), dbnumcols(), dbprtype().
 */
DBINT
dbvarylen(DBPROCESS * dbproc, int column)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return FALSE;
	colinfo = resinfo->columns[column - 1];

	if (colinfo->column_cur_size < 0)
		return TRUE;

	switch (colinfo->column_type) {
		/* variable length fields */
	case SYBNVARCHAR:
	case SYBVARBINARY:
	case SYBVARCHAR:
		return TRUE;

		/* types that can be null */
	case SYBBITN:
	case SYBDATETIMN:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBINTN:
	case SYBMONEYN:
	case SYBNUMERIC:
		return TRUE;

		/* blob types */
	case SYBIMAGE:
	case SYBNTEXT:
	case SYBTEXT:
		return TRUE;
	}
	return FALSE;
}

/**
 * \ingroup dblib_api
 * \brief   Get size of current row's data in a regular result column.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \return size of the data, in bytes.
 * \sa dbcollen(), dbcolname(), dbcoltype(), dbdata(), dbnumcols().
 */
DBINT
dbdatlen(DBPROCESS * dbproc, int column)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	DBINT ret;

	/* FIXME -- this is the columns info, need per row info */
	/*
	 * Fixed by adding cur_row_size to colinfo, filled in by process_row
	 * in token.c. (mlilback, 11/7/01)
	 */
	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return -1;
	colinfo = resinfo->columns[column - 1];
	tdsdump_log(TDS_DBG_INFO1, "dbdatlen() type = %d\n", colinfo->column_type);

	if (colinfo->column_cur_size < 0)
		ret = 0;
	else
		ret = colinfo->column_cur_size;
	tdsdump_log(TDS_DBG_FUNC, "leaving dbdatlen() returning %d\n", ret);
	return ret;
}

/**
 * \ingroup dblib_api
 * \brief Get address of data in a regular result column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column Nth in the result set, starting from 1.
 * \return pointer the data, or NULL if data are NULL, or if \a column is out of range.  
 * \sa dbbind(), dbcollen(), dbcolname(), dbcoltype(), dbdatlen(), dbnumcols().
 */
BYTE *
dbdata(DBPROCESS * dbproc, int column)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return NULL;

	colinfo = resinfo->columns[column - 1];
	if (colinfo->column_cur_size < 0) {
		return NULL;
	}
	if (is_blob_type(colinfo->column_type)) {
		return (BYTE *) ((TDSBLOB *) (resinfo->current_row + colinfo->column_offset))->textvalue;
	}

	return (BYTE *) & resinfo->current_row[colinfo->column_offset];
}

/**
 * \ingroup dblib_api
 * \brief Cancel the current command batch.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval SUCCEED always.
 * \sa dbcanquery(), dbnextrow(), dbresults(), dbsetinterrupt(), dbsqlexec(), dbsqlok(), dbsqlsend().
 * \todo Check for failure and return accordingly.
 */
RETCODE
dbcancel(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;

	tds_send_cancel(dbproc->tds_socket);
	tds_process_cancel(dbproc->tds_socket);

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Determine size buffer required to hold the results returned by dbsprhead(), dbsprline(), and  dbspr1row().
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return size of buffer requirement, in bytes.  
 * \remarks An esoteric function.
 * \sa dbprhead(), dbprrow(), dbspr1row(), dbsprhead(), dbsprline().
 */
DBINT
dbspr1rowlen(DBPROCESS * dbproc)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int col, len = 0, collen, namlen;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = colinfo->column_namelen;
		len += collen > namlen ? collen : namlen;
	}
	/* the space between each column */
	len += (resinfo->num_cols - 1) * dbstring_length(dbproc->dbopts[DBPRCOLSEP].optparam);
	/* the newline */
	len += dbstring_length(dbproc->dbopts[DBPRLINESEP].optparam);

	return len;
}

/**
 * \ingroup dblib_api
 * \brief Print a regular result row to a buffer.  
 * 
 * Fills a buffer with one data row, represented as a null-terminated ASCII string.  Helpful for debugging.  
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param buffer \em output: Address of a buffer to hold ASCII null-terminated string.
 * \param buf_len size of \a buffer, in bytes. 
 * \retval SUCCEED on success.
 * \retval FAIL trouble encountered.  
 * \sa dbclropt(), dbisopt(), dbprhead(), dbprrow(), dbspr1rowlen(), dbsprhead(), dbsprline().
 */
RETCODE
dbspr1row(DBPROCESS * dbproc, char *buffer, DBINT buf_len)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	TDSDATEREC when;
	int i, col, collen, namlen;
	int desttype, srctype;
	int padlen;
	DBINT len;
	int c;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;

	if (dbnextrow(dbproc) != REG_ROW)
		return FAIL;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		if (colinfo->column_cur_size < 0) {
			len = 4;
			if (buf_len < len) {
				return FAIL;
			}
			strcpy(buffer, "NULL");
		} else {
			desttype = _db_get_server_type(STRINGBIND);
			srctype = tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
			if (srctype == SYBDATETIME || srctype == SYBDATETIME4) {
				memset(&when, 0, sizeof(when));
				tds_datecrack(srctype, dbdata(dbproc, col + 1), &when);
				len = tds_strftime(buffer, buf_len, "%b %e %Y %I:%M%p", &when);
			} else {
				len = dbconvert(dbproc, srctype, dbdata(dbproc, col + 1), -1, desttype, (BYTE *) buffer, buf_len);
			}
			if (len == -1) {
				return FAIL;
			}
		}
		buffer += len;
		buf_len -= len;
		collen = _get_printable_size(colinfo);
		namlen = colinfo->column_namelen;
		padlen = (collen > namlen ? collen : namlen) - len;
		if ((c = dbstring_getchar(dbproc->dbopts[DBPRPAD].optparam, 0)) == -1) {
			c = ' ';
		}
		for (; padlen > 0; padlen--) {
			if (buf_len < 1) {
				return FAIL;
			}
			*buffer++ = c;
			buf_len--;
		}
		i = 0;
		while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
			if (buf_len < 1) {
				return FAIL;
			}
			*buffer++ = c;
			buf_len--;
			i++;
		}
	}
	i = 0;
	while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
		if (buf_len < 1) {
			return FAIL;
		}
		*buffer++ = c;
		buf_len--;
		i++;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Print a result set to stdout. 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbbind(), dbnextrow(), dbprhead(), dbresults(), dbspr1row(), dbsprhead(), dbsprline(). 
 */
RETCODE
dbprrow(DBPROCESS * dbproc)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int i, col, collen, namlen, len;
	char dest[256];
	int desttype, srctype;
	TDSDATEREC when;
	DBINT status;
	int padlen;
	int c;
	int selcol;
	int linechar;
	int op;
	const char *opname;

	/* these are for compute rows */
	DBINT computeid, num_cols, colid;
	TDS_SMALLINT *col_printlens = NULL;

	tds = dbproc->tds_socket;

	while ((status = dbnextrow(dbproc)) != NO_MORE_ROWS) {

		if (status == FAIL) {
			return FAIL;
		}

		if (status == REG_ROW) {

			resinfo = tds->res_info;

			if (col_printlens == NULL) {
				col_printlens = (TDS_SMALLINT *) malloc(sizeof(TDS_SMALLINT) * resinfo->num_cols);
			}

			for (col = 0; col < resinfo->num_cols; col++) {
				colinfo = resinfo->columns[col];
				if (colinfo->column_cur_size < 0) {
					len = 4;
					strcpy(dest, "NULL");
				} else {
					desttype = _db_get_server_type(STRINGBIND);
					srctype = tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
					if (srctype == SYBDATETIME || srctype == SYBDATETIME4) {
						memset(&when, 0, sizeof(when));
						tds_datecrack(srctype, dbdata(dbproc, col + 1), &when);
						len = tds_strftime(dest, sizeof(dest), "%b %e %Y %I:%M%p", &when);
					} else {
						len = dbconvert(dbproc, srctype, dbdata(dbproc, col + 1), -1, desttype,
								(BYTE *) dest, sizeof(dest));
					}
				}

				printf("%.*s", len, dest);
				collen = _get_printable_size(colinfo);
				namlen = colinfo->column_namelen;
				padlen = (collen > namlen ? collen : namlen) - len;
				c = dbstring_getchar(dbproc->dbopts[DBPRPAD].optparam, 0);
				if (c == -1) {
					c = ' ';
				}
				for (; padlen > 0; padlen--) {
					putchar(c);
				}

				i = 0;
				while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
					putchar(c);
					i++;
				}
				col_printlens[col] = collen;
			}
			i = 0;
			while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
				putchar(c);
				i++;
			}

		} else {

			computeid = status;

			for (i = 0;; ++i) {
				if (i >= tds->num_comp_info)
					return FAIL;
				resinfo = tds->comp_info[i];
				if (resinfo->computeid == computeid)
					break;
			}

			num_cols = dbnumalts(dbproc, computeid);
			tdsdump_log(TDS_DBG_FUNC, "dbprrow num compute cols = %d\n", num_cols);

			i = 0;
			while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
				putchar(c);
				i++;
			}
			for (selcol = col = 1; col <= num_cols; col++) {
				tdsdump_log(TDS_DBG_FUNC, "dbprrow calling dbaltcolid(%d,%d)\n", computeid, col);
				colid = dbaltcolid(dbproc, computeid, col);
				while (selcol < colid) {
					for (i = 0; i < col_printlens[selcol - 1]; i++) {
						putchar(' ');
					}
					selcol++;
					i = 0;
					while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
						putchar(c);
						i++;
					}
				}
				op = dbaltop(dbproc, computeid, col);
				opname = dbprtype(op);
				printf("%s", opname);
				for (i = 0; i < col_printlens[selcol - 1] - strlen(opname); i++) {
					putchar(' ');
				}
				selcol++;
				i = 0;
				while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
					putchar(c);
					i++;
				}
			}
			i = 0;
			while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
				putchar(c);
				i++;
			}

			for (selcol = col = 1; col <= num_cols; col++) {
				tdsdump_log(TDS_DBG_FUNC, "dbprrow calling dbaltcolid(%d,%d)\n", computeid, col);
				colid = dbaltcolid(dbproc, computeid, col);
				while (selcol < colid) {
					for (i = 0; i < col_printlens[selcol - 1]; i++) {
						putchar(' ');
					}
					selcol++;
					i = 0;
					while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
						putchar(c);
						i++;
					}
				}
				if (resinfo->by_cols > 0) {
					linechar = '-';
				} else {
					linechar = '=';
				}
				for (i = 0; i < col_printlens[colid - 1]; i++)
					putchar(linechar);
				selcol++;
				i = 0;
				while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
					putchar(c);
					i++;
				}
			}
			i = 0;
			while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
				putchar(c);
				i++;
			}

			for (selcol = col = 1; col <= num_cols; col++) {
				colinfo = resinfo->columns[col - 1];

				desttype = _db_get_server_type(STRINGBIND);
				srctype = dbalttype(dbproc, computeid, col);

				if (srctype == SYBDATETIME || srctype == SYBDATETIME4) {
					memset(&when, 0, sizeof(when));
					tds_datecrack(srctype, dbadata(dbproc, computeid, col), &when);
					len = tds_strftime(dest, sizeof(dest), "%b %e %Y %I:%M%p", &when);
				} else {
					len = dbconvert(dbproc, srctype, dbadata(dbproc, computeid, col), -1, desttype,
							(BYTE *) dest, sizeof(dest));
				}

				tdsdump_log(TDS_DBG_FUNC, "dbprrow calling dbaltcolid(%d,%d)\n", computeid, col);
				colid = dbaltcolid(dbproc, computeid, col);
				tdsdump_log(TDS_DBG_FUNC, "dbprrow select column = %d\n", colid);

				while (selcol < colid) {
					for (i = 0; i < col_printlens[selcol - 1]; i++) {
						putchar(' ');
					}
					selcol++;
					i = 0;
					while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
						putchar(c);
						i++;
					}
				}
				printf("%.*s", len, dest);
				collen = _get_printable_size(colinfo);
				namlen = colinfo->column_namelen;
				padlen = (collen > namlen ? collen : namlen) - len;
				if ((c = dbstring_getchar(dbproc->dbopts[DBPRPAD].optparam, 0)) == -1) {
					c = ' ';
				}
				for (; padlen > 0; padlen--) {
					putchar(c);
				}
				selcol++;
				i = 0;
				while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
					putchar(c);
					i++;
				}
			}
		}
	}

	if (col_printlens != NULL)
		free(col_printlens);

	return SUCCEED;
}

static int
_get_printable_size(TDSCOLUMN * colinfo)
{
	switch (colinfo->column_type) {
	case SYBINTN:
		switch (colinfo->column_size) {
		case 1:
			return 3;
		case 2:
			return 6;
		case 4:
			return 11;
		case 8:
			return 21;
		}
	case SYBINT1:
		return 3;
	case SYBINT2:
		return 6;
	case SYBINT4:
		return 11;
	case SYBINT8:
		return 21;
	case SYBVARCHAR:
	case SYBCHAR:
		return colinfo->column_size;
	case SYBFLT8:
		return 11;	/* FIX ME -- we do not track precision */
	case SYBREAL:
		return 11;	/* FIX ME -- we do not track precision */
	case SYBMONEY:
		return 12;	/* FIX ME */
	case SYBMONEY4:
		return 12;	/* FIX ME */
	case SYBDATETIME:
		return 26;	/* FIX ME */
	case SYBDATETIME4:
		return 26;	/* FIX ME */
	case SYBBIT:
	case SYBBITN:
		return 1;
		/* FIX ME -- not all types present */
	default:
		return 0;
	}

}

/**
 * \ingroup dblib_api
 * \brief Get formatted string for underlining dbsprhead() column names.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param buffer output buffer
 * \param buf_len size of \a buffer
 * \param line_char character to use to represent underlining.
 * \retval SUCCEED \a buffer filled.
 * \retval FAIL insufficient space in \a buffer, usually.
 * \sa dbprhead(), dbprrow(), dbspr1row(), dbspr1rowlen(), dbsprhead(). 
 */
RETCODE
dbsprline(DBPROCESS * dbproc, char *buffer, DBINT buf_len, DBCHAR line_char)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int i, col, len, collen, namlen;
	int c;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = colinfo->column_namelen;
		len = collen > namlen ? collen : namlen;
		for (i = 0; i < len; i++) {
			if (buf_len < 1) {
				return FAIL;
			}
			*buffer++ = line_char;
			buf_len--;
		}
		i = 0;
		while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
			if (buf_len < 1) {
				return FAIL;
			}
			*buffer++ = c;
			buf_len--;
			i++;
		}
	}
	i = 0;
	while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
		if (buf_len < 1) {
			return FAIL;
		}
		*buffer++ = c;
		buf_len--;
		i++;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Print result set headings to a buffer.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param buffer output buffer
 * \param buf_len size of \a buffer
 * \retval SUCCEED \a buffer filled.
 * \retval FAIL insufficient spaace in \a buffer, usually.
 * \sa dbprhead(), dbprrow(), dbsetopt(), dbspr1row(), dbspr1rowlen(), dbsprline().
 */
RETCODE
dbsprhead(DBPROCESS * dbproc, char *buffer, DBINT buf_len)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int i, col, collen, namlen;
	int padlen;
	int c;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = colinfo->column_namelen;
		padlen = (collen > namlen ? collen : namlen) - namlen;
		if (buf_len < namlen) {
			return FAIL;
		}
		strncpy(buffer, colinfo->column_name, namlen);
		buffer += namlen;
		if ((c = dbstring_getchar(dbproc->dbopts[DBPRPAD].optparam, 0)) == -1) {
			c = ' ';
		}
		for (; padlen > 0; padlen--) {
			if (buf_len < 1) {
				return FAIL;
			}
			*buffer++ = c;
			buf_len--;
		}
		i = 0;
		while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
			if (buf_len < 1) {
				return FAIL;
			}
			*buffer++ = c;
			buf_len--;
			i++;
		}
	}
	i = 0;
	while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
		if (buf_len < 1) {
			return FAIL;
		}
		*buffer++ = c;
		buf_len--;
		i++;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Print result set headings to stdout.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa 
 */
void
dbprhead(DBPROCESS * dbproc)
{
	TDSCOLUMN *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int i, col, len, collen, namlen;
	int padlen;
	int c;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo == NULL) {
		return;
	}
	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = colinfo->column_namelen;
		padlen = (collen > namlen ? collen : namlen) - namlen;
		printf("%*.*s", colinfo->column_namelen, colinfo->column_namelen, colinfo->column_name);

		c = dbstring_getchar(dbproc->dbopts[DBPRPAD].optparam, 0);
		if (c == -1) {
			c = ' ';
		}
		for (; padlen > 0; padlen--) {
			putchar(c);
		}

		i = 0;
		while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
			putchar(c);
			i++;
		}
	}
	i = 0;
	while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
		putchar(c);
		i++;
	}
	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = colinfo->column_namelen;
		len = collen > namlen ? collen : namlen;
		for (i = 0; i < len; i++)
			putchar('-');
		i = 0;
		while ((c = dbstring_getchar(dbproc->dbopts[DBPRCOLSEP].optparam, i)) != -1) {
			putchar(c);
			i++;
		}
	}
	i = 0;
	while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
		putchar(c);
		i++;
	}
}

/** \internal
 * \ingroup dblib_internal
 * \brief Indicate whether a query returned rows.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa DBROWS(), DBCMDROW(), dbnextrow(), dbresults(), DBROWTYPE().
 */
RETCODE
dbrows(DBPROCESS * dbproc)
{
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;

	if (resinfo && resinfo->rows_exist)
		return SUCCEED;
	else
		return FAIL;
}

/**
 * \ingroup dblib_api
 * \brief Set the default character set for an application.
 * 
 * \param language ASCII null-terminated string.  
 * \sa dbsetdeflang(), dbsetdefcharset(), dblogin(), dbopen().
 * \retval SUCCEED Always.  
 * \todo Unimplemented.
 */
RETCODE
dbsetdeflang(char *language)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetdeflang()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get TDS packet size for the connection.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return TDS packet size, in bytes.  
 * \sa DBSETLPACKET()
 */
int
dbgetpacket(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	if (!tds) {
		return TDS_DEF_BLKSZ;
	} else {
		return tds->env.block_size;
	}
}

/**
 * \ingroup dblib_api
 * \brief Set maximum simultaneous connections db-lib will open to the server.
 * 
 * \param maxprocs Limit for process.
 * \retval SUCCEED Always.  
 * \sa dbgetmaxprocs(), dbopen()
 */
RETCODE
dbsetmaxprocs(int maxprocs)
{
	int i;
	TDSSOCKET **old_list = g_dblib_ctx.connection_list;

	tdsdump_log(TDS_DBG_FUNC, "UNTESTED dbsetmaxprocs()\n");
	/*
	 * Don't reallocate less memory.  
	 * If maxprocs is less than was initially allocated, just reduce the represented list size.  
	 * If larger, reallocate and copy.
	 * We probably should check for valid connections beyond the new max.
	 */
	if (maxprocs < g_dblib_ctx.connection_list_size) {
		g_dblib_ctx.connection_list_size_represented = maxprocs;
		return SUCCEED;
	}

	g_dblib_ctx.connection_list = (TDSSOCKET **) calloc(maxprocs, sizeof(TDSSOCKET *));

	if (g_dblib_ctx.connection_list == NULL) {
		g_dblib_ctx.connection_list = old_list;
		return FAIL;
	}

	for (i = 0; i < g_dblib_ctx.connection_list_size; i++) {
		g_dblib_ctx.connection_list[i] = old_list[i];
	}

	g_dblib_ctx.connection_list_size = maxprocs;
	g_dblib_ctx.connection_list_size_represented = maxprocs;

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief get maximum simultaneous connections db-lib will open to the server.
 * 
 * \return Current maximum.  
 * \sa dbsetmaxprocs(), dbopen()
 */
int
dbgetmaxprocs(void)
{
	return g_dblib_ctx.connection_list_size_represented;
}

/**
 * \ingroup dblib_api
 * \brief Set maximum seconds db-lib waits for a server response to query.  
 * 
 * \param seconds New limit for application.  
 * \retval SUCCEED Always.  
 * \sa dberrhandle(), DBGETTIME(), dbsetlogintime(), dbsqlexec(), dbsqlok(), dbsqlsend().
 * \todo Unimplemented.
 */
RETCODE
dbsettime(int seconds)
{
	g_dblib_query_timeout = seconds;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Set maximum seconds db-lib waits for a server response to a login attempt.  
 * 
 * \param seconds New limit for application.  
 * \retval SUCCEED Always.  
 * \sa dberrhandle(), dbsettime()
 */
RETCODE
dbsetlogintime(int seconds)
{
	g_dblib_login_timeout = seconds;
	return SUCCEED;
}

/** \internal
 * \ingroup dblib_internal
 * \brief See if the current command can return rows.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval SUCCEED Yes, it can.  
 * \retval FAIL No, it can't.
 * \remarks Use   DBCMDROW() macro instead.  
 * \sa DBCMDROW(), dbnextrow(), dbresults(), DBROWS(), DBROWTYPE().
 */
RETCODE
dbcmdrow(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	if (tds->res_info)
		return SUCCEED;
	return TDS_FAIL;
}

/**
 * \ingroup dblib_api
 * \brief Get column ID of a compute column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \return Nth column in the base result set, on which \a column was computed.  
 * \sa dbadata(), dbadlen(), dbaltlen(), dbgetrow(), dbnextrow(), dbnumalts(), dbprtype(). 
 */
int
dbaltcolid(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *curcol;
	TDS_SMALLINT compute_id;
	int i;

	compute_id = computeid;
	tdsdump_log(TDS_DBG_FUNC, "in dbaltcolid(%d,%d)\n", compute_id, column);

	tdsdump_log(TDS_DBG_FUNC, "in dbaltcolid() num_comp_info = %d\n", tds->num_comp_info);
	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		tdsdump_log(TDS_DBG_FUNC, "in dbaltcolid() found computeid = %d\n", info->computeid);
		if (info->computeid == compute_id)
			break;
	}
	tdsdump_log(TDS_DBG_FUNC, "in dbaltcolid() num_cols = %d\n", info->num_cols);

	if (column < 1 || column > info->num_cols)
		return -1;

	curcol = info->columns[column - 1];

	return curcol->column_operand;

}

/**
 * \ingroup dblib_api
 * \brief Get size of data in a compute column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \return size of the data, in bytes.
 * \retval -1 no such \a column or \a computeid. 
 * \retval 0 data are NULL.
 * \sa dbadata(), dbaltlen(), dbalttype(), dbgetrow(), dbnextrow(), dbnumalts().
 */
DBINT
dbadlen(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *colinfo;
	TDS_SMALLINT compute_id;
	int i;
	DBINT ret;

	tdsdump_log(TDS_DBG_FUNC, "in dbadlen()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return -1;

	colinfo = info->columns[column - 1];
	tdsdump_log(TDS_DBG_INFO1, "dbadlen() type = %d\n", colinfo->column_type);

	if (colinfo->column_cur_size < 0)
		ret = 0;
	else
		ret = colinfo->column_cur_size;
	tdsdump_log(TDS_DBG_FUNC, "leaving dbadlen() returning %d\n", ret);

	return ret;

}

/**
 * \ingroup dblib_api
 * \brief Get datatype for a compute column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \return \c SYB* dataype token.
 * \retval -1 no such \a column or \a computeid. 
 * \sa dbadata(), dbadlen(), dbaltlen(), dbnextrow(), dbnumalts(), dbprtype().
 */
int
dbalttype(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *colinfo;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "in dbalttype()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return -1;

	colinfo = info->columns[column - 1];

	switch (colinfo->column_type) {
	case SYBVARCHAR:
		return SYBCHAR;
	case SYBVARBINARY:
		return SYBBINARY;
	case SYBDATETIMN:
		if (colinfo->column_size == 8)
			return SYBDATETIME;
		else if (colinfo->column_size == 4)
			return SYBDATETIME4;
	case SYBMONEYN:
		if (colinfo->column_size == 4)
			return SYBMONEY4;
		else
			return SYBMONEY;
	case SYBFLTN:
		if (colinfo->column_size == 8)
			return SYBFLT8;
		else if (colinfo->column_size == 4)
			return SYBREAL;
	case SYBINTN:
		if (colinfo->column_size == 8)
			return SYBINT8;
		else if (colinfo->column_size == 4)
			return SYBINT4;
		else if (colinfo->column_size == 2)
			return SYBINT2;
		else if (colinfo->column_size == 1)
			return SYBINT1;
	default:
		return colinfo->column_type;
	}
	return -1;		/* something went wrong */
}

/**
 * \ingroup dblib_api
 * \brief Bind a compute column to a program variable.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \param vartype datatype of the host variable that will receive the data
 * \param varlen size of host variable pointed to \a varaddr
 * \param varaddr address of host variable
 * \retval SUCCEED everything worked.
 * \retval FAIL no such \a computeid or \a column, or no such conversion possible, or target buffer too small.
 * \sa dbadata(), dbaltbind_ps(), dbanullbind(), dbbind(), dbbind_ps(), dbconvert(),
 * 	dbconvert_ps(), dbnullbind(), dbsetnull(), dbsetversion(), dbwillconvert().
 */
RETCODE
dbaltbind(DBPROCESS * dbproc, int computeid, int column, int vartype, DBINT varlen, BYTE * varaddr)
{
	TDSSOCKET *tds = NULL;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *colinfo = NULL;

	TDS_SMALLINT compute_id;

	int srctype = -1;
	int desttype = -1;
	int i;

	tdsdump_log(TDS_DBG_INFO1, "dbaltbind() compteid %d column = %d %d %d\n", computeid, column, vartype, varlen);

	dbproc->avail_flag = FALSE;

	compute_id = computeid;

	if (dbproc == NULL || dbproc->tds_socket == NULL || varaddr == NULL)
		goto Failed;

	tds = dbproc->tds_socket;
	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			goto Failed;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		goto Failed;

	colinfo = info->columns[column - 1];
	srctype = tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
	desttype = _db_get_server_type(vartype);

	tdsdump_log(TDS_DBG_INFO1, "dbaltbind() srctype = %d desttype = %d \n", srctype, desttype);

	if (!dbwillconvert(srctype, _db_get_server_type(vartype)))
		goto Failed;

	colinfo->column_varaddr = (char *) varaddr;
	colinfo->column_bindtype = vartype;
	colinfo->column_bindlen = varlen;

	return SUCCEED;
      Failed:
	return FAIL;
}				/* dbaltbind()  */


/**
 * \ingroup dblib_api
 * \brief Get address of compute column data.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \return pointer to columns's data buffer.  
 * \retval NULL no such \a computeid or \a column.  
 * \sa dbadlen(), dbaltbind(), dbaltlen(), dbalttype(), dbgetrow(), dbnextrow(), dbnumalts().
 */
BYTE *
dbadata(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *colinfo;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "in dbadata()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return NULL;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return NULL;

	colinfo = info->columns[column - 1];

	if (is_blob_type(colinfo->column_type)) {
		return (BYTE *) ((TDSBLOB *) (info->current_row + colinfo->column_offset))->textvalue;
	}

	return (BYTE *) & info->current_row[colinfo->column_offset];
}

/**
 * \ingroup dblib_api
 * \brief Get aggregation operator for a compute column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \return token value for the type of the compute column's aggregation operator.
 * \retval -1 no such \a computeid or \a column.  
 * \sa dbadata(), dbadlen(), dbaltlen(), dbnextrow(), dbnumalts(), dbprtype().
 */
int
dbaltop(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *curcol;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "in dbaltop()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return -1;

	curcol = info->columns[column - 1];

	return curcol->column_operator;
}

/**
 * \ingroup dblib_api
 * \brief Set db-lib or server option.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param option option to set.  
 * \param char_param value to set \a option to, if it wants a null-teminated ASCII string.  
 * \param int_param  value to set \a option to, if it wants an integer value.  
 * \retval SUCCEED everything worked.
 * \retval FAIL no such \a option, or insufficient memory, or unimplemented.  
 * \remarks Many are unimplemented.
 * \sa dbclropt(), dbisopt().
 * \todo Implement more options.  
 */
RETCODE
dbsetopt(DBPROCESS * dbproc, int option, const char *char_param, int int_param)
{
	char *cmd;
	RETCODE rc;

	if ((option < 0) || (option >= DBNUMOPTIONS)) {
		_dblib_client_msg(dbproc, SYBEUNOP, EXNONFATAL, "Unknown option passed to dbsetopt().");
		return FAIL;
	}
	dbproc->dbopts[option].optactive = 1;
	switch (option) {
	case DBARITHABORT:
	case DBARITHIGNORE:
	case DBCHAINXACTS:
	case DBFIPSFLAG:
	case DBISOLATION:
	case DBNOCOUNT:
	case DBNOEXEC:
	case DBPARSEONLY:
	case DBSHOWPLAN:
	case DBSTORPROCID:
	case DBQUOTEDIDENT:
		/* server options (on/off) */
		if (asprintf(&cmd, "set %s on\n", dbproc->dbopts[option].opttext) < 0) {
			return FAIL;
		}
		rc = dbstring_concat(&(dbproc->dboptcmd), cmd);
		free(cmd);
		return rc;
		break;
	case DBNATLANG:
	case DBDATEFIRST:
	case DBDATEFORMAT:
		/* server options (char_param) */
		if (asprintf(&cmd, "set %s %s\n", dbproc->dbopts[option].opttext, char_param) < 0) {
			return FAIL;
		}
		rc = dbstring_concat(&(dbproc->dboptcmd), cmd);
		free(cmd);
		return rc;
		break;
	case DBOFFSET:
		/* server option */
		/* requires param
		 * "select", "from", "table", "order", "compute",
		 * "statement", "procedure", "execute", or "param"
		 */
		break;
	case DBROWCOUNT:
		/* server option */
		/* requires param "0" to "2147483647" */
		break;
	case DBSTAT:
		/* server option */
		/* requires param "io" or "time" */
		break;
	case DBTEXTLIMIT:
		/* dblib option */
		/* requires param "0" to "2147483647" */
		/* dblib do not return more than this length from text/image */
		/* TODO required for PHP */
		break;
	case DBTEXTSIZE:
		/* server option */
		/* requires param "0" to "2147483647" */
		/* limit text/image from network */
		break;
	case DBAUTH:
		/* ??? */
		break;
	case DBNOAUTOFREE:
		/* dblib option */
		break;
	case DBBUFFER:
		/* 
		 * Requires param "2" to "2147483647" 
		 * (0 or 1 is an error, < 0 yields the default 100) 
		 */
		{ 
			/* 100 is the default, according to Microsoft */
			if( !char_param )
				char_param = "100";

			int nrows = atoi(char_param);

			nrows = (nrows < 0 )? 100 : nrows;

			if( 1 < nrows && nrows <= 2147483647 ) {
				buffer_set_buffering(&(dbproc->row_buf), nrows);
				return SUCCEED;
			}
		}
		break;
	case DBPRCOLSEP:
	case DBPRLINELEN:
	case DBPRLINESEP:
	case DBPRPAD:
		/* dblib options */
		rc = dbstring_assign(&(dbproc->dbopts[option].optparam), char_param);
		/* XXX DBPADON/DBPADOFF */
		return rc;
		break;
	default:
		break;
	}
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetopt(option = %d)\n", option);
	return FAIL;
}

/**
 * \ingroup dblib_api
 * \brief Set interrupt handler for db-lib to use while blocked against a read from the server.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param chkintr
 * \param hndlintr
 * \sa dbcancel(), dbgetuserdata(), dbsetuserdata(), dbsetbusy(), dbsetidle().
 */
void
dbsetinterrupt(DBPROCESS * dbproc, DB_DBCHKINTR_FUNC chkintr, DB_DBHNDLINTR_FUNC hndlintr)
{
	dbproc->dbchkintr = chkintr;
	dbproc->dbhndlintr = hndlintr;
}

/**
 * \ingroup dblib_api
 * \brief Determine if query generated a return status number.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval TRUE fetch return status with dbretstatus().  
 * \retval FALSE no return status.  
 * \sa dbnextrow(), dbresults(), dbretdata(), dbretstatus(), dbrpcinit(), dbrpcparam(), dbrpcsend().
 */
DBBOOL
dbhasretstat(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	dbnumrets(dbproc);

	if (tds->has_status) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/**
 * \ingroup dblib_api
 * \brief Fetch status value returned by query or remote procedure call.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return return value 
 * \sa dbhasretstat(), dbnextrow(), dbresults(), dbretdata(), dbrpcinit(), dbrpcparam(), dbrpcsend().
 */
DBINT
dbretstatus(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	dbnumrets(dbproc);

	return tds->ret_status;
}

/**
 * \ingroup dblib_api
 * \brief Get count of output parameters filled by a stored procedure.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return How many, possibly zero.  
 * \remarks This name sounds funny.  
 * \sa 
 */
int
dbnumrets(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;
	TDS_INT result_type;

	tds = dbproc->tds_socket;

	tdsdump_log(TDS_DBG_FUNC, "dbnumrets() finds %d columns\n", (tds->param_info? tds->param_info->num_cols : 0));

	/* try to fetch output parameters and return status, if we have not already done so */
	if (!tds->param_info) 
		tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_TRAILING);
		
	if (!tds->param_info)
		return 0;

	return tds->param_info->num_cols;
}

/**
 * \ingroup dblib_api
 * \brief Get name of an output parameter filled by a stored procedure.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param retnum Nth parameter between \c 1 and the return value from \c dbnumrets().
 * \returns ASCII null-terminated string, \c NULL if no such \a retnum.  
 * \sa dbnextrow(), dbnumrets(), dbresults(), dbretdata(), dbretlen(), dbrettype(), dbrpcinit(), dbrpcparam().
 */
char *
dbretname(DBPROCESS * dbproc, int retnum)
{
	TDSPARAMINFO *param_info;

	if (!dbproc || !dbproc->tds_socket)
		return NULL;

	dbnumrets(dbproc);

	param_info = dbproc->tds_socket->param_info;
	if (!param_info || !param_info->columns || retnum < 1 || retnum > param_info->num_cols)
		return NULL;
	assert(param_info->columns[retnum - 1]->column_name[param_info->columns[retnum - 1]->column_namelen] == 0);
	return param_info->columns[retnum - 1]->column_name;
}

/**
 * \ingroup dblib_api
 * \brief Get value of an output parameter filled by a stored procedure.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param retnum Nth parameter between \c 1 and the return value from \c dbnumrets().
 * \returns Address of a return parameter value, or \c NULL if no such \a retnum.  
 * \sa dbnextrow(), dbnumrets(), dbresults(), dbretlen(), dbretname(), dbrettype(), dbrpcinit(), dbrpcparam().
 * \todo Handle blobs.
 */
BYTE *
dbretdata(DBPROCESS * dbproc, int retnum)
{
	TDSCOLUMN *colinfo;
	TDSPARAMINFO *param_info;
	TDSSOCKET *tds;

	dbnumrets(dbproc);

	tds = dbproc->tds_socket;
	param_info = tds->param_info;
	if (!param_info || !param_info->columns || retnum < 1 || retnum > param_info->num_cols)
		return NULL;

	colinfo = param_info->columns[retnum - 1];
	/* FIXME blob are stored is different way */
	return &param_info->current_row[colinfo->column_offset];
}

/**
 * \ingroup dblib_api
 * \brief Get size of an output parameter filled by a stored procedure.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param retnum Nth parameter between \c 1 and the return value from \c dbnumrets().
 * \returns Size of a return parameter value, or \c NULL if no such \a retnum.  
 * \sa dbnextrow(), dbnumrets(), dbresults(), dbretdata(), dbretname(), dbrettype(), dbrpcinit(), dbrpcparam().
 */
int
dbretlen(DBPROCESS * dbproc, int retnum)
{
	TDSCOLUMN *colinfo;
	TDSPARAMINFO *param_info;
	TDSSOCKET *tds;

	dbnumrets(dbproc);

	tds = dbproc->tds_socket;
	param_info = tds->param_info;
	if (!param_info || !param_info->columns || retnum < 1 || retnum > param_info->num_cols)
		return -1;

	colinfo = param_info->columns[retnum - 1];
	if (colinfo->column_cur_size < 0)
		return 0;

	return colinfo->column_cur_size;
}

/**
 * \ingroup dblib_api
 * \brief Wait for results of a query from the server.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval SUCCEED everything worked, fetch results with \c dbnextresults().
 * \retval FAIL SQL syntax error, typically.  
 * \sa dbcmd(), dbfcmd(), DBIORDESC(), DBIOWDESC(), dbmoretext(), dbnextrow(),
	dbpoll(), DBRBUF(), dbresults(), dbretstatus(), dbrpcsend(), dbsettime(), dbsqlexec(),
	dbsqlsend(), dbwritetext().
 */
RETCODE
dbsqlok(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;

	unsigned char marker;
	int done = 0, done_flags;
	TDS_INT result_type;

	tdsdump_log(TDS_DBG_FUNC, "in dbsqlok() \n");
	tds = dbproc->tds_socket;

	/* dbsqlok has been called after dbmoretext() */
	/* This is the trigger to send the text data. */

	if (dbproc->text_sent) {
		tds_flush_packet(tds);
		dbproc->text_sent = 0;

	}

	/* See what the next packet from the server is. */

	/* 1. we want to skip any messages which are not processable */
	/* we're looking for a result token or a done token.         */

	while (!done) {
		marker = tds_peek(tds);
		tdsdump_log(TDS_DBG_FUNC, "dbsqlok() marker is %x\n", marker);

		/* If we hit a result token, then we know  */
		/* everything is fine with the command...  */

		if (is_result_token(marker)) {
			tdsdump_log(TDS_DBG_FUNC, "dbsqlok() found result token\n");
			return SUCCEED;
		}

		/* if we hit an end token, for example if the command */
		/* submitted returned no data (like an insert), then  */
		/* we have to process the end token to extract the    */
		/* status code therein....but....                     */

		switch (tds_process_tokens(tds, &result_type, &done_flags, TDS_TOKEN_RESULTS)) {
		case TDS_NO_MORE_RESULTS:
			return SUCCEED;
			break;

		case TDS_FAIL:
			return FAIL;
			break;

		case TDS_SUCCEED:
			switch (result_type) {
			case TDS_ROWFMT_RESULT:
			case TDS_COMPUTE_RESULT:
			case TDS_ROW_RESULT:
			case TDS_COMPUTEFMT_RESULT:
				tdsdump_log(TDS_DBG_FUNC, "dbsqlok() found result token\n");
				return SUCCEED;
				break;
			case TDS_DONE_RESULT:
			case TDS_DONEPROC_RESULT:
				if (done_flags & TDS_DONE_ERROR) {
					tdsdump_log(TDS_DBG_FUNC, "dbsqlok() end status was error\n");

					if (done_flags & TDS_DONE_MORE_RESULTS) {
						dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
					} else {
						dbproc->dbresults_state = _DB_RES_NO_MORE_RESULTS;
					}

					return FAIL;
				} else {
					tdsdump_log(TDS_DBG_FUNC, "dbsqlok() end status was success\n");

					dbproc->dbresults_state = _DB_RES_SUCCEED;
					return SUCCEED;
				}
				break;
			default:
				break;
			}
			break;
		}
	}

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get count of columns in a compute row.
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \return number of columns, else -1 if no such \a computeid.  
 * \sa dbadata(), dbadlen(), dbaltlen(), dbalttype(), dbgetrow(), dbnextrow(), dbnumcols(). 
 */
int
dbnumalts(DBPROCESS * dbproc, int computeid)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDS_SMALLINT compute_id;
	int i;

	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	return info->num_cols;
}

/**
 * \ingroup dblib_api
 * \brief Get count of \c COMPUTE clauses for a result set.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return number of compute clauses for the current query, possibly zero.  
 * \sa dbnumalts(), dbresults().
 */
int
dbnumcompute(DBPROCESS * dbproc)
{
	TDSSOCKET *tds = dbproc->tds_socket;

	return tds->num_comp_info;
}


/**
 * \ingroup dblib_api
 * \brief Get \c bylist for a compute row.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param size \em output: size of \c bylist buffer whose address is returned, possibly zero.  
 * \return address of \c bylist for \a computeid.  
 * \retval NULL no such \a computeid.
 * \remarks Do not free returned pointer.  
 * \sa dbadata(), dbadlen(), dbaltlen(), dbalttype(), dbcolname(), dbgetrow(), dbnextrow().
 */
BYTE *
dbbylist(DBPROCESS * dbproc, int computeid, int *size)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	int i;
	const TDS_SMALLINT byte_flag = 0x8000;

	tdsdump_log(TDS_DBG_FUNC, "in dbbylist() \n");

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info) {
			if (size)
				*size = 0;
			return NULL;
		}
		info = tds->comp_info[i];
		if (info->computeid == computeid)
			break;
	}

	if (size)
		*size = info->by_cols;

	/*
	 * libTDS store this information using TDS_SMALLINT so we 
	 * have to convert it. We can do this cause libTDS just
	 * store these informations
	 */
	if (info->by_cols > 1 && info->bycolumns[0] != byte_flag) {
		unsigned int n;
		TDS_TINYINT *p = (TDS_TINYINT *) malloc(sizeof(info->bycolumns[0]) + info->by_cols);
		if (!p)
			return NULL;
		for (n = 0; n < info->by_cols; ++n)
			p[sizeof(info->bycolumns[0]) + n] = info->bycolumns[n] > 255 ? 255 : info->bycolumns[n];
		*((TDS_SMALLINT *)p) = byte_flag;
		free(info->bycolumns);
		info->bycolumns = (TDS_SMALLINT *) p;
	}
	return (BYTE *) (&info->bycolumns[1]);
}

/** \internal
 * \ingroup dblib_internal
 * \brief Check if \a dbproc is an ex-parrot.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval TRUE process has been marked \em dead.
 * \retval FALSE process is OK.  
 * \remarks dbdead() does not communicate with the server.  
 * 	Unless a previously db-lib marked \a dbproc \em dead, dbdead() returns \c FALSE.  
 * \sa dberrhandle().
 */
DBBOOL
dbdead(DBPROCESS * dbproc)
{
	if ((dbproc == NULL) || IS_TDSDEAD(dbproc->tds_socket))
		return TRUE;
	else
		return FALSE;
}

/**
 * \ingroup dblib_api
 * \brief Set an error handler, for messages from db-lib.
 * 
 * \param handler pointer to callback function that will handle errors.
 * \sa DBDEAD(), dbmsghandle().
 */
EHANDLEFUNC
dberrhandle(EHANDLEFUNC handler)
{
	EHANDLEFUNC retFun = _dblib_err_handler;

	_dblib_err_handler = handler;
	return retFun;
}

/**
 * \ingroup dblib_api
 * \brief Set a message handler, for messages from the server.
 * 
 * \param handler address of the function that will process the messages.
 * \sa DBDEAD(), dberrhandle().
 */
MHANDLEFUNC
dbmsghandle(MHANDLEFUNC handler)
{
	MHANDLEFUNC retFun = _dblib_msg_handler;

	_dblib_msg_handler = handler;
	return retFun;
}

/**
 * \ingroup dblib_api
 * \brief Add two DBMONEY values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 first operand.
 * \param m2 other operand. 
 * \param sum \em output: result of computation.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnyadd(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * sum)
{
	if (!m1 || !m2 || !sum)
		return FAIL;
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnyadd()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Subtract two DBMONEY values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 first operand.
 * \param m2 other operand, subtracted from \a m1. 
 * \param difference \em output: result of computation.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnysub(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * difference)
{
	if (!m1 || !m2 || !difference)
		return FAIL;
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnysyb()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Multiply two DBMONEY values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 first operand.
 * \param m2 other operand. 
 * \param prod \em output: result of computation.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnymul(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * prod)
{
	if (!m1 || !m2 || !prod)
		return FAIL;
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnymul()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Divide two DBMONEY values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 dividend.
 * \param m2 divisor. 
 * \param quotient \em output: result of computation.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnydivide(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * quotient)
{
	if (!m1 || !m2 || !quotient)
		return FAIL;
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnydivide()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Compare two DBMONEY values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 some money.
 * \param m2 some other money. 
 * \retval 0 m1 == m2. 
 * \retval -1 m1 < m2.
 * \retval  1 m1 > m2.
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
int
dbmnycmp(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2)
{

	if (m1->mnyhigh < m2->mnyhigh) {
		return -1;
	}
	if (m1->mnyhigh > m2->mnyhigh) {
		return 1;
	}
	if (m1->mnylow < m2->mnylow) {
		return -1;
	}
	if (m1->mnylow > m2->mnylow) {
		return 1;
	}
	return 0;
}

/**
 * \ingroup dblib_api
 * \brief Multiply a DBMONEY value by a positive integer, and add an amount. 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount starting amount of money, also holds output.
 * \param multiplier amount to multiply \a amount by. 
 * \param addend amount to add to \a amount, after multiplying by \a multiplier. 
 * \retval SUCCEED Always.  
 * \remarks This function is goofy.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnyscale(DBPROCESS * dbproc, DBMONEY * amount, int multiplier, int addend)
{
	if (!amount)
		return FAIL;
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnyscale()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Set a DBMONEY value to zero.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param dest address of a DBMONEY structure.  
 * \retval SUCCEED unless \a amount is NULL.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmnyzero(DBPROCESS * dbproc, DBMONEY * dest)
{

	if (dest == NULL) {
		return FAIL;
	}
	dest->mnylow = 0;
	dest->mnyhigh = 0;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get maximum positive DBMONEY value supported.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount address of a DBMONEY structure.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmnymaxpos(DBPROCESS * dbproc, DBMONEY * amount)
{
	if (!amount)
		return FAIL;
	amount->mnylow = 0xFFFFFFFFlu;
	amount->mnyhigh = 0x7FFFFFFFl;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get maximum negative DBMONEY value supported.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount address of a DBMONEY structure.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmnymaxneg(DBPROCESS * dbproc, DBMONEY * amount)
{
	if (!amount)
		return FAIL;
	amount->mnylow = 0;
	amount->mnyhigh = -0x80000000l;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get the least significant digit of a DBMONEY value, represented as a character.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param mnyptr \em input the money amount, \em and \em output: \a mnyptr divided by 10.  
 * \param digit the character value (between '0' and '9') of the rightmost digit in \a mnyptr.  
 * \param zero \em output: \c TRUE if \a mnyptr is zero on output, else \c FALSE.  
 * \retval SUCCEED Always.  
 * \sa dbconvert(), dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \remarks Unimplemented and likely to remain so.  We'd be amused to learn anyone wants this function.  
 * \todo Unimplemented.
 */
RETCODE
dbmnyndigit(DBPROCESS * dbproc, DBMONEY * mnyptr, DBCHAR * digit, DBBOOL * zero)
{
	if (!mnyptr)
		return FAIL;
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnyndigit()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Prepare a DBMONEY value for use with dbmnyndigit().
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount address of a DBMONEY structure.  
 * \param trim number of digits to trim from \a amount.
 * \param negative \em output: \c TRUE if \a amount < 0.  
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnyinit(DBPROCESS * dbproc, DBMONEY * amount, int trim, DBBOOL * negative)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnyinit()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Divide a DBMONEY value by a positive integer.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount address of a DBMONEY structure.  
 * \param divisor of \a amount.
 * \param remainder \em output: modulo of integer division.
 * \retval SUCCEED Always.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmnydown(DBPROCESS * dbproc, DBMONEY * amount, int divisor, int *remainder)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmnydown()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Add $0.0001 to a DBMONEY value.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount address of a DBMONEY structure.  
 * \retval SUCCEED or FAIL if overflow or amount NULL.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmnyinc(DBPROCESS * dbproc, DBMONEY * amount)
{
	if (!amount)
		return FAIL;
	if (amount->mnylow != 0xFFFFFFFFlu) {
		++amount->mnylow;
		return SUCCEED;
	}
	if (amount->mnyhigh == 0x7FFFFFFFl)
		return FAIL;
	amount->mnylow = 0;
	++amount->mnyhigh;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Subtract $0.0001 from a DBMONEY value.
 *
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param amount address of a DBMONEY structure.  
 * \retval SUCCEED or FAIL if overflow or amount NULL.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmnydec(DBPROCESS * dbproc, DBMONEY * amount)
{
	if (!amount)
		return FAIL;
	if (amount->mnylow != 0) {
		--amount->mnylow;
		return SUCCEED;
	}
	if (amount->mnyhigh == -0x80000000l)
		return FAIL;
	amount->mnylow = 0xFFFFFFFFlu;
	--amount->mnyhigh;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Negate a DBMONEY value.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param src address of a DBMONEY structure.  
 * \param dest \em output: result of negation. 
 * \retval SUCCEED or FAIL if overflow or src/dest NULL.
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmnyminus(DBPROCESS * dbproc, DBMONEY * src, DBMONEY * dest)
{
	if (!src || !dest)
		return FAIL;
	if (src->mnyhigh == -0x80000000l && src->mnylow == 0)
		return FAIL;
	dest->mnyhigh = -src->mnyhigh;
	dest->mnylow = (~src->mnylow) + 1u;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Negate a DBMONEY4 value.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param src address of a DBMONEY4 structure.  
 * \param dest \em output: result of negation. 
 * \retval SUCCEED usually.  
 * \retval FAIL  on overflow.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmny4minus(DBPROCESS * dbproc, DBMONEY4 * src, DBMONEY4 * dest)
{
	DBMONEY4 zero;

	dbmny4zero(dbproc, &zero);
	return (dbmny4sub(dbproc, &zero, src, dest));
}

/**
 * \ingroup dblib_api
 * \brief Zero a DBMONEY4 value.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param dest address of a DBMONEY structure.  
 * \retval SUCCEED usually.  
 * \retval FAIL  \a dest is NULL.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmny4zero(DBPROCESS * dbproc, DBMONEY4 * dest)
{

	if (dest == NULL) {
		return FAIL;
	}
	dest->mny4 = 0;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Add two DBMONEY4 values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 first operand.
 * \param m2 other operand. 
 * \param sum \em output: result of computation.  
 * \retval SUCCEED usually.  
 * \retval FAIL  on overflow.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmny4add(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2, DBMONEY4 * sum)
{

	if ((m1 == NULL) || (m2 == NULL) || (sum == NULL)) {
		return FAIL;
	}
	sum->mny4 = m1->mny4 + m2->mny4;
	if (((m1->mny4 < 0) && (m2->mny4 < 0) && (sum->mny4 >= 0))
	    || ((m1->mny4 > 0) && (m2->mny4 > 0) && (sum->mny4 <= 0))) {
		/* overflow */
		sum->mny4 = 0;
		return FAIL;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Subtract two DBMONEY4 values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 first operand.
 * \param m2 other operand, subtracted from \a m1. 
 * \param diff \em output: result of computation.  
 * \retval SUCCEED usually.  
 * \retval FAIL  on overflow.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
RETCODE
dbmny4sub(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2, DBMONEY4 * diff)
{

	if ((m1 == NULL) || (m2 == NULL) || (diff == NULL)) {
		return FAIL;
	}
	diff->mny4 = m1->mny4 - m2->mny4;
	if (((m1->mny4 <= 0) && (m2->mny4 > 0) && (diff->mny4 > 0))
	    || ((m1->mny4 >= 0) && (m2->mny4 < 0) && (diff->mny4 < 0))) {
		/* overflow */
		diff->mny4 = 0;
		return FAIL;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Multiply two DBMONEY4 values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 first operand.
 * \param m2 other operand. 
 * \param prod \em output: result of computation.  
 * \retval SUCCEED usually.  
 * \retval FAIL a parameter is NULL.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmny4mul(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2, DBMONEY4 * prod)
{

	if ((m1 == NULL) || (m2 == NULL) || (prod == NULL)) {
		return FAIL;
	}
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmny4mul()\n");
	return FAIL;
}

/**
 * \ingroup dblib_api
 * \brief Divide two DBMONEY4 values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 dividend.
 * \param m2 divisor. 
 * \param quotient \em output: result of computation.  
 * \retval SUCCEED usually.  
 * \retval FAIL a parameter is NULL.  
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 * \todo Unimplemented.
 */
RETCODE
dbmny4divide(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2, DBMONEY4 * quotient)
{

	if ((m1 == NULL) || (m2 == NULL) || (quotient == NULL)) {
		return FAIL;
	}
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbmny4divide()\n");
	return FAIL;
}

/**
 * \ingroup dblib_api
 * \brief Compare two DBMONEY4 values.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param m1 some money.
 * \param m2 some other money. 
 * \retval 0 m1 == m2. 
 * \retval -1 m1 < m2.
 * \retval  1 m1 > m2.
 * \sa dbmnyadd(), dbmnysub(), dbmnymul(), dbmnydivide(), dbmnyminus(), dbmny4add(), dbmny4sub(), dbmny4mul(), dbmny4divide(), dbmny4minus().
 */
int
dbmny4cmp(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2)
{

	if (m1->mny4 < m2->mny4) {
		return -1;
	}
	if (m1->mny4 > m2->mny4) {
		return 1;
	}
	return 0;
}

/**
 * \ingroup dblib_api
 * \brief Copy a DBMONEY4 value.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param src address of a DBMONEY4 structure.  
 * \param dest \em output: new money. 
 * \retval SUCCEED or FAIL if src/dest NULL.
 * \sa dbmnycopy(), dbmnyminus(), dbmny4minus(). 
 */
RETCODE
dbmny4copy(DBPROCESS * dbproc, DBMONEY4 * src, DBMONEY4 * dest)
{

	if ((src == NULL) || (dest == NULL)) {
		return FAIL;
	}
	dest->mny4 = src->mny4;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Compare DBDATETIME values, similar to strcmp(3).
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param d1 a \c DBDATETIME structure address
 * \param d2 another \c DBDATETIME structure address
 * \retval   0 d1 = d2.
 * \retval  -1 d1 < d2.
 * \retval   1 d1 > d2.
 * \sa dbdate4cmp(), dbmnycmp(), dbmny4cmp().
 */
RETCODE
dbdatecmp(DBPROCESS * dbproc, DBDATETIME * d1, DBDATETIME * d2)
{
	if (d1->dtdays == d2->dtdays) {
		if (d1->dttime == d2->dttime)
			return 0;
		else
			return d1->dttime > d2->dttime ? 1 : -1;
	}

	/* date 1 is before 1900 */
	if (d1->dtdays > 2958463) {

		if (d2->dtdays > 2958463)	/* date 2 is before 1900 */
			return d1->dtdays > d2->dtdays ? 1 : -1;
		else
			return -1;
	} else {
		/* date 1 is after 1900 */
		if (d2->dtdays < 2958463)	/* date 2 is after 1900 */
			return d1->dtdays > d2->dtdays ? 1 : -1;
		else
			return 1;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Break a DBDATETIME value into useful pieces.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param di \em output: structure to contain the exploded parts of \a datetime.
 * \param datetime \em input: \c DBDATETIME to be converted.
 * \retval SUCCEED always.
 * \remarks The members of \a di have different names, depending on whether \c --with-msdblib was configured. 
 * \sa dbconvert(), dbdata(), dbdatechar(), dbdatename(), dbdatepart().
 */
RETCODE
dbdatecrack(DBPROCESS * dbproc, DBDATEREC * di, DBDATETIME * datetime)
{
	TDSDATEREC dr;

	tds_datecrack(SYBDATETIME, datetime, &dr);

	di->dateyear = dr.year;
	di->datemonth = dr.month;
	di->datedmonth = dr.day;
	di->datedyear = dr.dayofyear;
	di->datedweek = dr.weekday;
	di->datehour = dr.hour;
	di->dateminute = dr.minute;
	di->datesecond = dr.second;
	di->datemsecond = dr.millisecond;
	if (dbproc->msdblib) {
		++di->datemonth;
		++di->datedweek;
	}
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Clear remote passwords from the LOGINREC structure.
 * 
 * \param login structure to pass to dbopen().
 * \sa dblogin(), dbopen(), dbrpwset(), DBSETLAPP(), DBSETLHOST(), DBSETLPWD(), DBSETLUSER().  
 * \remarks Useful for remote stored procedure calls, but not in high demand from FreeTDS.  
 * \todo Unimplemented.
 */
void
dbrpwclr(LOGINREC * login)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbrpwclr()\n");
}

/**
 * \ingroup dblib_api
 * \brief Add a remote password to the LOGINREC structure.
 * 
 * \param login structure to pass to dbopen().
 * \param srvname server for which \a password should be used.  
 * \param password you guessed it, let's hope no else does.  
 * \param pwlen count of \a password, in bytes.
 * \remarks Useful for remote stored procedure calls, but not in high demand from FreeTDS.  
 * \sa dblogin(), dbopen(), dbrpwclr(), DBSETLAPP(), DBSETLHOST(), DBSETLPWD(), DBSETLUSER().  
 * \todo Unimplemented.
 */
RETCODE
dbrpwset(LOGINREC * login, char *srvname, char *password, int pwlen)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbrpwset()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get server process ID for a \c DBPROCESS.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return \em "spid", the server's process ID.  
 * \sa dbopen().  
 */
int
dbspid(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;

	if (dbproc == NULL) {
		_dblib_client_msg(dbproc, SYBESPID, EXPROGRAM, "Called dbspid() with a NULL dbproc.");
		return FAIL;
	}
	tds = dbproc->tds_socket;
	if (IS_TDSDEAD(tds))
		return FAIL;

	return tds->spid;
}

/**
 * \ingroup dblib_api
 * \brief Associate client-allocated (and defined) data with a \c DBPROCESS.   
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param ptr address of client-defined data.
 * \remarks \a ptr is the location of user data that \c db-lib will associate with \a dbproc. 
 * The client allocates the buffer addressed by \a ptr.  \c db-lib never examines or uses the information; 
 * it just stashes the pointer for later retrieval by the application with \c dbgetuserdata().  
 * \sa dbgetuserdata().  
 */
void
dbsetuserdata(DBPROCESS * dbproc, BYTE * ptr)
{
	dbproc->user_data = ptr;
	return;
}

/**
 * \ingroup dblib_api
 * \brief Get address of user-allocated data from a \c DBPROCESS.   
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return address of user-defined data that \c db-lib associated with \a dbproc when the client called  dbsetuserdata(). 
 * \retval undefined (probably \c NULL) dbsetuserdata() was not previously called.  
 * \sa dbsetuserdata().  
 */
BYTE *
dbgetuserdata(DBPROCESS * dbproc)
{
	return dbproc->user_data;
}

/**
 * \ingroup dblib_api
 * \brief Specify a db-lib version level.
 * 
 * \param version anything, really. 
 * \retval SUCCEED Always.  
 * \remarks No effect on behavior of \c db-lib in \c FreeTDS.  
 * \sa 
 */
RETCODE
dbsetversion(DBINT version)
{
	g_dblib_version = version;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Copy a DBMONEY value.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param src address of a DBMONEY structure.  
 * \param dest \em output: new money. 
 * \retval SUCCEED always, unless \a src or \a dest is \c NULL.  
 * \sa 
 */
RETCODE
dbmnycopy(DBPROCESS * dbproc, DBMONEY * src, DBMONEY * dest)
{
	if ((src == NULL) || (dest == NULL)) {
		return FAIL;
	}
	dest->mnylow = src->mnylow;
	dest->mnyhigh = src->mnyhigh;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Cancel the query currently being retrieved, discarding all pending rows.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa 
 */
RETCODE
dbcanquery(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;
	int rc;
	TDS_INT result_type;

	if (dbproc == NULL)
		return FAIL;
	tds = dbproc->tds_socket;
	if (IS_TDSDEAD(tds))
		return FAIL;

	/* Just throw away all pending rows from the last query */

	rc = tds_process_tokens(dbproc->tds_socket, &result_type, NULL, TDS_STOPAT_ROWFMT|TDS_RETURN_DONE);

	if (rc == TDS_FAIL)
		return FAIL;

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Erase the command buffer, in case \c DBNOAUTOFREE was set with dbsetopt(). 
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbcmd(), dbfcmd(), dbgetchar(), dbsqlexec(), dbsqlsend(), dbsetopt(), dbstrcpy(), dbstrlen().  
 */
void
dbfreebuf(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "in dbfreebuf()\n");
	if (dbproc->dbbuf)
		TDS_ZERO_FREE(dbproc->dbbuf);
	dbproc->dbbufsz = 0;
}				/* dbfreebuf()  */

/**
 * \ingroup dblib_api
 * \brief Reset an option.
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param option to be turned off.
 * \param param clearing some options requires a parameter, believe it or not.  
 * \retval SUCCEED \a option and \a parameter seem sane.
 * \retval FAIL no such \a option.
 * \remarks Only the following options are recognized:
	- DBARITHABORT
	- DBARITHIGNORE
	- DBCHAINXACTS
	- DBFIPSFLAG
	- DBISOLATION
	- DBNOCOUNT
	- DBNOEXEC
	- DBPARSEONLY
	- DBSHOWPLAN
	- DBSTORPROCID
	- DBQUOTEDIDENT
 * \sa dbisopt(), dbsetopt().
 */
RETCODE
dbclropt(DBPROCESS * dbproc, int option, char *param)
{
	char *cmd;

	if ((option < 0) || (option >= DBNUMOPTIONS)) {
		return FAIL;
	}
	dbproc->dbopts[option].optactive = 0;
	switch (option) {
	case DBARITHABORT:
	case DBARITHIGNORE:
	case DBCHAINXACTS:
	case DBFIPSFLAG:
	case DBISOLATION:
	case DBNOCOUNT:
	case DBNOEXEC:
	case DBPARSEONLY:
	case DBSHOWPLAN:
	case DBSTORPROCID:
	case DBQUOTEDIDENT:
		/* server options (on/off) */
		if (asprintf(&cmd, "set %s off\n", dbproc->dbopts[option].opttext) < 0) {
			return FAIL;
		}
		dbstring_concat(&(dbproc->dboptcmd), cmd);
		free(cmd);
		break;
	default:
		break;
	}
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbclropt(option = %d)\n", option);
	return FAIL;
}

/**
 * \ingroup dblib_api
 * \brief Get value of an option
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param option the option
 * \param param a parameter to \a option. 
 * \sa dbclropt(), dbsetopt().
 */
DBBOOL
dbisopt(DBPROCESS * dbproc, int option, char *param)
{
	if ((option < 0) || (option >= DBNUMOPTIONS)) {
		return FALSE;
	}
	return dbproc->dbopts[option].optactive;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get number of the row currently being read.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return ostensibly the row number, or 0 if no rows have been read yet.  
 * \retval 0 Always.  
 * \sa DBCURROW(), dbclrbuf(), DBFIRSTROW(), dbgetrow(), DBLASTROW(), dbnextrow(), dbsetopt(),.  
 * \todo Unimplemented.
 */
DBINT
dbcurrow(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbcurrow()\n");
	return 0;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get returned row's type.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa DBROWTYPE().  
 */
STATUS
dbrowtype(DBPROCESS * dbproc)
{
	return (dbproc)? dbproc->row_type : NO_MORE_ROWS;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get number of the row just returned.
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa DBCURROW().
 * \todo Unimplemented.
 */
int
dbcurcmd(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbcurcmd()\n");
	return 0;
}

/** 
 * \ingroup dblib_api
 * \brief See if more commands are to be processed.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa DBMORECMDS(). DBCMDROW(), dbresults(), DBROWS(), DBROWTYPE().  
 */
RETCODE
dbmorecmds(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "dbmorecmds: ");
	
	if (dbproc->tds_socket->res_info == NULL) {
		return FAIL;
	} 

	if (dbproc->tds_socket->res_info->more_results == 0) {
		tdsdump_log(TDS_DBG_FUNC, "more_results == 0; returns FAIL\n");
		return FAIL;
	}
	
	assert(dbproc->tds_socket->res_info->more_results == 1);
	
	tdsdump_log(TDS_DBG_FUNC, "more_results == 1; returns SUCCEED\n");
	
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get datatype of a stored procedure's return parameter.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param retnum Nth return parameter, between 1 and \c dbnumrets().  
 * \return SYB* datatype token, or -1 if \a retnum is out of range. 
 * \sa dbnextrow(), dbnumrets(), dbprtype(), dbresults(), dbretdata(), dbretlen(), dbretname(), dbrpcinit(), dbrpcparam(). 
 */
int
dbrettype(DBPROCESS * dbproc, int retnum)
{
	TDSCOLUMN *colinfo;
	TDSPARAMINFO *param_info;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	param_info = tds->param_info;
	if (retnum < 1 || retnum > param_info->num_cols)
		return -1;

	colinfo = param_info->columns[retnum - 1];

	return tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
}

/**
 * \ingroup dblib_api
 * \brief Get size of the command buffer, in bytes. 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbcmd(), dbfcmd(), dbfreebuf(), dbgetchar(), dbstrcpy(). 
 */
int
dbstrlen(DBPROCESS * dbproc)
{
	return dbproc->dbbufsz;
}

/**
 * \ingroup dblib_api
 * \brief Get address of a position in the command buffer.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param pos offset within the command buffer, starting at \em 0.   
 * \remarks A bit overspecialized, this one.  
 * \sa dbcmd(), dbfcmd(), dbfreebuf(), dbstrcpy(), dbstrlen(),
 */
char *
dbgetchar(DBPROCESS * dbproc, int pos)
{
	tdsdump_log(TDS_DBG_FUNC, "in dbgetchar() bufsz = %d, pos = %d\n", dbproc->dbbufsz, pos);
	if (dbproc->dbbufsz > 0) {
		if (pos >= 0 && pos < dbproc->dbbufsz)
			return (char *) &dbproc->dbbuf[pos];
		else
			return NULL;
	} else
		return NULL;
}

/**
 * \ingroup dblib_api
 * \brief Get a copy of a chunk of the command buffer.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param start position in the command buffer to start copying from, starting from \em 0.  
 *  	If start is past the end of the command buffer, dbstrcpy() inserts a null terminator at dest[0].
 * \param numbytes number of bytes to copy. 
 	- If -1, dbstrcpy() copies the whole command buffer.  
	- If  0 dbstrcpy() writes a \c NULL to dest[0]. 
	- If the command buffer contains fewer than \a numbytes (taking \a start into account) dbstrcpy() 
	copies the rest of it.  
 * \param dest \em output: the buffer to write to.  Make sure it's big enough.  
 * \retval SUCCEED the inputs were valid and \a dest was affected.  
 * \retval FAIL \a start < 0 or \a numbytes < -1.  
 * \sa dbcmd(), dbfcmd(), dbfreebuf(), dbgetchar(), dbstrlen().  
 */
RETCODE
dbstrcpy(DBPROCESS * dbproc, int start, int numbytes, char *dest)
{
	if (start < 0) {
		_dblib_client_msg(dbproc, SYBENSIP, EXPROGRAM, "Negative starting index passed to dbstrcpy().");
		return FAIL;
	}
	if (numbytes < -1) {
		_dblib_client_msg(dbproc, SYBEBNUM, EXPROGRAM, "Bad numbytes parameter passed to dbstrcpy().");
		return FAIL;
	}
	dest[0] = 0;		/* start with empty string being returned */
	if (numbytes == -1) {
		numbytes = dbproc->dbbufsz;
	}
	if (dbproc->dbbufsz > 0) {
		strncpy(dest, (char *) &dbproc->dbbuf[start], numbytes);
	}
	dest[numbytes] = '\0';
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief safely quotes character values in SQL text.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param src input string.
 * \param srclen length of \a src in bytes, or -1 to indicate it's null-terminated.  
 * \param dest \em output: client-provided output buffer. 
 * \param destlen size of \a dest in bytes, or -1 to indicate it's "big enough" and the data should be null-terminated.  
 * \param quotetype
	- \c DBSINGLE Doubles all single quotes (').
	- \c DBDOUBLE Doubles all double quotes (").
	- \c DBBOTH   Doubles all single and double quotes.
 * \retval SUCCEED everything worked.
 * \retval FAIL no such \a quotetype, or insufficient room in \a dest.
 * \sa dbcmd(), dbfcmd(). 
 */
RETCODE
dbsafestr(DBPROCESS * dbproc, const char *src, DBINT srclen, char *dest, DBINT destlen, int quotetype)
{
	int i, j = 0;
	int squote = FALSE, dquote = FALSE;


	/* check parameters */
	if (srclen < -1 || destlen < -1)
		return FAIL;

	if (srclen == -1)
		srclen = strlen(src);

	if (quotetype == DBSINGLE || quotetype == DBBOTH)
		squote = TRUE;
	if (quotetype == DBDOUBLE || quotetype == DBBOTH)
		dquote = TRUE;

	/* return FAIL if invalid quotetype */
	if (!dquote && !squote)
		return FAIL;


	for (i = 0; i < srclen; i++) {

		/* dbsafestr returns fail if the deststr is not big enough */
		/* need one char + one for terminator */
		if (destlen >= 0 && j >= destlen)
			return FAIL;

		if (squote && src[i] == '\'')
			dest[j++] = '\'';
		else if (dquote && src[i] == '\"')
			dest[j++] = '\"';

		if (destlen >= 0 && j >= destlen)
			return FAIL;

		dest[j++] = src[i];
	}

	if (destlen >= 0 && j >= destlen)
		return FAIL;

	dest[j] = '\0';
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Print a token value's name to a buffer
 * 
 * \param token server SYB* value, e.g. SYBINT.  

 * \return ASCII null-terminated string.  
 * \sa dbaltop(), dbalttype(), dbcoltype(), dbrettype().
 */
const char *
dbprtype(int token)
{

	return tds_prtype(token);

}

/**
 * \ingroup dblib_api
 * \brief Get text timestamp for a column in the current row.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column number of the column in the \c SELECT statement, starting at 1.
 * \return timestamp for \a column, may be NULL.
 * \sa dbtxptr(), dbwritetext().  
 */
DBBINARY *
dbtxtimestamp(DBPROCESS * dbproc, int column)
{
	TDSSOCKET *tds;
	TDSRESULTINFO *resinfo;
	TDSBLOB *blob;

	tds = dbproc->tds_socket;
	if (!tds->res_info)
		return NULL;
	resinfo = tds->res_info;
	--column;
	if (column < 0 || column >= resinfo->num_cols)
		return NULL;
	if (!is_blob_type(resinfo->columns[column]->column_type))
		return NULL;
	blob = (TDSBLOB *) & (resinfo->current_row[resinfo->columns[column]->column_offset]);
	return (DBBINARY *) blob->timestamp;
}

/**
 * \ingroup dblib_api
 * \brief Get text pointer for a column in the current row.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param column number of the column in the \c SELECT statement, starting at 1.
 * \return text pointer for \a column, may be NULL.
 * \sa dbtxtimestamp(), dbwritetext(). 
 */
DBBINARY *
dbtxptr(DBPROCESS * dbproc, int column)
{
	TDSSOCKET *tds;
	TDSRESULTINFO *resinfo;
	TDSBLOB *blob;

	tds = dbproc->tds_socket;
	if (!tds->res_info)
		return NULL;
	resinfo = tds->res_info;
	--column;
	if (column < 0 || column >= resinfo->num_cols)
		return NULL;
	if (!is_blob_type(resinfo->columns[column]->column_type))
		return NULL;
	blob = (TDSBLOB *) & (resinfo->current_row[resinfo->columns[column]->column_offset]);
	return (DBBINARY *) blob->textptr;
}

/**
 * \ingroup dblib_api
 * \brief Send text or image data to the server.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param objname table name
 * \param textptr text pointer to be modified, obtained from dbtxptr(). 
 * \param textptrlen \em Ignored.  Supposed to be \c DBTXPLEN.
 * \param timestamp text timestamp to be modified, obtained from dbtxtimestamp() or dbtxtsnewval(), may be \c NULL.
 * \param log \c TRUE if the operation is to be recorded in the transaction log.
 * \param size overall size of the data (in total, not just for this call), in bytes.  A guideline, must not overstate the case.  
 * \param text the chunk of data to write.  
 * \retval SUCCEED everything worked.
 * \retval FAIL not sent, possibly because \a timestamp is invalid or was changed in the database since it was fetched.  
 * \sa dbmoretext(), dbtxptr(), dbtxtimestamp(), dbwritetext(), dbtxtsput().  
 */
RETCODE
dbwritetext(DBPROCESS * dbproc, char *objname, DBBINARY * textptr, DBTINYINT textptrlen, DBBINARY * timestamp, DBBOOL log,
	    DBINT size, BYTE * text)
{
	char textptr_string[35];	/* 16 * 2 + 2 (0x) + 1 */
	char timestamp_string[19];	/* 8 * 2 + 2 (0x) + 1 */
	TDS_INT result_type;

	if (IS_TDSDEAD(dbproc->tds_socket))
		return FAIL;

	if (textptrlen > DBTXPLEN)
		return FAIL;

	dbconvert(dbproc, SYBBINARY, (BYTE *) textptr, textptrlen, SYBCHAR, (BYTE *) textptr_string, -1);
	dbconvert(dbproc, SYBBINARY, (BYTE *) timestamp, 8, SYBCHAR, (BYTE *) timestamp_string, -1);

	dbproc->dbresults_state = _DB_RES_INIT;

    if (dbproc->tds_socket->state == TDS_PENDING) {

        if (tds_process_tokens(dbproc->tds_socket, &result_type, NULL, TDS_TOKEN_TRAILING) != TDS_NO_MORE_RESULTS) {
            _dblib_client_msg(dbproc, 20019, 7, "Attempt to initiate a new SQL Server operation with results pending.");
            dbproc->command_state = DBCMDSENT;
            return FAIL;
        }
    }

	if (tds_submit_queryf(dbproc->tds_socket,
			      "writetext bulk %s 0x%s timestamp = 0x%s %s",
			      objname, textptr_string, timestamp_string, ((log == TRUE) ? "with log" : ""))
	    != TDS_SUCCEED) {
		return FAIL;
	}

	/* read the end token */
	if (tds_process_simple_query(dbproc->tds_socket) != TDS_SUCCEED)
		return FAIL;

	dbproc->tds_socket->out_flag = 0x07;
	tds_set_state(dbproc->tds_socket, TDS_QUERYING);
	tds_put_int(dbproc->tds_socket, size);

	if (!text) {
		dbproc->text_size = size;
		dbproc->text_sent = 0;
		return SUCCEED;
	}

	tds_put_n(dbproc->tds_socket, text, size);
	tds_flush_packet(dbproc->tds_socket);
	tds_set_state(dbproc->tds_socket, TDS_PENDING);

	if (dbsqlok(dbproc) == SUCCEED) {
		if (dbresults(dbproc) == FAIL)
			return FAIL;
		else
			return SUCCEED;
	} else {
		return FAIL;
	}
}

/**
 * \ingroup dblib_api
 * \brief Fetch part of a text or image value from the server.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param buf \em output: buffer into which text will be placed.
 * \param bufsize size of \a buf, in bytes. 
 * \return 
	- \c >0 count of bytes placed in \a buf.
	- \c  0 end of row.  
	- \c -1 \em error, no result set ready for \dbproc.
	- \c NO_MORE_ROWS all rows read, no further data. 
 * \sa dbmoretext(), dbnextrow(), dbwritetext(). 
 */
STATUS
dbreadtext(DBPROCESS * dbproc, void *buf, DBINT bufsize)
{
	TDSSOCKET *tds;
	TDSCOLUMN *curcol;
	int cpbytes, bytes_avail;
	TDS_INT result_type;
	TDSRESULTINFO *resinfo;

	tds = dbproc->tds_socket;

	if (!tds || !tds->res_info || !tds->res_info->columns[0])
		return -1;

	resinfo = tds->res_info;
	curcol = resinfo->columns[0];

	/*
	 * if the current position is beyond the end of the text
	 * set pos to 0 and return 0 to denote the end of the 
	 * text 
	 */
	if (curcol->column_textpos && curcol->column_textpos >= curcol->column_cur_size) {
		curcol->column_textpos = 0;
		return 0;
	}

	/*
	 * if pos is 0 (first time through or last call exhausted the text)
	 * then read another row
	 */

	if (curcol->column_textpos == 0) {
		switch (tds_process_tokens(dbproc->tds_socket, &result_type, NULL, TDS_STOPAT_ROWFMT|TDS_STOPAT_DONE|TDS_RETURN_ROW|TDS_RETURN_COMPUTE)) {
		case TDS_FAIL:
			return -1;
		case TDS_SUCCEED:
			if (result_type == TDS_ROW_RESULT || result_type == TDS_COMPUTE_RESULT)
				break;
		case TDS_NO_MORE_RESULTS:
			return NO_MORE_ROWS;
		}
	}

	/* find the number of bytes to return */
	bytes_avail = curcol->column_cur_size - curcol->column_textpos;
	cpbytes = bytes_avail > bufsize ? bufsize : bytes_avail;
	memcpy(buf, &((TDSBLOB *) (resinfo->current_row + curcol->column_offset))->textvalue[curcol->column_textpos], cpbytes);
	curcol->column_textpos += cpbytes;
	return cpbytes;
}

/**
 * \ingroup dblib_api
 * \brief Send chunk of a text/image value to the server.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param size count of bytes to send.  
 * \param text textpointer, obtained from dbtxptr.
 * \retval SUCCEED always.
 * \sa dbtxptr(), dbtxtimestamp(), dbwritetext(). 
 * \todo Check return value of called functions and return \c FAIL if appropriate. 
 */
RETCODE
dbmoretext(DBPROCESS * dbproc, DBINT size, BYTE * text)
{
	/* TODO test dbproc value */
	/* FIXME test wire state */
	dbproc->tds_socket->out_flag = 0x07;
	tds_put_n(dbproc->tds_socket, text, size);
	dbproc->text_sent += size;

	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Record to a file all SQL commands sent to the server
 * 
 * \param filename name of file to write to. 
 * \remarks Files are named \em filename.n, where n is an integer, starting with 0, and incremented with each callto dbopen().  
 * \sa dbopen(), TDSDUMP environment variable(). 
 */
void
dbrecftos(char *filename)
{
	g_dblib_ctx.recftos_filename = malloc(strlen(filename) + 1);
	if (g_dblib_ctx.recftos_filename != NULL) {
		strcpy(g_dblib_ctx.recftos_filename, filename);
		g_dblib_ctx.recftos_filenum = 0;
	}
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get the TDS version in use for \a dbproc.  
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return a \c DBTDS* token.  
 * \remarks The integer values of the constants are counterintuitive.  
 * \sa DBTDS().
 */
int
dbtds(DBPROCESS * dbproc)
{
	if (dbproc && dbproc->tds_socket) {
		switch (dbproc->tds_socket->major_version) {
		case 4:
			switch (dbproc->tds_socket->minor_version) {
			case 2:
				return DBTDS_4_2;
			case 6:
				return DBTDS_4_6;
			default:
				return DBTDS_UNKNOWN;
			}
		case 5:
			return DBTDS_5_0;
		case 7:
			return DBTDS_7_0;
		case 8:
			return DBTDS_8_0;
		default:
			return DBTDS_UNKNOWN;
		}
	}
	return DBTDS_UNKNOWN;
}

/**
 * \ingroup dblib_api
 * \brief See which version of db-lib is in use.
 * 
 * \return null-terminated ASCII string representing the version of db-lib.  
 * \remarks FreeTDS returns the CVS version string of dblib.c.  
 * \sa 
 */
const char *
dbversion()
{
	return software_version;
}

/**
 * \ingroup dblib_api
 * \brief Set the default character set.  
 * 
 * \param charset null-terminated ASCII string, matching a row in master..syscharsets.  
 * \sa dbsetdeflang(), dbsetdefcharset(), dblogin(), dbopen(). 
 * \todo Unimplemented.
 */
RETCODE
dbsetdefcharset(char *charset)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbsetdefcharset()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Ready execution of a registered procedure.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param procedure_name to call.
 * \param namelen size of \a procedure_name, in bytes.
 * \sa dbregparam(), dbregexec(), dbregwatch(), dbreglist(), dbregwatchlist
 * \todo Unimplemented.
 */
RETCODE
dbreginit(DBPROCESS * dbproc, DBCHAR * procedure_name, DBSMALLINT namelen)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbreginit()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get names of Open Server registered procedures.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbregparam(), dbregexec(), dbregwatch(), dbreglist(), dbregwatchlist(). 
 * \todo Unimplemented.
 */
RETCODE
dbreglist(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbreglist()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief  Describe parameter of registered procedure .
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param param_name
 * \param type \c SYB* datatype. 
 * \param datalen size of \a data.
 * \param data address of buffer holding value for the parameter.  
 * \sa dbreginit(), dbregexec(), dbnpdefine(), dbnpcreate(), dbregwatch(). 
 * \todo Unimplemented.
 */
RETCODE
dbregparam(DBPROCESS * dbproc, char *param_name, int type, DBINT datalen, BYTE * data)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbregparam()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Execute a registered procedure.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param options
 * \sa dbreginit(), dbregparam(), dbregwatch(), dbregnowatch
 * \todo Unimplemented.
 */
RETCODE
dbregexec(DBPROCESS * dbproc, DBUSMALLINT options)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbregexec()\n");
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get name of a month, in some human language.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param language \em ignored.
 * \param monthnum number of the month, starting with 1.
 * \param shortform set to \c TRUE for a three letter output ("Jan" - "Dec"), else zero.  
 * \return address of null-terminated ASCII string, or \c NULL on error. 
 * \sa db12hour(), dbdateorder(), dbdayname(), DBSETLNATLANG(), dbsetopt().  
 */
const char *
dbmonthname(DBPROCESS * dbproc, char *language, int monthnum, DBBOOL shortform)
{
	static const char shortmon[][4] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	static const char *const longmon[] = {
		"January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December"
	};

	if (monthnum < 1 || monthnum > 12)
		return NULL;
	return (shortform) ? shortmon[monthnum - 1] : longmon[monthnum - 1];
}

/**
 * \ingroup dblib_api
 * \brief See if a command caused the current database to change.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return name of new database, if changed, as a null-terminated ASCII string, else \c NULL.

 * \sa dbname(), dbresults(), dbsqlexec(), dbsqlsend(), dbuse().  
 */
char *
dbchange(DBPROCESS * dbproc)
{

	if (dbproc->envchange_rcv & (1 << (TDS_ENV_DATABASE - 1))) {
		return dbproc->dbcurdb;
	}
	return NULL;
}

/**
 * \ingroup dblib_api
 * \brief Get name of current database.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return current database name, as null-terminated ASCII string.
 * \sa dbchange(), dbuse().  
 */
char *
dbname(DBPROCESS * dbproc)
{

	return dbproc->dbcurdb;
}

/**
 * \ingroup dblib_api
 * \brief Get \c syscharset name of the server character set.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \return name of server's charset, as null-terminated ASCII string.
 * \sa dbcharsetconv(), dbgetcharset(), DBSETLCHARSET().  
 */
char *
dbservcharset(DBPROCESS * dbproc)
{

	return dbproc->servcharset;
}

/**
 * \ingroup dblib_api
 * \brief Transmit the command buffer to the server.  \em Non-blocking, does not wait for a response.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \retval SUCCEED SQL sent.
 * \retval FAIL protocol problem, unless dbsqlsend() when it's not supposed to be (in which case a db-lib error
 message will be emitted).  
 * \sa dbcmd(), dbfcmd(), DBIORDESC(), DBIOWDESC(), dbnextrow(), dbpoll(), dbresults(), dbsettime(), dbsqlexec(), dbsqlok().  
 */
RETCODE
dbsqlsend(DBPROCESS * dbproc)
{
	TDSSOCKET *tds;
	char *cmdstr;
	int rc;
	TDS_INT result_type;
	char timestr[256];

	dbproc->avail_flag = FALSE;
	dbproc->envchange_rcv = 0;
	dbproc->dbresults_state = _DB_RES_INIT;

	tdsdump_log(TDS_DBG_FUNC, "in dbsqlsend()\n");
	tds = dbproc->tds_socket;

	if (tds->state == TDS_PENDING) {

		if (tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_TRAILING) != TDS_NO_MORE_RESULTS) {
			_dblib_client_msg(dbproc, 20019, 7, "Attempt to initiate a new SQL Server operation with results pending.");
			dbproc->command_state = DBCMDSENT;
			return FAIL;
		}
	}

	if (dbproc->dboptcmd) {
		if ((cmdstr = dbstring_get(dbproc->dboptcmd)) == NULL) {
			return FAIL;
		}
		rc = tds_submit_query(dbproc->tds_socket, cmdstr);
		free(cmdstr);
		dbstring_free(&(dbproc->dboptcmd));
		if (rc != TDS_SUCCEED) {
			return FAIL;
		}
		while ((rc = tds_process_tokens(tds, &result_type, NULL, TDS_TOKEN_RESULTS))
		       == TDS_SUCCEED);
		if (rc != TDS_NO_MORE_RESULTS) {
			return FAIL;
		}
	}
	dbproc->more_results = TRUE;

	if (dbproc->ftos != NULL) {
		fprintf(dbproc->ftos, "%s\n", dbproc->dbbuf);
		fprintf(dbproc->ftos, "go /* %s */\n", _dbprdate(timestr));
		fflush(dbproc->ftos);
	}

	if (tds_submit_query(dbproc->tds_socket, (char *) dbproc->dbbuf) != TDS_SUCCEED) {
		return FAIL;
	}
	dbproc->command_state = DBCMDSENT;
	return SUCCEED;
}

/**
 * \ingroup dblib_api
 * \brief Get user-defined datatype of a compute column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \returns user-defined datatype of compute column, else -1.
 * \sa dbalttype(), dbcolutype().
 */
DBINT
dbaltutype(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *colinfo;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "in dbaltutype()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return -1;

	colinfo = info->columns[column - 1];
	return colinfo->column_usertype;
}

/**
 * \ingroup dblib_api
 * \brief Get size of data in compute column.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param computeid of \c COMPUTE clause to which we're referring. 
 * \param column Nth column in \a computeid, starting from 1.
 * \sa dbadata(), dbadlen(), dbalttype(), dbgetrow(), dbnextrow(), dbnumalts().
 */
DBINT
dbaltlen(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLUMN *colinfo;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "in dbaltlen()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return -1;

	colinfo = info->columns[column - 1];

	return colinfo->column_size;

}

/**
 * \ingroup dblib_api
 * \brief See if a server response has arrived.
 * 
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param milliseconds how long to wait for the server before returning:
 	- \c  0 return immediately.
	- \c -1 do not return until the server responds or a system interrupt occurs.
 * \param ready_dbproc \em output: DBPROCESS for which a response arrived, of \c NULL. 
 * \param return_reason \em output: 
	- \c DBRESULT server responded.
	- \c DBNOTIFICATION registered procedure notification has arrived. dbpoll() the registered handler, if
		any, before it returns.
	- \c DBTIMEOUT \a milliseconds elapsed before the server responded.
	- \c DBINTERRUPT operating-system interrupt occurred before the server responded.
 * \retval SUCCEED everything worked.
 * \retval FAIL a server connection died.
 * \sa  DBIORDESC(), DBRBUF(), dbresults(), dbreghandle(), dbsqlok(). 
 * \todo Unimplemented.
 */
RETCODE
dbpoll(DBPROCESS * dbproc, long milliseconds, DBPROCESS ** ready_dbproc, int *return_reason)
{
	tdsdump_log(TDS_DBG_FUNC, "UNIMPLEMENTED dbpoll()\n");
	return SUCCEED;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get number of the last row in the row buffer.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa DBLASTROW(), dbclrbuf(), DBCURROW(), DBFIRSTROW(), dbgetrow(), dbnextrow(), dbsetopt(). 
 */
DBINT
dblastrow(DBPROCESS * dbproc)
{
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = dbproc->tds_socket;
	resinfo = tds->res_info;
	return resinfo->row_count;
#if 0
	DBINT result;

	if (dbproc->row_buf.rows_in_buf == 0) {
		result = 0;
	} else {
		/*
		 * rows returned from the row buffer start with 1 instead of 0
		 * newest is 0 based.
		 */
		result = dbproc->row_buf.newest + 1;
	}
	return result;
#endif
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get number of the first row in the row buffer.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa DBFIRSTROW(), dbclrbuf(), DBCURROW(), dbgetrow(), DBLASTROW(), dbnextrow(), dbsetopt(). 
 */
DBINT
dbfirstrow(DBPROCESS * dbproc)
{
	DBINT result;

	if (dbproc->row_buf.rows_in_buf == 0) {
		result = 0;
	} else {
		result = dbproc->row_buf.oldest;
	}
	return result;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get file descriptor of the socket used by a \c DBPROCESS to read data coming from the server. (!)
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbcmd(), DBIORDESC(), DBIOWDESC(), dbnextrow(), dbpoll(), DBRBUF(), dbresults(), dbsqlok(), dbsqlsend().  
 */
int
dbiordesc(DBPROCESS * dbproc)
{
	return dbproc->tds_socket->s;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Get file descriptor of the socket used by a \c DBPROCESS to write data coming to the server. (!)
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \sa dbcmd(), DBIORDESC(), DBIOWDESC(), dbnextrow(), dbpoll(), DBRBUF(), dbresults(), dbsqlok(), dbsqlsend().  
 */
int
dbiowdesc(DBPROCESS * dbproc)
{
	return dbproc->tds_socket->s;
}

static void
_set_null_value(BYTE * varaddr, int datatype, int maxlen)
{
	switch (datatype) {
	case SYBINT8:
		memset(varaddr, '\0', 8);
		break;
	case SYBINT4:
		memset(varaddr, '\0', 4);
		break;
	case SYBINT2:
		memset(varaddr, '\0', 2);
		break;
	case SYBINT1:
		memset(varaddr, '\0', 1);
		break;
	case SYBFLT8:
		memset(varaddr, '\0', 8);
		break;
	case SYBREAL:
		memset(varaddr, '\0', 4);
		break;
	case SYBDATETIME:
		memset(varaddr, '\0', 8);
		break;
	case SYBCHAR:
		if (varaddr)
			varaddr[0] = '\0';
		break;
	case SYBVARCHAR:
		if (varaddr)
			((DBVARYCHAR *)varaddr)->len = 0;
			((DBVARYCHAR *)varaddr)->str[0] = '\0';
		break;
	}
}

/** \internal
 * \ingroup dblib_internal
 * \brief See if a \c DBPROCESS is marked "available".
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \remarks Basically bogus.  \c FreeTDS behaves the way Sybase's implementation does, but so what?  
 	Many \c db-lib functions set the \c DBPROCESS to "not available", but only 
	dbsetavail() resets it to "available".  
 * \retval TRUE \a dbproc is "available".   
 * \retval FALSE \a dbproc is not "available".   
 * \sa DBISAVAIL(). DBSETAVAIL().
 */
DBBOOL
dbisavail(DBPROCESS * dbproc)
{
	return dbproc->avail_flag;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Mark a \c DBPROCESS as "available".
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \remarks Basically bogus.  \c FreeTDS behaves the way Sybase's implementation does, but so what?  
 	Many \c db-lib functions set the \c DBPROCESS to "not available", but only 
	dbsetavail() resets it to "available".  
 * \sa DBISAVAIL(). DBSETAVAIL().
 */
void
dbsetavail(DBPROCESS * dbproc)
{
	dbproc->avail_flag = TRUE;
}

/**
 * \ingroup dblib_api
 * \brief Build a printable string from text containing placeholders for variables.
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param charbuf \em output: buffer that will contain the ASCII null-terminated string built by \c dbstrbuild().
 * \param bufsize size of \a charbuf, in bytes. 
 * \param text null-terminated ASCII string, with \em placeholders for variables. \em A Placeholder is a
 * 	three-byte string, made up of: 
 	- '%' a percent sign
	- 0-9 an integer (designates the argument number to use, starting with 1.)
	- '!' an exclamation point
 * \param formats null-terminated ASCII sprintf-style string.  Has one format specifier for each placeholder in \a text.
 * \remarks Following \a formats are the arguments, the values to substitute for the placeholders.  
 * \sa dbconvert(), dbdatename(), dbdatepart().  
 */
int
dbstrbuild(DBPROCESS * dbproc, char *charbuf, int bufsize, char *text, char *formats, ...)
{
	va_list ap;
	int rc;
	int resultlen;

	va_start(ap, formats);
	rc = tds_vstrbuild(charbuf, bufsize, &resultlen, text, TDS_NULLTERM, formats, TDS_NULLTERM, ap);
	charbuf[resultlen] = '\0';
	va_end(ap);
	return rc;
}

/** \internal
 * \ingroup dblib_internal
 * \brief Pass a server-generated error message to the client's installed handler.  
 * 
 * \param dbproc contains all information needed by db-lib to manage communications with the server.
 * \param dberr error number
 * \param severity severity level
 * \param dberrstr null-terminated ASCII string, the error message.  
 * \return Propogates return code of tds_client_msg().  
 * \sa tds_client_msg().  
 */
int
_dblib_client_msg(DBPROCESS * dbproc, int dberr, int severity, const char *dberrstr)
{
	TDSSOCKET *tds = NULL;

	if (dbproc)
		tds = dbproc->tds_socket;
	return tds_client_msg(g_dblib_ctx.tds_ctx, tds, dberr, severity, -1, -1, dberrstr);
}

static char *
_dbprdate(char *timestr)
{
	time_t currtime;

	currtime = time(NULL);

	strcpy(timestr, asctime(gmtime(&currtime)));
	timestr[strlen(timestr) - 1] = '\0';	/* remove newline */
	return timestr;

}

static char *
tds_prdatatype(TDS_SERVER_TYPE datatype_token)
{
	switch (datatype_token) {
	case SYBCHAR:		return "SYBCHAR";
	case SYBVARCHAR:		return "SYBVARCHAR";
	case SYBINTN:		return "SYBINTN";
	case SYBINT1:		return "SYBINT1";
	case SYBINT2:		return "SYBINT2";
	case SYBINT4:		return "SYBINT4";
	case SYBINT8:		return "SYBINT8";
	case SYBFLT8:		return "SYBFLT8";
	case SYBDATETIME:		return "SYBDATETIME";
	case SYBBIT:		return "SYBBIT";
	case SYBTEXT:		return "SYBTEXT";
	case SYBNTEXT:		return "SYBNTEXT";
	case SYBIMAGE:		return "SYBIMAGE";
	case SYBMONEY4:		return "SYBMONEY4";
	case SYBMONEY:		return "SYBMONEY";
	case SYBDATETIME4:		return "SYBDATETIME4";
	case SYBREAL:		return "SYBREAL";
	case SYBBINARY:		return "SYBBINARY";
	case SYBVOID:		return "SYBVOID";
	case SYBVARBINARY:		return "SYBVARBINARY";
	case SYBNVARCHAR:		return "SYBNVARCHAR";
	case SYBBITN:		return "SYBBITN";
	case SYBNUMERIC:		return "SYBNUMERIC";
	case SYBDECIMAL:		return "SYBDECIMAL";
	case SYBFLTN:		return "SYBFLTN";
	case SYBMONEYN:		return "SYBMONEYN";
	case SYBDATETIMN:		return "SYBDATETIMN";
	case XSYBCHAR:		return "XSYBCHAR";
	case XSYBVARCHAR:		return "XSYBVARCHAR";
	case XSYBNVARCHAR:		return "XSYBNVARCHAR";
	case XSYBNCHAR:		return "XSYBNCHAR";
	case XSYBVARBINARY:	return "XSYBVARBINARY";
	case XSYBBINARY:		return "XSYBBINARY";
	case SYBLONGBINARY:	return "SYBLONGBINARY";
	case SYBSINT1:		return "SYBSINT1";
	case SYBUINT2:		return "SYBUINT2";
	case SYBUINT4:		return "SYBUINT4";
	case SYBUINT8:		return "SYBUINT8";
	case SYBUNIQUE:		return "SYBUNIQUE";
	case SYBVARIANT:		return "SYBVARIANT";
	default: break;
	}
	return "(unknown)";
}

static void
copy_data_to_host_var(DBPROCESS * dbproc, int srctype, const BYTE * src, DBINT srclen, 
				int desttype, BYTE * dest, DBINT destlen,
				int bindtype, DBSMALLINT *indicator)
{
	TDSSOCKET *tds = NULL;

	CONV_RESULT dres;
	DBINT ret;
	int i;
	int len;
	DBNUMERIC *num;
    DBSMALLINT indicator_value = 0;

    int limited_dest_space = 0;

	tdsdump_log(TDS_DBG_INFO1, "copy_data_to_host_var(%d [%s] len %d => %d [%s] len %d)\n", 
		     srctype, tds_prdatatype(srctype), srclen, desttype, tds_prdatatype(desttype), destlen);

	if (dbproc) {
		tds = dbproc->tds_socket;
	}

	if (src == NULL || (srclen == 0 && is_nullable_type(srctype))) {
		_set_null_value(dest, desttype, destlen);
		return;
	}

	if (destlen > 0) {
		limited_dest_space = 1;
	}

	/* oft times we are asked to convert a data type to itself */

	if ((srctype == desttype) ||
		(is_similar_type(srctype, desttype))) {

		tdsdump_log(TDS_DBG_INFO1, "copy_data_to_host_var() srctype == desttype\n");
		switch (desttype) {

		case SYBBINARY:
		case SYBIMAGE:
			if (srclen > destlen && destlen >= 0) {
				_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
			} else {
				memcpy(dest, src, srclen);
				if (srclen < destlen)
					memset(dest + srclen, 0, destlen - srclen);
			}
			break;

		case SYBCHAR:
		case SYBVARCHAR:
		case SYBTEXT:

			switch (bindtype) {
				case NTBSTRINGBIND: /* strip trailing blanks, null term */
					while (srclen && src[srclen - 1] == ' ') {
						--srclen;
					}
					if (limited_dest_space) {
						if (srclen + 1 > destlen) {
							_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
							indicator_value = srclen + 1;
							srclen = destlen - 1;
						}
					}
					memcpy(dest, src, srclen);
					dest[srclen] = '\0';
					break;
				case STRINGBIND:   /* pad with blanks, null term */
					if (limited_dest_space) {
						if (srclen + 1 > destlen) {
							_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
							indicator_value = srclen + 1;
							srclen = destlen - 1;
						}
					} else {
						destlen = srclen; 
					}
					memcpy(dest, src, srclen);
					for (i = srclen; i < destlen - 1; i++)
						dest[i] = ' ';
					dest[i] = '\0';
					break;
				case CHARBIND:   /* pad with blanks, NO null term */
					if (limited_dest_space) {
						if (srclen > destlen) {
							_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
							indicator_value = srclen;
							srclen = destlen;
						}
					} else {
						destlen = srclen; 
					}
					memcpy(dest, src, srclen);
					for (i = srclen; i < destlen; i++)
						dest[i] = ' ';
					break;
				case VARYCHARBIND: /* strip trailing blanks, NO null term */
					if (limited_dest_space) {
						if (srclen > destlen) {
							_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
							indicator_value = srclen;
							srclen = destlen;
						}
					}
					memcpy(((DBVARYCHAR *)dest)->str, src, srclen);
					((DBVARYCHAR *)dest)->len = srclen;
					break;
			} 
			break;
		case SYBINT1:
		case SYBINT2:
		case SYBINT4:
		case SYBINT8:
		case SYBFLT8:
		case SYBREAL:
		case SYBBIT:
		case SYBBITN:
		case SYBMONEY:
		case SYBMONEY4:
		case SYBDATETIME:
		case SYBDATETIME4:
		case SYBUNIQUE:
			ret = tds_get_size_by_type(desttype);
			memcpy(dest, src, ret);
			break;

		case SYBNUMERIC:
		case SYBDECIMAL:
			memcpy(dest, src, sizeof(DBNUMERIC));
			break;

		default:
			break;
		}
		if (indicator)
			*(DBINT *)(indicator) = indicator_value;

		return;

	} /* end srctype == desttype */

	assert(srctype != desttype);

	if (is_numeric_type(desttype)) {
		num = (DBNUMERIC *) dest;
		if (num->precision == 0)
			dres.n.precision = 18;
		else
			dres.n.precision = num->precision;
		if (num->scale == 0)
			dres.n.scale = 0;
		else
			dres.n.scale = num->scale;
	}

	len = tds_convert(g_dblib_ctx.tds_ctx, srctype, (const TDS_CHAR *) src, srclen, desttype, &dres);
	tdsdump_log(TDS_DBG_INFO1, "copy_data_to_host_var() called tds_convert returned %d\n", len);

	switch (len) {
	case TDS_CONVERT_NOAVAIL:
		_dblib_client_msg(dbproc, SYBERDCN, EXCONVERSION, "Requested data-conversion does not exist.");
		return;
		break;
	case TDS_CONVERT_SYNTAX:
		_dblib_client_msg(dbproc, SYBECSYN, EXCONVERSION, "Attempt to convert data stopped by syntax error in source field.");
		return;
		break;
	case TDS_CONVERT_NOMEM:
		_dblib_client_msg(dbproc, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return;
		break;
	case TDS_CONVERT_OVERFLOW:
		_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data conversion resulted in overflow.");
		return;
		break;
	case TDS_CONVERT_FAIL:
		return;
		break;
	default:
		if (len < 0) {
			return;
		}
		break;
	}

	switch (desttype) {
	case SYBBINARY:
	case SYBIMAGE:
		if (len > destlen && destlen >= 0) {
			_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
		} else {
			memcpy(dest, dres.ib, len);
			free(dres.ib);
			if (len < destlen)
				memset(dest + len, 0, destlen - len);
		}
		break;
	case SYBINT1:
		memcpy(dest, &(dres.ti), 1);
		break;
	case SYBINT2:
		memcpy(dest, &(dres.si), 2);
		break;
	case SYBINT4:
		memcpy(dest, &(dres.i), 4);
		break;
	case SYBINT8:
		memcpy(dest, &(dres.bi), 8);
		break;
	case SYBFLT8:
		memcpy(dest, &(dres.f), 8);
		break;
	case SYBREAL:
		memcpy(dest, &(dres.r), 4);
		break;
	case SYBBIT:
	case SYBBITN:
		memcpy(dest, &(dres.ti), 1);
		break;
	case SYBMONEY:
		memcpy(dest, &(dres.m), sizeof(TDS_MONEY));
		break;
	case SYBMONEY4:
		memcpy(dest, &(dres.m4), sizeof(TDS_MONEY4));
		break;
	case SYBDATETIME:
		memcpy(dest, &(dres.dt), sizeof(TDS_DATETIME));
		break;
	case SYBDATETIME4:
		memcpy(dest, &(dres.dt4), sizeof(TDS_DATETIME4));
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		memcpy(dest, &(dres.n), sizeof(TDS_NUMERIC));
		break;
	case SYBUNIQUE:
		memcpy(dest, &(dres.u), sizeof(TDS_UNIQUE));
		break;
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
		tdsdump_log(TDS_DBG_INFO1, "copy_data_to_host_var() outputting %d bytes character data destlen = %d \n", len, destlen);
		switch (bindtype) {
			case NTBSTRINGBIND: /* strip trailing blanks, null term */
				while (len && dres.c[len - 1] == ' ') {
					--len;
				}
				if (limited_dest_space) {
					if (len + 1 > destlen) {
						_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
						len = destlen - 1;
					}
				}
				memcpy(dest, dres.c, len);
				dest[len] = '\0';
				break;
			case STRINGBIND:   /* pad with blanks, null term */
				if (limited_dest_space) {
					if (len + 1 > destlen) {
						_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
						len = destlen - 1;
					}
				} else {
					destlen = len; 
				}
				memcpy(dest, dres.c, len);
				for (i = len; i < destlen - 1; i++)
					dest[i] = ' ';
				dest[i] = '\0';
				break;
			case CHARBIND:   /* pad with blanks, NO null term */
				if (limited_dest_space) {
					if (len > destlen) {
						_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
						indicator_value = len;
						len = destlen;
					}
				} else {
					destlen = len; 
				}
				memcpy(dest, dres.c, len);
				for (i = len; i < destlen; i++)
					dest[i] = ' ';
				break;
			case VARYCHARBIND: /* strip trailing blanks, NO null term */
				if (limited_dest_space) {
					if (len > destlen) {
						_dblib_client_msg(dbproc, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
						indicator_value = len;
						len = destlen;
					}
				} 
				memcpy(((DBVARYCHAR *)dest)->str, dres.c, len);
				((DBVARYCHAR *)dest)->len = len;
				break;
		} 

		free(dres.c);
		break;
	default:
		tdsdump_log(TDS_DBG_INFO1, "error: dbconvert(): unrecognized desttype %d \n", desttype);
		break;

	}
	if (indicator)
		*indicator = indicator_value;

	return;
}
