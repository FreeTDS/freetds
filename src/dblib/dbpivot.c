/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 2011  James K. Lowden
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

#include <config.h>

#include <stdarg.h>

#include <freetds/time.h>

#include <assert.h>
#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */

#if HAVE_ERRNO_H
# include <errno.h>
#endif /* HAVE_ERRNO_H */

 
#include <freetds/tds.h>
#include <freetds/thread.h>
#include <freetds/convert.h>
#include <freetds/utils/string.h>
#include <freetds/replacements.h>
#include <sybfront.h>
#include <sybdb.h>
#include <syberror.h>
#include <dblib.h>

#define TDS_FIND(k,b,c) tds_find(k, b, TDS_VECTOR_SIZE(b), sizeof(b[0]), c)

typedef bool (*compare_func)(const void *, const void *);

static void *
tds_find(const void *key, const void *base, size_t nelem, size_t width,
         compare_func compar)
{
	size_t n;
	char *p = (char *) base;

	for (n = nelem; n != 0; --n) {
		if (compar(key, p))
			return p;
		p += width;
	}
	return NULL;
}


struct col_t
{
	size_t len;
	TDS_SERVER_TYPE type;
	int null_indicator;
	char *s;
	union {
		DBTINYINT 	ti;
		DBSMALLINT	si;
		DBINT		i;
		DBREAL		r;
		DBFLT8		f;
	} data;
};

static TDS_SERVER_TYPE infer_col_type(int sybtype);

static struct col_t *
col_init(struct col_t *pcol, int sybtype, size_t collen)
{
	assert(pcol);
	
	pcol->type = infer_col_type(sybtype);
	if (pcol->type == TDS_INVALID_TYPE)
		return NULL;
	pcol->len = collen;
	pcol->s = NULL;

	switch (sybtype) {
	case 0:
		pcol->len = 0;
		return NULL;
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
		collen = 30;
		/* fall through */
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
	case SYBNTEXT:
		pcol->len = collen;
		if ((pcol->s = tds_new(char, 1+collen)) == NULL) {
			return NULL;
		}
		break;
	}
	return pcol;
}

static void
col_free(struct col_t *p)
{
	free(p->s);
	memset(p, 0, sizeof(*p));
}

static bool
col_equal(const struct col_t *pc1, const struct col_t *pc2)
{
	assert( pc1 && pc2 );
	assert( pc1->type == pc2->type );
	
	switch (pc1->type) {
	
	case SYBCHAR:
	case SYBVARCHAR:
		if( pc1->len != pc2->len)
			return false;
		return strncmp(pc1->s, pc2->s, pc1->len) == 0;
	case SYBINT1:
	case SYBUINT1:
	case SYBSINT1:
		return pc1->data.ti == pc2->data.ti;
	case SYBINT2:
	case SYBUINT2:
		return pc1->data.si == pc2->data.si;
	case SYBINT4:
	case SYBUINT4:
		return pc1->data.i == pc2->data.i;
	case SYBFLT8:
		return pc1->data.f == pc2->data.f;
	case SYBREAL:
		return pc1->data.r == pc2->data.r;

	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBMSTABLE:
	case SYBNVARCHAR:
	case SYBINT8:
	case XSYBCHAR:
	case XSYBVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
	case XSYBVARBINARY:
	case XSYBBINARY:
	case SYBUNIQUE:
	case SYBVARIANT:
	case SYBMSUDT:
	case SYBMSXML:
	case SYBMSDATE:
	case SYBMSTIME:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
	case SYBLONGBINARY:
	case SYBUINT8:
	case SYBDATE:
	case SYBDATEN:
	case SYB5INT8:
	case SYBINTERVAL:
	case SYBTIME:
	case SYBTIMEN:
	case SYBUINTN:
	case SYBUNITEXT:
	case SYBXML:
	case SYB5BIGDATETIME:
	case SYB5BIGTIME:

		assert( false && pc1->type );
		break;
	}
	return false;
}

