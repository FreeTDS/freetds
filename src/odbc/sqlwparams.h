//#define FUNC NAME(SQLTest) (P(SQLSMALLINT, x), PCHAR(y) WIDE)


#undef NAME
#undef WIDE
#undef P
#undef PCHAR
#define NAME(a) _ ## a
#define WIDE
#define P(a,b) a b
#define PCHAR(a) SQLCHAR* a
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



#undef FUNC
