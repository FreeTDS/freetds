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

#include <tdsutil.h>
#include <tds.h>
#include <tdsodbc.h>
#include "odbc_util.h"
#include <time.h>
#include <assert.h>
#include <sqlext.h>

static char  software_version[]   = "$Id: odbc_util.c,v 1.1 2002-05-29 10:58:25 brianb Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

int odbc_set_stmt_query(struct _hstmt *stmt, char *sql, int sql_len)
{
	if (sql_len==SQL_NTS)
		sql_len = strlen(sql);
	else if (sql_len<=0)
		return SQL_ERROR;

	if (stmt->query)
	        free(stmt->query);

	stmt->query = malloc(sql_len+1);
	if (!stmt->query)
		return SQL_ERROR;

	if (sql){
		memcpy(stmt->query, sql, sql_len);
		stmt->query[sql_len] = 0;
	}else{
		stmt->query[0] = 0;
	}
	
	return SQL_SUCCESS;
}


int odbc_set_stmt_prepared_query(struct _hstmt *stmt, char *sql, int sql_len)
{
	if (sql_len==SQL_NTS)
		sql_len = strlen(sql);
	else if (sql_len<=0)
		return SQL_ERROR;

	if (stmt->prepared_query)
	        free(stmt->prepared_query);

	stmt->prepared_query = malloc(sql_len+1);
	if (!stmt->prepared_query)
		return SQL_ERROR;

	if (sql){
		memcpy(stmt->prepared_query, sql, sql_len);
		stmt->prepared_query[sql_len] = 0;
	}else{
		stmt->prepared_query[0] = 0;
	}
	
	return SQL_SUCCESS;
}


struct _sql_param_info * 
odbc_find_param(struct _hstmt *stmt, int param_num)
{
	struct _sql_param_info *cur;

	/* find parameter number n */
	cur = stmt->param_head;
	while (cur) {
		if (cur->param_number==param_num) 
			return cur;
		cur = cur->next;
	}
	return NULL;
}


int odbc_fix_literals(struct _hstmt *stmt)
{
	char *tmp,begin_tag[11];
	char *s, *d, *p;
	int i, quoted = 0, find_end = 0;
	char quote_char;
	int res;

	if (stmt->prepared_query)
	        s=stmt->prepared_query;
	else if (stmt->query)
	        s=stmt->query;
	else
		return SQL_ERROR;

	tmp = malloc(strlen(s)+1);
	if (!tmp)
		return SQL_ERROR;

        d=tmp;
        while (*s) {
		if (!quoted && (*s=='"' || *s=='\'')) {
			quoted = 1;
			quote_char = *s;
		} else if (quoted && *s==quote_char) {
			quoted = 0;
		}
		if (!quoted && find_end && *s=='}') {
			s++; /* ignore the end of tag */
		} else if (!quoted && *s=='{') {
			for (p=s,i=0;*p && *p!=' ';p++) i++;
			if (i>10) {
				/* garbage */
				*d++=*s++;
			} else {
				strncpy(begin_tag, s, i);
				begin_tag[i] = '\0';
				/* printf("begin tag %s\n", begin_tag); */
				s += i;
				find_end = 1;
			}
		} else {
			*d++=*s++;	
		}
        }
	*d='\0';
	if (stmt->prepared_query)
		res = odbc_set_stmt_prepared_query(stmt, tmp, strlen(tmp));
	else
		res = odbc_set_stmt_query(stmt, tmp, strlen(tmp));

	free(tmp);
		
	return SQL_SUCCESS!=res?SQL_ERROR:SQL_SUCCESS;
}


int odbc_get_string_size(int size, char *str)
{
	if (!str) {
		return 0;
	}
	if (size==SQL_NTS) {
		return strlen(str);
	} else {
		return size;
	}
}


SQLSMALLINT odbc_get_client_type(int col_type, int col_size)
{
	/* FIXME finish*/
	switch (col_type) {
	case SYBCHAR:
		return SQL_CHAR;
	case SYBVARCHAR:
		return SQL_VARCHAR;
	case SYBTEXT:
		return SQL_LONGVARCHAR;
	case SYBBIT:
		return SQL_BIT;
	case SYBBITN:
		/* FIXME correct ?*/
		return SQL_BIT;
	case SYBINT8:
		break;
	case SYBINT4:
		return SQL_INTEGER;
	case SYBINT2:
		return SQL_SMALLINT;
	case SYBINT1:
		return SQL_TINYINT;
	case SYBINTN:
		switch(col_size) {
			case 1: return SQL_TINYINT;
			case 2: return SQL_SMALLINT;
			case 4: return SQL_INTEGER;
		}
		break;
	case SYBREAL:
		return SQL_FLOAT;
	case SYBFLT8:
		return SQL_DOUBLE;
	case SYBFLTN:
		switch(col_size) {
			case 4: return SQL_FLOAT;
			case 8: return SQL_DOUBLE;
		}
		break;
	case SYBMONEY:
		return SQL_DOUBLE;
	case SYBMONEY4:
		return SQL_FLOAT;
	case SYBMONEYN:
		break;
	case SYBDATETIME:
		return SQL_TIMESTAMP;
	case SYBDATETIME4:
		return SQL_TIMESTAMP;
	case SYBDATETIMN:
		return SQL_TIMESTAMP;
	case SYBBINARY:
		return SQL_BINARY;
	case SYBIMAGE:
		/* FIXME correct ? */
		return SQL_LONGVARBINARY;
	case SYBVARBINARY:
		return SQL_VARBINARY;
	case SYBNUMERIC:
	case SYBDECIMAL:
		return SQL_BIGINT;
	case SYBNTEXT:
	case SYBVOID:
	case SYBNVARCHAR:
	case XSYBCHAR:
	case XSYBVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
		break;
	case SYBUNIQUE:
		break;
	case SYBVARIANT:
		break;
	}
	return SQL_UNKNOWN_TYPE;
}


