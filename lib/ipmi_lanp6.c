/*
 * Copyright (c) 2016 Pentair Technical Products. All right reserved
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
 * Neither the name of Pentair Technical Products or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * PENTAIR TECHNICAL SOLUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ipmitool/helper.h>
#include <ipmitool/ipmi_cc.h>
#include <ipmitool/ipmi_cfgp.h>
#include <ipmitool/ipmi_lanp.h>
#include <ipmitool/ipmi_lanp6.h>
#include <ipmitool/log.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/*
 * LAN6 command values.
 */
enum {
	LANP_CMD_SAVE,
	LANP_CMD_SET,
	LANP_CMD_PRINT,
	LANP_CMD_LOCK,
	LANP_CMD_COMMIT,
	LANP_CMD_DISCARD,
	LANP_CMD_HELP,
	LANP_CMD_ANY = 0xFF
};

/*
 * Generic LAN configuration parameters.
 */
const struct ipmi_lanp generic_lanp6[] = {
	{ 0,	"Set In Progress", 1 },
	{ 50,	"IPv6/IPv4 Support", 1 },
	{ 51,	"IPv6/IPv4 Addressing Enables", 1 },
	{ 52,	"IPv6 Header Traffic Class", 1 },
	{ 53,	"IPv6 Header Static Hop Limit", 1 },
	{ 54,	"IPv6 Header Flow Label", 3 },
	{ 55,	"IPv6 Status", 3 },
	{ 56,	"IPv6 Static Address", 20 },
	{ 57,	"IPv6 DHCPv6 Static DUID Storage Length", 1 },
	{ 58,	"IPv6 DHCPv6 Static DUID", 18 },
	{ 59,	"IPv6 Dynamic Address", 20 },
	{ 60,	"IPv6 DHCPv6 Dynamic DUID Storage Length", 1 },
	{ 61,	"IPv6 DHCPv6 Dynamic DUID", 18 },
	{ 62,	"IPv6 DHCPv6 Timing Configuration Support", 1 },
	{ 63,	"IPv6 DHCPv6 Timing Configuration", 18 },
	{ 64,	"IPv6 Router Address Configuration Control", 1 },
	{ 65,	"IPv6 Static Router 1 IP Address", 16 },
	{ 66,	"IPv6 Static Router 1 MAC Address", 6 },
	{ 67,	"IPv6 Static Router 1 Prefix Length", 1 },
	{ 68,	"IPv6 Static Router 1 Prefix Value", 16 },
	{ 69,	"IPv6 Static Router 2 IP Address", 16 },
	{ 70,	"IPv6 Static Router 2 MAC Address", 6 },
	{ 71,	"IPv6 Static Router 2 Prefix Length", 1 },
	{ 72,	"IPv6 Static Router 2 Prefix Value", 16 },
	{ 73,	"IPv6 Number of Dynamic Router Info Sets", 1 },
	{ 74,	"IPv6 Dynamic Router Info IP Address", 17 },
	{ 75,	"IPv6 Dynamic Router Info MAC Address", 7 },
	{ 76,	"IPv6 Dynamic Router Info Prefix Length", 2 },
	{ 77,	"IPv6 Dynamic Router Info Prefix Value", 17 },
	{ 78,	"IPv6 Dynamic Router Received Hop Limit", 1 },
	{ 79,	"IPv6 ND/SLAAC Timing Configuration Support", 1 },
	{ 80,	"IPv6 ND/SLAAC Timing Configuration", 18 },
	{ 0,	NULL, 0 }
};

/*
 * Set/Get LAN Configuration Parameters
 * command-specific completion codes.
 */
const struct valstr lanp_cc_vals[] = {
	{ 0x80, "Parameter not supported" },
	{ 0x81, "Set already in progress" },
	{ 0x82, "Parameter is read-only" },
	{ 0x83, "Write-only parameter" },
	{ 0x00, NULL }
};

/*
 * IPv6/IPv4 Addressing Enables.
 */
const struct valstr ip6_enable_vals[] = {
	{ 0, "ipv4" },
	{ 1, "ipv6" },
	{ 2, "both" },
	{ 0xFF, NULL }
};

/*
 * Enable/Disable a static address.
 */
const struct valstr ip6_addr_enable_vals[] = {
	{ 0x00, "disable" },
	{ 0x80, "enable" },
	{ 0xFF, NULL }
};

/*
 * IPv6 address source values.
 */
const struct valstr ip6_addr_sources[] = {
	{ 0, "static" },
	{ 1, "SLAAC" },
	{ 2, "DHCPv6" },
	{ 0, NULL }
};

/*
 * IPv6 address status values.
 */
