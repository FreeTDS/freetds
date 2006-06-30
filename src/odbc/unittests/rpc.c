/* 
 * Purpose: Test remote procedure calls
 * Functions:  
 */

#include "common.h"
#include <assert.h>

static char software_version[] = "$Id: rpc.c,v 1.1 2006-06-30 14:34:47 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

static const char procedure_sql[] = 
		"CREATE PROCEDURE %s \n"
			"  @null_input varchar(30) OUTPUT \n"
			", @first_type varchar(30) OUTPUT \n"
			", @nullout int OUTPUT\n"
			", @nrows int OUTPUT \n"
			", @c varchar(20)\n"
		"AS \n"
		"BEGIN \n"
			"select @null_input = max(convert(varchar(30), name)) from systypes \n"
			"select @first_type = min(convert(varchar(30), name)) from systypes \n"
			"select name from sysobjects where 0=1\n"
			"select distinct convert(varchar(30), name) as 'type'  from systypes \n"
				"where name in ('int', 'char', 'text') \n"
			"select @nrows = @@rowcount \n"
			"select distinct convert(varchar(30), name) as name  from sysobjects where type = 'S' \n"
			"return 42 \n"
		"END \n";


static int
init_proc(const char *name)
{
	static char cmd[4096];

	if (name[0] != '#') {
		fprintf(stdout, "Dropping procedure %s\n", name);
		sprintf(cmd, "if exists (select 1 from sysobjects where name = '%s' and type = 'P') "
				"DROP PROCEDURE %s", name, name);
		if (!SQL_SUCCEEDED(SQLExecDirect(Statement, (SQLCHAR *) cmd, SQL_NTS))) {
			CheckReturn();
			exit(1);
		}
	}

	fprintf(stdout, "Creating procedure %s\n", name);
	sprintf(cmd, procedure_sql, name);

	if (!SQL_SUCCEEDED(SQLExecDirect(Statement, (SQLCHAR *) cmd, SQL_NTS))) {
		if (name[0] == '#')
			fprintf(stdout, "Failed to create procedure %s. Wrong permission or not MSSQL.\n", name);
		else
			fprintf(stdout, "Failed to create procedure %s. Wrong permission.\n", name);
		CheckReturn();
		exit(1);
	}
	return 0;
}

