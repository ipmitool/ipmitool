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
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/poll.h>

#include <linux/ipmi.h>
#include <config.h>

#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>

extern int errno;
int verbose = 0;
int csv_output = 0;

static int enable_event_msg_buffer(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char bmc_global_enables;
	int r;

	/* we must read/modify/write bmc global enables */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x2f;	/* Get BMC Global Enables */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("ERROR:%x Get BMC Global Enables command\n",
		       rsp ? rsp->ccode : 0);
		return -1;
	}

	bmc_global_enables = rsp->data[0] | 0x04;
	req.msg.cmd = 0x2e;	/* Set BMC Global Enables */
	req.msg.data = &bmc_global_enables;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("ERROR:%x Set BMC Global Enables command\n",
		       rsp ? rsp->ccode : 0);
		return -1;
	}

	if (verbose)
		printf("BMC Event Message Buffer enabled.\n");

	return 0;
}

static void read_event(struct ipmi_intf * intf)
{
	struct ipmi_addr addr;
	struct ipmi_recv recv;
	unsigned char data[80];
	int rv;

	recv.addr = (char *) &addr;
	recv.addr_len = sizeof(addr);
	recv.msg.data = data;
	recv.msg.data_len = sizeof(data);

	rv = ioctl(intf->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv);
	if (rv < 0) {
		if (errno == EINTR)
			/* abort */
			return;
		if (errno == EMSGSIZE) {
			/* message truncated */
			recv.msg.data_len = sizeof(data);
		} else {
			printf("ERROR: receiving IPMI message: %s\n",
			       strerror(errno));
			return;
		}
	}

	if (!recv.msg.data || recv.msg.data_len == 0) {
		printf("ERROR: No data in event!\n");
		return;
	}

	if (verbose > 1) {
		printf("  type      = %d\n",   recv.recv_type);
		printf("  channel   = 0x%x\n", addr.channel);
		printf("  msgid     = %ld\n",  recv.msgid);
		printf("  netfn     = 0x%x\n", recv.msg.netfn);
		printf("  cmd       = 0x%x\n", recv.msg.cmd);
		printf("  data_len  = %d\n",   recv.msg.data_len);
		printbuf(recv.msg.data, recv.msg.data_len, "data");
	}

	if (recv.recv_type != IPMI_ASYNC_EVENT_RECV_TYPE) {
		printf("ERROR: Not an event!\n");
		return;
	}

	if (verbose)
		ipmi_sel_print_std_entry_verbose((struct sel_event_record *)recv.msg.data);
	else
		ipmi_sel_print_std_entry((struct sel_event_record *)recv.msg.data);
}

static int do_exit(struct ipmi_intf * intf, int rv)
{
	if (intf)
		intf->close(intf);
	ipmi_intf_exit();
	exit(rv);
}

static void usage(void)
{
	printf("usage: ipmievd [-hv]\n");
	printf("\n");
	printf("       -h        This help\n");
	printf("       -v        Verbose (can use multiple times)\n");
	printf("\n");
}

int main(int argc, char ** argv)
{
	struct ipmi_intf * intf;
	int r, i=1, a;
	struct pollfd pfd;

	while ((a = getopt(argc, (char **)argv, "hv")) != -1) {
		switch (a) {
		case 'h':
			usage();
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}

	if (verbose)
		printf("Loading OpenIPMI interface plugin.\n");

	/* init interface plugin support */
	r = ipmi_intf_init();
	if (r < 0) {
		printf("ERROR: Unable to initialize plugin interface\n");
		exit(EXIT_FAILURE);
	}

	/* load interface plugin */
	intf = ipmi_intf_load("intf_open");
	if (!intf) {
		printf("ERROR: unable to load OpenIPMI interface plugin\n");
		exit(EXIT_FAILURE);
	}

	if (verbose)
		printf("Connecting to OpenIPMI device.\n");

	/* open connection to openipmi device */
	r = intf->open(intf, NULL, 0, NULL, NULL);
	if (r < 0) {
		printf("ERROR: Unable to open OpenIPMI device\n");
		exit(EXIT_FAILURE);
	}

	/* enable event message buffer */
	r = enable_event_msg_buffer(intf);
	if (r < 0) {
		printf("ERROR: Unable to enable event message buffer.\n");
		do_exit(intf, EXIT_FAILURE);
	}

	if (verbose)
		printf("Enabling event receiver.\n");

	/* enable OpenIPMI event receiver */
	if (ioctl(intf->fd, IPMICTL_SET_GETS_EVENTS_CMD, &i)) {
		perror("ERROR: Could not enable event receiver");
		do_exit(intf, EXIT_FAILURE);
	}

	printf("ipmievd loaded, waiting for events...\n");

	for (;;) {
		pfd.fd = intf->fd;	/* wait on openipmi device */
		pfd.events = POLLIN;	/* wait for input */
		r = poll(&pfd, 1, -1);

		switch (r) {
		case 0:
			/* timeout is disabled */
			break;
		case -1:
			perror("ERROR: poll operation failed");
			do_exit(intf, EXIT_FAILURE);
			break;
		default:
			if (pfd.revents && POLLIN)
				read_event(intf);
		}
	}

	if (verbose)
		printf("Exiting.\n");

	do_exit(intf, EXIT_SUCCESS);
}
