/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 * Copyright (C) 2005-2015  Frediano Ziglio
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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <freetds/tds.h>
#include <freetds/convert.h>
#include <freetds/bytes.h>
#include <freetds/windows.h>
#include <stdlib.h>

/**
 * tds_numeric_bytes_per_prec is indexed by precision and will
 * tell us the number of bytes required to store the specified
 * precision (with the sign).
 * Support precision up to 77 digits
 */
const uint8_t tds_numeric_bytes_per_prec[] = {
	/*
	 * precision can't be 0 but using a value > 0 assure no
	 * core if for some bug it's 0...
	 */
	1, 
	2,  2,  3,  3,  4,  4,  4,  5,  5,
	6,  6,  6,  7,  7,  8,  8,  9,  9,  9,
	10, 10, 11, 11, 11, 12, 12, 13, 13, 14,
	14, 14, 15, 15, 16, 16, 16, 17, 17, 18,
	18, 19, 19, 19, 20, 20, 21, 21, 21, 22,
	22, 23, 23, 24, 24, 24, 25, 25, 26, 26,
	26, 27, 27, 28, 28, 28, 29, 29, 30, 30,
	31, 31, 31, 32, 32, 33, 33, 33
};

TDS_COMPILE_CHECK(maxprecision,
	MAXPRECISION < TDS_VECTOR_SIZE(tds_numeric_bytes_per_prec) );

/*
 * money is a special case of numeric really...that why its here
 */
char *
tds_money_to_string(const TDS_MONEY * money, char *s, bool use_2_digits)
{
	TDS_INT8 mymoney;
	TDS_UINT8 n;
	char *p;

	/* sometimes money it's only 4-byte aligned so always compute 64-bit */
	mymoney = (((TDS_INT8) money->tdsoldmoney.mnyhigh) << 32) | money->tdsoldmoney.mnylow;

	p = s;
	if (mymoney < 0) {
		*p++ = '-';
		/* we use unsigned because this causes arithmetic problem for -2^63*/
		n = (TDS_UINT8) -mymoney;
	} else {
		n = (TDS_UINT8) mymoney;
	}
	/* if machine is 64 bit you do not need to split n */
	if (use_2_digits) {
		n = (n+ 50) / 100;
		sprintf(p, "%" PRIu64 ".%02u", n / 100u, (unsigned) (n % 100u));
	} else {
		sprintf(p, "%" PRIu64 ".%04u", n / 10000u, (unsigned) (n % 10000u));
	}
	return s;
}

/**
 * @return <0 if error
 */
