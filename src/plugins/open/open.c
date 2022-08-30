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
#include <sys/select.h>
#include <sys/stat.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

#if defined(HAVE_SYS_IOCCOM_H)
# include <sys/ioccom.h>
#endif

#if defined(HAVE_OPENIPMI_H)
# if defined(HAVE_LINUX_COMPILER_H)
#  include <linux/compiler.h>
# endif
# include <linux/ipmi.h>
#elif defined(HAVE_FREEBSD_IPMI_H)
/* FreeBSD OpenIPMI-compatible header */
# include <sys/ipmi.h>
#else
# include "open.h"
#endif

/**
 * Maximum input message size for KCS/SMIC is 40 with 2 utility bytes and
 * 38 bytes of data.
 * Maximum input message size for BT is 42 with 4 utility bytes and
 * 38 bytes of data.
 */
#define IPMI_OPENIPMI_MAX_RQ_DATA_SIZE 38

/**
 * Maximum output message size for KCS/SMIC is 38 with 2 utility bytes, a byte
 * for completion code and 35 bytes of data.
 * Maximum output message size for BT is 40 with 4 utility bytes, a byte
 * for completion code and 35 bytes of data.
 */
#define IPMI_OPENIPMI_MAX_RS_DATA_SIZE 35

/* Timeout for reading data from BMC in seconds */
#define IPMI_OPENIPMI_READ_TIMEOUT 15

extern int verbose;

static
int
ipmi_openipmi_open(struct ipmi_intf *intf)
{
	char ipmi_dev[16];
	char ipmi_devfs[16];
	char ipmi_devfs2[16];
	int devnum = 0;

	devnum = intf->devnum;

	sprintf(ipmi_dev, "/dev/ipmi%d", devnum);
	sprintf(ipmi_devfs, "/dev/ipmi/%d", devnum);
	sprintf(ipmi_devfs2, "/dev/ipmidev/%d", devnum);
	lprintf(LOG_DEBUG, "Using ipmi device %d", devnum);

	intf->fd = open(ipmi_dev, O_RDWR);

	if (intf->fd < 0) {
		intf->fd = open(ipmi_devfs, O_RDWR);
		if (intf->fd < 0) {
			intf->fd = open(ipmi_devfs2, O_RDWR);
		}
		if (intf->fd < 0) {
			lperror(LOG_ERR, "Could not open device at %s or %s or %s",
			        ipmi_dev, ipmi_devfs, ipmi_devfs2);
			return -1;
		}
	}

	int receive_events = TRUE;

	if (ioctl(intf->fd, IPMICTL_SET_GETS_EVENTS_CMD, &receive_events) < 0) {
		lperror(LOG_ERR, "Could not enable event receiver");
		return -1;
	}

	intf->opened = 1;

	/* This is never set to 0, the default is IPMI_BMC_SLAVE_ADDR */
	if (intf->my_addr != 0) {
		if (intf->set_my_addr(intf, intf->my_addr) < 0) {
			lperror(LOG_ERR, "Could not set IPMB address");
			return -1;
		}
		lprintf(LOG_DEBUG, "Set IPMB address to 0x%x", intf->my_addr);
	}

	intf->manufacturer_id = ipmi_get_oem(intf);
	return intf->fd;
}

static
int
ipmi_openipmi_set_my_addr(struct ipmi_intf *intf, uint8_t addr)
{
	unsigned int a = addr;
	if (ioctl(intf->fd, IPMICTL_SET_MY_ADDRESS_CMD, &a) < 0) {
		lperror(LOG_ERR, "Could not set IPMB address");
		return -1;
	}
	intf->my_addr = addr;
	return 0;
}

static
void
ipmi_openipmi_close(struct ipmi_intf *intf)
{
	if (intf->fd >= 0) {
		close(intf->fd);
		intf->fd = -1;
	}

	intf->opened = 0;
	intf->manufacturer_id = IPMI_OEM_UNKNOWN;
}

