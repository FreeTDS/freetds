#include "common.h"
#define TDSODBC_BCP
#include <odbcss.h>
#include <assert.h>

#ifdef UNICODE
typedef SQLWCHAR bcp_init_char_t;
#else
typedef char bcp_init_char_t;
#endif

struct prefixed_int {
	ODBCINT64 prefix;
	int value;
};
struct prefixed_str {
	ODBCINT64 prefix;
	char value[64];
};

/*
 * Static data for insertion
 */
static struct prefixed_int not_null_bit           = {4, 1};
static struct prefixed_str not_null_char          = {64, "a char"};
static struct prefixed_str not_null_varchar       = {64, "a varchar"};
static struct prefixed_str not_null_datetime      = {64, "2003-12-17 15:44:00.000"};
static struct prefixed_str not_null_smalldatetime = {64, "2003-12-17 15:44:00"};
static struct prefixed_str not_null_money         = {64, "12.34"};
static struct prefixed_str not_null_smallmoney    = {64, "12.34"};
static struct prefixed_str not_null_float         = {64, "12.34"};
static struct prefixed_str not_null_real          = {64, "12.34"};
static struct prefixed_str not_null_decimal       = {64, "12.34"};
static struct prefixed_str not_null_numeric       = {64, "12.34"};
static struct prefixed_int not_null_int           = {4, 1234};
static struct prefixed_int not_null_smallint      = {4, 1234};
static struct prefixed_int not_null_tinyint       = {4, 123};
static struct prefixed_str not_null_nvarchar      = {64, "a wide var"};
static ODBCINT64 null_prefix = -1;

static const char *expected[] = {
	"1",
	"a char    ","a varchar","2003-12-17 15:44:00.000","2003-12-17 15:44:00",
	"12.34","12.34","12.34","12.3400002","12.34","12.34",
	"1234","1234","123",
	"a wide var",
};
static const int total_cols = 29;

static const char *expected_special[] = {
	"2015-03-14 15:26:53.000",
	"2015-03-14 15:26:53.589793",
	"3.141593000",
	"3.141593",		/* MS driver has "3141593" here. Bug? Should we be bug-compatible? */
	"",
};

static int tds_version;

static void
init(void)
{
	odbc_command("if exists (select 1 from sysobjects where type = 'U' and name = 'all_types_bcp_unittest') drop table all_types_bcp_unittest");
	odbc_command("if exists (select 1 from sysobjects where type = 'U' and name = 'special_types_bcp_unittest') drop table special_types_bcp_unittest");

	odbc_command("CREATE TABLE all_types_bcp_unittest ("
		"  not_null_bit                  bit NOT NULL"
		""
		", not_null_char                 char(10) NOT NULL"
		", not_null_varchar              varchar(10) NOT NULL"
		""
		", not_null_datetime             datetime NOT NULL"
		", not_null_smalldatetime        smalldatetime NOT NULL"
		""
		", not_null_money                money NOT NULL"
		", not_null_smallmoney           smallmoney NOT NULL"
		""
		", not_null_float                float NOT NULL"
		", not_null_real                 real NOT NULL"
		""
		", not_null_decimal              decimal(5,2) NOT NULL"
		", not_null_numeric              numeric(5,2) NOT NULL"
		""
		", not_null_int                  int NOT NULL"
		", not_null_smallint             smallint NOT NULL"
		", not_null_tinyint              tinyint NOT NULL"
		", not_null_nvarchar             nvarchar(10) NOT NULL"
		""
		", nullable_char                 char(10)  NULL"
		", nullable_varchar              varchar(10)  NULL"
		""
		", nullable_datetime             datetime  NULL"
		", nullable_smalldatetime        smalldatetime  NULL"
		""
		", nullable_money                money  NULL"
		", nullable_smallmoney           smallmoney  NULL"
		""
		", nullable_float                float  NULL"
		", nullable_real                 real  NULL"
		""
		", nullable_decimal              decimal(5,2)  NULL"
		", nullable_numeric              numeric(5,2)  NULL"
		""
		", nullable_int                  int  NULL"
		", nullable_smallint             smallint  NULL"
		", nullable_tinyint              tinyint  NULL"
		", nullable_nvarchar             nvarchar(10)  NULL"
		")");

	if (tds_version < 0x703)
		return;

		/* Excludes:
		 * binary
		 * image
		 * uniqueidentifier
		 * varbinary
		 * text
		 * timestamp
		 * nchar
		 * ntext
		 * nvarchar
		 */
	odbc_command("CREATE TABLE special_types_bcp_unittest ("
		"dt datetime not null,"
		"dt2 datetime2(6) not null,"
		"num decimal(19,9) not null,"
		"numstr varchar(64) not null,"
		"empty varchar(64) not null,"
		"bitnull bit null"
		")");
}

#define VARCHAR_BIND(x) \
	bcp_bind( odbc_conn, (unsigned char *) (prefixlen == 0 ? (void*)&x.value : &x), prefixlen, strlen(x.value), NULL, termlen, BCP_TYPE_SQLVARCHAR, col++ )

