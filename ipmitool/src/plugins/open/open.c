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
#include <ipmitool/log.h>

#include <config.h>

#ifdef HAVE_OPENIPMI_H
# include <linux/ipmi.h>
#else
# include "open.h"
#endif

extern int verbose;

#define IPMI_OPENIPMI_DEV	"/dev/ipmi0"

static int ipmi_openipmi_open(struct ipmi_intf * intf)
{
	int i = 0;

	intf->fd = open(IPMI_OPENIPMI_DEV, O_RDWR);

	if (intf->fd < 0) {
		lperror(LOG_ERR, "Could not open ipmi device");
		return -1;
	}

	if (ioctl(intf->fd, IPMICTL_SET_GETS_EVENTS_CMD, &i)) {
		lperror(LOG_ERR, "Could not set to get events");
		return -1;
	}

	if (intf->my_addr) {
		unsigned int a = intf->my_addr;
		if (ioctl(intf->fd, IPMICTL_SET_MY_ADDRESS_CMD, &a)) {
			lperror(LOG_ERR, "Could not set IPMB address");
			return -1;
		}
		lprintf(LOG_DEBUG, "Set my IPMB address to 0x%x", intf->my_addr);
	}

	intf->opened = 1;

	return intf->fd;
}

static void ipmi_openipmi_close(struct ipmi_intf * intf)
{
	if (intf && intf->fd >= 0)
		close(intf->fd);
	intf->opened = 0;
}

static struct ipmi_rs *ipmi_openipmi_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	struct ipmi_recv recv;
	struct ipmi_addr addr;
	struct ipmi_system_interface_addr bmc_addr = {
		addr_type:	IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
		channel:	IPMI_BMC_CHANNEL,
	};
	struct ipmi_ipmb_addr ipmb_addr = {
		addr_type:	IPMI_IPMB_ADDR_TYPE,
	};
	struct ipmi_req _req;
	static struct ipmi_rs rsp;
	static int curr_seq = 0;
	fd_set rset;

	if (!intf || !req)
		return NULL;
	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

	if (verbose > 2)
		printbuf(req->msg.data, req->msg.data_len, "send_cmd");

	/*
	 * setup and send message
	 */

	memset(&_req, 0, sizeof(struct ipmi_req));

	if (intf->target_addr &&
	    intf->target_addr != intf->my_addr) {
		/* use IPMB address if needed */
		ipmb_addr.slave_addr = intf->target_addr;
		_req.addr = (char *) &ipmb_addr;
		_req.addr_len = sizeof(ipmb_addr);
		lprintf(LOG_DEBUG, "Sending request to IPMB target @ 0x%x",
			intf->target_addr);
	} else {
		/* otherwise use system interface */
		lprintf(LOG_DEBUG, "Sending request to System Interface");
		_req.addr = (char *) &bmc_addr;
		_req.addr_len = sizeof(bmc_addr);
	}

	_req.msgid = curr_seq++;
	_req.msg.netfn = req->msg.netfn;
	_req.msg.cmd = req->msg.cmd;
	_req.msg.data = req->msg.data;
	_req.msg.data_len = req->msg.data_len;

	if (ioctl(intf->fd, IPMICTL_SEND_COMMAND, &_req) < 0) {
		lperror(LOG_ERR, "Error sending command");
		return NULL;
	}

	/*
	 * wait for and retrieve response
	 */

	FD_ZERO(&rset);
	FD_SET(intf->fd, &rset);

	if (select(intf->fd+1, &rset, NULL, NULL, NULL) < 0) {
		lperror(LOG_ERR, "Error doing select");
		return NULL;
	}
	if (!FD_ISSET(intf->fd, &rset)) {
		lprintf(LOG_ERR, "Error no data available");
		return NULL;
	}

	recv.addr = (char *) &addr;
	recv.addr_len = sizeof(addr);
	recv.msg.data = rsp.data;
	recv.msg.data_len = sizeof(rsp.data);

	/* get data */
	if (ioctl(intf->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv) < 0) {
		lperror(LOG_ERR, "Error receiving message");
		if (errno != EMSGSIZE)
			return NULL;
	}

	if (verbose > 4) {
		fprintf(stderr, "Got message:");
		fprintf(stderr, "  type      = %d\n", recv.recv_type);
		fprintf(stderr, "  channel   = 0x%x\n", addr.channel);
		fprintf(stderr, "  msgid     = %ld\n", recv.msgid);
		fprintf(stderr, "  netfn     = 0x%x\n", recv.msg.netfn);
		fprintf(stderr, "  cmd       = 0x%x\n", recv.msg.cmd);
		if (recv.msg.data && recv.msg.data_len) {
			fprintf(stderr, "  data_len  = %d\n", recv.msg.data_len);
			fprintf(stderr, "  data      = %s\n",
				buf2str(recv.msg.data, recv.msg.data_len));
		}
	}

	/* save completion code */
	rsp.ccode = recv.msg.data[0];
	rsp.data_len = recv.msg.data_len - 1;

	/* save response data for caller */
	if (!rsp.ccode && rsp.data_len) {
		memmove(rsp.data, rsp.data + 1, rsp.data_len);
		rsp.data[recv.msg.data_len] = 0;
	}

	return &rsp;
}

struct ipmi_intf ipmi_open_intf = {
	name:		"open",
	desc:		"Linux OpenIPMI Interface",
	open:		ipmi_openipmi_open,
	close:		ipmi_openipmi_close,
	sendrecv:	ipmi_openipmi_send_cmd,
	target_addr:	IPMI_BMC_SLAVE_ADDR,
};

