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

#if TIME_WITH_SYS_TIME
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <assert.h>
#include <ctype.h>

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */

#include "tds.h"
#include "tdsconvert.h"
#include "replacements.h"
#ifdef DMALLOC
#include <dmalloc.h>
#endif

static char software_version[] = "$Id: convert.c,v 1.138 2004-04-15 19:27:33 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version,
	no_unused_var_warn
};

typedef unsigned short utf16_t;

static TDS_INT tds_convert_int1(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr);
static TDS_INT tds_convert_int2(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr);
static TDS_INT tds_convert_int4(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr);
static TDS_INT tds_convert_int8(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr);
static int string_to_datetime(const char *datestr, int desttype, CONV_RESULT * cr);
static int is_dd_mon_yyyy(char *t);
static int store_dd_mon_yyy_date(char *datestr, struct tds_time *t);

#define test_alloc(x) {if ((x)==NULL) return TDS_CONVERT_NOMEM;}

#define IS_TINYINT(x) ( 0 <= (x) && (x) <= 0xff )
#define IS_SMALLINT(x) ( -32768 <= (x) && (x) <= 32767 )
/* f77: I don't write -2147483648, some compiler seem to have some problem 
 * with this constant although is a valid 32bit value */
#define IS_INT(x) ( (-2147483647l-1l) <= (x) && (x) <= 2147483647l )

/**
 * \ingroup libtds
 * \defgroup convert Conversion
 */

/** \addtogroup convert
 *  \@{ 
 */

/**
 * convert a number in string to a TDSNUMERIC
 * @return sizeof(TDS_NUMERIC) on success, TDS_CONVERT_* failure code on failure 
 */
static int string_to_numeric(const char *instr, const char *pend, CONV_RESULT * cr);

/**
 * convert a zero terminated string to NUMERIC
 * @return sizeof(TDS_NUMERIC) on success, TDS_CONVERT_* failure code on failure 
 */
static int stringz_to_numeric(const char *instr, CONV_RESULT * cr);

/**
 * convert a number in string to TDS_INT 
 * @return TDS_CONVERT_* failure code if failure
 */
static TDS_INT string_to_int(const char *buf, const char *pend, TDS_INT * res);

/**
 * convert a number in string to TDS_INT8
 * @return TDS_CONVERT_* failure code if failure
 */
static TDS_INT string_to_int8(const char *buf, const char *pend, TDS_INT8 * res);


static int store_hour(char *, char *, struct tds_time *);
static int store_time(char *, struct tds_time *);
static int store_yymmdd_date(char *, struct tds_time *);
static int store_monthname(char *, struct tds_time *);
static int store_numeric_date(char *, struct tds_time *);
static int store_mday(char *, struct tds_time *);
static int store_year(int, struct tds_time *);

/* static int days_this_year (int years); */
static int is_timeformat(char *);
static int is_numeric(char *);
static int is_alphabetic(char *);
static int is_ampm(char *);
static int is_monthname(char *);
static int is_numeric_dateformat(char *);

#if 0
static TDS_UINT utf16len(const utf16_t * s);
static const char *tds_prtype(int token);
#endif

const char tds_hex_digits[16] = "0123456789abcdef";

/**
 * Return type suitable for conversions (convert all nullable types to 
 * fixed type)
 * @param srctype type to convert
 * @param colsize size of type
 * @result type for conversion
 */
int
tds_get_conversion_type(int srctype, int colsize)
{
	switch (srctype) {
	case SYBINTN:
		switch (colsize) {
		case 8:
			return SYBINT8;
		case 4:
			return SYBINT4;
		case 2:
			return SYBINT2;
		case 1:
			return SYBINT1;
		}
		break;
	case SYBFLTN:
		switch (colsize) {
		case 8:
			return SYBFLT8;
		case 4:
			return SYBREAL;
		}
		break;
	case SYBDATETIMN:
		switch (colsize) {
		case 8:
			return SYBDATETIME;
		case 4:
			return SYBDATETIME4;
		}
		break;
	case SYBMONEYN:
		switch (colsize) {
		case 8:
			return SYBMONEY;
		case 4:
			return SYBMONEY4;
		}
		break;
		/*
		 * altough tds_convert handle SYBBITN other routine use this
		 * function to retrieve not variant type
		 */
	case SYBBITN:
		return SYBBIT;
		break;
	}
	return srctype;
}

/**
 * Copy a terminated string to result and return len or TDS_CONVERT_NOMEM
 */
static TDS_INT
string_to_result(const char *s, CONV_RESULT * cr)
{
	int len = strlen(s);

	cr->c = (TDS_CHAR *) malloc(len + 1);
	test_alloc(cr->c);
	memcpy(cr->c, s, len + 1);
	return len;
}

/**
 * Copy binary data to to result and return len or TDS_CONVERT_NOMEM
 */
static TDS_INT
binary_to_result(const void *data, size_t len, CONV_RESULT * cr)
{
	cr->ib = (TDS_CHAR *) malloc(len);
	test_alloc(cr->ib);
	memcpy(cr->ib, data, len);
	return len;
}


#define CASE_ALL_CHAR \
	SYBCHAR: case SYBVARCHAR: case SYBTEXT: case XSYBCHAR: case XSYBVARCHAR
#define CASE_ALL_BINARY \
	SYBBINARY: case SYBVARBINARY: case SYBIMAGE: case XSYBBINARY: case XSYBVARBINARY

/* TODO implement me */
/*
static TDS_INT 
tds_convert_ntext(int srctype,TDS_CHAR *src,TDS_UINT srclen,
      int desttype, CONV_RESULT *cr)
{
      return TDS_CONVERT_NOAVAIL;
}
*/

