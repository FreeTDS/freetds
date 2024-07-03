/*
 * Purpose: Test bcp truncation
 * Functions: bcp_batch bcp_bind bcp_done bcp_init bcp_sendrow
 */

#include "common.h"

#include <freetds/bool.h>

static DBINT last_error;
static const char * error_descriptions[32768];

struct full_result
{
	RETCODE rc;
	DBINT   error; // zero if none
};

struct bi_input
{
	int          type;
	BYTE *       terminator;
	int          termlen;
	const char * var_text;
	const BYTE * var_addr;
	DBINT        var_len;
};


static int
record_msg(DBPROCESS * dbproc TDS_UNUSED, DBINT msgno,
	   int msgstate TDS_UNUSED, int severity TDS_UNUSED,
	   char *msgtext, char *srvname TDS_UNUSED, char *procname TDS_UNUSED,
	   int line TDS_UNUSED)
{
	last_error = msgno;
	if (error_descriptions[msgno] == NULL) {
		error_descriptions[msgno] = strdup(msgtext);
	}
	return 0;
}

static int
record_err(DBPROCESS * dbproc TDS_UNUSED, int severity TDS_UNUSED,
	   int dberr, int oserr TDS_UNUSED, char *dberrstr,
	   char *oserrstr TDS_UNUSED)
{
	last_error = dberr;
	if (error_descriptions[dberr] == NULL) {
		error_descriptions[dberr] = strdup(dberrstr);
	}
	return INT_CANCEL;
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
	case SUCCEED:  fputs(" +", stdout); break;
	case FAIL:     fputs("- ", stdout); break;
	case BUF_FULL: fputs("-B", stdout); break;
	default:       fputs("-?", stdout); break;
	}
	if (fr->error) {
		printf("%d", fr->error);
	}
}


static RETCODE
do_bind(DBPROCESS * dbproc, struct bi_input * bi, bool is_null)
{
	return bcp_bind(dbproc, (BYTE *) bi->var_addr, 0 /* prefixlen */,
			is_null ? 0 : bi->var_len, bi->terminator,
			bi->termlen, bi->type, 1 /* colnum */);
}


static char *
bin_str(const void * p, DBINT l)
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


static void
do_fetch(DBPROCESS * dbproc, int type)
{
	BYTE buffer[256];
	RETCODE rc;

	rc = dbbind(dbproc, 1, type, sizeof(buffer), buffer);
	if (rc != SUCCEED) {
		fputs(" [FAIL]", stdout);
		return;
	}
	while ((rc = dbnextrow(dbproc)) == REG_ROW) {
		int len = dbdatlen(dbproc, 1);
		if (type == CHARBIND) {
			printf(" '%.*s'", len, buffer);
		} else if (len > 0) {
			char * s = bin_str(buffer, len);
			printf(" %s", s);
			free(s);
		}
	}
	if (rc != NO_MORE_ROWS)
		fputs(" [FAIL]", stdout);
}

static void
do_query(DBPROCESS * dbproc, const char * query, int type)
{
	dbcmd(dbproc, query);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		do_fetch(dbproc, type);
	}
}

static const char *
host_type_name(int host_type)
{
	switch (host_type) {
	case SYBCHAR:             return "CHAR";
	case SYBVARCHAR:          return "VARCHAR";
	case SYBINT1:             return "INT1";
	case SYBINT2:             return "INT2";
	case SYBINT4:             return "INT4";
#ifdef SYBINT8
	case SYBINT8:             return "INT8";
#endif
	case SYBFLT8:             return "FLT8";
	case SYBDATETIME:         return "DT";
	case SYBBIT:              return "BIT";
	case SYBTEXT:             return "TEXT";
#ifdef SYBNTEXT
	case SYBNTEXT:            return "NTEXT";
#endif
	case SYBIMAGE:            return "IMAGE";
	case SYBMONEY4:           return "MONEY4";
	case SYBMONEY:            return "MONEY";
	case SYBDATETIME4:        return "DT4";
	case SYBREAL:             return "REAL";
	case SYBBINARY:           return "BINARY";
	case SYBVOID:             return "VOID";
	case SYBVARBINARY:        return "VARBIN";
	case SYBNUMERIC:          return "NUMERIC";
	case SYBDECIMAL:          return "DECIMAL";
#ifdef SYBNVARCHAR
	case SYBNVARCHAR:         return "NVRCHAR";
#endif
#ifdef SYBDATE
	case SYBDATE:             return "DATE";
	case SYBTIME:             return "TIME";
	case SYBBIGDATETIME:      return "BIGDT";
	case SYBBIGTIME:          return "BIGTIME";
	case SYBMSDATE:           return "MSDATE";
	case SYBMSTIME:           return "MSTIME";
	case SYBMSDATETIME2:      return "MSDT2";
	case SYBMSDATETIMEOFFSET: return "MSDTO";
#endif
	}
	return "???";
}

