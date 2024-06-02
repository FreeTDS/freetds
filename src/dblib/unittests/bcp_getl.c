
#include "common.h"

#include <freetds/test_assert.h>

int
main(void)
{
	LOGINREC *c = dblogin();
	BCP_SETL(c, TRUE);
	assert(bcp_getl(c));
	dbloginfree(c);
	return 0;
}