static void *
col_buffer(struct col_t *pcol) 
{
	switch (pcol->type) {
	
	case SYBCHAR:
	case SYBVARCHAR:
		return pcol->s;
	case SYBINT1:
	case SYBUINT1:
	case SYBSINT1:
		return &pcol->data.ti;
	case SYBINT2:
	case SYBUINT2:
		return &pcol->data.si;
	case SYBINT4:
	case SYBUINT4:
		return &pcol->data.i;
	case SYBFLT8:
		return &pcol->data.f;
	case SYBREAL:
		return &pcol->data.r;

	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	case SYBMSTABLE:
	case SYBNVARCHAR:
	case SYBINT8:
	case XSYBCHAR:
	case XSYBVARCHAR:
	case XSYBNVARCHAR:
	case XSYBNCHAR:
	case XSYBVARBINARY:
	case XSYBBINARY:
	case SYBUNIQUE:
	case SYBVARIANT:
	case SYBMSUDT:
	case SYBMSXML:
	case SYBMSDATE:
	case SYBMSTIME:
	case SYBMSDATETIME2:
	case SYBMSDATETIMEOFFSET:
	case SYBLONGBINARY:
	case SYBUINT8:
	case SYBDATE:
	case SYBDATEN:
	case SYB5INT8:
	case SYBINTERVAL:
	case SYBTIME:
	case SYBTIMEN:
	case SYBUINTN:
	case SYBUNITEXT:
	case SYBXML:
	case SYB5BIGDATETIME:
	case SYB5BIGTIME:
		assert( false && pcol->type );
		break;
	}
	return NULL;

}

#if 0
static int
col_print(FILE* out, const struct col_t *pcol) 
{
	char *fmt;
	
	switch (pcol->type) {
	
	case SYBCHAR:
	case SYBVARCHAR:
		return (int) fwrite(pcol->s, pcol->len, 1, out);
	case SYBINT1:
		return fprintf(out, "%d", (int)pcol->ti);
	case SYBINT2:
		return fprintf(out, "%d", (int)pcol->si);
	case SYBINT4:
		return fprintf(out, "%d", (int)pcol->i);
	case SYBFLT8:
		return fprintf(out, "%f",      pcol->f);
	case SYBREAL:
		return fprintf(out, "%f", (double)pcol->r);

	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
		assert( false && pcol->type );
		break;
	}
	return false;
}
#endif
static struct col_t *
col_cpy(struct col_t *pdest, const struct col_t *psrc)
{
	assert( pdest && psrc );
	assert( psrc->len > 0 || psrc->null_indicator == -1);
	
	memcpy(pdest, psrc, sizeof(*pdest));
	
	if (psrc->s) {
		assert(psrc->len >= 0);
		if ((pdest->s = tds_new(char, psrc->len)) == NULL)
			return NULL;
		memcpy(pdest->s, psrc->s, psrc->len);
	}
	
	assert( pdest->len > 0 || pdest->null_indicator == -1);
	return pdest;
}

static bool
col_null( const struct col_t *pcol )
{
	assert(pcol);
	return pcol->null_indicator == -1;
} 

static char *
string_value(const struct col_t *pcol)
{
	char *output = NULL;
	int len = -1;

	switch (pcol->type) {
	case SYBCHAR:
	case SYBVARCHAR:
		if ((output = tds_new0(char, 1 + pcol->len)) == NULL)
			return NULL;
		strncpy(output, pcol->s, pcol->len);
		return output;
		break;
	case SYBINT1:
		len = asprintf(&output, "%d", (int)pcol->data.ti);
		break;
	case SYBINT2:
		len = asprintf(&output, "%d", (int)pcol->data.si);
		break;
	case SYBINT4:
		len = asprintf(&output, "%d", (int)pcol->data.i);
		break;
	case SYBFLT8:
		len = asprintf(&output, "%f", pcol->data.f);
		break;
	case SYBREAL:
		len = asprintf(&output, "%f", (double)pcol->data.r);
		break;

	default:
	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
		assert( false && pcol->type );
		return NULL;
		break;
	}

	return len >= 0? output : NULL;
}

static char *
join(int argc, char *argv[], const char sep[])
{
	size_t len = 0;
	char **p, *output;
		
	for (p=argv; p < argv + argc; p++) {
		len += strlen(*p);
	}
	
	len += 1 + argc * strlen(sep); /* allows one too many */ 
	
	output = tds_new0(char, len);
	if (!output)
		return NULL;
	
	for (p=argv; p < argv + argc; p++) {
		if (p != argv)
			strcat(output, sep);
		strcat(output, *p);
	}
	return output;
}

static TDS_SERVER_TYPE
infer_col_type(int sybtype) 
{
	switch (sybtype) {
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
	case SYBNTEXT:
		return SYBCHAR;
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
		return SYBCHAR;
	case SYBINT1:
	case SYBBIT:
	case SYBBITN:
		return SYBINT1;
	case SYBINT2:
		return SYBINT2;
	case SYBINT4:
	case SYBINTN:
		return SYBINT4;
	case SYBFLT8:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBNUMERIC:
	case SYBDECIMAL:
		return SYBFLT8;
	case SYBREAL:
		return SYBREAL;

	case SYBIMAGE:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
		assert( false && sybtype );
		break;
	}
	return TDS_INVALID_TYPE;
}