static TDS_INT
tds_convert_binary(int srctype, const TDS_UCHAR * src, TDS_INT srclen, int desttype, CONV_RESULT * cr)
{
	int cplen;
	int s;
	char *c;

	switch (desttype) {
	case CASE_ALL_CHAR:

		/* NOTE: Do not prepend 0x to string.  
		 * The libraries all expect a bare string, without a 0x prefix. 
		 * Applications such as isql and query analyzer provide the "0x" prefix. */

		/* 2 * source length + 1 for terminator */

		cr->c = (TDS_CHAR *) malloc((srclen * 2) + 1);
		test_alloc(cr->c);

		c = cr->c;

		for (s = 0; s < srclen; s++) {
			*c++ = tds_hex_digits[src[s]>>4];
			*c++ = tds_hex_digits[src[s]&0xF];
		}

		*c = '\0';
		return (srclen * 2);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, srclen, cr);
		break;
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBREAL:
	case SYBFLT8:
		cplen = tds_get_size_by_type(desttype);
		if (cplen <= srclen)
			return binary_to_result(src, cplen, cr);
		cr->ib = (TDS_CHAR *) malloc(cplen);
		test_alloc(cr->ib);
		memcpy(cr->ib, src, srclen);
		memset(cr->ib + srclen, 0, cplen - srclen);
		return cplen;
		break;

		/* conversions not allowed */
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:

		/* TODO should we do some test for these types or work as ints ?? */
	case SYBDECIMAL:
	case SYBNUMERIC:
	case SYBBIT:
	case SYBBITN:

	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_char(int srctype, const TDS_CHAR * src, TDS_UINT srclen, int desttype, CONV_RESULT * cr)
{
	int i, j;
	unsigned char hex1;

	TDS_INT8 mymoney;
	TDS_INT mymoney4;
	char mynumber[39];

	const char *ptr, *pend;
	int point_found, places;
	TDS_INT tds_i;
	TDS_INT8 tds_i8;
	TDS_INT rc;

	switch (desttype) {
	case CASE_ALL_CHAR:
		cr->c = (TDS_CHAR *) malloc(srclen + 1);
		test_alloc(cr->c);
		memcpy(cr->c, src, srclen);
		cr->c[srclen] = 0;
		return srclen;
		break;

	case CASE_ALL_BINARY:

		/* skip leading "0x" or "0X" */

		if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) {
			src += 2;
			srclen -= 2;
		}

		/* ignore trailing blanks and nulls */
		/* FIXME is good to ignore null ?? */
		while (srclen > 0 && (src[srclen - 1] == ' ' || src[srclen - 1] == '\0'))
			--srclen;

		/* a binary string output will be half the length of */
		/* the string which represents it in hexadecimal     */

		/* if srclen if odd we must add a "0" before ... */
		j = 0;		/* number where to start converting */
		if (srclen & 1) {
			++srclen;
			j = 1;
			--src;
		}
		cr->ib = (TDS_CHAR *) malloc(srclen / 2);
		test_alloc(cr->ib);

#if 0
		/* hey, I know this looks a bit cruddy,   */
		/* and I'm sure it can all be done in one */
		/* statement, so go on, make my day!      */

		for (i = 0, j = 0; i < srclen; i++, j++) {

			inp = src[i];
			if (inp > 47 && inp < 58)	/* '0' thru '9' */
				hex1 = inp - 48;
			else if (inp > 96 && inp < 103)	/* 'a' thru 'f' */
				hex1 = inp - 87;
			else if (inp > 64 && inp < 71)	/* 'A' thru 'F' */
				hex1 = inp - 55;
			else {
				fprintf(stderr,
					"error_handler:  Attempt to convert data stopped by syntax error in source field \n");
				return;
			}

			hex1 = hex1 << 4;

			i++;

			inp = src[i];
			if (inp > 47 && inp < 58)	/* '0' thru '9' */
				hex1 = hex1 | (inp - 48);
			else if (inp > 96 && inp < 103)	/* 'a' thru 'f' */
				hex1 = hex1 | (inp - 87);
			else if (inp > 64 && inp < 71)	/* 'A' thru 'F' */
				hex1 = hex1 | (inp - 55);
			else {
				fprintf(stderr,
					"error_handler:  Attempt to convert data stopped by syntax error in source field \n");
				return;
			}

			cr->ib[j] = hex1;
		}
#else
		for (i = srclen; --i >= j;) {
			hex1 = src[i];

			if ('0' <= hex1 && hex1 <= '9')
				hex1 &= 0x0f;
			else {
				hex1 &= 0x20 ^ 0xff;	/* mask off 0x20 to ensure upper case */
				if ('A' <= hex1 && hex1 <= 'F') {
					hex1 -= ('A' - 10);
				} else {
					tdsdump_log(TDS_DBG_INFO1,
						    "error_handler:  attempt to convert data stopped by syntax error in source field \n");
					return TDS_CONVERT_SYNTAX;
				}
			}
			assert(hex1 < 0x10);

			if (i & 1)
				cr->ib[i / 2] = hex1;
			else
				cr->ib[i / 2] |= hex1 << 4;
		}
#endif
		return srclen / 2;
		break;
	case SYBINT1:
		if ((rc = string_to_int(src, src + srclen, &tds_i)) < 0)
			return rc;
		if (!IS_TINYINT(tds_i))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = tds_i;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		if ((rc = string_to_int(src, src + srclen, &tds_i)) < 0)
			return rc;
		if (!IS_SMALLINT(tds_i))
			return TDS_CONVERT_OVERFLOW;
		cr->si = tds_i;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		if ((rc = string_to_int(src, src + srclen, &tds_i)) < 0)
			return rc;
		cr->i = tds_i;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		if ((rc = string_to_int8(src, src + srclen, &tds_i8)) < 0)
			return rc;
		cr->bi = tds_i8;
		return sizeof(TDS_INT8);
		break;
	case SYBFLT8:
		/* TODO check syntax and overflow */
		cr->f = atof(src);
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		/* TODO check syntax and overflow */
		cr->r = atof(src);
		return sizeof(TDS_REAL);
		break;
	case SYBBIT:
	case SYBBITN:
		if ((rc = string_to_int(src, src + srclen, &tds_i)) < 0)
			return rc;
		cr->ti = tds_i ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBMONEY:
	case SYBMONEY4:

		/* TODO code similar to string_to_numeric... */
		i = 0;
		places = 0;
		point_found = 0;
		pend = src + srclen;

		/* skip leading blanks */
		for (ptr = src; ptr != pend && *ptr == ' '; ++ptr);

		switch (ptr != pend ? *ptr : 0) {
		case '-':
			mynumber[i++] = '-';
			/* fall through */
		case '+':
			ptr++;
			for (; ptr != pend && *ptr == ' '; ++ptr);
			break;
		}

		for (; ptr != pend; ptr++) {	/* deal with the rest */
			if (isdigit((unsigned char) *ptr)) {	/* it's a number */
				/* no more than 4 decimal digits */
				if (places < 4)
					mynumber[i++] = *ptr;
				/* assure not buffer overflow */
				if (i == 30)
					return TDS_CONVERT_OVERFLOW;
				if (point_found) {	/* if we passed a decimal point */
					/* count digits after that point  */
					++places;
				}
			} else if (*ptr == '.') {	/* found a decimal point */
				if (point_found)	/* already had one. error */
					return TDS_CONVERT_SYNTAX;
				point_found = 1;
			} else	/* first invalid character */
				return TDS_CONVERT_SYNTAX;	/* lose the rest.          */
		}
		for (j = places; j < 4; j++)
			mynumber[i++] = '0';
		mynumber[i] = 0;

		/* FIXME overflow not handled */
		if (desttype == SYBMONEY) {
			mymoney = atoll(mynumber);
			memcpy(&(cr->m), &mymoney, sizeof(TDS_MONEY));
			return sizeof(TDS_MONEY);
		} else {
			mymoney4 = atol(mynumber);
			memcpy(&(cr->m4), &mymoney4, sizeof(TDS_MONEY4));
			return sizeof(TDS_MONEY4);
		}
		break;
	case SYBDATETIME:
		return string_to_datetime(src, SYBDATETIME, cr);
		break;
	case SYBDATETIME4:
		return string_to_datetime(src, SYBDATETIME4, cr);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		return string_to_numeric(src, src + srclen, cr);
		break;
	case SYBUNIQUE:{
			unsigned n = 0;
			char c;

			/* 
			 * format:	 XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX 
			 * or 		{XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} 
			 * or 		 XXXXXXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX  
			 * or 		{XXXXXXXX-XXXX-XXXX-XXXXXXXXXXXXXXXX} 
			 * SQL seem to ignore the additional braces.
			 */
			if (srclen < (32 + 3))
				return TDS_CONVERT_SYNTAX;

			if (src[0] == '{') {
				int last = (src[8+1 + 4+1 + 4+1 + 4 + 1] == '-') ? 32+4+1 : 32+3+1;
				if (srclen <= last || src[last] != '}')
					return TDS_CONVERT_SYNTAX;
				++src;
			}

			/* 
			 * Test each character and get value.  
			 * sscanf works if the number terminates with less digits. 
			 */
			for (i = 0; i < 32 + 3; ++i) {
				c = src[i];
				switch (i) {
				case 8:
					if (c != '-')
						return TDS_CONVERT_SYNTAX;
					cr->u.Data1 = n;
					n = 0;
					break;
				case 8+1 + 4:
					if (c != '-')
						return TDS_CONVERT_SYNTAX;
					cr->u.Data2 = n;
					n = 0;
					break;
				case 8+1 + 4+1 + 4:
					if (c != '-')
						return TDS_CONVERT_SYNTAX;
					cr->u.Data3 = n;
					n = 0;
					break;
				case 8+1 + 4+1 + 4+1 + 4:
					/* skip last (optional) dash */
					if (c == '-') {
						if (--srclen < 32 + 3)
							return TDS_CONVERT_SYNTAX;
						c = (++src)[i];
					}
					/* fall through */
				default:
					n = n << 4;
					if (c >= '0' && c <= '9')
						n += c - '0';
					else {
						c &= 0x20 ^ 0xff;
						if (c >= 'A' && c <= 'F')
							n += c - ('A' - 10);
						else
							return TDS_CONVERT_SYNTAX;
					}
					if (i > (16 + 2) && !(i & 1)) {
						cr->u.Data4[(i >> 1) - 10] = n;
						n = 0;
					}
				}
			}
		}
		return sizeof(TDS_UNIQUE);
	default:
		return TDS_CONVERT_NOAVAIL;
		break;
	}			/* end switch */
}				/* tds_convert_char */

