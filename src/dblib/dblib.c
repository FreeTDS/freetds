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

#include "tds.h"
#include "sybfront.h"
#include "sybdb.h"
#include "syberror.h"
#include "dblib.h"
#include "tdsconvert.h"
#include "replacements.h"

static char software_version[] = "$Id: dblib.c,v 1.123 2003-03-07 15:04:36 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static int _db_get_server_type(int bindtype);
static int _get_printable_size(TDSCOLINFO * colinfo);
static char *_dbprdate();

static void _set_null_value(DBPROCESS * dbproc, BYTE * varaddr, int datatype, int maxlen);

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
}				/* buffer_init()  */

static void
buffer_clear(DBPROC_ROWBUF * buf)
{
	buf->next_row = 1;
	buf->first_in_buf = 0;
	buf->newest = -1;
	buf->oldest = 0;
	buf->rows_in_buf = 0;
	if (buf->rows) {
		free(buf->rows);
	}
	buf->rows = NULL;
}				/* buffer_clear()  */


static void
buffer_free(DBPROC_ROWBUF * buf)
{
	if (buf->rows != NULL) {
		free(buf->rows);
	}
	buf->rows = NULL;
}				/* clear_buffer()  */

static void
buffer_delete_rows(DBPROC_ROWBUF * buf,	/* (U) buffer to clear             */
		   int count)
{				/* (I) number of elements to purge */
	assert(count <= buf->elcount);	/* possibly a little to pedantic */

	if (count > buf->rows_in_buf) {
		count = buf->rows_in_buf;
	}


	buf->oldest = (buf->oldest + count) % buf->elcount;
	buf->rows_in_buf -= count;
	buf->first_in_buf = count == buf->rows_in_buf ? buf->next_row - 1 : buf->first_in_buf + count;


	assert(buf->first_in_buf >= 0);
}				/* buffer_delete_rows() */


static int
buffer_start_resultset(DBPROC_ROWBUF * buf,	/* (U) buffer to clear */
		       int element_size)
{				/*                     */
	int space_needed = -1;

	assert(element_size > 0);

	if (buf->rows != NULL) {
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
}				/* buffer_start_resultset()  */


static void
buffer_delete_all_rows(DBPROC_ROWBUF * buf)
{				/* (U) buffer to clear */
	buffer_delete_rows(buf, buf->rows_in_buf);
}				/* delete_all_buffer_rows() */

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
}				/* buffer_index_of_resultset_row()  */


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
}				/* buffer_row_address()  */


static void
buffer_add_row(DBPROC_ROWBUF * buf,	/* (U) buffer to add row into  */
	       void *row,	/* (I) pointer to the row data */
	       int row_size)
{
	void *dest = NULL;

	assert(row_size > 0);
/*
   assert(row_size == buf->element_size);
*/
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
}				/* buffer_add_row()  */


static int
buffer_is_full(DBPROC_ROWBUF * buf)
{
	return buf->rows_in_buf == buf->elcount;
}				/* buffer_is_full()  */

static void
buffer_set_buffering(DBPROC_ROWBUF * buf,	/* (U)                                         */
		     int buf_size)
{				/* (I) number of rows to buffer, 0 to turn off */
	/* XXX If the user calls this routine in the middle of 
	 * a result set and changes the size of the buffering
	 * they are pretty much toast.
	 *
	 * We need to figure out what to do if the user shrinks the 
	 * size of the row buffer.  What rows should be thrown out, 
	 * what should happen to the current row, etc?
	 */

	assert(buf_size >= 0);

	if (buf_size < 0) {
		buf_size = 100;	/* the default according to Microsoft */
	}

	if (buf_size == 0) {
		buf->buffering_on = 0;
		buf->elcount = 1;
		buffer_delete_all_rows(buf);
	} else {
		buf->buffering_on = 1;
		buffer_clear(buf);
		buffer_free(buf);
		buf->elcount = buf_size;
		if (buf->element_size > 0) {
			buf->rows = malloc(buf->element_size * buf->elcount);
		} else {
			buf->rows = NULL;
		}
	}
}				/* buffer_set_buffering()  */

static void
buffer_transfer_bound_data(TDS_INT rowtype, TDS_INT compute_id, DBPROC_ROWBUF * buf,	/* (U)                                         */
			   DBPROCESS * dbproc,	/* (I)                                         */
			   int row_num)
{				/* (I) resultset row number                    */

	int i;
	TDSCOLINFO *curcol;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;
	int srctype;
	BYTE *src;
	int desttype;
	int destlen;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (rowtype == TDS_REG_ROW) {
		resinfo = tds->res_info;
	} else {		/* TDS_COMP_ROW */
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
			if (tds_get_null(resinfo->current_row, i)) {
				*((DBINT *) curcol->column_nullbind) = -1;
			} else {
				*((DBINT *) curcol->column_nullbind) = 0;
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
				src = (BYTE*) ((TDSBLOBINFO *) src)->textvalue;
			}
			desttype = _db_get_server_type(curcol->column_bindtype);
			srctype = tds_get_conversion_type(curcol->column_type, curcol->column_size);

			if (tds_get_null(resinfo->current_row, i)) {
				_set_null_value(dbproc, (BYTE*)curcol->column_varaddr, desttype, curcol->column_bindlen);
			} else {

				/* FIXME use also curcol->column_bindlen, 
				 * for CHARBIND, curcol->column_bindlen < 0 use srclen
				 * check limit for other bind types */
				switch (curcol->column_bindtype) {
				case STRINGBIND:
					destlen = -2;
					break;
				case NTBSTRINGBIND:
					destlen = -1;
					break;
				default:
					if (curcol->column_bindlen == 0)
						destlen = -1;
					else
						destlen = curcol->column_bindlen;
					break;
				}

				dbconvert(dbproc, srctype,	/* srctype  */
					  src,	/* src      */
					  srclen,	/* srclen   */
					  desttype,	/* desttype */
					  (BYTE *) curcol->column_varaddr,	/* dest     */
					  destlen);	/* destlen  */
			}	/* else not null */
		}
	}
}				/* buffer_transfer_bound_data()  */

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

RETCODE
dbinit(void)
{
	/* 
	 * DBLIBCONTEXT stores a list of current connections so they may be closed
	 * with dbexit() 
	 */
	memset(&g_dblib_ctx, '\0', sizeof(DBLIBCONTEXT));

	g_dblib_ctx.connection_list = (TDSSOCKET**) calloc(TDS_MAX_CONN, sizeof(TDSSOCKET*));
	if (g_dblib_ctx.connection_list == NULL) {
		tdsdump_log(TDS_DBG_FUNC, "%L dbinit: out of memory\n");
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
	g_dblib_ctx.tds_ctx->msg_handler = dblib_handle_info_message;
	g_dblib_ctx.tds_ctx->err_handler = dblib_handle_err_message;


	if (g_dblib_ctx.tds_ctx->locale && !g_dblib_ctx.tds_ctx->locale->date_fmt) {
		/* set default in case there's no locale file */
		g_dblib_ctx.tds_ctx->locale->date_fmt = strdup("%b %e %Y %I:%M:%S:%z%p");
	}

	return SUCCEED;
}

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
	/* tds_set_charset(loginrec->tds_login, "iso_1"); */
	/* tds_set_packet(loginrec->tds_login, TDS_DEF_BLKSZ); */

	return loginrec;
}

void
dbloginfree(LOGINREC * login)
{
	if (login) {
		tds_free_login(login->tds_login);
		free(login);
	}
}

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
		tds_set_charset(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETNATLANG:
		tds_set_language(login->tds_login, value);
		return SUCCEED;
		break;
	case DBSETHID:
	default:
		tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetlname() which = %d\n", which);
		return FAIL;
		break;
	}
}

RETCODE
dbsetllong(LOGINREC * login, long value, int which)
{
	switch (which) {
	case DBSETPACKET:
		tds_set_packet(login->tds_login, value);
		return SUCCEED;
		break;
	default:
		tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetllong() which = %d\n", which);
		return FAIL;
		break;
	}
}

RETCODE
dbsetlshort(LOGINREC * login, int value, int which)
{
	switch (which) {
	case DBSETHIER:
	default:
		tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetlshort() which = %d\n", which);
		return FAIL;
		break;
	}
}

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
		tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetlbool() which = %d\n", which);
		return FAIL;
		break;

	}

}

