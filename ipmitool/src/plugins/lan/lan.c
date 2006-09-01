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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_oem.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_constants.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "lan.h"
#include "rmcp.h"
#include "asf.h"
#include "auth.h"

#define IPMI_LAN_TIMEOUT	2
#define IPMI_LAN_RETRY		4
#define IPMI_LAN_PORT		0x26f
#define IPMI_LAN_CHANNEL_E	0x0e

extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_session_vals[];
extern int verbose;

struct ipmi_rq_entry * ipmi_req_entries;
static struct ipmi_rq_entry * ipmi_req_entries_tail;
static uint8_t bridge_possible = 0;

static int ipmi_lan_send_packet(struct ipmi_intf * intf, uint8_t * data, int data_len);
static struct ipmi_rs * ipmi_lan_recv_packet(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lan_poll_recv(struct ipmi_intf * intf);
static int ipmi_lan_setup(struct ipmi_intf * intf);
static int ipmi_lan_keepalive(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lan_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req);
static int ipmi_lan_send_rsp(struct ipmi_intf * intf, struct ipmi_rs * rsp);
static int ipmi_lan_open(struct ipmi_intf * intf);
static void ipmi_lan_close(struct ipmi_intf * intf);
static int ipmi_lan_ping(struct ipmi_intf * intf);

struct ipmi_intf ipmi_lan_intf = {
	name:		"lan",
	desc:		"IPMI v1.5 LAN Interface",
	setup:		ipmi_lan_setup,
	open:		ipmi_lan_open,
	close:		ipmi_lan_close,
	sendrecv:	ipmi_lan_send_cmd,
	sendrsp:	ipmi_lan_send_rsp,
	keepalive:	ipmi_lan_keepalive,
	target_addr:	IPMI_BMC_SLAVE_ADDR,
};

static struct ipmi_rq_entry *
ipmi_req_add_entry(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	struct ipmi_rq_entry * e;

	e = malloc(sizeof(struct ipmi_rq_entry));
	if (e == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}

	memset(e, 0, sizeof(struct ipmi_rq_entry));
	memcpy(&e->req, req, sizeof(struct ipmi_rq));

	e->intf = intf;

	if (ipmi_req_entries == NULL)
		ipmi_req_entries = e;
	else
		ipmi_req_entries_tail->next = e;

	ipmi_req_entries_tail = e;
	lprintf(LOG_DEBUG+3, "added list entry seq=0x%02x cmd=0x%02x",
		e->rq_seq, e->req.msg.cmd);
	return e;
}

static struct ipmi_rq_entry *
ipmi_req_lookup_entry(uint8_t seq, uint8_t cmd)
{
	struct ipmi_rq_entry * e = ipmi_req_entries;
	while (e && (e->rq_seq != seq || e->req.msg.cmd != cmd)) {
		if (e->next == NULL || e == e->next)
			return NULL;
		e = e->next;
	}
	return e;
}

static void
ipmi_req_remove_entry(uint8_t seq, uint8_t cmd)
{
	struct ipmi_rq_entry * p, * e;

	e = p = ipmi_req_entries;

	while (e && (e->rq_seq != seq || e->req.msg.cmd != cmd)) {
		p = e;
		e = e->next;
	}
	if (e) {
		lprintf(LOG_DEBUG+3, "removed list entry seq=0x%02x cmd=0x%02x",
			seq, cmd);
		p->next = (p->next == e->next) ? NULL : e->next;
		if (ipmi_req_entries == e) {
			if (ipmi_req_entries != p)
				ipmi_req_entries = p;
			else
				ipmi_req_entries = NULL;
		}
		if (ipmi_req_entries_tail == e) {
			if (ipmi_req_entries_tail != p)
				ipmi_req_entries_tail = p;
			else
				ipmi_req_entries_tail = NULL;
		}
		if (e->msg_data)
			free(e->msg_data);
		free(e);
	}
}

static void
ipmi_req_clear_entries(void)
{
	struct ipmi_rq_entry * p, * e;

	e = ipmi_req_entries;
	while (e) {
		lprintf(LOG_DEBUG+3, "cleared list entry seq=0x%02x cmd=0x%02x",
			e->rq_seq, e->req.msg.cmd);
		if (e->next != NULL) {
			p = e->next;
			free(e);
			e = p;
		} else {
			free(e);
			break;
		}
	}
	ipmi_req_entries = NULL;
}

static int
get_random(void *data, int len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	int rv;

	if (fd < 0 || len < 0)
		return errno;

	rv = read(fd, data, len);

	close(fd);
	return rv;
}

static int
ipmi_lan_send_packet(struct ipmi_intf * intf, uint8_t * data, int data_len)
{
	if (verbose > 2)
		printbuf(data, data_len, "send_packet");

	return send(intf->fd, data, data_len, 0);
}

static struct ipmi_rs *
ipmi_lan_recv_packet(struct ipmi_intf * intf)
{
	static struct ipmi_rs rsp;
	fd_set read_set, err_set;
	struct timeval tmout;
	int ret;

	FD_ZERO(&read_set);
	FD_SET(intf->fd, &read_set);

	FD_ZERO(&err_set);
	FD_SET(intf->fd, &err_set);

	tmout.tv_sec = intf->session->timeout;
	tmout.tv_usec = 0;

	ret = select(intf->fd + 1, &read_set, NULL, &err_set, &tmout);
	if (ret < 0 || FD_ISSET(intf->fd, &err_set) || !FD_ISSET(intf->fd, &read_set))
		return NULL;

	/* the first read may return ECONNREFUSED because the rmcp ping
	 * packet--sent to UDP port 623--will be processed by both the
	 * BMC and the OS.
	 *
	 * The problem with this is that the ECONNREFUSED takes
	 * priority over any other received datagram; that means that
	 * the Connection Refused shows up _before_ the response packet,
	 * regardless of the order they were sent out.  (unless the
	 * response is read before the connection refused is returned)
	 */
	ret = recv(intf->fd, &rsp.data, IPMI_BUF_SIZE, 0);

	if (ret < 0) {
		FD_ZERO(&read_set);
		FD_SET(intf->fd, &read_set);

		FD_ZERO(&err_set);
		FD_SET(intf->fd, &err_set);

		tmout.tv_sec = intf->session->timeout;
		tmout.tv_usec = 0;

		ret = select(intf->fd + 1, &read_set, NULL, &err_set, &tmout);
		if (ret < 0) {
			if (FD_ISSET(intf->fd, &err_set) || !FD_ISSET(intf->fd, &read_set))
				return NULL;

			ret = recv(intf->fd, &rsp.data, IPMI_BUF_SIZE, 0);
			if (ret < 0)
				return NULL;
		}
	}

	if (ret == 0)
		return NULL;

	rsp.data[ret] = '\0';
	rsp.data_len = ret;

	if (verbose > 2)
		printbuf(rsp.data, rsp.data_len, "recv_packet");

	return &rsp;
}

/*
 * parse response RMCP "pong" packet
 *
 * return -1 if ping response not received
 * returns 0 if IPMI is NOT supported
 * returns 1 if IPMI is supported
 *
 * udp.source	= 0x026f	// RMCP_UDP_PORT
 * udp.dest	= ?		// udp.source from rmcp-ping
 * udp.len	= ?
 * udp.check	= ?
 * rmcp.ver	= 0x06		// RMCP Version 1.0
 * rmcp.__res	= 0x00		// RESERVED
 * rmcp.seq	= 0xff		// no RMCP ACK
 * rmcp.class	= 0x06		// RMCP_CLASS_ASF
 * asf.iana	= 0x000011be	// ASF_RMCP_IANA
 * asf.type	= 0x40		// ASF_TYPE_PONG
 * asf.tag	= ?		// asf.tag from rmcp-ping
 * asf.__res	= 0x00		// RESERVED
 * asf.len	= 0x10		// 16 bytes
 * asf.data[3:0]= 0x000011be	// IANA# = RMCP_ASF_IANA if no OEM
 * asf.data[7:4]= 0x00000000	// OEM-defined (not for IPMI)
 * asf.data[8]	= 0x81		// supported entities
 * 				// [7]=IPMI [6:4]=RES [3:0]=ASF_1.0
 * asf.data[9]	= 0x00		// supported interactions (reserved)
 * asf.data[f:a]= 0x000000000000
 */
static int
ipmi_handle_pong(struct ipmi_intf * intf, struct ipmi_rs * rsp)
{
	struct rmcp_pong * pong;

	if (rsp == NULL)
		return -1;

	pong = (struct rmcp_pong *)rsp->data;

	lprintf(LOG_DEBUG,
		"Received IPMI/RMCP response packet: \n"
		"  IPMI%s Supported\n"
		"  ASF Version %s\n"
		"  RMCP Version %s\n"
		"  RMCP Sequence %d\n"
		"  IANA Enterprise %ld\n",
		(pong->sup_entities & 0x80) ? "" : " NOT",
		(pong->sup_entities & 0x01) ? "1.0" : "unknown",
		(pong->rmcp.ver == 6) ? "1.0" : "unknown",
		pong->rmcp.seq,
		ntohl(pong->iana));

	return (pong->sup_entities & 0x80) ? 1 : 0;
}

/* build and send RMCP presence ping packet
 *
 * RMCP ping
 *
 * udp.source	= ?
 * udp.dest	= 0x026f	// RMCP_UDP_PORT
 * udp.len	= ?
 * udp.check	= ?
 * rmcp.ver	= 0x06		// RMCP Version 1.0
 * rmcp.__res	= 0x00		// RESERVED
 * rmcp.seq	= 0xff		// no RMCP ACK
 * rmcp.class	= 0x06		// RMCP_CLASS_ASF
 * asf.iana	= 0x000011be	// ASF_RMCP_IANA
 * asf.type	= 0x80		// ASF_TYPE_PING
 * asf.tag	= ?		// ASF sequence number
 * asf.__res	= 0x00		// RESERVED
 * asf.len	= 0x00
 *
 */
static int
ipmi_lan_ping(struct ipmi_intf * intf)
{
	struct asf_hdr asf_ping = {
		.iana	= htonl(ASF_RMCP_IANA),
		.type	= ASF_TYPE_PING,
	};
	struct rmcp_hdr rmcp_ping = {
		.ver	= RMCP_VERSION_1,
		.class	= RMCP_CLASS_ASF,
		.seq	= 0xff,
	};
	uint8_t * data;
	int len = sizeof(rmcp_ping) + sizeof(asf_ping);
	int rv;

	data = malloc(len);
	if (data == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return -1;
	}
	memset(data, 0, len);
	memcpy(data, &rmcp_ping, sizeof(rmcp_ping));
	memcpy(data+sizeof(rmcp_ping), &asf_ping, sizeof(asf_ping));

	lprintf(LOG_DEBUG, "Sending IPMI/RMCP presence ping packet");

	rv = ipmi_lan_send_packet(intf, data, len);

	free(data);

	if (rv < 0) {
		lprintf(LOG_ERR, "Unable to send IPMI presence ping packet");
		return -1;
	}

	if (ipmi_lan_poll_recv(intf) == 0)
		return 0;

	return 1;
}

/*
 * The "thump" functions are used to send an extra packet following each
 * request message.  This may kick-start some BMCs that get confused with
 * bad passwords or operate poorly under heavy network load.
 */
static void
ipmi_lan_thump_first(struct ipmi_intf * intf)
{
	/* is this random data? */
	uint8_t data[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x07, 0x20, 0x18, 0xc8, 0xc2, 0x01, 0x01, 0x3c };
	ipmi_lan_send_packet(intf, data, 16);
}

static void
ipmi_lan_thump(struct ipmi_intf * intf)
{
	uint8_t data[10] = "thump";
	ipmi_lan_send_packet(intf, data, 10);
}

static struct ipmi_rs *
ipmi_lan_poll_recv(struct ipmi_intf * intf)
{
	struct rmcp_hdr rmcp_rsp;
	struct ipmi_rs * rsp;
	struct ipmi_rq_entry * entry;
	int x=0, rv;
	uint8_t our_address = intf->my_addr;

	if (our_address == 0)
		our_address = IPMI_BMC_SLAVE_ADDR;

	rsp = ipmi_lan_recv_packet(intf);

	while (rsp != NULL) {

		/* parse response headers */
		memcpy(&rmcp_rsp, rsp->data, 4);

		switch (rmcp_rsp.class) {
		case RMCP_CLASS_ASF:
			/* ping response packet */
			rv = ipmi_handle_pong(intf, rsp);
			return (rv <= 0) ? NULL : rsp;
		case RMCP_CLASS_IPMI:
			/* handled by rest of function */
			break;
		default:
			lprintf(LOG_DEBUG, "Invalid RMCP class: %x",
				rmcp_rsp.class);
			rsp = ipmi_lan_recv_packet(intf);
			continue;
		}

		x = 4;
		rsp->session.authtype = rsp->data[x++];
		memcpy(&rsp->session.seq, rsp->data+x, 4);
		x += 4;
		memcpy(&rsp->session.id, rsp->data+x, 4);
		x += 4;

		if (intf->session->active && (rsp->session.authtype || intf->session->authtype))
			x += 16;

		rsp->session.msglen = rsp->data[x++];
		rsp->payload.ipmi_response.rq_addr = rsp->data[x++];
		rsp->payload.ipmi_response.netfn   = rsp->data[x] >> 2;
		rsp->payload.ipmi_response.rq_lun  = rsp->data[x++] & 0x3;
		x++;		/* checksum */
		rsp->payload.ipmi_response.rs_addr = rsp->data[x++];
		rsp->payload.ipmi_response.rq_seq  = rsp->data[x] >> 2;
		rsp->payload.ipmi_response.rs_lun  = rsp->data[x++] & 0x3;
		rsp->payload.ipmi_response.cmd     = rsp->data[x++];
		rsp->ccode          = rsp->data[x++];

		if (verbose > 2)
			printbuf(rsp->data, rsp->data_len, "ipmi message header");

		lprintf(LOG_DEBUG+1, "<< IPMI Response Session Header");
		lprintf(LOG_DEBUG+1, "<<   Authtype   : %s",
		       val2str(rsp->session.authtype, ipmi_authtype_session_vals));
		lprintf(LOG_DEBUG+1, "<<   Sequence   : 0x%08lx",
			(long)rsp->session.seq);
		lprintf(LOG_DEBUG+1, "<<   Session ID : 0x%08lx",
			(long)rsp->session.id);
		lprintf(LOG_DEBUG+1, "<< IPMI Response Message Header");
		lprintf(LOG_DEBUG+1, "<<   Rq Addr    : %02x",
			rsp->payload.ipmi_response.rq_addr);
		lprintf(LOG_DEBUG+1, "<<   NetFn      : %02x",
			rsp->payload.ipmi_response.netfn);
		lprintf(LOG_DEBUG+1, "<<   Rq LUN     : %01x",
			rsp->payload.ipmi_response.rq_lun);
		lprintf(LOG_DEBUG+1, "<<   Rs Addr    : %02x",
			rsp->payload.ipmi_response.rs_addr);
		lprintf(LOG_DEBUG+1, "<<   Rq Seq     : %02x",
			rsp->payload.ipmi_response.rq_seq);
		lprintf(LOG_DEBUG+1, "<<   Rs Lun     : %01x",
			rsp->payload.ipmi_response.rs_lun);
		lprintf(LOG_DEBUG+1, "<<   Command    : %02x",
			rsp->payload.ipmi_response.cmd);
		lprintf(LOG_DEBUG+1, "<<   Compl Code : 0x%02x",
			rsp->ccode);

		/* now see if we have outstanding entry in request list */
		entry = ipmi_req_lookup_entry(rsp->payload.ipmi_response.rq_seq,
					      rsp->payload.ipmi_response.cmd);
		if (entry) {
			lprintf(LOG_DEBUG+2, "IPMI Request Match found");
			if ((intf->target_addr != our_address) && bridge_possible) {
				if ((rsp->data_len) &&
				    (rsp->payload.ipmi_response.cmd != 0x34)) {
					printbuf(&rsp->data[x], rsp->data_len-x,
						 "bridge command response");
				}
				/* bridged command: lose extra header */
				if (rsp->payload.ipmi_response.cmd == 0x34) {
					if (rsp->data_len == 38) {
						entry->req.msg.cmd = entry->req.msg.target_cmd;
						rsp = ipmi_lan_recv_packet(intf);
						continue;
					}
				} else {
					//x += sizeof(rsp->payload.ipmi_response);
					if (rsp->data[x-1] != 0)
						lprintf(LOG_DEBUG, "WARNING: Bridged "
							"cmd ccode = 0x%02x",
						       rsp->data[x-1]);
				}
			}
			ipmi_req_remove_entry(rsp->payload.ipmi_response.rq_seq,
					      rsp->payload.ipmi_response.cmd);
		} else {
			lprintf(LOG_INFO, "IPMI Request Match NOT FOUND");
			rsp = ipmi_lan_recv_packet(intf);
			continue;
		}

		break;
	}

	/* shift response data to start of array */
	if (rsp && rsp->data_len > x) {
		rsp->data_len -= x + 1;
		memmove(rsp->data, rsp->data + x, rsp->data_len);
		memset(rsp->data + rsp->data_len, 0, IPMI_BUF_SIZE - rsp->data_len);
	}

	return rsp;
}

/*
 * IPMI LAN Request Message Format
 * +--------------------+
 * |  rmcp.ver          | 4 bytes
 * |  rmcp.__reserved   |
 * |  rmcp.seq          |
 * |  rmcp.class        |
 * +--------------------+
 * |  session.authtype | 9 bytes
 * |  session.seq   |
 * |  session.id    |
 * +--------------------+
 * | [session.authcode] | 16 bytes (AUTHTYPE != none)
 * +--------------------+
 * |  message length    | 1 byte
 * +--------------------+
 * |  message.rs_addr   | 6 bytes
 * |  message.netfn_lun |
 * |  message.checksum  |
 * |  message.rq_addr   |
 * |  message.rq_seq    |
 * |  message.cmd       |
 * +--------------------+
 * | [request data]     | data_len bytes
 * +--------------------+
 * |  checksum          | 1 byte
 * +--------------------+
 */
static struct ipmi_rq_entry *
ipmi_lan_build_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	struct rmcp_hdr rmcp = {
		.ver		= RMCP_VERSION_1,
		.class		= RMCP_CLASS_IPMI,
		.seq		= 0xff,
	};
	uint8_t * msg, * temp;
	int cs, mp, tmp;
	int ap = 0;
	int len = 0;
	int cs2 = 0;
	struct ipmi_rq_entry * entry;
	struct ipmi_session * s = intf->session;
	static int curr_seq = 0;
	uint8_t our_address = intf->my_addr;
	uint8_t bridge_request = 0;

	if (our_address == 0)
		our_address = IPMI_BMC_SLAVE_ADDR;

	if (curr_seq >= 64)
		curr_seq = 0;

	entry = ipmi_req_add_entry(intf, req);
	if (entry == NULL)
		return NULL;

	len = req->msg.data_len + 29;
	if (s->active && s->authtype)
		len += 16;
	msg = malloc(len);
	if (msg == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(msg, 0, len);

	/* rmcp header */
	memcpy(msg, &rmcp, sizeof(rmcp));
	len = sizeof(rmcp);

	/* ipmi session header */
	msg[len++] = s->active ? s->authtype : 0;

	msg[len++] = s->in_seq & 0xff;
	msg[len++] = (s->in_seq >> 8) & 0xff;
	msg[len++] = (s->in_seq >> 16) & 0xff;
	msg[len++] = (s->in_seq >> 24) & 0xff;
	memcpy(msg+len, &s->session_id, 4);
	len += 4;

	/* ipmi session authcode */
	if (s->active && s->authtype) {
		ap = len;
		memcpy(msg+len, s->authcode, 16);
		len += 16;
	}

	/* message length */
	if ((intf->target_addr == our_address) || !bridge_possible) {
		msg[len++] = req->msg.data_len + 7;
		cs = mp = len;
	} else {
		/* bridged request: encapsulate w/in Send Message */
		bridge_request = 1;
		msg[len++] = req->msg.data_len + 15;
		cs = mp = len;
		msg[len++] = IPMI_BMC_SLAVE_ADDR;
		msg[len++] = IPMI_NETFN_APP << 2;
		tmp = len - cs;
		msg[len++] = ipmi_csum(msg+cs, tmp);
		cs2 = len;
		msg[len++] = IPMI_REMOTE_SWID;
		msg[len++] = curr_seq << 2;
		msg[len++] = 0x34;			/* Send Message rqst */
		entry->req.msg.target_cmd = entry->req.msg.cmd;	/* Save target command */
		entry->req.msg.cmd = 0x34;		/* (fixup request entry) */
		msg[len++] = (0x40|intf->target_channel); /* Track request*/
		cs = len;
	}

	/* ipmi message header */
	msg[len++] = intf->target_addr;
	msg[len++] = req->msg.netfn << 2 | (req->msg.lun & 3);
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);
	cs = len;

	if (!bridge_request)
		msg[len++] = IPMI_REMOTE_SWID;
	else  /* Bridged message */
		msg[len++] = intf->my_addr;

	entry->rq_seq = curr_seq++;
	msg[len++] = entry->rq_seq << 2;
	msg[len++] = req->msg.cmd;

	lprintf(LOG_DEBUG+1, ">> IPMI Request Session Header");
	lprintf(LOG_DEBUG+1, ">>   Authtype   : %s",
	       val2str(s->authtype, ipmi_authtype_session_vals));
	lprintf(LOG_DEBUG+1, ">>   Sequence   : 0x%08lx", (long)s->in_seq);
	lprintf(LOG_DEBUG+1, ">>   Session ID : 0x%08lx", (long)s->session_id);
	lprintf(LOG_DEBUG+1, ">> IPMI Request Message Header");
	lprintf(LOG_DEBUG+1, ">>   Rs Addr    : %02x", intf->target_addr);
	lprintf(LOG_DEBUG+1, ">>   NetFn      : %02x", req->msg.netfn);
	lprintf(LOG_DEBUG+1, ">>   Rs LUN     : %01x", 0);
	lprintf(LOG_DEBUG+1, ">>   Rq Addr    : %02x", IPMI_REMOTE_SWID);
	lprintf(LOG_DEBUG+1, ">>   Rq Seq     : %02x", entry->rq_seq);
	lprintf(LOG_DEBUG+1, ">>   Rq Lun     : %01x", 0);
	lprintf(LOG_DEBUG+1, ">>   Command    : %02x", req->msg.cmd);

	/* message data */
	if (req->msg.data_len) {
 		memcpy(msg+len, req->msg.data, req->msg.data_len);
		len += req->msg.data_len;
	}

	/* second checksum */
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);

	/* bridged request: 2nd checksum */
	if (bridge_request) {
		tmp = len - cs2;
		msg[len++] = ipmi_csum(msg+cs2, tmp);
	}

	if (s->active) {
		/*
		 * s->authcode is already copied to msg+ap but some
		 * authtypes require portions of the ipmi message to
		 * create the authcode so they must be done last.
		 */
		switch (s->authtype) {
		case IPMI_SESSION_AUTHTYPE_MD5:
			temp = ipmi_auth_md5(s, msg+mp, msg[mp-1]);
			memcpy(msg+ap, temp, 16);
			break;
		case IPMI_SESSION_AUTHTYPE_MD2:
			temp = ipmi_auth_md2(s, msg+mp, msg[mp-1]);
			memcpy(msg+ap, temp, 16);
			break;
		}
	}

	if (s->in_seq) {
		s->in_seq++;
		if (s->in_seq == 0)
			s->in_seq++;
	}

	entry->msg_len = len;
	entry->msg_data = msg;

	return entry;
}

