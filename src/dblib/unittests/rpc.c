/* 
 * Purpose: Test remote procedure calls
 * Functions:  dbretdata dbretlen dbretname dbretstatus dbrettype dbrpcinit dbrpcparam dbrpcsend 
 */

#include "common.h"

static RETCODE init_proc(DBPROCESS * dbproc, const char *name);
int ignore_err_handler(DBPROCESS * dbproc, int severity, int dberr, int oserr, char *dberrstr, char *oserrstr);
int ignore_msg_handler(DBPROCESS * dbproc, DBINT msgno, int state, int severity, char *text, char *server, char *proc, int line);

typedef struct {
	char *name, *value;
	int type, len;
} RETPARAM;

static RETPARAM* save_retparam(RETPARAM *param, char *name, char *value, int type, int len);

static RETCODE
init_proc(DBPROCESS * dbproc, const char *name)
{
	RETCODE ret = FAIL;

	if (name[0] != '#') {
		fprintf(stdout, "Dropping procedure %s\n", name);
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}

	fprintf(stdout, "Creating procedure %s\n", name);
	sql_cmd(dbproc);
	if ((ret = dbsqlexec(dbproc)) == FAIL) {
		if (name[0] == '#')
			fprintf(stdout, "Failed to create procedure %s. Wrong permission or not MSSQL.\n", name);
		else
			fprintf(stdout, "Failed to create procedure %s. Wrong permission.\n", name);
	}
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	return ret;
}

static RETPARAM*
save_retparam(RETPARAM *param, char *name, char *value, int type, int len)
{
	free(param->name);
	free(param->value);
	
	param->name = strdup(name);
	param->value = strdup(value);
	
	param->type = type;
	param->len = len;
	
	return param;
}

static void
free_retparam(RETPARAM *param)
{
	free(param->name);
	free(param->value);
	param->name = param->value = NULL;
}

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

static int 
colwidth( DBPROCESS * dbproc, int icol ) 
{
	int width = dbwillconvert(dbcoltype(dbproc, icol), SYBCHAR);
	return 255 == width? dbcollen(dbproc, icol) : width;
}

char param_data1[64], param_data3[8000+1], param_data4[2 * 4000 + 1];
int param_data2, param_data5;

struct parameters_t {
	char         *name;
	BYTE         status;
	int          type;
	DBINT        maxlen;
	DBINT        datalen;
	BYTE         *value;
} bindings[] = 
	{ { "@null_input", DBRPCRETURN, SYBCHAR,  -1,   0, NULL }
	, { "@first_type", DBRPCRETURN, SYBCHAR,  sizeof(param_data1), 0, (BYTE *) &param_data1 }
	, { "@nullout",    DBRPCRETURN, SYBINT4,  -1,   0, (BYTE *) &param_data2 }
	, { "@varchar_tds7_out", DBRPCRETURN, SYBVARCHAR,  sizeof(param_data3),   0, (BYTE *) &param_data3 }
	, { "@nvarchar_tds7_out", DBRPCRETURN, 231,  sizeof(param_data4),   0, (BYTE *) &param_data4 }
	, { "@nrows",      DBRPCRETURN, SYBINT4,  -1,  -1, (BYTE *) &param_data5 }
	, { "@c_this_name_is_way_more_than_thirty_characters_charlie",
		           0,        SYBVARCHAR,   0,   0, NULL }
	, { "@nv",         0,        SYBVARCHAR,  -1,   2, (BYTE *) "OK:" }
	}, *pb = bindings;