static void
dbstring_free(DBSTRING ** dbstrp)
{
	if ((dbstrp != NULL) && (*dbstrp != NULL)) {
		if ((*dbstrp)->strnext != NULL) {
			dbstring_free(&((*dbstrp)->strnext));
		}
		free(*dbstrp);
		*dbstrp = NULL;
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
		free(*strp);
		*strp = NULL;
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
	if ((ret = (char*) malloc(len + 1)) == NULL) {
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

static const char *opttext[DBNUMOPTIONS] = {
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
	"cnv_date2char_short"
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
	return dbopts;
}

DBPROCESS *
tdsdbopen(LOGINREC * login, char *server)
{
DBPROCESS *dbproc;
TDSCONNECTINFO *connect_info;
char temp_filename[256];

	dbproc = (DBPROCESS *) malloc(sizeof(DBPROCESS));
	if (dbproc == NULL) {
		_dblib_client_msg(NULL, SYBEMEM, EXRESOURCE, "Unable to allocate sufficient memory.");
		return NULL;
	}
	memset(dbproc, '\0', sizeof(DBPROCESS));

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
	dbproc->tds_socket->env_chg_func = db_env_chg;
	dbproc->envchange_rcv = 0;
	dbproc->dbcurdb[0] = '\0';
	dbproc->servcharset[0] = '\0';

	connect_info = tds_read_config_info(NULL, login->tds_login, g_dblib_ctx.tds_ctx->locale);
	if (!connect_info)
		return NULL;

	if (g_dblib_login_timeout >= 0) {
		connect_info->connect_timeout = g_dblib_login_timeout;
	}

	if (tds_connect(dbproc->tds_socket, connect_info) == TDS_FAIL) {
		dbproc->tds_socket = NULL;
		tds_free_connect(connect_info);
		return NULL;
	}
	tds_free_connect(connect_info);
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

	if (g_dblib_ctx.recftos_filename != (char *) NULL) {
		sprintf(temp_filename, "%s.%d", g_dblib_ctx.recftos_filename, g_dblib_ctx.recftos_filenum);
		dbproc->ftos = fopen(temp_filename, "w");
		if (dbproc->ftos != (FILE *) NULL) {
			fprintf(dbproc->ftos, "/* dbopen() at %s */\n", _dbprdate());
			fflush(dbproc->ftos);
			g_dblib_ctx.recftos_filenum++;
		}
	}
	return dbproc;
}

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

RETCODE
dbcmd(DBPROCESS * dbproc, const char *cmdstring)
{
int newsz;
void *p;

	if (dbproc == NULL) {
		return FAIL;
	}

	dbproc->avail_flag = FALSE;

	tdsdump_log(TDS_DBG_FUNC, "%L inside dbcmd() bufsz = %d\n", dbproc->dbbufsz);
	if (dbproc->command_state == DBCMDSENT ) {
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

RETCODE
dbsqlexec(DBPROCESS * dbproc)
{
RETCODE rc = FAIL;
TDSSOCKET *tds;

	if (dbproc == NULL) {
		return FAIL;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L in dbsqlexec()\n");

	tds = (TDSSOCKET *) dbproc->tds_socket;
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

RETCODE
dbuse(DBPROCESS * dbproc, char *name)
{
	tdsdump_log(TDS_DBG_FUNC, "%L inside dbuse()\n");
	/* FIXME quote name if needed */
	if ((dbproc == NULL)
	    || (dbfcmd(dbproc, "use %s", name) == FAIL)
	    || (dbsqlexec(dbproc) == FAIL)
	    || (dbresults(dbproc) == FAIL)
	    || (dbcanquery(dbproc) == FAIL))
		return FAIL;
	return SUCCEED;
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

void
dbclose(DBPROCESS * dbproc)
{
TDSSOCKET *tds;
int i;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (tds) {
		buffer_free(&(dbproc->row_buf));
		tds_free_socket(tds);
	}

	if (dbproc->ftos != (FILE *) NULL) {
		fprintf(dbproc->ftos, "/* dbclose() at %s */\n", _dbprdate());
		fclose(dbproc->ftos);
	}

	if (dbproc->bcp_tablename)
		free(dbproc->bcp_tablename);
	if (dbproc->bcp_hostfile)
		free(dbproc->bcp_hostfile);
	if (dbproc->bcp_errorfile)
		free(dbproc->bcp_errorfile);
	if (dbproc->bcp_columns) {
		for (i = 0; i < dbproc->bcp_colcount; i++) {
			if (dbproc->bcp_columns[i]->data)
				free(dbproc->bcp_columns[i]->data);
			free(dbproc->bcp_columns[i]);
		}
		free(dbproc->bcp_columns);
	}
	if (dbproc->host_columns) {
		for (i = 0; i < dbproc->host_colcount; i++) {
			if (dbproc->host_columns[i]->terminator)
				free(dbproc->host_columns[i]->terminator);
			free(dbproc->host_columns[i]);
		}
		free(dbproc->host_columns);
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
			dbclose(dbproc);
		}
	}
	free(g_dblib_ctx.connection_list);
	tds_free_context(g_dblib_ctx.tds_ctx);
}

RETCODE
dbresults_r(DBPROCESS * dbproc, int recursive)
{
	RETCODE retcode = FAIL;
	TDSSOCKET *tds;
	int result_type;
	int done;

	tdsdump_log(TDS_DBG_FUNC, "%L inside dbresults_r()\n");
	if (dbproc == NULL)
		return FAIL;
	buffer_clear(&(dbproc->row_buf));

	tds = dbproc->tds_socket;
	if (IS_TDSDEAD(tds))
		return FAIL;

	done = 0;

	while (!done && (retcode = tds_process_result_tokens(tds, &result_type)) == TDS_SUCCEED) {
		tdsdump_log(TDS_DBG_FUNC, "%L inside dbresults_r() result_type = %d retcode = %d\n", result_type, retcode);
		switch (result_type) {
		case TDS_ROWFMT_RESULT:
			dbproc->dbresults_state = DBRESINIT;
			retcode = buffer_start_resultset(&(dbproc->row_buf), tds->res_info->row_size);
		case TDS_COMPUTEFMT_RESULT:
			break;

		case TDS_ROW_RESULT:
		case TDS_COMPUTE_RESULT:

			if (dbproc->dbresults_state != DBRESINIT) {
				_dblib_client_msg(dbproc, 20019, 7,
						  "Attempt to initiate a new SQL Server operation with results pending.");
				retcode = TDS_FAIL;
			}
			done = 1;
			break;

		case TDS_CMD_SUCCEED:
		case TDS_CMD_DONE:
			if (dbproc->dbresults_state == DBRESINIT) {
				done = 1;
				break;
			}
		case TDS_CMD_FAIL:
		case TDS_MSG_RESULT:
		case TDS_DESCRIBE_RESULT:
		case TDS_STATUS_RESULT:
		case TDS_PARAM_RESULT:
		default:
			break;
		}
	}

	switch (retcode) {
	case TDS_SUCCEED:
		dbproc->dbresults_state = DBRESSUCC;
		return SUCCEED;
		break;


	case TDS_NO_MORE_RESULTS:
		if (dbproc->dbresults_state == DBRESINIT) {
			dbproc->dbresults_state = DBRESSUCC;
			return SUCCEED;
		} else {
			if (dbproc->dbresults_state == DBRESSUCC) {
				dbproc->dbresults_state = DBRESDONE;
				return NO_MORE_RESULTS;
			}
		}
		break;

	case TDS_FAIL:
	default:
		break;
	}

	return FAIL;
}

/* =============================== dbresults() ===============================
 * 
 * Def: 
 * 
 * Ret:  SUCCEED, FAIL, NO_MORE_RESULTS, or NO_MORE_RPC_RESULTS
 * 
 * ===========================================================================
 */
RETCODE
dbresults(DBPROCESS * dbproc)
{
RETCODE rc;

	tdsdump_log(TDS_DBG_FUNC, "%L inside dbresults()\n");
	if (dbproc == NULL)
		return FAIL;

	rc = dbresults_r(dbproc, 0);
	tdsdump_log(TDS_DBG_FUNC, "%L leaving dbresults() returning %d\n", rc);
	return rc;
}


int
dbnumcols(DBPROCESS * dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo)
		return resinfo->num_cols;
	return 0;
}

char *
dbcolname(DBPROCESS * dbproc, int column)
{
	static char buf[255];
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return NULL;
	strcpy(buf, resinfo->columns[column - 1]->column_name);
	return buf;
}

RETCODE
dbgetrow(DBPROCESS * dbproc, DBINT row)
{
RETCODE result = FAIL;
int idx = buffer_index_of_resultset_row(&(dbproc->row_buf), row);

	if (-1 == idx) {
		result = NO_MORE_ROWS;
	} else {
		dbproc->row_buf.next_row = row;
		buffer_transfer_bound_data(TDS_REG_ROW, 0, &(dbproc->row_buf), dbproc, row);
		dbproc->row_buf.next_row++;
		result = REG_ROW;
	}

	return result;
}

RETCODE
dbnextrow(DBPROCESS * dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
RETCODE result = FAIL;
TDS_INT rowtype;
TDS_INT computeid;
TDS_INT ret;

	tdsdump_log(TDS_DBG_FUNC, "%L inside dbnextrow()\n");

	if (dbproc == NULL)
		return FAIL;
	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (IS_TDSDEAD(tds)) {
		tdsdump_log(TDS_DBG_FUNC, "%L leaving dbnextrow() returning %d\n", FAIL);
		return FAIL;
	}

	resinfo = tds->res_info;
	if (!resinfo) {
		tdsdump_log(TDS_DBG_FUNC, "%L leaving dbnextrow() returning %d\n", NO_MORE_ROWS);
		return NO_MORE_ROWS;
	}

	if (dbproc->row_buf.buffering_on && buffer_is_full(&(dbproc->row_buf))
	    && (-1 == buffer_index_of_resultset_row(&(dbproc->row_buf), dbproc->row_buf.next_row))) {
		result = BUF_FULL;
	} else {
		/*
		 * Now try to get the dbproc->row_buf.next_row item into the row
		 * buffer
		 */
		if (-1 != buffer_index_of_resultset_row(&(dbproc->row_buf), dbproc->row_buf.next_row)) {
			/*
			 * Cool, the item we want is already there
			 */
			result = REG_ROW;
			rowtype = TDS_REG_ROW;
		} else {

			/* Get the row from the TDS stream.  */

			if ((ret = tds_process_row_tokens(dbproc->tds_socket, &rowtype, &computeid)) == TDS_SUCCEED) {
				if (rowtype == TDS_REG_ROW) {
					/* Add the row to the row buffer */

					resinfo = tds->curr_resinfo;
					buffer_add_row(&(dbproc->row_buf), resinfo->current_row, resinfo->row_size);
					result = REG_ROW;
				} else if (rowtype == TDS_COMP_ROW) {
					/* Add the row to the row buffer */

					resinfo = tds->curr_resinfo;
					buffer_add_row(&(dbproc->row_buf), resinfo->current_row, resinfo->row_size);
					result = computeid;
				} else
					result = FAIL;
			} else if (ret == TDS_NO_MORE_ROWS) {
				result = NO_MORE_ROWS;
			} else
				result = FAIL;
		}

		if (rowtype == TDS_REG_ROW || rowtype == TDS_COMP_ROW) {
			/*
			 * The data is in the row buffer, now transfer it to the 
			 * bound variables
			 */
			buffer_transfer_bound_data(rowtype, computeid, &(dbproc->row_buf), dbproc, dbproc->row_buf.next_row);
			dbproc->row_buf.next_row++;
		}
	}
	tdsdump_log(TDS_DBG_FUNC, "%L leaving dbnextrow() returning %d\n", result);
	return result;
}				/* dbnextrow()  */

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
 * Conversion functions are handled in the TDS layer.
 * 
 * The main reason for this is that ctlib and ODBC (and presumably DBI) need
 * to be able to do conversions between datatypes. This is possible because
 * the format of complex data (dates, money, numeric, decimal) is defined by
 * its representation on the wire; thus what we call DBMONEY is exactly its
 * format on the wire. CLIs that need a different representation (ODBC?) 
 * need to convert from this format anyway, so the code would already be in
 * place.
 * 
 * Each datatype is also defined by its Server-type so all CLIs should be 
 * able to map native types to server types as well.
 *
 * tds_convert copies from src to dest and returns the output data length,
 * period.  All padding and termination is the responsibility of the API library
 * and is done post-conversion.  The peculiar rule in dbconvert() is that
 * a destlen of -1 and a desttype of SYBCHAR means the output buffer
 * should be null-terminated.  
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

	tdsdump_log(TDS_DBG_INFO1, "%L inside dbconvert() srctype = %d desttype = %d\n", srctype, desttype);

	if (dbproc) {
		tds = (TDSSOCKET *) dbproc->tds_socket;
	}

	if (src == NULL || srclen == 0) {

		/* FIX set appropriate NULL value for destination type */
		if (destlen > 0)
			memset(dest, '\0', destlen);
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

		tdsdump_log(TDS_DBG_INFO1, "%L inside dbconvert() srctype == desttype\n");
		switch (desttype) {

		case SYBBINARY:
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

			if (destlen == 0 || destlen < -2) {
				ret = FAIL;
			} else if (destlen == -1) {	/* rtrim and null terminate */
				for (i = srclen - 1; i >= 0 && src[i] == ' '; --i) {
					srclen = i;
				}
				memcpy(dest, src, srclen);
				dest[srclen] = '\0';
				ret = srclen;
			} else if (destlen == -2) {	/* just null terminate */
				memcpy(dest, src, srclen);
				dest[srclen] = '\0';
				ret = srclen;
			} else {	/* destlen is > 0 */
				if (srclen > destlen) {
					_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
					ret = -1;
				} else {
					memcpy(dest, src, srclen);
					for (i = srclen; i < destlen; i++)
						dest[i] = ' ';
					ret = srclen;
				}
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
		return ret;
	}



	/* srctype == desttype */
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

	tdsdump_log(TDS_DBG_INFO1, "%L inside dbconvert() calling tds_convert\n");

	len = tds_convert(g_dblib_ctx.tds_ctx, srctype, (const TDS_CHAR *) src, srclen, desttype, &dres);
	tdsdump_log(TDS_DBG_INFO1, "%L inside dbconvert() called tds_convert returned %d\n", len);

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
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:

		tdsdump_log(TDS_DBG_INFO1, "%L inside dbconvert() outputting %d bytes character data destlen = %d \n", len,
			    destlen);
		if (destlen == 0 || destlen < -2) {
			ret = FAIL;
		} else if (destlen == -1) {	/* rtrim and null terminate */
			for (i = len - 1; i >= 0 && dres.c[i] == ' '; --i) {
				len = i;
			}
			memcpy(dest, dres.c, len);
			dest[len] = '\0';
			ret = len;
		} else if (destlen == -2) {	/* just null terminate */
			memcpy(dest, dres.c, len);
			dest[len] = 0;
			ret = len;
		} else {	/* destlen is > 0 */
			if (len > destlen) {
				_dblib_client_msg(NULL, SYBECOFL, EXCONVERSION, "Data-conversion resulted in overflow.");
				ret = -1;
			} else
				memcpy(dest, dres.c, len);
			for (i = len; i < destlen; i++)
				dest[i] = ' ';
			ret = len;
		}

		free(dres.c);

		break;
	default:
		ret = -1;
		break;

	}
	return (ret);
}

DBINT
dbconvert_ps(DBPROCESS * dbproc,
	     int srctype, BYTE * src, DBINT srclen, int desttype, BYTE * dest, DBINT destlen, DBTYPEINFO * typeinfo)
{
DBNUMERIC *s;
DBNUMERIC *d;

	if (is_numeric_type(desttype)) {
		if (typeinfo == (DBTYPEINFO *) NULL) {
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

RETCODE
dbbind(DBPROCESS * dbproc, int column, int vartype, DBINT varlen, BYTE * varaddr)
{
TDSCOLINFO *colinfo = NULL;
TDSRESULTINFO *resinfo = NULL;
TDSSOCKET *tds = NULL;
int srctype = -1;
int desttype = -1;
int okay = TRUE;		/* so far, so good */
TDS_SMALLINT num_cols = 0;

	tdsdump_log(TDS_DBG_INFO1, "%L dbbind() column = %d %d %d\n", column, vartype, varlen);
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
		tds = (TDSSOCKET *) dbproc->tds_socket;
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

		tdsdump_log(TDS_DBG_INFO1, "%L dbbind() srctype = %d desttype = %d \n", srctype, desttype);

		okay = okay && dbwillconvert(srctype, _db_get_server_type(vartype));
	}

	if (okay) {
		colinfo->column_varaddr = (char *) varaddr;
		colinfo->column_bindtype = vartype;
		colinfo->column_bindlen = varlen;
	}

	return okay ? SUCCEED : FAIL;
}				/* dbbind()  */

void
dbsetifile(char *filename)
{
	tds_set_interfaces_file_loc(filename);
}

RETCODE
dbnullbind(DBPROCESS * dbproc, int column, DBINT * indicator)
{
	TDSCOLINFO *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	/*
	 *  XXX Need to check for possibly problems before assuming
	 *  everything is okay
	 */
	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	colinfo->column_nullbind = (TDS_CHAR *) indicator;

	return SUCCEED;
}

RETCODE
dbanullbind(DBPROCESS * dbproc, int computeid, int column, DBINT * indicator)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
TDSCOMPUTEINFO *info;
TDSCOLINFO *curcol;
TDS_SMALLINT compute_id;
int i;

	compute_id = computeid;
	tdsdump_log(TDS_DBG_FUNC, "%L in dbanullbind(%d,%d)\n", compute_id, column);

	tdsdump_log(TDS_DBG_FUNC, "%L in dbanullbind() num_comp_info = %d\n", tds->num_comp_info);
	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return FAIL;
		info = tds->comp_info[i];
		tdsdump_log(TDS_DBG_FUNC, "%L in dbanullbind() found computeid = %d\n", info->computeid);
		if (info->computeid == compute_id)
			break;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L in dbanullbind() num_cols = %d\n", info->num_cols);

	if (column < 1 || column > info->num_cols)
		return FAIL;

	curcol = info->columns[column - 1];
	/*
	 *  XXX Need to check for possibly problems before assuming
	 *  everything is okay
	 */
	curcol->column_nullbind = (TDS_CHAR *) indicator;

	return SUCCEED;
}

DBINT
dbcount(DBPROCESS * dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo)
		return resinfo->row_count;
	else
		return tds->rows_affected;
}

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

DBBOOL
dbwillconvert(int srctype, int desttype)
{
	return tds_willconvert(srctype, desttype);
}

int
dbcoltype(DBPROCESS * dbproc, int column)
{
	TDSCOLINFO *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
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

int
dbcolutype(DBPROCESS * dbproc, int column)
{
	TDSCOLINFO *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	return colinfo->column_usertype;
}

DBTYPEINFO *
dbcoltypeinfo(DBPROCESS * dbproc, int column)
{
/* moved typeinfo from static into dbproc structure to make thread safe. 
	(mlilback 11/7/01) */
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[column - 1];
	dbproc->typeinfo.precision = colinfo->column_prec;
	dbproc->typeinfo.scale = colinfo->column_scale;
	return &dbproc->typeinfo;
}

char *
dbcolsource(DBPROCESS * dbproc, int colnum)
{
	TDSCOLINFO *colinfo;
	TDSRESULTINFO *resinfo;
	TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	colinfo = resinfo->columns[colnum - 1];
	return colinfo->column_name;
}

DBINT
dbcollen(DBPROCESS * dbproc, int column)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return -1;
	colinfo = resinfo->columns[column - 1];
	return colinfo->column_size;
}

/* dbvarylen(), pkleef@openlinksw.com 01/21/02 */
DBINT
dbvarylen(DBPROCESS * dbproc, int column)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return FALSE;
	colinfo = resinfo->columns[column - 1];

	if (tds_get_null(resinfo->current_row, column))
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

DBINT
dbdatlen(DBPROCESS * dbproc, int column)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
DBINT ret;

	/* FIXME -- this is the columns info, need per row info */
	/* Fixed by adding cur_row_size to colinfo, filled in by process_row
	 * in token.c. (mlilback, 11/7/01) */
	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return -1;
	colinfo = resinfo->columns[column - 1];
	tdsdump_log(TDS_DBG_INFO1, "%L dbdatlen() type = %d\n", colinfo->column_type);

	if (tds_get_null(resinfo->current_row, column - 1))
		ret = 0;
	else
		ret = colinfo->column_cur_size;
	tdsdump_log(TDS_DBG_FUNC, "%L leaving dbdatlen() returning %d\n", ret);
	return ret;
}

BYTE *
dbdata(DBPROCESS * dbproc, int column)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (column < 1 || column > resinfo->num_cols)
		return NULL;

	colinfo = resinfo->columns[column - 1];
	if (tds_get_null(resinfo->current_row, column - 1)) {
		return NULL;
	}
	if (is_blob_type(colinfo->column_type)) {
		return (BYTE*) ((TDSBLOBINFO *) (resinfo->current_row + colinfo->column_offset))->textvalue;
	}

	return (BYTE*) &resinfo->current_row[colinfo->column_offset];
}

RETCODE
dbcancel(DBPROCESS * dbproc)
{
int marker;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;

	tds_send_cancel(dbproc->tds_socket);
	tds_process_cancel(dbproc->tds_socket);

	return SUCCEED;
}

DBINT
dbspr1rowlen(DBPROCESS * dbproc)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
int col, len = 0, collen, namlen;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		len += collen > namlen ? collen : namlen;
	}
	/* the space between each column */
	len += (resinfo->num_cols - 1) * dbstring_length(dbproc->dbopts[DBPRCOLSEP].optparam);
	/* the newline */
	len += dbstring_length(dbproc->dbopts[DBPRLINESEP].optparam);

	return len;
}

RETCODE
dbspr1row(DBPROCESS * dbproc, char *buffer, DBINT buf_len)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
TDSDATEREC when;
int i, col, collen, namlen;
int desttype, srctype;
int padlen;
DBINT len;
int c;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	if (dbnextrow(dbproc) != REG_ROW)
		return FAIL;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		if (tds_get_null(resinfo->current_row, col)) {
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
		namlen = strlen(colinfo->column_name);
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

RETCODE
dbprrow(DBPROCESS * dbproc)
{
TDSCOLINFO *colinfo;
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

	tds = (TDSSOCKET *) dbproc->tds_socket;

	while ((status = dbnextrow(dbproc)) != NO_MORE_ROWS) {

		if (status == FAIL) {
			return FAIL;
		}

		if (status == REG_ROW) {

			resinfo = tds->res_info;

			if (col_printlens == NULL) {
				col_printlens = (TDS_SMALLINT*) malloc(sizeof(TDS_SMALLINT) * resinfo->num_cols);
			}

			for (col = 0; col < resinfo->num_cols; col++) {
				colinfo = resinfo->columns[col];
				if (tds_get_null(resinfo->current_row, col)) {
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
				namlen = strlen(colinfo->column_name);
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
			tdsdump_log(TDS_DBG_FUNC, "%L dbprrow num compute cols = %d\n", num_cols);

			i = 0;
			while ((c = dbstring_getchar(dbproc->dbopts[DBPRLINESEP].optparam, i)) != -1) {
				putchar(c);
				i++;
			}
			for (selcol = col = 1; col <= num_cols; col++) {
				tdsdump_log(TDS_DBG_FUNC, "%L dbprrow calling dbaltcolid(%d,%d)\n", computeid, col);
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
				tdsdump_log(TDS_DBG_FUNC, "%L dbprrow calling dbaltcolid(%d,%d)\n", computeid, col);
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

				tdsdump_log(TDS_DBG_FUNC, "%L dbprrow calling dbaltcolid(%d,%d)\n", computeid, col);
				colid = dbaltcolid(dbproc, computeid, col);
				tdsdump_log(TDS_DBG_FUNC, "%L dbprrow select column = %d\n", colid);

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
				namlen = strlen(colinfo->column_name);
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
_get_printable_size(TDSCOLINFO * colinfo)
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

RETCODE
dbsprline(DBPROCESS * dbproc, char *buffer, DBINT buf_len, DBCHAR line_char)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
int i, col, len, collen, namlen;
int c;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
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

RETCODE
dbsprhead(DBPROCESS * dbproc, char *buffer, DBINT buf_len)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
int i, col, collen, namlen;
int padlen;
int c;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;

	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
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

void
dbprhead(DBPROCESS * dbproc)
{
TDSCOLINFO *colinfo;
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;
int i, col, len, collen, namlen;
int padlen;
int c;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo == NULL) {
		return;
	}
	for (col = 0; col < resinfo->num_cols; col++) {
		colinfo = resinfo->columns[col];
		collen = _get_printable_size(colinfo);
		namlen = strlen(colinfo->column_name);
		padlen = (collen > namlen ? collen : namlen) - namlen;
		printf("%s", colinfo->column_name);

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
		namlen = strlen(colinfo->column_name);
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

RETCODE
dbrows(DBPROCESS * dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	if (resinfo && resinfo->rows_exist)
		return SUCCEED;
	else
		return FAIL;
}

RETCODE
dbsetdeflang(char *language)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetdeflang()\n");
	return SUCCEED;
}

int
dbgetpacket(DBPROCESS * dbproc)
{
TDSSOCKET *tds = dbproc->tds_socket;

	if (!tds || !tds->env) {
		return TDS_DEF_BLKSZ;
	} else {
		return tds->env->block_size;
	}
}

RETCODE
dbsetmaxprocs(int maxprocs)
{
	int i;
	TDSSOCKET **old_list = g_dblib_ctx.connection_list;

	tdsdump_log(TDS_DBG_FUNC, "%L UNTESTED dbsetmaxprocs()\n");
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

	g_dblib_ctx.connection_list = (TDSSOCKET**) calloc(maxprocs, sizeof(TDSSOCKET*));

	if (g_dblib_ctx.connection_list == NULL) {
		g_dblib_ctx.connection_list = old_list;
		return FAIL;
	}

	for (i=0; i < g_dblib_ctx.connection_list_size; i++) {
		g_dblib_ctx.connection_list[i] = old_list[i];
	}

	g_dblib_ctx.connection_list_size = maxprocs;
	g_dblib_ctx.connection_list_size_represented = maxprocs;

	return SUCCEED;
}

int
dbgetmaxprocs(void)
{
	return g_dblib_ctx.connection_list_size_represented;
}

RETCODE
dbsettime(int seconds)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsettime()\n");
	return SUCCEED;
}

RETCODE
dbsetlogintime(int seconds)
{
	g_dblib_login_timeout = seconds;
	return SUCCEED;
}

DBBOOL
dbhasretstat(DBPROCESS * dbproc)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;

	if (tds->has_status) {
		return TRUE;
	} else {
		return FALSE;
	}
}

DBINT
dbretstatus(DBPROCESS * dbproc)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;

	return tds->ret_status;
}

RETCODE
dbcmdrow(DBPROCESS * dbproc)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;

	if (tds->res_info)
		return SUCCEED;
	return TDS_FAIL;
}

int
dbaltcolid(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLINFO *curcol;
	TDS_SMALLINT compute_id;
	int i;

	compute_id = computeid;
	tdsdump_log(TDS_DBG_FUNC, "%L in dbaltcolid(%d,%d)\n", compute_id, column);

	tdsdump_log(TDS_DBG_FUNC, "%L in dbaltcolid() num_comp_info = %d\n", tds->num_comp_info);
	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return -1;
		info = tds->comp_info[i];
		tdsdump_log(TDS_DBG_FUNC, "%L in dbaltcolid() found computeid = %d\n", info->computeid);
		if (info->computeid == compute_id)
			break;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L in dbaltcolid() num_cols = %d\n", info->num_cols);

	if (column < 1 || column > info->num_cols)
		return -1;

	curcol = info->columns[column - 1];

	return curcol->column_operand;

}

DBINT
dbadlen(DBPROCESS * dbproc, int computeid, int column)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
TDSCOMPUTEINFO *info;
TDSCOLINFO *colinfo;
TDS_SMALLINT compute_id;
int i;
DBINT ret;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbadlen()\n");
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
	tdsdump_log(TDS_DBG_INFO1, "%L dbadlen() type = %d\n", colinfo->column_type);

	if (tds_get_null(info->current_row, column - 1))
		ret = 0;
	else
		ret = colinfo->column_cur_size;
	tdsdump_log(TDS_DBG_FUNC, "%L leaving dbadlen() returning %d\n", ret);

	return ret;

}

int
dbalttype(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLINFO *colinfo;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbalttype()\n");
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
		if (colinfo->column_size == 4)
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

RETCODE
dbaltbind(DBPROCESS * dbproc, int computeid, int column, int vartype, DBINT varlen, BYTE * varaddr)
{
TDSSOCKET *tds = NULL;
TDSCOMPUTEINFO *info;
TDSCOLINFO *colinfo = NULL;

TDS_SMALLINT compute_id;

int srctype = -1;
int desttype = -1;
int i;

	tdsdump_log(TDS_DBG_INFO1, "%L dbaltbind() compteid %d column = %d %d %d\n", computeid, column, vartype, varlen);

	dbproc->avail_flag = FALSE;

	compute_id = computeid;

	if (dbproc == NULL || dbproc->tds_socket == NULL || varaddr == NULL)
		goto Failed;

	tds = (TDSSOCKET *) dbproc->tds_socket;
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

	tdsdump_log(TDS_DBG_INFO1, "%L dbaltbind() srctype = %d desttype = %d \n", srctype, desttype);

	if (!dbwillconvert(srctype, _db_get_server_type(vartype)))
		goto Failed;

	colinfo->column_varaddr = (char *) varaddr;
	colinfo->column_bindtype = vartype;
	colinfo->column_bindlen = varlen;

	return SUCCEED;
      Failed:
	return FAIL;
}				/* dbaltbind()  */


BYTE *
dbadata(DBPROCESS * dbproc, int computeid, int column)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
TDSCOMPUTEINFO *info;
TDSCOLINFO *colinfo;
TDS_SMALLINT compute_id;
int i;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbadata()\n");
	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info)
			return (BYTE *) NULL;
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	/* if either the compute id or the column number are invalid, return -1 */
	if (column < 1 || column > info->num_cols)
		return (BYTE *) NULL;

	colinfo = info->columns[column - 1];

#if 0
	if (tds_get_null(info->current_row, column - 1)) {
		return (BYTE *) NULL;
	}
#endif

	if (is_blob_type(colinfo->column_type)) {
		return (BYTE*) ((TDSBLOBINFO *) (info->current_row + colinfo->column_offset))->textvalue;
	}

	return (BYTE*) &info->current_row[colinfo->column_offset];
}

int
dbaltop(DBPROCESS * dbproc, int computeid, int column)
{
	TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
	TDSCOMPUTEINFO *info;
	TDSCOLINFO *curcol;
	TDS_SMALLINT compute_id;
	int i;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbaltop()\n");
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
		break;
	case DBTEXTSIZE:
		/* server option */
		/* requires param "0" to "2147483647" */
		break;
	case DBAUTH:
		/* ??? */
		break;
	case DBNOAUTOFREE:
		/* dblib option */
		break;
	case DBBUFFER:
		/* dblib option */
		/* requires param "0" to "2147483647" */
		/* XXX should be more robust than just a atoi() */
		buffer_set_buffering(&(dbproc->row_buf), atoi(char_param));
		return SUCCEED;
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
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetopt(option = %d)\n", option);
	return FAIL;
}

void
dbsetinterrupt(DBPROCESS * dbproc, DB_DBCHKINTR_FUNC ckintr, DB_DBHNDLINTR_FUNC hndlintr)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetinterrupt()\n");
}

int
dbnumrets(DBPROCESS * dbproc)
{
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;

	if (!tds->param_info)
		return 0;

	return tds->param_info->num_cols;
}

char *
dbretname(DBPROCESS * dbproc, int retnum)
{
	TDSSOCKET *tds;
	TDSPARAMINFO *param_info;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	param_info = tds->param_info;
	if (retnum < 1 || retnum > param_info->num_cols)
		return NULL;
	return param_info->columns[retnum - 1]->column_name;
}

BYTE *
dbretdata(DBPROCESS * dbproc, int retnum)
{
TDSCOLINFO *colinfo;
TDSPARAMINFO *param_info;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	param_info = tds->param_info;
	if (retnum < 1 || retnum > param_info->num_cols)
		return NULL;

	colinfo = param_info->columns[retnum - 1];
	/* FIXME blob are stored is different way */
	return &param_info->current_row[colinfo->column_offset];
}

int
dbretlen(DBPROCESS * dbproc, int retnum)
{
	TDSCOLINFO *colinfo;
	TDSPARAMINFO *param_info;
	TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	param_info = tds->param_info;
	if (retnum < 1 || retnum > param_info->num_cols)
		return -1;

	colinfo = param_info->columns[retnum - 1];

	return colinfo->column_cur_size;
}

RETCODE
dbsqlok(DBPROCESS * dbproc)
{
TDSSOCKET *tds;

unsigned char marker;
int done = 0;
int done_flags = 0;

RETCODE rc = SUCCEED;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbsqlok() \n");
	tds = (TDSSOCKET *) dbproc->tds_socket;

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

		marker = tds_get_byte(tds);
		tdsdump_log(TDS_DBG_FUNC, "%L dbsqlok() marker is %x\n", marker);

		/* If we hit a result token, then we know  */
		/* everything is fine with the command...  */

		if (is_result_token(marker)) {
			tdsdump_log(TDS_DBG_FUNC, "%L dbsqlok() found result token\n");
			tds_unget_byte(tds);
			done = 1;
			rc = SUCCEED;
			break;
		}

		/* if we hit an end token, for example if the command */
		/* submitted returned no data (like an insert), then  */
		/* we have to process the end token to extract the    */
		/* status code therein....but....                     */

		else if (is_end_token(marker)) {

			tdsdump_log(TDS_DBG_FUNC, "%L dbsqlok() found end token\n");
			if (tds_process_end(tds, marker, &done_flags) != TDS_SUCCEED) {
				tdsdump_log(TDS_DBG_FUNC, "%L dbsqlok() end status was error\n");
				rc = FAIL;
			} else {
				tdsdump_log(TDS_DBG_FUNC, "%L dbsqlok() end status was success\n");
				rc = SUCCEED;
			}
			done = 1;

		} else {
			tdsdump_log(TDS_DBG_FUNC, "%L dbsqlok() found throwaway token\n");
			tds_process_default_tokens(tds, marker);
		}
	}

	return rc;
}

int
dbnumalts(DBPROCESS * dbproc, int computeid)
{
	TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
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

int
dbnumcompute(DBPROCESS * dbproc)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;

	return tds->num_comp_info;
}


BYTE *
dbbylist(DBPROCESS * dbproc, int computeid, int *size)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
TDSCOMPUTEINFO *info;
TDS_SMALLINT compute_id;
int i;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbbylist() \n");

	compute_id = computeid;

	for (i = 0;; ++i) {
		if (i >= tds->num_comp_info) {
			if (size)
				*size = 0;
			return (BYTE *) NULL;
		}
		info = tds->comp_info[i];
		if (info->computeid == compute_id)
			break;
	}

	if (size)
		*size = info->by_cols;
	return info->bycolumns;
}

