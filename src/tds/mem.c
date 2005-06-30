/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005  Brian Bruns
 * Copyright (C) 2005 Frediano Ziglio
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

#include "tds.h"
#include "tdsiconv.h"
#include "tdsstring.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: mem.c,v 1.148 2005-06-30 12:14:14 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version,
	no_unused_var_warn
};

static void tds_free_env(TDSSOCKET * tds);
static void tds_free_compute_results(TDSSOCKET * tds);
static void tds_free_compute_result(TDSCOMPUTEINFO * comp_info);

#undef TEST_MALLOC
#define TEST_MALLOC(dest,type) \
	{if (!(dest = (type*)malloc(sizeof(type)))) goto Cleanup;}

#undef TEST_CALLOC
#define TEST_CALLOC(dest,type,n) \
	{if (!(dest = (type*)calloc((n), sizeof(type)))) goto Cleanup;}

#undef TEST_STRDUP
#define TEST_STRDUP(dest,str) \
	{if (!(dest = strdup(str))) goto Cleanup;}

/**
 * \ingroup libtds
 * \defgroup mem Memory allocation
 * Allocate or free resources. Allocation can fail only on out of memory. 
 * In such case they return NULL and leave the state as before call.
 */

/**
 * \addtogroup mem
 * \@{
 */

/**
 * \fn TDSDYNAMIC *tds_alloc_dynamic(TDSSOCKET *tds, const char *id)
 * \brief Allocate a dynamic statement.
 * \param tds the connection within which to allocate the statement.
 * \param id a character label identifying the statement.
 * \return a pointer to the allocated structure (NULL on failure).
 *
 * tds_alloc_dynamic is used to implement placeholder code under TDS 5.0
 */
TDSDYNAMIC *
tds_alloc_dynamic(TDSSOCKET * tds, const char *id)
{
	TDSDYNAMIC *dyn, *curr;

	/* check to see if id already exists (shouldn't) */
	for (curr = tds->dyns; curr != NULL; curr = curr->next)
		if (!strcmp(curr->id, id)) {
			/* id already exists! just return it */
			return curr;
		}

	dyn = (TDSDYNAMIC *) malloc(sizeof(TDSDYNAMIC));
	if (!dyn)
		return NULL;
	memset(dyn, 0, sizeof(TDSDYNAMIC));

	/* insert into list */
	dyn->next = tds->dyns;
	tds->dyns = dyn;

	tds_strlcpy(dyn->id, id, TDS_MAX_DYNID_LEN);

	return dyn;
}

/**
 * \fn void tds_free_input_params(TDSDYNAMIC *dyn)
 * \brief Frees all allocated input parameters of a dynamic statement.
 * \param dyn the dynamic statement whose input parameter are to be freed
 *
 * tds_free_input_params frees all parameters for the give dynamic statement
 */
void
tds_free_input_params(TDSDYNAMIC * dyn)
{
	TDSPARAMINFO *info;

	info = dyn->params;
	if (info) {
		tds_free_param_results(info);
		dyn->params = NULL;
	}
}

/**
 * \fn void tds_free_dynamic(TDSSOCKET *tds, TDSDYNAMIC *dyn)
 * \brief Frees dynamic statement and remove from TDS
 * \param tds state information for the socket and the TDS protocol
 * \param dyn dynamic statement to be freed.
 */
void
tds_free_dynamic(TDSSOCKET * tds, TDSDYNAMIC * dyn)
{
	TDSDYNAMIC **pcurr;

	/* avoid pointer to garbage */
	if (tds->cur_dyn == dyn)
		tds->cur_dyn = NULL;

	if (tds->current_results == dyn->res_info)
		tds->current_results = NULL;

	/* free from tds */
	for (pcurr = &tds->dyns; *pcurr != NULL; pcurr = &(*pcurr)->next)
		if (dyn == *pcurr) {
			*pcurr = dyn->next;
			break;
		}

	tds_free_results(dyn->res_info);
	tds_free_input_params(dyn);
	if (dyn->query)
		free(dyn->query);
	free(dyn);
}

/**
 * \fn TDSPARAMINFO *tds_alloc_param_result(TDSPARAMINFO *old_param)
 * \brief Adds a output parameter to TDSPARAMINFO.
 * \param old_param a pointer to the TDSPARAMINFO structure containing the 
 * current set of output parameter, or NULL if none exists.
 * \return a pointer to the new TDSPARAMINFO structure.
 *
 * tds_alloc_param_result() works a bit differently than the other alloc result
 * functions.  Output parameters come in individually with no total number 
 * given in advance, so we simply call this func every time with get a
 * TDS_PARAM_TOKEN and let it realloc the columns struct one bigger. 
 * tds_free_all_results() usually cleans up after us.
 */
TDSPARAMINFO *
tds_alloc_param_result(TDSPARAMINFO * old_param)
{
	TDSPARAMINFO *param_info;
	TDSCOLUMN *colinfo;
	TDSCOLUMN **cols;

	colinfo = (TDSCOLUMN *) malloc(sizeof(TDSCOLUMN));
	if (!colinfo)
		return NULL;
	memset(colinfo, 0, sizeof(TDSCOLUMN));

	if (!old_param || !old_param->num_cols) {
		cols = (TDSCOLUMN **) malloc(sizeof(TDSCOLUMN *));
	} else {
		cols = (TDSCOLUMN **) realloc(old_param->columns, sizeof(TDSCOLUMN *) * (old_param->num_cols + 1));
	}
	if (!cols)
		goto Cleanup;

	if (!old_param) {
		param_info = (TDSPARAMINFO *) malloc(sizeof(TDSPARAMINFO));
		if (!param_info) {
			free(cols);
			goto Cleanup;
		}
		memset(param_info, '\0', sizeof(TDSPARAMINFO));
	} else {
		param_info = old_param;
	}
	param_info->columns = cols;
	param_info->columns[param_info->num_cols++] = colinfo;
	return param_info;
      Cleanup:
	free(colinfo);
	return NULL;
}