static int
bind_type(int sybtype)
{
	switch (sybtype) {
	case SYBCHAR:
	case SYBVARCHAR:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBDATETIME:
	case SYBDATETIME4:
	case SYBDATETIMN:
		return NTBSTRINGBIND;
	case SYBINT1:
	case SYBBIT:
	case SYBBITN:
		return TINYBIND;
	case SYBINT2:
		return SMALLBIND;
	case SYBINT4:
	case SYBINTN:
		return INTBIND;
	case SYBFLT8:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBNUMERIC:
	case SYBDECIMAL:
		return FLT8BIND;
	case SYBREAL:
		return REALBIND;

	case SYBIMAGE:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
		assert( false && sybtype );
		break;
	}
	return 0;
}

typedef struct KEY_T
{
	int nkeys;
	struct col_t *keys;
} KEY_T;

static bool
key_equal(const KEY_T *a, const KEY_T *b)
{
	int i;
	
	assert(a && b);
	assert(a->keys && b->keys);
	assert(a->nkeys == b->nkeys);
	
	for (i=0; i < a->nkeys; i++) {
		if (! col_equal(a->keys+i, b->keys+i))
			return false;
	}
	return true;
}


static void
key_free(KEY_T *p)
{
	col_free(p->keys);
	free(p->keys);
	memset(p, 0, sizeof(*p));
}

static KEY_T *
key_cpy(KEY_T *pdest, const KEY_T *psrc)
{
	int i;
	
	assert( pdest && psrc );
	
	if ((pdest->keys = tds_new0(struct col_t, psrc->nkeys)) == NULL)
		return NULL;

	pdest->nkeys = psrc->nkeys;
	
	for( i=0; i < psrc->nkeys; i++) {
		if (NULL == col_cpy(pdest->keys+i, psrc->keys+i))
			return NULL;
	}

	return pdest;
}


static char *
make_col_name(DBPROCESS *dbproc, const KEY_T *k)
{
	const struct col_t *pc;
	char **names, **s, *output;
	
	assert(k);
	assert(k->nkeys);
	assert(k->keys);
	
	s = names = tds_new0(char *, k->nkeys);
	if (!s) {
		dbperror(dbproc, SYBEMEM, errno);
		return NULL;
	}
	for(pc=k->keys; pc < k->keys + k->nkeys; pc++) {
		*s++ = string_value(pc);
	}
	
	output = join(k->nkeys, names, "/");
	
	for(s=names; s < names + k->nkeys; s++) {
		free(*s);
	}
	free(names);
	
	return output;
}
	

typedef struct agg_t
{
	KEY_T row_key, col_key;
	struct col_t value;
} AGG_T;

#if 0
static bool
agg_key_equal(const void *a, const void *b)
{
	int i;
	const AGG_T *p1 = a, *p2 = b;
	
	assert(p1 && p2);
	assert(p1->row_key.keys  && p2->row_key.keys);
	assert(p1->row_key.nkeys == p2->row_key.nkeys);
	
	for( i=0; i < p1->row_key.nkeys; i++ ) {
		if (! col_equal(p1->row_key.keys+i, p2->row_key.keys+i))
			return false;
	}

	return true;
}
#endif

static bool
agg_next(const AGG_T *p1, const AGG_T *p2)
{
	int i;

	assert(p1 && p2);
	
	if (p1->row_key.keys == NULL || p2->row_key.keys == NULL)
		return false;
	
	assert(p1->row_key.keys  && p2->row_key.keys);
	assert(p1->row_key.nkeys == p2->row_key.nkeys);
	
	assert(p1->col_key.keys  && p2->col_key.keys);
	assert(p1->col_key.nkeys == p2->col_key.nkeys);
	
	for( i=0; i < p1->row_key.nkeys; i++ ) {
		assert(p1->row_key.keys[i].type);
		assert(p2->row_key.keys[i].type);
		if (p1->row_key.keys[i].type != p2->row_key.keys[i].type)
			return false;
	}

	for( i=0; i < p1->row_key.nkeys; i++ ) {
		if (! col_equal(p1->row_key.keys+i, p2->row_key.keys+i))
			return false;
	}

	for( i=0; i < p1->col_key.nkeys; i++ ) {
		if (p1->col_key.keys[i].type != p2->col_key.keys[i].type)
			return false;
	}

	for( i=0; i < p1->col_key.nkeys; i++ ) {
		if (! col_equal(p1->col_key.keys+i, p2->col_key.keys+i))
			return false;
	}

	return true;
}

