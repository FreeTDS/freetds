/* 
 * Purpose: Test bcp with a default value column
 * Functions: bcp_batch bcp_bind bcp_done bcp_init bcp_sendrow 
 */

#include "common.h"

#include <assert.h>

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif /* HAVE_SYS_STAT_H */

#include "bcp.h"

static char cmd[1024];
static int init(DBPROCESS * dbproc, const char *name);
static void test_bind(DBPROCESS * dbproc);

/*
 * Static data for insertion
 */
static int not_null_int        = 1234;

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
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, (DBINT) strlen(x), \
		  NULL, termlen, SYBVARCHAR, col++ )

#define INT_BIND(x) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, -1, NULL, termlen, SYBINT4,    col++ )

#define NULL_BIND(x, type) \
	bcp_bind( dbproc, (unsigned char *) &x, prefixlen, 0, NULL, termlen, type,    col++ )

static void
test_bind(DBPROCESS * dbproc)
{
	enum { prefixlen = 0 };
	enum { termlen = 0 };
	static char dt[] = "2025-12-01 01:23:45";

	RETCODE fOK;
	int col=1;

	/* Bind the non-null int field */
 	fOK = INT_BIND(not_null_int);

	/* Non-nullable datetime: we can only leave it unbound on ASE, as 
	 * MSSQL doesn't support generating defaults for NOT NULL fields via BCP.
	 */
	bool is_ms = DBTDS_5_0 != DBTDS(dbproc);
	if (is_ms)
		fOK = VARCHAR_BIND(dt);

	/* Leave nullable fields unbound */

	assert(fOK == SUCCEED); 
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i, rows_sent=0;
	const char *s;
	const char *table_name = "bcp_defaultdate";

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
	DBSETLAPP(login, "bcp_defaultdate.c unit test");
	BCP_SETL(login, 1);

	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	/* Executes up to the third "go" in the input SQL file */
	if (init(dbproc, table_name))
		exit(1);

	/* set up and send the bcp */
	sprintf(cmd, "%s..%s", DATABASE, table_name);
	printf("preparing to insert into %s ... ", cmd);
	if (bcp_init(dbproc, cmd, NULL, NULL, DB_IN) == FAIL) {
		fprintf(stderr, "failed\n");
    		exit(1);
	}
	printf("OK\n");

	test_bind(dbproc);

	printf("Sending row...\n");
 	if (bcp_sendrow(dbproc) == FAIL) {
		fprintf(stderr, "send failed\n");
		exit(1);
	}

	rows_sent = bcp_batch(dbproc);
	if (rows_sent == -1) {
		fprintf(stderr, "batch failed\n");
	        exit(1);
	}

	printf("OK\n");

	/* end bcp.  */
	if ((rows_sent += bcp_done(dbproc)) == -1)
	    printf("Bulk copy unsuccessful.\n");
	else
	    printf("%d rows copied.\n", rows_sent);

	printf("done\n");

#if 1
	/* The rest of the input SQL file except for the footer (where we don't use "go" until we're done) */
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

	/* Footer (everything after the fourth "go") */
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

	printf("%s OK\n", __FILE__);
	return 0;
}
