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

#ifndef IPMI_LAN_H
#define IPMI_LAN_H

#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>

#define IPMI_LAN_CHANNEL_1	0x07
#define IPMI_LAN_CHANNEL_2	0x06
#define IPMI_LAN_CHANNEL_E	0x0e
#define IPMI_LAN_PORT		0x26f

#define IPMI_LAN_TIMEOUT	2
#define IPMI_LAN_RETRY		4

#define IPMI_SESSION_AUTHTYPE_NONE	0x0
#define IPMI_SESSION_AUTHTYPE_MD2	0x1
#define IPMI_SESSION_AUTHTYPE_MD5	0x2
#define IPMI_SESSION_AUTHTYPE_PASSWORD	0x4
#define IPMI_SESSION_AUTHTYPE_OEM	0x5

#define IPMI_SESSION_PRIV_CALLBACK	0x1
#define IPMI_SESSION_PRIV_USER		0x2
#define IPMI_SESSION_PRIV_OPERATOR	0x3
#define IPMI_SESSION_PRIV_ADMIN		0x4
#define IPMI_SESSION_PRIV_OEM		0x5



struct ipmi_rs * ipmi_lan_send_cmd(struct ipmi_intf * intf, struct ipmi_rq * req);
int  ipmi_lan_open(struct ipmi_intf * intf);
void ipmi_lan_close(struct ipmi_intf * intf);
void ipmi_get_channel_info(struct ipmi_intf * intf, unsigned char channel);
int ipmi_lan_ping(struct ipmi_intf * intf);

int lan_intf_setup(struct ipmi_intf ** intf);
struct ipmi_intf ipmi_lan_intf;

#endif /*IPMI_LAN_H*/
