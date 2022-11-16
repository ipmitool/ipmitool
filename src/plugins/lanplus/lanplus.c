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
#include <time.h>
#include <fcntl.h>
#include <assert.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_sel.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/hpm2.h>
#include <ipmitool/bswap.h>
#include <openssl/rand.h>

#include "lanplus.h"
#include "lanplus_crypt.h"
#include "lanplus_crypt_impl.h"
#include "lanplus_dump.h"
#include "rmcp.h"
#include "asf.h"

/*
 * LAN interface is required to support 45 byte request transactions and
 * 42 byte response transactions.
 */
#define IPMI_LAN_MAX_REQUEST_SIZE	38	/* 45 - 7 */
#define IPMI_LAN_MAX_RESPONSE_SIZE	34	/* 42 - 8 */

extern const struct valstr ipmi_rakp_return_codes[];
extern const struct valstr ipmi_priv_levels[];
extern const struct valstr ipmi_auth_algorithms[];
extern const struct valstr ipmi_integrity_algorithms[];
extern const struct valstr ipmi_encryption_algorithms[];

static struct ipmi_rq_entry * ipmi_req_entries;
static struct ipmi_rq_entry * ipmi_req_entries_tail;


static int ipmi_lanplus_setup(struct ipmi_intf * intf);
static int ipmi_lanplus_keepalive(struct ipmi_intf * intf);
static int ipmi_lan_send_packet(struct ipmi_intf * intf, uint8_t * data, int data_len);
static struct ipmi_rs * ipmi_lan_recv_packet(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lan_poll_recv(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lanplus_send_ipmi_cmd(struct ipmi_intf * intf, struct ipmi_rq * req);
static struct ipmi_rs * ipmi_lanplus_send_payload(struct ipmi_intf * intf,
												  struct ipmi_v2_payload * payload);
static void getIpmiPayloadWireRep(
								  struct ipmi_intf       * intf,
								  struct ipmi_v2_payload * payload,  /* in  */
								  uint8_t  * out,
								  struct ipmi_rq * req,
								  uint8_t    rq_seq,
								  uint8_t curr_seq);
static void getSolPayloadWireRep(
								 uint8_t          * msg,
								 struct ipmi_v2_payload * payload);
static void read_open_session_response(struct ipmi_rs * rsp, int offset);
static void read_rakp2_message(struct ipmi_rs * rsp, int offset, uint8_t alg);
static void read_rakp4_message(struct ipmi_rs * rsp, int offset, uint8_t alg);
static void read_session_data(struct ipmi_rs * rsp, int * offset);
static void read_session_data_v15(struct ipmi_rs * rsp, int * offset);
static void read_session_data_v2x(struct ipmi_rs * rsp, int * offset);
static void read_ipmi_response(struct ipmi_rs * rsp, int * offset);
static void read_sol_packet(struct ipmi_rs * rsp, int * offset);
static struct ipmi_rs * ipmi_lanplus_recv_sol(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lanplus_send_sol(
											  struct ipmi_intf * intf,
											  struct ipmi_v2_payload * payload);
static int check_sol_packet_for_new_data(
									 struct ipmi_rs *rsp);
static void ack_sol_packet(
							struct ipmi_intf * intf,
							struct ipmi_rs * rsp);
static void ipmi_lanp_set_max_rq_data_size(struct ipmi_intf * intf, uint16_t size);
static void ipmi_lanp_set_max_rp_data_size(struct ipmi_intf * intf, uint16_t size);

static uint8_t bridgePossible = 0;

struct ipmi_intf ipmi_lanplus_intf = {
	.name = "lanplus",
	.desc = "IPMI v2.0 RMCP+ LAN Interface",
	.setup = ipmi_lanplus_setup,
	.open = ipmi_lanplus_open,
	.close = ipmi_lanplus_close,
	.sendrecv = ipmi_lanplus_send_ipmi_cmd,
	.recv_sol = ipmi_lanplus_recv_sol,
	.send_sol = ipmi_lanplus_send_sol,
	.keepalive = ipmi_lanplus_keepalive,
	.set_max_request_data_size = ipmi_lanp_set_max_rq_data_size,
	.set_max_response_data_size = ipmi_lanp_set_max_rp_data_size,
	.target_addr = IPMI_BMC_SLAVE_ADDR,
};


extern int verbose;



/*
 * lanplus_get_requested_ciphers
 *
 * Set the authentication, integrity and encryption algorithms based
 * on the cipher suite ID.  See table 22-19 in the IPMIv2 spec for the
 * source of this information.
 *
 * param cipher_suite_id [in]
 * param auth_alg        [out]
 * param integrity_alg   [out]
 * param crypt_alg       [out]
 *
 * returns 0 on success
 *         1 on failure
 */
int
lanplus_get_requested_ciphers(enum cipher_suite_ids cipher_suite_id,
                              uint8_t *auth_alg,
                              uint8_t *integrity_alg,
                              uint8_t *crypt_alg)
{
		/* See table 22-19 for the source of the statement */
	switch (cipher_suite_id)
	{
	case IPMI_LANPLUS_CIPHER_SUITE_0:
		*auth_alg      = IPMI_AUTH_RAKP_NONE;
		*integrity_alg = IPMI_INTEGRITY_NONE;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_1:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA1;
		*integrity_alg = IPMI_INTEGRITY_NONE;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_2:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA1;
		*integrity_alg = IPMI_INTEGRITY_HMAC_SHA1_96;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_3:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA1;
		*integrity_alg = IPMI_INTEGRITY_HMAC_SHA1_96;
		*crypt_alg     = IPMI_CRYPT_AES_CBC_128;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_4:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA1;
		*integrity_alg = IPMI_INTEGRITY_HMAC_SHA1_96;
		*crypt_alg     = IPMI_CRYPT_XRC4_128;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_5:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA1;
		*integrity_alg = IPMI_INTEGRITY_HMAC_SHA1_96;
		*crypt_alg     = IPMI_CRYPT_XRC4_40;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_6:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_NONE;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_7:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_HMAC_MD5_128;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_8:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_HMAC_MD5_128;
		*crypt_alg     = IPMI_CRYPT_AES_CBC_128;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_9:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_HMAC_MD5_128;
		*crypt_alg     = IPMI_CRYPT_XRC4_128;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_10:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_HMAC_MD5_128;
		*crypt_alg     = IPMI_CRYPT_XRC4_40;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_11:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_MD5_128;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_12:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_MD5_128;
		*crypt_alg     = IPMI_CRYPT_AES_CBC_128;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_13:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_MD5_128;
		*crypt_alg     = IPMI_CRYPT_XRC4_128;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_14:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_MD5;
		*integrity_alg = IPMI_INTEGRITY_MD5_128;
		*crypt_alg     = IPMI_CRYPT_XRC4_40;
		break;
#ifdef HAVE_CRYPTO_SHA256
	case IPMI_LANPLUS_CIPHER_SUITE_15:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA256;
		*integrity_alg = IPMI_INTEGRITY_NONE;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_16:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA256;
		*integrity_alg = IPMI_INTEGRITY_HMAC_SHA256_128;
		*crypt_alg     = IPMI_CRYPT_NONE;
		break;
	case IPMI_LANPLUS_CIPHER_SUITE_17:
		*auth_alg      = IPMI_AUTH_RAKP_HMAC_SHA256;
		*integrity_alg = IPMI_INTEGRITY_HMAC_SHA256_128;
		*crypt_alg     = IPMI_CRYPT_AES_CBC_128;
		break;
#endif /* HAVE_CRYPTO_SHA256 */
	case IPMI_LANPLUS_CIPHER_SUITE_RESERVED:
	default:
		return 1;
	}

	return 0;
}

static const struct valstr plus_payload_types_vals[] = {
	 { IPMI_PAYLOAD_TYPE_IPMI,              "IPMI (0)" },	// IPMI Message
	 { IPMI_PAYLOAD_TYPE_SOL,               "SOL  (1)" },	// SOL (Serial over LAN)
	 { IPMI_PAYLOAD_TYPE_OEM,               "OEM  (2)" },	// OEM Explicid

	 { IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST, "OpenSession Req (0x10)" },
	 { IPMI_PAYLOAD_TYPE_RMCP_OPEN_RESPONSE,"OpenSession Resp (0x11)" },
	 { IPMI_PAYLOAD_TYPE_RAKP_1,            "RAKP1 (0x12)" },
	 { IPMI_PAYLOAD_TYPE_RAKP_2,            "RAKP2 (0x13)" },
	 { IPMI_PAYLOAD_TYPE_RAKP_3,            "RAKP3 (0x14)" },
	 { IPMI_PAYLOAD_TYPE_RAKP_4,            "RAKP4 (0x15)" },
	{ 0x00, NULL },
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
		if (e == e->next)
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
		p = e->next;
		free(e);
		e = p;
	}

	ipmi_req_entries = NULL;
	ipmi_req_entries_tail = NULL;
}


int
ipmi_lan_send_packet(
					 struct ipmi_intf * intf,
					 uint8_t * data, int
					 data_len)
{
	if (verbose >= 5)
		printbuf(data, data_len, ">> sending packet");

	return send(intf->fd, data, data_len, 0);
}



struct ipmi_rs *
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

	if (verbose >= 5)
		printbuf(rsp.data, rsp.data_len, "<< received packet");

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
	struct rmcp_pong {
		struct rmcp_hdr rmcp;
		struct asf_hdr asf;
		uint32_t iana;
		uint32_t oem;
		uint8_t sup_entities;
		uint8_t sup_interact;
		uint8_t reserved[6];
	} *pong;

	if (!rsp)
		return -1;

	pong = (struct rmcp_pong *)rsp->data;

	if (verbose)
		printf("Received IPMI/RMCP response packet: "
				"IPMI%s Supported\n",
				(pong->sup_entities & 0x80) ? "" : " NOT");

	if (verbose > 1)
		printf("  ASF Version %s\n"
				"  RMCP Version %s\n"
				"  RMCP Sequence %d\n"
				"  IANA Enterprise %lu\n\n",
				(pong->sup_entities & 0x01) ? "1.0" : "unknown",
				(pong->rmcp.ver == 6) ? "1.0" : "unknown",
				pong->rmcp.seq,
				(unsigned long)ntohl(pong->iana));

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
ipmiv2_lan_ping(struct ipmi_intf * intf)
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


/**
 *
 * ipmi_lan_poll_single
 *
 * Receive whatever comes back.  Ignore received packets that don't correspond
 * to a request we've sent.
 *
 * Returns: the ipmi_rs packet describing the/a response we expect.
 */
static struct ipmi_rs *
ipmi_lan_poll_single(struct ipmi_intf * intf)
{
	struct rmcp_hdr * rmcp_rsp;
	struct ipmi_rs * rsp;
	struct ipmi_session * session = intf->session;
	int offset, rv;
	uint16_t payload_size;

	/* receive packet */
	rsp = ipmi_lan_recv_packet(intf);

	/* check if no packet has come */
	if (!rsp) {
		return NULL;
	}

	/* parse response headers */
	rmcp_rsp = (struct rmcp_hdr *)rsp->data;

	if (rmcp_rsp->class == RMCP_CLASS_ASF) {
		/* might be ping response packet */
		rv = ipmi_handle_pong(rsp);
		return (rv <= 0) ? NULL : rsp;
	}

	if (rmcp_rsp->class != RMCP_CLASS_IPMI) {
		lprintf(LOG_DEBUG, "Invalid RMCP class: %x", rmcp_rsp->class);
		/* read one more packet */
		return (struct ipmi_rs *)1;
	}

	/*
	 * The authtype / payload type determines what we are receiving
	 */
	offset = 4;

	/*--------------------------------------------------------------------
	 *
	 * The current packet could be one of several things:
	 *
	 * 1) An IPMI 1.5 packet (the response to our GET CHANNEL
	 *    AUTHENTICATION CAPABILITIES request)
	 * 2) An RMCP+ message with an IPMI response payload
	 * 3) AN RMCP+ open session response
	 * 4) An RAKP-2 message (response to an RAKP 1 message)
	 * 5) An RAKP-4 message (response to an RAKP 3 message)
	 * 6) A Serial Over LAN packet
	 * 7) An Invalid packet (one that doesn't match a request)
	 * -------------------------------------------------------------------
	 */

	read_session_data(rsp, &offset);

	/*
	 * Skip packets that are not intended for this session
	 */
	if ((session->v2_data.session_state == LANPLUS_STATE_ACTIVE)    &&
		 (rsp->session.authtype == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
		 (rsp->session.id != intf->session->v2_data.console_id))
	{
		lprintf(LOG_INFO, "packet session id 0x%x does not "
			"match active session 0x%0x",
			rsp->session.id, intf->session->v2_data.console_id);
		lprintf(LOG_ERR, "ERROR: Received an Unexpected message ID");
		/* read one more packet */
		return (struct ipmi_rs *)1;
	}

	if (lanplus_has_valid_auth_code(rsp, intf->session) == 0) {
		lprintf(LOG_ERR, "ERROR: Received message with invalid authcode!");
		return NULL;
	}

	if ((session->v2_data.session_state == LANPLUS_STATE_ACTIVE)    &&
			(rsp->session.authtype == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
			(rsp->session.bEncrypted)) {
		lanplus_decrypt_payload(session->v2_data.crypt_alg,
				session->v2_data.k2,
				rsp->data + offset,
				rsp->session.msglen,
				rsp->data + offset,
				&payload_size);
	} else {
		payload_size = rsp->session.msglen;
	}

	/*
	 * Handle IPMI responses (case #1 and #2) -- all IPMI responses
	 */
	if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_IPMI) {
		struct ipmi_rq_entry * entry;
		int payload_start = offset;
		int extra_data_length;
		int loop = 1;

		while (loop--) {
			/* fill-in response data */
			read_ipmi_response(rsp, &offset);

			lprintf(LOG_DEBUG+1, "<< IPMI Response Session Header");
			lprintf(LOG_DEBUG+1, "<<   Authtype                : %s",
				val2str(rsp->session.authtype, ipmi_authtype_session_vals));
			lprintf(LOG_DEBUG+1, "<<   Payload type            : %s",
				val2str(rsp->session.payloadtype, plus_payload_types_vals));
			lprintf(LOG_DEBUG+1, "<<   Session ID              : 0x%08lx",
				(long)rsp->session.id);
			lprintf(LOG_DEBUG+1, "<<   Sequence                : 0x%08lx",
				(long)rsp->session.seq);
			lprintf(LOG_DEBUG+1, "<<   IPMI Msg/Payload Length : %d",
				rsp->session.msglen);
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

			/* Are we expecting this packet? */
			entry = ipmi_req_lookup_entry(rsp->payload.ipmi_response.rq_seq,
								rsp->payload.ipmi_response.cmd);

			if (!entry) {
				lprintf(LOG_INFO, "IPMI Request Match NOT FOUND");
				/* read one more packet */
				return (struct ipmi_rs *)1;
			};

			lprintf(LOG_DEBUG+2, "IPMI Request Match found");

			if (entry->bridging_level) {
				/* Check completion code */
				if (rsp->ccode) {
					lprintf(LOG_DEBUG, "WARNING: Bridged "
							"cmd ccode = 0x%02x", rsp->ccode);
				} else {
					/* decrement bridging level */
					entry->bridging_level--;
					if (!entry->bridging_level) {
						entry->req.msg.cmd = entry->req.msg.target_cmd;
					}

					/* check if bridged response is embedded */
					if (payload_size > 8) {
						printbuf(&rsp->data[offset], (rsp->data_len-offset-1),
								"bridge command response");
						/*
						 * need to make a loop for embedded bridged response
						 */
						loop++;
					} else {
						lprintf(LOG_DEBUG, "Bridged command answer,"
								" waiting for next answer... ");
						/* read one more packet */
						return (struct ipmi_rs *)1;
					}
				}
			}

			/* Remove request entry */
			ipmi_req_remove_entry(rsp->payload.ipmi_response.rq_seq,
					rsp->payload.ipmi_response.cmd);

			/*
			 * Good packet.  Shift response data to start of array.
			 * rsp->data becomes the variable length IPMI response data
			 * rsp->data_len becomes the length of that data
			 */
			extra_data_length = payload_size - (offset - payload_start) - 1;
			if (extra_data_length > 0) {
				rsp->data_len = extra_data_length;
				memmove(rsp->data, rsp->data + offset, extra_data_length);
				offset = 0;
				payload_start = 0;
				payload_size = extra_data_length;
			} else {
				rsp->data_len = 0;
			}
		}
	/*
	 * Open Response
	 */
	} else if (rsp->session.payloadtype ==
			IPMI_PAYLOAD_TYPE_RMCP_OPEN_RESPONSE) {
		if (session->v2_data.session_state !=
				LANPLUS_STATE_OPEN_SESSION_SENT) {
			lprintf(LOG_ERR, "Error: Received an Unexpected Open Session "
					"Response");
			/* read one more packet */
			return (struct ipmi_rs *)1;
		}
		read_open_session_response(rsp, offset);
	/*
	 * RAKP 2
	 */
	} else if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_RAKP_2) {
		if (session->v2_data.session_state != LANPLUS_STATE_RAKP_1_SENT) {
			lprintf(LOG_ERR, "Error: Received an Unexpected RAKP 2 message");
			/* read one more packet */
			return (struct ipmi_rs *)1;
		}
		read_rakp2_message(rsp, offset, session->v2_data.auth_alg);
	/*
	 * RAKP 4
	 */
	} else if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_RAKP_4) {
		if (session->v2_data.session_state != LANPLUS_STATE_RAKP_3_SENT) {
			lprintf(LOG_ERR, "Error: Received an Unexpected RAKP 4 message");
			/* read one more packet */
			return (struct ipmi_rs *)1;
		}
		read_rakp4_message(rsp, offset, session->v2_data.auth_alg);
	/*
	 * SOL
	 */
	} else if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL) {
		int payload_start = offset;
		int extra_data_length;

		if (session->v2_data.session_state != LANPLUS_STATE_ACTIVE) {
			lprintf(LOG_ERR, "Error: Received an Unexpected SOL packet");
			/* read one more packet */
			return (struct ipmi_rs *)1;
		}
		read_sol_packet(rsp, &offset);
		extra_data_length = payload_size - (offset - payload_start);
		if (extra_data_length > 0) {
			rsp->data_len = extra_data_length;
			memmove(rsp->data, rsp->data + offset, extra_data_length);
		} else {
			rsp->data_len = 0;
		}
	/*
	 * Unknown Payload type
	 */
	} else {
		lprintf(LOG_ERR, "Invalid RMCP+ payload type : 0x%x",
				rsp->session.payloadtype);
		/* read one more packet */
		return (struct ipmi_rs *)1;
	}

	return rsp;
}



