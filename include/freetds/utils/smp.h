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

#ifndef _tdsguard_hfFl8IZw2Vf65YyyYQLJTS_
#define _tdsguard_hfFl8IZw2Vf65YyyYQLJTS_

#include <tds_sysdep_public.h>
#include <freetds/bool.h>

typedef struct {
	uint16_t comp[8];
} smp;

extern const smp smp_zero;
extern const smp smp_one;

/** sum a and b */
smp smp_add(smp a, smp b);

/** subtract a and b */
smp smp_sub(smp a, smp b);

/** bitwise not */
smp smp_not(smp a);

/** returns opposite of a */
smp smp_negate(smp a);

/** convert int64 to multiple precision */
smp smp_from_int(int64_t n);

/** checks if number is negative */
bool smp_is_negative(smp a);

/** checks if number is zero */
bool smp_is_zero(smp a);

/** compare a and b, returns >0 if a > b, <0 if a < b, 0 if a == b */
int smp_cmp(smp a, smp b);

/** converts to double */
double smp_to_double(smp a);

/** converts to strings, must be freed with free() */
char *smp_to_string(smp a);

/** converts a string to a number */
smp smp_from_string(const char *s);

#endif /* _tdsguard_hfFl8IZw2Vf65YyyYQLJTS_ */