static TDS_INT
tds_convert_bit(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	int canonic = src[0] ? 1 : 0;

	switch (desttype) {
	case CASE_ALL_CHAR:
		cr->c = (TDS_CHAR *) malloc(2);
		test_alloc(cr->c);
		cr->c[0] = '0' + canonic;
		cr->c[1] = 0;
		return 1;
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, 1, cr);
		break;
	case SYBINT1:
		cr->ti = canonic;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		cr->si = canonic;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		cr->i = canonic;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = canonic;
		return sizeof(TDS_INT8);
		break;
	case SYBFLT8:
		cr->f = canonic;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = (TDS_REAL) canonic;
		return sizeof(TDS_REAL);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = src[0];
		return sizeof(TDS_TINYINT);
		break;
	case SYBMONEY:
	case SYBMONEY4:
		return tds_convert_int1(SYBINT1, (src[0]) ? "\1" : "\0", desttype, cr);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		return stringz_to_numeric(canonic ? "1" : "0", cr);
		break;

		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_int1(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	TDS_TINYINT buf;
	TDS_CHAR tmp_str[5];

	memcpy(&buf, src, sizeof(buf));
	switch (desttype) {
	case CASE_ALL_CHAR:
		sprintf(tmp_str, "%d", buf);
		return string_to_result(tmp_str, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, 1, cr);
		break;
	case SYBINT1:
		cr->ti = buf;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		cr->si = buf;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		cr->i = buf;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = buf;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = buf ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBFLT8:
		cr->f = buf;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = buf;
		return sizeof(TDS_REAL);
		break;
	case SYBMONEY4:
		cr->m4.mny4 = buf * 10000;
		return sizeof(TDS_MONEY4);
		break;
	case SYBMONEY:
		cr->m.mny = buf * 10000;
		return sizeof(TDS_MONEY);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		sprintf(tmp_str, "%d", buf);
		return stringz_to_numeric(tmp_str, cr);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_int2(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	TDS_SMALLINT buf;
	TDS_CHAR tmp_str[16];

	memcpy(&buf, src, sizeof(buf));
	switch (desttype) {
	case CASE_ALL_CHAR:
		sprintf(tmp_str, "%d", buf);
		return string_to_result(tmp_str, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, 2, cr);
		break;
	case SYBINT1:
		if (!IS_TINYINT(buf))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) buf;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		cr->si = buf;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		cr->i = buf;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = buf;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = buf ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBFLT8:
		cr->f = buf;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = buf;
		return sizeof(TDS_REAL);
		break;
	case SYBMONEY4:
		cr->m4.mny4 = buf * 10000;
		return sizeof(TDS_MONEY4);
		break;
	case SYBMONEY:
		cr->m.mny = buf * 10000;
		return sizeof(TDS_MONEY);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		sprintf(tmp_str, "%d", buf);
		return stringz_to_numeric(tmp_str, cr);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_int4(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	TDS_INT buf;
	TDS_CHAR tmp_str[16];

	memcpy(&buf, src, sizeof(buf));
	switch (desttype) {
	case CASE_ALL_CHAR:
		sprintf(tmp_str, "%d", buf);
		return string_to_result(tmp_str, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, 4, cr);
		break;
	case SYBINT1:
		if (!IS_TINYINT(buf))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = buf;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		if (!IS_SMALLINT(buf))
			return TDS_CONVERT_OVERFLOW;
		cr->si = buf;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		cr->i = buf;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = buf;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = buf ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBFLT8:
		cr->f = buf;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = (TDS_REAL) buf;
		return sizeof(TDS_REAL);
		break;
	case SYBMONEY4:
		if (buf > 214748 || buf < -214748)
			return TDS_CONVERT_OVERFLOW;
		cr->m4.mny4 = buf * 10000;
		return sizeof(TDS_MONEY4);
		break;
	case SYBMONEY:
		cr->m.mny = (TDS_INT8) buf *10000;

		return sizeof(TDS_MONEY);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		sprintf(tmp_str, "%d", buf);
		return stringz_to_numeric(tmp_str, cr);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_int8(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	TDS_INT8 buf;
	TDS_CHAR tmp_str[24];

	memcpy(&buf, src, sizeof(buf));
	switch (desttype) {
	case CASE_ALL_CHAR:
		/* TODO: fix for all platform. Search for lltoa/_i64toa */
#ifndef WIN32
# if SIZEOF_LONG < 8
		sprintf(tmp_str, "%lld", buf);
# else
		sprintf(tmp_str, "%ld", buf);
# endif
#else
		_i64toa(buf, tmp_str, 10);
#endif
		return string_to_result(tmp_str, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_INT8), cr);
		break;
	case SYBINT1:
		if (!IS_TINYINT(buf))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) buf;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		if (!IS_SMALLINT(buf))
			return TDS_CONVERT_OVERFLOW;
		cr->si = (TDS_SMALLINT) buf;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		if (!IS_INT(buf))
			return TDS_CONVERT_OVERFLOW;
		cr->i = (TDS_INT) buf;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = buf;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = buf ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBFLT8:
		cr->f = (TDS_FLOAT) buf;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = (TDS_REAL) buf;
		return sizeof(TDS_REAL);
		break;
	case SYBMONEY4:
		if (buf > 214748 || buf < -214748)
			return TDS_CONVERT_OVERFLOW;
		cr->m4.mny4 = (TDS_INT) (buf * 10000);
		return sizeof(TDS_MONEY4);
		break;
	case SYBMONEY:
		/* TODO check overflow */
		cr->m.mny = buf * 10000;
		return sizeof(TDS_MONEY);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		/* TODO portability problem. See above */
#ifndef WIN32
# if SIZEOF_LONG < 8
		sprintf(tmp_str, "%lld", buf);
# else
		sprintf(tmp_str, "%ld", buf);
# endif
#else
		_i64toa(buf, tmp_str, 10);
#endif
		return stringz_to_numeric(tmp_str, cr);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_numeric(int srctype, const TDS_NUMERIC * src, TDS_INT srclen, int desttype, CONV_RESULT * cr)
{
	char tmpstr[MAXPRECISION];
	long i;

	switch (desttype) {
	case CASE_ALL_CHAR:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		return string_to_result(tmpstr, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_NUMERIC), cr);
		break;
	case SYBINT1:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		/* TODO what happen if numeric is too big ?? */
		i = atoi(tmpstr);
		if (!IS_TINYINT(i))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) i;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		i = atoi(tmpstr);
		if (!IS_SMALLINT(i))
			return TDS_CONVERT_OVERFLOW;
		cr->si = (TDS_SMALLINT) i;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		i = atoi(tmpstr);
		if (!IS_INT(i))
			return TDS_CONVERT_OVERFLOW;
		cr->i = i;
		return 4;
		break;
	case SYBINT8:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		/* TODO check for overflow */
		cr->bi = atoll(tmpstr);
		return 8;
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = 0;
		for (i = tds_numeric_bytes_per_prec[src->precision]; --i > 0;)
			if (src->array[i] != 0) {
				cr->ti = 1;
				break;
			}
		return 1;
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		memcpy(&(cr->n), src, sizeof(TDS_NUMERIC));
		return sizeof(TDS_NUMERIC);
		break;
	case SYBFLT8:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		cr->f = atof(tmpstr);
		return 8;
		break;
	case SYBREAL:
		if (tds_numeric_to_string(src, tmpstr) < 0)
			return TDS_CONVERT_FAIL;
		cr->r = atof(tmpstr);
		return 4;
		break;
		/* TODO conversions to money */
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_money4(int srctype, const TDS_CHAR * src, int srclen, int desttype, CONV_RESULT * cr)
{
	TDS_MONEY4 mny;
	long dollars, fraction;
	char tmp_str[33];
	char *p;

	memcpy(&mny, src, sizeof(mny));
	switch (desttype) {
	case CASE_ALL_CHAR:
		/*
		 * round to 2 decimal digits
		 * rounding with dollars = (mny.mny4 + 5000) /10000
		 * can give arithmetic overflow so I use
		 * dollars = (mny.mny4/50 + 1)/2 
		 */
		/* TODO round also all conversions to int and from money ?? */
		p = tmp_str;
		if (mny.mny4 < 0) {
			*p++ = '-';
			dollars = (-mny.mny4 / 50 + 1 ) / 2;
		} else {
			dollars = (mny.mny4 / 50 + 1 ) / 2;
		}
		/* print only 2 decimal digits as server does */
		sprintf(p, "%ld.%02lu", dollars / 100, dollars % 100);
		return string_to_result(tmp_str, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_MONEY4), cr);
		break;
	case SYBINT1:
		dollars = mny.mny4 / 10000;
		if (!IS_TINYINT(dollars))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) dollars;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		dollars = mny.mny4 / 10000;
		if (!IS_SMALLINT(dollars))
			return TDS_CONVERT_OVERFLOW;
		cr->si = (TDS_SMALLINT) dollars;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		cr->i = mny.mny4 / 10000;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = mny.mny4 / 10000;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = mny.mny4 ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBFLT8:
		cr->f = ((TDS_FLOAT) mny.mny4) / 10000.0;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = (TDS_REAL) (mny.mny4 / 10000.0);
		return sizeof(TDS_REAL);
		break;
	case SYBMONEY:
		cr->m.mny = (TDS_INT8) mny.mny4;
		return sizeof(TDS_MONEY);
		break;
	case SYBMONEY4:
		memcpy(&(cr->m4), src, sizeof(TDS_MONEY4));
		return sizeof(TDS_MONEY4);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
		break;
	case SYBDECIMAL:
	case SYBNUMERIC:
		dollars = mny.mny4 / 10000;
		fraction = mny.mny4 % 10000;
		if (fraction < 0) {
			fraction = -fraction;
		}
		sprintf(tmp_str, "%ld.%04lu", dollars, fraction);
		return stringz_to_numeric(tmp_str, cr);
	default:
		return TDS_CONVERT_NOAVAIL;
		break;
	}
	return TDS_CONVERT_FAIL;
}

static TDS_INT
tds_convert_money(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	char *s;

	TDS_INT8 mymoney, dollars;
	char tmpstr[64];

	tdsdump_log(TDS_DBG_FUNC, "%L tds_convert_money()\n");
#if defined(WORDS_BIGENDIAN) || !defined(HAVE_INT64)
	memcpy(&mymoney, src, sizeof(TDS_INT8));
#else
	memcpy(((char *) &mymoney) + 4, src, 4);
	memcpy(&mymoney, src + 4, 4);
#endif

	switch (desttype) {
	case CASE_ALL_CHAR:
		s = tds_money_to_string((const TDS_MONEY *) src, tmpstr);
		return string_to_result(s, cr);
		break;

	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_MONEY), cr);
		break;
	case SYBINT1:
		/* TODO: round ?? */
		dollars = mymoney / 10000;
		if (!IS_TINYINT(dollars))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) dollars;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		dollars = mymoney / 10000;
		if (!IS_SMALLINT(dollars))
			return TDS_CONVERT_OVERFLOW;
		cr->si = (TDS_SMALLINT) dollars;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		dollars = mymoney / 10000;
		if (!IS_INT(dollars))
			return TDS_CONVERT_OVERFLOW;
		cr->i = (TDS_INT) dollars;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		cr->bi = mymoney / 10000;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = mymoney ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;
	case SYBFLT8:
		cr->f = ((TDS_FLOAT) mymoney) / 10000.0;
		return sizeof(TDS_FLOAT);
		break;
	case SYBREAL:
		cr->r = (TDS_REAL) (mymoney / 10000.0);
		return sizeof(TDS_REAL);
		break;
	case SYBMONEY4:
		if (!IS_INT(mymoney))
			return TDS_CONVERT_OVERFLOW;
		cr->m4.mny4 = (TDS_INT) mymoney;
		return sizeof(TDS_MONEY4);
		break;
	case SYBMONEY:
		cr->m.mny = mymoney;
		return sizeof(TDS_MONEY);
		break;
	case SYBDECIMAL:
	case SYBNUMERIC:
		s = tds_money_to_string((const TDS_MONEY *) src, tmpstr);
		return stringz_to_numeric(tmpstr, cr);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_datetime(TDSCONTEXT * tds_ctx, int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{

	unsigned int dt_days, dt_time;

	char whole_date_string[30];
	TDSDATEREC when;

	switch (desttype) {
	case CASE_ALL_CHAR:
		if (!src) {
			cr->c = (TDS_CHAR *) malloc(1);
			test_alloc(cr->c);
			*(cr->c) = '\0';
			return 0;
		}

		memset(&when, 0, sizeof(when));

		tds_datecrack(SYBDATETIME, src, &when);
		tds_strftime(whole_date_string, sizeof(whole_date_string), tds_ctx->locale->date_fmt, &when);

		return string_to_result(whole_date_string, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_DATETIME), cr);
		break;
	case SYBDATETIME:
		memcpy(&dt_days, src, 4);
		memcpy(&dt_time, src + 4, 4);
		cr->dt.dtdays = dt_days;
		cr->dt.dttime = dt_time;
		return sizeof(TDS_DATETIME);
		break;
	case SYBDATETIME4:
		memcpy(&dt_days, src, 4);
		memcpy(&dt_time, src + 4, 4);
		cr->dt4.days = dt_days;
		cr->dt4.minutes = (dt_time / 300) / 60;
		return sizeof(TDS_DATETIME4);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBBIT:
	case SYBBITN:
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBNUMERIC:
	case SYBDECIMAL:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}


/*
static int days_this_year (int years)
{
int year;

   year = 1900 + years;
   if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
      return 366;
   else
      return 365;
}
*/

static TDS_INT
tds_convert_datetime4(TDSCONTEXT * tds_ctx, int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{

	TDS_USMALLINT dt_days, dt_mins;

	char whole_date_string[30];
	TDSDATEREC when;

	switch (desttype) {
	case CASE_ALL_CHAR:
		if (!src) {
			cr->c = (TDS_CHAR *) malloc(1);
			test_alloc(cr->c);
			*(cr->c) = '\0';
			return 0;
		} else {

			memset(&when, 0, sizeof(when));

			tds_datecrack(SYBDATETIME4, src, &when);
			tds_strftime(whole_date_string, sizeof(whole_date_string), tds_ctx->locale->date_fmt, &when);

			return string_to_result(whole_date_string, cr);
		}
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_DATETIME4), cr);
		break;
	case SYBDATETIME:
		memcpy(&dt_days, src, 2);
		memcpy(&dt_mins, src + 2, 2);
		cr->dt.dtdays = dt_days;
		cr->dt.dttime = (dt_mins * 60) * 300;
		return sizeof(TDS_DATETIME);
		break;
	case SYBDATETIME4:
		memcpy(&dt_days, src, 2);
		memcpy(&dt_mins, src + 2, 2);
		cr->dt4.days = dt_days;
		cr->dt4.minutes = dt_mins;
		return sizeof(TDS_DATETIME4);
		break;
		/* conversions not allowed */
	case SYBUNIQUE:
	case SYBBIT:
	case SYBBITN:
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBNUMERIC:
	case SYBDECIMAL:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_real(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	TDS_REAL the_value;

/* FIXME how many big should be this buffer ?? */
	char tmp_str[128];
	TDS_INT mymoney4;
	TDS_INT8 mymoney;

	memcpy(&the_value, src, 4);

	switch (desttype) {
	case CASE_ALL_CHAR:
		sprintf(tmp_str, "%.7g", the_value);
		return string_to_result(tmp_str, cr);
		break;

	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_REAL), cr);
		break;
	case SYBINT1:
		if (!IS_TINYINT(the_value))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) the_value;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		if (!IS_SMALLINT(the_value))
			return TDS_CONVERT_OVERFLOW;
		cr->si = (TDS_SMALLINT) the_value;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		if (!IS_INT(the_value))
			return TDS_CONVERT_OVERFLOW;
		cr->i = (TDS_INT) the_value;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		/* TODO check overflow */
		cr->bi = the_value;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = the_value ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;

	case SYBFLT8:
		cr->f = the_value;
		return sizeof(TDS_FLOAT);
		break;

	case SYBREAL:
		cr->r = the_value;
		return sizeof(TDS_REAL);
		break;

	case SYBMONEY:
		/* TODO check overflow */
		mymoney = the_value * 10000;
		memcpy(&(cr->m), &mymoney, sizeof(TDS_MONEY));
		return sizeof(TDS_MONEY);
		break;

	case SYBMONEY4:
		/* TODO check overflow */
		mymoney4 = the_value * 10000;
		memcpy(&(cr->m4), &mymoney4, sizeof(TDS_MONEY4));
		return sizeof(TDS_MONEY4);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		sprintf(tmp_str, "%.*f", cr->n.scale, the_value);
		return stringz_to_numeric(tmp_str, cr);
		break;
		/* not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_flt8(int srctype, const TDS_CHAR * src, int desttype, CONV_RESULT * cr)
{
	TDS_FLOAT the_value;
	char tmp_str[25];

	memcpy(&the_value, src, 8);
	switch (desttype) {
	case CASE_ALL_CHAR:
		sprintf(tmp_str, "%.15g", the_value);
		return string_to_result(tmp_str, cr);
		break;

	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_FLOAT), cr);
		break;
	case SYBINT1:
		if (!IS_TINYINT(the_value))
			return TDS_CONVERT_OVERFLOW;
		cr->ti = (TDS_TINYINT) the_value;
		return sizeof(TDS_TINYINT);
		break;
	case SYBINT2:
		if (!IS_SMALLINT(the_value))
			return TDS_CONVERT_OVERFLOW;
		cr->si = (TDS_SMALLINT) the_value;
		return sizeof(TDS_SMALLINT);
		break;
	case SYBINT4:
		if (!IS_INT(the_value))
			return TDS_CONVERT_OVERFLOW;
		cr->i = (TDS_INT) the_value;
		return sizeof(TDS_INT);
		break;
	case SYBINT8:
		/* TODO check overflow */
		cr->bi = the_value;
		return sizeof(TDS_INT8);
		break;
	case SYBBIT:
	case SYBBITN:
		cr->ti = the_value ? 1 : 0;
		return sizeof(TDS_TINYINT);
		break;

	case SYBMONEY:
		/* TODO check overflow */
		cr->m.mny = (TDS_INT8) the_value *10000.0;

		return sizeof(TDS_MONEY);
		break;
	case SYBMONEY4:
		/* TODO check overflow */
		cr->m4.mny4 = the_value * 10000.0;
		return sizeof(TDS_MONEY4);
		break;
	case SYBREAL:
		/* TODO check overflow */
		cr->r = the_value;
		return sizeof(TDS_REAL);
		break;
	case SYBFLT8:
		cr->f = the_value;
		return sizeof(TDS_FLOAT);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		sprintf(tmp_str, "%.15g", the_value);
		return stringz_to_numeric(tmp_str, cr);
		break;
		/* not allowed */
	case SYBUNIQUE:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

static TDS_INT
tds_convert_unique(int srctype, const TDS_CHAR * src, TDS_INT srclen, int desttype, CONV_RESULT * cr)
{

/* Raw data is equivalent to structure and always aligned, so this cast 
   is portable */

	const TDS_UNIQUE *u = (const TDS_UNIQUE *) src;
	char buf[37];

	switch (desttype) {
	case CASE_ALL_CHAR:
		sprintf(buf, "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
			(int) u->Data1, (int) u->Data2, (int) u->Data3,
			u->Data4[0], u->Data4[1], u->Data4[2], u->Data4[3], u->Data4[4], u->Data4[5], u->Data4[6], u->Data4[7]);
		return string_to_result(buf, cr);
		break;
	case CASE_ALL_BINARY:
		return binary_to_result(src, sizeof(TDS_UNIQUE), cr);
		break;
	case SYBUNIQUE:
		/* Here we can copy raw to structure because we adjust
		 * byte order in tds_swap_datatype */
		memcpy(&(cr->u), src, sizeof(TDS_UNIQUE));
		return sizeof(TDS_UNIQUE);
		break;
		/* no not warning for not convertible types */
	case SYBBIT:
	case SYBBITN:
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBDATETIME:
	case SYBDATETIMN:
	case SYBREAL:
	case SYBFLT8:
	default:
		break;
	}
	return TDS_CONVERT_NOAVAIL;
}

/**
 * tds_convert
 * convert a type to another.
 * If you convert to SYBDECIMAL/SYBNUMERIC you MUST initialize precision 
 * and scale of cr.
 * Do not expect string to be zero terminated. Databases support zero inside
 * string. Doing strlen on result may result on data loss or even core.
 * Use memcpy to copy destination using length returned.
 * This function do not handle NULL, srclen should be >0, if not undefinited 
 * behaviour...
 * @param tds_ctx  context (used in conversion to data and to return messages)
 * @param srctype  type of source
 * @param src      pointer to source data to convert
 * @param srclen   length in bytes of source (not counting terminator or strings)
 * @param desttype type of destination
 * @param cr       structure to hold result
 * @return length of result or TDS_CONVERT_* failure code on failure. All TDS_CONVERT_* constants are <0.
 */
TDS_INT
tds_convert(TDSCONTEXT * tds_ctx, int srctype, const TDS_CHAR * src, TDS_UINT srclen, int desttype, CONV_RESULT * cr)
{
	TDS_INT length = 0;

	assert(srclen >= 0 && srclen <= 2147483647u);

	switch (srctype) {
	case CASE_ALL_CHAR:
		length = tds_convert_char(srctype, src, srclen, desttype, cr);
		break;
	case SYBMONEY4:
		length = tds_convert_money4(srctype, src, srclen, desttype, cr);
		break;
	case SYBMONEY:
		length = tds_convert_money(srctype, src, desttype, cr);
		break;
	case SYBNUMERIC:
	case SYBDECIMAL:
		length = tds_convert_numeric(srctype, (const TDS_NUMERIC *) src, srclen, desttype, cr);
		break;
	case SYBBIT:
	case SYBBITN:
		length = tds_convert_bit(srctype, src, desttype, cr);
		break;
	case SYBINT1:
		length = tds_convert_int1(srctype, src, desttype, cr);
		break;
	case SYBINT2:
		length = tds_convert_int2(srctype, src, desttype, cr);
		break;
	case SYBINT4:
		length = tds_convert_int4(srctype, src, desttype, cr);
		break;
	case SYBINT8:
		length = tds_convert_int8(srctype, src, desttype, cr);
		break;
	case SYBREAL:
		length = tds_convert_real(srctype, src, desttype, cr);
		break;
	case SYBFLT8:
		length = tds_convert_flt8(srctype, src, desttype, cr);
		break;
	case SYBDATETIME:
		length = tds_convert_datetime(tds_ctx, srctype, src, desttype, cr);
		break;
	case SYBDATETIME4:
		length = tds_convert_datetime4(tds_ctx, srctype, src, desttype, cr);
		break;
	case SYBLONGBINARY:
	case CASE_ALL_BINARY:
		length = tds_convert_binary(srctype, (const TDS_UCHAR *) src, srclen, desttype, cr);
		break;
	case SYBUNIQUE:
		length = tds_convert_unique(srctype, src, srclen, desttype, cr);
		break;
	case SYBNVARCHAR:
	case SYBNTEXT:
	default:
		return TDS_CONVERT_NOAVAIL;
		break;
	}

/* fix MONEY case */
#if !defined(WORDS_BIGENDIAN) && defined(HAVE_INT64)
	if (length > 0 && desttype == SYBMONEY) {
		cr->m.mny = ((TDS_UINT8) cr->m.mny) >> 32 | (cr->m.mny << 32);
	}
#endif
	return length;
}

static int
string_to_datetime(const char *instr, int desttype, CONV_RESULT * cr)
{
	enum states
	{ GOING_IN_BLIND,
		PUT_NUMERIC_IN_CONTEXT,
		DOING_ALPHABETIC_DATE,
		STRING_GARBLED
	};

	char *in;
	char *tok;
	char *lasts;
	char last_token[32];
	int monthdone = 0;
	int yeardone = 0;
	int mdaydone = 0;

	struct tds_time mytime;
	struct tds_time *t;

	unsigned int dt_time;
	TDS_INT dt_days;
	int i;

	int current_state;

	memset(&mytime, '\0', sizeof(struct tds_time));
	mytime.tm_mday = 1;
	t = &mytime;

	in = (char *) malloc(strlen(instr) + 1);
	test_alloc(in);
	strcpy(in, instr);

	tok = strtok_r(in, " ,", &lasts);

	current_state = GOING_IN_BLIND;

	while (tok != (char *) NULL) {

		tdsdump_log(TDS_DBG_INFO1, "string_to_datetime: current_state = %d\n", current_state);
		switch (current_state) {
		case GOING_IN_BLIND:
			/* If we have no idea of current context, then if we have */
			/* encountered a purely alphabetic string, it MUST be an  */
			/* alphabetic month name or prefix...                     */

			if (is_alphabetic(tok)) {
				tdsdump_log(TDS_DBG_INFO1, "string_to_datetime: is_alphabetic\n");
				if (is_monthname(tok)) {
					store_monthname(tok, t);
					monthdone++;
					current_state = DOING_ALPHABETIC_DATE;
				} else {
					current_state = STRING_GARBLED;
				}
			}

			/* ...whereas if it is numeric, it could be a number of   */
			/* things...                                              */

			else if (is_numeric(tok)) {
				tdsdump_log(TDS_DBG_INFO1, "string_to_datetime: is_numeric\n");
				switch (strlen(tok)) {
					/* in this context a 4 character numeric can   */
					/* ONLY be the year part of an alphabetic date */

				case 4:
					store_year(atoi(tok), t);
					yeardone++;
					current_state = DOING_ALPHABETIC_DATE;
					break;

					/* whereas these could be the hour part of a   */
					/* time specification ( 4 PM ) or the leading  */
					/* day part of an alphabetic date ( 15 Jan )   */

				case 2:
				case 1:
					strcpy(last_token, tok);
					current_state = PUT_NUMERIC_IN_CONTEXT;
					break;

					/* this must be a [YY]YYMMDD date             */

				case 6:
				case 8:
					if (store_yymmdd_date(tok, t))
						current_state = GOING_IN_BLIND;
					else
						current_state = STRING_GARBLED;
					break;

					/* anything else is nonsense...               */

				default:
					current_state = STRING_GARBLED;
					break;
				}
			}

			/* it could be [M]M/[D]D/[YY]YY format              */

			else if (is_numeric_dateformat(tok)) {
				store_numeric_date(tok, t);
				current_state = GOING_IN_BLIND;
			} else if (is_dd_mon_yyyy(tok)) {
				store_dd_mon_yyy_date(tok, t);
				current_state = GOING_IN_BLIND;
			} else if (is_timeformat(tok)) {
				store_time(tok, t);
				current_state = GOING_IN_BLIND;
			} else {
				current_state = STRING_GARBLED;
			}

			break;	/* end of GOING_IN_BLIND */

		case DOING_ALPHABETIC_DATE:

			if (is_alphabetic(tok)) {
				if (!monthdone && is_monthname(tok)) {
					store_monthname(tok, t);
					monthdone++;
					if (monthdone && yeardone && mdaydone)
						current_state = GOING_IN_BLIND;
					else
						current_state = DOING_ALPHABETIC_DATE;
				} else {
					current_state = STRING_GARBLED;
				}
			} else if (is_numeric(tok)) {
				if (mdaydone && yeardone)
					current_state = STRING_GARBLED;
				else
					switch (strlen(tok)) {
					case 4:
						store_year(atoi(tok), t);
						yeardone++;
						if (monthdone && yeardone && mdaydone)
							current_state = GOING_IN_BLIND;
						else
							current_state = DOING_ALPHABETIC_DATE;
						break;

					case 2:
					case 1:
						if (!mdaydone) {
							store_mday(tok, t);

							mdaydone++;
							if (monthdone && yeardone && mdaydone)
								current_state = GOING_IN_BLIND;
							else
								current_state = DOING_ALPHABETIC_DATE;
						} else {
							store_year(atoi(tok), t);
							yeardone++;
							if (monthdone && yeardone && mdaydone)
								current_state = GOING_IN_BLIND;
							else
								current_state = DOING_ALPHABETIC_DATE;
						}
						break;

					default:
						current_state = STRING_GARBLED;
					}
			} else {
				current_state = STRING_GARBLED;
			}

			break;	/* end of DOING_ALPHABETIC_DATE */

		case PUT_NUMERIC_IN_CONTEXT:

			if (is_alphabetic(tok)) {
				if (is_monthname(tok)) {
					store_mday(last_token, t);
					mdaydone++;
					store_monthname(tok, t);
					monthdone++;
					if (monthdone && yeardone && mdaydone)
						current_state = GOING_IN_BLIND;
					else
						current_state = DOING_ALPHABETIC_DATE;
				} else if (is_ampm(tok)) {
					store_hour(last_token, tok, t);
					current_state = GOING_IN_BLIND;
				} else {
					current_state = STRING_GARBLED;
				}
			} else if (is_numeric(tok)) {
				switch (strlen(tok)) {
				case 4:
				case 2:
					store_mday(last_token, t);
					mdaydone++;
					store_year(atoi(tok), t);
					yeardone++;

					if (monthdone && yeardone && mdaydone)
						current_state = GOING_IN_BLIND;
					else
						current_state = DOING_ALPHABETIC_DATE;
					break;

				default:
					current_state = STRING_GARBLED;
				}
			} else {
				current_state = STRING_GARBLED;
			}

			break;	/* end of PUT_NUMERIC_IN_CONTEXT */

		case STRING_GARBLED:

			tdsdump_log(TDS_DBG_INFO1,
				    "error_handler:  Attempt to convert data stopped by syntax error in source field \n");
			return TDS_CONVERT_SYNTAX;
		}

		tok = strtok_r((char *) NULL, " ,", &lasts);
	}

	i = (t->tm_mon - 13) / 12;
	dt_days = 1461 * (t->tm_year + 300 + i) / 4 +
		(367 * (t->tm_mon - 1 - 12 * i)) / 12 - (3 * ((t->tm_year + 400 + i) / 100)) / 4 + t->tm_mday - 109544;

	free(in);

	/* TODO check for overflow */
	if (desttype == SYBDATETIME) {
		cr->dt.dtdays = dt_days;
		dt_time = (t->tm_hour * 60 + t->tm_min) * 60 + t->tm_sec;
		cr->dt.dttime = dt_time * 300 + (t->tm_ms * 300 + 150) / 1000;
		return sizeof(TDS_DATETIME);
	} else {
		/* SYBDATETIME4 */
		cr->dt4.days = dt_days;
		cr->dt4.minutes = t->tm_hour * 60 + t->tm_min;
		return sizeof(TDS_DATETIME4);
	}

}

static int
stringz_to_numeric(const char *instr, CONV_RESULT * cr)
{
	return string_to_numeric(instr, instr + strlen(instr), cr);
}

static int
string_to_numeric(const char *instr, const char *pend, CONV_RESULT * cr)
{

	char mynumber[(MAXPRECISION + 7) / 8 * 8];

/* num packaged 8 digit, see below for detail */
	TDS_UINT packed_num[TDS_VECTOR_SIZE(mynumber) / 8];

	char *ptr;
	const char *pdigits;
	const char *pstr;

	TDS_UINT carry = 0;
	char not_zero = 1;
	int i = 0;
	int j = 0;
	short int bytes, places, point_found, digits;
	unsigned char sign;

	sign = 0;
	point_found = 0;
	places = 0;

	/* FIXME: application can pass invalid value for precision and scale ?? */
	if (cr->n.precision > 77)
		return TDS_CONVERT_FAIL;

	if (cr->n.precision == 0)
		cr->n.precision = 77;	/* assume max precision */

	if (cr->n.scale > cr->n.precision)
		return TDS_CONVERT_FAIL;


	/* skip leading blanks */
	for (pstr = instr;; ++pstr) {
		if (pstr == pend)
			return TDS_CONVERT_SYNTAX;
		if (*pstr != ' ')
			break;
	}

	if (*pstr == '-' || *pstr == '+') {	/* deal with a leading sign */
		if (*pstr == '-')
			sign = 1;
		pstr++;
	}

	digits = 0;
	pdigits = pstr;
	for (; pstr != pend; ++pstr) {	/* deal with the rest */
		if (isdigit((unsigned char) *pstr)) {	/* its a number */
			if (point_found)	/* if we passed a decimal point */
				++places;	/* count digits after that point  */
			else
				++digits;	/* count digits before point  */
		} else if (*pstr == '.') {	/* found a decimal point */
			if (point_found)	/* already had one. return error */
				return TDS_CONVERT_SYNTAX;
			point_found = 1;
		} else		/* first invalid character */
			return TDS_CONVERT_SYNTAX;	/* return error. */
	}

	/* no digits? no number! */
	if (!digits)
		return TDS_CONVERT_SYNTAX;

	/* truncate decimal digits */
	if (places > cr->n.scale)
		places = cr->n.scale;

	/* too digits, error */
	if ((digits + cr->n.scale) > cr->n.precision)
		return TDS_CONVERT_OVERFLOW;


	/* TODO: this can be optimized in a single step */

	/* scale specified, pad out number with zeroes to the scale...  */
	ptr = mynumber + sizeof(mynumber) - (cr->n.scale - places);
	memset(ptr, 48, cr->n.scale - places);
	ptr -= places;
	/* copy number without point */
	memcpy(ptr, pdigits + digits + 1, places);
	ptr -= digits;
	memcpy(ptr, pdigits, digits);
	memset(mynumber, 48, ptr - mynumber);

	/* transform ASCII string into a numeric array */
	for (ptr = mynumber; ptr != mynumber + sizeof(mynumber); ++ptr)
		*ptr -= 48;

	/*
	 * Packaged number explanation
	 * I package 8 decimal digit in one number
	 * This because 10^8 = 5^8 * 2^8 = 5^8 * 256
	 * So dividing 10^8 for 256 make no remainder
	 * So I can split for bytes in an optmized way
	 */

	/* transform to packaged one */
	for (j = 0; j < TDS_VECTOR_SIZE(packed_num); ++j) {
		TDS_UINT n = mynumber[j * 8];

		for (i = 1; i < 8; ++i)
			n = n * 10 + mynumber[j * 8 + i];
		packed_num[j] = n;
	}

	memset(cr->n.array, 0, sizeof(cr->n.array));
	cr->n.array[0] = sign;
	bytes = tds_numeric_bytes_per_prec[cr->n.precision];

	while (not_zero) {
		not_zero = 0;
		carry = 0;
		for (i = 0; i < TDS_VECTOR_SIZE(packed_num); ++i) {
			TDS_UINT tmp;

			if (packed_num[i] > 0)
				not_zero = 1;

			/* divide for 256 for find another byte */
			tmp = packed_num[i];
			/* carry * (25u*25u*25u*25u) = carry * 10^8 / 256u
			 * using unsigned number is just an optimization
			 * compiler can translate division to a shift and remainder 
			 * to a binary and
			 */
			packed_num[i] = carry * (25u * 25u * 25u * 25u) + packed_num[i] / 256u;
			carry = tmp % 256u;

			if (i == (TDS_VECTOR_SIZE(packed_num) - 1) && not_zero) {
				/* source number is limited to 38 decimal digit
				 * 10^39-1 < 2^128 (16 byte) so this cannot make an overflow
				 */
				cr->n.array[--bytes] = carry;
			}
		}
	}
	return sizeof(TDS_NUMERIC);
}

static int
is_numeric_dateformat(char *t)
{
	char *instr;
	int ret = 1;
	int slashes = 0;
	int hyphens = 0;
	int periods = 0;
	int digits = 0;

	for (instr = t; *instr; instr++) {
		if (!isdigit((unsigned char) *instr) && *instr != '/' && *instr != '-' && *instr != '.') {
			ret = 0;
			break;
		}
		if (*instr == '/')
			slashes++;
		else if (*instr == '-')
			hyphens++;
		else if (*instr == '.')
			periods++;
		else
			digits++;

	}
	if (hyphens + slashes + periods != 2)
		ret = 0;
	if (hyphens == 1 || slashes == 1 || periods == 1)
		ret = 0;

	if (digits < 4 || digits > 8)
		ret = 0;

	return (ret);

}

/* This function will check if an alphanumeric string */
/* holds a date in any of the following formats :     */
/*    DD-MON-YYYY */
/*    DD-MON-YY   */
/*    DDMONYY     */
/*    DDMONYYYY   */
static int
is_dd_mon_yyyy(char *t)
{
	char *instr;
	char month[4];

	instr = t;

	if (!isdigit((unsigned char) *instr))
		return (0);

	instr++;

	if (!isdigit((unsigned char) *instr))
		return (0);

	instr++;

	if (*instr == '-') {
		instr++;

		strncpy(month, instr, 3);
		month[3] = '\0';

		if (!is_monthname(month))
			return (0);

		instr += 3;

		if (*instr != '-')
			return (0);

		instr++;
		if (!isdigit((unsigned char) *instr))
			return (0);

		instr++;
		if (!isdigit((unsigned char) *instr))
			return (0);

		instr++;

		if (*instr) {
			if (!isdigit((unsigned char) *instr))
				return (0);

			instr++;
			if (!isdigit((unsigned char) *instr))
				return (0);
		}

	} else {

		strncpy(month, instr, 3);
		month[3] = '\0';

		if (!is_monthname(month))
			return (0);

		instr += 3;

		if (!isdigit((unsigned char) *instr))
			return (0);

		instr++;
		if (!isdigit((unsigned char) *instr))
			return (0);

		instr++;
		if (*instr) {
			if (!isdigit((unsigned char) *instr))
				return (0);

			instr++;
			if (!isdigit((unsigned char) *instr))
				return (0);
		}
	}

	return (1);

}

static int
is_monthname(char *datestr)
{

	int ret = 0;

	if (strlen(datestr) == 3) {
		if (strcasecmp(datestr, "jan") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "feb") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "mar") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "apr") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "may") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "jun") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "jul") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "aug") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "sep") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "oct") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "nov") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "dec") == 0)
			ret = 1;
		else
			ret = 0;
	} else {
		if (strcasecmp(datestr, "january") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "february") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "march") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "april") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "june") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "july") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "august") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "september") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "october") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "november") == 0)
			ret = 1;
		else if (strcasecmp(datestr, "december") == 0)
			ret = 1;
		else
			ret = 0;

	}
	return (ret);

}
static int
is_ampm(char *datestr)
{

	int ret = 0;

	if (strcasecmp(datestr, "am") == 0)
		ret = 1;
	else if (strcasecmp(datestr, "pm") == 0)
		ret = 1;
	else
		ret = 0;

	return (ret);

}

