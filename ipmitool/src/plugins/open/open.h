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

#ifndef IPMI_OPENIPMI_H
#define IPMI_OPENIPMI_H

#define IPMI_MAX_ADDR_SIZE		0x20
#define IPMI_BMC_CHANNEL		0xf
#define IPMI_NUM_CHANNELS		0x10

#define IPMI_SYSTEM_INTERFACE_ADDR_TYPE	0x0c
#define IPMI_IPMB_ADDR_TYPE		0x01
#define IPMI_IPMB_BROADCAST_ADDR_TYPE	0x41

#define IPMI_RESPONSE_RECV_TYPE		1
#define IPMI_ASYNC_EVENT_RECV_TYPE	2
#define IPMI_CMD_RECV_TYPE		3

struct ipmi_addr {
	int addr_type;
	short channel;
	char data[IPMI_MAX_ADDR_SIZE];
};

struct ipmi_msg {
	unsigned char netfn;
        unsigned char cmd;
        unsigned short data_len;
        unsigned char *data;
};

struct ipmi_req {
	unsigned char *addr;
	unsigned int addr_len;
	long msgid;
	struct ipmi_msg msg;
};

struct ipmi_recv {
	int recv_type;
	unsigned char *addr;
	unsigned int addr_len;
	long msgid;
	struct ipmi_msg msg;
};

struct ipmi_cmdspec {
	unsigned char netfn;
	unsigned char cmd;
};

struct ipmi_system_interface_addr {
	int addr_type;
	short channel;
	unsigned char lun;
};

struct ipmi_ipmb_addr {
	int addr_type;
	short channel;
	unsigned char slave_addr;
	unsigned char lun;
#ifdef ENABLE_INTF_OPEN_DUAL_BRIDGE
	short transit_channel;
	unsigned char transit_slave_addr;
#endif
};

#define IPMI_IOC_MAGIC			'i'
#define IPMICTL_RECEIVE_MSG_TRUNC	_IOWR(IPMI_IOC_MAGIC, 11, struct ipmi_recv)
#define IPMICTL_RECEIVE_MSG		_IOWR(IPMI_IOC_MAGIC, 12, struct ipmi_recv)
#define IPMICTL_SEND_COMMAND		_IOR(IPMI_IOC_MAGIC, 13, struct ipmi_req)
#define IPMICTL_REGISTER_FOR_CMD	_IOR(IPMI_IOC_MAGIC, 14, struct ipmi_cmdspec)
#define IPMICTL_UNREGISTER_FOR_CMD	_IOR(IPMI_IOC_MAGIC, 15, struct ipmi_cmdspec)
#define IPMICTL_SET_GETS_EVENTS_CMD	_IOR(IPMI_IOC_MAGIC, 16, int)
#define IPMICTL_SET_MY_ADDRESS_CMD	_IOR(IPMI_IOC_MAGIC, 17, unsigned int)
#define IPMICTL_GET_MY_ADDRESS_CMD	_IOR(IPMI_IOC_MAGIC, 18, unsigned int)
#define IPMICTL_SET_MY_LUN_CMD		_IOR(IPMI_IOC_MAGIC, 19, unsigned int)
#define IPMICTL_GET_MY_LUN_CMD		_IOR(IPMI_IOC_MAGIC, 20, unsigned int)

#endif /*IPMI_OPENIPMI_H*/
