/* 
 * Purpose: Test bcp functions
 * Functions: bcp_batch bcp_bind bcp_done bcp_init bcp_sendrow 
 */

#include "common.h"

#include <assert.h>

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#include "bcp.h"

static char cmd[512];
static int init(DBPROCESS * dbproc, const char *name);
static void test_bind(DBPROCESS * dbproc);

/*
 * Static data for insertion
 */
static int not_null_bit = 1;
static char not_null_char[] = "a char";
static char not_null_varchar[] = "a varchar";
static char not_null_datetime[] 		= "Dec 17 2003  3:44PM";
static char not_null_smalldatetime[] 	= "Dec 17 2003  3:44PM";
static char not_null_money[] = "12.34";
static char not_null_smallmoney[] = "12.34";
static char not_null_float[] = "12.34";
static char not_null_real[] = "12.34";
static char not_null_decimal[] = "12.34";
static char not_null_numeric[] = "12.34";
static int not_null_int        = 1234;
static int not_null_smallint   = 1234;
static int not_null_tinyint    = 123;


static int
init(DBPROCESS * dbproc, const char *name)
{
	int res = 0;
	RETCODE rc;

	printf("Dropping %s.%s..%s\n", SERVER, DATABASE, name);
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while ((rc=dbresults(dbproc)) == SUCCEED) {
		/* nop */
	}
	if (rc != NO_MORE_RESULTS)
		return 1;

	printf("Creating %s.%s..%s\n", SERVER, DATABASE, name);
	sql_cmd(dbproc);
	sql_cmd(dbproc);

	if (dbsqlexec(dbproc) == FAIL) {
		res = 1;
	}
	while ((rc=dbresults(dbproc)) == SUCCEED) {
		dbprhead(dbproc);
		dbprrow(dbproc);
		while ((rc=dbnextrow(dbproc)) == REG_ROW) {
			dbprrow(dbproc);
		}
	}
	if (rc != NO_MORE_RESULTS)
		return 1;
	printf("%s\n", res? "error" : "ok");
	return res;
}

#define VARCHAR_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, strlen(x), NULL, termlen, SYBVARCHAR, col++ )

#define INT_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, -1, NULL, termlen, SYBINT4,    col++ )

#define NULL_BIND(x, type) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, 0, NULL, termlen, type,    col++ )

static void
test_bind(DBPROCESS * dbproc)
{
	enum { prefixlen = 0 };
	enum { termlen = 0 };
	enum NullValue { IsNull, IsNotNull };

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

	/* nulls */
	fOK = NULL_BIND(not_null_char, SYBVARCHAR);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_varchar, SYBVARCHAR);
	assert(fOK == SUCCEED); 

	fOK = NULL_BIND(not_null_datetime, SYBVARCHAR);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_smalldatetime, SYBVARCHAR);
	assert(fOK == SUCCEED); 

	fOK = NULL_BIND(not_null_money, SYBVARCHAR);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_smallmoney, SYBVARCHAR);
	assert(fOK == SUCCEED); 

	fOK = NULL_BIND(not_null_float, SYBVARCHAR);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_real, SYBVARCHAR);
	assert(fOK == SUCCEED); 

	fOK = NULL_BIND(not_null_decimal, SYBVARCHAR);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_numeric, SYBVARCHAR);
	assert(fOK == SUCCEED); 

	fOK = NULL_BIND(not_null_int, SYBINT4);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_smallint, SYBINT4);
	assert(fOK == SUCCEED); 
	fOK = NULL_BIND(not_null_tinyint, SYBINT4);
	assert(fOK == SUCCEED); 

}

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i, rows_sent=0;
	int failed = 0;
	const char *s;
	const char *table_name = "all_types_bcp_unittest";

	set_malloc_options();

	read_login_info(argc, argv);

	printf("Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "bcp.c unit test");
	BCP_SETL(login, 1);

	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	if (init(dbproc, table_name))
		exit(1);

	/* set up and send the bcp */
	sprintf(cmd, "%s..%s", DATABASE, table_name);
	printf("preparing to insert into %s ... ", cmd);
	if (bcp_init(dbproc, cmd, NULL, NULL, DB_IN) == FAIL) {
		printf("failed\n");
    		exit(1);
	}
	printf("OK\n");

	test_bind(dbproc);

	printf("Sending same row 10 times... \n");
	for (i=0; i<10; i++) {
		if (bcp_sendrow(dbproc) == FAIL) {
			printf("send failed\n");
		        exit(1);
		}
	}
	
	printf("Sending 5 more rows ... \n");
	for (i=15; i <= 27; i++) {
		int type = dbcoltype(dbproc, i);
		int len = (type == SYBCHAR || type == SYBVARCHAR)? dbcollen(dbproc, i) : -1;
		if (bcp_collen(dbproc, len, i) == FAIL) {
			printf("bcp_collen failed for column %d\n", i);
		        exit(1);
		}
	}
	for (i=0; i<5; i++) {
		if (bcp_sendrow(dbproc) == FAIL) {
			printf("send failed\n");
		        exit(1);
		}
	}
#if 1
	rows_sent = bcp_batch(dbproc);
	if (rows_sent == -1) {
		printf("batch failed\n");
	        exit(1);
	}
#endif

	printf("OK\n");

	/* end bcp.  */
	if ((rows_sent += bcp_done(dbproc)) == -1)
	    printf("Bulk copy unsuccessful.\n");
	else
	    printf("%d rows copied.\n", rows_sent);


	printf("done\n");


#if 1
	sql_cmd(dbproc);

	dbsqlexec(dbproc);
	while ((i=dbresults(dbproc)) == SUCCEED) {
		dbprhead(dbproc);
		dbprrow(dbproc);
		while ((i=dbnextrow(dbproc)) == REG_ROW) {
			dbprrow(dbproc);
		}
	}
#endif
	if ((s = getenv("BCP")) != NULL && 0 == strcmp(s, "nodrop")) {
		printf("BCP=nodrop: '%s..%s' kept\n", DATABASE, table_name);
	} else {
		printf("Dropping table %s\n", table_name);
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}
	dbexit();

	failed = 0;

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
