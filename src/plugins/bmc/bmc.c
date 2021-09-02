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
#include <stddef.h>
#include <stropts.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include "bmc_intf.h"

#include "bmc.h"

static int	curr_seq;
static int bmc_method(int fd, int *if_type);
struct ipmi_rs *(*sendrecv_fn)(struct ipmi_intf *, struct ipmi_rq *) = NULL;
extern int	verbose;

static void dump_request(bmc_req_t *request);
static void dump_response(bmc_rsp_t *response);
static struct ipmi_rs *ipmi_bmc_send_cmd_ioctl(struct ipmi_intf *intf,
	struct ipmi_rq *req);
static struct ipmi_rs *ipmi_bmc_send_cmd_putmsg(struct ipmi_intf *intf,
	struct ipmi_rq *req);

#define	MESSAGE_BUFSIZE 1024

struct ipmi_intf ipmi_bmc_intf = {
	.name = "bmc",
	.desc = "IPMI v2.0 BMC interface",
	.open = ipmi_bmc_open,
	.close = ipmi_bmc_close,
	.sendrecv = ipmi_bmc_send_cmd
};

void
ipmi_bmc_close(struct ipmi_intf *intf)
{
	if (intf && intf->fd >= 0)
		close(intf->fd);

	intf->opened = 0;
	intf->manufacturer_id = IPMI_OEM_UNKNOWN;
	intf->fd = -1;
}

int
ipmi_bmc_open(struct ipmi_intf *intf)
{
	int method;

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

	if (bmc_method(intf->fd, &method) < 0) {
		perror("Could not determine bmc messaging interface");
		return (-1);
	}

	sendrecv_fn = (method == BMC_PUTMSG_METHOD) ?
	    ipmi_bmc_send_cmd_putmsg : ipmi_bmc_send_cmd_ioctl;

	intf->manufacturer_id = ipmi_get_oem(intf);
	return (intf->fd);
}

struct ipmi_rs *
ipmi_bmc_send_cmd(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	/* If not already opened open the device or network connection */
	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

	/* sendrecv_fn cannot be NULL at this point */
	return ((*sendrecv_fn)(intf, req));
}

