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

#include <freetds/bool.h>
#include <freetds/utils.h>
#include <freetds/macros.h>

#if !defined(DLIST_PREFIX) || !defined(DLIST_ITEM_TYPE) || !defined(DLIST_LIST_TYPE)
#error Required defines for dlist missing!
#endif

#if defined(DLIST_NAME) || defined(DLIST_PASTER) || \
	defined(DLIST_EVALUATOR) || defined(DLIST_ITEM)
#error Some internal dlist macros already defined
#endif

typedef struct
{
	dlist_ring ring;
} DLIST_LIST_TYPE;

#define DLIST_PASTER(x,y) x ## _ ## y
#define DLIST_EVALUATOR(x,y)  DLIST_PASTER(x,y)
#define DLIST_NAME(suffix) DLIST_EVALUATOR(DLIST_PREFIX, suffix)
#define DLIST_ITEM(ring) \
	((DLIST_ITEM_TYPE *) (((char *) (ring)) - TDS_OFFSET(DLIST_ITEM_TYPE, DLIST_NAME(item))))

static inline void DLIST_NAME(check)(DLIST_LIST_TYPE *list)
{
#if ENABLE_EXTRA_CHECKS
	assert(list != NULL);
	dlist_ring_check(&list->ring);
#endif
}

static inline void DLIST_NAME(init)(DLIST_LIST_TYPE *list)
{
	list->ring.next = list->ring.prev = &list->ring;
	DLIST_NAME(check)(list);
}

static inline DLIST_ITEM_TYPE *DLIST_NAME(first)(DLIST_LIST_TYPE *list)
{
	return list->ring.next == &list->ring ? NULL : DLIST_ITEM(list->ring.next);
}

static inline DLIST_ITEM_TYPE *DLIST_NAME(last)(DLIST_LIST_TYPE *list)
{
	return list->ring.prev == &list->ring ? NULL : DLIST_ITEM(list->ring.prev);
}

static inline DLIST_ITEM_TYPE *DLIST_NAME(next)(DLIST_LIST_TYPE *list, DLIST_ITEM_TYPE *item)
{
	return item->DLIST_NAME(item).next == &list->ring ? NULL : DLIST_ITEM(item->DLIST_NAME(item).next);
}

static inline DLIST_ITEM_TYPE *DLIST_NAME(prev)(DLIST_LIST_TYPE *list, DLIST_ITEM_TYPE *item)
{
	return item->DLIST_NAME(item).prev == &list->ring ? NULL : DLIST_ITEM(item->DLIST_NAME(item).prev);
}

static inline void DLIST_NAME(prepend)(DLIST_LIST_TYPE *list, DLIST_ITEM_TYPE *item)
{
	DLIST_NAME(check)(list);
	dlist_insert_after(&list->ring, &item->DLIST_NAME(item));
	DLIST_NAME(check)(list);
}

static inline void DLIST_NAME(append)(DLIST_LIST_TYPE *list, DLIST_ITEM_TYPE *item)
{
	DLIST_NAME(check)(list);
	dlist_insert_after(list->ring.prev, &item->DLIST_NAME(item));
	DLIST_NAME(check)(list);
}

static inline void DLIST_NAME(remove)(DLIST_LIST_TYPE *list, DLIST_ITEM_TYPE *item)
{
	dlist_ring *prev = item->DLIST_NAME(item).prev, *next = item->DLIST_NAME(item).next;
	DLIST_NAME(check)(list);
	if (prev) {
		prev->next = next;
		next->prev = prev;
	}
	item->DLIST_NAME(item).prev = NULL;
	item->DLIST_NAME(item).next = NULL;
	DLIST_NAME(check)(list);
}

static inline bool DLIST_NAME(in_list)(DLIST_LIST_TYPE *list, DLIST_ITEM_TYPE *item)
{
	DLIST_NAME(check)(list);
	return item->DLIST_NAME(item).prev != NULL || item->DLIST_NAME(item).next != NULL;
}

#undef DLIST_ITEM
#undef DLIST_NAME
#undef DLIST_ITEM_TYPE
#undef DLIST_LIST_TYPE
#undef DLIST_PREFIX
#undef DLIST_PASTER
#undef DLIST_EVALUATOR