static struct ipmi_rs *
ipmi_lan_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	struct ipmi_rq_entry * entry;
	struct ipmi_rs * rsp = NULL;
	int try = 0;

	lprintf(LOG_DEBUG, "ipmi_lan_send_cmd:opened=[%d], open=[%d]",
		intf->opened, intf->open);

	if (intf->opened == 0 && intf->open != NULL) {
		if (intf->open(intf) < 0) {
			lprintf(LOG_DEBUG, "Failed to open LAN interface");
			return NULL;
		}
		lprintf(LOG_DEBUG, "\topened=[%d], open=[%d]",
			intf->opened, intf->open);
	}

	for (;;) {
		entry = ipmi_lan_build_cmd(intf, req);
		if (entry == NULL) {
			lprintf(LOG_ERR, "Aborting send command, unable to build");
			return NULL;
		}

		if (ipmi_lan_send_packet(intf, entry->msg_data, entry->msg_len) < 0) {
			try++;
			usleep(5000);
			continue;
		}

		/* if we are set to noanswer we do not expect response */
		if (intf->noanswer)
			break;

		if (ipmi_oem_active(intf, "intelwv2"))
			ipmi_lan_thump(intf);

		usleep(100);

		rsp = ipmi_lan_poll_recv(intf);
		if (rsp)
			break;

		usleep(5000);
		if (++try >= intf->session->retry) {
			lprintf(LOG_DEBUG, "  No response from remote controller");
			break;
		}
	}

	return rsp;
}

