#if 0
# ODBC_FUNC(SQLTest, (P(SQLSMALLINT, x), PCHAR(y) WIDE))
#endif

#undef WIDE
#undef P
#undef PCHAR
#undef PCHARIN
#undef PCHAROUT

#ifdef ENABLE_ODBC_WIDE
#  define WIDE , int wide
#  define PCHAR(name) ODBC_CHAR* name
#else
#  define WIDE
#  define PCHAR(name) SQLCHAR* name
#endif

/** Generic parameter */
#define P(type, name) type name
/** Input character parameter */
#define PCHARIN(name, len_type) \
	PCHAR(sz ## name), P(len_type, cb ## name)
/** Output character parameter */
#define PCHAROUT(name, len_type) \
	PCHAR(sz ## name), P(len_type, cb ## name ## Max), P(len_type FAR*, pcb ## name)

#define ODBC_FUNC(name, params) \
	static SQLRETURN odbc_ ## name params

