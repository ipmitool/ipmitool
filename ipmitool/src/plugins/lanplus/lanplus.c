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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>

#include <config.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/bswap.h>
#include <openssl/rand.h>

#include "lanplus.h"
#include "lanplus_crypt.h"
#include "lanplus_crypt_impl.h"
#include "lanplus_dump.h"
#include "rmcp.h"
#include "asf.h"

extern const struct valstr ipmi_rakp_return_codes[];
extern const struct valstr ipmi_priv_levels[];
extern const struct valstr ipmi_auth_algorithms[];
extern const struct valstr ipmi_integrity_algorithms[];
extern const struct valstr ipmi_encryption_algorithms[];

static struct ipmi_rq_entry * ipmi_req_entries;
static struct ipmi_rq_entry * ipmi_req_entries_tail;


static sigjmp_buf jmpbuf;


static int ipmi_lanplus_setup(struct ipmi_intf * intf);
static int ipmi_lanplus_keepalive(struct ipmi_intf * intf);
static int ipmi_lan_send_packet(struct ipmi_intf * intf, unsigned char * data, int data_len);
static struct ipmi_rs * ipmi_lan_recv_packet(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lan_poll_recv(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lanplus_send_ipmi_cmd(struct ipmi_intf * intf, struct ipmi_rq * req);
static struct ipmi_rs * ipmi_lanplus_send_payload(struct ipmi_intf * intf,
												  struct ipmi_v2_payload * payload);
static void getIpmiPayloadWireRep(
								  unsigned char  * out,
								  struct ipmi_rq * req,
								  unsigned char    rq_seq);
static void getSolPayloadWireRep(
								 unsigned char          * msg,
								 struct ipmi_v2_payload * payload);
static void read_open_session_response(struct ipmi_rs * rsp, int offset);
static void read_rakp2_message(struct ipmi_rs * rsp, int offset, unsigned char alg);
static void read_rakp4_message(struct ipmi_rs * rsp, int offset, unsigned char alg);
static void read_session_data(struct ipmi_rs * rsp, int * offset, struct ipmi_session *s);
static void read_session_data_v15(struct ipmi_rs * rsp, int * offset, struct ipmi_session *s);
static void read_session_data_v2x(struct ipmi_rs * rsp, int * offset, struct ipmi_session *s);
static void read_ipmi_response(struct ipmi_rs * rsp, int * offset);
static void read_sol_packet(struct ipmi_rs * rsp, int * offset);
static struct ipmi_rs * ipmi_lanplus_recv_sol(struct ipmi_intf * intf);
static struct ipmi_rs * ipmi_lanplus_send_sol(
											  struct ipmi_intf * intf,
											  struct ipmi_v2_payload * payload);
static int check_sol_packet_for_new_data(
									 struct ipmi_intf * intf,
									 struct ipmi_rs *rsp);
static void ack_sol_packet(
						   struct ipmi_intf * intf,
						   struct ipmi_rs * rsp);


struct ipmi_intf ipmi_lanplus_intf = {
	name:		"lanplus",
	desc:		"IPMI v2.0 RMCP+ LAN Interface",
	setup:		ipmi_lanplus_setup,
	open:		ipmi_lanplus_open,
	close:		ipmi_lanplus_close,
	sendrecv:	ipmi_lanplus_send_ipmi_cmd,
	recv_sol:	ipmi_lanplus_recv_sol,
	send_sol:	ipmi_lanplus_send_sol,
	keepalive:	ipmi_lanplus_keepalive,
	target_addr:	IPMI_BMC_SLAVE_ADDR,
};


extern int verbose;


/*
 * Reverse the order of arbitrarily long strings of bytes
 */
void lanplus_swap(
				  unsigned char * buffer,
                  int             length)
{
	int i;
	unsigned char temp;

	for (i =0; i < length/2; ++i)
	{
		temp = buffer[i];
		buffer[i] = buffer[length - 1 - i];
		buffer[length - 1 - i] = temp;
	}
}



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


int
ipmi_lan_send_packet(
					 struct ipmi_intf * intf,
					 unsigned char * data, int
					 data_len)
{
	if (verbose >= 2)
		printbuf(data, data_len, ">> sending packet");

	return send(intf->fd, data, data_len, 0);
}



struct ipmi_rs *
ipmi_lan_recv_packet(struct ipmi_intf * intf)
{
	static struct ipmi_rs rsp;
	int rc;

	/* setup alarm timeout */
	if (sigsetjmp(jmpbuf, 1) != 0) {
		alarm(0);
		return NULL;
	}

	alarm(intf->session->timeout);
	rc = recv(intf->fd, &rsp.data, IPMI_BUF_SIZE, 0);
	alarm(0);

	/* the first read may return ECONNREFUSED because the rmcp ping
	 * packet--sent to UDP port 623--will be processed by both the
	 * BMC and the OS.
	 *
	 * The problem with this is that the ECONNREFUSED takes
	 * priority over any other received datagram; that means that
	 * the Connection Refused shows up _before_ the response packet,
	 * regardless of the order they were sent out.	(unless the
	 * response is read before the connection refused is returned)
	 */
	if (rc < 0) {
		alarm(intf->session->timeout);
		rc = recv(intf->fd, &rsp.data, IPMI_BUF_SIZE, 0);
		alarm(0);
		if (rc < 0) {
			perror("recv failed");
			return NULL;
		}
	}
	rsp.data[rc] = '\0';
	rsp.data_len = rc;

	if (verbose >= 2)
	{
		printbuf(rsp.data, rsp.data_len, "<< Received data");
	}

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

	if (verbose)
		printf("Received IPMI/RMCP response packet: "
			   "IPMI%s Supported\n",
			   (pong->sup_entities & 0x80) ? "" : " NOT");

	if (verbose > 1)
		printf("  ASF Version %s\n"
			   "  RMCP Version %s\n"
			   "  RMCP Sequence %d\n"
			   "  IANA Enterprise %d\n\n",
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
ipmiv2_lan_ping(struct ipmi_intf * intf)
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

	if (verbose)
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


/**
 *
 * ipmi_lan_poll_recv
 *
 * Receive whatever comes back.  Ignore received packets that don't correspond
 * to a request we've sent.
 *
 * Returns: the ipmi_rs packet describing the/a reponse we expect.
 */
static struct ipmi_rs *
ipmi_lan_poll_recv(struct ipmi_intf * intf)
{
	struct rmcp_hdr rmcp_rsp;
	struct ipmi_rs * rsp;
	struct ipmi_session * session = intf->session;
	
	int offset, rv;
	unsigned short payload_size;

	rsp = ipmi_lan_recv_packet(intf);

	/*
	 * Not positive why we're looping.  Do we sometimes get stuff we don't
	 * expect?
	 */
	while (rsp) {

		/* parse response headers */
		memcpy(&rmcp_rsp, rsp->data, 4);

		if (rmcp_rsp.class == RMCP_CLASS_ASF) {
			/* might be ping response packet */
			rv = ipmi_handle_pong(intf, rsp);
			return (rv <= 0) ? NULL : rsp;
		}
			
		if (rmcp_rsp.class != RMCP_CLASS_IPMI) {
			printf("Invalid RMCP class: %x\n", rmcp_rsp.class);
			rsp = ipmi_lan_recv_packet(intf);
			continue;
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
		 * 2) An RMCP+ message with an IPMI reponse payload
		 * 3) AN RMCP+ open session response
		 * 4) An RAKP-2 message (response to an RAKP 1 message)
		 * 5) An RAKP-4 message (response to an RAKP 3 message)
		 * 6) A Serial Over LAN packet
		 * 7) An Invalid packet (one that doesn't match a request)
		 * -------------------------------------------------------------------
		 */
		read_session_data(rsp, &offset, intf->session);


		if (! lanplus_has_valid_auth_code(rsp, intf->session))
		{
			printf("ERROR: Received message with invalid authcode!\n");
			rsp = ipmi_lan_recv_packet(intf);
			assert(0);
			//continue;
		}

		
		if ((session->v2_data.session_state == LANPLUS_STATE_ACTIVE)    &&
			(rsp->session.authtype == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
			(rsp->session.bEncrypted))

		{
			lanplus_decrypt_payload(session->v2_data.crypt_alg,
									session->v2_data.k2,
									rsp->data + offset,
									rsp->session.msglen,
									rsp->data + offset,
									&payload_size);
		}
		else
			payload_size = rsp->session.msglen;


		/*
		 * Handle IPMI responses (case #1 and #2) -- all IPMI reponses
		 */
		if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_IPMI)
		{
			struct ipmi_rq_entry * entry;
			int payload_start = offset;
			int extra_data_length;
			read_ipmi_response(rsp, &offset);

			if (verbose > 2) {
				printf("<< IPMI Response Session Header\n");
				printf("<<   Authtype                : %s\n",
					   val2str(rsp->session.authtype, ipmi_authtype_vals));
				printf("<<   Payload type            : 0x%x\n",
					   rsp->session.payloadtype);
				printf("<<   Session ID              : 0x%08lx\n",
					   (long)rsp->session.id);
				printf("<<   Sequence                : 0x%08lx\n",
					   (long)rsp->session.seq);
					
				printf("<<   IPMI Msg/Payload Length : %d\n",
					   rsp->session.msglen);
				printf("<< IPMI Response Message Header\n");
				printf("<<   Rq Addr    : %02x\n",
					   rsp->payload.ipmi_response.rq_addr);
				printf("<<   NetFn      : %02x\n",
					   rsp->payload.ipmi_response.netfn);
				printf("<<   Rq LUN     : %01x\n",
					   rsp->payload.ipmi_response.rq_lun);
				printf("<<   Rs Addr    : %02x\n",
					   rsp->payload.ipmi_response.rs_addr);
				printf("<<   Rq Seq     : %02x\n",
					   rsp->payload.ipmi_response.rq_seq);
				printf("<<   Rs Lun     : %01x\n",
					   rsp->payload.ipmi_response.rs_lun);
				printf("<<   Command    : %02x\n",
					   rsp->payload.ipmi_response.cmd);
				printf("<<   Compl Code : 0x%02x\n",
					   rsp->ccode);
			}

			/* Are we expecting this packet? */
			entry = ipmi_req_lookup_entry(rsp->payload.ipmi_response.rq_seq,
										  rsp->payload.ipmi_response.cmd);
			if (entry) {
				if (verbose > 2)
					printf("IPMI Request Match found\n");
				ipmi_req_remove_entry(rsp->payload.ipmi_response.rq_seq,
									  rsp->payload.ipmi_response.cmd);
			} else {
				printf("WARNING: IPMI Request Match NOT FOUND!\n");
				rsp = ipmi_lan_recv_packet(intf);
				continue;
			}			


			/*
			 * Good packet.  Shift response data to start of array.
			 * rsp->data becomes the variable length IPMI response data
			 * rsp->data_len becomes the length of that data
			 */
			extra_data_length = payload_size - (offset - payload_start) - 1;
			if (rsp && extra_data_length)
			{
				rsp->data_len = extra_data_length;
				memmove(rsp->data, rsp->data + offset, extra_data_length);
			}

			break;
		}


		/*
		 * Open Response
		 */
  		else if (rsp->session.payloadtype ==
				 IPMI_PAYLOAD_TYPE_RMCP_OPEN_RESPONSE)
		{
			if (session->v2_data.session_state !=
				LANPLUS_STATE_OPEN_SESSION_SENT)
			{
				printf("Error: Received an Unexpected Open Session "
					   "Response\n");
				rsp = ipmi_lan_recv_packet(intf);
				continue;
			}

			read_open_session_response(rsp, offset);
			break;
		}
		

		/*
		 * RAKP 2
		 */
 		else if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_RAKP_2)
		{
			if (session->v2_data.session_state != LANPLUS_STATE_RAKP_1_SENT)
			{
				printf("Error: Received an Unexpected RAKP 2 message\n");
				rsp = ipmi_lan_recv_packet(intf);
				continue;
			}

			read_rakp2_message(rsp, offset, session->v2_data.auth_alg);
			break;
		}


		/*
		 * RAKP 4
		 */
	 	else if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_RAKP_4)
		{
			if (session->v2_data.session_state != LANPLUS_STATE_RAKP_3_SENT)
			{
				printf("Error: Received an Unexpected RAKP 4 message\n");
				rsp = ipmi_lan_recv_packet(intf);
				continue;
			}

			read_rakp4_message(rsp, offset, session->v2_data.integrity_alg);
			break;
		}

		
		/*
		 * SOL
		 */
 		else if (rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL)
		{
			int payload_start = offset;
			int extra_data_length;

			if (session->v2_data.session_state != LANPLUS_STATE_ACTIVE)
			{
				printf("Error: Received an Unexpected SOL packet\n");
				rsp = ipmi_lan_recv_packet(intf);
				continue;
			}

			read_sol_packet(rsp, &offset);
			extra_data_length = payload_size - (offset - payload_start);
			if (rsp && extra_data_length)
			{
				rsp->data_len = extra_data_length;
				memmove(rsp->data, rsp->data + offset, extra_data_length);
			}
			else
				rsp->data_len = 0;

			break;
		}

		else
		{
			printf("Invalid RMCP+ payload type : 0x%x\n",
				   rsp->session.payloadtype);
			assert(0);
		}
	}

	return rsp;
}



