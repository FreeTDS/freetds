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

#if !defined(DLIST_NAME) || !defined(DLIST_TYPE) || !defined(DLIST_LIST_TYPE)
#error Required defines missing!
#endif

typedef struct
{
	dlist_ring ring;
} DLIST_LIST_TYPE;

#undef DLIST_ITEM
#define DLIST_ITEM(ring) ((DLIST_TYPE *) (((char *) (ring)) - TDS_OFFSET(DLIST_TYPE, DLIST_NAME(item))))

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

static inline DLIST_TYPE *DLIST_NAME(first)(DLIST_LIST_TYPE *list)
{
	return list->ring.next == &list->ring ? NULL : DLIST_ITEM(list->ring.next);
}

static inline DLIST_TYPE *DLIST_NAME(last)(DLIST_LIST_TYPE *list)
{
	return list->ring.prev == &list->ring ? NULL : DLIST_ITEM(list->ring.prev);
}

static inline DLIST_TYPE *DLIST_NAME(next)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	return item->DLIST_NAME(item).next == &list->ring ? NULL : DLIST_ITEM(item->DLIST_NAME(item).next);
}

static inline DLIST_TYPE *DLIST_NAME(prev)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	return item->DLIST_NAME(item).prev == &list->ring ? NULL : DLIST_ITEM(item->DLIST_NAME(item).prev);
}

static inline void DLIST_NAME(prepend)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_NAME(check)(list);
	assert(item->DLIST_NAME(item).next == NULL && item->DLIST_NAME(item).prev == NULL);
	list->ring.next->prev = &item->DLIST_NAME(item);
	item->DLIST_NAME(item).next = list->ring.next;
	item->DLIST_NAME(item).prev = &list->ring;
	list->ring.next = &item->DLIST_NAME(item);
	assert(item->DLIST_NAME(item).next != NULL && item->DLIST_NAME(item).prev != NULL);
	DLIST_NAME(check)(list);
}

static inline void DLIST_NAME(append)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_NAME(check)(list);
	assert(item->DLIST_NAME(item).next == NULL && item->DLIST_NAME(item).prev == NULL);
	list->ring.prev->next = &item->DLIST_NAME(item);
	item->DLIST_NAME(item).prev = list->ring.prev;
	item->DLIST_NAME(item).next = &list->ring;
	list->ring.prev = &item->DLIST_NAME(item);
	assert(item->DLIST_NAME(item).next != NULL && item->DLIST_NAME(item).prev != NULL);
	DLIST_NAME(check)(list);
}

static inline void DLIST_NAME(remove)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	dlist_ring *prev = item->DLIST_NAME(item).prev, *next = item->DLIST_NAME(item).next;
	DLIST_NAME(check)(list);
	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
	item->DLIST_NAME(item).prev = NULL;
	item->DLIST_NAME(item).next = NULL;
	DLIST_NAME(check)(list);
}

static inline bool DLIST_NAME(in_list)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_NAME(check)(list);
	return item->DLIST_NAME(item).prev != NULL || item->DLIST_NAME(item).next != NULL;
}

#undef DLIST_ITEM
#undef DLIST_NAME
#undef DLIST_TYPE
#undef DLIST_LIST_TYPE

