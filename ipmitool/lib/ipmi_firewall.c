/*
 * Copyright (c) 2005 International Business Machines, Inc.  All Rights Reserved.
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
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <ipmitool/helper.h>
#include <ipmitool/log.h>
#include <ipmitool/bswap.h>
#include <ipmitool/ipmi.h>
#include <ipmitool/ipmi_intf.h>
#include <ipmitool/ipmi_firewall.h>
#include <ipmitool/ipmi_strings.h>

static void
printf_firewall_usage(void)
{
	printf("Firmware Firewall Commands:\n");
	printf("\tinfo [channel H] [lun L]\n");
	printf("\tinfo [channel H] [lun L [netfn N [command C [subfn S]]]]\n");
	printf("\tenable [channel H] [lun L [netfn N [command C [subfn S]]]]\n");
	printf("\tdisable [channel H] [lun L [netfn N [command C [subfn S]]]] [force])\n");
	printf("\treset [channel H] \n");
	printf("\t\twhere H is a Channel, L is a LUN, N is a NetFn,\n");
	printf("\t\tC is a Command and S is a Sub-Function\n");
}

// print n bytes of bit field bf (if invert, print ~bf)
static void print_bitfield(const unsigned char * bf, int n, int invert, int loglevel) {
	int i = 0;
	if (loglevel < 0) {
		while (i<n) {
			printf("%02x", (unsigned char) (invert?~bf[i]:bf[i]));
			if (++i % 4 == 0)
				printf(" ");
		}
		printf("\n");
	} else {
		while (i<n) {
			lprintf(loglevel, "%02x", (unsigned char) (invert?~bf[i]:bf[i]));
			if (++i % 4 == 0)
				lprintf(loglevel, " ");
		}
		lprintf(loglevel, "\n");
	}

}

static int
ipmi_firewall_parse_args(int argc, char ** argv, struct ipmi_function_params * p)
{
	int i;

	if (!p) {
		lprintf(LOG_ERR, "ipmi_firewall_parse_args: p is NULL");
		return -1;
	}
	for (i=0; i<argc; i++) {
		if (strncmp(argv[i], "channel", 7) == 0) {
			if (++i < argc)
				p->channel = strtol(argv[i], NULL, 0);
		}
		else if (strncmp(argv[i], "lun", 3) == 0) {
			if (++i < argc)
				p->lun = strtol(argv[i], NULL, 0);
		}
		else if (strncmp(argv[i], "force", 5) == 0) {
			p->force = 1;
		}
		else if (strncmp(argv[i], "netfn", 5) == 0) {
			if (++i < argc)
				p->netfn = strtol(argv[i], NULL, 0);
		}
		else if (strncmp(argv[i], "command", 7) == 0) {
			if (++i < argc)
				p->command = strtol(argv[i], NULL, 0);
		}
		else if (strncmp(argv[i], "subfn", 5) == 0) {
			if (++i < argc)
				p->subfn = strtol(argv[i], NULL, 0);
		}
	}
	if (p->subfn >= MAX_SUBFN) {
		printf("subfn is out of range (0-%d)\n", MAX_SUBFN-1);
		return -1;
	}
	if (p->command >= MAX_COMMAND) {
		printf("command is out of range (0-%d)\n", MAX_COMMAND-1);
		return -1;
	}
	if (p->netfn >= MAX_NETFN) {
		printf("netfn is out of range (0-%d)\n", MAX_NETFN-1);
		return -1;
	}
	if (p->lun >= MAX_LUN) {
		printf("lun is out of range (0-%d)\n", MAX_LUN-1);
		return -1;
	}
	if (p->netfn >= 0 && p->lun < 0) {
		printf("if netfn is set, lun must be set also\n");
		return -1;
	}
	if (p->command >= 0 && p->netfn < 0) {
		printf("if command is set, netfn must be set also\n");
		return -1;
	}
	if (p->subfn >= 0 && p->command < 0) {
		printf("if subfn is set, command must be set also\n");
		return -1;
	}
	return 0;
}

/* _get_netfn_suport
 *
 * @intf:	ipmi interface
 * @channel:	ipmi channel
 * @lun:	a pointer to a 4 byte field
 * @netfn:	a pointer to a 128-bit bitfield (16 bytes)
 *
 * returns 0 on success and fills in the bitfield for 
 * the 32 netfn * 4 LUN pairs that support commands
 * returns -1 on error
 */
