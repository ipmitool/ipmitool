/*
 * Copyright (c) 2004 Sun Microsystems, Inc.  All Rights Reserved.
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


/*
 * interface routines between ipmitool and the bmc kernel driver
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
#include "bmc_intf.h"

#include "bmc.h"

static int	curr_seq;
extern int	verbose;

struct ipmi_intf ipmi_bmc_intf = {
	name:		"bmc",
	desc:		"IPMI v2.0 BMC interface",
	open:		ipmi_bmc_open,
	close:		ipmi_bmc_close,
	sendrecv:	ipmi_bmc_send_cmd};

void
ipmi_bmc_close(struct ipmi_intf *intf)
{
	if (intf && intf->fd >= 0)
		close(intf->fd);

	intf->opened = 0;
	intf->fd = -1;
}

int
ipmi_bmc_open(struct ipmi_intf *intf)
{
	if (!intf)
                return -1;

	/* Open local device */
	intf->fd = open(BMC_DEV, O_RDWR);

	if (intf->fd <= 0) {
		perror("Could not open bmc device");
		return (-1);
	}
	curr_seq = 0;

	intf->opened = 1;
	return (intf->fd);
}

struct ipmi_rs *
ipmi_bmc_send_cmd(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	struct strioctl istr;
	static struct bmc_reqrsp reqrsp;
	static struct ipmi_rs rsp;

	/* If not already opened open the device or network connection */
	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

	memset(&reqrsp, 0, sizeof (reqrsp));
	reqrsp.req.fn = req->msg.netfn;
	reqrsp.req.lun = 0;
	reqrsp.req.cmd = req->msg.cmd;
	reqrsp.req.datalength = req->msg.data_len;
	memcpy(reqrsp.req.data, req->msg.data, req->msg.data_len);
	reqrsp.rsp.datalength = RECV_MAX_PAYLOAD_SIZE;

	istr.ic_cmd = IOCTL_IPMI_KCS_ACTION;
	istr.ic_timout = 0;
	istr.ic_dp = (char *)&reqrsp;
	istr.ic_len = sizeof (struct bmc_reqrsp);

	if (verbose) {
		printf("BMC req.fn         : %x\n", reqrsp.req.fn);
		printf("BMC req.lun        : %x\n", reqrsp.req.lun);
		printf("BMC req.cmd        : %x\n", reqrsp.req.cmd);
		printf("BMC req.datalength : %d\n", reqrsp.req.datalength);
	}
	if (ioctl(intf->fd, I_STR, &istr) < 0) {
		perror("BMC IOCTL: I_STR");
		return (NULL);
	}
	memset(&rsp, 0, sizeof (struct ipmi_rs));
	rsp.ccode = reqrsp.rsp.ccode;
	rsp.data_len = reqrsp.rsp.datalength;

	/* Decrement for sizeof lun, cmd and ccode */
	rsp.data_len -= 3;

	if (!rsp.ccode && (rsp.data_len > 0))
		memcpy(rsp.data, reqrsp.rsp.data, rsp.data_len);

	return (&rsp);
}
