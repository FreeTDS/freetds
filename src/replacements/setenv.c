/*
 * setenv/unsetenv
 */

#include <config.h>

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */

#if HAVE_ERRNO_H
#include <errno.h>
#endif /* HAVE_ERRNO_H */

#include <freetds/sysdep_private.h>
#include <freetds/bool.h>
#include <freetds/replacements.h>

#ifndef _WIN32
#error Platform should have setenv/unsetenv
#endif

static bool
check_name(const char *name)
{
	if (!name || !name[0] || strchr(name, '=') != NULL) {
		errno = EINVAL;
		return false;
	}
	return true;
}

static int
set_env_value(const char *name, const char *value)
{
	char *s;
	int res;

	if (asprintf(&s, "%s=%s", name, value ? value : "") < 0) {
		errno = ENOMEM;
		return -1;
	}
	res = _putenv(s);
	free(s);
	return res;
}

int
tds_setenv(const char *name, const char *value, int overwrite)
{
	if (!check_name(name))
		return -1;
	if (!overwrite && getenv(name))
		return 0;
	return set_env_value(name, value);
}

int
tds_unsetenv(const char *name)
{
	if (!check_name(name))
		return -1;
	return set_env_value(name, NULL);
}
