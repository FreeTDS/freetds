/* test win64 consistency */
#include "common.h"

static char software_version[] = "$Id: test64.c,v 1.5 2008-02-13 08:52:09 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

/*
set ipd processed_ptr with
SQLParamOptions/SQLSetDescField/SQL_ATTR_PARAMS_PROCESSED_PTR
check always same value IPD->processed_ptr attr 
*/

static void
check_ipd_params(void)
{
	void *ptr, *ptr2;
	SQLHDESC desc;
	SQLINTEGER ind;

	CHK(SQLGetStmtAttr,(Statement, SQL_ATTR_PARAMS_PROCESSED_PTR, &ptr, sizeof(ptr), NULL));

	/* get IPD */
	CHK(SQLGetStmtAttr, (Statement, SQL_ATTR_IMP_PARAM_DESC, &desc, sizeof(desc), &ind));

	CHK(SQLGetDescField, (desc, 0, SQL_DESC_ROWS_PROCESSED_PTR, &ptr2, sizeof(ptr2), &ind));

	if (ptr != ptr2) {
		fprintf(stderr, "IPD inconsistency ptr %p ptr2 %p\n", ptr, ptr2);
		exit(1);
	}
}

static void
set_ipd_params1(SQLULEN *ptr)
{
	CHK(SQLSetStmtAttr,(Statement, SQL_ATTR_PARAMS_PROCESSED_PTR, ptr, 0));
}

static void
set_ipd_params2(SQLULEN *ptr)
{
	SQLHDESC desc;
	SQLINTEGER ind;

	/* get IPD */
	CHK(SQLGetStmtAttr, (Statement, SQL_ATTR_IMP_PARAM_DESC, &desc, sizeof(desc), &ind));

	CHK(SQLSetDescField, (desc, 1, SQL_DESC_ROWS_PROCESSED_PTR, ptr, 0));
}

static void
set_ipd_params3(SQLULEN *ptr)
{
	CHK(SQLParamOptions, (Statement, 2, ptr));
}

typedef void (*rows_set_t)(SQLULEN*);

static const rows_set_t param_set[] = {
	set_ipd_params1,
	set_ipd_params2,
	set_ipd_params3,
	NULL
};

#define MALLOC_N(t, n) (t*) malloc(n*sizeof(t))

static void
test_params(void)
{
#define ARRAY_SIZE 2
	const rows_set_t *p;
	SQLULEN len;
	SQLUINTEGER *ids = MALLOC_N(SQLUINTEGER,ARRAY_SIZE);
	SQLLEN *id_lens = MALLOC_N(SQLLEN,ARRAY_SIZE);
	unsigned long int h, l;
	unsigned int n;

	for (n = 0; n < ARRAY_SIZE; ++n) {
		ids[n] = n;
		id_lens[n] = 0;
	}

	/* test setting just some test pointers */
	set_ipd_params1(int2ptr(0x01020304));
	check_ipd_params();
	set_ipd_params2(int2ptr(0xabcdef12));
	check_ipd_params();
	set_ipd_params3(int2ptr(0x87654321));
	check_ipd_params();

	/* now see results */
	for (p = param_set; *p != NULL; ++p) {
		ResetStatement();
		len = 0xdeadbeef;
		len <<= 16;
		len <<= 16;
		len |= 12345678;

		(*p)(&len);
		check_ipd_params();

		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_PARAMSET_SIZE, (void *) int2ptr(ARRAY_SIZE), 0));
		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 5, 0, ids, 0, id_lens));

		Command(Statement, "INSERT INTO #tmp1(i) VALUES(?)");
		SQLMoreResults(Statement);
		for (n = 0; n < ARRAY_SIZE; ++n)
			SQLMoreResults(Statement);
		l = len;
		len >>= 16;
		h = len >> 16;
		l &= 0xfffffffflu;
		if (h != 0 || l != 2) {
			fprintf(stderr, "Wrong number returned in param rows high %lu low %lu\n", h, l);
			exit(1);
		}
	}

	free(ids);
	free(id_lens);
}

/*
set ird processed_ptr with
SQLExtendedFetch/SQLSetDescField/SQL_ATTR_ROWS_FETCHED_PTR
check always same value IRD->processed_ptr attr 
*/

