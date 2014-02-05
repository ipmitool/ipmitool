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

#ifndef IPMI_CHASSIS_H
#define IPMI_CHASSIS_H

#include <ipmitool/ipmi.h>

#define IPMI_CHASSIS_CTL_POWER_DOWN	0x0
#define IPMI_CHASSIS_CTL_POWER_UP	0x1
#define IPMI_CHASSIS_CTL_POWER_CYCLE	0x2
#define IPMI_CHASSIS_CTL_HARD_RESET	0x3
#define IPMI_CHASSIS_CTL_PULSE_DIAG	0x4
#define IPMI_CHASSIS_CTL_ACPI_SOFT	0x5

#define IPMI_CHASSIS_POLICY_NO_CHANGE	0x3
#define IPMI_CHASSIS_POLICY_ALWAYS_ON	0x2
#define IPMI_CHASSIS_POLICY_PREVIOUS	0x1
#define IPMI_CHASSIS_POLICY_ALWAYS_OFF	0x0

int ipmi_chassis_power_status(struct ipmi_intf * intf);
int ipmi_chassis_power_control(struct ipmi_intf * intf, uint8_t ctl);
int ipmi_chassis_main(struct ipmi_intf * intf, int argc, char ** argv);
int ipmi_power_main(struct ipmi_intf * intf, int argc, char ** argv);

#endif /*IPMI_CHASSIS_H*/
