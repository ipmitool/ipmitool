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

#ifndef IPMI_INTF_H
#define IPMI_INTF_H

#include <ipmitool/ipmi.h>

#define IPMI_SESSION_AUTHTYPE_NONE	0x0
#define IPMI_SESSION_AUTHTYPE_MD2	0x1
#define IPMI_SESSION_AUTHTYPE_MD5	0x2
#define IPMI_SESSION_AUTHTYPE_KEY	0x4
#define IPMI_SESSION_AUTHTYPE_OEM	0x5

#define IPMI_SESSION_PRIV_CALLBACK	0x1
#define IPMI_SESSION_PRIV_USER		0x2
#define IPMI_SESSION_PRIV_OPERATOR	0x3
#define IPMI_SESSION_PRIV_ADMIN		0x4
#define IPMI_SESSION_PRIV_OEM		0x5

struct ipmi_session {
	unsigned char hostname[64];
	unsigned char username[16];
	unsigned char authcode[16];
	unsigned char challenge[16];
	unsigned char authtype;
	unsigned char privlvl;
	int password;
	int port;
	int active;
	uint32_t session_id;
	uint32_t in_seq;
	uint32_t out_seq;
};

struct ipmi_intf {
	int fd;
	int opened;
	int abort;
	int pedantic;
	int (*open)(struct ipmi_intf *);
	void (*close)(struct ipmi_intf *);
	struct ipmi_rs *(*sendrecv)(struct ipmi_intf *, struct ipmi_rq *);
	struct ipmi_session * session;
};

struct static_intf {
	char * name;
	int (*setup)(struct ipmi_intf ** intf);
};

int ipmi_intf_init(void);
void ipmi_intf_exit(void);
struct ipmi_intf * ipmi_intf_load(char * name);

int ipmi_intf_session_set_hostname(struct ipmi_intf * intf, char * hostname);
int ipmi_intf_session_set_username(struct ipmi_intf * intf, char * username);
int ipmi_intf_session_set_password(struct ipmi_intf * intf, char * password);
int ipmi_intf_session_set_privlvl(struct ipmi_intf * intf, unsigned char privlvl);
int ipmi_intf_session_set_port(struct ipmi_intf * intf, int port);

#endif /* IPMI_INTF_H */
