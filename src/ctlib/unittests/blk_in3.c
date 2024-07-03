#include <config.h>
#include <freetds/macros.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bkpublic.h>
#include <ctpublic.h>
#include "common.h"

static int last_error;
static const char * error_descriptions[512];

struct full_result
{
	CS_RETCODE rc;
	int        error; // zero if none
};

struct bi_input
{
	CS_INT       host_type;
	CS_INT       host_format;
	CS_INT       host_maxlen;
	const char * var_text;
	const void * var_addr;
	CS_INT       var_len;
};


static CS_INT
record_cslibmsg(CS_CONTEXT * context TDS_UNUSED, CS_CLIENTMSG * errmsg)
{
	int index = (((CS_LAYER(errmsg->msgnumber) - 1) << 8)
		     | CS_NUMBER(errmsg->msgnumber));
	last_error = errmsg->msgnumber;
	if (error_descriptions[index] == NULL) {
		error_descriptions[index] = strdup(errmsg->msgstring);
	}
	return CS_SUCCEED;
}

static CS_RETCODE
record_ctlibmsg(CS_CONTEXT * context, CS_CONNECTION * connection TDS_UNUSED,
		CS_CLIENTMSG * errmsg)
{
	return record_cslibmsg(context, errmsg);
}


#define capture_result(fr, f, args) \
	do { \
		struct full_result *_fr = (fr); \
		last_error = 0; \
		_fr->rc = f args; \
		_fr->error = last_error; \
	} while(0)

static void
print_full_result(const struct full_result * fr)
{
	switch (fr->rc) {
	case CS_SUCCEED:        fputs(" +", stdout); break;
	case CS_FAIL:           fputs("- ", stdout); break;
	case CS_MEM_ERROR:      fputs("-M", stdout); break;
	case CS_PENDING:        fputs("-P", stdout); break;
	case CS_QUIET:          fputs("-Q", stdout); break;
	case CS_BUSY:           fputs("-B", stdout); break;
	case CS_INTERRUPT:      fputs("-I", stdout); break;
	case CS_BLK_HAS_TEXT:   fputs("-K", stdout); break;
	case CS_CONTINUE:       fputs("-C", stdout); break;
	case CS_FATAL:          fputs("-F", stdout); break;
	case CS_RET_HAFAILOVER: fputs("-H", stdout); break;
	case CS_CANCELED:       fputs("-X", stdout); break;
	case CS_ROW_FAIL:       fputs("-R", stdout); break;
	case CS_END_DATA:	fputs("-d", stdout); break;
	case CS_END_RESULTS:	fputs("-r", stdout); break;
	case CS_END_ITEM:	fputs("-i", stdout); break;
	case CS_NOMSG:		fputs("-N", stdout); break;
	case CS_TIMED_OUT:	fputs("-T", stdout); break;
	default:		fputs("-?", stdout); break;
	}
	if (fr->error) {
		printf("%d:%d", CS_LAYER(fr->error), CS_NUMBER(fr->error));
	}
}


static CS_RETCODE
do_bind(CS_BLKDESC * blkdesc, const struct bi_input * bi, bool is_null)
{
	CS_DATAFMT datafmt;
	CS_RETCODE ret;

	static /* const */ CS_INT colnum = 1, zero_len = 0;
	static /* const */ CS_SMALLINT null_ind = -1, not_null_ind = 0;

	ret = blk_describe(blkdesc, colnum, &datafmt);
	if (ret != CS_SUCCEED) {
		fprintf(stderr, "blk_describe(%d) failed", colnum);
		return ret;
	}

	datafmt.format = bi->host_format;
	datafmt.datatype = bi->host_type;
	datafmt.maxlength = bi->host_maxlen;
	datafmt.count = 1;
	switch (bi->host_type) {
	case CS_DECIMAL_TYPE:
		datafmt.scale = ((CS_DECIMAL *) bi->var_addr)->scale;
		datafmt.precision = ((CS_DECIMAL *) bi->var_addr)->precision;
		break;
	case CS_NUMERIC_TYPE:
		datafmt.scale = ((CS_NUMERIC *) bi->var_addr)->scale;
		datafmt.precision = ((CS_NUMERIC *) bi->var_addr)->precision;
		break;
	default:
		break;
	}

	return blk_bind(blkdesc, colnum, &datafmt, (CS_VOID *) bi->var_addr,
			is_null ? &zero_len : (CS_INT *) &bi->var_len,
			is_null ? &null_ind : &not_null_ind);
}


