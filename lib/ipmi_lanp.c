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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <netdb.h>
#include <limits.h>

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_channel.h>
#include <ipmitool/ipmi_user.h>

extern int verbose;

static struct lan_param {
	int cmd;
	int size;
	char desc[24];
	uint8_t *data;
	int data_len;
} ipmi_lan_params[] = {
	{ IPMI_LANP_SET_IN_PROGRESS,	1,	"Set in Progress", NULL, 0 },
	{ IPMI_LANP_AUTH_TYPE,		1,	"Auth Type Support", NULL, 0 },
	{ IPMI_LANP_AUTH_TYPE_ENABLE,	5,	"Auth Type Enable", NULL, 0	},
	{ IPMI_LANP_IP_ADDR,		4,	"IP Address", NULL, 0 },
	{ IPMI_LANP_IP_ADDR_SRC,	1,	"IP Address Source", NULL, 0 },
	{ IPMI_LANP_MAC_ADDR,		6,	"MAC Address", NULL, 0 }, /* 5 */
	{ IPMI_LANP_SUBNET_MASK,	4,	"Subnet Mask", NULL, 0 },
	{ IPMI_LANP_IP_HEADER,		3,	"IP Header", NULL, 0 },
	{ IPMI_LANP_PRI_RMCP_PORT,	2,	"Primary RMCP Port", NULL, 0 },
	{ IPMI_LANP_SEC_RMCP_PORT,	2,	"Secondary RMCP Port", NULL, 0 },
	{ IPMI_LANP_BMC_ARP,		1,	"BMC ARP Control", NULL, 0}, /* 10 */
	{ IPMI_LANP_GRAT_ARP,		1,	"Gratituous ARP Intrvl", NULL, 0 },
	{ IPMI_LANP_DEF_GATEWAY_IP,	4,	"Default Gateway IP", NULL, 0 },
	{ IPMI_LANP_DEF_GATEWAY_MAC,	6,	"Default Gateway MAC", NULL, 0 },
	{ IPMI_LANP_BAK_GATEWAY_IP,	4,	"Backup Gateway IP", NULL, 0 },
	{ IPMI_LANP_BAK_GATEWAY_MAC,	6,	"Backup Gateway MAC", NULL, 0 }, /* 15 */
	{ IPMI_LANP_SNMP_STRING,	18,	"SNMP Community String", NULL, 0 },
	{ IPMI_LANP_NUM_DEST,		1,	"Number of Destinations", NULL, 0 },
	{ IPMI_LANP_DEST_TYPE,		4,	"Destination Type", NULL, 0 },
	{ IPMI_LANP_DEST_ADDR,		13,	"Destination Addresses", NULL, 0 },
	{ IPMI_LANP_VLAN_ID,		2,	"802.1q VLAN ID", NULL, 0 }, /* 20 */
	{ IPMI_LANP_VLAN_PRIORITY,	1,	"802.1q VLAN Priority", NULL, 0 },
	{ IPMI_LANP_RMCP_CIPHER_SUPPORT,1,	"RMCP+ Cipher Suite Count", NULL, 0 },
	{ IPMI_LANP_RMCP_CIPHERS,	16,	"RMCP+ Cipher Suites", NULL, 0 },
	{ IPMI_LANP_RMCP_PRIV_LEVELS,	9,	"Cipher Suite Priv Max", NULL, 0 },
	{ IPMI_LANP_BAD_PASS_THRESH,	6,	"Bad Password Threshold", NULL, 0 },
	{ IPMI_LANP_OEM_ALERT_STRING,	28,	"OEM Alert String", NULL, 0 }, /* 25 */
	{ IPMI_LANP_ALERT_RETRY,	1,	"Alert Retry Algorithm", NULL, 0 },
	{ IPMI_LANP_UTC_OFFSET,		3,	"UTC Offset", NULL, 0 },
	{ IPMI_LANP_DHCP_SERVER_IP,	4,	"DHCP Server IP", NULL, 0 },
	{ IPMI_LANP_DHCP_SERVER_MAC,	6,	"DHDP Server MAC", NULL, 0},
	{ IPMI_LANP_DHCP_ENABLE,	1,	"DHCP Enable", NULL, 0 }, /* 30 */
	{ IPMI_LANP_CHAN_ACCESS_MODE,	2,	"Channel Access Mode", NULL, 0 },
	{ -1, -1, "", NULL, -1 }
};

static const struct valstr set_lan_cc_vals[] = {
	{ 0x80, "Unsupported parameter" },
	{ 0x81, "Attempt to set 'in progress' while not in 'complete' state" },
	{ 0x82, "Parameter is read-only" },
	{ 0x83, "Parameter is wrote-only" },
	{ 0x00, NULL }
};

static const struct valstr get_lan_cc_vals[] = {
	{ 0x80, "Unsupported parameter" },
	{ 0x00, NULL }
};

static void print_lan_alert_print_usage(void);
static void print_lan_alert_set_usage(void);
static void print_lan_set_usage(void);
static void print_lan_set_access_usage(void);
static void print_lan_set_arp_usage(void);
static void print_lan_set_auth_usage(void);
static void print_lan_set_bakgw_usage(void);
static void print_lan_set_cipher_privs_usage(void);
static void print_lan_set_defgw_usage(void);
static void print_lan_set_ipsrc_usage(void);
static void print_lan_set_snmp_usage(void);
static void print_lan_set_vlan_usage(void);
static void print_lan_usage(void);

/* is_lan_channel - Check if channel is LAN medium
 *
 * return 1 if channel is LAN
 * return 0 if channel is not LAN
 *
 * @intf:    ipmi interface handle
 * @chan:    channel number to check
 */
static int
is_lan_channel(struct ipmi_intf *intf, uint8_t chan)
{
	uint8_t medium;

	if (chan < 1 || chan > IPMI_CHANNEL_NUMBER_MAX)
		return 0;

	medium = ipmi_get_channel_medium(intf, chan);

	if (medium == IPMI_CHANNEL_MEDIUM_LAN ||
	    medium == IPMI_CHANNEL_MEDIUM_LAN_OTHER)
		return 1;

	return 0;
}

/* find_lan_channel - Find first channel that is LAN
 *
 * return channel number if successful
 * return 0 if no lan channel found, which is not a valid LAN channel
 *
 * @intf:    ipmi interface handle
 * @start:   channel number to start searching from
 */
uint8_t
find_lan_channel(struct ipmi_intf *intf, uint8_t start)
{
	uint8_t chan = 0;

	for (chan = start; chan < IPMI_CHANNEL_NUMBER_MAX; chan++) {
		if (is_lan_channel(intf, chan)) {
			return chan;
		}
	}
	return 0;
}

/* get_lan_param_select - Query BMC for LAN parameter data
 *
 * return pointer to lan_param if successful
 * if parameter not supported then
 *   return pointer to lan_param with
 *   lan_param->data == NULL and lan_param->data_len == 0
 * return NULL on error
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 * @param:   lan parameter id
 * @select:  lan parameter set selector
 */
static struct lan_param *
get_lan_param_select(struct ipmi_intf *intf, uint8_t chan, int param, int select)
{
	struct lan_param *p = NULL;
	struct lan_param *rc = NULL;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	int i = 0;
	uint8_t msg_data[4];

	for (i = 0; ipmi_lan_params[i].cmd != (-1); i++) {
		if (ipmi_lan_params[i].cmd == param) {
			p = &ipmi_lan_params[i];
			break;
		}
	}

	if (!p) {
		lprintf(LOG_INFO, "Get LAN Parameter failed: Unknown parameter.");
		return rc;
	}

	msg_data[0] = chan;
	msg_data[1] = p->cmd;
	msg_data[2] = select;
	msg_data[3] = 0;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = IPMI_LAN_GET_CONFIG;
	req.msg.data     = msg_data;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_INFO, "Get LAN Parameter '%s' command failed", p->desc);
		return rc;
	}

	switch (rsp->ccode)
	{
	case 0x00: /* successful */
		break;

	case 0x80: /* parameter not supported */
	case 0xc9: /* parameter out of range */
	case 0xcc: /* invalid data field in request */
		/* We treat them as valid but empty response */
		p->data = NULL;
		p->data_len = 0;
		rc = p;
		/* fall through */
	default:
		/* other completion codes are treated as error */
		lprintf(LOG_INFO, "Get LAN Parameter '%s' command failed: %s",
			p->desc,
			specific_val2str(rsp->ccode,
			                 get_lan_cc_vals,
			                 completion_code_vals));
		return NULL;
	}

	p->data = rsp->data + 1;
	p->data_len = rsp->data_len - 1;

	return p;
}

/* get_lan_param - Query BMC for LAN parameter data
 *
 * return pointer to lan_param if successful
 * if parameter not supported then
 *   return pointer to lan_param with
 *   lan_param->data == NULL and lan_param->data_len == 0
 * return NULL on error
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 * @param:   lan parameter id
 */
static struct lan_param *
get_lan_param(struct ipmi_intf *intf, uint8_t chan, int param)
{
	return get_lan_param_select(intf, chan, param, 0);
}

/* set_lan_param_wait - Wait for Set LAN Parameter command to complete
 *
 * On some systems this can take unusually long so we wait for the write
 * to take effect and verify that the data was written successfully
 * before continuing or retrying.
 *
 * returns 0 on success
 * returns -1 on error
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 * @param:   lan parameter id
 * @data:    lan parameter data
 * @len:     length of lan parameter data
 */
