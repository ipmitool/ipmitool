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

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>
#include <fcntl.h>

#include <ipmitool/helper.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#if HAVE_CONFIG_H
# include <config.h>
#endif

#include "lan.h"
#include "md5.h"
#include "rmcp.h"
#include "asf.h"

extern const struct valstr ipmi_privlvl_vals[];
extern const struct valstr ipmi_authtype_vals[];

struct ipmi_rq_entry * ipmi_req_entries;
static struct ipmi_rq_entry * ipmi_req_entries_tail;

extern int verbose;

const struct valstr ipmi_authtype_session_vals[] = {
	{ IPMI_SESSION_AUTHTYPE_NONE,     "NONE" },
	{ IPMI_SESSION_AUTHTYPE_MD2,      "MD2" },
	{ IPMI_SESSION_AUTHTYPE_MD5,      "MD5" },
	{ IPMI_SESSION_AUTHTYPE_PASSWORD, "PASSWORD" },
	{ IPMI_SESSION_AUTHTYPE_OEM,      "OEM" },
	{ 0,                               NULL },
};

static int recv_timeout = IPMI_LAN_TIMEOUT;
static sigjmp_buf jmpbuf;

static int ipmi_lan_send_packet(struct ipmi_intf * intf, unsigned char * data, int data_len);
static struct ipmi_rs * ipmi_lan_recv_packet(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lan_poll_recv(struct ipmi_intf * intf);

struct ipmi_intf ipmi_lan_intf = {
	.open     = ipmi_lan_open,
	.close    = ipmi_lan_close,
	.sendrecv = ipmi_lan_send_cmd,
};

static void
query_alarm(int signo)
{
	siglongjmp(jmpbuf, 1);
}

static const struct valstr ipmi_channel_protocol_vals[] = {
	{ 0x00, "reserved" },
	{ 0x01, "IPMB-1.0" },
	{ 0x02, "ICMB-1.0" },
	{ 0x03, "reserved" },
	{ 0x04, "IPMI-SMBus" },
	{ 0x05, "KCS" },
	{ 0x06, "SMIC" },
	{ 0x07, "BT-10" },
	{ 0x08, "BT-15" },
	{ 0x09, "TMode" },
	{ 0x1c, "OEM 1" },
	{ 0x1d, "OEM 2" },
	{ 0x1e, "OEM 3" },
	{ 0x1f, "OEM 4" },
	{ 0x00, NULL },
};

static const struct valstr ipmi_channel_medium_vals[] = {
	{ 0x00, "reserved" },
	{ 0x01, "IPMB (I2C)" },
	{ 0x02, "ICMB v1.0" },
	{ 0x03, "ICMB v0.9" },
	{ 0x04, "802.3 LAN" },
	{ 0x05, "Serial/Modem" },
	{ 0x06, "Other LAN" },
	{ 0x07, "PCI SMBus" },
	{ 0x08, "SMBus v1.0/v1.1" },
	{ 0x09, "SMBus v2.0" },
	{ 0x0a, "USB 1.x" },
	{ 0x0b, "USB 2.x" },
	{ 0x0c, "System Interface" },
	{ 0x00, NULL },
};

static struct ipmi_rq_entry *
ipmi_req_lookup_entry(unsigned char seq, unsigned char cmd)
{
	struct ipmi_rq_entry * e = ipmi_req_entries;
	while (e && (e->rq_seq != seq || e->req.msg.cmd != cmd)) {
		if (e == e->next)
			return NULL;
		e = e->next;
	}
	return e;
}

static void
ipmi_req_remove_entry(unsigned char seq, unsigned char cmd)
{
	struct ipmi_rq_entry * p, * e;

	e = p = ipmi_req_entries;

	while (e && (e->rq_seq != seq || e->req.msg.cmd != cmd)) {
		p = e;
		e = e->next;
	}
	if (e) {
		if (verbose > 3)
			printf("removed list entry seq=0x%02x cmd=0x%02x\n", seq, cmd);
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
		if (verbose > 3)
			printf("cleared list entry seq=0x%02x cmd=0x%02x\n",
			       e->rq_seq, e->req.msg.cmd);
		p = e->next;
		free(e);
		e = p;
	}
}

static int
get_random(void *data, unsigned int len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	int rv;

	if (fd == -1)
		return errno;

	rv = read(fd, data, len);

	close(fd);
	return rv;
}

int ipmi_lan_send_packet(struct ipmi_intf * intf, unsigned char * data, int data_len)
{
	if (verbose > 2)
		printbuf(data, data_len, "send_packet");

	return send(intf->fd, data, data_len, 0);
}

struct ipmi_rs * ipmi_lan_recv_packet(struct ipmi_intf * intf)
{
	static struct ipmi_rs rsp;
	int rc;

	/* setup alarm timeout */
	if (sigsetjmp(jmpbuf, 1) != 0) {
		alarm(0);
		return NULL;
	}

	alarm(IPMI_LAN_TIMEOUT);
	rc = recv(intf->fd, &rsp.data, BUF_SIZE, 0);
	alarm(0);

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
	if (rc < 0) {
		alarm(IPMI_LAN_TIMEOUT);
		rc = recv(intf->fd, &rsp.data, BUF_SIZE, 0);
		alarm(0);
		if (rc < 0) {
			perror("recv failed");
			return NULL;
		}
	}
	rsp.data[rc] = '\0';
	rsp.data_len = rc;

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
	struct rmcp_pong {
		struct rmcp_hdr rmcp;
		struct asf_hdr asf;
		uint32_t iana;
		uint32_t oem;
		unsigned char sup_entities;
		unsigned char sup_interact;
		unsigned char reserved[6];
	} * pong;

	if (!rsp)
		return -1;

	pong = (struct rmcp_pong *)rsp->data;

	if (verbose > 1)
		printf("Received IPMI/RMCP response packet: \n"
		       "  IPMI%s Supported\n"
		       "  ASF Version %s\n"
		       "  RMCP Version %s\n"
		       "  RMCP Sequence %d\n"
		       "  IANA Enterprise %d\n\n",
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
int
ipmi_lan_ping(struct ipmi_intf * intf)
{
	struct asf_hdr asf_ping = {
		.iana	= ASF_RMCP_IANA,
		.type	= ASF_TYPE_PING,
	};
	struct rmcp_hdr rmcp_ping = {
		.ver	= RMCP_VERSION_1,
		.class	= RMCP_CLASS_ASF,
		.seq	= 0xff,
	};
	unsigned char * data;
	int len = sizeof(rmcp_ping) + sizeof(asf_ping);
	int rv;

	data = malloc(len);
	memset(data, 0, len);
	memcpy(data, &rmcp_ping, sizeof(rmcp_ping));
	memcpy(data+sizeof(rmcp_ping), &asf_ping, sizeof(asf_ping));

	if (verbose > 1)
		printf("Sending IPMI/RMCP presence ping packet\n");

	rv = ipmi_lan_send_packet(intf, data, len);

	free(data);

	if (rv < 0) {
		printf("Unable to send IPMI presence ping packet\n");
		return -1;
	}

	if (!ipmi_lan_poll_recv(intf))
		return 0;

	return 1;
}

/* special packet, no idea what it does */
ipmi_lan_first(struct ipmi_intf * intf)
{
	unsigned char data[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				   0x07, 0x20, 0x18, 0xc8, 0xc2, 0x01, 0x01, 0x3c };
	ipmi_lan_send_packet(intf, data, 16);
	return 0;
}

static int
ipmi_lan_pedantic(struct ipmi_intf * intf)
{
	unsigned char data[10] = "dummy";
	ipmi_lan_send_packet(intf, data, 10);
	return 0;
}

/*
 * multi-session authcode generation for md5
 * H(password + session_id + msg + session_seq + password)
 */
static unsigned char *
ipmi_auth_md5(struct ipmi_session * s, unsigned char * data, int data_len)
{
	md5_state_t state;
	static md5_byte_t digest[16];
	uint32_t temp;

	memset(digest, 0, 16);
	memset(&state, 0, sizeof(md5_state_t));

	md5_init(&state);

	md5_append(&state, (const md5_byte_t *)s->authcode, 16);
	md5_append(&state, (const md5_byte_t *)&s->session_id, 4);
	md5_append(&state, (const md5_byte_t *)data, data_len);

#if WORDS_BIGENDIAN
	temp = BSWAP_32(s->in_seq);
#else
	temp = s->in_seq;
#endif
	md5_append(&state, (const md5_byte_t *)&temp, 4);
	md5_append(&state, (const md5_byte_t *)s->authcode, 16);

	md5_finish(&state, digest);

	if (verbose > 3)
		printf("  MD5 AuthCode    : %s\n", buf2str(digest, 16));
	return digest;
}


static struct ipmi_rs *
ipmi_lan_poll_recv(struct ipmi_intf * intf)
{
	struct rmcp_hdr rmcp_rsp;
	struct ipmi_rs * rsp;
	struct ipmi_rq_entry * entry;
	int x, rv;

	rsp = ipmi_lan_recv_packet(intf);
	while (rsp) {

		/* parse response headers */
		memcpy(&rmcp_rsp, rsp->data, 4);

		if (rmcp_rsp.class == RMCP_CLASS_ASF) {
			/* might be ping response packet */
			rv = ipmi_handle_pong(intf, rsp);
			return (rv <= 0) ? NULL : rsp;
		}
			
		if (rmcp_rsp.class != RMCP_CLASS_IPMI) {
			if (verbose > 1)
				printf("Invalid RMCP class: %x\n", rmcp_rsp.class);
			rsp = ipmi_lan_recv_packet(intf);
			continue;
		}

		x = 4;
		rsp->session.authtype = rsp->data[x++];
		memcpy(&rsp->session.seq, rsp->data+x, 4);
		x += 4;
		memcpy(&rsp->session.id, rsp->data+x, 4);
		x += 4;

		if (intf->session->active && intf->session->authtype)
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

		if (verbose > 2) {
			printbuf(rsp->data, rsp->data_len, "ipmi message header");
			printf("<< IPMI Response Session Header\n");
			printf("<<   Authtype   : %s\n",
			       val2str(rsp->session.authtype, ipmi_authtype_session_vals));
			printf("<<   Sequence   : 0x%08lx\n", rsp->session.seq);
			printf("<<   Session ID : 0x%08lx\n", rsp->session.id);
			
			printf("<< IPMI Response Message Header\n");
			printf("<<   Rq Addr    : %02x\n", rsp->payload.ipmi_response.rq_addr);
			printf("<<   NetFn      : %02x\n", rsp->payload.ipmi_response.netfn);
			printf("<<   Rq LUN     : %01x\n", rsp->payload.ipmi_response.rq_lun);
			printf("<<   Rs Addr    : %02x\n", rsp->payload.ipmi_response.rs_addr);
			printf("<<   Rq Seq     : %02x\n", rsp->payload.ipmi_response.rq_seq);
			printf("<<   Rs Lun     : %01x\n", rsp->payload.ipmi_response.rs_lun);
			printf("<<   Command    : %02x\n", rsp->payload.ipmi_response.cmd);
			printf("<<   Compl Code : 0x%02x\n", rsp->ccode);
		}

		/* now see if we have oustanding entry in request list */
		entry = ipmi_req_lookup_entry(rsp->payload.ipmi_response.rq_seq,
									  rsp->payload.ipmi_response.cmd);
		if (entry) {
			if (verbose > 2)
				printf("IPMI Request Match found\n");
			ipmi_req_remove_entry(rsp->payload.ipmi_response.rq_seq,
								  rsp->payload.ipmi_response.cmd);
		} else {
			if (verbose)
				printf("WARNING: IPMI Request Match NOT FOUND!\n");
			rsp = ipmi_lan_recv_packet(intf);
			continue;
		}			

		break;
	}

	/* shift response data to start of array */
	if (rsp && rsp->data_len > x) {
		rsp->data_len -= x + 1;
		memmove(rsp->data, rsp->data + x, rsp->data_len);
		memset(rsp->data + rsp->data_len, 0, BUF_SIZE - rsp->data_len);
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
	unsigned char * msg;
	int cs, mp, ap = 0, len = 0, tmp;
	struct ipmi_rq_entry * entry;
	struct ipmi_session * s = intf->session;
	static curr_seq = 0;

	if (curr_seq >= 64)
		curr_seq = 0;

	entry = malloc(sizeof(struct ipmi_rq_entry));
	memset(entry, 0, sizeof(struct ipmi_rq_entry));
	memcpy(&entry->req, req, sizeof(struct ipmi_rq));

	entry->intf = intf;

	if (!ipmi_req_entries) {
		ipmi_req_entries = entry;
	} else {
		ipmi_req_entries_tail->next = entry;
	}
	ipmi_req_entries_tail = entry;
	if (verbose > 3)
		printf("added list entry seq=0x%02x cmd=0x%02x\n",
		       curr_seq, req->msg.cmd);
	
	len = req->msg.data_len + 21;
	if (s->active && s->authtype)
		len += 16;
	msg = malloc(len);
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
	msg[len++] = req->msg.data_len + 7;

	/* ipmi message header */
	cs = mp = len;
	msg[len++] = IPMI_BMC_SLAVE_ADDR;
	msg[len++] = req->msg.netfn << 2;
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);
	cs = len;
	msg[len++] = IPMI_REMOTE_SWID;

	entry->rq_seq = curr_seq++;
	msg[len++] = entry->rq_seq << 2;
	msg[len++] = req->msg.cmd;

	if (verbose > 2) {
		printf(">> IPMI Request Session Header\n");
		printf(">>   Authtype   : %s\n",
		       val2str(s->authtype, ipmi_authtype_session_vals));
		printf(">>   Sequence   : 0x%08lx\n", s->in_seq);
		printf(">>   Session ID : 0x%08lx\n", s->session_id);
		
		printf(">> IPMI Request Message Header\n");
		printf(">>   Rs Addr    : %02x\n", IPMI_BMC_SLAVE_ADDR);
		printf(">>   NetFn      : %02x\n", req->msg.netfn);
		printf(">>   Rs LUN     : %01x\n", 0);
		printf(">>   Rq Addr    : %02x\n", IPMI_REMOTE_SWID);
		printf(">>   Rq Seq     : %02x\n", entry->rq_seq);
		printf(">>   Rq Lun     : %01x\n", 0);
		printf(">>   Command    : %02x\n", req->msg.cmd);
	}

	/* message data */
	if (req->msg.data_len) {
		memcpy(msg+len, req->msg.data, req->msg.data_len);
		len += req->msg.data_len;
	}

	/* second checksum */
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);

	if (s->active &&
	    s->authtype == IPMI_SESSION_AUTHTYPE_MD5) {
		unsigned char * d = ipmi_auth_md5(intf->session, msg+mp, msg[mp-1]);
		memcpy(msg+ap, d, 16);
	}

	if (s->in_seq) {
		s->in_seq++;
		if (!s->in_seq)
			s->in_seq++;
	}

	entry->msg_len = len;
	entry->msg_data = msg;

	return entry;
}

struct ipmi_rs *
ipmi_lan_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req)
{
	struct ipmi_rq_entry * entry;
	struct ipmi_rs * rsp;
	int try = 0;

	if (!intf->opened) {
		intf->opened = 1;
		if (intf->open(intf) < 0) {
			intf->opened = 0;
			return NULL;
		}
	}

	entry = ipmi_lan_build_cmd(intf, req);
	if (!entry) {
		printf("Aborting send command, unable to build.\n");
		return NULL;
	}

	while (try < IPMI_LAN_RETRY) {
		if (ipmi_lan_send_packet(intf, entry->msg_data, entry->msg_len) < 0) {
			try++;
			usleep(5000);
			continue;
		}

		if (intf->pedantic)
			ipmi_lan_pedantic(intf);

		usleep(100);

		rsp = ipmi_lan_poll_recv(intf);
		if (rsp)
			break;

		usleep(5000);
		try++;
	}

	return rsp;
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
	unsigned char msg_data[2];

	msg_data[0] = IPMI_LAN_CHANNEL_E;
	msg_data[1] = s->privlvl;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;
	req.msg.cmd      = 0x38;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		if (verbose)
			printf("error in Get Auth Capabilities Command\n");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "get_auth_capabilities");

	if (rsp->ccode) {
		printf("Get Auth Capabilities error: %02x\n", rsp->ccode);
		return -1;
	}

	if (verbose > 1) {
		printf("Channel %02x Authentication Capabilities:\n",
		       rsp->data[0]);
		printf("  Privilege Level : %s\n",
		       val2str(req.msg.data[1], ipmi_privlvl_vals));
		printf("  Auth Types      : ");
		if (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_NONE)
			printf("NONE ");
		if (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD2)
			printf("MD2 ");
		if (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD5)
			printf("MD5 ");
		if (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD)
			printf("PASSWORD ");
		if (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_OEM)
			printf("OEM ");
		printf("\n");
		printf("  Per-msg auth    : %sabled\n",
		       (rsp->data[2] & 1<<4) ? "en" : "dis");
		printf("  User level auth : %sabled\n",
		       (rsp->data[2] & 1<<3) ? "en" : "dis");
		printf("  Non-null users  : %sabled\n",
		       (rsp->data[2] & 1<<2) ? "en" : "dis");
		printf("  Null users      : %sabled\n",
		       (rsp->data[2] & 1<<1) ? "en" : "dis");
		printf("  Anonymous login : %sabled\n",
		       (rsp->data[2] & 1<<0) ? "en" : "dis");
		printf("\n");
	}

	if (s->password &&
	    rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD5) {
		s->authtype = IPMI_SESSION_AUTHTYPE_MD5;
	}
	else if (s->password &&
		 rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) {
		s->authtype = IPMI_SESSION_AUTHTYPE_PASSWORD;
	}
	else if (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_NONE) {
		s->authtype = IPMI_SESSION_AUTHTYPE_NONE;
	}
	else {
		printf("No supported authtypes found!\n");
		return -1;
	}

	if (verbose > 1)
		printf("Proceeding with AuthType %s\n",
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
	unsigned char msg_data[17];

	memset(msg_data, 0, 17);
	msg_data[0] = s->authtype;
	memcpy(msg_data+1, s->username, 16);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x39;
	req.msg.data		= msg_data;
	req.msg.data_len	= 17; /* 1 byte for authtype, 16 for user */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("error in Get Session Challenge Command\n");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "get_session_challenge");

	if (rsp->ccode) {
		printf("Get Session Challenge error: ");
		switch (rsp->ccode) {
		case 0x81:
			printf("Invalid user name\n");
			break;
		case 0x82:
			printf("NULL user name not enabled\n");
			break;
		default:
			printf("%02x\n", rsp->ccode);
		}
		return -1;
	}

	memcpy(&s->session_id, rsp->data, 4);
	memcpy(s->challenge, rsp->data + 4, 16);

	if (verbose > 1) {
		printf("Opening Session\n");
		printf("  Session ID      : %08lx\n",
		       s->session_id);
		printf("  Challenge       : %s\n",
		       buf2str(s->challenge, 16));
	}


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
	unsigned char msg_data[22];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x3a;

	msg_data[0] = s->authtype;
	msg_data[1] = s->privlvl;
	memcpy(msg_data + 2, s->challenge, 16);

	/* setup initial outbound sequence number */
	get_random(msg_data+18, 4);

	req.msg.data = msg_data;
	req.msg.data_len = 22;

	s->active = 1;

	if (verbose > 1) {
		printf("  Privilege Level : %s\n",
		       val2str(msg_data[1], ipmi_privlvl_vals));
		printf("  Auth Type       : %s\n",
		       val2str(s->authtype, ipmi_authtype_session_vals));
	}

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("Error in Activate Session Command\n");
		s->active = 0;
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "activate_session");

	if (rsp->ccode) {
		printf("Activate Session error: ");
		switch (rsp->ccode) {
		case 0x81:
			printf("No session slot available\n");
			break;
		case 0x82:
			printf("No slot available for given user - "
			       "limit reached\n");
			break;
		case 0x83:
			printf("No slot available to support user "
			       "due to maximum privilege capacity\n");
			break;
		case 0x84:
			printf("Session sequence number out of range\n");
			break;
		case 0x85:
			printf("Invalid session ID in request\n");
			break;
		case 0x86:
			printf("Requested privilege level exceeds limit\n");
			break;
		case 0xd4:
			printf("Insufficient privilege level\n");
			break;
		default:
			printf("%02x\n", rsp->ccode);
		}
		return -1;
	}

	memcpy(&s->session_id, rsp->data + 1, 4);
	s->in_seq = rsp->data[8] << 24 | rsp->data[7] << 16 | rsp->data[6] << 8 | rsp->data[5];

	if (verbose > 1) {
		printf("\nSession Activated\n");
		printf("  Auth Type       : %s\n",
		       val2str(rsp->data[0], ipmi_authtype_session_vals));
		printf("  Max Priv Level  : %s\n",
		       val2str(rsp->data[9], ipmi_privlvl_vals));
		printf("  Session ID      : %08lx\n", s->session_id);
		printf("  Inbound Seq     : %08lx\n", s->in_seq);
		printf("\n");
	}

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
	unsigned char privlvl = intf->session->privlvl;

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x3b;
	req.msg.data		= &privlvl;
	req.msg.data_len	= 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("error in Set Session Privilege Level Command\n");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "set_session_privlvl");

	if (rsp->ccode) {
		printf("Failed to set session privilege level to %s\n\n",
		       val2str(privlvl, ipmi_privlvl_vals));
		return -1;
	}
	if (verbose > 1)
		printf("Set Session Privilege Level to %s\n\n",
		       val2str(rsp->data[0], ipmi_privlvl_vals));
	return 0;
}