static char *
bin_str(const void * p, CS_INT l)
{
	/* Slight overkill, but simplifies logic */
	char * s = malloc(l * 3 + 1);
	int i;
	for (i = 0;  i < l;  ++i) {
		snprintf(s + 3 * i, 4, "%02x ", ((unsigned char *) p)[i]);
	}
	s[l * 3 - 1] = '\0';
	return s;
}


static int
do_fetch(CS_COMMAND * cmd)
{
	CS_INT count, row_count = 0;
	CS_RETCODE ret;

	while ((ret = ct_fetch(cmd, CS_UNUSED, CS_UNUSED, CS_UNUSED, &count))
	       == CS_SUCCEED) {
		char buffer[256];
		CS_DATAFMT datafmt;
		CS_INT len;

		row_count += count;
		ct_describe(cmd, 1, &datafmt);
		ct_get_data(cmd, 1, buffer, sizeof(buffer), &len);
		if (datafmt.datatype == CS_CHAR_TYPE) {
			printf(" '%.*s'", len, buffer);
		} else if (len > 0) {
			char * s = bin_str(buffer, len);
			printf(" %s", s);
			free(s);
		}
	}
	if (ret == CS_ROW_FAIL) {
		fputs(" [FAIL]", stdout);
		return 1;
	} else if (ret == CS_END_DATA) {
		return 0;
	} else {
		printf(" [??? (%d)]", ret);
		return 1;
	}
}

static CS_RETCODE
do_query(CS_COMMAND * cmd, const char * query)
{
	int result_num;
	CS_RETCODE results_ret, result_type;

	check_call(ct_command, (cmd, CS_LANG_CMD, query, CS_NULLTERM,
				CS_UNUSED));
	check_call(ct_send, (cmd));

	result_num = 0;
	while ((results_ret = ct_results(cmd, &result_type)) == CS_SUCCEED) {
		if (result_type == CS_STATUS_RESULT)
			continue;
		switch ((int) result_type) {
		case CS_ROW_RESULT:
			if (do_fetch(cmd)) {
				return CS_FAIL;
			}
			break;
		}
		result_num++;
	}
	return results_ret;
}

static const char *
host_type_name(CS_INT host_type)
{
	switch (host_type) {
	case CS_CHAR_TYPE:           return "CHAR";
	case CS_BINARY_TYPE:         return "BINARY";
	case CS_LONGCHAR_TYPE:       return "LNGCHAR";
	case CS_LONGBINARY_TYPE:     return "LONGBIN";
	case CS_TEXT_TYPE:           return "TEXT";
	case CS_IMAGE_TYPE:          return "IMAGE";
	case CS_TINYINT_TYPE:        return "TINYINT";
	case CS_SMALLINT_TYPE:       return "SMOLINT";
	case CS_INT_TYPE:            return "INT";
	case CS_REAL_TYPE:           return "REAL";
	case CS_FLOAT_TYPE:          return "FLOAT";
	case CS_BIT_TYPE:            return "BIT";
	case CS_DATETIME_TYPE:       return "DT";
	case CS_DATETIME4_TYPE:      return "DT4";
	case CS_MONEY_TYPE:          return "MONEY";
	case CS_MONEY4_TYPE:         return "MONEY4";
	case CS_NUMERIC_TYPE:        return "NUMERIC";
	case CS_DECIMAL_TYPE:        return "DECIMAL";
	case CS_VARCHAR_TYPE:        return "VARCHAR";
	case CS_VARBINARY_TYPE:      return "VARBIN";
	case CS_LONG_TYPE:           return "LONG";
	case CS_SENSITIVITY_TYPE:    return "SENS";
	case CS_BOUNDARY_TYPE:       return "BOUND";
	case CS_VOID_TYPE:           return "VOID";
	case CS_USHORT_TYPE:         return "USHORT";
	case CS_UNICHAR_TYPE:        return "UNICHAR";
	case CS_BLOB_TYPE:           return "BLOB";
	case CS_DATE_TYPE:           return "DATE";
	case CS_TIME_TYPE:           return "TIME";
#ifdef CS_UNITEXT_TYPE
	case CS_UNITEXT_TYPE:        return "UNITEXT";
#endif
#ifdef CS_BIGINT_TYPE
	case CS_BIGINT_TYPE:         return "BIGINT";
#endif
#ifdef CS_UINT_TYPE
	case CS_USMALLINT_TYPE:      return "USMLINT";
	case CS_UINT_TYPE:           return "UINT";
	case CS_UBIGINT_TYPE:        return "UBIGINT";
#endif
#ifdef CS_XML_TYPE
	case CS_XML_TYPE:            return "XML";
#endif
#ifdef CS_BIGDATETIME_TYPE
	case CS_BIGDATETIME_TYPE:    return "BIGDT";
	case CS_BIGTIME_TYPE:        return "BIGTIME";
#endif
#ifdef CS_TEXTLOCATOR_TYPE
	case CS_TEXTLOCATOR_TYPE:    return "TEXTLOC";
	case CS_UNITEXTLOCATOR_TYPE: return "UTXTLOC";
#endif
#ifdef CS_UNIQUE_TYPE
	case CS_UNIQUE_TYPE:         return "UNIQUE";
#endif
	}
	return "???";
}