static struct ipmi_rs *
ipmi_bmc_send_cmd_ioctl(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	struct strioctl istr;
	static struct bmc_reqrsp reqrsp;
	static struct ipmi_rs rsp;

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
		printf("--\n");
		dump_request(&reqrsp.req);
		printf("--\n");
	}

	if (ioctl(intf->fd, I_STR, &istr) < 0) {
		perror("BMC IOCTL: I_STR");
		return (NULL);
	}

	if (verbose > 2) {
		dump_response(&reqrsp.rsp);
		printf("--\n");
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

static struct ipmi_rs *
ipmi_bmc_send_cmd_putmsg(struct ipmi_intf *intf, struct ipmi_rq *req)
{
	struct strbuf sb;
	int flags = 0;
	static uint32_t msg_seq = 0;

	/*
	 * The length of the message structure is equal to the size of the
	 * bmc_req_t structure, PLUS any additional data space in excess of
	 * the data space already reserved in the data member + <n> for
	 * the rest of the members in the bmc_msg_t structure.
	 */
	int msgsz = offsetof(bmc_msg_t, msg) + sizeof(bmc_req_t) +
		((req->msg.data_len > SEND_MAX_PAYLOAD_SIZE) ?
			(req->msg.data_len - SEND_MAX_PAYLOAD_SIZE) : 0);
	bmc_msg_t *msg = malloc(msgsz);
	bmc_req_t *request = (bmc_req_t *)&msg->msg[0];
	bmc_rsp_t *response;
	static struct ipmi_rs rsp;
	struct ipmi_rs *ret = NULL;

	msg->m_type = BMC_MSG_REQUEST;
	msg->m_id = msg_seq++;
	request->fn = req->msg.netfn;
	request->lun = 0;
	request->cmd = req->msg.cmd;
	request->datalength = req->msg.data_len;
	memcpy(request->data, req->msg.data, req->msg.data_len);

	sb.len = msgsz;
	sb.buf = (unsigned char *)msg;

	if (verbose) {
		printf("--\n");
		dump_request(request);
		printf("--\n");
	}

	if (putmsg(intf->fd, NULL, &sb, 0) < 0) {
		perror("BMC putmsg: ");
		free(msg);
		msg = NULL;
		return (NULL);
	}

	free(msg);
	msg = NULL;

	sb.buf = malloc(MESSAGE_BUFSIZE);
	sb.maxlen = MESSAGE_BUFSIZE;

	if (getmsg(intf->fd, NULL, &sb, &flags) < 0) {
		perror("BMC getmsg: ");
		free(sb.buf);
		sb.buf = NULL;
		return (NULL);
	}

	msg = (bmc_msg_t *)sb.buf;

	if (verbose > 3) {
		printf("Got msg (id 0x%x) type 0x%x\n", msg->m_id, msg->m_type);
	}


	/* Did we get an error back from the stream? */
	switch (msg->m_type) {

	case BMC_MSG_RESPONSE:
		response = (bmc_rsp_t *)&msg->msg[0];

		if (verbose > 2) {
			dump_response(response);
			printf("--\n");
		}

		memset(&rsp, 0, sizeof (struct ipmi_rs));
		rsp.ccode = response->ccode;
		rsp.data_len = response->datalength;

		if (!rsp.ccode && (rsp.data_len > 0))
			memcpy(rsp.data, response->data, rsp.data_len);

		ret = &rsp;
		break;

	case BMC_MSG_ERROR:
		/* In case of an error, msg->msg[0] has the error code */
		printf("bmc_send_cmd: %s\n", strerror(msg->msg[0]));
		break;

	}
	
	free(sb.buf);
	sb.buf = NULL;
	return (ret);
}

/*
 * Determine which interface to use.  Returns the interface method
 * to use.
 */
static int
bmc_method(int fd, int *if_type)
{
	struct strioctl istr;
	int retval = 0;
	uint8_t method = BMC_PUTMSG_METHOD;

	istr.ic_cmd = IOCTL_IPMI_INTERFACE_METHOD;
	istr.ic_timout = 0;
	istr.ic_dp = (uint8_t *)&method;
	istr.ic_len = 1;

	/*
	 * If the ioctl doesn't exist, we should get an EINVAL back.
	 * Bail out on any other error.
	 */
	if (ioctl(fd, I_STR, &istr) < 0) {

		if (errno != EINVAL)
			retval = -1;
		else
			method = BMC_IOCTL_METHOD;
	}

	if (retval == 0)
		*if_type = method;

	return (retval);
}

static void
dump_request(bmc_req_t *request)
{
	int i;

	printf("BMC req.fn         : 0x%x\n", request->fn);
	printf("BMC req.lun        : 0x%x\n", request->lun);
	printf("BMC req.cmd        : 0x%x\n", request->cmd);
	printf("BMC req.datalength : 0x%x\n", request->datalength);
	printf("BMC req.data       : ");

	if (request->datalength > 0) {
		for (i = 0; i < request->datalength; i++)
			printf("0x%x ", request->data[i]);
	} else {
		printf("<NONE>");
	}
	printf("\n");
}

static void
dump_response(bmc_rsp_t *response)
{
	int i;

	printf("BMC rsp.fn         : 0x%x\n", response->fn);
	printf("BMC rsp.lun        : 0x%x\n", response->lun);
	printf("BMC rsp.cmd        : 0x%x\n", response->cmd);
	printf("BMC rsp.ccode      : 0x%x\n", response->ccode);
	printf("BMC rsp.datalength : 0x%x\n", response->datalength);
	printf("BMC rsp.data       : ");

	if (response->datalength > 0) {
		for (i = 0; i < response->datalength; i++)
			printf("0x%x ", response->data[i]);
	} else {
		printf("<NONE>");
	}
	printf("\n");
}
