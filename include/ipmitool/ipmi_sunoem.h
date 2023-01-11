/*
 * Copyright (c) 2009, 2014, Oracle and/or its affiliates. All rights reserved.
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
#include <ipmitool/ipmi_sdr.h>

#define IPMI_NETFN_SUNOEM				0x2e

#define IPMI_SUNOEM_SET_SSH_KEY				0x01
#define IPMI_SUNOEM_DEL_SSH_KEY				0x02
#define IPMI_SUNOEM_GET_HEALTH_STATUS			0x10
#define IPMI_SUNOEM_CLI					0x19
#define IPMI_SUNOEM_SET_FAN_SPEED			0x20
#define IPMI_SUNOEM_LED_GET				0x21
#define IPMI_SUNOEM_LED_SET				0x22
#define IPMI_SUNOEM_ECHO				0x23
#define IPMI_SUNOEM_VERSION				0x24
#define IPMI_SUNOEM_NACNAME				0x29
#define IPMI_SUNOEM_GETVAL				0x2A
#define IPMI_SUNOEM_SETVAL				0x2C
#define IPMI_SUNOEM_SENSOR_SET				0x3A
#define IPMI_SUNOEM_SET_FAN_MODE			0x41
#define IPMI_SUNOEM_CORE_TUNNEL                         0x44

/*
 * Error codes of sunoem functions
 */
typedef enum {
	SUNOEM_EC_SUCCESS            = 0,
	SUNOEM_EC_INVALID_ARG        = 1,
	SUNOEM_EC_BMC_NOT_RESPONDING = 2,
	SUNOEM_EC_BMC_CCODE_NONZERO  = 3
} sunoem_ec_t;

int ipmi_sunoem_main(struct ipmi_intf *, int, char **);
