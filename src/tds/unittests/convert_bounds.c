/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2022  Frediano Ziglio
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

/* 
 * Purpose: test conversion bounds of integers and floating points.
 */
#include "common.h"
#include <assert.h>
#include <freetds/convert.h>
#include <freetds/utils/smp.h>

static TDS_INT convert_and_free(int srctype, const void *src, TDS_UINT srclen, int desttype, CONV_RESULT *cr);
static bool is_valid(const char *num, int type, CONV_RESULT *cr);
static double convert_to_float(smp n, int type);
static int64_t get_float_precision_factor(smp n, int type);
static void real_test(smp n, int type, bool is_integer);
static void double_to_string(char *out, double d);

static TDSCONTEXT *ctx;

typedef struct {
	int type;
	bool is_integer;
} type_desc;

/* list of integer and floating point types */
static const type_desc number_types[] = {
	{ SYBINT1, true },
	{ SYBSINT1, true },
	{ SYBUINT1, true },
	{ SYBINT2, true },
	{ SYBUINT2, true },
	{ SYBINT4, true },
	{ SYBUINT4, true },
	{ SYBINT8, true },
	{ SYBUINT8, true },
	{ SYBMONEY4, true },
	{ SYBMONEY, true },
	{ SYBREAL, false },
	{ SYBFLT8, false },
	{ SYBNUMERIC, true },
	{ 0, false },
};

/* list of upper and lower bounds for numbers */
static const char *bounds[] = {
	"0",
	/* 8 bits */
	"128",
	"256",
	/* 16 bits */
	"0x8000",
	"0x1'0000",
	/* money */
	"214748",
	"922337203685477",
	/* 32 bits */
	"0x8000'0000",
	"0x1'0000'0000",
	/* 64 bits */
	"0x8000'0000'0000'0000",
	"0x1'0000'0000'0000'0000",
	NULL,
};

TEST_MAIN()
{
	const char **bound;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	ctx = tds_alloc_context(NULL);
	assert(ctx);
	if (!ctx->locale->datetime_fmt) {
		/* set default in case there's no locale file */
		ctx->locale->datetime_fmt = strdup(STD_DATETIME_FMT);
	}

	/* test all bounds for all types */
	for (bound = bounds; *bound; ++bound) {
		const type_desc *t;
		smp n = smp_from_string(*bound);

		for (t = number_types; t->type != 0; ++t)
			real_test(n, t->type, t->is_integer);
	}

	tds_free_context(ctx);
	return 0;
}

static TDS_INT
convert_and_free(int srctype, const void *src, TDS_UINT srclen, int desttype, CONV_RESULT *cr)
{
	TDS_INT res;

	cr->n.precision = 20;
	cr->n.scale = 0;

	res = tds_convert(ctx, srctype, src, srclen, desttype, cr);
	if (res < 0)
		return res;

	switch (desttype) {
	case SYBCHAR: case SYBVARCHAR: case SYBTEXT: case XSYBCHAR: case XSYBVARCHAR:
	case SYBBINARY: case SYBVARBINARY: case SYBIMAGE: case XSYBBINARY: case XSYBVARBINARY:
	case SYBLONGBINARY:
		free(cr->c);
		break;
	}
	return res;
}