/*
 * read_open_session_reponse
 *
 * Initialize the ipmi_rs from the IMPI 2.x open session response data.
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
	 /*  Message tag */
	 rsp->payload.open_session_response.message_tag = rsp->data[offset];

	 /* RAKP reponse code */
	 rsp->payload.open_session_response.rakp_return_code = rsp->data[offset + 1];

	 /* Maximum privilege level */
	 rsp->payload.open_session_response.max_priv_level = rsp->data[offset + 2];

	 /* Remote console session ID */
	 memcpy(&(rsp->payload.open_session_response.console_id),
			rsp->data + offset + 4,
			4);
	 #if WORDS_BIGENDIAN
	 rsp->payload.open_session_response.console_id =
		 BSWAP_32(rsp->payload.open_session_response.console_id);
	 #endif

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
 * Initialize the ipmi_rs from the IMPI 2.x RAKP 2 message
 *
 * The offset should point the first byte of the the RAKP 2 payload when this
 * function is called.
 *
 * param rsp [in/out] reading from the data variable and writing to the rakp 2
 *       section
 * param offset [in] tells us where hte rakp2 payload starts
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
				   unsigned char auth_alg)
{
	 int i;

	 /*  Message tag */
	 rsp->payload.rakp2_message.message_tag = rsp->data[offset];

	 /* RAKP reponse code */
	 rsp->payload.rakp2_message.rakp_return_code = rsp->data[offset + 1];

	 /* Console session ID */
	 memcpy(&(rsp->payload.rakp2_message.console_id),
			rsp->data + offset + 4,
			4);
	 #if WORDS_BIGENDIAN
	 rsp->payload.rakp2_message.console_id =
		 BSWAP_32(rsp->payload.rakp2_message.console_id);
	 #endif

	 /* BMC random number */
	 memcpy(&(rsp->payload.rakp2_message.bmc_rand),
			rsp->data + offset + 8,
			16);
	 #if WORDS_BIGENDIAN
	 lanplus_swap(rsp->payload.rakp2_message.bmc_rand, 16);
	 #endif

	 /* BMC GUID */
	 memcpy(&(rsp->payload.rakp2_message.bmc_guid),
			rsp->data + offset + 24,
			16);
	 #if WORDS_BIGENDIAN
	 lanplus_swap(rsp->payload.rakp2_message.bmc_guid, 16);
	 #endif
	 
	 /* Key exchange authentication code */
	 switch (auth_alg)
	 {
	 case  IPMI_AUTH_RAKP_NONE:
		 /* Nothing to do here */
		 break;

	 case IPMI_AUTH_RAKP_HMAC_SHA1:
		 /* We need to copy 20 bytes */
		 for (i = 0; i < 20; ++i)
			 rsp->payload.rakp2_message.key_exchange_auth_code[i] =
				 rsp->data[offset + 40 + i];
		 break;

	 case IPMI_AUTH_RAKP_HMAC_MD5:
		 printf("read_rakp2_message: no support for "
				"IPMI_AUTH_RAKP_HMAC_MD5\n");
		 assert(0);
		 break;
	 }
}



