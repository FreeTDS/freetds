#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif /* HAVE_STDLIB_H */

#include "common.h"

static char  software_version[]   = "$Id: t0002.c,v 1.5 2002-10-19 03:02:34 jklowden Exp $";
static void *no_unused_var_warn[] = {software_version, no_unused_var_warn};

int main( int argc, char * argv[] ) 
{ 

int res; 
HSTMT stmt;
SQLCHAR command[512]; 

    setenv("TDSDUMP","",1);

    Connect();

	sprintf(command,"drop table #odbctestdata");
	printf("%s\n",command);
	if( SQLExecDirect( Statement, command, SQL_NTS ) 
		!= SQL_SUCCESS ) { 
		printf( "Unable to execute statement\n" ); 
	} 

	Command(Statement,"create table #odbctestdata (i int)");
	Command(Statement,"insert #odbctestdata values (123)" );

	/* now we allocate another statement, select, get all results
	 * then make another query with first select and drop this statement
	 * result should not disappear (required for DBD::ODBC) */

	if( SQLAllocStmt( Connection, &stmt ) != SQL_SUCCESS ) {
		printf( "Unable to allocate statement\n" );
		CheckReturn();
		exit( 1 );
	}

	Command(stmt,"select * from #odbctestdata where 0=1");

	if (SQLFetch(stmt) != SQL_NO_DATA) {
		printf("Data not expected\n");
		exit(1);
	}

    	res = SQLCloseCursor(stmt);
    	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to close cursr\n");
		CheckReturn();
		exit(1);
	}

	Command(Statement,"select * from #odbctestdata");

	/* drop first statement .. data should not disappear */
	if (SQLFreeStmt(stmt,SQL_DROP) != SQL_SUCCESS) {
		printf("Error dropping??\n");
		exit(1);
	}

	res = SQLFetch(Statement); 
	if( res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO ) { 
	        printf( "Unable to fetch row. Drop of previous statement discard results... bad!\n" ); 
        	CheckReturn(); 
		exit( 1 ); 
	} 

    	res = SQLFetch(Statement); 
    	if( res != SQL_NO_DATA ) { 
		printf( "Unable to fetch row\n" ); 
		CheckReturn(); 
		exit( 1 ); 
	} 
	
	res = SQLCloseCursor(Statement);
	if (!SQL_SUCCEEDED(res)) {
		printf("Unable to close cursr\n");
		CheckReturn();
		exit(1);
	}

	Command(Statement,"drop table #odbctestdata");

	Disconnect();

	printf( "Done.\n" );
	return 0;
} 

