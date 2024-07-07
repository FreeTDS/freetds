/* try all types from server */

#include "common.h"

#include <ctlib.h>

#define TDS_DONT_DEFINE_DEFAULT_FUNCTIONS
#include "../../tds/unittests/common.h"
#include <freetds/tds.h>

static CS_CONTEXT *ctx = NULL;

static void test_type(TDSSOCKET *tds TDS_UNUSED, TDSCOLUMN *col)
{
	TDSRESULTINFO *resinfo, *bindinfo;
	TDSCOLUMN *oldcol, *bindcol;
	char out_buf[256];
	CS_INT len;

	/* we should be able to support a type coming from server */
	if (_ct_get_client_type(col, false) == CS_ILLEGAL_TYPE) {
		fprintf(stderr, "not supported\n");
		assert(0);
	}

	/* try to convert anyway, this should succeed */
	resinfo = tds_alloc_results(1);
	assert(resinfo);
	bindinfo = tds_alloc_results(1);
	assert(bindinfo);

	/* hack to pass our column to _ct_bind_data */
	oldcol = resinfo->columns[0];
	resinfo->columns[0] = col;

	memset(out_buf, '-', sizeof(out_buf));
	bindcol = bindinfo->columns[0];
	bindcol->column_varaddr = out_buf;
	bindcol->column_bindlen = sizeof(out_buf);
	bindcol->column_bindtype = CS_CHAR_TYPE;
	bindcol->column_bindfmt = CS_FMT_NULLTERM;
	len = -1;
	bindcol->column_lenbind = &len;

	/* every column should be at least be convertible to something */
	if (_ct_bind_data(ctx, resinfo, bindinfo, 0)) {
		fprintf(stderr, "conversion failed\n");
		assert(0);
	}

	/* just safety, we use small data for now */
	assert(len < sizeof(out_buf));

	/* we said terminated, check for terminator */
	assert(len >= 1 && len < sizeof(out_buf));
	assert(out_buf[len - 1] == 0);
	printf("output (%d): %s\n", len, out_buf);

	resinfo->columns[0] = oldcol;
	tds_free_results(resinfo);
	tds_free_results(bindinfo);
}

int
main(void)
{
	TDSCONTEXT *tds_ctx;
	TDSSOCKET *tds;

	tdsdump_open(tds_dir_getenv(TDS_DIR("TDSDUMP")));

	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));

	tds_ctx = tds_alloc_context(NULL);
	assert(tds_ctx);
	tds = tds_alloc_socket(tds_ctx, 512);
	assert(tds);
	tds->conn->use_iconv = 0;
	if (TDS_FAILED(tds_iconv_open(tds->conn, "UTF-8", 1))) {
		fprintf(stderr, "Failed to initialize iconv\n");
		return 1;
	}

	tds_all_types(tds, test_type);

	tds_free_socket(tds);
	tds_free_context(tds_ctx);

	cs_ctx_drop(ctx);

	return 0;
}
