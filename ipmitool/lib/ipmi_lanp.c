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

#include <ipmitool/ipmi.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_lanp.h>

extern int verbose;
extern struct ipmi_session lan_session;

const struct valstr ipmi_privlvl_vals[] = {
	{ IPMI_SESSION_PRIV_CALLBACK,	"CALLBACK" },
	{ IPMI_SESSION_PRIV_USER,	"USER" },
	{ IPMI_SESSION_PRIV_OPERATOR,	"OPERATOR" },
	{ IPMI_SESSION_PRIV_ADMIN,	"ADMINISTRATOR" },
	{ IPMI_SESSION_PRIV_OEM,	"OEM" },
	{ 0xF,				"NO ACCESS" },
	{ 0,				NULL },
};

const struct valstr ipmi_authtype_vals[] = {
	{ IPMI_SESSION_AUTHTYPE_NONE,	"NONE" },
	{ IPMI_SESSION_AUTHTYPE_MD2,	"MD2" },
	{ IPMI_SESSION_AUTHTYPE_MD5,	"MD5" },
	{ IPMI_SESSION_AUTHTYPE_KEY,	"PASSWORD" },
	{ IPMI_SESSION_AUTHTYPE_OEM,	"OEM" },
	{ 0,				NULL },
};

static const struct valstr ipmi_channel_protocol_vals[] = {
	{ 0x00, "reserved" },
	{ 0x01, "IPMB-1.0" },
	{ 0x02, "ICMB-1.0" },
	{ 0x03, "reserved" },
	{ 0x04, "IPMI-SMBus" },
	{ 0x05, "KCS" },
	{ 0x06, "SMIC" },
	{ 0x07, "BT-10" },
	{ 0x08, "BT-15" },
	{ 0x09, "TMode" },
	{ 0x1c, "OEM 1" },
	{ 0x1d, "OEM 2" },
	{ 0x1e, "OEM 3" },
	{ 0x1f, "OEM 4" },
	{ 0x00, NULL },
};

static const struct valstr ipmi_channel_medium_vals[] = {
	{ 0x00, "reserved" },
	{ 0x01, "IPMB (I2C)" },
	{ 0x02, "ICMB v1.0" },
	{ 0x03, "ICMB v0.9" },
	{ 0x04, "802.3 LAN" },
	{ 0x05, "Serial/Modem" },
	{ 0x06, "Other LAN" },
	{ 0x07, "PCI SMBus" },
	{ 0x08, "SMBus v1.0/v1.1" },
	{ 0x09, "SMBus v2.0" },
	{ 0x0a, "USB 1.x" },
	{ 0x0b, "USB 2.x" },
	{ 0x0c, "System Interface" },
	{ 0x00, NULL },
};

static struct lan_param *
get_lan_param(struct ipmi_intf * intf, unsigned char chan, int param)
{
	struct lan_param * p;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
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

static int
set_lan_param(struct ipmi_intf * intf, unsigned char chan, int param, unsigned char * data, int len)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
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

void
ipmi_get_channel_info(struct ipmi_intf * intf, unsigned char channel)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[2];

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x42;
	req.msg.data = &channel;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);

	if (!rsp || rsp->ccode) {
		printf("Error:%x Get Channel Info Command (0x%x)\n",
		       rsp ? rsp->ccode : 0, channel);
		return;
	}

	if (!verbose)
		return;

	printf("Channel 0x%x info:\n", rsp->data[0] & 0xf);

	printf("  Channel Medium Type   : %s\n",
	       val2str(rsp->data[1] & 0x7f, ipmi_channel_medium_vals));

	printf("  Channel Protocol Type : %s\n",
	       val2str(rsp->data[2] & 0x1f, ipmi_channel_protocol_vals));

	printf("  Session Support       : ");
	switch (rsp->data[3] & 0xc0) {
	case 0x00:
		printf("session-less\n");
		break;
	case 0x40:
		printf("single-session\n");
		break;
	case 0x80:
		printf("multi-session\n");
		break;
	case 0xc0:
	default:
		printf("session-based\n");
		break;
	}

	printf("  Active Session Count  : %d\n",
	       rsp->data[3] & 0x3f);
	printf("  Protocol Vendor ID    : %d\n",
	       rsp->data[4] | rsp->data[5] << 8 | rsp->data[6] << 16);

	memset(&req, 0, sizeof(req));
	rqdata[0] = channel & 0xf;

	/* get volatile settings */

	rqdata[1] = 0x80; /* 0x80=active */
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x41;
	req.msg.data = rqdata;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		return;
	}

	printf("  Volatile(active) Settings\n");
	printf("    Alerting            : %sabled\n", (rsp->data[0] & 0x20) ? "dis" : "en");
	printf("    Per-message Auth    : %sabled\n", (rsp->data[0] & 0x10) ? "dis" : "en");
	printf("    User Level Auth     : %sabled\n", (rsp->data[0] & 0x08) ? "dis" : "en");
	printf("    Access Mode         : ");
	switch (rsp->data[0] & 0x7) {
	case 0:
		printf("disabled\n");
		break;
	case 1:
		printf("pre-boot only\n");
		break;
	case 2:
		printf("always available\n");
		break;
	case 3:
		printf("shared\n");
		break;
	default:
		printf("unknown\n");
		break;
	}

	/* get non-volatile settings */

	rqdata[1] = 0x40; /* 0x40=non-volatile */
	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		return;
	}

	printf("  Non-Volatile Settings\n");
	printf("    Alerting            : %sabled\n", (rsp->data[0] & 0x20) ? "dis" : "en");
	printf("    Per-message Auth    : %sabled\n", (rsp->data[0] & 0x10) ? "dis" : "en");
	printf("    User Level Auth     : %sabled\n", (rsp->data[0] & 0x08) ? "dis" : "en");
	printf("    Access Mode         : ");
	switch (rsp->data[0] & 0x7) {
	case 0:
		printf("disabled\n");
		break;
	case 1:
		printf("pre-boot only\n");
		break;
	case 2:
		printf("always available\n");
		break;
	case 3:
		printf("shared\n");
		break;
	default:
		printf("unknown\n");
		break;
	}
	
}

