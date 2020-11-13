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

#define SOL_ESCAPE_CHARACTER_DEFAULT        '~'
#define SOL_KEEPALIVE_TIMEOUT               15
#define SOL_KEEPALIVE_RETRIES               3

#define IPMI_SOL_SERIAL_ALERT_MASK_SUCCEED  0x08
#define IPMI_SOL_SERIAL_ALERT_MASK_DEFERRED 0x04
#define IPMI_SOL_SERIAL_ALERT_MASK_FAIL     0x00
#define IPMI_SOL_BMC_ASSERTS_CTS_MASK_TRUE  0x00
#define IPMI_SOL_BMC_ASSERTS_CTS_MASK_FALSE 0x02


struct sol_config_parameters {
	uint8_t  set_in_progress;
	uint8_t  enabled;
	uint8_t  force_encryption;
	uint8_t  force_authentication;
	uint8_t  privilege_level;
	uint8_t  character_accumulate_level;
	uint8_t  character_send_threshold;
	uint8_t  retry_count;
	uint8_t  retry_interval;
	uint8_t  non_volatile_bit_rate;
	uint8_t  volatile_bit_rate;
	uint8_t  payload_channel;
	uint16_t payload_port;
};


/*
 * The ACTIVATE PAYLOAD command response structure
 * From table 24-2 of the IPMI v2.0 spec
 */
#ifdef PRAGMA_PACK
#pramga pack(1)
#endif
struct activate_payload_rsp {
	uint8_t auxiliary_data[4];
	uint8_t inbound_payload_size[2];  /* LS byte first */
	uint8_t outbound_payload_size[2]; /* LS byte first */
	uint8_t payload_udp_port[2];      /* LS byte first */
	uint8_t payload_vlan_number[2];   /* LS byte first */
} ATTRIBUTE_PACKING;
#ifdef PRAGMA_PACK
#pramga pack(0)
#endif

/*
 * Small function to validate that user-supplied SOL
 * configuration parameter values we store in uint8_t
 * data type falls within valid range.  With minval
 * and maxval parameters we can use the same function
 * to validate parameters that have different ranges
 * of values.
 *
 * function will return -1 if value is not valid, or
 * will return 0 if valid.
 */
int ipmi_sol_set_param_isvalid_uint8_t(const char *strval,
				       const char *name,
				       uint8_t minval,
				       uint8_t maxval,
				       uint8_t *out_value);

int ipmi_sol_main(struct ipmi_intf *, int, char **);
int ipmi_get_sol_info(struct ipmi_intf *intf,
		      uint8_t channel,
		      struct sol_config_parameters *params);
