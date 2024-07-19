
#include "common.h"

TEST_MAIN()
{
	LOGINREC *c = dblogin();
	BCP_SETL(c, TRUE);
	assert(bcp_getl(c));
	dbloginfree(c);
	return 0;
}