static void
agg_free(AGG_T *p)
{
	key_free(&p->row_key);
	key_free(&p->col_key);
	col_free(&p->value);
}

static bool
agg_equal(const AGG_T *p1, const AGG_T *p2)
{
	int i;
	
	assert(p1 && p2);
	assert(p1->row_key.keys && p1->col_key.keys);
	assert(p2->row_key.keys && p2->col_key.keys);

	assert(p1->row_key.nkeys == p2->row_key.nkeys);
	assert(p1->col_key.nkeys == p2->col_key.nkeys);
	
	/* todo: use key_equal */
	for( i=0; i < p1->row_key.nkeys; i++ ) {
		if (! col_equal(p1->row_key.keys+i, p2->row_key.keys+i))
			return false;
	}
	for( i=0; i < p1->col_key.nkeys; i++ ) {
		if (! col_equal(p1->col_key.keys+i, p2->col_key.keys+i))
			return false;
	}
	return true;
}

#undef TEST_MALLOC
#define TEST_MALLOC(dest,type) \
	{if (!(dest = (type*)calloc(1, sizeof(type)))) goto Cleanup;}

#undef TEST_CALLOC
#define TEST_CALLOC(dest,type,n) \
	{if (!(dest = (type*)calloc((n), sizeof(type)))) goto Cleanup;}

#define tds_alloc_column() ((TDSCOLUMN*) calloc(1, sizeof(TDSCOLUMN)))

static TDSRESULTINFO *
alloc_results(TDS_USMALLINT num_cols)
{
	TDSRESULTINFO *res_info;
	TDSCOLUMN **ppcol;

	TEST_MALLOC(res_info, TDSRESULTINFO);
	res_info->ref_count = 1;
	TEST_CALLOC(res_info->columns, TDSCOLUMN *, num_cols);
	
	for (ppcol = res_info->columns; ppcol < res_info->columns + num_cols; ppcol++)
		if ((*ppcol = tds_alloc_column()) == NULL)
			goto Cleanup;
	res_info->num_cols = num_cols;
	res_info->row_size = 0;
	return res_info;

      Cleanup:
	tds_free_results(res_info);
	return NULL;
}

static TDSRET
set_result_column(TDSSOCKET * tds, TDSCOLUMN * curcol, const char name[], const struct col_t *pvalue)
{
	assert(curcol && pvalue);
	assert(name);

	curcol->column_usertype = pvalue->type;
	curcol->column_nullable = true;
	curcol->column_writeable = false;
	curcol->column_identity = false;

	tds_set_column_type(tds->conn, curcol, pvalue->type);	/* sets "cardinal" type */

	curcol->column_timestamp = (curcol->column_type == SYBBINARY && curcol->column_usertype == TDS_UT_TIMESTAMP);

#if 0
	curcol->funcs->get_info(tds, curcol);
#endif
	curcol->on_server.column_size = curcol->column_size;

	if (!tds_dstr_copy(&curcol->column_name, name))
		return TDS_FAIL;

	tdsdump_log(TDS_DBG_INFO1, "tds7_get_data_info: \n"
		    "\tcolname = %s\n"
		    "\ttype = %d (%s)\n"
		    "\tserver's type = %d (%s)\n"
		    "\tcolumn_varint_size = %d\n"
		    "\tcolumn_size = %d (%d on server)\n",
		    tds_dstr_cstr(&curcol->column_name),
		    curcol->column_type, tds_prtype(curcol->column_type), 
		    curcol->on_server.column_type, tds_prtype(curcol->on_server.column_type), 
		    curcol->column_varint_size,
		    curcol->column_size, curcol->on_server.column_size);

	return TDS_SUCCESS;
}

struct metadata_t { KEY_T *pacross; char *name; struct col_t col; };


