#ifndef _tdsstr_h_
#define _tdsstr_h_

extern char tds_str_empty[];

/* TODO do some function and use inline if available */

/** init a string with empty */
#define tds_dstr_init(s) \
	{ *(s) = tds_str_empty; }

/** clear all string filling with zeroes (mainly for security reason) */
#define tds_dstr_zero(s) \
	{ char **p = (s); if (*p) memset(*p,0,strlen(*p)); }

/** free string */
#define tds_dstr_free(s) \
	{ char **p = (s); if (*p != tds_str_empty) free(*p); }

/** copy a string from another */
#define tds_dstr_copy(s,src) \
	{ char **p = (s); \
	if (*p != tds_str_empty) free(*p); \
	*p = strdup(src); }

/** set a string from another buffer */
#define tds_dstr_set(s,src) \
	{ char **p = (s); \
	if (*p != tds_str_empty) free(*p); \
	*p = (src); }

/** test if string is empty */
#define tds_dstr_isempty(s) \
	(**(s) == '\0')

#endif /* _tdsstr_h_ */