static int
set_lan_param_wait(struct ipmi_intf *intf, uint8_t chan,
		   int param, uint8_t *data, int len)
{
	struct lan_param *p;
	int retry = 10;		/* 10 retries */

	lprintf(LOG_DEBUG, "Waiting for Set LAN Parameter to complete...");
	if (verbose > 1)
		printbuf(data, len, "SET DATA");

	for (;;) {
		p = get_lan_param(intf, chan, param);
		if (!p) {
			sleep(IPMI_LANP_TIMEOUT);
			if (retry-- == 0)
				return -1;
			continue;
		}
		if (verbose > 1)
			printbuf(p->data, p->data_len, "READ DATA");
		if (p->data_len != len) {
			sleep(IPMI_LANP_TIMEOUT);
			if (retry-- == 0) {
				lprintf(LOG_WARNING, "Mismatched data lengths: %d != %d",
				       p->data_len, len);
				return -1;
			}
			continue;
		}
		if (memcmp(data, p->data, len) != 0) {
			sleep(IPMI_LANP_TIMEOUT);
			if (retry-- == 0) {
				lprintf(LOG_WARNING, "LAN Parameter Data does not match!  "
				       "Write may have failed.");
				return -1;
			}
			continue;
		}
		break;
	}
	return 0;
}

/* __set_lan_param - Write LAN Parameter data to BMC
 *
 * This function does the actual work of writing the LAN parameter
 * to the BMC and calls set_lan_param_wait() if requested.
 *
 * returns 0 on success
 * returns -1 on error
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 * @param:   lan parameter id
 * @data:    lan parameter data
 * @len:     length of lan parameter data
 * @wait:    whether to wait for write completion
 */
static int
__set_lan_param(struct ipmi_intf *intf, uint8_t chan,
		int param, uint8_t *data, int len, int wait)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[32];

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
	if (!rsp) {
		lprintf(LOG_ERR, "Set LAN Parameter failed");
		return -1;
	}
	if (rsp->ccode && wait) {
		lprintf(LOG_DEBUG, "Warning: Set LAN Parameter failed: %s",
			specific_val2str(rsp->ccode,
			                 set_lan_cc_vals,
			                 completion_code_vals));
		if (rsp->ccode == 0xcc) {
			/* retry hack for invalid data field ccode */
			int retry = 10;		/* 10 retries */
			lprintf(LOG_DEBUG, "Retrying...");
			for (;;) {
				if (retry-- == 0)
					break;
				sleep(IPMI_LANP_TIMEOUT);
				rsp = intf->sendrecv(intf, &req);
				if (!rsp || rsp->ccode)
					continue;
				return set_lan_param_wait(intf, chan, param, data, len);
			}
		}
		else if (rsp->ccode != 0xff) {
			/* let 0xff ccode continue */
			return -1;
		}
	}

	if (!wait)
		return 0;
	return set_lan_param_wait(intf, chan, param, data, len);
}

/* ipmi_lanp_lock_state - Retrieve set-in-progress status
 *
 * returns one of:
 *  IPMI_LANP_WRITE_UNLOCK
 *  IPMI_LANP_WRITE_LOCK
 *  IPMI_LANP_WRITE_COMMIT
 *  -1 on error/if not supported
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 */
static int
ipmi_lanp_lock_state(struct ipmi_intf *intf, uint8_t chan)
{
	struct lan_param *p;
	p = get_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS);
	if (!p)
		return -1;
	if (!p->data)
		return -1;
	return (p->data[0] & 3);
}

/* ipmi_lanp_lock - Lock set-in-progress bits for our use
 *
 * Write to the Set-In-Progress LAN parameter to indicate
 * to other management software that we are modifying parameters.
 *
 * No meaningful return value because this is an optional
 * requirement in IPMI spec and not found on many BMCs.
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 */
static void
ipmi_lanp_lock(struct ipmi_intf *intf, uint8_t chan)
{
	uint8_t val = IPMI_LANP_WRITE_LOCK;
	int retry = 3;

	for (;;) {
		int state = ipmi_lanp_lock_state(intf, chan);
		if (state == -1)
			break;
		if (state == val)
			break;
		if (retry-- == 0)
			break;
		__set_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS,
				&val, 1, 0);
	}
}

/* ipmi_lanp_unlock - Unlock set-in-progress bits
 *
 * Write to the Set-In-Progress LAN parameter, first with
 * a "commit" instruction and then unlocking it.
 *
 * No meaningful return value because this is an optional
 * requirement in IPMI spec and not found on many BMCs.
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 */
static void
ipmi_lanp_unlock(struct ipmi_intf *intf, uint8_t chan)
{
	uint8_t val = IPMI_LANP_WRITE_COMMIT;
	int rc;

	rc = __set_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS, &val, 1, 0);
	if (rc < 0) {
		lprintf(LOG_DEBUG, "LAN Parameter Commit not supported");
	}

	val = IPMI_LANP_WRITE_UNLOCK;
	__set_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS, &val, 1, 0);
}

/* set_lan_param - Wrap LAN parameter write with set-in-progress lock
 *
 * Returns value from __set_lan_param()
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 * @param:   lan parameter id
 * @data:    lan parameter data
 * @len:     length of lan parameter data
 */
static int
set_lan_param(struct ipmi_intf *intf, uint8_t chan,
	      int param, uint8_t *data, int len)
{
	int rc;
	ipmi_lanp_lock(intf, chan);
	rc = __set_lan_param(intf, chan, param, data, len, 1);
	ipmi_lanp_unlock(intf, chan);
	return rc;
}

/* set_lan_param_nowait - Wrap LAN parameter write without set-in-progress lock
 *
 * Returns value from __set_lan_param()
 *
 * @intf:    ipmi interface handle
 * @chan:    ipmi channel
 * @param:   lan parameter id
 * @data:    lan parameter data
 * @len:     length of lan parameter data
 */
static int
set_lan_param_nowait(struct ipmi_intf *intf, uint8_t chan,
		     int param, uint8_t *data, int len)
{
	int rc;
	ipmi_lanp_lock(intf, chan);
	rc = __set_lan_param(intf, chan, param, data, len, 0);
	ipmi_lanp_unlock(intf, chan);
	return rc;
}

static int
lan_set_arp_interval(struct ipmi_intf *intf, uint8_t chan, uint8_t ival)
{
	struct lan_param *lp;
	uint8_t interval = 0;
	int rc = 0;

	lp = get_lan_param(intf, chan, IPMI_LANP_GRAT_ARP);
	if (!lp)
		return -1;
	if (!lp->data)
		return -1;

	if (ival != 0) {
		if (((UINT8_MAX - 1) / 2) < ival) {
			lprintf(LOG_ERR, "Given ARP interval '%u' is too big.", ival);
			return (-1);
		}
		interval = (ival * 2) - 1;
		rc = set_lan_param(intf, chan, IPMI_LANP_GRAT_ARP, &interval, 1);
	} else {
		interval = lp->data[0];
	}

	printf("BMC-generated Gratuitous ARP interval:  %.1f seconds\n",
	       (float)((interval + 1) / 2));

	return rc;
}

static int
lan_set_arp_generate(struct ipmi_intf *intf,
		     uint8_t chan, uint8_t ctl)
{
	struct lan_param *lp;
	uint8_t data;

	lp = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (!lp)
		return -1;
	if (!lp->data)
		return -1;
	data = lp->data[0];

	/* set arp generate bitflag */
	if (ctl == 0)
		data &= ~0x1;
	else
		data |= 0x1;

	printf("%sabling BMC-generated Gratuitous ARPs\n", ctl ? "En" : "Dis");
	return set_lan_param(intf, chan, IPMI_LANP_BMC_ARP, &data, 1);
}

static int
lan_set_arp_respond(struct ipmi_intf *intf,
		    uint8_t chan, uint8_t ctl)
{
	struct lan_param *lp;
	uint8_t data;

	lp = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (!lp)
		return -1;
	if (!lp->data)
		return -1;
	data = lp->data[0];

	/* set arp response bitflag */
	if (ctl == 0)
		data &= ~0x2;
	else
		data |= 0x2;

	printf("%sabling BMC-generated ARP responses\n", ctl ? "En" : "Dis");
	return set_lan_param(intf, chan, IPMI_LANP_BMC_ARP, &data, 1);
}

/* TODO - probably move elsewhere */
static char priv_level_to_char(unsigned char priv_level)
{
	char ret = 'X';

	switch (priv_level)
	{
	case IPMI_SESSION_PRIV_CALLBACK:
		ret = 'c';
		break;
	case IPMI_SESSION_PRIV_USER:
		ret = 'u';
		break;
	case IPMI_SESSION_PRIV_OPERATOR:
		ret = 'o';
		break;
	case IPMI_SESSION_PRIV_ADMIN:
		ret = 'a';
		break;
	case IPMI_SESSION_PRIV_OEM:
		ret = 'O';
		break;
	}

 	return ret;
}