static void
test_one_case(CS_CONNECTION * conn, CS_COMMAND * cmd,
              const struct bi_input * bi, const char * sql_abbrev,
              bool is_null)
{
	struct full_result fr;
	CS_BLKDESC * blkdesc;
	CS_INT count;

	printf("%s\t%s\t", host_type_name(bi->host_type), sql_abbrev);

	check_call(blk_alloc, (conn, UT_BLK_VERSION, &blkdesc));
	check_call(blk_init, (blkdesc, CS_BLK_IN, "#t", CS_NULLTERM));
	capture_result(&fr, do_bind, (blkdesc, bi, is_null));
	print_full_result(&fr);
	putchar('\t');
	capture_result(&fr, blk_rowxfer, (blkdesc));
	print_full_result(&fr);
	check_call(blk_done, (blkdesc, CS_BLK_ALL, &count));
        blk_drop(blkdesc);
	if (is_null) {
		fputs("\tNULL ->", stdout);
	} else {
		printf("\t%s ->", bi->var_text);
	}
	if (count) {
		do_query(cmd, "SELECT x FROM #t");
	} else {
		fputs(" N/A", stdout);
	}
	putchar('\n');
	fflush(stdout);
}

static void
test_case(CS_CONNECTION * conn, CS_COMMAND * cmd, const struct bi_input * bi,
	  const char * sql_type, const char * sql_abbrev)
{
	char sql[64];
	snprintf(sql, sizeof(sql), "CREATE TABLE #t (x %s NULL)", sql_type);
	run_command(cmd, sql);
	test_one_case(conn, cmd, bi, sql_abbrev, false);
	run_command(cmd, "TRUNCATE TABLE #t");
	test_one_case(conn, cmd, bi, sql_abbrev, true);
	run_command(cmd, "DROP TABLE #t");
}

static void
routine_tests(CS_CONNECTION * conn, CS_COMMAND * cmd,
              const struct bi_input * bi)
{
	test_case(conn, cmd, bi, "BINARY(1)",     "B(1)");
	test_case(conn, cmd, bi, "VARBINARY(1)",  "VB(1)");
	test_case(conn, cmd, bi, "BINARY(64)",    "B(64)");
	test_case(conn, cmd, bi, "VARBINARY(64)", "VB(64)");
	test_case(conn, cmd, bi, "CHAR(1)",       "C(1)");
	test_case(conn, cmd, bi, "VARCHAR(1)",    "VC(1)");
	test_case(conn, cmd, bi, "CHAR(64)",      "C(64)");
	test_case(conn, cmd, bi, "VARCHAR(64)",   "VC(64)");
}

