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
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_sel.h>

static int
ipmi_send_platform_event(struct ipmi_intf * intf, int num)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[8];

	ipmi_intf_session_set_privlvl(intf, IPMI_SESSION_PRIV_ADMIN);

	memset(&req, 0, sizeof(req));
	memset(rqdata, 0, 8);

	printf("Sending ");

	/* IPMB/LAN/etc */
	switch (num) {
	case 1:			/* temperature */
		printf("Temperature - Upper Critical - Going High");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x01;	/* Sensor Type */
		rqdata[2] = 0x30;	/* Sensor # */
		rqdata[3] = 0x01;	/* Event Dir / Event Type */
		rqdata[4] = 0x59;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	case 2:			/* voltage error */
		printf("Voltage Threshold - Lower Critical - Going Low");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x02;	/* Sensor Type */
		rqdata[2] = 0x60;	/* Sensor # */
		rqdata[3] = 0x01;	/* Event Dir / Event Type */
		rqdata[4] = 0x52;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	case 3:			/* correctable ECC */
		printf("Memory - Correctable ECC");
		rqdata[0] = 0x04;	/* EvMRev */
		rqdata[1] = 0x0c;	/* Sensor Type */
		rqdata[2] = 0x01;	/* Sensor # */
		rqdata[3] = 0x6f;	/* Event Dir / Event Type */
		rqdata[4] = 0x00;	/* Event Data 1 */
		rqdata[5] = 0x00;	/* Event Data 2 */
		rqdata[6] = 0x00;	/* Event Data 3 */
		break;
	default:
		printf("Invalid event number: %d\n", num);
		return -1;
	}

	printf(" event to BMC\n");

	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = 0x02;
	req.msg.data = rqdata;
	req.msg.data_len = 7;


	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Platform Event Message Command\n", rsp?rsp->ccode:0);
		return -1;
	}

	return 0;
}

int ipmi_event_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	unsigned char c;

	if (!argc || !strncmp(argv[0], "help", 4)) {
		printf("usage: event <num>\n");
		printf("   1 : Temperature - Upper Critical - Going High\n");
		printf("   2 : Voltage Threshold - Lower Critical - Going Low\n");
		printf("   3 : Memory - Correctable ECC\n");
	} else {
		c = (unsigned char)strtol(argv[0], NULL, 0);
		ipmi_send_platform_event(intf, c);
	}

	return 0;
}
