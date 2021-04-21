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

#include <ipmitool/ipmi.h>

int ipmi_firewall_main(struct ipmi_intf *, int, char **);

#define BMC_GET_NETFN_SUPPORT				0x09
#define BMC_GET_COMMAND_SUPPORT				0x0A
#define BMC_GET_COMMAND_SUBFUNCTION_SUPPORT		0x0B
#define BMC_GET_CONFIGURABLE_COMMANDS			0x0C
#define BMC_GET_CONFIGURABLE_COMMAND_SUBFUNCTIONS 	0x0D
#define BMC_SET_COMMAND_ENABLES				0x60
#define BMC_GET_COMMAND_ENABLES				0x61
#define BMC_SET_COMMAND_SUBFUNCTION_ENABLES		0x62
#define BMC_GET_COMMAND_SUBFUNCTION_ENABLES		0x63
#define BMC_OEM_NETFN_IANA_SUPPORT			0x64

#define SET_COMMAND_ENABLE_BYTE (BMC_SET_COMMAND_ENABLES / 8)
#define SET_COMMAND_ENABLE_BIT (BMC_SET_COMMAND_ENABLES % 8)

#define MAX_LUN 4
#define MAX_NETFN 64
#define MAX_NETFN_PAIR (MAX_NETFN/2)
#define MAX_COMMAND 256
#define MAX_SUBFN 32
#define MAX_COMMAND_BYTES (MAX_COMMAND>>3)
#define MAX_SUBFN_BYTES (MAX_SUBFN>>3)

// support is a bitfield with the following bits set...
#define BIT_AVAILABLE 0x01
#define BIT_CONFIGURABLE 0x02
#define BIT_ENABLED 0x04

extern int verbose;

struct command_support {
	unsigned char support;
	unsigned char version[3];
	unsigned char subfn_support[MAX_SUBFN_BYTES];
	unsigned char subfn_config[MAX_SUBFN_BYTES];
	unsigned char subfn_enable[MAX_SUBFN_BYTES];
};
struct lun_netfn_support {
	unsigned char support;
	struct command_support command[MAX_COMMAND];
	unsigned char command_mask[MAX_COMMAND_BYTES];
	unsigned char config_mask[MAX_COMMAND_BYTES];
	unsigned char enable_mask[MAX_COMMAND_BYTES];
};
struct lun_support {
	unsigned char support;
	struct lun_netfn_support netfn[MAX_NETFN_PAIR];
};
struct bmc_fn_support {
	struct lun_support lun[MAX_LUN];
};
struct ipmi_function_params {
	int channel;
	int lun;
	int netfn;
	int command;
	int subfn;
	unsigned char force;
};

static inline int bit_test(const unsigned char * bf, int n) {
	return !!(bf[n>>3]&(1<<(n%8)));
}
static inline void bit_set(unsigned char * bf, int n, int v) {
	bf[n>>3] = (bf[n>>3] & ~(1<<(n%8))) | ((v?1:0)<<(n%8));
}
