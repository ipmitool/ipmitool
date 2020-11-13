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

#pragma once

#include <stdint.h>
#include <ipmitool/ipmi_intf.h>

/* Global HPM.2 defines */
#define HPM2_REVISION		0x01
#define HPM3_REVISION		0x01
#define HPM2_LAN_PARAMS_REV	0x01
#define HPM2_SOL_PARAMS_REV	0x01
#define HPM3_LAN_PARAMS_REV	0x01
/* IPMI defines parameter revision as
 * MSN = present revision,
 * LSN = oldest revision parameter is
 * backward compatible with. */
#define LAN_PARAM_REV(x, y)	(((x) << 4) | ((y) & 0xF))

/* HPM.2 capabilities */
#define HPM2_CAPS_SOL_EXTENSION		0x01
#define HPM2_CAPS_PACKET_TRACE		0x02
#define HPM2_CAPS_EXT_MANAGEMENT	0x04
#define HPM2_CAPS_VERSION_SENSOR	0x08
#define HPM2_CAPS_DYNAMIC_SESSIONS	0x10

#if HAVE_PRAGMA_PACK
# pragma pack(push, 1)
#endif

/* HPM.2 LAN attach capabilities */
struct hpm2_lan_attach_capabilities {
	uint8_t hpm2_revision_id;
	uint16_t lan_channel_mask;
	uint8_t hpm2_caps;
	uint8_t hpm2_lan_params_start;
	uint8_t hpm2_lan_params_rev;
	uint8_t hpm2_sol_params_start;
	uint8_t hpm2_sol_params_rev;
} ATTRIBUTE_PACKING;

/* HPM.2 LAN channel capabilities */
struct hpm2_lan_channel_capabilities {
	uint8_t capabilities;
	uint8_t attach_type;
	uint8_t bandwidth_class;
	uint16_t max_inbound_pld_size;
	uint16_t max_outbound_pld_size;
} ATTRIBUTE_PACKING;

#if HAVE_PRAGMA_PACK
# pragma pack(pop)
#endif

/* HPM.2 command assignments */
#define HPM2_GET_LAN_ATTACH_CAPABILITIES	0x3E

extern int hpm2_get_capabilities(struct ipmi_intf * intf,
		struct hpm2_lan_attach_capabilities * caps);
extern int hpm2_get_lan_channel_capabilities(struct ipmi_intf * intf,
		uint8_t hpm2_lan_params_start,
		struct hpm2_lan_channel_capabilities * caps);
extern int hpm2_detect_max_payload_size(struct ipmi_intf * intf);
