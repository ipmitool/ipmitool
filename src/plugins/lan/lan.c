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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
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
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_oem.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/hpm2.h>

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

/*
 * LAN interface is required to support 45 byte request transactions and
 * 42 byte response transactions.
 */
#define IPMI_LAN_MAX_REQUEST_SIZE	38	/* 45 - 7 */
#define IPMI_LAN_MAX_RESPONSE_SIZE	34	/* 42 - 8 */

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
static struct ipmi_rs * ipmi_lan_recv_sol(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lan_send_sol(struct ipmi_intf * intf,
					  struct ipmi_v2_payload * payload);
static struct ipmi_rs * ipmi_lan_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req);
static int ipmi_lan_open(struct ipmi_intf * intf);
static void ipmi_lan_close(struct ipmi_intf * intf);
static int ipmi_lan_ping(struct ipmi_intf * intf);
static void ipmi_lan_set_max_rq_data_size(struct ipmi_intf * intf, uint16_t size);
static void ipmi_lan_set_max_rp_data_size(struct ipmi_intf * intf, uint16_t size);

struct ipmi_intf ipmi_lan_intf = {
	.name = "lan",
	.desc = "IPMI v1.5 LAN Interface",
	.setup = ipmi_lan_setup,
	.open = ipmi_lan_open,
	.close = ipmi_lan_close,
	.sendrecv = ipmi_lan_send_cmd,
	.recv_sol = ipmi_lan_recv_sol,
	.send_sol = ipmi_lan_send_sol,
	.keepalive = ipmi_lan_keepalive,
	.set_max_request_data_size = ipmi_lan_set_max_rq_data_size,
	.set_max_response_data_size = ipmi_lan_set_max_rp_data_size,
	.target_addr = IPMI_BMC_SLAVE_ADDR,
};

static struct ipmi_rq_entry *
ipmi_req_add_entry(struct ipmi_intf * intf, struct ipmi_rq * req, uint8_t req_seq)
{
	struct ipmi_rq_entry * e;

	e = malloc(sizeof(struct ipmi_rq_entry));
	if (!e) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}

	memset(e, 0, sizeof(struct ipmi_rq_entry));
	memcpy(&e->req, req, sizeof(struct ipmi_rq));

	e->intf = intf;
	e->rq_seq = req_seq;

	if (!ipmi_req_entries)
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
		if (!e->next || e == e->next)
			return NULL;
		e = e->next;
	}
	return e;
}

static void
ipmi_req_remove_entry(uint8_t seq, uint8_t cmd)
{
	struct ipmi_rq_entry * p, * e, * saved_next_entry;

	e = p = ipmi_req_entries;

	while (e && (e->rq_seq != seq || e->req.msg.cmd != cmd)) {
		p = e;
		e = e->next;
	}
	if (e) {
		lprintf(LOG_DEBUG+3, "removed list entry seq=0x%02x cmd=0x%02x",
			seq, cmd);
		saved_next_entry = e->next;
		p->next = (p->next == e->next) ? NULL : e->next;
		/* If entry being removed is first in list, fix up list head */
		if (ipmi_req_entries == e) {
			if (ipmi_req_entries != p)
				ipmi_req_entries = p;
			else
				ipmi_req_entries = saved_next_entry;
		}
		/* If entry being removed is last in list, fix up list tail */
		if (ipmi_req_entries_tail == e) {
			if (ipmi_req_entries_tail != p)
				ipmi_req_entries_tail = p;
			else
				ipmi_req_entries_tail = NULL;
		}
		if (e->msg_data) {
			free(e->msg_data);
			e->msg_data = NULL;
		}
		free(e);
		e = NULL;
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
		if (e->next) {
			p = e->next;
			free(e);
			e = p;
		} else {
			free(e);
			e = NULL;
			break;
		}
	}
	ipmi_req_entries = NULL;
	ipmi_req_entries_tail = NULL;
}

