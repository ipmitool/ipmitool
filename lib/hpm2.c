/*
 * Copyright (c) 2012 Pigeon Point Systems.  All Rights Reserved.
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
 * Neither the name of Pigeon Point Systems nor the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * PIGEON POINT SYSTEMS ("PPS") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * PPS OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF PPS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <ipmitool/bswap.h>
#include <ipmitool/hpm2.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>

/* From src/plugins/ipmi_intf.c: */
void
ipmi_intf_set_max_request_data_size(struct ipmi_intf * intf, uint16_t size);
void
ipmi_intf_set_max_response_data_size(struct ipmi_intf * intf, uint16_t size);

#if HAVE_PRAGMA_PACK
# pragma pack(push, 1)
#endif

/* HPM.x Get Capabilities request */
struct hpmx_cmd_get_capabilities_rq {
	uint8_t picmg_id;
	uint8_t hpmx_id;
} ATTRIBUTE_PACKING;

/* HPM.2 Get Capabilities response */
struct hpm2_cmd_get_capabilities_rp {
	uint8_t picmg_id;
	struct hpm2_lan_attach_capabilities caps;
} ATTRIBUTE_PACKING;

#if HAVE_PRAGMA_PACK
# pragma pack(pop)
#endif

/* IPMI Get LAN Configuration Parameters command */
#define IPMI_LAN_GET_CONFIG	0x02

int hpm2_get_capabilities(struct ipmi_intf * intf,
		struct hpm2_lan_attach_capabilities * caps)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	struct hpmx_cmd_get_capabilities_rq rq;

	/* reset result */
	memset(caps, 0, sizeof(struct hpm2_lan_attach_capabilities));

	/* prepare request */
	rq.picmg_id = 0;
	rq.hpmx_id = 2;

	/* prepare request */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_PICMG;
	req.msg.cmd = HPM2_GET_LAN_ATTACH_CAPABILITIES;
	req.msg.data = (uint8_t *)&rq;
	req.msg.data_len = sizeof(rq);


	/* send */
	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_NOTICE, "Error sending request.");
		return -1;
	}

	if (rsp->ccode == 0xC1) {
		lprintf(LOG_DEBUG, "IPM Controller is not HPM.2 compatible");
		return rsp->ccode;
	} else if (rsp->ccode) {
		lprintf(LOG_NOTICE, "Get HPM.x Capabilities request failed,"
				" compcode = %x", rsp->ccode);
		return rsp->ccode;
	}

	/* check response length */
	if (rsp->data_len < 2 || rsp->data_len > 10) {
		lprintf(LOG_NOTICE, "Bad response length, len=%d", rsp->data_len);
		return -1;
	}

	/* check HPM.x identifier */
	if (rsp->data[1] != 2) {
		lprintf(LOG_NOTICE, "Bad HPM.x ID, id=%d", rsp->data[1]);
		return rsp->ccode;
	}

	/*
	 * this hardly can happen, since completion code is already checked.
	 * but check for safety
	 */
	if (rsp->data_len < 4) {
		lprintf(LOG_NOTICE, "Bad response length, len=%d", rsp->data_len);
		return -1;
	}

	/* copy HPM.2 capabilities */
	memcpy(caps, rsp->data + 2, rsp->data_len - 2);

#if WORDS_BIGENDIAN
	/* swap bytes to convert from little-endian format */
	caps->lan_channel_mask = BSWAP_16(caps->lan_channel_mask);
#endif

	/* check HPM.2 revision */
	if (caps->hpm2_revision_id == 0) {
		lprintf(LOG_NOTICE, "Bad HPM.2 revision, rev=%d",
				caps->hpm2_revision_id);
		return -1;
	}

	if (!caps->lan_channel_mask) {
		return -1;
	}

	/* check response length */
	if (rsp->data_len < 8) {
		lprintf(LOG_NOTICE, "Bad response length, len=%d", rsp->data_len);
		return -1;
	}

	/* check HPM.2 LAN parameters start */
	if (caps->hpm2_lan_params_start < 0xC0) {
		lprintf(LOG_NOTICE, "Bad HPM.2 LAN params start, start=%x",
				caps->hpm2_lan_params_start);
		return -1;
	}

	/* check HPM.2 LAN parameters revision */
	if (caps->hpm2_lan_params_rev != HPM2_LAN_PARAMS_REV) {
		lprintf(LOG_NOTICE, "Bad HPM.2 LAN params revision, rev=%d",
				caps->hpm2_lan_params_rev);
		return -1;
	}

	/* check for HPM.2 SOL extension */
	if (!(caps->hpm2_caps & HPM2_CAPS_SOL_EXTENSION)) {
		/* no further checks */
		return 0;
	}

	/* check response length */
	if (rsp->data_len < 10) {
		lprintf(LOG_NOTICE, "Bad response length, len=%d", rsp->data_len);
		return -1;
	}

	/* check HPM.2 SOL parameters start */
	if (caps->hpm2_sol_params_start < 0xC0) {
		lprintf(LOG_NOTICE, "Bad HPM.2 SOL params start, start=%x",
				caps->hpm2_sol_params_start);
		return -1;
	}

	/* check HPM.2 SOL parameters revision */
	if (caps->hpm2_sol_params_rev != HPM2_SOL_PARAMS_REV) {
		lprintf(LOG_NOTICE, "Bad HPM.2 SOL params revision, rev=%d",
				caps->hpm2_sol_params_rev);
		return -1;
	}

	return 0;
}

