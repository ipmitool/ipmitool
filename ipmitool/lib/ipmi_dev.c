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
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <linux/ipmi.h>
#include <ipmi.h>
#include <ipmi_dev.h>
#include <helper.h>

static int curr_seq;
extern int verbose;

struct ipmi_intf ipmi_dev_intf = {
	.open     = ipmi_dev_open,
	.close    = ipmi_dev_close,
	.sendrecv = ipmi_dev_send_cmd,
};

void ipmi_dev_close(struct ipmi_intf * intf)
{
	if (intf && intf->fd >= 0)
		close(intf->fd);
}

int ipmi_dev_open(struct ipmi_intf * intf, char * dev, int __unused1, char * __unused2)
{
	int i = 0;

	if (!dev)
		intf->fd = open("/dev/ipmi/0", O_RDWR);
	else
		intf->fd = open(dev, O_RDWR);

	if (intf->fd < 0) {
		perror("Could not open ipmi device");
		return -1;
	}

	if (ioctl(intf->fd, IPMICTL_SET_GETS_EVENTS_CMD, &i)) {
		perror("Could not set to get events");
		return -1;
	}

	curr_seq = 0;

	return intf->fd;
}

struct ipmi_rsp * ipmi_dev_send_cmd(struct ipmi_intf * intf, struct ipmi_req * req)
{
	struct ipmi_recv recv;
	struct ipmi_addr addr;
	struct ipmi_system_interface_addr bmc_addr;
	static struct ipmi_rsp rsp;
	fd_set rset;

	if (!req)
		return NULL;

	if (verbose > 2)
		printbuf(req->msg.data, req->msg.data_len, "send_ipmi_cmd_dev");

	FD_ZERO(&rset);
	FD_SET(intf->fd, &rset);

	bmc_addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
	bmc_addr.channel = IPMI_BMC_CHANNEL;
	bmc_addr.lun = 0;
	req->addr = (char *) &bmc_addr;
	req->addr_len = sizeof(bmc_addr);
	req->msgid = curr_seq++;

	if (ioctl(intf->fd, IPMICTL_SEND_COMMAND, req) < 0) {
		printf("Error sending command: %s\n", strerror(errno));
		return NULL;
	}
	if (select(intf->fd+1, &rset, NULL, NULL, NULL) < 0) {
		perror("select");
		return NULL;
	}
	if (!FD_ISSET(intf->fd, &rset)) {
		printf("Error no data available\n");
		return NULL;
	}

	recv.addr = (char *) &addr;
	recv.addr_len = sizeof(addr);
	recv.msg.data = rsp.data;
	recv.msg.data_len = sizeof(rsp.data);

	/* get data */
	if (ioctl(intf->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv) < 0) {
		printf("Error receiving message: %s\n", strerror(errno));
		if (errno != EMSGSIZE)
			return NULL;
	}

	if (verbose > 1) {
		printf("Got message:\n");
		printf("  type      = %d\n", recv.recv_type);
		printf("  channel   = 0x%x\n", addr.channel);
		printf("  msgid     = %ld\n", recv.msgid);
		printf("  netfn     = 0x%x\n", recv.msg.netfn);
		printf("  cmd       = 0x%x\n", recv.msg.cmd);
		if (recv.msg.data && recv.msg.data_len) {
			printf("  data_len  = %d\n", recv.msg.data_len);
			printf("  data      =");
			printbuf(recv.msg.data, recv.msg.data_len, "data");
		}
	}

	rsp.ccode = recv.msg.data[0];
	rsp.data_len = recv.msg.data_len - 1;

	if (!rsp.ccode && rsp.data_len) {
		memmove(rsp.data, rsp.data + 1, rsp.data_len);
		rsp.data[recv.msg.data_len] = 0;
	}

	return &rsp;
}

