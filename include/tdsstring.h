#ifndef _tdsstring_h_
#define _tdsstring_h_

extern char tds_str_empty[];

/* TODO do some function and use inline if available */

/** \addtogroup dstring
 *  \@{ 
 */

/** init a string with empty */
#define tds_dstr_init(s) \
	{ *(s) = tds_str_empty; }

void tds_dstr_zero(char **s);
void tds_dstr_free(char **s);

char* tds_dstr_copy(char **s,const char* src);
char* tds_dstr_copyn(char **s,const char* src,unsigned int length);
char* tds_dstr_set(char **s,char *src);

/** test if string is empty */
#define tds_dstr_isempty(s) \
	(**(s) == '\0')

/** \@} */

#endif /* _tdsstring_h_ */