static int
is_alphabetic(char *datestr)
{
	char *s;
	int ret = 1;

	for (s = datestr; *s; s++) {
		if (!isalpha((unsigned char) *s))
			ret = 0;
	}
	return (ret);
}

static int
is_numeric(char *datestr)
{
	char *s;
	int ret = 1;

	for (s = datestr; *s; s++) {
		if (!isdigit((unsigned char) *s))
			ret = 0;
	}
	return (ret);
}

static int
is_timeformat(char *datestr)
{
	char *s;
	int ret = 1;

	for (s = datestr; *s; s++) {
		if (!isdigit((unsigned char) *s) && *s != ':' && *s != '.')
			break;
	}
	if (*s) {
		if (strcasecmp(s, "am") != 0 && strcasecmp(s, "pm") != 0)
			ret = 0;
	}


	return (ret);
}

static int
store_year(int year, struct tds_time *t)
{

	if (year <= 0)
		return 0;

	if (year < 100) {
		if (year > 49)
			t->tm_year = year;
		else
			t->tm_year = 100 + year;
		return (1);
	}

	if (year < 1753)
		return (0);

	if (year <= 9999) {
		t->tm_year = year - 1900;
		return (1);
	}

	return (0);

}
static int
store_mday(char *datestr, struct tds_time *t)
{
	int mday = 0;

	mday = atoi(datestr);

	if (mday > 0 && mday < 32) {
		t->tm_mday = mday;
		return (1);
	} else
		return 0;
}