static int
ipmi_lan_print(struct ipmi_intf *intf, uint8_t chan)
{
	struct lan_param *p;

	if (chan < 1 || chan > IPMI_CHANNEL_NUMBER_MAX) {
		lprintf(LOG_ERR, "Invalid Channel %d", chan);
		return -1;
	}

	/* find type of channel and only accept 802.3 LAN */
	if (!is_lan_channel(intf, chan)) {
		lprintf(LOG_ERR, "Channel %d is not a LAN channel", chan);
		return -1;
	}

	p = get_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS);
	if (!p)
		return -1;
	if (p->data) {
		printf("%-24s: ", p->desc);
		p->data[0] &= 3;
		switch (p->data[0]) {
		case 0:
			printf("Set Complete\n");
			break;
		case 1:
			printf("Set In Progress\n");
			break;
		case 2:
			printf("Commit Write\n");
			break;
		case 3:
			printf("Reserved\n");
			break;
		default:
			printf("Unknown\n");
		}
	}

	p = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE);
	if (!p)
		return -1;
	if (p->data) {
		printf("%-24s: %s%s%s%s%s\n", p->desc,
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
	}

	p = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE);
	if (!p)
		return -1;
	if (p->data) {
		printf("%-24s: Callback : %s%s%s%s%s\n", p->desc,
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
		printf("%-24s: User     : %s%s%s%s%s\n", "",
		       (p->data[1] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[1] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[1] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[1] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
		printf("%-24s: Operator : %s%s%s%s%s\n", "",
		       (p->data[2] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[2] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[2] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[2] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[2] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
		printf("%-24s: Admin    : %s%s%s%s%s\n", "",
		       (p->data[3] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[3] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[3] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[3] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[3] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
		printf("%-24s: OEM      : %s%s%s%s%s\n", "",
		       (p->data[4] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[4] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[4] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[4] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[4] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
	}

	p = get_lan_param(intf, chan, IPMI_LANP_IP_ADDR_SRC);
	if (!p)
		return -1;
	if (p->data) {
		printf("%-24s: ", p->desc);
		p->data[0] &= 0xf;
		switch (p->data[0]) {
		case 0:
			printf("Unspecified\n");
			break;
		case 1:
			printf("Static Address\n");
			break;
		case 2:
			printf("DHCP Address\n");
			break;
		case 3:
			printf("BIOS Assigned Address\n");
			break;
		default:
			printf("Other\n");
			break;
		}
	}

	p = get_lan_param(intf, chan, IPMI_LANP_IP_ADDR);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_SUBNET_MASK);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_MAC_ADDR);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %s\n", p->desc, mac2str(p->data));

	p = get_lan_param(intf, chan, IPMI_LANP_SNMP_STRING);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %s\n", p->desc, p->data);

	p = get_lan_param(intf, chan, IPMI_LANP_IP_HEADER);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: TTL=0x%02x Flags=0x%02x Precedence=0x%02x TOS=0x%02x\n",
		       p->desc, p->data[0], p->data[1] & 0xe0, p->data[2] & 0xe0, p->data[2] & 0x1e);

	p = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: ARP Responses %sabled, Gratuitous ARP %sabled\n", p->desc,
		       (p->data[0] & 2) ? "En" : "Dis", (p->data[0] & 1) ? "En" : "Dis");

	p = get_lan_param(intf, chan, IPMI_LANP_GRAT_ARP);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %.1f seconds\n", p->desc, (float)((p->data[0] + 1) / 2));

	p = get_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_IP);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_MAC);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %s\n", p->desc, mac2str(p->data));

	p = get_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_IP);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_MAC);
	if (!p)
		return -1;
	if (p->data)
		printf("%-24s: %s\n", p->desc, mac2str(p->data));

	p = get_lan_param(intf, chan, IPMI_LANP_VLAN_ID);
	if (p && p->data) {
		int id = ((p->data[1] & 0x0f) << 8) + p->data[0];
		if (p->data[1] & 0x80)
			printf("%-24s: %d\n", p->desc, id);
		else
			printf("%-24s: Disabled\n", p->desc);
	}

	p = get_lan_param(intf, chan, IPMI_LANP_VLAN_PRIORITY);
	if (p && p->data)
		printf("%-24s: %d\n", p->desc, p->data[0] & 0x07);

	/* Determine supported Cipher Suites -- Requires two calls */
	p = get_lan_param(intf, chan, IPMI_LANP_RMCP_CIPHER_SUPPORT);
	if (!p)
		return -1;
	else if (p->data)
	{
		unsigned char cipher_suite_count = p->data[0];
		p = get_lan_param(intf, chan, IPMI_LANP_RMCP_CIPHERS);
		if (!p)
			return -1;

		printf("%-24s: ", p->desc);

		/* Now we're dangerous.  There are only 15 fixed cipher
		   suite IDs, but the spec allows for 16 in the return data.*/
		if (p->data && p->data_len <= 17)
		{
			unsigned int i;
			for (i = 0; (i < 16) && (i < cipher_suite_count); ++i)
			{
				printf("%s%d",
				       (i > 0? ",": ""),
				       p->data[i + 1]);
			}
			printf("\n");
		}
		else
		{
			printf("None\n");
		}
	}

	/* RMCP+ Messaging Cipher Suite Privilege Levels */
	/* These are the privilege levels for the 15 fixed cipher suites */
	p = get_lan_param(intf, chan, IPMI_LANP_RMCP_PRIV_LEVELS);
	if (!p)
		return -1;
	if (p->data && 9 == p->data_len)
	{
		printf("%-24s: %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n", p->desc,
		       priv_level_to_char(p->data[1] & 0x0F),
		       priv_level_to_char(p->data[1] >> 4),
		       priv_level_to_char(p->data[2] & 0x0F),
		       priv_level_to_char(p->data[2] >> 4),
		       priv_level_to_char(p->data[3] & 0x0F),
		       priv_level_to_char(p->data[3] >> 4),
		       priv_level_to_char(p->data[4] & 0x0F),
		       priv_level_to_char(p->data[4] >> 4),
		       priv_level_to_char(p->data[5] & 0x0F),
		       priv_level_to_char(p->data[5] >> 4),
		       priv_level_to_char(p->data[6] & 0x0F),
		       priv_level_to_char(p->data[6] >> 4),
		       priv_level_to_char(p->data[7] & 0x0F),
		       priv_level_to_char(p->data[7] >> 4),
		       priv_level_to_char(p->data[8] & 0x0F));

		/* Now print a legend */
		printf("%-24s: %s\n", "", "    X=Cipher Suite Unused");
		printf("%-24s: %s\n", "", "    c=CALLBACK");
		printf("%-24s: %s\n", "", "    u=USER");
		printf("%-24s: %s\n", "", "    o=OPERATOR");
		printf("%-24s: %s\n", "", "    a=ADMIN");
		printf("%-24s: %s\n", "", "    O=OEM");
	}
	else
		printf("%-24s: Not Available\n", p->desc);

	/* Bad Password Threshold */
	p = get_lan_param(intf, chan, IPMI_LANP_BAD_PASS_THRESH);
	if (!p)
		return -1;
	if (p->data && 6 == p->data_len) {
		int tmp;

		printf("%-24s: %d\n", p->desc, p->data[1]);
		printf("%-24s: %s\n", "Invalid password disable",
				p->data[0] & 1 ? "yes" : "no" );
		tmp = p->data[2] + (p->data[3] << 8);
		printf("%-24s: %d\n", "Attempt Count Reset Int.", tmp * 10);
		tmp = p->data[4] + (p->data[5] << 8);
		printf("%-24s: %d\n", "User Lockout Interval", tmp * 10);
	} else {
		printf("%-24s: Not Available\n", p->desc);
	}

	return 0;
}

/* Configure Authentication Types */
/* TODO - probably some code duplication going on ??? */
static int
ipmi_lan_set_auth(struct ipmi_intf *intf, uint8_t chan, char *level, char *types)
{
	uint8_t data[5];
	uint8_t authtype = 0;
	char *p;
	struct lan_param *lp;

	if (!level || !types)
		return -1;

	lp = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE);
	if (!lp)
		return -1;
	if (!lp->data)
		return -1;

	lprintf(LOG_DEBUG, "%-24s: callback=0x%02x user=0x%02x operator=0x%02x admin=0x%02x oem=0x%02x",
		lp->desc, lp->data[0], lp->data[1], lp->data[2], lp->data[3], lp->data[4]);

	memset(data, 0, 5);
	memcpy(data, lp->data, 5);

	p = types;
	while (p) {
		if (strcasecmp(p, "none") == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_NONE;
		else if (strcasecmp(p, "md2") == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_MD2;
		else if (strcasecmp(p, "md5") == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_MD5;
		else if ((strcasecmp(p, "password") == 0) ||
			 (strcasecmp(p, "key") == 0))
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_KEY;
		else if (strcasecmp(p, "oem") == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_OEM;
		else
			lprintf(LOG_WARNING, "Invalid authentication type: %s", p);
		p = strchr(p, ',');
		if (p)
			p++;
	}

	p = level;
	while (p) {
		if (strcasecmp(p, "callback") == 0)
			data[0] = authtype;
		else if (strcasecmp(p, "user") == 0)
			data[1] = authtype;
		else if (strcasecmp(p, "operator") == 0)
			data[2] = authtype;
		else if (strcasecmp(p, "admin") == 0)
			data[3] = authtype;
		else
			lprintf(LOG_WARNING, "Invalid authentication level: %s", p);
		p = strchr(p, ',');
		if (p)
			p++;
	}

	if (verbose > 1)
		printbuf(data, 5, "authtype data");

	return set_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE, data, 5);
}

static int
ipmi_lan_set_password(struct ipmi_intf *intf,
		uint8_t user_id, const char *password)
{
	int ccode = 0;
	ccode = _ipmi_set_user_password(intf, user_id,
			IPMI_PASSWORD_SET_PASSWORD, password, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Unable to Set LAN Password for user %d",
				user_id);
		return (-1);
	}
	/* adjust our session password
	 * or we will no longer be able to communicate with BMC
	 */
	ipmi_intf_session_set_password(intf, (char *)password);
	printf("Password %s for user %d\n",
	       password ? "set" : "cleared", user_id);

	return 0;
}

/* ipmi_set_alert_enable - enable/disable PEF alerting for given channel.
 *
 * @channel - IPMI channel
 * @enable - whether to enable/disable PEF alerting for given channel
 * 
 * returns - 0 on success, (-1) on error.
 */
static int
ipmi_set_alert_enable(struct ipmi_intf *intf, uint8_t channel, uint8_t enable)
{
	struct channel_access_t channel_access;
	int ccode = 0;
	memset(&channel_access, 0, sizeof(channel_access));
	channel_access.channel = channel;
	ccode = _ipmi_get_channel_access(intf, &channel_access, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Get Channel Access(non-volatile) for channel %d",
				channel);
		return (-1);
	}
	if (enable != 0) {
		channel_access.alerting = 1;
	} else {
		channel_access.alerting = 0;
	}
	/* non-volatile */
	ccode = _ipmi_set_channel_access(intf, channel_access, 1, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Set Channel Access(non-volatile) for channel %d",
				channel);
		return (-1);
	}
	/* volatile */
	ccode = _ipmi_set_channel_access(intf, channel_access, 2, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Set Channel Access(volatile) for channel %d",
				channel);
		return (-1);
	}
	printf("PEF alerts for channel %d %s.\n",
			channel,
			(enable) ? "enabled" : "disabled");
	return 0;
}