static void
c2b_tests(CS_CONNECTION * conn, CS_COMMAND * cmd,
	  const struct bi_input * bi_in, const char * sql_type,
	  const char * sql_abbrev)
{
	char sql[64];
	struct bi_input bi;
	char * s;
	char * alt_text;
	CS_SMALLINT l, offset;

	memcpy(&bi, bi_in, sizeof(bi));
	bi.var_addr = malloc(bi_in->host_maxlen);
	memcpy((void *) bi.var_addr, bi_in->var_addr, bi.var_len);
	if (bi.host_type == CS_VARCHAR_TYPE) {
		s = ((CS_VARCHAR *) bi.var_addr)->str;
		offset = sizeof(CS_SMALLINT);
	} else {
		s = (char*) bi.var_addr;
		offset = 0;
	}
	alt_text = malloc(bi_in->host_maxlen - offset);

	snprintf(sql, sizeof(sql), "CREATE TABLE #t (x %s NULL)", sql_type);
	run_command(cmd, sql);
	for (l = 1;  l + offset <= bi_in->var_len;  ++l) {
		memcpy(alt_text, bi_in->var_text, l);
		memcpy(s, bi_in->var_text, l);
		alt_text[l] = s[l] = '\0';
		bi.var_text = alt_text;
		bi.var_len = l + offset;
		if (bi.host_type == CS_VARCHAR_TYPE)
			((CS_VARCHAR *) bi.var_addr)->len = l;
		run_command(cmd, "TRUNCATE TABLE #t");
		test_one_case(conn, cmd, &bi, sql_abbrev, false);
		run_command(cmd, "TRUNCATE TABLE #t");
		s[l >> 1] = '!';
		alt_text[l >> 1] = '!';
		test_one_case(conn, cmd, &bi, sql_abbrev, false);
		if (l > 1) {
			run_command(cmd, "TRUNCATE TABLE #t");
			alt_text[(l >> 1) - 1] = ' ';
			s[(l >> 1) - 1] = '\0';
			test_one_case(conn, cmd, &bi, sql_abbrev, false);
		}
	}
	run_command(cmd, "DROP TABLE #t");
	free(alt_text);
	free((void *) bi.var_addr);
}

static void
anychar_tests(CS_CONNECTION * conn, CS_COMMAND * cmd,
              const struct bi_input * bi)
{
	routine_tests(conn, cmd, bi);
	c2b_tests(conn, cmd, bi, "BINARY(1)",    "B(1)");
	c2b_tests(conn, cmd, bi, "VARBINARY(1)", "VB(1)");
	c2b_tests(conn, cmd, bi, "BINARY(2)",    "B(2)");
	c2b_tests(conn, cmd, bi, "VARBINARY(2)", "VB(2)");
	c2b_tests(conn, cmd, bi, "BINARY(3)",    "B(3)");
	c2b_tests(conn, cmd, bi, "VARBINARY(3)", "VB(3)");
	c2b_tests(conn, cmd, bi, "BINARY(4)",    "B(4)");
	c2b_tests(conn, cmd, bi, "VARBINARY(4)", "VB(4)");
}

static void
char_tests(CS_CONNECTION * conn, CS_COMMAND * cmd, const char * s)
{
	struct bi_input bi =
		{ CS_CHAR_TYPE, CS_NULLTERM, strlen(s) + 1, s, s, strlen(s) };
	anychar_tests(conn, cmd, &bi);
        bi.host_type = CS_LONGCHAR_TYPE;
	anychar_tests(conn, cmd, &bi);
#if 0
        bi.host_type = CS_UNICHAR_TYPE;
	anychar_tests(conn, cmd, &bi);
#endif
}

static void
varchar_tests(CS_CONNECTION * conn, CS_COMMAND * cmd, const CS_VARCHAR * vc)
{
	struct bi_input bi =
		{ CS_VARCHAR_TYPE, CS_UNUSED, vc->len + sizeof(vc->len) + 1,
		  vc->str, vc, vc->len + sizeof(vc->len) };
	anychar_tests(conn, cmd, &bi);
}


