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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <bkpublic.h>

static char  software_version[]   = "$Id: blk.c,v 1.3 2002-09-27 03:09:50 castellano Exp $";
static void *no_unused_var_warn[] = {software_version,
                                     no_unused_var_warn};

CS_RETCODE blk_init(CS_BLKDESC *blkdesc, CS_INT direction, CS_CHAR *tablename, CS_INT tnamelen)
{
	return CS_SUCCEED;
}
CS_RETCODE blk_alloc(CS_CONNECTION *conn, CS_INT version, CS_BLKDESC **blkptr)
{
	return CS_SUCCEED;
}
CS_RETCODE blk_props(CS_BLKDESC *blkdesc, CS_INT action, CS_INT property, CS_VOID *buffer, CS_INT buflen, CS_INT *outlen)
{
	return CS_SUCCEED;
}
CS_RETCODE blk_done(CS_BLKDESC *blkdesc, CS_INT type, CS_INT *outrow)
{
	return CS_SUCCEED;
}
CS_RETCODE blk_bind(CS_BLKDESC *blkdesc, CS_INT colnum, CS_DATAFMT *datafmt, CS_VOID *buffer, CS_INT *datalen, CS_SMALLINT *indicator)
{
	return CS_SUCCEED;
}
CS_RETCODE blk_rowxfer(CS_BLKDESC *blkdesc)
{
	return CS_SUCCEED;
}
CS_RETCODE blk_drop(CS_BLKDESC *blkdesc)
{
	return CS_SUCCEED;
}
