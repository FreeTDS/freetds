/* 
 * Purpose: Test binding strings and ints, attempt 2nd query with results pending. 
 * Functions: dbbind dbcmd dbnextrow dbopen dbresults dbsqlexec 
 */

#include "common.h"

static void
create_tables(DBPROCESS * dbproc, int rows_to_add)
{
	int i;

	printf("creating table\n");
	sql_cmd(dbproc);
	dbsqlexec(dbproc);
	while (dbresults(dbproc) != NO_MORE_RESULTS) {
		/* nop */
	}

	printf("insert\n");
	for (i = 1; i < rows_to_add; i++) {
		sql_cmd(dbproc);
		dbsqlexec(dbproc);
		while (dbresults(dbproc) != NO_MORE_RESULTS) {
			/* nop */
		}
	}
}


static int
start_query(DBPROCESS * dbproc)
{
	int i;

	if (SUCCEED != sql_cmd(dbproc)) {
		return 0;
	}
	if (SUCCEED != dbsqlexec(dbproc)) {
		return 0;
	}

	if (dbresults(dbproc) != SUCCEED)
		return 0;

	for (i = 1; i <= dbnumcols(dbproc); i++)
		printf("col %d is named \"%s\"\n", i, dbcolname(dbproc, i));

	return 1;
}

static const char *
hex_buffer(BYTE *binarybuffer, int size, char *textbuf)
{
	int i;
	strcpy(textbuf, "0x");  /* must be large enough */
	for (i = 0; i < size; i++) {
		sprintf(textbuf + 2 + i * 2, "%02X", binarybuffer[i]);
	}
	return textbuf;  /* convenience */
}