/* ipmi_set_channel_access - enable/disable IPMI messaging for given channel and
 * set Privilege Level to Administrator.
 *
 * @channel - IPMI channel
 * @enable - whether to enable/disable IPMI messaging for given channel.
 *
 * returns - 0 on success, (-1) on error
 */
static int
ipmi_set_channel_access(struct ipmi_intf *intf, uint8_t channel,
		uint8_t enable)
{
	struct channel_access_t channel_access;
	int ccode = 0;
	memset(&channel_access, 0, sizeof(channel_access));
	channel_access.channel = channel;
	/* Get Non-Volatile Channel Access first */
	ccode = _ipmi_get_channel_access(intf, &channel_access, 0);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Get Channel Access(non-volatile) for channel %d",
				channel);
		return (-1);
	}

	if (enable != 0) {
		channel_access.access_mode = 2;
	} else {
		channel_access.access_mode = 0;
	}
	channel_access.privilege_limit = 0x04;
	ccode = _ipmi_set_channel_access(intf, channel_access, 1, 1);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Set Channel Access(non-volatile) for channel %d",
				channel);
		return (-1);
	}

	memset(&channel_access, 0, sizeof(channel_access));
	channel_access.channel = channel;
	/* Get Volatile Channel Access */
	ccode = _ipmi_get_channel_access(intf, &channel_access, 1);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Get Channel Access(volatile) for channel %d",
				channel);
		return (-1);
	}

	if (enable != 0) {
		channel_access.access_mode = 2;
	} else {
		channel_access.access_mode = 0;
	}
	channel_access.privilege_limit = 0x04;
	ccode = _ipmi_set_channel_access(intf, channel_access, 2, 2);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR,
				"Unable to Set Channel Access(volatile) for channel %d",
				channel);
		return (-1);
	}

	/* can't send close session if access off so abort instead */
	if (enable == 0) {
		intf->abort = 1;
	}
	printf("Set Channel Access for channel %d was successful.\n",
			channel);
	return 0;
}

/* ipmi_set_user_access - set admin access for given user and channel.
 *
 * @intf - IPMI interface
 * @channel - IPMI channel
 * @user_id - IPMI User ID
 *
 * returns - 0 on success, (-1) on error.
 */
static int
ipmi_set_user_access(struct ipmi_intf *intf, uint8_t channel, uint8_t user_id)
{
	struct user_access_t user_access;
	int ccode = 0;
	memset(&user_access, 0, sizeof(user_access));
	user_access.channel = channel;
	user_access.user_id = user_id;
	user_access.privilege_limit = 0x04;

	ccode = _ipmi_set_user_access(intf, &user_access, 1);
	if (eval_ccode(ccode) != 0) {
		lprintf(LOG_ERR, "Set User Access for channel %d failed",
				channel);
		return (-1);
	} else {
		printf("Set User Access for channel %d was successful.",
				channel);
		return 0;
	}
}



static int
get_cmdline_cipher_suite_priv_data(char *arg, uint8_t *buf)
{
	int i, ret = 0;

	if (strlen(arg) != 15)
	{
		lprintf(LOG_ERR, "Invalid privilege specification length: %d",
			strlen(arg));
		return -1;
	}

	/*
	 * The first byte is reserved (0).  The rest of the buffer is setup
	 * so that each nibble holds the maximum privilege level available for
	 * that cipher suite number.  The number of nibbles (15) matches the number
	 * of fixed cipher suite IDs.  This command documentation mentions 16 IDs
	 * but table 22-19 shows that there are only 15 (0-14).
	 *
	 * data 1 - reserved
	 * data 2 - maximum priv level for first (LSN) and second (MSN) ciphers
	 * data 3 - maximum priv level for third (LSN) and fourth (MSN) ciphers
	 * data 9 - maximum priv level for 15th (LSN) cipher.
	 */
	memset(buf, 0, 9);
	for (i = 0; i < 15; ++i)
	{
		unsigned char priv_level = IPMI_SESSION_PRIV_ADMIN;

		switch (arg[i])
		{
		case 'X':
			priv_level = IPMI_SESSION_PRIV_UNSPECIFIED; /* 0 */
			break;
		case 'c':
			priv_level = IPMI_SESSION_PRIV_CALLBACK;    /* 1 */
			break;
		case 'u':
			priv_level = IPMI_SESSION_PRIV_USER;        /* 2 */
			break;
		case 'o':
			priv_level = IPMI_SESSION_PRIV_OPERATOR;    /* 3 */
			break;
		case 'a':
			priv_level = IPMI_SESSION_PRIV_ADMIN;       /* 4 */
			break;
		case 'O':
			priv_level = IPMI_SESSION_PRIV_OEM;         /* 5 */
			break;
		default:
			lprintf(LOG_ERR, "Invalid privilege specification char: %c",
				arg[i]);
			ret = -1;
			break;
		}

		if (ret != 0)
			break;
		else
		{
			if ((i + 1) % 2)
			{
				// Odd number cipher suites will be in the LSN
				buf[1 + (i / 2)] += priv_level;
			}
			else
			{
				// Even number cipher suites will be in the MSN
				buf[1 + (i / 2)] += (priv_level << 4);
			}
		}
	}

	return ret;
}


static int
get_cmdline_ipaddr(char *arg, uint8_t *buf)
{
	uint32_t ip1, ip2, ip3, ip4;
	if (sscanf(arg,
				"%" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32,
				&ip1, &ip2, &ip3, &ip4) != 4) {
		lprintf(LOG_ERR, "Invalid IP address: %s", arg);
		return (-1);
	}
	if (ip1 > UINT8_MAX || ip2 > UINT8_MAX
			|| ip3 > UINT8_MAX || ip4 > UINT8_MAX) {
		lprintf(LOG_ERR, "Invalid IP address: %s", arg);
		return (-1);
	}
	buf[0] = (uint8_t)ip1;
	buf[1] = (uint8_t)ip2;
	buf[2] = (uint8_t)ip3;
	buf[3] = (uint8_t)ip4;
	return 0;
}

static int
ipmi_lan_set_vlan_id(struct ipmi_intf *intf,  uint8_t chan, char *string)
{
	struct lan_param *p;
	uint8_t data[2];
	int rc = -1;

	if (!string) { /* request to disable VLAN */
		lprintf(LOG_DEBUG, "Get current VLAN ID from BMC.");
		p = get_lan_param(intf, chan, IPMI_LANP_VLAN_ID);
		if (p && p->data && p->data_len > 1) {
			int id = ((p->data[1] & 0x0f) << 8) + p->data[0];
			if (IPMI_LANP_VLAN_DISABLE == id) {
				printf("VLAN is already disabled for channel %"
				       PRIu8 "\n", chan);
				rc = 0;
				goto out;
			}
			if (!IPMI_LANP_IS_VLAN_VALID(id)) {
				lprintf(LOG_ERR,
				        "Retrieved VLAN ID %i is out of "
				        "range <%d..%d>.",
				        id,
				        IPMI_LANP_VLAN_ID_MIN,
				        IPMI_LANP_VLAN_ID_MAX);
				goto out;
			}
			data[0] = p->data[0];
			data[1] = p->data[1] & 0x0F;
		} else {
			data[0] = 0;
			data[1] = 0;
		}
	}
	else {
		int id = 0;
		if (str2int(string, &id) != 0) {
			lprintf(LOG_ERR,
			        "Given VLAN ID '%s' is invalid.",
			        string);
			goto out;
		}

		if (!IPMI_LANP_IS_VLAN_VALID(id)) {
			lprintf(LOG_NOTICE,
			        "VLAN ID must be between %d and %d.",
			        IPMI_LANP_VLAN_ID_MIN,
			        IPMI_LANP_VLAN_ID_MAX);
			goto out;
		}
		else {
			data[0] = (uint8_t)id;
			data[1] = (uint8_t)(id >> 8) | 0x80;
		}
	}
	rc = set_lan_param(intf, chan, IPMI_LANP_VLAN_ID, data, 2);

out:
	return rc;
}