static int
store_numeric_date(char *datestr, struct tds_time *t)
{
	int TDS_MONTH = 0;
	int TDS_DAY = 0;
	int TDS_YEAR = 0;

	int state;
	char last_char = 0;
	char *s;
	int month = 0, year = 0, mday = 0;

	/* Its YYYY-MM-DD format */

	if (strlen(datestr) == 10 && *(datestr + 4) == '-' && *(datestr + 7) == '-') {

		TDS_YEAR = 0;
		TDS_MONTH = 1;
		TDS_DAY = 2;
		state = TDS_YEAR;

	}
	/* else we assume MDY */
	else {
		TDS_MONTH = 0;
		TDS_DAY = 1;
		TDS_YEAR = 2;
		state = TDS_MONTH;
	}
	for (s = datestr; *s; s++) {
		if (!isdigit((unsigned char) *s) && isdigit((unsigned char) last_char)) {
			state++;
		} else {
			if (state == TDS_MONTH)
				month = (month * 10) + (*s - '0');
			if (state == TDS_DAY)
				mday = (mday * 10) + (*s - '0');
			if (state == TDS_YEAR)
				year = (year * 10) + (*s - '0');
		}
		last_char = *s;
	}

	if (month > 0 && month < 13)
		t->tm_mon = month - 1;
	else
		return 0;
	if (mday > 0 && mday < 32)
		t->tm_mday = mday;
	else
		return 0;

	return store_year(year, t);

}

