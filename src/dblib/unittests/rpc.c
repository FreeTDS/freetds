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

#ifdef DBNTWIN32
#include "winhackery.h"
#endif

#include <sqlfront.h>
#include <sqldb.h>

#include "common.h"

static char software_version[] = "$Id: rpc.c,v 1.11 2004-03-25 04:51:14 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static char cmd[4096];
static int init_proc(DBPROCESS * dbproc, const char *name);

static const char procedure_sql[] = 
		"CREATE PROCEDURE %s \n"
			"@nrows int OUTPUT \n"
		"AS \n"
		"BEGIN \n"
			"select distinct convert(varchar(30), name) as 'type'  from systypes \n"
			"select @nrows = @@rowcount \n"
			"select distinct convert(varchar(30), name) as name  from sysobjects where type = 'S'"
			"return 42 \n"
		"END \n";

static int
init_proc(DBPROCESS * dbproc, const char *name)
{
	int res = 0;

	fprintf(stdout, "Dropping procedure %s\n", name);
	add_bread_crumb();
	sprintf(cmd, "DROP PROCEDURE %s", name);
	dbcmd(dbproc, cmd);
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	add_bread_crumb();

	fprintf(stdout, "Creating procedure %s\n", name);
	sprintf(cmd, procedure_sql, name);
	dbcmd(dbproc, cmd);
	if (dbsqlexec(dbproc) == FAIL) {
		add_bread_crumb();
		res = 1;
		if (name[0] == '#')
			fprintf(stdout, "Failed to create procedure %s. Wrong permission or not MSSQL.\n", name);
		else
			fprintf(stdout, "Failed to create procedure %s. Wrong permission.\n", name);
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	return res;
}

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i, r;
	char teststr[1024];
	int failed = 0;
	char *retname = NULL;
	int rettype = 0, retlen = 0, retval = 0xdeadbeef;
	char proc[] = "#t0022", param[] = "@nrows";
	char *proc_name = proc;
	RETCODE erc, row_code;

	set_malloc_options();

	read_login_info();

	fprintf(stdout, "Start\n");
	add_bread_crumb();

	dbinit();

	add_bread_crumb();
	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	add_bread_crumb();
	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "#t0022");

	fprintf(stdout, "About to open %s.%s\n", SERVER, DATABASE);

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();

	add_bread_crumb();

	if (init_proc(dbproc, proc_name))
		if (init_proc(dbproc, ++proc_name))
			exit(1);

	/* set up and send the rpc */
	erc = dbrpcinit(dbproc, proc_name, 0);	/* no options */
	printf("executing dbrpcinit\n");
	if (erc == FAIL) {
		fprintf(stderr, "Failed: dbrpcinit\n");
		failed = 1;
	}

	printf("executing dbrpcparam\n");
	erc = dbrpcparam(dbproc, param, DBRPCRETURN, SYBINT4, /*maxlen= */ -1, sizeof(retval), (BYTE *) & retval);
	if (erc == FAIL) {
		fprintf(stderr, "Failed: dbrpcparam\n");
		failed = 1;
	}

	printf("executing dbrpcsend\n");
	erc = dbrpcsend(dbproc);
	if (erc == FAIL) {
		fprintf(stderr, "Failed: dbrpcsend\n");
		exit(1);
	}

	/* wait for it to execute */
	printf("executing dbsqlok\n");
	erc = dbsqlok(dbproc);
	if (erc == FAIL) {
		fprintf(stderr, "Failed: dbsqlok\n");
		failed = 1;
	}

	add_bread_crumb();

	/* retrieve outputs per usual */
	r = 0;
	while ((erc = dbresults(dbproc)) != NO_MORE_RESULTS) {
		if (erc == SUCCEED) {
			const int ncols = dbnumcols(dbproc);
			printf("bound 1 of %d columns in result %d.\n", ncols, ++r);
			dbbind(dbproc, 1, STRINGBIND, -1, (BYTE *) teststr);
			
			while ((row_code = dbnextrow(dbproc)) != NO_MORE_ROWS) {
				if (row_code == REG_ROW) {
					printf("\t%s\n", teststr);
				} else {
					/* not supporting computed rows in this unit test */
					failed = 1;
					fprintf(stderr, "Failed.  Expected a row\n");
					exit(1);
				}
			}
			/* check return status */
			printf("retrieving return status... ");
			if (dbhasretstat(dbproc) == TRUE) {
				printf("(return status %d)\n", dbretstatus(dbproc));
			} else {
				printf("none\n");
			}
			/* check return parameter values */
			printf("retrieving output parameter... ");
			if (dbnumrets(dbproc) > 0) {
				for (i = 1; i <= dbnumrets(dbproc); i++) {
					add_bread_crumb();
					retname = dbretname(dbproc, i);
					printf("ret name %d is %s\n", i, retname);
					rettype = dbrettype(dbproc, i);
					printf("ret type %d is %d\n", i, rettype);
					retlen = dbretlen(dbproc, i);
					printf("ret len %d is %d\n", i, retlen);
					dbconvert(dbproc, rettype, dbretdata(dbproc, i), retlen, SYBVARCHAR, (BYTE*) teststr, -1);
					printf("ret data %d is %s\n", i, teststr);
					add_bread_crumb();
				}
			} else {
				printf("none\n");
			}
		} else {
			add_bread_crumb();
			fprintf(stdout, "Was expecting a result set.\n");
			exit(1);
		}
	} /* while dbresults */
	
	if ((retname == NULL) || strcmp(retname, param)) {
		fprintf(stdout, "Was expecting a retname to be '%s'.\n", param);
		exit(1);
	}
	if (strcmp(teststr, "42")) {
		fprintf(stdout, "Was expecting a retdata to be 42.\n");
		exit(1);
	}
	if (rettype != SYBINT4) {
		fprintf(stdout, "Was expecting a rettype to be SYBINT4 was %d.\n", rettype);
		exit(1);
	}
	if (retlen != 4) {
		fprintf(stdout, "Was expecting a retlen to be 4.\n");
		exit(1);
	}

	printf("done\n");

	add_bread_crumb();


	fprintf(stdout, "Dropping procedure\n");
	add_bread_crumb();
	sprintf(cmd, "DROP PROCEDURE %s", proc_name);
	dbcmd(dbproc, cmd);
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	add_bread_crumb();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	return failed ? 1 : 0;
}
