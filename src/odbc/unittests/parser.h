#include <freetds/pushvis.h>

extern unsigned int odbc_line_num;

void odbc_fatal(const char *msg, ...);

const char *odbc_get_tok(char **p);
const char *odbc_get_str(char **p);

void odbc_set_bool(const char *name, int value);
void odbc_init_bools(void);
void odbc_clear_bools(void);

void odbc_init_parser(FILE *f);
const char *odbc_get_cmd_line(char **p, int *cond);

#include <freetds/popvis.h>
