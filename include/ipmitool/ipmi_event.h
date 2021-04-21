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

#if HAVE_CONFIG_H
# include <config.h>
#endif
#include <ipmitool/ipmi.h>

#define EVENT_DIR_ASSERT	0
#define EVENT_DIR_DEASSERT	1

#ifdef HAVE_PRAGMA_PACK
#pragma pack(1)
#endif
struct platform_event_msg {
	uint8_t evm_rev;
	uint8_t sensor_type;
	uint8_t sensor_num;
#if WORDS_BIGENDIAN
	uint8_t event_dir  : 1;
	uint8_t event_type : 7;
#else
	uint8_t event_type : 7;
	uint8_t event_dir  : 1;
#endif
	uint8_t event_data[3];
} ATTRIBUTE_PACKING;
#ifdef HAVE_PRAGMA_PACK
#pragma pack(0)
#endif

/* See IPMI 2.0 Specification, Appendix G, Table G-1, "Event Commands"  */
typedef enum {
	IPMI_CMD_SET_EVENT_RCVR = 0,
	IPMI_CMD_GET_EVENT_RCVR,
	IPMI_CMD_PLATFORM_EVENT
} ipmi_event_cmd_t;

typedef enum {
	PLATFORM_EVENT_DATA_LEN_NON_SI = sizeof(struct platform_event_msg),
	PLATFORM_EVENT_DATA_LEN_SI, /* System interfaces require generator ID */
	PLATFORM_EVENT_DATA_LEN_MAX = PLATFORM_EVENT_DATA_LEN_SI
} ipmi_platform_event_data_len_t;

/* See Table 5-4 */
typedef enum {
	EVENT_SWID_BIOS_BASE = 0x00, /* BIOS */
	EVENT_SWID_SMI_BASE = 0x10, /* SMI Handler */
	EVENT_SWID_SMS_BASE = 0x20, /* System Management Software */
	EVENT_SWID_OEM_BASE = 0x30, /* OEM */
	EVENT_SWID_REMOTE_CONSOLE_BASE = 0x40, /* Remote Console SW */
	EVENT_SWID_TERMINAL_MODE_BASE = 0x47 /* Terminal Mode RC SW */
} ipmi_event_swid_t;
#define EVENT_SWID(base, index) ((EVENT_SWID_##base##_BASE + index) & 0x7F)

/* See Figure 29-2, Table 32-1 */
#define EVENT_GENERATOR(base, index) (EVENT_SWID(base,index) << 1 | 1)

int  ipmi_event_main(struct ipmi_intf *, int, char **);