/**
 * Add another field to row. Is assumed that last TDSCOLUMN contain information about this.
 * Update also info structure.
 * @param info     parameters info where is contained row
 * @param curparam parameter to retrieve size information
 * @return NULL on failure or new row
 */
unsigned char *
tds_alloc_param_row(TDSPARAMINFO * info, TDSCOLUMN * curparam)
{
	TDS_INT row_size;
	unsigned char *row;

#if ENABLE_EXTRA_CHECKS
	assert(info->row_size % TDS_ALIGN_SIZE == 0);
#endif

	curparam->column_offset = info->row_size;
	if (is_numeric_type(curparam->column_type)) {
		row_size = sizeof(TDS_NUMERIC);
	} else if (is_blob_type(curparam->column_type)) {
		row_size = sizeof(TDSBLOB);
	} else {
		row_size = curparam->column_size;
	}
	row_size += info->row_size + (TDS_ALIGN_SIZE - 1);
	row_size -= row_size % TDS_ALIGN_SIZE;


#if ENABLE_EXTRA_CHECKS
	assert((row_size % TDS_ALIGN_SIZE) == 0);
#endif

	/* make sure the row buffer is big enough */
	if (info->current_row) {
		row = (unsigned char *) realloc(info->current_row, row_size);
	} else {
		row = (unsigned char *) malloc(row_size);
	}
	if (!row)
		return NULL;
	/* if is a blob reset buffer */
	if (is_blob_type(curparam->column_type))
		memset(row + info->row_size, 0, sizeof(TDSBLOB));
	info->current_row = row;
	info->row_size = row_size;

	return row;
}

/**
 * Allocate memory for storing compute info
 * return NULL on out of memory
 */

static TDSCOMPUTEINFO *
tds_alloc_compute_result(int num_cols, int by_cols)
{
	int col;
	TDSCOMPUTEINFO *info;

	TEST_MALLOC(info, TDSCOMPUTEINFO);
	memset(info, '\0', sizeof(TDSCOMPUTEINFO));

	TEST_CALLOC(info->columns, TDSCOLUMN *, num_cols);

	tdsdump_log(TDS_DBG_INFO1, "alloc_compute_result. point 1\n");
	info->num_cols = num_cols;
	for (col = 0; col < num_cols; col++) {
		TEST_MALLOC(info->columns[col], TDSCOLUMN);
		memset(info->columns[col], '\0', sizeof(TDSCOLUMN));
	}

	tdsdump_log(TDS_DBG_INFO1, "alloc_compute_result. point 2\n");

	if (by_cols) {
		TEST_CALLOC(info->bycolumns, TDS_SMALLINT, by_cols);
		tdsdump_log(TDS_DBG_INFO1, "alloc_compute_result. point 3\n");
		info->by_cols = by_cols;
	}

	return info;
      Cleanup:
	tds_free_compute_result(info);
	return NULL;
}

TDSCOMPUTEINFO **
tds_alloc_compute_results(TDSSOCKET * tds, int num_cols, int by_cols)
{
	int n;
	TDSCOMPUTEINFO **comp_info;
	TDSCOMPUTEINFO *cur_comp_info;

	tdsdump_log(TDS_DBG_INFO1, "alloc_compute_result. num_cols = %d bycols = %d\n", num_cols, by_cols);
	tdsdump_log(TDS_DBG_INFO1, "alloc_compute_result. num_comp_info = %d\n", tds->num_comp_info);

	cur_comp_info = tds_alloc_compute_result(num_cols, by_cols);
	if (!cur_comp_info)
		return NULL;

	n = tds->num_comp_info;
	if (n == 0)
		comp_info = (TDSCOMPUTEINFO **) malloc(sizeof(TDSCOMPUTEINFO *));
	else
		comp_info = (TDSCOMPUTEINFO **) realloc(tds->comp_info, sizeof(TDSCOMPUTEINFO *) * (n + 1));

	if (!comp_info) {
		tds_free_compute_result(cur_comp_info);
		return NULL;
	}

	tds->comp_info = comp_info;
	comp_info[n] = cur_comp_info;
	tds->num_comp_info = n + 1;

	tdsdump_log(TDS_DBG_INFO1, "alloc_compute_result. num_comp_info = %d\n", tds->num_comp_info);

	return comp_info;
}

TDSRESULTINFO *
tds_alloc_results(int num_cols)
{
	TDSRESULTINFO *res_info;
	int col;

	TEST_MALLOC(res_info, TDSRESULTINFO);
	memset(res_info, '\0', sizeof(TDSRESULTINFO));
	TEST_CALLOC(res_info->columns, TDSCOLUMN *, num_cols);
	for (col = 0; col < num_cols; col++) {
		TEST_MALLOC(res_info->columns[col], TDSCOLUMN);
		memset(res_info->columns[col], '\0', sizeof(TDSCOLUMN));
	}
	res_info->num_cols = num_cols;
	res_info->row_size = 0;
	return res_info;
      Cleanup:
	tds_free_results(res_info);
	return NULL;
}

