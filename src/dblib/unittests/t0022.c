/* 
 * Purpose: Test retrieving output parameters and return status 
 * Functions: DBTDS dbnumrets dbresults dbretdata dbretlen dbretname dbrettype dbsqlexec
 */

#include "common.h"


static char software_version[] = "$Id: t0022.c,v 1.21 2006-07-06 12:48:16 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };



int
main(int argc, char **argv)
{
	char cmd[1024];
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024];
	int failed = 0;
	char *retname = NULL;
	int rettype = 0, retlen = 0;

	set_malloc_options();

	read_login_info(argc, argv);

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
	DBSETLAPP(login, "t0022");

	fprintf(stdout, "About to open\n");

	add_bread_crumb();
	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	add_bread_crumb();
	dbloginfree(login);
	add_bread_crumb();

	fprintf(stdout, "Dropping proc\n");
	add_bread_crumb();
	dbcmd(dbproc, "if object_id('t0022') is not null drop proc t0022");
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	add_bread_crumb();

	fprintf(stdout, "creating proc\n");
	dbcmd(dbproc, "create proc t0022 (@b int out) as\nbegin\n select @b = 42\nend\n");
	if (dbsqlexec(dbproc) == FAIL) {
		add_bread_crumb();
		fprintf(stdout, "Failed to create proc t0022.\n");
		exit(1);
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	sprintf(cmd, "declare @b int\nexec t0022 @b = @b output\n");
	fprintf(stdout, "%s\n", cmd);
	dbcmd(dbproc, cmd);
	dbsqlexec(dbproc);
	add_bread_crumb();


	if (dbresults(dbproc) == FAIL) {
		add_bread_crumb();
		fprintf(stdout, "Was expecting a result set.\n");
		exit(1);
	}
	add_bread_crumb();

	if ((dbnumrets(dbproc) == 0)
	    && ((DBTDS(dbproc) == DBTDS_7_0)
		|| (DBTDS(dbproc) == DBTDS_8_0))) {
		fprintf(stdout, "WARNING:  Received no return parameters from server!\n");
		fprintf(stdout, "WARNING:  This is likely due to a bug in Microsoft\n");
		fprintf(stdout, "WARNING:  SQL Server 7.0 SP3 and later.\n");
		fprintf(stdout, "WARNING:  Please try again using TDS protocol 4.2.\n");
		dbcmd(dbproc, "drop proc t0022");
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
		dbexit();
		free_bread_crumb();
		exit(0);
	}
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
	if ((retname == NULL) || strcmp(retname, "@b")) {
		fprintf(stdout, "Was expecting a retname to be @b.\n");
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

	fprintf(stdout, "Dropping proc\n");
	add_bread_crumb();
	dbcmd(dbproc, "drop proc t0022");
	add_bread_crumb();
	dbsqlexec(dbproc);
	add_bread_crumb();
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	add_bread_crumb();
	dbexit();
	add_bread_crumb();

	fprintf(stdout, "dblib %s on %s\n", (failed ? "failed!" : "okay"), __FILE__);
	free_bread_crumb();
	return failed ? 1 : 0;
}