DBBOOL
dbdead(DBPROCESS * dbproc)
{
	if ((dbproc == NULL) || IS_TDSDEAD(dbproc->tds_socket))
		return TRUE;
	else
		return FALSE;
}

EHANDLEFUNC
dberrhandle(EHANDLEFUNC handler)
{
EHANDLEFUNC retFun = _dblib_err_handler;

	_dblib_err_handler = handler;
	return retFun;
}

MHANDLEFUNC
dbmsghandle(MHANDLEFUNC handler)
{
MHANDLEFUNC retFun = _dblib_msg_handler;

	_dblib_msg_handler = handler;
	return retFun;
}

RETCODE
dbmnyadd(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * sum)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyadd()\n");
	return SUCCEED;
}

RETCODE
dbmnysub(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * diff)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnysyb()\n");
	return SUCCEED;
}

RETCODE
dbmnymul(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * prod)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnymul()\n");
	return SUCCEED;
}

RETCODE
dbmnydivide(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2, DBMONEY * quotient)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnydivide()\n");
	return SUCCEED;
}

int
dbmnycmp(DBPROCESS * dbproc, DBMONEY * m1, DBMONEY * m2)
{

	if (m1->tdsoldmoney.mnyhigh < m2->tdsoldmoney.mnyhigh) {
		return -1;
	}
	if (m1->tdsoldmoney.mnyhigh > m2->tdsoldmoney.mnyhigh) {
		return 1;
	}
	if (m1->tdsoldmoney.mnylow < m2->tdsoldmoney.mnylow) {
		return -1;
	}
	if (m1->tdsoldmoney.mnylow > m2->tdsoldmoney.mnylow) {
		return 1;
	}
	return 0;
}

