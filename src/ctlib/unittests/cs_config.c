#if HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <cspublic.h>
#include <ctpublic.h>
#include "common.h"

static char software_version[] = "$Id: cs_config.c,v 1.1 2003-04-14 03:06:37 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

int
main(int argc, char **argv)
{
	int verbose = 1;
	CS_CONTEXT *ctx,*ctx1;
	CS_CHAR *name1, *name2;
	CS_INT len,len1, ret_len, ret_len2;
	CS_VOID *return_name,*return_name2, *return_name_1, *return_name_2, *return_name_3;

	if (verbose) {
		fprintf(stdout, "Trying cs_config with cs_userdata\n\n");
	}
	if (cs_ctx_alloc(CS_VERSION_100, &ctx) != CS_SUCCEED) {
		fprintf(stderr, "cs_ctx_alloc() for first context failed\n");
	}
	if (ct_init(ctx, CS_VERSION_100) != CS_SUCCEED) {
		fprintf(stderr, "ct_init() for first context failed\n");
	}
	if (cs_ctx_alloc(CS_VERSION_100, &ctx1) != CS_SUCCEED) {
		fprintf(stderr, "cs_ctx_alloc() for 2nd context failed\n");
	}
	if (ct_init(ctx1, CS_VERSION_100) != CS_SUCCEED) {
		fprintf(stderr, "ct_init() for 2nd context failed\n");
	}

	fprintf(stdout, "Testing with first context\n");

	name1="FreeTDS";
	len=8;

	fprintf(stdout, "expected value = %s ", name1);

	if (cs_config(ctx, CS_SET, CS_USERDATA,(CS_VOID *)&name1, len, NULL)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() set failed\n");
		return 1;
	}
	if (cs_config(ctx, CS_GET, CS_USERDATA, &return_name, len, &ret_len)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() get failed\n");
		return 1;
	}

	fprintf(stdout, "returned value = %s\n", (char *)return_name);

	len1=0;

	fprintf(stderr, "expected value = <empty string> ");

	if (cs_config(ctx, CS_SET, CS_USERDATA,(CS_VOID *) &name1, len, NULL)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() set failed\n");
		return 1;
	}
	
	if (cs_config(ctx, CS_GET, CS_USERDATA, &return_name2, len1, &ret_len2)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() get failed\n");
		return 1;
	}

	fprintf(stderr, "returned value = %s \n", (char *)return_name2);

	name1 =NULL;

	if (cs_config(ctx, CS_SET, CS_USERDATA,(CS_VOID *) &name1, len, NULL)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() set failed\n");
		return 1;
	}
	if (cs_config(ctx, CS_GET, CS_USERDATA, &return_name2, len, &ret_len2)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() get failed\n");
		return 1;
	}
	if(return_name2) {
	  fprintf(stderr, "Error: Expecting a NULL value but returned value is = %s\n", (char *)return_name2);
	}
	else {
	  	fprintf(stderr, "expected value is <NULL> returned value is <NULL>\n");
	}

	fprintf(stdout, "Testing with second context\n");

	name2="FPRINTF";
	len=8;

	fprintf(stdout, "expected value = %s ", name2);

	if (cs_config(ctx1, CS_SET, CS_USERDATA,(CS_VOID *)&name2, len, NULL)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() set failed\n");
		return 1;
	}
	if (cs_config(ctx1, CS_GET, CS_USERDATA, &return_name_1, len, &ret_len)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() get failed\n");
		return 1;
	}

	fprintf(stdout, "returned value = %s\n", (char *)return_name_1);
	
	len1=0;

	fprintf(stderr, "expected value = <empty string> ");

	if (cs_config(ctx1, CS_SET, CS_USERDATA,(CS_VOID *) &name2, len, NULL)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() set failed\n");
		return 1;
	}
	
	if (cs_config(ctx1, CS_GET, CS_USERDATA, &return_name_3, len1, &ret_len2)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() get failed\n");
		return 1;
	}

	fprintf(stderr, "returned value = %s \n", (char *)return_name_3);

	name2 =NULL;

	if (cs_config(ctx1, CS_SET, CS_USERDATA,(CS_VOID *) &name2, len, NULL)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() set failed\n");
		return 1;
	}
	if (cs_config(ctx1, CS_GET, CS_USERDATA, &return_name_2, len, &ret_len2)
	    != CS_SUCCEED) {
		fprintf(stderr, "cs_config() get failed\n");
		return 1;
	}
	if(return_name2) {
	  fprintf(stderr, "Error: Expecting a NULL value but returned value is = %s\n", (char *)return_name_2);
	}
	else {
	  	fprintf(stderr, "expected value is <NULL> returned value is <NULL>\n");
	}

return 0;
}
