#include "common.h"

/* Test for SQLMoreResults */

static void Command(HSTMT stmt, const char* command)
{
	printf("%s\n",command);
	if( SQLExecDirect( stmt, command, SQL_NTS ) 
		!= SQL_SUCCESS ) { 
		printf( "Unable to execute statement\n" ); 
		CheckReturn(); 
		exit( 1 ); 
	}
}

int main( int argc, char * argv[] ) 
{ 

int res; 
int i;

SQLINTEGER cnamesize; 

SQLCHAR command[512]; 
SQLCHAR output [256];

    setenv("TDSDUMP","",1);

    Connect();

	sprintf(command,"drop table #odbctestdata");
	printf("%s\n",command);
	if( SQLExecDirect( Statement, command, SQL_NTS ) 
		!= SQL_SUCCESS ) { 
		printf( "Unable to execute statement\n" ); 
	} 

	Command(Statement,"create table #odbctestdata (i int)");
//	Command(Statement,"insert #odbctestdata values (123)" );

	Command(Statement,"select * from #odbctestdata select * from #odbctestdata");

/*	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Data expected\n");
		exit(1);
	}*/

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_SUCCESS) {
		printf("Expected another recordset\n");
		exit(1);
	}
	printf("Getting next recordset\n");

/*	if (SQLFetch(Statement) != SQL_SUCCESS) {
		printf("Data expected\n");
		exit(1);
	}*/

	if (SQLFetch(Statement) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

	if (SQLMoreResults(Statement) != SQL_NO_DATA) {
		printf("Not expected another recordset\n");
		exit(1);
	}

	/* FIXME why this do not work ?? */
/*	res = SQLCloseCursor(Statement);
	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to close cursor\n");
		CheckReturn();
		exit(1);
	}*/

	Command(Statement,"drop table #odbctestdata");

	Disconnect();

	printf( "Done.\n" );
	return 0;
} 