static int
impi_close_session_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[4];
	uint32_t session_id = intf->session->session_id;

	if (!intf->session->active)
		return -1;

	memcpy(&msg_data, &session_id, 4);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x3c;
	req.msg.data		= msg_data;
	req.msg.data_len	= 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		printf("error in Close Session Command\n");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "close_session");

	if (rsp->ccode == 0x87) {
		printf("Failed to Close Session: invalid session ID %08lx\n",
		       session_id);
		return -1;
	}
	if (rsp->ccode) {
		printf("Failed to Close Session: %x\n", rsp->ccode);
		return -1;
	}

	if (verbose > 1)
		printf("\nClosed Session %08lx\n\n", session_id);

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

	/* don't fail on ping because its not always supported */
	ipmi_lan_ping(intf);

	if (intf->pedantic)
		ipmi_lan_first(intf);

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

	rc = ipmi_set_session_privlvl_cmd(intf);
	if (rc < 0)
		goto fail;

	intf->abort = 0;

	return 0;

 fail:
	printf("Error: Unable to establish LAN session\n");
	return -1;
}

void ipmi_lan_close(struct ipmi_intf * intf)
{
	if (!intf->abort)
		impi_close_session_cmd(intf);
	if (intf->fd >= 0)
		close(intf->fd);
	ipmi_req_clear_entries();
	if (intf->session)
		free(intf->session);
	intf->session = NULL;
	intf = NULL;
}

