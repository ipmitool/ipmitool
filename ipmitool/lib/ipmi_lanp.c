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

#include <ipmitool/ipmi.h>
#include <ipmitool/log.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/helper.h>
#include <ipmitool/ipmi_constants.h>
#include <ipmitool/ipmi_strings.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_channel.h>

extern int verbose;


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
get_lan_param(struct ipmi_intf * intf, uint8_t chan, int param)
{
	struct lan_param * p;
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t msg_data[4];

	p = &ipmi_lan_params[param];
	if (p == NULL)
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
	if (rsp == NULL) {
		lprintf(LOG_INFO, "Get LAN Parameter '%s' command failed", p->desc);
		return NULL;
	}

	switch (rsp->ccode)
	{
	case 0x00: /* successful */
		break;

	case 0x80: /* parameter not supported */
	case 0xc9: /* parameter out of range */
	case 0xcc: /* invalid data field in request */

		/* these completion codes usually mean parameter not supported */
		lprintf(LOG_INFO, "Get LAN Parameter '%s' command failed: %s",
			p->desc, val2str(rsp->ccode, completion_code_vals));
		p->data = NULL;
		p->data_len = 0;
		return p;

	default:

		/* other completion codes are treated as error */
		lprintf(LOG_INFO, "Get LAN Parameter '%s' command failed: %s",
			p->desc, val2str(rsp->ccode, completion_code_vals));
		return NULL;
	}

	p->data = rsp->data + 1;
	p->data_len = rsp->data_len - 1;

	return p;
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
set_lan_param_wait(struct ipmi_intf * intf, uint8_t chan,
		   int param, uint8_t * data, int len)
{
	struct lan_param * p;
	int retry = 10;		/* 10 retries */

	lprintf(LOG_DEBUG, "Waiting for Set LAN Parameter to complete...");
	if (verbose > 1)
		printbuf(data, len, "SET DATA");

	for (;;) {
		p = get_lan_param(intf, chan, param);
		if (p == NULL) {
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
__set_lan_param(struct ipmi_intf * intf, uint8_t chan,
		int param, uint8_t * data, int len, int wait)
{
	struct ipmi_rs * rsp;
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
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set LAN Parameter failed");
		return -1;
	}
	if ((rsp->ccode > 0) && (wait != 0)) {
		lprintf(LOG_DEBUG, "Warning: Set LAN Parameter failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		if (rsp->ccode == 0xcc) {
			/* retry hack for invalid data field ccode */
			int retry = 10;		/* 10 retries */
			lprintf(LOG_DEBUG, "Retrying...");
			for (;;) {
				if (retry-- == 0)
					break;
				sleep(IPMI_LANP_TIMEOUT);
				rsp = intf->sendrecv(intf, &req);
				if (rsp == NULL)
					continue;
				if (rsp->ccode > 0)
					continue;
				return set_lan_param_wait(intf, chan, param, data, len);
			}
		}
		else if (rsp->ccode != 0xff) {
			/* let 0xff ccode continue */
			return -1;
		}
	}

	if (wait == 0)
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
ipmi_lanp_lock_state(struct ipmi_intf * intf, uint8_t chan)
{
	struct lan_param * p;
	p = get_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS);
	if (p == NULL)
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
ipmi_lanp_lock(struct ipmi_intf * intf, uint8_t chan)
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
ipmi_lanp_unlock(struct ipmi_intf * intf, uint8_t chan)
{
	uint8_t val = IPMI_LANP_WRITE_COMMIT;
	int rc;

	rc = __set_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS, &val, 1, 0);
	if (rc < 0) {
		lprintf(LOG_DEBUG, "LAN Parameter Commit not supported");
		val = IPMI_LANP_WRITE_UNLOCK;
		__set_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS, &val, 0, 0);
	}
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
set_lan_param(struct ipmi_intf * intf, uint8_t chan,
	      int param, uint8_t * data, int len)
{
	int rc;
	ipmi_lanp_lock(intf, chan);
	rc = __set_lan_param(intf, chan, param, data, len, 1);
	ipmi_lanp_unlock(intf, chan);
	return rc;
}


static int
lan_set_arp_interval(struct ipmi_intf * intf,
		     uint8_t chan, uint8_t * ival)
{
	struct lan_param *lp;
	uint8_t interval;
	int rc = 0;
	
	lp = get_lan_param(intf, chan, IPMI_LANP_GRAT_ARP);
	if (lp == NULL)
		return -1;
	if (lp->data == NULL)
		return -1;

	if (ival != 0) {
		interval = ((uint8_t)atoi(ival) * 2) - 1;
		rc = set_lan_param(intf, chan, IPMI_LANP_GRAT_ARP, &interval, 1);
	} else {
		interval = lp->data[0];
	}

	printf("BMC-generated Gratuitous ARP interval:  %.1f seconds\n",
	       (float)((interval + 1) / 2));

	return rc;
}

static int
lan_set_arp_generate(struct ipmi_intf * intf,
		     uint8_t chan, uint8_t ctl)
{
	struct lan_param *lp;
	uint8_t data;

	lp = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (lp == NULL)
		return -1;
	if (lp->data == NULL)
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
lan_set_arp_respond(struct ipmi_intf * intf,
		    uint8_t chan, uint8_t ctl)
{
	struct lan_param *lp;
	uint8_t data;

	lp = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (lp == NULL)
		return -1;
	if (lp->data == NULL)
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
ipmi_lan_print(struct ipmi_intf * intf, uint8_t chan)
{
	struct lan_param * p;
	uint8_t medium;
	int rc = 0;

	if (chan < 1 || chan > IPMI_CHANNEL_NUMBER_MAX) {
		lprintf(LOG_ERR, "Invalid Channel %d", chan);
		return -1;
	}

	/* find type of channel and only accept 802.3 LAN */
	medium = ipmi_get_channel_medium(intf, chan);
	if (medium != IPMI_CHANNEL_MEDIUM_LAN &&
	    medium != IPMI_CHANNEL_MEDIUM_LAN_OTHER) {
		lprintf(LOG_ERR, "Channel %d (%s) is not a LAN channel",
			chan, val2str(medium, ipmi_channel_medium_vals), medium);
		return -1;
	}

	p = get_lan_param(intf, chan, IPMI_LANP_SET_IN_PROGRESS);
	if (p == NULL)
		return -1;
	if (p->data != NULL) {
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
	if (p == NULL)
		return -1;
	if (p->data != NULL) {
		printf("%-24s: %s%s%s%s%s\n", p->desc,
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_NONE) ? "NONE " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_MD2) ? "MD2 " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_MD5) ? "MD5 " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_PASSWORD) ? "PASSWORD " : "",
		       (p->data[0] & 1<<IPMI_SESSION_AUTHTYPE_OEM) ? "OEM " : "");
	}

	p = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE);
	if (p == NULL)
		return -1;
	if (p->data != NULL) {
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
	if (p == NULL)
		return -1;
	if (p->data != NULL) {
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
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_SUBNET_MASK);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_MAC_ADDR);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %02x:%02x:%02x:%02x:%02x:%02x\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3], p->data[4], p->data[5]);

	p = get_lan_param(intf, chan, IPMI_LANP_SNMP_STRING);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %s\n", p->desc, p->data);

	p = get_lan_param(intf, chan, IPMI_LANP_IP_HEADER);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: TTL=0x%02x Flags=0x%02x Precedence=0x%02x TOS=0x%02x\n",
		       p->desc, p->data[0], p->data[1] & 0xe0, p->data[2] & 0xe0, p->data[2] & 0x1e);

	p = get_lan_param(intf, chan, IPMI_LANP_BMC_ARP);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: ARP Responses %sabled, Gratuitous ARP %sabled\n", p->desc,
		       (p->data[0] & 2) ? "En" : "Dis", (p->data[0] & 1) ? "En" : "Dis");

	p = get_lan_param(intf, chan, IPMI_LANP_GRAT_ARP);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %.1f seconds\n", p->desc, (float)((p->data[0] + 1) / 2));

	p = get_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_IP);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_MAC);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %02x:%02x:%02x:%02x:%02x:%02x\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3], p->data[4], p->data[5]);

	p = get_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_IP);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %d.%d.%d.%d\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3]);

	p = get_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_MAC);
	if (p == NULL)
		return -1;
	if (p->data != NULL)
		printf("%-24s: %02x:%02x:%02x:%02x:%02x:%02x\n", p->desc,
		       p->data[0], p->data[1], p->data[2], p->data[3], p->data[4], p->data[5]);



	/* Determine supported Cipher Suites -- Requires two calls */
	p = get_lan_param(intf, chan, IPMI_LANP_RMCP_CIPHER_SUPPORT);
	if (p == NULL)
		return -1;
	else if (p->data != NULL)
	{
		p = get_lan_param(intf, chan, IPMI_LANP_RMCP_CIPHERS);
		if (p == NULL)
			return -1;

		printf("%-24s: ", p->desc);

		/* Now we're dangerous.  There are only 15 fixed cipher
		   suite IDs, but the spec allows for 16 in the return data.*/
		if ((p->data != NULL) && (p->data_len <= 16))
		{
			unsigned char cipher_suite_count = p->data[0];
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
	if (p == NULL)
		return -1;
	if ((p->data != NULL) && (p->data_len == 9))
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

	return rc;
}

/* Configure Authentication Types */
static int
ipmi_lan_set_auth(struct ipmi_intf * intf, uint8_t chan, char * level, char * types)
{
	uint8_t data[5];
	uint8_t authtype = 0;
	char * p;
	struct lan_param * lp;

	if (level == NULL || types == NULL)
		return -1;

	lp = get_lan_param(intf, chan, IPMI_LANP_AUTH_TYPE_ENABLE);
	if (lp == NULL)
		return -1;
	if (lp->data == NULL)
		return -1;

	lprintf(LOG_DEBUG, "%-24s: callback=0x%02x user=0x%02x operator=0x%02x admin=0x%02x oem=0x%02x",
		lp->desc, lp->data[0], lp->data[1], lp->data[2], lp->data[3], lp->data[4]);

	memset(data, 0, 5);
	memcpy(data, lp->data, 5);

	p = types;
	while (p) {
		if (strncasecmp(p, "none", 4) == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_NONE;
		else if (strncasecmp(p, "md2", 3) == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_MD2;
		else if (strncasecmp(p, "md5", 3) == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_MD5;
		else if ((strncasecmp(p, "password", 8) == 0) ||
			 (strncasecmp(p, "key", 3) == 0))
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_KEY;
		else if (strncasecmp(p, "oem", 3) == 0)
			authtype |= 1 << IPMI_SESSION_AUTHTYPE_OEM;
		else
			lprintf(LOG_WARNING, "Invalid authentication type: %s", p);
		p = strchr(p, ',');
		if (p)
			p++;
	}

	p = level;
	while (p) {
		if (strncasecmp(p, "callback", 8) == 0)
			data[0] = authtype;
		else if (strncasecmp(p, "user", 4) == 0)
			data[1] = authtype;
		else if (strncasecmp(p, "operator", 8) == 0)
			data[2] = authtype;
		else if (strncasecmp(p, "admin", 5) == 0)
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
ipmi_lan_set_password(struct ipmi_intf * intf,
	uint8_t userid, uint8_t * password)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t data[18];

	memset(&data, 0, sizeof(data));
	data[0] = userid & 0x3f;/* user ID */
	data[1] = 0x02;		/* set password */

	if (password != NULL)
		memcpy(data+2, password, __min(strlen(password), 16));

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x47;
	req.msg.data = data;
	req.msg.data_len = 18;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Set LAN Password for user %d", userid);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set LAN Password for user %d failed: %s",
			userid, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	/* adjust our session password
	 * or we will no longer be able to communicate with BMC
	 */
	ipmi_intf_session_set_password(intf, password);
	printf("Password %s for user %d\n",
	       (password == NULL) ? "cleared" : "set", userid);

	return 0;
}

static int
ipmi_set_channel_access(struct ipmi_intf * intf, uint8_t channel, uint8_t enable)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[3];

	memset(&req, 0, sizeof(req));

	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x40;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	/* SAVE TO NVRAM */
	memset(rqdata, 0, 3);
	rqdata[0] = channel & 0xf;
	rqdata[1] = 0x60;	/* set pef disabled, per-msg auth enabled */
	if (enable != 0)
		rqdata[1] |= 0x2; /* set always available if enable is set */
	rqdata[2] = 0x44; 	/* set channel privilege limit to ADMIN */
	
	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Set Channel Access for channel %d", channel);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Channel Access for channel %d failed: %s",
			channel, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	/* SAVE TO CURRENT */
	memset(rqdata, 0, 3);
	rqdata[0] = channel & 0xf;
	rqdata[1] = 0xa0;	/* set pef disabled, per-msg auth enabled */
	if (enable != 0)
		rqdata[1] |= 0x2; /* set always available if enable is set */
	rqdata[2] = 0x84; 	/* set channel privilege limit to ADMIN */

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Set Channel Access for channel %d", channel);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Channel Access for channel %d failed: %s",
			channel, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	/* can't send close session if access off so abort instead */
	if (enable == 0)
		intf->abort = 1;

	return 0;
}

static int
ipmi_set_user_access(struct ipmi_intf * intf, uint8_t channel, uint8_t userid)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	uint8_t rqdata[4];

	memset(rqdata, 0, 4);
	rqdata[0] = 0x90 | (channel & 0xf);
	rqdata[1] = userid & 0x3f;
	rqdata[2] = 0x4;
	rqdata[3] = 0;
	
	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = 0x43;
	req.msg.data = rqdata;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Unable to Set User Access for channel %d", channel);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set User Access for channel %d failed: %s",
			channel, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

static int
get_cmdline_macaddr(char * arg, uint8_t * buf)
{
	uint32_t m1, m2, m3, m4, m5, m6;
	if (sscanf(arg, "%02x:%02x:%02x:%02x:%02x:%02x",
		   &m1, &m2, &m3, &m4, &m5, &m6) != 6) {
		lprintf(LOG_ERR, "Invalid MAC address: %s", arg);
		return -1;
	}
	buf[0] = (uint8_t)m1;
	buf[1] = (uint8_t)m2;
	buf[2] = (uint8_t)m3;
	buf[3] = (uint8_t)m4;
	buf[4] = (uint8_t)m5;
	buf[5] = (uint8_t)m6;
	return 0;
}


static int
get_cmdline_cipher_suite_priv_data(char * arg, uint8_t * buf)
{
	int i, ret = 0;

	if (strlen(arg) != 15)
	{
		lprintf(LOG_ERR, "Invalid privilege specification length: %d",
			strlen(arg));
		return -1;
	}

	/*
	 * The first byte is reservered (0).  The resst of the buffer is setup
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
	bzero(buf, 9);

	for (i = 0; i < 15; ++i)
	{
		unsigned char priv_level;

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
get_cmdline_ipaddr(char * arg, uint8_t * buf)
{
	uint32_t ip1, ip2, ip3, ip4;
	if (sscanf(arg, "%d.%d.%d.%d", &ip1, &ip2, &ip3, &ip4) != 4) {
		lprintf(LOG_ERR, "Invalid IP address: %s", arg);
		return -1;
	}
	buf[0] = (uint8_t)ip1;
	buf[1] = (uint8_t)ip2;
	buf[2] = (uint8_t)ip3;
	buf[3] = (uint8_t)ip4;
	return 0;
}

static void ipmi_lan_set_usage(void)
{
	lprintf(LOG_NOTICE, "\nusage: lan set <channel> <command> [option]\n");
	lprintf(LOG_NOTICE, "LAN set commands:");
	lprintf(LOG_NOTICE, "  ipaddr <x.x.x.x>               Set channel IP address");
	lprintf(LOG_NOTICE, "  netmask <x.x.x.x>              Set channel IP netmask");
	lprintf(LOG_NOTICE, "  macaddr <x:x:x:x:x:x>          Set channel MAC address");
	lprintf(LOG_NOTICE, "  defgw ipaddr <x.x.x.x>         Set default gateway IP address");
	lprintf(LOG_NOTICE, "  defgw macaddr <x:x:x:x:x:x>    Set default gateway MAC address");
	lprintf(LOG_NOTICE, "  bakgw ipaddr <x.x.x.x>         Set backup gateway IP address");
	lprintf(LOG_NOTICE, "  bakgw macaddr <x:x:x:x:x:x>    Set backup gateway MAC address");
	lprintf(LOG_NOTICE, "  password <password>            Set session password for this channel");
	lprintf(LOG_NOTICE, "  snmp <community string>        Set SNMP public community string");
	lprintf(LOG_NOTICE, "  user                           Enable default user for this channel");
	lprintf(LOG_NOTICE, "  access <on|off>                Enable or disable access to this channel");
	lprintf(LOG_NOTICE, "  arp response <on|off>          Enable or disable BMC ARP responding");
	lprintf(LOG_NOTICE, "  arp generate <on|off>          Enable or disable BMC gratuitous ARP generation");
	lprintf(LOG_NOTICE, "  arp interval <seconds>         Set gratuitous ARP generation interval");
	lprintf(LOG_NOTICE, "  auth <level> <type,..>         Set channel authentication types");
	lprintf(LOG_NOTICE, "    level  = CALLBACK, USER, OPERATOR, ADMIN");
	lprintf(LOG_NOTICE, "    type   = NONE, MD2, MD5, PASSWORD, OEM");
	lprintf(LOG_NOTICE, "  ipsrc <source>                 Set IP Address source");
	lprintf(LOG_NOTICE, "    none   = unspecified source");
	lprintf(LOG_NOTICE, "    static = address manually configured to be static");
	lprintf(LOG_NOTICE, "    dhcp   = address obtained by BMC running DHCP");
	lprintf(LOG_NOTICE, "    bios   = address loaded by BIOS or system software");
	lprintf(LOG_NOTICE, "  cipher_privs XXXXXXXXXXXXXXX   Set RMCP+ cipher suite privilege levels");
	lprintf(LOG_NOTICE, "    X = Cipher Suite Unused");
	lprintf(LOG_NOTICE, "    c = CALLBACK");
	lprintf(LOG_NOTICE, "    u = USER");
	lprintf(LOG_NOTICE, "    o = OPERATOR");
	lprintf(LOG_NOTICE, "    a = ADMIN");
	lprintf(LOG_NOTICE, "    O = OEM\n");
}

static int
ipmi_lan_set(struct ipmi_intf * intf, int argc, char ** argv)
{
	uint8_t data[32];
	uint8_t chan, medium;
	int rc = 0;

	if (argc < 2) {
		ipmi_lan_set_usage();
		return 0;
	}

	if (strncmp(argv[0], "help", 4) == 0 ||
	    strncmp(argv[1], "help", 4) == 0) {
		ipmi_lan_set_usage();
		return 0;
	}

	chan = (uint8_t)strtol(argv[0], NULL, 0);

	if (chan < 1 || chan > IPMI_CHANNEL_NUMBER_MAX) {
		lprintf(LOG_ERR, "Invalid Channel %d", chan);
		ipmi_lan_set_usage();
		return -1;
	}

	/* find type of channel and only accept 802.3 LAN */
	medium = ipmi_get_channel_medium(intf, chan);
	if (medium != IPMI_CHANNEL_MEDIUM_LAN &&
	    medium != IPMI_CHANNEL_MEDIUM_LAN_OTHER) {
		lprintf(LOG_ERR, "Channel %d is not a LAN channel!", chan);
		return -1;
	}

	memset(&data, 0, sizeof(data));

	/* set user access */
	if (strncmp(argv[1], "user", 4) == 0) {
		rc = ipmi_set_user_access(intf, chan, 1);
	}
	/* set channel access mode */
	else if (strncmp(argv[1], "access", 6) == 0) {
		if (argc < 3 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE, "lan set access <on|off>");
		}
		else if (strncmp(argv[2], "on", 2) == 0) {
			rc = ipmi_set_channel_access(intf, chan, 1);
		}
		else if (strncmp(argv[2], "off", 3) == 0) {
			rc = ipmi_set_channel_access(intf, chan, 0);
		}
		else {
			lprintf(LOG_NOTICE, "lan set access <on|off>");
		}
	}
	/* set ARP control */
	else if (strncmp(argv[1], "arp", 3) == 0) {
		if (argc < 3 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE,
				"lan set <channel> arp respond <on|off>\n"
				"lan set <channel> arp generate <on|off>\n"
				"lan set <channel> arp interval <seconds>\n\n"
				"example: lan set 7 arp gratuitous off\n");
		}
		else if (strncmp(argv[2], "interval", 8) == 0) {
			rc = lan_set_arp_interval(intf, chan, argv[3]);
		}
		else if (strncmp(argv[2], "generate", 8) == 0) {
			if (argc < 4)
				lprintf(LOG_NOTICE, "lan set <channel> arp generate <on|off>");
			else if (strncmp(argv[3], "on", 2) == 0)
				rc = lan_set_arp_generate(intf, chan, 1);
			else if (strncmp(argv[3], "off", 3) == 0)
				rc = lan_set_arp_generate(intf, chan, 0);
			else
				lprintf(LOG_NOTICE, "lan set <channel> arp generate <on|off>");
		}
		else if (strncmp(argv[2], "respond", 7) == 0) {
			if (argc < 4)
				lprintf(LOG_NOTICE, "lan set <channel> arp respond <on|off>");
			else if (strncmp(argv[3], "on", 2) == 0)
				rc = lan_set_arp_respond(intf, chan, 1);
			else if (strncmp(argv[3], "off", 3) == 0)
				rc = lan_set_arp_respond(intf, chan, 0);
			else
				lprintf(LOG_NOTICE, "lan set <channel> arp respond <on|off>");
		}
		else {
			lprintf(LOG_NOTICE,
				"lan set <channel> arp respond <on|off>\n"
				"lan set <channel> arp generate <on|off>\n"
				"lan set <channel> arp interval <seconds>\n");
		}
	}
	/* set authentication types */
	else if (strncmp(argv[1], "auth", 4) == 0) {
		if (argc < 3 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE,
				"lan set <channel> auth <level> <type,type,...>\n"
				"  level = CALLBACK, USER, OPERATOR, ADMIN\n"
				"  types = NONE, MD2, MD5, PASSWORD, OEM\n"
				"example: lan set 7 auth ADMIN PASSWORD,MD5\n");
		} else {
			rc = ipmi_lan_set_auth(intf, chan, argv[2], argv[3]);
		}
	}
	/* ip address source */
	else if (strncmp(argv[1], "ipsrc", 5) == 0) {
		if (argc < 3 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE,
				"lan set <channel> ipsrc <source>\n"
				"  none   = unspecified\n"
				"  static = static address (manually configured)\n"
				"  dhcp   = address obtained by BMC running DHCP\n"
				"  bios   = address loaded by BIOS or system software\n");
		}
		else if (strncmp(argv[2], "none", 4) == 0)
			data[0] = 0;
		else if (strncmp(argv[2], "static", 5) == 0)
			data[0] = 1;
		else if (strncmp(argv[2], "dhcp", 4) == 0)
			data[0] = 2;
		else if (strncmp(argv[2], "bios", 4) == 0)
			data[0] = 3;
		else {
			lprintf(LOG_NOTICE,
				"lan set <channel> ipsrc <source>\n"
				"  none   = unspecified\n"
				"  static = static address (manually configured)\n"
				"  dhcp   = address obtained by BMC running DHCP\n"
				"  bios   = address loaded by BIOS or system software\n");
			return -1;
		}
		rc = set_lan_param(intf, chan, IPMI_LANP_IP_ADDR_SRC, data, 1);
	}
	/* session password
	 * not strictly a lan setting, but its used for lan connections */
	else if (strncmp(argv[1], "password", 8) == 0) {
		rc = ipmi_lan_set_password(intf, 1, argv[2]);
	}
	/* snmp community string */
	else if (strncmp(argv[1], "snmp", 4) == 0) {
		if (argc < 3 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE, "lan set <channel> snmp <community string>");
		} else {
			memcpy(data, argv[2], __min(strlen(argv[2]), 18));
			printf("Setting LAN %s to %s\n",
			       ipmi_lan_params[IPMI_LANP_SNMP_STRING].desc, data);
			rc = set_lan_param(intf, chan, IPMI_LANP_SNMP_STRING, data, 18);
		}
	}
	/* ip address */
	else if ((strncmp(argv[1], "ipaddr", 6) == 0) &&
		 (get_cmdline_ipaddr(argv[2], data) == 0)) {
		printf("Setting LAN %s to %d.%d.%d.%d\n",
		       ipmi_lan_params[IPMI_LANP_IP_ADDR].desc,
		       data[0], data[1], data[2], data[3]);
		rc = set_lan_param(intf, chan, IPMI_LANP_IP_ADDR, data, 4);
	}
	/* network mask */
	else if ((strncmp(argv[1], "netmask", 7) == 0) &&
		 (get_cmdline_ipaddr(argv[2], data) == 0)) {
		printf("Setting LAN %s to %d.%d.%d.%d\n",
		       ipmi_lan_params[IPMI_LANP_SUBNET_MASK].desc,
		       data[0], data[1], data[2], data[3]);
		rc = set_lan_param(intf, chan, IPMI_LANP_SUBNET_MASK, data, 4);
	}
	/* mac address */
	else if ((strncmp(argv[1], "macaddr", 7) == 0) &&
		 (get_cmdline_macaddr(argv[2], data) == 0)) {
		printf("Setting LAN %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
		       ipmi_lan_params[IPMI_LANP_MAC_ADDR].desc,
		       data[0], data[1], data[2], data[3], data[4], data[5]);
		rc = set_lan_param(intf, chan, IPMI_LANP_MAC_ADDR, data, 6);
	}
	/* default gateway settings */
	else if (strncmp(argv[1], "defgw", 5) == 0) {
		if (argc < 4 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE, "LAN set default gateway Commands: ipaddr, macaddr");
		}
		else if ((strncmp(argv[2], "ipaddr", 5) == 0) &&
			 (get_cmdline_ipaddr(argv[3], data) == 0)) {
			printf("Setting LAN %s to %d.%d.%d.%d\n",
			       ipmi_lan_params[IPMI_LANP_DEF_GATEWAY_IP].desc,
			       data[0], data[1], data[2], data[3]);
			rc = set_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_IP, data, 4);
		}
		else if ((strncmp(argv[2], "macaddr", 7) == 0) &&
			 (get_cmdline_macaddr(argv[3], data) == 0)) {
			printf("Setting LAN %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
			       ipmi_lan_params[IPMI_LANP_DEF_GATEWAY_MAC].desc,
			       data[0], data[1], data[2], data[3], data[4], data[5]);
			rc = set_lan_param(intf, chan, IPMI_LANP_DEF_GATEWAY_MAC, data, 6);
		}
	}
	/* backup gateway settings */
	else if (strncmp(argv[1], "bakgw", 5) == 0) {
		if (argc < 4 || (strncmp(argv[2], "help", 4) == 0)) {
			lprintf(LOG_NOTICE, "LAN set backup gateway commands: ipaddr, macaddr");
		}
		else if ((strncmp(argv[2], "ipaddr", 5) == 0) &&
			 (get_cmdline_ipaddr(argv[3], data) == 0)) {
			printf("Setting LAN %s to %d.%d.%d.%d\n",
			       ipmi_lan_params[IPMI_LANP_BAK_GATEWAY_IP].desc,
			       data[0], data[1], data[2], data[3]);
			rc = set_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_IP, data, 4);
		}
		else if ((strncmp(argv[2], "macaddr", 7) == 0) &&
			 (get_cmdline_macaddr(argv[3], data) == 0)) {
			printf("Setting LAN %s to %02x:%02x:%02x:%02x:%02x:%02x\n",
			       ipmi_lan_params[IPMI_LANP_BAK_GATEWAY_MAC].desc,
			       data[0], data[1], data[2], data[3], data[4], data[5]);
			rc = set_lan_param(intf, chan, IPMI_LANP_BAK_GATEWAY_MAC, data, 6);
		}
	}

	/* RMCP+ cipher suite privilege levels */
	else if (strncmp(argv[1], "cipher_privs", 12) == 0)
	{
		if ((argc != 3)                        ||
		    (strncmp(argv[2], "help", 4) == 0) ||
		    get_cmdline_cipher_suite_priv_data(argv[2], data))
		{
			lprintf(LOG_NOTICE, "lan set <channel> cipher_privs XXXXXXXXXXXXXXX");
			lprintf(LOG_NOTICE, "    X = Cipher Suite Unused");
			lprintf(LOG_NOTICE, "    c = CALLBACK");
			lprintf(LOG_NOTICE, "    u = USER");
			lprintf(LOG_NOTICE, "    o = OPERATOR");
			lprintf(LOG_NOTICE, "    a = ADMIN");
			lprintf(LOG_NOTICE, "    O = OEM\n");
		}
		else
		{
			rc = set_lan_param(intf, chan, IPMI_LANP_RMCP_PRIV_LEVELS, data, 9);
		}
	}
		
	return rc;
}


int
ipmi_lanp_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc == 0 || (strncmp(argv[0], "help", 4) == 0)) {
		lprintf(LOG_NOTICE, "LAN Commands:  print, set");
		return -1;
	}


	if ((strncmp(argv[0], "printconf", 9) == 0) ||
	    (strncmp(argv[0], "print", 5) == 0)) {
		uint8_t chan = 7;
		if (argc > 1)
			chan = (uint8_t)strtol(argv[1], NULL, 0);
		if (chan < 1 || chan > IPMI_CHANNEL_NUMBER_MAX)
			lprintf(LOG_NOTICE, "usage: lan print <channel>");
		else
			rc = ipmi_lan_print(intf, chan);
	}
	else if (strncmp(argv[0], "set", 3) == 0)
		rc = ipmi_lan_set(intf, argc-1, &(argv[1]));
	else
		lprintf(LOG_NOTICE, "Invalid LAN command: %s", argv[0]);

	return rc;
}

