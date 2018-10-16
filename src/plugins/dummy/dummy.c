/* Copyright (c) 2013 Zdenek Styblik, All Rights Reserved
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
 * Neither the name of Zdenek Styblik or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * Zdenek Styblik SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * Zdenek Styblik BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF Zdenek Styblik HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/log.h>

#include "dummy.h"

#if defined(HAVE_CONFIG_H)
# include <config.h>
#endif

extern int verbose;

/* data_read - read data from socket
 *
 * @data_ptr - pointer to memory where to store read data
 * @data_len - how much to read from socket
 *
 * return 0 on success, otherwise (-1)
 */
int
data_read(int fd, void *data_ptr, int data_len)
{
	int rc = 0;
	int data_read = 0;
	int data_total = 0;
	int try = 1;
	int errno_save = 0;
	if (data_len < 0) {
		return (-1);
	}
	while (data_total < data_len && try < 4) {
		errno = 0;
		/* TODO - add poll() */
		data_read = read(fd, data_ptr, data_len);
		errno_save = errno;
		if (data_read > 0) {
			data_total+= data_read;
		}
		if (errno_save != 0) {
			if (errno_save == EINTR || errno_save == EAGAIN) {
				try++;
				sleep(2);
				continue;
			} else {
				errno = errno_save;
				perror("dummy failed on read(): ");
				rc = (-1);
				break;
			}
		}
	}
	if (try > 3 && data_total != data_len) {
		rc = (-1);
	}
	return rc;
}

/* data_write - write data to the socket
 *
 * @data_ptr - ptr to data to send
 * @data_len - how long is the data to send
 *
 * returns 0 on success, otherwise (-1)
 */
int
data_write(int fd, void *data_ptr, int data_len)
{
	int rc = 0;
	int data_written = 0;
	int data_total = 0;
	int try = 1;
	int errno_save = 0;
	if (data_len < 0) {
		return (-1);
	}
	while (data_total < data_len && try < 4) {
		errno = 0;
		/* TODO - add poll() */
		data_written = write(fd, data_ptr, data_len);
		errno_save = errno;
		if (data_written > 0) {
			data_total+= data_written;
		}
		if (errno_save != 0) {
			if (errno_save == EINTR || errno_save == EAGAIN) {
				try++;
				sleep(2);
				continue;
			} else {
				errno = errno_save;
				perror("dummy failed on read(): ");
				rc = (-1);
				break;
			}
		}
	}
	if (try > 3 && data_total != data_len) {
		rc = (-1);
	}
	return rc;
}

/* ipmi_dummyipmi_close - send "BYE" and close socket
 *
 * @intf - ptr to initialize ipmi_intf struct
 *
 * returns void
 */
static void
ipmi_dummyipmi_close(struct ipmi_intf *intf)
{
	struct dummy_rq req;
	if (intf->fd < 0) {
		return;
	}
	memset(&req, 0, sizeof(req));
	req.msg.netfn = 0x3f;
	req.msg.cmd = 0xff;
	if (data_write(intf->fd, &req, sizeof(req)) != 0) {
		lprintf(LOG_ERR, "dummy failed to send 'BYE'");
	}
	close(intf->fd);
	intf->fd = (-1);
	intf->opened = 0;
}

/* ipmi_dummyipmi_open - open socket and prepare ipmi_intf struct
 *
 * @intf - ptr to ipmi_inf struct
 *
 * returns 0 on success, (-1) on error
 */
static int
ipmi_dummyipmi_open(struct ipmi_intf *intf)
{
	struct sockaddr_un address;
	int len;
	int rc;
	char *dummy_sock_path;

	dummy_sock_path = getenv("IPMI_DUMMY_SOCK");
	if (!dummy_sock_path) {
		lprintf(LOG_DEBUG, "No IPMI_DUMMY_SOCK set. Using " IPMI_DUMMY_DEFAULTSOCK);
		dummy_sock_path = IPMI_DUMMY_DEFAULTSOCK;
	}

	if (intf->opened == 1) {
		return intf->fd;
	}
	intf->fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (intf->fd == (-1)) {
		lprintf(LOG_ERR, "dummy failed on socket()");
		return (-1);
	}
	address.sun_family = AF_UNIX;
	strcpy(address.sun_path, dummy_sock_path);
	len = sizeof(address);
	rc = connect(intf->fd, (struct sockaddr *)&address, len);
	if (rc != 0) {
		perror("dummy failed on connect(): ");
		return (-1);
	}
	intf->opened = 1;
	return intf->fd;
}