/**
 * Allocate space for row store
 * return NULL on out of memory
 */
unsigned char *
tds_alloc_row(TDSRESULTINFO * res_info)
{
	unsigned char *ptr;

	ptr = (unsigned char *) malloc(res_info->row_size);
	if (!ptr)
		return NULL;
	memset(ptr, '\0', res_info->row_size);
	return ptr;
}

unsigned char *
tds_alloc_compute_row(TDSCOMPUTEINFO * res_info)
{
	unsigned char *ptr;

	ptr = (unsigned char *) malloc(res_info->row_size);
	if (!ptr)
		return NULL;
	memset(ptr, '\0', res_info->row_size);
	return ptr;
}

void
tds_free_param_results(TDSPARAMINFO * param_info)
{
	tds_free_results(param_info);
}

static void
tds_free_compute_result(TDSCOMPUTEINFO * comp_info)
{
	tds_free_results(comp_info);
}

static void
tds_free_compute_results(TDSSOCKET * tds)
{
	int i;
	TDSCOMPUTEINFO ** comp_info = tds->comp_info;
	TDS_INT num_comp = tds->num_comp_info;

	tds->comp_info = NULL;
	tds->num_comp_info = 0;

	for (i = 0; i < num_comp; i++) {
		if (comp_info && comp_info[i]) {
			if (tds->current_results == comp_info[i])
				tds->current_results = NULL;
			tds_free_compute_result(comp_info[i]);
		}
	}
	if (num_comp)
		free(comp_info);
}

void
tds_free_results(TDSRESULTINFO * res_info)
{
	int i;
	TDSCOLUMN *curcol;

	if (!res_info)
		return;

	if (res_info->num_cols && res_info->columns) {
		for (i = 0; i < res_info->num_cols; i++)
			if ((curcol = res_info->columns[i]) != NULL) {
				if (curcol->bcp_terminator)
					free(curcol->bcp_terminator);
				tds_free_bcp_column_data(curcol->bcp_column_data);
				if (res_info->current_row && is_blob_type(curcol->column_type)) {
					free(((TDSBLOB *) (res_info->current_row + curcol->column_offset))->textvalue);
				}
				free(curcol);
			}
		free(res_info->columns);
	}

	if (res_info->current_row)
		free(res_info->current_row);

	if (res_info->bycolumns)
		free(res_info->bycolumns);

	free(res_info);
}

void
tds_free_all_results(TDSSOCKET * tds)
{
	tdsdump_log(TDS_DBG_FUNC, "tds_free_all_results()\n");
	if (tds->current_results == tds->res_info)
		tds->current_results = NULL;
	tds_free_results(tds->res_info);
	tds->res_info = NULL;
	if (tds->current_results == tds->param_info)
		tds->current_results = NULL;
	tds_free_param_results(tds->param_info);
	tds->param_info = NULL;
	tds_free_compute_results(tds);
	tds->has_status = 0;
	tds->ret_status = 0;
}

TDSCONTEXT *
tds_alloc_context(void * parent)
{
	TDSCONTEXT *context;
	TDSLOCALE *locale;

	locale = tds_get_locale();
	if (!locale)
		return NULL;

	context = (TDSCONTEXT *) malloc(sizeof(TDSCONTEXT));
	if (!context) {
		tds_free_locale(locale);
		return NULL;
	}
	memset(context, '\0', sizeof(TDSCONTEXT));
	context->locale = locale;
	context->parent = parent;

	return context;
}

void
tds_free_context(TDSCONTEXT * context)
{
	if (!context)
		return;

	tds_free_locale(context->locale);
	free(context);
}

TDSLOCALE *
tds_alloc_locale(void)
{
	TDSLOCALE *locale;

	locale = (TDSLOCALE *) malloc(sizeof(TDSLOCALE));
	if (!locale)
		return NULL;
	memset(locale, 0, sizeof(TDSLOCALE));

	return locale;
}
static const unsigned char defaultcaps[] = { 0x01, 0x09, 0x00, 0x00, 0x06, 0x6D, 0x7F, 0xFF, 0xFF, 0xFF, 0xFE,
	0x02, 0x09, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x68, 0x00, 0x00, 0x00
};

/**
 * Allocate space for configure structure and initialize with default values
 * @param locale locale information (copied to configuration information)
 * @result allocated structure or NULL if out of memory
 */