RETCODE
dbmnyscale(DBPROCESS * dbproc, DBMONEY * dest, int multiplier, int addend)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyscale()\n");
	return SUCCEED;
}

RETCODE
dbmnyzero(DBPROCESS * dbproc, DBMONEY * dest)
{

	if (dest == NULL) {
		return FAIL;
	}
	dest->tdsoldmoney.mnylow = 0;
	dest->tdsoldmoney.mnyhigh = 0;
	return SUCCEED;
}

RETCODE
dbmnymaxpos(DBPROCESS * dbproc, DBMONEY * dest)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnymaxpos()\n");
	return SUCCEED;
}

RETCODE
dbmnymaxneg(DBPROCESS * dbproc, DBMONEY * dest)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnymaxneg()\n");
	return SUCCEED;
}

RETCODE
dbmnyndigit(DBPROCESS * dbproc, DBMONEY * mnyptr, DBCHAR * value, DBBOOL * zero)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyndigit()\n");
	return SUCCEED;
}

RETCODE
dbmnyinit(DBPROCESS * dbproc, DBMONEY * mnyptr, int trim, DBBOOL * negative)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyinit()\n");
	return SUCCEED;
}

RETCODE
dbmnydown(DBPROCESS * dbproc, DBMONEY * mnyptr, int divisor, int *remainder)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnydown()\n");
	return SUCCEED;
}