static int
store_dd_mon_yyy_date(char *datestr, struct tds_time *t)
{

	char dd[3];
	int mday;
	char mon[4];
	char yyyy[5];
	int year;

	strncpy(dd, datestr, 2);
	dd[2] = '\0';
	mday = atoi(dd);

	if (mday > 0 && mday < 32)
		t->tm_mday = mday;
	else
		return 0;

	if (datestr[2] == '-') {
		strncpy(mon, &datestr[3], 3);
		mon[3] = '\0';

		if (!store_monthname(mon, t))
			return 0;

		strcpy(yyyy, &datestr[7]);
		year = atoi(yyyy);

		return store_year(year, t);
	} else {
		strncpy(mon, &datestr[2], 3);
		mon[3] = '\0';

		if (!store_monthname(mon, t))
			return 0;

		strcpy(yyyy, &datestr[5]);
		year = atoi(yyyy);

		return store_year(year, t);
	}

}

static int
store_monthname(char *datestr, struct tds_time *t)
{

	int ret = 1;

	if (strlen(datestr) == 3) {
		if (strcasecmp(datestr, "jan") == 0)
			t->tm_mon = 0;
		else if (strcasecmp(datestr, "feb") == 0)
			t->tm_mon = 1;
		else if (strcasecmp(datestr, "mar") == 0)
			t->tm_mon = 2;
		else if (strcasecmp(datestr, "apr") == 0)
			t->tm_mon = 3;
		else if (strcasecmp(datestr, "may") == 0)
			t->tm_mon = 4;
		else if (strcasecmp(datestr, "jun") == 0)
			t->tm_mon = 5;
		else if (strcasecmp(datestr, "jul") == 0)
			t->tm_mon = 6;
		else if (strcasecmp(datestr, "aug") == 0)
			t->tm_mon = 7;
		else if (strcasecmp(datestr, "sep") == 0)
			t->tm_mon = 8;
		else if (strcasecmp(datestr, "oct") == 0)
			t->tm_mon = 9;
		else if (strcasecmp(datestr, "nov") == 0)
			t->tm_mon = 10;
		else if (strcasecmp(datestr, "dec") == 0)
			t->tm_mon = 11;
		else
			ret = 0;
	} else {
		if (strcasecmp(datestr, "january") == 0)
			t->tm_mon = 0;
		else if (strcasecmp(datestr, "february") == 0)
			t->tm_mon = 1;
		else if (strcasecmp(datestr, "march") == 0)
			t->tm_mon = 2;
		else if (strcasecmp(datestr, "april") == 0)
			t->tm_mon = 3;
		else if (strcasecmp(datestr, "june") == 0)
			t->tm_mon = 5;
		else if (strcasecmp(datestr, "july") == 0)
			t->tm_mon = 6;
		else if (strcasecmp(datestr, "august") == 0)
			t->tm_mon = 7;
		else if (strcasecmp(datestr, "september") == 0)
			t->tm_mon = 8;
		else if (strcasecmp(datestr, "october") == 0)
			t->tm_mon = 9;
		else if (strcasecmp(datestr, "november") == 0)
			t->tm_mon = 10;
		else if (strcasecmp(datestr, "december") == 0)
			t->tm_mon = 11;
		else
			ret = 0;

	}
	return (ret);

}
static int
store_yymmdd_date(char *datestr, struct tds_time *t)
{
	int month = 0, year = 0, mday = 0;

	int wholedate;

	wholedate = atoi(datestr);

	year = wholedate / 10000;
	month = (wholedate - (year * 10000)) / 100;
	mday = (wholedate - (year * 10000) - (month * 100));

	if (month > 0 && month < 13)
		t->tm_mon = month - 1;
	else
		return 0;
	if (mday > 0 && mday < 32)
		t->tm_mday = mday;
	else
		return 0;

	return (store_year(year, t));

}

