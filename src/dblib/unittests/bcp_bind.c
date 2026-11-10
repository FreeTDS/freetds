/*
 * Purpose: Test bcp_bind argument checks, results were compared with Sybase library (DBVERSION_100)
 * and MS ODBC bcp functions
 * Functions: bcp_bind bcp_done bcp_init bcp_sendrow
 */

#include "common.h"

#include <assert.h>

static DBPROCESS *dbproc;

static void
exec_sql(const char *sql)
{
	RETCODE rc;

	printf("exec: %s\n", sql);
	dbcmd(dbproc, sql);
	if (dbsqlexec(dbproc) == FAIL) {
		fprintf(stderr, "failed: %s\n", sql);
		exit(1);
	}
	while ((rc = dbresults(dbproc)) == SUCCEED)
		while (dbnextrow(dbproc) != NO_MORE_ROWS)
			continue;
	assert(rc == NO_MORE_RESULTS);
}

/* return value read back from table, "NULL" for null, "none" if no rows */
static const char *
read_back(const char *table)
{
	static char buf[64];
	char sql[128];
	int rows = 0;

	sprintf(sql, "select convert(varchar(40), x) from %s", table);
	dbcmd(dbproc, sql);
	if (dbsqlexec(dbproc) == FAIL)
		return "error";
	strcpy(buf, "none");
	while (dbresults(dbproc) == SUCCEED) {
		while (dbnextrow(dbproc) == REG_ROW) {
			BYTE *data = dbdata(dbproc, 1);
			DBINT len = dbdatlen(dbproc, 1);

			if (data == NULL)
				strcpy(buf, "NULL");
			else
				sprintf(buf, "%.*s", (int) len, (const char *) data);
			++rows;
		}
	}
	if (rows > 1)
		return "too many rows";
	return buf;
}

typedef struct
{
	int line;
	const char *table;
	int vartype;
	const void *varaddr;
	int prefixlen;
	DBINT varlen;
	const char *terminator;
	int termlen;
	/* error expected from bcp_bind or bcp_sendrow, 0 if none */
	int bind_err;
	int send_err;
	/* data expected in the table, if row inserted */
	const char *result;
} test_case;

static void
test(const test_case *t)
{
	char table_name[64];
	int msgno;
	RETCODE rc;
	DBINT rows;

	printf("Test from line %d\n", t->line);

	sprintf(table_name, "truncate table %s", t->table);
	exec_sql(table_name);

	sprintf(table_name, "%s", t->table);
	rc = bcp_init(dbproc, table_name, NULL, NULL, DB_IN);
	assert(rc == SUCCEED);

	msgno = t->bind_err;
	dbsetuserdata(dbproc, (BYTE *) & msgno);
	rc = bcp_bind(dbproc, (BYTE *) t->varaddr, t->prefixlen, t->varlen, (BYTE *) t->terminator, t->termlen, t->vartype, 1);
	dbsetuserdata(dbproc, NULL);
	if (t->bind_err) {
		assert(rc == FAIL);
		assert(msgno == 0);
		bcp_done(dbproc);
		return;
	}
	assert(rc == SUCCEED);

	msgno = t->send_err;
	dbsetuserdata(dbproc, (BYTE *) & msgno);
	rc = bcp_sendrow(dbproc);
	dbsetuserdata(dbproc, NULL);
	if (t->send_err) {
		assert(rc == FAIL);
		assert(msgno == 0);
	} else {
		assert(rc == SUCCEED);
	}
	rows = bcp_done(dbproc);
	assert(rows == (t->send_err ? 0 : 1));

	if (t->result && strcmp(read_back(t->table), t->result) != 0) {
		fprintf(stderr, "line %d: wrong result '%s' expected '%s'\n", t->line, read_back(t->table), t->result);
		exit(1);
	}
}

static DBINT int_value = 123456;
static DBINT int_value256 = 256;
static DBFLT8 flt_value = 1234.0;
static DBDATETIME dt_value;
static DBDECIMAL dec_value;
static unsigned char int_prefix4[5];
static unsigned char int_prefix8[9];
static char str_value[] = "hello world";
static const unsigned char str_prefix[] = "\005hello world";

