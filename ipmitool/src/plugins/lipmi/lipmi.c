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
#include <sys/stropts.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>

#include <sys/lipmi/lipmi_intf.h>

#define IPMI_LIPMI_DEV		"/dev/lipmi"

extern int verbose;

static int ipmi_lipmi_open(struct ipmi_intf * intf)
{
	intf->fd = open(IPMI_LIPMI_DEV, O_RDWR);
	if (intf->fd < 0) {
		perror("Could not open lipmi device");
		return -1;
	}
	intf->opened = 1;
	return intf->fd;
}

static void ipmi_lipmi_close(struct ipmi_intf * intf)
{
	if (intf && intf->fd >= 0)
		close(intf->fd);
	intf->fd = -1;
	intf->opened = 0;
}

static struct ipmi_rs * ipmi_lipmi_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	struct strioctl istr;
	static struct lipmi_reqrsp reqrsp;
	static struct ipmi_rs rsp;	
	static int curr_seq = 0;

	if (!intf || !req)
		return NULL;

	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

	memset(&reqrsp, 0, sizeof(reqrsp));
	reqrsp.req.fn = req->msg.netfn;
	reqrsp.req.lun = 0;
	reqrsp.req.cmd = req->msg.cmd;
	reqrsp.req.datalength = req->msg.data_len;
	memcpy(reqrsp.req.data, req->msg.data, req->msg.data_len);
	reqrsp.rsp.datalength = RECV_MAX_PAYLOAD_SIZE;

	istr.ic_cmd = IOCTL_IPMI_KCS_ACTION;
	istr.ic_timout = 0;
	istr.ic_dp = (char *)&reqrsp;
	istr.ic_len = sizeof(struct lipmi_reqrsp);

	if (verbose > 1) {
		printf("LIPMI req.fn         : %x\n", reqrsp.req.fn);
		printf("LIPMI req.lun        : %x\n", reqrsp.req.lun);
		printf("LIPMI req.cmd        : %x\n", reqrsp.req.cmd);
		printf("LIPMI req.datalength : %d\n", reqrsp.req.datalength);
	}

	if (ioctl(intf->fd, I_STR, &istr) < 0) {
		perror("LIPMI IOCTL: I_STR");
		return NULL;
	}

	memset(&rsp, 0, sizeof(struct ipmi_rs));
	rsp.ccode = reqrsp.rsp.ccode;
	rsp.data_len = reqrsp.rsp.datalength;

	if (!rsp.ccode && rsp.data_len)
		memcpy(rsp.data, reqrsp.rsp.data, rsp.data_len);

	return &rsp;
}

struct ipmi_intf ipmi_lipmi_intf = {
	name:		"lipmi",
	desc:		"Solaris x86 LIPMI Interface",
	open:		ipmi_lipmi_open,
	close:		ipmi_lipmi_close,
	sendrecv:	ipmi_lipmi_send_cmd,
	target_addr:	IPMI_BMC_SLAVE_ADDR,
};

