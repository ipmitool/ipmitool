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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>

#include <ipmi.h>
#include <ipmi_lan.h>
#include <helper.h>

static struct lan_param * get_lan_param(struct ipmi_intf * intf, unsigned char chan, int param)
{
	struct lan_param * p;
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned char msg_data[4];

	p = &ipmi_lan_params[param];
	if (!p)
		return NULL;

	msg_data[0] = chan;
	msg_data[1] = p->cmd;
	msg_data[2] = 0;
	msg_data[3] = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = IPMI_LAN_GET_CONFIG;
	req.msg.data     = msg_data;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode)
		return NULL;

	p->data = rsp->data + 1;

	return p;
}

static int set_lan_param(struct ipmi_intf * intf, unsigned char chan, int param, unsigned char * data, int len)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned char msg_data[32];
	
	if (param < 0)
		return -1;

	msg_data[0] = chan;
	msg_data[1] = param;

	memcpy(&msg_data[2], data, len);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_LAN_SET_CONFIG;
	req.msg.data = msg_data;
	req.msg.data_len = len+2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode)
		return -1;

	return 0;
}

#if 0
static void suspend_lan_arp(struct ipmi_intf * intf, int suspend)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned char msg_data[2];

	msg_data[0] = IPMI_LAN_CHANNEL_1;
	msg_data[1] = suspend;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_TRANSPORT;
	req.msg.cmd = IPMI_LAN_SUSPEND_ARP;
	req.msg.data = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (rsp && !rsp->ccode)
		printf("Suspend BMC ARP status  : 0x%x\n", rsp->data[0]);
}

/* Enable Gratuitous ARP and Interval */
static void lan_set_arp(struct ipmi_intf * intf)
{
	unsigned char data;

	data = 0x01;
	set_lan_param(intf, IPMI_LANP_BMC_ARP, &data, 1);

	data = 0x03;
	set_lan_param(intf, IPMI_LANP_GRAT_ARP, &data, 1);
}

static void set_lan_params(struct ipmi_intf * intf)
{
	lan_set_auth(intf);
	lan_set_arp(intf);
	suspend_lan_arp(intf, 0);
}

#endif

static void ipmi_lan_print(struct ipmi_intf * intf, unsigned char chan)
{
	struct lan_param * p;

	p = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE);
	if (p) printf("%-24s: 0x%02x\n", p->desc, p->data[0]);

	p = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE);
	if (p) printf("%-24s: callback=0x%02x user=0x%02x operator=0x%02x admin=0x%02x oem=0x%02x\n",
		      p->desc, p->data[0], p->data[1], p->data[2], p->data[3], p->data[4]);

	p = get_lan_param(intf, chan, IPMI_LANP_IP_ADDR_SRC);
	if (p) printf("%-24s: 0x%02x\n", p->desc, p->data[0]);

	p = get_lan_param(intf, chan, IPMI_LANP_IP_ADDR);
	if (p) printf("%-24s: %d.%d.%d.%d\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_SUBNET_MASK);
	if (p) printf("%-24s: %d.%d.%d.%d\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_MAC_ADDR);
	if (p) printf("%-24s: %02x:%02x:%02x:%02x:%02x:%02x\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3], p->data[4], p->data[5]);

	p = get_lan_param(intf, chan, IPMI_LANP_SNMP_STRING);
	if (p) printf("%-24s: %s\n", p->desc, p->data);

	p = get_lan_param(intf, chan, IPMI_LANP_IP_HEADER);
	if (p) printf("%-24s: TTL=0x%02x flags=0x%02x precedence=0x%02x TOS=0x%02x\n",
		      p->desc, p->data[0], p->data[1] & 0xe0, p->data[2] & 0xe0, p->data[2] & 0x1e);

	p = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (p) printf("%-24s: 0x%02x\n", p->desc, p->data[0]);

	p = get_lan_param(intf, chan, IPMI_LANP_GRAT_ARP);
	if (p) printf("%-24s: 0x%02x\n", p->desc, p->data[0]);

	p = get_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_IP);
	if (p) printf("%-24s: %d.%d.%d.%d\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_MAC);
	if (p) printf("%-24s: %02x:%02x:%02x:%02x:%02x:%02x\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3], p->data[4], p->data[5]);

	p = get_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_IP);
	if (p) printf("%-24s: %d.%d.%d.%d\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_MAC);
	if (p) printf("%-24s: %02x:%02x:%02x:%02x:%02x:%02x\n", p->desc,
		      p->data[0], p->data[1], p->data[2], p->data[3], p->data[4], p->data[5]);
}

/* Configure Authentication Types */
static void ipmi_lan_set_auth(struct ipmi_intf * intf, unsigned char chan, char * level, char * types)
{
	unsigned char data[5];
	unsigned char authtype = 0;
	char * p;
	struct lan_param * lp;

	if (!level || !types)
		return;

	lp = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE);
	if (!lp)
		return;
	if (verbose > 1)
		printf("%-24s: callback=0x%02x user=0x%02x operator=0x%02x admin=0x%02x oem=0x%02x\n",
		       lp->desc, lp->data[0], lp->data[1], lp->data[2], lp->data[3], lp->data[4]);
	memset(data, 0, 5);
	memcpy(data, lp->data, 5);

	p = types;
	while (p) {
		if (!strncmp(p, "none", 4))
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_NONE;
		else if (!strncmp(p, "md2", 3))
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_MD2;
		else if (!strncmp(p, "key", 3))
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_KEY;
		else
			printf("invalid authtype: %s\n", p);
		p = strchr(p, ',');
		if (p)
			p++;
	}

	p = level;
	while (p) {
		if (!strncmp(p, "callback", 8))
			data[0] = authtype;
		else if (!strncmp(p, "user", 4))
			data[1] = authtype;
		else if (!strncmp(p, "operator", 8))
			data[2] = authtype;
		else if (!strncmp(p, "admin", 5))
			data[3] = authtype;
		else 
			printf("invalid auth level: %s\n", p);
		p = strchr(p, ',');
		if (p)
			p++;
	}

	if (verbose > 1)
		printbuf(data, 5, "authtype data");

	set_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE, data, 5);
}

static void
ipmi_lan_set_password(struct ipmi_intf * intf,
	unsigned char userid, unsigned char * password)
{
	struct ipmi_rsp * rsp;
	struct ipmi_req req;
	unsigned char data[18];

	memset(&data, 0, sizeof(data));
	data[0] = userid & 0x3f;/* user ID */
	data[1] = 0x02;		/* set password */
	memcpy(data+2, password, (strlen(password) > 16) ? 16 : strlen(password));

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x47;
	req.msg.data = data;
	req.msg.data_len = 18;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x setting user %d password to %s\n",
		       rsp ? rsp->ccode : 0, userid, password);
		return;
	}

	/* adjust our session password
	 * or we will no longer be able to communicate with BMC
	 */
	lan_session.password = 1;
	memset(lan_session.authcode, 0, 16);
	memcpy(lan_session.authcode, password, strlen(password));

	printf("Password for user %d set to %s\n", userid, lan_session.authcode);
}