static uint8_t *
ipmi_lan_build_rsp(struct ipmi_intf * intf, struct ipmi_rs * rsp, int * llen)
{
	struct rmcp_hdr rmcp = {
		.ver	= RMCP_VERSION_1,
		.class	= RMCP_CLASS_IPMI,
		.seq	= 0xff,
	};
	struct ipmi_session * s = intf->session;
	int cs, mp, ap = 0, tmp;
	int len;
	uint8_t * msg;

	len = rsp->data_len + 22;
	if (s->active)
		len += 16;

	msg = malloc(len);
	if (msg == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(msg, 0, len);

	/* rmcp header */
	memcpy(msg, &rmcp, 4);
	len = sizeof(rmcp);

	/* ipmi session header */
	msg[len++] = s->active ? s->authtype : 0;

	if (s->in_seq) {
		s->in_seq++;
		if (s->in_seq == 0)
			s->in_seq++;
	}
	memcpy(msg+len, &s->in_seq, 4);
	len += 4;
	memcpy(msg+len, &s->session_id, 4);
	len += 4;

	/* session authcode, if session active and authtype is not none */
	if (s->active && s->authtype) {
		ap = len;
		memcpy(msg+len, s->authcode, 16);
		len += 16;
	}

	/* message length */
	msg[len++] = rsp->data_len + 8;

	/* message header */
	cs = mp = len;
	msg[len++] = IPMI_REMOTE_SWID;
	msg[len++] = rsp->msg.netfn << 2;
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);
	cs = len;
	msg[len++] = IPMI_BMC_SLAVE_ADDR;
	msg[len++] = (rsp->msg.seq << 2) | (rsp->msg.lun & 3);
	msg[len++] = rsp->msg.cmd;

	/* completion code */
	msg[len++] = rsp->ccode;

	/* message data */
	if (rsp->data_len) {
		memcpy(msg+len, rsp->data, rsp->data_len);
		len += rsp->data_len;
	}

	/* second checksum */
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);

	if (s->active) {
		uint8_t * d;
		switch (s->authtype) {
		case IPMI_SESSION_AUTHTYPE_MD5:
			d = ipmi_auth_md5(s, msg+mp, msg[mp-1]);
			memcpy(msg+ap, d, 16);
			break;
		case IPMI_SESSION_AUTHTYPE_MD2:
			d = ipmi_auth_md2(s, msg+mp, msg[mp-1]);
			memcpy(msg+ap, d, 16);
			break;
		}
	}

	*llen = len;
	return msg;
}

