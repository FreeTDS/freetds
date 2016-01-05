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

typedef struct
{
	DLIST_FIELDS(DLIST_TYPE);
} DLIST_LIST_TYPE;

#define DLIST_FAKE(list) ((DLIST_TYPE *) (((char *) (list)) - TDS_OFFSET(DLIST_TYPE, next)))

static inline void DLIST_FUNC(init)(DLIST_LIST_TYPE *list)
{
	list->next = list->prev = DLIST_FAKE(list);
}

static inline void DLIST_FUNC(check)(DLIST_LIST_TYPE *list)
{
#if ENABLE_EXTRA_CHECKS
	DLIST_TYPE *item = list->next;
	DLIST_TYPE *end  = DLIST_FAKE(list);
	assert(item != NULL);
	do {
		assert(item->prev->next == item);
		assert(item->next->prev == item);
		item = item->next;
	} while (item != end);
#endif
}

static inline DLIST_TYPE *DLIST_FUNC(first)(DLIST_LIST_TYPE *list)
{
	return list->next == DLIST_FAKE(list) ? NULL : list->next;
}

static inline DLIST_TYPE *DLIST_FUNC(last)(DLIST_LIST_TYPE *list)
{
	return list->prev == DLIST_FAKE(list) ? NULL : list->prev;
}

static inline DLIST_TYPE *DLIST_FUNC(next)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	return item->next == DLIST_FAKE(list) ? NULL : item->next;
}

static inline DLIST_TYPE *DLIST_FUNC(prev)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	return item->prev == DLIST_FAKE(list) ? NULL : item->prev;
}

static inline void DLIST_FUNC(prepend)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_FUNC(check)(list);
	assert(item->next == NULL && item->prev == NULL);
	list->next->prev = item;
	item->next = list->next;
	item->prev = DLIST_FAKE(list);
	list->next = item;
	assert(item->next != NULL && item->prev != NULL);
	DLIST_FUNC(check)(list);
}

static inline void DLIST_FUNC(append)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_FUNC(check)(list);
	assert(item->next == NULL && item->prev == NULL);
	list->prev->next = item;
	item->prev = list->prev;
	item->next = DLIST_FAKE(list);
	list->prev = item;
	assert(item->next != NULL && item->prev != NULL);
	DLIST_FUNC(check)(list);
}

static inline void DLIST_FUNC(remove)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_TYPE *prev = item->prev, *next = item->next;
	DLIST_FUNC(check)(list);
	if (prev)
		prev->next = next;
	if (next)
		next->prev = prev;
	item->prev = NULL;
	item->next = NULL;
	DLIST_FUNC(check)(list);
}

static inline bool DLIST_FUNC(in_list)(DLIST_LIST_TYPE *list, DLIST_TYPE *item)
{
	DLIST_FUNC(check)(list);
	return item->prev != NULL || item->next != NULL;
}

#undef DLIST_FAKE
#undef DLIST_NAME
#undef DLIST_TYPE
#undef DLIST_LIST_TYPE

