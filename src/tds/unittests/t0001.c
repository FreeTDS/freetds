/* FreeTDS - Library of routines accessing Sybase and Microsoft databases
 * Copyright (C) 1998-1999  Brian Bruns
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "common.h"

static char software_version[] = "$Id: t0001.c,v 1.7 2006-12-15 03:20:30 jklowden Exp $";
static void *no_unused_var_warn[] = { software_version, no_unused_var_warn };

#define TESTING_CAPABILITY 0
#if TESTING_CAPABILITY
#include <assert.h>
#include "enum_cap.h"
static const unsigned char defaultcaps[] = { 
     /* type,  len, data, data, data, data, data, data, data, data, data (9 bytes) */
	0x01, 0x09, 0x00, 0x08, 0x06, 0x6D, 0x7F, 0xFF, 0xFF, 0xFF, 0xFE,
	0x02, 0x09, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x68, 0x00, 0x00, 0x00
};

static const TDS_REQUEST_CAPABILITY request_capabilities[] = 
	{  /* no zero */ TDS_REQ_LANG, TDS_REQ_RPC, TDS_REQ_EVT,
	  TDS_REQ_MSTMT, TDS_REQ_BCP, TDS_REQ_CURSOR, TDS_REQ_DYNF				/* capability.data[8] */
	, TDS_REQ_MSG, TDS_REQ_PARAM, TDS_REQ_DATA_INT1, TDS_REQ_DATA_INT2, 
	  TDS_REQ_DATA_INT4, TDS_REQ_DATA_BIT, TDS_REQ_DATA_CHAR, TDS_REQ_DATA_VCHAR 		/* capability.data[7] */
	, TDS_REQ_DATA_BIN, TDS_REQ_DATA_VBIN, TDS_REQ_DATA_MNY8, TDS_REQ_DATA_MNY4, 
	  TDS_REQ_DATA_DATE8, TDS_REQ_DATA_DATE4, TDS_REQ_DATA_FLT4, TDS_REQ_DATA_FLT8		/* capability.data[6] */
	, TDS_REQ_DATA_NUM, TDS_REQ_DATA_TEXT, TDS_REQ_DATA_IMAGE, TDS_REQ_DATA_DEC, 
	  TDS_REQ_DATA_LCHAR, TDS_REQ_DATA_LBIN, TDS_REQ_DATA_INTN, TDS_REQ_DATA_DATETIMEN	/* capability.data[5] */
	, TDS_REQ_DATA_MONEYN, TDS_REQ_CSR_PREV, TDS_REQ_CSR_FIRST, TDS_REQ_CSR_LAST, 
	  TDS_REQ_CSR_ABS, TDS_REQ_CSR_REL, TDS_REQ_CSR_MULTI					/* capability.data[4] */
	, TDS_REQ_CON_INBAND,                   TDS_REQ_PROTO_TEXT, TDS_REQ_PROTO_BULK, 
	  TDS_REQ_DATA_SENSITIVITY, TDS_REQ_DATA_BOUNDARY					/* capability.data[3] */
	,                           TDS_REQ_DATA_FLTN, TDS_REQ_DATA_BITN			/* capability.data[2] */
	, TDS_REQ_WIDETABLE									/* capability.data[1] */
	};
static const TDS_RESPONSE_CAPABILITY response_capabilities[] = 
	{ TDS_RES_CON_NOOOB
	, TDS_RES_PROTO_NOTEXT
	, TDS_RES_PROTO_NOBULK
	, TDS_RES_NOTDSDEBUG
	, TDS_RES_DATA_NOINT8
	};

/*
 * The TDSLOGIN::capabilities member is a little wrong because it includes the type and typelen members.
 * The 22 bytes are structured as:
 *	offset	name	value	meaning
 *	------	----	-----	--------------------------
 *	  0	type	  1	request
 *	  1	len	  9	9 capability bytes follow
 *	 2-10	data	  
 *	 11	type	  2	response
 *	 12	len	  9	9 capability bytes follow
 *	13-21	data	  
 *
 * This function manipulates the data portion without altering the length.
 * 
 * \param capabilities 	address of the data portion in the TDSLOGIN member to be affected.
 * \param capability 	capability to set or reset.  Pass as negative to reset.  
 */
static unsigned char *
tds_capability_set(unsigned char capabilities[], int capability, size_t len)
{
	int cap = (capability < 0)? -capability : capability;
	int rindex = cap / 8;
	int index = (len - cap/8) - 1;
	unsigned char mask = 1 << ((8+cap) % 8);
	assert(0 < index && index < len);

	if (capability < 0) {
		mask ^= mask;
		capabilities[index] &= mask;
	} else {
		capabilities[index] |= mask;
	}
	fprintf(stderr, "capability: %2d, index %u(%u), mask %02x: %02x (default %02x)\n", 
					cap, index, rindex, mask, capabilities[index], defaultcaps[2+index]);
	return capabilities;
}
#endif /* TESTING_CAPABILITY */

int
main(int argc, char **argv)
{
	TDSLOGIN *login;
	TDSSOCKET *tds;
	int ret;
	int verbose = 0;
#if TESTING_CAPABILITY
	int i, c, *pcap, ncap;
	unsigned char *capabilities[2];
	unsigned char caps[TDS_MAX_CAPABILITY];
	
	memset(caps, 0, TDS_MAX_CAPABILITY);
	capabilities[0] = caps;
	capabilities[1] = caps + TDS_MAX_CAPABILITY / 2;
	pcap = (int*)request_capabilities;
	ncap = TDS_VECTOR_SIZE(request_capabilities);
	for (c=0; c < 2; c++) {
		const int bufsize = TDS_MAX_CAPABILITY / 2 - 2;
		capabilities[c][0] = 1 + c; /* request/response */
		capabilities[c][1] = bufsize;
		for (i=0; i < ncap; i++) {
			tds_capability_set(capabilities[c]+2, pcap[i], bufsize);
		}
		fprintf(stderr, "[%d]\n", ncap);
		pcap = response_capabilities;
		ncap = TDS_VECTOR_SIZE(response_capabilities);
	}
	for (c=0; c < 2; c++) {
		int ncap = sizeof(defaultcaps) / 2;
		for (i=0; i < ncap; i++) {
			fprintf(stderr, "%02x=%02x ", capabilities[c][i], defaultcaps[c * ncap + i]);
		}
		fprintf(stderr, "%d\n", TDS_MAX_CAPABILITY);
	}
	assert(0 == memcmp(caps, defaultcaps, sizeof(defaultcaps)));
#endif /* TESTING_CAPABILITY */

	fprintf(stdout, "%s: Testing login, logout\n", __FILE__);
	ret = try_tds_login(&login, &tds, __FILE__, verbose);
	if (ret != TDS_SUCCEED) {
		fprintf(stderr, "try_tds_login() failed\n");
		return 1;
	}

	try_tds_logout(login, tds, verbose);
	return 0;
}