static
struct ipmi_rs *
ipmi_openipmi_send_cmd(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	struct ipmi_recv recv = {};
	struct ipmi_addr addr;
	struct ipmi_system_interface_addr bmc_addr = {
		.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE,
		.channel = IPMI_BMC_CHANNEL,
	};
	struct ipmi_ipmb_addr ipmb_addr = {
		.addr_type = IPMI_IPMB_ADDR_TYPE,
	};
	struct ipmi_req _req;
	static struct ipmi_rs rsp;
	struct timeval read_timeout;
	static int curr_seq = 0;
	fd_set rset;

	uint8_t *data = NULL;
	int data_len = 0;
	int retval = 0;

	if (!intf || !req)
		return NULL;

	ipmb_addr.channel = intf->target_channel & 0x0f;

	if (!intf->opened && intf->open)
		if (intf->open(intf) < 0)
			return NULL;

	if (verbose > 2) {
		fprintf(stderr, "OpenIPMI Request Message Header:\n");
		fprintf(stderr, "  netfn     = 0x%x\n", req->msg.netfn);
		fprintf(stderr, "  cmd       = 0x%x\n", req->msg.cmd);
		printbuf(req->msg.data, req->msg.data_len,
		         "OpenIPMI Request Message Data");
	}

	/*
	 * setup and send message
	 */

	memset(&_req, 0, sizeof(struct ipmi_req));

	if (intf->target_addr != 0 &&
	    intf->target_addr != intf->my_addr)
	{
		/* use IPMB address if needed */
		ipmb_addr.slave_addr = intf->target_addr;
		ipmb_addr.lun = req->msg.lun;
		lprintf(LOG_DEBUG,
		        "Sending request 0x%x to "
		        "IPMB target @ 0x%x:0x%x (from 0x%x)",
		        req->msg.cmd, intf->target_addr, intf->target_channel,
		        intf->my_addr);

		if (intf->transit_addr != 0 &&
		    intf->transit_addr != intf->my_addr)
		{
			uint8_t index = 0;

			lprintf(LOG_DEBUG,
			        "Encapsulating data sent to "
			        "end target [0x%02x,0x%02x] using "
			        "transit [0x%02x,0x%02x] from 0x%x ",
			        (0x40 | intf->target_channel),
			        intf->target_addr,
			        intf->transit_channel,
			        intf->transit_addr,
			        intf->my_addr);

			/* Convert Message to 'Send Message' */
			/* Supplied req : req , internal req : _req  */

			if (verbose > 4) {
				fprintf(stderr, "Converting message:\n");
				fprintf(stderr, "  netfn     = 0x%x\n", req->msg.netfn);
				fprintf(stderr, "  cmd       = 0x%x\n", req->msg.cmd);
				if (req->msg.data && req->msg.data_len) {
					fprintf(stderr, "  data_len  = %d\n", req->msg.data_len);
					fprintf(stderr, "  data      = %s\n",
					        buf2str(req->msg.data, req->msg.data_len));
				}
			}

			/* Modify target address to use 'transit' instead */
			ipmb_addr.slave_addr = intf->transit_addr;
			ipmb_addr.channel = intf->transit_channel;

			/* FIXME backup "My address" */
			data_len = req->msg.data_len + 8;
			data = malloc(data_len);
			if (!data) {
				lprintf(LOG_ERR, "ipmitool: malloc failure");
				return NULL;
			}

			memset(data, 0, data_len);

			data[index++] = (0x40 | intf->target_channel);
			data[index++] = intf->target_addr;
			data[index++] = (req->msg.netfn << 2) | req->msg.lun;
			data[index++] = ipmi_csum(data + 1, 2);
			data[index++] = 0xFF; /* normally 0x20 , overwritten by IPMC  */
			data[index++] = ((0) << 2) | 0; /* FIXME */
			data[index++] = req->msg.cmd;
			memcpy((data + index), req->msg.data, req->msg.data_len);
			index += req->msg.data_len;
			data[index++] = ipmi_csum((data + 4), (req->msg.data_len + 3));

			if (verbose > 4) {
				fprintf(stderr, "Encapsulated message:\n");
				fprintf(stderr, "  netfn     = 0x%x\n", IPMI_NETFN_APP);
				fprintf(stderr, "  cmd       = 0x%x\n", 0x34);
				if (data && data_len) {
					fprintf(stderr, "  data_len  = %d\n", data_len);
					fprintf(stderr, "  data      = %s\n",
					        buf2str(data, data_len));
				}
			}
		}
		_req.addr = (unsigned char *)&ipmb_addr;
		_req.addr_len = sizeof(ipmb_addr);
	} else {
		/* otherwise use system interface */
		lprintf(LOG_DEBUG + 2, "Sending request 0x%x to System Interface",
		        req->msg.cmd);
		bmc_addr.lun = req->msg.lun;
		_req.addr = (unsigned char *)&bmc_addr;
		_req.addr_len = sizeof(bmc_addr);
	}

	_req.msgid = curr_seq++;

	/* In case of a bridge request */
	if (data && data_len != 0) {
		_req.msg.data = data;
		_req.msg.data_len = data_len;
		_req.msg.netfn = IPMI_NETFN_APP;
		_req.msg.cmd = 0x34;

	} else {
		_req.msg.data = req->msg.data;
		_req.msg.data_len = req->msg.data_len;
		_req.msg.netfn = req->msg.netfn;
		_req.msg.cmd = req->msg.cmd;
	}

	if (ioctl(intf->fd, IPMICTL_SEND_COMMAND, &_req) < 0) {
		lperror(LOG_ERR, "Unable to send command");
		free_n(&data);
		return NULL;
	}

	/*
	 * wait for and retrieve response
	 */

	if (intf->noanswer) {
		free_n(&data);
		return NULL;
	}

	FD_ZERO(&rset);
	FD_SET(intf->fd, &rset);
	read_timeout.tv_sec = IPMI_OPENIPMI_READ_TIMEOUT;
	read_timeout.tv_usec = 0;
	do {
		do {
			retval = select(intf->fd + 1, &rset, NULL, NULL, &read_timeout);
		} while (retval < 0 && errno == EINTR);
		if (retval < 0) {
			lperror(LOG_ERR, "I/O Error");
			free_n(&data);
			return NULL;
		} else if (retval == 0) {
			lprintf(LOG_ERR, "No data available");
			free_n(&data);
			return NULL;
		}
		if (FD_ISSET(intf->fd, &rset) == 0) {
			lprintf(LOG_ERR, "No data available");
			free_n(&data);
			return NULL;
		}

		recv.addr = (unsigned char *)&addr;
		recv.addr_len = sizeof(addr);
		recv.msg.data = rsp.data;
		recv.msg.data_len = sizeof(rsp.data);

		/* get data */
		if (ioctl(intf->fd, IPMICTL_RECEIVE_MSG_TRUNC, &recv) < 0) {
			lperror(LOG_ERR, "Error receiving message");
			if (errno != EMSGSIZE) {
				free_n(&data);
				return NULL;
			}
		}

		/* If the message received wasn't expected, try to grab the
		 * next message until it's out of messages.  -EAGAIN is
		 * returned if the list empty, but basically if it returns a
		 * message, check if it's alright.
		 */
		if (_req.msgid != recv.msgid) {
			lprintf(LOG_NOTICE,
			        "Received a response with unexpected ID %ld vs. %ld",
			        recv.msgid, _req.msgid);
		}
	} while (_req.msgid != recv.msgid);

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

	if (intf->transit_addr != 0 && intf->transit_addr != intf->my_addr) {
		/* ipmb_addr.transit_slave_addr = intf->transit_addr; */
		lprintf(LOG_DEBUG,
		        "Decapsulating data received from transit "
		        "IPMB target @ 0x%x",
		        intf->transit_addr);

		/* comp code */
		/* Check data */

		if (recv.msg.data[0] == 0) {
			recv.msg.netfn = recv.msg.data[2] >> 2;
			recv.msg.cmd = recv.msg.data[6];

			recv.msg.data = memmove(recv.msg.data, recv.msg.data + 7,
			                        recv.msg.data_len - 7);
			recv.msg.data_len -= 8;

			if (verbose > 4) {
				fprintf(stderr, "Decapsulated  message:\n");
				fprintf(stderr, "  netfn     = 0x%x\n", recv.msg.netfn);
				fprintf(stderr, "  cmd       = 0x%x\n", recv.msg.cmd);
				if (recv.msg.data && recv.msg.data_len) {
					fprintf(stderr, "  data_len  = %d\n", recv.msg.data_len);
					fprintf(stderr, "  data      = %s\n",
					        buf2str(recv.msg.data, recv.msg.data_len));
				}
			}
		}
	}

	/* save completion code */
	rsp.ccode = recv.msg.data[0];
	rsp.data_len = recv.msg.data_len - 1;

	/* save response data for caller */
	if (!rsp.ccode && rsp.data_len > 0) {
		memmove(rsp.data, rsp.data + 1, rsp.data_len);
		rsp.data[rsp.data_len] = 0;
	}

	free_n(&data);

	return &rsp;
}

int
ipmi_openipmi_setup(struct ipmi_intf *intf)
{
	/* set default payload size */
	intf->max_request_data_size = IPMI_OPENIPMI_MAX_RQ_DATA_SIZE;
	intf->max_response_data_size = IPMI_OPENIPMI_MAX_RS_DATA_SIZE;

	return 0;
}

struct ipmi_intf ipmi_open_intf = {
	.name = "open",
	.desc = "Linux OpenIPMI Interface",
	.setup = ipmi_openipmi_setup,
	.open = ipmi_openipmi_open,
	.close = ipmi_openipmi_close,
	.sendrecv = ipmi_openipmi_send_cmd,
	.set_my_addr = ipmi_openipmi_set_my_addr,
	.my_addr = IPMI_BMC_SLAVE_ADDR,
	.target_addr = 0, /* init so -m local_addr does not cause bridging */
};