/**
 *
 * ipmi_lan_poll_recv
 *
 * Receive whatever comes back.  Ignore received packets that don't correspond
 * to a request we've sent.
 *
 * Returns: the ipmi_rs packet describing the/a response we expect.
 */
static struct ipmi_rs *
ipmi_lan_poll_recv(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;

	do {
		/* poll single packet */
		rsp = ipmi_lan_poll_single(intf);
	} while (rsp == (struct ipmi_rs *) 1);

	return rsp;
}



/*
 * read_open_session_response
 *
 * Initialize the ipmi_rs from the IPMI 2.x open session response data.
 *
 * The offset should point to the first byte of the the Open Session Response
 * payload when this function is called.
 *
 * param rsp    [in/out] reading from the data and writing to the open_session_response
 *              section
 * param offset [in] tells us where the Open Session Response payload starts
 *
 * returns 0 on success, 1 on error
 */
void
read_open_session_response(struct ipmi_rs * rsp, int offset)
{
	memset(&rsp->payload.open_session_response, 0,
			 sizeof(rsp->payload.open_session_response));

	 /*  Message tag */
	 rsp->payload.open_session_response.message_tag = rsp->data[offset];

	 /* RAKP response code */
	 rsp->payload.open_session_response.rakp_return_code = rsp->data[offset + 1];

	 /* Maximum privilege level */
	 rsp->payload.open_session_response.max_priv_level = rsp->data[offset + 2];

	 /*** offset + 3 is reserved ***/

	 /* Remote console session ID */
	 memcpy(&(rsp->payload.open_session_response.console_id),
			rsp->data + offset + 4,
			4);
	 #if WORDS_BIGENDIAN
	 rsp->payload.open_session_response.console_id =
		 BSWAP_32(rsp->payload.open_session_response.console_id);
	 #endif

	/* only tag, status, privlvl, and console id are returned if error */
	 if (rsp->payload.open_session_response.rakp_return_code !=
		  IPMI_RAKP_STATUS_NO_ERRORS)
		 return;

	 /* BMC session ID */
	 memcpy(&(rsp->payload.open_session_response.bmc_id),
			rsp->data + offset + 8,
			4);
	 #if WORDS_BIGENDIAN
	 rsp->payload.open_session_response.bmc_id =
		 BSWAP_32(rsp->payload.open_session_response.bmc_id);
	 #endif

	 /* And of course, our negotiated algorithms */
	 rsp->payload.open_session_response.auth_alg      = rsp->data[offset + 16];
	 rsp->payload.open_session_response.integrity_alg = rsp->data[offset + 24];
	 rsp->payload.open_session_response.crypt_alg     = rsp->data[offset + 32];
}



/*
 * read_rakp2_message
 *
 * Initialize the ipmi_rs from the IPMI 2.x RAKP 2 message
 *
 * The offset should point the first byte of the the RAKP 2 payload when this
 * function is called.
 *
 * param rsp [in/out] reading from the data variable and writing to the rakp 2
 *       section
 * param offset [in] tells us where the rakp2 payload starts
 * param auth_alg [in] describes the authentication algorithm was agreed upon in
 *       the open session request/response phase.  We need to know that here so
 *       that we know how many bytes (if any) to read fromt the packet.
 *
 * returns 0 on success, 1 on error
 */
void
read_rakp2_message(
					struct ipmi_rs * rsp,
					int offset,
					uint8_t auth_alg)
{
	 int i;

	 /*  Message tag */
	 rsp->payload.rakp2_message.message_tag = rsp->data[offset];

	 /* RAKP response code */
	 rsp->payload.rakp2_message.rakp_return_code = rsp->data[offset + 1];

	/* Console session ID */
	rsp->payload.rakp2_message.console_id = ipmi32toh(&rsp->data[offset + 4]);

	/* BMC random number */
	memcpy(&(rsp->payload.rakp2_message.bmc_rand),
	       array_letoh(&rsp->data[offset + 8], 16),
	       16);

	/* BMC GUID */
	memcpy(&(rsp->payload.rakp2_message.bmc_guid),
	       array_letoh(&rsp->data[offset + 24], 16),
	       16);

	 /* Key exchange authentication code */
	 switch (auth_alg)
	 {
	 case  IPMI_AUTH_RAKP_NONE:
		 /* Nothing to do here */
		 break;

	 case IPMI_AUTH_RAKP_HMAC_SHA1:
		/* We need to copy 20 bytes */
		for (i = 0; i < IPMI_SHA_DIGEST_LENGTH; ++i) {
			rsp->payload.rakp2_message.key_exchange_auth_code[i] =
				rsp->data[offset + 40 + i];
		}
		break;

	 case IPMI_AUTH_RAKP_HMAC_MD5:
		/* We need to copy 16 bytes */
		for (i = 0; i < IPMI_MD5_DIGEST_LENGTH; ++i) {
			rsp->payload.rakp2_message.key_exchange_auth_code[i] =
				rsp->data[offset + 40 + i];
		}
		break;

#ifdef HAVE_CRYPTO_SHA256
	 case IPMI_AUTH_RAKP_HMAC_SHA256:
		/* We need to copy 32 bytes */
		for (i = 0; i < IPMI_SHA256_DIGEST_LENGTH; ++i) {
			rsp->payload.rakp2_message.key_exchange_auth_code[i] =
				rsp->data[offset + 40 + i];
		}
		break;
#endif /* HAVE_CRYPTO_SHA256 */

	 default:
		lprintf(LOG_ERR, "read_rakp2_message: no support "
			"for authentication algorithm 0x%x", auth_alg);
		 assert(0);
		 break;
	 }
}



/*
 * read_rakp4_message
 *
 * Initialize the ipmi_rs from the IPMI 2.x RAKP 4 message
 *
 * The offset should point the first byte of the the RAKP 4 payload when this
 * function is called.
 *
 * param rsp [in/out] reading from the data variable and writing to the rakp
 *       4 section
 * param offset [in] tells us where the rakp4 payload starts
 * param integrity_alg [in] describes the authentication algorithm was
 *       agreed upon in the open session request/response phase.  We need
 *       to know that here so that we know how many bytes (if any) to read
 *       from the packet.
 *
 * returns 0 on success, 1 on error
 */
void
read_rakp4_message(
					struct ipmi_rs * rsp,
					int offset,
					uint8_t auth_alg)
{
	 int i;

	 /*  Message tag */
	 rsp->payload.rakp4_message.message_tag = rsp->data[offset];

	 /* RAKP response code */
	 rsp->payload.rakp4_message.rakp_return_code = rsp->data[offset + 1];

	/* Console session ID */
	rsp->payload.rakp4_message.console_id = ipmi32toh(&rsp->data[offset + 4]);

	 /* Integrity check value */
	 switch (auth_alg)
	 {
	 case  IPMI_AUTH_RAKP_NONE:
		 /* Nothing to do here */
		 break;

	 case IPMI_AUTH_RAKP_HMAC_SHA1:
		/* We need to copy 12 bytes */
		for (i = 0; i < IPMI_SHA1_AUTHCODE_SIZE; ++i) {
			rsp->payload.rakp4_message.integrity_check_value[i] =
				rsp->data[offset + 8 + i];
		}
		break;

	 case IPMI_AUTH_RAKP_HMAC_MD5:
		/* We need to copy 16 bytes */
		for (i = 0; i < IPMI_HMAC_MD5_AUTHCODE_SIZE; ++i) {
			rsp->payload.rakp4_message.integrity_check_value[i] =
				rsp->data[offset + 8 + i];
		}
		break;

#ifdef HAVE_CRYPTO_SHA256
	 case IPMI_AUTH_RAKP_HMAC_SHA256:
		/* We need to copy 16 bytes */
		for (i = 0; i < IPMI_HMAC_SHA256_AUTHCODE_SIZE; ++i) {
			rsp->payload.rakp4_message.integrity_check_value[i] =
				rsp->data[offset + 8 + i];
		}
		break;
#endif /* HAVE_CRYPTO_SHA256 */

	 default:
		 lprintf(LOG_ERR, "read_rakp4_message: no support "
			 "for authentication algorithm 0x%x", auth_alg);
		 assert(0);
		 break;	 
	 }
}