static void
test_one_case(DBPROCESS * dbproc, struct bi_input * bi,
	      const char *sql_abbrev, int result_type, bool is_null)
{
	struct full_result fr;
	DBINT count;

	printf("%s\t%s\t", host_type_name(bi->type), sql_abbrev);
	bcp_init(dbproc, "#t", NULL, NULL, DB_IN);
	capture_result(&fr, do_bind, (dbproc, bi, is_null));
	print_full_result(&fr);
	putchar('\t');
	capture_result(&fr, bcp_sendrow, (dbproc));
	print_full_result(&fr);
	count = bcp_batch(dbproc);
	bcp_done(dbproc);
	if (is_null) {
		fputs("\tNULL ->", stdout);
	} else {
		printf("\t%s ->", bi->var_text);
	}
	if (count) {
		do_query(dbproc, "SELECT x FROM #t", result_type);
	} else {
		fputs(" N/A", stdout);
	}
	putchar('\n');
	fflush(stdout);
}

static void
run_command(DBPROCESS * dbproc, const char * sql)
{
	dbcmd(dbproc, sql);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

}

static void
test_case(DBPROCESS * dbproc, struct bi_input * bi, const char * sql_type,
	  const char * sql_abbrev, int result_type)
{
	char sql[64];
	snprintf(sql, sizeof(sql), "CREATE TABLE #t (x %s NULL)", sql_type);
	run_command(dbproc, sql);
	test_one_case(dbproc, bi, sql_abbrev, result_type, false);
	run_command(dbproc, "TRUNCATE TABLE #t");
	test_one_case(dbproc, bi, sql_abbrev, result_type, true);
	run_command(dbproc, "DROP TABLE #t");
}

static void
routine_tests(DBPROCESS * dbproc, struct bi_input * bi)
{
	test_case(dbproc, bi, "BINARY(1)",     "B(1)",   BINARYBIND);
	test_case(dbproc, bi, "VARBINARY(1)",  "VB(1)",  BINARYBIND);
#ifndef TDS_STATIC_CAST /* avoid crashes with commercial libraries */
	if (bi->type != SYBDECIMAL  &&  bi->type != SYBNUMERIC)
#endif
	{
		test_case(dbproc, bi, "BINARY(64)",    "B(64)",  BINARYBIND);
		test_case(dbproc, bi, "VARBINARY(64)", "VB(64)", BINARYBIND);
	}
	test_case(dbproc, bi, "CHAR(1)",       "C(1)",   CHARBIND);
	test_case(dbproc, bi, "VARCHAR(1)",    "VC(1)",  CHARBIND);
#ifndef TDS_STATIC_CAST
	if (bi->type != SYBDECIMAL  &&  bi->type != SYBNUMERIC)
#endif
	{
		test_case(dbproc, bi, "CHAR(64)",      "C(64)",  CHARBIND);
		test_case(dbproc, bi, "VARCHAR(64)",   "VC(64)", CHARBIND);
	}
}