RETCODE
dbmnyinc(DBPROCESS * dbproc, DBMONEY * mnyptr)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyinc()\n");
	return SUCCEED;
}

RETCODE
dbmnydec(DBPROCESS * dbproc, DBMONEY * mnyptr)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnydec()\n");
	return SUCCEED;
}

RETCODE
dbmnyminus(DBPROCESS * dbproc, DBMONEY * src, DBMONEY * dest)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmnyminus()\n");
	return SUCCEED;
}

RETCODE
dbmny4minus(DBPROCESS * dbproc, DBMONEY4 * src, DBMONEY4 * dest)
{
DBMONEY4 zero;

	dbmny4zero(dbproc, &zero);
	return (dbmny4sub(dbproc, &zero, src, dest));
}

RETCODE
dbmny4zero(DBPROCESS * dbproc, DBMONEY4 * dest)
{

	if (dest == NULL) {
		return FAIL;
	}
	dest->mny4 = 0;
	return SUCCEED;
}

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

RETCODE
dbmny4mul(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2, DBMONEY4 * prod)
{

	if ((m1 == NULL) || (m2 == NULL) || (prod == NULL)) {
		return FAIL;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4mul()\n");
	return FAIL;
}

RETCODE
dbmny4divide(DBPROCESS * dbproc, DBMONEY4 * m1, DBMONEY4 * m2, DBMONEY4 * quotient)
{

	if ((m1 == NULL) || (m2 == NULL) || (quotient == NULL)) {
		return FAIL;
	}
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmny4divide()\n");
	return FAIL;
}

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

RETCODE
dbmny4copy(DBPROCESS * dbproc, DBMONEY4 * src, DBMONEY4 * dest)
{

	if ((src == NULL) || (dest == NULL)) {
		return FAIL;
	}
	dest->mny4 = src->mny4;
	return SUCCEED;
}

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

RETCODE
dbdatecrack(DBPROCESS * dbproc, DBDATEREC * di, DBDATETIME * dt)
{
TDSDATEREC dr;

	tds_datecrack(SYBDATETIME, dt, &dr);

#ifndef MSDBLIB
	di->dateyear = dr.year;
	di->datemonth = dr.month;
	di->datedmonth = dr.day;
	di->datedyear = dr.dayofyear;
	di->datedweek = dr.weekday;
	di->datehour = dr.hour;
	di->dateminute = dr.minute;
	di->datesecond = dr.second;
	di->datemsecond = dr.millisecond;
#else
	di->year = dr.year;
	di->month = dr.month + 1;
	di->day = dr.day;
	di->dayofyear = dr.dayofyear;
	di->weekday = dr.weekday + 1;
	di->hour = dr.hour;
	di->minute = dr.minute;
	di->second = dr.second;
	di->millisecond = dr.millisecond;
#endif
	return SUCCEED;
}

void
dbrpwclr(LOGINREC * login)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrpwclr()\n");
}

