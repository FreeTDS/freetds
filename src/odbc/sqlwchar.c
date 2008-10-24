/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2008  Ziglio Frediano
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

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdio.h>

#include "tdsodbc.h"

TDS_RCSID(var, "$Id: sqlwchar.c,v 1.2 2008-10-24 08:29:22 freddy77 Exp $");

#if SIZEOF_SQLWCHAR != SIZEOF_WCHAR_T
size_t sqlwcslen(const SQLWCHAR * s)
{
	const SQLWCHAR *p = s;

	while (*p)
		++p;
	return p - s;
}
#endif