/*
 * read_rakp4_message
 *
 * Initialize the ipmi_rs from the IMPI 2.x RAKP 4 message
 *
 * The offset should point the first byte of the the RAKP 4 payload when this
 * function is called.
 *
 * param rsp [in/out] reading from the data variable and writing to the rakp
 *       4 section
 * param offset [in] tells us where hte rakp4 payload starts
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
				   unsigned char integrity_alg)
{
	 int i;

	 /*  Message tag */
	 rsp->payload.rakp4_message.message_tag = rsp->data[offset];

	 /* RAKP reponse code */
	 rsp->payload.rakp4_message.rakp_return_code = rsp->data[offset + 1];

	 /* Console session ID */
	 memcpy(&(rsp->payload.rakp4_message.console_id),
			rsp->data + offset + 4,
			4);
	 #if WORDS_BIGENDIAN
	 rsp->payload.rakp4_message.console_id =
		 BSWAP_32(rsp->payload.rakp4_message.console_id);
	 #endif

	 
	 /* Integrity check value */
	 switch (integrity_alg)
	 {
	 case  IPMI_INTEGRITY_NONE:
		 /* Nothing to do here */
		 break;

	 case IPMI_INTEGRITY_HMAC_SHA1_96:
		 /* We need to copy 12 bytes */
		 for (i = 0; i < 12; ++i)
			 rsp->payload.rakp4_message.integrity_check_value[i] =
				 rsp->data[offset + 8 + i];
		 break;

	 case IPMI_INTEGRITY_HMAC_MD5_128:
	 case IPMI_INTEGRITY_MD5_128:
		 printf("read_rakp4_message: no support for integrity algorithm "
				"0x%x\n", integrity_alg);
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
 * param session holds our session state
 */
void
read_session_data(
				  struct ipmi_rs * rsp,
				  int * offset,
				  struct ipmi_session * s)
{
	/* We expect to read different stuff depending on the authtype */
	rsp->session.authtype = rsp->data[*offset];

	if (rsp->session.authtype == IPMI_SESSION_AUTHTYPE_RMCP_PLUS)
		read_session_data_v2x(rsp, offset, s);
	else
		read_session_data_v15(rsp, offset, s);
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
					  int                 * offset,
					  struct ipmi_session * s)
{
	rsp->session.authtype = rsp->data[(*offset)++];

	rsp->session.bEncrypted     = (rsp->data[*offset] & 0x80 ? 1 : 0);
	rsp->session.bAuthenticated = (rsp->data[*offset] & 0x40 ? 1 : 0);


	/* Payload type */
	rsp->session.payloadtype = rsp->data[(*offset)++] & 0x3F;

	/* Session ID */
	memcpy(&rsp->session.id, rsp->data + *offset, 4);
	*offset += 4;
	#if WORDS_BIGENDIAN
	rsp->session.id = BSWAP_32(rsp->session.id);
	#endif


	/*
	 * Verify that the session ID is what we think it should be
	 */
	if ((s->v2_data.session_state == LANPLUS_STATE_ACTIVE) &&
		(rsp->session.id != s->v2_data.console_id))
	{
		printf("packet session id 0x%x does not match active session 0x%0x\n",
			   rsp->session.id, s->v2_data.console_id);
		assert(0);
	}


	/* Ignored, so far */
	memcpy(&rsp->session.seq, rsp->data + *offset, 4);
	*offset += 4;
	#if WORDS_BIGENDIAN
	rsp->session.seq = BSWAP_32(rsp->session.seq);
	#endif		

	memcpy(&rsp->session.msglen, rsp->data + *offset, 2);
	*offset += 2;
	#if WORDS_BIGENDIAN
	rsp->session.msglen = BSWAP_16(rsp->session.msglen);
	#endif
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
						   int * offset,
						   struct ipmi_session * s)
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
 * Initialize the impi_rs from with the IPMI response specific data
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
 * Initialize the impi_rs with the SOL response data
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

	if (verbose)
	{
		printf("SOL sequence number     : 0x%02x\n",
			   rsp->payload.sol_packet.packet_sequence_number);

		printf("SOL acked packet        : 0x%02x\n",
			   rsp->payload.sol_packet.acked_packet_number);

		printf("SOL accepted char count : 0x%02x\n",
			   rsp->payload.sol_packet.accepted_character_count);

		printf("SOL is nack             : %s\n",
			   rsp->payload.sol_packet.is_nack? "true" : "false");

		printf("SOL xfer unavailable    : %s\n",
			   rsp->payload.sol_packet.transfer_unavailable? "true" : "false");
		
		printf("SOL inactive            : %s\n",
			   rsp->payload.sol_packet.sol_inactive? "true" : "false");

		printf("SOL transmit overrun    : %s\n",
			   rsp->payload.sol_packet.transmit_overrun? "true" : "false");

		printf("SOL break detected      : %s\n",
			   rsp->payload.sol_packet.break_detected? "true" : "false");
	}
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
						   unsigned char  * msg,
						   struct ipmi_rq * req,
						   unsigned char    rq_seq)
{
	int cs, mp, tmp, i;

	i = 0;

	/* IPMI Message Header -- Figure 13-4 of the IPMI v2.0 spec */
	cs = mp = i,

	/* rsAddr */
	msg[i++] = IPMI_BMC_SLAVE_ADDR; 

	/* net Fn */
	msg[i++] = req->msg.netfn << 2;
	tmp = i - cs;

	/* checkSum */
	msg[i++] = ipmi_csum(msg+cs, tmp);
	cs = i;

	/* rqAddr */
	msg[i++] = IPMI_REMOTE_SWID;

	/* rqSeq / rqLUN */
	msg[i++] = rq_seq << 2;

	/* cmd */
	msg[i++] = req->msg.cmd;

	/* message data */
	if (req->msg.data_len) {
		memcpy(msg + i,
			   req->msg.data,
			   req->msg.data_len);
		i += req->msg.data_len;
	}

	/* second checksum */
	tmp = i - cs;
	msg[i++] = ipmi_csum(msg+cs, tmp);
}