const struct valstr ip6_addr_statuses[] = {
	{ 0, "active" },
	{ 1, "disabled" },
	{ 2, "pending" },
	{ 3, "failed" },
	{ 4, "deprecated" },
	{ 5, "invalid" },
	{ 0xFF, NULL }
};

/*
 * DHCPv6 DUID type values.
 */
const struct valstr ip6_duid_types[] = {
	{ 0, "unknown" },
	{ 1, "DUID-LLT" },
	{ 2, "DUID-EN" },
	{ 3, "DUID-LL" },
	{ 0xFF, NULL }
};

/*
 * Timing Configuration support values.
 */
const struct valstr ip6_cfg_sup_vals[] = {
	{ 0, "not supported" },
	{ 1, "global" },
	{ 2, "per interface" },
	{ 0xFF, NULL }
};

/*
 * Router Address Configuration Control values.
 */
const struct valstr ip6_rtr_configs[] = {
	{ 1, "static" },
	{ 2, "dynamic" },
	{ 3, "both" },
	{ 0xFF, NULL }
};


const struct valstr ip6_command_vals[] = {
	{ LANP_CMD_SET,		"set" },
	{ LANP_CMD_SAVE,	"save" },
	{ LANP_CMD_PRINT,	"print" },
	{ LANP_CMD_LOCK,	"lock" },
	{ LANP_CMD_COMMIT,	"commit" },
	{ LANP_CMD_DISCARD,	"discard" },
	{ LANP_CMD_HELP,	"help" },
	{ LANP_CMD_ANY,		NULL }
};

static const struct ipmi_cfgp lan_cfgp[] = {
	{ .name = "support", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_SUPPORT
	},
	{ .name = "enables", .format = "{ipv4|ipv6|both}", .size = 1,
		.access = CFGP_RDWR,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_ENABLES
	},
	{ .name = "traffic_class", .format = "<value>", .size = 1,
		.access = CFGP_RDWR,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_TRAFFIC_CLASS
	},
	{ .name = "static_hops", .format = "<value>", .size = 1,
		.access = CFGP_RDWR,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_STATIC_HOPS
	},
	{ .name = "flow_label", .format = "<value>", .size = 3,
		.access = CFGP_RDWR,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_FLOW_LABEL
	},
	{ .name = "status", .format = NULL, .size = 3,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_STATUS
	},
	{ .name = "static_addr",
		.format = "{enable|disable} <addr> <pfx_len>", .size = 20,
		.access = CFGP_RDWR,
		.is_set = 1, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_STATIC_ADDR
	},
	{ .name = "static_duid_stg", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_STATIC_DUID_STG
	},
	{ .name = "static_duid", .format = "<data>", .size = 18,
		.access = CFGP_RDWR,
		.is_set = 1, .first_set = 0, .has_blocks = 1, .first_block = 0,
		.specific = IPMI_LANP_IP6_STATIC_DUID
	},
	{ .name = "dynamic_addr", .format = NULL, .size = 20,
		.access = CFGP_RDONLY,
		.is_set = 1, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_DYNAMIC_ADDR
	},
	{ .name = "dynamic_duid_stg", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_DYNAMIC_DUID_STG
	},
	{ .name = "dynamic_duid", .format = "<data>", .size = 18,
		.access = CFGP_RDWR,
		.is_set = 1, .first_set = 0, .has_blocks = 1, .first_block = 0,
		.specific = IPMI_LANP_IP6_DYNAMIC_DUID
	},
	{ .name = "dhcp6_cfg_sup", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_DHCP6_CFG_SUP
	},
	{ .name = "dhcp6_cfg", .format = "<data> <data>", .size = 36,
		.access = CFGP_RDWR,
		.is_set = 1, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_DHCP6_CFG
	},
	{ .name = "rtr_cfg", .format = "{static|dynamic|both}", .size = 1,
		.access = CFGP_RDWR,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_ROUTER_CFG
	},
	{ .name = "static_rtr",
		.format = "<addr> <macaddr> <prefix> <prefix_len>", .size = 43,
		.access = CFGP_RDWR,
		.is_set = 1, .first_set = 1, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_STATIC_RTR1_ADDR
	},
	{ .name = "num_dynamic_rtrs", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_NUM_DYNAMIC_RTRS
	},
	{ .name = "dynamic_rtr", .format = NULL, .size = 43,
		.access = CFGP_RDONLY,
		.is_set = 1, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_DYNAMIC_RTR_ADDR
	},
	{ .name = "dynamic_hops", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_DYNAMIC_HOPS
	},
	{ .name = "ndslaac_cfg_sup", .format = NULL, .size = 1,
		.access = CFGP_RDONLY,
		.is_set = 0, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_NDSLAAC_CFG_SUP
	},
	{ .name = "ndslaac_cfg", .format = "<data>", .size = 18,
		.access = CFGP_RDWR,
		.is_set = 1, .first_set = 0, .has_blocks = 0, .first_block = 0,
		.specific = IPMI_LANP_IP6_NDSLAAC_CFG
	}
};