static void
lan_set_arp_interval(struct ipmi_intf * intf, unsigned char chan, unsigned char * ival)
{
	struct lan_param *lp;
	unsigned char interval;
	
	lp = get_lan_param(intf, chan, IPMI_LANP_GRAT_ARP);
	if (!lp)
		return;

	if (ival) {
		interval = ((unsigned char)atoi(ival) * 2) - 1;
		set_lan_param(intf, chan, IPMI_LANP_GRAT_ARP, &interval, 1);
	} else {
		interval = lp->data[0];
	}

	printf("BMC-generated Gratuitous ARP interval:  %.1f seconds\n",
	       (float)((interval + 1) / 2));
}

static void
lan_set_arp_generate(struct ipmi_intf * intf, unsigned char chan, unsigned char ctl)
{
	struct lan_param *lp;
	unsigned char data;

	lp = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (!lp)
		return;
	data = lp->data[0];

	if (ctl)
		data |= 0x1;
	else
		data &= ~0x1;

	printf("BMC-generated Gratuitous ARPs %sabled\n", ctl ? "en" : "dis");
	set_lan_param(intf, chan, IPMI_LANP_BMC_ARP, &data, 1);
}

static void
lan_set_arp_respond(struct ipmi_intf * intf, unsigned char chan, unsigned char ctl)
{
	struct lan_param *lp;
	unsigned char data;

	lp = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (!lp)
		return;
	data = lp->data[0];

	if (ctl)
		data |= 0x2;
	else
		data &= ~0x2;

	printf("BMC-generated ARP response %sabled\n", ctl ? "en" : "dis");
	set_lan_param(intf, chan, IPMI_LANP_BMC_ARP, &data, 1);
}

static void
ipmi_lan_print(struct ipmi_intf * intf, unsigned char chan)
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
static void
ipmi_lan_set_auth(struct ipmi_intf * intf, unsigned char chan, char * level, char * types)
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
		else if (!strncmp(p, "md5", 3))
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_MD5;
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
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
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
ipmi_set_channel_access(struct ipmi_intf * intf, unsigned char channel, unsigned char enable)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[3];

	memset(&req, 0, sizeof(req));
	rqdata[0] = channel & 0xf;

	rqdata[1] = 0x60;	/* save to nvram first */
	if (enable)
		rqdata[1] |= 0x2; /* set always available if enable is set */
	rqdata[2] = 0x44;	/* save to nvram first */
	
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x40;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Set Channel Access Command (0x%x)\n",
		       rsp ? rsp->ccode : 0, channel);
		return -1;
	}

	rqdata[1] = 0xa0;	/* set pef disabled, per-msg auth enabled */
	if (enable)
		rqdata[1] |= 0x2; /* set always available if enable is set */
	rqdata[2] = 0x84; 	/* set channel privilege limit to ADMIN */

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Set Channel Access Command (0x%x)\n",
		       rsp ? rsp->ccode : 0, channel);
		return -1;
	}

	if (!enable)		/* can't send close session if access off */
		intf->abort = 1;

	return 0;
}

