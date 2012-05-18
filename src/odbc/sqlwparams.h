#if 0
# define FUNC NAME(SQLTest) (P(SQLSMALLINT, x), PCHAR(y) WIDE) 
#endif

#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#undef PCHARIN
#undef PCHAROUT

#ifdef ENABLE_ODBC_WIDE
#  define WIDE , int wide
#  define PCHAR(a) ODBC_CHAR* a
#else
#  define WIDE
#  define PCHAR(a) SQLCHAR* a
#endif

#define NAME(a) _ ## a
#define P(a,b) a b
#define PCHARIN(n,t) PCHAR(sz ## n), P(t, cb ## n)
#define PCHAROUT(n,t) PCHAR(sz ## n), P(t, cb ## n ## Max), P(t FAR*, pcb ## n)
static SQLRETURN FUNC

#undef FUNC