static int
ipmi_lan_set_vlan_priority(struct ipmi_intf *intf,  uint8_t chan, char *string)
{
	uint8_t data;
	int rc;
	int priority = 0;
	if (str2int(string, &priority) != 0) {
		lprintf(LOG_ERR, "Given VLAN priority '%s' is invalid.", string);
		return (-1);
	}

	if (priority < 0 || priority > 7) {
		lprintf(LOG_NOTICE, "VLAN priority must be between 0 and 7.");
		return (-1);
	}
	data = (uint8_t)priority;
	rc = set_lan_param(intf, chan, IPMI_LANP_VLAN_PRIORITY, &data, 1);
	return rc;
}

static void
print_lan_set_bad_pass_thresh_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> bad_pass_thresh <thresh_num> <1|0> <reset_interval> <lockout_interval>\n"
"        <thresh_num>         Bad Password Threshold number.\n"
"        <1|0>                1 = generate a Session Audit sensor event.\n"
"                             0 = do not generate an event.\n"
"        <reset_interval>     Attempt Count Reset Interval. In tens of seconds.\n"
"        <lockount_interval>  User Lockout Interval. In tens of seconds.");
}

/* get_cmdline_bad_pass_thresh - parse-out bad password threshold from given
 * string and store it into buffer.
 *
 * @arg: string to be parsed.
 * @buf: buffer of 6 to hold parsed Bad Password Threshold.
 *
 * returns zero on success, (-1) on error.
 */
static int
get_cmdline_bad_pass_thresh(char *argv[], uint8_t *buf)
{
	uint16_t reset, lockout;

	if (str2uchar(argv[0], &buf[1])) {
		return -1;
	}

	if (str2uchar(argv[1], &buf[0]) || buf[0] > 1) {
		return -1;
	}

	if (str2ushort(argv[2], &reset)) {
		return -1;
	}

	if (str2ushort(argv[3], &lockout)) {
		return -1;
	}

	/* store parsed data */
	buf[2] = reset & 0xFF;
	buf[3] = reset >> 8;
	buf[4] = lockout & 0xFF;
	buf[5] = lockout >> 8;
	return 0;
}

static int
ipmi_lan_set(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t data[32];
	uint8_t chan;
	int rc = 0;

	if (argc < 2) {
		print_lan_set_usage();
		return (-1);
	}

	if (!strcmp(argv[0], "help")
	    || !strcmp(argv[1], "help"))
	{
		print_lan_set_usage();
		return 0;
	}
	
	if (str2uchar(argv[0], &chan) != 0) {
		lprintf(LOG_ERR, "Invalid channel: %s", argv[0]);
		return (-1);
	}

	/* find type of channel and only accept 802.3 LAN */
	if (!is_lan_channel(intf, chan)) {
		lprintf(LOG_ERR, "Channel %d is not a LAN channel!", chan);
		print_lan_set_usage();
		return -1;
	}

	memset(&data, 0, sizeof(data));

	/* set user access */
	if (!strcmp(argv[1], "user")) {
		rc = ipmi_set_user_access(intf, chan, 1);
	}
	/* set channel access mode */
	else if (!strcmp(argv[1], "access")) {
		if (argc < 3) {
			print_lan_set_access_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_access_usage();
			return 0;
		}
		else if (!strcmp(argv[2], "on")) {
			rc = ipmi_set_channel_access(intf, chan, 1);
		}
		else if (!strcmp(argv[2], "off")) {
			rc = ipmi_set_channel_access(intf, chan, 0);
		}
		else {
			print_lan_set_access_usage();
			return (-1);
		}
	}
	/* set ARP control */
	else if (!strcmp(argv[1], "arp")) {
		if (argc < 3) {
			print_lan_set_arp_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_arp_usage();
		}
		else if (!strcmp(argv[2], "interval")) {
			uint8_t interval = 0;
			if (str2uchar(argv[3], &interval) != 0) {
				lprintf(LOG_ERR, "Given ARP interval '%s' is invalid.", argv[3]);
				return (-1);
			}
			rc = lan_set_arp_interval(intf, chan, interval);
		}
		else if (!strcmp(argv[2], "generate")) {
			if (argc < 4) {
				print_lan_set_arp_usage();
				return (-1);
			}
			else if (!strcmp(argv[3], "on"))
				rc = lan_set_arp_generate(intf, chan, 1);
			else if (!strcmp(argv[3], "off"))
				rc = lan_set_arp_generate(intf, chan, 0);
			else {
				print_lan_set_arp_usage();
				return (-1);
			}
		}
		else if (!strcmp(argv[2], "respond")) {
			if (argc < 4) {
				print_lan_set_arp_usage();
				return (-1);
			}
			else if (!strcmp(argv[3], "on"))
				rc = lan_set_arp_respond(intf, chan, 1);
			else if (!strcmp(argv[3], "off"))
				rc = lan_set_arp_respond(intf, chan, 0);
			else {
				print_lan_set_arp_usage();
				return (-1);
			}
		}
		else {
			print_lan_set_arp_usage();
		}
	}
	/* set authentication types */
	else if (!strcmp(argv[1], "auth")) {
		if (argc < 3) {
			print_lan_set_auth_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_auth_usage();
			return 0;
		} else {
			rc = ipmi_lan_set_auth(intf, chan, argv[2], argv[3]);
		}
	}
	/* ip address source */
	else if (!strcmp(argv[1], "ipsrc")) {
		if (argc < 3) {
			print_lan_set_ipsrc_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_ipsrc_usage();
			return 0;
		}
		else if (!strcmp(argv[2], "none"))
			data[0] = 0;
		else if (!strcmp(argv[2], "static"))
			data[0] = 1;
		else if (!strcmp(argv[2], "dhcp"))
			data[0] = 2;
		else if (!strcmp(argv[2], "bios"))
			data[0] = 3;
		else {
			print_lan_set_ipsrc_usage();
			return -1;
		}
		rc = set_lan_param(intf, chan, IPMI_LANP_IP_ADDR_SRC, data, 1);
	}
	/* session password
	 * not strictly a lan setting, but its used for lan connections */
	else if (!strcmp(argv[1], "password")) {
		rc = ipmi_lan_set_password(intf, 1, argv[2]);
	}
	/* snmp community string */
	else if (!strcmp(argv[1], "snmp")) {
		if (argc < 3) {
			print_lan_set_snmp_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_snmp_usage();
			return 0;
		} else {
			memcpy(data, argv[2], __min(strlen(argv[2]), 18));
			printf("Setting LAN %s to %s\n",
			       ipmi_lan_params[IPMI_LANP_SNMP_STRING].desc, data);
			rc = set_lan_param(intf, chan, IPMI_LANP_SNMP_STRING, data, 18);
		}
	}
	/* ip address */
	else if (!strcmp(argv[1], "ipaddr")) {
		if(argc != 3)
		{
			print_lan_set_usage();
			return -1;
		}
		rc = get_cmdline_ipaddr(argv[2], data);
		if (rc == 0) {
			printf("Setting LAN %s to %d.%d.%d.%d\n",
				ipmi_lan_params[IPMI_LANP_IP_ADDR].desc,
				data[0], data[1], data[2], data[3]);
			rc = set_lan_param(intf, chan, IPMI_LANP_IP_ADDR, data, 4);
		}
	}
	/* network mask */
	else if (!strcmp(argv[1], "netmask")) {
		if(argc != 3)
		{
			print_lan_set_usage();
			return -1;
		}
		rc = get_cmdline_ipaddr(argv[2], data);
		if (rc == 0) {
			printf("Setting LAN %s to %d.%d.%d.%d\n",
		       		ipmi_lan_params[IPMI_LANP_SUBNET_MASK].desc,
		       		data[0], data[1], data[2], data[3]);
			rc = set_lan_param(intf, chan, IPMI_LANP_SUBNET_MASK, data, 4);
		}
	}
	/* mac address */
	else if (!strcmp(argv[1], "macaddr")) {
		if(argc != 3)
		{
			print_lan_set_usage();
			return -1;
		}
		rc = str2mac(argv[2], data);
		if (rc == 0) {
			printf("Setting LAN %s to %s\n",
				ipmi_lan_params[IPMI_LANP_MAC_ADDR].desc,
				mac2str(data));
			rc = set_lan_param(intf, chan, IPMI_LANP_MAC_ADDR, data, 6);
		}
	}
	/* default gateway settings */
	else if (!strcmp(argv[1], "defgw")) {
		if (argc < 4) {
			print_lan_set_defgw_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_defgw_usage();
			return 0;
		}
		else if (!strcmp(argv[2], "ipaddr")
		         && !get_cmdline_ipaddr(argv[3], data))
		{
			printf("Setting LAN %s to %d.%d.%d.%d\n",
			       ipmi_lan_params[IPMI_LANP_DEF_GATEWAY_IP].desc,
			       data[0], data[1], data[2], data[3]);
			rc = set_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_IP, data, 4);
		}
		else if (!strcmp(argv[2], "macaddr")
		         && !str2mac(argv[3], data))
		{
			printf("Setting LAN %s to %s\n",
				ipmi_lan_params[IPMI_LANP_DEF_GATEWAY_MAC].desc,
				mac2str(data));
			rc = set_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_MAC, data, 6);
		}
		else {
			print_lan_set_usage();
			return -1;
		}
	}
	/* backup gateway settings */
	else if (!strcmp(argv[1], "bakgw")) {
		if (argc < 4) {
			print_lan_set_bakgw_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_bakgw_usage();
			return 0;
		}
		else if (!strcmp(argv[2], "ipaddr")
		         && !get_cmdline_ipaddr(argv[3], data))
		{
			printf("Setting LAN %s to %d.%d.%d.%d\n",
			       ipmi_lan_params[IPMI_LANP_BAK_GATEWAY_IP].desc,
			       data[0], data[1], data[2], data[3]);
			rc = set_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_IP, data, 4);
		}
		else if (!strcmp(argv[2], "macaddr")
		         && !str2mac(argv[3], data)) {
			printf("Setting LAN %s to %s\n",
				ipmi_lan_params[IPMI_LANP_BAK_GATEWAY_MAC].desc,
				mac2str(data));
			rc = set_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_MAC, data, 6);
		}
		else {
			print_lan_set_usage();
			return -1;
		}
	}
	else if (!strcmp(argv[1], "vlan")) {
		if (argc < 4) {
			print_lan_set_vlan_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")) {
			print_lan_set_vlan_usage();
			return 0;
		}
		else if (!strcmp(argv[2], "id")) {
			if (!strcmp(argv[3], "off")) {
				ipmi_lan_set_vlan_id(intf, chan, NULL);
			}
			else {
				ipmi_lan_set_vlan_id(intf, chan, argv[3]);
			}
		}
		else if (!strcmp(argv[2], "priority")) {
			ipmi_lan_set_vlan_priority(intf, chan, argv[3]);
		}
		else {
			print_lan_set_vlan_usage();
			return (-1);
		}
	}
	/* set PEF alerting on or off */
	else if (!strcmp(argv[1], "alert")) {
		if (argc < 3) {
			lprintf(LOG_NOTICE, "LAN set alert must be 'on' or 'off'");
			return (-1);
		}
		else if (!strcmp(argv[2], "on") ||
			 !strcmp(argv[2], "enable")) {
			printf("Enabling PEF alerts for LAN channel %d\n", chan);
			rc = ipmi_set_alert_enable(intf, chan, 1);
		}
		else if (!strcmp(argv[2], "off") ||
			 !strcmp(argv[2], "disable")) {
			printf("Disabling PEF alerts for LAN channel %d\n", chan);
			rc = ipmi_set_alert_enable(intf, chan, 0);
		}
		else {
			lprintf(LOG_NOTICE, "LAN set alert must be 'on' or 'off'");
			return 0;
		}
	}
	/* RMCP+ cipher suite privilege levels */
	else if (!strcmp(argv[1], "cipher_privs"))
	{
		if (argc != 3) {
			print_lan_set_cipher_privs_usage();
			return (-1);
		}
		else if (!strcmp(argv[2], "help")
		         || get_cmdline_cipher_suite_priv_data(argv[2], data))
		{
			print_lan_set_cipher_privs_usage();
			return 0;
		}
		else
		{
			rc = set_lan_param(intf, chan, IPMI_LANP_RMCP_PRIV_LEVELS, data, 9);
		}
	}
	else if (!strcmp(argv[1], "bad_pass_thresh"))
	{
		if (argc == 3 && !strcmp(argv[2], "help")) {
			print_lan_set_bad_pass_thresh_usage();
			return 0;
		}
		if (argc < 6 || get_cmdline_bad_pass_thresh(&argv[2], data)) {
			print_lan_set_bad_pass_thresh_usage();
			return (-1);
		}
		rc = set_lan_param(intf, chan, IPMI_LANP_BAD_PASS_THRESH, data, 6);
	}
	else {
		print_lan_set_usage();
		return (-1);
	}

	return rc;
}