TDSCONNECTION *
tds_alloc_connection(TDSLOCALE * locale)
{
	TDSCONNECTION *connection;
	char hostname[128];

	TEST_MALLOC(connection, TDSCONNECTION);
	memset(connection, '\0', sizeof(TDSCONNECTION));
	tds_dstr_init(&connection->server_name);
	tds_dstr_init(&connection->language);
	tds_dstr_init(&connection->server_charset);
	tds_dstr_init(&connection->host_name);
	tds_dstr_init(&connection->app_name);
	tds_dstr_init(&connection->user_name);
	tds_dstr_init(&connection->password);
	tds_dstr_init(&connection->library);
	tds_dstr_init(&connection->ip_addr);
	tds_dstr_init(&connection->database);
	tds_dstr_init(&connection->dump_file);
	tds_dstr_init(&connection->client_charset);
	tds_dstr_init(&connection->instance_name);

	/* fill in all hardcoded defaults */
	if (!tds_dstr_copy(&connection->server_name, TDS_DEF_SERVER))
		goto Cleanup;
	connection->major_version = TDS_DEF_MAJOR;
	connection->minor_version = TDS_DEF_MINOR;
	connection->port = TDS_DEF_PORT;
	connection->block_size = 0;
	/* TODO use system default ?? */
	if (!tds_dstr_copy(&connection->client_charset, "ISO-8859-1"))
		goto Cleanup;
	if (locale) {
		if (locale->language)
			if (!tds_dstr_copy(&connection->language, locale->language))
				goto Cleanup;
		if (locale->char_set)
			if (!tds_dstr_copy(&connection->server_charset, locale->char_set))
				goto Cleanup;
	}
	if (tds_dstr_isempty(&connection->language)) {
		if (!tds_dstr_copy(&connection->language, TDS_DEF_LANG))
			goto Cleanup;
	}
	memset(hostname, '\0', sizeof(hostname));
	gethostname(hostname, sizeof(hostname));
	hostname[sizeof(hostname) - 1] = '\0';	/* make sure it's truncated */
	if (!tds_dstr_copy(&connection->host_name, hostname))
		goto Cleanup;

	memcpy(connection->capabilities, defaultcaps, TDS_MAX_CAPABILITY);
	return connection;
      Cleanup:
	tds_free_connection(connection);
	return NULL;
}

TDSCURSOR *
tds_alloc_cursor(TDSSOCKET *tds, const char *name, TDS_INT namelen, const char *query, TDS_INT querylen)
{
	TDSCURSOR *cursor;
	TDSCURSOR *pcursor;

	TEST_MALLOC(cursor, TDSCURSOR);
	memset(cursor, '\0', sizeof(TDSCURSOR));

	if ( tds->cursors == NULL ) {
		tds->cursors = cursor;
	} else {
		pcursor = tds->cursors;
		for (;;) {
			tdsdump_log(TDS_DBG_FUNC, "tds_alloc_cursor() : stepping thru existing cursors\n");
			if (pcursor->next == NULL)
				break;
			pcursor = pcursor->next;
		}
		pcursor->next = cursor;
	}

	TEST_CALLOC(cursor->cursor_name, char, namelen + 1);

	strcpy(cursor->cursor_name, name);
	cursor->cursor_name_len = namelen;

	TEST_CALLOC(cursor->query, char, querylen + 1);

	strcpy(cursor->query, query);
	cursor->query_len = querylen;

	return cursor;

      Cleanup:
	if (cursor)
		tds_free_cursor(tds, cursor);
	return NULL;
}

void
tds_free_cursor(TDSSOCKET *tds, TDSCURSOR *cursor)
{
	TDSCURSOR *victim = NULL;
	TDSCURSOR *prev = NULL;
	TDSCURSOR *next = NULL;

	tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : freeing cursor_id %d\n", cursor->cursor_id);
	victim = tds->cursors;

	if (tds->cur_cursor == cursor)
		tds->cur_cursor = NULL;

	if (tds->current_results == cursor->res_info)
		tds->current_results = NULL;

	if (victim == NULL) {
		tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : no allocated cursors %d\n", cursor->cursor_id);
		return;
	}

	for (;;) {
		if (victim == cursor)
			break;
		prev = victim;
		victim = victim->next;
		if (victim == NULL) {
			tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : cannot find cursor_id %d\n", cursor->cursor_id);
			return;
		}
	}

	tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : cursor_id %d found\n", cursor->cursor_id);

	next = victim->next;

	if (victim->cursor_name) {
		tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : freeing cursor name\n");
		free(victim->cursor_name);
	}

	if (victim->query) {
		tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : freeing cursor query\n");
		free(victim->query);
	}

	tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : freeing cursor results\n");
	if (victim->res_info == tds->current_results)
		tds->current_results = NULL;
	tds_free_results(victim->res_info);

	tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : relinking list\n");

	if (prev)
		prev->next = next;
	else
		tds->cursors = next;

	tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : relinked list\n");
	tdsdump_log(TDS_DBG_FUNC, "tds_free_cursor() : cursor_id %d freed\n", cursor->cursor_id);

	free(victim);
}

TDSLOGIN *
tds_alloc_login(void)
{
	TDSLOGIN *tds_login;

	tds_login = (TDSLOGIN *) malloc(sizeof(TDSLOGIN));
	if (!tds_login)
		return NULL;
	memset(tds_login, '\0', sizeof(TDSLOGIN));
	tds_dstr_init(&tds_login->server_name);
	tds_dstr_init(&tds_login->server_addr);
	tds_dstr_init(&tds_login->language);
	tds_dstr_init(&tds_login->server_charset);
	tds_dstr_init(&tds_login->host_name);
	tds_dstr_init(&tds_login->app_name);
	tds_dstr_init(&tds_login->user_name);
	tds_dstr_init(&tds_login->password);
	tds_dstr_init(&tds_login->library);
	tds_dstr_init(&tds_login->client_charset);
	memcpy(tds_login->capabilities, defaultcaps, TDS_MAX_CAPABILITY);
	return tds_login;
}

