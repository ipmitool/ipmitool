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

#ifndef _IPMI_H
#define _IPMI_H

#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <linux/ipmi.h>

#define BUF_SIZE 256

extern int verbose;
extern int csv_output;

struct ipmi_session {
	unsigned char username[16];
	unsigned char challenge[16];
	unsigned char password;
	unsigned char authtype;
	unsigned char authcode[16];
	unsigned char privlvl;
	unsigned long in_seq;
	unsigned long out_seq;
	unsigned long id;
	int active;
};

struct ipmi_req_entry {
	struct ipmi_req req;
	struct ipmi_intf * intf;
	struct ipmi_session * session;
	unsigned char rq_seq;
	unsigned char * msg_data;
	int msg_len;
	struct ipmi_req_entry * next;
};

struct ipmi_rsp {
	unsigned char ccode;
	unsigned char data[BUF_SIZE];
	int data_len;
	struct {
		unsigned char authtype;
		unsigned long seq;
		unsigned long id;
	} session;
	unsigned char msglen;
	struct {
		unsigned char rq_addr;
		unsigned char netfn;
		unsigned char rq_lun;
		unsigned char rs_addr;
		unsigned char rq_seq;
		unsigned char rs_lun;
		unsigned char cmd;
	} header;
};

struct ipmi_intf {
	int fd;
	struct sockaddr_in addr;
	int (*open)(struct ipmi_intf *, char *, int, char *);
	void (*close)(struct ipmi_intf *);
	struct ipmi_rsp *(*sendrecv)(struct ipmi_intf *, struct ipmi_req *);
	int (*ll_send)(struct ipmi_intf *, unsigned char *, int);
	struct ipmi_rsp *(*ll_recv)(struct ipmi_intf *, int);
};

#define IPMI_NETFN_CHASSIS		0x0
#define IPMI_NETFN_BRIDGE		0x2
#define IPMI_NETFN_SE			0x4
#define IPMI_NETFN_APP			0x6
#define IPMI_NETFN_FIRMWARE		0x8
#define IPMI_NETFN_STORAGE		0xa
#define IPMI_NETFN_TRANSPORT		0xc

#define IPMI_BMC_SLAVE_ADDR		0x20
#define IPMI_REMOTE_SWID		0x81

int handle_ipmi(struct ipmi_intf *intf, unsigned char * data, int data_len);

#endif /* _IPMI_H */