static int
is_alert_destination(struct ipmi_intf *intf, uint8_t channel, uint8_t alert)
{
	struct lan_param *p;

	p = get_lan_param(intf, channel, IPMI_LANP_NUM_DEST);
	if (!p)
		return 0;
	if (!p->data)
		return 0;

	if (alert <= (p->data[0] & 0xf))
		return 1;
	else
		return 0;
}

static int
ipmi_lan_alert_print(struct ipmi_intf *intf, uint8_t channel, uint8_t alert)
{
# define PTYPE_LEN	4
# define PADDR_LEN	13
	struct lan_param *lp_ptr = NULL;
	int isack = 0;
	uint8_t ptype[PTYPE_LEN];
	uint8_t paddr[PADDR_LEN];

	lp_ptr = get_lan_param_select(intf, channel, IPMI_LANP_DEST_TYPE, alert);
	if (!lp_ptr || !lp_ptr->data
			|| lp_ptr->data_len < PTYPE_LEN) {
		return (-1);
	}
	memcpy(ptype, lp_ptr->data, PTYPE_LEN);

	lp_ptr = get_lan_param_select(intf, channel, IPMI_LANP_DEST_ADDR, alert);
	if (!lp_ptr || !lp_ptr->data || lp_ptr->data_len < PADDR_LEN) {
		return (-1);
	}
	memcpy(paddr, lp_ptr->data, PADDR_LEN);

	printf("%-24s: %d\n", "Alert Destination",
			ptype[0]);

	if (ptype[1] & 0x80) {
		isack = 1;
	}
	printf("%-24s: %s\n", "Alert Acknowledge",
			isack ? "Acknowledged" : "Unacknowledged");

	printf("%-24s: ", "Destination Type");
	switch (ptype[1] & 0x7) {
	case 0:
		printf("PET Trap\n");
		break;
	case 6:
		printf("OEM 1\n");
		break;
	case 7:
		printf("OEM 2\n");
		break;
	default:
		printf("Unknown\n");
		break;
	}

	printf("%-24s: %d\n",
			isack ? "Acknowledge Timeout" : "Retry Interval",
			ptype[2]);

	printf("%-24s: %d\n", "Number of Retries",
			ptype[3] & 0x7);

	if ((paddr[1] & 0xf0) != 0) {
		/* unknown address format */
		printf("\n");
		return 0;
	}

	printf("%-24s: %s\n", "Alert Gateway",
			(paddr[2] & 1) ? "Backup" : "Default");

	printf("%-24s: %d.%d.%d.%d\n", "Alert IP Address",
			paddr[3], paddr[4], paddr[5], paddr[6]);

	printf("%-24s: %s\n", "Alert MAC Address",
			mac2str(&paddr[7]));

	printf("\n");
	return 0;
}

static int
ipmi_lan_alert_print_all(struct ipmi_intf *intf, uint8_t channel)
{
	int j, ndest;
	struct lan_param *p;

	p = get_lan_param(intf, channel, IPMI_LANP_NUM_DEST);
	if (!p)
		return -1;
	if (!p->data)
		return -1;
	ndest = p->data[0] & 0xf;

	for (j=0; j<=ndest; j++) {
		ipmi_lan_alert_print(intf, channel, j);
	}

	return 0;
}

