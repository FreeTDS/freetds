/* 
 * Purpose: Test bcp in, with dbvarylen()
 * Functions: bcp_colfmt bcp_columns bcp_exec bcp_init dbvarylen 
 */

#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"

static char software_version[] = "$Id: t0017.c,v 1.19 2005-04-19 03:51:04 jklowden Exp $";
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
	fprintf(stdout, "Start\n");
	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	BCP_SETL(login, TRUE);
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "t0017");

	fprintf(stdout, "About to open, PASSWORD: %s, USER: %s, SERVER: %s\n", "", "", "");	/* PASSWORD, USER, SERVER); */

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
	}
	dbloginfree(login);
	fprintf(stdout, "After logon\n");

	fprintf(stdout, "Dropping table\n");
	dbcmd(dbproc, "drop table #dblib0017");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	fprintf(stdout, "Creating table\n");
	dbcmd(dbproc, "create table #dblib0017 (c1 int, c2 text)");
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	/* BCP in */

	ret = bcp_init(dbproc, "#dblib0017", in_file, err_file, DB_IN);
	if (ret != SUCCEED)
		failed = 1;

	fprintf(stderr, "select\n");
	dbcmd(dbproc, "select * from #dblib0017 where 0=1");
	dbsqlexec(dbproc);
	if (dbresults(dbproc) != FAIL) {
		num_cols = dbnumcols(dbproc);
		for (i = 0; i < num_cols; i++) {
			col_type[i] = dbcoltype(dbproc, i + 1);
			col_varylen[i] = dbvarylen(dbproc, i + 1);
		}
		while (dbnextrow(dbproc) != NO_MORE_ROWS) {
		}
	}

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
			fprintf(stdout, "return from bcp_colfmt = %d\n", ret);
			failed = 1;
		}
	}

	ret = bcp_exec(dbproc, &rows_copied);
	if (ret != SUCCEED)
		failed = 1;

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

	fprintf(stdout, "%d rows copied\n", rows_copied);
	dbclose(dbproc);
	dbexit();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	return failed ? 1 : 0;
}