static void
b2c_tests(CS_CONNECTION * conn, CS_COMMAND * cmd,
	  const struct bi_input * bi_in, const char * sql_type,
	  const char * sql_abbrev)
{
	char sql[64];
	struct bi_input bi;
	char * alt_text;
	CS_SMALLINT l, offset;

	memcpy(&bi, bi_in, sizeof(bi));
	bi.var_addr = malloc(bi.var_len);
	memcpy((void *) bi.var_addr, bi_in->var_addr, bi.var_len);
	if (bi.host_type == CS_VARBINARY_TYPE) {
		offset = sizeof(CS_SMALLINT);
	} else {
		offset = 0;
	}
	alt_text = malloc((bi_in->host_maxlen - offset) * 3);
	bi.var_text = alt_text;

	snprintf(sql, sizeof(sql), "CREATE TABLE #t (x %s NULL)", sql_type);
	run_command(cmd, sql);
	for (l = 1 + offset;  l <= bi_in->var_len;  ++l) {
		memcpy(alt_text, bi_in->var_text, (l - offset) * 3 - 1);
		alt_text[(l - offset) * 3 - 1] = '\0';
		bi.var_len = l;
		if (bi.host_type == CS_VARBINARY_TYPE)
			((CS_VARBINARY *) bi.var_addr)->len = l - offset;
		run_command(cmd, "TRUNCATE TABLE #t");
		test_one_case(conn, cmd, &bi, sql_abbrev, false);
	}
	run_command(cmd, "DROP TABLE #t");
	free(alt_text);
	free((void *) bi.var_addr);
}

static void
anybin_tests(CS_CONNECTION * conn, CS_COMMAND * cmd, struct bi_input * bi_in)
{
	struct bi_input bi;
	routine_tests(conn, cmd, bi_in);
	memcpy(&bi, bi_in, sizeof(bi));
	b2c_tests(conn, cmd, bi_in, "CHAR(2)",    "C(2)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(2)", "VC(2)");
	b2c_tests(conn, cmd, bi_in, "CHAR(3)",    "C(3)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(3)", "VC(3)");
	b2c_tests(conn, cmd, bi_in, "CHAR(4)",    "C(4)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(4)", "VC(4)");
	b2c_tests(conn, cmd, bi_in, "CHAR(5)",    "C(5)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(5)", "VC(5)");
	b2c_tests(conn, cmd, bi_in, "CHAR(6)",    "C(6)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(6)", "VC(6)");
	b2c_tests(conn, cmd, bi_in, "CHAR(7)",    "C(7)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(7)", "VC(7)");
	b2c_tests(conn, cmd, bi_in, "CHAR(8)",    "C(8)");
	b2c_tests(conn, cmd, bi_in, "VARCHAR(8)", "VC(8)");
}

static void
bin_tests(CS_CONNECTION * conn, CS_COMMAND * cmd, const void * p, CS_INT l)
{
	char * s = bin_str(p, l);
	struct bi_input bi = { CS_BINARY_TYPE, CS_UNUSED, l, s, p, l };
	anybin_tests(conn, cmd, &bi);
        bi.host_type = CS_LONGBINARY_TYPE;
	anybin_tests(conn, cmd, &bi);
	free(s);
}

static void
varbin_tests(CS_CONNECTION * conn, CS_COMMAND * cmd, const CS_VARBINARY * vb)
{
	char * s = bin_str(vb->array, vb->len);
	CS_SMALLINT tl = vb->len + sizeof(vb->len);
	struct bi_input bi = { CS_VARBINARY_TYPE, CS_UNUSED, tl, s, vb, tl };
	anybin_tests(conn, cmd, &bi);
        free(s);
}