/*
 * Lookup LAN parameter descriptor by parameter selector.
 */
const struct ipmi_lanp *
lookup_lanp(int param)
{
	const struct ipmi_lanp *p = generic_lanp6;

	while (p->name) {
		if (p->selector == param) {
			return p;
		}

		p++;
	}

	return NULL;
}

/*
 * Print request error.
 */
static int
ipmi_lanp_err(const struct ipmi_rs *rsp, const struct ipmi_lanp *p,
		const char *action, int quiet)
{
	const char *reason;
	char cc_msg[10];
	int log_level = LOG_ERR;
	int err;

	if (!rsp) {
		reason = "No response";
		err = -1;
	} else {
		err = rsp->ccode;
		if (quiet == 1
			&& (rsp->ccode == 0x80
			|| rsp->ccode == IPMI_CC_PARAM_OUT_OF_RANGE
			|| rsp->ccode == IPMI_CC_INV_DATA_FIELD_IN_REQ)) {
			/* be quiet */
			return err;
		}

		if (rsp->ccode >= 0xC0) {
			/* browse for generic completion codes */
			reason = val2str(rsp->ccode, completion_code_vals);
		} else {
			/* browse for command-specific completion codes first */
			reason = val2str(rsp->ccode, lanp_cc_vals);
		}

		if (!reason) {
			/* print completion code value */
			snprintf(cc_msg, sizeof(cc_msg), "CC=%02x", rsp->ccode);
			reason = cc_msg;
		}

		if (rsp->ccode == IPMI_CC_OK) {
			log_level = LOG_DEBUG;
		}
	}

	lprintf(log_level, "Failed to %s %s: %s", action, p->name, reason);
	return err;
}

/*
 * Get dynamic OEM LAN configuration parameter from BMC.
 * Dynamic in this context is when the base for OEM LAN parameters
 * is not known apriori.
 */
int
ipmi_get_dynamic_oem_lanp(void *priv, const struct ipmi_lanp *param,
		int oem_base, int set_selector, int block_selector,
		void *data, int quiet)
{
	struct ipmi_lanp_priv *lp = priv;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t req_data[4];
	int length;

	if (!priv || !param || !data) {
		return -1;
	}
	req_data[0] = lp->channel;
	req_data[1] = param->selector + oem_base;
	req_data[2] = set_selector;
	req_data[3] = block_selector;

	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = 2;
	req.msg.data     = req_data;
	req.msg.data_len = 4;

	lprintf(LOG_INFO, "Getting parameter '%s' set %d block %d",
		param->name, set_selector, block_selector);

	rsp = lp->intf->sendrecv(lp->intf, &req);
	if (!rsp || rsp->ccode) {
		return ipmi_lanp_err(rsp, param, "get", quiet);
	}

	memset(data, 0, param->size);

	if (rsp->data_len - 1 < param->size) {
		length = rsp->data_len - 1;
	} else {
		length = param->size;
	}

	if (length) {
		memcpy(data, rsp->data + 1, length);
	}

	return 0;
}

/*
 * Get generic LAN configuration parameter.
 */
int
ipmi_get_lanp(void *priv, int param_selector, int set_selector,
		int block_selector, void *data, int quiet)
{
	return ipmi_get_dynamic_oem_lanp(priv, lookup_lanp(param_selector), 0,
			set_selector, block_selector, data, quiet);
}

/*
 * Set dynamic OEM LAN configuration parameter to BMC.
 * Dynamic in this context is when the base for OEM LAN parameters
 * is not known apriori.
 */
int
ipmi_set_dynamic_oem_lanp(void *priv, const struct ipmi_lanp *param,
		int base, const void *data)
{
	struct ipmi_lanp_priv *lp = priv;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;
	uint8_t req_data[32];

	if (!priv || !param || !data) {
		return -1;
	}
	/* fill the first two bytes */
	req_data[0] = lp->channel;
	req_data[1] = param->selector + base;

	/* fill the rest data */
	memcpy(&req_data[2], data, param->size);

	/* fill request */
	memset(&req, 0, sizeof(req));
	req.msg.netfn    = IPMI_NETFN_TRANSPORT;
	req.msg.cmd      = 1;
	req.msg.data     = req_data;
	req.msg.data_len = param->size + 2;

	lprintf(LOG_INFO, "Setting parameter '%s'", param->name);

	rsp = lp->intf->sendrecv(lp->intf, &req);
	if (!rsp || rsp->ccode) {
		return ipmi_lanp_err(rsp, param, "set", 0);
	}

	return 0;
}

/*
 * Set generic LAN configuration parameter.
 */
