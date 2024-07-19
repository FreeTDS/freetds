/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2016  Frediano Ziglio
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
 * Purpose: test dlist code.
 */

#include <freetds/utils/test_base.h>

#include <stdio.h>
#include <assert.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <freetds/utils/dlist.h>

typedef struct
{
	DLIST_FIELDS(list_item);
	int n;
} test_item;

#define DLIST_PREFIX list
#define DLIST_LIST_TYPE test_items
#define DLIST_ITEM_TYPE test_item
#include <freetds/utils/dlist.tmpl.h>

TEST_MAIN()
{
	test_items list[1];
	test_item items[6], *p;
	int n;

	list_init(list);
	memset(&items, 0, sizeof(items));

	assert(list_first(list) == NULL);
	assert(list_last(list) == NULL);

	p = &items[0];
	p->n = 2;
	list_append(list, p++);
	p->n = 3;
	list_append(list, p++);
	p->n = 1;
	list_prepend(list, p++);

	assert(!list_in_list(list, p));
	assert(list_first(list)->n == 1);
	assert(list_last(list)->n == 3);

	/* check sequence is [1, 2, 3] */
	n = 0;
	DLIST_FOREACH(list, list, p) {
		assert(p->n == ++n);
	}
	assert(n == 3);

	/* remove item with n == 2, sequence will be [1, 3] */
	assert(list_in_list(list, &items[0]));
	list_remove(list, &items[0]);
	assert(!list_in_list(list, &items[0]));

	/* change sequence to [1, 2] */
	items[1].n = 2;

	n = 0;
	DLIST_FOREACH(list, list, p) {
		assert(p->n == ++n);
	}
	assert(n == 2);

	return 0;
}