static void
c2b_tests(DBPROCESS * dbproc, const struct bi_input * bi_in,
	  const char * sql_type, const char * sql_abbrev)
{
	char sql[64];
	struct bi_input bi;
	char * alt_text;
	int l;

	memcpy(&bi, bi_in, sizeof(bi));
	bi.var_addr = malloc(bi_in->var_len + 1);
	memcpy((void *) bi.var_addr, bi_in->var_addr, bi.var_len);
	alt_text = malloc(bi_in->var_len + 1);

	snprintf(sql, sizeof(sql), "CREATE TABLE #t (x %s NULL)", sql_type);
	run_command(dbproc, sql);
	for (l = 1;  l <= bi_in->var_len;  ++l) {
		char * s = (char *) bi.var_addr;
		memcpy(alt_text, bi_in->var_text, l);
		memcpy(s, bi_in->var_text, l);
		alt_text[l] = s[l] = '\0';
		bi.var_text = alt_text;
		bi.var_len = l;
		run_command(dbproc, "TRUNCATE TABLE #t");
		test_one_case(dbproc, &bi, sql_abbrev, BINARYBIND, false);
		run_command(dbproc, "TRUNCATE TABLE #t");
		s[l >> 1] = '!';
		alt_text[l >> 1] = '!';
		test_one_case(dbproc, &bi, sql_abbrev, BINARYBIND, false);
		if (l > 1) {
			run_command(dbproc, "TRUNCATE TABLE #t");
			alt_text[(l >> 1) - 1] = ' ';
			s[(l >> 1) - 1] = '\0';
			test_one_case(dbproc, &bi, sql_abbrev, BINARYBIND,
				      false);
		}
	}
	run_command(dbproc, "DROP TABLE #t");
	free(alt_text);
	free((void *) bi.var_addr);
}

static void
char_tests(DBPROCESS * dbproc, const char * s)
{
	struct bi_input bi = { SYBCHAR, (BYTE *) "", 1, s, (const BYTE *) s,
		               strlen(s) };
	routine_tests(dbproc, &bi);
	c2b_tests(dbproc, &bi, "BINARY(1)",    "B(1)");
	c2b_tests(dbproc, &bi, "VARBINARY(1)", "VB(1)");
	c2b_tests(dbproc, &bi, "BINARY(2)",    "B(2)");
	c2b_tests(dbproc, &bi, "VARBINARY(2)", "VB(2)");
	c2b_tests(dbproc, &bi, "BINARY(3)",    "B(3)");
	c2b_tests(dbproc, &bi, "VARBINARY(3)", "VB(3)");
	c2b_tests(dbproc, &bi, "BINARY(4)",    "B(4)");
	c2b_tests(dbproc, &bi, "VARBINARY(4)", "VB(4)");
}


static void
b2c_tests(DBPROCESS * dbproc, const struct bi_input * bi_in,
	  const char * sql_type, const char * sql_abbrev)
{
	char sql[64];
	struct bi_input bi;
	char * alt_text;
	int l;

	memcpy(&bi, bi_in, sizeof(bi));
	bi.var_addr = malloc(bi.var_len);
	memcpy((void *) bi.var_addr, bi_in->var_addr, bi.var_len);
	alt_text = malloc(bi_in->var_len * 3);
	bi.var_text = alt_text;

	snprintf(sql, sizeof(sql), "CREATE TABLE #t (x %s NULL)", sql_type);
	run_command(dbproc, sql);
	for (l = 1;  l <= bi_in->var_len;  ++l) {
		memcpy(alt_text, bi_in->var_text, l * 3 - 1);
		alt_text[l * 3 - 1] = '\0';
		bi.var_len = l;
		run_command(dbproc, "TRUNCATE TABLE #t");
		test_one_case(dbproc, &bi, sql_abbrev, CHARBIND, false);
	}
	run_command(dbproc, "DROP TABLE #t");
	free(alt_text);
	free((void *) bi.var_addr);
}

