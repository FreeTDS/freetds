#define MSDBLIB 1
#include "common.h"

static void
dump_addr(FILE *out, const char *msg, const void *p, size_t len)
{
	size_t n;
	if (msg)
		fprintf(out, "%s", msg);
	for (n = 0; n < len; ++n)
		fprintf(out, " %02X", ((unsigned char*) p)[n]);
	fprintf(out, "\n");
}

static void
chk(RETCODE ret, const char *msg)
{
	printf("%s: res %d\n", msg, ret);
	if (ret == SUCCEED)
		return;
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

static void
zero_end(DBNUMERIC *num)
{
	/* 27213 == math.floor(math.log(10,256)*65536) */
	unsigned len = 4u+num->precision*27213u/65536u;
	if (num->precision < 1 || num->precision > 77)
		return;
	assert(len >= 4 && len <= sizeof(*num));
	memset(((char*) num) + len, 0, sizeof(*num) - len);
}

static int msdblib;

static void
test(int bind_type, const char *bind_name, int override_prec, int override_scale, int out_prec, int out_scale, int line)
{
	LOGINREC *login;
	DBPROCESS *dbproc;
	DBNUMERIC *num = NULL, *num2 = NULL;
	RETCODE ret;
	DBTYPEINFO ti;
	int i;

	printf("*** Starting test msdblib %d bind %s prec %d scale %d out prec %d out scale %d line %d\n",
		msdblib, bind_name, override_prec, override_scale, out_prec, out_scale, line);
	chk(sql_rewind(), "sql_rewind");
	login = dblogin();

	DBSETLUSER(login, USER);
	DBSETLPWD(login, PASSWORD);
	DBSETLAPP(login, "numeric");
	dbsetmaxprocs(25);
	DBSETLHOST(login, SERVER);

	dbproc = tdsdbopen(login, SERVER, msdblib);
	dbloginfree(login);
	login = NULL;
	if (strlen(DATABASE))
		dbuse(dbproc, DATABASE);

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

	if (DBTDS_5_0 < DBTDS(dbproc)) {
		ret = dbcmd(dbproc,
			    "SET ARITHABORT ON;"
			    "SET CONCAT_NULL_YIELDS_NULL ON;"
			    "SET ANSI_NULLS ON;"
			    "SET ANSI_NULL_DFLT_ON ON;"
			    "SET ANSI_PADDING ON;"
			    "SET ANSI_WARNINGS ON;"
			    "SET ANSI_NULL_DFLT_ON ON;"
			    "SET CURSOR_CLOSE_ON_COMMIT ON;"
			    "SET QUOTED_IDENTIFIER ON");
		chk(ret, "dbcmd");
		ret = dbsqlexec(dbproc);
		chk(ret, "dbsqlexec");

		ret = dbcancel(dbproc);
		chk(ret, "dbcancel");
	}

	ret = dbrpcinit(dbproc, "testDecimal", 0);
	chk(ret, "dbrpcinit");

	num = (DBDECIMAL *) calloc(1, sizeof(DBDECIMAL));
	ti.scale = 5;
	ti.precision = 16;
	ret = dbconvert_ps(dbproc, SYBVARCHAR, (const BYTE *) "123.45", -1, SYBDECIMAL, (BYTE *) num, sizeof(*num), &ti);
	chk(ret > 0, "dbconvert_ps");

	ret = dbrpcparam(dbproc, "@idecimal", 0, SYBDECIMAL, -1, sizeof(DBDECIMAL), (BYTE *) num);
	chk(ret, "dbrpcparam");
	ret = dbrpcsend(dbproc);
	chk(ret, "dbrpcsend");
	ret = dbsqlok(dbproc);
	chk(ret, "dbsqlok");

	/* TODO check MS/Sybase format */
	num2 = (DBDECIMAL *) calloc(1, sizeof(DBDECIMAL));
	ti.precision = override_prec;
	ti.scale     = override_scale;
	ret = dbconvert_ps(dbproc, SYBVARCHAR, (const BYTE *) "246.9", -1, SYBDECIMAL, (BYTE *) num2, sizeof(*num2), &ti);
	chk(ret > 0, "dbconvert_ps");

	for (i=0; (ret = dbresults(dbproc)) != NO_MORE_RESULTS; ++i) {
		RETCODE row_code;

		switch (ret) {
		case SUCCEED:
			if (DBROWS(dbproc) == FAIL)
				continue;
			assert(DBROWS(dbproc) == SUCCEED);
			printf("dbrows() returned SUCCEED, processing rows\n");

			memset(num, 0, sizeof(*num));
			num->precision = out_prec  ? out_prec  : num2->precision;
			num->scale     = out_scale ? out_scale : num2->scale;
			dbbind(dbproc, 1, bind_type, 0, (BYTE *) num);

			while ((row_code = dbnextrow(dbproc)) != NO_MORE_ROWS) {
				if (row_code == REG_ROW) {
					zero_end(num);
					zero_end(num2);
					if (memcmp(num, num2, sizeof(*num)) != 0) {
						fprintf(stderr, "Failed. Output results does not match\n");
						dump_addr(stderr, "got:      ", num, sizeof(*num));
						dump_addr(stderr, "expected: ", num2, sizeof(*num2));
						exit(1);
					}
				} else {
					/* not supporting computed rows in this unit test */
					fprintf(stderr, "Failed.  Expected a row\n");
					exit(1);
				}
			}
			break;
		case FAIL:
			fprintf(stderr, "dbresults returned FAIL\n");
			exit(1);
		default:
			fprintf(stderr, "unexpected return code %d from dbresults\n", ret);
			exit(1);
		}
	} /* while dbresults */

	sql_cmd(dbproc);

	free(num2);
	free(num);

	dbclose(dbproc);
}

#define test(a,b,c,d,e) test(a, #a, b, c, d, e, __LINE__)

int
main(int argc, char **argv)
{
	read_login_info(argc, argv);

	dbsetversion(DBVERSION_100);
	dbinit();

	/* tests with MS behaviour */
	msdblib = 1;
	test(DECIMALBIND,    20, 10, 0, 0);
	test(NUMERICBIND,    20, 10, 0, 0);
	test(SRCDECIMALBIND, 20, 10, 0, 0);
	test(SRCNUMERICBIND, 20, 10, 0, 0);
	/* in these 2 case MS override what server returns */
	test(DECIMALBIND,    10,  4, 10, 4);
	test(NUMERICBIND,    10,  4, 10, 4);
	test(SRCDECIMALBIND, 20, 10, 10, 4);
	test(SRCNUMERICBIND, 20, 10, 10, 4);

	/* tests with Sybase behaviour */
	msdblib = 0;
	test(DECIMALBIND,    20, 10, 0, 0);
	test(NUMERICBIND,    20, 10, 0, 0);
	test(SRCDECIMALBIND, 20, 10, 0, 0);
	test(SRCNUMERICBIND, 20, 10, 0, 0);
	/* no matter what Sybase return always according to source */
	test(DECIMALBIND,    20, 10, 10, 4);
	test(NUMERICBIND,    20, 10, 10, 4);
	test(SRCDECIMALBIND, 20, 10, 10, 4);
	test(SRCNUMERICBIND, 20, 10, 10, 4);

	chk(sql_reopen("numeric_2"), "sql_reopen");

	msdblib = 1;
	/* on MS use always output */
	test(DECIMALBIND,    20,  0, 20, 0);
	test(NUMERICBIND,    19,  0, 19, 0);
	test(SRCDECIMALBIND, 18,  0, 18, 0);
	test(SRCNUMERICBIND, 17,  0, 17, 0);

	msdblib = 0;
	test(DECIMALBIND,    18,  0, 20, 0);
	test(NUMERICBIND,    18,  0, 19, 0);
	/* this is MS only and behave like MS */
	test(SRCDECIMALBIND, 18,  0, 18, 0);
	test(SRCNUMERICBIND, 17,  0, 17, 0);

	dbexit();

	printf("Succeed\n");
	return 0;
}