int
ipmi_set_lanp(void *priv, int param_selector, const void *data)
{
	return ipmi_set_dynamic_oem_lanp(priv, lookup_lanp(param_selector),
		0, data);
}

static int
lanp_parse_cfgp(const struct ipmi_cfgp *p, int set, int block,
		int argc, const char *argv[], unsigned char *data)
{
	unsigned int v;

	if (argc == 0) {
		return -1;
	}

	switch(p->specific) {
	case IPMI_LANP_IP6_ENABLES:
		data[0] = str2val(argv[0], ip6_enable_vals);
		if (data[0] == 0xFF) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
		break;

	case IPMI_LANP_IP6_FLOW_LABEL:
		if (str2uint(argv[0], &v)) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}

		data[0] = (v >> 16) & 0x0F;
		data[1] = (v >> 8) & 0xFF;
		data[2] = v & 0xFF;
		break;

	case IPMI_LANP_IP6_STATUS:
		if (argc < 3) {
			return -1;
		}

		if (str2uchar(argv[0], &data[0])
				|| str2uchar(argv[1], &data[1])
				|| str2uchar(argv[2], &data[2])) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
		break;

	case IPMI_LANP_IP6_STATIC_ADDR:
	case IPMI_LANP_IP6_DYNAMIC_ADDR:
		if (argc < 3) {
			return -1;
		}

		data[0] = set;
		if (p->specific == IPMI_LANP_IP6_STATIC_ADDR) {
			data[1] = str2val(argv[0], ip6_addr_enable_vals);
		} else {
			data[1] = str2val(argv[0], ip6_addr_sources);
		}
		if (data[1] == 0xFF) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}

		if (inet_pton(AF_INET6, argv[1], &data[2]) != 1) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}

		if (str2uchar(argv[2], &data[18])) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}

		if (argc >= 4) {
			data[19] = str2val(argv[3], ip6_addr_statuses);
		}
		break;

	case IPMI_LANP_IP6_STATIC_DUID:
	case IPMI_LANP_IP6_DYNAMIC_DUID:
	case IPMI_LANP_IP6_NDSLAAC_CFG:
		data[0] = set;
		data[1] = block;
		if (ipmi_parse_hex(argv[0], &data[2], 16) < 0) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
		break;

	case IPMI_LANP_IP6_DHCP6_CFG:
		data[0] = set;
		data[1] = 0;
		data[18] = set;
		data[19] = 1;

		if (ipmi_parse_hex(argv[0], &data[2], 16) < 0
			|| (argc > 1 &&
			    ipmi_parse_hex(argv[1], &data[20], 6) < 0)) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
		break;

	case IPMI_LANP_IP6_ROUTER_CFG:
		data[0] = str2val(argv[0], ip6_rtr_configs);
		if (data[0] == 0xFF) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
		break;

	case IPMI_LANP_IP6_STATIC_RTR1_ADDR:
		if (set > 2) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}

	case IPMI_LANP_IP6_DYNAMIC_RTR_ADDR:
		if (argc < 4) {
			return -1;
		}

		/*
		 * Data is stored in the following way:
		 *  0: <set> <addr1>...<addr16>
		 * 17: <set> <mac1>...<mac6>
		 * 24: <set> <pfxlen>
		 * 26: <set> <pfx1>...<pfx16>
		 */
		data[0] = data[17] = data[24] = data[26] = set;

		if (inet_pton(AF_INET6, argv[0], &data[1]) != 1
				|| str2mac(argv[1], &data[18])
				|| inet_pton(AF_INET6, argv[2], &data[27]) != 1
				|| str2uchar(argv[3], &data[25])) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
		break;

	default:
		if (str2uchar(argv[0], &data[0])) {
			lprintf(LOG_ERR, "invalid value");
			return -1;
		}
	}

	return 0;
}

static int
lanp_set_cfgp(void *priv, const struct ipmi_cfgp *p, const unsigned char *data)
{
	int ret;
	int param = p->specific;
	int off = 0;

	switch(param) {
	case IPMI_LANP_IP6_DHCP6_CFG:
		ret = ipmi_set_lanp(priv, param, &data[0]);
		if (ret == 0) {
			ret = ipmi_set_lanp(priv, param, &data[18]);
		}
		break;

	case IPMI_LANP_IP6_STATIC_RTR1_ADDR:
		if (data[0] == 2) {
			param = IPMI_LANP_IP6_STATIC_RTR2_ADDR;
		}
		off = 1;

	case IPMI_LANP_IP6_DYNAMIC_RTR_ADDR:
		ret = ipmi_set_lanp(priv, param, &data[0 + off]);
		if (ret == 0) {
			ret = ipmi_set_lanp(priv, param + 1, &data[17 + off]);
		}
		if (ret == 0) {
			ret = ipmi_set_lanp(priv, param + 2, &data[24 + off]);
		}
		if (ret == 0) {
			ret = ipmi_set_lanp(priv, param + 3, &data[26 + off]);
		}
		break;


	default:
		ret = ipmi_set_lanp(priv, param, data);
	}

	return ret;
}

