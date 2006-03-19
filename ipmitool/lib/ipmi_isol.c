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
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_isol.h>

const struct valstr ipmi_isol_baud_vals[] = {
	{ ISOL_BAUD_RATE_9600,   "9600" },
	{ ISOL_BAUD_RATE_19200,  "19200" },
	{ ISOL_BAUD_RATE_38400,  "38400" },
	{ ISOL_BAUD_RATE_57600,  "57600" },
	{ ISOL_BAUD_RATE_115200, "115200" },
	{ 0x00, NULL }
};

extern int verbose;

static int ipmi_isol_setup(struct ipmi_intf * intf, char baudsetting)
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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error in Set ISOL Config Command");
		return -1;
	}
	if (rsp->ccode == 0xc1) {
		lprintf(LOG_ERR, "IPMI v1.5 Serial Over Lan (ISOL) not supported!");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Error in Set ISOL Config Command: %s",
			val2str(rsp->ccode, completion_code_vals));
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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Error in Get ISOL Config Command: %s",
			val2str(rsp->ccode, completion_code_vals));
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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error in Set ISOL Config (Authentication) Command");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Error in Set ISOL Config (Authentication) Command: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	/* SET ISOL CONFIG - BAUD RATE */

	memset(data, 0, 6);
	data[0] = 0x00;
	data[1] = ISOL_BAUD_RATE_PARAM;
	data[2] = baudsetting;
	req.msg.cmd = SET_ISOL_CONFIG;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Error in Set ISOL Config (Baud Rate) Command");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Error in Set ISOL Config (Baud Rate) Command: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	printf("Set ISOL Baud Rate to %s\n",
	       val2str(baudsetting, ipmi_isol_baud_vals));

	return 0;
}

int ipmi_isol_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int ret = 0;

	if (argc < 2 || strncmp(argv[0], "help", 4) == 0) {
		lprintf(LOG_NOTICE, "ISOL Commands: setup <baud>");
		lprintf(LOG_NOTICE, "ISOL Baud Rates:  9600, 19200, 38400, 57600, 115200");
		return 0;
	}
		
	if (strncmp(argv[0], "setup", 5) == 0) {
		if (strncmp(argv[1], "9600", 4) == 0) {
			ret = ipmi_isol_setup(intf, ISOL_BAUD_RATE_9600);
		}
		else if (strncmp(argv[1], "19200", 5) == 0) {
			ret = ipmi_isol_setup(intf, ISOL_BAUD_RATE_19200);
		}
		else if (strncmp(argv[1], "38400", 5) == 0) {
			ret = ipmi_isol_setup(intf, ISOL_BAUD_RATE_38400);
		}
		else if (strncmp(argv[1], "57600", 5) == 0) {
			ret = ipmi_isol_setup(intf, ISOL_BAUD_RATE_57600);
		}
		else if (strncmp(argv[1], "115200", 6) == 0) {
			ret = ipmi_isol_setup(intf, ISOL_BAUD_RATE_115200);
		}
		else {
			lprintf(LOG_ERR, "ISOL - Unsupported baud rate: %s", argv[1]);
			ret = -1;
		}
	}
	return ret;
}
