#include "common.h"


int main( int argc, char * argv[] ) 
{ 

int res; 
int i;

SQLINTEGER cnamesize; 

SQLCHAR command[512]; 
SQLCHAR output [256];

    tdsdump_open(NULL);

    Connect();

    sprintf(command,"drop table odbctestdata");
    printf("%s\n",command);
    if( SQLExecDirect( Statement, command, SQL_NTS ) 
        != SQL_SUCCESS ) { 
        printf( "Unable to execute statement\n" ); 
    } 

    sprintf(command,"create table odbctestdata ("
                    "col1 varchar(30) not null,"
                    "col2 int not null,"
                    "col3 float not null,"
                    "col4 numeric(18,6) not null,"
                    "col5 datetime not null)" );

    printf("%s\n",command);
    if( SQLExecDirect( Statement, command, SQL_NTS ) 
        != SQL_SUCCESS ) { 
        printf( "Unable to execute statement\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    sprintf(command,"insert odbctestdata values ("
                    "'ABCDEFGHIJKLMNOP',"
                    "123456,"
                    "1234.56,"
                    "123456.78,"
                    "'Sep 11 2001 10:00AM')" );

    printf("%s\n",command);
    if( SQLExecDirect( Statement, command, SQL_NTS ) 
        != SQL_SUCCESS ) { 
        printf( "Unable to execute statement\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    sprintf(command,"select * from odbctestdata");

    printf("%s\n",command);
    if( SQLExecDirect( Statement, command, SQL_NTS ) 
        != SQL_SUCCESS ) { 
        printf( "Unable to execute statement\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    res = SQLFetch(Statement); 
    if( res != SQL_SUCCESS && res != SQL_SUCCESS_WITH_INFO ) { 
        printf( "Unable to fetch row\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    for ( i = 1 ; i < 6; i++ ) {
       if( SQLGetData(Statement, i, SQL_C_CHAR, output, sizeof(output), &cnamesize) 
           != SQL_SUCCESS) { 
           printf( "Unable to get data col %d\n", i ); 
           CheckReturn(); 
           exit( 1 ); 
       } 
   
       printf("output data >%s< len_or_ind = %d\n", output,(int)cnamesize);
       if (cnamesize != strlen(output))
	       return 1;
    }

    res = SQLFetch(Statement); 
    if( res != SQL_NO_DATA ) { 
        printf( "Unable to fetch row\n" ); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    sprintf(command,"drop table odbctestdata");
    printf("%s\n",command);
    if( SQLExecDirect( Statement, command, SQL_NTS ) 
        != SQL_SUCCESS ) { 
        printf( "Unable to drop table odbctestdata \n"); 
        CheckReturn(); 
        exit( 1 ); 
    } 

    Disconnect();

    printf( "Done.\n" );
    return 0;
} 