#define PRIMITIVE_TESTS_EX(tag, value, s) \
	do { \
		CS_##tag _value = (value); \
		struct bi_input bi = \
			{ CS_##tag##_TYPE, CS_UNUSED, sizeof(CS_##tag), \
			  s, &_value, sizeof(CS_##tag) }; \
		routine_tests(conn, cmd, &bi); \
	} while (0)

#define PRIMITIVE_TESTS(tag, value) PRIMITIVE_TESTS_EX(tag, value, #value)

#define COMPOUND_TESTS(tag, s, ...) \
	do { \
		CS_##tag _value = { __VA_ARGS__ }; \
		struct bi_input bi = \
			{ CS_##tag##_TYPE, CS_UNUSED, sizeof(CS_##tag), \
			  s, &_value, sizeof(CS_##tag) }; \
		routine_tests(conn, cmd, &bi); \
	} while (0)

int
main(int argc TDS_UNUSED, char** argv TDS_UNUSED)
{
	CS_CONTEXT *ctx;
	CS_CONNECTION *conn;
	CS_COMMAND *cmd;
	int i;

	check_call(try_ctlogin, (&ctx, &conn, &cmd, false));
	check_call(cs_config, (ctx, CS_SET, CS_MESSAGE_CB,
                               (CS_VOID*) record_cslibmsg, CS_UNUSED, NULL));
	check_call(ct_callback, (ctx, NULL, CS_SET, CS_CLIENTMSG_CB,
				 (CS_VOID*) record_ctlibmsg));
	/* Compensate for Sybase ct_callback semantics */
	check_call(ct_callback, (NULL, conn, CS_SET, CS_CLIENTMSG_CB,
				 (CS_VOID*) record_ctlibmsg));

	printf("# FROM\tTO\tBIND\tROWXFER\tVALUES\n");
	char_tests(conn, cmd, "abcde12345");
	char_tests(conn, cmd, "0x123456789a");
	{
		CS_VARCHAR vc = { 10, "abcde12345" };
		varchar_tests(conn, cmd, &vc);
	}
	{
		CS_VARCHAR vc = { 12, "0x123456789a" };
		varchar_tests(conn, cmd, &vc);
	}
	{
		CS_BYTE bin[] = { 0x34, 0x56, 0x78 };
		bin_tests(conn, cmd, bin, sizeof(bin));
	}
	{
		CS_VARBINARY vb = { 3, { 0x34, 0x56, 0x78 } };
		varbin_tests(conn, cmd, &vb);
	}
	PRIMITIVE_TESTS(BIT, true);
	COMPOUND_TESTS(DATETIME,  "2003-12-17T15:44", 37970, 944 * 18000);
	COMPOUND_TESTS(DATETIME4, "2003-12-17T15:44", 37970, 944);
	COMPOUND_TESTS(MONEY,  "$12.34", 0, 123400);
	COMPOUND_TESTS(MONEY4, "$12.34", 123400);
	PRIMITIVE_TESTS(FLOAT, 12.34);
	PRIMITIVE_TESTS(REAL, 12.34f);
	COMPOUND_TESTS(DECIMAL, "12.34", 4, 2, { 0, 1234 / 256, 1234 % 256 });
	COMPOUND_TESTS(NUMERIC, "12.34", 4, 2, { 0, 1234 / 256, 1234 % 256 });
	PRIMITIVE_TESTS(INT, 1234);
	PRIMITIVE_TESTS(SMALLINT, 1234);
	PRIMITIVE_TESTS(TINYINT, 123);
        PRIMITIVE_TESTS(LONG, 1234);
        PRIMITIVE_TESTS_EX(DATE, 37970, "2003-12-17");
        PRIMITIVE_TESTS_EX(TIME, 944 * 18000, "15:44");
#ifdef CS_BIGINT_TYPE
	PRIMITIVE_TESTS(BIGINT, 1234);
#endif
        PRIMITIVE_TESTS(USHORT, 1234);
#ifdef CS_UINT_TYPE
        PRIMITIVE_TESTS(USMALLINT, 123);
        PRIMITIVE_TESTS(UINT, 1234);
        PRIMITIVE_TESTS(UBIGINT, 1234);
#endif
#ifdef CS_BIGDATETIME_TYPE
        PRIMITIVE_TESTS_EX(BIGDATETIME,
                           ((693961 + 37970) * 86400UL + 944 * 60) * 1000000,
                           "2003-12-17T15:44");
        PRIMITIVE_TESTS_EX(BIGTIME, 944 * 60000000UL, "15:44");
#endif

	check_call(try_ctlogout, (ctx, conn, cmd, false));

	printf("\n");
	for (i = 0;
	     i < sizeof(error_descriptions) / sizeof(*error_descriptions);
	     ++i) {
		if (error_descriptions[i]) {
			printf("%d:%d: %s\n",
			       (i >> 8) + 1, i & 255, error_descriptions[i]);
		}
	}

	return 0;
}
