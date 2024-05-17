/* Simple multiprecision - small MP library for testing
 * Copyright (C) 2022-2024 Frediano Ziglio
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

#undef NDEBUG
#include <config.h>

#include <assert.h>
#include <string.h>

#include <freetds/utils/smp.h>
#include <freetds/sysdep_private.h>

#define SMP_NUM_COMPONENTS \
	(sizeof(((smp*)0)->comp) / sizeof(((smp*)0)->comp[0]))

const smp smp_zero = {
	{ 0, 0, 0, 0, 0, 0, 0, 0 },
};

const smp smp_one = {
	{ 1, 0, 0, 0, 0, 0, 0, 0 },
};

smp
smp_add(smp a, smp b)
{
	int i;
	uint32_t carry = 0;
	smp res;

	for (i = 0; i < SMP_NUM_COMPONENTS; ++i) {
		uint32_t sum = carry + a.comp[i] + b.comp[i];
		res.comp[i] = (uint16_t) (sum & 0xffffu);
		carry = sum >> 16;
	}
	assert(smp_is_negative(a) != smp_is_negative(b) || smp_is_negative(a) == smp_is_negative(res));
	return res;
}

smp
smp_not(smp a)
{
	int i;
	smp res;
	for (i = 0; i < SMP_NUM_COMPONENTS; ++i)
		res.comp[i] = a.comp[i] ^ 0xffffu;
	return res;
}

smp
smp_negate(smp a)
{
	return smp_add(smp_not(a), smp_one);
}

smp
smp_from_int(int64_t n)
{
	int i;
	uint64_t un;
	smp res;

	if (n >= 0) {
		un = (uint64_t) n;
	} else {
		un = (uint64_t) -n;
	}
	for (i = 0; i < SMP_NUM_COMPONENTS; ++i) {
		res.comp[i] = (uint16_t) (un & 0xffffu);
		un = un >> 16;
	}
	if (n < 0)
		return smp_negate(res);
	return res;
}

bool
smp_is_negative(smp a)
{
	return (a.comp[SMP_NUM_COMPONENTS-1] >> 15) != 0;
}

bool
smp_is_zero(smp a)
{
	int i;
	uint16_t or = 0;

	for (i = 0; i < SMP_NUM_COMPONENTS; ++i)
		or |= a.comp[i];
	return or == 0;
}

smp
smp_sub(smp a, smp b)
{
	smp c = smp_negate(b);
	return smp_add(a, c);
}

double
smp_to_double(smp a)
{
	int i;
	double n = 0;
	double mult = 1.0;
	smp b = a;
	if (smp_is_negative(a))
		b = smp_negate(b);
	for (i = 0; i < SMP_NUM_COMPONENTS; ++i) {
		n += b.comp[i] * mult;
		mult = mult * 65536.0;
	}
	if (smp_is_negative(a))
		return -n;
	return n;
}

int
smp_cmp(smp a, smp b)
{
	smp diff = smp_sub(a, b);
	if (smp_is_negative(diff))
		return -1;
	if (smp_is_zero(diff))
		return 0;
	return 1;
}

// divide and return remainder
static uint16_t
div_small(smp *n, uint16_t div)
{
	int i;
	uint32_t remainder = 0;
	for (i = SMP_NUM_COMPONENTS; --i >= 0;) {
		uint32_t comp = remainder * 0x10000u + n->comp[i];
		remainder = comp % div;
		n->comp[i] = (uint16_t) (comp / div);
	}
	return (uint16_t) remainder;
}

char *
smp_to_string(smp a)
{
	char buf[SMP_NUM_COMPONENTS * 16 * 4 / 10 + 2];
	char *p = buf+sizeof(buf);
	const bool negative = smp_is_negative(a);
	smp n = negative ? smp_negate(a) : a;

	*--p = 0;
	do
		*--p = div_small(&n, 10) + '0';
	while (!smp_is_zero(n));
	if (negative)
		*--p = '-';
	return strdup(p);
}

// multiple a number
static void
mul_small(smp *n, uint16_t factor)
{
	int i;
	uint32_t carry = 0;
	for (i = 0; i < SMP_NUM_COMPONENTS; ++i) {
		uint32_t comp = (uint32_t) n->comp[i] * factor + carry;
		carry = comp >> 16;
		n->comp[i] = (uint16_t) (comp & 0xffffu);
	}
	assert(carry == 0);
}

smp
smp_from_string(const char *s)
{
	bool negative = false;
	smp n = smp_zero;
	uint16_t base = 10;

	switch (*s) {
	case '-':
		negative = true;
	case '+':
		++s;
	}
	if (*s == '0') {
		base = 8;
		++s;
		if (*s == 'x' || *s == 'X') {
			++s;
			base = 16;
		}
	}
	for (;*s; ++s) {
		int digit = 0;
		if (*s == '\'')
			continue;
		mul_small(&n, base);
		if (*s >= '0' && *s <= '9')
			digit = *s - '0';
		else if (*s >= 'a' && *s <= 'z')
			digit = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'Z')
			digit = *s - 'A' + 10;
		else
			assert(!!"Invalid digit entered");
		n = smp_add(n, smp_from_int(digit));
	}
	if (negative)
		return smp_negate(n);
	return n;
}