RETCODE
dbrpwset(LOGINREC * login, char *srvname, char *password, int pwlen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrpwset()\n");
	return SUCCEED;
}

int
dbspid(DBPROCESS * dbproc)
{
TDSSOCKET *tds;

	if (dbproc == NULL) {
		_dblib_client_msg(dbproc, SYBESPID, EXPROGRAM, "Called dbspid() with a NULL dbproc.");
		return FAIL;
	}
	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (IS_TDSDEAD(tds))
		return FAIL;

	return tds->spid;
}

void
dbsetuserdata(DBPROCESS * dbproc, BYTE * ptr)
{
	dbproc->user_data = ptr;
	return;
}

BYTE *
dbgetuserdata(DBPROCESS * dbproc)
{
	return dbproc->user_data;
}

RETCODE
dbsetversion(DBINT version)
{
	g_dblib_version = version;
	return SUCCEED;
}

RETCODE
dbmnycopy(DBPROCESS * dbproc, DBMONEY * src, DBMONEY * dest)
{
	if ((src == NULL) || (dest == NULL)) {
		return FAIL;
	}
	dest->tdsoldmoney.mnylow = src->tdsoldmoney.mnylow;
	dest->tdsoldmoney.mnyhigh = src->tdsoldmoney.mnyhigh;
	return SUCCEED;
}