static int
lanp_get_cfgp(void *priv, const struct ipmi_cfgp *p,
		int set, int block,  unsigned char *data, int quiet)
{
	int ret;
	int param = p->specific;
	int off = 0;

	switch(param) {
	case IPMI_LANP_IP6_DHCP6_CFG:
		ret = ipmi_get_lanp(priv, param, set, 0, &data[0], quiet);
		if (ret == 0) {
			ret = ipmi_get_lanp(priv, param, set,
				1, &data[18], quiet);
		}
		break;

	case IPMI_LANP_IP6_STATIC_RTR1_ADDR:
		if (set > 2) {
			return -1;
		}

		if (set == 2) {
			param = IPMI_LANP_IP6_STATIC_RTR2_ADDR;
		}
		set = 0;
		off = 1;
		data[0] = data[17] = data[24] = data[26] = set;

	case IPMI_LANP_IP6_DYNAMIC_RTR_ADDR:
		ret = ipmi_get_lanp(priv, param, set, block,
			&data[0 + off], quiet);
		if (ret == 0) {
			ret = ipmi_get_lanp(priv, param + 1, set, block,
				&data[17 + off], 0);
		}
		if (ret == 0) {
			ret = ipmi_get_lanp(priv, param + 2, set, block,
				&data[24 + off], 0);
		}
		if (ret == 0) {
			ret = ipmi_get_lanp(priv, param + 3, set, block,
				&data[26 + off], 0);
		}
		break;

	default:
		ret = ipmi_get_lanp(priv, param, set, block, data, quiet);
	}

	return ret;
}

static int
lanp_save_cfgp(const struct ipmi_cfgp *p, const unsigned char *data, FILE *file)
{
	char addr[INET6_ADDRSTRLEN];
	char pfx[INET6_ADDRSTRLEN];
	const char *src;

	switch(p->specific) {
	case IPMI_LANP_IP6_ENABLES:
		fputs(val2str(data[0], ip6_enable_vals), file);
		break;

	case IPMI_LANP_IP6_FLOW_LABEL:
		fprintf(file, "0x%xd", (data[0] << 16 ) | (data[1] << 8) | data[2]);
		break;

	case IPMI_LANP_IP6_STATUS:
		fprintf(file, "%d %d %d", data[0], data[1], data[2]);
		break;

	case IPMI_LANP_IP6_STATIC_ADDR:
	case IPMI_LANP_IP6_DYNAMIC_ADDR:
		if (p->specific == IPMI_LANP_IP6_STATIC_ADDR) {
			src = val2str(data[1], ip6_addr_enable_vals);
		} else {
			src = val2str(data[1], ip6_addr_sources);
		}

		fprintf(file, "%s %s %d %s", src,
				inet_ntop(AF_INET6, &data[2], addr, sizeof(addr)),
				data[18], val2str(data[19], ip6_addr_statuses));
		break;

	case IPMI_LANP_IP6_STATIC_DUID:
	case IPMI_LANP_IP6_DYNAMIC_DUID:
	case IPMI_LANP_IP6_NDSLAAC_CFG:
		fprintf(file, "%s", buf2str(&data[2], 16));
		break;

	case IPMI_LANP_IP6_DHCP6_CFG:
		fprintf(file, "%s", buf2str(&data[2], 16));
		fprintf(file, " %s", buf2str(&data[20], 6));
		break;

	case IPMI_LANP_IP6_ROUTER_CFG:
		fputs(val2str(data[0], ip6_rtr_configs), file);
		break;

	case IPMI_LANP_IP6_STATIC_RTR1_ADDR:
	case IPMI_LANP_IP6_DYNAMIC_RTR_ADDR:
		fprintf(file, "%s %s %s %d",
				inet_ntop(AF_INET6, &data[1], addr, sizeof(addr)),
				mac2str(&data[18]),
				inet_ntop(AF_INET6, &data[27], pfx, sizeof(pfx)), data[25]);
		break;

	default:
		fprintf(file, "%d", data[0]);
	}

	return 0;
}


