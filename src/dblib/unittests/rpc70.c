/*
 * Purpose: Test RPC NTEXT input with TDS 7.0
 * Functions:  dbretdata dbretlen dbrettype dbrpcinit dbrpcparam dbrpcsend
 */

#include "common.h"

static RETCODE init_proc(DBPROCESS * dbproc, const char *name);
int ignore_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
int ignore_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line);

static RETCODE
init_proc(DBPROCESS * dbproc, const char *name)
{
	RETCODE ret = FAIL;

	if (name[0] != '#') {
		printf("Dropping procedure %s\n", name);
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	printf("Creating procedure %s\n", name);
	sql_cmd(dbproc);
	if ((ret = dbsqlexec(dbproc)) == FAIL) {
		if (name[0] == '#')
			printf("Failed to create procedure %s. Wrong permission or not MSSQL.\n", name);
		else
			printf("Failed to create procedure %s. Wrong permission.\n", name);
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	return ret;
}

static int failed = 0;

int
ignore_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line)
{
	int ret;

	dbsetuserdata(dbproc, (BYTE*) &msgno);
	/* printf("(ignoring message %d)\n", msgno); */
	ret = syb_msg_handler(dbproc, msgno, state, severity, text, server, proc, line);
	dbsetuserdata(dbproc, NULL);
	return ret;
}
/*
 * The bad procedure name message has severity 15, causing db-lib to call the error handler after calling the message handler.
 * This wrapper anticipates that behavior, and again sets the userdata, telling the handler this error is expected. 
 */
int
ignore_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr)
{	
	int erc;
	static int recursion_depth = 0;
	
	if (dbproc == NULL) {	
		printf("expected error %d: \"%s\"\n", dberr, dberrstr? dberrstr : "");
		return INT_CANCEL;
	}
	
	if (recursion_depth++) {
		printf("error %d: \"%s\"\n", dberr, dberrstr? dberrstr : "");
		printf("logic error: recursive call to ignore_err_handler\n");
		exit(1);
	}
	dbsetuserdata(dbproc, (BYTE*) &dberr);
	/* printf("(ignoring error %d)\n", dberr); */
	erc = syb_err_handler(dbproc, severity, dberr, oserr, dberrstr, oserrstr);
	dbsetuserdata(dbproc, NULL);
	recursion_depth--;
	return erc;
}


struct parameters_t {
	const char   *name;
	BYTE         status;
	int          type;
	DBINT        maxlen;
	DBINT        datalen;
	BYTE         *value;
};

#define PARAM_STR(s) sizeof(s)-1, (BYTE*) s
static struct parameters_t bindings[] = {
	  { "", 0, SYBNTEXT,  -1,  PARAM_STR("test123") }
	, { "", DBRPCRETURN, SYBVARCHAR,  7, 0, NULL }
	, { NULL, 0, 0, 0, 0, NULL }
};

static void
bind_param(DBPROCESS *dbproc, struct parameters_t *pb)
{
	RETCODE erc;
	const char *name = pb->name[0] ? pb->name : NULL;

	if ((erc = dbrpcparam(dbproc, name, pb->status, pb->type, pb->maxlen, pb->datalen, pb->value)) == FAIL) {
		fprintf(stderr, "Failed line %d: dbrpcparam\n", __LINE__);
		failed++;
	}
}

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;

	char teststr[8000+1];
	int i;
	int rettype = 0, retlen = 0;
	char proc[] = "#rpc70";
	char *proc_name = proc;

	struct parameters_t *pb;

	RETCODE erc;

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
	DBSETLAPP(login, "rpc70");
	/* force TDS 7.0 login */
	DBSETLVERSION(login, DBVERSION_70);

	printf("About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	if (dbtds(dbproc) != DBTDS_7_0) {
		fprintf(stderr, "Failed line %d: version is not 7.0\n", __LINE__);
		failed = 1;
	}

	dberrhandle(ignore_err_handler);
	dbmsghandle(ignore_msg_handler);

	printf("trying to create a temporary stored procedure\n");
	if (FAIL == init_proc(dbproc, proc_name)) {
		printf("trying to create a permanent stored procedure\n");
		if (FAIL == init_proc(dbproc, ++proc_name))
			exit(EXIT_FAILURE);
	}

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	printf("Created procedure %s\n", proc_name);

	erc = dbrpcinit(dbproc, proc_name, 0);	/* no options */
	if (erc == FAIL) {
		fprintf(stderr, "Failed line %d: dbrpcinit\n", __LINE__);
		failed = 1;
	}
	for (pb = bindings; pb->name != NULL; pb++)
		bind_param(dbproc, pb);
	erc = dbrpcsend(dbproc);
	if (erc == FAIL) {
		fprintf(stderr, "Failed line %d: dbrpcsend\n", __LINE__);
		exit(1);
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS)
		continue;
	if (dbnumrets(dbproc) != 1) {	/* dbnumrets missed something */
		fprintf(stderr, "Expected 1 output parameters.\n");
		exit(1);
	}
	i = 1;
	rettype = dbrettype(dbproc, i);
	retlen = dbretlen(dbproc, i);
	dbconvert(dbproc, rettype, dbretdata(dbproc, i), retlen, SYBVARCHAR, (BYTE*) teststr, -1);
	if (strcmp(teststr, "test123") != 0) {
		fprintf(stderr, "Unexpected '%s' results.\n", teststr);
		exit(1);
	}

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));

	return failed ? 1 : 0;
}