static int
ipmi_lan_alert_set(struct ipmi_intf *intf, uint8_t chan, uint8_t alert,
		   int argc, char **argv)
{
	struct lan_param *p;
	uint8_t data[32], temp[32];
	int rc = 0;

	if (argc < 2) {
		print_lan_alert_set_usage();
		return (-1);
	}

	if (!strcmp(argv[0], "help")
	    || !strcmp(argv[1], "help"))
	{
		print_lan_alert_set_usage();
		return 0;
	}

	memset(data, 0, sizeof(data));
	memset(temp, 0, sizeof(temp));

	/* alert destination ip address */
	if (strcasecmp(argv[0], "ipaddr") == 0 &&
	    (get_cmdline_ipaddr(argv[1], temp) == 0)) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_ADDR, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));
		/* set new ipaddr */
		memcpy(data+3, temp, 4);
		printf("Setting LAN Alert %d IP Address to %d.%d.%d.%d\n", alert,
		       data[3], data[4], data[5], data[6]);
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_ADDR, data, p->data_len);
	}
	/* alert destination mac address */
	else if (strcasecmp(argv[0], "macaddr") == 0 &&
		 (str2mac(argv[1], temp) == 0)) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_ADDR, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));
		/* set new macaddr */
		memcpy(data+7, temp, 6);
		printf("Setting LAN Alert %d MAC Address to "
		       "%s\n", alert, mac2str(&data[7]));
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_ADDR, data, p->data_len);
	}
	/* alert destination gateway selector */
	else if (strcasecmp(argv[0], "gateway") == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_ADDR, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));

		if (strcasecmp(argv[1], "def") == 0 ||
		    strcasecmp(argv[1], "default") == 0) {
			printf("Setting LAN Alert %d to use Default Gateway\n", alert);
			data[2] = 0;
		}
		else if (strcasecmp(argv[1], "bak") == 0 ||
			 strcasecmp(argv[1], "backup") == 0) {
			printf("Setting LAN Alert %d to use Backup Gateway\n", alert);
			data[2] = 1;
		}
		else {
			print_lan_alert_set_usage();
			return -1;
		}

		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_ADDR, data, p->data_len);
	}
	/* alert acknowledgement */
	else if (strcasecmp(argv[0], "ack") == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));

		if (strcasecmp(argv[1], "on") == 0 ||
		    strcasecmp(argv[1], "yes") == 0) {
			printf("Setting LAN Alert %d to Acknowledged\n", alert);
			data[1] |= 0x80;
		}
		else if (strcasecmp(argv[1], "off") == 0 ||
			 strcasecmp(argv[1], "no") == 0) {
			printf("Setting LAN Alert %d to Unacknowledged\n", alert);
			data[1] &= ~0x80;
		}
		else {
			print_lan_alert_set_usage();
			return -1;
		}
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	/* alert destination type */
	else if (strcasecmp(argv[0], "type") == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));

		if (strcasecmp(argv[1], "pet") == 0) {
			printf("Setting LAN Alert %d destination to PET Trap\n", alert);
			data[1] &= ~0x07;
		}
		else if (strcasecmp(argv[1], "oem1") == 0) {
			printf("Setting LAN Alert %d destination to OEM 1\n", alert);
			data[1] &= ~0x07;
			data[1] |= 0x06;
		}
		else if (strcasecmp(argv[1], "oem2") == 0) {
			printf("Setting LAN Alert %d destination to OEM 2\n", alert);
			data[1] |= 0x07;
		}
		else {
			print_lan_alert_set_usage();
			return -1;
		}
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	/* alert acknowledge timeout or retry interval */
	else if (strcasecmp(argv[0], "time") == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));

		if (str2uchar(argv[1], &data[2]) != 0) {
			lprintf(LOG_ERR, "Invalid time: %s", argv[1]);
			return (-1);
		}
		printf("Setting LAN Alert %d timeout/retry to %d seconds\n", alert, data[2]);
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	/* number of retries */
	else if (strcasecmp(argv[0], "retry") == 0) {
		/* get current parameter */
		p = get_lan_param_select(intf, chan, IPMI_LANP_DEST_TYPE, alert);
		if (!p) {
			return (-1);
		}
		memcpy(data, p->data, __min(p->data_len, sizeof(data)));

		if (str2uchar(argv[1], &data[3]) != 0) {
			lprintf(LOG_ERR, "Invalid retry: %s", argv[1]);
			return (-1);
		}
		data[3] = data[3] & 0x7;
		printf("Setting LAN Alert %d number of retries to %d\n", alert, data[3]);
		rc = set_lan_param_nowait(intf, chan, IPMI_LANP_DEST_TYPE, data, p->data_len);
	}
	else {
		print_lan_alert_set_usage();
		return -1;
	}

	return rc;
}

static int
ipmi_lan_alert(struct ipmi_intf *intf, int argc, char **argv)
{
	uint8_t alert;
	uint8_t channel = 1;

	if (argc < 1) {
		print_lan_alert_print_usage();
		print_lan_alert_set_usage();
		return (-1);
	}
	else if (strcasecmp(argv[0], "help") == 0) {
		print_lan_alert_print_usage();
		print_lan_alert_set_usage();
		return 0;
	}

	/* alert print [channel] [alert] */
	if (strcasecmp(argv[0], "print") == 0) {
		if (argc < 2) {
			channel = find_lan_channel(intf, 1);
			if (!is_lan_channel(intf, channel)) {
				lprintf(LOG_ERR, "Channel %d is not a LAN channel", channel);
				return -1;
			}
			return ipmi_lan_alert_print_all(intf, channel);
		}

		if (strcasecmp(argv[1], "help") == 0) {
			print_lan_alert_print_usage();
			return 0;
		}

		if (str2uchar(argv[1], &channel) != 0) {
			lprintf(LOG_ERR, "Invalid channel: %s", argv[1]);
			return (-1);
		}
		if (!is_lan_channel(intf, channel)) {
			lprintf(LOG_ERR, "Channel %d is not a LAN channel", channel);
			return -1;
		}

		if (argc < 3)
			return ipmi_lan_alert_print_all(intf, channel);

		if (str2uchar(argv[2], &alert) != 0) {
			lprintf(LOG_ERR, "Invalid alert: %s", argv[2]);
			return (-1);
		}
		if (is_alert_destination(intf, channel, alert) == 0) {
			lprintf(LOG_ERR, "Alert %d is not a valid destination", alert);
			return -1;
		}
		return ipmi_lan_alert_print(intf, channel, alert);
	}

	/* alert set <channel> <alert> [option] */
	if (strcasecmp(argv[0], "set") == 0) {
		if (argc < 5) {
			print_lan_alert_set_usage();
			return (-1);
		}
		else if (strcasecmp(argv[1], "help") == 0) {
			print_lan_alert_set_usage();
			return 0;
		}

		if (str2uchar(argv[1], &channel) != 0) {
			lprintf(LOG_ERR, "Invalid channel: %s", argv[1]);
			return (-1);
		}
		if (!is_lan_channel(intf, channel)) {
			lprintf(LOG_ERR, "Channel %d is not a LAN channel", channel);
			return -1;
		}

		if (str2uchar(argv[2], &alert) != 0) {
			lprintf(LOG_ERR, "Invalid alert: %s", argv[2]);
			return (-1);
		}
		if (is_alert_destination(intf, channel, alert) == 0) {
			lprintf(LOG_ERR, "Alert %d is not a valid destination", alert);
			return -1;
		}

		return ipmi_lan_alert_set(intf, channel, alert, argc-3, &(argv[3]));
	}

	return 0;
}


static int
ipmi_lan_stats_get(struct ipmi_intf *intf, uint8_t chan)
{
	int rc = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];
	uint16_t statsTemp;

	if (!is_lan_channel(intf, chan)) {
		lprintf(LOG_ERR, "Channel %d is not a LAN channel", chan);
		return -1;
	}

	/* From here, we are ready to get the stats */

	msg_data[0] = chan;
	msg_data[1] = 0;   /* Don't clear */

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = IPMI_LAN_GET_STAT;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_ERR, "Get LAN Stats command failed");
		return (-1);
	}

	if (rsp->ccode) {
		lprintf(LOG_ERR, "Get LAN Stats command failed: %s",
			specific_val2str(rsp->ccode,
			                 get_lan_cc_vals,
			                 completion_code_vals));
		return (-1);
	}

	if (verbose > 1) {
		uint8_t counter;
		printf("--- Rx Stats ---\n");
		for (counter=0; counter<18; counter+=2) {
			printf("%02X", *(rsp->data + counter));
			printf(" %02X - ", *(rsp->data + counter+1));
		}
		printf("\n");
	}

	statsTemp = ((*(rsp->data + 0)) << 8) | (*(rsp->data + 1));
	printf("IP Rx Packet              : %d\n", statsTemp);

	statsTemp = ((*(rsp->data + 2)) << 8) | (*(rsp->data + 3));
	printf("IP Rx Header Errors       : %u\n", statsTemp);

	statsTemp = ((*(rsp->data + 4)) << 8) | (*(rsp->data + 5));
	printf("IP Rx Address Errors      : %u\n", statsTemp);

	statsTemp = ((*(rsp->data + 6)) << 8) | (*(rsp->data + 7));
	printf("IP Rx Fragmented          : %u\n", statsTemp);

	statsTemp = ((*(rsp->data + 8)) << 8) | (*(rsp->data + 9));
	printf("IP Tx Packet              : %u\n", statsTemp);

	statsTemp = ((*(rsp->data +10)) << 8) | (*(rsp->data +11));
	printf("UDP Rx Packet             : %u\n", statsTemp);

	statsTemp = ((*(rsp->data + 12)) << 8) | (*(rsp->data + 13));
	printf("RMCP Rx Valid             : %u\n", statsTemp);

	statsTemp = ((*(rsp->data + 14)) << 8) | (*(rsp->data + 15));
	printf("UDP Proxy Packet Received : %u\n", statsTemp);

	statsTemp = ((*(rsp->data + 16)) << 8) | (*(rsp->data + 17));
	printf("UDP Proxy Packet Dropped  : %u\n", statsTemp);

	return rc;
}


static int
ipmi_lan_stats_clear(struct ipmi_intf *intf, uint8_t chan)
{
	int rc = 0;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t msg_data[2];

	if (!is_lan_channel(intf, chan)) {
		lprintf(LOG_ERR, "Channel %d is not a LAN channel", chan);
		return -1;
	}

	/* From here, we are ready to get the stats */
	msg_data[0] = chan;
	msg_data[1] = 1;   /* Clear */

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = IPMI_LAN_GET_STAT;
	req.msg.data     = msg_data;
	req.msg.data_len = 2;

	rsp = intf->sendrecv(intf, &req);
	if (!rsp) {
		lprintf(LOG_INFO, "Get LAN Stats command failed");
		return (-1);
	}

	if (rsp->ccode) {
		lprintf(LOG_INFO, "Get LAN Stats command failed: %s",
			specific_val2str(rsp->ccode,
			                 get_lan_cc_vals,
			                 completion_code_vals));
		return (-1);
	}

	return rc;
}

static void
print_lan_alert_print_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"usage: lan alert print [channel number] [alert destination]");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"Default will print all alerts for the first found LAN channel");
}