TEST_MAIN()
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	int i;
	char teststr[1024], teststr2[1024];
	DBINT testint, binvaluelength;
	DBVARYBIN  testvbin, testvbin2;
	DBVARYCHAR testvstr;
	BYTE testbin[10];
	int failed = 0;
	int expected_error;

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
	DBSETLAPP(login, "t0007");

	printf("About to open\n");

	dbproc = dbopen(login, SERVER);
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);
	dbloginfree(login);

	create_tables(dbproc, 10);

	if (!start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) & testint);
	dbbind(dbproc, 2, STRINGBIND, 0, (BYTE *) teststr);

	for (i = 1; i <= 2; i++) {
		char expected[1024];

		sprintf(expected, "row %07d", i);

		if (i % 5 == 0) {
			dbclrbuf(dbproc, 5);
		}

		testint = -1;
		strcpy(teststr, "bogus");

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			abort();
		}
		if (testint != i) {
			fprintf(stderr, "Failed.  Expected i to be %d, was %d\n", i, (int) testint);
			abort();
		}
		if (0 != strncmp(teststr, expected, strlen(expected))) {
			printf("Failed.  Expected s to be |%s|, was |%s|\n", expected, teststr);
			abort();
		}
		printf("Read a row of data -> %d %s\n", (int) testint, teststr);
	}


	printf("second select.  Should fail.\n");

	expected_error = 20019;
	dbsetuserdata(dbproc, (BYTE*) &expected_error);

	if (start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query should have failed but didn't\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbcancel(dbproc);
	
	/* 
	 * Test Binary binding
	 */
	if (!start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbbind(dbproc, 1, VARYBINBIND, sizeof(testvbin), (BYTE *) &testvbin);
	dbbind(dbproc, 2, VARYCHARBIND, sizeof(testvstr), (BYTE *) &testvstr);
	dbbind(dbproc, 3, BINARYBIND, sizeof(testint), (BYTE *) &testint);

	for (i = 1; i <= 2; i++) {
		char expected[1024];

		sprintf(expected, "row %07d ", i);

		testint = -1;
		memset(&testvbin, '*', sizeof(testvbin));
		memset(&testvstr, '*', sizeof(testvstr));

		if (REG_ROW != dbnextrow(dbproc)) {
			fprintf(stderr, "Failed.  Expected a row\n");
			abort();
		}
		if (testint != i) {
			fprintf(stderr, "Failed, line %d.  Expected i to be %d, was %d (0x%x)\n", __LINE__, i, (int) testint, (int) testint);
			abort();
		}
		if (testvbin.len != sizeof(testint)) {
			fprintf(stderr, "Failed, line %d.  Expected bin length to be %d, was %d\n", __LINE__, (int) sizeof(testint), (int) testvbin.len);
			abort();
		}
		memcpy(&testint, testvbin.array, sizeof(testint));
		if (testint != i) {
			fprintf(stderr, "Failed, line %d.  Expected i to be %d, was %d (0x%x)\n", __LINE__, i, (int) testint, (int) testint);
			abort();
		}
		if (testvstr.len != strlen(expected) || 0 != strncmp(testvstr.str, expected, strlen(expected))) {
			printf("Failed, line %d.  Expected s to be |%s|, was |%s|\n", __LINE__, expected, testvstr.str);
			abort();
		}
		testvstr.str[testvstr.len] = 0;
		printf("Read a row of data -> %d %s\n", (int) testint, testvstr.str);
	}

	dbcancel(dbproc);

	/*
	 * Test var binary bindings of binary values
	 */
	if (!start_query(dbproc)) {
		fprintf(stderr, "%s:%d: start_query failed\n", __FILE__, __LINE__);
		failed = 1;
	}

	dbbind(dbproc, 1, INTBIND, 0, (BYTE *) &binvaluelength);  /* returned binary string length (all columns) */
	dbbind(dbproc, 2, VARYBINBIND, sizeof(testvbin), (BYTE *) &testvbin);  /* returned as varbinary, bound varbinary */
	dbbind(dbproc, 3, VARYBINBIND, sizeof(testvbin2), (BYTE *) &testvbin2); /* returned as binary, bound varbinary */
	dbbind(dbproc, 4, BINARYBIND, sizeof(testbin), (BYTE *) &testbin);  /* returned as varbinary, bound binary */

	memset(&testvbin, '*', sizeof(testvbin));
	memset(&testvbin2, '*', sizeof(testvbin2));
	memset(&testbin, '@', sizeof(testbin));  /* different. After fetch all buffers should have same value */

	if (REG_ROW != dbnextrow(dbproc)) {
		fprintf(stderr, "Failed.  Expected a row\n");
		abort();
	}

	if (testvbin.len != binvaluelength) {
		fprintf(stderr, "Failed, line %d.  Expected bin length to be %d, was %d\n", __LINE__,
			(int) binvaluelength, (int) testvbin.len);
		abort();
	}

	if (testvbin2.len != binvaluelength) {
		fprintf(stderr, "Failed, line %d.  Expected bin length to be %d, was %d\n", __LINE__,
			(int) binvaluelength, (int) testvbin.len);
		abort();
	}

	if (memcmp(testvbin.array, testbin, binvaluelength) != 0) {
		fprintf(stderr, "Failed, line %d.  Expected buffer to be %s, was %s\n", __LINE__, 
			hex_buffer(testbin, binvaluelength, teststr),
			hex_buffer(testvbin.array, binvaluelength, teststr2));
		abort();
	}

	if (memcmp(testvbin2.array, testbin, binvaluelength) != 0) {
		fprintf(stderr, "Failed, line %d.  Expected buffer to be %s, was %s\n", __LINE__,
			hex_buffer(testbin, binvaluelength, teststr),
			hex_buffer(testvbin2.array, binvaluelength, teststr2));
		abort();
	}

	memset(teststr2, 0, sizeof(teststr2));  /* finally, test binary padding is all zeroes */
	if (memcmp(testbin + binvaluelength, teststr2, sizeof(testbin) - binvaluelength) != 0) {
		fprintf(stderr, "Failed, line %d.  Expected binary padding to be zeroes, was %s\n", __LINE__,
			hex_buffer(testbin + binvaluelength, sizeof(testbin) - binvaluelength, teststr));
		abort();
	}

	dbexit();

	printf("%s %s\n", __FILE__, (failed ? "failed!" : "OK"));
	return failed ? 1 : 0;
}