#define INT_BIND(x) \
	bcp_bind( odbc_conn, (unsigned char *) (prefixlen == 0 ? (void*)&x.value : &x), prefixlen, SQL_VARLEN_DATA, NULL, termlen, BCP_TYPE_SQLINT4,    col++ )

#define NULL_BIND(x, type) \
	bcp_bind( odbc_conn, (unsigned char *) (prefixlen == 0 ? (void*)&x.value : &null_prefix), prefixlen, prefixlen == 0 ? SQL_NULL_DATA : SQL_VARLEN_DATA, NULL, termlen, type,    col++ )

static void
test_bind(int prefixlen)
{
	enum { termlen = 0 };

	RETCODE fOK;
	int col=1;

	/* non nulls */
	fOK = INT_BIND(not_null_bit);
	assert(fOK == SUCCEED);

	fOK = VARCHAR_BIND(not_null_char);
	assert(fOK == SUCCEED);
	fOK = VARCHAR_BIND(not_null_varchar);
	assert(fOK == SUCCEED);

	fOK = VARCHAR_BIND(not_null_datetime);
	assert(fOK == SUCCEED);
	fOK = VARCHAR_BIND(not_null_smalldatetime);
	assert(fOK == SUCCEED);

	fOK = VARCHAR_BIND(not_null_money);
	assert(fOK == SUCCEED);
	fOK = VARCHAR_BIND(not_null_smallmoney);
	assert(fOK == SUCCEED);

	fOK = VARCHAR_BIND(not_null_float);
	assert(fOK == SUCCEED);
	fOK = VARCHAR_BIND(not_null_real);
	assert(fOK == SUCCEED);

	fOK = VARCHAR_BIND(not_null_decimal);
	assert(fOK == SUCCEED);
	fOK = VARCHAR_BIND(not_null_numeric);
	assert(fOK == SUCCEED);

	fOK = INT_BIND(not_null_int);
	assert(fOK == SUCCEED);
	fOK = INT_BIND(not_null_smallint);
	assert(fOK == SUCCEED);
	fOK = INT_BIND(not_null_tinyint);
	assert(fOK == SUCCEED);
	fOK = VARCHAR_BIND(not_null_nvarchar);
	assert(fOK == SUCCEED);

	/* nulls */
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_char, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_varchar, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);

	fOK = NULL_BIND(not_null_datetime, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_smalldatetime, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);

	fOK = NULL_BIND(not_null_money, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_smallmoney, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);

	fOK = NULL_BIND(not_null_float, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_real, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);

	fOK = NULL_BIND(not_null_decimal, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_numeric, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);

	fOK = NULL_BIND(not_null_int, BCP_TYPE_SQLINT4);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_smallint, BCP_TYPE_SQLINT4);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_tinyint, BCP_TYPE_SQLINT4);
	assert(fOK == SUCCEED);
	fOK = NULL_BIND(not_null_nvarchar, BCP_TYPE_SQLVARCHAR);
	assert(fOK == SUCCEED);

}

static void
set_attr(void)
{
	SQLSetConnectAttr(odbc_conn, SQL_COPT_SS_BCP, (SQLPOINTER)SQL_BCP_ON, 0);
}

static void
report_bcp_error(const char *errmsg, int line, const char *file)
{
	odbc_stmt = NULL;
	odbc_report_error(errmsg, line, file);
}

static void normal_inserts(int prefixlen);
static void normal_select(void);
static void special_inserts(void);
static void special_select(void);

static const char table_name[] = "all_types_bcp_unittest";

int
main(int argc, char *argv[])
{
	const char *s;

	odbc_set_conn_attr = set_attr;
	odbc_connect();

	tds_version = odbc_tds_version();

	init();

	normal_inserts(0);
	if (tds_version >= 0x703)
		special_inserts();
	normal_select();
	if (tds_version >= 0x703)
		special_select();

	odbc_command("delete from all_types_bcp_unittest");
	normal_inserts(8);
	normal_select();

	if ((s = getenv("BCP")) != NULL && 0 == strcmp(s, "nodrop")) {
		fprintf(stdout, "BCP=nodrop: '%s' kept\n", table_name);
	} else {
		fprintf(stdout, "Dropping table %s\n", table_name);
		odbc_command("drop table all_types_bcp_unittest");
		if (tds_version >= 0x703)
			odbc_command("drop table special_types_bcp_unittest");
	}

	odbc_disconnect();

	printf("Done.\n");
	return 0;
}

static void normal_inserts(int prefixlen)
{
	int i;
	int rows_sent;

	/* set up and send the bcp */
	fprintf(stdout, "preparing to insert into %s ... ", table_name);
	if (bcp_init(odbc_conn, (bcp_init_char_t *) T(table_name), NULL, NULL, BCP_DIRECTION_IN) == FAIL)
		report_bcp_error("bcp_init", __LINE__, __FILE__);
	fprintf(stdout, "OK\n");

	test_bind(prefixlen);

	fprintf(stdout, "Sending same row 10 times... \n");
	for (i=0; i<10; i++)
		if (bcp_sendrow(odbc_conn) == FAIL)
			report_bcp_error("bcp_sendrow", __LINE__, __FILE__);

#if 1
	rows_sent = bcp_batch(odbc_conn);
	if (rows_sent == -1)
		report_bcp_error("bcp_batch", __LINE__, __FILE__);
#endif

	printf("OK\n");

	/* end bcp.  */
	rows_sent = bcp_done(odbc_conn);
	if (rows_sent != 0)
		report_bcp_error("bcp_done", __LINE__, __FILE__);
	else
		printf("%d rows copied.\n", rows_sent);

	printf("done\n");
}