/*
 * read_session_data
 *
 * Initialize the ipmi_rsp from the session data in the packet
 *
 * The offset should point the first byte of the the IPMI session when this
 * function is called.
 *
 * param rsp     [in/out] we read from the data buffer and populate the session
 *               specific fields.
 * param offset  [in/out] should point to the beginning of the session when
 *               this function is called.  The offset will be adjusted to
 *               point to the end of the session when this function exits.
 */
void
read_session_data(
				  struct ipmi_rs * rsp,
				  int * offset)
{
	/* We expect to read different stuff depending on the authtype */
	rsp->session.authtype = rsp->data[*offset];

	if (rsp->session.authtype == IPMI_SESSION_AUTHTYPE_RMCP_PLUS)
		read_session_data_v2x(rsp, offset);
	else
		read_session_data_v15(rsp, offset);
}



/*
 * read_session_data_v2x
 *
 * Initialize the ipmi_rsp from the v2.x session header of the packet.
 *
 * The offset should point to the first byte of the the IPMI session when this
 * function is called.  When this function exits, offset will point to the
 * start of payload.
 *
 * Should decrypt and perform integrity checking here?
 *
 * param rsp    [in/out] we read from the data buffer and populate the session
 *              specific fields.
 * param offset [in/out] should point to the beginning of the session when this
 *              function is called.  The offset will be adjusted to point to
 *              the end of the session when this function exits.
 *  param s      holds our session state
 */
void
read_session_data_v2x(
					  struct ipmi_rs      * rsp,
					  int                 * offset)
{
	rsp->session.authtype = rsp->data[(*offset)++];

	rsp->session.bEncrypted     = (rsp->data[*offset] & 0x80 ? 1 : 0);
	rsp->session.bAuthenticated = (rsp->data[*offset] & 0x40 ? 1 : 0);


	/* Payload type */
	rsp->session.payloadtype = rsp->data[(*offset)++] & 0x3F;

	/* Session ID */
	rsp->session.id = ipmi32toh(&rsp->data[*offset]);
	*offset += 4;

	/* Ignored, so far */
	rsp->session.seq = ipmi32toh(&rsp->data[*offset]);
	*offset += 4;

	rsp->session.msglen = ipmi16toh(&rsp->data[*offset]);
	*offset += 2;
}



/*
 * read_session_data_v15
 *
 * Initialize the ipmi_rsp from the session header of the packet.
 *
 * The offset should point the first byte of the the IPMI session when this
 * function is called.  When this function exits, the offset will point to
 * the start of the IPMI message.
 *
 * param rsp    [in/out] we read from the data buffer and populate the session
 *              specific fields.
 * param offset [in/out] should point to the beginning of the session when this
 *              function is called.  The offset will be adjusted to point to the
 *              end of the session when this function exits.
 * param s      holds our session state
 */
void read_session_data_v15(
							struct ipmi_rs * rsp,
							int * offset)
{
	/* All v15 messages are IPMI messages */
	rsp->session.payloadtype = IPMI_PAYLOAD_TYPE_IPMI;

	rsp->session.authtype = rsp->data[(*offset)++];

	/* All v15 messages that we will receive are unencrypted/unauthenticated */
	rsp->session.bEncrypted     = 0;
	rsp->session.bAuthenticated = 0;

	/* skip the session id and sequence number fields */
	*offset += 8;

	/* This is the size of the whole payload */
	rsp->session.msglen = rsp->data[(*offset)++];
}



/*
 * read_ipmi_response
 *
 * Initialize the ipmi_rs from with the IPMI response specific data
 *
 * The offset should point the first byte of the the IPMI payload when this
 * function is called. 
 *
 * param rsp    [in/out] we read from the data buffer and populate the IPMI
 *              specific fields.
 * param offset [in/out] should point to the beginning of the IPMI payload when
 *              this function is called.
 */
void read_ipmi_response(struct ipmi_rs * rsp, int * offset)
{
	/*
	 * The data here should be decrypted by now.
	 */
	rsp->payload.ipmi_response.rq_addr = rsp->data[(*offset)++];
	rsp->payload.ipmi_response.netfn   = rsp->data[*offset] >> 2;
	rsp->payload.ipmi_response.rq_lun  = rsp->data[(*offset)++] & 0x3;
	(*offset)++;		/* checksum */
	rsp->payload.ipmi_response.rs_addr = rsp->data[(*offset)++];
	rsp->payload.ipmi_response.rq_seq  = rsp->data[*offset] >> 2;
	rsp->payload.ipmi_response.rs_lun  = rsp->data[(*offset)++] & 0x3;
	rsp->payload.ipmi_response.cmd     = rsp->data[(*offset)++]; 
	rsp->ccode                         = rsp->data[(*offset)++];

}



/*
 * read_sol_packet
 *
 * Initialize the ipmi_rs with the SOL response data
 *
 * The offset should point the first byte of the the SOL payload when this
 * function is called. 
 *
 * param rsp    [in/out] we read from the data buffer and populate the
 *              SOL specific fields.
 * param offset [in/out] should point to the beginning of the SOL payload
 *              when this function is called.
 */
