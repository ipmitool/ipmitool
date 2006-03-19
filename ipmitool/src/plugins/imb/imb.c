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

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>

#include "imbapi.h"

#define IPMI_IMB_TIMEOUT	(1000 * 1000)
#define IPMI_IMB_MAX_RETRY	3
#define IPMI_IMB_DEV		"/dev/imb"
#define IPMI_IMB_BUF_SIZE	64

extern int verbose;

static int ipmi_imb_open(struct ipmi_intf * intf)
{
	struct stat stbuf;

	if (stat(IPMI_IMB_DEV, &stbuf) < 0) {
		printf("Error: no IMB driver found at %s!\n", IPMI_IMB_DEV);
		return -1;
	}
		
	intf->opened = 1;

	return 0;
}

static void ipmi_imb_close(struct ipmi_intf * intf)
{
	intf->opened = 0;
}

static struct ipmi_rs * ipmi_imb_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	IMBPREQUESTDATA imbreq;
	static struct ipmi_rs rsp;	
	int status, i;
	unsigned char ccode;

	imbreq.rsSa	= IPMI_BMC_SLAVE_ADDR;
	imbreq.rsLun	= 0;
	imbreq.busType	= 0;
	imbreq.netFn	= req->msg.netfn;
	imbreq.cmdType	= req->msg.cmd;

	imbreq.data = req->msg.data;
	imbreq.dataLength = req->msg.data_len;

	if (verbose > 1) {
		printf("IMB rsSa       : %x\n", imbreq.rsSa);
		printf("IMB netFn      : %x\n", imbreq.netFn);
		printf("IMB cmdType    : %x\n", imbreq.cmdType);
		printf("IMB dataLength : %d\n", imbreq.dataLength);
	}

	rsp.data_len = IPMI_IMB_BUF_SIZE;
	memset(rsp.data, 0, rsp.data_len);

	for (i=0; i<IPMI_IMB_MAX_RETRY; i++) {
		if (verbose > 2)
			printbuf(imbreq.data, imbreq.dataLength, "ipmi_imb request");
		status = SendTimedImbpRequest(&imbreq, IPMI_IMB_TIMEOUT,
					      rsp.data, &rsp.data_len, &ccode);
		if (status == 0) {
			if (verbose > 2)
				printbuf(rsp.data, rsp.data_len, "ipmi_imb response");
			break;
		}
		/* error */
		printf("Error sending IMB request, status=%x ccode=%x\n",
		       status, ccode);
	}

	rsp.ccode = ccode;

	return &rsp;
}

struct ipmi_intf ipmi_imb_intf = {
	name:		"imb",
	desc:		"Intel IMB Interface",
	open:		ipmi_imb_open,
	close:		ipmi_imb_close,
	sendrecv:	ipmi_imb_send_cmd,
	target_addr:	IPMI_BMC_SLAVE_ADDR,
};