static int
lanp_print_cfgp(const struct ipmi_cfgp *p,
		int set, int block, const unsigned char *data, FILE *file)
{
	char addr[INET6_ADDRSTRLEN];
	char pfx[INET6_ADDRSTRLEN];
	const char *pname;
	const struct ipmi_lanp *lanp = lookup_lanp(p->specific);

	if (!lanp || !p || !file || !data || !lanp->name) {
		return -1;
	}
	pname = lanp->name;

	switch(p->specific) {
	case IPMI_LANP_IP6_SUPPORT:
		fprintf(file, "%s:\n"
				"    IPv6 only: %s\n"
				"    IPv4 and IPv6: %s\n"
				"    IPv6 Destination Addresses for LAN alerting: %s\n",
				pname,
				data[0] & 1 ? "yes" : "no",
				data[0] & 2 ? "yes" : "no",
				data[0] & 4 ? "yes" : "no");
		break;

	case IPMI_LANP_IP6_ENABLES:
		fprintf(file, "%s: %s\n",
				pname, val2str(data[0], ip6_enable_vals));
		break;

	case IPMI_LANP_IP6_FLOW_LABEL:
		fprintf(file, "%s: %d\n",
				pname, (data[0] << 16 ) | (data[1] << 8) | data[2]);
		break;

	case IPMI_LANP_IP6_STATUS:
		fprintf(file, "%s:\n"
				"    Static address max:  %d\n"
				"    Dynamic address max: %d\n"
				"    DHCPv6 support:      %s\n"
				"    SLAAC support:       %s\n",
				pname,
				data[0], data[1],
				(data[2] & 1) ? "yes" : "no",
				(data[2] & 2) ? "yes" : "no");
		break;

	case IPMI_LANP_IP6_STATIC_ADDR:
		fprintf(file, "%s %d:\n"
				"    Enabled:        %s\n"
				"    Address:        %s/%d\n"
				"    Status:         %s\n",
				pname, set,
				(data[1] & 0x80) ? "yes" : "no",
				inet_ntop(AF_INET6, &data[2], addr, sizeof(addr)),
				data[18], val2str(data[19] & 0xF, ip6_addr_statuses));
		break;

	case IPMI_LANP_IP6_DYNAMIC_ADDR:
		fprintf(file, "%s %d:\n"
				"    Source/Type:    %s\n"
				"    Address:        %s/%d\n"
				"    Status:         %s\n",
				pname, set,
				val2str(data[1] & 0xF, ip6_addr_sources),
				inet_ntop(AF_INET6, &data[2], addr, sizeof(addr)),
				data[18], val2str(data[19] & 0xF, ip6_addr_statuses));
		break;

	case IPMI_LANP_IP6_STATIC_DUID:
	case IPMI_LANP_IP6_DYNAMIC_DUID:
		if (block == 0) {
			fprintf(file, "%s %d:\n"
				"    Length:   %d\n"
				"    Type:     %s\n",
				pname, set, data[2],
				val2str((data[3] << 8) + data[4], ip6_duid_types));
		}
		fprintf(file, "    %s\n", buf2str(&data[2], 16));
		break;

	case IPMI_LANP_IP6_DHCP6_CFG_SUP:
	case IPMI_LANP_IP6_NDSLAAC_CFG_SUP:
		fprintf(file, "%s: %s\n",
				pname, val2str(data[0], ip6_cfg_sup_vals));
		break;

	case IPMI_LANP_IP6_DHCP6_CFG:
		fprintf(file, "%s %d:\n", pname, set);

		fprintf(file,
				"    SOL_MAX_DELAY:   %d\n"
				"    SOL_TIMEOUT:     %d\n"
				"    SOL_MAX_RT:      %d\n"
				"    REQ_TIMEOUT:     %d\n"
				"    REQ_MAX_RT:      %d\n"
				"    REQ_MAX_RC:      %d\n"
				"    CNF_MAX_DELAY:   %d\n"
				"    CNF_TIMEOUT:     %d\n"
				"    CNF_MAX_RT:      %d\n"
				"    CNF_MAX_RD:      %d\n"
				"    REN_TIMEOUT:     %d\n"
				"    REN_MAX_RT:      %d\n"
				"    REB_TIMEOUT:     %d\n"
				"    REB_MAX_RT:      %d\n"
				"    INF_MAX_DELAY:   %d\n"
				"    INF_TIMEOUT:     %d\n"
				"    INF_MAX_RT:      %d\n"
				"    REL_TIMEOUT:     %d\n"
				"    REL_MAX_RC:      %d\n"
				"    DEC_TIMEOUT:     %d\n"
				"    DEC_MAX_RC:      %d\n"
				"    HOP_COUNT_LIMIT: %d\n",
				data[2], data[3], data[4], data[5],
				data[6], data[7], data[8], data[9],
				data[10], data[11], data[12], data[13],
				data[14], data[15], data[16], data[17],
				data[20], data[21], data[22], data[23],
				data[24], data[25]);
		break;

	case IPMI_LANP_IP6_ROUTER_CFG:
		fprintf(file, "%s:\n"
				"    Enable static router address:  %s\n"
				"    Enable dynamic router address: %s\n",
				pname,
				(data[0] & 1) ? "yes" : "no",
				(data[0] & 2) ? "yes" : "no");
		break;

	case IPMI_LANP_IP6_STATIC_RTR1_ADDR:
	case IPMI_LANP_IP6_DYNAMIC_RTR_ADDR:
		if (p->specific == IPMI_LANP_IP6_STATIC_RTR1_ADDR) {
			pname = "IPv6 Static Router";
		} else {
			pname = "IPv6 Dynamic Router";
		}

		fprintf(file, "%s %d:\n"
				"    Address: %s\n"
				"    MAC:     %s\n"
				"    Prefix:  %s/%d\n",
				pname, set,
				inet_ntop(AF_INET6, &data[1], addr, sizeof(addr)),
				mac2str(&data[18]),
				inet_ntop(AF_INET6, &data[27], pfx, sizeof(pfx)), data[25]);
		break;

	case IPMI_LANP_IP6_NDSLAAC_CFG:
		fprintf(file, "%s %d:\n"
				"    MAX_RTR_SOLICITATION_DELAY: %d\n"
				"    RTR_SOLICITATION_INTERVAL:  %d\n"
				"    MAX_RTR_SOLICITATIONS:      %d\n"
				"    DupAddrDetectTransmits:     %d\n"
				"    MAX_MULTICAST_SOLICIT:      %d\n"
				"    MAX_UNICAST_SOLICIT:        %d\n"
				"    MAX_ANYCAST_DELAY_TIME:     %d\n"
				"    MAX_NEIGHBOR_ADVERTISEMENT: %d\n"
				"    REACHABLE_TIME:             %d\n"
				"    RETRANS_TIMER:              %d\n"
				"    DELAY_FIRST_PROBE_TIME:     %d\n"
				"    MAX_RANDOM_FACTOR:          %d\n"
				"    MIN_RANDOM_FACTOR:          %d\n",
				pname, set,
				data[2], data[3], data[4], data[5],
				data[6], data[7], data[8], data[9],
				data[10], data[11], data[12], data[13],
				data[14]);
		break;

	default:
		fprintf(file, "%s: %d\n", pname, data[0]);
	}

	return 0;
}