void read_sol_packet(struct ipmi_rs * rsp, int * offset)
{

	/*
	 * The data here should be decrypted by now.
	 */
	rsp->payload.sol_packet.packet_sequence_number =
		rsp->data[(*offset)++] & 0x0F;

	rsp->payload.sol_packet.acked_packet_number =
		rsp->data[(*offset)++] & 0x0F;

	rsp->payload.sol_packet.accepted_character_count =
		rsp->data[(*offset)++];

	rsp->payload.sol_packet.is_nack =
		rsp->data[*offset] & 0x40;

	rsp->payload.sol_packet.transfer_unavailable =
		rsp->data[*offset] & 0x20;

	rsp->payload.sol_packet.sol_inactive =
		rsp->data[*offset] & 0x10;

	rsp->payload.sol_packet.transmit_overrun =
		rsp->data[*offset] & 0x08;

	rsp->payload.sol_packet.break_detected =
		rsp->data[(*offset)++] & 0x04;

	lprintf(LOG_DEBUG, "<<<<<<<<<< RECV FROM BMC <<<<<<<<<<<");
	lprintf(LOG_DEBUG, "< SOL sequence number     : 0x%02x",
		rsp->payload.sol_packet.packet_sequence_number);
	lprintf(LOG_DEBUG, "< SOL acked packet        : 0x%02x",
		rsp->payload.sol_packet.acked_packet_number);
	lprintf(LOG_DEBUG, "< SOL accepted char count : 0x%02x",
		rsp->payload.sol_packet.accepted_character_count);
	lprintf(LOG_DEBUG, "< SOL is nack             : %s",
		rsp->payload.sol_packet.is_nack? "true" : "false");
	lprintf(LOG_DEBUG, "< SOL xfer unavailable    : %s",
		rsp->payload.sol_packet.transfer_unavailable? "true" : "false");
	lprintf(LOG_DEBUG, "< SOL inactive            : %s",
		rsp->payload.sol_packet.sol_inactive? "true" : "false");
	lprintf(LOG_DEBUG, "< SOL transmit overrun    : %s",
		rsp->payload.sol_packet.transmit_overrun? "true" : "false");
	lprintf(LOG_DEBUG, "< SOL break detected      : %s",
		rsp->payload.sol_packet.break_detected? "true" : "false");
	lprintf(LOG_DEBUG, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<");

	if (verbose >= 5)
		printbuf(rsp->data + *offset - 4, 4, "SOL MSG FROM BMC");
}



/*
 * getIpmiPayloadWireRep
 *
 * param out [out] will contain our wire representation
 * param req [in] is the IPMI request to be written
 * param crypt_alg [in] specifies the encryption to use
 * param rq_seq [in] is the IPMI command sequence number.
 */
void getIpmiPayloadWireRep(
							struct ipmi_intf       * intf,  /* in out */
							struct ipmi_v2_payload * payload,  /* in  */
							uint8_t  * msg,
							struct ipmi_rq * req,
							uint8_t    rq_seq,
							uint8_t curr_seq)
{
	int cs, tmp, len;
	int cs2 = 0;
	int cs3 = 0;
	uint8_t ourAddress = intf->my_addr;
	uint8_t bridgedRequest = 0;

	if (ourAddress == 0)
		ourAddress = IPMI_BMC_SLAVE_ADDR;

	len = 0;

	/* IPMI Message Header -- Figure 13-4 of the IPMI v2.0 spec */
	if ((intf->target_addr == ourAddress) || (!bridgePossible)) {
		cs = len;
	} else {
		bridgedRequest = 1;

		if(intf->transit_addr != ourAddress && intf->transit_addr != 0)
		{
			bridgedRequest++;
		}
		/* bridged request: encapsulate w/in Send Message */
		cs = len;
		msg[len++] = IPMI_BMC_SLAVE_ADDR;
		msg[len++] = IPMI_NETFN_APP << 2;
		tmp = len - cs;
		msg[len++] = ipmi_csum(msg+cs, tmp);
		cs2 = len;
		msg[len++] = IPMI_REMOTE_SWID;
		msg[len++] = curr_seq << 2;

		
		msg[len++] = 0x34;			/* Send Message rqst */
		if(bridgedRequest == 2)
			msg[len++] = (0x40|intf->transit_channel); /* Track request*/
		else
			msg[len++] = (0x40|intf->target_channel);    /* Track request*/
			
		payload->payload_length += 7;
		cs = len;
		
		if(bridgedRequest == 2)
		{
			/* bridged request: encapsulate w/in Send Message */
			cs = len;
			msg[len++] = intf->transit_addr;
			msg[len++] = IPMI_NETFN_APP << 2;
			tmp = len - cs;
			msg[len++] = ipmi_csum(msg+cs, tmp);
			cs3 = len;
			msg[len++] = IPMI_REMOTE_SWID;
			msg[len++] = curr_seq << 2;
			msg[len++] = 0x34;			/* Send Message rqst */
			msg[len++] = (0x40|intf->target_channel); /* Track request*/

			payload->payload_length += 7;

			cs = len;
		}
	}

        lprintf(LOG_DEBUG,"%s RqAddr %#x transit %#x:%#x target %#x:%#x "
		"bridgePossible %d",
		bridgedRequest ? "Bridging" : "Local",
		intf->my_addr, intf->transit_addr, intf->transit_channel,
		intf->target_addr, intf->target_channel,
		bridgePossible);

	/* rsAddr */
	if (bridgedRequest)
		msg[len++] = intf->target_addr;
	else
		msg[len++] = IPMI_BMC_SLAVE_ADDR;

	/* net Fn */
	msg[len++] = req->msg.netfn << 2 | (req->msg.lun & 3);
	tmp = len - cs;

	/* checkSum */
	msg[len++] = ipmi_csum(msg+cs, tmp);
	cs = len;

	/* rqAddr */
	if (bridgedRequest < 2)
		msg[len++] = IPMI_REMOTE_SWID;
	else  /* Bridged message */
		msg[len++] = intf->my_addr;

	/* rqSeq / rqLUN */
	msg[len++] = rq_seq << 2;

	/* cmd */
	msg[len++] = req->msg.cmd;

	/* message data */
	if (req->msg.data_len) {
		memcpy(msg + len, req->msg.data, req->msg.data_len);
		len += req->msg.data_len;
	}

	/* second checksum */
	tmp = len - cs;
	msg[len++] = ipmi_csum(msg+cs, tmp);

	/* Dual bridged request: 2nd checksum */
	if (bridgedRequest == 2) {
		tmp = len - cs3;
		msg[len++] = ipmi_csum(msg+cs3, tmp);
		payload->payload_length += 1;
	}

	/* bridged request: 2nd checksum */
	if (bridgedRequest) {
		tmp = len - cs2;
		msg[len++] = ipmi_csum(msg+cs2, tmp);
		payload->payload_length += 1;
	}
}



/*
 * getSolPayloadWireRep
 *
 * param msg [out] will contain our wire representation
 * param payload [in] holds the v2 payload with our SOL data
 */
void getSolPayloadWireRep(
						  uint8_t          * msg,     /* output */
						  struct ipmi_v2_payload * payload) /* input */
{
	int i = 0;

	lprintf(LOG_DEBUG, ">>>>>>>>>> SENDING TO BMC >>>>>>>>>>");
	lprintf(LOG_DEBUG, "> SOL sequence number     : 0x%02x",
		payload->payload.sol_packet.packet_sequence_number);
	lprintf(LOG_DEBUG, "> SOL acked packet        : 0x%02x",
		payload->payload.sol_packet.acked_packet_number);
	lprintf(LOG_DEBUG, "> SOL accepted char count : 0x%02x",
		payload->payload.sol_packet.accepted_character_count);
	lprintf(LOG_DEBUG, "> SOL is nack             : %s",
		payload->payload.sol_packet.is_nack ? "true" : "false");
	lprintf(LOG_DEBUG, "> SOL assert ring wor     : %s",
		payload->payload.sol_packet.assert_ring_wor ? "true" : "false");
	lprintf(LOG_DEBUG, "> SOL generate break      : %s",
		payload->payload.sol_packet.generate_break ? "true" : "false");
	lprintf(LOG_DEBUG, "> SOL deassert cts        : %s",
		payload->payload.sol_packet.deassert_cts ? "true" : "false");
	lprintf(LOG_DEBUG, "> SOL deassert dcd dsr    : %s",
		payload->payload.sol_packet.deassert_dcd_dsr ? "true" : "false");
	lprintf(LOG_DEBUG, "> SOL flush inbound       : %s",
		payload->payload.sol_packet.flush_inbound ? "true" : "false");
	lprintf(LOG_DEBUG, "> SOL flush outbound      : %s",
		payload->payload.sol_packet.flush_outbound ? "true" : "false");

	msg[i++] = payload->payload.sol_packet.packet_sequence_number;
	msg[i++] = payload->payload.sol_packet.acked_packet_number;
	msg[i++] = payload->payload.sol_packet.accepted_character_count;

	msg[i]    = payload->payload.sol_packet.is_nack           ? 0x40 : 0;
	msg[i]   |= payload->payload.sol_packet.assert_ring_wor   ? 0x20 : 0;
	msg[i]   |= payload->payload.sol_packet.generate_break    ? 0x10 : 0;
	msg[i]   |= payload->payload.sol_packet.deassert_cts      ? 0x08 : 0;
	msg[i]   |= payload->payload.sol_packet.deassert_dcd_dsr  ? 0x04 : 0;
	msg[i]   |= payload->payload.sol_packet.flush_inbound     ? 0x02 : 0;
	msg[i++] |= payload->payload.sol_packet.flush_outbound    ? 0x01 : 0;

	/* We may have data to add */
	memcpy(msg + i,
			payload->payload.sol_packet.data,
			payload->payload.sol_packet.character_count);

	lprintf(LOG_DEBUG, "> SOL character count     : %d",
		payload->payload.sol_packet.character_count);
	lprintf(LOG_DEBUG, ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");

	if (verbose >= 5 && payload->payload.sol_packet.character_count)
		printbuf(payload->payload.sol_packet.data, payload->payload.sol_packet.character_count, "SOL SEND DATA");

	/*
	 * At this point, the payload length becomes the whole payload
	 * length, including the 4 bytes at the beginning of the SOL
	 * packet
	 */
	payload->payload_length = payload->payload.sol_packet.character_count + 4;
}



/*
 * ipmi_lanplus_build_v2x_msg
 *
 * Encapsulates the payload data to create the IPMI v2.0 / RMCP+ packet.
 * 
 *
 * IPMI v2.0 LAN Request Message Format
 * +----------------------+
 * |  rmcp.ver            | 4 bytes
 * |  rmcp.__reserved     |
 * |  rmcp.seq            |
 * |  rmcp.class          |
 * +----------------------+
 * |  session.authtype    | 10 bytes
 * |  session.payloadtype |
 * |  session.id          |
 * |  session.seq         |
 * +----------------------+
 * |  message length      | 2 bytes
 * +----------------------+
 * | Confidentiality Hdr  | var (possibly absent)
 * +----------------------+
 * |  Paylod              | var Payload
 * +----------------------+
 * | Confidentiality Trlr | var (possibly absent)
 * +----------------------+
 * | Integrity pad        | var (possibly absent)
 * +----------------------+
 * | Pad length           | 1 byte (WTF?)
 * +----------------------+
 * | Next Header          | 1 byte (WTF?)
 * +----------------------+
 * | Authcode             | var (possibly absent)
 * +----------------------+
 */
void
ipmi_lanplus_build_v2x_msg(
							struct ipmi_intf       * intf,     /* in  */
							struct ipmi_v2_payload * payload,  /* in  */
							int                    * msg_len,  /* out */
							uint8_t         ** msg_data, /* out */
							uint8_t curr_seq)
{
	uint32_t session_trailer_length = 0;
	struct ipmi_session * session = intf->session;
	struct rmcp_hdr rmcp = {
		.ver		= RMCP_VERSION_1,
		.class		= RMCP_CLASS_IPMI,
		.seq		= 0xff,
	};

	/* msg will hold the entire message to be sent */
	uint8_t * msg;
	int len = 0;


	len =
		sizeof(rmcp)                +  // RMCP Header (4)
		10                          +  // IPMI Session Header
		2                           +  // Message length
		payload->payload_length     +  // The actual payload
		IPMI_MAX_INTEGRITY_PAD_SIZE +  // Integrity Pad
		1                           +  // Pad Length
		1                           +  // Next Header
		IPMI_MAX_AUTH_CODE_SIZE;       // Authcode


	msg = malloc(len);
	if (!msg) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return;
	}
	memset(msg, 0, len);

	/*
	 *------------------------------------------
	 * RMCP HEADER
	 *------------------------------------------
	 */
	memcpy(msg, &rmcp, sizeof(rmcp));
	len = sizeof(rmcp);


	/*
	 *------------------------------------------
	 * IPMI SESSION HEADER
	 *------------------------------------------
	 */
	/* ipmi session Auth Type / Format is always 0x06 for IPMI v2 */
	msg[IPMI_LANPLUS_OFFSET_AUTHTYPE] = 0x06;

	/* Payload Type -- also specifies whether were authenticated/encyrpted */
	msg[IPMI_LANPLUS_OFFSET_PAYLOAD_TYPE] = payload->payload_type;

	if (session->v2_data.session_state == LANPLUS_STATE_ACTIVE)
	{
		msg[IPMI_LANPLUS_OFFSET_PAYLOAD_TYPE] |=
			((session->v2_data.crypt_alg != IPMI_CRYPT_NONE	)? 0x80 : 0x00);
		msg[IPMI_LANPLUS_OFFSET_PAYLOAD_TYPE] |=
			((session->v2_data.integrity_alg  != IPMI_INTEGRITY_NONE)? 0x40 : 0x00);
	}

	if (session->v2_data.session_state == LANPLUS_STATE_ACTIVE)
	{
		/* Session ID  -- making it LSB */
		msg[IPMI_LANPLUS_OFFSET_SESSION_ID    ] = session->v2_data.bmc_id         & 0xff;
		msg[IPMI_LANPLUS_OFFSET_SESSION_ID + 1] = (session->v2_data.bmc_id >> 8)  & 0xff;
		msg[IPMI_LANPLUS_OFFSET_SESSION_ID + 2] = (session->v2_data.bmc_id >> 16) & 0xff;
		msg[IPMI_LANPLUS_OFFSET_SESSION_ID + 3] = (session->v2_data.bmc_id >> 24) & 0xff;

		/* Sequence Number -- making it LSB */
		msg[IPMI_LANPLUS_OFFSET_SEQUENCE_NUM    ] = session->out_seq         & 0xff;
		msg[IPMI_LANPLUS_OFFSET_SEQUENCE_NUM + 1] = (session->out_seq >> 8)  & 0xff;
		msg[IPMI_LANPLUS_OFFSET_SEQUENCE_NUM + 2] = (session->out_seq >> 16) & 0xff;
		msg[IPMI_LANPLUS_OFFSET_SEQUENCE_NUM + 3] = (session->out_seq >> 24) & 0xff;
	}

	/*
	 * Payload Length is set below (we don't know how big the payload is until after
	 * encryption).
	 */

	/*
	 * Payload
	 *
	 * At this point we are ready to slam the payload in.
	 * This includes:
	 * 1) The confidentiality header
	 * 2) The payload proper (possibly encrypted)
	 * 3) The confidentiality trailer
	 *
	 */
	switch (payload->payload_type)
	{
	case IPMI_PAYLOAD_TYPE_IPMI:
		getIpmiPayloadWireRep(intf,
							  payload,  /* in  */
							  msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
							  payload->payload.ipmi_request.request,
							  payload->payload.ipmi_request.rq_seq,
							  curr_seq);
		break;

	case IPMI_PAYLOAD_TYPE_SOL:
		getSolPayloadWireRep(
							 msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
							 payload);

		if (verbose >= 5)
			printbuf(msg + IPMI_LANPLUS_OFFSET_PAYLOAD, 4, "SOL MSG TO BMC");

		len += payload->payload_length;

		break;

	case IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST:
		/* never encrypted, so our job is easy */
		memcpy(msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
				payload->payload.open_session_request.request,
				payload->payload_length);
		len += payload->payload_length;
		break;

	case IPMI_PAYLOAD_TYPE_RAKP_1:
		/* never encrypted, so our job is easy */
		memcpy(msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
				payload->payload.rakp_1_message.message,
				payload->payload_length);
		len += payload->payload_length;
		break;

	case IPMI_PAYLOAD_TYPE_RAKP_3:
		/* never encrypted, so our job is easy */
		memcpy(msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
				payload->payload.rakp_3_message.message,
				payload->payload_length);
		len += payload->payload_length;
		break;

	default:
		lprintf(LOG_ERR, "unsupported payload type 0x%x",
			payload->payload_type);
		free(msg);
		msg = NULL;
		assert(0);
		break;
	}


	/*
	 *------------------------------------------
	 * ENCRYPT THE PAYLOAD IF NECESSARY
	 *------------------------------------------
	 */
	if (session->v2_data.session_state == LANPLUS_STATE_ACTIVE)
	{
		/* Payload len is adjusted as necessary by lanplus_encrypt_payload */
		lanplus_encrypt_payload(session->v2_data.crypt_alg,        /* input  */
								session->v2_data.k2,               /* input  */
								msg + IPMI_LANPLUS_OFFSET_PAYLOAD, /* input  */
								payload->payload_length,           /* input  */
								msg + IPMI_LANPLUS_OFFSET_PAYLOAD, /* output */
								&(payload->payload_length));       /* output */

	}

	/* Now we know the payload length */
	msg[IPMI_LANPLUS_OFFSET_PAYLOAD_SIZE    ] =
		payload->payload_length        & 0xff;
	msg[IPMI_LANPLUS_OFFSET_PAYLOAD_SIZE + 1] =
		(payload->payload_length >> 8) & 0xff;


	/*
	 *------------------------------------------
	 * SESSION TRAILER
	 *------------------------------------------
	 */
	if ((session->v2_data.session_state == LANPLUS_STATE_ACTIVE) &&
		(session->v2_data.integrity_alg != IPMI_INTEGRITY_NONE))
	{
		uint32_t i;
		uint32_t hmac_length;
		uint32_t auth_length = 0;
		uint32_t integrity_pad_size = 0;
		uint32_t hmac_input_size;
		uint8_t * hmac_output;
		uint32_t start_of_session_trailer =
			IPMI_LANPLUS_OFFSET_PAYLOAD +
			payload->payload_length;


		/*
		 * Determine the required integrity pad length.  We have to make the
		 * data range covered by the authcode a multiple of 4.
		 */
		uint32_t length_before_authcode;

		if (ipmi_oem_active(intf, "icts")) {
			length_before_authcode =
				12                          + /* the stuff before the payload */
				payload->payload_length;
		} else {
			length_before_authcode =
				12                          + /* the stuff before the payload */
				payload->payload_length     +
				1                           + /* pad length field  */
				1;                            /* next header field */
		}

		if (length_before_authcode % 4)
			integrity_pad_size = 4 - (length_before_authcode % 4);

		for (i = 0; i < integrity_pad_size; ++i)
			msg[start_of_session_trailer + i] = 0xFF;

		/* Pad length */
		msg[start_of_session_trailer + integrity_pad_size] = integrity_pad_size;

		/* Next Header */
		msg[start_of_session_trailer + integrity_pad_size + 1] =
			0x07; /* Hardcoded per the spec, table 13-8 */

		hmac_input_size =
			12                      +
			payload->payload_length +
			integrity_pad_size      +
			2;

		hmac_output =
			msg                         +
			IPMI_LANPLUS_OFFSET_PAYLOAD +
			payload->payload_length     +
			integrity_pad_size          +
			2;

		if (verbose > 2)
			printbuf(msg + IPMI_LANPLUS_OFFSET_AUTHTYPE, hmac_input_size, "authcode input");


		/* Auth Code */
		lanplus_HMAC(session->v2_data.integrity_alg,
					 session->v2_data.k1,                /* key        */
					 session->v2_data.k1_len,            /* key length */
					 msg + IPMI_LANPLUS_OFFSET_AUTHTYPE, /* hmac input */
					 hmac_input_size,
					 hmac_output,
					 &hmac_length);

		switch(session->v2_data.integrity_alg) {
			case IPMI_INTEGRITY_HMAC_SHA1_96:
				assert(hmac_length == IPMI_SHA_DIGEST_LENGTH);
				auth_length = IPMI_SHA1_AUTHCODE_SIZE;
				break;
			case IPMI_INTEGRITY_HMAC_MD5_128 :
				assert(hmac_length == IPMI_MD5_DIGEST_LENGTH);
				auth_length = IPMI_HMAC_MD5_AUTHCODE_SIZE;
				break;
#ifdef HAVE_CRYPTO_SHA256
			case IPMI_INTEGRITY_HMAC_SHA256_128:
				assert(hmac_length == IPMI_SHA256_DIGEST_LENGTH);
				auth_length = IPMI_HMAC_SHA256_AUTHCODE_SIZE;
				break;
#endif /* HAVE_CRYPTO_SHA256 */
			default:
				assert(0);
				break;
		}

		if (verbose > 2)
			printbuf(hmac_output, auth_length, "authcode output");

		/* Set session_trailer_length appropriately */
		session_trailer_length =
			integrity_pad_size +
			2                  + /* pad length + next header */
			auth_length;         /* Size of the authcode. We only
			                      * use the first 12(SHA1) or
			                      * 16(MD5/SHA256) bytes.
			                      */
	}


	++(session->out_seq);
	if (!session->out_seq)
		++(session->out_seq);

	*msg_len =
		IPMI_LANPLUS_OFFSET_PAYLOAD +
		payload->payload_length     +
		session_trailer_length;
	*msg_data = msg;
}



/*
 * ipmi_lanplus_build_v2x_ipmi_cmd
 *
 * Wraps ipmi_lanplus_build_v2x_msg and returns a new entry object for the
 * command
 *
 */
static struct ipmi_rq_entry *
ipmi_lanplus_build_v2x_ipmi_cmd(
								struct ipmi_intf * intf,
								struct ipmi_rq * req,
								int isRetry)
{
	struct ipmi_v2_payload v2_payload;
	struct ipmi_rq_entry * entry;

	/*
	 * We have a problem.  we need to know the sequence number here,
	 * because we use it in our stored entry.  But we also need to
	 * know the sequence number when we generate our IPMI
	 * representation far below.
	 */
	static uint8_t curr_seq = 0;

	if( isRetry == 0 )
		curr_seq += 1;

	if (curr_seq >= 64)
		curr_seq = 0;


	/* IPMI Message Header -- Figure 13-4 of the IPMI v2.0 spec */
	if ((intf->target_addr == intf->my_addr) || (!bridgePossible)) {
		entry = ipmi_req_add_entry(intf, req, curr_seq);
	/* it's a bridge command */
	} else {
		/* Add entry for cmd */
		entry = ipmi_req_add_entry(intf, req, curr_seq);

		if (entry) {
			entry->req.msg.target_cmd = entry->req.msg.cmd;
			entry->req.msg.cmd = 0x34;

			if (intf->transit_addr &&
					intf->transit_addr != intf->my_addr)
				entry->bridging_level = 2;
			else
				entry->bridging_level = 1;
		}
	}

	if (!entry)
		return NULL;

	// Build our payload
	v2_payload.payload_type                 = IPMI_PAYLOAD_TYPE_IPMI;
	v2_payload.payload_length               = req->msg.data_len + 7;
	v2_payload.payload.ipmi_request.request = req;
	v2_payload.payload.ipmi_request.rq_seq  = curr_seq;

	ipmi_lanplus_build_v2x_msg(intf,                // in
					&v2_payload,         // in
					&(entry->msg_len),   // out
					&(entry->msg_data),  // out
					curr_seq); 		// in

	return entry;
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
ipmi_lanplus_build_v15_ipmi_cmd(
								struct ipmi_intf * intf,
								struct ipmi_rq * req)
{
	struct rmcp_hdr rmcp = {
		.ver		= RMCP_VERSION_1,
		.class		= RMCP_CLASS_IPMI,
		.seq		= 0xff,
	};
	uint8_t * msg;
	int cs, mp, len = 0, tmp;
	struct ipmi_session  * session = intf->session;
	struct ipmi_rq_entry * entry;

	entry = ipmi_req_add_entry(intf, req, 0);
	if (!entry)
		return NULL;

	len = req->msg.data_len + 21;

	msg = malloc(len);
	if (!msg) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return NULL;
	}
	memset(msg, 0, len);

	/* rmcp header */
	memcpy(msg, &rmcp, sizeof(rmcp));
	len = sizeof(rmcp);

	/*
	 * ipmi session header
	 */
	/* Authtype should always be none for 1.5 packets sent from this
	 * interface
	 */
	msg[len++] = IPMI_SESSION_AUTHTYPE_NONE;

	msg[len++] = session->out_seq & 0xff;
	msg[len++] = (session->out_seq >> 8) & 0xff;
	msg[len++] = (session->out_seq >> 16) & 0xff;
	msg[len++] = (session->out_seq >> 24) & 0xff;

	/*
	 * The session ID should be all zeroes for pre-session commands.  We
	 * should only be using the 1.5 interface for the pre-session Get
	 * Channel Authentication Capabilities command
	 */
	msg[len++] = 0;
	msg[len++] = 0;
	msg[len++] = 0;
	msg[len++] = 0;

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

	entry->rq_seq = 0;

	msg[len++] = entry->rq_seq << 2;
	msg[len++] = req->msg.cmd;

	lprintf(LOG_DEBUG+1, ">> IPMI Request Session Header");
	lprintf(LOG_DEBUG+1, ">>   Authtype   : %s",
		val2str(IPMI_SESSION_AUTHTYPE_NONE, ipmi_authtype_session_vals));
	lprintf(LOG_DEBUG+1, ">>   Sequence   : 0x%08lx",
		(long)session->out_seq);
	lprintf(LOG_DEBUG+1, ">>   Session ID : 0x%08lx",
		(long)0);

	lprintf(LOG_DEBUG+1, ">> IPMI Request Message Header");
	lprintf(LOG_DEBUG+1, ">>   Rs Addr    : %02x", IPMI_BMC_SLAVE_ADDR);
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

	entry->msg_len = len;
	entry->msg_data = msg;

	return entry;
}



/*
 * is_sol_packet
 */
static int
is_sol_packet(struct ipmi_rs * rsp)
{
	return (rsp                                                           &&
			(rsp->session.authtype    == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
			(rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL));
}



/*
 * sol_response_acks_packet
 */
static int
sol_response_acks_packet(
						 struct ipmi_rs         * rsp,
						 struct ipmi_v2_payload * payload)
{
	return (is_sol_packet(rsp)                                            &&
			payload                                                       &&
			(payload->payload_type    == IPMI_PAYLOAD_TYPE_SOL)           && 
			(rsp->payload.sol_packet.acked_packet_number ==
			 payload->payload.sol_packet.packet_sequence_number));
}



/*
 * ipmi_lanplus_send_payload
 *
 */
struct ipmi_rs *
ipmi_lanplus_send_payload(
						  struct ipmi_intf * intf,
						  struct ipmi_v2_payload * payload)
{
	struct ipmi_rs      * rsp = NULL;
	uint8_t             * msg_data = NULL;
	int                   msg_length;
	struct ipmi_session * session = intf->session;
	struct ipmi_rq_entry * entry = NULL;
	int                   try = 0;
	int                   xmit = 1;
	time_t                ltime;
	uint32_t	      saved_timeout;

	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

	/*
	 * The session timeout is initialized in the above interface open,
	 * so it will only be valid after the open completes.
	 */
	saved_timeout = session->timeout;
	while (try < intf->ssn_params.retry) {
		//ltime = time(NULL);

		if (xmit) {
			ltime = time(NULL);

			if (payload->payload_type == IPMI_PAYLOAD_TYPE_IPMI)
			{
				/*
				 * Build an IPMI v1.5 or v2 command
				 */
				struct ipmi_rq * ipmi_request = payload->payload.ipmi_request.request;

				lprintf(LOG_DEBUG, "");
				lprintf(LOG_DEBUG, ">> Sending IPMI command payload");
				lprintf(LOG_DEBUG, ">>    netfn   : 0x%02x", ipmi_request->msg.netfn);
				lprintf(LOG_DEBUG, ">>    command : 0x%02x", ipmi_request->msg.cmd);

				if (verbose > 1)
				{
					uint16_t i;
					fprintf(stderr, ">>    data    : ");
					for (i = 0; i < ipmi_request->msg.data_len; ++i)
						fprintf(stderr, "0x%02x ", ipmi_request->msg.data[i]);
					fprintf(stderr, "\n\n");
				}


				/*
				 * If we are presession, and the command is GET CHANNEL AUTHENTICATION
				 * CAPABILITIES, we will build the command in v1.5 format.  This is so
				 * that we can ask any server whether it supports IPMI v2 / RMCP+
				 * before we attempt to open a v2.x session.
				 */
				if ((ipmi_request->msg.netfn == IPMI_NETFN_APP) &&
					 (ipmi_request->msg.cmd   == IPMI_GET_CHANNEL_AUTH_CAP) &&
					 (session->v2_data.bmc_id  == 0)) // jme - check
				{
					lprintf(LOG_DEBUG+1, "BUILDING A v1.5 COMMAND");
					entry = ipmi_lanplus_build_v15_ipmi_cmd(intf, ipmi_request);
				}
				else
				{
					int isRetry = ( try > 0 ? 1 : 0 );

					lprintf(LOG_DEBUG+1, "BUILDING A v2 COMMAND");
					entry = ipmi_lanplus_build_v2x_ipmi_cmd(intf, ipmi_request, isRetry);
				}

				if (!entry) {
					lprintf(LOG_ERR, "Aborting send command, unable to build");
					return NULL;
				}

				msg_data   = entry->msg_data;
				msg_length = entry->msg_len;
			}

			else if (payload->payload_type == IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST)
			{
				lprintf(LOG_DEBUG, ">> SENDING AN OPEN SESSION REQUEST\n");
				assert(session->v2_data.session_state == LANPLUS_STATE_PRESESSION
						|| session->v2_data.session_state == LANPLUS_STATE_OPEN_SESSION_SENT);

				ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								payload,     /* in  */
								&msg_length, /* out */
								&msg_data,   /* out */
								0);  /* irrelevant for this msg*/

			}

			else if (payload->payload_type == IPMI_PAYLOAD_TYPE_RAKP_1)
			{
				lprintf(LOG_DEBUG, ">> SENDING A RAKP 1 MESSAGE\n");
				assert(session->v2_data.session_state ==
						 LANPLUS_STATE_OPEN_SESSION_RECEIEVED);

				ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								payload,     /* in  */
								&msg_length, /* out */
								&msg_data,   /* out */
								0);  /* irrelevant for this msg*/

			}

			else if (payload->payload_type == IPMI_PAYLOAD_TYPE_RAKP_3)
			{
				lprintf(LOG_DEBUG, ">> SENDING A RAKP 3 MESSAGE\n");
				assert(session->v2_data.session_state ==
						 LANPLUS_STATE_RAKP_2_RECEIVED);

				ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								payload,     /* in  */
								&msg_length, /* out */
								&msg_data,   /* out */
								0);  /* irrelevant for this msg*/

			}

			else if (payload->payload_type == IPMI_PAYLOAD_TYPE_SOL)
			{
				lprintf(LOG_DEBUG, ">> SENDING A SOL MESSAGE\n");
				assert(session->v2_data.session_state == LANPLUS_STATE_ACTIVE);

				ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								payload,     /* in  */
								&msg_length, /* out */
								&msg_data,   /* out */
								0);  /* irrelevant for this msg*/
			}

			else
			{
				lprintf(LOG_ERR, "Payload type 0x%0x is unsupported!",
					payload->payload_type);
				assert(0);
			}


			if (ipmi_lan_send_packet(intf, msg_data, msg_length) < 0) {
				lprintf(LOG_ERR, "IPMI LAN send command failed");
				return NULL;
			}
		}

		/* if we are set to noanswer we do not expect response */
		if (intf->noanswer)
			break;

		usleep(100); 			/* Not sure what this is for */

		/* Remember our connection state */
		switch (payload->payload_type)
		{
		case IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST:
			session->v2_data.session_state = LANPLUS_STATE_OPEN_SESSION_SENT;
			/* not retryable for timeouts, force no retry */
			try = intf->ssn_params.retry;
			break;
		case IPMI_PAYLOAD_TYPE_RAKP_1:
			session->v2_data.session_state = LANPLUS_STATE_RAKP_1_SENT;
			/* not retryable for timeouts, force no retry */
			try = intf->ssn_params.retry;
			break;
		case IPMI_PAYLOAD_TYPE_RAKP_3:
			/* not retryable for timeouts, force no retry */
			try = intf->ssn_params.retry;
			session->v2_data.session_state = LANPLUS_STATE_RAKP_3_SENT;
			break;
		}


		/*
		 * Special case for SOL outbound packets.
		 */
		if (payload->payload_type == IPMI_PAYLOAD_TYPE_SOL)
		{
			if (! payload->payload.sol_packet.packet_sequence_number)
			{
				/* We're just sending an ACK.  No need to retry. */
				break;
			}


			rsp = ipmi_lanplus_recv_sol(intf); /* Grab the next packet */

			if (!is_sol_packet(rsp)) {
				break;
			}

			if (sol_response_acks_packet(rsp, payload))
				break;

			else if (is_sol_packet(rsp) && rsp->data_len)
			{
				/*
				 * We're still waiting for our ACK, but we more data from
				 * the BMC
				 */
				intf->session->sol_data.sol_input_handler(rsp);
				/* In order to avoid duplicate output, just set data_len to 0 */
				rsp->data_len = 0;
				break;
			}
		}


		/* Non-SOL processing */
		else
		{
			rsp = ipmi_lan_poll_recv(intf);

			/* Duplicate Request ccode most likely indicates a response to
			   a previous retry. Ignore and keep polling. */
			while (rsp && rsp->ccode == 0xcf)
			{
				rsp = NULL;
				rsp = ipmi_lan_poll_recv(intf);
			}

			if (rsp)
				break;
			/* This payload type is retryable for timeouts. */
			if ((payload->payload_type == IPMI_PAYLOAD_TYPE_IPMI) && entry) {
				ipmi_req_remove_entry( entry->rq_seq, entry->req.msg.cmd);
			}
		}

		/* only timeout if time exceeds the timeout value */
		xmit = ((time(NULL) - ltime) >= session->timeout);

		usleep(5000);

		if (xmit) {
			/* increment session timeout by 1 second each retry */
			session->timeout++;
		}

		try++;
	}
	session->timeout = saved_timeout;

	/* IPMI messages are deleted under ipmi_lan_poll_recv() */
	switch (payload->payload_type) {
	case IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST:
	case IPMI_PAYLOAD_TYPE_RAKP_1:
	case IPMI_PAYLOAD_TYPE_RAKP_3:
	case IPMI_PAYLOAD_TYPE_SOL:
		free(msg_data);
		msg_data = NULL;
		break;
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
int is_sol_partial_ack(
						struct ipmi_intf       * intf,
						struct ipmi_v2_payload * v2_payload,
						struct ipmi_rs         * rs)
{
	int chars_to_resend = 0;

	if (v2_payload                               &&
		rs                                       &&
		is_sol_packet(rs)                        &&
		sol_response_acks_packet(rs, v2_payload) &&
		(rs->payload.sol_packet.accepted_character_count <
		 v2_payload->payload.sol_packet.character_count))
	{
		if (ipmi_oem_active(intf, "intelplus") &&
			 rs->payload.sol_packet.accepted_character_count == 0)
			return 0;

		chars_to_resend =
			v2_payload->payload.sol_packet.character_count -
			rs->payload.sol_packet.accepted_character_count;
	}

	return chars_to_resend;
}



/*
 * set_sol_packet_sequence_number
 */
static void set_sol_packet_sequence_number(
											struct ipmi_intf * intf,
											struct ipmi_v2_payload * v2_payload)
{
	/* Keep our sequence number sane */
	if (intf->session->sol_data.sequence_number > 0x0F)
		intf->session->sol_data.sequence_number = 1;

	v2_payload->payload.sol_packet.packet_sequence_number =
		intf->session->sol_data.sequence_number++;
}



/*
 * ipmi_lanplus_send_sol
 *
 * Sends a SOL packet..  We handle partial ACK/NACKs from the BMC here.
 *
 * Returns a pointer to the SOL ACK we received, or
 *         0 on failure
 * 
 */
struct ipmi_rs *
ipmi_lanplus_send_sol(
					  struct ipmi_intf * intf,
					  struct ipmi_v2_payload * v2_payload)
{
	struct ipmi_rs * rs;

	/*
	 * chars_to_resend indicates either that we got a NACK telling us
	 * that we need to resend some part of our data.
	 */
	int chars_to_resend = 0;

	v2_payload->payload_type   = IPMI_PAYLOAD_TYPE_SOL;

	/*
	 * Payload length is just the length of the character
	 * data here.
	 */
	v2_payload->payload_length = v2_payload->payload.sol_packet.character_count;

	v2_payload->payload.sol_packet.acked_packet_number = 0; /* NA */

	set_sol_packet_sequence_number(intf, v2_payload);

	v2_payload->payload.sol_packet.accepted_character_count = 0; /* NA */

	rs = ipmi_lanplus_send_payload(intf, v2_payload);

	/* Determine if we need to resend some of our data */
	chars_to_resend = is_sol_partial_ack(intf, v2_payload, rs);

	while (rs && !rs->payload.sol_packet.transfer_unavailable &&
			 !rs->payload.sol_packet.is_nack &&
			 chars_to_resend)
	{
		/*
		 * We first need to handle any new data we might have
		 * received in our NACK
		 */
		if (rs->data_len)
			intf->session->sol_data.sol_input_handler(rs);

		set_sol_packet_sequence_number(intf, v2_payload);

		/* Just send the required data */
		memmove(v2_payload->payload.sol_packet.data,
				v2_payload->payload.sol_packet.data +
				rs->payload.sol_packet.accepted_character_count,
				chars_to_resend);

		v2_payload->payload.sol_packet.character_count = chars_to_resend;

		v2_payload->payload_length = v2_payload->payload.sol_packet.character_count;

		rs = ipmi_lanplus_send_payload(intf, v2_payload);

		chars_to_resend = is_sol_partial_ack(intf, v2_payload, rs);
	}

	return rs;
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
 * returns the number of new bytes in the SOL packet
 */
static int
check_sol_packet_for_new_data(
							  struct ipmi_rs *rsp)
{
	static uint8_t last_received_sequence_number = 0;
	static uint8_t last_received_byte_count      = 0;
	int new_data_size                                  = 0;


	if (rsp &&
		(rsp->session.authtype    == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
		(rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL))
	{
		/* Store the data length before we mod it */
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
		if (rsp->payload.sol_packet.packet_sequence_number)
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
ack_sol_packet(
				struct ipmi_intf * intf,
				struct ipmi_rs * rsp)
{
	if (rsp                                                           &&
		(rsp->session.authtype    == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
		(rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL)           &&
		(rsp->payload.sol_packet.packet_sequence_number))
	{
		struct ipmi_v2_payload ack;

		memset(&ack, 0, sizeof(struct ipmi_v2_payload));

		ack.payload_type   = IPMI_PAYLOAD_TYPE_SOL;

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

		ipmi_lanplus_send_payload(intf, &ack);
	}
}



/*
 * ipmi_lanplus_recv_sol
 *
 * Receive a SOL packet and send an ACK in response.
 *
 */
struct ipmi_rs *
ipmi_lanplus_recv_sol(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp = ipmi_lan_poll_recv(intf);

	if (rsp && rsp->session.authtype != 0)
	{
		ack_sol_packet(intf, rsp);

		/*
		 * Remembers the data sent, and alters the data to just
		 * include the new stuff.
		 */
		check_sol_packet_for_new_data(rsp);
	}
	return rsp;
}



/**
 * ipmi_lanplus_send_ipmi_cmd
 *
 * Build a payload request and dispatch it.
 */
struct ipmi_rs *
ipmi_lanplus_send_ipmi_cmd(
							struct ipmi_intf * intf,
							struct ipmi_rq * req)
{
	struct ipmi_v2_payload v2_payload;

	v2_payload.payload_type = IPMI_PAYLOAD_TYPE_IPMI;
	v2_payload.payload.ipmi_request.request = req;

	return ipmi_lanplus_send_payload(intf, &v2_payload);
}


/*
 * ipmi_get_auth_capabilities_cmd
 *
 * This command may have to be sent twice.  We first ask for the
 * authentication capabilities with the "request IPMI v2 data bit"
 * set.  If this fails, we send the same command without that bit
 * set.
 *
 * param intf is the initialized (but possibly) pre-session interface
 *       on which we will send the command
 * param auth_cap [out] will be initialized to hold the Get Channel
 *       Authentication Capabilities return data on success.  Its
 *       contents will be undefined on error.
 * 
 * returns 0 on success
 *         non-zero if we were unable to contact the BMC, or we cannot
 *         get a successful response
 *
 */
static int
ipmi_get_auth_capabilities_cmd(
								struct ipmi_intf * intf,
								struct get_channel_auth_cap_rsp * auth_cap)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];
	uint8_t backupBridgePossible;

	backupBridgePossible = bridgePossible;

	bridgePossible = 0;

	msg_data[0] = IPMI_LAN_CHANNEL_E | 0x80; // Ask for IPMI v2 data as well
	msg_data[1] = intf->ssn_params.privlvl;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_APP;            // 0x06
	req.msg.cmd      = IPMI_GET_CHANNEL_AUTH_CAP; // 0x38
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		/*
		 * It's very possible that this failed because we asked for IPMI
		 * v2 data. Ask again, without requesting IPMI v2 data.
		 */
		msg_data[0] &= 0x7F;

		rsp = intf->sendrecv(intf, &req);

		if (!rsp) {
			lprintf(LOG_INFO, "Get Auth Capabilities error");
			return 1;
		}
		if (rsp->ccode) {
			lprintf(LOG_INFO, "Get Auth Capabilities error: %s",
				val2str(rsp->ccode, completion_code_vals));
			return 1;
		}
	}


	memcpy(auth_cap,
			rsp->data,
			sizeof(struct get_channel_auth_cap_rsp));

	bridgePossible = backupBridgePossible;

	return 0;
}



static int
ipmi_close_session_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4];
	uint8_t backupBridgePossible;

	if (!intf->session
			|| intf->session->v2_data.session_state != LANPLUS_STATE_ACTIVE)
		return -1;

	backupBridgePossible = bridgePossible;

	intf->target_addr = IPMI_BMC_SLAVE_ADDR;
	bridgePossible = 0;

	htoipmi32(intf->session->v2_data.bmc_id, msg_data);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		    = 0x3c;
	req.msg.data		= msg_data;
	req.msg.data_len	= 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		/* Looks like the session was closed */
		lprintf(LOG_ERR, "Close Session command failed");
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "close_session");

	if (rsp->ccode == 0x87) {
		lprintf(LOG_ERR, "Failed to Close Session: invalid "
			"session ID %08lx",
			(long)intf->session->v2_data.bmc_id);
		return -1;
	}
	if (rsp->ccode) {
		lprintf(LOG_ERR, "Close Session command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	lprintf(LOG_DEBUG, "Closed Session %08lx\n",
		(long)intf->session->v2_data.bmc_id);

	bridgePossible = backupBridgePossible;

	return 0;
}



/*
 * ipmi_lanplus_open_session
 *
 * Build and send the open session command.  See section 13.17 of the IPMI
 * v2 specification for details.
 */
static int
ipmi_lanplus_open_session(struct ipmi_intf * intf)
{
	struct ipmi_v2_payload v2_payload;
	struct ipmi_session * session = intf->session;
	uint8_t * msg;
	struct ipmi_rs * rsp;
	/* 0 = success, 1 = error, 2 = timeout */
	int rc = 0;


	/*
	 * Build an Open Session Request Payload
	 */
	msg = (uint8_t*)malloc(IPMI_OPEN_SESSION_REQUEST_SIZE);
	if (!msg) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return 1;
	}

	memset(msg, 0, IPMI_OPEN_SESSION_REQUEST_SIZE);

	msg[0] = 0; /* Message tag */
	if (ipmi_oem_active(intf, "intelplus") || intf->ssn_params.privlvl != IPMI_SESSION_PRIV_ADMIN)
		msg[1] = intf->ssn_params.privlvl;
	else
		msg[1] = 0; /* Give us highest privlg level based on supported algorithms */
	msg[2] = 0; /* reserved */
	msg[3] = 0; /* reserved */

	/* Choose our session ID for easy recognition in the packet dump */
	session->v2_data.console_id = 0xA0A2A3A4;
	msg[4] = session->v2_data.console_id & 0xff;
	msg[5] = (session->v2_data.console_id >> 8)  & 0xff;
	msg[6] = (session->v2_data.console_id >> 16) & 0xff;
	msg[7] = (session->v2_data.console_id >> 24) & 0xff;


	if (lanplus_get_requested_ciphers(intf->ssn_params.cipher_suite_id,
									  &(session->v2_data.requested_auth_alg),
									  &(session->v2_data.requested_integrity_alg),
									  &(session->v2_data.requested_crypt_alg)))
	{
		lprintf(LOG_WARNING, "Unsupported cipher suite ID : %d\n",
				intf->ssn_params.cipher_suite_id);
		free(msg);
		msg = NULL;
		return 1;
	}


	/*
	 * Authentication payload
	 */
	msg[8]  = 0; /* specifies authentication payload */
	msg[9]  = 0; /* reserved */
	msg[10] = 0; /* reserved */
	msg[11] = 8; /* payload length */
	msg[12] = session->v2_data.requested_auth_alg;
	msg[13] = 0; /* reserved */	
	msg[14] = 0; /* reserved */
	msg[15] = 0; /* reserved */	

	/*
	 * Integrity payload
	 */
	msg[16] = 1; /* specifies integrity payload */
	msg[17] = 0; /* reserved */
	msg[18] = 0; /* reserved */
	msg[19] = 8; /* payload length */
	msg[20] = session->v2_data.requested_integrity_alg;
	msg[21] = 0; /* reserved */	
	msg[22] = 0; /* reserved */
	msg[23] = 0; /* reserved */

	/*
	 * Confidentiality/Encryption payload
	 */
	msg[24] = 2; /* specifies confidentiality payload */
	msg[25] = 0; /* reserved */
	msg[26] = 0; /* reserved */
	msg[27] = 8; /* payload length */
	msg[28] = session->v2_data.requested_crypt_alg;
	msg[29] = 0; /* reserved */	
	msg[30] = 0; /* reserved */
	msg[31] = 0; /* reserved */


	v2_payload.payload_type   = IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST;
	v2_payload.payload_length = IPMI_OPEN_SESSION_REQUEST_SIZE;
	v2_payload.payload.open_session_request.request = msg;

	rsp = ipmi_lanplus_send_payload(intf, &v2_payload);

	free(msg);
	msg = NULL;
	if (!rsp ) {
		lprintf(LOG_DEBUG, "Timeout in open session response message.");
		return 2;
	}
	if (verbose)
		lanplus_dump_open_session_response(rsp);

	if (rsp->payload.open_session_response.rakp_return_code !=
		IPMI_RAKP_STATUS_NO_ERRORS)
	{
		lprintf(LOG_WARNING, "Error in open session response message : %s\n",
			val2str(rsp->payload.open_session_response.rakp_return_code,
				ipmi_rakp_return_codes));
		return 1;
	}
	else
	{
		if (rsp->payload.open_session_response.console_id !=
			 session->v2_data.console_id) {
			lprintf(LOG_WARNING, "Warning: Console session ID is not "
				"what we requested");
		}

		session->v2_data.max_priv_level =
			rsp->payload.open_session_response.max_priv_level;
		session->v2_data.bmc_id         =
			rsp->payload.open_session_response.bmc_id;
		session->v2_data.auth_alg       =
			rsp->payload.open_session_response.auth_alg;
		session->v2_data.integrity_alg  =
			rsp->payload.open_session_response.integrity_alg;
		session->v2_data.crypt_alg      =
			rsp->payload.open_session_response.crypt_alg;
		session->v2_data.session_state  =
			LANPLUS_STATE_OPEN_SESSION_RECEIEVED;


		/*
		 * Verify that we have agreed on a cipher suite
		 */
		if (rsp->payload.open_session_response.auth_alg !=
			session->v2_data.requested_auth_alg)
		{
			lprintf(LOG_WARNING, "Authentication algorithm 0x%02x is "
					"not what we requested 0x%02x\n",
					rsp->payload.open_session_response.auth_alg,
					session->v2_data.requested_auth_alg);
			rc = 1;
		}
		else if (rsp->payload.open_session_response.integrity_alg !=
				 session->v2_data.requested_integrity_alg)
		{
			lprintf(LOG_WARNING, "Integrity algorithm 0x%02x is "
					"not what we requested 0x%02x\n",
					rsp->payload.open_session_response.integrity_alg,
					session->v2_data.requested_integrity_alg);
			rc = 1;
		}
		else if (rsp->payload.open_session_response.crypt_alg !=
				 session->v2_data.requested_crypt_alg)
		{
			lprintf(LOG_WARNING, "Encryption algorithm 0x%02x is "
					"not what we requested 0x%02x\n",
					rsp->payload.open_session_response.crypt_alg,
					session->v2_data.requested_crypt_alg);
			rc = 1;
		}

	}

	return rc;
}