static int
store_time(char *datestr, struct tds_time *t)
{
	enum
	{ TDS_HOURS,
		TDS_MINUTES,
		TDS_SECONDS,
		TDS_FRACTIONS
	};

	int state = TDS_HOURS;
	char last_sep = '\0';
	char *s;
	int hours = 0, minutes = 0, seconds = 0, millisecs = 0;
	int ret = 1;
	int ms_len = 0;

	for (s = datestr; *s && strchr("apmAPM", (int) *s) == (char *) NULL; s++) {
		if (*s == ':' || *s == '.') {
			last_sep = *s;
			state++;
		} else
			switch (state) {
			case TDS_HOURS:
				hours = (hours * 10) + (*s - '0');
				break;
			case TDS_MINUTES:
				minutes = (minutes * 10) + (*s - '0');
				break;
			case TDS_SECONDS:
				seconds = (seconds * 10) + (*s - '0');
				break;
			case TDS_FRACTIONS:
				millisecs = (millisecs * 10) + (*s - '0');
				ms_len++;
				break;
			}
	}
	if (*s) {
		if (strcasecmp(s, "am") == 0) {
			if (hours == 12)
				hours = 0;

			t->tm_hour = hours;
		}
		if (strcasecmp(s, "pm") == 0) {
			if (hours == 0)
				ret = 0;
			if (hours > 0 && hours < 12)
				t->tm_hour = hours + 12;
			else
				t->tm_hour = hours;
		}
	} else {
		if (hours >= 0 && hours < 24)
			t->tm_hour = hours;
		else
			ret = 0;
	}
	if (minutes >= 0 && minutes < 60)
		t->tm_min = minutes;
	else
		ret = 0;
	if (seconds >= 0 && minutes < 60)
		t->tm_sec = seconds;
	else
		ret = 0;
	tdsdump_log(TDS_DBG_FUNC, "%L store_time() millisecs = %d\n", millisecs);
	if (millisecs) {
		if (millisecs >= 0 && millisecs < 1000) {
			if (last_sep == ':')
				t->tm_ms = millisecs;
			else {
				if (ms_len == 1)
					t->tm_ms = millisecs * 100;
				else if (ms_len == 2)
					t->tm_ms = millisecs * 10;
				else
					t->tm_ms = millisecs;
			}
		} else
			ret = 0;
	}


	return (ret);
}

static int
store_hour(char *hour, char *ampm, struct tds_time *t)
{
	int ret = 1;
	int hours;

	hours = atoi(hour);

	if (hours >= 0 && hours < 24) {
		if (strcasecmp(ampm, "am") == 0) {
			if (hours == 12)
				hours = 0;

			t->tm_hour = hours;
		}
		if (strcasecmp(ampm, "pm") == 0) {
			if (hours == 0)
				ret = 0;
			if (hours > 0 && hours < 12)
				t->tm_hour = hours + 12;
			else
				t->tm_hour = hours;
		}
	}
	return (ret);
}

/**
 * Get same type but nullable
 * @param srctype type requires
 * @return nullable type
 */
TDS_INT
tds_get_null_type(int srctype)
{

	switch (srctype) {
	case SYBCHAR:
		return SYBVARCHAR;
		break;
	case SYBINT1:
	case SYBINT2:
	case SYBINT4:
	case SYBINT8:
		return SYBINTN;
		break;
	case SYBREAL:
	case SYBFLT8:
		return SYBFLTN;
		break;
	case SYBDATETIME:
	case SYBDATETIME4:
		return SYBDATETIMN;
		break;
	case SYBBIT:
		return SYBBITN;
		break;
	default:
		break;
	}
	return srctype;
}

/**
 * format a date string according to an "extended" strftime formatting definition.
 * @param buf     output buffer
 * @param maxsize size of buffer in bytes (space include terminator)
 * @param format  format string similar to strftime. %z for milliseconds
 * @param dr      date to convert
 * @return length of string returned, 0 for error
 */
size_t
tds_strftime(char *buf, size_t maxsize, const char *format, const TDSDATEREC * dr)
{
	struct tm tm;

	int length = 0;
	char *s, *our_format;
	char millibuf[8];

	char *pz = NULL;

	tm.tm_sec = dr->second;
	tm.tm_min = dr->minute;
	tm.tm_hour = dr->hour;
	tm.tm_mday = dr->day;
	tm.tm_mon = dr->month;
	tm.tm_year = dr->year - 1900;
	tm.tm_wday = dr->weekday;
	tm.tm_yday = dr->dayofyear;
	tm.tm_isdst = 0;

	/* NOTE 2 in intentional. one more character is required because we replace %z with 3 digits */
	our_format = (char *) malloc(strlen(format) + 2);
	if (!our_format)
		return 0;
	strcpy(our_format, format);

	pz = strstr(our_format, "%z");

	/*
	 * Look for "%z" in the format string.  If found, replace it with dr->milliseconds.
	 * For example, if milliseconds is 124, the format string
	 * "%b %d %Y %H:%M:%S.%z" would become
	 * "%b %d %Y %H:%M:%S.124".
	 */

	/* Skip any escaped cases (%%z) */

	while (pz && *(pz - 1) == '%')
		pz = strstr(++pz, "%z");

	if (pz && length < maxsize - 1) {

		sprintf(millibuf, "%03d", dr->millisecond);

		/* move everything back one, then overwrite "?%z" with millibuf */
		for (s = our_format + strlen(our_format); s > pz; s--) {
			*(s + 1) = *s;
		}

		strncpy(pz, millibuf, 3);	/* don't copy the null */
	}

	length = strftime(buf, maxsize, our_format, &tm);

	free(our_format);

	return length;
}

#if 0
static TDS_UINT
utf16len(const utf16_t * s)
{
	const utf16_t *p = s;

	while (*p++);
	return p - s;
}
#endif

/* *INDENT-OFF* */
#ifdef DONT_TRY_TO_COMPILE_THIS
	Try this: "perl -x convert.c > tds_willconvert.h"
	(Perl will generate useful C from the data below.)  
#!perl
	$indent = "\t ";
	printf qq(/* ** %-65s ** */\n), "Please do not edit this file!";
	printf qq(/* ** %-65s ** */\n), "It was generated with 'perl -x convert.c > tds_willconvert.h' ";
	printf qq(/* ** %-65s ** */\n), "See the comments about tds_willconvert in convert.c";
	printf qq(/* ** %-65s ** */\n), "It is much easier to edit the __DATA__ table than this file.  ";
	printf qq(/* ** %-65s ** */\n), " ";
	printf qq(/* ** %65s ** */\n\n), "Thank you.";
	
	while(<DATA>) {
		next if /^\s+To\s+$/;
		next if /^From/;
		if( /^\s+VARCHAR CHAR/ ) {
			@to = split;
			next;
		}
		last if /^BOUNDARY/;
		
		@yn = split;
		$from = shift @yn;
		$i = 0;
		foreach $to (@to) {
			last if $to =~ /^BOUNDARY/;

			$yn = $yn[$i];	# default
			$yn = 1 if $yn[$i] eq 'T';
			$yn = 0 if $yn[$i] eq 'F';
			$yn = 0 if $yn[$i] eq 't';	# means it should be true, but isnt so far.

			printf "$indent %-30.30s, %s", "{ SYB${from}, SYB${to}", "$yn }\n"; 

			$i++;
			$indent = "\t,";
		}
	}

__DATA__
          To
From
          VARCHAR CHAR TEXT BINARY IMAGE INT1 INT2 INT4 INT8 FLT8 REAL NUMERIC DECIMAL BIT MONEY MONEY4 DATETIME DATETIME4 BOUNDARY SENSITIVITY
VARCHAR     T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      T        T         T        T
CHAR        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      T        T         T        T
TEXT        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      T        T         T        T
BINARY      T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
IMAGE       T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
INT1        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
INT2        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
INT4        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
INT8        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
FLT8        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
REAL        T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
NUMERIC     T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
DECIMAL     T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
BIT         T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
MONEY       T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
MONEY4      T      T   T    T      T     T    T    T    T    T    T    T       T       T   T     T      F        F         F        F
DATETIME    T      T   T    T      T     F    F    F    F    F    F    F       F       F   F     F      T        T         F        F
DATETIME4   T      T   T    T      T     F    F    F    F    F    F    F       F       F   F     F      T        T         F        F
BOUNDARY    T      T   T    F      F     F    F    F    F    F    F    F       F       F   F     F      F        F         T        F
SENSITIVITY T      T   T    F      F     F    F    F    F    F    F    F       F       F   F     F      F        F         F        T
#endif
/* *INDENT-ON* */

/**
 * Test if a conversion is possible
 * @param srctype  source type
 * @param desttype destination type
 * @return 0 if not convertible
 */