static bool
reinit_results(TDSSOCKET * tds, TDS_USMALLINT num_cols, const struct metadata_t meta[])
{
	TDSRESULTINFO *info;
	int i;

	assert(tds);
	assert(num_cols);
	assert(meta);
	
	tds_free_all_results(tds);
	tds->rows_affected = TDS_NO_COUNT;

	if ((info = alloc_results(num_cols)) == NULL)
		return false;

	tds_set_current_results(tds, info);
	if (tds->cur_cursor) {
		tds_free_results(tds->cur_cursor->res_info);
		tds->cur_cursor->res_info = info;
		tdsdump_log(TDS_DBG_INFO1, "set current_results to cursor->res_info\n");
	} else {
		tds->res_info = info;
		tdsdump_log(TDS_DBG_INFO1, "set current_results (%u column%s) to tds->res_info\n", (unsigned) num_cols, (num_cols==1? "":"s"));
	}

	tdsdump_log(TDS_DBG_INFO1, "setting up %u columns\n", (unsigned) num_cols);
	
	for (i = 0; i < num_cols; i++) {
		set_result_column(tds, info->columns[i], meta[i].name, &meta[i].col);
		info->columns[i]->bcp_terminator = (char*) meta[i].pacross;	/* overload available pointer */
	}
		
	if (num_cols > 0) {
		static const char dashes[31] = "------------------------------";
		tdsdump_log(TDS_DBG_INFO1, " %-20s %-15s %-15s %-7s\n", "name", "size/wsize", "type/wtype", "utype");
		tdsdump_log(TDS_DBG_INFO1, " %-20s %15s %15s %7s\n", dashes+10, dashes+30-15, dashes+30-15, dashes+30-7);
	}
	for (i = 0; i < num_cols; i++) {
		TDSCOLUMN *curcol = info->columns[i];

		tdsdump_log(TDS_DBG_INFO1, " %-20s %7d/%-7d %7d/%-7d %7d\n", 
						tds_dstr_cstr(&curcol->column_name),
						curcol->column_size, curcol->on_server.column_size, 
						curcol->column_type, curcol->on_server.column_type, 
						curcol->column_usertype);
	}

#if 1
	/* all done now allocate a row for tds_process_row to use */
	if (TDS_FAILED(tds_alloc_row(info))) return false;
#endif
	return true;
}

typedef struct pivot_t
{
	DBPROCESS *dbproc;
	STATUS status;
	DB_RESULT_STATE dbresults_state;
	
	AGG_T *output;
	KEY_T *across;
	size_t nout;
	TDS_USMALLINT nacross;
} PIVOT_T;

static bool
pivot_key_equal(const PIVOT_T *a, const PIVOT_T *b)
{
	assert(a && b);
	
	return a->dbproc == b->dbproc;
}

static PIVOT_T *pivots = NULL;
static size_t npivots = 0;

PIVOT_T *
dbrows_pivoted(DBPROCESS *dbproc)
{
	PIVOT_T P;

	assert(dbproc);
	P.dbproc = dbproc;
	
	return (PIVOT_T *) tds_find(&P, pivots, npivots, sizeof(*pivots), (compare_func) pivot_key_equal);
}

STATUS
dbnextrow_pivoted(DBPROCESS *dbproc, PIVOT_T *pp)
{
	int i;
	AGG_T candidate, *pout;

	assert(pp);
	assert(dbproc && dbproc->tds_socket);
	assert(dbproc->tds_socket->res_info);
	assert(dbproc->tds_socket->res_info->columns || 0 == dbproc->tds_socket->res_info->num_cols);
	
	for (pout = pp->output; pout < pp->output + pp->nout; pout++) {
		if (pout->row_key.keys != NULL)
			break;
	}

	if (pout == pp->output + pp->nout) {
		dbproc->dbresults_state = _DB_RES_NEXT_RESULT;
		return NO_MORE_ROWS;
	}

	memset(&candidate, 0, sizeof(candidate));
	key_cpy(&candidate.row_key, &pout->row_key);
	
	/* "buffer_transfer_bound_data" */
	for (i = 0; i < dbproc->tds_socket->res_info->num_cols; i++) {
		struct col_t *pval = NULL;
		TDSCOLUMN *pcol = dbproc->tds_socket->res_info->columns[i];
		assert(pcol);
		
		if (pcol->column_nullbind) {
			if (pcol->column_cur_size < 0) {
				*(DBINT *)(pcol->column_nullbind) = -1;
			} else {
				*(DBINT *)(pcol->column_nullbind) = 0;
			}
		}
		if (!pcol->column_varaddr) {
			tdsdump_log(TDS_DBG_ERROR, "no pcol->column_varaddr in col %d\n", i);
			continue;
		}

		/* find column in output */
		if (pcol->bcp_terminator == NULL) { /* not a cross-tab column */
			pval = &candidate.row_key.keys[i];
		} else {
			AGG_T *pcan;
			key_cpy(&candidate.col_key, (KEY_T *) pcol->bcp_terminator);
			pcan = (AGG_T *) tds_find(&candidate, pout, pp->output + pp->nout - pout,
						  sizeof(*pp->output), (compare_func) agg_next);
			if (pcan != NULL) {
				/* flag this output as used */
				pout->row_key.keys = NULL;
				pval = &pcan->value;
			}
		}
		
		if (!pval || col_null(pval)) {  /* nothing in output for this x,y location */
			dbgetnull(dbproc, pcol->column_bindtype, pcol->column_bindlen, (BYTE *) pcol->column_varaddr);
			continue;
		}
		
		assert(pval);
		
		pcol->column_size = pval->len;
		pcol->column_data = (unsigned char *) col_buffer(pval);
		
		copy_data_to_host_var(	dbproc, 
					pval->type, 
					(BYTE *) col_buffer(pval),
					pval->len, 
					(BYTE *) pcol->column_varaddr,  
					pcol->column_bindlen,
					pcol->column_bindtype, 
					(DBINT*) pcol->column_nullbind
					);
	}

	return REG_ROW;
}

