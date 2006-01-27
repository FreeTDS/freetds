/* 
 * Purpose: Test bcp in, with dbvarylen()
 * Functions: bcp_colfmt bcp_columns bcp_exec bcp_init dbvarylen 
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <assert.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"

static char software_version[] = "$Id: t0017.c,v 1.22 2006-01-27 15:55:58 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };
int failed = 0;


int
main(int argc, char *argv[])
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	RETCODE ret;

#if 0
	char *out_file = "t0017.out";
#endif
	const char *in_file = FREETDS_SRCDIR "/t0017.in";
	const char *err_file = "t0017.err";
	DBINT rows_copied;
	int num_cols = 0;
	int col_type[256];
	DBBOOL col_varylen[256];
	int prefix_len;

	set_malloc_options();

	read_login_info(argc, argv);
	fprintf(stdout, "Starting %s\n", software_version);
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon ... ");

	login = dblogin();
	assert(login);
	BCP_SETL(login, TRUE);
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0017");
	fprintf(stdout, "done\n");

	fprintf(stdout, "Opening \"%s\" for \"%s\" ... ", SERVER, USER);
	dbproc = dbopen(login, SERVER);
	assert(dbproc);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
	}
	dbloginfree(login);
	fprintf(stdout, "done\n");

	fprintf(stdout, "Creating table ... ");
	dbcmd(dbproc, "create table #dblib0017 (c1 int, c2 text)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	fprintf(stdout, "done\n");

	/* 
	 * BCP in 
	 */
	fprintf(stdout, "bcp_init... ");
	ret = bcp_init(dbproc, "#dblib0017", in_file, err_file, DB_IN);
	if (ret != SUCCEED)
		failed = 1;
	else
		fprintf(stdout, "done\n");
		

	fprintf(stderr, "Issuiug SELECT ... ");
	dbcmd(dbproc, "select * from #dblib0017 where 0=1");
	dbsqlexec(dbproc);
	fprintf(stderr, "done\nFetching metadata ... ");
	if (dbresults(dbproc) != FAIL) {
		num_cols = dbnumcols(dbproc);
		for (i = 0; i < num_cols; i++) {
			col_type[i] = dbcoltype(dbproc, i + 1);
			col_varylen[i] = dbvarylen(dbproc, i + 1);
		}
		while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		}
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "bcp_columns ... ");
	ret = bcp_columns(dbproc, num_cols);
	if (ret != SUCCEED)
		failed = 1;
	for (i = 0; i < num_cols; i++) {
		prefix_len = 0;
		if (col_type[i] == SYBIMAGE) {
			prefix_len = 4;
		} else if (col_varylen[i]) {
			prefix_len = 1;
		}
		ret = bcp_colfmt(dbproc, i + 1, col_type[i], prefix_len, -1, NULL, 0, i + 1);
		if (ret == FAIL) {
			fprintf(stderr, "return from bcp_colfmt = %d\n", ret);
			failed = 1;
		}
	}
	fprintf(stderr, "done\n");

	fprintf(stderr, "bcp_exec ... ");
	ret = bcp_exec(dbproc, &rows_copied);
	if (ret != SUCCEED)
		failed = 1;
	else
		fprintf(stderr, "done\n");

#if 0
	/* BCP out */
	ret = bcp_init(dbproc, "#dblib0017", out_file, err_file, DB_OUT);

	fprintf(stderr, "select\n");
	dbcmd(dbproc, "select * from #dblib0017 where 0=1");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) == SUCCEED) {
		num_cols = dbnumcols(dbproc);
		for (i = 0; i < num_cols; i++)
			col_type[i] = dbcoltype(dbproc, i + 1);
		while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		}
	}

	ret = bcp_columns(dbproc, num_cols);
	for (i = 0; i < num_cols; i++) {
		prefix_len = 0;
		if (col_type[i] == SYBIMAGE) {
			prefix_len = 4;
		} else if (!is_fixed_type(col_type[i])) {
			prefix_len = 1;
		}
		bcp_colfmt(dbproc, i + 1, col_type[i], prefix_len, -1, NULL, 0, i);
	}

	ret = bcp_exec(dbproc, &rows_copied);
#endif

	fprintf(stderr, "%d rows copied\n", rows_copied);
	dbclose(dbproc);
	dbexit();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	return failed ? 1 : 0;
}
