/* Base tests utilities
 * Copyright (C) 2025 Aaron M. Ucko
 * Copyright (C) 2025 Frediano Ziglio
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

#ifndef _tdsguard_afBM6E9n8CuIFSBHNNblq5_
#define _tdsguard_afBM6E9n8CuIFSBHNNblq5_

/*
 * Base header for FreeTDS unit tests, even those just covering helpers
 * from the utils and replacements trees.  Should be included first
 * (possibly via a common.h) to be certain of preceding <assert.h>.
 */

/* Ensure assert is always active. */
#if defined(assert) && defined(NDEBUG)
#  error "Include test_base.h (or common.h) earlier"
#endif

#undef NDEBUG

#include <config.h>

#include <freetds/macros.h>

#define TEST_MAIN() int main(int argc TDS_UNUSED, char **argv TDS_UNUSED)

#endif /* _tdsguard_afBM6E9n8CuIFSBHNNblq5_ */
