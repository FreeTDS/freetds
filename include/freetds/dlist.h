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

#ifndef TDS_DLIST_H
#define TDS_DLIST_H

typedef struct dlist_ring {
	struct dlist_ring *next;
	struct dlist_ring *prev;
} dlist_ring;

#if ENABLE_EXTRA_CHECKS
void dlist_ring_check(dlist_ring *ring);
#endif

#define DLIST_FIELDS(name) \
	dlist_ring name

#define DLIST_FOREACH(prefix, list, p) \
	for (p = prefix ## _ ## first(list); p != NULL; p = prefix ## _ ## next(list, p))

#endif /* TDS_DLIST_H */
