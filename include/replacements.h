/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
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

#ifndef _replacements_h_
#define _replacements_h_

#ifdef __cplusplus
extern "C" {
#endif

static char  rcsid_replacements_h[] =
         "$Id: replacements.h,v 1.1 2002-09-23 02:13:52 castellano Exp $";
static void *no_unused_replacements_h_warn[] = {
	rcsid_replacements_h,
	no_unused_replacements_h_warn};

#ifndef HAVE_ASPRINTF
int asprintf(char **ret, const char *fmt, ...);
#endif /* HAVE_ASPRINTF */

#ifndef HAVE_VASPRINTF
int vasprintf(char **ret, const char *fmt, va_list ap);
#endif /* HAVE_VASPRINTF */

#ifdef __cplusplus
}
#endif

#endif
