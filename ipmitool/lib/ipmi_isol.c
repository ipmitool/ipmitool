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
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_isol.h>

extern int verbose;

static int ipmi_isol_setup(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char data[6];	

	/* TEST FOR AVAILABILITY */

	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_ENABLE_PARAM;
	data[2] = ISOL_ENABLE_FLAG;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_ISOL;
	req.msg.cmd = SET_ISOL_CONFIG;
	req.msg.data = data;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in Set ISOL Config Command\n");
		return -1;
	}

	if (rsp->ccode == 0xc1) {
		printf("Serial Over Lan not supported!\n");
		return -1;
	}
	if (rsp->ccode) {
		printf("Set Serial Over Lan Config returned %x\n", rsp->ccode);
		return -1;
	}

	/* GET ISOL CONFIG */

	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_AUTHENTICATION_PARAM;
	data[2] = 0x00;		/* block */
	data[3] = 0x00;		/* selector */
	req.msg.cmd = GET_ISOL_CONFIG;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x in Get ISOL Config command\n", rsp?rsp->ccode:0);
		return -1;
	}

	if (verbose > 1)
		printbuf(rsp->data, rsp->data_len, "ISOL Config");

	/* SET ISOL CONFIG - AUTHENTICATION */

	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_AUTHENTICATION_PARAM;
	data[2] = ISOL_PRIVILEGE_LEVEL_USER | (rsp->data[1] & 0x80);
	req.msg.cmd = SET_ISOL_CONFIG;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x in Set ISOL Config (Authentication) command\n", rsp?rsp->ccode:0);
		return -1;
	}

	/* SET ISOL CONFIG - BAUD RATE */

	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_BAUD_RATE_PARAM;
	data[2] = ISOL_PREFERRED_BAUD_RATE;
	req.msg.cmd = SET_ISOL_CONFIG;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x in Set ISOL Config (Baud Rate) command\n", rsp?rsp->ccode:0);
		return -1;
	}

	return 0;
}

int ipmi_isol_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4)) {
		printf("ISOL Commands:  setup\n");
		return 0;
	}
	else if (!strncmp(argv[0], "setup", 5)) {
		ipmi_isol_setup(intf);
	}
	return 0;
}