/** 
 * Pivot the rows, creating a new resultset
 *
 * Call dbpivot() immediately after dbresults().  It calls dbnextrow() as long as
 * it returns REG_ROW, transforming the results into a cross-tab report.  
 * dbpivot() modifies the metadata such that DB-Library can be used tranparently: 
 * retrieve the rows as usual with dbnumcols(), dbnextrow(), etc. 
 *
 * @dbproc, our old friend
 * @nkeys the number of left-edge columns to group by
 * @keys  an array of left-edge columns to group by
 * @ncols the number of top-edge columns to group by
 * @cols  an array of top-edge columns to group by
 * @func  the aggregation function to use
 * @val   the number of the column to which @func is applied
 *
 * @returns the return code from the final call to dbnextrow().  
 *  Success is normally indicated by NO_MORE_ROWS.  
 */
RETCODE
dbpivot(DBPROCESS *dbproc, int nkeys, int *keys, int ncols, int *cols, DBPIVOT_FUNC func, int val)
{
	enum { logalot = 1 };
	PIVOT_T P, *pp;
	AGG_T input, *pout = NULL;
	struct metadata_t *metadata, *pmeta;
	int i;
	TDS_USMALLINT nmeta = 0;

	tdsdump_log(TDS_DBG_FUNC, "dbpivot(%p, %d,%p, %d,%p, %p, %d)\n", dbproc, nkeys, keys, ncols, cols, func, val);
	if (logalot) {
		char buffer[1024] = {'\0'}, *s = buffer;
		static const char *const names[2] = { "\tkeys (down)", "\n\tcols (across)" };
		int *p = keys, *pend = p + nkeys;
		
		for (i=0; i < 2; i++) {
			const char *sep = "";
			s += sprintf(s, "%s: ", names[i]);
			for ( ; p < pend; p++) {
				s += sprintf(s, "%s%d", sep, *p);
				sep = ", ";
			}
			p = cols;
			pend = p + ncols;
			assert(s < buffer + sizeof(buffer));
		}
		tdsdump_log(TDS_DBG_FUNC, "%s\n", buffer);
	}
	
	memset(&input,  0, sizeof(input));
	
	P.dbproc = dbproc;
	pp = (PIVOT_T *) tds_find(&P, pivots, npivots, sizeof(*pivots),
				  (compare_func) pivot_key_equal);
	if (pp == NULL) {
		pp = (PIVOT_T *) TDS_RESIZE(pivots, 1 + npivots);
		if (!pp)
			return FAIL;
		pp += npivots++;
	} else {
		agg_free(pp->output);
		key_free(pp->across);		
	}
	memset(pp, 0, sizeof(*pp));

	if ((input.row_key.keys = tds_new0(struct col_t, nkeys)) == NULL)
		return FAIL;
	input.row_key.nkeys = nkeys;
	for (i=0; i < nkeys; i++) {
		int type = dbcoltype(dbproc, keys[i]);
		int len = dbcollen(dbproc, keys[i]);
		assert(type && len);
		
		if (!col_init(input.row_key.keys+i, type, len))
			return FAIL;
		if (FAIL == dbbind(dbproc, keys[i], bind_type(type), (DBINT) input.row_key.keys[i].len,
				   (BYTE *) col_buffer(input.row_key.keys+i)))
			return FAIL;
		if (FAIL == dbnullbind(dbproc, keys[i], &input.row_key.keys[i].null_indicator))
			return FAIL;
	}
	
	if ((input.col_key.keys = tds_new0(struct col_t, ncols)) == NULL)
		return FAIL;
	input.col_key.nkeys = ncols;
	for (i=0; i < ncols; i++) {
		int type = dbcoltype(dbproc, cols[i]);
		int len = dbcollen(dbproc, cols[i]);
		assert(type && len);
		
		if (!col_init(input.col_key.keys+i, type, len))
			return FAIL;
		if (FAIL == dbbind(dbproc, cols[i], bind_type(type), (DBINT) input.col_key.keys[i].len,
				   (BYTE *) col_buffer(input.col_key.keys+i)))
			return FAIL;
		if (FAIL == dbnullbind(dbproc, cols[i], &input.col_key.keys[i].null_indicator))
			return FAIL;
	}
	
	/* value */ {
		int type = dbcoltype(dbproc, val);
		int len = dbcollen(dbproc, val);
		assert(type && len);
		
		if (!col_init(&input.value, type, len))
			return FAIL;
		if (FAIL == dbbind(dbproc, val, bind_type(type), input.value.len,
				   (BYTE *) col_buffer(&input.value)))
			return FAIL;
		if (FAIL == dbnullbind(dbproc, val, &input.value.null_indicator))
			return FAIL;
	}
	
	while ((pp->status = dbnextrow(dbproc)) == REG_ROW) {
		/* add to unique list of crosstab columns */
		if (tds_find(&input.col_key, pp->across, pp->nacross, sizeof(*pp->across),
			     (compare_func) key_equal) == NULL) {
			if (!TDS_RESIZE(pp->across, 1 + pp->nacross))
				return FAIL;
			key_cpy(pp->across + pp->nacross, &input.col_key);
		}
		assert(pp->across);
		
		if ((pout = tds_find(&input, pp->output, pp->nout, sizeof(*pp->output), (compare_func) agg_equal)) == NULL ) {
			if (!TDS_RESIZE(pp->output, 1 + pp->nout))
				return FAIL;
			pout = pp->output + pp->nout++;

			
			if ((pout->row_key.keys = tds_new0(struct col_t, input.row_key.nkeys)) == NULL)
				return FAIL;
			key_cpy(&pout->row_key, &input.row_key);

			if ((pout->col_key.keys = tds_new0(struct col_t, input.col_key.nkeys)) == NULL)
				return FAIL;
			key_cpy(&pout->col_key, &input.col_key);

			if (!col_init(&pout->value, input.value.type, input.value.len))
				return FAIL;
		}
		
		func(&pout->value, &input.value);

	}

	/* Mark this proc as pivoted, so that dbnextrow() sees it when the application calls it */
	pp->dbproc = dbproc;
	pp->dbresults_state = dbproc->dbresults_state;
	dbproc->dbresults_state = pp->output < pout? _DB_RES_RESULTSET_ROWS : _DB_RES_RESULTSET_EMPTY;
	
	/*
	 * Initialize new metadata
	 */
	nmeta = input.row_key.nkeys + pp->nacross;	
	metadata = tds_new0(struct metadata_t, nmeta);
	if (!metadata) {
		dbperror(dbproc, SYBEMEM, errno);
		return FAIL;
	}
	assert(pp->across || pp->nacross == 0);
	
	/* key columns are passed through as-is, verbatim */
	for (i=0; i < input.row_key.nkeys; i++) {
		assert(i < nkeys);
		metadata[i].name = strdup(dbcolname(dbproc, keys[i]));
		metadata[i].pacross = NULL;
		col_cpy(&metadata[i].col, input.row_key.keys+i);
	}

	/* pivoted columms are found in the "across" data */
	for (i=0, pmeta = metadata + input.row_key.nkeys; i < pp->nacross; i++) {
		struct col_t col;
		if (!col_init(&col, SYBFLT8, sizeof(double)))
			return FAIL;
		assert(pmeta + i < metadata + nmeta);
		pmeta[i].name = make_col_name(dbproc, pp->across+i);
		if (!pmeta[i].name)
			return FAIL;
		assert(pp->across);
		pmeta[i].pacross = pp->across + i;
		col_cpy(&pmeta[i].col, pp->nout? &pp->output[0].value : &col);
	}

	if (!reinit_results(dbproc->tds_socket, nmeta, metadata)) {
		return FAIL;
	}
	
	return SUCCEED;
}

