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

#include <config.h>

#ifdef HAVE_OPENIPMI_H
# include <linux/ipmi.h>
#else
# include "plugins/open/open.h"
#endif

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>

extern int errno;
int verbose = 0;
int csv_output = 0;

static void daemonize(void)
{
	pid_t pid;
	int fd;
	sigset_t sighup;

	/* if we are started from init no need to become daemon */
	if (getppid() == 1)
		return;

#ifdef SIGHUP
	sigemptyset(&sighup);
	sigaddset(&sighup, SIGHUP);
	if (sigprocmask(SIG_UNBLOCK, &sighup, nil) < 0)
		fprintf(stderr, "ERROR: could not unblock SIGHUP signal\n");
	SIG_IGNORE(SIGHUP);
#endif
#ifdef SIGTTOU
	SIG_IGNORE(SIGTTOU);
#endif
#ifdef SIGTTIN
	SIG_IGNORE(SIGTTIN);
#endif
#ifdef SIGQUIT
	SIG_IGNORE(SIGQUIT);
#endif
#ifdef SIGTSTP
	SIG_IGNORE(SIGTSTP);
#endif

	pid = (pid_t) fork();
	if (pid < 0 || pid > 0)
		exit(0);
	
#if defined(SIGTSTP) && defined(TIOCNOTTY)
	if (setpgid(0, getpid()) == -1)
		exit(1);
	if ((fd = open(_PATH_TTY, O_RDWR)) >= 0) {
		ioctl(fd, TIOCNOTTY, NULL);
		close(fd);
	}
#else
	if (setpgrp() == -1)
		exit(1);
	pid = (pid_t) fork();
	if (pid < 0 || pid > 0)
		exit(0);
#endif

	chdir("/");
	umask(0);

	for (fd=0; fd<64; fd++)
		close(fd);

	open("/dev/null", O_RDWR);
	dup(0);
	dup(0);
}

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
		lprintf(LOG_WARNING, "Get BMC Global Enables command filed [ccode %02x]",
		       rsp ? rsp->ccode : 0);
		return -1;
	}

	bmc_global_enables = rsp->data[0] | 0x04;
	req.msg.cmd = 0x2e;	/* Set BMC Global Enables */
	req.msg.data = &bmc_global_enables;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		lprintf(LOG_WARNING, "Set BMC Global Enables command failed [ccode %02x]",
		       rsp ? rsp->ccode : 0);
		return -1;
	}

	lprintf(LOG_DEBUG, "BMC Event Message Buffer enabled");

	return 0;
}

static void log_event(struct sel_event_record * evt)
{
	char *desc;

	if (!evt)
		return;

	if (evt->record_type == 0xf0)
		lprintf(LOG_ALERT, "Linux kernel panic: %.11s", (char *) evt + 5);
	else if (evt->record_type >= 0xc0)
		lprintf(LOG_NOTICE, "IPMI Event OEM Record %02x", evt->record_type);
	else {
		ipmi_get_event_desc(evt, &desc);
		if (desc) {
			lprintf(LOG_NOTICE, "%s Sensor %02x - %s",
				ipmi_sel_get_sensor_type(evt->sensor_type),
				evt->sensor_num, desc);
			free(desc);
		}
	}
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
		switch (errno) {
		case EINTR:
			return; /* abort */
		case EMSGSIZE:
			recv.msg.data_len = sizeof(data); /* truncated */
			break;
		default:
			lperror(LOG_ERR, "Unable to receive IPMI message");
			return;
		}
	}

	if (!recv.msg.data || recv.msg.data_len == 0) {
		lprintf(LOG_ERR, "No data in event");
		return;
	}
	if (recv.recv_type != IPMI_ASYNC_EVENT_RECV_TYPE) {
		lprintf(LOG_ERR, "Type %x is not an event", recv.recv_type);
		return;
	}

	lprintf(LOG_DEBUG, "netfn:%x cmd:%x ccode:%d",
	    recv.msg.netfn, recv.msg.cmd, recv.msg.data[0]);

	log_event((struct sel_event_record *)recv.msg.data);
}

static int do_exit(struct ipmi_intf * intf, int rv)
{
	if (intf)
		intf->close(intf);
	log_halt();
	exit(rv);
}

static void usage(void)
{
	fprintf(stderr, "usage: ipmievd [-hvd]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "       -h        This help\n");
	fprintf(stderr, "       -v        Verbose (can use multiple times)\n");
	fprintf(stderr, "       -s        Do NOT daemonize\n");
	fprintf(stderr, "\n");
}

int main(int argc, char ** argv)
{
	struct ipmi_intf * intf;
	int r, a;
	int i = 1;
	int daemon = 1;
	struct pollfd pfd;

	/* make sure we have UID 0 */
	if (geteuid() || getuid()) {
		fprintf(stderr, "Inadequate privledges\n");
		do_exit(NULL, EXIT_FAILURE);
	}

	while ((a = getopt(argc, (char **)argv, "hvs")) != -1) {
		switch (a) {
		case 'h':
			usage();
			do_exit(NULL, EXIT_SUCCESS);
			break;
		case 'v':
			verbose++;
			break;
		case 's':
			daemon = 0;
			break;
		default:
			usage();
			do_exit(NULL, EXIT_FAILURE);
		}
	}

	if (daemon)
		daemonize();

	log_init("ipmievd", daemon, verbose);

	/* load interface */
	lprintf(LOG_DEBUG, "Loading OpenIPMI interface");
	intf = ipmi_intf_load("open");
	if (!intf) {
		lprintf(LOG_ERR, "Unable to load OpenIPMI interface");
		do_exit(NULL, EXIT_FAILURE);
	}

	/* open connection to openipmi device */
	lprintf(LOG_DEBUG, "Connecting to OpenIPMI device");
	r = intf->open(intf);
	if (r < 0) {
		lprintf(LOG_ERR, "Unable to open OpenIPMI device");
		do_exit(NULL, EXIT_FAILURE);
	}

	/* enable event message buffer */
	lprintf(LOG_DEBUG, "Enabling event message buffer");
	r = enable_event_msg_buffer(intf);
	if (r < 0) {
		lprintf(LOG_ERR, "Could not enable event message buffer");
		do_exit(intf, EXIT_FAILURE);
	}

	/* enable OpenIPMI event receiver */
	lprintf(LOG_DEBUG, "Enabling event receiver");
	r = ioctl(intf->fd, IPMICTL_SET_GETS_EVENTS_CMD, &i);
	if (r != 0) {
		lperror(LOG_ERR, "Could not enable event receiver");
		do_exit(intf, EXIT_FAILURE);
	}

	lprintf(LOG_NOTICE, "Waiting for events...");

	for (;;) {
		pfd.fd = intf->fd;	/* wait on openipmi device */
		pfd.events = POLLIN;	/* wait for input */
		r = poll(&pfd, 1, -1);

		switch (r) {
		case 0:
			/* timeout is disabled */
			break;
		case -1:
			lperror(LOG_CRIT, "Unable to read from IPMI device");
			do_exit(intf, EXIT_FAILURE);
			break;
		default:
			if (pfd.revents && POLLIN)
				read_event(intf);
		}
	}

	lprintf(LOG_DEBUG, "Shutting down...");
	do_exit(intf, EXIT_SUCCESS);
}
