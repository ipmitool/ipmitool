/*
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 * 
 * You acknowledge that this software is not designed or intended for use
 * in the design, construction, operation or maintenance of any nuclear
 * facility.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_raw.h>

int
ipmi_raw_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t netfn, cmd;
	int i;

	uint8_t data[256];

	if (argc < 2 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "RAW Commands:  raw <netfn> <cmd> [data]");
		return -1;
	}
	else if (argc > sizeof(data))
	{
		printf("Raw command input limit (%d bytes) exceeded\n", sizeof(data));
		return -1;
	}

	netfn = (uint8_t)strtol(argv[0], NULL, 0);
	cmd = (uint8_t)strtol(argv[1], NULL, 0);

	memset(data, 0, sizeof(data));
	memset(&req, 0, sizeof(req));
	req.msg.netfn = netfn;
	req.msg.cmd = cmd;
	req.msg.data = data;

	for (i=2; i<argc; i++) {
		uint8_t val = (uint8_t)strtol(argv[i], NULL, 0);
		req.msg.data[i-2] = val;
		req.msg.data_len++;
	}

	lprintf(LOG_INFO, "RAW REQ (netfn=0x%x cmd=0x%x data_len=%d)",
		req.msg.netfn, req.msg.cmd, req.msg.data_len);

	printbuf(req.msg.data, req.msg.data_len, "RAW REQUEST");

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to send RAW command "
			"(netfn=0x%x cmd=0x%x)",
			req.msg.netfn, req.msg.cmd);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Unable to send RAW command "
			"(netfn=0x%x cmd=0x%x): %s",
			req.msg.netfn, req.msg.cmd,
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_INFO, "RAW RSP (%d bytes)", rsp->data_len);

	/* print the raw response buffer */
	for (i=0; i<rsp->data_len; i++) {
		if (((i%16) == 0) && (i != 0))
			printf("\n");
		printf(" %2.2x", rsp->data[i]);
	}
	printf("\n");

	return 0;
}