RETCODE
dbcanquery(DBPROCESS * dbproc)
{
TDSSOCKET *tds;
int rc;
TDS_INT rowtype;
TDS_INT computeid;

	if (dbproc == NULL)
		return FAIL;
	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (IS_TDSDEAD(tds))
		return FAIL;

	/* Just throw away all pending rows from the last query */

	while ((rc = tds_process_row_tokens(dbproc->tds_socket, &rowtype, &computeid)) == TDS_SUCCEED);

	if (rc == TDS_FAIL)
		return FAIL;

	return SUCCEED;
}

void
dbfreebuf(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L in dbfreebuf()\n");
	if (dbproc->dbbuf) {
		free(dbproc->dbbuf);
		dbproc->dbbuf = NULL;
	}
	dbproc->dbbufsz = 0;
}				/* dbfreebuf()  */

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
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbclropt(option = %d)\n", option);
	return FAIL;
}

DBBOOL
dbisopt(DBPROCESS * dbproc, int option, char *param)
{
	if ((option < 0) || (option >= DBNUMOPTIONS)) {
		return FALSE;
	}
	return dbproc->dbopts[option].optactive;
}

DBINT
dbcurrow(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbcurrow()\n");
	return 0;
}

STATUS
dbrowtype(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbrowtype()\n");
	return NO_MORE_ROWS;
}

int
dbcurcmd(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbcurcmd()\n");
	return 0;
}

RETCODE
dbmorecmds(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbmorecmds()\n");
	return SUCCEED;
}

int
dbrettype(DBPROCESS * dbproc, int retnum)
{
	TDSCOLINFO *colinfo;
	TDSPARAMINFO *param_info;
	TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	param_info = tds->param_info;
	if (retnum < 1 || retnum > param_info->num_cols)
		return -1;

	colinfo = param_info->columns[retnum - 1];

	return tds_get_conversion_type(colinfo->column_type, colinfo->column_size);
}

int
dbstrlen(DBPROCESS * dbproc)
{
	return dbproc->dbbufsz;
}

char *
dbgetchar(DBPROCESS * dbproc, int pos)
{
	tdsdump_log(TDS_DBG_FUNC, "%L in dbgetchar() bufsz = %d, pos = %d\n", dbproc->dbbufsz, pos);
	if (dbproc->dbbufsz > 0) {
		if (pos >= 0 && pos < dbproc->dbbufsz)
			return (char *) &dbproc->dbbuf[pos];
		else
			return (char *) NULL;
	} else
		return (char *) NULL;
}

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

const char *
dbprtype(int token)
{

	return tds_prtype(token);

}

DBBINARY *
dbtxtimestamp(DBPROCESS * dbproc, int column)
{
TDSSOCKET *tds;
TDSRESULTINFO *resinfo;
TDSBLOBINFO *blob_info;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (!tds->res_info)
		return NULL;
	resinfo = tds->res_info;
	--column;
	if (column < 0 || column >= resinfo->num_cols)
		return NULL;
	if (!is_blob_type(resinfo->columns[column]->column_type))
		return NULL;
	blob_info = (TDSBLOBINFO *) & (resinfo->current_row[resinfo->columns[column]->column_offset]);
	return (DBBINARY *) blob_info->timestamp;
}

DBBINARY *
dbtxptr(DBPROCESS * dbproc, int column)
{
TDSSOCKET *tds;
TDSRESULTINFO *resinfo;
TDSBLOBINFO *blob_info;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	if (!tds->res_info)
		return NULL;
	resinfo = tds->res_info;
	--column;
	if (column < 0 || column >= resinfo->num_cols)
		return NULL;
	if (!is_blob_type(resinfo->columns[column]->column_type))
		return NULL;
	blob_info = (TDSBLOBINFO *) & (resinfo->current_row[resinfo->columns[column]->column_offset]);
	return (DBBINARY *) blob_info->textptr;
}

RETCODE
dbwritetext(DBPROCESS * dbproc, char *objname, DBBINARY * textptr, DBTINYINT textptrlen, DBBINARY * timestamp, DBBOOL log,
	    DBINT size, BYTE * text)
{
char textptr_string[35];	/* 16 * 2 + 2 (0x) + 1 */
char timestamp_string[19];	/* 8 * 2 + 2 (0x) + 1 */
int marker;

	if (IS_TDSDEAD(dbproc->tds_socket))
		return FAIL;

	if (textptrlen > DBTXPLEN)
		return FAIL;

	dbconvert(dbproc, SYBBINARY, (BYTE *) textptr, textptrlen, SYBCHAR, (BYTE*) textptr_string, -1);
	dbconvert(dbproc, SYBBINARY, (BYTE *) timestamp, 8, SYBCHAR, (BYTE*) timestamp_string, -1);

	dbproc->dbresults_state = DBRESINIT;

	if (tds_submit_queryf(dbproc->tds_socket,
			      "writetext bulk %s 0x%s timestamp = 0x%s %s",
			      objname, textptr_string, timestamp_string, ((log == TRUE) ? "with log" : ""))
	    != TDS_SUCCEED) {
		return FAIL;
	}

	/* read the end token */
	marker = tds_get_byte(dbproc->tds_socket);

	if (marker != TDS_DONE_TOKEN) {
		return FAIL;
	}

	if (tds_process_end(dbproc->tds_socket, marker, NULL) != TDS_SUCCEED) {
		return FAIL;
	}

	dbproc->tds_socket->out_flag = 0x07;
	tds_put_int(dbproc->tds_socket, size);

	if (!text) {
		dbproc->text_size = size;
		dbproc->text_sent = 0;
		return SUCCEED;
	}

	tds_put_bulk_data(dbproc->tds_socket, text, size);
	tds_flush_packet(dbproc->tds_socket);

	if (dbsqlok(dbproc) == SUCCEED) {
		if (dbresults(dbproc) == FAIL)
			return FAIL;
		else
			return SUCCEED;
	} else {
		return FAIL;
	}
}

