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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_bmc.h>

extern int verbose;

static int ipmi_bmc_reset(struct ipmi_intf * intf, int cmd)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = cmd;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in BMC Reset Command\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("BMC Reset Command returned %x\n", rsp->ccode);
		return -1;
	}

	return 0;
}

int ipmi_bmc_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4)) {
		printf("BMC Commands:  reset\n");
		return 0;
	}
	else if (!strncmp(argv[0], "reset", 5)) {
		if (argc < 2 || !strncmp(argv[1], "help", 4)) {
			printf("reset commands: warm, cold\n");
		}
		else if (!strncmp(argv[1], "cold", 4)) {
			ipmi_bmc_reset(intf, BMC_COLD_RESET);
		}
		else if (!strncmp(argv[1], "warm", 4)) {
			ipmi_bmc_reset(intf, BMC_WARM_RESET);
		}
		else {
			printf("reset commands: warm, cold\n");
		}
	}
	return 0;
}