static int
lanp_ip6_cfgp(void *priv, const struct ipmi_cfgp *p,
		const struct ipmi_cfgp_action *action, unsigned char *data)
{
	switch (action->type) {
	case CFGP_PARSE:
		return lanp_parse_cfgp(p, action->set, action->block,
				action->argc, action->argv, data);

	case CFGP_GET:
		return lanp_get_cfgp(priv, p, action->set, action->block,
				data, action->quiet);

	case CFGP_SET:
		return lanp_set_cfgp(priv, p, data);

	case CFGP_SAVE:
		return lanp_save_cfgp(p, data, action->file);

	case CFGP_PRINT:
		return lanp_print_cfgp(p, action->set, action->block,
				data, action->file);

	default:
		return -1;
	}

	return 0;
}

static void lanp_print_usage(int cmd)
{
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_HELP) {
		printf("  help [command]\n");
	}
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_SAVE) {
		printf("  save <channel> [<parameter> [<set_sel> [<block_sel>]]]\n");
	}
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_SET) {
		printf("  set <channel> [nolock] <parameter> [<set_sel> [<block_sel>]] <values...>\n");
	}
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_PRINT) {
		printf("  print <channel> [<parameter> [<set_sel> [<block_sel>]]]\n");
	}
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_LOCK) {
		printf("  lock <channel>\n");
	}
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_COMMIT) {
		printf("  commit <channel>\n");
	}
	if (cmd == LANP_CMD_ANY || cmd == LANP_CMD_DISCARD) {
		printf("  discard <channel>\n");
	}
	if (cmd == LANP_CMD_SAVE
		|| cmd == LANP_CMD_PRINT
		|| cmd == LANP_CMD_SET) {
		printf("\n   available parameters:\n");
		/* 'save' shall use 'write' filter, since it outputs a block
		 * of 'set's */
		ipmi_cfgp_usage(lan_cfgp, ARRAY_SIZE(lan_cfgp), cmd != LANP_CMD_PRINT);
	}
}

static int
lanp_lock(struct ipmi_lanp_priv *lp)
{
	unsigned char byte = 1;

	return ipmi_set_lanp(lp, 0, &byte);
}

static int
lanp_discard(struct ipmi_lanp_priv *lp)
{
	unsigned char byte = 0;

	return ipmi_set_lanp(lp, 0, &byte);
}

static int
lanp_commit(struct ipmi_lanp_priv *lp)
{
	unsigned char byte = 2;
	int ret;

	ret = ipmi_set_lanp(lp, 0, &byte);
	if (ret == 0) {
		ret = lanp_discard(lp);
	}

	return ret;
}

