/* 
 * Purpose: Test sending and receiving TEXT datatype
 * Functions: dbmoretext dbreadtext dbtxptr dbtxtimestamp dbwritetext 
 */

#include "common.h"

#include <freetds/bool.h>

#define BLOB_BLOCK_SIZE 4096

static bool failed = false;

#define TABLE_NAME "freetds_dblib_t0013"

static char *testargs[] = { "", FREETDS_SRCDIR "/data.bin", "t0013.out" };

static DBPROCESS *dbproc, *dbprocw;

static void
drop_table(void)
{
	if (!dbproc) 
		return;

	dbcancel(dbproc);

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}
}

static int
test(int argc, char **argv, bool over4k)
{
	LOGINREC *login;
	int i;
	DBINT testint;
	FILE *fp;
	ptrdiff_t result;
	long isiz;
	char *blob, *rblob;
	DBBINARY *textPtr = NULL, *timeStamp = NULL;
	char objname[256];
	char rbuf[BLOB_BLOCK_SIZE];
	size_t numread;
	int data_ok;
	int numtowrite, numwritten;
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
	DBSETLAPP(login, "t0013");

	printf("About to open, PASSWORD: %s, USER: %s, SERVER: %s\n", "", "", "");	/* PASSWORD, USER, SERVER); */

	dbproc = dbopen(login, SERVER);
	dbprocw = dbopen(login, SERVER);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
		dbuse(dbprocw, DATABASE);
	}
	dbloginfree(login);
	printf("logged in\n");

	if (argc == 1) {
		argv = testargs;
		argc = 3;
	}
	if (argc < 3) {
		fprintf(stderr, "Usage: %s infile outfile\n", argv[0]);
		return 1;
	}

	if ((fp = fopen(argv[1], "rb")) == NULL) {
		fprintf(stderr, "Cannot open input file: %s\n", argv[1]);
		return 2;
	}
	printf("Reading binary input file\n");

	fseek(fp, 0, SEEK_END);
	isiz = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	blob = (char *) malloc(isiz);
	assert(blob);
	result = fread((void *) blob, isiz, 1, fp);
	assert(result == 1);
	fclose(fp);

	drop_table();

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "Error inserting blob\n");
		return 4;
	}

	for (i=0; (result = dbnextrow(dbproc)) != NO_MORE_ROWS; i++) {
		assert(REG_ROW == result);
		printf("fetching row %d\n", i+1);
		strcpy(objname, TABLE_NAME ".PigTure");
		textPtr = dbtxptr(dbproc, 1);
		timeStamp = dbtxtimestamp(dbproc, 1);
		break; /* can't proceed until no more rows */
	}
	assert(REG_ROW == result || 0 < i);

#ifdef DBTDS_7_2
	if (!textPtr && !timeStamp && dbtds(dbproc) >= DBTDS_7_2) {
		printf("Protocol 7.2+ detected, test not supported\n");
		free(blob);
		dbclose(dbproc);
		dbproc = NULL;
		dbexit();
		exit(0);
	}
#endif

	if (!textPtr) {
		fprintf(stderr, "Error getting textPtr\n");
		exit(1);
	}

	/*
	 * Use #ifdef if you want to test dbmoretext mode (needed for 16-bit apps)
	 * Use #ifndef for big buffer version (32-bit)
	 */
	printf("writing text ... ");
	if (over4k) {
		if (dbwritetext(dbprocw, objname, textPtr, DBTXPLEN, timeStamp, FALSE, isiz, (BYTE*) blob) != SUCCEED)
			return 5;
		printf("done (in one shot)\n");
		for (; (result = dbnextrow(dbproc)) != NO_MORE_ROWS; i++) {
			assert(REG_ROW == result);
			printf("fetching row %d?\n", i+1);
		}
	} else {
		if (dbwritetext(dbprocw, objname, textPtr, DBTXPLEN, timeStamp, FALSE, isiz, NULL) != SUCCEED)
			return 15;
		printf("done\n");

		printf("dbsqlok\n");
		dbsqlok(dbprocw);
		printf("dbresults\n");
		dbresults(dbprocw);

		numtowrite = 0;
		/* Send the update value in chunks. */
		for (numwritten = 0; numwritten < isiz; numwritten += numtowrite) {
			printf("dbmoretext %d\n", 1 + numwritten);
			numtowrite = (isiz - numwritten);
			if (numtowrite > BLOB_BLOCK_SIZE)
				numtowrite = BLOB_BLOCK_SIZE;
			dbmoretext(dbprocw, (DBINT) numtowrite, (BYTE *) (blob + numwritten));
		}
		dbsqlok(dbprocw);
		while (dbresults(dbprocw) != NO_MORE_RESULTS){
			printf("suprise!\n");
		}
	}