/*
 * getSolPayloadWireRep
 *
 * param msg [out] will contain our wire representation
 * param payload [in] holds the v2 payload with our SOL data
 */
void getSolPayloadWireRep(
						  unsigned char          * msg,     /* output */
						  struct ipmi_v2_payload * payload) /* input */
{
	int i = 0;

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
						   unsigned char         ** msg_data) /* out */
{
	unsigned int session_trailer_length = 0;
	struct ipmi_session * session = intf->session;
	struct rmcp_hdr rmcp = {
		.ver		= RMCP_VERSION_1,
		.class		= RMCP_CLASS_IPMI,
		.seq		= 0xff,
	};

	/* msg will hold the entire message to be sent */
	unsigned char * msg;

	int len = 0;


	len =
		sizeof(rmcp)                +  // RMCP Header (4)
		10                          +  // IPMI Session Header
		2                           +  // Message length
		IPMI_MAX_PAYLOAD_SIZE       +  // The actual payload
		IPMI_MAX_INTEGRITY_PAD_SIZE +  // Integrity Pad
		1                           +  // Pad Length
		1                           +  // Next Header
		IPMI_MAX_AUTH_CODE_SIZE;       // Authcode


	msg = malloc(len);
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
	msg[IMPI_LANPLUS_OFFSET_AUTHTYPE] = 0x06;

	/* Payload Type -- also specifies whether were authenticated/encyrpted */
	msg[IMPI_LANPLUS_OFFSET_PAYLOAD_TYPE] = payload->payload_type;

	
	if (session->v2_data.session_state == LANPLUS_STATE_ACTIVE)
	{
		msg[IMPI_LANPLUS_OFFSET_PAYLOAD_TYPE] |=
			((session->v2_data.crypt_alg != IPMI_CRYPT_NONE	)? 0x80 : 0x00);
		msg[IMPI_LANPLUS_OFFSET_PAYLOAD_TYPE] |=
			((session->v2_data.auth_alg  != IPMI_AUTH_RAKP_NONE)? 0x40 : 0x00);
	}

	/* Session ID  -- making it LSB */
	msg[IMPI_LANPLUS_OFFSET_SESSION_ID    ] = session->v2_data.bmc_id         & 0xff;
	msg[IMPI_LANPLUS_OFFSET_SESSION_ID + 1] = (session->v2_data.bmc_id >> 8)  & 0xff;
	msg[IMPI_LANPLUS_OFFSET_SESSION_ID + 2] = (session->v2_data.bmc_id >> 16) & 0xff;
	msg[IMPI_LANPLUS_OFFSET_SESSION_ID + 3] = (session->v2_data.bmc_id >> 24) & 0xff;


	/* Sequence Number -- making it LSB */
	msg[IMPI_LANPLUS_OFFSET_SEQUENCE_NUM    ] = session->out_seq         & 0xff;
	msg[IMPI_LANPLUS_OFFSET_SEQUENCE_NUM + 1] = (session->out_seq >> 8)  & 0xff;
	msg[IMPI_LANPLUS_OFFSET_SEQUENCE_NUM + 2] = (session->out_seq >> 16) & 0xff;
	msg[IMPI_LANPLUS_OFFSET_SEQUENCE_NUM + 3] = (session->out_seq >> 24) & 0xff;

	
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
		getIpmiPayloadWireRep(msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
							  payload->payload.ipmi_request.request,
							  payload->payload.ipmi_request.rq_seq);
		break;

	case IPMI_PAYLOAD_TYPE_SOL: 
		getSolPayloadWireRep(msg + IPMI_LANPLUS_OFFSET_PAYLOAD,
							 payload);
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
		printf("unsupported payload type 0x%x\n", payload->payload_type);
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
	msg[IMPI_LANPLUS_OFFSET_PAYLOAD_SIZE    ] =
		payload->payload_length        & 0xff;
	msg[IMPI_LANPLUS_OFFSET_PAYLOAD_SIZE + 1] =
		(payload->payload_length >> 8) & 0xff;

	

	/*
	 *------------------------------------------
	 * SESSION TRAILER
	 *------------------------------------------
	 */
	if ((session->v2_data.session_state == LANPLUS_STATE_ACTIVE) &&
		(session->v2_data.auth_alg      != IPMI_AUTH_RAKP_NONE))
	{
		unsigned int i, hmac_length, integrity_pad_size = 0, hmac_input_size;
		unsigned char * hmac_output;
		unsigned int start_of_session_trailer =
			IPMI_LANPLUS_OFFSET_PAYLOAD +
			payload->payload_length;


		/*
		 * Determine the required integrity pad length.  We have to make the
		 * data range covered by the authcode a multiple of 4.
		 */
		unsigned int length_before_authcode =
			12                          + /* the stuff before the payload */
			payload->payload_length     +
			1                           + /* pad length field  */
			1;                            /* next header field */

		
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
			printbuf(msg + IMPI_LANPLUS_OFFSET_AUTHTYPE, hmac_input_size, "authcode input");


		/* Auth Code */
		lanplus_HMAC(session->v2_data.auth_alg,
					 session->v2_data.k1,                /* key        */
					 20,                                 /* key length */
					 msg + IMPI_LANPLUS_OFFSET_AUTHTYPE, /* hmac input */
					 hmac_input_size,
					 hmac_output,
					 &hmac_length);

		assert(hmac_length == 20);

		if (verbose > 2)
			printbuf(hmac_output, 12, "authcode output");

		/* Set session_trailer_length appropriately */
		session_trailer_length =
			integrity_pad_size +
			2                  + /* pad length + next header */
			12;                  /* Size of the authcode (we only use the first 12 bytes) */
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
								struct ipmi_rq * req)
{
	struct ipmi_v2_payload v2_payload;
	struct ipmi_rq_entry * entry;

	/*
	 * We have a problem.  we need to know the sequence number here,
	 * because we use it in our stored entry.  But we also need to
	 * know the sequence number when we generate our IPMI
	 * representation far below.
	 */
 	static unsigned char curr_seq = 0;
	if (curr_seq >= 64)
		curr_seq = 0;
	
	entry = malloc(sizeof(struct ipmi_rq_entry));
	memset(entry, 0, sizeof(struct ipmi_rq_entry));
	memcpy(&entry->req, req, sizeof(struct ipmi_rq));

	entry->intf = intf;
	entry->rq_seq = curr_seq;

	// Factor out
	if (!ipmi_req_entries)
		ipmi_req_entries = entry;
	else
		ipmi_req_entries_tail->next = entry;
	ipmi_req_entries_tail = entry;


	if (verbose > 3)
		printf("added list entry seq=0x%02x cmd=0x%02x\n",
			   curr_seq, req->msg.cmd);


	// Build our payload
	v2_payload.payload_type                 = IPMI_PAYLOAD_TYPE_IPMI;
	v2_payload.payload_length               = req->msg.data_len + 7;
	v2_payload.payload.ipmi_request.request = req;
	v2_payload.payload.ipmi_request.rq_seq  = curr_seq;
	
	ipmi_lanplus_build_v2x_msg(intf,                // in 
							   &v2_payload,         // in 
							   &(entry->msg_len),   // out
							   &(entry->msg_data)); // out

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
	unsigned char * msg;
	int cs, mp, len = 0, tmp;
	struct ipmi_session  * session = intf->session;
	struct ipmi_rq_entry * entry;

	entry = malloc(sizeof(struct ipmi_rq_entry));
	memset(entry, 0, sizeof(struct ipmi_rq_entry));
	memcpy(&entry->req, req, sizeof(struct ipmi_rq));

	entry->intf = intf;

	// Can be factored out
	if (!ipmi_req_entries)
		ipmi_req_entries = entry;
	else
		ipmi_req_entries_tail->next = entry;

	ipmi_req_entries_tail = entry;

	len = req->msg.data_len + 21;

	msg = malloc(len);
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

	if (verbose > 2) {
		printf(">> IPMI Request Session Header\n");
		printf(">>   Authtype   : %s\n", val2str(IPMI_SESSION_AUTHTYPE_NONE,
												 ipmi_authtype_vals));
		printf(">>   Sequence   : 0x%08lx\n", (long)session->out_seq);
		printf(">>   Session ID : 0x%08lx\n", (long)0);

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
	unsigned char       * msg_data;
	int                   msg_length;
	struct ipmi_session * session = intf->session;
	int                   try = 0;

	if (!intf->opened && intf->open && intf->open(intf) < 0)
		return NULL;

	if (payload->payload_type == IPMI_PAYLOAD_TYPE_IPMI)
	{
		/*
		 * Build an IPMI v1.5 or v2 command
		 */
		struct ipmi_rq_entry * entry;
		struct ipmi_rq * ipmi_request = payload->payload.ipmi_request.request;

		if (verbose >= 1)
		{
			unsigned short i;

			printf("\n");
			printf(">> Sending IPMI command payload\n");
			printf(">>    netfn   : 0x%02x\n", ipmi_request->msg.netfn);
			printf(">>    command : 0x%02x\n", ipmi_request->msg.cmd);
			printf(">>    data    : ");

			for (i = 0; i < ipmi_request->msg.data_len; ++i)
				printf("0x%02x ", ipmi_request->msg.data[i]);
			printf("\n\n");
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
			if (verbose >= 2)
				printf("BUILDING A v1.5 COMMAND\n");
			entry = ipmi_lanplus_build_v15_ipmi_cmd(intf, ipmi_request);
		}
		else
		{
			if (verbose >= 2)
				printf("BUILDING A v2 COMMAND\n");
			entry = ipmi_lanplus_build_v2x_ipmi_cmd(intf, ipmi_request);
		}

		if (!entry) {
			printf("Aborting send command, unable to build.\n");
			return NULL;
		}

		msg_data   = entry->msg_data;
		msg_length = entry->msg_len;
	}

	else if (payload->payload_type == IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST)
	{
		if (verbose)
			printf(">> SENDING AN OPEN SESSION REQUEST\n\n");
		assert(session->v2_data.session_state == LANPLUS_STATE_PRESESSION);

		ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								   payload,     /* in  */
								   &msg_length, /* out */
								   &msg_data);  /* out */

	}

	else if (payload->payload_type == IPMI_PAYLOAD_TYPE_RAKP_1)
	{
		if (verbose)
			printf(">> SENDING A RAKP 1 MESSAGE \n\n");
		assert(session->v2_data.session_state ==
			   LANPLUS_STATE_OPEN_SESSION_RECEIEVED);

		ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								   payload,     /* in  */
								   &msg_length, /* out */
								   &msg_data);  /* out */

	}

	else if (payload->payload_type == IPMI_PAYLOAD_TYPE_RAKP_3)
	{
		if (verbose)
			printf(">> SENDING A RAKP 3 MESSAGE \n\n");
		assert(session->v2_data.session_state ==
			   LANPLUS_STATE_RAKP_2_RECEIVED);

		ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								   payload,     /* in  */
								   &msg_length, /* out */
								   &msg_data);  /* out */

	}

	else if (payload->payload_type == IPMI_PAYLOAD_TYPE_SOL)
	{
		if (verbose)
			printf(">> SENDING A SOL MESSAGE \n\n");
		assert(session->v2_data.session_state == LANPLUS_STATE_ACTIVE);

		ipmi_lanplus_build_v2x_msg(intf,        /* in  */
								   payload,     /* in  */
								   &msg_length, /* out */
								   &msg_data);  /* out */
	}

	else
	{

		printf("we dont yet support sending other payload types (0x%0x)!\n",
			   payload->payload_type);
		assert(0);
	}


	while (try < IPMI_LAN_RETRY) {


		if (ipmi_lan_send_packet(intf, msg_data, msg_length) < 0) {
			printf("ipmi_lan_send_cmd failed\n");
			return NULL;
		}

		usleep(100); 			/* Not sure what this is for */

		/* Remember our connection state */
		switch (payload->payload_type)
		{
		case IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST:
			session->v2_data.session_state = LANPLUS_STATE_OPEN_SESSION_SENT;
			break;
		case IPMI_PAYLOAD_TYPE_RAKP_1:
			session->v2_data.session_state = LANPLUS_STATE_RAKP_1_SENT;
			break;
		case IPMI_PAYLOAD_TYPE_RAKP_3:
			session->v2_data.session_state = LANPLUS_STATE_RAKP_3_SENT;
			break;
		}


		/*
		 * Special case for SOL outbound packets.
		 *
		 * Non-ACK packets require an ACK from the BMC (that matches
		 * our packet!).
		 *
		 * While waiting for our ACK, it's very possible that we
		 * will receive additional data form the BMC (that we will have
		 * to ACK).
		 *
		 * Also, this is not perfectly correct.  We would like
		 * to give the appropriate timeout for SOL retries, but our
		 * timeout mechanism is in our recv() call.  Thus _any_
		 * incoming packet from the BMC will use up one of our tries,
		 * even if it is not our ACK, and even if it comes in before
		 * our retry timeout.  I will make this code more sophisticated
		 * if I see that this is a problem.
		 */
		if (payload->payload_type == IPMI_PAYLOAD_TYPE_SOL)
		{
			if (! payload->payload.sol_packet.packet_sequence_number)
			{
				/* We're just sending an ACK.  No need to retry. */
				break;
			}


			rsp = ipmi_lanplus_recv_sol(intf); /* Grab the next packet */

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
		}


		/* Non-SOL processing */
		else
		{
			rsp = ipmi_lan_poll_recv(intf);
			if (rsp)
				break;

			usleep(5000);
		}

		try++;
	}

	
	/* IPMI messages are deleted under ipmi_lan_poll_recv() */
	if (payload->payload_type == IPMI_PAYLOAD_TYPE_SOL)
		free(msg_data);

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
	v2_payload->payload.sol_packet.acked_packet_number = 0; /* NA */

	set_sol_packet_sequence_number(intf, v2_payload);
	
	v2_payload->payload.sol_packet.accepted_character_count = 0; /* NA */

	rs = ipmi_lanplus_send_payload(intf, v2_payload);

	/* Determine if we need to resend some of our data */
	chars_to_resend = is_sol_partial_ack(v2_payload, rs);


	while (chars_to_resend)
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

		rs = ipmi_lanplus_send_payload(intf, v2_payload);

		chars_to_resend = is_sol_partial_ack(v2_payload, rs);
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
							  struct ipmi_intf * intf,
							  struct ipmi_rs *rsp)
{
	static unsigned char last_received_sequence_number = 0;
	static unsigned char last_received_byte_count      = 0;
	int new_data_size                                  = 0;


