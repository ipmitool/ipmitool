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

#ifndef IPMI_SOL_H
#define IPMI_SOL_H

#include <ipmitool/ipmi.h>


#define IPMI_SOL_SERIAL_ALERT_MASK_SUCCEED  0x08
#define IPMI_SOL_SERIAL_ALERT_MASK_DEFERRED 0x04
#define IPMI_SOL_SERIAL_ALERT_MASK_FAIL     0x00
#define IPMI_SOL_BMC_ASSERTS_CTS_MASK_TRUE  0x00
#define IPMI_SOL_BMC_ASSERTS_CTS_MASK_FALSE 0x02


struct sol_config_parameters {
	unsigned char  set_in_progress;
	unsigned char  enabled;
	unsigned char  force_encryption;
	unsigned char  force_authentication;
	unsigned char  privilege_level;
	unsigned char  character_accumulate_level;
	unsigned char  character_send_threshold;
	unsigned char  retry_count;
	unsigned char  retry_interval;
	unsigned char  non_volatile_bit_rate;
	unsigned char  volatile_bit_rate;
	unsigned char  payload_channel;
	unsigned short payload_port;
};


/*
 * The ACTIVATE PAYLOAD command reponse structure
 * From table 24-2 of the IPMI v2.0 spec
 */
struct activate_payload_rsp {
	unsigned char auxiliary_data[4];
	unsigned char inbound_payload_size[2];  /* LS byte first */
	unsigned char outbound_payload_size[2]; /* LS byte first */
	unsigned char payload_udp_port[2];      /* LS byte first */
	unsigned char payload_vlan_number[2];   /* LS byte first */
} __attribute__ ((packed));


int ipmi_sol_main(struct ipmi_intf *, int, char **);
int ipmi_get_sol_info(struct ipmi_intf             * intf,
					  unsigned char                  channel,
					  struct sol_config_parameters * params);


#endif /* IPMI_SOL_H */