static void
real_test(smp n, int srctype, bool is_integer)
{
	int i;

	/* test both positive and negative */
	for (i = 0; i < 2; ++i) {
		const type_desc *t;
		char *s_num = smp_to_string(n);
		int diff;
		int64_t precision_factor = 1;

		printf("Testing conversions from %s for number %s\n", tds_prtype(srctype), s_num);
		free(s_num);

		if (!is_integer)
			precision_factor = get_float_precision_factor(n, srctype);

		/* test bound conversions to all number types */
		for (t = number_types; t->type != 0; ++t) {
			for (diff = -10; diff <= 10; ++diff) {
				bool valid_src, valid_dest;
				int desttype = t->type;
				CONV_RESULT cr_src, cr_dest;
				int result;
				smp num = smp_add(n, smp_from_int(diff * precision_factor));

				/* convert from char to check for validity */
				s_num = smp_to_string(num);
				valid_src = is_valid(s_num, srctype, &cr_src);

				/* if we were not able to get the source number do not check conversion */
				if (!valid_src) {
					TDS_ZERO_FREE(s_num);
					continue;
				}

				/* NUMERIC has a special encoding for -0 number */
				if (srctype == SYBNUMERIC && i > 0 && smp_is_zero(num))
					cr_src.n.array[0] = 1;

				if (is_integer) {
					valid_dest = is_valid(s_num, desttype, NULL);
				} else {
					/* in order to account for possible lost of precision convert from
					 * conversion result */
					double d = convert_to_float(smp_from_string(s_num), srctype);
					char out_n[128];

					double_to_string(out_n, d);
					valid_dest = is_valid(out_n, desttype, NULL);
				}

				/* try to convert */
				result = convert_and_free(srctype, &cr_src, 8, desttype, &cr_dest);

				/* conversion should succeed if previously succeeded or viceversa */
				if (valid_dest != (result >= 0)) {
					fprintf(stderr, "Unmatch results from %s to %s for %s\n"
						"results %d (from string) %d (from source type)\n",
						tds_prtype(srctype), tds_prtype(desttype), s_num,
						valid_dest, (result >= 0));
					TDS_ZERO_FREE(s_num);
					exit(1);
				}

				/* if failed it should have been an overflow, types are compatible */
				assert((result >= 0) || result == TDS_CONVERT_OVERFLOW);
				TDS_ZERO_FREE(s_num);
			}
		}

		if (smp_is_zero(n) && srctype != SYBNUMERIC)
			break;
		n = smp_negate(n);
	}
}

/* check if a number is valid converted to some type */
static bool
is_valid(const char *num, int type, CONV_RESULT *cr)
{
	CONV_RESULT dummy_cr;

	if (!cr)
		cr = &dummy_cr;

	return convert_and_free(SYBVARCHAR, num, (TDS_UINT) strlen(num), type, cr) >= 0;
}

/* convert multiple precision to a floating number of a specific type */
static double
convert_to_float(smp n, int type)
{
	double ret = smp_to_double(n);

	switch (type) {
	case SYBREAL:
		return (TDS_REAL) ret;
	case SYBFLT8:
		return (TDS_FLOAT) ret;
	default:
		assert(!"wrong type");
	}
	return 0;
}

/* Compute a factor in order to see the change to the original number expressed
 * with a given floating point number.
 * This is due to the limited precision of a floating point type.
 * If we use numbers with 20 digits but the precision is just 10 we need
 * increment in the order of 10 digits to see changes in the floating point
 * numbers if we increment them.
 */
static int64_t
get_float_precision_factor(smp n, int type)
{
	int shift;
	const double orig = convert_to_float(n, type);
	for (shift = 0; ; ++shift) {
		smp diff = smp_from_int(((int64_t) 1) << shift);

		/* returns only if both n+diff and n-diff change the original number */
		if (orig != convert_to_float(smp_add(n, diff), type) &&
		    orig != convert_to_float(smp_sub(n, diff), type))
			break;
	}
	return ((int64_t) 1) << (shift ? shift - 1 : shift);
}

/* Convert a double to a string.
 * This is done to avoid a bug in some old versions of Visual Studio (maybe CRT).
 * It's assumed that the double contains a number with no digits after the unit.
 */
static void
double_to_string(char *out, double d)
{
	int bdigits = 0;
	double exp = 1.f;
	bool negative = false;
	char *s;
	smp n = smp_zero;

	if (d < 0) {
		negative = true;
		d = -d;
	}
	while (exp <= d) {
		exp *= 2.f;
		++bdigits;
	}
	for (; bdigits >= 0; --bdigits) {
		n = smp_add(n,n);
		if (exp <= d) {
			n = smp_add(n, smp_one);
			d -= exp;
		}
		exp *= 0.5f;
	}
	if (negative)
		n = smp_negate(n);
	s = smp_to_string(n);
	strcpy(out, s);
	free(s);
}