static int
_get_netfn_support(struct ipmi_intf * intf, int channel, unsigned char * lun, unsigned char * netfn)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char * d, rqdata;
	unsigned int l;

	if (!lun || !netfn) {
		lprintf(LOG_ERR, "_get_netfn_suport: lun or netfn is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_NETFN_SUPPORT;
	rqdata = (unsigned char) channel;
	req.msg.data = &rqdata;
	req.msg.data_len = 1;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get NetFn Support command failed");
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get NetFn Support command failed: %s",
			val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (l=0; l<4; l++) {
		lun[l] = (*d)>>(2*l) & 0x3;
	}
	d++;

	memcpy(netfn, d, 16);

	return 0;
}

/* _get_command_suport
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @lnfn:	a pointer to a struct lun_netfn_support
 *
 * returns 0 on success and fills in lnfn according to the request in p
 * returns -1 on error
 */
static int
_get_command_support(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct lun_netfn_support * lnfn)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char * d, rqdata[3];
	unsigned int c;

	if (!p || !lnfn) {
		lprintf(LOG_ERR, "_get_netfn_suport: p or lnfn is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_COMMAND_SUPPORT;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Command Support (LUN=%d, NetFn=%d, op=0) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Command Support (LUN=%d, NetFn=%d, op=0) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (c=0; c<128; c++) {
		if (!(d[c>>3] & (1<<(c%8))))
			lnfn->command[c].support |= BIT_AVAILABLE;
	}
	memcpy(lnfn->command_mask, d, MAX_COMMAND_BYTES/2);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_COMMAND_SUPPORT;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = 0x40 | p->netfn;
	rqdata[2] = p->lun;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Command Support (LUN=%d, NetFn=%d, op=1) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Command Support (LUN=%d, NetFn=%d, op=1) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (c=0; c<128; c++) {
		if (!(d[c>>3] & (1<<(c%8))))
			lnfn->command[128+c].support |= BIT_AVAILABLE;
	}
	memcpy(lnfn->command_mask+MAX_COMMAND_BYTES/2, d, MAX_COMMAND_BYTES/2);
	return 0;
}

/* _get_command_configurable
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @lnfn:	a pointer to a struct lun_netfn_support
 *
 * returns 0 on success and fills in lnfn according to the request in p
 * returns -1 on error
 */
static int
_get_command_configurable(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct lun_netfn_support * lnfn)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char * d, rqdata[3];
	unsigned int c;

	if (!p || !lnfn) {
		lprintf(LOG_ERR, "_get_command_configurable: p or lnfn is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_CONFIGURABLE_COMMANDS;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Configurable Command (LUN=%d, NetFn=%d, op=0) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Configurable Command (LUN=%d, NetFn=%d, op=0) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (c=0; c<128; c++) {
		if (d[c>>3] & (1<<(c%8)))
			lnfn->command[c].support |= BIT_CONFIGURABLE;
	}
	memcpy(lnfn->config_mask, d, MAX_COMMAND_BYTES/2);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_CONFIGURABLE_COMMANDS;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = 0x40 | p->netfn;
	rqdata[2] = p->lun;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Configurable Command (LUN=%d, NetFn=%d, op=1) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Configurable Command (LUN=%d, NetFn=%d, op=1) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (c=0; c<128; c++) {
		if (d[c>>3] & (1<<(c%8)))
			lnfn->command[128+c].support |= BIT_CONFIGURABLE;
	}
	memcpy(lnfn->config_mask+MAX_COMMAND_BYTES/2, d, MAX_COMMAND_BYTES/2);
	return 0;
}

/* _get_command_enables
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @lnfn:	a pointer to a struct lun_netfn_support
 *
 * returns 0 on success and fills in lnfn according to the request in p
 * returns -1 on error
 */
static int
_get_command_enables(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct lun_netfn_support * lnfn)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char * d, rqdata[3];
	unsigned int c;

	if (!p || !lnfn) {
		lprintf(LOG_ERR, "_get_command_enables: p or lnfn is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_COMMAND_ENABLES;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Command Enables (LUN=%d, NetFn=%d, op=0) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Command Enables (LUN=%d, NetFn=%d, op=0) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (c=0; c<128; c++) {
		if (d[c>>3] & (1<<(c%8)))
			lnfn->command[c].support |= BIT_ENABLED;
	}
	memcpy(lnfn->enable_mask, d, MAX_COMMAND_BYTES/2);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_COMMAND_ENABLES;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = 0x40 | p->netfn;
	rqdata[2] = p->lun;
	req.msg.data = rqdata;
	req.msg.data_len = 3;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Command Enables (LUN=%d, NetFn=%d, op=1) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Command Enables (LUN=%d, NetFn=%d, op=1) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	for (c=0; c<128; c++) {
		if (d[c>>3] & (1<<(c%8)))
			lnfn->command[128+c].support |= BIT_ENABLED;
	}
	memcpy(lnfn->enable_mask+MAX_COMMAND_BYTES/2, d, MAX_COMMAND_BYTES/2);
	return 0;
}

/* _set_command_enables
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @lnfn:	a pointer to a struct lun_netfn_support that contains current info
 * @enable:	a pointer to a 32 byte bitfield that contains the desired enable state
 * @gun:	here is a gun to shoot yourself in the foot.  If this is true
 * 		you are allowed to disable this command
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
_set_command_enables(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct lun_netfn_support * lnfn,
	unsigned char * enable, int gun)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char * d, rqdata[19];
	unsigned int c;

	if (!p || !lnfn) {
		lprintf(LOG_ERR, "_set_command_enables: p or lnfn is NULL");
		return -1;
	}

	lprintf(LOG_INFO, "support:            ");
	print_bitfield(lnfn->command_mask, MAX_COMMAND_BYTES, 1, LOG_INFO);
	lprintf(LOG_INFO, "configurable:       ");
	print_bitfield(lnfn->config_mask, MAX_COMMAND_BYTES, 0, LOG_INFO);
	lprintf(LOG_INFO, "enabled:            ");
	print_bitfield(lnfn->enable_mask, MAX_COMMAND_BYTES, 0, LOG_INFO);
	lprintf(LOG_INFO, "enable mask before: ");
	print_bitfield(enable, MAX_COMMAND_BYTES, 0, LOG_INFO);

	// mask off the appropriate bits (if not configurable, set enable bit
	// must be the same as the current enable bit)
	for (c=0; c<(MAX_COMMAND_BYTES); c++) {
		enable[c] = (lnfn->config_mask[c] & enable[c]) |
			(~lnfn->config_mask[c] & lnfn->enable_mask[c]);
	}

	// take the gun out of their hand if they are not supposed to have it
	if (!gun) {
		enable[SET_COMMAND_ENABLE_BYTE] = 
			(lnfn->config_mask[SET_COMMAND_ENABLE_BYTE]
			 & SET_COMMAND_ENABLE_BIT) |
			(~lnfn->config_mask[SET_COMMAND_ENABLE_BYTE]
			 & lnfn->enable_mask[SET_COMMAND_ENABLE_BYTE]);
	}
	lprintf(LOG_INFO, "enable mask after: ");
	print_bitfield(enable, MAX_COMMAND_BYTES, 0, LOG_INFO);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_SET_COMMAND_ENABLES;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	memcpy(&rqdata[3], enable, MAX_COMMAND_BYTES/2);
	req.msg.data = rqdata;
	req.msg.data_len = 19;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set Command Enables (LUN=%d, NetFn=%d, op=0) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Command Enables (LUN=%d, NetFn=%d, op=0) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_SET_COMMAND_ENABLES;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = 0x40 | p->netfn;
	rqdata[2] = p->lun;
	memcpy(&rqdata[3], enable+MAX_COMMAND_BYTES/2, MAX_COMMAND_BYTES/2);
	req.msg.data = rqdata;
	req.msg.data_len = 19;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set Command Enables (LUN=%d, NetFn=%d, op=1) command failed", p->lun, p->netfn);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Command Enables (LUN=%d, NetFn=%d, op=1) command failed: %s",
			p->lun, p->netfn, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	d = rsp->data;
	return 0;
}

/* _get_subfn_support
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @cmd:	a pointer to a struct command_support
 *
 * returns 0 on success and fills in cmd according to the request in p
 * returns -1 on error
 */
static int
_get_subfn_support(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct command_support * cmd)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[4];

	if (!p || !cmd) {
		lprintf(LOG_ERR, "_get_subfn_support: p or cmd is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_COMMAND_SUBFUNCTION_SUPPORT;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	rqdata[3] = p->command;
	req.msg.data = rqdata;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Command Sub-function Support (LUN=%d, NetFn=%d, command=%d) command failed", p->lun, p->netfn, p->command);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Command Sub-function Support (LUN=%d, NetFn=%d, command=%d) command failed: %s",
			p->lun, p->netfn, p->command, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(cmd->subfn_support, rsp->data, sizeof(cmd->subfn_support));
	return 0;
}

/* _get_subfn_configurable
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @cmd:	a pointer to a struct command_support
 *
 * returns 0 on success and fills in cmd according to the request in p
 * returns -1 on error
 */
static int
_get_subfn_configurable(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct command_support * cmd)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[4];

	if (!p || !cmd) {
		lprintf(LOG_ERR, "_get_subfn_configurable: p or cmd is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_CONFIGURABLE_COMMAND_SUBFUNCTIONS;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	rqdata[3] = p->command;
	req.msg.data = rqdata;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Configurable Command Sub-function (LUN=%d, NetFn=%d, command=%d) command failed", p->lun, p->netfn, p->command);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Configurable Command Sub-function (LUN=%d, NetFn=%d, command=%d) command failed: %s",
			p->lun, p->netfn, p->command, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(cmd->subfn_config, rsp->data, sizeof(cmd->subfn_config));
	return 0;
}

/* _get_subfn_enables
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @cmd:	a pointer to a struct command_support
 *
 * returns 0 on success and fills in cmd according to the request in p
 * returns -1 on error
 */
static int
_get_subfn_enables(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct command_support * cmd)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[4];

	if (!p || !cmd) {
		lprintf(LOG_ERR, "_get_subfn_enables: p or cmd is NULL");
		return -1;
	}

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_COMMAND_SUBFUNCTION_ENABLES;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	rqdata[3] = p->command;
	req.msg.data = rqdata;
	req.msg.data_len = 4;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Get Command Sub-function Enables (LUN=%d, NetFn=%d, command=%d) command failed", p->lun, p->netfn, p->command);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Get Command Sub-function Enables (LUN=%d, NetFn=%d, command=%d) command failed: %s",
			p->lun, p->netfn, p->command, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	memcpy(cmd->subfn_enable, rsp->data, sizeof(cmd->subfn_enable));
	return 0;
}

/* _set_subfn_enables
 *
 * @intf:	ipmi interface
 * @p:  	a pointer to a struct ipmi_function_params
 * @cmd:	a pointer to a struct command_support
 * @enable:	a pointer to a 4 byte bitfield that contains the desired enable state
 *
 * returns 0 on success (and modifies enable to be the bits it actually set)
 * returns -1 on error
 */
static int
_set_subfn_enables(struct ipmi_intf * intf,
	struct ipmi_function_params * p, struct command_support * cmd,
	unsigned char * enable)
{
	struct ipmi_rs * rsp;
	struct ipmi_rq req;
	unsigned char rqdata[8];
	unsigned int c;

	if (!p || !cmd) {
		lprintf(LOG_ERR, "_set_subfn_enables: p or cmd is NULL");
		return -1;
	}

	lprintf(LOG_INFO, "support:            ");
	print_bitfield(cmd->subfn_support, MAX_SUBFN_BYTES, 1, LOG_INFO);
	lprintf(LOG_INFO, "configurable:       ");
	print_bitfield(cmd->subfn_config, MAX_SUBFN_BYTES, 0, LOG_INFO);
	lprintf(LOG_INFO, "enabled:            ");
	print_bitfield(cmd->subfn_enable, MAX_SUBFN_BYTES, 0, LOG_INFO);
	lprintf(LOG_INFO, "enable mask before: ");
	print_bitfield(enable, MAX_SUBFN_BYTES, 0, LOG_INFO);
	// mask off the appropriate bits (if not configurable, set enable bit
	// must be the same as the current enable bit)
	for (c=0; c<sizeof(cmd->subfn_enable); c++) {
		enable[c] = (cmd->subfn_config[c] & enable[c]) |
			(~cmd->subfn_config[c] & cmd->subfn_enable[c]);
	}
	lprintf(LOG_INFO, "enable mask after: ");
	print_bitfield(enable, MAX_SUBFN_BYTES, 0, LOG_INFO);

	memset(&req, 0, sizeof(req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_SET_COMMAND_SUBFUNCTION_ENABLES;
	rqdata[0] = (unsigned char) p->channel;
	rqdata[1] = p->netfn;
	rqdata[2] = p->lun;
	rqdata[3] = p->command;
	memcpy(&rqdata[4], enable, MAX_SUBFN_BYTES);
	req.msg.data = rqdata;
	req.msg.data_len = 8;

	rsp = intf->sendrecv(intf, &req);
	if (rsp == NULL) {
		lprintf(LOG_ERR, "Set Command Sub-function Enables (LUN=%d, NetFn=%d, command=%d) command failed", p->lun, p->netfn, p->command);
		return -1;
	}
	if (rsp->ccode > 0) {
		lprintf(LOG_ERR, "Set Command Sub-function Enables (LUN=%d, NetFn=%d, command=%d) command failed: %s",
			p->lun, p->netfn, p->command, val2str(rsp->ccode, completion_code_vals));
		return -1;
	}

	return 0;
}

/* _gather_info
 *
 * @intf:	ipmi interface
 * @p:		a pointer to a struct ipmi_function_params
 * @bmc:	a pointer to a struct bmc_fn_support
 * @enable:	a pointer to a 4 byte bitfield that contains the desired enable state
 *
 * returns 0 on success and fills in bmc according to request p
 * returns -1 on error
 */
static int _gather_info(struct ipmi_intf * intf, struct ipmi_function_params * p, struct bmc_fn_support * bmc)
{
	int ret, l, n;
	unsigned char lun[MAX_LUN], netfn[16];

	ret = _get_netfn_support(intf, p->channel, lun, netfn);
	if (!ret) {
		for (l=0; l<MAX_LUN; l++) {
			if (p->lun >= 0 && p->lun != l)
				continue;
			bmc->lun[l].support = lun[l];
			if (lun[l]) {
				for (n=0; n<MAX_NETFN_PAIR; n++) {
					int offset = l*MAX_NETFN_PAIR+n;
					bmc->lun[l].netfn[n].support = 
						!!(netfn[offset>>3] & (1<<(offset%8)));
				}
			}
		}
	}
	if (p->netfn >= 0) {
		if (!((p->lun < 0 || bmc->lun[p->lun].support) &&
		      (p->netfn < 0 || bmc->lun[p->lun].netfn[p->netfn>>1].support))) {
			lprintf(LOG_ERR, "LUN or LUN/NetFn pair %d,%d not supported", p->lun, p->netfn);
			return 0;
		}
		ret = _get_command_support(intf, p, &(bmc->lun[p->lun].netfn[p->netfn>>1]));
		ret |= _get_command_configurable(intf, p, &(bmc->lun[p->lun].netfn[p->netfn>>1]));
		ret |= _get_command_enables(intf, p, &(bmc->lun[p->lun].netfn[p->netfn>>1]));
		if (!ret && p->command >= 0) {
			ret = _get_subfn_support(intf, p,
						 &(bmc->lun[p->lun].netfn[p->netfn>>1].command[p->command]));
			ret |= _get_subfn_configurable(intf, p,
						       &(bmc->lun[p->lun].netfn[p->netfn>>1].command[p->command]));
			ret |= _get_subfn_enables(intf, p,
						  &(bmc->lun[p->lun].netfn[p->netfn>>1].command[p->command]));
		}
	}
	else if (p->lun >= 0) {
		l = p->lun;
		if (bmc->lun[l].support) {
			for (n=0; n<MAX_NETFN_PAIR; n++) {
				p->netfn = n*2;
				if (bmc->lun[l].netfn[n].support) {
					ret = _get_command_support(intf, p, &(bmc->lun[l].netfn[n]));
					ret |= _get_command_configurable(intf, p, &(bmc->lun[l].netfn[n]));
					ret |= _get_command_enables(intf, p, &(bmc->lun[l].netfn[n]));
				}
				if (ret)
					bmc->lun[l].netfn[n].support = 0;
			}
		}
		p->netfn = -1;
	} else {
		for (l=0; l<4; l++) {
			p->lun = l;
			if (bmc->lun[l].support) {
				for (n=0; n<MAX_NETFN_PAIR; n++) {
					p->netfn = n*2;
					if (bmc->lun[l].netfn[n].support) {
						ret = _get_command_support(intf, p, &(bmc->lun[l].netfn[n]));
						ret |= _get_command_configurable(intf, p, &(bmc->lun[l].netfn[n]));
						ret |= _get_command_enables(intf, p, &(bmc->lun[l].netfn[n]));
					}
					if (ret)
						bmc->lun[l].netfn[n].support = 0;
				}
			}
		}
		p->lun = -1;
		p->netfn = -1;
	}

	return 0;
}

/* ipmi_firewall_info - print out info for firewall functions
 *
 * @intf:	ipmi inteface
 * @argc:	argument count
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_firewall_info(struct ipmi_intf * intf, int argc, char ** argv)
{
	int ret = 0;
	struct ipmi_function_params p = {0xe, -1, -1, -1, -1};
	struct bmc_fn_support * bmc_fn_support;
	unsigned int l, n, c;

	if ((argc > 0 && strncmp(argv[0], "help", 4) == 0) || ipmi_firewall_parse_args(argc, argv, &p) < 0)
	{
		printf("info [channel H]\n");
		printf("\tlist all of the firewall information for all LUNs, NetFns, and Commands\n");
		printf("\tthis is a long list and is not very human readable\n");
		printf("info [channel H] lun L\n");
		printf("\tthis also prints a long list that is not very human readable\n");
		printf("info [channel H] lun L netfn N\n");
		printf("\tthis prints out information for a single LUN/NetFn pair\n");
		printf("\tthat is not really very usable, but at least it is short\n");
		printf("info [channel H] lun L netfn N command C\n");
		printf("\tthis is the one you want -- it prints out detailed human\n");
		printf("\treadable information.  It shows the support, configurable, and\n");
		printf("\tenabled bits for the Command C on LUN/NetFn pair L,N and the\n");
		printf("\tsame information about each of its Sub-functions\n");
		return 0;
	}

	bmc_fn_support = malloc(sizeof(struct bmc_fn_support));
	if (!bmc_fn_support) {
		lprintf(LOG_ERR, "malloc struct bmc_fn_support failed");
		return -1;
	}

	ret = _gather_info(intf, &p, bmc_fn_support);

	if (p.command >= 0) {
      struct command_support * cmd;
		if (!((p.lun < 0 || bmc_fn_support->lun[p.lun].support) &&
			(p.netfn < 0 || bmc_fn_support->lun[p.lun].netfn[p.netfn>>1].support) &&
			bmc_fn_support->lun[p.lun].netfn[p.netfn>>1].command[p.command].support))
		{
			lprintf(LOG_ERR, "Command 0x%02x not supported on LUN/NetFn pair %02x,%02x",
				p.command, p.lun, p.netfn);
			free(bmc_fn_support);
			return 0;
		}
		cmd =
			&bmc_fn_support->lun[p.lun].netfn[p.netfn>>1].command[p.command];
		c = cmd->support;
		printf("(A)vailable, (C)onfigurable, (E)nabled: | A | C | E |\n");
		printf("-----------------------------------------------------\n");
		printf("LUN %01d, NetFn 0x%02x, Command 0x%02x:        | %c | %c | %c |\n",
			p.lun, p.netfn, p.command,
			(c & BIT_AVAILABLE) ? 'X' : ' ',
			(c & BIT_CONFIGURABLE) ? 'X' : ' ',
			(c & BIT_ENABLED) ? 'X': ' ');

		for (n=0; n<MAX_SUBFN; n++) {
			printf("sub-function 0x%02x:                      | %c | %c | %c |\n", n,
				(!bit_test(cmd->subfn_support, n)) ? 'X' : ' ',
				(bit_test(cmd->subfn_config, n)) ? 'X' : ' ',
				(bit_test(cmd->subfn_enable, n)) ? 'X' : ' ');
		}
	}
	else if (p.netfn >= 0) {
		if (!((p.lun < 0 || bmc_fn_support->lun[p.lun].support) &&
			(bmc_fn_support->lun[p.lun].netfn[p.netfn>>1].support)))
		{
			lprintf(LOG_ERR, "LUN or LUN/NetFn pair %02x,%02x not supported",
				p.lun, p.netfn);
			free(bmc_fn_support);
			return 0;
		}
		n = p.netfn >> 1;
		l = p.lun;
		printf("Commands on LUN 0x%02x, NetFn 0x%02x\n", p.lun, p.netfn);
		printf("support:      ");
		print_bitfield(bmc_fn_support->lun[l].netfn[n].command_mask,
				MAX_COMMAND_BYTES, 1, -1);
		printf("configurable: ");
		print_bitfield(bmc_fn_support->lun[l].netfn[n].config_mask,
				MAX_COMMAND_BYTES, 0, -1);
		printf("enabled:      ");
		print_bitfield(bmc_fn_support->lun[l].netfn[n].enable_mask,
				MAX_COMMAND_BYTES, 0, -1);
	}
	else {
	    for (l=0; l<4; l++) {
                p.lun = l;
                if (bmc_fn_support->lun[l].support) {
                    for (n=0; n<MAX_NETFN_PAIR; n++) {
                        p.netfn = n*2;
                        if (bmc_fn_support->lun[l].netfn[n].support) {
                            printf("%02x,%02x support:      ", p.lun, p.netfn);
                            print_bitfield(bmc_fn_support->lun[l].netfn[n].command_mask,
                                    MAX_COMMAND_BYTES, 1, -1);
                            printf("%02x,%02x configurable: ", p.lun, p.netfn);
                            print_bitfield(bmc_fn_support->lun[l].netfn[n].config_mask,
                                    MAX_COMMAND_BYTES, 0, -1);
                            printf("%02x,%02x enabled:      ", p.lun, p.netfn);
                            print_bitfield(bmc_fn_support->lun[l].netfn[n].enable_mask,
                                    MAX_COMMAND_BYTES, 0, -1);
                        }
                    }
                }
            }
            p.lun = -1;
            p.netfn = -1;
	}

	free(bmc_fn_support);
	return ret;
}

/* ipmi_firewall_enable_disable  -  enable/disable BMC functions
 *
 * @intf:	ipmi inteface
 * @enable:     whether to enable or disable
 * @argc:	argument count
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_firewall_enable_disable(struct ipmi_intf * intf, int enable, int argc, char ** argv)
{
	struct ipmi_function_params p = {0xe, -1, -1, -1, -1};
	struct bmc_fn_support * bmc_fn_support;
	unsigned int l, n, c, ret;
	unsigned char enables[MAX_COMMAND_BYTES];

	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
		char * s1 = enable?"en":"dis";
		char * s2 = enable?"":" [force]";
		printf("%sable [channel H] lun L netfn N%s\n", s1, s2);
		printf("\t%sable all commands on this LUN/NetFn pair\n", s1);
		printf("%sable [channel H] lun L netfn N command C%s\n", s1, s2);
		printf("\t%sable Command C and all its Sub-functions for this LUN/NetFn pair\n", s1);
		printf("%sable [channel H] lun L netfn N command C subfn S\n", s1);
		printf("\t%sable Sub-function S for Command C for this LUN/NetFn pair\n", s1);
		if (!enable) {
			printf("* force will allow you to disable the \"Command Set Enable\" command\n");
			printf("\tthereby letting you shoot yourself in the foot\n");
			printf("\tthis is only recommended for advanced users\n");
		}
		return 0;
	}
	if (ipmi_firewall_parse_args(argc, argv, &p) < 0)
		return -1;

	bmc_fn_support = malloc(sizeof(struct bmc_fn_support));
	if (!bmc_fn_support) {
		lprintf(LOG_ERR, "malloc struct bmc_fn_support failed");
		return -1;
	}

	ret = _gather_info(intf, &p, bmc_fn_support);
	if (ret < 0) {
		free(bmc_fn_support);
		return ret;
	}

	l = p.lun;
	n = p.netfn>>1;
	c = p.command;
	if (p.subfn >= 0) {
		// firewall (en|dis)able [channel c] lun l netfn n command m subfn s
		// (en|dis)able this sub-function for this commnad on this lun/netfn pair
		memcpy(enables,
			bmc_fn_support->lun[l].netfn[n].command[c].subfn_enable,
			MAX_SUBFN_BYTES);
		bit_set(enables, p.subfn, enable);
		ret = _set_subfn_enables(intf, &p,
			&bmc_fn_support->lun[l].netfn[n].command[c], enables);

	} else if (p.command >= 0) {
		// firewall (en|dis)able [channel c] lun l netfn n command m
		//    (en|dis)able all subfn and command for this commnad on this lun/netfn pair
		memset(enables, enable?0xff:0, MAX_SUBFN_BYTES);
		ret = _set_subfn_enables(intf, &p,
			&bmc_fn_support->lun[l].netfn[n].command[c], enables);
		memcpy(enables,
			&bmc_fn_support->lun[l].netfn[n].enable_mask, sizeof(enables));
		bit_set(enables, p.command, enable);
		ret |= _set_command_enables(intf, &p,
			&bmc_fn_support->lun[l].netfn[n], enables, p.force);
	} else if (p.netfn >= 0) {
		// firewall (en|dis)able [channel c] lun l netfn n
		//    (en|dis)able all commnads on this lun/netfn pair
		memset(enables, enable?0xff:0, sizeof(enables));
		ret = _set_command_enables(intf, &p,
			&bmc_fn_support->lun[l].netfn[n], enables, p.force);
		/*
		   } else if (p.lun >= 0) {
		// firewall (en|dis)able [channel c] lun l
		//    (en|dis)able all commnads on all netfn pairs for this lun
		*/
	}
	free(bmc_fn_support);
	return ret;
}

/* ipmi_firewall_reset - reset firmware firewall to enable everything
 *
 * @intf:	ipmi inteface
 * @argc:	argument count
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
static int
ipmi_firewall_reset(struct ipmi_intf * intf, int argc, char ** argv)
{
	struct ipmi_function_params p = {0xe, -1, -1, -1, -1};
	struct bmc_fn_support * bmc_fn_support;
	unsigned int l, n, c, ret;
	unsigned char enables[MAX_COMMAND_BYTES];

	if (argc > 0 || (argc > 0 && strncmp(argv[0], "help", 4) == 0)) {
		printf_firewall_usage();
		return 0;
	}
	if (ipmi_firewall_parse_args(argc, argv, &p) < 0)
		return -1;

	bmc_fn_support = malloc(sizeof(struct bmc_fn_support));
	if (!bmc_fn_support) {
		lprintf(LOG_ERR, "malloc struct bmc_fn_support failed");
		return -1;
	}

	ret = _gather_info(intf, &p, bmc_fn_support);
	if (ret < 0) {
		free(bmc_fn_support);
		return ret;
	}

	for (l=0; l<MAX_LUN; l++) {
		p.lun = l;
		for (n=0; n<MAX_NETFN; n+=2) {
			p.netfn = n;
			for (c=0; c<MAX_COMMAND; c++) {
				p.command = c;
				printf("reset lun %d, netfn %d, command %d, subfn\n", l, n, c);
				memset(enables, 0xff, MAX_SUBFN_BYTES);
				ret = _set_subfn_enables(intf, &p,
					&bmc_fn_support->lun[l].netfn[n].command[c], enables);
			}
			printf("reset lun %d, netfn %d, command\n", l, n);
			memset(enables, 0xff, sizeof(enables));
			ret = _set_command_enables(intf, &p,
				&bmc_fn_support->lun[l].netfn[n], enables, 0);
		}
	}

	free(bmc_fn_support);
	return ret;
}


/* ipmi_firewall_main - top-level handler for firmware firewall functions
 *
 * @intf:	ipmi interface
 * @argc:	number of arguments
 * @argv:	argument list
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_firewall_main(struct ipmi_intf * intf, int argc, char ** argv)
{
	int rc = 0;

	if (argc < 1 || strncmp(argv[0], "help", 4) == 0) {
		printf_firewall_usage();
	}
	else if (strncmp(argv[0], "info", 4) == 0) {
		rc = ipmi_firewall_info(intf, argc-1, &(argv[1]));
	}
	else if (strncmp(argv[0], "enable", 6) == 0) {
		rc = ipmi_firewall_enable_disable(intf, 1, argc-1, &(argv[1]));
	}
	else if (strncmp(argv[0], "disable", 7) == 0) {
		rc = ipmi_firewall_enable_disable(intf, 0, argc-1, &(argv[1]));
	}
	else if (strncmp(argv[0], "reset", 5) == 0) {
		rc = ipmi_firewall_reset(intf, argc-1, &(argv[1]));
	}
	else {
		printf_firewall_usage();
	}

	return rc;
}