static int
get_random(void *data, int len)
{
	int fd = open("/dev/urandom", O_RDONLY);
	int rv;

	if (fd < 0)
		return errno;
	if (len < 0) {
		close(fd);
		return errno; /* XXX: ORLY? */
	}

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
	fd_set read_set;
	fd_set err_set;
	struct timeval tmout;
	int ret;

	FD_ZERO(&read_set);
	FD_SET(intf->fd, &read_set);

	FD_ZERO(&err_set);
	FD_SET(intf->fd, &err_set);

	tmout.tv_sec = intf->ssn_params.timeout;
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

		tmout.tv_sec = intf->ssn_params.timeout;
		tmout.tv_usec = 0;

		ret = select(intf->fd + 1, &read_set, NULL, &err_set, &tmout);
		if (ret < 0 || FD_ISSET(intf->fd, &err_set) || !FD_ISSET(intf->fd, &read_set))
			return NULL;

		ret = recv(intf->fd, &rsp.data, IPMI_BUF_SIZE, 0);
		if (ret < 0)
			return NULL;
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
ipmi_handle_pong(struct ipmi_rs *rsp)
{
	struct rmcp_pong *pong;

	if (!rsp)
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
	if (!data) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return -1;
	}
	memset(data, 0, len);
	memcpy(data, &rmcp_ping, sizeof(rmcp_ping));
	memcpy(data+sizeof(rmcp_ping), &asf_ping, sizeof(asf_ping));

	lprintf(LOG_DEBUG, "Sending IPMI/RMCP presence ping packet");

	rv = ipmi_lan_send_packet(intf, data, len);

	free(data);
	data = NULL;

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

	while (rsp) {

		/* parse response headers */
		memcpy(&rmcp_rsp, rsp->data, 4);

		switch (rmcp_rsp.class) {
		case RMCP_CLASS_ASF:
			/* ping response packet */
			rv = ipmi_handle_pong(rsp);
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

		if (rsp->session.id == (intf->session->session_id + 0x10000000)) {
			/* With SOL, authtype is always NONE, so we have no authcode */
			rsp->session.payloadtype = IPMI_PAYLOAD_TYPE_SOL;
	
			rsp->session.msglen = rsp->data[x++];
			
			rsp->payload.sol_packet.packet_sequence_number =
				rsp->data[x++] & 0x0F;

			rsp->payload.sol_packet.acked_packet_number =
				rsp->data[x++] & 0x0F;

			rsp->payload.sol_packet.accepted_character_count =
				rsp->data[x++];

			rsp->payload.sol_packet.is_nack =
				rsp->data[x] & 0x40;

			rsp->payload.sol_packet.transfer_unavailable =
				rsp->data[x] & 0x20;

			rsp->payload.sol_packet.sol_inactive = 
				rsp->data[x] & 0x10;

			rsp->payload.sol_packet.transmit_overrun =
				rsp->data[x] & 0x08;
	
			rsp->payload.sol_packet.break_detected =
				rsp->data[x++] & 0x04;

			x++; /* On ISOL there's and additional fifth byte before the data starts */
	
			lprintf(LOG_DEBUG, "SOL sequence number     : 0x%02x",
				rsp->payload.sol_packet.packet_sequence_number);

			lprintf(LOG_DEBUG, "SOL acked packet        : 0x%02x",
				rsp->payload.sol_packet.acked_packet_number);
			
			lprintf(LOG_DEBUG, "SOL accepted char count : 0x%02x",
				rsp->payload.sol_packet.accepted_character_count);
			
			lprintf(LOG_DEBUG, "SOL is nack             : %s",
				rsp->payload.sol_packet.is_nack? "true" : "false");
			
			lprintf(LOG_DEBUG, "SOL xfer unavailable    : %s",
				rsp->payload.sol_packet.transfer_unavailable? "true" : "false");
			
			lprintf(LOG_DEBUG, "SOL inactive            : %s",
				rsp->payload.sol_packet.sol_inactive? "true" : "false");
			
			lprintf(LOG_DEBUG, "SOL transmit overrun    : %s",
				rsp->payload.sol_packet.transmit_overrun? "true" : "false");
			
			lprintf(LOG_DEBUG, "SOL break detected      : %s",
				rsp->payload.sol_packet.break_detected? "true" : "false");
		}
		else
		{
			/* Standard IPMI 1.5 packet */
			rsp->session.payloadtype = IPMI_PAYLOAD_TYPE_IPMI;
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
					if ((rsp->data_len) && (rsp->payload.ipmi_response.netfn == 7) &&
					    (rsp->payload.ipmi_response.cmd != 0x34)) {
						if (verbose > 2)
							printbuf(&rsp->data[x], rsp->data_len-x,
								 "bridge command response");
					}
					/* bridged command: lose extra header */
					if (entry->bridging_level &&
					    rsp->payload.ipmi_response.netfn == 7 &&
					    rsp->payload.ipmi_response.cmd == 0x34) {
						entry->bridging_level--;
						if (rsp->data_len - x - 1 == 0) {
							rsp = !rsp->ccode ? ipmi_lan_recv_packet(intf) : NULL;
							if (!entry->bridging_level)
								entry->req.msg.cmd = entry->req.msg.target_cmd;
							if (!rsp) {
								ipmi_req_remove_entry(entry->rq_seq, entry->req.msg.cmd);
							}
							continue;
						} else {
							/* The bridged answer data are inside the incoming packet */
							memmove(rsp->data + x - 7,
								rsp->data + x, 
								rsp->data_len - x - 1);
							rsp->data[x - 8] -= 8;
							rsp->data_len -= 8;
							entry->rq_seq = rsp->data[x - 3] >> 2;
							if (!entry->bridging_level)
								entry->req.msg.cmd = entry->req.msg.target_cmd;
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
		}

		break;
	}

	/* shift response data to start of array */
	if (rsp && rsp->data_len > x) {
		rsp->data_len -= x;
		if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_IPMI)
			rsp->data_len -= 1; /* We don't want the checksum */
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
 * |  session.authtype  | 9 bytes
 * |  session.seq       |
 * |  session.id        |
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
ipmi_lan_build_cmd(struct ipmi_intf * intf, struct ipmi_rq * req, int isRetry)
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
	int cs2 = 0, cs3 = 0;
	struct ipmi_rq_entry * entry;
	struct ipmi_session * s = intf->session;
	static int curr_seq = 0;
	uint8_t our_address = intf->my_addr;

	if (our_address == 0)
		our_address = IPMI_BMC_SLAVE_ADDR;

	if (isRetry == 0)
		curr_seq++;

	if (curr_seq >= 64)
		curr_seq = 0;

	// Bug in the existing code where it keeps on adding same command/seq pair 
	// in the lookup entry list.
	// Check if we have cmd,seq pair already in our list. As we are not changing 
	// the seq number we have to re-use the node which has existing
	// command and sequence number. If we add then we will have redundant node with
	// same cmd,seq pair
	entry = ipmi_req_lookup_entry(curr_seq, req->msg.cmd);
	if (entry)
	{
		// This indicates that we have already same command and seq in list
		// No need to add once again and we will re-use the existing node.
		// Only thing we have to do is clear the msg_data as we create
		// a new one below in the code for it.
		if (entry->msg_data) {
			free(entry->msg_data);
			entry->msg_data = NULL;
		}
	}
	else
	{
		// We don't have this request in the list so we can add it
		// to the list
		entry = ipmi_req_add_entry(intf, req, curr_seq);
		if (!entry)
			return NULL;
	}
 
	len = req->msg.data_len + 29;
	if (s->active && s->authtype)
		len += 16;
	if (intf->transit_addr != intf->my_addr && intf->transit_addr != 0)
		len += 8;
	msg = malloc(len);
	if (!msg) {
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
		entry->bridging_level = 0;
		msg[len++] = req->msg.data_len + 7;
		cs = mp = len;
	} else {
		/* bridged request: encapsulate w/in Send Message */
		entry->bridging_level = 1;
		msg[len++] = req->msg.data_len + 15 +
		  (intf->transit_addr != intf->my_addr && intf->transit_addr != 0 ? 8 : 0);
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

		if (intf->transit_addr == intf->my_addr || intf->transit_addr == 0) {
		        msg[len++] = (0x40|intf->target_channel); /* Track request*/
		} else {
		        entry->bridging_level++;
               		msg[len++] = (0x40|intf->transit_channel); /* Track request*/
			cs = len;
			msg[len++] = intf->transit_addr;
			msg[len++] = IPMI_NETFN_APP << 2;
			tmp = len - cs;
			msg[len++] = ipmi_csum(msg+cs, tmp);
			cs3 = len;
			msg[len++] = intf->my_addr;
			msg[len++] = curr_seq << 2;
			msg[len++] = 0x34;			/* Send Message rqst */
			msg[len++] = (0x40|intf->target_channel); /* Track request */
		}
		cs = len;
	}

	/* ipmi message header */
	msg[len++] = entry->bridging_level ? intf->target_addr : IPMI_BMC_SLAVE_ADDR;
	msg[len++] = req->msg.netfn << 2 | (req->msg.lun & 3);
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);
	cs = len;

	if (!entry->bridging_level)
		msg[len++] = IPMI_REMOTE_SWID;
   /* Bridged message */ 
	else if (entry->bridging_level) 
		msg[len++] = intf->my_addr;
   
	entry->rq_seq = curr_seq;
	msg[len++] = entry->rq_seq << 2;
	msg[len++] = req->msg.cmd;

	lprintf(LOG_DEBUG+1, ">> IPMI Request Session Header (level %d)", entry->bridging_level);
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
	if (entry->bridging_level) {
		if (intf->transit_addr != intf->my_addr && intf->transit_addr != 0) {
			tmp = len - cs3;
			msg[len++] = ipmi_csum(msg+cs3, tmp);
		}
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
	int isRetry = 0;

	lprintf(LOG_DEBUG, "ipmi_lan_send_cmd:opened=[%d], open=[%d]",
		intf->opened, intf->open);

	if (!intf->opened && intf->open) {
		if (intf->open(intf) < 0) {
			lprintf(LOG_DEBUG, "Failed to open LAN interface");
			return NULL;
		}
		lprintf(LOG_DEBUG, "\topened=[%d], open=[%d]",
			intf->opened, intf->open);
	}

	for (;;) {
		isRetry = ( try > 0 ) ? 1 : 0;

		entry = ipmi_lan_build_cmd(intf, req, isRetry);
		if (!entry) {
			lprintf(LOG_ERR, "Aborting send command, unable to build");
			return NULL;
		}

		if (ipmi_lan_send_packet(intf, entry->msg_data, entry->msg_len) < 0) {
			try++;
			usleep(5000);
			ipmi_req_remove_entry(entry->rq_seq, entry->req.msg.target_cmd);	
			continue;
		}

		/* if we are set to noanswer we do not expect response */
		if (intf->noanswer)
			break;

		if (ipmi_oem_active(intf, "intelwv2"))
			ipmi_lan_thump(intf);

		usleep(100);

		rsp = ipmi_lan_poll_recv(intf);

		/* Duplicate Request ccode most likely indicates a response to
		   a previous retry. Ignore and keep polling. */
		if(rsp && rsp->ccode == 0xcf) {
			rsp = NULL;
			rsp = ipmi_lan_poll_recv(intf);
		}
		
		if (rsp)
			break;

		usleep(5000);
		if (++try >= intf->ssn_params.retry) {
			lprintf(LOG_DEBUG, "  No response from remote controller");
			break;
		}
	}

	// We need to cleanup the existing entries from the list. Because if we 
	// keep it and then when we send the new command and if the response is for
	// old command it still matches it and then returns success.
	// This is the corner case where the remote controller responds very slowly.
	//
	// Example: We have to send command 23 and 2d.
	// If we send command,seq as 23,10 and if we don't get any response it will
	// retry 4 times with 23,10 and then come out here and indicate that there is no
	// response from the remote controller and will send the next command for
	// ie 2d,11. And if the BMC is slow to respond and returns 23,10 then it 
	// will match it in the list and will take response of command 23 as response 
	// for command 2d and return success. So ideally when retries are done and 
	// are out of this function we should be clearing the list to be safe so that
	// we don't match the old response with new request.
	//          [23, 10] --> BMC
	//          [23, 10] --> BMC
	//          [23, 10] --> BMC
	//          [23, 10] --> BMC
	//          [2D, 11] --> BMC
	//                   <-- [23, 10]
	//  here if we maintain 23,10 in the list then it will get matched and consider
	//  23 response as response for 2D.   
	ipmi_req_clear_entries();
 
	return rsp;
}

/*
 * IPMI SOL Payload Format
 * +--------------------+
 * |  rmcp.ver          | 4 bytes
 * |  rmcp.__reserved   |
 * |  rmcp.seq          |
 * |  rmcp.class        |
 * +--------------------+
 * |  session.authtype  | 9 bytes
 * |  session.seq       |
 * |  session.id        |
 * +--------------------+
 * |  message length    | 1 byte
 * +--------------------+
 * |  sol.seq           | 5 bytes
 * |  sol.ack_seq       |
 * |  sol.acc_count     |
 * |  sol.control       |
 * |  sol.__reserved    |
 * +--------------------+
 * | [request data]     | data_len bytes
 * +--------------------+
 */
uint8_t * ipmi_lan_build_sol_msg(struct ipmi_intf * intf,
				 struct ipmi_v2_payload * payload,
				 int * llen)
{
	struct rmcp_hdr rmcp = {
		.ver		= RMCP_VERSION_1,
		.class		= RMCP_CLASS_IPMI,
		.seq		= 0xff,
	};
	struct ipmi_session * session = intf->session;

	/* msg will hold the entire message to be sent */
	uint8_t * msg;

	int len = 0;

	len =	sizeof(rmcp)                                 +  // RMCP Header (4)
		10                                           +  // IPMI Session Header
		5                                            +  // SOL header
		payload->payload.sol_packet.character_count;    // The actual payload

	msg = malloc(len);
	if (!msg) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(msg, 0, len);

	/* rmcp header */
	memcpy(msg, &rmcp, sizeof(rmcp));
	len = sizeof(rmcp);

	/* ipmi session header */
	msg[len++] = 0; /* SOL is always authtype = NONE */
	msg[len++] = session->in_seq & 0xff;
	msg[len++] = (session->in_seq >> 8) & 0xff;
	msg[len++] = (session->in_seq >> 16) & 0xff;
	msg[len++] = (session->in_seq >> 24) & 0xff;

	msg[len++] = session->session_id & 0xff;
	msg[len++] = (session->session_id >> 8) & 0xff;
	msg[len++] = (session->session_id >> 16) & 0xff;
	msg[len++] = ((session->session_id >> 24) + 0x10) & 0xff; /* Add 0x10 to MSB for SOL */

	msg[len++] = payload->payload.sol_packet.character_count + 5;
	
	/* sol header */
	msg[len++] = payload->payload.sol_packet.packet_sequence_number;
	msg[len++] = payload->payload.sol_packet.acked_packet_number;
	msg[len++] = payload->payload.sol_packet.accepted_character_count;
	msg[len]    = payload->payload.sol_packet.is_nack           ? 0x40 : 0;
	msg[len]   |= payload->payload.sol_packet.assert_ring_wor   ? 0x20 : 0;
	msg[len]   |= payload->payload.sol_packet.generate_break    ? 0x10 : 0;
	msg[len]   |= payload->payload.sol_packet.deassert_cts      ? 0x08 : 0;
	msg[len]   |= payload->payload.sol_packet.deassert_dcd_dsr  ? 0x04 : 0;
	msg[len]   |= payload->payload.sol_packet.flush_inbound     ? 0x02 : 0;
	msg[len++] |= payload->payload.sol_packet.flush_outbound    ? 0x01 : 0;

	len++; /* On SOL there's and additional fifth byte before the data starts */

	if (payload->payload.sol_packet.character_count) {
		/* We may have data to add */
		memcpy(msg + len,
		       payload->payload.sol_packet.data,
		       payload->payload.sol_packet.character_count);
		len += payload->payload.sol_packet.character_count;		
	}

	session->in_seq++;
	if (session->in_seq == 0)
		session->in_seq++;
	
	*llen = len;
	return msg;
}

/*
 * is_sol_packet
 */
static int
is_sol_packet(struct ipmi_rs * rsp)
{
	return (rsp                                                           &&
		(rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL));
}



/*
 * sol_response_acks_packet
 */
static int
sol_response_acks_packet(struct ipmi_rs         * rsp,
			 struct ipmi_v2_payload * payload)
{
	return (is_sol_packet(rsp)                                            &&
		payload                                                       &&
		(payload->payload_type    == IPMI_PAYLOAD_TYPE_SOL)           && 
		(rsp->payload.sol_packet.acked_packet_number ==
		 payload->payload.sol_packet.packet_sequence_number));
}

/*
 * ipmi_lan_send_sol_payload
 *
 */
static struct ipmi_rs *
ipmi_lan_send_sol_payload(struct ipmi_intf * intf,
			  struct ipmi_v2_payload * payload)
{
	struct ipmi_rs      * rsp = NULL;
	uint8_t             * msg;
	int                   len;
	int                   try = 0;

	if (!intf->opened && intf->open) {
		if (intf->open(intf) < 0)
			return NULL;
	}

	msg = ipmi_lan_build_sol_msg(intf, payload, &len);
	if (len <= 0 || !msg) {
		lprintf(LOG_ERR, "Invalid SOL payload packet");
		if (msg) {
			free(msg);
			msg = NULL;
		}
		return NULL;
	}

	lprintf(LOG_DEBUG, ">> SENDING A SOL MESSAGE\n");

	for (;;) {
		if (ipmi_lan_send_packet(intf, msg, len) < 0) {
			try++;
			usleep(5000);
			continue;
		}

		/* if we are set to noanswer we do not expect response */
		if (intf->noanswer)
			break;
		
		if (payload->payload.sol_packet.packet_sequence_number == 0) {
			/* We're just sending an ACK.  No need to retry. */
			break;
		}

		usleep(100);
		
		rsp = ipmi_lan_recv_sol(intf); /* Grab the next packet */

		if (sol_response_acks_packet(rsp, payload))
			break;

		else if (is_sol_packet(rsp) && rsp->data_len)
		{
			/*
			 * We're still waiting for our ACK, but we more data from
			 * the BMC
			 */
			intf->session->sol_data.sol_input_handler(rsp);
		}

		usleep(5000);
		if (++try >= intf->ssn_params.retry) {
			lprintf(LOG_DEBUG, "  No response from remote controller");
			break;
		}
	}

	if (msg) {
		free(msg);
		msg = NULL;
	}
	return rsp;
}

/*
 * is_sol_partial_ack
 *
 * Determine if the response is a partial ACK/NACK that indicates
 * we need to resend part of our packet.
 *
 * returns the number of characters we need to resend, or
 *         0 if this isn't an ACK or we don't need to resend anything
 */
static int is_sol_partial_ack(struct ipmi_v2_payload * v2_payload,
			      struct ipmi_rs         * rsp)
{
	int chars_to_resend = 0;

	if (v2_payload                                &&
	    rsp                                       &&
	    is_sol_packet(rsp)                        &&
	    sol_response_acks_packet(rsp, v2_payload) &&
	    (rsp->payload.sol_packet.accepted_character_count <
	     v2_payload->payload.sol_packet.character_count))
	{
		if (rsp->payload.sol_packet.accepted_character_count == 0) {
			/* We should not resend data */
			chars_to_resend = 0;
		}
		else
		{
			chars_to_resend =
				v2_payload->payload.sol_packet.character_count -
				rsp->payload.sol_packet.accepted_character_count;
		}
	}

	return chars_to_resend;
}

/*
 * set_sol_packet_sequence_number
 */
static void set_sol_packet_sequence_number(struct ipmi_intf * intf,
					   struct ipmi_v2_payload * v2_payload)
{
	/* Keep our sequence number sane */
	if (intf->session->sol_data.sequence_number > 0x0F)
		intf->session->sol_data.sequence_number = 1;

	v2_payload->payload.sol_packet.packet_sequence_number =
		intf->session->sol_data.sequence_number++;
}

/*
 * ipmi_lan_send_sol
 *
 * Sends a SOL packet..  We handle partial ACK/NACKs from the BMC here.
 *
 * Returns a pointer to the SOL ACK we received, or
 *         0 on failure
 * 
 */
struct ipmi_rs *
ipmi_lan_send_sol(struct ipmi_intf * intf,
		  struct ipmi_v2_payload * v2_payload)
{
	struct ipmi_rs * rsp;
	int chars_to_resend = 0;

	v2_payload->payload_type   = IPMI_PAYLOAD_TYPE_SOL;

	/*
	 * Payload length is just the length of the character
	 * data here.
	 */
	v2_payload->payload.sol_packet.acked_packet_number = 0; /* NA */

	set_sol_packet_sequence_number(intf, v2_payload);
	
	v2_payload->payload.sol_packet.accepted_character_count = 0; /* NA */

	rsp = ipmi_lan_send_sol_payload(intf, v2_payload);

	/* Determine if we need to resend some of our data */
	chars_to_resend = is_sol_partial_ack(v2_payload, rsp);

	while (chars_to_resend)
	{
		/*
		 * We first need to handle any new data we might have
		 * received in our NACK
		 */
		if (rsp->data_len)
			intf->session->sol_data.sol_input_handler(rsp);

		set_sol_packet_sequence_number(intf, v2_payload);
		
		/* Just send the required data */
		memmove(v2_payload->payload.sol_packet.data,
			v2_payload->payload.sol_packet.data +
			rsp->payload.sol_packet.accepted_character_count,
			chars_to_resend);

		v2_payload->payload.sol_packet.character_count = chars_to_resend;

		rsp = ipmi_lan_send_sol_payload(intf, v2_payload);

		chars_to_resend = is_sol_partial_ack(v2_payload, rsp);
	}

	return rsp;
}

/*
 * check_sol_packet_for_new_data
 *
 * Determine whether the SOL packet has already been seen
 * and whether the packet has new data for us.
 *
 * This function has the side effect of removing an previously
 * seen data, and moving new data to the front.
 *
 * It also "Remembers" the data so we don't get repeats.
 *
 */
static int
check_sol_packet_for_new_data(struct ipmi_rs *rsp)
{
	static uint8_t last_received_sequence_number = 0;
	static uint8_t last_received_byte_count      = 0;
	int new_data_size                            = 0;

	if (rsp &&
	    (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL))
	    
	{
		uint8_t unaltered_data_len = rsp->data_len;
		if (rsp->payload.sol_packet.packet_sequence_number ==
		    last_received_sequence_number)
		{
			/*
			 * This is the same as the last packet, but may include
			 * extra data
			 */
			new_data_size = rsp->data_len - last_received_byte_count;
			
			if (new_data_size > 0)
			{
				/* We have more data to process */
				memmove(rsp->data,
					rsp->data +
					rsp->data_len - new_data_size,
					new_data_size);
			}
			
			rsp->data_len = new_data_size;
		}
	
		/*
		 * Remember the data for next round
		 */
		if (rsp && rsp->payload.sol_packet.packet_sequence_number)
		{
			last_received_sequence_number =
				rsp->payload.sol_packet.packet_sequence_number;
			last_received_byte_count = unaltered_data_len;
		}
	}

	return new_data_size;
}

/*
 * ack_sol_packet
 *
 * Provided the specified packet looks reasonable, ACK it.
 */
static void
ack_sol_packet(struct ipmi_intf * intf,
	       struct ipmi_rs * rsp)
{
	if (rsp &&
	    (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL) &&
	    (rsp->payload.sol_packet.packet_sequence_number))
	{
		struct ipmi_v2_payload ack;

		memset(&ack, 0, sizeof(struct ipmi_v2_payload));

		ack.payload_type = IPMI_PAYLOAD_TYPE_SOL;

		/*
		 * Payload length is just the length of the character
		 * data here.
		 */
		ack.payload_length = 0;

		/* ACK packets have sequence numbers of 0 */
		ack.payload.sol_packet.packet_sequence_number = 0;

		ack.payload.sol_packet.acked_packet_number =
			rsp->payload.sol_packet.packet_sequence_number;

		ack.payload.sol_packet.accepted_character_count = rsp->data_len;
		
		ipmi_lan_send_sol_payload(intf, &ack);
	}
}

/*
 * ipmi_recv_sol
 *
 * Receive a SOL packet and send an ACK in response.
 *
 */
static struct ipmi_rs *
ipmi_lan_recv_sol(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp = ipmi_lan_poll_recv(intf);

	ack_sol_packet(intf, rsp);              

	/*
	 * Remembers the data sent, and alters the data to just
	 * include the new stuff.
	 */
	check_sol_packet_for_new_data(rsp);

	return rsp;
}

/* send a get device id command to keep session active */
static int
ipmi_lan_keepalive(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req = {
		.msg = {
			.netfn = IPMI_NETFN_APP,
			.cmd = 1,
		}
	};

	if (!intf->opened)
		return 0;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
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
	struct ipmi_session_params *p = &intf->ssn_params;
	uint8_t msg_data[2];

	msg_data[0] = IPMI_LAN_CHANNEL_E;
	msg_data[1] = p->privlvl;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;
	req.msg.cmd      = 0x38;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_INFO, "Get Auth Capabilities command failed");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "get_auth_capabilities");

	if (rsp->ccode) {
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

	if (p->password &&
	    (p->authtype_set == 0 ||
	     p->authtype_set == IPMI_SESSION_AUTHTYPE_MD5) &&
	    (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD5))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_MD5;
	}
	else if (p->password &&
		 (p->authtype_set == 0 ||
		  p->authtype_set == IPMI_SESSION_AUTHTYPE_MD2) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD2))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_MD2;
	}
	else if (p->password &&
		 (p->authtype_set == 0 ||
		  p->authtype_set == IPMI_SESSION_AUTHTYPE_PASSWORD) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_PASSWORD;
	}
	else if (p->password &&
		 (p->authtype_set == 0 ||
		  p->authtype_set == IPMI_SESSION_AUTHTYPE_OEM) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_OEM))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_OEM;
	}
	else if ((p->authtype_set == 0 ||
		  p->authtype_set == IPMI_SESSION_AUTHTYPE_NONE) &&
		 (rsp->data[1] & 1<<IPMI_SESSION_AUTHTYPE_NONE))
	{
		s->authtype = IPMI_SESSION_AUTHTYPE_NONE;
	}
	else {
		if (!(rsp->data[1] & 1<<p->authtype_set))
			lprintf(LOG_ERR, "Authentication type %s not supported",
			       val2str(p->authtype_set, ipmi_authtype_session_vals));
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
	memcpy(msg_data+1, intf->ssn_params.username, 16);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x39;
	req.msg.data		= msg_data;
	req.msg.data_len	= 17; /* 1 byte for authtype, 16 for user */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get Session Challenge command failed");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "get_session_challenge");

	if (rsp->ccode) {
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
	msg_data[1] = intf->ssn_params.privlvl;

	/* supermicro oem authentication hack */
	if (ipmi_oem_active(intf, "supermicro")) {
		uint8_t * special = ipmi_auth_special(s);
		memcpy(intf->session->authcode, special, 16);
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
	if (!rsp) {
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
	uint8_t privlvl = intf->ssn_params.privlvl;
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

	if (!rsp) {
		lprintf(LOG_ERR, "Set Session Privilege Level to %s failed",
			val2str(privlvl, ipmi_privlvl_vals));
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "set_session_privlvl");

	if (rsp->ccode) {
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
	if (!rsp) {
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
	if (rsp->ccode) {
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
		goto close_fail;

	return 0;

 close_fail:
	ipmi_close_session_cmd(intf);
 fail:
	lprintf(LOG_ERR, "Error: Unable to establish LAN session");
	return -1;
}

void
ipmi_lan_close(struct ipmi_intf * intf)
{
	if (!intf->abort && intf->session)
		ipmi_close_session_cmd(intf);

	if (intf->fd >= 0) {
		close(intf->fd);
		intf->fd = -1;
	}

	ipmi_req_clear_entries();
	ipmi_intf_session_cleanup(intf);
	intf->opened = 0;
	intf->manufacturer_id = IPMI_OEM_UNKNOWN;
	intf = NULL;
}

static int
ipmi_lan_open(struct ipmi_intf * intf)
{
	int rc;
	struct ipmi_session *s;
	struct ipmi_session_params *p;

	if (!intf || intf->opened)
		return -1;

	s = intf->session;
	p = &intf->ssn_params;

	if (p->port == 0)
		p->port = IPMI_LAN_PORT;
	if (p->privlvl == 0)
		p->privlvl = IPMI_SESSION_PRIV_ADMIN;
	if (p->timeout == 0)
		p->timeout = IPMI_LAN_TIMEOUT;
	if (p->retry == 0)
		p->retry = IPMI_LAN_RETRY;

	if (!p->hostname || strlen((const char *)p->hostname) == 0) {
		lprintf(LOG_ERR, "No hostname specified!");
		return -1;
	}

	if (ipmi_intf_socket_connect(intf) == -1) {
		lprintf(LOG_ERR, "Could not open socket!");
		return -1;
	}

	s = (struct ipmi_session *)malloc(sizeof(struct ipmi_session));
	if (!s) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		goto fail;
	}

	intf->opened = 1;
	intf->abort = 1;

	intf->session = s;

	memset(s, 0, sizeof(struct ipmi_session));
	s->sol_data.sequence_number = 1;
	s->timeout = p->timeout;
	memcpy(&s->authcode, &p->authcode_set, sizeof(s->authcode));
	s->addrlen = sizeof(s->addr);
	if (getsockname(intf->fd, (struct sockaddr *)&s->addr, &s->addrlen)) {
		goto fail;
	}

	/* try to open session */
	rc = ipmi_lan_activate_session(intf);
	if (rc < 0) {
	    goto fail;
	}

	/* automatically detect interface request and response sizes */
	hpm2_detect_max_payload_size(intf);

	/* set manufactirer OEM id */
	intf->manufacturer_id = ipmi_get_oem(intf);

	/* now allow bridging */
	bridge_possible = 1;
	return intf->fd;

 fail:
	lprintf(LOG_ERR, "Error: Unable to establish IPMI v1.5 / RMCP session");
	intf->close(intf);
	return -1;
}

static int
ipmi_lan_setup(struct ipmi_intf * intf)
{
	/* setup default LAN maximum request and response sizes */
	intf->max_request_data_size = IPMI_LAN_MAX_REQUEST_SIZE;
	intf->max_response_data_size = IPMI_LAN_MAX_RESPONSE_SIZE;

	return 0;
}

static void
ipmi_lan_set_max_rq_data_size(struct ipmi_intf * intf, uint16_t size)
{
	if (size + 7 > 0xFF) {
		size = 0xFF - 7;
	}

	intf->max_request_data_size = size;
}

static void
ipmi_lan_set_max_rp_data_size(struct ipmi_intf * intf, uint16_t size)
{
	if (size + 8 > 0xFF) {
		size = 0xFF - 8;
	}

	intf->max_response_data_size = size;
}