/*
 * ipmi_lanplus_rakp1
 *
 * Build and send the RAKP 1 message as part of the IPMI v2 / RMCP+ session
 * negotiation protocol.  We also read and validate the RAKP 2 message received
 * from the BMC, here.  See section 13.20 of the IPMI v2 specification for
 * details.
 *
 * returns 0 on success
 *         1 on failure
 *
 * Note that failure is only indicated if we have an internal error of
 * some kind. If we actually get a RAKP 2 message in response to our
 * RAKP 1 message, any errors will be stored in
 * session->v2_data.rakp2_return_code and sent to the BMC in the RAKP
 * 3 message.
 */
static int
ipmi_lanplus_rakp1(struct ipmi_intf * intf)
{
	struct ipmi_v2_payload v2_payload;
	struct ipmi_session * session = intf->session;
	uint8_t * msg;
	struct ipmi_rs * rsp;
	int rc = 0;    /* 0 = success, 1 = error, 2 = timeout */

	/*
	 * Build a RAKP 1 message
	 */
	msg = (uint8_t*)malloc(IPMI_RAKP1_MESSAGE_SIZE);
	if (!msg) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return 1;
	}
	memset(msg, 0, IPMI_RAKP1_MESSAGE_SIZE);


	msg[0] = 0; /* Message tag */

	msg[1] = 0; /* reserved */
	msg[2] = 0; /* reserved */
	msg[3] = 0; /* reserved */

	/* BMC session ID */
	msg[4] = session->v2_data.bmc_id & 0xff;
	msg[5] = (session->v2_data.bmc_id >> 8)  & 0xff;
	msg[6] = (session->v2_data.bmc_id >> 16) & 0xff;
	msg[7] = (session->v2_data.bmc_id >> 24) & 0xff;


	/* We need a 16 byte random number */
	if (lanplus_rand(session->v2_data.console_rand, 16))
	{
		// ERROR;
		lprintf(LOG_ERR, "ERROR generating random number "
			"in ipmi_lanplus_rakp1");
		free(msg);
		msg = NULL;
		return 1;
	}
	memcpy(msg + 8, session->v2_data.console_rand, 16);
	array_letoh(msg + 8, 16);

	if (verbose > 1)
		printbuf(session->v2_data.console_rand, 16,
				 ">> Console generated random number");


	/*
	 * Requested maximum privilege level.
	 */
	msg[24] = intf->ssn_params.privlvl | intf->ssn_params.lookupbit;
	session->v2_data.requested_role = msg[24];
	msg[25] = 0; /* reserved */
	msg[26] = 0; /* reserved */


	/* Username specification */
	msg[27] = strlen((const char *)intf->ssn_params.username);
	if (msg[27] > IPMI_MAX_USER_NAME_LENGTH)
	{
		lprintf(LOG_ERR, "ERROR: user name too long.  "
			"(Exceeds %d characters)",
			IPMI_MAX_USER_NAME_LENGTH);
		free(msg);
		msg = NULL;
		return 1;
	}
	memcpy(msg + 28, intf->ssn_params.username, msg[27]);

	v2_payload.payload_type = IPMI_PAYLOAD_TYPE_RAKP_1;
	if (ipmi_oem_active(intf, "i82571spt")) {
		/*
		 * The IPMI v2.0 spec hints on that all user name bytes
		 * must be occupied (29:44).  The Intel 82571 GbE refuses
		 * to establish a session if this field is shorter.
		 */
		v2_payload.payload_length = IPMI_RAKP1_MESSAGE_SIZE;
	} else {
		v2_payload.payload_length =
			IPMI_RAKP1_MESSAGE_SIZE - (16 - msg[27]);
	}
	v2_payload.payload.rakp_1_message.message = msg;

	rsp = ipmi_lanplus_send_payload(intf, &v2_payload);

	free(msg);
	msg = NULL;

	if (!rsp)
	{
		lprintf(LOG_WARNING, "> Error: no response from RAKP 1 message");
		return 2;
	}

	session->v2_data.session_state = LANPLUS_STATE_RAKP_2_RECEIVED;

	if (verbose)
		lanplus_dump_rakp2_message(rsp, session->v2_data.auth_alg);



	if (rsp->payload.rakp2_message.rakp_return_code != IPMI_RAKP_STATUS_NO_ERRORS)
	{
		lprintf(LOG_INFO, "RAKP 2 message indicates an error : %s",
			val2str(rsp->payload.rakp2_message.rakp_return_code,
				ipmi_rakp_return_codes));
		rc = 1;
	}

	else
	{
		memcpy(session->v2_data.bmc_rand, rsp->payload.rakp2_message.bmc_rand, 16);
		memcpy(session->v2_data.bmc_guid, rsp->payload.rakp2_message.bmc_guid, 16);

		if (verbose > 2)
			printbuf(session->v2_data.bmc_rand, 16, "bmc_rand");

		/*
		 * It is at this point that we have to decode the random number and determine
		 * whether the BMC has authenticated.
		 */
		if (! lanplus_rakp2_hmac_matches(session,
										 rsp->payload.rakp2_message.key_exchange_auth_code, 
										 intf))
		{
			/* Error */
			lprintf(LOG_INFO, "> RAKP 2 HMAC is invalid");
			session->v2_data.rakp2_return_code = IPMI_RAKP_STATUS_INVALID_INTEGRITY_CHECK_VALUE;
								rc = 1;
		}
		else
		{
			/* Success */
			session->v2_data.rakp2_return_code = IPMI_RAKP_STATUS_NO_ERRORS;
		}
	}

	return rc;
}