TDS_INT
tds_numeric_to_string(const TDS_NUMERIC * numeric, char *s)
{
	const unsigned char *number;

	unsigned int packet[sizeof(numeric->array) / 2];
	unsigned int *pnum, *packet_start;
	unsigned int *const packet_end = packet + TDS_VECTOR_SIZE(packet);

	unsigned int packet10k[(MAXPRECISION + 3) / 4];
	unsigned int *p;

	int num_bytes;
	unsigned int remainder, n, i, m;

	/* a bit of debug */
#if ENABLE_EXTRA_CHECKS
	memset(packet, 0x55, sizeof(packet));
	memset(packet10k, 0x55, sizeof(packet10k));
#endif

	if (numeric->precision < 1 || numeric->precision > MAXPRECISION || numeric->scale > numeric->precision)
		return TDS_CONVERT_FAIL;

	/* set sign */
	if (numeric->array[0] == 1)
		*s++ = '-';

	/* put number in a 16bit array */
	number = numeric->array;
	num_bytes = tds_numeric_bytes_per_prec[numeric->precision];

	n = num_bytes - 1;
	pnum = packet_end;
	for (; n > 1; n -= 2)
		*--pnum = TDS_GET_UA2BE(&number[n - 1]);
	if (n == 1)
		*--pnum = number[n];
	/* remove leading zeroes */
	while (!*pnum) {
		++pnum;
		/* we consumed all numbers, it's a zero */
		if (pnum == packet_end) {
			*s++ = '0';
			if (numeric->scale) {
				*s++ = '.';
				i = numeric->scale;
				do {
					*s++ = '0';
				} while (--i);
			}
			*s = 0;
			return 1;
		}
	}
	packet_start = pnum;

	/* transform 2^16 base number in 10^4 base number */
	for (p = packet10k + TDS_VECTOR_SIZE(packet10k); packet_start != packet_end;) {
		pnum = packet_start;
		n = *pnum;
		remainder = n % 10000u;
		if (!(*pnum++ = (n / 10000u)))
			packet_start = pnum;
		for (; pnum != packet_end; ++pnum) {
			n = remainder * (256u * 256u) + *pnum;
			remainder = n % 10000u;
			*pnum = n / 10000u;
		}
		*--p = remainder;
	}

	/* transform to 10 base number and output */
	i = 4 * (unsigned int)((packet10k + TDS_VECTOR_SIZE(packet10k)) - p);	/* current digit */
	/* skip leading zeroes */
	n = 1000;
	remainder = *p;
	while (remainder < n)
		n /= 10, --i;
	if (i <= numeric->scale) {
		*s++ = '0';
		*s++ = '.';
		m = i;
		while (m < numeric->scale)
			*s++ = '0', ++m;
	}
	for (;;) {
		*s++ = (remainder / n) + '0';
		--i;
		remainder %= n;
		n /= 10;
		if (!n) {
			n = 1000;
			if (++p == packet10k + TDS_VECTOR_SIZE(packet10k))
				break;
			remainder = *p;
		}
		if (i == numeric->scale)
			*s++ = '.';
	}
	*s = 0;

	return 1;
}

#define TDS_WORD  uint32_t
#define TDS_DWORD uint64_t
#define TDS_WORD_DDIGIT 9

/* include to check limits */

#include "num_limits.h"

static int
tds_packet_check_overflow(TDS_WORD *packet, unsigned int packet_len, unsigned int prec)
{
	unsigned int i, len, stop;
	const TDS_WORD *limit = &limits[limit_indexes[prec] + LIMIT_INDEXES_ADJUST * prec];
	len = limit_indexes[prec+1] - limit_indexes[prec] + LIMIT_INDEXES_ADJUST;
	stop = prec / (sizeof(TDS_WORD) * 8);
	/*
	 * Now a number is
	 * ... P[3] P[2] P[1] P[0]
	 * while upper limit + 1 is
 	 * zeroes limit[0 .. len-1] 0[0 .. stop-1]
	 * we must assure that number < upper limit + 1
	 */
	if (packet_len >= len + stop) {
		/* higher packets must be zero */
		for (i = packet_len; --i >= len + stop; )
			if (packet[i] > 0)
				return TDS_CONVERT_OVERFLOW;
		/* test limit */
		for (;; --i, ++limit) {
			if (i <= stop) {
				/* last must be >= not > */
				if (packet[i] >= *limit)
					return TDS_CONVERT_OVERFLOW;
				break;
			}
			if (packet[i] > *limit)
				return TDS_CONVERT_OVERFLOW;
			if (packet[i] < *limit)
				break;
		}
	}
	return 0;
}

#undef USE_128_MULTIPLY
#if defined(__GNUC__) && SIZEOF___INT128 > 0
#define USE_128_MULTIPLY 1
#undef __umulh
#define __umulh(multiplier, multiplicand) \
	((uint64_t) ((((unsigned __int128) (multiplier)) * (multiplicand)) >> 64))
#endif
#if defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#define USE_128_MULTIPLY 1
#endif

#undef USE_I386_DIVIDE
#undef USE_64_MULTIPLY
#ifndef USE_128_MULTIPLY
# if defined(__GNUC__) && __GNUC__ >= 3 && defined(__i386__)
#  define USE_I386_DIVIDE 1
# else
#  define USE_64_MULTIPLY
# endif
#endif

