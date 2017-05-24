
#include "common.h"

int
main(void)
{
	LOGINREC *c = dblogin();
	BCP_SETL(c, TRUE);
	assert(bcp_getl(c));
	return 0;
}