/*
 * ipmi_lanplus_rakp3
 *
 * Build and send the RAKP 3 message as part of the IPMI v2 / RMCP+ session
 * negotiation protocol.  We also read and validate the RAKP 4 message received
 * from the BMC, here.  See section 13.20 of the IPMI v2 specification for
 * details.
 *
 * If the RAKP 2 return code is not IPMI_RAKP_STATUS_NO_ERRORS, we will
 * exit with an error code immediately after sendint the RAKP 3 message.
 *
 * param intf is the intf that holds all the state we are concerned with
 *
 * returns 0 on success
 *         1 on failure
 */
static int
ipmi_lanplus_rakp3(struct ipmi_intf * intf)
{
	struct ipmi_v2_payload v2_payload;
	struct ipmi_session * session = intf->session;
	uint8_t * msg;
	struct ipmi_rs * rsp;

	assert(session->v2_data.session_state == LANPLUS_STATE_RAKP_2_RECEIVED);
	
	/*
	 * Build a RAKP 3 message
	 */
	msg = (uint8_t*)malloc(IPMI_RAKP3_MESSAGE_MAX_SIZE);
	if (!msg) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		return 1;
	}
	memset(msg, 0, IPMI_RAKP3_MESSAGE_MAX_SIZE);


	msg[0] = 0; /* Message tag */
	msg[1] = session->v2_data.rakp2_return_code;
	
	msg[2] = 0; /* reserved */
	msg[3] = 0; /* reserved */

	/* BMC session ID */
	msg[4] = session->v2_data.bmc_id & 0xff;
	msg[5] = (session->v2_data.bmc_id >> 8)  & 0xff;
	msg[6] = (session->v2_data.bmc_id >> 16) & 0xff;
	msg[7] = (session->v2_data.bmc_id >> 24) & 0xff;

	v2_payload.payload_type                   = IPMI_PAYLOAD_TYPE_RAKP_3;
	v2_payload.payload_length                 = 8;
	v2_payload.payload.rakp_3_message.message = msg;

	/*
	 * If the rakp2 return code indicates and error, we don't have to
	 * generate an authcode or session integrity key.  In that case, we
	 * are simply sending a RAKP 3 message to indicate to the BMC that the
	 * RAKP 2 message caused an error.
	 */
	if (session->v2_data.rakp2_return_code == IPMI_RAKP_STATUS_NO_ERRORS)
	{
		uint32_t auth_length;
		
		if (lanplus_generate_rakp3_authcode(msg + 8, session, &auth_length, intf))
		{
			/* Error */
			lprintf(LOG_INFO, "> Error generating RAKP 3 authcode");
			free(msg);
			msg = NULL;
			return 1;
		}
		else
		{
			/* Success */
			v2_payload.payload_length += auth_length;
		}

		/* Generate our Session Integrity Key, K1, and K2 */
		if (lanplus_generate_sik(session, intf))
		{
			/* Error */
			lprintf(LOG_INFO, "> Error generating session integrity key");
			free(msg);
			msg = NULL;
			return 1;
		}
		else if (lanplus_generate_k1(session))
		{
			/* Error */
			lprintf(LOG_INFO, "> Error generating K1 key");
			free(msg);
			msg = NULL;
			return 1;
		}
		else if (lanplus_generate_k2(session))
		{
			/* Error */
			lprintf(LOG_INFO, "> Error generating K1 key");
			free(msg);
			msg = NULL;
			return 1;
		}
	}
	

	rsp = ipmi_lanplus_send_payload(intf, &v2_payload);

	free(msg);
	msg = NULL;

	if (session->v2_data.rakp2_return_code != IPMI_RAKP_STATUS_NO_ERRORS)
	{
		/*
		 * If the previous RAKP 2 message received was deemed erroneous,
		 * we have nothing else to do here.  We only sent the RAKP 3 message
		 * to indicate to the BMC that the RAKP 2 message failed.
		 */
		return 1;
	}
	else if (!rsp)
	{
		lprintf(LOG_WARNING, "> Error: no response from RAKP 3 message");
		return 2;
	}


	/*
	 * We have a RAKP 4 message to chew on.
	 */
	if (verbose)
		lanplus_dump_rakp4_message(rsp, session->v2_data.auth_alg);
	

	if (rsp->payload.open_session_response.rakp_return_code != IPMI_RAKP_STATUS_NO_ERRORS)
	{
		lprintf(LOG_INFO, "RAKP 4 message indicates an error : %s",
			val2str(rsp->payload.rakp4_message.rakp_return_code,
				ipmi_rakp_return_codes));
		return 1;
	}

	else
	{
		/* Validate the authcode */
		if (lanplus_rakp4_hmac_matches(session,
										rsp->payload.rakp4_message.integrity_check_value,
										intf))
		{
			/* Success */
			session->v2_data.session_state = LANPLUS_STATE_ACTIVE;
		}
		else
		{
			/* Error */
			lprintf(LOG_INFO, "> RAKP 4 message has invalid integrity check value");
			return 1;
		}
	}

	intf->abort = 0;
	return 0;
}