int
ipmi_lan6_main(struct ipmi_intf *intf, int argc, char **argv)
{
	struct ipmi_cfgp_ctx ctx;
	struct ipmi_cfgp_sel sel;
	struct ipmi_lanp_priv lp;
	int cmd;
	int chan;
	int nolock = 0;
	int ret;

	if (argc == 0) {
		lanp_print_usage(LANP_CMD_ANY);
		return 0;
	}

	cmd = str2val(argv[0], ip6_command_vals);
	if (cmd == LANP_CMD_ANY) {
		lanp_print_usage(cmd);
		return -1;
	}

	if (cmd == LANP_CMD_HELP) {
		if (argc == 1) {
			cmd = LANP_CMD_ANY;
		} else {
			cmd = str2val(argv[1], ip6_command_vals);
		}

		lanp_print_usage(cmd);
		return 0;
	}

	/*
	 * the rest commands expect channel number
	 * with the exception of 'get' and 'print'
	 */
	if (argc == 1) {
		if (cmd == LANP_CMD_SAVE || cmd == LANP_CMD_PRINT) {
			chan = find_lan_channel(intf, 1);
			if (chan == 0) {
				lprintf(LOG_ERR, "No LAN channel found");
				return -1;
			}
		} else {
			lanp_print_usage(cmd);
			return -1;
		}

		argc -= 1;
		argv += 1;
	} else {
		if (str2int(argv[1], &chan) != 0) {
			lprintf(LOG_ERR, "Invalid channel: %s", argv[1]);
			return -1;
		}

		argc -= 2;
		argv += 2;

		if (cmd == LANP_CMD_SET) {
			if (argc && !strcasecmp(argv[0], "nolock")) {
				nolock = 1;

				argc -= 1;
				argv += 1;
			}
		}

	}

	lp.intf = intf;
	lp.channel = chan;

	/*
	 * lock/commit/discard commands do not require parsing
	 * of parameter selection
	 */

	switch (cmd) {
	case LANP_CMD_LOCK:
		lprintf(LOG_NOTICE, "Lock parameter(s)...");
		return lanp_lock(&lp);

	case LANP_CMD_COMMIT:
		lprintf(LOG_NOTICE, "Commit parameter(s)...");
		return lanp_commit(&lp);

	case LANP_CMD_DISCARD:
		lprintf(LOG_NOTICE, "Discard parameter(s)...");
		return lanp_discard(&lp);
	}

	/*
	 * initialize configuration context and parse parameter selection
	 */

	ipmi_cfgp_init(&ctx, lan_cfgp,
	               ARRAY_SIZE(lan_cfgp), "lan6 set nolock",
	               lanp_ip6_cfgp, &lp);

	ret = ipmi_cfgp_parse_sel(&ctx, argc, (const char **)argv, &sel);
	if (ret == -1) {
		lanp_print_usage(cmd);
		ipmi_cfgp_uninit(&ctx);
		return -1;
	}

	argc -= ret;
	argv += ret;

	/*
	 * handle the rest commands
	 */

	switch (cmd) {
	case LANP_CMD_SAVE:
	case LANP_CMD_PRINT:
		lprintf(LOG_NOTICE, "Getting parameter(s)...");

		ret = ipmi_cfgp_get(&ctx, &sel);
		if (ret != 0) {
			break;
		}

		if (cmd == LANP_CMD_SAVE) {
			static char cmd[20];
			FILE *out = stdout;
			snprintf(cmd, sizeof(cmd) - 1, "lan6 set %d nolock",
				lp.channel);
			cmd[sizeof(cmd) - 1] = '\0';
			ctx.cmdname = cmd;
			fprintf(out, "lan6 lock %d\n", lp.channel);
			ret = ipmi_cfgp_save(&ctx, &sel, out);
			fprintf(out, "lan6 commit %d\nlan6 discard %d\nexit\n",
				lp.channel, lp.channel);
		} else {
			ret = ipmi_cfgp_print(&ctx, &sel, stdout);
		}
		break;

	case LANP_CMD_SET:
		ret = ipmi_cfgp_parse_data(&ctx, &sel, argc,
			(const char **)argv);
		if (ret != 0) {
			break;
		}

		lprintf(LOG_NOTICE, "Setting parameter(s)...");

		if (!nolock) {
			ret = lanp_lock(&lp);
			if (ret != 0) {
				break;
			}
		}

		ret = ipmi_cfgp_set(&ctx, &sel);
		if (!nolock) {
			if (ret == 0) {
				ret = lanp_commit(&lp);
			} else {
				lanp_discard(&lp);
			}
		}
		break;
	}

	/*
	 * free allocated memory
	 */
	ipmi_cfgp_uninit(&ctx);

	return ret;
}