STATUS
dbreadtext(DBPROCESS * dbproc, void *buf, DBINT bufsize)
{
TDSSOCKET *tds;
TDSCOLINFO *curcol;
int cpbytes, bytes_avail, rc;
TDS_INT rowtype;
TDS_INT computeid;
TDSRESULTINFO *resinfo;

	tds = dbproc->tds_socket;

	if (!tds || !tds->res_info || !tds->res_info->columns[0])
		return -1;

	resinfo = tds->res_info;
	curcol = resinfo->columns[0];

	/* if the current position is beyond the end of the text
	 * ** set pos to 0 and return 0 to denote the end of the 
	 * ** text */
	if (curcol->column_textpos && curcol->column_textpos >= curcol->column_cur_size) {
		curcol->column_textpos = 0;
		return 0;
	}

	/* if pos is 0 (first time through or last call exhausted the text)
	 * ** then read another row */

	if (curcol->column_textpos == 0) {
		rc = tds_process_row_tokens(dbproc->tds_socket, &rowtype, &computeid);
		if (rc == TDS_NO_MORE_ROWS) {
			return NO_MORE_ROWS;
		}
	}

	/* find the number of bytes to return */
	bytes_avail = curcol->column_cur_size - curcol->column_textpos;
	cpbytes = bytes_avail > bufsize ? bufsize : bytes_avail;
	memcpy(buf, &((TDSBLOBINFO *) (resinfo->current_row + curcol->column_offset))->textvalue[curcol->column_textpos], cpbytes);
	curcol->column_textpos += cpbytes;
	return cpbytes;
}

RETCODE
dbmoretext(DBPROCESS * dbproc, DBINT size, BYTE * text)
{
	tds_put_bulk_data(dbproc->tds_socket, text, size);
	dbproc->text_sent += size;

	return SUCCEED;
}

void
dbrecftos(char *filename)
{
	g_dblib_ctx.recftos_filename = malloc(strlen(filename) + 1);
	if (g_dblib_ctx.recftos_filename != (char *) NULL) {
		strcpy(g_dblib_ctx.recftos_filename, filename);
		g_dblib_ctx.recftos_filenum = 0;
	}
}

/**
 * The integer values of the constants are counterintuitive.  
 */
int
dbtds(DBPROCESS * dbprocess)
{
	if (dbprocess && dbprocess->tds_socket) {
		switch (dbprocess->tds_socket->major_version) {
		case 4:
			switch (dbprocess->tds_socket->minor_version) {
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

const char *
dbversion()
{
	return software_version;
}

RETCODE
dbsetdefcharset(char *charset)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbsetdefcharset()\n");
	return SUCCEED;
}

RETCODE
dbreginit(DBPROCESS * dbproc, DBCHAR * procedure_name, DBSMALLINT namelen)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbreginit()\n");
	return SUCCEED;
}

RETCODE
dbreglist(DBPROCESS * dbproc)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbreglist()\n");
	return SUCCEED;
}

RETCODE
dbregparam(DBPROCESS * dbproc, char *param_name, int type, DBINT datalen, BYTE * data)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbregparam()\n");
	return SUCCEED;
}

RETCODE
dbregexec(DBPROCESS * dbproc, DBUSMALLINT options)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbregexec()\n");
	return SUCCEED;
}

const char *
dbmonthname(DBPROCESS * dbproc, char *language, int monthnum, DBBOOL shortform)
{
	static const char *shortmon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	static const char *longmon[] =
		{ "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
		"December"
	};

	if (shortform)
		return shortmon[monthnum - 1];
	else
		return longmon[monthnum - 1];
}

char *
dbchange(DBPROCESS * dbproc)
{

	if (dbproc->envchange_rcv & (1 << (TDS_ENV_DATABASE - 1))) {
		return dbproc->dbcurdb;
	}
	return NULL;
}

char *
dbname(DBPROCESS * dbproc)
{

	return dbproc->dbcurdb;
}

char *
dbservcharset(DBPROCESS * dbproc)
{

	return dbproc->servcharset;
}

RETCODE
dbsqlsend(DBPROCESS * dbproc)
{
int result = FAIL;
TDSSOCKET *tds;
char *cmdstr;
int rc;
TDS_INT result_type;
unsigned char marker;
int done_flags;
int pending_error = 0;

	dbproc->avail_flag = FALSE;
	dbproc->envchange_rcv = 0;
	dbproc->dbresults_state = DBRESINIT;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbsqlsend()\n");
	tds = (TDSSOCKET *) dbproc->tds_socket;

	if (tds->state == TDS_PENDING) {

		if (tds_process_trailing_tokens(tds) != TDS_SUCCEED) {
			_dblib_client_msg(dbproc, 20019, 7, "Attempt to initiate a new SQL Server operation with results pending.");
			dbproc->command_state = DBCMDSENT;
			return FAIL;
		}
	}

	if (dbproc->dboptcmd) {
		if ((cmdstr = dbstring_get(dbproc->dboptcmd)) == NULL) {
			return FAIL;
		}
		rc = tds_submit_query(dbproc->tds_socket, cmdstr, NULL);
		free(cmdstr);
		dbstring_free(&(dbproc->dboptcmd));
		if (rc != TDS_SUCCEED) {
			return FAIL;
		}
		while ((rc = tds_process_result_tokens(tds, &result_type))
		       == TDS_SUCCEED);
		if (rc != TDS_NO_MORE_RESULTS) {
			return FAIL;
		}
	}
	dbproc->more_results = TRUE;

	if (dbproc->ftos != (FILE *) NULL) {
		fprintf(dbproc->ftos, "%s\n", dbproc->dbbuf);
		fprintf(dbproc->ftos, "go /* %s */\n", _dbprdate());
		fflush(dbproc->ftos);
	}

	if (tds_submit_query(dbproc->tds_socket, (char *) dbproc->dbbuf, NULL) != TDS_SUCCEED) {
		return FAIL;
	}
	dbproc->command_state = DBCMDSENT;
	return SUCCEED;
}

DBINT
dbaltutype(DBPROCESS * dbproc, int computeid, int column)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
TDSCOMPUTEINFO *info;
TDSCOLINFO *colinfo;
TDS_SMALLINT compute_id;
int i;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbaltutype()\n");
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

DBINT
dbaltlen(DBPROCESS * dbproc, int computeid, int column)
{
TDSSOCKET *tds = (TDSSOCKET *) dbproc->tds_socket;
TDSCOMPUTEINFO *info;
TDSCOLINFO *colinfo;
TDS_SMALLINT compute_id;
int i;

	tdsdump_log(TDS_DBG_FUNC, "%L in dbaltlen()\n");
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

RETCODE
dbpoll(DBPROCESS * dbproc, long milliseconds, DBPROCESS ** ready_dbproc, int *return_reason)
{
	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED dbpoll()\n");
	return SUCCEED;
}

DBINT
dblastrow(DBPROCESS * dbproc)
{
TDSRESULTINFO *resinfo;
TDSSOCKET *tds;

	tds = (TDSSOCKET *) dbproc->tds_socket;
	resinfo = tds->res_info;
	return resinfo->row_count;
#if 0
DBINT result;

	if (dbproc->row_buf.rows_in_buf == 0) {
		result = 0;
	} else {
		/* rows returned from the row buffer start with 1 instead of 0
		 * ** newest is 0 based. */
		result = dbproc->row_buf.newest + 1;
	}
	return result;
#endif
}

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

int
dbiordesc(DBPROCESS * dbproc)
{
	return dbproc->tds_socket->s;
}

int
dbiowdesc(DBPROCESS * dbproc)
{
	return dbproc->tds_socket->s;
}

static void
_set_null_value(DBPROCESS * dbproc, BYTE * varaddr, int datatype, int maxlen)
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
	case SYBCHAR:
	case SYBVARCHAR:
		varaddr[0] = '\0';
		break;
	}
}

DBBOOL
dbisavail(DBPROCESS * dbproc)
{
	return dbproc->avail_flag;
}

void
dbsetavail(DBPROCESS * dbproc)
{
	dbproc->avail_flag = TRUE;
}

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

int
_dblib_client_msg(DBPROCESS * dbproc, int dberr, int severity, const char *dberrstr)
{
	TDSSOCKET *tds = NULL;

	if (dbproc)
		tds = dbproc->tds_socket;
	return tds_client_msg(g_dblib_ctx.tds_ctx, tds, dberr, severity, -1, -1, dberrstr);
}
static char *
_dbprdate()
{

time_t currtime;
char timestr[256];

	currtime = time((time_t *) NULL);

	strcpy(timestr, asctime(gmtime(&currtime)));
	timestr[strlen(timestr) - 1] = '\0';	/* remove newline */
	return timestr;

}