int ipmi_lan_open(struct ipmi_intf * intf)
{
	int rc;
	struct sigaction act;
	struct sockaddr_in addr;
	struct ipmi_session *s;

	if (!intf || !intf->session)
		return -1;
	s = intf->session;

	if (!s->port)
		s->port = IPMI_LAN_PORT;
	if (!s->privlvl)
		s->privlvl = IPMI_SESSION_PRIV_USER;

	if (!s->hostname) {
		printf("No hostname specified!\n");
		return -1;
	}

	intf->abort = 1;

	/* open port to BMC */
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(s->port);

	rc = inet_pton(AF_INET, s->hostname, &addr.sin_addr);
	if (rc <= 0) {
		struct hostent *host = gethostbyname(s->hostname);
		if (!host) {
			printf("address lookup failed\n");
			return -1;
		}
		addr.sin_family = host->h_addrtype;
		memcpy(&addr.sin_addr, host->h_addr, host->h_length);
	}

	if (verbose > 1)
		printf("IPMI LAN host %s port %d\n",
		       s->hostname, ntohs(addr.sin_port));

	intf->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (intf->fd < 0) {
		perror("socket failed");
		return -1;
	}

	/* connect to UDP socket so we get async errors */
	rc = connect(intf->fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	if (rc < 0) {
		perror("connect failed");
		intf->close(intf);
		return -1;
	}

	/* setup alarm handler */
	act.sa_handler = query_alarm;
	act.sa_flags = 0;
	if (!sigemptyset(&act.sa_mask) && sigaction(SIGALRM, &act, NULL) < 0) {
		perror("alarm signal");
		intf->close(intf);
		return -1;
	}

	/* try to open session */
	rc = ipmi_lan_activate_session(intf);
	if (rc < 0) {
		intf->close(intf);
		return -1;
	}

	return intf->fd;
}

int lan_intf_setup(struct ipmi_intf ** intf)
{
	*intf = &ipmi_lan_intf;
	(*intf)->session = malloc(sizeof(struct ipmi_session));
	memset((*intf)->session, 0, sizeof(struct ipmi_session));
	return ((*intf)->session) ? 0 : -1;
}

int intf_setup(struct ipmi_intf ** intf) __attribute__ ((weak, alias("lan_intf_setup")));