static void
check_ird_params(void)
{
	void *ptr, *ptr2;
	SQLHDESC desc;
	SQLINTEGER ind;

	CHK(SQLGetStmtAttr,(Statement, SQL_ATTR_ROWS_FETCHED_PTR, &ptr, sizeof(ptr), NULL));

	/* get IRD */
	CHK(SQLGetStmtAttr, (Statement, SQL_ATTR_IMP_ROW_DESC, &desc, sizeof(desc), &ind));

	CHK(SQLGetDescField, (desc, 0, SQL_DESC_ROWS_PROCESSED_PTR, &ptr2, sizeof(ptr2), &ind));

	if (ptr != ptr2) {
		fprintf(stderr, "IRD inconsistency ptr %p ptr2 %p\n", ptr, ptr2);
		exit(1);
	}
}

static void
set_ird_params1(SQLULEN *ptr)
{
	CHK(SQLSetStmtAttr,(Statement, SQL_ATTR_ROWS_FETCHED_PTR, ptr, 0));
}

static void
set_ird_params2(SQLULEN *ptr)
{
	SQLHDESC desc;
	SQLINTEGER ind;

	/* get IRD */
	CHK(SQLGetStmtAttr, (Statement, SQL_ATTR_IMP_ROW_DESC, &desc, sizeof(desc), &ind));

	CHK(SQLSetDescField, (desc, 1, SQL_DESC_ROWS_PROCESSED_PTR, ptr, 0));
}

static const rows_set_t row_set[] = {
	set_ird_params1,
	set_ird_params2,
	NULL
};

#define MALLOC_N(t, n) (t*) malloc(n*sizeof(t))

static void
test_rows(void)
{
	const rows_set_t *p;
	SQLULEN len;
	SQLUINTEGER *ids = MALLOC_N(SQLUINTEGER,ARRAY_SIZE);
	SQLLEN *id_lens = MALLOC_N(SQLLEN,ARRAY_SIZE);
	unsigned long int h, l;
	unsigned int n;

	for (n = 0; n < ARRAY_SIZE; ++n) {
		ids[n] = n;
		id_lens[n] = 0;
	}

	/* test setting just some test pointers */
	set_ird_params1(int2ptr(0x01020304));
	check_ird_params();
	set_ird_params2(int2ptr(0xabcdef12));
	check_ird_params();

	/* now see results */
	for (p = row_set; ; ++p) {
		ResetStatement();
		len = 0xdeadbeef;
		len <<= 16;
		len <<= 16;
		len |= 12345678;
		if (*p)
			(*p)(&len);
		check_ird_params();

//		CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_PARAMSET_SIZE, (void *) int2ptr(ARRAY_SIZE), 0));
//		CHK(SQLBindParameter, (Statement, 1, SQL_PARAM_INPUT, SQL_C_ULONG, SQL_INTEGER, 5, 0, ids, 0, id_lens));

		CHK(SQLBindCol, (Statement, 1, SQL_C_ULONG, ids, 0, id_lens));
		if (*p) {
			CHK(SQLSetStmtAttr, (Statement, SQL_ATTR_ROW_ARRAY_SIZE, (void *) int2ptr(ARRAY_SIZE), 0));

			Command(Statement, "SELECT DISTINCT i FROM #tmp1");
			SQLFetch(Statement);
		} else {
			CHK(SQLSetStmtAttr, (Statement, SQL_ROWSET_SIZE, (void *) int2ptr(ARRAY_SIZE), 0));
			Command(Statement, "SELECT DISTINCT i FROM #tmp1");
			CHK(SQLExtendedFetch, (Statement, SQL_FETCH_NEXT, 0, &len, NULL));
		}
		SQLMoreResults(Statement);

		l = len;
		len >>= 16;
		h = len >> 16;
		l &= 0xfffffffflu;
		if (h != 0 || l != 2) {
			fprintf(stderr, "Wrong number returned in rows high %lu(0x%lx) low %lu(0x%lx)\n", h, h, l, l);
			exit(1);
		}

		if (!*p)
			break;
	}

	free(ids);
	free(id_lens);
}

int
main(void)
{
	use_odbc_version3 = 1;
	Connect();

	Command(Statement, "create table #tmp1 (i int)");

	test_params();
	test_rows();

	Disconnect();
	return 0;
}

