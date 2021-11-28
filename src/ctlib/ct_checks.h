#include <config.h>

#include "ctpublic.h"
#include "ctlib.h"

#define TDS_ATTR(field_name) \
        (sizeof(((STRUCTUREA*)0)->field_name) == sizeof(((STRUCTUREB*)0)->FIELDB field_name) && \
	TDS_OFFSET(STRUCTUREA,field_name) == TDS_OFFSET(STRUCTUREB,FIELDB field_name))

#define TDS_DATAFMT_TEST(test_name) TDS_COMPILE_CHECK(check_ ## test_name,\
	TDS_ATTR(name) &&\
	TDS_ATTR(namelen) &&\
	TDS_ATTR(datatype) &&\
	TDS_ATTR(format) &&\
	TDS_ATTR(maxlength) &&\
	TDS_ATTR(scale) &&\
	TDS_ATTR(precision) &&\
	TDS_ATTR(status) &&\
	TDS_ATTR(count) &&\
	TDS_ATTR(usertype) &&\
	TDS_ATTR(locale))

#define TDS_SERVERMSG_TEST(test_name) TDS_COMPILE_CHECK(check_ ## test_name,\
	TDS_ATTR(msgnumber) &&\
	TDS_ATTR(state) &&\
	TDS_ATTR(severity) &&\
	TDS_ATTR(text) &&\
	TDS_ATTR(textlen) &&\
	TDS_ATTR(svrname) &&\
	TDS_ATTR(svrnlen) &&\
	TDS_ATTR(proc) &&\
	TDS_ATTR(proclen) &&\
	TDS_ATTR(line) &&\
	TDS_ATTR(status) &&\
	TDS_ATTR(sqlstate) &&\
	TDS_ATTR(sqlstatelen))