void
tds_free_login(TDSLOGIN * login)
{
	if (login) {
		/* for security reason clear memory */
		tds_dstr_zero(&login->password);
		tds_dstr_free(&login->password);
		tds_dstr_free(&login->server_name);
		tds_dstr_free(&login->server_addr);
		tds_dstr_free(&login->language);
		tds_dstr_free(&login->server_charset);
		tds_dstr_free(&login->host_name);
		tds_dstr_free(&login->app_name);
		tds_dstr_free(&login->user_name);
		tds_dstr_free(&login->library);
		tds_dstr_free(&login->client_charset);
		free(login);
	}
}
TDSSOCKET *
tds_alloc_socket(TDSCONTEXT * context, int bufsize)
{
	TDSSOCKET *tds_socket;

	TEST_MALLOC(tds_socket, TDSSOCKET);
	memset(tds_socket, '\0', sizeof(TDSSOCKET));
	tds_socket->tds_ctx = context;
	tds_socket->in_buf_max = 0;
	TEST_CALLOC(tds_socket->out_buf, unsigned char, bufsize);

	tds_socket->parent = NULL;
	/*
	 * TDS 7.0: 
	 * 0x02 indicates ODBC driver
	 * 0x01 means change to initial language must succeed
	 */
	tds_socket->option_flag2 = 0x03;
	tds_socket->env.block_size = bufsize;

	if (tds_iconv_alloc(tds_socket))
		goto Cleanup;

	/* Jeff's hack, init to no timeout */
	tds_socket->query_timeout = 0;
	tds_init_write_buf(tds_socket);
	tds_socket->s = INVALID_SOCKET;
	tds_socket->env_chg_func = NULL;
	return tds_socket;
      Cleanup:
	tds_free_socket(tds_socket);
	return NULL;
}

TDSSOCKET *
tds_realloc_socket(TDSSOCKET * tds, int bufsize)
{
	unsigned char *new_out_buf;

	assert(tds && tds->out_buf);

	if (tds->env.block_size == bufsize)
		return tds;

	if ((new_out_buf = (unsigned char *) realloc(tds->out_buf, bufsize)) != NULL) {
		tds->out_buf = new_out_buf;
		tds->env.block_size = bufsize;
		return tds;
	}
	return NULL;
}

void
tds_free_socket(TDSSOCKET * tds)
{
	if (tds) {
		tds_free_all_results(tds);
		tds_free_env(tds);
		while (tds->dyns)
			tds_free_dynamic(tds, tds->dyns);
		while (tds->cursors)
			tds_free_cursor(tds, tds->cursors);
		if (tds->in_buf)
			free(tds->in_buf);
		if (tds->out_buf)
			free(tds->out_buf);
#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
		tds_ssl_deinit(tds);
#endif
		tds_close_socket(tds);
		if (tds->date_fmt)
			free(tds->date_fmt);
		tds_iconv_free(tds);
		if (tds->product_name)
			free(tds->product_name);
		free(tds);
	}
}
void
tds_free_locale(TDSLOCALE * locale)
{
	if (!locale)
		return;

	if (locale->language)
		free(locale->language);
	if (locale->char_set)
		free(locale->char_set);
	if (locale->date_fmt)
		free(locale->date_fmt);
	free(locale);
}

void
tds_free_connection(TDSCONNECTION * connection)
{
	tds_dstr_free(&connection->server_name);
	tds_dstr_free(&connection->host_name);
	tds_dstr_free(&connection->language);
	tds_dstr_free(&connection->server_charset);
	tds_dstr_free(&connection->ip_addr);
	tds_dstr_free(&connection->database);
	tds_dstr_free(&connection->dump_file);
	tds_dstr_free(&connection->client_charset);
	tds_dstr_free(&connection->app_name);
	tds_dstr_free(&connection->user_name);
	/* cleared for security reason */
	tds_dstr_zero(&connection->password);
	tds_dstr_free(&connection->password);
	tds_dstr_free(&connection->library);
	tds_dstr_free(&connection->instance_name);
	free(connection);
}

static void
tds_free_env(TDSSOCKET * tds)
{
	if (tds->env.language)
		TDS_ZERO_FREE(tds->env.language);
	if (tds->env.charset)
		TDS_ZERO_FREE(tds->env.charset);
	if (tds->env.database)
		TDS_ZERO_FREE(tds->env.database);
}

void
tds_free_msg(TDSMESSAGE * message)
{
	if (message) {
		message->priv_msg_type = 0;
		message->msgno = 0;
		message->state = 0;
		message->severity = 0;
		message->line_number = 0;
		if (message->message)
			TDS_ZERO_FREE(message->message);
		if (message->server)
			TDS_ZERO_FREE(message->server);
		if (message->proc_name)
			TDS_ZERO_FREE(message->proc_name);
		if (message->sql_state)
			TDS_ZERO_FREE(message->sql_state);
	}
}

#define SQLS_ENTRY(number,state) case number: p = state; break

char *
tds_alloc_client_sqlstate(int msgno)
{
	char *p = NULL;

	switch (msgno) {
		SQLS_ENTRY(17000, "S1T00");	/* timeouts ??? */
		SQLS_ENTRY(20004, "08S01");	/* Communication link failure */
		SQLS_ENTRY(20006, "08S01");
		SQLS_ENTRY(20009, "08S01");
		SQLS_ENTRY(20020, "08S01");
		SQLS_ENTRY(20019, "24000");	/* Invalid cursor state */
		SQLS_ENTRY(20014, "28000");	/* Invalid authorization specification */
		SQLS_ENTRY(2400, "42000");	/* Syntax error or access violation */
		SQLS_ENTRY(2401, "42000");
		SQLS_ENTRY(2403, "42000");
		SQLS_ENTRY(2404, "42000");
		SQLS_ENTRY(2402, "S1000");	/* General error */
	}

	if (p != NULL)
		return strdup(p);
	else
		return NULL;
}

