//#define FUNC NAME(SQLTest) (P(SQLSMALLINT, x), PCHAR(y) WIDE)

#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) a
#define WIDE
#define P(a,b) a b
#define PCHAR(a) SQLCHAR* a
SQLRETURN ODBC_API FUNC

#undef FUNC
