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

#ifndef _tdsguard_cXIvgxMqCzaBT7f54eQeMp_
#define _tdsguard_cXIvgxMqCzaBT7f54eQeMp_

#include <freetds/pushvis.h>

#ifdef __cplusplus
extern "C"
{
#if 0
}
#endif
#endif

typedef union conv_result
{
	/* fixed */
	TDS_TINYINT ti;
	TDS_SMALLINT si;
	TDS_USMALLINT usi;
	TDS_INT i;
	TDS_UINT ui;
	TDS_INT8 bi;
	TDS_UINT8 ubi;
	TDS_FLOAT f;
	TDS_REAL r;
	TDS_MONEY m;
	TDS_MONEY4 m4;
	TDS_DATETIME dt;
	TDS_DATETIME4 dt4;
	TDS_DATETIMEALL dta;
	TDS_TIME time;
	TDS_DATE date;
	TDS_BIGTIME bigtime;
	TDS_BIGDATETIME bigdatetime;
	TDS_NUMERIC n;
	TDS_UNIQUE u;

	/* variable */
	TDS_CHAR *c;
	TDS_CHAR *ib;

	/* sized buffer types */
	struct cc_t {
		TDS_CHAR *c;
		TDS_UINT len;
	} cc;
	struct cb_t {
		TDS_CHAR *ib;
		TDS_UINT len;
	} cb;
}
CONV_RESULT;

/*
 * Failure return codes for tds_convert()
 */
#define TDS_CONVERT_FAIL	-1	/* unspecified failure */
#define TDS_CONVERT_NOAVAIL	-2	/* conversion does not exist */
#define TDS_CONVERT_SYNTAX	-3	/* syntax error in source field */
#define TDS_CONVERT_NOMEM	-4	/* insufficient memory */
#define TDS_CONVERT_OVERFLOW	-5	/* result too large */

/* sized types */
#define TDS_CONVERT_CHAR	256
#define TDS_CONVERT_BINARY	257

unsigned char tds_willconvert(int srctype, int desttype);

TDS_SERVER_TYPE tds_get_null_type(TDS_SERVER_TYPE srctype);
ptrdiff_t tds_char2hex(TDS_CHAR *dest, size_t destlen, const TDS_CHAR * src, size_t srclen);
TDS_INT tds_convert(const TDSCONTEXT *context, int srctype, const void *src, TDS_UINT srclen, int desttype, CONV_RESULT *cr);

size_t tds_strftime(char *buf, size_t maxsize, const char *format, const TDSDATEREC * timeptr, int prec);

#ifdef __cplusplus
#if 0
{
#endif
}
#endif

#include <freetds/popvis.h>

#endif /* _tdsguard_cXIvgxMqCzaBT7f54eQeMp_ */
