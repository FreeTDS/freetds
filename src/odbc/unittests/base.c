#include "common.h"

/* TODO place comment here */

static char software_version[] = "$Id: base.c,v 1.1 2008-01-28 13:36:07 freddy77 Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char *argv[])
{
	/* TODO remove if not neeeded */
	use_odbc_version3 = 1;
	Connect();

	/* TODO write your test */

	Disconnect();
	return 0;
}