static int
get_cmdline_macaddr(char * arg, unsigned char * buf)
{
	unsigned m1, m2, m3, m4, m5, m6;
	if (sscanf(arg, "%02x:%02x:%02x:%02x:%02x:%02x",
		   &m1, &m2, &m3, &m4, &m5, &m6) != 6) {
		printf("Invalid MAC address: %s\n", arg);
		return -1;
	}
	buf[0] = m1;
	buf[1] = m2;
	buf[2] = m3;
	buf[3] = m4;
	buf[4] = m5;
	buf[5] = m6;
	return 0;
}

static int
get_cmdline_ipaddr(char * arg, unsigned char * buf)
{
	unsigned ip1, ip2, ip3, ip4;
	if (sscanf(arg, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4) {
		printf("Invalid IP address: %s\n", arg);
		return -1;
	}
	buf[0] = ip1;
	buf[1] = ip2;
	buf[2] = ip3;
	buf[3] = ip4;
	return 0;
}

static void ipmi_lan_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	unsigned char data[32];
	unsigned char chan;
	memset(&data, 0, sizeof(data));

	if (argc < 2 || !strncmp(argv[0], "help", 4)) {
		printf("usage: lan set <channel> <command>\n");
		printf("LAN set commands: ipaddr, netmask, macaddr, defgw, bakgw, password, auth, ipsrc\n");
		return;
	}

	chan = (unsigned char) strtod(argv[0], NULL);
	if (chan != 0x6 && chan != 0x7) {
		printf("valid LAN channels are 6 and 7\n");
		return;
	}

	/* set authentication types */
	if (!strncmp(argv[1], "auth", 4)) {
		if (argc < 3 || !strncmp(argv[2], "help", 4)) {
			printf("lan set <channel> auth <level> <type,type,...>\n");
			printf("  level = callback, user, operator, admin\n");
			printf("  types = none, md2, key\n");
			printf("example: lan set 7 auth admin key\n");
			return;
		}
		ipmi_lan_set_auth(intf, chan, argv[2], argv[3]);
		return;
	}
	/* ip address source */
	else if (!strncmp(argv[1], "ipsrc", 5)) {
		if (argc < 3 || !strncmp(argv[2], "help", 4)) {
			printf("lan set <channel> ipsrc <source>\n");
			printf("  none   = unspecified\n");
			printf("  static = static address (manually configured)\n");
			printf("  dhcp   = address obtained by BMC running DHCP\n");
			printf("  bios   = address loaded by BIOS or system software\n");
			return;
		}
		if (!strncmp(argv[2], "none", 4))
			data[0] = 0;
		else if (!strncmp(argv[2], "static", 5))
			data[0] = 1;
		else if (!strncmp(argv[2], "dhcp", 4))
			data[0] = 2;
		else if (!strncmp(argv[2], "bios", 4))
			data[0] = 3;
		else
			return;
		set_lan_param(intf, chan, IPMI_LANP_IP_ADDR_SRC, data, 1);
	}
	/* session password
	 * not strictly a lan setting, but its used for lan connections */
	else if (!strncmp(argv[1], "password", 8)) {
		ipmi_lan_set_password(intf, 1, argv[2]);
	}
	/* ip address */
	else if (!strncmp(argv[1], "ipaddr", 6) &&
		 !get_cmdline_ipaddr(argv[2], data)) {
		printf("Setting LAN %s to %d.%d.%d.%d\n",
		       ipmi_lan_params[IPMI_LANP_IP_ADDR].desc,
		       data[0], data[1], data[2], data[3]);
		set_lan_param(intf, chan, IPMI_LANP_IP_ADDR, data, 4);
		/* also set ip address source to "static" */
		data[0] = 0x1;
		set_lan_param(intf, chan, IPMI_LANP_IP_ADDR_SRC, data, 1);
	}
	/* network mask */
	else if (!strncmp(argv[1], "netmask", 7) &&
		 !get_cmdline_ipaddr(argv[2], data)) {
		printf("Setting LAN %s to %d.%d.%d.%d\n",
		       ipmi_lan_params[IPMI_LANP_SUBNET_MASK].desc,
		       data[0], data[1], data[2], data[3]);
		set_lan_param(intf, chan, IPMI_LANP_SUBNET_MASK, data, 4);
	}
	/* mac address */
	else if (!strncmp(argv[1], "macaddr", 7) &&
		 !get_cmdline_macaddr(argv[2], data)) {
		printf("Setting LAN %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
		       ipmi_lan_params[IPMI_LANP_MAC_ADDR].desc,
		       data[0], data[1], data[2], data[3], data[4], data[5]);
		set_lan_param(intf, chan, IPMI_LANP_MAC_ADDR, data, 6);
	}
	/* default gateway settings */
	else if (!strncmp(argv[1], "defgw", 5)) {
		if (argc < 4 || !strncmp(argv[2], "help", 4)) {
			printf("LAN set default gateway Commands: ipaddr, mac\n");
		}
		else if (!strncmp(argv[2], "ipaddr", 5) &&
			 !get_cmdline_ipaddr(argv[3], data)) {
			printf("Setting LAN %s to %d.%d.%d.%d\n",
			       ipmi_lan_params[IPMI_LANP_DEF_GATEWAY_IP].desc,
			       data[0], data[1], data[2], data[3]);
			set_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_IP, data, 4);
		}
		else if (!strncmp(argv[2], "macaddr", 7) &&
			 !get_cmdline_macaddr(argv[3], data)) {
			printf("Setting LAN %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
			       ipmi_lan_params[IPMI_LANP_DEF_GATEWAY_MAC].desc,
			       data[0], data[1], data[2], data[3], data[4], data[5]);
			set_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_MAC, data, 6);
		}
	}
	/* backup gateway settings */
	else if (!strncmp(argv[1], "bakgw", 5)) {
		if (argc < 4 || !strncmp(argv[2], "help", 4)) {
			printf("LAN set backup gateway commands: ipaddr, mac\n");
		}
		else if (!strncmp(argv[2], "ipaddr", 5) &&
			 !get_cmdline_ipaddr(argv[3], data)) {
			printf("Setting LAN %s to %d.%d.%d.%d\n",
			       ipmi_lan_params[IPMI_LANP_BAK_GATEWAY_IP].desc,
			       data[0], data[1], data[2], data[3]);
			set_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_IP, data, 4);
		}
		else if (!strncmp(argv[2], "macaddr", 7) &&
			 !get_cmdline_macaddr(argv[3], data)) {
			printf("Setting LAN %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
			       ipmi_lan_params[IPMI_LANP_BAK_GATEWAY_MAC].desc,
			       data[0], data[1], data[2], data[3], data[4], data[5]);
			set_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_MAC, data, 6);
		}
	}
}

int ipmi_lan_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4))
		printf("LAN Commands:  print, set\n");
	else if (!strncmp(argv[0], "printconf", 9) ||
		 !strncmp(argv[0], "print", 5)) {
		unsigned char chan = 7;
		if (argc > 1)
			chan = (unsigned char) strtod(argv[1], NULL);
		ipmi_lan_print(intf, chan);
	}
	else if (!strncmp(argv[0], "set", 3))
		ipmi_lan_set(intf, argc-1, &(argv[1]));
	else
		printf("Invalid LAN command: %s\n", argv[0]);

	return 0;
}