#if 0
	if (SUCCEED != dbclose(dbproc)){
		printf("dbclose failed");
		exit(1);
	}
	dbproc = dbopen(login, SERVER);
	assert(dbproc);
	if (strlen(DATABASE)) {
		dbuse(dbproc, DATABASE);
	}
#endif
	sql_cmd(dbproc);
	dbsqlexec(dbproc);

	if (dbresults(dbproc) != SUCCEED) {
		failed = true;
		printf("Was expecting a result set.");
		exit(1);
	}

	for (i = 1; i <= dbnumcols(dbproc); i++) {
		printf("col %d, \"%s\", is type %s\n", 
			i, dbcolname(dbproc, i), dbprtype(dbcoltype(dbproc, i)));
	}
	if (2 != dbnumcols(dbproc)) {
		fprintf(stderr, "Failed.  Expected 2 columns\n");
		exit(1);
	}

	if (SUCCEED != dbbind(dbproc, 1, INTBIND, -1, (BYTE *) & testint)) {
		fprintf(stderr, "Had problem with bind\n");
		exit(1);
	}

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "Failed.  Expected a row\n");
		exit(1);
	}
	if (testint != 1) {
		failed = true;
		fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", 1, (int) testint);
		exit(1);
	}
	dbnextrow(dbproc);

	/* get the image */
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	dbresults(dbproc);

	printf("select 2\n");

	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	if (dbresults(dbproc) != SUCCEED) {
		fprintf(stderr, "Error extracting blob\n");
		return 6;
	}

	numread = 0;
	rblob = NULL;
	while ((result = dbreadtext(dbproc, rbuf, BLOB_BLOCK_SIZE)) != NO_MORE_ROWS) {
		assert(result >= 0);
		if (result != 0) {	/* this indicates not end of row */
			rblob = (char*) realloc(rblob, result + numread);
			assert(rblob);
			memcpy((void *) (rblob + numread), (void *) rbuf, result);
			numread += result;
		}
	}

	data_ok = 1;
	if (rblob == NULL) {
		fputs("No blob data received", stderr);
		return 7;
	}
	if (memcmp(blob, rblob, numread) != 0) {
		printf("Saving first blob data row to file: %s\n", argv[2]);
		if ((fp = fopen(argv[2], "wb")) == NULL) {
			free(blob);
			free(rblob);
			fprintf(stderr, "Unable to open output file: %s\n", argv[2]);
			return 3;
		}
		fwrite((void *) rblob, numread, 1, fp);
		fclose(fp);
		failed = true;
		data_ok = 0;
	}

	printf("Read blob data row %d --> %s %lu byte comparison\n",
	       (int) testint, data_ok ? "PASSED" : "failed", (long unsigned int) numread);
	free(rblob);

	if (dbnextrow(dbproc) != NO_MORE_ROWS) {
		failed = true;
		fprintf(stderr, "Was expecting no more rows\n");
		exit(1);
	}

	free(blob);
	drop_table();
	dbclose(dbproc);
	dbproc = NULL;

	dbexit();

	return 0;
}

TEST_MAIN()
{
	int res = test(argc, argv, false);
	if (!res)
		res = test(argc, argv, true);
	if (res)
		return res;

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}