static int
ipmi_lan_send_rsp(struct ipmi_intf * intf, struct ipmi_rs * rsp)
{
	uint8_t * msg;
	int len = 0;
	int rv;

	msg = ipmi_lan_build_rsp(intf, rsp, &len);
	if (len <= 0 || msg == NULL) {
		lprintf(LOG_ERR, "Invalid response packet");
		if (msg != NULL)
			free(msg);
		return -1;
	}

	rv = sendto(intf->fd, msg, len, 0,
		    (struct sockaddr *)&intf->session->addr,
		    intf->session->addrlen);
	if (rv < 0) {
		lprintf(LOG_ERR, "Packet send failed");
		if (msg != NULL)
			free(msg);
		return -1;
	}

	if (msg != NULL)
		free(msg);
	return 0;
}

/* send a get device id command to keep session active */
static int
ipmi_lan_keepalive(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req = { msg: {
		netfn: IPMI_NETFN_APP,
		cmd: 1,
	}};

	if (!intf->opened)
		return 0;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL)
		return -1;
	if (rsp->ccode > 0)
		return -1;

	return 0;
}

/*
 * IPMI Get Channel Authentication Capabilities Command
 */
static int
ipmi_get_auth_capabilities_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipmi_session * s = intf->session;
	uint8_t msg_data[2];

	msg_data[0] = IPMI_LAN_CHANNEL_E;
	msg_data[1] = s->privlvl;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;
	req.msg.cmd      = 0x38;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_INFO, "Get Auth Capabilities command failed");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "get_auth_capabilities");

	if (rsp->ccode > 0) {
		lprintf(LOG_INFO, "Get Auth Capabilities command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_DEBUG, "Channel %02x Authentication Capabilities:",
		rsp->data[0]);
	lprintf(LOG_DEBUG, "  Privilege Level : %s",
		val2str(req.msg.data[1], ipmi_privlvl_vals));
	lprintf(LOG_DEBUG, "  Auth Types      : %s%s%s%s%s",
		(rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		(rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		(rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		(rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		(rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
	lprintf(LOG_DEBUG, "  Per-msg auth    : %sabled",
		(rsp->data[2] & IPMI_AUTHSTATUS_PER_MSG_DISABLED) ?
		"dis" : "en");
	lprintf(LOG_DEBUG, "  User level auth : %sabled",
		(rsp->data[2] & IPMI_AUTHSTATUS_PER_USER_DISABLED) ?
		"dis" : "en");
	lprintf(LOG_DEBUG, "  Non-null users  : %sabled",
		(rsp->data[2] & IPMI_AUTHSTATUS_NONNULL_USERS_ENABLED) ?
		"en" : "dis");
	lprintf(LOG_DEBUG, "  Null users      : %sabled",
		(rsp->data[2] & IPMI_AUTHSTATUS_NULL_USERS_ENABLED) ?
		"en" : "dis");
	lprintf(LOG_DEBUG, "  Anonymous login : %sabled",
		(rsp->data[2] & IPMI_AUTHSTATUS_ANONYMOUS_USERS_ENABLED) ?
		"en" : "dis");
	lprintf(LOG_DEBUG, "");

	s->authstatus = rsp->data[2];

	if (s->password &&
	    (s->authtype_set == 0 ||
	     s->authtype_set == IPMI_SESSION_AUTHTYPE_MD5) &&
	    (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD5))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_MD5;
	}
	else if (s->password &&
		 (s->authtype_set == 0 ||
		  s->authtype_set == IPMI_SESSION_AUTHTYPE_MD2) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD2))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_MD2;
	}
	else if (s->password &&
		 (s->authtype_set == 0 ||
		  s->authtype_set == IPMI_SESSION_AUTHTYPE_PASSWORD) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_PASSWORD;
	}
	else if (s->password &&
		 (s->authtype_set == 0 ||
		  s->authtype_set == IPMI_SESSION_AUTHTYPE_OEM) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_OEM))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_OEM;
	}
	else if ((s->authtype_set == 0 ||
		  s->authtype_set == IPMI_SESSION_AUTHTYPE_NONE) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_NONE))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_NONE;
	}
	else {
		if (!(rsp->data[1] & 1<<s->authtype_set))
			lprintf(LOG_ERR, "Authentication type %s not supported",
			       val2str(s->authtype_set, ipmi_authtype_session_vals));
		else
			lprintf(LOG_ERR, "No supported authtypes found");

		return -1;
	}

	lprintf(LOG_DEBUG, "Proceeding with AuthType %s",
		val2str(s->authtype, ipmi_authtype_session_vals));

	return 0;
}

