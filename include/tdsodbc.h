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

#ifndef _sql_h_
#define _sql_h_

#include <tds.h>

#ifdef __cplusplus
extern "C" {
#endif

static char  rcsid_sql_h [ ] =
         "$Id: tdsodbc.h,v 1.13 2002-11-16 15:21:14 freddy77 Exp $";
static void *no_unused_sql_h_warn[]={rcsid_sql_h, no_unused_sql_h_warn};

struct _henv {
	TDSCONTEXT *tds_ctx;
};
struct _hstmt;
struct _hdbc {
	struct _henv *henv;
	TDSLOGIN *tds_login;
	TDSSOCKET *tds_socket;
	/** statement executing */
	struct _hstmt *current_statement;
	/* spinellia@acm.org */
	/** 0 = OFF, 1 = ON, -1 = UNKNOWN */
	int	autocommit_state;
};
struct _hstmt {
	struct _hdbc *hdbc;
	char *query;
	/* begin prepared query stuff */
	char *prepared_query;
	char *prepared_query_s;
	char *prepared_query_d;
	int  prepared_query_need_bytes;
	int  prepared_query_param_num;
	int  prepared_query_quoted;
	char prepared_query_quote_char;
        int  prepared_query_is_func;
	/* end prepared query stuff */
	struct _sql_bind_info *bind_head;
	struct _sql_param_info *param_head;
	unsigned int param_count;
	int row;
	TDSDYNAMIC *dyn; /* FIXME check if freed */
};

struct _sql_param_info {
	int param_number;
	int param_type;
	int param_bindtype;
	int param_sqltype;
	int param_col_size;
	int param_scale;
	char *varaddr;
	int param_bindlen;
	char *param_lenbind;
	struct _sql_param_info *next;
};
struct _sql_bind_info {
	int column_number;
	int column_bindtype;
	int column_bindlen;
	char *varaddr;
	char *column_lenbind;
	struct _sql_bind_info *next;
};

#ifdef __cplusplus
}
#endif

#endif
