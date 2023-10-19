#include <config.h>

#include <stdio.h>

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <cspublic.h>
#include <ctpublic.h>
#include "common.h"

int
main(void)
{
	int verbose = 1;
	CS_CONTEXT *ctx;

	CS_CHAR string_in[16], string_out[16];
	CS_INT  int_in,        int_out;
	CS_INT ret_len;

	if (verbose) {
		printf("Trying cs_config with CS_USERDATA\n\n");
	}

	check_call(cs_ctx_alloc, (CS_VERSION_100, &ctx));
	check_call(ct_init, (ctx, CS_VERSION_100));

	printf("Testing CS_SET/GET USERDATA with char array\n");

	strcpy(string_in,"FreeTDS");

	check_call(cs_config, (ctx, CS_SET, CS_USERDATA, (CS_VOID *)string_in,  CS_NULLTERM, NULL));
	check_call(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)string_out, 16, &ret_len));

	if (strcmp(string_in, string_out)) {
		printf("returned value >%s< not as stored >%s<\n", (char *)string_out, (char *)string_in);
		return 1;
	}
	if (ret_len != (strlen(string_in) + 1)) {
		printf("returned length >%d< not as expected >%u<\n", ret_len, (unsigned int) (strlen(string_in) + 1));
		return 1;
	}

	printf("Testing CS_SET/GET USERDATA with char array\n");

	strcpy(string_in,"FreeTDS");

	check_call(cs_config, (ctx, CS_SET, CS_USERDATA, (CS_VOID *)string_in,  CS_NULLTERM, NULL));

	strcpy(string_out,"XXXXXXXXXXXXXXX");

	check_call(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)string_out, 4, &ret_len));

	if (strcmp(string_out, "FreeXXXXXXXXXXX")) {
		printf("returned value >%s< not as expected >%s<\n", (char *)string_out, "FreeXXXXXXXXXXX");
		return 1;
	}
	if (ret_len != (strlen(string_in) + 1)) {
		printf("returned length >%d< not as expected >%u<\n", ret_len, (unsigned int) (strlen(string_in) + 1));
		return 1;
	}

	printf("Testing CS_SET/GET USERDATA with int\n");

	int_in = 255;

	check_call(cs_config, (ctx, CS_SET, CS_USERDATA, (CS_VOID *)&int_in,  sizeof(int), NULL));
	check_call(cs_config, (ctx, CS_GET, CS_USERDATA, (CS_VOID *)&int_out, sizeof(int), &ret_len));

	if (int_in != int_out) {
		printf("returned value >%d< not as stored >%d<\n", int_out, int_in);
		return 1;
	}
	if (ret_len != (sizeof(int))) {
		printf("returned length >%d< not as expected >%u<\n", ret_len, (unsigned int) sizeof(int));
		return 1;
	}

	cs_ctx_drop(ctx);

	return 0;
}
