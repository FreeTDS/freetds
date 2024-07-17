#ifndef _tdsguard_afBM6E9n8CuIFSBHNNblq5
#define _tdsguard_afBM6E9n8CuIFSBHNNblq5

/*
 * Base header for FreeTDS unit tests, even those just covering helpers
 * from the utils and replacements trees.  Should be included first
 * (possibly via a common.h) to be certain of preceding <assert.h>.
 */

/* Ensure assert is always active. */
#undef NDEBUG
#ifdef assert
#  error "Include test_base.h (or common.h) earlier"
#endif

#include <config.h>

#endif /* _tdsguard_afBM6E9n8CuIFSBHNNblq5 */
