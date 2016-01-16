/* Dlist - dynamic list
 * Copyright (C) 2016 Frediano Ziglio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#if !defined(DLIST_FUNC) || !defined(DLIST_TYPE) || !defined(DLIST_LIST_TYPE)
#error Required defines missing!
#endif

#include <assert.h>

#define DLIST_FAKE(list) ((DLIST_TYPE *) (((char *) (list)) - TDS_OFFSET(DLIST_TYPE, next)))

#if ENABLE_EXTRA_CHECKS
void DLIST_FUNC(check)(DLIST_LIST_TYPE *list)
{
	const DLIST_TYPE *end  = DLIST_FAKE(list);
	const DLIST_TYPE *item = end;
	assert(list != NULL);
	do {
		assert(item->prev->next == item);
		assert(item->next->prev == item);
		item = item->next;
	} while (item != end);
}
#endif

#undef DLIST_FAKE
#undef DLIST_FUNC
#undef DLIST_TYPE
#undef DLIST_LIST_TYPE