	if (rsp &&
		(rsp->session.authtype    == IPMI_SESSION_AUTHTYPE_RMCP_PLUS) &&
		(rsp->session.payloadtype == IPMI_PAYLOAD_TYPE_SOL))
	{
		/* Store the data length before we mod it */
		unsigned char unaltered_data_len = rsp->data_len;
		
		
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
		 *Rember the data for next round
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

		bzero(&ack, sizeof(struct ipmi_v2_payload));

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
	
	ack_sol_packet(intf, rsp);	              

	/*
	 * Remembers the data sent, and alters the data to just
	 * include the new stuff.
	 */
	check_sol_packet_for_new_data(intf, rsp);

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
 * authentication capabilities with the "request IMPI v2 data bit"
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
	unsigned char msg_data[2];

	msg_data[0] = IPMI_LAN_CHANNEL_E | 0x80; // Ask for IPMI v2 data as well
	msg_data[1] = intf->session->privlvl;

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

		if (!rsp || rsp->ccode) {
			if (rsp && verbose)
				printf("Get Auth Capabilities error: %02x\n", rsp->ccode);	
			return 1;
		}
	}


	memcpy(auth_cap,
		   rsp->data,
		   sizeof(struct get_channel_auth_cap_rsp));


	return 0;
}



static int
impi_close_session_cmd(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char msg_data[4];
	uint32_t bmc_session_lsbf;

	if (intf->session->v2_data.session_state != LANPLUS_STATE_ACTIVE)
		return -1;


	bmc_session_lsbf = intf->session->v2_data.bmc_id;
	#if WORDS_BIGENDIAN
	bmc_session_lsbf = BSWAP_32(bmc_session_lsbf);
	#endif
	
	memcpy(&msg_data, &bmc_session_lsbf, 4);

	memset(&req, 0, sizeof(req));
	req.msg.netfn		= IPMI_NETFN_APP;
	req.msg.cmd		    = 0x3c;
	req.msg.data		= msg_data;
	req.msg.data_len	= 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		/* Looks like the session was closed */
		return -1;
	}
	if (verbose > 2)
		printbuf(rsp->data, rsp->data_len, "close_session");

	if (rsp->ccode == 0x87) {
		printf("Failed to Close Session: invalid session ID %08lx\n",
			   (long)intf->session->v2_data.bmc_id);
		return -1;
	}
	if (rsp->ccode) {
		printf("Failed to Close Session: %x\n", rsp->ccode);
		return -1;
	}

	if (verbose > 1)
		printf("\nClosed Session %08lx\n\n", (long)intf->session->v2_data.bmc_id);

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
	unsigned char * msg;
	struct ipmi_rs * rsp;
	int rc = 0;


	/*
	 * Build an Open Session Request Payload
	 */
	msg = (unsigned char*)malloc(IPMI_OPEN_SESSION_REQUEST_SIZE);
	memset(msg, 0, IPMI_OPEN_SESSION_REQUEST_SIZE);

	msg[0] = 0; /* Message tag */
	msg[1] = 0; /* Give us highest privlg level based on supported algorithms */
	msg[2] = 0; /* reserved */
	msg[3] = 0; /* reserved */

	/* Choose our session ID for easy recognition in the packet dump */
	session->v2_data.console_id = 0xA0A2A3A4;
	msg[4] = session->v2_data.console_id & 0xff;
	msg[5] = (session->v2_data.console_id >> 8)  & 0xff;
	msg[6] = (session->v2_data.console_id >> 16) & 0xff;
	msg[7] = (session->v2_data.console_id >> 24) & 0xff;

	/*
	 * Authentication payload
	 */
	msg[8]  = 0; /* specifies authentication payload */
	msg[9]  = 0; /* reserved */
	msg[10] = 0; /* reserved */
	msg[11] = 8; /* payload length */
	msg[12] = IPMI_AUTH_RAKP_HMAC_SHA1; 
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
	msg[20] = IPMI_INTEGRITY_HMAC_SHA1_96; 
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
	msg[28] = IPMI_CRYPT_AES_CBC_128; 
	msg[29] = 0; /* reserved */	
	msg[30] = 0; /* reserved */
	msg[31] = 0; /* reserved */


	v2_payload.payload_type   = IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST;
	v2_payload.payload_length = IPMI_OPEN_SESSION_REQUEST_SIZE;
	v2_payload.payload.open_session_request.request = msg;

	rsp = ipmi_lanplus_send_payload(intf, &v2_payload);


	if (verbose)
		lanplus_dump_open_session_response(rsp);


	if (rsp->payload.open_session_response.rakp_return_code !=
		IPMI_RAKP_STATUS_NO_ERRORS)
	{
		if (verbose)
			printf("Error in open session response message : %s\n",
				   val2str(rsp->payload.open_session_response.rakp_return_code,
						   ipmi_rakp_return_codes));
		return rc = 1;
	}
	else
	{
		if (rsp->payload.open_session_response.console_id !=
			session->v2_data.console_id)
			printf("Warning: Console session ID is not what we requested\n");

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
	}

	return rc;
}