/*
 * IPMI Get Session Challenge Command
 * returns a temporary session ID and 16 byte challenge string
 */
static int
ipmi_get_session_challenge_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipmi_session * s = intf->session;
	uint8_t msg_data[17];

	memset(msg_data, 0, 17);
	msg_data[0] = s->authtype;
	memcpy(msg_data+1, s->username, 16);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x39;
	req.msg.data		= msg_data;
	req.msg.data_len	= 17; /* 1 byte for authtype, 16 for user */

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Session Challenge command failed");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "get_session_challenge");

	if (rsp->ccode > 0) {
		switch (rsp->ccode) {
		case 0x81:
			lprintf(LOG_ERR, "Invalid user name");
			break;
		case 0x82:
			lprintf(LOG_ERR, "NULL user name not enabled");
			break;
		default:
			lprintf(LOG_ERR, "Get Session Challenge command failed: %s",
				val2str(rsp->ccode, completion_code_vals));
		}
		return -1;
	}

	memcpy(&s->session_id, rsp->data, 4);
	memcpy(s->challenge, rsp->data + 4, 16);

	lprintf(LOG_DEBUG, "Opening Session");
	lprintf(LOG_DEBUG, "  Session ID      : %08lx", (long)s->session_id);
	lprintf(LOG_DEBUG, "  Challenge       : %s", buf2str(s->challenge, 16));

	return 0;
}

