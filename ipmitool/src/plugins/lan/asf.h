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

#ifndef IPMI_ASF_H
#define IPMI_ASF_H

#include <ipmitool/helper.h>
#include "lan.h"

#define ASF_RMCP_IANA		0x000011be

#define ASF_TYPE_PING		0x80
#define ASF_TYPE_PONG		0x40

static const struct valstr asf_type_vals[] __attribute__((unused)) = {
	{ 0x10, "Reset" },
	{ 0x11, "Power-up" },
	{ 0x12, "Unconditional Power-down" },
	{ 0x13, "Power Cycle" },
	{ 0x40, "Presence Pong" },
	{ 0x41, "Capabilities Response" },
	{ 0x42, "System State Response" },
	{ 0x80, "Presence Ping" },
	{ 0x81, "Capabilities Request" },
	{ 0x82, "System State Request" },
	{ 0x00, NULL }
};

/* ASF message header */
struct asf_hdr {
	uint32_t	iana;
	uint8_t		type;
	uint8_t		tag;
	uint8_t		__reserved;
	uint8_t		len;
} __attribute__((packed));

int handle_asf(struct ipmi_intf * intf, uint8_t * data, int data_len);

#endif /* IPMI_ASF_H */