int hpm2_get_lan_channel_capabilities(struct ipmi_intf * intf,
		uint8_t hpm2_lan_params_start,
		struct hpm2_lan_channel_capabilities * caps)
{
	struct ipmi_rq req;
	struct ipmi_rs * rsp;
	uint8_t rq[4];

	/* reset result */
	memset(caps, 0, sizeof(struct hpm2_lan_channel_capabilities));

	/* prepare request */
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_LAN_GET_CONFIG;
	req.msg.data = (uint8_t *)&rq;
	req.msg.data_len = sizeof(rq);

	/* prepare request data */
	rq[0] = 0xE;					/* sending channel */
	rq[1] = hpm2_lan_params_start;	/* HPM.2 Channel Caps */
	rq[2] = rq[3] = 0;

	/* send */
	rsp = intf->sendrecv(intf, &req);

	if (!rsp) {
		lprintf(LOG_NOTICE, "Error sending request.");
		return -1;
	}

	if (rsp->ccode == 0x80) {
		lprintf(LOG_DEBUG, "HPM.2 Channel Caps parameter is not supported");
		return rsp->ccode;
	} else if (rsp->ccode) {
		lprintf(LOG_NOTICE, "Get LAN Configuration Parameters request failed,"
				" compcode = %x", rsp->ccode);
		return rsp->ccode;
	}

	/* check response length */
	if (rsp->data_len != sizeof (struct hpm2_lan_channel_capabilities) + 1) {
		lprintf(LOG_NOTICE, "Bad response length, len=%d", rsp->data_len);
		return -1;
	}

	/* check parameter revision */
	if (rsp->data[0] !=
			LAN_PARAM_REV(HPM2_LAN_PARAMS_REV, HPM2_LAN_PARAMS_REV)) {
		lprintf(LOG_NOTICE, "Bad HPM.2 LAN parameter revision, rev=%d",
				rsp->data[0]);
		return -1;
	}

	/* copy parameter data */
	memcpy(caps, &rsp->data[1], sizeof (struct hpm2_lan_channel_capabilities));

#if WORDS_BIGENDIAN
	/* swap bytes to convert from little-endian format */
	caps->max_inbound_pld_size = BSWAP_16(caps->max_inbound_pld_size);
	caps->max_outbound_pld_size = BSWAP_16(caps->max_outbound_pld_size);
#endif

	return 0;
}

int hpm2_detect_max_payload_size(struct ipmi_intf * intf)
{
	struct hpm2_lan_attach_capabilities attach_caps;
	struct hpm2_lan_channel_capabilities channel_caps;
	int err;

	/* query HPM.2 support */
	err = hpm2_get_capabilities(intf, &attach_caps);

	/* check if HPM.2 is supported */
	if (err != 0 || !attach_caps.lan_channel_mask) {
		return err;
	}

	/* query channel capabilities */
	err = hpm2_get_lan_channel_capabilities(intf,
			attach_caps.hpm2_lan_params_start, &channel_caps);

	/* check if succeeded */
	if (err != 0) {
		return err;
	}

	/* update request and response sizes */
	ipmi_intf_set_max_request_data_size(intf,
			channel_caps.max_inbound_pld_size - 7);
	ipmi_intf_set_max_response_data_size(intf,
			channel_caps.max_outbound_pld_size - 8);

	/* print debug info */
	lprintf(LOG_DEBUG, "Set maximum request size to %d\n"
			"Set maximum response size to %d",
			intf->max_request_data_size, intf->max_response_data_size);

	return 0;
}
