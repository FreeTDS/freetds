#include "common.h"

/* TODO place comment here */

int
main(void)
{
	/* TODO remove if not neeeded */
	odbc_use_version3 = 1;
	odbc_connect();

	/* TODO write your test */

	odbc_disconnect();
	return 0;
}
