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

#ifndef IPMI_H
#define IPMI_H

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <ipmitool/helper.h>

#define IPMI_BUF_SIZE 1024

/* From table 13.16 of the IPMI v2 specification */
#define IPMI_PAYLOAD_TYPE_IPMI               0x00
#define IPMI_PAYLOAD_TYPE_SOL                0x01
#define IPMI_PAYLOAD_TYPE_RMCP_OPEN_REQUEST  0x10
#define IPMI_PAYLOAD_TYPE_RMCP_OPEN_RESPONSE 0x11
#define IPMI_PAYLOAD_TYPE_RAKP_1             0x12
#define IPMI_PAYLOAD_TYPE_RAKP_2             0x13
#define IPMI_PAYLOAD_TYPE_RAKP_3             0x14
#define IPMI_PAYLOAD_TYPE_RAKP_4             0x15


extern int verbose;
extern int csv_output;

struct ipmi_rq {
	struct {
		unsigned char netfn;
		unsigned char cmd;
		unsigned short data_len;
		unsigned char *data;
	} msg;
};


/*
 * This is what the sendrcv_v2() function would take as an argument. The common case
 * is for payload_type to be IPMI_PAYLOAD_TYPE_IPMI.
 */
struct ipmi_v2_payload {
	unsigned short payload_length;
	unsigned char  payload_type;

	union {

		struct {
			unsigned char    rq_seq;
			struct ipmi_rq * request;
		} ipmi_request;

		struct {
			unsigned char	rs_seq;
			struct ipmi_rs * response;
		} ipmi_response;

		/* Only used internally by the lanplus interface */
		struct {
			unsigned char * request;
		} open_session_request;

		/* Only used internally by the lanplus interface */
		struct {
			unsigned char * message;
		} rakp_1_message;

		/* Only used internally by the lanplus interface */
		struct {
			unsigned char * message;
		} rakp_2_message;

		/* Only used internally by the lanplus interface */
		struct {
			unsigned char * message;
		} rakp_3_message;

		/* Only used internally by the lanplus interface */
		struct {
			unsigned char * message;
		} rakp_4_message;

		struct {
			unsigned char data[IPMI_BUF_SIZE];
			unsigned short character_count;
			unsigned char packet_sequence_number;
			unsigned char acked_packet_number;
			unsigned char accepted_character_count;
			unsigned char is_nack;              /* bool */
			unsigned char assert_ring_wor;      /* bool */
			unsigned char generate_break;       /* bool */
			unsigned char deassert_cts;         /* bool */
			unsigned char deassert_dcd_dsr;     /* bool */
			unsigned char flush_inbound;        /* bool */
			unsigned char flush_outbound;       /* bool */
		} sol_packet;

	} payload;
};



struct ipmi_rq_entry {
	struct ipmi_rq req;
	struct ipmi_intf * intf;
	unsigned char rq_seq;
	unsigned char * msg_data;
	int msg_len;
	struct ipmi_rq_entry * next;
};




struct ipmi_rs {
	unsigned char ccode;
	unsigned char data[IPMI_BUF_SIZE];

	/*
	 * Looks like this is the length of the entire packet, including the RMCP
	 * stuff, then modified to be the length of the extra IPMI message data
	 */
	int data_len;

	struct {
		unsigned char netfn;
		unsigned char cmd;
		unsigned char seq;
		unsigned char lun;
	} msg;

	struct {
		unsigned char  authtype;
		uint32_t       seq;
		uint32_t       id;
		unsigned char  bEncrypted;     /* IPMI v2 only */
		unsigned char  bAuthenticated; /* IPMI v2 only */
		unsigned char  payloadtype;    /* IPMI v2 only */
		/* This is the total length of the payload or
		   IPMI message.  IPMI v2.0 requires this to
		   be 2 bytes.  Not really used for much. */
		unsigned short msglen;
	} session;


	/*
	 * A union of the different possible payload meta-data
	 */
	union {
		struct {
			unsigned char rq_addr;
			unsigned char netfn;
			unsigned char rq_lun;
			unsigned char rs_addr;
			unsigned char rq_seq;
			unsigned char rs_lun;
			unsigned char cmd;
		} ipmi_response;
		struct {
			unsigned char message_tag;
			unsigned char rakp_return_code;
			unsigned char max_priv_level;
			unsigned int  console_id;
			unsigned int  bmc_id;
			unsigned char auth_alg;
			unsigned char integrity_alg;
			unsigned char crypt_alg;
		} open_session_response;
		struct {
			unsigned char message_tag;
			unsigned char rakp_return_code;
			unsigned int  console_id;
			unsigned char bmc_rand[16]; /* Random number generated by the BMC */
			unsigned char bmc_guid[16];
			unsigned char key_exchange_auth_code[20];
		} rakp2_message;
		struct {
			unsigned char message_tag;
			unsigned char rakp_return_code;
			unsigned int  console_id;
			unsigned char integrity_check_value[20];
		} rakp4_message;
		struct {
			unsigned char packet_sequence_number;
			unsigned char acked_packet_number;
			unsigned char accepted_character_count;
			unsigned char is_nack;              /* bool */
			unsigned char transfer_unavailable; /* bool */
			unsigned char sol_inactive;         /* bool */
			unsigned char transmit_overrun;     /* bool */
			unsigned char break_detected;       /* bool */
		} sol_packet;
			
	} payload;
};



#define IPMI_NETFN_CHASSIS		0x0
#define IPMI_NETFN_BRIDGE		0x2
#define IPMI_NETFN_SE			0x4
#define IPMI_NETFN_APP			0x6
#define IPMI_NETFN_FIRMWARE		0x8
#define IPMI_NETFN_STORAGE		0xa
#define IPMI_NETFN_TRANSPORT		0xc
#define IPMI_NETFN_ISOL			0x34

#define IPMI_BMC_SLAVE_ADDR		0x20
#define IPMI_REMOTE_SWID		0x81

extern const struct valstr completion_code_vals[25];

#endif /* IPMI_H */
