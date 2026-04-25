#ifndef _tdsguard_cExsdJWxoVtc74CtjdQri5_
#define _tdsguard_cExsdJWxoVtc74CtjdQri5_

#include <freetds/utils/test_base.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#ifdef _WIN32
#include <process.h>
#define EXE_SUFFIX ".exe"
#else
#define EXE_SUFFIX ""
#endif

/* replace all space sequences with one space */
void normalize_spaces(char *s);

/* read a file and output on a stream */
void cat(const char *fn, FILE * out);

/* read a text file into memory, return it as a string */
char *read_file(const char *fn);

char *quote_arg(char *dest, char *const dest_end, const char *arg);

char *add_string(char *dest, char *const dest_end, const char *str);

char *add_server(char *dest, char *const dest_end);

/* Add ".." to the environment PATH */
void update_path(void);

/* Execute tsql command passing input_data */
void tsql(const char *input_data);

/* Execute tsql command passing input_data, return output */
char *tsql_out(const char *input_data);

/* Return filename to use as input */
const char *input_fn(void);

/* Return filename to use as output */
const char *output_fn(void);

#endif