static int
ipmi_set_user_access(struct ipmi_intf * intf, unsigned char channel, unsigned char userid)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[4];

	memset(&req, 0, sizeof(req));
	rqdata[0] = 0x90 | (channel & 0xf);
	rqdata[1] = userid & 0x3f;
	rqdata[2] = 0x4;
	rqdata[3] = 0;
	
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x43;
	req.msg.data = rqdata;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp || rsp->ccode) {
		printf("Error:%x Set User Access Command (0x%x)\n",
		       rsp ? rsp->ccode : 0, channel);
		return -1;
	}

	return 0;
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

static void
ipmi_lan_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	unsigned char data[32];
	unsigned char chan;
	memset(&data, 0, sizeof(data));

	if (argc < 2 || !strncmp(argv[0], "help", 4)) {
		printf("usage: lan set <channel> <command>\n");
		printf("LAN set commands: ipaddr, netmask, macaddr, defgw, bakgw, password, auth, ipsrc, access, user, arp\n");
		return;
	}

	chan = (unsigned char)strtol(argv[0], NULL, 0);
	if (chan != 0x6 && chan != 0x7) {
		printf("valid LAN channels are 6 and 7\n");
		return;
	}

	/* set user access */
	if (!strncmp(argv[1], "user", 4)) {
		ipmi_set_user_access(intf, chan, 1);
		return;
	}

	/* set channel access mode */
	if (!strncmp(argv[1], "access", 6)) {
		if (argc < 3 || !strncmp(argv[2], "help", 4)) {
			printf("lan set access <on|off>\n");
		}
		else if (!strncmp(argv[2], "on", 2)) {
			ipmi_set_channel_access(intf, chan, 1);
		}
		else if (!strncmp(argv[2], "off", 3)) {
			ipmi_set_channel_access(intf, chan, 0);
		}
		else {
			printf("lan set access <on|off>\n");
		}
		return;
	}

	/* set ARP control */
	if (!strncmp(argv[1], "arp", 3)) {
		if (argc < 3 || !strncmp(argv[2], "help", 4)) {
			printf("lan set <channel> arp respond <on|off>\n");
			printf("lan set <channel> arp generate <on|off>\n");
			printf("lan set <channel> arp interval <seconds>\n\n");
			printf("example: lan set 7 arp gratuitous off\n");
		}
		else if (!strncmp(argv[2], "interval", 8)) {
			lan_set_arp_interval(intf, chan, argv[3]);
		}
		else if (!strncmp(argv[2], "generate", 8)) {
			if (argc < 4)
				printf("lan set <channel> arp generate <on|off>\n");
			else if (!strncmp(argv[3], "on", 2))
				lan_set_arp_generate(intf, chan, 1);
			else if (!strncmp(argv[3], "off", 3))
				lan_set_arp_generate(intf, chan, 0);
			else
				printf("lan set <channel> arp generate <on|off>\n");
		}
		else if (!strncmp(argv[2], "respond", 7)) {
			if (argc < 4)
				printf("lan set <channel> arp respond <on|off>\n");
			else if (!strncmp(argv[3], "on", 2))
				lan_set_arp_respond(intf, chan, 1);
			else if (!strncmp(argv[3], "off", 3))
				lan_set_arp_respond(intf, chan, 0);
			else
				printf("lan set <channel> arp respond <on|off>\n");
		}
		else {
			printf("lan set <channel> arp respond <on|off>\n");
			printf("lan set <channel> arp generate <on|off>\n");
			printf("lan set <channel> arp interval <seconds>\n");
		}
		return;
	}

	/* set authentication types */
	if (!strncmp(argv[1], "auth", 4)) {
		if (argc < 3 || !strncmp(argv[2], "help", 4)) {
			printf("lan set <channel> auth <level> <type,type,...>\n");
			printf("  level = callback, user, operator, admin\n");
			printf("  types = none, md2, md5, key\n");
			printf("example: lan set 7 auth admin key,md5\n");
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

int
ipmi_lanp_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	if (!argc || !strncmp(argv[0], "help", 4))
		printf("LAN Commands:  print, set\n");
	else if (!strncmp(argv[0], "printconf", 9) ||
		 !strncmp(argv[0], "print", 5)) {
		unsigned char chan = 7;
		if (argc > 1)
			chan = (unsigned char)strtol(argv[1], NULL, 0);
		ipmi_lan_print(intf, chan);
	}
	else if (!strncmp(argv[0], "set", 3))
		ipmi_lan_set(intf, argc-1, &(argv[1]));
	else
		printf("Invalid LAN command: %s\n", argv[0]);

	return 0;
}