/*
 * IPMI Activate Session Command
 */
static int
ipmi_activate_session_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	struct ipmi_session * s = intf->session;
	uint8_t msg_data[22];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x3a;

	msg_data[0] = s->authtype;
	msg_data[1] = s->privlvl;

	/* supermicro oem authentication hack */
	if (ipmi_oem_active(intf, "supermicro")) {
		uint8_t * special = ipmi_auth_special(s);
		memcpy(s->authcode, special, 16);
		memset(msg_data + 2, 0, 16);
		lprintf(LOG_DEBUG, "  OEM Auth        : %s",
			buf2str(special, 16));
	} else {
		memcpy(msg_data + 2, s->challenge, 16);
	}

	/* setup initial outbound sequence number */
	get_random(msg_data+18, 4);

	req.msg.data = msg_data;
	req.msg.data_len = 22;

	s->active = 1;

	lprintf(LOG_DEBUG, "  Privilege Level : %s",
		val2str(msg_data[1], ipmi_privlvl_vals));
	lprintf(LOG_DEBUG, "  Auth Type       : %s",
		val2str(s->authtype, ipmi_authtype_session_vals));

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Activate Session command failed");
		s->active = 0;
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "activate_session");

	if (rsp->ccode) {
		fprintf(stderr, "Activate Session error:");
		switch (rsp->ccode) {
		case 0x81:
			lprintf(LOG_ERR, "\tNo session slot available");
			break;
		case 0x82:
			lprintf(LOG_ERR, "\tNo slot available for given user - "
				"limit reached");
			break;
		case 0x83:
			lprintf(LOG_ERR, "\tNo slot available to support user "
				"due to maximum privilege capacity");
			break;
		case 0x84:
			lprintf(LOG_ERR, "\tSession sequence out of range");
			break;
		case 0x85:
			lprintf(LOG_ERR, "\tInvalid session ID in request");
			break;
		case 0x86:
			lprintf(LOG_ERR, "\tRequested privilege level "
				"exceeds limit");
			break;
		case 0xd4:
			lprintf(LOG_ERR, "\tInsufficient privilege level");
			break;
		default:
			lprintf(LOG_ERR, "\t%s",
				val2str(rsp->ccode, completion_code_vals));
		}
		return -1;
	}

	memcpy(&s->session_id, rsp->data + 1, 4);
	s->in_seq = rsp->data[8] << 24 | rsp->data[7] << 16 | rsp->data[6] << 8 | rsp->data[5];
	if (s->in_seq == 0)
		++s->in_seq;

	if (s->authstatus & IPMI_AUTHSTATUS_PER_MSG_DISABLED)
		s->authtype = IPMI_SESSION_AUTHTYPE_NONE;
	else if (s->authtype != (rsp->data[0] & 0xf)) {
		lprintf(LOG_ERR, "Invalid Session AuthType %s in response",
			val2str(s->authtype, ipmi_authtype_session_vals));
		return -1;
	}

	bridge_possible = 1;

	lprintf(LOG_DEBUG, "\nSession Activated");
	lprintf(LOG_DEBUG, "  Auth Type       : %s",
		val2str(rsp->data[0], ipmi_authtype_session_vals));
	lprintf(LOG_DEBUG, "  Max Priv Level  : %s",
		val2str(rsp->data[9], ipmi_privlvl_vals));
	lprintf(LOG_DEBUG, "  Session ID      : %08lx", (long)s->session_id);
	lprintf(LOG_DEBUG, "  Inbound Seq     : %08lx\n", (long)s->in_seq);

	return 0;
}