int
main(int argc, char **argv)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	RETPARAM save_param, save_varchar_tds7_param, save_nvarchar_tds7_param;
	
	char teststr[8000+1], abbrev_data[10+3+1], *output;
	char *retname = NULL;
	int i, failed = 0;
	int rettype = 0, retlen = 0, return_status = 0;
	char proc[] = "#t0022";
	char *proc_name = proc;

	int num_resultset = 0, num_empty_resultset = 0;
	int num_params = 6;

	static const char dashes30[] = "------------------------------";
	static const char  *dashes5 = dashes30 + (sizeof(dashes30) - 5), 
			  *dashes20 = dashes30 + (sizeof(dashes30) - 20);

	RETCODE erc, row_code;

	set_malloc_options();
	
	memset(&save_param, 0, sizeof(save_param));
	memset(&save_varchar_tds7_param, 0, sizeof(save_varchar_tds7_param));
	memset(&save_nvarchar_tds7_param, 0, sizeof(save_nvarchar_tds7_param));

	read_login_info(argc, argv);

	fprintf(stdout, "Starting %s\n", argv[0]);

	dbinit();

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "About to logon\n");

	login = dblogin();
	DBSETLPWD(login, PASSWORD);
	DBSETLUSER(login, USER);
	DBSETLAPP(login, "rpc");
	dberrhandle(ignore_err_handler);
	DBSETLPACKET(login, -1);
	dberrhandle(syb_err_handler);


	fprintf(stdout, "About to open %s.%s\n", SERVER, DATABASE);

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	printf("Check if server support long identifiers\n");
	sql_cmd(dbproc);
	i = 103;
	dbsetuserdata(dbproc, (BYTE*) &i);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS)
		while (dbnextrow(dbproc) != NO_MORE_ROWS)
			continue;
	dbsetuserdata(dbproc, NULL);
	if (i == 0) {
		fprintf(stderr, "This server does not support long identifiers\n");
		dbexit();
		return 0;
	}

	dberrhandle(ignore_err_handler);
	dbmsghandle(ignore_msg_handler);

	printf("trying to create a temporary stored procedure\n");
	if (FAIL == init_proc(dbproc, proc_name)) {
		num_params = 4;
		printf("trying to create a permanent stored procedure\n");
		if (FAIL == init_proc(dbproc, ++proc_name))
			exit(EXIT_FAILURE);
	}

	dberrhandle(syb_err_handler);
	dbmsghandle(syb_msg_handler);

	fprintf(stdout, "Created procedure %s\n", proc_name);

	/* set up and send the rpc */
	printf("executing dbrpcinit\n");
	erc = dbrpcinit(dbproc, proc_name, 0);	/* no options */
	if (erc == FAIL) {
		fprintf(stderr, "Failed line %d: dbrpcinit\n", __LINE__);
		failed = 1;
	}

	for (pb = bindings, i = 0; pb < bindings + sizeof(bindings)/sizeof(bindings[0]); pb++, i++) {
		printf("executing dbrpcparam for %s\n", pb->name);
		if (num_params == 4 && (i == 3 || i == 4))
			continue;
		if ((erc = dbrpcparam(dbproc, pb->name, pb->status, pb->type, pb->maxlen, pb->datalen, pb->value)) == FAIL) {
			fprintf(stderr, "Failed line %d: dbrpcparam\n", __LINE__);
			failed++;
		}

	}
	printf("executing dbrpcsend\n");
	param_data5 = 0x11223344;
	erc = dbrpcsend(dbproc);
	if (erc == FAIL) {
		fprintf(stderr, "Failed line %d: dbrpcsend\n", __LINE__);
		exit(1);
	}

	/* wait for it to execute */
	printf("executing dbsqlok\n");
	erc = dbsqlok(dbproc);
	if (erc == FAIL) {
		fprintf(stderr, "Failed line %d: dbsqlok\n", __LINE__);
		exit(1);
	}

	/* retrieve outputs per usual */
	printf("fetching results\n");
	while ((erc = dbresults(dbproc)) != NO_MORE_RESULTS) {
		printf("fetched resultset %d %s:\n", 1+num_resultset, erc==SUCCEED? "successfully":"unsuccessfully");
		if (erc == SUCCEED) { 
			const int ncol = dbnumcols(dbproc);
			int empty_resultset = 1, c;
			enum {buflen=1024, nbuf=5};
			char bound_buffers[nbuf][buflen] = { "one", "two", "three", "four", "five" };

			++num_resultset;
			
			for( c=0; c < ncol && c < nbuf; c++ ) {
				printf("column %d (%s) is %d wide, ", c+1, dbcolname(dbproc, c+1), colwidth(dbproc, c+1));
				printf("buffer initialized to '%s'\n", bound_buffers[c]);
			}
			for( c=0; c < ncol && c < nbuf; c++ ) {
				erc = dbbind(dbproc, c+1, STRINGBIND, 0, (BYTE *) bound_buffers[c]);
				if (erc == FAIL) {
					fprintf(stderr, "Failed line %d: dbbind\n", __LINE__);
					exit(1);
				}

				printf("%-*s ", colwidth(dbproc, c+1), dbcolname(dbproc, c+1));
			}
			printf("\n");

			while ((row_code = dbnextrow(dbproc)) != NO_MORE_ROWS) {
				empty_resultset = 0;
				if (row_code == REG_ROW) {
					int c;
					for( c=0; c < ncol && c < nbuf; c++ ) {
						printf("%-*s ", colwidth(dbproc, c+1), bound_buffers[c]);
					}
					printf("\n");
				} else {
					/* not supporting computed rows in this unit test */
					failed = 1;
					fprintf(stderr, "Failed.  Expected a row\n");
					exit(1);
				}
			}
			printf("row count %d\n", (int) dbcount(dbproc));
			if (empty_resultset)
				++num_empty_resultset;
		} else {
			fprintf(stderr, "Expected a result set.\n");
			exit(1);
		}
	} /* while dbresults */
	
	/* check return status */
	printf("retrieving return status...\n");
	if (dbhasretstat(dbproc) == TRUE) {
		printf("%d\n", return_status = dbretstatus(dbproc));
	} else {
		printf("none\n");
	}

	/* 
	 * Check output parameter values 
	 */
	if (dbnumrets(dbproc) != num_params) {	/* dbnumrets missed something */
		fprintf(stderr, "Expected %d output parameters.\n", num_params);
		exit(1);
	}
	printf("retrieving output parameters...\n");
	printf("%-5s %-20s %5s %6s  %-30s\n", "param", "name", "type", "length", "data");
	printf("%-5s %-20s %5s %5s- %-30s\n", dashes5, dashes20, dashes5, dashes5, dashes30);
	for (i = 1; i <= dbnumrets(dbproc); i++) {
		retname = dbretname(dbproc, i);
		rettype = dbrettype(dbproc, i);
		retlen = dbretlen(dbproc, i);
		dbconvert(dbproc, rettype, dbretdata(dbproc, i), retlen, SYBVARCHAR, (BYTE*) teststr, -1);
		if(retlen <= 10) {
			output = teststr;
		} else {
			memcpy(abbrev_data, teststr, 10);
			sprintf(&abbrev_data[10], "...");
			output = abbrev_data;
		}
		printf("%-5d %-20s %5d %6d  %-30s\n", i, retname, rettype, retlen, output);

		save_retparam(&save_param, retname, teststr, rettype, retlen);
		if (i == 4) {
			save_retparam(&save_varchar_tds7_param, retname, teststr, rettype, retlen);
		}
		if (i == 5) {
			save_retparam(&save_nvarchar_tds7_param, retname, teststr, rettype, retlen);
		}
	}

	/* 
	 * Test the last parameter for expected outcome 
	 */
	if ((save_param.name == NULL) || strcmp(save_param.name, bindings[5].name)) {
		fprintf(stderr, "Expected retname to be '%s', got ", bindings[5].name);
		if (save_param.name == NULL) 
			fprintf(stderr, "<NULL> instead.\n");
		else
			fprintf(stderr, "'%s' instead.\n", save_param.name);
		exit(1);
	}
	if (strcmp(save_param.value, "3")) {
		fprintf(stderr, "Expected retdata to be 3.\n");
		exit(1);
	}
	if (save_param.type != SYBINT4) {
		fprintf(stderr, "Expected rettype to be SYBINT4 was %d.\n", save_param.type);
		exit(1);
	}
	if (save_param.len != 4) {
		fprintf(stderr, "Expected retlen to be 4.\n");
		exit(1);
	}

	if (num_params == 6) {
		/*
		 * Test name, size, contents of the VARCHAR(8000) output parameter
		 */
		if ((save_varchar_tds7_param.name == NULL) || strcmp(save_varchar_tds7_param.name, bindings[3].name)) {
			fprintf(stderr, "Expected retname to be '%s', got ", bindings[3].name);
			if (save_varchar_tds7_param.name == NULL)
				fprintf(stderr, "<NULL> instead.\n");
			else
				fprintf(stderr, "'%s' instead.\n", save_varchar_tds7_param.name);
			exit(1);
		}
		if (save_varchar_tds7_param.type != SYBVARCHAR) {
			fprintf(stderr, "Expected rettype to be SYBVARCHAR was %d.\n", save_varchar_tds7_param.type);
			exit(1);
		}
		if (save_varchar_tds7_param.len != 8000) {
			fprintf(stderr, "Expected retlen to be 8000 was %d.\n", save_varchar_tds7_param.len);
			exit(1);
		}

		/*
		 * Test name, size, contents of the NVARCHAR(4000) output parameter
		 */
		if ((save_nvarchar_tds7_param.name == NULL) || strcmp(save_nvarchar_tds7_param.name, bindings[4].name)) {
			fprintf(stderr, "Expected retname to be '%s', got ", bindings[4].name);
			if (save_varchar_tds7_param.name == NULL)
				fprintf(stderr, "<NULL> instead.\n");
			else
				fprintf(stderr, "'%s' instead.\n", save_nvarchar_tds7_param.name);
			exit(1);
		}
		if (save_nvarchar_tds7_param.len != 4000) {
			fprintf(stderr, "Expected retlen to be 4000 was %d.\n", save_nvarchar_tds7_param.len);
			exit(1);
		}
	}

	if(42 != return_status) {
		fprintf(stderr, "Expected status to be 42.\n");
		exit(1);
	}

	printf("Good: Got 6 output parameters and 1 return status of %d.\n", return_status);


	/* Test number of result sets */
	if (num_resultset != 4) {
		fprintf(stderr, "Expected 4 resultset got %d.\n", num_resultset);
		exit(1);
	}
	if (num_empty_resultset != 1) {
		fprintf(stderr, "Expected an empty resultset got %d.\n", num_empty_resultset);
		exit(1);
	}
	printf("Good: Got %d resultsets and %d empty resultset.\n", num_resultset, num_empty_resultset);



	fprintf(stdout, "Dropping procedure\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
	dbexit();

	fprintf(stdout, "%s %s\n", __FILE__, (failed ? "failed!" : "OK"));

	free_retparam(&save_param);
	free_retparam(&save_varchar_tds7_param);
	free_retparam(&save_nvarchar_tds7_param);

	return failed ? 1 : 0;
}
