//#define FUNC NAME(SQLTest) (P(SQLSMALLINT, x), PCHAR(y) WIDE)

#ifdef ENABLE_ODBC_WIDE

#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#undef PCHARIN
#undef PCHAROUT
#define NAME(a) _ ## a
#define WIDE , int wide
#define P(a,b) a b
#define PCHAR(a) ODBC_CHAR* a
#define PCHARIN(n,t) PCHAR(sz ## n), P(t, cb ## n)
#define PCHAROUT(n,t) PCHAR(sz ## n), P(t, cb ## n ## Max), P(t FAR*, pcb ## n)
static SQLRETURN FUNC;



#undef NAME
#undef WIDE
#undef PCHAR
#define NAME(a) a
#define WIDE
#define PCHAR(a) SQLCHAR* a
SQLRETURN ODBC_PUBLIC ODBC_API FUNC {

#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) _ ## a
#define WIDE ,0
#define P(a,b) b
#define PCHAR(a) (ODBC_CHAR*) a
	return FUNC;
}



#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) a ## W
#define WIDE
#define P(a,b) a b
#define PCHAR(a) SQLWCHAR * a
SQLRETURN ODBC_PUBLIC ODBC_API FUNC {

#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) _ ## a
#define WIDE ,1
#define P(a,b) b
#define PCHAR(a) (ODBC_CHAR*) a
	return FUNC;
}



#undef WIDE
#undef P
#undef PCHAR
#define WIDE , int wide
#define P(a,b) a b
#define PCHAR(a) ODBC_CHAR* a
static SQLRETURN FUNC



#else



#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#undef PCHARIN
#undef PCHAROUT
#define NAME(a) _ ## a
#define WIDE
#define P(a,b) a b
#define PCHAR(a) SQLCHAR* a
#define PCHARIN(n,t) PCHAR(sz ## n), P(t, cb ## n)
#define PCHAROUT(n,t) PCHAR(sz ## n), P(t, cb ## n ## Max), P(t FAR*, pcb ## n)
static SQLRETURN FUNC;



#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) a
#define WIDE
#define P(a,b) a b
#define PCHAR(a) SQLCHAR* a
SQLRETURN ODBC_API FUNC {

#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) _ ## a
#define WIDE
#define P(a,b) b
#define PCHAR(a) a
	return FUNC;
}


#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) _ ## a
#define WIDE
#define P(a,b) a b
#define PCHAR(a) SQLCHAR* a
static SQLRETURN FUNC

#endif


#undef FUNC