/**
 * ipmi_lan_close
 */
void
ipmi_lanplus_close(struct ipmi_intf * intf)
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
ipmi_set_session_privlvl_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t backupBridgePossible;
	uint8_t privlvl = intf->ssn_params.privlvl;

	if (privlvl <= IPMI_SESSION_PRIV_USER)
		return 0;	/* no need to set higher */

	backupBridgePossible = bridgePossible;

	bridgePossible = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		= 0x3b;
	req.msg.data		= &privlvl;
	req.msg.data_len	= 1;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Set Session Privilege Level to %s failed",
			val2str(privlvl, ipmi_privlvl_vals));
		bridgePossible = backupBridgePossible;
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "set_session_privlvl");

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Set Session Privilege Level to %s failed: %s",
			val2str(privlvl, ipmi_privlvl_vals),
			val2str(rsp->ccode, completion_code_vals));
		bridgePossible = backupBridgePossible;
		return -1;
	}

	lprintf(LOG_DEBUG, "Set Session Privilege Level to %s\n",
		val2str(rsp->data[0], ipmi_privlvl_vals));

	bridgePossible = backupBridgePossible;

	return 0;
}

static uint8_t
ipmi_find_best_cipher_suite(struct ipmi_intf *intf)
{
	enum cipher_suite_ids best_suite = IPMI_LANPLUS_CIPHER_SUITE_RESERVED;
#ifdef HAVE_CRYPTO_SHA256
	struct cipher_suite_info suites[MAX_CIPHER_SUITE_COUNT];
	size_t nr_suites = ARRAY_SIZE(suites);

	/* cipher suite best order is chosen with this criteria:
	 * HMAC-MD5 and MD5 are BAD; xRC4 is bad; AES128 is required
	 * HMAC-SHA256 > HMAC-SHA1
	 * secure authentication > encrypted content
	 *
	 * With xRC4 out, all cipher suites with MD5 out, and cipher suite 3
	 * being required by the spec, the only better defined standard cipher
	 * suite is 17. So if SHA256 is available, we should try to use that,
	 * otherwise, fall back to 3.
	 */
	const enum cipher_suite_ids cipher_order_preferred[] = {
		IPMI_LANPLUS_CIPHER_SUITE_17,
		IPMI_LANPLUS_CIPHER_SUITE_3,
	};
	const size_t nr_preferred = ARRAY_SIZE(cipher_order_preferred);
	size_t ipref, i;

	if (ipmi_get_channel_cipher_suites(intf, "ipmi", IPMI_LAN_CHANNEL_E,
	                                   suites, &nr_suites) < 0)
	{
		/* default legacy behavior - fall back to cipher suite 3 */
		return IPMI_LANPLUS_CIPHER_SUITE_3;
	}
	for (ipref = 0;
	     ipref < nr_preferred &&
	     IPMI_LANPLUS_CIPHER_SUITE_RESERVED == best_suite;
	     ipref++)
	{
		for (i = 0; i < nr_suites; i++) {
			if (cipher_order_preferred[ipref]
			    == suites[i].cipher_suite_id)
			{
				best_suite = cipher_order_preferred[ipref];
				break;
			}
		}
	}
#endif /* HAVE_CRYPTO_SHA256 */
	if (IPMI_LANPLUS_CIPHER_SUITE_RESERVED == best_suite) {
		/* IPMI 2.0 spec requires that cipher suite 3 is implemented
		 * so we should always be able to fall back to that if better
		 * options are not available. */
		best_suite = IPMI_LANPLUS_CIPHER_SUITE_3;
	}
	lprintf(LOG_INFO, "Using best available cipher suite %d\n", best_suite);
	return best_suite;
}