TDS_INT
tds_numeric_change_prec_scale(TDS_NUMERIC * numeric, unsigned char new_prec, unsigned char new_scale)
{
#define TDS_WORD_BITS (8 * sizeof(TDS_WORD))
	static const TDS_WORD factors[] = {
		1, 10, 100, 1000, 10000,
		100000, 1000000, 10000000, 100000000, 1000000000
	};
#ifndef USE_I386_DIVIDE
	/* These numbers are computed as
	 * (2 ** (reverse_dividers_shift[i] + 64)) / (10 ** i) + 1
	 * (** is power).
	 * The shifts are computed to make sure the multiplication error
	 * does not cause a wrong result.
	 *
	 * See also misc/reverse_divisor script.
	 */
	static const TDS_DWORD reverse_dividers[] = {
		1 /* not used */,
		UINT64_C(1844674407370955162),
		UINT64_C(184467440737095517),
		UINT64_C(18446744073709552),
		UINT64_C(1844674407370956),
		UINT64_C(737869762948383),
		UINT64_C(2361183241434823),
		UINT64_C(15111572745182865),
		UINT64_C(48357032784585167),
		UINT64_C(1237940039285380275),
	};
	static const uint8_t reverse_dividers_shift[] = {
		0 /* not used */,
		0,
		0,
		0,
		0,
		2,
		7,
		13,
		18,
		26,
	};
#endif

	TDS_WORD packet[(sizeof(numeric->array) - 1) / sizeof(TDS_WORD)];

	unsigned int i, packet_len;
	int scale_diff, bytes;

	if (numeric->precision < 1 || numeric->precision > MAXPRECISION || numeric->scale > numeric->precision)
		return TDS_CONVERT_FAIL;

	if (new_prec < 1 || new_prec > MAXPRECISION || new_scale > new_prec)
		return TDS_CONVERT_FAIL;

	scale_diff = new_scale - numeric->scale;
	if (scale_diff == 0 && new_prec >= numeric->precision) {
		i = tds_numeric_bytes_per_prec[new_prec] - tds_numeric_bytes_per_prec[numeric->precision];
		if (i > 0) {
			memmove(numeric->array + 1 + i, numeric->array + 1, sizeof(numeric->array) - 1 - i);
			memset(numeric->array + 1, 0, i);
		}
		numeric->precision = new_prec;
		return sizeof(TDS_NUMERIC);
	}

	/* package number */
	bytes = tds_numeric_bytes_per_prec[numeric->precision] - 1;
	i = 0;
	do {
		/*
		 * note that if bytes are smaller we have a small buffer
		 * overflow in numeric->array however is not a problem
		 * cause overflow occurs in numeric and number is fixed below
		 */
		packet[i] = TDS_GET_UA4BE(&numeric->array[bytes-3]);
		++i;
	} while ( (bytes -= sizeof(TDS_WORD)) > 0);
	/* fix last packet */
	if (bytes < 0)
		packet[i-1] &= 0xffffffffu >> (8 * -bytes);
	while (i > 1 && packet[i-1] == 0)
		--i;
	packet_len = i;

	if (scale_diff >= 0) {
		/* check overflow before multiply */
		if (tds_packet_check_overflow(packet, packet_len, new_prec - scale_diff))
			return TDS_CONVERT_OVERFLOW;

		if (scale_diff == 0) {
			i = tds_numeric_bytes_per_prec[numeric->precision] - tds_numeric_bytes_per_prec[new_prec];
			if (i > 0)
				memmove(numeric->array + 1, numeric->array + 1 + i, sizeof(numeric->array) - 1 - i);
			numeric->precision = new_prec;
			return sizeof(TDS_NUMERIC);
		}

		/* multiply */
		do {
			/* multiply by at maximun TDS_WORD_DDIGIT */
			unsigned int n = TDS_MIN(scale_diff, TDS_WORD_DDIGIT);
			TDS_WORD factor = factors[n];
			TDS_WORD carry = 0;
			scale_diff -= n; 
			for (i = 0; i < packet_len; ++i) {
				TDS_DWORD n = packet[i] * ((TDS_DWORD) factor) + carry;
				packet[i] = (TDS_WORD) n;
				carry = n >> TDS_WORD_BITS;
			}
			/* here we can expand number safely cause we know that it can't overflow */
			if (carry)
				packet[packet_len++] = carry;
		} while (scale_diff > 0);
	} else {
		/* check overflow */
		if (new_prec - scale_diff < numeric->precision)
			if (tds_packet_check_overflow(packet, packet_len, new_prec - scale_diff))
				return TDS_CONVERT_OVERFLOW;

		/* divide */
		scale_diff = -scale_diff;
		do {
			unsigned int n = TDS_MIN(scale_diff, TDS_WORD_DDIGIT);
			TDS_WORD factor = factors[n];
			TDS_WORD borrow = 0;
#if defined(USE_128_MULTIPLY)
			TDS_DWORD reverse_divider = reverse_dividers[n];
			uint8_t shift = reverse_dividers_shift[n];
#elif defined(USE_64_MULTIPLY)
			TDS_WORD reverse_divider_low = (TDS_WORD) reverse_dividers[n];
			TDS_WORD reverse_divider_high = (TDS_WORD) (reverse_dividers[n] >> TDS_WORD_BITS);
			uint8_t shift = reverse_dividers_shift[n];
#endif
			scale_diff -= n;
			for (i = packet_len; i > 0; ) {
#ifdef USE_I386_DIVIDE
				--i;
				/* For different reasons this code is still here.
				 * But mainly because although compilers do wonderful things this is hard to get.
				 * One of the reason is that it's hard to understand that the double-precision division
				 * result will fit into 32-bit.
				 */
				__asm__ ("divl %4": "=a"(packet[i]), "=d"(borrow): "0"(packet[i]), "1"(borrow), "r"(factor));
#else
				TDS_DWORD n = (((TDS_DWORD) borrow) << TDS_WORD_BITS) + packet[--i];
#if defined(USE_128_MULTIPLY)
				TDS_DWORD quotient = __umulh(n, reverse_divider);
#else
				TDS_DWORD mul1 = (TDS_DWORD) packet[i] * reverse_divider_low;
				TDS_DWORD mul2 = (TDS_DWORD) borrow * reverse_divider_low + (mul1 >> TDS_WORD_BITS);
				TDS_DWORD mul3 = (TDS_DWORD) packet[i] * reverse_divider_high;
				TDS_DWORD quotient = (TDS_DWORD) borrow * reverse_divider_high + (mul3 >> TDS_WORD_BITS);
				quotient += (mul2 + (mul3 & 0xffffffffu)) >> TDS_WORD_BITS;
#endif
				quotient >>= shift;
				packet[i] = (TDS_WORD) quotient;
				borrow = (TDS_WORD) (n - quotient * factor);
#endif
			}
		} while (scale_diff > 0);
	}

	/* back to our format */
	numeric->precision = new_prec;
	numeric->scale = new_scale;
	bytes = tds_numeric_bytes_per_prec[numeric->precision] - 1;
	for (i = bytes / sizeof(TDS_WORD); i >= packet_len; --i)
		packet[i] = 0;
	for (i = 0; bytes >= (int) sizeof(TDS_WORD); bytes -= sizeof(TDS_WORD), ++i) {
		TDS_PUT_UA4BE(&numeric->array[bytes-3], packet[i]);
	}

	if (bytes) {
		TDS_WORD remainder = packet[i];
		do {
			numeric->array[bytes] = (TDS_UCHAR) remainder;
			remainder >>= 8;
		} while (--bytes);
	}

	return sizeof(TDS_NUMERIC);
}

