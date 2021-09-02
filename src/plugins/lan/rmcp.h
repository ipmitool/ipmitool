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

#pragma once

#include <ipmitool/helper.h>
#include "lan.h"
#include "asf.h"

#define RMCP_VERSION_1		0x06

#define RMCP_UDP_PORT		0x26f /* port 623 */
#define RMCP_UDP_SECURE_PORT	0x298 /* port 664 */

#define RMCP_TYPE_MASK		0x80
#define RMCP_TYPE_NORM		0x00
#define RMCP_TYPE_ACK		0x01

#define RMCP_CLASS_MASK		0x1f
#define RMCP_CLASS_ASF		0x06
#define RMCP_CLASS_IPMI		0x07
#define RMCP_CLASS_OEM		0x08

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
/* RMCP message header */
struct rmcp_hdr {
	uint8_t ver;
	uint8_t __reserved;
	uint8_t seq;
	uint8_t class;
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct rmcp_pong {
	struct rmcp_hdr rmcp;
	struct asf_hdr asf;
	uint32_t iana;
	uint32_t oem;
	uint8_t sup_entities;
	uint8_t sup_interact;
	uint8_t reserved[6];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

int handle_rmcp(struct ipmi_intf * intf, uint8_t * data, int data_len);