/**
 * ipmi_lanplus_open
 */
int
ipmi_lanplus_open(struct ipmi_intf * intf)
{
	int rc;
	int retry;
	struct get_channel_auth_cap_rsp auth_cap;
	struct ipmi_session_params *params;
	struct ipmi_session *session;

	if (!intf)
		return -1;

	if (intf->opened)
		return intf->fd;

	params = &intf->ssn_params;

	if (!params->port)
		params->port = IPMI_LANPLUS_PORT;
	if (!params->privlvl)
		params->privlvl = IPMI_SESSION_PRIV_ADMIN;
	if (!params->timeout)
		params->timeout = IPMI_LAN_TIMEOUT;
	if (!params->retry)
		params->retry = IPMI_LAN_RETRY;

	if (!params->hostname || strlen((const char *)params->hostname) == 0) {
		lprintf(LOG_ERR, "No hostname specified!");
		return -1;
	}

	if (ipmi_intf_socket_connect(intf) == -1) {
		lprintf(LOG_ERR, "Could not open socket!");
		goto fail;
	}

	session = (struct ipmi_session *)malloc(sizeof (struct ipmi_session));
	if (!session) {
		lprintf(LOG_ERR, "ipmitool: malloc failure");
		goto fail;
	}

	intf->session = session;

	/* Setup our lanplus session state */
	memset(session, 0, sizeof(struct ipmi_session));
	session->timeout = params->timeout;
	memcpy(&session->authcode,
	       &params->authcode_set,
	       sizeof(session->authcode));
	session->v2_data.auth_alg         = IPMI_AUTH_RAKP_NONE;
	session->v2_data.crypt_alg        = IPMI_CRYPT_NONE;
	session->sol_data.sequence_number = 1;

	intf->opened = 1;
	intf->abort = 1;

	/*
	 *
	 * Make sure the BMC supports IPMI v2 / RMCP+
	 */
	if (!ipmi_oem_active(intf, "i82571spt") &&
			ipmi_get_auth_capabilities_cmd(intf, &auth_cap)) {
		lprintf(LOG_INFO, "Error issuing Get Channel "
			"Authentication Capabilities request");
		goto fail;
	}

	if (!ipmi_oem_active(intf, "i82571spt") &&
	    !auth_cap.v20_data_available)
	{
		lprintf(LOG_INFO, "This BMC does not support IPMI v2 / RMCP+");
		goto fail;
	}

	/*
	 * If no cipher suite was provided, query the channel cipher suite list
	 * and pick the best one available
	 */
	if (IPMI_LANPLUS_CIPHER_SUITE_RESERVED ==
	    intf->ssn_params.cipher_suite_id)
	{
		ipmi_intf_session_set_cipher_suite_id(
			intf,
			ipmi_find_best_cipher_suite(intf)
		);
	}

	/*
	 * If the open/rakp1/rakp3 sequence encounters a timeout, the whole
	 * sequence needs to restart. The individual messages are not
	 * individually retryable, as the session state is advancing.
	 */
	for (retry = 0; retry < IPMI_LAN_RETRY; retry++) {
		session->v2_data.session_state = LANPLUS_STATE_PRESESSION;
		/*
		 * Open session
		 */
		if ((rc = ipmi_lanplus_open_session(intf)) == 1) {
			goto fail;
		}
		if (rc == 2) {
			lprintf(LOG_DEBUG, "Retry lanplus open session, %d", retry);
			continue;
		}
		/*
		 * RAKP 1
		 */
		if ((rc = ipmi_lanplus_rakp1(intf)) == 1) {
			goto fail;
		}
		if (rc == 2) {
			lprintf(LOG_DEBUG, "Retry lanplus rakp1, %d", retry);
			continue;
		}
		/*
		 * RAKP 3
		 */
		if ((rc = ipmi_lanplus_rakp3(intf)) == 1) {
			goto fail;
		}
		if (rc == 0) break;
		lprintf(LOG_DEBUG,"Retry lanplus rakp3, %d", retry);
	}

	lprintf(LOG_DEBUG, "IPMIv2 / RMCP+ SESSION OPENED SUCCESSFULLY\n");

	intf->abort = 0;

	if (!ipmi_oem_active(intf, "i82571spt")) {
		rc = ipmi_set_session_privlvl_cmd(intf);
		if (rc < 0) {
			goto fail;
		}

		/* automatically detect interface request and response sizes */
		hpm2_detect_max_payload_size(intf);
	}

	bridgePossible = 1;

	if (!ipmi_oem_active(intf, "i82571spt")) {
		intf->manufacturer_id = ipmi_get_oem(intf);
	}

	return intf->fd;

 fail:
	lprintf(LOG_ERR, "Error: Unable to establish IPMI v2 / RMCP+ session");
	intf->close(intf);
	return -1;
}



void test_crypt1(void)
{
	uint8_t key[]  =
		{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
		 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};

	uint16_t  bytes_encrypted;
	uint16_t  bytes_decrypted;
	uint8_t   decrypt_buffer[1000];
	uint8_t   encrypt_buffer[1000];

	uint8_t data[] =
		{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
		 0x11, 0x12};

	printbuf(data, sizeof(data), "original data");

	if (lanplus_encrypt_payload(IPMI_CRYPT_AES_CBC_128,
								key,
								data,
								sizeof(data),
								encrypt_buffer,
								&bytes_encrypted))
	{
		lprintf(LOG_ERR, "Encrypt test failed");
		assert(0);
	}
	printbuf(encrypt_buffer, bytes_encrypted, "encrypted payload");
	

	if (lanplus_decrypt_payload(IPMI_CRYPT_AES_CBC_128,
								key,
								encrypt_buffer,
								bytes_encrypted,
								decrypt_buffer,
								&bytes_decrypted))
	{
		lprintf(LOG_ERR, "Decrypt test failed\n");
		assert(0);
	}	
	printbuf(decrypt_buffer, bytes_decrypted, "decrypted payload");
	
	lprintf(LOG_DEBUG, "\nDone testing the encrypt/decyrpt methods!\n");
	exit(0);
}	



void test_crypt2(void)
{
	uint8_t key[]  =
		{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
		 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
	uint8_t iv[]  =
		  {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
			0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
	uint8_t data[8] = "12345678";

	uint8_t encrypt_buffer[1000];
	uint8_t decrypt_buffer[1000];
	uint32_t bytes_encrypted;
	uint32_t bytes_decrypted;

	printbuf((const uint8_t *)data, strlen((const char *)data), "input data");

	lanplus_encrypt_aes_cbc_128(iv,
								key,
								data,
								strlen((const char *)data),
								encrypt_buffer,
								&bytes_encrypted);
	printbuf((const uint8_t *)encrypt_buffer, bytes_encrypted, "encrypt_buffer");

	lanplus_decrypt_aes_cbc_128(iv,
								key,
								encrypt_buffer,
								bytes_encrypted,
								decrypt_buffer,
								&bytes_decrypted);
	printbuf((const uint8_t *)decrypt_buffer, bytes_decrypted, "decrypt_buffer");

	lprintf(LOG_INFO, "\nDone testing the encrypt/decyrpt methods!\n");
	exit(0);
}


/**
 * send a get device id command to keep session active
 */
static int
ipmi_lanplus_keepalive(struct ipmi_intf * intf)
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
	while (rsp && is_sol_packet(rsp)) {
					 /* rsp was SOL data instead of our answer */
					 /* since it didn't go through the sol recv, do sol recv stuff here */
					 ack_sol_packet(intf, rsp);
					 check_sol_packet_for_new_data(rsp);
					 if (rsp->data_len)
								intf->session->sol_data.sol_input_handler(rsp);
		rsp = ipmi_lan_poll_recv(intf);
		if (!rsp) /* the get device id answer never got back, but retry mechanism was bypassed by SOL data */
			return 0; /* so get device id command never returned, the connection is still alive */
		  }

	if (!rsp || rsp->ccode)
		return -1;

	return 0;
}


/**
 * ipmi_lanplus_setup
 */
static int ipmi_lanplus_setup(struct ipmi_intf * intf)
{
	//test_crypt1();
	assert("ipmi_lanplus_setup");

	if (lanplus_seed_prng(16))
		return -1;

    /* setup default LAN maximum request and response sizes */
    intf->max_request_data_size = IPMI_LAN_MAX_REQUEST_SIZE;
    intf->max_response_data_size = IPMI_LAN_MAX_RESPONSE_SIZE;

	return 0;
}

static void ipmi_lanp_set_max_rq_data_size(struct ipmi_intf * intf, uint16_t size)
{
	if (intf->ssn_params.cipher_suite_id == IPMI_LANPLUS_CIPHER_SUITE_3) {
		/*
		 * encrypted payload can only be multiple of 16 bytes
		 */
		size &= ~15;

		/*
		 * decrement payload size on confidentiality header size
		 * plus minimal confidentiality trailer size
		 */
		size -= (16 + 1);
	}

	intf->max_request_data_size = size;
}

static void ipmi_lanp_set_max_rp_data_size(struct ipmi_intf * intf, uint16_t size)
{
	if (intf->ssn_params.cipher_suite_id == IPMI_LANPLUS_CIPHER_SUITE_3) {
		/*
		 * encrypted payload can only be multiple of 16 bytes
		 */
		size &= ~15;

		/*
		 * decrement payload size on confidentiality header size
		 * plus minimal confidentiality trailer size
		 */
		size -= (16 + 1);
	}

	intf->max_response_data_size = size;
}