static void
print_lan_alert_set_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"usage: lan alert set <channel number> <alert destination> <command> <parameter>");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"    Command/parameter options:");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"    ipaddr <x.x.x.x>               Set alert IP address");
	lprintf(LOG_NOTICE,
"    macaddr <x:x:x:x:x:x>          Set alert MAC address");
	lprintf(LOG_NOTICE,
"    gateway <default|backup>       Set channel gateway to use for alerts");
	lprintf(LOG_NOTICE,
"    ack <on|off>                   Set Alert Acknowledge on or off");
	lprintf(LOG_NOTICE,
"    type <pet|oem1|oem2>           Set destination type as PET or OEM");
	lprintf(LOG_NOTICE,
"    time <seconds>                 Set ack timeout or unack retry interval");
	lprintf(LOG_NOTICE,
"    retry <number>                 Set number of alert retries");
	lprintf(LOG_NOTICE,
"");
}

static void
print_lan_set_usage(void)
{
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"usage: lan set <channel> <command> <parameter>");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"LAN set command/parameter options:");
	lprintf(LOG_NOTICE,
"  ipaddr <x.x.x.x>               Set channel IP address");
	lprintf(LOG_NOTICE,
"  netmask <x.x.x.x>              Set channel IP netmask");
	lprintf(LOG_NOTICE,
"  macaddr <x:x:x:x:x:x>          Set channel MAC address");
	lprintf(LOG_NOTICE,
"  defgw ipaddr <x.x.x.x>         Set default gateway IP address");
	lprintf(LOG_NOTICE,
"  defgw macaddr <x:x:x:x:x:x>    Set default gateway MAC address");
	lprintf(LOG_NOTICE,
"  bakgw ipaddr <x.x.x.x>         Set backup gateway IP address");
	lprintf(LOG_NOTICE,
"  bakgw macaddr <x:x:x:x:x:x>    Set backup gateway MAC address");
	lprintf(LOG_NOTICE,
"  password <password>            Set session password for this channel");
	lprintf(LOG_NOTICE,
"  snmp <community string>        Set SNMP public community string");
	lprintf(LOG_NOTICE,
"  user                           Enable default user for this channel");
	lprintf(LOG_NOTICE,
"  access <on|off>                Enable or disable access to this channel");
	lprintf(LOG_NOTICE,
"  alert <on|off>                 Enable or disable PEF alerting for this channel");
	lprintf(LOG_NOTICE,
"  arp respond <on|off>           Enable or disable BMC ARP responding");
	lprintf(LOG_NOTICE,
"  arp generate <on|off>          Enable or disable BMC gratuitous ARP generation");
	lprintf(LOG_NOTICE,
"  arp interval <seconds>         Set gratuitous ARP generation interval");
	lprintf(LOG_NOTICE,
"  vlan id <off|<id>>             Disable or enable VLAN and set ID (1-4094)");
	lprintf(LOG_NOTICE,
"  vlan priority <priority>       Set vlan priority (0-7)");
	lprintf(LOG_NOTICE,
"  auth <level> <type,..>         Set channel authentication types");
	lprintf(LOG_NOTICE,
"    level  = CALLBACK, USER, OPERATOR, ADMIN");
	lprintf(LOG_NOTICE,
"    type   = NONE, MD2, MD5, PASSWORD, OEM");
	lprintf(LOG_NOTICE,
"  ipsrc <source>                 Set IP Address source");
	lprintf(LOG_NOTICE,
"    none   = unspecified source");
	lprintf(LOG_NOTICE,
"    static = address manually configured to be static");
	lprintf(LOG_NOTICE,
"    dhcp   = address obtained by BMC running DHCP");
	lprintf(LOG_NOTICE,
"    bios   = address loaded by BIOS or system software");
	lprintf(LOG_NOTICE,
"  cipher_privs XXXXXXXXXXXXXXX   Set RMCP+ cipher suite privilege levels");
	lprintf(LOG_NOTICE,
"    X = Cipher Suite Unused");
	lprintf(LOG_NOTICE,
"    c = CALLBACK");
	lprintf(LOG_NOTICE,
"    u = USER");
	lprintf(LOG_NOTICE,
"    o = OPERATOR");
	lprintf(LOG_NOTICE,
"    a = ADMIN");
	lprintf(LOG_NOTICE,
"    O = OEM");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"  bad_pass_thresh <thresh_num> <1|0> <reset_interval> <lockout_interval>\n"
"                                Set bad password threshold");
}

static void
print_lan_set_access_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set access <on|off>");
}

static void
print_lan_set_arp_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> arp respond <on|off>");
	lprintf(LOG_NOTICE,
"lan set <channel> arp generate <on|off>");
	lprintf(LOG_NOTICE,
"lan set <channel> arp interval <seconds>");
	lprintf(LOG_NOTICE,
"");
	lprintf(LOG_NOTICE,
"example: lan set 7 arp gratuitous off");
}

static void
print_lan_set_auth_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> auth <level> <type,type,...>");
	lprintf(LOG_NOTICE,
"  level = CALLBACK, USER, OPERATOR, ADMIN");
	lprintf(LOG_NOTICE,
"  types = NONE, MD2, MD5, PASSWORD, OEM");
	lprintf(LOG_NOTICE,
"example: lan set 7 auth ADMIN PASSWORD,MD5");
}

static void
print_lan_set_bakgw_usage(void)
{
	lprintf(LOG_NOTICE,
"LAN set backup gateway commands: ipaddr, macaddr");
}

static void
print_lan_set_cipher_privs_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> cipher_privs XXXXXXXXXXXXXXX");
	lprintf(LOG_NOTICE,
"    X = Cipher Suite Unused");
	lprintf(LOG_NOTICE,
"    c = CALLBACK");
	lprintf(LOG_NOTICE,
"    u = USER");
	lprintf(LOG_NOTICE,
"    o = OPERATOR");
	lprintf(LOG_NOTICE,
"    a = ADMIN");
	lprintf(LOG_NOTICE,
"    O = OEM");
	lprintf(LOG_NOTICE,
"");
}

static void
print_lan_set_defgw_usage(void)
{
	lprintf(LOG_NOTICE,
"LAN set default gateway Commands: ipaddr, macaddr");
}

static void
print_lan_set_ipsrc_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> ipsrc <source>");
	lprintf(LOG_NOTICE,
"  none   = unspecified");
	lprintf(LOG_NOTICE,
"  static = static address (manually configured)");
	lprintf(LOG_NOTICE,
"  dhcp   = address obtained by BMC running DHCP");
	lprintf(LOG_NOTICE,
"  bios   = address loaded by BIOS or system software");
}

static void
print_lan_set_snmp_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> snmp <community string>");
}

static void
print_lan_set_vlan_usage(void)
{
	lprintf(LOG_NOTICE,
"lan set <channel> vlan id <id>");
	lprintf(LOG_NOTICE,
"lan set <channel> vlan id off");
	lprintf(LOG_NOTICE,
"lan set <channel> vlan priority <priority>");
}

/*
 * print_lan_usage
 */
static void
print_lan_usage(void)
{
	lprintf(LOG_NOTICE,
"LAN Commands:");
	lprintf(LOG_NOTICE,
"		   print [<channel number>]");
	lprintf(LOG_NOTICE,
"		   set <channel number> <command> <parameter>");
	lprintf(LOG_NOTICE,
"		   alert print <channel number> <alert destination>");
	lprintf(LOG_NOTICE,
"		   alert set <channel number> <alert destination> <command> <parameter>");
	lprintf(LOG_NOTICE,
"		   stats get [<channel number>]");
	lprintf(LOG_NOTICE,
"		   stats clear [<channel number>]");
}


int
ipmi_lanp_main(struct ipmi_intf *intf, int argc, char **argv)
{
	int rc = 0;
	uint8_t chan = 0;

	if (argc == 0) {
		print_lan_usage();
		return (-1);
	} else if (!strcmp(argv[0], "help")) {
		print_lan_usage();
		return 0;
	}

	if (!strcmp(argv[0], "printconf")
	    || !strcmp(argv[0], "print")) 
	{
		if (argc > 2) {
			print_lan_usage();
			return (-1);
		} else if (argc == 2) {
			if (str2uchar(argv[1], &chan) != 0) {
				lprintf(LOG_ERR, "Invalid channel: %s", argv[1]);
				return (-1);
			}
		} else {
			chan = find_lan_channel(intf, 1);
		}
		if (!is_lan_channel(intf, chan)) {
			lprintf(LOG_ERR, "Invalid channel: %d", chan);
			return (-1);
		}
		rc = ipmi_lan_print(intf, chan);
	} else if (!strcmp(argv[0], "set")) {
		rc = ipmi_lan_set(intf, argc-1, &(argv[1]));
	} else if (!strcmp(argv[0], "alert")) {
		rc = ipmi_lan_alert(intf, argc-1, &(argv[1]));
	} else if (!strcmp(argv[0], "stats")) {
		if (argc < 2) {
			print_lan_usage();
			return (-1);
		} else if (argc == 3) {
			if (str2uchar(argv[2], &chan) != 0) {
				lprintf(LOG_ERR, "Invalid channel: %s", argv[2]);
				return (-1);
			}
		} else {
			chan = find_lan_channel(intf, 1);
		}
		if (!is_lan_channel(intf, chan)) {
			lprintf(LOG_ERR, "Invalid channel: %d", chan);
			return (-1);
		}
		if (!strcmp(argv[1], "get")) {
			rc = ipmi_lan_stats_get(intf, chan);
		} else if (!strcmp(argv[1], "clear")) {
			rc = ipmi_lan_stats_clear(intf, chan);
		} else {
			print_lan_usage();
			return (-1);
		}
	} else {
		lprintf(LOG_NOTICE, "Invalid LAN command: %s", argv[0]);
		return (-1);
	}
	return rc;
}
