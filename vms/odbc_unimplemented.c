/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2003  Craig A. Berry	craigberry@mac.com	1-FEB-2003
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
 
/* This should never be called if the driver manager is doing its job,
 * but we do need an actual last chance function to alias to in the linker
 * options file when creating the driver shareable image.  Get rid of this
 * file entirely if and when all of the functions are implemented and the 
 * aliases in odbc_driver_axp.opt have been changed to direct procedure
 * references.
 */

#include <errno.h>
#include <stdarg.h>

#include "sql.h"
#include "tdsodbc.h"

static char software_version[] = "$Id: odbc_unimplemented.c,v 1.1 2003-03-01 12:48:36 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

SQLRETURN SQL_API
freetds_odbc_unimplemented(va_list ap)
{
	errno = ENOTSUP;
	return SQL_ERROR;
}