/* 
 * Aggregation functions 
 */

void
dbpivot_count (struct col_t *tgt, const struct col_t *src)
{
	assert( tgt && src);
	assert (src->type);
	
	tgt->type = SYBINT4;
	
	if (! col_null(src))
		tgt->data.i++;
}

void
dbpivot_sum (struct col_t *tgt, const struct col_t *src)
{
	assert( tgt && src);
	assert (src->type);
	
	tgt->type = src->type;
	
	if (col_null(src))
		return;

	switch (src->type) {
	case SYBINT1:
		tgt->data.ti += src->data.ti;
		break;
	case SYBINT2:
		tgt->data.si += src->data.si;
		break;
	case SYBINT4:
		tgt->data.i += src->data.i;
		break;
	case SYBFLT8:
		tgt->data.f += src->data.f;
		break;
	case SYBREAL:
		tgt->data.r += src->data.r;
		break;

	case SYBCHAR:
	case SYBVARCHAR:
	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	default:
		tdsdump_log(TDS_DBG_INFO1, "dbpivot_sum(): invalid operand %d\n", src->type);
		tgt->type = SYBINT4;
		tgt->data.i = 0;
		break;
	}
}

void
dbpivot_min (struct col_t *tgt, const struct col_t *src)
{
	assert( tgt && src);
	assert (src->type);
	
	tgt->type = src->type;
	
	if (col_null(src))
		return;

	switch (src->type) {
	case SYBINT1:
		tgt->data.ti = tgt->data.ti < src->data.ti? tgt->data.ti : src->data.ti;
		break;
	case SYBINT2:
		tgt->data.si = tgt->data.si < src->data.si? tgt->data.si : src->data.si;
		break;
	case SYBINT4:
		tgt->data.i = tgt->data.i < src->data.i? tgt->data.i : src->data.i;
		break;
	case SYBFLT8:
		tgt->data.f = tgt->data.f < src->data.f? tgt->data.f : src->data.f;
		break;
	case SYBREAL:
		tgt->data.r = tgt->data.r < src->data.r? tgt->data.r : src->data.r;
		break;

	case SYBCHAR:
	case SYBVARCHAR:
	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	default:
		tdsdump_log(TDS_DBG_INFO1, "dbpivot_sum(): invalid operand %d\n", src->type);
		tgt->type = SYBINT4;
		tgt->data.i = 0;
		break;
	}
}

