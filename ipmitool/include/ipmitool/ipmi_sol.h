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

//#define ACTIVATE_SOL			0x01
//#define SET_SOL_CONFIG			0x03
//#define GET_SOL_CONFIG			0x04

//#define SOL_ENABLE_PARAM		0x01
//#define SOL_AUTHENTICATION_PARAM	0x02
//#define SOL_ENABLE_FLAG			0x01
//#define SOL_PRIVILEGE_LEVEL_USER	0x02
//#define SOL_BAUD_RATE_PARAM		0x05
//#define SOL_PREFERRED_BAUD_RATE	0x07

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


int ipmi_sol_main(struct ipmi_intf *, int, char **);
int ipmi_get_sol_info(struct ipmi_intf             * intf,
					  unsigned char                  channel,
					  struct sol_config_parameters * params);


#endif /* IPMI_SOL_H */
