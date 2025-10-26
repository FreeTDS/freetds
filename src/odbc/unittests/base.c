#include "common.h"

/* TODO place comment here */

TEST_MAIN()
{
	/* TODO remove if not neeeded */
	odbc_use_version3 = true;
	odbc_connect();

	/* TODO write your test */

	odbc_disconnect();
	return 0;
}