void
dbpivot_max (struct col_t *tgt, const struct col_t *src)
{
	assert( tgt && src);
	assert (src->type);
	
	tgt->type = src->type;
	
	if (col_null(src))
		return;

	switch (src->type) {
	case SYBINT1:
		tgt->data.ti = tgt->data.ti > src->data.ti? tgt->data.ti : src->data.ti;
		break;
	case SYBINT2:
		tgt->data.si = tgt->data.si > src->data.si? tgt->data.si : src->data.si;
		break;
	case SYBINT4:
		tgt->data.i = tgt->data.i > src->data.i? tgt->data.i : src->data.i;
		break;
	case SYBFLT8:
		tgt->data.f = tgt->data.f > src->data.f? tgt->data.f : src->data.f;
		break;
	case SYBREAL:
		tgt->data.r = tgt->data.r > src->data.r? tgt->data.r : src->data.r;
		break;

	case SYBCHAR:
	case SYBVARCHAR:
	case SYBINTN:
	case SYBDATETIME:
	case SYBBIT:
	case SYBTEXT:
	case SYBNTEXT:
	case SYBIMAGE:
	case SYBMONEY4:
	case SYBMONEY:
	case SYBDATETIME4:
	case SYBBINARY:
	case SYBVOID:
	case SYBVARBINARY:
	case SYBBITN:
	case SYBNUMERIC:
	case SYBDECIMAL:
	case SYBFLTN:
	case SYBMONEYN:
	case SYBDATETIMN:
	default:
		tdsdump_log(TDS_DBG_INFO1, "dbpivot_sum(): invalid operand %d\n", src->type);
		tgt->type = SYBINT4;
		tgt->data.i = 0;
		break;
	}
}

static const struct name_t {
	char name[14];
	DBPIVOT_FUNC func;
} names[] = 
	{ { "count", 	dbpivot_count }
	, { "sum", 	dbpivot_sum }
	, { "min", 	dbpivot_min }
	, { "max",	dbpivot_max }
	};

static bool
name_equal( const struct name_t *n1, const struct name_t *n2 ) 
{
	assert(n1 && n2);
	return strcmp(n1->name, n2->name) == 0;
}

DBPIVOT_FUNC 
dbpivot_lookup_name( const char name[] )
{
	struct name_t *n = TDS_FIND(name, names, (compare_func) name_equal);
	
	return n ? n->func : NULL;
}