char *
tds_alloc_lookup_sqlstate(TDSSOCKET * tds, int msgno)
{
	const char *p = NULL;
	char *q = NULL;

	if (TDS_IS_MSSQL(tds)) {
		switch (msgno) {	/* MSSQL Server */

			SQLS_ENTRY(3621,"01000");
			SQLS_ENTRY(8153,"01003");	/* Null in aggregate */
			SQLS_ENTRY(911, "08004");	/* Server rejected connection */
			SQLS_ENTRY(512, "21000");	/* Subquery returns more than one value */
			SQLS_ENTRY(213, "21S01");	/* Insert column list mismatch */
			SQLS_ENTRY(109, "21S01");
			SQLS_ENTRY(110, "21S01");
			SQLS_ENTRY(1774,"21S02");	/* Ref column mismatch */
			SQLS_ENTRY(8152,"22001");	/* String data would be truncated */
			SQLS_ENTRY(5146,"22003");	/* Numeric value out of range */
			SQLS_ENTRY(168,	"22003");	/* Arithmetic overflow */
			SQLS_ENTRY(220, "22003");
			SQLS_ENTRY(232, "22003");
			SQLS_ENTRY(234, "22003");
			SQLS_ENTRY(236, "22003");
			SQLS_ENTRY(238, "22003");
			SQLS_ENTRY(244, "22003");
			SQLS_ENTRY(246, "22003");
			SQLS_ENTRY(248, "22003");
			SQLS_ENTRY(519, "22003");
			SQLS_ENTRY(520, "22003");
			SQLS_ENTRY(521, "22003");
			SQLS_ENTRY(522, "22003");
			SQLS_ENTRY(523, "22003");
			SQLS_ENTRY(524, "22003");
			SQLS_ENTRY(1007,"22003");
			SQLS_ENTRY(3606,"22003");
			SQLS_ENTRY(8115,"22003");
			SQLS_ENTRY(206, "22005");	/* Error in assignment */
			SQLS_ENTRY(235, "22005");
			SQLS_ENTRY(247, "22005");
			SQLS_ENTRY(249, "22005");
			SQLS_ENTRY(256, "22005");
			SQLS_ENTRY(257, "22005");
			SQLS_ENTRY(305, "22005");
			SQLS_ENTRY(409, "22005");
			SQLS_ENTRY(518, "22005");
			SQLS_ENTRY(529, "22005");
			SQLS_ENTRY(210, "22007");	/* Invalid datetime format */
			SQLS_ENTRY(241, "22007");
			SQLS_ENTRY(295, "22007");
			SQLS_ENTRY(242, "22008");	/* Datetime out of range */
			SQLS_ENTRY(296, "22008");
			SQLS_ENTRY(298, "22008");
			SQLS_ENTRY(535, "22008");
			SQLS_ENTRY(542, "22008");
			SQLS_ENTRY(517, "22008");
			SQLS_ENTRY(3607, "22012");	/* Div by zero */
			SQLS_ENTRY(8134, "22012");
			SQLS_ENTRY(245, "22018");	/* Syntax error? */
			SQLS_ENTRY(2627, "23000");	/* Constraint violation */
			SQLS_ENTRY(515, "23000");
			SQLS_ENTRY(233,	"23000");
			SQLS_ENTRY(273,	"23000");
			SQLS_ENTRY(530,	"23000");
			SQLS_ENTRY(2601,"23000");
			SQLS_ENTRY(2615,"23000");
			SQLS_ENTRY(2626,"23000");
			SQLS_ENTRY(3604,"23000");
			SQLS_ENTRY(3605,"23000");
			SQLS_ENTRY(544, "23000");
			SQLS_ENTRY(547, "23000");
			SQLS_ENTRY(550, "23000");
			SQLS_ENTRY(4415, "23000");
			SQLS_ENTRY(1505, "23000");
			SQLS_ENTRY(1508, "23000");
			SQLS_ENTRY(3725, "23000");
			SQLS_ENTRY(3726, "23000");
			SQLS_ENTRY(4712, "23000");
			SQLS_ENTRY(10055, "23000");
			SQLS_ENTRY(10065, "23000");
			SQLS_ENTRY(11011, "23000");
			SQLS_ENTRY(11040, "23000");
			SQLS_ENTRY(16999, "24000");	/* Invalid cursor state */
			SQLS_ENTRY(16905, "24000");
			SQLS_ENTRY(16917, "24000");
			SQLS_ENTRY(16946, "24000");
			SQLS_ENTRY(16950, "24000");
			SQLS_ENTRY(266, "25000");	/* Invalid transaction state */
			SQLS_ENTRY(277,"25000");
			SQLS_ENTRY(611,"25000");
			SQLS_ENTRY(3906,"25000");
			SQLS_ENTRY(3908,"25000");
			SQLS_ENTRY(6401,"25000");
			SQLS_ENTRY(626, "25000");
			SQLS_ENTRY(627, "25000");
			SQLS_ENTRY(628, "25000");
			SQLS_ENTRY(3902, "25000");
			SQLS_ENTRY(3903, "25000");
			SQLS_ENTRY(3916, "25000");
			SQLS_ENTRY(3918, "25000");
			SQLS_ENTRY(3919, "25000");
			SQLS_ENTRY(3921, "25000");
			SQLS_ENTRY(3922, "25000");
			SQLS_ENTRY(3926, "25000");
			SQLS_ENTRY(7969, "25000");
			SQLS_ENTRY(8506, "25000");
			SQLS_ENTRY(15626, "25000");
			SQLS_ENTRY(18456, "28000");	/* Login failed? */
			SQLS_ENTRY(6104, "37000");	/* Syntax error or access violation */
			SQLS_ENTRY(8114, "37000");
			SQLS_ENTRY(131, "37000");
			SQLS_ENTRY(137, "37000");
			SQLS_ENTRY(170, "37000");
			SQLS_ENTRY(174, "37000");
			SQLS_ENTRY(201, "37000");
			SQLS_ENTRY(2812, "37000");
			SQLS_ENTRY(2526, "37000");
			SQLS_ENTRY(8144, "37000");
			SQLS_ENTRY(17308, "42000");	/* Syntax/Access violation */
			SQLS_ENTRY(17571, "42000");
			SQLS_ENTRY(18002, "42000");
			SQLS_ENTRY(229, "42000");
			SQLS_ENTRY(230, "42000");
			SQLS_ENTRY(262, "42000");
			SQLS_ENTRY(2557, "42000");
			SQLS_ENTRY(2571, "42000");
			SQLS_ENTRY(2760, "42000");
			SQLS_ENTRY(3110, "42000");
			SQLS_ENTRY(3704, "42000");
			SQLS_ENTRY(4613, "42000");
			SQLS_ENTRY(4618, "42000");
			SQLS_ENTRY(4834, "42000");
			SQLS_ENTRY(5011, "42000");
			SQLS_ENTRY(5116, "42000");
			SQLS_ENTRY(5812, "42000");
			SQLS_ENTRY(6004, "42000");
			SQLS_ENTRY(6102, "42000");
			SQLS_ENTRY(7956, "42000");
			SQLS_ENTRY(11010, "42000");
			SQLS_ENTRY(11045, "42000");
			SQLS_ENTRY(14126, "42000");
			SQLS_ENTRY(15247, "42000");
			SQLS_ENTRY(15622, "42000");
			SQLS_ENTRY(20604, "42000");
			SQLS_ENTRY(21049, "42000");
			SQLS_ENTRY(113, "42000");
			SQLS_ENTRY(2714, "42S01");	/* Table or view already exists */
			SQLS_ENTRY(208, "42S02");	/* Table or view not found */
			SQLS_ENTRY(3701, "42S02");
			SQLS_ENTRY(1913, "42S11");	/* Index already exists */
			SQLS_ENTRY(15605, "42S11");
			SQLS_ENTRY(307, "42S12");	/* Index not found */
			SQLS_ENTRY(308, "42S12");
			SQLS_ENTRY(10033, "42S12");
			SQLS_ENTRY(15323, "42S12");
			SQLS_ENTRY(18833, "42S12");
			SQLS_ENTRY(4925, "42S21");	/* Column already exists */
			SQLS_ENTRY(21255, "42S21");
			SQLS_ENTRY(1911, "42S22");	/* Column not found */
			SQLS_ENTRY(207, "42S22");
			SQLS_ENTRY(4924, "42S22");
			SQLS_ENTRY(4926, "42S22");
			SQLS_ENTRY(15645, "42S22");
			SQLS_ENTRY(21166, "42S22");
		}
	} else {
		switch (msgno) {	/* Sybase */
			SQLS_ENTRY(3621, "01000");
			SQLS_ENTRY(9501, "01003");	/* Null in aggregate */
			SQLS_ENTRY(911, "08004");	/* Server rejected connection */
			SQLS_ENTRY(512, "21000");	/* Subquery returns more than one value */
			SQLS_ENTRY(213, "21S01");	/* Insert column list mismatch */
			SQLS_ENTRY(109, "21S01");
			SQLS_ENTRY(110, "21S01");
			SQLS_ENTRY(1715, "21S02");	/* Ref column mismatch */
			SQLS_ENTRY(9502, "22001");	/* String data would be truncated */
			SQLS_ENTRY(220, "22003");	/* Arithmetic overflow */
			SQLS_ENTRY(168, "22003");
			SQLS_ENTRY(227, "22003");
			SQLS_ENTRY(232, "22003");
			SQLS_ENTRY(234, "22003");
			SQLS_ENTRY(236, "22003");
			SQLS_ENTRY(238, "22003");
			SQLS_ENTRY(244, "22003");
			SQLS_ENTRY(246, "22003");
			SQLS_ENTRY(247, "22003");
			SQLS_ENTRY(248, "22003");
			SQLS_ENTRY(519, "22003");
			SQLS_ENTRY(520, "22003");
			SQLS_ENTRY(521, "22003");
			SQLS_ENTRY(522, "22003");
			SQLS_ENTRY(523, "22003");
			SQLS_ENTRY(524, "22003");
			SQLS_ENTRY(3606, "22003");
			SQLS_ENTRY(206, "22005");	/* Error in assignment */
			SQLS_ENTRY(235, "22005");
			SQLS_ENTRY(249, "22005");
			SQLS_ENTRY(256, "22005");
			SQLS_ENTRY(305, "22005");
			SQLS_ENTRY(409, "22005");
			SQLS_ENTRY(518, "22005");
			SQLS_ENTRY(529, "22005");
			SQLS_ENTRY(535, "22008");	/* Datetime out of range */
			SQLS_ENTRY(542, "22008");
			SQLS_ENTRY(517, "22008");
			SQLS_ENTRY(3607, "22012");	/* Div by zero */
			SQLS_ENTRY(245, "22018");	/* Syntax error? */
			SQLS_ENTRY(544, "23000");	/* Constraint violation */
			SQLS_ENTRY(233, "23000");
			SQLS_ENTRY(273,	"23000");
			SQLS_ENTRY(530,	"23000");
			SQLS_ENTRY(2601,"23000");
			SQLS_ENTRY(2615,"23000");
			SQLS_ENTRY(2626,"23000");
			SQLS_ENTRY(3604,"23000");
			SQLS_ENTRY(3605,"23000");
			SQLS_ENTRY(545, "23000");
			SQLS_ENTRY(546, "23000");
			SQLS_ENTRY(547, "23000");
			SQLS_ENTRY(548, "23000");
			SQLS_ENTRY(549, "23000");
			SQLS_ENTRY(550, "23000");
			SQLS_ENTRY(1505, "23000");
			SQLS_ENTRY(1508, "23000");
			SQLS_ENTRY(565, "24000");	/* Invalid cursor state */
			SQLS_ENTRY(558, "24000");
			SQLS_ENTRY(559, "24000");
			SQLS_ENTRY(6235, "24000");
			SQLS_ENTRY(583, "24000");
			SQLS_ENTRY(6259, "24000");
			SQLS_ENTRY(6260, "24000");
			SQLS_ENTRY(562, "24000");
			SQLS_ENTRY(277, "25000");	/* Invalid transaction state */
			SQLS_ENTRY(611,"25000");
			SQLS_ENTRY(3906,"25000");
			SQLS_ENTRY(3908,"25000");
			SQLS_ENTRY(6401,"25000");
			SQLS_ENTRY(627, "25000");
			SQLS_ENTRY(628, "25000");
			SQLS_ENTRY(641, "25000");
			SQLS_ENTRY(642, "25000");
			SQLS_ENTRY(1276, "25000");
			SQLS_ENTRY(3902, "25000");
			SQLS_ENTRY(3903, "25000");
			SQLS_ENTRY(6104, "37000");	/* Syntax error or access violation */
			SQLS_ENTRY(102, "37000");
			SQLS_ENTRY(137, "37000");
			SQLS_ENTRY(7327, "37000");
			SQLS_ENTRY(201, "37000");
			SQLS_ENTRY(257, "37000");
			SQLS_ENTRY(2812, "37000");
			SQLS_ENTRY(2526, "37000");
			SQLS_ENTRY(11021, "37000");
			SQLS_ENTRY(229, "42000");	/* Syntax/Access violation */
			SQLS_ENTRY(230, "42000");
			SQLS_ENTRY(262, "42000");
			SQLS_ENTRY(4602, "42000");
			SQLS_ENTRY(4603, "42000");
			SQLS_ENTRY(4608, "42000");
			SQLS_ENTRY(10306, "42000");
			SQLS_ENTRY(10323, "42000");
			SQLS_ENTRY(10330, "42000");
			SQLS_ENTRY(10331, "42000");
			SQLS_ENTRY(10332, "42000");
			SQLS_ENTRY(11110, "42000");
			SQLS_ENTRY(11113, "42000");
			SQLS_ENTRY(11118, "42000");
			SQLS_ENTRY(11121, "42000");
			SQLS_ENTRY(17222, "42000");
			SQLS_ENTRY(17223, "42000");
			SQLS_ENTRY(18350, "42000");
			SQLS_ENTRY(18351, "42000");
			SQLS_ENTRY(113, "42000");
			SQLS_ENTRY(2714, "42S01");	/* Table or view already exists */
			SQLS_ENTRY(208, "42S02");	/* Table or view not found */
			SQLS_ENTRY(3701, "42S02");
			SQLS_ENTRY(1913, "42S11");	/* Index already exists */
			SQLS_ENTRY(307, "42S12");	/* Index not found */
			SQLS_ENTRY(7010, "42S12");
			SQLS_ENTRY(18091, "42S12");
			SQLS_ENTRY(1921, "42S21");	/* Column already exists */
			SQLS_ENTRY(1720, "42S22");	/* Column not found */
			SQLS_ENTRY(207, "42S22");
			SQLS_ENTRY(4934, "42S22");
			SQLS_ENTRY(18117, "42S22");
		}
	}

	if (p != NULL && (q = strdup(p)) != NULL) {
		/* FIXME correct here ?? */
		/* Convert known ODBC 3.x states listed above to 2.x */
		if (memcmp(q, "42S", 3) == 0)
			memcpy(q, "S00", 3);

		return q;
	}
	return NULL;
}

BCPCOLDATA *
tds_alloc_bcp_column_data(int column_size)
{
	BCPCOLDATA *coldata;

	TEST_MALLOC(coldata, BCPCOLDATA);
	memset(coldata, '\0', sizeof(BCPCOLDATA));

	TEST_CALLOC(coldata->data, unsigned char, column_size);

	return coldata;
Cleanup:
	tds_free_bcp_column_data(coldata);
	return NULL;
}

void
tds_free_bcp_column_data(BCPCOLDATA * coldata)
{
	if (!coldata)
		return;

	if (coldata->data)
		free(coldata->data);
	free(coldata);
}

/** \@} */
