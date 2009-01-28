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

#ifndef IPMI_H
#define IPMI_H

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_cc.h>


#define IPMI_BUF_SIZE 1024

#if HAVE_PRAGMA_PACK
#define ATTRIBUTE_PACKING
#else
#define ATTRIBUTE_PACKING __attribute__ ((packed))
#endif


/* From table 13.16 of the IPMI v2 specification */
#define IPMI_PAYLOAD_TYPE_IPMI               0x00
#define IPMI_PAYLOAD_TYPE_SOL                0x01
#define IPMI_PAYLOAD_TYPE_OEM                0x02
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
		uint8_t netfn:6;
		uint8_t lun:2;
		uint8_t cmd;
		uint8_t target_cmd;
		uint16_t data_len;
		uint8_t *data;
	} msg;
};

/*
 * This is what the sendrcv_v2() function would take as an argument. The common case
 * is for payload_type to be IPMI_PAYLOAD_TYPE_IPMI.
 */
struct ipmi_v2_payload {
	uint16_t payload_length;
	uint8_t payload_type;

	union {

		struct {
			uint8_t rq_seq;
			struct ipmi_rq *request;
		} ipmi_request;

		struct {
			uint8_t rs_seq;
			struct ipmi_rs *response;
		} ipmi_response;

		/* Only used internally by the lanplus interface */
		struct {
			uint8_t *request;
		} open_session_request;

		/* Only used internally by the lanplus interface */
		struct {
			uint8_t *message;
		} rakp_1_message;

		/* Only used internally by the lanplus interface */
		struct {
			uint8_t *message;
		} rakp_2_message;

		/* Only used internally by the lanplus interface */
		struct {
			uint8_t *message;
		} rakp_3_message;

		/* Only used internally by the lanplus interface */
		struct {
			uint8_t *message;
		} rakp_4_message;

		struct {
			uint8_t data[IPMI_BUF_SIZE];
			uint16_t character_count;
			uint8_t packet_sequence_number;
			uint8_t acked_packet_number;
			uint8_t accepted_character_count;
			uint8_t is_nack;	/* bool */
			uint8_t assert_ring_wor;	/* bool */
			uint8_t generate_break;	/* bool */
			uint8_t deassert_cts;	/* bool */
			uint8_t deassert_dcd_dsr;	/* bool */
			uint8_t flush_inbound;	/* bool */
			uint8_t flush_outbound;	/* bool */
		} sol_packet;

	} payload;
};

struct ipmi_rq_entry {
	struct ipmi_rq req;
	struct ipmi_intf *intf;
	uint8_t rq_seq;
	uint8_t *msg_data;
	int msg_len;
	int bridging_level;
	struct ipmi_rq_entry *next;
};

struct ipmi_rs {
	uint8_t ccode;
	uint8_t data[IPMI_BUF_SIZE];

	/*
	 * Looks like this is the length of the entire packet, including the RMCP
	 * stuff, then modified to be the length of the extra IPMI message data
	 */
	int data_len;

	struct {
		uint8_t netfn;
		uint8_t cmd;
		uint8_t seq;
		uint8_t lun;
	} msg;

	struct {
		uint8_t authtype;
		uint32_t seq;
		uint32_t id;
		uint8_t bEncrypted;	/* IPMI v2 only */
		uint8_t bAuthenticated;	/* IPMI v2 only */
		uint8_t payloadtype;	/* IPMI v2 only */
		/* This is the total length of the payload or
		   IPMI message.  IPMI v2.0 requires this to
		   be 2 bytes.  Not really used for much. */
		uint16_t msglen;
	} session;

	/*
	 * A union of the different possible payload meta-data
	 */
	union {
		struct {
			uint8_t rq_addr;
			uint8_t netfn;
			uint8_t rq_lun;
			uint8_t rs_addr;
			uint8_t rq_seq;
			uint8_t rs_lun;
			uint8_t cmd;
		} ipmi_response;
		struct {
			uint8_t message_tag;
			uint8_t rakp_return_code;
			uint8_t max_priv_level;
			uint32_t console_id;
			uint32_t bmc_id;
			uint8_t auth_alg;
			uint8_t integrity_alg;
			uint8_t crypt_alg;
		} open_session_response;
		struct {
			uint8_t message_tag;
			uint8_t rakp_return_code;
			uint32_t console_id;
			uint8_t bmc_rand[16];	/* Random number generated by the BMC */
			uint8_t bmc_guid[16];
			uint8_t key_exchange_auth_code[20];
		} rakp2_message;
		struct {
			uint8_t message_tag;
			uint8_t rakp_return_code;
			uint32_t console_id;
			uint8_t integrity_check_value[20];
		} rakp4_message;
		struct {
			uint8_t packet_sequence_number;
			uint8_t acked_packet_number;
			uint8_t accepted_character_count;
			uint8_t is_nack;	/* bool */
			uint8_t transfer_unavailable;	/* bool */
			uint8_t sol_inactive;	/* bool */
			uint8_t transmit_overrun;	/* bool */
			uint8_t break_detected;	/* bool */
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
#define IPMI_NETFN_PICMG		0x2C
#define IPMI_NETFN_ISOL			0x34
#define IPMI_NETFN_TSOL			0x30

#define IPMI_BMC_SLAVE_ADDR		0x20
#define IPMI_REMOTE_SWID		0x81


/* These values are IANA numbers */
typedef enum IPMI_OEM {
     IPMI_OEM_UNKNOWN    = 0,
     IPMI_OEM_HP         = 11,
     IPMI_OEM_SUN        = 42,
     IPMI_OEM_NOKIA      = 94,
     IPMI_OEM_BULL       = 107,
     IPMI_OEM_HITACHI_116 = 116,
     IPMI_OEM_NEC        = 119,
     IPMI_OEM_TOSHIBA    = 186,
     IPMI_OEM_INTEL      = 343,
     IPMI_OEM_TATUNG     = 373,
     IPMI_OEM_HITACHI_399 = 399,
     IPMI_OEM_DELL       = 674,
     IPMI_OEM_LMC        = 2168,
     IPMI_OEM_RADISYS    = 4337,
     IPMI_OEM_MAGNUM     = 5593,
     IPMI_OEM_TYAN       = 6653,
     IPMI_OEM_NEWISYS    = 9237,
     IPMI_OEM_FUJITSU_SIEMENS = 10368,
     IPMI_OEM_AVOCENT    = 10418,
     IPMI_OEM_PEPPERCON  = 10437,
     IPMI_OEM_SUPERMICRO = 10876,
     IPMI_OEM_OSA        = 11102,
     IPMI_OEM_GOOGLE     = 11129,
     IPMI_OEM_PICMG      = 12634,
     IPMI_OEM_RARITAN    = 13742,
     IPMI_OEM_KONTRON    = 15000,
     IPMI_OEM_PPS        = 16394,
     IPMI_OEM_AMI        = 20974,
     IPMI_OEM_NOKIA_SIEMENS_NETWORKS = 28458
} IPMI_OEM;

extern const struct valstr completion_code_vals[];

#endif				/* IPMI_H */