/*
 * ipmi_lanplus_rakp1
 *
 * Build and send the RAKP 1 message as part of the IMPI v2 / RMCP+ session
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
	unsigned char * msg;
	struct ipmi_rs * rsp;
	int rc = 0;

	/*
	 * Build a RAKP 1 message
	 */
	msg = (unsigned char*)malloc(IPMI_RAKP1_MESSAGE_SIZE);
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
		printf("ERROR generating random number in ipmi_lanplus_rakp1\n");
		return 1;
	}
	memcpy(msg + 8, session->v2_data.console_rand, 16);
	#if WORDS_BIGENDIAN
	lanplus_swap(msg + 8, 16);
	#endif

	if (verbose > 1)
		printbuf(session->v2_data.console_rand, 16,
				 ">> Console generated random number");


	/*
	 * Requested maximum privilege level.
	 */
	msg[24]  = 0x10; /* We will specify a name-only lookup */
	msg[24] |= session->privlvl;
	session->v2_data.requested_role = msg[24];
	
	msg[25] = 0; /* reserved */
	msg[26] = 0; /* reserved */


	/* Username specification */
	msg[27] = strlen(session->username);
	if (msg[27] > IPMI_MAX_USER_NAME_LENGTH)
	{
		printf("ERROR: user name too long.  (Exceeds %d characters)\n",
			   IPMI_MAX_USER_NAME_LENGTH);
		return 1;
	}
	memcpy(msg + 28, session->username, msg[27]);


	v2_payload.payload_type                   = IPMI_PAYLOAD_TYPE_RAKP_1;
	v2_payload.payload_length                 = IPMI_RAKP1_MESSAGE_SIZE;
	v2_payload.payload.rakp_1_message.message = msg;

	rsp = ipmi_lanplus_send_payload(intf, &v2_payload);

	if (! rsp)
	{
		if (verbose)
			printf("> Error: no response from RAKP 1 message\n");
		return 1;
	}

	session->v2_data.session_state = LANPLUS_STATE_RAKP_2_RECEIVED;
	
	if (verbose)
		lanplus_dump_rakp2_message(rsp, session->v2_data.auth_alg);



	if (rsp->payload.rakp2_message.rakp_return_code != IPMI_RAKP_STATUS_NO_ERRORS)
	{
		if (verbose)
			printf("RAKP 2 message indicates an error : %s\n",
				   val2str(rsp->payload.rakp2_message.rakp_return_code,
						   ipmi_rakp_return_codes));
		rc = 1;
	}

	else
	{
		memcpy(session->v2_data.bmc_rand, rsp->payload.rakp2_message.bmc_rand, 16);
		memcpy(session->v2_data.bmc_guid, rsp->payload.rakp2_message.bmc_guid, 16);

		/*
		 * It is at this point that we have to decode the random number and determine
		 * whether the BMC has authenticated.
		 */
		if (! lanplus_rakp2_hmac_matches(session,
										 rsp->payload.rakp2_message.key_exchange_auth_code))
		{
			/* Error */
			if (verbose)
				printf("> RAKP 2 HMAC is invalid\n");
			session->v2_data.rakp2_return_code = IPMI_RAKP_STATUS_INVALID_INTEGRITY_CHECK_VALUE;
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
 * Build and send the RAKP 3 message as part of the IMPI v2 / RMCP+ session
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
	unsigned char * msg;
	struct ipmi_rs * rsp;

	assert(session->v2_data.session_state == LANPLUS_STATE_RAKP_2_RECEIVED);
	
	/*
	 * Build a RAKP 3 message
	 */
	msg = (unsigned char*)malloc(IPMI_RAKP3_MESSAGE_MAX_SIZE);
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
	v2_payload.payload.rakp_1_message.message = msg;

	/*
	 * If the rakp2 return code indicates and error, we don't have to
	 * generate an authcode or session integrity key.  In that case, we
	 * are simply sending a RAKP 3 message to indicate to the BMC that the
	 * RAKP 2 message caused an error.
	 */
	if (session->v2_data.rakp2_return_code == IPMI_RAKP_STATUS_NO_ERRORS)
	{
		int auth_length;
		
		if (lanplus_generate_rakp3_authcode(msg + 8, session, &auth_length))
		{
			/* Error */
			if (verbose)
				printf("> Error generating RAKP 3 authcode\n");

			return 1;
		}
		else
		{
			/* Success */
			v2_payload.payload_length += auth_length;
		}

		/* Generate our Session Integrity Key, K1, and K2 */
		if (lanplus_generate_sik(session))
		{
			/* Error */
			if (verbose)
				printf("> Error generating session integrity key\n");
			return 1;
		}
		else if (lanplus_generate_k1(session))
		{
			/* Error */
			if (verbose)
				printf("> Error generating K1 key\n");
			return 1;
		}
		else if (lanplus_generate_k2(session))
		{
			/* Error */
			if (verbose)
				printf("> Error generating K1 key\n");
			return 1;
		}
	}
	

	rsp = ipmi_lanplus_send_payload(intf, &v2_payload);

	if (session->v2_data.rakp2_return_code != IPMI_RAKP_STATUS_NO_ERRORS)
	{
		/*
		 * If the previous RAKP 2 message received was deemed erroneous,
		 * we have nothing else to do here.  We only sent the RAKP 3 message
		 * to indicate to the BMC that the RAKP 2 message failed.
		 */
		return 1;
	}
	else if (! rsp)
	{
		if (verbose)
			printf("> Error: no response from RAKP 3 message\n");
		return 1;
	}


	/*
	 * We have a RAKP 4 message to chew on.
	 */
	if (verbose)
		lanplus_dump_rakp4_message(rsp, session->v2_data.auth_alg);
	

	if (rsp->payload.open_session_response.rakp_return_code != IPMI_RAKP_STATUS_NO_ERRORS)
	{
		if (verbose)
			printf("RAKP 4 message indicates an error : %s\n",
				   val2str(rsp->payload.rakp4_message.rakp_return_code,
						   ipmi_rakp_return_codes));
		return 1;
	}

	else
	{
		/* Validate the authcode */
		if (lanplus_rakp4_hmac_matches(session,
									   rsp->payload.rakp4_message.integrity_check_value))
		{
			/* Success */
			session->v2_data.session_state = LANPLUS_STATE_ACTIVE;
		}
		else
		{
			/* Error */
			if (verbose)
				printf("> RAKP 4 message has invalid integrity check value\n");
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
	if (!intf->abort)
		impi_close_session_cmd(intf);
	
	if (intf->fd >= 0)
		close(intf->fd);
	
	ipmi_req_clear_entries();
	
	if (intf->session)
		free(intf->session);
	
	intf->session = NULL;
	intf->opened = 0;
	intf = NULL;
}



/**
 * ipmi_lanplus_open
 */
int
ipmi_lanplus_open(struct ipmi_intf * intf)
{
	int rc;
	struct sigaction act;
	struct get_channel_auth_cap_rsp auth_cap;
	struct sockaddr_in addr;
	struct ipmi_session *session;

	if (!intf || !intf->session)
		return -1;
	session = intf->session;


	if (!session->port)
		session->port = IPMI_LANPLUS_PORT;
	if (!session->privlvl)
		session->privlvl = IPMI_SESSION_PRIV_USER;

	if (!session->hostname) {
		printf("No hostname specified!\n");
		return -1;
	}

	intf->abort = 1;


	/* Setup our lanplus session state */
	session->v2_data.session_state    = LANPLUS_STATE_PRESESSION;
	session->v2_data.auth_alg         = IPMI_AUTH_RAKP_NONE;
	session->v2_data.crypt_alg        = IPMI_CRYPT_NONE;
	session->v2_data.console_id       = 0x00;
	session->v2_data.bmc_id           = 0x00;
	session->sol_data.sequence_number = 1;
	//session->sol_data.last_received_sequence_number = 0;
	//session->sol_data.last_received_byte_count      = 0;
	memset(session->v2_data.sik, 0, IPMI_SIK_BUFFER_SIZE);
	memset(session->v2_data.kg,  0, IPMI_KG_BUFFER_SIZE);
	session->timeout = IPMI_LAN_TIMEOUT;


	/* open port to BMC */
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(session->port);

	rc = inet_pton(AF_INET, session->hostname, &addr.sin_addr);
	if (rc <= 0) {
		struct hostent *host = gethostbyname(session->hostname);
		if (!host) {
			printf("address lookup failed\n");
			return -1;
		}
		addr.sin_family = host->h_addrtype;
		memcpy(&addr.sin_addr, host->h_addr, host->h_length);
	}

	if (verbose > 1)
		printf("IPMI LAN host %s port %d\n",
			   session->hostname, ntohs(addr.sin_port));

	intf->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (intf->fd < 0) {
		perror("socket failed");
		return -1;
	}


	/* connect to UDP socket so we get async errors */
	rc = connect(intf->fd,
				 (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
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

	intf->opened = 1;

	/*
	 *
	 * Make sure the BMC supports IPMI v2 / RMCP+
	 *
	 * I'm not sure why we accept a failure for the first call
	 */
	if (ipmi_get_auth_capabilities_cmd(intf, &auth_cap)) {
		sleep(1);
		if (ipmi_get_auth_capabilities_cmd(intf, &auth_cap));
		{
			if (verbose)
				printf("Error issuing Get Channel Authentication "
					   "Capabilies request\n");
			goto fail;
		}
	}

	if (! auth_cap.v20_data_available)
	{
		if (verbose)
			printf("This BMC does not support IPMI v2 / RMCP+\n");
		goto fail;
	}


	/*
	 * Open session
	 */
	if (ipmi_lanplus_open_session(intf)){
		intf->close(intf);
		goto fail;
	}

	/*
	 * RAKP 1
	 */
	if (ipmi_lanplus_rakp1(intf)){
		intf->close(intf);
		goto fail;
	}

	/*
	 * RAKP 3
	 */
	if (ipmi_lanplus_rakp3(intf)){
		intf->close(intf);
		goto fail;
	}


	if (verbose)
		printf("IPMIv2 / RMCP+ SESSION OPENED SUCCESSFULLY\n\n");

	return intf->fd;

 fail:
	printf("Error: Unable to establish IPMI v2 / RMCP+ session\n");
	intf->opened = 0;
	return -1;
}



void test_crypt1()
{
	unsigned char key[]  =
		{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
		 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};

	unsigned short  bytes_encrypted;
	unsigned short  bytes_decrypted;
	unsigned char   decrypt_buffer[1000];
	unsigned char   encrypt_buffer[1000];

	unsigned char data[] =
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
		printf("test encrypt failed\n");
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
		printf("test decrypt failed\n");
		assert(0);
	}	
	printbuf(decrypt_buffer, bytes_decrypted, "decrypted payload");
	
	printf("\nDone testing the encrypt/decyrpt methods!\n\n");
	exit(0);
}	



void test_crypt2()
{
	unsigned char key[]  =
		{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
		 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
	unsigned char iv[]  =
        {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
         0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14};
	unsigned char * data = "12345678";

	char encrypt_buffer[1000];
    char decrypt_buffer[1000];
    int bytes_encrypted;
    int bytes_decrypted;

	printbuf(data, strlen(data), "input data");

	lanplus_encrypt_aes_cbc_128(iv,
								key,
								data,
								strlen(data),
								encrypt_buffer,
								&bytes_encrypted);
	printbuf(encrypt_buffer, bytes_encrypted, "encrypt_buffer");

	lanplus_decrypt_aes_cbc_128(iv,
								key,
								encrypt_buffer,
								bytes_encrypted,
								decrypt_buffer,
								&bytes_decrypted);
	printbuf(decrypt_buffer, bytes_decrypted, "decrypt_buffer");

	printf("\nDone testing the encrypt/decyrpt methods!\n\n");
	exit(0);
}


/**
 * send a get device id command to keep session active
 */
static int
ipmi_lanplus_keepalive(struct ipmi_intf * intf)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req = { msg: {
		netfn: IPMI_NETFN_APP,
		cmd: 1,
	}};

	if (!intf->opened)
		return 0;

	rsp = intf->sendrecv(intf, &req);
	return (!rsp || rsp->ccode) ? -1 : 0;
}


/**
 * ipmi_lanplus_setup
 */
static int ipmi_lanplus_setup(struct ipmi_intf * intf)
{
	//test_crypt1();
	assert("lanplus_intf_setup");

	if (lanplus_seed_prng(16))
		return -1;

	intf->session = malloc(sizeof(struct ipmi_session));
	memset(intf->session, 0, sizeof(struct ipmi_session));
	return (intf->session) ? 0 : -1;
}