static const test_case tests[] = {
	/* fixed type, varlen must be -1, 0 (NULL) or type size, checked when sending */
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, -1, NULL, 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, 0, NULL, 0, 0, 0, "NULL"},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, 4, NULL, 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, 1, NULL, 0, 0, SYBEBCIS, NULL},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, 10, NULL, 0, 0, SYBEBCIS, NULL},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, -1, NULL, -1, 0, 0, "123456"},

	/* fixed type with prefix, a bigger length is accepted */
	{__LINE__, "bcp_bind_int", SYBINT4, int_prefix4, 1, -1, NULL, 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, int_prefix8, 1, -1, NULL, 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, int_prefix4, 1, 10, NULL, 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, int_prefix8, 1, 4, NULL, 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, int_prefix4, 1, 2, NULL, 0, 0, SYBEBCIS, NULL},

	/*
	 * terminators are allowed for all types, for types other than
	 * character and binary they are ignored
	 */
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, -1, "", 1, 0, 0, "123456"},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value256, 0, -1, "", 1, 0, 0, "256"},
	{__LINE__, "bcp_bind_int", SYBFLT8, &flt_value, 0, -1, "", 1, 0, 0, "1234"},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, -1, NULL, 1, SYBEBCIT, 0, NULL},
	{__LINE__, "bcp_bind_int", SYBINT4, &int_value, 0, -1, "", 0, 0, 0, "123456"},
	{__LINE__, "bcp_bind_dt", SYBDATETIME, &dt_value, 0, -1, "", 1, 0, 0, "Nov 10 2026 12:00PM"},
	{__LINE__, "bcp_bind_dt", SYBDATETIME, &dt_value, 0, -1, NULL, 1, SYBEBCIT, 0, NULL},
	{__LINE__, "bcp_bind_num", SYBDECIMAL, &dec_value, 0, -1, "", 1, 0, 0, "123.45"},
	{__LINE__, "bcp_bind_num", SYBDECIMAL, &dec_value, 0, -1, NULL, 1, SYBEBCIT, 0, NULL},

	/* variable types require length, prefix or terminator */
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, 5, NULL, 0, 0, 0, "hello"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, 5, NULL, -1, 0, 0, "hello"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, -1, NULL, 0, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, -1, NULL, -1, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, -1, NULL, 1, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, -1, "", 0, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBCHAR, str_value, 0, -1, NULL, 0, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBBINARY, str_value, 0, -1, NULL, 0, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBVARBINARY, str_value, 0, -1, NULL, 0, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBTEXT, str_value, 0, -1, NULL, 0, SYBEVDPT, 0, NULL},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, 0, NULL, 0, 0, 0, "NULL"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, 0, "", 1, 0, 0, "NULL"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, -1, "", 1, 0, 0, "hello world"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_value, 0, 5, "", 1, 0, 0, "hello"},
	{__LINE__, "bcp_bind_str", SYBCHAR, str_value, 0, -1, "", 1, 0, 0, "hello world"},
	{__LINE__, "bcp_bind_str", SYBTEXT, str_value, 0, -1, "", 1, 0, 0, "hello world"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_prefix, 1, -1, NULL, 0, 0, 0, "hello"},

	/* for variable types prefix takes precedence over length */
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_prefix, 1, 3, NULL, 0, 0, 0, "hello"},
	{__LINE__, "bcp_bind_str", SYBVARCHAR, str_prefix, 1, 8, NULL, 0, 0, 0, "hello"},

	/* binary data are copied as is to character columns */
	{__LINE__, "bcp_bind_str", SYBVARBINARY, str_value, 0, 11, NULL, 0, 0, 0, "hello world"},
	{__LINE__, "bcp_bind_str", SYBBINARY, str_value, 0, -1, "", 1, 0, 0, "hello world"},
	{__LINE__, "bcp_bind_str", SYBVARBINARY, str_prefix, 1, -1, NULL, 0, 0, 0, "hello"},
	{__LINE__, "bcp_bind_str", SYBIMAGE, str_value, 0, 5, NULL, 0, 0, 0, "hello"},
};

TEST_MAIN()
{
	LOGINREC *login;
	DBTYPEINFO ti;
	unsigned i;

	int_prefix4[0] = 4;
	memcpy(int_prefix4 + 1, &int_value, 4);
	memcpy(int_prefix8, int_prefix4, 5);
	int_prefix8[0] = 8;

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbsetversion(DBVERSION_100);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "bcp_bind.c unit test");
	BCP_SETL(login, TRUE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	exec_sql("if exists (select 1 from sysobjects where type = 'U' and name = 'bcp_bind_int') drop table bcp_bind_int");
	exec_sql("if exists (select 1 from sysobjects where type = 'U' and name = 'bcp_bind_str') drop table bcp_bind_str");
	exec_sql("create table bcp_bind_int (x int null)");
	exec_sql("create table bcp_bind_str (x varchar(20) null)");
	exec_sql("if exists (select 1 from sysobjects where type = 'U' and name = 'bcp_bind_dt') drop table bcp_bind_dt");
	exec_sql("if exists (select 1 from sysobjects where type = 'U' and name = 'bcp_bind_num') drop table bcp_bind_num");
	exec_sql("create table bcp_bind_dt (x datetime null)");
	exec_sql("create table bcp_bind_num (x numeric(10,2) null)");

	/* values with zero bytes inside, to check terminators are ignored */
	ti.precision = 10;
	ti.scale = 2;
	assert(dbconvert_ps(dbproc, SYBVARCHAR, (const BYTE *) "123.45", -1, SYBDECIMAL, (BYTE *) & dec_value,
			    sizeof(dec_value), &ti) > 0);
	assert(dbconvert(dbproc, SYBVARCHAR, (const BYTE *) "2026-11-10 12:00:00", -1, SYBDATETIME, (BYTE *) & dt_value,
			 sizeof(dt_value)) > 0);
	assert(memchr(&dt_value, 0, sizeof(dt_value)) != NULL);
	assert(memchr(&dec_value, 0, sizeof(dec_value)) != NULL);

	for (i = 0; i < TDS_VECTOR_SIZE(tests); ++i)
		test(&tests[i]);

	exec_sql("drop table bcp_bind_int");
	exec_sql("drop table bcp_bind_str");
	exec_sql("drop table bcp_bind_dt");
	exec_sql("drop table bcp_bind_num");

	dbexit();

	printf("%s OK\n", __FILE__);
	return 0;
}