/*
 * IPMI Set Session Privilege Level Command
 */
static int
ipmi_set_session_privlvl_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t privlvl = intf->session->privlvl;
	uint8_t backup_bridge_possible = bridge_possible;

	if (privlvl <= IPMI_SESSION_PRIV_USER)
		return 0;	/* no need to set higher */

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x3b;
	req.msg.data		= &privlvl;
	req.msg.data_len	= 1;

	bridge_possible = 0;
	rsp = intf->sendrecv(intf, &req);
	bridge_possible = backup_bridge_possible;

	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set Session Privilege Level to %s failed",
			val2str(privlvl, ipmi_privlvl_vals));
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "set_session_privlvl");

	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Session Privilege Level to %s failed: %s",
			val2str(privlvl, ipmi_privlvl_vals),
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_DEBUG, "Set Session Privilege Level to %s\n",
		val2str(rsp->data[0], ipmi_privlvl_vals));

	return 0;
}

static int
ipmi_close_session_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4];
	uint32_t session_id = intf->session->session_id;

	if (intf->session->active == 0)
		return -1;

	intf->target_addr = IPMI_BMC_SLAVE_ADDR;
	bridge_possible = 0;  /* Not a bridge message */

	memcpy(&msg_data, &session_id, 4);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x3c;
	req.msg.data		= msg_data;
	req.msg.data_len	= 4;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Close Session command failed");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "close_session");

	if (rsp->ccode == 0x87) {
		lprintf(LOG_ERR, "Failed to Close Session: invalid "
			"session ID %08lx", (long)session_id);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Close Session command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_DEBUG, "Closed Session %08lx\n", (long)session_id);

	return 0;
}