static void
Test(const char *name)
{
	unsigned char sqlstate[6], msg[256];
	SQLRETURN erc;
	int ipar=0, nrows=0;
	HSTMT stmt;
	char *call_cmd;
	struct Argument { 
                SQLSMALLINT       InputOutputType;  /* fParamType */
                SQLSMALLINT       ValueType;        /* fCType */
                SQLSMALLINT       ParameterType;    /* fSqlType */
                SQLUINTEGER       ColumnSize;       /* cbColDef */
                SQLSMALLINT       DecimalDigits;    /* ibScale */
                SQLPOINTER        ParameterValuePtr;/* rgbValue */
                SQLINTEGER        BufferLength;     /* cbValueMax */
                SQLINTEGER        ind;              /* pcbValue */
	};
	struct Argument args[] = {
		/* InputOutputType 	  ValueType   ParamType    ColumnSize 
								    | DecimalDigits 
								    |  | ParameterValuePtr 
								    |  |  |  BufferLength 
								    |  |  |   |	 ind */
		{ SQL_PARAM_OUTPUT,       SQL_C_LONG, SQL_INTEGER,  0, 0, 0,  4, 3 }, /* return status */
		{ SQL_PARAM_INPUT_OUTPUT, SQL_C_CHAR, SQL_VARCHAR, 30, 0, 0, 30, SQL_NULL_DATA }, 
										      /* @null_input varchar(30) OUTPUT */
		{ SQL_PARAM_INPUT_OUTPUT, SQL_C_CHAR, SQL_VARCHAR, 30, 0, 0, 30, 3 }, /* @first_type varchar(30) OUTPUT */
		{ SQL_PARAM_INPUT_OUTPUT, SQL_C_LONG, SQL_INTEGER,  0, 0, 0,  4, 4 }, /* @nullout int OUTPUT\ */
		{ SQL_PARAM_INPUT_OUTPUT, SQL_C_LONG, SQL_INTEGER,  0, 0, 0,  4, 4 }, /* @nrows int OUTPUT */
		{ SQL_PARAM_INPUT,        SQL_C_CHAR, SQL_VARCHAR, 20, 0, 0, 20, 3 }  /* @c varchar(20) */
	};


	printf("executing SQLAllocStmt\n");
	if (SQLAllocStmt(Connection, &stmt) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to allocate statement");

	for( ipar=0; ipar < sizeof(args)/sizeof(args[0]); ipar++ ) {
		printf("executing SQLBindParameter for parameter %d\n", 1+ipar);
		if( args[ipar].BufferLength > 0 ) {
			args[ipar].ParameterValuePtr = (SQLPOINTER) malloc(args[ipar].BufferLength);
			assert(args[ipar].ParameterValuePtr != NULL);
		}
		erc = SQLBindParameter	( stmt, 1+ipar
					, args[ipar].InputOutputType
					, args[ipar].ValueType
					, args[ipar].ParameterType
					, args[ipar].ColumnSize
					, args[ipar].DecimalDigits
					, args[ipar].ParameterValuePtr
					, args[ipar].BufferLength
					, &args[ipar].ind
					);
		if (erc != SQL_SUCCESS) {
			fprintf(stderr, "Failed: SQLBindParameter\n");
			exit(1);
		}
	}
		


	asprintf(&call_cmd, "{?=call %s(?,?,?,?,?)}", name );
	printf("executing SQLPrepare: %s\n", call_cmd);
	assert(call_cmd != NULL);
	if (SQLPrepare(stmt, (SQLCHAR *) call_cmd, SQL_NTS) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to prepare statement");

	printf("executing SQLExecute\n");
	if (! SQL_SUCCEEDED(SQLExecute(stmt))) {
		erc = SQLGetDiagRec(SQL_HANDLE_STMT, stmt, 1, sqlstate, NULL, msg, sizeof(msg), NULL);
		fprintf(stderr, "SQL error %s -- %s\n", sqlstate, msg);
		ODBC_REPORT_ERROR("Unable to execute SQLExecute\n");
		
	}

	printf("executing SQLFetch...\n");
	while ((erc = SQLFetch(stmt)) == SQL_SUCCESS) {
		const SQLINTEGER icol = 1;
		char buf[60];
		SQLINTEGER len;
		erc = SQLGetData( stmt
				, icol
				, SQL_C_CHAR	/* fCType */
				, buf		/* rgbValue */
				, sizeof(buf)	/* cbValueMax */
                		, &len		/* pcbValue */	
				);
		printf("%d bytes fetched:\t%30s\n", (int)len, buf);
		nrows++;
		
	}
	if (erc != SQL_NO_DATA_FOUND) {
		ODBC_REPORT_ERROR("Unable to execute SQLFetch\n");
	} else {
		printf("done.\n");
	}
	if( nrows < 15 ) {
		printf("error: expected at least 15 rows to be returned, found %d\n", nrows);
		exit(1);
	}

	for( ipar=0; ipar < sizeof(args)/sizeof(args[0]); ipar++ ) {
		printf("bound data for parameter %d is %d bytes: ", 1+ipar, (int)args[ipar].ind);
		switch( args[ipar].ValueType ) {
		case SQL_C_LONG:
			printf("%d.\n", args[ipar].ParameterValuePtr);
			break;
		case SQL_C_CHAR:
			printf("'%s'.\n", args[ipar].ParameterValuePtr);
			break;
		default:
			printf("type unsupported in this test\n");
			assert(0);
			break;
		}
	}

	printf("executing SQLFreeStmt\n");
	if (SQLFreeStmt(stmt, SQL_DROP) != SQL_SUCCESS)
		ODBC_REPORT_ERROR("Unable to free statement");
}

int
main(int argc, char *argv[])
{
	const char proc_name[] = "freetds_odbc_rpc_test";
	char drop_proc[256] = "DROP PROCEDURE ";
	
	strcat(drop_proc, proc_name);
	
	printf("connecting\n");
	Connect();
	
	init_proc(proc_name);

	printf("running test\n");
	Test(proc_name);
	
	printf("dropping procedure\n");
	if (!SQL_SUCCEEDED(SQLExecDirect(Statement, (SQLCHAR *) drop_proc, SQL_NTS))) {
		printf("Unable to drop procedure\n");
		CheckReturn();
		exit(1);
	}


	Disconnect();

	printf("Done.\n");
	return 0;
}