/* ipmi_dummyipmi_send_cmd - send IPMI payload and await reply
 *
 * @intf - ptr to initialized ipmi_intf struct
 * @req - ptr to ipmi_rq struct to send
 *
 * return pointer to struct ipmi_rs OR NULL on error
 */
static struct ipmi_rs*
ipmi_dummyipmi_send_cmd(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	static struct ipmi_rs rsp;
	struct dummy_rq req_dummy;
	struct dummy_rs rsp_dummy;

	if (!intf || intf->fd < 0 || intf->opened != 1) {
		lprintf(LOG_ERR, "dummy failed on intf check.");
		return NULL;
	}

	memset(&req_dummy, 0, sizeof(req_dummy));
	req_dummy.msg.netfn = req->msg.netfn;
	req_dummy.msg.lun = req->msg.lun;
	req_dummy.msg.cmd = req->msg.cmd;
	req_dummy.msg.target_cmd = req->msg.target_cmd;
	req_dummy.msg.data_len = req->msg.data_len;
	req_dummy.msg.data = req->msg.data;
	if (verbose) {
		lprintf(LOG_NOTICE, ">>> IPMI req");
		lprintf(LOG_NOTICE, "msg.data_len: %i",
				req_dummy.msg.data_len);
		lprintf(LOG_NOTICE, "msg.netfn: %x", req_dummy.msg.netfn);
		lprintf(LOG_NOTICE, "msg.cmd: %x", req_dummy.msg.cmd);
		lprintf(LOG_NOTICE, "msg.target_cmd: %x",
				req_dummy.msg.target_cmd);
		lprintf(LOG_NOTICE, "msg.lun: %x", req_dummy.msg.lun);
		lprintf(LOG_NOTICE, ">>>");
	}
	if (data_write(intf->fd, &req_dummy,
				sizeof(struct dummy_rq)) != 0) {
		return NULL;
	}
	if (req->msg.data_len > 0) {
		if (data_write(intf->fd, (uint8_t *)(req->msg.data),
					req_dummy.msg.data_len) != 0) {
			return NULL;
		}
	}

	memset(&rsp_dummy, 0, sizeof(rsp_dummy));
	if (data_read(intf->fd, &rsp_dummy, sizeof(struct dummy_rs)) != 0) {
		return NULL;
	}
	if (rsp_dummy.data_len > 0) {
		if (data_read(intf->fd, (uint8_t *)&rsp.data,
					rsp_dummy.data_len) != 0) {
			return NULL;
		}
	}
	rsp.ccode = rsp_dummy.ccode;
	rsp.data_len = rsp_dummy.data_len;
	rsp.msg.netfn = rsp_dummy.msg.netfn;
	rsp.msg.cmd = rsp_dummy.msg.cmd;
	rsp.msg.seq = rsp_dummy.msg.seq;
	rsp.msg.lun = rsp_dummy.msg.lun;
	if (verbose) {
		lprintf(LOG_NOTICE, "<<< IPMI rsp");
		lprintf(LOG_NOTICE, "ccode: %x", rsp.ccode);
		lprintf(LOG_NOTICE, "data_len: %i", rsp.data_len);
		lprintf(LOG_NOTICE, "msg.netfn: %x", rsp.msg.netfn);
		lprintf(LOG_NOTICE, "msg.cmd: %x", rsp.msg.cmd);
		lprintf(LOG_NOTICE, "msg.seq: %x", rsp.msg.seq);
		lprintf(LOG_NOTICE, "msg.lun: %x", rsp.msg.lun);
		lprintf(LOG_NOTICE, "<<<");
	}
	return &rsp;
}

struct ipmi_intf ipmi_dummy_intf = {
	.name = "dummy",
	.desc = "Linux DummyIPMI Interface",
	.open = ipmi_dummyipmi_open,
	.close = ipmi_dummyipmi_close,
	.sendrecv = ipmi_dummyipmi_send_cmd,
	.my_addr = IPMI_BMC_SLAVE_ADDR,
	.target_addr = IPMI_BMC_SLAVE_ADDR,
};