static void
bin_tests(DBPROCESS * dbproc, const void * p, DBINT l)
{
	char * s = bin_str(p, l);
	struct bi_input bi = { SYBBINARY, NULL, 0, s, p, l };
	routine_tests(dbproc, &bi);
	b2c_tests(dbproc, &bi, "CHAR(2)",    "C(2)");
	b2c_tests(dbproc, &bi, "VARCHAR(2)", "VC(2)");
	b2c_tests(dbproc, &bi, "CHAR(3)",    "C(3)");
	b2c_tests(dbproc, &bi, "VARCHAR(3)", "VC(3)");
	b2c_tests(dbproc, &bi, "CHAR(4)",    "C(4)");
	b2c_tests(dbproc, &bi, "VARCHAR(4)", "VC(4)");
	b2c_tests(dbproc, &bi, "CHAR(5)",    "C(5)");
	b2c_tests(dbproc, &bi, "VARCHAR(5)", "VC(5)");
	b2c_tests(dbproc, &bi, "CHAR(6)",    "C(6)");
	b2c_tests(dbproc, &bi, "VARCHAR(6)", "VC(6)");
	b2c_tests(dbproc, &bi, "CHAR(7)",    "C(7)");
	b2c_tests(dbproc, &bi, "VARCHAR(7)", "VC(7)");
	b2c_tests(dbproc, &bi, "CHAR(8)",    "C(8)");
	b2c_tests(dbproc, &bi, "VARCHAR(8)", "VC(8)");
	free(s);
}

#define PRIMITIVE_TESTS_EX(tag, type, value, s) \
	do { \
		DB##type _value = (value); \
		struct bi_input bi = \
			{ SYB##tag, NULL, 0, s, (const BYTE *) &_value, -1 }; \
		routine_tests(dbproc, &bi); \
	} while (0)

#define PRIMITIVE_TESTS(tag, type, value) \
	PRIMITIVE_TESTS_EX(tag, type, value, #value)

#define COMPOUND_TESTS(tag, s, ...) \
	do { \
		DB##tag _value = { __VA_ARGS__ }; \
		struct bi_input bi = \
			{ SYB##tag, NULL, 0, s, (const BYTE *) &_value, -1 }; \
		routine_tests(dbproc, &bi); \
	} while (0)

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbsetversion(DBVERSION_100);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "bcp3.c unit test");
	BCP_SETL(login, 1);

	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	dberrhandle(record_err);
	dbmsghandle(record_msg);

	printf("# FROM\tTO\tBIND\tROWXFER\tVALUES\n");
	char_tests(dbproc, "abcde12345");
	char_tests(dbproc, "0x123456789a");
	{
		BYTE bin[] = { 0x34, 0x56, 0x78 };
		bin_tests(dbproc, bin, sizeof(bin));
	}
	PRIMITIVE_TESTS(BIT, BIT, true);
	COMPOUND_TESTS(DATETIME,  "2003-12-17T15:44", 37970, 944 * 18000);
	COMPOUND_TESTS(DATETIME4, "2003-12-17T15:44", 37970, 944);
	COMPOUND_TESTS(MONEY,  "$12.34", 0, 123400);
	COMPOUND_TESTS(MONEY4, "$12.34", 123400);
	PRIMITIVE_TESTS(FLT8, FLT8, 12.34);
	PRIMITIVE_TESTS(REAL, REAL, 12.34f);
	COMPOUND_TESTS(DECIMAL, "12.34", 4, 2, { 0, 1234 / 256, 1234 % 256 });
	COMPOUND_TESTS(NUMERIC, "12.34", 4, 2, { 0, 1234 / 256, 1234 % 256 });
	PRIMITIVE_TESTS(INT4, INT, 1234);
	PRIMITIVE_TESTS(INT2, SMALLINT, 1234);
	PRIMITIVE_TESTS(INT1, TINYINT, 123);
#ifdef SYBDATE
        PRIMITIVE_TESTS_EX(DATE, INT, 37970, "2003-12-17");
        PRIMITIVE_TESTS_EX(TIME, INT, 944 * 18000, "15:44");
#endif
#ifdef SYBINT8
	PRIMITIVE_TESTS(INT8, BIGINT, 1234);
#endif
#ifdef SYBBIGDATETIME
        PRIMITIVE_TESTS_EX(BIGDATETIME, BIGINT,
                           ((693961 + 37970) * 86400UL + 944 * 60) * 1000000,
                           "2003-12-17T15:44");
        PRIMITIVE_TESTS_EX(BIGTIME, BIGINT, 944 * 60000000UL, "15:44");
#endif

	dbexit();

	printf("\n");
	for (i = 0;
	     i < sizeof(error_descriptions) / sizeof(*error_descriptions);
	     ++i) {
		if (error_descriptions[i]) {
			printf("%d: %s\n", i, error_descriptions[i]);
		}
	}

	return 0;
}