static void special_inserts(void)
{
	int rows_sent;
	SQL_TIMESTAMP_STRUCT timestamp;
	DBDATETIME datetime;
	SQL_NUMERIC_STRUCT numeric;

	printf("sending special types\n");
	rows_sent = 0;

	if (bcp_init(odbc_conn, (bcp_init_char_t *) T("special_types_bcp_unittest"), NULL, NULL, BCP_DIRECTION_IN) == FAIL)
		report_bcp_error("bcp_init", __LINE__, __FILE__);
	printf("OK\n");

	datetime.dtdays = 42075;
	datetime.dttime = 16683900;
	timestamp.year = 2015;
	timestamp.month = 3;
	timestamp.day = 14;
	timestamp.hour = 15;
	timestamp.minute = 26;
	timestamp.second = 53;
	timestamp.fraction = 589793238;
	memset(&numeric, 0, sizeof(numeric));
	numeric.precision = 19;
	numeric.scale = 6;
	numeric.sign = 1;
	numeric.val[0] = 0xd9;
	numeric.val[1] = 0xef;
	numeric.val[2] = 0x2f;
	bcp_bind(odbc_conn, (unsigned char *) &datetime, 0, sizeof(datetime), NULL, 0, BCP_TYPE_SQLDATETIME, 1);
	bcp_bind(odbc_conn, (unsigned char *) &timestamp, 0, sizeof(timestamp), NULL, 0, BCP_TYPE_SQLDATETIME2, 2);
	bcp_bind(odbc_conn, (unsigned char *) &numeric, 0, sizeof(numeric), NULL, 0, BCP_TYPE_SQLDECIMAL, 3);
	bcp_bind(odbc_conn, (unsigned char *) &numeric, 0, sizeof(numeric), NULL, 0, BCP_TYPE_SQLDECIMAL, 4);
	bcp_bind(odbc_conn, (unsigned char *) "", 0, 0, NULL, 0, BCP_TYPE_SQLVARCHAR, 5);
	bcp_bind(odbc_conn, (unsigned char *) &not_null_bit, 0, SQL_NULL_DATA, NULL, 0, BCP_TYPE_SQLINT4, 6);

	if (bcp_sendrow(odbc_conn) == FAIL)
		report_bcp_error("bcp_sendrow", __LINE__, __FILE__);

	rows_sent = bcp_batch(odbc_conn);
	if (rows_sent != 1)
		report_bcp_error("bcp_batch", __LINE__, __FILE__);

	printf("OK\n");

	/* end bcp.  */

	rows_sent = bcp_done(odbc_conn);
	if (rows_sent != 0)
		report_bcp_error("bcp_done", __LINE__, __FILE__);
	else
		printf("%d rows copied.\n", rows_sent);

	printf("done\n");
}

static void normal_select(void)
{
	int ok = 1, i;

	odbc_command("select * from all_types_bcp_unittest");
	CHKFetch("SI");

	/* first columns have values */
	for (i = 0; i < sizeof(expected)/sizeof(expected[0]); ++i) {
		char output[128];
		SQLLEN dataSize;
		CHKGetData(i + 1, SQL_C_CHAR, output, sizeof(output), &dataSize, "S");
		if (strcmp(output, expected[i]) || dataSize <= 0) {
			fprintf(stderr, "Invalid returned col %d: '%s'!='%s'\n", i, expected[i], output);
			ok = 0;
		}
	}
	/* others are NULL */
	for (; i < total_cols; ++i) {
		char output[128];
		SQLLEN dataSize;
		CHKGetData(i + 1, SQL_C_CHAR, output, sizeof(output), &dataSize, "S");
		if (dataSize != SQL_NULL_DATA) {
			fprintf(stderr, "Invalid returned col %d: should be NULL'\n", i);
			ok = 0;
		}
	}
	if (!ok)
		exit(1);
	CHKCloseCursor("SI");
}

static void special_select(void)
{
	int ok = 1, i;

	odbc_command("select top 1 * from special_types_bcp_unittest");
	CHKFetch("SI");
	for (i = 0; i < sizeof(expected_special)/sizeof(expected_special[0]); ++i) {
		char output[128];
		SQLLEN dataSize;
		CHKGetData(i + 1, SQL_C_CHAR, output, sizeof(output), &dataSize, "S");
		if (strcmp(output, expected_special[i]) || (dataSize <= 0 && expected_special[i][0] != '\0')) {
			fprintf(stderr, "Invalid returned col %d: '%s'!='%s'\n", i, expected_special[i], output);
			ok = 0;
		}
	}
	if (!ok)
		exit(1);
	CHKCloseCursor("SI");
}
