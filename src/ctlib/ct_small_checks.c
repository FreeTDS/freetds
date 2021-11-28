#undef CS_NO_LARGE_IDENTIFIERS
#define CS_NO_LARGE_IDENTIFIERS 1

#include "ct_checks.h"

#undef STRUCTUREA
#undef STRUCTUREB
#define STRUCTUREA CS_DATAFMT
#define STRUCTUREB CS_DATAFMT_INTERNAL

#undef FIELDB
#define FIELDB small.
TDS_DATAFMT_TEST(internal);

#undef FIELDB
#define FIELDB user.
TDS_DATAFMT_TEST(external);

#undef STRUCTUREA
#undef STRUCTUREB
#define STRUCTUREA CS_SERVERMSG
#define STRUCTUREB CS_SERVERMSG_INTERNAL

#undef FIELDB
#define FIELDB small.
TDS_SERVERMSG_TEST(internal_msg);

#undef FIELDB
#define FIELDB user.
TDS_SERVERMSG_TEST(external_msg);