unsigned char
tds_willconvert(int srctype, int desttype)
{
	typedef struct
	{
		int srctype;
		int desttype;
		int yn;
	}
	ANSWER;
	static const ANSWER answers[] = {
#	include "tds_willconvert.h"
	};
	int i;

	tdsdump_log(TDS_DBG_FUNC, "%L tds_willconvert()\n");

	for (i = 0; i < sizeof(answers) / sizeof(ANSWER); i++) {
		if (srctype == answers[i].srctype && desttype == answers[i].desttype) {
			tdsdump_log(TDS_DBG_FUNC, "%L tds_willconvert() %d %d %d\n", answers[i].srctype, answers[i].desttype,
				    answers[i].yn);
			return answers[i].yn;
		}
	}

	return 0;

}

/**
 * Convert from db date format to a structured date format
 * @param datetype source date type. SYBDATETIME or SYBDATETIME4
 * @param di       source date
 * @param dr       destination date
 * @return TDS_FAIL or TDS_SUCCEED
 */
TDS_INT
tds_datecrack(TDS_INT datetype, const void *di, TDSDATEREC * dr)
{

	const TDS_DATETIME *dt;
	const TDS_DATETIME4 *dt4;

	int dt_days;
	unsigned int dt_time;

	int years, months, days, ydays, wday, hours, mins, secs, ms;
	int l, n, i, j;

	if (datetype == SYBDATETIME) {
		dt = (const TDS_DATETIME *) di;
		dt_time = dt->dttime;
		ms = ((dt_time % 300) * 1000 + 150) / 300;
		dt_time = dt_time / 300;
		secs = dt_time % 60;
		dt_time = dt_time / 60;
		dt_days = dt->dtdays;
	} else if (datetype == SYBDATETIME4) {
		dt4 = (const TDS_DATETIME4 *) di;
		secs = 0;
		ms = 0;
		dt_days = dt4->days;
		dt_time = dt4->minutes;
	} else
		return TDS_FAIL;

	/* -53690 is minimun  (1753-1-1) (Gregorian calendar start in 1732) 
	 * 2958463 is maximun (9999-12-31) */
	l = dt_days + 146038;
	wday = (l + 4) % 7;
	n = (4 * l) / 146097;	/* n century */
	l = l - (146097 * n + 3) / 4;	/* days from xx00-02-28 (y-m-d) */
	i = (4000 * (l + 1)) / 1461001;	/* years from xx00-02-28 */
	l = l - (1461 * i) / 4;	/* year days from xx00-02-28 */
	ydays = l >= 306 ? l - 305 : l + 60;
	l += 31;
	j = (80 * l) / 2447;
	days = l - (2447 * j) / 80;
	l = j / 11;
	months = j + 1 - 12 * l;
	years = 100 * (n + 15) + i + l;
	if (l == 0 && (years & 3) == 0 && (years % 100 != 0 || years % 400 == 0))
		++ydays;

	hours = dt_time / 60;
	mins = dt_time % 60;

	dr->year = years;
	dr->month = months;
	dr->day = days;
	dr->dayofyear = ydays;
	dr->weekday = wday;
	dr->hour = hours;
	dr->minute = mins;
	dr->second = secs;
	dr->millisecond = ms;
	return TDS_SUCCEED;
}

/**
 * sybase's char->int conversion tolerates embedded blanks, 
 * such that "convert( int, ' - 13 ' )" works.  
 * if we find blanks, we copy the string to a temporary buffer, 
 * skipping the blanks.  
 * we return the results of atoi() with a clean string.  
 * 
 * n.b. it is possible to embed all sorts of non-printable characters, but we
 * only check for spaces.  at this time, no one on the project has tested anything else.  
 */
static TDS_INT
string_to_int(const char *buf, const char *pend, TDS_INT * res)
{
	enum
	{ blank = ' ' };
	const char *p;
	int sign;
	unsigned int num;	/* we use unsigned here for best overflow check */

	p = buf;

	/* ignore leading spaces */
	while (p != pend && *p == blank)
		++p;
	if (p == pend)
		return TDS_CONVERT_SYNTAX;

	/* check for sign */
	sign = 0;
	switch (*p) {
	case '-':
		sign = 1;
		/* fall thru */
	case '+':
		/* skip spaces between sign and number */
		++p;
		while (p != pend && *p == blank)
			++p;
		break;
	}

	/* a digit must be present */
	if (p == pend)
		return TDS_CONVERT_SYNTAX;

	num = 0;
	for (; p != pend; ++p) {
		/* check for trailing spaces */
		if (*p == blank) {
			while (p != pend && *++p == blank);
			if (p != pend)
				return TDS_CONVERT_SYNTAX;
			break;
		}

		/* must be a digit */
		if (!isdigit((unsigned char) *p))
			return TDS_CONVERT_SYNTAX;

		/* add a digit to number and check for overflow */
		/* NOTE I didn't forget a digit, I check overflow before multiply to prevent overflow */
		if (num > 214748364u)
			return TDS_CONVERT_OVERFLOW;
		num = num * 10u + (*p - '0');
	}

	/* check for overflow and convert unsigned to signed */
	if (sign) {
		if (num > 2147483648u)
			return TDS_CONVERT_OVERFLOW;
		*res = 0 - num;
	} else {
		if (num >= 2147483648u)
			return TDS_CONVERT_OVERFLOW;
		*res = num;
	}

	return TDS_SUCCEED;
}

/* copied from string_ti_int and modified */
static TDS_INT
string_to_int8(const char *buf, const char *pend, TDS_INT8 * res)
{
	enum
	{ blank = ' ' };
	const char *p;
	int sign;
	TDS_UINT8 num;		/* we use unsigned here for best overflow check */

	p = buf;

	/* ignore leading spaces */
	while (p != pend && *p == blank)
		++p;
	if (p == pend)
		return TDS_CONVERT_SYNTAX;

	/* check for sign */
	sign = 0;
	switch (*p) {
	case '-':
		sign = 1;
		/* fall thru */
	case '+':
		/* skip spaces between sign and number */
		++p;
		while (p != pend && *p == blank)
			++p;
		break;
	}

	/* a digit must be present */
	if (p == pend)
		return TDS_CONVERT_SYNTAX;

	num = 0;
	for (; p != pend; ++p) {
		/* check for trailing spaces */
		if (*p == blank) {
			while (p != pend && *++p == blank);
			if (p != pend)
				return TDS_CONVERT_SYNTAX;
			break;
		}

		/* must be a digit */
		if (!isdigit((unsigned char) *p))
			return TDS_CONVERT_SYNTAX;

		/* add a digit to number and check for overflow */
		if (num > ((((TDS_UINT8) 1) << 63) / ((TDS_UINT8) 10)))
			return TDS_CONVERT_OVERFLOW;
		num = num * 10u + (*p - '0');
	}

	/* check for overflow and convert unsigned to signed */
	if (sign) {
		if (num > (((TDS_UINT8) 1) << 63))
			return TDS_CONVERT_OVERFLOW;
		*res = 0 - num;
	} else {
		if (num >= (((TDS_UINT8) 1) << 63))
			return TDS_CONVERT_OVERFLOW;
		*res = num;
	}

	return TDS_SUCCEED;
}

#if 0
/* 
 * Offer string equivalents of conversion tokens.  
 */
static const char *
tds_prtype(int token)
{
	const char *result = "???";

	switch (token) {
	case SYBBINARY:
		result = "SYBBINARY";
		break;
	case SYBBIT:
		result = "SYBBIT";
		break;
	case SYBBITN:
		result = "SYBBITN";
		break;
	case SYBCHAR:
		result = "SYBCHAR";
		break;
	case SYBDATETIME4:
		result = "SYBDATETIME4";
		break;
	case SYBDATETIME:
		result = "SYBDATETIME";
		break;
	case SYBDATETIMN:
		result = "SYBDATETIMN";
		break;
	case SYBDECIMAL:
		result = "SYBDECIMAL";
		break;
	case SYBFLT8:
		result = "SYBFLT8";
		break;
	case SYBFLTN:
		result = "SYBFLTN";
		break;
	case SYBIMAGE:
		result = "SYBIMAGE";
		break;
	case SYBINT1:
		result = "SYBINT1";
		break;
	case SYBINT2:
		result = "SYBINT2";
		break;
	case SYBINT4:
		result = "SYBINT4";
		break;
	case SYBINT8:
		result = "SYBINT8";
		break;
	case SYBINTN:
		result = "SYBINTN";
		break;
	case SYBMONEY4:
		result = "SYBMONEY4";
		break;
	case SYBMONEY:
		result = "SYBMONEY";
		break;
	case SYBMONEYN:
		result = "SYBMONEYN";
		break;
	case SYBNTEXT:
		result = "SYBNTEXT";
		break;
	case SYBNVARCHAR:
		result = "SYBNVARCHAR";
		break;
	case SYBNUMERIC:
		result = "SYBNUMERIC";
		break;
	case SYBREAL:
		result = "SYBREAL";
		break;
	case SYBTEXT:
		result = "SYBTEXT";
		break;
	case SYBUNIQUE:
		result = "SYBUNIQUE";
		break;
	case SYBVARBINARY:
		result = "SYBVARBINARY";
		break;
	case SYBVARCHAR:
		result = "SYBVARCHAR";
		break;

	case SYBVARIANT:
		result = "SYBVARIANT";
		break;
	case SYBVOID:
		result = "SYBVOID";
		break;
	case XSYBBINARY:
		result = "XSYBBINARY";
		break;
	case XSYBCHAR:
		result = "XSYBCHAR";
		break;
	case XSYBNCHAR:
		result = "XSYBNCHAR";
		break;
	case XSYBNVARCHAR:
		result = "XSYBNVARCHAR";
		break;
	case XSYBVARBINARY:
		result = "XSYBVARBINARY";
		break;
	case XSYBVARCHAR:
		result = "XSYBVARCHAR";
		break;
	case SYBLONGBINARY:
		result = "SYBLONGBINARY";
		break;
	default:
		break;
	}
	return result;
}
#endif

/** \@} */
