#include <freetds/pushvis.h>

typedef struct odbc_parser odbc_parser;

void
odbc_fatal(odbc_parser *parser, const char *msg, ...)
#ifdef __GNUC__
	__attribute__((noreturn))
#endif
;

const char *odbc_get_tok(char **p);
const char *odbc_get_str(odbc_parser *parser, char **p);

typedef char *(*odbc_read_line_p)(void *param, char *s, size_t size);

odbc_parser *odbc_init_parser(FILE *f);
odbc_parser *odbc_init_parser_func(odbc_read_line_p read_func, void *param);
void odbc_free_parser(odbc_parser *parser);
const char *odbc_get_cmd_line(odbc_parser *parser, char **p, bool *cond);
void odbc_set_bool(odbc_parser *parser, const char *name, bool value);
unsigned int odbc_line_num(odbc_parser *parser);

#include <freetds/popvis.h>