/*
 * IPMI LAN Session Activation (IPMI spec v1.5 section 12.9)
 *
 * 1. send "RMCP Presence Ping" message, response message will
 *    indicate whether the platform supports IPMI
 * 2. send "Get Channel Authentication Capabilities" command
 *    with AUTHTYPE = none, response packet will contain information
 *    about supported challenge/response authentication types
 * 3. send "Get Session Challenge" command with AUTHTYPE = none
 *    and indicate the authentication type in the message, response
 *    packet will contain challenge string and temporary session ID.
 * 4. send "Activate Session" command, authenticated with AUTHTYPE
 *    sent in previous message.  Also sends the initial value for
 *    the outbound sequence number for BMC.
 * 5. BMC returns response confirming session activation and
 *    session ID for this session and initial inbound sequence.
 */
static int
ipmi_lan_activate_session(struct ipmi_intf * intf)
{
	int rc;

	/* don't fail on ping because its not always supported.
	 * Supermicro's IPMI LAN 1.5 cards don't tolerate pings.
	 */
	if (!ipmi_oem_active(intf, "supermicro"))
		ipmi_lan_ping(intf);

	/* Some particular Intel boards need special help
	 */
	if (ipmi_oem_active(intf, "intelwv2"))
		ipmi_lan_thump_first(intf);

	rc = ipmi_get_auth_capabilities_cmd(intf);
	if (rc < 0) {
		sleep(1);
		rc = ipmi_get_auth_capabilities_cmd(intf);
		if (rc < 0)
			goto fail;
	}

	rc = ipmi_get_session_challenge_cmd(intf);
	if (rc < 0)
		goto fail;

	rc = ipmi_activate_session_cmd(intf);
	if (rc < 0)
		goto fail;

	intf->abort = 0;

	rc = ipmi_set_session_privlvl_cmd(intf);
	if (rc < 0)
		goto fail;

	return 0;

 fail:
	lprintf(LOG_ERR, "Error: Unable to establish LAN session");
	return -1;
}

static void
ipmi_lan_close(struct ipmi_intf * intf)
{
	if (intf->abort == 0)
		ipmi_close_session_cmd(intf);

	if (intf->fd >= 0)
		close(intf->fd);

	ipmi_req_clear_entries();

	if (intf->session != NULL) {
		free(intf->session);
		intf->session = NULL;
	}

	intf->opened = 0;
	intf = NULL;
}

static int
ipmi_lan_open(struct ipmi_intf * intf)
{
	int rc;
	struct ipmi_session *s;

	if (intf == NULL || intf->session == NULL)
		return -1;
	s = intf->session;

	if (s->port == 0)
		s->port = IPMI_LAN_PORT;
	if (s->privlvl == 0)
		s->privlvl = IPMI_SESSION_PRIV_ADMIN;
	if (s->timeout == 0)
		s->timeout = IPMI_LAN_TIMEOUT;
	if (s->retry == 0)
		s->retry = IPMI_LAN_RETRY;

	if (s->hostname == NULL || strlen((const char *)s->hostname) == 0) {
		lprintf(LOG_ERR, "No hostname specified!");
		return -1;
	}

	intf->abort = 1;

	/* open port to BMC */
	memset(&s->addr, 0, sizeof(struct sockaddr_in));
	s->addr.sin_family = AF_INET;
	s->addr.sin_port = htons(s->port);

	rc = inet_pton(AF_INET, (const char *)s->hostname, &s->addr.sin_addr);
	if (rc <= 0) {
		struct hostent *host = gethostbyname((const char *)s->hostname);
		if (host == NULL) {
			lprintf(LOG_ERR, "Address lookup for %s failed",
				s->hostname);
			return -1;
		}
		s->addr.sin_family = host->h_addrtype;
		memcpy(&s->addr.sin_addr, host->h_addr, host->h_length);
	}

	lprintf(LOG_DEBUG, "IPMI LAN host %s port %d",
		s->hostname, ntohs(s->addr.sin_port));

	intf->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (intf->fd < 0) {
		lperror(LOG_ERR, "Socket failed");
		return -1;
	}

	/* connect to UDP socket so we get async errors */
	rc = connect(intf->fd, (struct sockaddr *)&s->addr,
		     sizeof(struct sockaddr_in));
	if (rc < 0) {
		lperror(LOG_ERR, "Connect failed");
		intf->close(intf);
		return -1;
	}

	intf->opened = 1;

	/* try to open session */
	rc = ipmi_lan_activate_session(intf);
	if (rc < 0) {
		intf->close(intf);
		intf->opened = 0;
		return -1;
	}

	return intf->fd;
}

static int
ipmi_lan_setup(struct ipmi_intf * intf)
{
	intf->session = malloc(sizeof(struct ipmi_session));
	if (intf->session == NULL) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return -1;
	}
	memset(intf->session, 0, sizeof(struct ipmi_session));
	return 0;
}
