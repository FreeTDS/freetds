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

#include "bkpublic.h"
#include "ctlib.h"

static char software_version[] = "$Id: blk.c,v 1.7 2004-03-22 20:41:07 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

CS_RETCODE
blk_alloc(CS_CONNECTION * connection, CS_INT version, CS_BLKDESC ** blk_pointer)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_alloc()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_bind(CS_BLKDESC * blkdesc, CS_INT colnum, CS_DATAFMT * datafmt, CS_VOID * buffer, CS_INT * datalen, CS_SMALLINT * indicator)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_bind()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_colval(SRV_PROC * srvproc, CS_BLKDESC * blkdescp, CS_BLK_ROW * rowp, CS_INT colnum, CS_VOID * valuep, CS_INT valuelen,
	   CS_INT * outlenp)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_colval()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_default(CS_BLKDESC * blkdesc, CS_INT colnum, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_default()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_describe(CS_BLKDESC * blkdesc, CS_INT colnum, CS_DATAFMT * datafmt)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_describe()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_done(CS_BLKDESC * blkdesc, CS_INT type, CS_INT * outrow)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_done()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_drop(CS_BLKDESC * blkdesc)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_drop()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_getrow(SRV_PROC * srvproc, CS_BLKDESC * blkdescp, CS_BLK_ROW * rowp)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_getrow()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_gettext(SRV_PROC * srvproc, CS_BLKDESC * blkdescp, CS_BLK_ROW * rowp, CS_INT bufsize, CS_INT * outlenp)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_gettext()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_init(CS_BLKDESC * blkdesc, CS_INT direction, CS_CHAR * tablename, CS_INT tnamelen)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_init()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_props(CS_BLKDESC * blkdesc, CS_INT action, CS_INT property, CS_VOID * buffer, CS_INT buflen, CS_INT * outlen)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_props()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_rowalloc(SRV_PROC * srvproc, CS_BLK_ROW ** row)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_rowalloc()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_rowdrop(SRV_PROC * srvproc, CS_BLK_ROW * row)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_rowdrop()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_rowxfer(CS_BLKDESC * blkdesc)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_rowxfer()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_rowxfer_mult(CS_BLKDESC * blkdesc, CS_INT * row_count)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_rowxfer_mult()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_sendrow(CS_BLKDESC * blkdesc, CS_BLK_ROW * row)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_sendrow()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_sendtext(CS_BLKDESC * blkdesc, CS_BLK_ROW * row, CS_BYTE * buffer, CS_INT buflen)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_sendtext()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_srvinit(SRV_PROC * srvproc, CS_BLKDESC * blkdescp)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_srvinit()\n");
	return CS_FAIL;
}

CS_RETCODE
blk_textxfer(CS_BLKDESC * blkdesc, CS_BYTE * buffer, CS_INT buflen, CS_INT * outlen)
{

	tdsdump_log(TDS_DBG_FUNC, "%L UNIMPLEMENTED blk_textxfer()\n");
	return CS_FAIL;
}
